"""Extract and compile the actual HookIat body for an owned Win32 console test."""
import argparse
import datetime
import hashlib
import os
from pathlib import Path
import re
import subprocess


def extract(source: str) -> str:
    signature = "static bool HookIat(HMODULE module, const char* dllName, const char* funcName, void* hook, void** orig)"
    if source.count(signature) != 1:
        raise SystemExit("HookIat signature is missing or ambiguous")
    begin = source.index(signature)
    brace = source.index("{", begin + len(signature))
    depth = 0
    mode = "code"
    i = brace
    while i < len(source):
        ch = source[i]
        nxt = source[i + 1] if i + 1 < len(source) else ""
        if mode == "line":
            if ch == "\n":
                mode = "code"
        elif mode == "block":
            if ch == "*" and nxt == "/":
                mode = "code"
                i += 1
        elif mode == "string":
            if ch == "\\":
                i += 1
            elif ch == '"':
                mode = "code"
        elif mode == "char":
            if ch == "\\":
                i += 1
            elif ch == "'":
                mode = "code"
        elif ch == "/" and nxt == "/":
            mode = "line"
            i += 1
        elif ch == "/" and nxt == "*":
            mode = "block"
            i += 1
        elif ch == '"':
            mode = "string"
        elif ch == "'":
            mode = "char"
        elif ch == "{":
            depth += 1
        elif ch == "}":
            depth -= 1
            if depth == 0:
                return source[begin:i + 1]
        i += 1
    raise SystemExit("HookIat body is unterminated")


def run(args: list[str], cwd: Path, log: Path) -> None:
    result = subprocess.run(args, cwd=cwd, stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT, text=True, errors="replace", timeout=90)
    log.write_text(result.stdout, encoding="utf-8")
    print(f"{log.name}: exit={result.returncode}")
    if result.returncode:
        print(result.stdout)
        raise SystemExit(result.returncode)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--build-root", type=Path)
    options = parser.parse_args()
    repo = Path(__file__).resolve().parents[2]
    private = Path(os.environ["LOCALAPPDATA"]) / "V2DLLFork" / "R02d" / "win32-contract"
    build = options.build_root or private / datetime.datetime.now().strftime("run-%Y%m%d-%H%M%S")
    build = build.resolve()
    if repo in build.parents or build == repo or build.exists():
        raise SystemExit(f"build root must be new and outside repository: {build}")
    if private.resolve() not in build.parents:
        raise SystemExit(f"build root must be under private contract directory: {build}")
    build.mkdir(parents=True)
    source_path = repo / "Source/V2/V2/V2TechButton.cpp"
    fixture_path = repo / "tests/fork/m0_win32_tests.cpp"
    source_bytes = source_path.read_bytes()
    body = extract(source_bytes.decode("utf-8-sig"))
    fixture = fixture_path.read_text(encoding="utf-8-sig")
    marker = "// ACTUAL_HOOKIAT_INJECTED"
    if fixture.count(marker) != 1:
        raise SystemExit("fixture marker is missing or ambiguous")
    generated = fixture.replace(marker, body)
    generated_path = build / "m0_actual_hookiat.cpp"
    generated_path.write_text(generated, encoding="utf-8")
    (build / "source_hashes.txt").write_text(
        f"V2TechButton.cpp sha256={hashlib.sha256(source_bytes).hexdigest()}\n"
        f"HookIat extracted utf8 sha256={hashlib.sha256(body.encode('utf-8')).hexdigest()}\n"
        f"m0_win32_tests.cpp sha256={hashlib.sha256(fixture_path.read_bytes()).hexdigest()}\n"
        f"test_m0_win32.py sha256={hashlib.sha256(Path(__file__).read_bytes()).hexdigest()}\n"
        f"generated sha256={hashlib.sha256(generated_path.read_bytes()).hexdigest()}\n",
        encoding="utf-8")
    vcvars = Path(r"C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat")
    if not vcvars.is_file():
        raise SystemExit(f"missing vcvarsall: {vcvars}")
    exe = build / "m0_actual_hookiat.exe"
    batch = build / "compile.cmd"
    batch.write_text(
        f'@echo off\ncall "{vcvars}" x86 >nul\n'
        f'if errorlevel 1 exit /b %errorlevel%\n'
        f'cl /nologo /W4 /WX /O2 /EHsc /std:c++17 '
        f'/Fe:"{exe}" "{generated_path}"\n'
        f'exit /b %errorlevel%\n', encoding="ascii")
    run(["cmd.exe", "/d", "/c", str(batch)], build, build / "compile.log")
    run([str(exe)], build, build / "runtime.log")
    result = (build / "runtime.log").read_text(encoding="utf-8")
    if not re.search(r"RESULT checks=\d+ failures=0\b", result):
        raise SystemExit("missing successful contract count")
    print(result, end="")
    print(f"ARTIFACT {build}")


if __name__ == "__main__":
    main()
