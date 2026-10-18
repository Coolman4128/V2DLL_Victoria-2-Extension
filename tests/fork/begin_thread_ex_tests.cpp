// Actual production definitions are inserted by test_begin_thread_ex.py.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <float.h>
#include <xmmintrin.h>
#include <intrin.h>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cwchar>

// ACTUAL_TYPEDEF_INJECTED

namespace {
volatile LONG checks = 0;
volatile LONG failures = 0;
void Check(bool yes, const char* what) {
    InterlockedIncrement(&checks);
    if (!yes) { InterlockedIncrement(&failures); std::fprintf(stderr, "FAIL %s\n", what); }
}
using RealBeginThreadEx = tBeginThreadEx;
RealBeginThreadEx osBeginThreadEx = nullptr;
volatile LONG allocCount = 0;
volatile LONG freeCount = 0;
volatile LONG freeSucceeded = 0;
volatile LONG callbackCount = 0;
volatile LONG freedBeforeCallback = 0;
bool injectAllocFailure = false;
bool injectProviderFailure = false;
bool captureOnly = false;
bool waitInsideProvider = false;
DWORD providerWait = 0;
LPVOID contextPointer = nullptr;
LPVOID freedPointer = nullptr;
LPVOID observedParam = nullptr;
LPTHREAD_START_ROUTINE observedStart = nullptr;
void* observedSecurity = nullptr;
unsigned observedStack = 0;
unsigned observedFlags = 0;
unsigned* observedIdPointer = nullptr;
unsigned observedId = 0;
DWORD callbackTid = 0;
LPVOID callbackParam = nullptr;
unsigned short callbackCw = 0;
unsigned int callbackMxcsr = 0;
constexpr DWORD returned = 0x5A17C0DE;
constexpr DWORD fakeId = 0x1234ABCD;
constexpr DWORD_PTR fakeHandle = 0x1357;

unsigned short ReadCw() { unsigned short value = 0; __asm { fnstcw value } return value; }
void WriteCw(unsigned short value) { __asm { fldcw value } }

HANDLE TestHeap() { return ::GetProcessHeap(); }
LPVOID TestAlloc(HANDLE heap, DWORD flags, SIZE_T bytes) {
    InterlockedIncrement(&allocCount);
    Check(heap == ::GetProcessHeap() && flags == HEAP_ZERO_MEMORY, "context allocation heap/flags");
    if (injectAllocFailure) return nullptr;
    LPVOID result = ::HeapAlloc(heap, flags, bytes);
    contextPointer = result;
    Check(result != nullptr && bytes == 2 * sizeof(void*), "real owned context allocation exact size");
    return result;
}
BOOL TestFree(HANDLE heap, DWORD flags, LPVOID memory) {
    InterlockedIncrement(&freeCount);
    freedPointer = memory;
    Check(heap == ::GetProcessHeap() && flags == 0 && memory == contextPointer,
        "owned context free heap/flags/pointer");
    BOOL result = ::HeapFree(heap, flags, memory);
    InterlockedExchange(&freeSucceeded, result ? 1 : 0);
    Check(result != FALSE, "real owned context HeapFree succeeds");
    return result;
}
DWORD_PTR __cdecl Provider(void* sa, unsigned stack,
    LPTHREAD_START_ROUTINE start, void* param, unsigned flags, unsigned* id) {
    observedSecurity = sa;
    observedStack = stack;
    observedStart = start;
    observedParam = param;
    observedFlags = flags;
    observedIdPointer = id;
    if (injectProviderFailure) return 0;
    if (captureOnly) { if (id) *id = fakeId; return fakeHandle; }
    DWORD_PTR handle = osBeginThreadEx(sa, stack, start, param, flags, id);
    if (id) observedId = *id;
    if (waitInsideProvider && handle) providerWait = WaitForSingleObject(reinterpret_cast<HANDLE>(handle), 10000);
    return handle;
}
DWORD WINAPI Callback(LPVOID param) {
    callbackParam = param;
    callbackTid = GetCurrentThreadId();
    callbackCw = ReadCw();
    callbackMxcsr = _mm_getcsr();
    InterlockedExchange(&freedBeforeCallback,
        freeCount == 1 && freeSucceeded == 1 && freedPointer == contextPointer ? 1 : 0);
    InterlockedIncrement(&callbackCount);
    return returned;
}
void Reset() {
    allocCount = freeCount = freeSucceeded = callbackCount = freedBeforeCallback = 0;
    injectAllocFailure = injectProviderFailure = captureOnly = waitInsideProvider = false;
    providerWait = 0;
    contextPointer = freedPointer = observedParam = callbackParam = nullptr;
    observedStart = nullptr;
    observedSecurity = nullptr;
    observedStack = 0;
    observedFlags = observedId = callbackTid = 0;
    observedIdPointer = nullptr;
    callbackCw = 0;
    callbackMxcsr = 0;
}
bool SupportsDaz() {
    int regs[4]{};
    __cpuid(regs, 1);
    if ((regs[3] & ((1 << 24) | (1 << 26))) != ((1 << 24) | (1 << 26))) return false;
    alignas(16) unsigned char fx[512]{};
    __asm { fxsave fx }
    unsigned int mask = 0;
    std::memcpy(&mask, fx + 28, sizeof(mask));
    return (mask & 0x40) != 0;
}
void ProviderIdentity() {
    wchar_t wow[MAX_PATH]{};
    UINT wowLength = GetSystemWow64DirectoryW(wow, MAX_PATH);
    Check(wowLength > 0 && wowLength < MAX_PATH - 14, "SysWOW64 provider directory available");
    if (!wowLength || wowLength >= MAX_PATH - 14) return;
    wcscat_s(wow, L"\\msvcr100.dll");
    HMODULE crt = LoadLibraryW(wow);
    Check(crt != nullptr, "exact physical SysWOW64 msvcr100.dll loads");
    FARPROC procedure = crt ? GetProcAddress(crt, "_beginthreadex") : nullptr;
    MEMORY_BASIC_INFORMATION memory{};
    SIZE_T queried = procedure ? VirtualQuery(reinterpret_cast<void*>(procedure), &memory, sizeof(memory)) : 0;
    auto* base = static_cast<unsigned char*>(memory.AllocationBase);
    auto* dos = queried == sizeof(memory) ? reinterpret_cast<IMAGE_DOS_HEADER*>(base) : nullptr;
    auto* nt = dos && dos->e_magic == IMAGE_DOS_SIGNATURE && dos->e_lfanew > 0
        ? reinterpret_cast<IMAGE_NT_HEADERS32*>(base + dos->e_lfanew) : nullptr;
    const bool pe32 = nt && nt->Signature == IMAGE_NT_SIGNATURE &&
        nt->OptionalHeader.Magic == IMAGE_NT_OPTIONAL_HDR32_MAGIC;
    const std::uintptr_t offset = queried == sizeof(memory) ?
        reinterpret_cast<std::uintptr_t>(procedure) - reinterpret_cast<std::uintptr_t>(base) : 0;
    wchar_t path[MAX_PATH]{};
    DWORD pathLength = queried == sizeof(memory) ?
        GetModuleFileNameW(static_cast<HMODULE>(memory.AllocationBase), path, MAX_PATH) : 0;
    std::printf("PROVIDER _beginthreadex pointer=%08X AllocationBase=%08X PE32machine=%04X image=%08X RVA=%08X path=%ls\n",
        static_cast<unsigned>(reinterpret_cast<std::uintptr_t>(procedure)),
        static_cast<unsigned>(reinterpret_cast<std::uintptr_t>(base)),
        pe32 ? nt->FileHeader.Machine : 0, pe32 ? nt->OptionalHeader.SizeOfImage : 0,
        static_cast<unsigned>(offset), path);
    BOOL wowProcess = FALSE;
    Check(IsWow64Process(GetCurrentProcess(), &wowProcess) && wowProcess,
        "owned process is WOW64 x86");
    const wchar_t* mappedName = wcsrchr(path, L'\\');
    Check(pe32 && nt->FileHeader.Machine == IMAGE_FILE_MACHINE_I386 &&
        offset == 0x5C601 && pathLength > 0 && pathLength < MAX_PATH &&
        mappedName && _wcsicmp(mappedName + 1, L"msvcr100.dll") == 0 &&
        memory.AllocationBase == crt,
        "resolved _beginthreadex is x86 MSVCR100 exact RVA in loaded module");
    osBeginThreadEx = reinterpret_cast<RealBeginThreadEx>(procedure);
}
}

