"""Compile current D3D installer and exercise its A/B transaction on owned x86 images."""

import argparse
import datetime
import hashlib
import os
from pathlib import Path
import re
import runpy
import struct
import subprocess

import pefile


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def run(command: list[str], cwd: Path, log: Path) -> str:
    result = subprocess.run(command, cwd=cwd, stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT, text=True, errors="replace",
                            timeout=180)
    log.write_text(result.stdout, encoding="utf-8")
    if result.returncode:
        print(result.stdout)
        raise SystemExit(f"{log.name} failed with exit {result.returncode}")
    return result.stdout


def surrogate_source(prior_driver: Path) -> str:
    text = prior_driver.read_text(encoding="utf-8")
    match = re.search(r"surrogate_source\.write_text\(r'''(.*?)'''\.lstrip\(\)",
                      text, re.S)
    if not match:
        raise SystemExit("R02t owned surrogate source extraction failed")
    return match.group(1).lstrip()


def patch_template_slots(path: Path) -> dict[str, int]:
    pe = pefile.PE(str(path))
    preferred = pe.OPTIONAL_HEADER.ImageBase
    exports = {symbol.name.decode(): symbol.address for symbol in
               pe.DIRECTORY_ENTRY_EXPORT.symbols if symbol.name}
    reset_rva = exports["_FixtureReset@8"]
    present_rva = exports["_FixturePresent@20"]
    section = next((s for s in pe.sections if s.Name.rstrip(b"\0") == b".text"), None)
    if section is None:
        raise SystemExit("surrogate lacks .text")
    slots = (0x1008 + 16 * 4, 0x1008 + 17 * 4,
             0x2034 + 16 * 4, 0x2034 + 17 * 4)
    if any(not (section.VirtualAddress <= rva and
                rva + 4 <= section.VirtualAddress + section.SizeOfRawData)
           for rva in slots):
        raise SystemExit("A/B template slots are outside backed surrogate .text")
    if not all(section.VirtualAddress <= rva <
               section.VirtualAddress + section.Misc_VirtualSize
               for rva in (reset_rva, present_rva)):
        raise SystemExit("surrogate Reset/Present methods are not in .text")
    data = bytearray(path.read_bytes())
    for rva, target in zip(slots, (reset_rva, present_rva,
                                   reset_rva, present_rva)):
        struct.pack_into("<I", data, pe.get_offset_from_rva(rva), preferred + target)
    directory = pe.OPTIONAL_HEADER.DATA_DIRECTORY[5]
    if directory.VirtualAddress == 0 or directory.Size == 0:
        raise SystemExit("surrogate lacks HIGHLOW relocation directory")
    relocation = bytearray()
    for block in pe.DIRECTORY_ENTRY_BASERELOC:
        page = block.struct.VirtualAddress
        entries = [(entry.type << 12) | (entry.rva - page)
                   for entry in block.entries if entry.type]
        entries.extend((3 << 12) | (rva - page) for rva in slots
                       if rva & ~0xFFF == page)
        entries.sort()
        if len(entries) & 1:
            entries.append(0)
        relocation.extend(struct.pack("<II", page, 8 + 2 * len(entries)))
        relocation.extend(struct.pack(f"<{len(entries)}H", *entries))
    reloc_section = next((s for s in pe.sections if
                          s.VirtualAddress <= directory.VirtualAddress <
                          s.VirtualAddress + s.Misc_VirtualSize), None)
    if reloc_section is None or len(relocation) > reloc_section.SizeOfRawData:
        raise SystemExit("surrogate relocation table has no bounded spare raw capacity")
    reloc_offset = pe.get_offset_from_rva(directory.VirtualAddress)
    data[reloc_offset:reloc_offset + reloc_section.SizeOfRawData] = bytes(
        reloc_section.SizeOfRawData)
    data[reloc_offset:reloc_offset + len(relocation)] = relocation
    struct.pack_into("<I", data, directory.get_file_offset() + 4,
                     len(relocation))
    struct.pack_into("<I", data, reloc_section.get_file_offset() + 8,
                     len(relocation))
    pe.close()
    path.write_bytes(data)
    return {"preferred": preferred, "reset_rva": reset_rva,
            "present_rva": present_rva, "a_reset_rva": slots[0],
            "a_present_rva": slots[1], "b_reset_rva": slots[2],
            "b_present_rva": slots[3], "relocation_bytes": len(relocation)}


