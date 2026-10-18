"""Compile unchanged sleep/wait/patch bodies in an isolated Win32 console."""
import argparse
import datetime
import hashlib
import os
from pathlib import Path
import re
import subprocess


SIGNATURES = (
    "static bool PatchImm8Sleep(DWORD rvaPush, unsigned char expectMs, unsigned char newMs, const char* tag)",
    "static void InstallMainLoopSleep0()",
    "static bool ShouldClampSleep(DWORD ms)",
    "static void WINAPI HookSleep(DWORD ms)",
    "static DWORD WINAPI HookWaitForSingleObject(HANDLE h, DWORD ms)",
    "static void MpClientPumpIdle()",
    "static bool InstallMpClientSleep()",
)
R02D_REFERENCE_SHA256 = "af628084fb91f8c2a025d5751ac4b16a1fb632608291e68fc82f5b0a9ca03fbd"


def body(source: str, signature: str) -> str:
    if source.count(signature) != 1:
        raise ValueError(f"missing or ambiguous signature: {signature}")
    start = source.index(signature)
    opening = source.index("{", start + len(signature))
    depth = 0
    state = "code"
    i = opening
    while i < len(source):
        c = source[i]
        following = source[i + 1] if i + 1 < len(source) else ""
        if state == "line":
            if c == "\n":
                state = "code"
        elif state == "block":
            if c == "*" and following == "/":
                state = "code"
                i += 1
        elif state in ("string", "char"):
            if c == "\\":
                i += 1
            elif c == ('"' if state == "string" else "'"):
                state = "code"
        elif c == "/" and following == "/":
            state = "line"
            i += 1
        elif c == "/" and following == "*":
            state = "block"
            i += 1
        elif c == '"':
            state = "string"
        elif c == "'":
            state = "char"
        elif c == "{":
            depth += 1
        elif c == "}":
            depth -= 1
            if depth == 0:
                return source[start:i + 1]
        i += 1
    raise ValueError(f"unterminated body: {signature}")


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def run(command: list[str], directory: Path, log: Path) -> str:
    result = subprocess.run(command, cwd=directory, stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT, text=True, errors="replace", timeout=120)
    log.write_text(result.stdout, encoding="utf-8")
    print(f"{log.name}: exit={result.returncode}")
    if result.returncode:
        print(result.stdout)
        raise SystemExit(result.returncode)
    return result.stdout


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--build-root", type=Path)
    options = parser.parse_args()
    repo = Path(__file__).resolve().parents[2]
    private = Path(os.environ["LOCALAPPDATA"]) / "V2DLLFork" / "R02f" / "sleep-wait-contract"
    build = (options.build_root or private / datetime.datetime.now().strftime("run-%Y%m%d-%H%M%S-%f")).resolve()
    if build == repo or repo in build.parents or build.exists() or private.resolve() not in build.parents:
        raise SystemExit(f"build root must be new and under private directory: {build}")
    build.mkdir(parents=True)
    source_path = repo / "Source/V2/V2/V2TechButton.cpp"
    fixture_path = repo / "tests/fork/sleep_wait_tests.cpp"
    source = source_path.read_text(encoding="utf-8-sig")
    if sha(source_path) != R02D_REFERENCE_SHA256:
        raise SystemExit("current main source differs from R02d frozen reference; review before extracting")
    if len(re.findall(r"^static void HookSleepOnModule\(HMODULE m\)$", source, re.M)) != 1 or source.count("static void HookSleepOnModule(HMODULE m);") != 1:
        raise SystemExit("HookSleepOnModule reachability changed")
    if source.count("static void InstallSleepIatHook()") != 1 or source.count("InstallSleepIatHook();") != 0:
        raise SystemExit("InstallSleepIatHook reachability changed")
    parts = [body(source, signature) for signature in SIGNATURES]
    constants = re.search(
        r"static const DWORD RVA_SLEEP_IAT = 0x88A0EC;\n"
        r"static const DWORD RVA_MAIN_SLEEP_A = 0x5DF2D5;\n"
        r"static const DWORD RVA_MAIN_SLEEP_B = 0x5DF684;", source)
    if not constants:
        raise SystemExit("sleep patch constants changed")
    mp_constant = "static const DWORD RVA_MP_CLIENT_SLEEP = 0x71DD2C;"
    if source.count(mp_constant) != 1:
        raise SystemExit("MP client sleep constant changed")
    injected = constants.group(0) + "\n" + mp_constant + "\n\n" + "\n\n".join(parts)
    fixture = fixture_path.read_text(encoding="utf-8-sig")
    marker = "// ACTUAL_SLEEP_WAIT_BODIES_INJECTED"
    if fixture.count(marker) != 1:
        raise SystemExit("fixture marker missing or ambiguous")
    generated = build / "sleep_wait_actual.cpp"
    generated.write_text(fixture.replace(marker, injected), encoding="utf-8", newline="\n")
    vcvars = Path(r"C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat")
    if not vcvars.is_file():
        raise SystemExit(f"missing vcvarsall: {vcvars}")
    exe = build / "sleep_wait_actual.exe"
    batch = build / "compile.cmd"
    batch.write_text(
        f'@echo off\ncall "{vcvars}" x86 >nul\n'
        f'if errorlevel 1 exit /b %errorlevel%\n'
        f'cl /nologo /W4 /WX /O2 /EHsc /std:c++17 '
        f'/Fe:"{exe}" "{generated}"\nexit /b %errorlevel%\n', encoding="ascii")
    compile_log = build / "compile.log"
    runtime_log = build / "runtime.log"
    run(["cmd.exe", "/d", "/c", str(batch)], build, compile_log)
    output = run([str(exe)], build, runtime_log)
    if not re.search(r"RESULT checks=\d+ failures=0\b", output):
        raise SystemExit("missing successful contract result")
    manifest = build / "hashes.txt"
    project_path = repo / "Source/V2/V2/V2.vcxproj"
    paths = [source_path, project_path, fixture_path, Path(__file__), generated, batch, compile_log, runtime_log, exe]
    manifest.write_text("\n".join(f"{path.name} sha256={sha(path)}" for path in paths) + "\n"
                        + "\n".join(f"body[{i}] sha256={hashlib.sha256(part.encode('utf-8')).hexdigest()}"
                                     for i, part in enumerate(parts)) + "\n",
                        encoding="utf-8")
    print(output, end="")
    print(f"ARTIFACT {build}")
    print(f"MANIFEST_SHA256 {sha(manifest)}")


if __name__ == "__main__":
    main()
