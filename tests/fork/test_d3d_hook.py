"""Build and run bounded actual-source D3D9 hooks against owned Win32 memory."""
import argparse
import hashlib
import os
from pathlib import Path
import re
import subprocess


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest().upper()


def definition(source: str, signature: str) -> str:
    if source.count(signature) != 1:
        raise SystemExit(f"missing or ambiguous source signature: {signature}")
    begin = source.index(signature)
    opening = source.find("{", begin + len(signature))
    if opening < 0:
        raise SystemExit(f"no source body: {signature}")
    depth = 0
    state = "code"
    i = opening
    while i < len(source):
        ch = source[i]
        nxt = source[i + 1] if i + 1 < len(source) else ""
        if state == "line":
            if ch == "\n":
                state = "code"
        elif state == "block":
            if ch == "*" and nxt == "/":
                state = "code"
                i += 1
        elif state in ("string", "char"):
            if ch == "\\":
                i += 1
            elif (state == "string" and ch == '"') or (state == "char" and ch == "'"):
                state = "code"
        elif ch == "/" and nxt == "/":
            state = "line"
            i += 1
        elif ch == "/" and nxt == "*":
            state = "block"
            i += 1
        elif ch == '"':
            state = "string"
        elif ch == "'":
            state = "char"
        elif ch == "{":
            depth += 1
        elif ch == "}":
            depth -= 1
            if depth == 0:
                return source[begin:i + 1]
        i += 1
    raise SystemExit(f"unclosed source body: {signature}")


