"""Compile the actual startup gate and an extracted production DllMain route.

The Win32 harness uses only owned synthetic bytes and mocked downstream startup
effects. It does not load the fork DLL or inspect/control the game process.
"""
import argparse
import ast
import hashlib
import os
import pathlib
import re
import subprocess


def function_definition(source: str, signature: str) -> str:
    """Extract one actual C++ definition by balancing braces and literals."""
    start = source.rfind(signature)
    if start < 0:
        raise SystemExit(f"missing production function: {signature}")
    opening = source.find("{", start)
    if opening < 0:
        raise SystemExit(f"missing body for production function: {signature}")
    depth = 0
    state = "code"
    i = opening
    while i < len(source):
        ch = source[i]
        nxt = source[i + 1] if i + 1 < len(source) else ""
        if state == "line_comment":
            if ch == "\n":
                state = "code"
        elif state == "block_comment":
            if ch == "*" and nxt == "/":
                state = "code"
                i += 1
        elif state == "string":
            if ch == "\\":
                i += 1
            elif ch == '"':
                state = "code"
        elif state == "char":
            if ch == "\\":
                i += 1
            elif ch == "'":
                state = "code"
        elif ch == "/" and nxt == "/":
            state = "line_comment"
            i += 1
        elif ch == "/" and nxt == "*":
            state = "block_comment"
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
                return source[start : i + 1]
        i += 1
    raise SystemExit(f"unterminated production function: {signature}")


