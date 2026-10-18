"""Compile the actual legacy helper in an isolated Win32 rejection harness.

This checks argument rejection only. It does not certify native instructions,
successful installation, relocation, ABI preservation, or legacy rollback.
"""
import argparse
import json
import pathlib
import subprocess
from xml.sax.saxutils import escape


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-root", required=True, type=pathlib.Path)
    parser.add_argument("--inventory", type=pathlib.Path, help="Optional private PE inventory for matched legacy raw spans")
    parser.add_argument("--msbuild", default=r"C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe")
    args = parser.parse_args()
    repo = pathlib.Path(__file__).resolve().parents[2]
    output = args.build_root.resolve()
    if output == repo or repo in output.parents or output.exists():
        raise SystemExit("Build root must be new and outside the repository")
    output.mkdir(parents=True)
    source = (repo / "Source/V2/V2/V2TechButton.cpp").read_text(encoding="utf-8")
    start = source.index("bool StealToTrampoline(DWORD")
    end = source.index("\n}\n", start) + 3
    helper = source[start:end]
    harness = r'''
#include <windows.h>
#include <cstring>
#include <cstdio>
#include <cassert>
#include "hde32.h"
#include "Fork/ViewsBoundary.h"
DWORD g_base = 0, g_imageSize = 0;
static unsigned protectionCalls = 0, flushCalls = 0;
void Log(const char*, ...) {}
void LogDbg(const char*, ...) {}
BOOL TestVirtualProtect(void*, SIZE_T, DWORD, DWORD*) { ++protectionCalls; return FALSE; }
BOOL TestFlushInstructionCache(HANDLE, const void*, SIZE_T) { ++flushCalls; return FALSE; }
#define VirtualProtect TestVirtualProtect
#define FlushInstructionCache TestFlushInstructionCache
'''
    harness += helper
    harness += r'''
int main() {
    unsigned char image[64], trampoline[32], expected[16];
    std::memset(image, 0x90, sizeof image);
    std::memset(trampoline, 0xCC, sizeof trampoline);
    std::memset(expected, 0x90, sizeof expected);
    g_base = reinterpret_cast<DWORD>(image); g_imageSize = sizeof image;
    void* original = reinterpret_cast<void*>(0x1234);
    void* hook = reinterpret_cast<void*>(0x4321);
    unsigned cases = 0;
#define REJECT(...) do { assert(!StealToTrampoline(__VA_ARGS__)); ++cases; } while (0)
    REJECT(16, 6, trampoline, 32, nullptr, hook, &original, nullptr);
    REJECT(16, 6, nullptr, 32, expected, hook, &original, "null buffer");
    REJECT(16, 6, trampoline, 32, expected, nullptr, &original, "null hook");
    REJECT(16, 6, trampoline, 32, expected, hook, nullptr, "null output");
    REJECT(16, 4, trampoline, 32, expected, hook, &original, "short span");
    REJECT(16, 17, trampoline, 32, expected, hook, &original, "long span");
    REJECT(16, 6, trampoline, 10, expected, hook, &original, "short buffer");
    REJECT(16, 6, trampoline, -1, expected, hook, &original, "negative capacity");
    REJECT(64, 6, trampoline, 32, expected, hook, &original, "outside image");
    REJECT(60, 6, trampoline, 32, expected, hook, &original, "crosses image end");
    REJECT(0xFFFFFFFFu, 6, trampoline, 32, expected, hook, &original, "overflow rva");
    g_base = 0;
    REJECT(16, 6, trampoline, 32, expected, hook, &original, "missing base");
    g_base = reinterpret_cast<DWORD>(image); g_imageSize = 0;
    REJECT(16, 6, trampoline, 32, expected, hook, &original, "missing image");
    assert(protectionCalls == 0 && flushCalls == 0);
    assert(original == reinterpret_cast<void*>(0x1234));
    for (auto byte : image) assert(byte == 0x90);
    for (auto byte : trampoline) assert(byte == 0xCC);
    unsigned decoderRejects = 0, decoderPasses = 0;
    auto span = [&](const unsigned char* code, unsigned length, bool decoderPass) {
        std::memcpy(image + 16, code, length);
        unsigned char before[sizeof image]; std::memcpy(before, image, sizeof image);
        protectionCalls = flushCalls = 0;
        assert(!StealToTrampoline(16, length, trampoline, 32, code, hook, &original, "decoder"));
        assert(protectionCalls == (decoderPass ? 1u : 0u));
        assert(flushCalls == 0 && original == reinterpret_cast<void*>(0x1234));
        assert(std::memcmp(before, image, sizeof image) == 0);
        for (auto byte : trampoline) assert(byte == 0xCC);
        if (decoderPass) ++decoderPasses; else ++decoderRejects;
    };
    g_imageSize = sizeof image;
    const unsigned char call[] = {0xE8, 0, 0, 0, 0}; span(call, sizeof call, false);
    const unsigned char jump[] = {0xEB, 0xFE, 0x90, 0x90, 0x90}; span(jump, sizeof jump, false);
    const unsigned char jcc[] = {0x75, 0xFE, 0x90, 0x90, 0x90}; span(jcc, sizeof jcc, false);
    const unsigned char loop[] = {0xE2, 0xFE, 0x90, 0x90, 0x90}; span(loop, sizeof loop, false);
    const unsigned char longJcc[] = {0x0F, 0x85, 0, 0, 0, 0}; span(longJcc, sizeof longJcc, false);
    const unsigned char split[] = {0x55, 0x8B, 0xEC, 0x81, 0xEC}; span(split, sizeof split, false);
    const unsigned char mainLoop[] = {0x55, 0x8B, 0xEC, 0x6A, 0xFF}; span(mainLoop, sizeof mainLoop, true);
    const unsigned char align[] = {0x55, 0x8B, 0xEC, 0x83, 0xE4, 0xF8}; span(align, sizeof align, true);
    const unsigned char seh[] = {0x55, 0x8B, 0xEC, 0x64, 0xA1, 0, 0, 0, 0}; span(seh, sizeof seh, true);
    const unsigned char absolute[] = {0x8B, 0x0D, 0x34, 0x12, 0x40, 0}; span(absolute, sizeof absolute, true);
    // INVENTORY_SPANS
    std::printf("PASS: %u invalid arguments + %u relative/split spans rejected before mutation; %u known/inventoried spans reached mocked protection; source/trampoline/output unchanged\n", cases, decoderRejects, decoderPasses);
}
'''
    fixtures = []
    if args.inventory:
        inventory = json.loads(args.inventory.read_text(encoding="utf-8-sig"))
        for site in inventory["signatures"]:
            if site["kind"] == "StealToTrampoline" and site.get("match"):
                fixtures.append(bytes.fromhex(site["expected"]))
        if not fixtures:
            raise SystemExit("Inventory did not provide any matched raw trampoline spans")
        declarations = []
        for i, code in enumerate(fixtures):
            declaration = ", ".join(hex(byte) for byte in code)
            declarations.append(f"const unsigned char fixture{i}[] = {{{declaration}}}; span(fixture{i}, sizeof fixture{i}, true);")
        harness = harness.replace("// INVENTORY_SPANS", "\n".join(declarations))
        (output / "inventory-input.json").write_text(json.dumps({"source": str(args.inventory.resolve()), "matched_spans": len(fixtures)}, indent=2), encoding="utf-8")
    (output / "legacy_contract.cpp").write_text(harness, encoding="utf-8")
    hde = repo / "Source/V2/V2/minhook/src/HDE"
    project = '''<Project DefaultTargets="Build" xmlns="http://schemas.microsoft.com/developer/msbuild/2003">
<ItemGroup Label="ProjectConfigurations"><ProjectConfiguration Include="Release|Win32"><Configuration>Release</Configuration><Platform>Win32</Platform></ProjectConfiguration></ItemGroup>
<PropertyGroup Label="Globals"><WindowsTargetPlatformVersion>10.0.26100.0</WindowsTargetPlatformVersion></PropertyGroup>
<Import Project="$(VCTargetsPath)\\Microsoft.Cpp.Default.props" />
<PropertyGroup Label="Configuration"><ConfigurationType>Application</ConfigurationType><PlatformToolset>v143</PlatformToolset><UseDebugLibraries>false</UseDebugLibraries></PropertyGroup>
<Import Project="$(VCTargetsPath)\\Microsoft.Cpp.props" />
<PropertyGroup><OutDir>$(ProjectDir)artifacts\\</OutDir><IntDir>$(ProjectDir)obj\\</IntDir><TargetName>legacy_contract</TargetName></PropertyGroup>
<ItemDefinitionGroup><ClCompile><LanguageStandard>stdcpp20</LanguageStandard><RuntimeLibrary>MultiThreaded</RuntimeLibrary><PreprocessorDefinitions>WIN32;_CONSOLE</PreprocessorDefinitions><AdditionalIncludeDirectories>HDE_INCLUDE;V2_INCLUDE</AdditionalIncludeDirectories></ClCompile></ItemDefinitionGroup>
<ItemGroup><ClCompile Include="legacy_contract.cpp" /><ClCompile Include="HDE_SOURCE" /><ClCompile Include="VIEWS_SOURCE" /><ClCompile Include="PROFILE_SOURCE" /></ItemGroup>
<Import Project="$(VCTargetsPath)\\Microsoft.Cpp.targets" /></Project>'''
    project = (project.replace("HDE_INCLUDE", escape(str(hde)))
               .replace("V2_INCLUDE", escape(str(repo / "Source/V2/V2")))
               .replace("HDE_SOURCE", escape(str(hde / "hde32.c")))
               .replace("VIEWS_SOURCE", escape(str(repo / "Source/V2/V2/Fork/ViewsBoundary.cpp")))
               .replace("PROFILE_SOURCE", escape(str(repo / "Source/V2/V2/Fork/AddressProfile.cpp"))))
    project_path = output / "legacy_contract.vcxproj"
    project_path.write_text(project, encoding="utf-8")
    result = subprocess.run([args.msbuild, str(project_path), "/t:Rebuild", "/p:Configuration=Release", "/p:Platform=Win32", "/v:minimal"], capture_output=True, text=True)
    (output / "build.log").write_text(result.stdout + result.stderr, encoding="utf-8")
    if result.returncode:
        print(result.stdout, result.stderr)
        raise SystemExit(result.returncode)
    result = subprocess.run([str(output / "artifacts/legacy_contract.exe")], capture_output=True, text=True)
    (output / "test.log").write_text(result.stdout + result.stderr, encoding="utf-8")
    print(result.stdout, end="")
    raise SystemExit(result.returncode)


if __name__ == "__main__":
    main()
