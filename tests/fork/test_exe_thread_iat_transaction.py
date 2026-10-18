"""Build the current EXE thread-IAT quartet module against an owned PE32 image."""

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
    matches = list(re.finditer(regex, source, re.M))
    if len(matches) != 1: raise ValueError(f"expected one {regex}: {len(matches)}")
    start = matches[0].start()
    opening = source.find("{", matches[0].end())
    if opening < 0: raise ValueError(f"missing body {regex}")
    depth, pos, state = 0, opening, "code"
    while pos < len(source):
        char = source[pos]
        ahead = source[pos + 1] if pos + 1 < len(source) else ""
        if state == "code":
            if char == "/" and ahead == "/": state = "line"; pos += 2; continue
            if char == "/" and ahead == "*": state = "block"; pos += 2; continue
            if char == '"': state = "string"
            elif char == "'": state = "char"
            elif char == "{": depth += 1
            elif char == "}":
                depth -= 1
                if depth == 0: return source[start:pos + 1]
        elif state == "line":
            if char == "\n": state = "code"
        elif state == "block":
            if char == "*" and ahead == "/": state = "code"; pos += 2; continue
        else:
            if char == "\\": pos += 2; continue
            if char == ('"' if state == "string" else "'"): state = "code"
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
    private = Path(os.environ["LOCALAPPDATA"]) / "V2DLLFork" / "R02r" / "luna-test"
    build = (args.build_root or private / datetime.datetime.now().strftime("run-%Y%m%d-%H%M%S-%f")).resolve()
    if build == repo or repo in build.parents or build.exists() or private.resolve() not in build.parents:
        raise SystemExit(f"new private build root required under {private}: {build}")
    build.mkdir(parents=True)

    source_path = repo / "Source/V2/V2/V2TechButton.cpp"
    header = repo / "Source/V2/V2/Fork/ExeThreadIatTransaction.h"
    module = repo / "Source/V2/V2/Fork/ExeThreadIatTransaction.cpp"
    memory = repo / "Source/V2/V2/Fork/Win32Memory.cpp"
    fixture = repo / "tests/fork/exe_thread_iat_transaction_tests.cpp"
    if not module.is_file(): raise SystemExit("quartet module has not landed")
    source_hash = sha(source_path)
    source = source_path.read_text(encoding="utf-8-sig")
    install = block(source, r"^static bool Install\(\)\s*$")
    order = ("LoadSettings();", "if (!PreflightExePatches())",
             "if (!InstallEngineStability())", "InstallExePatches()")
    positions = [install.find(item) for item in order]
    if any(pos < 0 for pos in positions) or positions != sorted(positions):
        raise SystemExit(f"Install source order changed: {positions}")
    abort_segment = install[positions[2]:install.find("InstallArmySelectDiag();")]
    if "return false;" not in abort_segment:
        raise SystemExit("Install does not abort after stability failure")
    dllmain = block(source, r"^BOOL APIENTRY DllMain\(HMODULE hModule, DWORD reason, LPVOID reserved\)\s*$")
    if "InterlockedExchange(&g_dllReady, ok ? 1 : 0);" not in dllmain:
        raise SystemExit("DllMain readiness result propagation changed")
    publish = block(source, r"^static void PublishExeThreadOriginals\(")
    originals = ("g_realCreateThread", "g_realLoadLibraryA", "g_realLoadLibraryW", "g_realGetTickCount")
    for index, name in enumerate(originals):
        pairing = (r"InterlockedExchangePointer\(\(PVOID volatile\*\)&" + name +
                   rf",\s*\(PVOID\)\(DWORD_PTR\)original\[{index}\]\);")
        if not re.search(pairing, publish):
            raise SystemExit(f"source original publication changed: {name}")
    quartet = block(source, r"^static bool InstallExeThreadIatQuartet\(\)\s*$")
    quartet_order = tuple(quartet.find(f"&Hook{name}") for name in
                          ("CreateThread", "LoadLibraryA", "LoadLibraryW", "GetTickCount"))
    if any(pos < 0 for pos in quartet_order) or quartet_order != tuple(sorted(quartet_order)) or \
       "g_exeThreadIatTransaction.Run(g_imageSize, hooks, backend)" not in quartet:
        raise SystemExit(f"source quartet hook order/transaction call changed: {quartet_order}")
    stability = block(source, r"^static bool InstallEngineStability\(\)\s*$")
    if not (stability.find("InstallExeThreadIatQuartet()") <
            stability.find("InstallMainLoopFpuPin();") < stability.find("if (!TryPatchLateModules())")) or \
       "return false;" not in stability[stability.find("InstallExeThreadIatQuartet()"):stability.find("InstallMainLoopFpuPin();")]:
        raise SystemExit("stability quartet abort/order changed")
    late = block(source, r"^static bool TryPatchLateModules\(\)\s*$")
    if not (late.find("if (!InstallExeThreadIatQuartet())") <
            late.find('GetModuleHandleA("d3d9.dll")') < late.find('GetModuleHandleA("tbb.dll")')) or \
       "return false;" not in late[late.find("if (!InstallExeThreadIatQuartet())"):late.find('GetModuleHandleA("d3d9.dll")')]:
        raise SystemExit("late quartet deferral/order changed")

    vcvars = Path(r"C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat")
    if not vcvars.is_file(): raise SystemExit(f"missing vcvarsall: {vcvars}")
    exe = build / "contract.exe"
    batch = build / "compile.cmd"
    include = repo / "Source/V2/V2/Fork"
    batch.write_text(f'@echo off\ncall "{vcvars}" x86 >nul\nif errorlevel 1 exit /b %errorlevel%\n'
                     f'cl /nologo /W4 /WX /O2 /EHsc /std:c++17 /I"{include}" /Fe:"{exe}" '
                     f'"{fixture}" "{module}" "{memory}"\nexit /b %errorlevel%\n', encoding="ascii")
    compile_log, runtime_log = build / "compile.log", build / "runtime.log"
    run(["cmd.exe", "/d", "/c", str(batch)], build, compile_log)
    output = run([str(exe)], build, runtime_log)
    if not re.search(r"RESULT checks=\d+ failures=0 slots=4", output):
        raise SystemExit("missing complete successful test result")
    if sha(source_path) != source_hash: raise SystemExit("main source changed during test")
    paths = [source_path, header, module, memory, fixture, Path(__file__), batch,
             compile_log, runtime_log, exe]
    manifest = build / "hashes.txt"
    manifest.write_text("\n".join(f"{path} sha256={sha(path)}" for path in paths) + "\n"
                        + f"Install order offsets={positions}\n"
                        + f"quartet hook offsets={quartet_order}\n"
                        + f"publish sha256={hashlib.sha256(publish.encode('utf-8')).hexdigest()}\n"
                        + f"quartet sha256={hashlib.sha256(quartet.encode('utf-8')).hexdigest()}\n"
                        + f"stability sha256={hashlib.sha256(stability.encode('utf-8')).hexdigest()}\n"
                        + f"late sha256={hashlib.sha256(late.encode('utf-8')).hexdigest()}\n",
                        encoding="utf-8")
    print(output, end="")
    print(f"ARTIFACT {build}")
    print(f"MANIFEST_SHA256 {sha(manifest)}")


if __name__ == "__main__":
    main()