def make_basis(repo: Path, build: Path, source: str) -> Path:
    prior = repo / "tests/fork/d3d_install_transaction_tests.cpp"
    prior_driver = repo / "tests/fork/test_d3d_install_transaction.py"
    block = runpy.run_path(str(prior_driver))["block"]
    callback_names = (
        r"^static void ForceImmediatePresent\(void\* params\)\s*$",
        r"^static HRESULT WINAPI HookCreateDevice\(",
        r"^static void\* WINAPI HookDirect3DCreate9\(UINT sdk\)\s*$",
    )
    callbacks = "\n\n".join(block(source, pattern) for pattern in callback_names)
    fixture = prior.read_text(encoding="utf-8")
    fixture = fixture.replace("static bool InstallD3dDeviceTablePair(void* device)",
                              "[[maybe_unused]] static bool InstallD3dDeviceTablePair(void* device)")
    marker = "// ACTUAL_D3D_CALLBACKS"
    if fixture.count(marker) != 1:
        raise SystemExit("R02t actual callback marker missing or ambiguous")
    output = build / "r02w_basis.cpp"
    static_stubs = (
        "static bool InstallD3dStaticDeviceTemplates() { ++staticInstallCalls; "
        "return staticInstaller ? staticInstaller() : false; }\n"
        "static bool VerifyD3dStaticDeviceCopy(void* device) { ++verifyCopyCalls; "
        "return copyVerifier ? copyVerifier(device) : false; }\n")
    output.write_text(fixture.replace(marker, static_stubs + callbacks),
                      encoding="utf-8")
    return output


