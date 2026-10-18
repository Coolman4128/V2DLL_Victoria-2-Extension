"""Run actual PinFpu and crash-diagnostic skip source in owned Win32 fixtures."""
import argparse
import ast
import hashlib
import os
import pathlib
import re
import subprocess


def function_definition(source: str, signature: str) -> str:
    start = source.rfind(signature)
    if start < 0:
        raise SystemExit(f"missing production function: {signature}")
    opening = source.find("{", start)
    depth = 0
    state = "code"
    i = opening
    while i < len(source):
        ch = source[i]
        nxt = source[i + 1] if i + 1 < len(source) else ""
        if state == "line":
            if ch == "\n": state = "code"
        elif state == "block":
            if ch == "*" and nxt == "/": state = "code"; i += 1
        elif state in ("string", "char"):
            if ch == "\\": i += 1
            elif (state == "string" and ch == '"') or (state == "char" and ch == "'"):
                state = "code"
        elif ch == "/" and nxt == "/": state = "line"; i += 1
        elif ch == "/" and nxt == "*": state = "block"; i += 1
        elif ch == '"': state = "string"
        elif ch == "'": state = "char"
        elif ch == "{": depth += 1
        elif ch == "}":
            depth -= 1
            if depth == 0: return source[start:i + 1]
        i += 1
    raise SystemExit(f"unterminated function: {signature}")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-root", type=pathlib.Path,
        default=pathlib.Path("%LOCALAPPDATA%/V2DLLFork/R02d/fpu-skip-contract/run-08"))
    parser.add_argument("--msbuild", default=r"C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe")
    args = parser.parse_args()
    repo = pathlib.Path(__file__).resolve().parents[2]
    appdata = os.environ.get("LOCALAPPDATA")
    if not appdata: raise SystemExit("LOCALAPPDATA is required")
    build = pathlib.Path(str(args.build_root).replace("%LOCALAPPDATA%", appdata)).resolve()
    if build == repo or repo in build.parents or build.exists():
        raise SystemExit(f"build root must be new and outside the repository: {build}")
    build.mkdir(parents=True)

    tech_path = repo / "Source/V2/V2/V2TechButton.cpp"
    tech = tech_path.read_text(encoding="utf-8-sig")
    pin = function_definition(tech, "static void PinFpu()")
    snapshot_path = pathlib.Path(appdata) / "V2DLLFork/R02c/build-checkpoint/source/V2TechButton.cpp"
    snapshot = snapshot_path.read_text(encoding="utf-8-sig")
    old_pin = function_definition(snapshot, "static void PinFpu()")
    if pin != old_pin:
        raise SystemExit("PinFpu body differs byte-for-byte from immutable R02c source snapshot")
    if "_controlfp_s(&ignored, _PC_53, _MCW_PC);" not in pin or "_controlfp_s(&ignored, _RC_NEAR, _MCW_RC);" not in pin:
        raise SystemExit("actual PinFpu precision/rounding calls changed")
    if "_MM_SET_FLUSH_ZERO_MODE(_MM_FLUSH_ZERO_ON);" not in pin or "_MM_SET_DENORMALS_ZERO_MODE(_MM_DENORMALS_ZERO_ON);" not in pin:
        raise SystemExit("actual PinFpu FTZ/DAZ calls changed")

    skip = function_definition(tech, "static void ReportUnavailableCrashWatch()")
    dllmain = function_definition(tech, "BOOL APIENTRY DllMain(HMODULE hModule")
    if len(re.findall(r"\bInstallCrashWatch\s*\(", tech)) != 1:
        raise SystemExit("original CrashWatch installer must remain definition-only")
    installer = function_definition(tech, "static void InstallCrashWatch()")
    old_installer = function_definition(snapshot, "static void InstallCrashWatch()")
    if installer != old_installer:
        raise SystemExit("dormant InstallCrashWatch body changed from immutable R02c source")
    dormant_handlers = (
        "static LONG CALLBACK CrashVectored(EXCEPTION_POINTERS* ep)",
        "static LONG WINAPI CrashUnhandled(EXCEPTION_POINTERS* ep)",
        "static LPTOP_LEVEL_EXCEPTION_FILTER WINAPI HookSetUEF(LPTOP_LEVEL_EXCEPTION_FILTER p)",
        "static void CrashFromAbort(DWORD code)",
        "static void __cdecl CrashOnAbort(int)",
        "static void __cdecl CrashOnInvalidParam(",
    )
    for signature in dormant_handlers:
        if function_definition(tech, signature) != function_definition(snapshot, signature):
            raise SystemExit(f"dormant CrashWatch handler changed from R02c source: {signature}")
    preserved_config = (
        "struct Settings",
        "static void ApplySetting(const char* key, const char* value)",
        "static void WriteDefaultSettings(const char* path)",
        "static void LoadSettingsFrom(const char* path)",
        "static void LoadSettings()",
        "void Log(const char* fmt, ...)",
    )
    for signature in preserved_config:
        if function_definition(tech, signature) != function_definition(snapshot, signature):
            raise SystemExit(f"settings/defaults/logging behavior changed from R02c source: {signature}")
    if len(re.findall(r"\bReportUnavailableCrashWatch\s*\(", tech)) != 2:
        raise SystemExit("skip reporter must have one definition and one production call")
    attach = dllmain[dllmain.index("if (reason == DLL_PROCESS_ATTACH)"):]
    if not re.search(r"ReportUnavailableCrashWatch\(\);\s*PinFpu\(\);", attach):
        raise SystemExit("DllMain must report skip immediately before the unchanged mandatory PinFpu")
    outside = tech.replace(installer, "")
    callback_registration = r"\bAddVectoredExceptionHandler\s*\([^;]*\bCrashVectored\b"
    if re.search(callback_registration, outside, re.DOTALL) or re.search(
            r"\bSetThreadStackGuarantee\s*\(", outside):
        raise SystemExit("CrashWatch callback/stack registration bypass found outside original installer")
    skip_code = re.sub(r'"(?:\\.|[^"\\])*"', '""', skip)
    if re.search(r"\b(?:AddVectoredExceptionHandler|SetUnhandledExceptionFilter|"
            r"SetThreadStackGuarantee|signal|_set_invalid_parameter_handler|HookIat)\b", skip_code):
        raise SystemExit("skip helper contains a diagnostic registration or handler dependency")
    calls = re.findall(r"\bLog\s*\((.*?)\);", skip, re.DOTALL)
    if len(calls) != 1:
        raise SystemExit("skip reporter must issue exactly one Log call")
    message = "".join(ast.literal_eval(s) for s in re.findall(r'"(?:\\.|[^"\\])*"', calls[0]))
    expected_message = ("CrashWatch unavailable (R02d): optional diagnostics blocked; "
        "registration/provider/lifetime profile unproved; no handlers/IAT/signal changes; "
        "mandatory PinFpu retained unchanged")
    if message != expected_message:
        raise SystemExit("skip reporter literal changed or implies diagnostics were installed")

    cpp = (repo / "tests/fork/m0_fpu_skip_tests.cpp").read_text(encoding="utf-8-sig")
    marker = "// ACTUAL_FUNCTIONS_INJECTED"
    if cpp.count(marker) != 1: raise SystemExit("C++ fixture insertion marker must occur once")
    generated = cpp.replace(marker, pin + "\n" + skip)
    generated_path = build / "m0_fpu_skip_contract.cpp"
    generated_path.write_text(generated, encoding="utf-8")
    hashes = [tech_path, repo / "tests/fork/test_m0_fpu_skip.py", repo / "tests/fork/m0_fpu_skip_tests.cpp"]
    (build / "source_hashes.txt").write_text("\n".join(
        f"{p.relative_to(repo)} {hashlib.sha256(p.read_bytes()).hexdigest()}" for p in hashes) + "\n", encoding="ascii")
    source_dir = repo / "Source/V2/V2"
    project = f'''<Project DefaultTargets="Build" xmlns="http://schemas.microsoft.com/developer/msbuild/2003">
<ItemGroup Label="ProjectConfigurations"><ProjectConfiguration Include="Release|Win32"><Configuration>Release</Configuration><Platform>Win32</Platform></ProjectConfiguration></ItemGroup>
<PropertyGroup Label="Globals"><WindowsTargetPlatformVersion>10.0.26100.0</WindowsTargetPlatformVersion></PropertyGroup>
<Import Project="$(VCTargetsPath)\\Microsoft.Cpp.Default.props" />
<PropertyGroup Label="Configuration"><ConfigurationType>Application</ConfigurationType><PlatformToolset>v143</PlatformToolset><UseDebugLibraries>false</UseDebugLibraries><WholeProgramOptimization>true</WholeProgramOptimization></PropertyGroup>
<Import Project="$(VCTargetsPath)\\Microsoft.Cpp.props" />
<PropertyGroup><OutDir>$(ProjectDir)artifacts\\</OutDir><IntDir>$(ProjectDir)obj\\</IntDir><TargetName>m0_fpu_skip_contract</TargetName></PropertyGroup>
<ItemDefinitionGroup><ClCompile><WarningLevel>Level4</WarningLevel><TreatWarningAsError>true</TreatWarningAsError><Optimization>MaxSpeed</Optimization><LanguageStandard>stdcpp20</LanguageStandard><RuntimeLibrary>MultiThreaded</RuntimeLibrary><PreprocessorDefinitions>WIN32;_CONSOLE</PreprocessorDefinitions><AdditionalIncludeDirectories>{source_dir}</AdditionalIncludeDirectories></ClCompile></ItemDefinitionGroup>
<ItemGroup><ClCompile Include="m0_fpu_skip_contract.cpp" /></ItemGroup>
<Import Project="$(VCTargetsPath)\\Microsoft.Cpp.targets" /></Project>'''
    project_path = build / "m0_fpu_skip_contract.vcxproj"
    project_path.write_text(project, encoding="utf-8")
    result = subprocess.run([args.msbuild, str(project_path), "/t:Rebuild",
        "/p:Configuration=Release", "/p:Platform=Win32", "/v:minimal"],
        capture_output=True, text=True)
    (build / "build.log").write_text(result.stdout + result.stderr, encoding="utf-8")
    if result.returncode: print(result.stdout, result.stderr); raise SystemExit(result.returncode)
    result = subprocess.run([str(build / "artifacts/m0_fpu_skip_contract.exe")], capture_output=True, text=True)
    (build / "test.log").write_text(result.stdout + result.stderr, encoding="utf-8")
    print(result.stdout, end="")
    raise SystemExit(result.returncode)


if __name__ == "__main__": main()
