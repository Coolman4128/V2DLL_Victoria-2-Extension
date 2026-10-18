"""Compile and run the unchanged actual _beginthreadex wrapper in an owned x86 process."""
import argparse
import hashlib
import os
from pathlib import Path
import re
import subprocess


def definition(source: str, signature: str, terminal: str = "}") -> str:
    if source.count(signature) != 1:
        raise SystemExit(f"missing/ambiguous exact production signature: {signature}")
    begin = source.index(signature)
    if terminal == ";":
        end = source.find(";", begin + len(signature))
        if end < 0:
            raise SystemExit(f"unterminated typedef: {signature}")
        return source[begin:end + 1]
    opening = source.find("{", begin + len(signature))
    if opening < 0:
        raise SystemExit(f"missing definition brace: {signature}")
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
                if terminal == "};":
                    if i + 1 >= len(source) or source[i + 1] != ";":
                        raise SystemExit(f"missing struct terminator: {signature}")
                    return source[begin:i + 2]
                return source[begin:i + 1]
        i += 1
    raise SystemExit(f"unterminated definition: {signature}")


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest().upper()


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-root", type=Path, required=True)
    parser.add_argument("--msbuild", default=r"C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe")
    args = parser.parse_args()
    repo = Path(__file__).resolve().parents[2]
    build = args.build_root.resolve()
    if build == repo or repo in build.parents or build.exists():
        raise SystemExit(f"build root must be new and outside repository: {build}")
    appdata = os.environ.get("LOCALAPPDATA")
    if not appdata:
        raise SystemExit("LOCALAPPDATA is required")
    private = (Path(appdata) / "V2DLLFork/R02g/crt-contract").resolve()
    if private != build and private not in build.parents:
        raise SystemExit(f"build root must be under {private}")
    provider_path = Path(os.environ["WINDIR"]) / "SysWOW64/msvcr100.dll"
    provider_sha = "8793353461826FBD48F25EA8B835BE204B758CE7510DB2AF631B28850355BD18"
    if not provider_path.is_file() or sha(provider_path) != provider_sha:
        raise SystemExit(f"pinned physical x86 MSVCR100 provider absent or changed: {provider_path}")
    current_path = repo / "Source/V2/V2/V2TechButton.cpp"
    staged_path = Path(appdata) / "V2DLLFork/R02d/build-checkpoint/source/V2TechButton.cpp"
    if not staged_path.is_file():
        raise SystemExit(f"missing frozen R02d staged source: {staged_path}")
    current = current_path.read_text(encoding="utf-8-sig")
    staged = staged_path.read_text(encoding="utf-8-sig")
    signatures = [
        ("typedef DWORD_PTR (__cdecl *tBeginThreadEx)(", ";"),
        ("struct ThreadStartWrap", "};"),
        ("static void PinFpu()", "}"),
        ("static DWORD WINAPI PinnedThreadStart(LPVOID raw)", "}"),
        ("static DWORD_PTR __cdecl HookBeginThreadEx(", "}"),
    ]
    pieces = []
    lines = [f"physical-provider {provider_path} {sha(provider_path)}",
             f"current {current_path} {sha(current_path)}",
             f"R02d-staged {staged_path} {sha(staged_path)}"]
    for signature, terminal in signatures:
        now = definition(current, signature, terminal)
        old = definition(staged, signature, terminal)
        if now != old:
            raise SystemExit(f"production definition differs from R02d staged source: {signature}")
        digest = hashlib.sha256(now.encode("utf-8")).hexdigest().upper()
        lines.append(f"unchanged-definition {signature} {digest}")
        pieces.append(now)
    if not re.search(r"_controlfp_s\(&ignored, _PC_53, _MCW_PC\);", pieces[2]) or not re.search(
            r"_controlfp_s\(&ignored, _RC_NEAR, _MCW_RC\);", pieces[2]):
        raise SystemExit("frozen PinFpu control calls changed")
    if "_MM_SET_FLUSH_ZERO_MODE(_MM_FLUSH_ZERO_ON);" not in pieces[2] or \
            "_MM_SET_DENORMALS_ZERO_MODE(_MM_DENORMALS_ZERO_ON);" not in pieces[2]:
        raise SystemExit("frozen PinFpu SSE control calls changed")
    fixture_path = repo / "tests/fork/begin_thread_ex_tests.cpp"
    driver_path = Path(__file__).resolve()
    fixture = fixture_path.read_text(encoding="utf-8-sig")
    for marker in ("// ACTUAL_TYPEDEF_INJECTED", "// ACTUAL_FUNCTIONS_INJECTED"):
        if fixture.count(marker) != 1:
            raise SystemExit(f"fixture marker missing/ambiguous: {marker}")
    generated = fixture.replace("// ACTUAL_TYPEDEF_INJECTED", pieces[0])
    generated = generated.replace("// ACTUAL_FUNCTIONS_INJECTED",
        "static tBeginThreadEx g_realBeginThreadEx = nullptr;\n" + "\n\n".join(pieces[1:]))
    build.mkdir(parents=True)
    generated_path = build / "begin_thread_ex_contract.cpp"
    generated_path.write_text(generated, encoding="utf-8", newline="\n")
    lines.extend([f"fixture {fixture_path} {sha(fixture_path)}",
                  f"driver {driver_path} {sha(driver_path)}",
                  f"generated {generated_path} {sha(generated_path)}"])
    (build / "source_hashes.txt").write_text("\n".join(lines) + "\n", encoding="utf-8")
    project = '''<Project DefaultTargets="Build" xmlns="http://schemas.microsoft.com/developer/msbuild/2003">
<ItemGroup Label="ProjectConfigurations"><ProjectConfiguration Include="Release|Win32"><Configuration>Release</Configuration><Platform>Win32</Platform></ProjectConfiguration></ItemGroup>
<PropertyGroup Label="Globals"><WindowsTargetPlatformVersion>10.0.26100.0</WindowsTargetPlatformVersion></PropertyGroup>
<Import Project="$(VCTargetsPath)\\Microsoft.Cpp.Default.props" />
<PropertyGroup Label="Configuration"><ConfigurationType>Application</ConfigurationType><PlatformToolset>v143</PlatformToolset><UseDebugLibraries>false</UseDebugLibraries><WholeProgramOptimization>true</WholeProgramOptimization></PropertyGroup>
<Import Project="$(VCTargetsPath)\\Microsoft.Cpp.props" />
<PropertyGroup><OutDir>$(ProjectDir)artifacts\\</OutDir><IntDir>$(ProjectDir)obj\\</IntDir><TargetName>begin_thread_ex_contract</TargetName></PropertyGroup>
<ItemDefinitionGroup><ClCompile><WarningLevel>Level4</WarningLevel><TreatWarningAsError>true</TreatWarningAsError><Optimization>MaxSpeed</Optimization><LanguageStandard>stdcpp20</LanguageStandard><RuntimeLibrary>MultiThreaded</RuntimeLibrary><PreprocessorDefinitions>WIN32;_CONSOLE</PreprocessorDefinitions></ClCompile></ItemDefinitionGroup>
<ItemGroup><ClCompile Include="begin_thread_ex_contract.cpp" /></ItemGroup>
<Import Project="$(VCTargetsPath)\\Microsoft.Cpp.targets" /></Project>'''
    project_path = build / "begin_thread_ex_contract.vcxproj"
    project_path.write_text(project, encoding="utf-8", newline="\n")
    build_result = subprocess.run([args.msbuild, str(project_path), "/t:Rebuild",
        "/p:Configuration=Release", "/p:Platform=Win32", "/v:minimal"],
        capture_output=True, text=True, timeout=90)
    (build / "build.log").write_text(build_result.stdout + build_result.stderr, encoding="utf-8")
    if build_result.returncode:
        print(build_result.stdout, build_result.stderr)
        raise SystemExit(build_result.returncode)
    exe = build / "artifacts/begin_thread_ex_contract.exe"
    test_result = subprocess.run([str(exe)], capture_output=True, text=True, timeout=60)
    (build / "test.log").write_text(test_result.stdout + test_result.stderr, encoding="utf-8")
    print(test_result.stdout, end="")
    if test_result.stderr:
        print(test_result.stderr, end="")
    raise SystemExit(test_result.returncode)


if __name__ == "__main__":
    main()
