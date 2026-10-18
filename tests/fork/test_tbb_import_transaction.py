"""Exercise the current late TBB import transaction on owned Win32 memory."""

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
    private = Path(os.environ["LOCALAPPDATA"]) / "V2DLLFork" / "R02s" / "luna-test"
    build = (args.build_root or private / datetime.datetime.now().strftime("run-%Y%m%d-%H%M%S-%f")).resolve()
    if build == repo or repo in build.parents or build.exists() or private.resolve() not in build.parents:
        raise SystemExit(f"new private build root required under {private}: {build}")
    build.mkdir(parents=True)
    source_path = repo / "Source/V2/V2/V2TechButton.cpp"
    header = repo / "Source/V2/V2/Fork/TbbImportTransaction.h"
    module = repo / "Source/V2/V2/Fork/TbbImportTransaction.cpp"
    memory = repo / "Source/V2/V2/Fork/Win32Memory.cpp"
    identity = repo / "Source/V2/V2/Fork/StartupIdentity.cpp"
    fixture = repo / "tests/fork/tbb_import_transaction_tests.cpp"
    provider_path = Path(os.environ["WINDIR"]) / "SysWOW64/msvcr100.dll"
    provider_hash = "8793353461826fbd48f25ea8b835be204b758ce7510db2af631b28850355bd18"
    if not provider_path.is_file() or sha(provider_path) != provider_hash:
        raise SystemExit(f"pinned x86 MSVCR100 provider absent or changed: {provider_path}")
    if not header.is_file() or not module.is_file():
        raise SystemExit("TBB transaction module has not landed")
    source_hash = sha(source_path)
    source = source_path.read_text(encoding="utf-8-sig")
    late = block(source, r"^static bool TryPatchLateModules\(\)\s*$")
    if not (late.find("InstallExeThreadIatQuartet()") < late.find('GetModuleHandleA("tbb.dll")')):
        raise SystemExit("late EXE quartet/TBB discovery order changed")
    if not re.search(r'HMODULE tbb = GetModuleHandleA\("tbb\.dll"\);\s*'
                     r'if \(tbb\)\s*\{\s*if \(!HookTbbModule\(tbb\)\)\s*'
                     r'return false;\s*HookTbbInitOnExe\(\);\s*\}\s*return true;', late):
        raise SystemExit("active TBB failure must abort; absent TBB must defer successfully")
    tbb_hook = block(source, r"^static bool HookTbbModule\(HMODULE tbb\)\s*$")
    named_guard = re.search(r'if \(GetModuleHandleA\("tbb\.dll"\) != tbb\)\s*return true;', tbb_hook)
    if not named_guard or named_guard.end() > tbb_hook.find("g_tbbBeginThreadImport.Run("):
        raise SystemExit("broad-name TBB candidate could consume transaction reference")
    binding = block(source, r"^static bool ActiveExeTbbInitializeBoundTo\(HMODULE tbb\)\s*$")
    required_binding = (
        "kTbbInitializeIatRva = 0x88A57C",
        "kTbbInitializeExportRva = 0xF170",
        "kTbbInitializeIatRva > g_imageSize - sizeof(DWORD)",
        "base > 0xFFFFFFFFu - kTbbInitializeExportRva",
        "ReadableExePatchSpan(kTbbInitializeIatRva, sizeof(DWORD))",
        "*(volatile DWORD*)(DWORD_PTR)(g_base + kTbbInitializeIatRva) == expected",
    )
    if any(piece not in binding for piece in required_binding) or \
       "GetProcAddress(" in binding or \
       not re.search(r'__try\s*\{[^{}]*return .*== expected;\s*\}\s*'
                     r'__except \(EXCEPTION_EXECUTE_HANDLER\)\s*\{\s*return false;', binding):
        raise SystemExit("active EXE TBB initialize import owner validation changed")
    owner_guard = re.search(r'if \(!ActiveExeTbbInitializeBoundTo\(tbb\)\)\s*'
                            r'\{[^{}]*return false;\s*\}', tbb_hook)
    if not owner_guard or owner_guard.end() > tbb_hook.find("g_tbbBeginThreadImport.Run("):
        raise SystemExit("EXE initialize-import mismatch does not fail closed before transaction")
    cap_guard = re.search(r'if \(g_settings\.engineWorkerThreads != 0\)\s*'
                          r'\{[^{}]*return false;\s*\}', tbb_hook)
    disabled_guard = re.search(r'if \(!g_settings\.patchThreadFpuPin\)\s*return true;', tbb_hook)
    if not cap_guard or not disabled_guard or not (
            named_guard.end() < cap_guard.start() < disabled_guard.start() <
            owner_guard.start() < tbb_hook.find("g_tbbBeginThreadImport.Run(")):
        raise SystemExit("TBB named/cap/disabled/owner/transaction order changed")
    if not re.search(r'if \(report\.state == fork_boundary::TbbIatState::Applied \|\|\s*'
                     r'report\.state == fork_boundary::TbbIatState::AlreadyInstalled\)', tbb_hook):
        raise SystemExit("active TBB success recognition changed")
    if tbb_hook.rfind("return false;") < tbb_hook.find("g_tbbBeginThreadImport.Run("):
        raise SystemExit("active TBB failure no longer returns false")
    for suffix in ("A", "W"):
        loader = block(source, rf"^static HMODULE WINAPI HookLoadLibrary{suffix}\(LPC(?:WSTR|STR) name\)\s*$")
        if not re.search(rf'HMODULE m = g_realLoadLibrary{suffix} \? '
                         rf'g_realLoadLibrary{suffix}\(name\) : 0;\s*'
                         rf'if \(m && NameHasTbb{suffix}\(name\)\)\s*'
                         r'HookTbbModule\(m\);\s*return m;', loader):
            raise SystemExit(f"HookLoadLibrary{suffix} must preserve successful HMODULE")
    engine = block(source, r"^static bool InstallEngineStability\(\)\s*$")
    if not re.search(r'if \(!TryPatchLateModules\(\)\)\s*\{[^{}]*return false;', engine):
        raise SystemExit("InstallEngineStability no longer propagates late failure")
    cap = block(source, r"^static void HookTbbInitOnExe\(\)\s*$")
    if "if (g_settings.engineWorkerThreads < 1)" not in cap or \
       cap.find("if (g_settings.engineWorkerThreads < 1)") > cap.find('GetModuleHandleA("tbb.dll")'):
        raise SystemExit("default zero-cap TBB initialize no-op changed")
    if "engineWorkerThreads = 0" not in source:
        raise SystemExit("compiled zero-cap default changed")

    vcvars = Path(r"C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat")
    if not vcvars.is_file(): raise SystemExit(f"missing vcvarsall: {vcvars}")
    exe = build / "contract.exe"
    surrogate_source = build / "surrogate.cpp"
    surrogate_dll = build / "surrogate.dll"
    surrogate_source.write_text('extern "C" __declspec(dllexport) int __stdcall RetentionProbe() { return 7; }\n',
                                encoding="ascii")
    batch = build / "compile.cmd"
    include = repo / "Source/V2/V2/Fork"
    batch.write_text(f'@echo off\ncall "{vcvars}" x86 >nul\nif errorlevel 1 exit /b %errorlevel%\n'
                     f'cl /nologo /W4 /WX /O2 /LD /Fe:"{surrogate_dll}" "{surrogate_source}"\n'
                     f'if errorlevel 1 exit /b %errorlevel%\n'
                     f'cl /nologo /W4 /WX /O2 /EHsc /std:c++17 /I"{include}" /Fe:"{exe}" '
                     f'"{fixture}" "{module}" "{memory}" "{identity}" bcrypt.lib\n'
                     'exit /b %errorlevel%\n', encoding="ascii")
    compile_log, runtime_log = build / "compile.log", build / "runtime.log"
    run(["cmd.exe", "/d", "/c", str(batch)], build, compile_log)
    output = run([str(exe), str(build)], build, runtime_log)
    if not re.search(r"RESULT checks=\d+ failures=0 active=1 skipped=1 cap=0", output):
        raise SystemExit("missing complete successful test result")
    if sha(source_path) != source_hash: raise SystemExit("main source changed during test")
    paths = [source_path, header, module, memory, identity, fixture, Path(__file__), provider_path,
             surrogate_source, surrogate_dll, batch, compile_log, runtime_log, exe]
    manifest = build / "hashes.txt"
    manifest.write_text("\n".join(f"{path} sha256={sha(path)}" for path in paths) + "\n"
                        + f"late sha256={hashlib.sha256(late.encode('utf-8')).hexdigest()}\n"
                        + f"cap sha256={hashlib.sha256(cap.encode('utf-8')).hexdigest()}\n"
                        + f"tbb_hook sha256={hashlib.sha256(tbb_hook.encode('utf-8')).hexdigest()}\n"
                        + f"binding sha256={hashlib.sha256(binding.encode('utf-8')).hexdigest()}\n"
                        + f"engine sha256={hashlib.sha256(engine.encode('utf-8')).hexdigest()}\n",
                        encoding="utf-8")
    print(output, end="")
    print(f"ARTIFACT {build}")
    print(f"MANIFEST_SHA256 {sha(manifest)}")


if __name__ == "__main__":
    main()