def source_route_checks(source: str) -> None:
    dllmain = function_definition(source, "BOOL APIENTRY DllMain(HMODULE hModule")
    attach = dllmain.index("if (reason == DLL_PROCESS_ATTACH)")
    gate = dllmain.index("const auto startup = fork_startup::CheckProductionStartup", attach)
    rejection = dllmain.index("if (startup.state != fork_startup::StartupState::Authorized)", gate)
    return_false = dllmain.index("return FALSE;", rejection)
    first_old_effect = min(dllmain.index(token, attach) for token in (
        "DisableThreadLibraryCalls(hModule)", "g_selfModule = hModule", "InitLogDir();",
        "InitializeCriticalSection(&g_logCs)", "ReportUnavailableCrashWatch();", "PinFpu();",
        "Install();", "Log(", "InterlockedExchange(&g_dllReady, 1)"))
    if not (gate < rejection < return_false < first_old_effect):
        raise SystemExit("production startup gate does not reject before all old attach effects")
    attach_body = dllmain[attach:]
    first_statement = re.sub(r"^\s*if\s*\(reason == DLL_PROCESS_ATTACH\)\s*\{", "",
        attach_body, count=1).lstrip()
    if not first_statement.startswith("const auto startup = fork_startup::CheckProductionStartup"):
        raise SystemExit("startup preflight must be the first PROCESS_ATTACH statement")
    preserved = [
        "DisableThreadLibraryCalls(hModule)", "g_selfModule = hModule", "InitLogDir();",
        "InitializeCriticalSection(&g_logCs)", "ReportUnavailableCrashWatch();", "PinFpu();",
        "ok = Install();", "InterlockedExchange(&g_dllReady, 1)"
    ]
    positions = [dllmain.index(token, return_false) for token in preserved]
    if positions != sorted(positions):
        raise SystemExit("authorized attach effects changed their existing relative order")
    if "InstallCrashWatch();" in dllmain:
        raise SystemExit("production DllMain must not call the retained private CrashWatch installer")
    if not re.search(r"ReportUnavailableCrashWatch\(\);\s*PinFpu\(\);", dllmain):
        raise SystemExit("optional diagnostics skip must immediately precede mandatory PinFpu")
    if len(re.findall(r"\bInstallCrashWatch\s*\(", source)) != 1:
        raise SystemExit("original private CrashWatch installer must remain definition-only")
    for api in ("AddVectoredExceptionHandler", "SetUnhandledExceptionFilter", "SetThreadStackGuarantee"):
        if re.search(rf"\b{api}\s*\(", dllmain):
            raise SystemExit(f"DllMain gained a CrashWatch registration bypass: {api}")
    for token in ("DLL_THREAD_ATTACH", "DLL_THREAD_DETACH"):
        if token in dllmain:
            raise SystemExit("thread notification behavior unexpectedly expanded")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-root", type=pathlib.Path,
        default=pathlib.Path("%LOCALAPPDATA%/V2DLLFork/R02d/startup-regression"))
    parser.add_argument("--msbuild", default=r"C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe")
    args = parser.parse_args()
    repo = pathlib.Path(__file__).resolve().parents[2]
    local_app_data = os.environ.get("LOCALAPPDATA")
    if not local_app_data:
        raise SystemExit("LOCALAPPDATA is required for the private R02d build root")
    build = pathlib.Path(str(args.build_root).replace("%LOCALAPPDATA%", local_app_data)).resolve()
    if build == repo or repo in build.parents or build.exists():
        raise SystemExit(f"build root must be new and outside the repository: {build}")
    build.mkdir(parents=True)

    tech_path = repo / "Source/V2/V2/V2TechButton.cpp"
    gate_h = repo / "Source/V2/V2/Fork/StartupGate.h"
    gate_cpp = repo / "Source/V2/V2/Fork/StartupGate.cpp"
    identity_h = repo / "Source/V2/V2/Fork/StartupIdentity.h"
    identity_cpp = repo / "Source/V2/V2/Fork/StartupIdentity.cpp"
    test_cpp = repo / "tests/fork/startup_gate_tests.cpp"
    tech = tech_path.read_text(encoding="utf-8-sig")
    gate = gate_h.read_text(encoding="utf-8-sig")
    gate_source = gate_cpp.read_text(encoding="utf-8-sig")
    source_route_checks(tech)
    if "return {StartupState::Withheld," not in gate_source or "Authorized" not in gate:
        raise SystemExit("pure positive gate must remain explicitly withheld")
    if "M0 crash diagnostics explicitly blocked;" not in gate_source or "M1 imported/deferred TBB/CRT/D3D" not in gate_source or "M2/M3" not in gate_source:
        raise SystemExit("positive preflight reason must enumerate mandatory withheld groups")

    dllmain = function_definition(tech, "BOOL APIENTRY DllMain(HMODULE hModule")
    # Route the extracted production body through the pure-gate fixture wrapper.
    # The actual CheckProductionStartup implementation is also compiled below
    # under a distinct name and tested directly for loader-mode rejection.
    dllmain = dllmain.replace("fork_startup::CheckProductionStartup", "TestCheckProductionStartup")
    dllmain = dllmain.replace("fork_startup::ReportStartupDecision", "TestReportStartupDecision")
    template = test_cpp.read_text(encoding="utf-8-sig")
    marker = "// ACTUAL_DLLMAIN_INJECTED"
    if template.count(marker) != 1:
        raise SystemExit("C++ route fixture must contain exactly one DllMain insertion point")
    generated = template.replace(marker, dllmain)
    harness_path = build / "startup_gate_contract.cpp"
    harness_path.write_text(generated, encoding="utf-8")
    source_files = (tech_path, gate_h, gate_cpp, identity_h, identity_cpp, test_cpp)
    hashes = "\n".join(
        f"{p.relative_to(repo)} {hashlib.sha256(p.read_bytes()).hexdigest()}"
        for p in source_files
    ) + "\n"
    (build / "source_hashes.txt").write_text(hashes, encoding="utf-8")
    inventory = """R02c routine startup inventory (source symbol/call-order summary)

M0: DllMain PROCESS_ATTACH -> ReportUnavailableCrashWatch -> PinFpu. Optional
crash-reporting registrations remain only in the private, unreachable
InstallCrashWatch definition; the skip logs one explicit blocked-profile reason.
Mandatory PinFpu remains immediately afterward and before Install.

M1: Install -> InstallEngineStability first. It pins FPU, sets timer resolution,
may install select/high-priority/LFH hooks, hooks exe CreateThread/LoadLibraryA/W/
GetTickCount IAT entries, installs main-loop FPU/sleep patches, then calls
TryPatchLateModules for TBB and D3D discovery. Imported-module, TBB/CRT/D3D closure
remains explicitly withheld by the production gate.

M2: InstallPopQuantize (RVA 0x85E40, signature 55 8B EC 64 A1 00 00 00 00) is
called after EngineStability. Its post-call thunk pins FPU then quantizes POPs.

M3: InstallOosWatch (RVA 0x282EC0, wrapper HookOosReport -> AccChkEnter ->
StateSavingsNormalize) and the daily/savings dependencies are in Install's later
sequence. ChecksumDiagnostic and LobbyEntry are separate parent dependencies.

No source/runtime claim for unlisted mandatory or optional installers is made.
"""
    (build / "startup_inventory.txt").write_text(inventory, encoding="utf-8")

    source_dir = repo / "Source/V2/V2"
    project = f'''<Project DefaultTargets="Build" xmlns="http://schemas.microsoft.com/developer/msbuild/2003">
<ItemGroup Label="ProjectConfigurations"><ProjectConfiguration Include="Release|Win32"><Configuration>Release</Configuration><Platform>Win32</Platform></ProjectConfiguration></ItemGroup>
<PropertyGroup Label="Globals"><WindowsTargetPlatformVersion>10.0.26100.0</WindowsTargetPlatformVersion></PropertyGroup>
<Import Project="$(VCTargetsPath)\\Microsoft.Cpp.Default.props" />
<PropertyGroup Label="Configuration"><ConfigurationType>Application</ConfigurationType><PlatformToolset>v143</PlatformToolset><UseDebugLibraries>false</UseDebugLibraries><WholeProgramOptimization>true</WholeProgramOptimization></PropertyGroup>
<Import Project="$(VCTargetsPath)\\Microsoft.Cpp.props" />
<PropertyGroup><OutDir>$(ProjectDir)artifacts\\</OutDir><IntDir>$(ProjectDir)obj\\</IntDir><TargetName>startup_gate_contract</TargetName></PropertyGroup>
<ItemDefinitionGroup><ClCompile><WarningLevel>Level4</WarningLevel><TreatWarningAsError>true</TreatWarningAsError><Optimization>MaxSpeed</Optimization><LanguageStandard>stdcpp20</LanguageStandard><RuntimeLibrary>MultiThreaded</RuntimeLibrary><PreprocessorDefinitions>WIN32;_CONSOLE</PreprocessorDefinitions><AdditionalIncludeDirectories>{source_dir}</AdditionalIncludeDirectories></ClCompile></ItemDefinitionGroup>
<ItemGroup><ClCompile Include="startup_gate_contract.cpp" /><ClCompile Include="{identity_cpp}" /></ItemGroup>
<Import Project="$(VCTargetsPath)\\Microsoft.Cpp.targets" /></Project>'''
    project_path = build / "startup_gate_contract.vcxproj"
    project_path.write_text(project, encoding="utf-8")
    result = subprocess.run([args.msbuild, str(project_path), "/t:Rebuild",
        "/p:Configuration=Release", "/p:Platform=Win32", "/v:minimal"],
        capture_output=True, text=True)
    (build / "build.log").write_text(result.stdout + result.stderr, encoding="utf-8")
    if result.returncode:
        print(result.stdout, result.stderr)
        raise SystemExit(result.returncode)
    result = subprocess.run([str(build / "artifacts/startup_gate_contract.exe")],
        capture_output=True, text=True)
    (build / "test.log").write_text(result.stdout + result.stderr, encoding="utf-8")
    print(result.stdout, end="")
    raise SystemExit(result.returncode)


if __name__ == "__main__":
    main()
