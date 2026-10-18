"""Compile and exercise the current EXE_PATCHES preflight in an owned Win32 fixture."""

import argparse
import datetime
import hashlib
import os
from pathlib import Path
import re
import subprocess


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def brace_end(source: str, opening: int) -> int:
    depth = 0
    mode = "code"
    index = opening
    while index < len(source):
        char = source[index]
        next_char = source[index + 1] if index + 1 < len(source) else ""
        if mode == "code":
            if char == "/" and next_char == "/":
                mode = "line"; index += 2; continue
            if char == "/" and next_char == "*":
                mode = "block"; index += 2; continue
            if char == '"': mode = "string"
            elif char == "'": mode = "char"
            elif char == "{": depth += 1
            elif char == "}":
                depth -= 1
                if depth == 0: return index + 1
        elif mode == "line":
            if char == "\n": mode = "code"
        elif mode == "block":
            if char == "*" and next_char == "/": mode = "code"; index += 2; continue
        elif mode in ("string", "char"):
            if char == "\\": index += 2; continue
            if char == ('"' if mode == "string" else "'"): mode = "code"
        index += 1
    raise ValueError("unterminated source body")


def extract_block(source: str, expression: str, suffix: str = "") -> str:
    matches = list(re.finditer(expression, source, re.M))
    if len(matches) != 1:
        raise ValueError(f"expected one source match for {expression}, got {len(matches)}")
    match = matches[0]
    opening = source.find("{", match.end())
    if opening < 0: raise ValueError(f"missing opening brace: {expression}")
    end = brace_end(source, opening)
    return source[match.start():end + len(suffix)]


def run(command: list[str], directory: Path, log: Path) -> str:
    result = subprocess.run(command, cwd=directory, stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT, text=True, errors="replace", timeout=180)
    log.write_text(result.stdout, encoding="utf-8")
    if result.returncode:
        print(result.stdout)
        raise SystemExit(f"{log.name} failed: {result.returncode}")
    return result.stdout


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--build-root", type=Path)
    options = parser.parse_args()
    repo = Path(__file__).resolve().parents[2]
    private = Path(os.environ["LOCALAPPDATA"]) / "V2DLLFork" / "R02p" / "luna-preflight"
    build = (options.build_root or private / datetime.datetime.now().strftime("run-%Y%m%d-%H%M%S-%f")).resolve()
    if build == repo or repo in build.parents or build.exists() or private.resolve() not in build.parents:
        raise SystemExit(f"new build root must be under {private}: {build}")
    build.mkdir(parents=True)
    source_path = repo / "Source/V2/V2/V2TechButton.cpp"
    fixture_path = repo / "tests/fork/bytepatch_preflight_tests.cpp"
    source_hash = sha(source_path)
    source = source_path.read_text(encoding="utf-8-sig")

    struct = extract_block(source, r"^struct BytePatch\s*$", ";")
    table = extract_block(source, r"^static BytePatch EXE_PATCHES\[\] =\s*$", ";")
    count_match = re.findall(r"^static const int EXE_PATCH_COUNT = sizeof\(EXE_PATCHES\) / sizeof\(EXE_PATCHES\[0\]\);$", source, re.M)
    if len(count_match) != 1: raise SystemExit("EXE_PATCH_COUNT changed")
    bodies = [extract_block(source, rf"^static (?:DWORD|bool) {name}\([^\n]*\)\s*$")
              for name in ("FileOffsetToRVA", "ReadableExePatchSpan", "PreflightExePatches")]
    install = extract_block(source, r"^static bool Install\(\)\s*$")
    order = [install.find(token) for token in ("LoadSettings();", "g_base = (DWORD)GetModuleHandleA(NULL);",
                                                "g_imageSize = 0;", "if (!PreflightExePatches())",
                                                "InstallEngineStability();")]
    if any(position < 0 for position in order) or order != sorted(order):
        raise SystemExit(f"Install source order changed: {order}")
    gate = install.find("if (!PreflightExePatches())\n        return false;")
    if gate < 0 or gate >= order[-1]:
        raise SystemExit("Install must abort before first native installer on preflight failure")

    fixture = fixture_path.read_text(encoding="utf-8-sig")
    marker = "// ACTUAL_BYTEPATCH_BODIES_INJECTED"
    if fixture.count(marker) != 1: raise SystemExit("fixture marker missing or ambiguous")
    parts = [struct, table, count_match[0], *bodies]
    generated = build / "bytepatch_preflight_actual.cpp"
    generated.write_text(fixture.replace(marker, "\n\n".join(parts)), encoding="utf-8", newline="\n")
    if sha(source_path) != source_hash: raise SystemExit("source changed during extraction")

    vcvars = Path(r"C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat")
    if not vcvars.is_file(): raise SystemExit(f"missing vcvarsall: {vcvars}")
    # Avoid Windows installer-detection heuristics for executable names with "patch".
    exe = build / "contract.exe"
    batch = build / "compile.cmd"
    batch.write_text(f'@echo off\ncall "{vcvars}" x86 >nul\nif errorlevel 1 exit /b %errorlevel%\n'
                     f'cl /nologo /W4 /WX /O2 /EHsc /std:c++17 /Fe:"{exe}" "{generated}"\n'
                     'exit /b %errorlevel%\n', encoding="ascii")
    compile_log = build / "compile.log"
    runtime_log = build / "runtime.log"
    run(["cmd.exe", "/d", "/c", str(batch)], build, compile_log)
    output = run([str(exe)], build, runtime_log)
    if not re.search(r"RESULT checks=\d+ failures=0 descriptors=21 enabled=18", output):
        raise SystemExit("missing complete successful test result")
    if sha(source_path) != source_hash: raise SystemExit("source changed during test run")
    manifest = build / "hashes.txt"
    paths = [source_path, fixture_path, Path(__file__), generated, batch, compile_log, runtime_log, exe]
    manifest.write_text("\n".join(f"{path} sha256={sha(path)}" for path in paths) + "\n"
                        + "\n".join(f"extracted[{i}] sha256={hashlib.sha256(part.encode('utf-8')).hexdigest()}"
                                    for i, part in enumerate(parts)) + "\n",
                        encoding="utf-8")
    print(output, end="")
    print(f"ORDER offsets={order}")
    print(f"ARTIFACT {build}")
    print(f"MANIFEST_SHA256 {sha(manifest)}")


if __name__ == "__main__":
    main()
