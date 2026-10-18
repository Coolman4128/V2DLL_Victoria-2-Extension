#define WIN32_LEAN_AND_MEAN
#define CINTERFACE
#include <windows.h>
#include <d3d9.h>
#include <cstddef>
#include <cstdio>
#include <cstring>

static constexpr DWORD SDK_PRESENT_INTERVAL_IMMEDIATE = D3DPRESENT_INTERVAL_IMMEDIATE;
#undef D3DPRESENT_INTERVAL_IMMEDIATE

static_assert(sizeof(void*) == 4, "Win32 pointer ABI");
static_assert(sizeof(D3DPRESENT_PARAMETERS) == 56, "D3D9 parameter size");
static_assert(offsetof(D3DPRESENT_PARAMETERS, PresentationInterval) == 52, "D3D9 interval offset");
static_assert(offsetof(IDirect3D9Vtbl, CreateDevice) / sizeof(void*) == 16, "IDirect3D9 CreateDevice slot");
static_assert(offsetof(IDirect3DDevice9Vtbl, Reset) / sizeof(void*) == 16, "IDirect3DDevice9 Reset slot");
static_assert(offsetof(IDirect3DDevice9Vtbl, Present) / sizeof(void*) == 17, "IDirect3DDevice9 Present slot");

static int checks = 0;
static int failures = 0;
static void Check(bool ok, const char* label) {
    ++checks;
    if (!ok) { ++failures; std::printf("FAIL %s\n", label); }
}

struct Settings { bool patchD3dFpuPreserve = true; bool patchD3dNoVsync = false; };
static Settings g_settings;
static bool denyProtection = false;
static int protectCalls = 0;
static int failProtectionAt = 0;
static BOOL TestVirtualProtect(LPVOID p, SIZE_T n, DWORD next, PDWORD prior) {
    ++protectCalls;
    if (denyProtection || protectCalls == failProtectionAt) return FALSE;
    return ::VirtualProtect(p, n, next, prior);
}
static int logCalls = 0;
static void Log(const char*, ...) { ++logCalls; }
static int presentStubCalls = 0;
static HRESULT WINAPI HookPresent(void*, const void*, const void*, HWND, const void*) {
    ++presentStubCalls;
    return E_FAIL;
}

#define VirtualProtect TestVirtualProtect
// ACTUAL_D3D_SNIPPETS
#undef VirtualProtect

