// Test-only definitions and the extracted production DllMain are injected by
// test_startup_gate.py. This compiles the actual pure gate modules in Win32.
#include <windows.h>
#ifdef max
#undef max
#endif
#ifdef min
#undef min
#endif
#include <array>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

#include "Fork/StartupGate.h"

namespace {
using namespace fork_startup;

constexpr std::uint32_t FixtureSize = KnownImageSize;
struct ImmutablePin {
    const char* name;
    std::uint32_t rva;
    unsigned size;
    unsigned char expected[16];
    int relocationOffset;
    std::uint32_t absoluteTargetRva;
};
// Independent fixture catalog copied from the manager's reviewed, immutable
// R02c dependency ledger. Never derive expected fixture bytes from production.
constexpr ImmutablePin FixturePins[] = {
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
    {"daily known caller",0x32ECFA,5,{0xE8,0xC1,0x41,0xF5,0xFF},-1,0}
};
unsigned checks = 0;
void Check(bool condition, const char* message) {
    ++checks;
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", message);
        std::abort();
    }
}

struct ImageFixture {
    unsigned char* bytes = nullptr;
    std::uint32_t base = 0x00400000;
    std::uint32_t unreadableRva = 0xFFFFFFFFu;
    std::uint32_t mismatchRva = 0xFFFFFFFFu;
    std::uint32_t partialReadRva = 0xFFFFFFFFu;
    std::uint32_t partialReadBytes = 0;
    unsigned reads = 0;

