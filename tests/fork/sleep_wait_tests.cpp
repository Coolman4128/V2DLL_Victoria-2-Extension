// Win32 owned-process sample. Python injects unchanged production bodies below.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdio>
#include <cstring>
#include <cstdint>
#include <climits>
#include <initializer_list>

static DWORD g_base = 0;
static DWORD g_imageSize = 0;
struct Settings { bool removeSleepers = true; int mpClientSleepMs = 1; int mainLoopSleepMs = 1; };
static Settings g_settings;
typedef void (WINAPI* tSleep)(DWORD);
typedef DWORD (WINAPI* tWaitForSingleObject)(HANDLE, DWORD);
static tSleep g_realSleep = nullptr;
static tWaitForSingleObject g_realWFSO = nullptr;
static volatile LONG g_sleepCalls = 0;
static volatile LONG g_sleepSumMs = 0;
static volatile LONG g_wfsoCalls = 0;
static volatile LONG g_wfsoSumMs = 0;
static volatile LONG g_noteSleepCalls = 0;
static volatile LONG g_noteWfsoCalls = 0;
static volatile LONG g_virtualProtectCalls = 0;
static bool g_denyProtect = false;
static bool g_denyResolver = false;
static bool g_captureResolver = false;
static DWORD g_resolvedSleepMs = UINT_MAX;
static DWORD g_fakeNow = 0;
static DWORD g_fakeReturn = 0;
static DWORD g_lastSleepOrig = 0;
static DWORD g_lastSleepFinal = 0;
static DWORD g_lastWaitOrig = 0;
static DWORD g_lastWaitFinal = 0;

static void Log(const char*, ...) {}
static DWORD Stub_ReturnAddress() { return g_fakeReturn; }
static DWORD NowMs() { return ++g_fakeNow; } // Instrumented clock, never timing evidence.
static void NoteSleep(DWORD, DWORD, DWORD original, DWORD next) {
    InterlockedIncrement(&g_noteSleepCalls);
    g_lastSleepOrig = original;
    g_lastSleepFinal = next;
}
static void NoteWfso(DWORD, DWORD original, DWORD next) {
    InterlockedIncrement(&g_noteWfsoCalls);
    g_lastWaitOrig = original;
    g_lastWaitFinal = next;
}
static BOOL TestVirtualProtect(LPVOID address, SIZE_T bytes, DWORD requested, PDWORD oldProtect) {
    InterlockedIncrement(&g_virtualProtectCalls);
    if (g_denyProtect) { SetLastError(ERROR_ACCESS_DENIED); return FALSE; }
    return ::VirtualProtect(address, bytes, requested, oldProtect);
}
static HMODULE TestGetModuleHandleA(LPCSTR name) {
    return g_denyResolver ? nullptr : ::GetModuleHandleA(name);
}
static void WINAPI ResolvedCaptureSleep(DWORD ms) { g_resolvedSleepMs = ms; }
static FARPROC TestGetProcAddress(HMODULE module, LPCSTR name) {
    if (g_denyResolver) return nullptr;
    if (g_captureResolver && std::strcmp(name, "Sleep") == 0)
        return reinterpret_cast<FARPROC>(&ResolvedCaptureSleep);
    return ::GetProcAddress(module, name);
}
#define _ReturnAddress Stub_ReturnAddress
#define VirtualProtect TestVirtualProtect
#define GetModuleHandleA TestGetModuleHandleA
#define GetProcAddress TestGetProcAddress
// ACTUAL_SLEEP_WAIT_BODIES_INJECTED
#undef GetProcAddress
#undef GetModuleHandleA
#undef VirtualProtect
#undef _ReturnAddress