struct OwnedObject { void** vtable; };
static void** NewTable() {
    return static_cast<void**>(::VirtualAlloc(nullptr, 128 * sizeof(void*),
        MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
}
static void FreeTable(void** table) { if (table) ::VirtualFree(table, 0, MEM_RELEASE); }

static int create9Calls = 0;
static UINT observedSdk = 0;
static void* create9Result = nullptr;
static void* WINAPI Create9Provider(UINT sdk) {
    ++create9Calls; observedSdk = sdk; return create9Result;
}
static int deviceCalls = 0;
static void* observedSelf = nullptr;
static UINT observedAdapter = 0, observedType = 0;
static HWND observedWindow = nullptr;
static DWORD observedFlags = 0;
static void* observedParams = nullptr;
static void** observedOut = nullptr;
static HRESULT deviceResult = S_OK;
static void* emittedDevice = nullptr;
static HRESULT WINAPI CreateDeviceProvider(void* self, UINT adapter, UINT type, HWND wnd,
    DWORD flags, void* params, void** out) {
    ++deviceCalls; observedSelf = self; observedAdapter = adapter; observedType = type;
    observedWindow = wnd; observedFlags = flags; observedParams = params; observedOut = out;
    if (out) *out = emittedDevice;
    return deviceResult;
}
static int resetCalls = 0;
static void* resetSelf = nullptr;
static void* resetParams = nullptr;
static DWORD resetInterval = 0;
static HRESULT resetResult = S_OK;
static HRESULT WINAPI ResetProvider(void* self, void* params) {
    ++resetCalls; resetSelf = self; resetParams = params;
    resetInterval = params ? static_cast<D3DPRESENT_PARAMETERS*>(params)->PresentationInterval : 0;
    return resetResult;
}
static HRESULT WINAPI PresentProvider(void*, const void*, const void*, HWND, const void*) {
    return S_OK;
}
static void* const originalCreate = reinterpret_cast<void*>(CreateDeviceProvider);
static void* const originalReset = reinterpret_cast<void*>(ResetProvider);
static void* const originalPresent = reinterpret_cast<void*>(PresentProvider);

int main() {
    Check(D3DCREATE_FPU_PRESERVE_FLAG == D3DCREATE_FPU_PRESERVE &&
        D3DPRESENT_INTERVAL_IMMEDIATE == SDK_PRESENT_INTERVAL_IMMEDIATE &&
        D3DPRESENT_INTERVAL_OFF == offsetof(D3DPRESENT_PARAMETERS, PresentationInterval) &&
        D3D9_VT_CREATEDEVICE == 16 && D3D9DEV_VT_RESET == 16 && D3D9DEV_VT_PRESENT == 17,
        "production constants match x86 D3D9 SDK");
    void** d3dTable = NewTable();
    void** deviceTable = NewTable();
    Check(d3dTable && deviceTable, "owned writable vtables allocated");
    if (!d3dTable || !deviceTable) { FreeTable(d3dTable); FreeTable(deviceTable); return 2; }
    OwnedObject d3d{d3dTable}, device{deviceTable};
    d3dTable[16] = originalCreate;
    deviceTable[16] = originalReset;
    deviceTable[17] = originalPresent;

    void* saved = nullptr;
    Check(!PatchVtableSlot(nullptr, 16, reinterpret_cast<void*>(HookCreateDevice), &saved) &&
        !PatchVtableSlot(&d3d, 16, nullptr, &saved) &&
        saved == nullptr && d3dTable[16] == originalCreate,
        "null object/hook fail without mutation");
    OwnedObject absent{nullptr};
    Check(!PatchVtableSlot(&absent, 16, reinterpret_cast<void*>(HookCreateDevice), &saved),
        "null table fails");
    denyProtection = true;
    Check(!PatchVtableSlot(&d3d, 16, reinterpret_cast<void*>(HookCreateDevice), &saved) &&
        d3dTable[16] == originalCreate && saved == nullptr,
        "protection failure leaves slot and original untouched");
    denyProtection = false;
    int before = protectCalls;
    Check(PatchVtableSlot(&d3d, 16, reinterpret_cast<void*>(HookCreateDevice), &saved) &&
        d3dTable[16] == reinterpret_cast<void*>(HookCreateDevice) && saved == originalCreate &&
        protectCalls == before + 2,
        "owned table patch saves original and restores page protection");
    MEMORY_BASIC_INFORMATION mbi{};
    Check(::VirtualQuery(&d3dTable[16], &mbi, sizeof(mbi)) == sizeof(mbi) &&
        mbi.Protect == PAGE_READWRITE, "actual owned page protection restored");
    before = protectCalls;
    Check(PatchVtableSlot(&d3d, 16, reinterpret_cast<void*>(HookCreateDevice), &saved) &&
        saved == originalCreate && protectCalls == before,
        "same hook is idempotent without protection change");
    void* preseed = originalPresent;
    Check(PatchVtableSlot(&device, 16, reinterpret_cast<void*>(HookReset), &preseed) &&
        preseed == originalPresent && deviceTable[16] == reinterpret_cast<void*>(HookReset),
        "preseeded original pointer remains unchanged");
    deviceTable[16] = originalReset;

    D3DPRESENT_PARAMETERS params{};
    params.PresentationInterval = D3DPRESENT_INTERVAL_DEFAULT;
    g_settings.patchD3dNoVsync = false;
    ForceImmediatePresent(&params);
    Check(params.PresentationInterval == D3DPRESENT_INTERVAL_DEFAULT,
        "frozen no-vsync false leaves interval alone");
    g_settings.patchD3dNoVsync = true;
    ForceImmediatePresent(nullptr);
    Check(params.PresentationInterval == D3DPRESENT_INTERVAL_DEFAULT,
        "null parameters leave interval alone");
    ForceImmediatePresent(&params);
    Check(params.PresentationInterval == D3DPRESENT_INTERVAL_IMMEDIATE,
        "opt-in no-vsync writes exact SDK interval field");
    g_settings.patchD3dNoVsync = false;

    g_realDirect3DCreate9 = Create9Provider;
    create9Result = &d3d;
    d3dTable[16] = originalCreate;
    g_realCreateDevice = nullptr;
    Check(HookDirect3DCreate9(32) == &d3d && create9Calls == 1 && observedSdk == 32 &&
        d3dTable[16] == reinterpret_cast<void*>(HookCreateDevice) &&
        g_realCreateDevice == CreateDeviceProvider,
        "Direct3DCreate9 returns provider object and hooks owned CreateDevice slot");
    create9Result = nullptr;
    before = protectCalls;
    Check(HookDirect3DCreate9(33) == nullptr && create9Calls == 2 && observedSdk == 33 &&
        protectCalls == before, "null provider object forwards without patch");
    g_realDirect3DCreate9 = nullptr;
    Check(HookDirect3DCreate9(34) == nullptr && create9Calls == 2,
        "missing Direct3DCreate9 provider returns null");

    g_realReset = ResetProvider;
    resetResult = static_cast<HRESULT>(0x88760868u);
    params.PresentationInterval = D3DPRESENT_INTERVAL_DEFAULT;
    Check(HookReset(&device, &params) == resetResult && resetCalls == 1 &&
        resetSelf == &device && resetParams == &params &&
        params.PresentationInterval == D3DPRESENT_INTERVAL_DEFAULT &&
        resetInterval == D3DPRESENT_INTERVAL_DEFAULT,
        "Reset forwards exact self/params and HRESULT with frozen no-vsync false");
    g_settings.patchD3dNoVsync = true;
    Check(HookReset(&device, &params) == resetResult && resetCalls == 2 &&
        params.PresentationInterval == D3DPRESENT_INTERVAL_IMMEDIATE &&
        resetInterval == D3DPRESENT_INTERVAL_IMMEDIATE,
        "Reset optional interval change precedes provider");
    g_settings.patchD3dNoVsync = false;
    g_realReset = nullptr;
    Check(HookReset(&device, &params) == E_FAIL && resetCalls == 2,
        "missing Reset provider returns E_FAIL");

    g_realCreateDevice = CreateDeviceProvider;
    g_realReset = nullptr;
    g_realPresent = nullptr;
    emittedDevice = &device;
    deviceTable[16] = originalReset;
    deviceTable[17] = originalPresent;
    deviceResult = S_OK;
    void* out = nullptr;
    HWND wnd = reinterpret_cast<HWND>(static_cast<UINT_PTR>(0x1234));
    Check(HookCreateDevice(&d3d, 2, 3, wnd, 0x40, &params, &out) == S_OK &&
        deviceCalls == 1 && observedSelf == &d3d && observedAdapter == 2 &&
        observedType == 3 && observedWindow == wnd && observedFlags == (0x40 | D3DCREATE_FPU_PRESERVE) &&
        observedParams == &params && observedOut == &out && out == &device &&
        deviceTable[16] == reinterpret_cast<void*>(HookReset) &&
        deviceTable[17] == reinterpret_cast<void*>(HookPresent) &&
        g_realReset == ResetProvider && g_realPresent == reinterpret_cast<tD3D9Present>(originalPresent) &&
        logCalls == 1 && presentStubCalls == 0,
        "CreateDevice frozen FPU flag and successful owned slot wiring");
    deviceResult = E_FAIL;
    out = nullptr;
    before = protectCalls;
    Check(HookCreateDevice(&d3d, 4, 5, wnd, 0, &params, &out) == E_FAIL &&
        deviceCalls == 2 && protectCalls == before,
        "failed CreateDevice forwards HRESULT without patch");
    deviceResult = S_OK;
    g_settings.patchD3dFpuPreserve = false;
    emittedDevice = nullptr;
    out = reinterpret_cast<void*>(static_cast<UINT_PTR>(0x5678));
    before = protectCalls;
    Check(HookCreateDevice(&d3d, 6, 7, wnd, 0x40, &params, &out) == S_OK &&
        observedFlags == 0x40 && out == nullptr && protectCalls == before,
        "no output object skips hooks; controlled FPU-off branch forwards flags");
    g_settings.patchD3dFpuPreserve = true;
    g_realCreateDevice = nullptr;
    before = deviceCalls;
    Check(HookCreateDevice(&d3d, 1, 1, wnd, 0, &params, &out) == E_FAIL &&
        deviceCalls == before, "missing CreateDevice provider returns E_FAIL");

    // The source does not roll back Reset when the second slot fails.
    g_realCreateDevice = CreateDeviceProvider;
    emittedDevice = &device;
    deviceTable[16] = originalReset;
    deviceTable[17] = originalPresent;
    g_realReset = nullptr;
    g_realPresent = nullptr;
    denyProtection = false;
    failProtectionAt = protectCalls + 3;
    out = nullptr;
    Check(HookCreateDevice(&d3d, 1, 1, wnd, 0, &params, &out) == S_OK &&
        deviceTable[16] == reinterpret_cast<void*>(HookReset) &&
        deviceTable[17] == originalPresent && g_realReset == ResetProvider &&
        g_realPresent == nullptr && logCalls == 1,
        "second-slot protection failure leaves first slot installed without rollback");
    failProtectionAt = 0;
    Check(!denyProtection && presentStubCalls == 0, "fixture made no D3D/Present call");
    FreeTable(d3dTable);
    FreeTable(deviceTable);
    std::printf("D3D owned actual-source contract: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