#define GetProcessHeap TestHeap
#define HeapAlloc TestAlloc
#define HeapFree TestFree
// ACTUAL_FUNCTIONS_INJECTED
#undef HeapFree
#undef HeapAlloc
#undef GetProcessHeap

namespace {
void TestActual(bool suspended, bool earlyCompletion = false) {
    Reset();
    g_realBeginThreadEx = Provider;
    int callerOwned = suspended ? 22 : 11;
    SECURITY_ATTRIBUTES sa{sizeof(sa), nullptr, FALSE};
    unsigned id = 0;
    const unsigned flags = suspended ? CREATE_SUSPENDED : 0;
    const unsigned stack = suspended ? 65536 : 0;
    waitInsideProvider = earlyCompletion;
    const unsigned short ownCw = ReadCw();
    const unsigned int ownMxcsr = _mm_getcsr();
    const unsigned short inputCw = static_cast<unsigned short>((ownCw & ~0x0F00) | 0x0F3F);
    const unsigned int inputMxcsr = (ownMxcsr & ~0xE040u) | 0x7F80u;
    WriteCw(inputCw);
    _mm_setcsr(inputMxcsr);
    const unsigned short beforeCw = ReadCw();
    const unsigned int beforeMxcsr = _mm_getcsr();
    Check((beforeCw & 0x0F00) == 0x0F00 && (beforeCw & 0x003F) == 0x003F &&
        (beforeMxcsr & 0xE040) == 0x6000 && (beforeMxcsr & 0x1F80) == 0x1F80,
        "fixture establishes distinct valid masked caller CW/MXCSR controls");
    DWORD_PTR handle = HookBeginThreadEx(&sa, stack, Callback, &callerOwned, flags, &id);
    Check(ReadCw() == beforeCw && _mm_getcsr() == beforeMxcsr,
        "caller exact CW/MXCSR unchanged after wrapper call");
    Check(handle != 0 && reinterpret_cast<HANDLE>(handle) != INVALID_HANDLE_VALUE, "actual CRT returns owned handle");
    Check(observedSecurity == &sa && observedStack == stack && observedFlags == flags &&
        observedIdPointer == &id && id != 0 && observedId == id,
        "actual provider receives exact sa/stack/flags/id pointer and writes thread id");
    if (handle)
        Check(GetThreadId(reinterpret_cast<HANDLE>(handle)) == id, "actual handle identifies returned thread id");
    Check(observedStart == PinnedThreadStart && observedParam == contextPointer &&
        observedParam != &callerOwned, "actual provider receives pinned entry and owned context");
    Check(allocCount == 1 && contextPointer != nullptr,
        "actual success allocates one owned context");
    if (earlyCompletion)
        Check(providerWait == WAIT_OBJECT_0 && callbackCount == 1 && freeSucceeded == 1,
            "actual child completes and frees context before provider returns");
    if (suspended && handle) {
        Check(WaitForSingleObject(reinterpret_cast<HANDLE>(handle), 0) == WAIT_TIMEOUT && callbackCount == 0 && freeCount == 0,
            "suspended thread has not entered callback or freed context");
        auto* published = static_cast<ThreadStartWrap*>(contextPointer);
        Check(published && published->orig == Callback && published->param == &callerOwned,
            "suspended owned context retains exact callback and caller parameter until resume");
        Check(ResumeThread(reinterpret_cast<HANDLE>(handle)) == 1, "suspended thread resumes exactly once");
    }
    if (handle) {
        DWORD wait = WaitForSingleObject(reinterpret_cast<HANDLE>(handle), 10000);
        Check(wait == WAIT_OBJECT_0, "actual thread completes within bounded wait");
        if (wait == WAIT_OBJECT_0) {
            DWORD result = 0;
            Check(GetExitCodeThread(reinterpret_cast<HANDLE>(handle), &result) && result == returned,
                "original callback DWORD result crosses WINAPI thread boundary");
        }
        Check(CloseHandle(reinterpret_cast<HANDLE>(handle)), "owned actual thread handle closes");
    }
    Check(callbackCount == 1 && callbackParam == &callerOwned && callerOwned == (suspended ? 22 : 11),
        "original callback runs once with exact caller-owned parameter");
    Check(callbackTid != 0 && callbackTid == id,
        "original callback runs on exact thread id returned by CRT");
    Check(freeCount == 1 && freedPointer == contextPointer && freedBeforeCallback == 1,
        "owned context is freed exactly once before original callback");
    Check((callbackCw & 0x0F00) == 0x0200 && (callbackMxcsr & 0xE040) == 0x8040,
        "callback observes PinFpu PC53/nearest/FTZ/DAZ");
    Check(ReadCw() == beforeCw && _mm_getcsr() == beforeMxcsr,
        "caller exact CW/MXCSR unchanged after child completion");
    WriteCw(ownCw);
    _mm_setcsr(ownMxcsr);
    Check(ReadCw() == ownCw && _mm_getcsr() == ownMxcsr,
        "fixture restores exact own FPU controls");
}
void TestMissingProvider() {
    Reset();
    g_realBeginThreadEx = nullptr;
    unsigned id = fakeId;
    Check(HookBeginThreadEx(nullptr, 0, Callback, nullptr, 0, &id) == 0 &&
        id == fakeId && allocCount == 0 && freeCount == 0 && callbackCount == 0,
        "missing provider returns null without allocation, callback or id mutation");
}
void TestAllocationFailure() {
    Reset();
    g_realBeginThreadEx = Provider;
    injectAllocFailure = true;
    int callerOwned = 44;
    unsigned id = 0;
    SECURITY_ATTRIBUTES sa{sizeof(sa), nullptr, FALSE};
    DWORD_PTR handle = HookBeginThreadEx(&sa, 0, Callback, &callerOwned, 0, &id);
    Check(handle != 0 && observedStart == Callback && observedParam == &callerOwned &&
        observedSecurity == &sa && observedStack == 0 && observedFlags == 0 && observedIdPointer == &id,
        "injected allocation failure forwards exact original call to actual provider");
    if (handle) {
        DWORD wait = WaitForSingleObject(reinterpret_cast<HANDLE>(handle), 10000);
        Check(wait == WAIT_OBJECT_0, "allocation-failure passthrough callback finishes");
        if (wait == WAIT_OBJECT_0) {
            DWORD result = 0;
            Check(GetExitCodeThread(reinterpret_cast<HANDLE>(handle), &result) && result == returned,
                "allocation-failure passthrough returns original DWORD");
        }
        Check(CloseHandle(reinterpret_cast<HANDLE>(handle)), "passthrough handle closes");
    }
    Check(allocCount == 1 && freeCount == 0 && callbackCount == 1 &&
        callbackParam == &callerOwned && id == observedId,
        "injected allocation failure has one callback and no owned context to free");
    Check(callbackTid != 0 && callbackTid == id,
        "allocation-failure passthrough callback runs on returned thread id");
}
void TestProviderFailure() {
    Reset();
    g_realBeginThreadEx = Provider;
    injectProviderFailure = true;
    int callerOwned = 55;
    unsigned id = fakeId;
    SECURITY_ATTRIBUTES sa{sizeof(sa), nullptr, FALSE};
    Check(HookBeginThreadEx(&sa, 32768, Callback, &callerOwned, CREATE_SUSPENDED, &id) == 0,
        "injected provider failure returns null");
    Check(observedSecurity == &sa && observedStack == 32768 &&
        observedStart == PinnedThreadStart && observedParam == contextPointer &&
        observedFlags == CREATE_SUSPENDED && observedIdPointer == &id && id == fakeId,
        "injected provider failure forwards exact wrapped arguments and preserves id");
    Check(allocCount == 1 && freeCount == 1 && freedPointer == contextPointer &&
        callbackCount == 0, "injected provider failure frees allocated context once");
}
void TestInvalidStartPassthrough(bool selfPinned) {
    Reset();
    g_realBeginThreadEx = Provider;
    captureOnly = true;
    int callerOwned = 66;
    unsigned id = 0;
    SECURITY_ATTRIBUTES sa{sizeof(sa), nullptr, FALSE};
    LPTHREAD_START_ROUTINE start = selfPinned ? PinnedThreadStart : nullptr;
    Check(HookBeginThreadEx(&sa, 8192, start, &callerOwned, CREATE_SUSPENDED, &id) == fakeHandle,
        "null/self-pinned start returns provider sentinel without OS execution");
    Check(observedSecurity == &sa && observedStack == 8192 && observedStart == start &&
        observedParam == &callerOwned && observedFlags == CREATE_SUSPENDED &&
        observedIdPointer == &id && id == fakeId && allocCount == 0 && freeCount == 0 &&
        callbackCount == 0, "null/self-pinned start forwards exact args with no context/callback");
}
void TestSelfPinnedActual() {
    Reset();
    g_realBeginThreadEx = Provider;
    int callerOwned = 77;
    unsigned id = 0;
    auto* wrap = static_cast<ThreadStartWrap*>(
        TestAlloc(::GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(ThreadStartWrap)));
    if (!wrap) { Check(false, "self-pinned fixture allocates valid context"); return; }
    wrap->orig = Callback;
    wrap->param = &callerOwned;
    DWORD_PTR handle = HookBeginThreadEx(nullptr, 0, PinnedThreadStart, wrap, 0, &id);
    Check(handle != 0 && observedStart == PinnedThreadStart && observedParam == wrap &&
        observedIdPointer == &id && id == observedId && id != 0 && allocCount == 1,
        "self-pinned passthrough uses exact valid private context and no added allocation");
    if (handle)
        Check(GetThreadId(reinterpret_cast<HANDLE>(handle)) == id, "self-pinned handle identifies returned thread id");
    if (handle) {
        DWORD wait = WaitForSingleObject(reinterpret_cast<HANDLE>(handle), 10000);
        Check(wait == WAIT_OBJECT_0, "self-pinned actual callback finishes");
        if (wait == WAIT_OBJECT_0) {
            DWORD result = 0;
            Check(GetExitCodeThread(reinterpret_cast<HANDLE>(handle), &result) && result == returned,
                "self-pinned actual callback returns original DWORD");
        }
        Check(CloseHandle(reinterpret_cast<HANDLE>(handle)), "self-pinned handle closes");
    }
    Check(callbackCount == 1 && callbackParam == &callerOwned && freeCount == 1 &&
        freeSucceeded == 1 && freedBeforeCallback == 1,
        "self-pinned actual path frees its valid private context before callback");
    Check(callbackTid != 0 && callbackTid == id,
        "self-pinned callback runs on exact thread id returned by CRT");
}
}

int main() {
    ProviderIdentity();
    if (!osBeginThreadEx || !SupportsDaz()) {
        std::fprintf(stderr, "unsupported host/provider: pinned x86 MSVCR100 and SSE2/FXSR/DAZ required\n");
        return 2;
    }
    Check(true, "host CPUID/FXSAVE supports SSE2, FXSR and DAZ");
    TestActual(false);
    TestActual(true);
    TestActual(false, true);
    TestMissingProvider();
    TestAllocationFailure();
    TestProviderFailure();
    TestInvalidStartPassthrough(false);
    TestInvalidStartPassthrough(true);
    TestSelfPinnedActual();
    std::printf("_beginthreadex actual-source contract: %ld checks, %ld failures\n", checks, failures);
    return failures ? 1 : 0;
}
