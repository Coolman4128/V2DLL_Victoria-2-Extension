"""Compile the current legacy byte table against the real transaction module."""

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
    state = "code"
    index = opening
    while index < len(source):
        char = source[index]
        ahead = source[index + 1] if index + 1 < len(source) else ""
        if state == "code":
            if char == "/" and ahead == "/": state = "line"; index += 2; continue
            if char == "/" and ahead == "*": state = "block"; index += 2; continue
            if char == '"': state = "string"
            elif char == "'": state = "char"
            elif char == "{": depth += 1
            elif char == "}":
                depth -= 1
                if depth == 0: return index + 1
        elif state == "line":
            if char == "\n": state = "code"
        elif state == "block":
            if char == "*" and ahead == "/": state = "code"; index += 2; continue
        else:
            if char == "\\": index += 2; continue
            if char == ('"' if state == "string" else "'"): state = "code"
        index += 1
    raise ValueError("unterminated source body")


def extract(source: str, regex: str, suffix: str = "") -> str:
    matches = list(re.finditer(regex, source, re.M))
    if len(matches) != 1: raise ValueError(f"expected one {regex}, got {len(matches)}")
    match = matches[0]
    opening = source.find("{", match.end())
    if opening < 0: raise ValueError(f"missing body: {regex}")
    return source[match.start():brace_end(source, opening) + len(suffix)]


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
    private = Path(os.environ["LOCALAPPDATA"]) / "V2DLLFork" / "R02q" / "luna-test"
    build = (args.build_root or private / datetime.datetime.now().strftime("run-%Y%m%d-%H%M%S-%f")).resolve()
    if build == repo or repo in build.parents or build.exists() or private.resolve() not in build.parents:
        raise SystemExit(f"new private build root required under {private}: {build}")
    build.mkdir(parents=True)

    source_path = repo / "Source/V2/V2/V2TechButton.cpp"
    fixture_path = repo / "tests/fork/legacy_byte_transaction_tests.cpp"
    module_h = repo / "Source/V2/V2/Fork/LegacyByteTransaction.h"
    module_cpp = repo / "Source/V2/V2/Fork/LegacyByteTransaction.cpp"
    backend_cpp = repo / "Source/V2/V2/Fork/Win32Memory.cpp"
    if not module_h.is_file() or not module_cpp.is_file():
        raise SystemExit("transaction module has not landed")
    source_hash = sha(source_path)
    source = source_path.read_text(encoding="utf-8-sig")
    parts = [extract(source, r"^struct BytePatch\s*$", ";"),
             extract(source, r"^static BytePatch EXE_PATCHES\[\] =\s*$", ";")]
    count = re.findall(r"^static const int EXE_PATCH_COUNT = sizeof\(EXE_PATCHES\) / sizeof\(EXE_PATCHES\[0\]\);$", source, re.M)
    if len(count) != 1: raise SystemExit("EXE_PATCH_COUNT changed")
    parts.append(count[0])
    parts.append(extract(source, r"^static DWORD FileOffsetToRVA\(DWORD fileOffset\)\s*$"))
    writer = extract(source, r"^static bool InstallExePatches\(\)\s*$")
    for snippet in ("site.enabled = bp.enabled;", "if (bp.enabled)",
                    "site.expected.assign(bp.expect, bp.expect + bp.len);",
                    "site.replacement.assign(bp.replace, bp.replace + bp.len);",
                    "InstallLegacyByteTable(sites, g_imageSize, memory)",
                    "return report.state == fork_boundary::LegacyByteState::Applied ||"):
        if writer.count(snippet) != 1:
            raise SystemExit(f"transaction writer conversion changed: {snippet}")
    install = extract(source, r"^static bool Install\(\)\s*$")
    ordered = ("LoadSettings();", "if (!PreflightExePatches())", "InstallEngineStability();",
               "InstallOptionalViewsFamily();", "if (!InstallExePatches())",
               "InstallOccupiedReinforceSplit();")
    positions = [install.find(snippet) for snippet in ordered]
    if any(pos < 0 for pos in positions) or positions != sorted(positions):
        raise SystemExit(f"Install transaction/preflight order changed: {positions}")
    if "if (!InstallExePatches())\n    {" not in install or "return false;" not in install[positions[4]:positions[5]]:
        raise SystemExit("Install must abort before later native installers on transaction failure")
    fixture = fixture_path.read_text(encoding="utf-8-sig")
    marker = "// ACTUAL_LEGACY_BYTE_TABLE_INJECTED"
    if fixture.count(marker) != 1: raise SystemExit("fixture marker missing or ambiguous")
    generated = build / "legacy_byte_actual.cpp"
    generated.write_text(fixture.replace(marker, "\n\n".join(parts)), encoding="utf-8", newline="\n")
    if sha(source_path) != source_hash: raise SystemExit("main source changed during extraction")

    vcvars = Path(r"C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat")
    if not vcvars.is_file(): raise SystemExit(f"missing vcvarsall: {vcvars}")
    exe = build / "contract.exe"
    batch = build / "compile.cmd"
    include = repo / "Source/V2/V2/Fork"
    batch.write_text(f'@echo off\ncall "{vcvars}" x86 >nul\nif errorlevel 1 exit /b %errorlevel%\n'
                     f'cl /nologo /W4 /WX /O2 /EHsc /std:c++17 /I"{include}" /Fe:"{exe}" '
                     f'"{generated}" "{module_cpp}" "{backend_cpp}"\nexit /b %errorlevel%\n',
                     encoding="ascii")
    compile_log = build / "compile.log"
    runtime_log = build / "runtime.log"
    run(["cmd.exe", "/d", "/c", str(batch)], build, compile_log)
    output = run([str(exe)], build, runtime_log)
    if not re.search(r"RESULT checks=\d+ failures=0 descriptors=21 enabled=18", output):
        raise SystemExit("missing complete success result")
    if sha(source_path) != source_hash: raise SystemExit("main source changed during test")
    paths = [source_path, fixture_path, Path(__file__), module_h, module_cpp, backend_cpp,
             generated, batch, compile_log, runtime_log, exe]
    manifest = build / "hashes.txt"
    manifest.write_text("\n".join(f"{path} sha256={sha(path)}" for path in paths) + "\n"
                        + "\n".join(f"extracted[{i}] sha256={hashlib.sha256(part.encode('utf-8')).hexdigest()}"
                                    for i, part in enumerate(parts)) + "\n"
                        + f"writer sha256={hashlib.sha256(writer.encode('utf-8')).hexdigest()}\n"
                        + f"Install order offsets={positions}\n", encoding="utf-8")
    print(output, end="")
    print(f"ARTIFACT {build}")
    print(f"MANIFEST_SHA256 {sha(manifest)}")


if __name__ == "__main__":
    main()