def check_route(source: str, prior_driver: Path) -> None:
    block = runpy.run_path(str(prior_driver))["block"]
    hook = block(source, r"^static HRESULT WINAPI HookCreateDevice\(")
    marker = "InstallD3dStaticDeviceTemplates"
    native = "g_realCreateDevice(self, adapter, type, hwnd, flags, params, outDevice)"
    if marker not in hook or native not in hook or hook.find(marker) > hook.find(native):
        raise SystemExit("static A/B transaction is not before native CreateDevice")
    if "return hr;" not in hook:
        raise SystemExit("native HRESULT preservation route changed")
    if "D3DCREATE_FPU_PRESERVE_FLAG" not in hook:
        raise SystemExit("frozen FPU preserve callback changed")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--build-root", type=Path)
    args = parser.parse_args()
    repo = Path(__file__).resolve().parents[2]
    private = Path(os.environ["LOCALAPPDATA"]) / "V2DLLFork" / "R02w" / "luna-tests"
    build = (args.build_root or private / datetime.datetime.now().strftime(
        "run-%Y%m%d-%H%M%S-%f")).resolve()
    if build == repo or repo in build.parents or build.exists() or \
       private.resolve() not in build.parents:
        raise SystemExit(f"new private build root required under {private}: {build}")
    build.mkdir(parents=True)
    source_path = repo / "Source/V2/V2/V2TechButton.cpp"
    source = source_path.read_text(encoding="utf-8-sig")
    source_hash = sha(source_path)
    check_route(source, repo / "tests/fork/test_d3d_install_transaction.py")
    basis = make_basis(repo, build, source)
    prior_driver = repo / "tests/fork/test_d3d_install_transaction.py"
    surrogate_cpp = build / "d3d_surrogate.cpp"
    surrogate_cpp.write_text(surrogate_source(prior_driver), encoding="ascii")
    surrogate_dll = build / "d3d_surrogate.dll"
    vcvars = Path(r"C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat")
    if not vcvars.is_file():
        raise SystemExit(f"missing vcvarsall: {vcvars}")
    module = repo / "Source/V2/V2/Fork/D3dInstallTransaction.cpp"
    header = repo / "Source/V2/V2/Fork/D3dInstallTransaction.h"
    memory = repo / "Source/V2/V2/Fork/Win32Memory.cpp"
    identity = repo / "Source/V2/V2/Fork/StartupIdentity.cpp"
    fixture = repo / "tests/fork/r02w_static_template_tests.cpp"
    exe = build / "r02w_contract.exe"
    batch = build / "compile.cmd"
    batch.write_text(f'@echo off\ncall "{vcvars}" x86 >nul\n'
                     'if errorlevel 1 exit /b %errorlevel%\n'
                     f'cl /nologo /W4 /WX /O2 /LD /Fe:"{surrogate_dll}" '
                     f'"{surrogate_cpp}" /link /BASE:0x68000000 /DYNAMICBASE:NO '
                     '/MERGE:.vtext=.text /EXPORT:Direct3DCreate9=_FixtureCreate9@4,@37\n'
                     'if errorlevel 1 exit /b %errorlevel%\n'
                     f'cl /nologo /W4 /WX /O2 /EHsc /std:c++17 '
                     f'/I"{repo / "Source/V2/V2/Fork"}" /I"{build}" /Fe:"{exe}" '
                     f'"{fixture}" "{module}" "{memory}" "{identity}" bcrypt.lib\n'
                     'exit /b %errorlevel%\n', encoding="ascii")
    compile_log = build / "compile.log"
    run(["cmd.exe", "/d", "/c", str(batch)], build, compile_log)
    patch_info = patch_template_slots(surrogate_dll)
    (build / "template-layout.txt").write_text("\n".join(
        f"{key}=0x{value:X}" for key, value in patch_info.items()) + "\n",
        encoding="ascii")
    cases = ("success", "aslr", "already", "foreign_retry", "pre_import",
             "pre_create", "prevalidate", "immutable_reject",
             "open_fail", "cas_before", "cas_after", "readback",
             "failed_readback", "foreign_mid", "rollback_close",
             "final_restore", "busy", "copy_a", "copy_b",
             "copy_c", "copy_bad_self", "copy_foreign", "copy_unreadable")
    results = []
    logs = []
    total_checks = 0
    for case in cases:
        log = build / f"runtime-{case}.log"
        output = run([str(exe), str(build), case], build, log)
        result = re.search(r"RESULT checks=(\d+) failures=0", output)
        if not result:
            raise SystemExit(f"missing successful completion for {case}")
        total_checks += int(result.group(1))
        logs.append(log)
        results.append(f"{case}: {output.strip()}")
    if sha(source_path) != source_hash:
        raise SystemExit("main source changed during fixture run")
    summary = build / "summary.md"
    summary.write_text(
        f"# R02w owned static-template transaction fixture\n\n"
        f"- Result: {len(cases)} cases, {total_checks} checks, zero failures; "
        "Release Win32 `/W4 /WX /O2`.\n"
        f"- Current shared source SHA256: `{sha(source_path)}`.\n"
        f"- Current transaction CPP SHA256: `{sha(module)}`; "
        f"header SHA256: `{sha(header)}`.\n"
        "- The privately rebuilt x86 PE32 surrogate has four fixed A/B slots "
        "inside RX image text. Its relocation directory was extended with exact "
        "HIGHLOW entries for those slots; the ASLR case blocked the preferred "
        "base before loading and verified relocated method pointers.\n"
        "- Failure cases use owned mapped pages and injected synchronous "
        "protection/read/CAS callbacks. The returned-device cases use owned "
        "VirtualAlloc memory and the production VQ-gated pointer reader.\n"
        "- Limits: this tests transaction and source callback contracts, not "
        "proprietary-game phase or native D3D COM lifetime. The separate real "
        "provider fixture owns that boundary. No game or debugger was run.\n",
        encoding="utf-8")
    paths = [source_path, module, header, memory, identity,
             fixture, Path(__file__), prior_driver,
             repo / "tests/fork/d3d_install_transaction_tests.cpp",
             basis, surrogate_cpp, surrogate_dll, exe, batch, compile_log,
             build / "template-layout.txt", summary, *logs]
    manifest = build / "hashes.txt"
    manifest.write_text("\n".join(f"{p} sha256={sha(p)}" for p in paths) + "\n",
                        encoding="utf-8")
    print("\n".join(results))
    print(f"TOTAL cases={len(cases)} checks={total_checks} failures=0")
    print(f"ARTIFACT {build}")
    print(f"MANIFEST_SHA256 {sha(manifest)}")


if __name__ == "__main__":
    main()
