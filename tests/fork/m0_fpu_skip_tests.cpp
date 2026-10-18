// Actual production function definitions are extracted and injected by the
// Python driver. Everything here is an owned Win32 control-state fixture.
#include <float.h>
#include <xmmintrin.h>
#include <intrin.h>
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cstring>

namespace {
unsigned checks = 0;
unsigned logCalls = 0;
char lastLog[256]{};
void Check(bool ok, const char* message) {
    ++checks;
    if (!ok) {
        std::fprintf(stderr, "FAIL: %s\n", message);
        std::abort();
    }
}
void Log(const char* text, ...) {
    ++logCalls;
    const auto count = std::strlen(text) < sizeof(lastLog) - 1 ?
        std::strlen(text) : sizeof(lastLog) - 1;
    std::memcpy(lastLog, text, count);
    lastLog[count] = '\0';
}
unsigned short ReadX87Control() {
    unsigned short value = 0;
    __asm { fnstcw value }
    return value;
}
void WriteX87Control(unsigned short value) {
    __asm { fldcw value }
}
bool SupportsDaz() {
    int regs[4]{};
    __cpuid(regs, 1);
    constexpr int fxsr = 1 << 24;
    constexpr int sse2 = 1 << 26;
    if ((regs[3] & (fxsr | sse2)) != (fxsr | sse2)) return false;
    alignas(16) unsigned char fxArea[512]{};
    __asm { fxsave fxArea }
    unsigned int mxcsrMask = 0;
    std::memcpy(&mxcsrMask, fxArea + 28, sizeof(mxcsrMask));
    return (mxcsrMask & 0x0040) != 0;
}
}

// ACTUAL_FUNCTIONS_INJECTED

namespace {
bool TestPinFpu() {
    constexpr unsigned short pcMask = 0x0300;
    constexpr unsigned short rcMask = 0x0C00;
    constexpr unsigned short x87ExceptionMasks = 0x003F;
    constexpr unsigned int mxcsrDaz = 0x0040;
    constexpr unsigned int mxcsrFtz = 0x8000;
    constexpr unsigned int mxcsrRcMask = 0x6000;
    constexpr unsigned int mxcsrExceptionMasks = 0x1F80;
    struct Variant { unsigned short pc; unsigned short rc; unsigned int mxcsrRc; bool daz; bool ftz; };
    const Variant variants[] = {
        {0x0000, 0x0000, 0x0000, false, false},
        {0x0200, 0x0400, 0x2000, true, false},
        {0x0300, 0x0C00, 0x6000, false, true},
        {0x0200, 0x0800, 0x4000, true, true}
    };
    unsigned int ownMxcsr = _mm_getcsr();
    unsigned short ownX87 = ReadX87Control();
    const bool dazSupported = SupportsDaz();
    if (dazSupported)
        Check(true, "host CPUID/FXSAVE reports SSE2, FXSR and MXCSR DAZ capability");
    if (!dazSupported) return false;
    bool allRestored = true;
    for (const auto& variant : variants) {
        const unsigned short configuredX87 = static_cast<unsigned short>(
            (ownX87 & static_cast<unsigned short>(~(pcMask | rcMask))) |
            x87ExceptionMasks | variant.pc | variant.rc);
        WriteX87Control(configuredX87);
        const unsigned int beforeMxcsr = (ownMxcsr | mxcsrExceptionMasks) &
            ~(mxcsrDaz | mxcsrFtz | mxcsrRcMask);
        _mm_setcsr(beforeMxcsr | variant.mxcsrRc |
            (variant.daz ? mxcsrDaz : 0) | (variant.ftz ? mxcsrFtz : 0));
        const unsigned short beforeX87 = ReadX87Control();
        const unsigned int beforeSse = _mm_getcsr();
        Check((beforeX87 & pcMask) == variant.pc &&
            (beforeX87 & rcMask) == variant.rc,
            "fixture establishes the requested valid x87 PC/RC variant");
        Check((beforeX87 & x87ExceptionMasks) == x87ExceptionMasks,
            "all x87 exceptions are masked before PinFpu");
        PinFpu();
        const unsigned short afterX87 = ReadX87Control();
        const unsigned int afterSse = _mm_getcsr();
        Check((afterX87 & pcMask) == 0x0200, "PinFpu selects x87 PC53");
        Check((afterX87 & rcMask) == 0, "PinFpu selects x87 round-to-nearest");
        Check((afterSse & mxcsrRcMask) == 0, "PinFpu leaves SSE rounding at nearest");
        Check((afterSse & mxcsrFtz) != 0, "PinFpu enables MXCSR FTZ");
        Check((afterSse & mxcsrDaz) != 0, "PinFpu enables MXCSR DAZ");
        Check((beforeX87 & static_cast<unsigned short>(~(pcMask | rcMask))) ==
            (afterX87 & static_cast<unsigned short>(~(pcMask | rcMask))),
            "PinFpu preserves every unrelated x87 control bit");
        Check((beforeSse & mxcsrExceptionMasks) == mxcsrExceptionMasks &&
            (afterSse & mxcsrExceptionMasks) == mxcsrExceptionMasks,
            "all SSE exceptions are masked before and after PinFpu");
        Check((afterX87 & x87ExceptionMasks) == x87ExceptionMasks,
            "all x87 exceptions remain masked after PinFpu");
        Check((beforeSse & mxcsrExceptionMasks) == (afterSse & mxcsrExceptionMasks),
            "PinFpu preserves SSE exception-mask control bits");

        WriteX87Control(ownX87);
        _mm_setcsr(ownMxcsr);
        const bool restored = ReadX87Control() == ownX87 && _mm_getcsr() == ownMxcsr;
        Check(restored, "test restores its exact incoming x87 CW and MXCSR");
        allRestored = allRestored && restored;
    }
    Check(allRestored, "each variant restores exact original FPU controls");
    Check(ReadX87Control() == ownX87 && _mm_getcsr() == ownMxcsr,
        "fixture leaves exact incoming x87 CW and MXCSR restored at completion");
    return true;
}

void TestUnavailableCrashWatchReport() {
    logCalls = 0;
    lastLog[0] = '\0';
    ReportUnavailableCrashWatch();
    Check(logCalls == 1, "skip helper emits exactly one log record");
    Check(std::strcmp(lastLog,
        "CrashWatch unavailable (R02d): optional diagnostics blocked; registration/provider/lifetime profile unproved; no handlers/IAT/signal changes; mandatory PinFpu retained unchanged") == 0,
        "skip helper records the literal blocked reason without claiming installation");
    ReportUnavailableCrashWatch();
    Check(logCalls == 2, "each skip helper call reports once");
}

void TestMain() {
    const bool fpuSupported = TestPinFpu();
    TestUnavailableCrashWatchReport();
    std::printf("M0 PinFpu/skip fixture: %u checks passed; PinFpu execution %s\n",
        checks, fpuSupported ? "PASS" : "UNSUPPORTED (DAZ capability absent)");
}
}

int main() { TestMain(); return 0; }
