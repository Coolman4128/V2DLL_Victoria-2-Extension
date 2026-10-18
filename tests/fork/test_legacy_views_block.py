"""Compile extracted production VIEWS-blocking paths in an isolated Win32 harness.

The harness exercises source-level routing and memory effects. It does not certify
the live DLL, native callback ABI/lifetime, or a supported VIEWS installation.
"""
import argparse
import hashlib
import os
import pathlib
import re
import subprocess


def function_definition(source: str, signature: str) -> str:
    """Return one actual C++ function definition, balancing its body braces."""
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


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-root", type=pathlib.Path,
                        default=pathlib.Path("%LOCALAPPDATA%/V2DLLFork/R02b/views-block-contract/"))
    parser.add_argument("--msbuild", default=r"C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe")
    args = parser.parse_args()
    repo = pathlib.Path(__file__).resolve().parents[2]
    local_app_data = os.environ.get("LOCALAPPDATA")
    if not local_app_data:
        raise SystemExit("LOCALAPPDATA is required for the private R02b build root")
    build = pathlib.Path(str(args.build_root).replace("%LOCALAPPDATA%", local_app_data)).resolve()
    if build == repo or repo in build.parents:
        raise SystemExit("build root must be outside the repository")
    if build.exists():
        raise SystemExit(f"build root must be new: {build}")
    build.mkdir(parents=True)

    tech_path = repo / "Source/V2/V2/V2TechButton.cpp"
    modloader_path = repo / "Source/V2/V2/V2DLL_ModLoader.cpp"
    tech = tech_path.read_text(encoding="utf-8-sig")
    modloader = modloader_path.read_text(encoding="utf-8-sig")
    patch_slot = function_definition(tech, "bool PatchSlot(DWORD rvaVtable, int slotIndex, void* replacement, void** outOriginal)")
    trampoline = function_definition(tech, "bool StealToTrampoline(DWORD rva, unsigned steal, unsigned char* tramp, int trampSize,")
    skip_helper = function_definition(tech, "static void InstallOptionalViewsFamily()")
    api_patch = function_definition(modloader, "static int __cdecl ApiPatchSlot(uint32_t rvaVtable, int slotIndex,")
    api_trampoline = function_definition(modloader, "static int __cdecl ApiStealToTrampoline(uint32_t rva, unsigned steal,")
    install = function_definition(tech, "static bool Install()")

    if tech.count("InstallOptionalViewsFamily();") != 1 or install.count("InstallOptionalViewsFamily();") != 1:
        raise SystemExit("production source must call InstallOptionalViewsFamily exactly once")
    if "for (int i = 0; i < VIEW_COUNT" in install or re.search(r"PatchSlot\s*\(\s*VIEWS\s*\[", install):
        raise SystemExit("legacy VIEWS PatchSlot loop remains in Install")
    if "PatchSlot" in skip_helper or "VirtualProtect" in skip_helper or "memcpy" in skip_helper or "g_origSlot" in skip_helper:
        raise SystemExit("skip helper contains a mutation path")
    if len(re.findall(r"\bLog\s*\(", skip_helper)) != 1:
        raise SystemExit("skip helper must emit exactly one truthful log")
    helper_body = skip_helper[skip_helper.index("{") + 1 : -1]
    helper_body = re.sub(r"/\*.*?\*/|//[^\n]*", "", helper_body, flags=re.S).strip()
    if not re.fullmatch(r'Log\s*\(\s*(?:"[^"\n]*"\s*)+\)\s*;', helper_body):
        raise SystemExit("skip helper must contain only its single literal Log call")
    expected_reason = "optional four-callback family blocked (R02b)"
    if expected_reason not in skip_helper or "no slots changed" not in skip_helper:
        raise SystemExit("skip helper log does not state the blocked family and zero-write outcome")

    harness = r'''#include <windows.h>
#include <cstdint>
#include <climits>
#include <cstdio>
#include <cstdarg>
#include <cstring>
#include <cassert>
#include "Fork/ViewsBoundary.h"
#include "hde32.h"
#ifdef NDEBUG
#error Assertions must stay enabled in the Release contract harness.
#endif

static unsigned protectionCalls = 0;
static unsigned flushCalls = 0;
static char lastLog[512] = {};
static unsigned logCalls = 0;
DWORD g_base = 0;
DWORD g_imageSize = 0;
void Log(const char* format, ...) {
    ++logCalls;
    va_list args; va_start(args, format);
    vsnprintf_s(lastLog, sizeof(lastLog), _TRUNCATE, format, args);
    va_end(args);
}
BOOL TestVirtualProtect(void*, SIZE_T, DWORD, DWORD* prior) {
    ++protectionCalls;
    if (prior) *prior = PAGE_READWRITE;
    return TRUE;
}
BOOL TestFlushInstructionCache(HANDLE, const void*, SIZE_T) { ++flushCalls; return TRUE; }
void LogDbg(const char*, ...) {}
#define VirtualProtect TestVirtualProtect
#define FlushInstructionCache TestFlushInstructionCache
'''
    harness += "\n" + patch_slot + "\n" + trampoline + "\n" + api_patch + "\n" + api_trampoline + "\n" + skip_helper
    harness += r'''
int main() {
    constexpr DWORD imageSize = 0xA18000;
    auto* image = static_cast<unsigned char*>(VirtualAlloc(nullptr, imageSize,
        MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
    assert(image != nullptr);
    g_base = static_cast<DWORD>(reinterpret_cast<uintptr_t>(image));
    g_imageSize = imageSize;
    assert(reinterpret_cast<uintptr_t>(image) <= 0xFFFFFFFFu);
    std::memset(image, 0x5A, imageSize);
    const uint32_t reserved[] = {0xA17FD0u, 0xA05A18u, 0xA0FEF4u, 0xA0E480u};
    unsigned helperAliasProbes = 0;
    unsigned negativeIndexProbes = 0;
    unsigned indexWrapProbes = 0;
    for (auto target : reserved) {
        for (int alias = -3; alias <= 3; ++alias) {
            // Use slot zero and shift the vtable RVA to exercise every byte alias.
            assert(fork_boundary::BlocksLegacyViewsSlot(target + alias, 0));
            ++helperAliasProbes;
        }
        // Negative indices and uint32 multiplication wrap must reach the same span.
        assert(fork_boundary::BlocksLegacyViewsSlot(target + 12u, -3));
        ++negativeIndexProbes;
        assert(fork_boundary::BlocksLegacyViewsSlot(target - 0xFFFFFFFCu, -1));
        ++indexWrapProbes;
        assert(fork_boundary::BlocksLegacyViewsSlot(
            target - static_cast<uint32_t>(INT_MAX) * 4u, INT_MAX));
        ++indexWrapProbes;
    }

    unsigned char* slot = image + 0x2000;
    const uintptr_t prior = 0x13572468u;
    *reinterpret_cast<void**>(slot) = reinterpret_cast<void*>(prior);
    void* original = reinterpret_cast<void*>(0x24681357u);
    unsigned char* before = new unsigned char[imageSize];
    std::memcpy(before, image, imageSize);
    protectionCalls = 0;
    flushCalls = 0;
    unsigned patchSlotRejects = 0;
    unsigned apiPatchRejects = 0;
    // Every reserved slot and alias is rejected before access/protection, including
    // calls through the exported plugin wrapper with null replacement/output.
    for (auto target : reserved) {
        for (int alias = -3; alias <= 3; ++alias) {
            assert(!PatchSlot(target + alias, 0, reinterpret_cast<void*>(0x1111), &original));
            ++patchSlotRejects;
            assert(original == reinterpret_cast<void*>(0x24681357u));
            assert(ApiPatchSlot(target + alias, 0, nullptr, nullptr) == 0);
            ++apiPatchRejects;
        }
        assert(!PatchSlot(target + 12u, -3, nullptr, nullptr));
        ++patchSlotRejects;
        assert(ApiPatchSlot(target - 0xFFFFFFFCu, -1, nullptr, nullptr) == 0);
        ++apiPatchRejects;
    }
    assert(protectionCalls == 0 && flushCalls == 0);
    assert(std::memcmp(before, image, imageSize) == 0);
    assert(*reinterpret_cast<void**>(slot) == reinterpret_cast<void*>(prior));
    assert(original == reinterpret_cast<void*>(0x24681357u));

    // A normal target cannot publish its original pointer into any byte of the
    // reserved callback spans, including through the plugin API wrapper.
    unsigned outputPatchRejects = 0;
    unsigned outputApiRejects = 0;
    for (auto target : reserved) {
        for (int alias = -3; alias <= 3; ++alias) {
            std::memcpy(before, image, imageSize);
            void** outputAlias = reinterpret_cast<void**>(image + target + alias);
            protectionCalls = 0;
            assert(!PatchSlot(0x2000u, 0, reinterpret_cast<void*>(0x1111), outputAlias));
            assert(ApiPatchSlot(0x2000u, 0, nullptr, outputAlias) == 0);
            assert(protectionCalls == 0 && flushCalls == 0);
            assert(std::memcmp(before, image, imageSize) == 0);
            ++outputPatchRejects;
            ++outputApiRejects;
        }
    }

    // Denial occurs before address formation/dereference even with an unrelated
    // base that would make the wrapped native pointer invalid in this process.
    const DWORD savedBase = g_base;
    g_base = 0xFFFFFF00u;
    void* untouchedOriginal = reinterpret_cast<void*>(0x24681357u);
    protectionCalls = 0;
    assert(!PatchSlot(0xA17FDCu, -3, nullptr, &untouchedOriginal));
    assert(ApiPatchSlot(0xA17FDCu, -3, nullptr, &untouchedOriginal) == 0);
    assert(!StealToTrampoline(0xA17FD0u, 5, nullptr, 0, nullptr, nullptr,
        &untouchedOriginal, "wrapped base blocked"));
    assert(protectionCalls == 0 && flushCalls == 0);
    assert(untouchedOriginal == reinterpret_cast<void*>(0x24681357u));
    g_base = savedBase;

    // The raw trampoline API also receives legacy writes, so source, trampoline,
    // and original-output spans are checked before any protection or flush call.
    unsigned char expected[8] = {0x55, 0x8B, 0xEC, 0x6A, 0xFF, 0x90, 0x90, 0x90};
    std::memcpy(image + 0x4000, expected, sizeof(expected));
    const uint32_t reservedForWrites[] = {0xA17FD0u, 0xA05A18u, 0xA0FEF4u, 0xA0E480u};
    unsigned trampolineRejects = 0;
    for (auto target : reservedForWrites) {
        for (int alias = -3; alias <= 3; ++alias) {
            for (int kind = 0; kind < 3; ++kind) {
                std::memcpy(before, image, imageSize);
                unsigned char* candidateTramp = image + 0x5000;
                void** candidateOrig = reinterpret_cast<void**>(image + 0x6000);
                DWORD sourceRva = 0x4000;
                if (kind == 0) {
                    sourceRva = target + alias;
                    std::memcpy(image + sourceRva, expected, 5);
                    std::memcpy(before, image, imageSize);
                }
                if (kind == 1) candidateTramp = image + target + alias;
                if (kind == 2) candidateOrig = reinterpret_cast<void**>(image + target + alias);
                void* origBefore = *candidateOrig;
                protectionCalls = flushCalls = 0;
                assert(!StealToTrampoline(sourceRva, 5, candidateTramp, 32,
                    expected, reinterpret_cast<void*>(0x7777), candidateOrig, "VIEWS-block"));
                assert(ApiStealToTrampoline(sourceRva, 5, candidateTramp, 32,
                    expected, reinterpret_cast<void*>(0x7777), candidateOrig, "VIEWS-block") == 0);
                assert(protectionCalls == 0 && flushCalls == 0);
                assert(*candidateOrig == origBefore);
                assert(std::memcmp(before, image, imageSize) == 0);
                ++trampolineRejects;
            }
        }
    }
    // Span-length cases exercise overlap beyond the starts tested above: source
    // crosses one reserved byte, trampoline's actual first 10 written bytes
    // cross, and its declared full 32-byte capacity reaches the slot at byte 31.
    unsigned spanLengthRejects = 0;
    for (auto target : reservedForWrites) {
        for (int kind = 0; kind < 3; ++kind) {
            DWORD sourceRva = 0x4000u;
            unsigned char* candidateTramp = image + 0x5000;
            int capacity = 32;
            void** candidateOrig = reinterpret_cast<void**>(image + 0x6000);
            if (kind == 0) {
                sourceRva = target - 4u;
                std::memcpy(image + sourceRva, expected, 5);
            } else if (kind == 1) {
                candidateTramp = image + target - 9u;
            } else {
                candidateTramp = image + target - 31u;
                capacity = 32;
            }
            std::memcpy(before, image, imageSize);
            void* origBefore = *candidateOrig;
            protectionCalls = flushCalls = 0;
            assert(!StealToTrampoline(sourceRva, 5, candidateTramp, capacity,
                expected, reinterpret_cast<void*>(0x7777), candidateOrig, "VIEWS-span"));
            assert(ApiStealToTrampoline(sourceRva, 5, candidateTramp, capacity,
                expected, reinterpret_cast<void*>(0x7777), candidateOrig, "VIEWS-span") == 0);
            assert(protectionCalls == 0 && flushCalls == 0);
            assert(*candidateOrig == origBefore);
            assert(std::memcmp(before, image, imageSize) == 0);
            ++spanLengthRejects;
        }
    }
    // Crossing spans and PE32 modulo arithmetic must detect reserved bytes even
    // when the requested write starts outside the canonical four-byte span.
    for (auto target : reservedForWrites) {
        assert(fork_boundary::BlocksLegacyViewsWrite(target - 2u, 5));
        assert(fork_boundary::BlocksLegacyViewsWrite(target + 2u, 5));
        assert(!fork_boundary::BlocksLegacyViewsWrite(target - 4u, 4));
        assert(!fork_boundary::BlocksLegacyViewsWrite(target + 4u, 4));
        assert(!fork_boundary::BlocksLegacyViewsWrite(target, 0));
    }
    assert(!fork_boundary::BlocksLegacyViewsWrite(0xFFFFFFFEu, 4));
    assert(fork_boundary::BlocksLegacyViewsWrite(0xFFFFFFFEu, 0xA17FD4u));
    assert(fork_boundary::BlocksLegacyViewsSlot(0xA17FDCu, -3));
    assert(fork_boundary::BlocksLegacyViewsSlot(0xA17FC4u, 3));
    assert(fork_boundary::BlocksLegacyViewsSlot(0xA17FD4u, -1));
    assert(fork_boundary::BlocksLegacyViewsSlot(0xA17FD0u - static_cast<uint32_t>(INT_MAX) * 4u, INT_MAX));
    std::memcpy(before, image, imageSize);
    protectionCalls = flushCalls = 0;
    assert(!StealToTrampoline(0x4000u, 5, image + 0xA17FCEu, 32,
        expected, reinterpret_cast<void*>(0x7777), reinterpret_cast<void**>(image + 0x6000), "crossing"));
    assert(protectionCalls == 0 && flushCalls == 0);
    assert(std::memcmp(before, image, imageSize) == 0);

    // Nearby unrelated PatchSlot and a concrete Budget slot6 control still use
    // the normal helper. The mock protection callback permits only these writes.
    void* controlOriginal = nullptr;
    void* replacement = reinterpret_cast<void*>(0x76543210u);
    assert(PatchSlot(0x2000u, 0, replacement, &controlOriginal));
    assert(controlOriginal == reinterpret_cast<void*>(prior));
    assert(*reinterpret_cast<void**>(slot) == replacement);
    assert(protectionCalls == 2);
    std::memcpy(slot, &prior, sizeof(prior));
    void** budgetSlot = reinterpret_cast<void**>(image + 0xA059F0u + 6u * 4u);
    uintptr_t budgetSample = 0;
    std::memcpy(&budgetSample, budgetSlot, sizeof(budgetSample));
    assert(budgetSample == 0x5A5A5A5Au);
    const uintptr_t budgetPrior = 0x10203040u;
    *budgetSlot = reinterpret_cast<void*>(budgetPrior);
    assert(PatchSlot(0xA059F0u, 6, replacement, &controlOriginal));
    assert(controlOriginal == reinterpret_cast<void*>(budgetPrior));
    assert(*budgetSlot == replacement);
    assert(protectionCalls == 4);
    std::memcpy(budgetSlot, &budgetSample, sizeof(budgetSample));
    assert(*budgetSlot == reinterpret_cast<void*>(budgetSample));

    // Immediate four-byte adjacency is allowed by the span guard, and the
    // helper still performs its ordinary write/protection path there.
    for (auto target : reserved) {
        const DWORD beforeCalls = protectionCalls;
        void* adjacentOriginal = nullptr;
        void* expectedAdjacentOriginal = nullptr;
        std::memcpy(&expectedAdjacentOriginal, before + target - 4u, sizeof(void*));
        assert(PatchSlot(target - 4u, 0, replacement, &adjacentOriginal));
        assert(adjacentOriginal == expectedAdjacentOriginal);
        assert(PatchSlot(target + 4u, 0, replacement, nullptr));
        assert(protectionCalls == beforeCalls + 4);
        std::memcpy(image + target - 4u, before + target - 4u, sizeof(void*));
        std::memcpy(image + target + 4u, before + target + 4u, sizeof(void*));
    }
    std::memcpy(before, image, imageSize);
    logCalls = 0;
    InstallOptionalViewsFamily();
    assert(logCalls == 1);
    assert(std::strstr(lastLog, "optional four-callback family blocked (R02b)") != nullptr);
    assert(std::strstr(lastLog, "no slots changed") != nullptr);
    assert(protectionCalls == 20);
    assert(std::memcmp(before, image, imageSize) == 0);

    // Prove the actual extracted trampoline helper reaches mutation for a valid,
    // unreserved fixture when mock protection succeeds. This is not native code
    // execution; it is a guard-removal sensitivity control for the deny tests.
    std::memcpy(before, image, imageSize);
    unsigned char* successSource = image + 0x7000;
    unsigned char* successTrampoline = image + 0x8000;
    void** successOriginal = reinterpret_cast<void**>(image + 0x9000);
    std::memcpy(successSource, expected, 5);
    std::memset(successTrampoline, 0xCC, 32);
    *successOriginal = reinterpret_cast<void*>(0x42424242u);
    std::memcpy(before, image, imageSize);
    protectionCalls = flushCalls = 0;
    assert(StealToTrampoline(0x7000u, 5, successTrampoline, 32, expected,
        reinterpret_cast<void*>(0x7777), successOriginal, "unreserved-control"));
    assert(protectionCalls == 3 && flushCalls == 2);
    assert(std::memcmp(successTrampoline, expected, 5) == 0);
    assert(successTrampoline[5] == 0xE9);
    assert(*successOriginal == successTrampoline);
    assert(successSource[0] == 0xE9);
    assert(std::memcmp(successSource, expected, 5) != 0);
    assert(std::memcmp(before, image, imageSize) != 0);
    std::memcpy(image, before, imageSize);
    assert(std::memcmp(before, image, imageSize) == 0);

    std::printf("PASS: helper alias=%u, negative-index=%u, index-wrap=%u; PatchSlot/API blocked-slot rejects=%u/%u; PatchSlot/API output-span rejects=%u/%u; StealToTrampoline/API byte-alias rejects=%u/%u each; span-length rejects=%u/%u each; zero protection/flush and unchanged memory on blocked calls; unreserved trampoline mutation control passed and was restored; adjacent controls and real Budget slot6 passed; skip helper logged once with no mutation\n", helperAliasProbes, negativeIndexProbes, indexWrapProbes, patchSlotRejects, apiPatchRejects, outputPatchRejects, outputApiRejects, trampolineRejects, trampolineRejects, spanLengthRejects, spanLengthRejects);
    delete[] before;
    VirtualFree(image, 0, MEM_RELEASE);
}
'''
    # va_list is used by the production-shaped Log stub above.
    (build / "views_block_contract.cpp").write_text(harness, encoding="utf-8")
    source_hashes = {str(p.relative_to(repo)): hashlib.sha256(p.read_bytes()).hexdigest()
                     for p in (tech_path, modloader_path,
                               repo / "Source/V2/V2/Fork/ViewsBoundary.cpp",
                               repo / "Source/V2/V2/Fork/ViewsBoundary.h",
                               repo / "Source/V2/V2/Fork/AddressProfile.cpp")}
    (build / "source_hashes.txt").write_text("\n".join(f"{k} {v}" for k, v in source_hashes.items()) + "\n", encoding="utf-8")

    hde = repo / "Source/V2/V2/minhook/src/HDE"
    project = f'''<Project DefaultTargets="Build" xmlns="http://schemas.microsoft.com/developer/msbuild/2003">
<ItemGroup Label="ProjectConfigurations"><ProjectConfiguration Include="Release|Win32"><Configuration>Release</Configuration><Platform>Win32</Platform></ProjectConfiguration></ItemGroup>
<PropertyGroup Label="Globals"><WindowsTargetPlatformVersion>10.0.26100.0</WindowsTargetPlatformVersion></PropertyGroup>
<Import Project="$(VCTargetsPath)\\Microsoft.Cpp.Default.props" />
<PropertyGroup Label="Configuration"><ConfigurationType>Application</ConfigurationType><PlatformToolset>v143</PlatformToolset><UseDebugLibraries>false</UseDebugLibraries><WholeProgramOptimization>true</WholeProgramOptimization></PropertyGroup>
<Import Project="$(VCTargetsPath)\\Microsoft.Cpp.props" />
<PropertyGroup><OutDir>$(ProjectDir)artifacts\\</OutDir><IntDir>$(ProjectDir)obj\\</IntDir><TargetName>views_block_contract</TargetName></PropertyGroup>
<ItemDefinitionGroup><ClCompile><WarningLevel>Level4</WarningLevel><TreatWarningAsError>true</TreatWarningAsError><Optimization>MaxSpeed</Optimization><LanguageStandard>stdcpp20</LanguageStandard><RuntimeLibrary>MultiThreaded</RuntimeLibrary><PreprocessorDefinitions>WIN32;_CONSOLE</PreprocessorDefinitions><AdditionalIncludeDirectories>{repo / 'Source/V2/V2'};{repo / 'Source/V2/V2/minhook/src/HDE'}</AdditionalIncludeDirectories></ClCompile></ItemDefinitionGroup>
<ItemGroup><ClCompile Include="views_block_contract.cpp" /><ClCompile Include="{repo / 'Source/V2/V2/Fork/ViewsBoundary.cpp'}" /><ClCompile Include="{repo / 'Source/V2/V2/Fork/AddressProfile.cpp'}" /><ClCompile Include="{hde / 'hde32.c'}" /></ItemGroup>
<Import Project="$(VCTargetsPath)\\Microsoft.Cpp.targets" /></Project>'''
    project_path = build / "views_block_contract.vcxproj"
    project_path.write_text(project, encoding="utf-8")
    result = subprocess.run([args.msbuild, str(project_path), "/t:Rebuild", "/p:Configuration=Release", "/p:Platform=Win32", "/v:minimal"], capture_output=True, text=True)
    (build / "build.log").write_text(result.stdout + result.stderr, encoding="utf-8")
    if result.returncode:
        print(result.stdout, result.stderr)
        raise SystemExit(result.returncode)
    result = subprocess.run([str(build / "artifacts/views_block_contract.exe")], capture_output=True, text=True)
    (build / "test.log").write_text(result.stdout + result.stderr, encoding="utf-8")
    print(result.stdout, end="")
    raise SystemExit(result.returncode)


if __name__ == "__main__":
    main()