namespace {
int checks = 0;
int failures = 0;
DWORD observedSleep = 0;
DWORD observedWait = 0;
HANDLE observedHandle = nullptr;
DWORD cannedWait = WAIT_OBJECT_0;
bool callRealSleep = false;
bool callRealWait = false;
tSleep actualSleep = nullptr;
tWaitForSingleObject actualWait = nullptr;

void Check(bool condition, const char* label) {
    ++checks;
    if (!condition) { ++failures; std::fprintf(stderr, "FAIL %s\n", label); }
}
void WINAPI CaptureSleep(DWORD ms) {
    observedSleep = ms;
    if (callRealSleep && actualSleep) actualSleep(ms);
}
DWORD WINAPI CaptureWait(HANDLE handle, DWORD ms) {
    observedHandle = handle;
    observedWait = ms;
    return callRealWait && actualWait ? actualWait(handle, ms) : cannedWait;
}

void Provider(const char* name, FARPROC function) {
    MEMORY_BASIC_INFORMATION info{};
    SIZE_T queried = function ? VirtualQuery(reinterpret_cast<void*>(function), &info, sizeof(info)) : 0;
    auto* base = static_cast<unsigned char*>(info.AllocationBase);
    auto* dos = queried ? reinterpret_cast<IMAGE_DOS_HEADER*>(base) : nullptr;
    auto* nt = dos && dos->e_magic == IMAGE_DOS_SIGNATURE
        ? reinterpret_cast<IMAGE_NT_HEADERS32*>(base + dos->e_lfanew) : nullptr;
    bool pe32 = nt && nt->Signature == IMAGE_NT_SIGNATURE &&
        nt->OptionalHeader.Magic == IMAGE_NT_OPTIONAL_HDR32_MAGIC;
    char path[MAX_PATH]{};
    DWORD pathLength = queried ? GetModuleFileNameA(static_cast<HMODULE>(info.AllocationBase), path, MAX_PATH) : 0;
    std::uintptr_t rva = queried ? reinterpret_cast<std::uintptr_t>(function) -
        reinterpret_cast<std::uintptr_t>(base) : 0;
    std::printf("PROVIDER %s allocation=%08X rva=%08X machine=%04X image=%08X path=%s\n",
        name, static_cast<unsigned>(reinterpret_cast<std::uintptr_t>(base)), static_cast<unsigned>(rva),
        pe32 ? nt->FileHeader.Machine : 0, pe32 ? nt->OptionalHeader.SizeOfImage : 0, path);
    Check(queried == sizeof(info) && pe32 && nt->FileHeader.Machine == IMAGE_FILE_MACHINE_I386 &&
        rva < nt->OptionalHeader.SizeOfImage && pathLength > 0 && pathLength < MAX_PATH, name);
}

void SleepContracts() {
    g_realSleep = CaptureSleep;
    for (DWORD ms = 15; ms <= 51; ++ms) {
        observedSleep = UINT_MAX;
        g_settings.removeSleepers = true;
        g_settings.mpClientSleepMs = 7;
        HookSleep(ms);
        const bool shouldClamp = ms >= 16 && ms <= 50 && ms != 30 && ms != 35;
        Check(ShouldClampSleep(ms) == shouldClamp, "clamp boundary decision");
        Check(observedSleep == (shouldClamp ? 7u : ms), "sleep boundary forwarded value");
        Check(g_lastSleepOrig == ms && g_lastSleepFinal == observedSleep, "sleep note forwarding");
    }
    const int settings[] = {-1, 0, 1, 127, 128};
    const DWORD expected[] = {0, 0, 1, 127, 127};
    for (unsigned i = 0; i < 5; ++i) {
        g_settings.mpClientSleepMs = settings[i];
        HookSleep(16);
        Check(observedSleep == expected[i], "sleep setting clamp");
        HookSleep(30);
        Check(observedSleep == 30, "audio 30 preserved");
        HookSleep(35);
        Check(observedSleep == 35, "audio 35 preserved");
    }
    g_settings.removeSleepers = false;
    g_settings.mpClientSleepMs = 0;
    HookSleep(40);
    Check(observedSleep == 40, "sleep disabled passthrough");
    g_realSleep = nullptr;
    const LONG prior = g_sleepCalls;
    HookSleep(40);
    Check(g_sleepCalls == prior + 1, "sleep missing provider remains counted");
    g_realSleep = CaptureSleep;
    callRealSleep = true;
    HookSleep(0);
    HookSleep(1);
    callRealSleep = false;
    std::puts("ACTUAL Sleep(0), Sleep(1) completed through kernel32 provider; no elapsed-time claim");
}

void WaitContracts() {
    HANDLE event = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    Check(event != nullptr, "owned manual reset event created");
    if (!event) return;
    g_realWFSO = CaptureWait;
    g_settings.removeSleepers = true;
    g_settings.mpClientSleepMs = 7;
    for (DWORD ms = 15; ms <= 51; ++ms) {
        cannedWait = WAIT_TIMEOUT;
        DWORD result = HookWaitForSingleObject(event, ms);
        bool shouldClamp = ms >= 16 && ms <= 50 && ms != 30 && ms != 35;
        Check(result == WAIT_TIMEOUT && observedHandle == event, "wait handle/timeout return");
        Check(observedWait == (shouldClamp ? 7u : ms), "wait boundary forwarded value");
        Check(g_lastWaitOrig == ms && g_lastWaitFinal == observedWait, "wait note forwarding");
    }
    const int settings[] = {-1, 0, 1, 127, 128};
    const DWORD expected[] = {0, 0, 1, 127, 127};
    for (unsigned i = 0; i < 5; ++i) {
        g_settings.mpClientSleepMs = settings[i];
        Check(HookWaitForSingleObject(event, 16) == WAIT_TIMEOUT && observedWait == expected[i],
            "wait setting clamp");
    }
    g_settings.removeSleepers = false;
    HookWaitForSingleObject(event, 40);
    Check(observedWait == 40, "wait disabled passthrough");
    g_settings.removeSleepers = true;
    HookWaitForSingleObject(event, INFINITE);
    Check(observedWait == INFINITE, "infinite preserved in instrumented branch");
    const DWORD returns[] = {WAIT_OBJECT_0, WAIT_TIMEOUT, WAIT_FAILED, WAIT_ABANDONED};
    for (DWORD result : returns) {
        cannedWait = result;
        Check(HookWaitForSingleObject(event, 16) == result, "wait provider return propagation");
    }
    g_realWFSO = nullptr;
    Check(HookWaitForSingleObject(event, 16) == WAIT_FAILED, "wait missing provider fallback");
    g_realWFSO = CaptureWait;
    callRealWait = true;
    g_settings.removeSleepers = false;
    Check(HookWaitForSingleObject(event, 0) == WAIT_TIMEOUT && observedWait == 0,
        "actual unsignaled event finite zero");
    Check(HookWaitForSingleObject(event, 1) == WAIT_TIMEOUT && observedWait == 1,
        "actual unsignaled event finite one");
    Check(SetEvent(event) != 0, "owned event signaled");
    Check(HookWaitForSingleObject(event, 1) == WAIT_OBJECT_0 && observedWait == 1,
        "actual signaled event finite one");
    Check(HookWaitForSingleObject(event, INFINITE) == WAIT_OBJECT_0 && observedWait == INFINITE,
        "actual signaled event infinite");
    callRealWait = false;
    Check(CloseHandle(event) != 0, "owned event closed");
    std::puts("ACTUAL manual reset event wait 0/1/INFINITE completed; no elapsed-time claim");
}

struct PatchImage {
    unsigned char* bytes = nullptr;
    static constexpr SIZE_T size = 0x890000;
    PatchImage() {
        bytes = static_cast<unsigned char*>(VirtualAlloc(nullptr, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
        if (bytes) { g_base = static_cast<DWORD>(reinterpret_cast<std::uintptr_t>(bytes)); g_imageSize = static_cast<DWORD>(size); }
    }
    ~PatchImage() { if (bytes) VirtualFree(bytes, 0, MEM_RELEASE); g_base = 0; g_imageSize = 0; }
    void Site(DWORD rva, BYTE immediate = 100) {
        auto* p = bytes + rva;
        const BYTE prefix[] = {0x6A, 0x00, 0xEB, 0x02, 0x6A, immediate, 0xFF, 0x15};
        std::memcpy(p - 4, prefix, sizeof(prefix));
        *reinterpret_cast<DWORD*>(p + 4) = g_base + RVA_SLEEP_IAT;
    }
    DWORD Protect(DWORD rva) const {
        MEMORY_BASIC_INFORMATION info{};
        return VirtualQuery(bytes + rva, &info, sizeof(info)) == sizeof(info) ? info.Protect : 0;
    }
    bool Readonly(DWORD rva) { DWORD old = 0; return VirtualProtect(bytes + rva, 2, PAGE_READONLY, &old) != 0; }
};

void PatchContracts() {
    PatchImage image;
    Check(image.bytes != nullptr, "owned patch image allocated");
    if (!image.bytes) return;
    auto* a = image.bytes + RVA_MAIN_SLEEP_A;
    auto* b = image.bytes + RVA_MAIN_SLEEP_B;
    image.Site(RVA_MAIN_SLEEP_A);
    image.Site(RVA_MAIN_SLEEP_B);
    BYTE expectedA[8];
    BYTE expectedB[8];
    std::memcpy(expectedA, a, 8);
    std::memcpy(expectedB, b, 8);
    expectedA[1] = 1;
    expectedB[1] = 1;
    Check(image.Readonly(RVA_MAIN_SLEEP_A) && image.Readonly(RVA_MAIN_SLEEP_B), "sites readonly");
    g_settings.mainLoopSleepMs = 1;
    InstallMainLoopSleep0();
    Check(a[1] == 1 && b[1] == 1, "actual two-site immediate mutation");
    Check(std::memcmp(a, expectedA, 8) == 0 && std::memcmp(b, expectedB, 8) == 0,
        "main loop exact eight-byte site outputs");
    Check(a[-3] == 0 && b[-3] == 0, "Sleep zero branch retained");
    Check(image.Protect(RVA_MAIN_SLEEP_A) == PAGE_READONLY &&
        image.Protect(RVA_MAIN_SLEEP_B) == PAGE_READONLY, "site protections restored");
    LONG prior = g_virtualProtectCalls;
    InstallMainLoopSleep0();
    Check(a[1] == 1 && b[1] == 1 && g_virtualProtectCalls == prior + 4,
        "already-new immediate accepted by both sites");
    const int settings[] = {-1, 0, 127, 128};
    const BYTE expected[] = {0, 0, 127, 127};
    for (unsigned i = 0; i < 4; ++i) {
        DWORD resetOld = 0;
        Check(VirtualProtect(a, 8, PAGE_READWRITE, &resetOld) != 0, "a writable for setting reset");
        image.Site(RVA_MAIN_SLEEP_A, 100);
        Check(VirtualProtect(a, 8, PAGE_READONLY, &resetOld) != 0, "a readonly after setting reset");
        Check(VirtualProtect(b, 8, PAGE_READWRITE, &resetOld) != 0, "b writable for setting reset");
        image.Site(RVA_MAIN_SLEEP_B, 100);
        Check(VirtualProtect(b, 8, PAGE_READONLY, &resetOld) != 0, "b readonly after setting reset");
        g_settings.mainLoopSleepMs = settings[i];
        InstallMainLoopSleep0();
        Check(a[1] == expected[i] && b[1] == expected[i], "main loop setting clamp");
    }
    DWORD old = 0;
    Check(VirtualProtect(a, 2, PAGE_READWRITE, &old) != 0, "a temporarily writable");
    a[-4] = 0x90;
    Check(VirtualProtect(a, 2, PAGE_READONLY, &old) != 0, "a readonly restored");
    prior = g_virtualProtectCalls;
    InstallMainLoopSleep0();
    Check(a[1] == 127 && b[1] == 127 && prior == g_virtualProtectCalls,
        "prefix mismatch zero writes");
    Check(VirtualProtect(a, 2, PAGE_READWRITE, &old) != 0, "a writable for reset");
    image.Site(RVA_MAIN_SLEEP_A, 100);
    Check(VirtualProtect(a, 2, PAGE_READONLY, &old) != 0, "a readonly after reset");
    Check(VirtualProtect(b, 2, PAGE_READWRITE, &old) != 0, "b writable for mismatch");
    image.Site(RVA_MAIN_SLEEP_B, 100);
    b[2] = 0x90;
    Check(VirtualProtect(b, 2, PAGE_READONLY, &old) != 0, "b readonly after mismatch");
    Check(!PatchImm8Sleep(RVA_MAIN_SLEEP_B, 100, 1, "test") && b[1] == 100,
        "signature mismatch zero writes for one site");
    g_settings.mainLoopSleepMs = 1;
    InstallMainLoopSleep0();
    Check(a[1] == 1 && b[1] == 100,
        "legacy installer partial write counterexample");
    Check(VirtualProtect(b, 2, PAGE_READWRITE, &old) != 0, "b writable for reset");
    image.Site(RVA_MAIN_SLEEP_B, 100);
    b[1] = 99;
    Check(VirtualProtect(b, 2, PAGE_READONLY, &old) != 0, "b readonly after reset");
    Check(!PatchImm8Sleep(RVA_MAIN_SLEEP_B, 100, 1, "test") && b[1] == 99,
        "unexpected immediate zero write for one site");
    Check(VirtualProtect(b, 2, PAGE_READWRITE, &old) != 0, "b writable for iat reset");
    image.Site(RVA_MAIN_SLEEP_B, 100);
    *reinterpret_cast<DWORD*>(b + 4) ^= 4;
    Check(VirtualProtect(b, 2, PAGE_READONLY, &old) != 0, "b readonly after iat reset");
    Check(!PatchImm8Sleep(RVA_MAIN_SLEEP_B, 100, 1, "test") && b[1] == 100,
        "IAT operand mismatch zero write for one site");
    g_denyProtect = true;
    prior = g_virtualProtectCalls;
    Check(!PatchImm8Sleep(RVA_MAIN_SLEEP_A, 1, 2, "test") && a[1] == 1 &&
        g_virtualProtectCalls == prior + 1, "instrumented protection failure zero write");
    g_denyProtect = false;
    std::puts("ACTUAL VirtualProtect/mutation/restoration on owned memory; instruction bytes never executed");
    std::puts("INSTRUMENTED protection failure and provider branches are not native-game proof");
}

void MpPumpContracts() {
    g_realSleep = CaptureSleep;
    g_settings.mpClientSleepMs = -1;
    MpClientPumpIdle();
    Check(observedSleep == 0, "MP pump negative becomes zero");
    g_settings.mpClientSleepMs = 0;
    MpClientPumpIdle();
    Check(observedSleep == 0, "MP pump zero retained");
    g_settings.mpClientSleepMs = 1;
    MpClientPumpIdle();
    Check(observedSleep == 1, "MP pump one retained");
    g_settings.mpClientSleepMs = 127;
    MpClientPumpIdle();
    Check(observedSleep == 127, "MP pump 127 retained");
    g_settings.mpClientSleepMs = 128;
    MpClientPumpIdle();
    Check(observedSleep == 1, "MP pump over 127 becomes one");
    g_realSleep = nullptr;
    g_settings.mpClientSleepMs = 0;
    g_denyResolver = true;
    observedSleep = UINT_MAX;
    MpClientPumpIdle();
    Check(observedSleep == UINT_MAX, "MP pump missing provider makes no call");
    g_denyResolver = false;
    g_captureResolver = true;
    g_resolvedSleepMs = UINT_MAX;
    g_settings.mpClientSleepMs = 1;
    MpClientPumpIdle();
    Check(g_resolvedSleepMs == 1, "MP pump fallback provider receives one");
    g_captureResolver = false;
    g_settings.mpClientSleepMs = 0;
    MpClientPumpIdle();
    std::puts("ACTUAL MP pump fallback resolved kernel32.Sleep(0); no elapsed-time claim");
    g_realSleep = CaptureSleep;
    callRealSleep = true;
    g_settings.mpClientSleepMs = 1;
    MpClientPumpIdle();
    callRealSleep = false;
    Check(observedSleep == 1, "MP pump actual Sleep(1) via capture provider");
}

void MpPatchContracts() {
    PatchImage image;
    Check(image.bytes != nullptr, "MP patch image allocated");
    if (!image.bytes) return;
    auto* p = image.bytes + RVA_MP_CLIENT_SLEEP;
    image.Site(RVA_MP_CLIENT_SLEEP, 40);
    Check(image.Readonly(RVA_MP_CLIENT_SLEEP), "MP patch site readonly");
    g_settings.mpClientSleepMs = 1;
    g_realSleep = CaptureSleep;
    Check(InstallMpClientSleep(), "MP exact source installer succeeds on owned image");
    const auto relative = static_cast<DWORD>(reinterpret_cast<std::uintptr_t>(&MpClientPumpIdle)) -
        (static_cast<DWORD>(reinterpret_cast<std::uintptr_t>(p)) + 5);
    Check(p[0] == 0xE8 && *reinterpret_cast<DWORD*>(p + 1) == relative &&
        p[5] == 0x90 && p[6] == 0x90 && p[7] == 0x90, "MP exact call displacement and 3 NOP bytes");
    Check(image.Protect(RVA_MP_CLIENT_SLEEP) == PAGE_READONLY, "MP protection restored");
    Check(p + 8 == image.bytes + RVA_MP_CLIENT_SLEEP + 8, "MP whole eight-byte resume");
    DWORD old = 0;
    Check(VirtualProtect(p, 8, PAGE_READWRITE, &old) != 0, "MP reset writable");
    image.Site(RVA_MP_CLIENT_SLEEP, 1);
    Check(VirtualProtect(p, 8, PAGE_READONLY, &old) != 0, "MP accepted-current readonly");
    Check(InstallMpClientSleep() && p[0] == 0xE8,
        "MP already configured immediate accepted for install");
    Check(VirtualProtect(p, 8, PAGE_READWRITE, &old) != 0, "MP reset writable again");
    image.Site(RVA_MP_CLIENT_SLEEP, 40);
    Check(VirtualProtect(p, 8, PAGE_READONLY, &old) != 0, "MP reset readonly");
    BYTE before[8];
    std::memcpy(before, p, 8);
    g_denyProtect = true;
    Check(!InstallMpClientSleep() && std::memcmp(before, p, 8) == 0,
        "MP instrumented protection failure zero image writes");
    g_denyProtect = false;
    Check(VirtualProtect(p, 8, PAGE_READWRITE, &old) != 0, "MP mismatch writable");
    p[0] = 0x90;
    Check(VirtualProtect(p, 8, PAGE_READONLY, &old) != 0, "MP mismatch readonly");
    std::memcpy(before, p, 8);
    Check(!InstallMpClientSleep() && std::memcmp(before, p, 8) == 0,
        "MP opcode mismatch zero image writes");
    Check(VirtualProtect(p, 8, PAGE_READWRITE, &old) != 0, "MP IAT reset writable");
    image.Site(RVA_MP_CLIENT_SLEEP, 40);
    *reinterpret_cast<DWORD*>(p + 4) ^= 4;
    Check(VirtualProtect(p, 8, PAGE_READONLY, &old) != 0, "MP IAT reset readonly");
    std::memcpy(before, p, 8);
    Check(!InstallMpClientSleep() && std::memcmp(before, p, 8) == 0,
        "MP IAT mismatch zero image writes");
    Check(VirtualProtect(p, 8, PAGE_READWRITE, &old) != 0, "MP imm reset writable");
    image.Site(RVA_MP_CLIENT_SLEEP, 39);
    Check(VirtualProtect(p, 8, PAGE_READONLY, &old) != 0, "MP imm reset readonly");
    std::memcpy(before, p, 8);
    Check(!InstallMpClientSleep() && std::memcmp(before, p, 8) == 0,
        "MP immediate mismatch zero image writes");
    std::puts("ACTUAL MP owned-image patch/protection; copied instruction bytes never executed");
}
} // namespace

int main() {
    HMODULE kernel32 = GetModuleHandleA("kernel32.dll");
    actualSleep = kernel32 ? reinterpret_cast<tSleep>(GetProcAddress(kernel32, "Sleep")) : nullptr;
    actualWait = kernel32 ? reinterpret_cast<tWaitForSingleObject>(GetProcAddress(kernel32, "WaitForSingleObject")) : nullptr;
    Provider("kernel32.Sleep", reinterpret_cast<FARPROC>(actualSleep));
    Provider("kernel32.WaitForSingleObject", reinterpret_cast<FARPROC>(actualWait));
    if (actualSleep && actualWait) {
        SleepContracts();
        WaitContracts();
        PatchContracts();
        MpPumpContracts();
        MpPatchContracts();
    }
    std::printf("RESULT checks=%d failures=%d\n", checks, failures);
    return failures ? 1 : 0;
}