    ImageFixture() {
        bytes = static_cast<unsigned char*>(VirtualAlloc(nullptr, FixtureSize,
            MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
        Check(bytes != nullptr, "allocate owned synthetic mapped image");
        std::memset(bytes, 0, FixtureSize);
        WritePeHeaders();
        PopulateSites();
    }
    ~ImageFixture() {
        if (bytes) Check(VirtualFree(bytes, 0, MEM_RELEASE) != FALSE,
            "free only owned synthetic image");
    }
    ImageFixture(const ImageFixture&) = delete;
    ImageFixture& operator=(const ImageFixture&) = delete;

    void Put16(std::uint32_t rva, std::uint16_t value) {
        bytes[rva] = static_cast<unsigned char>(value);
        bytes[rva + 1] = static_cast<unsigned char>(value >> 8);
    }
    void Put32(std::uint32_t rva, std::uint32_t value) {
        for (unsigned i = 0; i != 4; ++i)
            bytes[rva + i] = static_cast<unsigned char>(value >> (i * 8));
    }
    void WritePeHeaders() {
        Put16(0, IMAGE_DOS_SIGNATURE);
        Put32(0x3C, 0x80);
        constexpr std::uint32_t nt = 0x80;
        Put32(nt, IMAGE_NT_SIGNATURE);
        Put16(nt + 4, IMAGE_FILE_MACHINE_I386);
        Put32(nt + 8, 1451925843);
        Put16(nt + 20, 224);
        Put16(nt + 24, IMAGE_NT_OPTIONAL_HDR32_MAGIC);
        Put32(nt + 24 + 16, 0x6B0F91);
        Put32(nt + 24 + 28, 0x00400000);
        Put32(nt + 24 + 56, KnownImageSize);
    }
    void PopulateSites() {
        std::size_t count = 0;
        const auto* sites = StartupSites(count);
        Check(count == sizeof(FixturePins) / sizeof(FixturePins[0]),
            "production catalog size matches independent fixture ledger");
        for (std::size_t s = 0; s < count; ++s) {
            const auto& site = FixturePins[s];
            const auto& actual = sites[s];
            Check(std::strcmp(site.name, actual.name) == 0 &&
                site.rva == actual.rva && site.size == actual.size &&
                site.relocationOffset == actual.relocationOffset &&
                site.absoluteTargetRva == actual.absoluteTargetRva &&
                std::memcmp(site.expected, actual.expected, site.size) == 0,
                "production pin matches independently reviewed fixture ledger");
            for (unsigned i = 0; i < site.size; ++i)
                bytes[site.rva + i] = site.expected[i];
            if (site.relocationOffset >= 0) {
                const std::uint32_t address = base + site.absoluteTargetRva;
                for (unsigned i = 0; i != 4; ++i)
                    bytes[site.rva + site.relocationOffset + i] =
                        static_cast<unsigned char>(address >> (i * 8));
            }
        }
    }
    static bool Read(void* opaque, std::uint32_t rva,
        unsigned char* output, std::uint32_t size) {
        auto& self = *static_cast<ImageFixture*>(opaque);
        ++self.reads;
        if (!output || !size || rva >= FixtureSize || size > FixtureSize - rva)
            return false;
        if (self.partialReadRva == rva) {
            const auto amount = self.partialReadBytes < size ? self.partialReadBytes : size;
            std::memcpy(output, self.bytes + rva, amount);
            return false;
        }
        if (self.unreadableRva >= rva && self.unreadableRva - rva < size)
            return false;
        std::memcpy(output, self.bytes + rva, size);
        return true;
    }
    StartupFacts Facts() const {
        StartupFacts f{};
        f.staticLoad = true;
        f.proxyRoleMatches = true;
        f.mappedBase = base;
        f.identityStatus = IdentityReadStatus::Ok;
        f.executable.size = KnownExecutableSize;
        std::memcpy(f.executable.sha256, KnownExecutableSha256(), 32);
        return f;
    }
    StartupDecision Evaluate(const StartupFacts& facts) {
        return CheckStartup(facts, Read, this);
    }
};

void TestPureGate() {
    ImageFixture image;
    auto facts = image.Facts();
    const auto before = std::vector<unsigned char>(image.bytes,
        image.bytes + FixtureSize);
    auto d = image.Evaluate(facts);
    Check(d.state == StartupState::Withheld, "known positive preflight is withheld");
    Check(std::strstr(d.reason, "WITHHELD") != nullptr,
        "positive reason names withheld profile");
    Check(std::memcmp(before.data(), image.bytes, FixtureSize) == 0,
        "gate reads fixture without modifying mapped bytes");

    auto rejected = [&](StartupFacts f, const char* why) {
        const auto result = image.Evaluate(f);
        Check(result.state == StartupState::Rejected, why);
    };
    facts.staticLoad = false;
    rejected(facts, "dynamic attach rejected"); facts = image.Facts();
    facts.proxyRoleMatches = false;
    rejected(facts, "wrong proxy role rejected"); facts = image.Facts();
    facts.identityStatus = IdentityReadStatus::OpenFailed;
    rejected(facts, "missing executable identity rejected"); facts = image.Facts();
    facts.executable.sha256[0] ^= 1;
    rejected(facts, "wrong executable hash rejected"); facts = image.Facts();
    facts.executable.size++;
    rejected(facts, "wrong executable size rejected"); facts = image.Facts();
    facts.mappedBase = 0;
    rejected(facts, "missing mapped base rejected"); facts = image.Facts();
    image.reads = 0;
    facts.mappedBase = UINT32_MAX - KnownImageSize + 1;
    rejected(facts, "overflowing mapped base rejected");
    Check(image.reads == 0, "overflowing mapped base rejects before callback reads");
    facts = image.Facts();
    image.reads = 0;
    Check(CheckStartup(facts, nullptr, &image).state == StartupState::Rejected,
        "missing read capability rejected");
    Check(image.reads == 0, "missing read capability rejects before callback reads");

    const IdentityReadStatus failedIdentityStatuses[] = {
        IdentityReadStatus::PathUnavailable, IdentityReadStatus::OpenFailed,
        IdentityReadStatus::SizeMismatch, IdentityReadStatus::ReadFailed,
        IdentityReadStatus::ChangedDuringRead
    };
    for (const auto status : failedIdentityStatuses) {
        image.reads = 0;
        facts = image.Facts();
        facts.identityStatus = status;
        rejected(facts, "non-Ok executable identity status rejected");
        Check(image.reads == 0,
            "non-Ok executable identity rejects before mapped image reads");
    }
    facts = image.Facts();

    const unsigned char oldDos = image.bytes[0];
    image.bytes[0] ^= 1;
    rejected(facts, "mapped DOS header mismatch rejected");
    image.bytes[0] = oldDos;
    const unsigned char oldNt = image.bytes[0x80];
    image.bytes[0x80] ^= 1;
    rejected(facts, "mapped PE signature mismatch rejected");
    image.bytes[0x80] = oldNt;

    image.reads = 0;
    image.unreadableRva = 0;
    rejected(facts, "unreadable mapped DOS header rejected");
    Check(image.reads == 1, "unreadable DOS header stops after its first read");
    image.unreadableRva = 0x80;
    image.reads = 0;
    rejected(facts, "unreadable mapped NT header rejected");
    Check(image.reads == 2, "unreadable NT header follows only the DOS read");
    image.unreadableRva = 0xFFFFFFFFu;

    image.Put32(0x3C, 0x1000);
    image.reads = 0;
    rejected(facts, "out-of-bounds mapped e_lfanew rejected");
    Check(image.reads == 1, "out-of-bounds e_lfanew rejects before NT read");
    image.Put32(0x3C, 0x80);

    struct NtField { std::uint32_t offset; unsigned width; std::uint32_t wrong; };
    const NtField badNtFields[] = {
        {0x80 + 4, 2, IMAGE_FILE_MACHINE_AMD64},
        {0x80 + 8, 4, 1451925842},
        {0x80 + 20, 2, 223},
        {0x80 + 24, 2, 0x20B},
        {0x80 + 24 + 16, 4, 0x6B0F90},
        {0x80 + 24 + 28, 4, 0x00410000},
        {0x80 + 24 + 56, 4, KnownImageSize - 0x1000}
    };
    for (const auto& field : badNtFields) {
        const std::uint32_t original = field.width == 2
            ? (image.bytes[field.offset] | (std::uint32_t(image.bytes[field.offset + 1]) << 8))
            : (std::uint32_t(image.bytes[field.offset]) |
                (std::uint32_t(image.bytes[field.offset + 1]) << 8) |
                (std::uint32_t(image.bytes[field.offset + 2]) << 16) |
                (std::uint32_t(image.bytes[field.offset + 3]) << 24));
        if (field.width == 2) image.Put16(field.offset, static_cast<std::uint16_t>(field.wrong));
        else image.Put32(field.offset, field.wrong);
        rejected(facts, "each mapped NT identity field mismatch rejected");
        if (field.width == 2) image.Put16(field.offset, static_cast<std::uint16_t>(original));
        else image.Put32(field.offset, original);
    }

    const auto unchangedBeforePartial = std::vector<unsigned char>(image.bytes,
        image.bytes + FixtureSize);
    image.partialReadRva = FixturePins[0].rva;
    image.partialReadBytes = 2;
    rejected(facts, "partially filled private read buffer followed by failure rejected");
    Check(std::memcmp(unchangedBeforePartial.data(), image.bytes, FixtureSize) == 0,
        "partial callback output does not modify the owned mapped fixture");
    image.partialReadRva = 0xFFFFFFFFu;
    image.partialReadBytes = 0;

    image.Put32(0x80 + 24 + 56, KnownImageSize - 0x1000);
    rejected(facts, "mapped PE image size mismatch rejected");
    image.Put32(0x80 + 24 + 56, KnownImageSize);

    std::size_t count = 0;
    const auto* sites = StartupSites(count);
    Check(count == 13, "fixture consumes manager's immutable 13-site catalog");
    for (std::size_t i = 0; i < count; ++i) {
        const auto& site = sites[i];
        const unsigned char saved = image.bytes[site.rva];
        image.bytes[site.rva] ^= 0x80;
        const auto mismatch = image.Evaluate(facts);
        Check(mismatch.state == StartupState::Rejected &&
            mismatch.failedRva == site.rva && mismatch.failedSite == site.name,
            "each pinned dependency mismatch identifies its site");
        image.bytes[site.rva] = saved;
        image.unreadableRva = site.rva;
        const auto unreadable = image.Evaluate(facts);
        Check(unreadable.state == StartupState::Rejected &&
            unreadable.failedRva == site.rva && unreadable.failedSite == site.name,
            "each pinned dependency read failure identifies its site");
        image.unreadableRva = 0xFFFFFFFFu;
    }

    // Relocation-sensitive positive control at a nonpreferred base, then prove
    // that a wrong absolute operand cannot be learned from current memory.
    image.base = 0x00210000;
    facts = image.Facts();
    image.PopulateSites();
    Check(image.Evaluate(facts).state == StartupState::Withheld,
        "reviewed HIGHLOW operands materialize at relocated base");
    const auto& relocated = FixturePins[2];
    image.bytes[relocated.rva + relocated.relocationOffset] ^= 1;
    const auto wrongRelocation = image.Evaluate(facts);
    Check(wrongRelocation.state == StartupState::Rejected &&
        wrongRelocation.failedRva == relocated.rva,
        "wrong relocated absolute operand rejected");
}

} // namespace

// Rename and compile the production capture implementation separately from the
// synthetic facts used to drive the extracted DllMain route.
#define CheckProductionStartup ActualCheckProductionStartup
#include "Fork/StartupGate.cpp"
#undef CheckProductionStartup

// DLL route state and downstream stubs are injected/defined by the harness
// generator below this point.
namespace fork_startup { struct StartupDecision; }
namespace {
fork_startup::StartupDecision TestCheckProductionStartup(void*, const void*);
void TestReportStartupDecision(const fork_startup::StartupDecision&);
void TestDisableThreadLibraryCalls(HMODULE);
void TestInitLogDir();
void TestInitializeCriticalSection(CRITICAL_SECTION*);
void TestReportUnavailableCrashWatch();
void TestPinFpu();
bool TestInstall();
void TestLog(const char*, ...);
LONG TestInterlockedExchange(volatile LONG*, LONG);
LONG TestInterlockedCompareExchange(volatile LONG*, LONG, LONG);
BOOL TestPostThreadMessageA(DWORD, UINT, WPARAM, LPARAM);
}
extern HMODULE g_selfModule;
extern CRITICAL_SECTION g_logCs;
extern bool g_logCsInit;
extern volatile LONG g_dllReady;
extern volatile LONG g_clipboardWorkerStop;
extern volatile LONG g_clipboardWorkerThreadId;
#undef InterlockedExchange
#undef InterlockedCompareExchange
#define CheckProductionStartup TestCheckProductionStartup
#define ReportStartupDecision TestReportStartupDecision
#define DisableThreadLibraryCalls TestDisableThreadLibraryCalls
#define InitLogDir TestInitLogDir
#define InitializeCriticalSection TestInitializeCriticalSection
#define ReportUnavailableCrashWatch TestReportUnavailableCrashWatch
#define PinFpu TestPinFpu
#define Install TestInstall
#define Log TestLog
#define InterlockedExchange TestInterlockedExchange
#define InterlockedCompareExchange TestInterlockedCompareExchange
#define PostThreadMessageA TestPostThreadMessageA
// ACTUAL_DLLMAIN_INJECTED
#undef CheckProductionStartup
#undef ReportStartupDecision
#undef DisableThreadLibraryCalls
#undef InitLogDir
#undef InitializeCriticalSection
#undef ReportUnavailableCrashWatch
#undef PinFpu
#undef Install
#undef Log
#undef InterlockedExchange
#undef InterlockedCompareExchange
#undef PostThreadMessageA

namespace {
using namespace fork_startup;
ImageFixture* routeImage = nullptr;
StartupFacts routeFacts{};
bool routeAuthorizedControl = false;
unsigned reportCalls = 0, disableCalls = 0, logDirCalls = 0;
unsigned criticalCalls = 0, skipCalls = 0, fpuCalls = 0, installCalls = 0;
unsigned logCalls = 0, threadPostCalls = 0, readyCalls = 0;
std::vector<const char*> routeOrder;

StartupDecision TestCheckProductionStartup(void*, const void*) {
    if (routeAuthorizedControl)
        return {StartupState::Authorized,
            "test-only Authorized control; no production profile evidence"};
    return routeImage->Evaluate(routeFacts);
}
void TestReportStartupDecision(const StartupDecision&) { ++reportCalls; }
void TestDisableThreadLibraryCalls(HMODULE) {
    ++disableCalls; routeOrder.push_back("disable");
}
void TestInitLogDir() { ++logDirCalls; routeOrder.push_back("logdir"); }
void TestInitializeCriticalSection(CRITICAL_SECTION*) {
    ++criticalCalls; routeOrder.push_back("critical");
}
void TestReportUnavailableCrashWatch() { ++skipCalls; routeOrder.push_back("skip"); }
void TestPinFpu() { ++fpuCalls; routeOrder.push_back("fpu"); }
bool TestInstall() { ++installCalls; routeOrder.push_back("install"); return true; }
void TestLog(const char*, ...) { ++logCalls; routeOrder.push_back("log"); }
LONG TestInterlockedExchange(volatile LONG* target, LONG value) {
    *target = value;
    if (target == &g_dllReady) {
        ++readyCalls; routeOrder.push_back("ready");
    }
    return value;
}
LONG TestInterlockedCompareExchange(volatile LONG* target, LONG exchange,
    LONG comparand) {
    const LONG old = *target;
    if (old == comparand) *target = exchange;
    return old;
}
BOOL TestPostThreadMessageA(DWORD, UINT, WPARAM, LPARAM) {
    ++threadPostCalls; return TRUE;
}

void ResetRouteCounters() {
    reportCalls = disableCalls = logDirCalls = criticalCalls = skipCalls = 0;
    fpuCalls = installCalls = logCalls = threadPostCalls = readyCalls = 0;
    routeOrder.clear();
    g_selfModule = reinterpret_cast<HMODULE>(static_cast<std::uintptr_t>(0x1234));
    g_logCsInit = false;
    g_dllReady = 37;
    g_clipboardWorkerStop = 0;
    g_clipboardWorkerThreadId = 0;
}
void CheckNoDependentEffects() {
    Check(disableCalls == 0 && logDirCalls == 0 && criticalCalls == 0 &&
        skipCalls == 0 && fpuCalls == 0 && installCalls == 0 && logCalls == 0 &&
        threadPostCalls == 0 && readyCalls == 0,
        "rejected/withheld route has zero dependent initialization effects");
    Check(g_selfModule == reinterpret_cast<HMODULE>(static_cast<std::uintptr_t>(0x1234)) &&
        g_logCsInit == false && g_dllReady == 37,
        "rejected/withheld route leaves published globals unchanged");
}

void TestActualProductionCapture() {
    auto dynamic = ActualCheckProductionStartup(reinterpret_cast<void*>(1), nullptr);
    Check(dynamic.state == StartupState::Rejected &&
        std::strstr(dynamic.reason, "dynamic/late") != nullptr,
        "actual production capture rejects dynamic attach before identity capture");
    auto wrongRole = ActualCheckProductionStartup(reinterpret_cast<void*>(1),
        reinterpret_cast<void*>(1));
    Check(wrongRole.state == StartupState::Rejected &&
        std::strstr(wrongRole.reason, "proxy module") != nullptr,
        "actual production capture rejects unsupported own-process proxy role");
}

void TestDllMainRejectionRoute() {
    ImageFixture image;
    routeImage = &image;
    routeFacts = image.Facts();
    struct Case { const char* name; void (*prepare)(); };
    const auto noChange = []() {};
    auto dynamic = []() { routeFacts.staticLoad = false; };
    auto missingIdentity = []() { routeFacts.identityStatus = IdentityReadStatus::OpenFailed; };
    auto wrongHash = []() { routeFacts.executable.sha256[0] ^= 0x80; };
    const Case cases[] = {
        {"static-load mismatch", dynamic},
        {"missing executable identity", missingIdentity},
        {"wrong executable hash", wrongHash},
        {"known positive preflight WITHHELD", noChange}
    };
    for (const auto& test : cases) {
        routeFacts = image.Facts();
        test.prepare();
        ResetRouteCounters();
        routeAuthorizedControl = false;
        Check(DllMain(reinterpret_cast<HMODULE>(static_cast<std::uintptr_t>(0x4321)),
            DLL_PROCESS_ATTACH, reinterpret_cast<void*>(1)) == FALSE,
            test.name);
        Check(reportCalls == 1, "rejected route emits only the bounded debug decision");
        CheckNoDependentEffects();
    }

    routeFacts = image.Facts();
    image.bytes[0x80] ^= 1;
    ResetRouteCounters();
    Check(DllMain(reinterpret_cast<HMODULE>(static_cast<std::uintptr_t>(0x4321)),
        DLL_PROCESS_ATTACH, reinterpret_cast<void*>(1)) == FALSE,
        "DllMain rejects mapped PE header mismatch");
    Check(reportCalls == 1, "mapped PE mismatch reports one reason");
    CheckNoDependentEffects();
    image.bytes[0x80] ^= 1;

    // For each pinned dependency mismatch and failed read, run the actual DllMain
    // branch and verify no dependent logging/config/handler/thread/FPU/write path.
    std::size_t count = 0;
    const auto* sites = StartupSites(count);
    for (std::size_t i = 0; i < count; ++i) {
        routeFacts = image.Facts();
        const auto& site = sites[i];
        const auto saved = image.bytes[site.rva];
        image.bytes[site.rva] ^= 0x40;
        ResetRouteCounters();
        Check(DllMain(reinterpret_cast<HMODULE>(static_cast<std::uintptr_t>(0x4321)),
            DLL_PROCESS_ATTACH, reinterpret_cast<void*>(1)) == FALSE,
            "DllMain rejects each mismatched pinned dependency");
        Check(reportCalls == 1, "dependency mismatch reports one reason");
        CheckNoDependentEffects();
        image.bytes[site.rva] = saved;

        image.unreadableRva = site.rva;
        ResetRouteCounters();
        Check(DllMain(reinterpret_cast<HMODULE>(static_cast<std::uintptr_t>(0x4321)),
            DLL_PROCESS_ATTACH, reinterpret_cast<void*>(1)) == FALSE,
            "DllMain rejects each unreadable pinned dependency");
        Check(reportCalls == 1, "dependency read failure reports one reason");
        CheckNoDependentEffects();
        image.unreadableRva = 0xFFFFFFFFu;
    }

    // Route sensitivity only. This impossible production state proves that the
    // unchanged accepted startup branch remains reachable when authorization is
    // deliberately fabricated by this test wrapper.
    ResetRouteCounters();
    routeAuthorizedControl = true;
    Check(DllMain(reinterpret_cast<HMODULE>(static_cast<std::uintptr_t>(0x4321)),
        DLL_PROCESS_ATTACH, reinterpret_cast<void*>(1)) == TRUE,
        "test-only Authorized sensitivity control reaches legacy route");
    Check(disableCalls == 1 && logDirCalls == 1 && criticalCalls == 1 &&
        skipCalls == 1 && fpuCalls == 1 && installCalls == 1 && logCalls == 1 &&
        readyCalls == 1, "authorized fixture control observes skip and expected branch effects");
    const std::vector<const char*> expected = {
        "disable", "logdir", "critical", "skip", "fpu", "install", "log", "ready"
    };
    bool sameOrder = routeOrder.size() == expected.size();
    for (std::size_t i = 0; sameOrder && i < expected.size(); ++i)
        sameOrder = std::strcmp(routeOrder[i], expected[i]) == 0;
    Check(sameOrder, "legacy initialization order retained after gate");
    Check(g_selfModule == reinterpret_cast<HMODULE>(static_cast<std::uintptr_t>(0x4321)) &&
        g_logCsInit && g_dllReady == 1,
        "authorized sensitivity control publishes legacy globals");
    routeAuthorizedControl = false;
}

void TestThreadAndDetachRoutes() {
    routeAuthorizedControl = false;
    ResetRouteCounters();
    Check(DllMain(nullptr, DLL_THREAD_ATTACH, nullptr) == TRUE &&
        DllMain(nullptr, DLL_THREAD_DETACH, nullptr) == TRUE,
        "thread notifications return normally without entering startup gate");
    Check(reportCalls == 0 && disableCalls == 0 && installCalls == 0,
        "thread notifications do not run process startup initialization");
    Check(DllMain(nullptr, DLL_PROCESS_DETACH, nullptr) == TRUE &&
        g_clipboardWorkerStop == 1 && threadPostCalls == 0,
        "detach signals worker stop and skips absent worker post");
    ResetRouteCounters();
    g_clipboardWorkerThreadId = 77;
    Check(DllMain(nullptr, DLL_PROCESS_DETACH, nullptr) == TRUE &&
        g_clipboardWorkerStop == 1 && threadPostCalls == 1,
        "detach posts quit to the existing worker id");
    Check(reportCalls == 0 && installCalls == 0,
        "detach never evaluates startup compatibility");
}
} // namespace

// The macro substitutions in the generated harness keep the extracted DllMain
// body intact while counting all downstream effects.
HMODULE g_selfModule = nullptr;
CRITICAL_SECTION g_logCs{};
bool g_logCsInit = false;
volatile LONG g_dllReady = 0;
volatile LONG g_clipboardWorkerStop = 0;
volatile LONG g_clipboardWorkerThreadId = 0;

int main() {
    TestPureGate();
    TestActualProductionCapture();
    TestDllMainRejectionRoute();
    TestThreadAndDetachRoutes();
    std::printf("PASS: %u startup identity/gate/route assertions; pure positives withheld, all rejects have zero dependent effects, test-only authorization control preserves order\n", checks);
    return 0;
}