def unique_line(source: str, pattern: str) -> str:
    matches = re.findall(pattern, source, re.MULTILINE)
    if len(matches) != 1:
        raise SystemExit(f"missing or ambiguous source declaration: {pattern}")
    return matches[0]


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--build-root", type=Path, required=True)
    ap.add_argument("--msbuild", default=r"C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe")
    args = ap.parse_args()
    repo = Path(__file__).resolve().parents[2]
    build = args.build_root.resolve()
    appdata = os.environ.get("LOCALAPPDATA")
    if not appdata:
        raise SystemExit("LOCALAPPDATA is required")
    private = (Path(appdata) / "V2DLLFork/R02h/owned-contract").resolve()
    if build == repo or repo in build.parents or build.exists() or private not in build.parents:
        raise SystemExit(f"build root must be new under private directory {private}: {build}")
    source_path = repo / "Source/V2/V2/V2TechButton.cpp"
    staged_path = Path(appdata) / "V2DLLFork/R02d/build-checkpoint/source/V2TechButton.cpp"
    if not staged_path.is_file() or sha(source_path) != sha(staged_path):
        raise SystemExit("current product source differs from frozen R02d staged source")
    source = source_path.read_text(encoding="utf-8-sig")
    staged = staged_path.read_text(encoding="utf-8-sig")
    for pattern in (r"^\s*bool patchD3dFpuPreserve\s*= true;",
                    r"^\s*bool patchD3dNoVsync\s*= false;"):
        if unique_line(source, pattern) != unique_line(staged, pattern):
            raise SystemExit(f"frozen D3D setting changed: {pattern}")
    constants = [
        "D3DCREATE_FPU_PRESERVE_FLAG", "D3DPRESENT_INTERVAL_IMMEDIATE",
        "D3DPRESENT_INTERVAL_OFF", "D3D9_VT_CREATEDEVICE",
        "D3D9DEV_VT_RESET", "D3D9DEV_VT_PRESENT",
    ]
    typedefs = ["tDirect3DCreate9", "tD3D9CreateDevice", "tD3D9Reset", "tD3D9Present"]
    lines = []
    for name in constants:
        pattern = rf"^static const (?:DWORD|int) {name} = [^;]+;"
        line = unique_line(source, pattern)
        if line != unique_line(staged, pattern):
            raise SystemExit(f"constant changed from frozen source: {name}")
        lines.append(line)
    for name in typedefs:
        pattern = rf"^typedef [^\n]+\*\s*{name}\)[^;]+;"
        line = unique_line(source, pattern)
        if line != unique_line(staged, pattern):
            raise SystemExit(f"typedef changed from frozen source: {name}")
        lines.append(line)
    for name in ("g_realDirect3DCreate9", "g_realCreateDevice", "g_realReset", "g_realPresent"):
        pattern = rf"^static t\w+\s+{name} = 0;"
        line = unique_line(source, pattern)
        if line != unique_line(staged, pattern):
            raise SystemExit(f"provider global changed from frozen source: {name}")
        lines.append(line)
    signatures = [
        "static bool PatchVtableSlot(void* obj, int slot, void* hook, void** orig)",
        "static void ForceImmediatePresent(void* params)",
        "static HRESULT WINAPI HookReset(void* self, void* params)",
        "static HRESULT WINAPI HookCreateDevice(",
        "static void* WINAPI HookDirect3DCreate9(UINT sdk)",
    ]
    for signature in signatures:
        now = definition(source, signature)
        if now != definition(staged, signature):
            raise SystemExit(f"hook changed from frozen source: {signature}")
        lines.append(now)
    fixture_path = repo / "tests/fork/d3d_hook_tests.cpp"
    driver_path = Path(__file__).resolve()
    fixture = fixture_path.read_text(encoding="utf-8")
    marker = "// ACTUAL_D3D_SNIPPETS"
    if fixture.count(marker) != 1:
        raise SystemExit("fixture injection marker missing or ambiguous")
    generated = fixture.replace(marker, "\n\n".join(lines))
    build.mkdir(parents=True)
    generated_path = build / "d3d_hook_contract.cpp"
    generated_path.write_text(generated, encoding="utf-8", newline="\n")
    project = r'''<Project DefaultTargets="Build" xmlns="http://schemas.microsoft.com/developer/msbuild/2003">
<ItemGroup Label="ProjectConfigurations"><ProjectConfiguration Include="Release|Win32"><Configuration>Release</Configuration><Platform>Win32</Platform></ProjectConfiguration></ItemGroup>
<PropertyGroup Label="Globals"><WindowsTargetPlatformVersion>10.0.26100.0</WindowsTargetPlatformVersion></PropertyGroup>
<Import Project="$(VCTargetsPath)\Microsoft.Cpp.Default.props" />
<PropertyGroup Label="Configuration"><ConfigurationType>Application</ConfigurationType><PlatformToolset>v143</PlatformToolset><UseDebugLibraries>false</UseDebugLibraries><WholeProgramOptimization>true</WholeProgramOptimization></PropertyGroup>
<Import Project="$(VCTargetsPath)\Microsoft.Cpp.props" />
<PropertyGroup><OutDir>$(ProjectDir)artifacts\</OutDir><IntDir>$(ProjectDir)obj\</IntDir><TargetName>d3d_hook_contract</TargetName></PropertyGroup>
<ItemDefinitionGroup><ClCompile><WarningLevel>Level4</WarningLevel><TreatWarningAsError>true</TreatWarningAsError><Optimization>MaxSpeed</Optimization><LanguageStandard>stdcpp20</LanguageStandard><RuntimeLibrary>MultiThreaded</RuntimeLibrary><PreprocessorDefinitions>WIN32;_CONSOLE</PreprocessorDefinitions></ClCompile></ItemDefinitionGroup>
<ItemGroup><ClCompile Include="d3d_hook_contract.cpp" /></ItemGroup>
<Import Project="$(VCTargetsPath)\Microsoft.Cpp.targets" /></Project>'''
    project_path = build / "d3d_hook_contract.vcxproj"
    project_path.write_text(project, encoding="utf-8", newline="\n")
    result = subprocess.run([args.msbuild, str(project_path), "/t:Rebuild",
        "/p:Configuration=Release", "/p:Platform=Win32", "/v:minimal"],
        capture_output=True, text=True, timeout=90)
    (build / "build.log").write_text(result.stdout + result.stderr, encoding="utf-8")
    if result.returncode:
        print((build / "build.log").read_text(encoding="utf-8"))
        return result.returncode
    exe = build / "artifacts/d3d_hook_contract.exe"
    run = subprocess.run([str(exe)], cwd=build, capture_output=True, text=True, timeout=10)
    (build / "runtime.log").write_text(run.stdout + run.stderr, encoding="utf-8")
    print(run.stdout + run.stderr, end="")
    identity = [f"source {source_path} {sha(source_path)}",
                f"R02d-staged {staged_path} {sha(staged_path)}"]
    for path in (fixture_path, driver_path, generated_path, project_path,
                 build / "build.log", exe, build / "runtime.log"):
        identity.append(f"artifact {path} {path.stat().st_size} {sha(path)}")
    (build / "identity.txt").write_text("\n".join(identity) + "\n", encoding="utf-8")
    return run.returncode

if __name__ == "__main__":
    raise SystemExit(main())
