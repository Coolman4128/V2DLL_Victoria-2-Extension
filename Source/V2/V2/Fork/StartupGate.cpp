#include "StartupGate.h"
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <limits>

#if !defined(_M_IX86)
#error This startup gate supports Release Win32 only.
#endif

namespace fork_startup {
namespace {
constexpr unsigned char ExecutableSha[32] = {
    0x62,0xD4,0x8C,0x20,0x43,0x64,0xDD,0x70,0x65,0x84,0x77,0x7C,0x2E,0x2B,0x3C,0x7A,
    0xB3,0xC5,0xF1,0xDD,0x01,0x70,0x87,0x25,0x54,0x94,0x35,0x75,0xD5,0x3D,0x66,0x48
};

// Immutable S02/frozen-boundaries/probe.json (EBB81E...A1E8CE) and reviewed
// native instruction/caller facts. This is a bounded preflight catalog, NOT the
// complete active native profile. Exactly three absolute SEH operands relocate.
constexpr StartupSite Sites[] = {
    {"POP entry",0x85E40,9,{0x55,0x8B,0xEC,0x64,0xA1,0,0,0,0},-1,0},
    {"POP resume",0x85E49,2,{0x6A,0xFF},-1,0},
    {"POP SEH operand",0x85E4B,5,{0x68,0x34,0x05,0xB6,0},1,0x760534},
    {"POP known return",0x8740B,3,{0xC2,0x18,0},-1,0},
    {"POP known caller",0x8468F,5,{0xE8,0xAC,0x17,0,0},-1,0},
    {"main-loop entry",0x5DF550,5,{0x55,0x8B,0xEC,0x6A,0xFF},-1,0},
    {"main-loop resume",0x5DF555,5,{0x68,0x38,0xBA,0xB3,0},1,0x73BA38},
    {"main-loop known caller",0x57E0C8,5,{0xE8,0x83,0x14,0x06,0},-1,0},
    {"daily/savings entry",0x282EC0,5,{0x55,0x8B,0xEC,0x6A,0xFF},-1,0},
    {"daily/savings resume",0x282EC5,5,{0x68,0x96,0xA8,0xBB,0},1,0x7BA896},
    {"daily post-SEH",0x282ED8,11,{0x81,0xEC,0x40,0x01,0,0,0x53,0x56,0x8B,0x75,0x08},-1,0},
    {"daily known return",0x283CCE,3,{0xC2,0x08,0},-1,0},
    {"daily known caller",0x32ECFA,5,{0xE8,0xC1,0x41,0xF5,0xFF},-1,0},
};

StartupDecision Reject(const char* reason, std::uint32_t rva = 0,
    const char* site = nullptr) {
    return {StartupState::Rejected, reason, rva, site};
}

bool Equal(const unsigned char* a, const unsigned char* b, unsigned size) {
    for (unsigned i = 0; i < size; ++i)
        if (a[i] != b[i]) return false;
    return true;
}

std::uint16_t U16(const unsigned char* b) {
    return static_cast<std::uint16_t>(b[0] | (b[1] << 8));
}

std::uint32_t U32(const unsigned char* b) {
    return std::uint32_t(b[0]) | (std::uint32_t(b[1]) << 8) |
        (std::uint32_t(b[2]) << 16) | (std::uint32_t(b[3]) << 24);
}

bool ReadMapped(void* context, std::uint32_t rva, unsigned char* output,
    std::uint32_t size) {
    if (!context || !output || size == 0 || rva >= KnownImageSize ||
        size > KnownImageSize - rva) return false;
    const auto base = reinterpret_cast<std::uintptr_t>(context);
    if (base > std::numeric_limits<std::uintptr_t>::max() - KnownImageSize)
        return false;
    const auto start = base + rva;
    const auto end = start + size;
    for (auto cursor = start; cursor < end;) {
        MEMORY_BASIC_INFORMATION region{};
        if (VirtualQuery(reinterpret_cast<const void*>(cursor), &region,
                sizeof(region)) != sizeof(region) ||
            region.State != MEM_COMMIT || region.Type != MEM_IMAGE ||
            region.AllocationBase != context ||
            (region.Protect & (PAGE_GUARD | PAGE_NOACCESS))) return false;
        const DWORD access = region.Protect & 0xFF;
        if (access != PAGE_READONLY && access != PAGE_READWRITE &&
            access != PAGE_WRITECOPY && access != PAGE_EXECUTE_READ &&
            access != PAGE_EXECUTE_READWRITE && access != PAGE_EXECUTE_WRITECOPY)
            return false;
        const auto regionBase = reinterpret_cast<std::uintptr_t>(region.BaseAddress);
        if (region.RegionSize == 0 || regionBase > cursor ||
            region.RegionSize > std::numeric_limits<std::uintptr_t>::max() - regionBase)
            return false;
        const auto next = regionBase + region.RegionSize;
        if (next <= cursor) return false;
        cursor = next < end ? next : end;
    }
    __try {
        const auto* source = reinterpret_cast<const unsigned char*>(start);
        for (std::uint32_t i = 0; i < size; ++i) output[i] = source[i];
    }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
    return true;
}

wchar_t FoldAscii(wchar_t c) {
    return c >= L'A' && c <= L'Z' ? static_cast<wchar_t>(c + L'a' - L'A') : c;
}

bool ProxyRole(HMODULE self, const wchar_t* exePath, DWORD exeLength) {
    if (!self || GetModuleHandleW(L"lua51.dll") != self) return false;
    wchar_t proxyPath[MAX_PATH]{};
    const DWORD proxyLength = GetModuleFileNameW(self, proxyPath, MAX_PATH);
    if (!proxyLength || proxyLength >= MAX_PATH) return false;
    DWORD directoryLength = exeLength;
    while (directoryLength && exePath[directoryLength - 1] != L'\\' &&
        exePath[directoryLength - 1] != L'/') --directoryLength;
    constexpr wchar_t proxyName[] = L"lua51.dll";
    constexpr DWORD nameLength = 9;
    if (!directoryLength || directoryLength + nameLength != proxyLength) return false;
    for (DWORD i = 0; i < directoryLength; ++i)
        if (FoldAscii(exePath[i]) != FoldAscii(proxyPath[i])) return false;
    for (DWORD i = 0; i < nameLength; ++i)
        if (FoldAscii(proxyPath[directoryLength + i]) != proxyName[i]) return false;
    return true;
}
} // namespace

const StartupSite* StartupSites(std::size_t& count) {
    count = sizeof(Sites) / sizeof(Sites[0]);
    return Sites;
}

const unsigned char* KnownExecutableSha256() { return ExecutableSha; }

StartupDecision CheckStartup(const StartupFacts& facts, StartupRead read, void* context) {
    if (!facts.staticLoad)
        return Reject("unsupported dynamic/late process attach; static loader mode required");
    if (!facts.proxyRoleMatches)
        return Reject("proxy module role/path does not match the executable's lua51.dll");
    if (facts.identityStatus != IdentityReadStatus::Ok)
        return Reject(IdentityReadReason(facts.identityStatus));
    if (facts.executable.size != KnownExecutableSize ||
        !Equal(facts.executable.sha256, ExecutableSha, sizeof(ExecutableSha)))
        return Reject("unknown executable disk identity; mandatory startup withheld");
    if (!facts.mappedBase || facts.mappedBase >
        std::numeric_limits<std::uint32_t>::max() - KnownImageSize || !read)
        return Reject("missing/overflowing mapped PE32 image or read capability");

    unsigned char dos[64]{};
    if (!read(context, 0, dos, sizeof(dos)))
        return Reject("mapped DOS header unreadable");
    if (U16(dos) != IMAGE_DOS_SIGNATURE)
        return Reject("mapped DOS header mismatch");
    const auto peRva = U32(dos + 0x3C);
    unsigned char nt[248]{};
    if (peRva < sizeof(dos) || peRva > 0x1000 - sizeof(nt) ||
        !read(context, peRva, nt, sizeof(nt)))
        return Reject("mapped NT header outside bounded header area or unreadable", peRva);
    if (U32(nt) != IMAGE_NT_SIGNATURE || U16(nt + 4) != IMAGE_FILE_MACHINE_I386 ||
        U32(nt + 8) != 1451925843 || U16(nt + 20) != 224 ||
        U16(nt + 24) != IMAGE_NT_OPTIONAL_HDR32_MAGIC ||
        U32(nt + 24 + 16) != 0x6B0F91 || U32(nt + 24 + 28) != 0x400000 ||
        U32(nt + 24 + 56) != KnownImageSize)
        return Reject("mapped PE32 identity/header mismatch", peRva);

    for (const auto& site : Sites) {
        unsigned char expected[16]{}, actual[16]{};
        for (unsigned i = 0; i < site.size; ++i) expected[i] = site.expected[i];
        if (site.relocationOffset >= 0) {
            const auto pointer = facts.mappedBase + site.absoluteTargetRva;
            for (unsigned i = 0; i < 4; ++i)
                expected[site.relocationOffset + i] = static_cast<unsigned char>(pointer >> (8 * i));
        }
        if (!read(context, site.rva, actual, site.size))
            return Reject("mandatory startup dependency unreadable", site.rva, site.name);
        if (!Equal(actual, expected, site.size))
            return Reject("mandatory startup dependency mismatch", site.rva, site.name);
    }

    // No caller assertion or matching signature may promote this to Authorized.
    // This finite checkpoint cannot deploy a partial baseline without remedies.
    return {StartupState::Withheld,
        "known executable preflight matches; active profile WITHHELD: M0 crash diagnostics explicitly blocked; "
        "M1 imported/deferred TBB/CRT/D3D, M2/M3 native object/phase and other retained "
        "installer dependencies remain unproved; no startup installer was invoked"};
}

StartupDecision CheckProductionStartup(void* self, const void* reserved) {
    if (!reserved)
        return Reject("unsupported dynamic/late process attach; static loader mode required");
    HMODULE exe = GetModuleHandleW(nullptr);
    if (!exe) return Reject("process executable module unavailable");
    wchar_t path[MAX_PATH]{};
    const DWORD length = GetModuleFileNameW(exe, path, MAX_PATH);
    if (!length || length >= MAX_PATH)
        return Reject("process executable path unavailable or truncated");
    StartupFacts facts{};
    facts.staticLoad = true;
    facts.proxyRoleMatches = ProxyRole(static_cast<HMODULE>(self), path, length);
    if (!facts.proxyRoleMatches)
        return Reject("proxy module role/path does not match the executable's lua51.dll");
    facts.identityStatus = ReadFileIdentity(path, KnownExecutableSize, facts.executable);
    facts.mappedBase = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(exe));
    return CheckStartup(facts, ReadMapped, exe);
}

void ReportStartupDecision(const StartupDecision& decision) {
    OutputDebugStringA("V2DLL startup unavailable: ");
    OutputDebugStringA(decision.reason);
    if (decision.failedSite) {
        OutputDebugStringA("; site: ");
        OutputDebugStringA(decision.failedSite);
    }
    OutputDebugStringA("\n");
}

} // namespace fork_startup
