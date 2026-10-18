"""Compile the current D3D9 installer against owned Win32 image and COM-layout fixtures."""

import argparse
import datetime
import hashlib
import os
from pathlib import Path
import re
import subprocess


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def block(source: str, regex: str) -> str:
    definitions = []
    for match in re.finditer(regex, source, re.M):
        opening = source.find("{", match.end())
        semicolon = source.find(";", match.end())
        if opening >= 0 and (semicolon < 0 or opening < semicolon):
            definitions.append((match.start(), opening))
    if len(definitions) != 1:
        raise ValueError(f"expected one definition {regex}: {len(definitions)}")
    start, opening = definitions[0]
    depth, pos, state = 0, opening, "code"
    while pos < len(source):
        char = source[pos]
        ahead = source[pos + 1] if pos + 1 < len(source) else ""
        if state == "code":
            if char == "/" and ahead == "/":
                state = "line"; pos += 2; continue
            if char == "/" and ahead == "*":
                state = "block"; pos += 2; continue
            if char == '"':
                state = "string"
            elif char == "'":
                state = "char"
            elif char == "{":
                depth += 1
            elif char == "}":
                depth -= 1
                if depth == 0:
                    return source[start:pos + 1]
        elif state == "line":
            if char == "\n":
                state = "code"
        elif state == "block":
            if char == "*" and ahead == "/":
                state = "code"; pos += 2; continue
        else:
            if char == "\\":
                pos += 2; continue
            if char == ('"' if state == "string" else "'"):
                state = "code"
        pos += 1
    raise ValueError(f"unterminated body {regex}")


def run(command: list[str], cwd: Path, log: Path) -> str:
    result = subprocess.run(command, cwd=cwd, stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT, text=True, errors="replace", timeout=180)
    log.write_text(result.stdout, encoding="utf-8")
    if result.returncode:
        print(result.stdout)
        raise SystemExit(f"{log.name} failed with exit {result.returncode}")
    return result.stdout


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--build-root", type=Path)
    args = parser.parse_args()
    repo = Path(__file__).resolve().parents[2]
    private = Path(os.environ["LOCALAPPDATA"]) / "V2DLLFork" / "R02t" / "luna-test"
    build = (args.build_root or private / datetime.datetime.now().strftime("run-%Y%m%d-%H%M%S-%f")).resolve()
    r02w_private = Path(os.environ["LOCALAPPDATA"]) / "V2DLLFork" / "R02w" / "luna-tests"
    if build == repo or repo in build.parents or build.exists() or not any(
        allowed.resolve() in build.parents for allowed in (private, r02w_private)
    ):
        raise SystemExit(f"new private build root required under {private} or {r02w_private}: {build}")
    build.mkdir(parents=True)

    source_path = repo / "Source/V2/V2/V2TechButton.cpp"
    fixture = repo / "tests/fork/d3d_install_transaction_tests.cpp"
    provider_path = Path(os.environ["WINDIR"]) / "SysWOW64/d3d9.dll"
    provider_hash = "fd72e021c128a98a5d688eb9acc07e3eb1948ca6b630b9fa5f328898e78c7dbc"
    if not provider_path.is_file() or sha(provider_path) != provider_hash:
        raise SystemExit(f"pinned x86 d3d9 provider absent or changed: {provider_path}")
    source_hash = sha(source_path)
    source = source_path.read_text(encoding="utf-8-sig")
    for text in ("D3DCREATE_FPU_PRESERVE_FLAG = 0x00000002",
                 "D3DPRESENT_INTERVAL_OFF = 52", "D3D9_VT_CREATEDEVICE = 16",
                 "D3D9DEV_VT_RESET = 16", "D3D9DEV_VT_PRESENT = 17"):
        if text not in source:
            raise SystemExit(f"frozen D3D source control changed: {text}")
    for pattern in (r"\bbool\s+patchD3dFpuPreserve\s*=\s*true\s*;",
                    r"\bbool\s+patchD3dNoVsync\s*=\s*false\s*;"):
        if not re.search(pattern, source):
            raise SystemExit(f"frozen D3D default changed: {pattern}")
    late = block(source, r"^static bool TryPatchLateModules\(\)\s*$")
    if not (late.find("InstallExeThreadIatQuartet()") <
            late.find('GetModuleHandleA("d3d9.dll")') <
            late.find('GetModuleHandleA("tbb.dll")')):
        raise SystemExit("late EXE/D3D/TBB install order changed")
    if not re.search(r'if \(d3d9 && !InstallD3dCreate9Import\(d3d9\)\)\s*return false;', late):
        raise SystemExit("active D3D failure must abort; absent provider must defer")
    import_route = block(source, r"^static bool InstallD3dCreate9Import\(HMODULE provider\)\s*$")
    if "ReadableExePatchSpan(fork_boundary::kPinnedD3dCreate9IatRva, sizeof(DWORD))" not in import_route or \
       not re.search(r'backend\.exeMemory\.read\(\s*'
                     r'fork_boundary::kPinnedD3dCreate9IatRva,\s*'
                     r'reinterpret_cast<unsigned char\*>\(&current\),\s*sizeof\(current\)\)',
                     import_route):
        raise SystemExit("D3D current EXE import must be read by bounded RVA")
    if import_route.find("backend.exeMemory.read(") > import_route.find(
            "g_d3dInstallTransaction.InstallExeImport("):
        raise SystemExit("D3D EXE import transaction must use sampled active DWORD")
    result_route = block(source, r"^static bool D3dInstallResultAvailable\(")
    if not re.search(r'Busy && mayDeferBusy\)\s*return false;', result_route) or \
       result_route.find("g_d3dInstallUnavailable, 1") < result_route.find(
           "Busy && mayDeferBusy"):
        raise SystemExit("only initial D3D import Busy may defer")
    object_route = block(source, r"^static void\* WINAPI HookDirect3DCreate9\(UINT sdk\)\s*$")
    device_route = block(source, r"^static HRESULT WINAPI HookCreateDevice\(")
    if not (object_route.find("g_realDirect3DCreate9(sdk)") <
            object_route.find("InstallD3dCreateDeviceTable(obj)") <
            object_route.rfind("return obj;")):
        raise SystemExit("successful native Direct3DCreate9 object not preserved")
    if not (device_route.find("InstallD3dStaticDeviceTemplates()") <
            device_route.find("g_realCreateDevice(self, adapter, type, hwnd, flags, params, outDevice)") <
            device_route.find("VerifyD3dStaticDeviceCopy(*outDevice)") <
            device_route.rfind("return hr;")):
        raise SystemExit("static template install must precede native CreateDevice; returned-copy check must follow successful result")
    if not re.search(r'if \(templatesReady && hr >= 0 && outDevice && \*outDevice\)',
                     device_route):
        raise SystemExit("returned-copy check must be gated by native success and object")

    vcvars = Path(r"C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat")
    if not vcvars.is_file():
        raise SystemExit(f"missing vcvarsall: {vcvars}")
    module = repo / "Source/V2/V2/Fork/D3dInstallTransaction.cpp"
    header = repo / "Source/V2/V2/Fork/D3dInstallTransaction.h"
    if not module.is_file() or not header.is_file():
        raise SystemExit("D3D9 transaction module has not landed")
    memory = repo / "Source/V2/V2/Fork/Win32Memory.cpp"
    identity = repo / "Source/V2/V2/Fork/StartupIdentity.cpp"
    exe = build / "contract.exe"
    callback_fixture = build / "d3d_actual_callbacks.cpp"
    marker = "// ACTUAL_D3D_CALLBACKS"
    fixture_text = fixture.read_text(encoding="utf-8")
    if fixture_text.count(marker) != 1:
        raise SystemExit("actual callback extraction marker missing or ambiguous")
    callback_names = (
        r"^static void ForceImmediatePresent\(void\* params\)\s*$",
        r"^static HRESULT WINAPI HookCreateDevice\(",
        r"^static void\* WINAPI HookDirect3DCreate9\(UINT sdk\)\s*$",
    )
    callbacks = "\n\n".join(block(source, pattern) for pattern in callback_names)
    fixture_text = fixture_text.replace(
        "static bool InstallD3dDeviceTablePair(void* device)",
        "[[maybe_unused]] static bool InstallD3dDeviceTablePair(void* device)")
    static_stubs = (
        "static bool InstallD3dStaticDeviceTemplates() { ++staticInstallCalls; "
        "return staticInstaller ? staticInstaller() : false; }\n"
        "static bool VerifyD3dStaticDeviceCopy(void* device) { ++verifyCopyCalls; "
        "return copyVerifier ? copyVerifier(device) : false; }\n")
    callback_fixture.write_text(fixture_text.replace(marker, static_stubs + callbacks),
                                encoding="utf-8")
    surrogate_source = build / "d3d_surrogate.cpp"
    surrogate_dll = build / "d3d_surrogate.dll"
    surrogate_source.write_text(r'''
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
extern "C" __declspec(dllexport) void* WINAPI FixtureCreate9(unsigned) { return nullptr; }
extern "C" __declspec(dllexport) HRESULT WINAPI FixtureCreateDevice(
    void*, unsigned, unsigned, HWND, DWORD, void*, void**) { return S_FALSE; }
extern "C" __declspec(dllexport) HRESULT WINAPI FixtureReset(void*, void*) { return S_FALSE; }
extern "C" __declspec(dllexport) HRESULT WINAPI FixturePresent(
    void*, const void*, const void*, HWND, const void*) { return S_FALSE; }
extern "C" __declspec(dllexport) HRESULT WINAPI FixturePresentAlternate(
    void*, const void*, const void*, HWND, const void*) { return E_FAIL; }
extern "C" __declspec(dllexport) INT_PTR WINAPI FixtureOtherMethod() { return 0; }
using Method = INT_PTR (WINAPI*)();
#pragma section(".vtext", execute, read)
#define OTHER reinterpret_cast<Method>(&FixtureOtherMethod)
extern "C" __declspec(dllexport) __declspec(allocate(".vtext"))
Method const FixtureD3dTable[17] = {
    OTHER, OTHER, OTHER, OTHER, OTHER, OTHER, OTHER, OTHER,
    OTHER, OTHER, OTHER, OTHER, OTHER, OTHER, OTHER, OTHER,
    reinterpret_cast<Method>(&FixtureCreateDevice)
};
extern "C" __declspec(dllexport) __declspec(allocate(".vtext"))
Method const FixtureD3dTableSame[17] = {
    OTHER, OTHER, OTHER, OTHER, OTHER, OTHER, OTHER, OTHER,
    OTHER, OTHER, OTHER, OTHER, OTHER, OTHER, OTHER, OTHER,
    reinterpret_cast<Method>(&FixtureCreateDevice)
};
extern "C" __declspec(dllexport) __declspec(allocate(".vtext"))
Method const FixtureDeviceTable[18] = {
    OTHER, OTHER, OTHER, OTHER, OTHER, OTHER, OTHER, OTHER,
    OTHER, OTHER, OTHER, OTHER, OTHER, OTHER, OTHER, OTHER,
    reinterpret_cast<Method>(&FixtureReset),
    reinterpret_cast<Method>(&FixturePresent)
};
extern "C" __declspec(dllexport) __declspec(allocate(".vtext"))
Method const FixtureDeviceTableSame[18] = {
    OTHER, OTHER, OTHER, OTHER, OTHER, OTHER, OTHER, OTHER,
    OTHER, OTHER, OTHER, OTHER, OTHER, OTHER, OTHER, OTHER,
    reinterpret_cast<Method>(&FixtureReset),
    reinterpret_cast<Method>(&FixturePresent)
};
extern "C" __declspec(dllexport) __declspec(allocate(".vtext"))
Method const FixtureDeviceTableDifferent[18] = {
    OTHER, OTHER, OTHER, OTHER, OTHER, OTHER, OTHER, OTHER,
    OTHER, OTHER, OTHER, OTHER, OTHER, OTHER, OTHER, OTHER,
    reinterpret_cast<Method>(&FixtureReset),
    reinterpret_cast<Method>(&FixturePresentAlternate)
};
extern "C" __declspec(dllexport) Method FixtureWritableDeviceTable[18] = {
    OTHER, OTHER, OTHER, OTHER, OTHER, OTHER, OTHER, OTHER,
    OTHER, OTHER, OTHER, OTHER, OTHER, OTHER, OTHER, OTHER,
    reinterpret_cast<Method>(&FixtureReset),
    reinterpret_cast<Method>(&FixturePresent)
};
'''.lstrip(), encoding="ascii")
    include = repo / "Source/V2/V2/Fork"
    batch = build / "compile.cmd"
    batch.write_text(f'@echo off\ncall "{vcvars}" x86 >nul\nif errorlevel 1 exit /b %errorlevel%\n'
                     f'cl /nologo /W4 /WX /O2 /LD /Fe:"{surrogate_dll}" "{surrogate_source}" '
                     '/link /BASE:0x68000000 /DYNAMICBASE:NO /MERGE:.vtext=.text '
                     '/EXPORT:Direct3DCreate9=_FixtureCreate9@4,@37\n'
                     'if errorlevel 1 exit /b %errorlevel%\n'
                     f'cl /nologo /W4 /WX /O2 /EHsc /std:c++17 /I"{include}" /Fe:"{exe}" '
                     f'"{callback_fixture}" "{module}" "{memory}" "{identity}" bcrypt.lib\n'
                     'exit /b %errorlevel%\n', encoding="ascii")
    compile_log = build / "compile.log"
    run(["cmd.exe", "/d", "/c", str(batch)], build, compile_log)
    cases = ("success", "aslr_success", "exe_recovery_same", "exe_recovery_different",
             "exe_unrelated_import", "exe_foreign", "exe_bad_import", "exe_retry_foreign",
             "create_heap", "create_data", "create_tuple_after_rollback",
             "pair_second_fail", "pair_after_swap", "pair_tuple_after_rollback",
             "pair_readback_fail", "pair_final_restore", "pair_rollback_close",
             "pair_begin_second", "pair_rollback_cas", "pair_foreign_mid_cas",
             "tuple_second",
             "owned_clobber", "busy_import", "busy_object", "callback_preserve")
    runtime_logs: list[Path] = []
    outputs: list[str] = []
    for case in cases:
        runtime_log = build / f"runtime-{case}.log"
        output = run([str(exe), str(build), case], build, runtime_log)
        if not re.search(r"RESULT checks=\d+ failures=0", output):
            raise SystemExit(f"missing complete successful {case} result")
        runtime_logs.append(runtime_log)
        outputs.append(f"{case}: {output.strip()}")
    if sha(source_path) != source_hash:
        raise SystemExit("main source changed during test")
    paths = [source_path, header, module, memory, identity, fixture,
             callback_fixture, Path(__file__),
             provider_path, surrogate_source, surrogate_dll, batch, compile_log, exe,
             *runtime_logs]
    manifest = build / "hashes.txt"
    manifest.write_text("\n".join(f"{path} sha256={sha(path)}" for path in paths) + "\n",
                        encoding="utf-8")
    print("\n".join(outputs))
    print(f"ARTIFACT {build}")
    print(f"MANIFEST_SHA256 {sha(manifest)}")


if __name__ == "__main__":
    main()
