// Owned Win32 process contract sample; the production HookIat body is inserted below.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <array>
#include <cstdio>
#include <cstring>
#include <cstdint>
#include <string>

// ACTUAL_HOOKIAT_INJECTED

namespace {
int checks = 0;
int failures = 0;
void Check(bool condition, const char* name) {
    ++checks;
    if (!condition) { ++failures; std::fprintf(stderr, "FAIL %s\n", name); }
}

std::string Path(HMODULE module) {
    std::array<wchar_t, 32768> wide{};
    DWORD n = GetModuleFileNameW(module, wide.data(), static_cast<DWORD>(wide.size()));
    if (!n || n >= wide.size()) return {};
    int bytes = WideCharToMultiByte(CP_UTF8, 0, wide.data(), static_cast<int>(n), nullptr, 0, nullptr, nullptr);
    if (bytes <= 0) return {};
    std::string result(static_cast<std::size_t>(bytes), '\0');
    if (WideCharToMultiByte(CP_UTF8, 0, wide.data(), static_cast<int>(n), &result[0], bytes, nullptr, nullptr) != bytes) return {};
    return result;
}

void Provider(const char* name, FARPROC procedure) {
    MEMORY_BASIC_INFORMATION mbi{};
    SIZE_T queried = procedure ? VirtualQuery(reinterpret_cast<void*>(procedure), &mbi, sizeof(mbi)) : 0;
    auto* base = static_cast<unsigned char*>(mbi.AllocationBase);
    auto* dos = queried ? reinterpret_cast<IMAGE_DOS_HEADER*>(base) : nullptr;
    auto* nt = dos && dos->e_magic == IMAGE_DOS_SIGNATURE && dos->e_lfanew > 0
        ? reinterpret_cast<IMAGE_NT_HEADERS32*>(base + dos->e_lfanew) : nullptr;
    bool pe32 = nt && nt->Signature == IMAGE_NT_SIGNATURE &&
        nt->OptionalHeader.Magic == IMAGE_NT_OPTIONAL_HDR32_MAGIC;
    const auto offset = queried ? reinterpret_cast<std::uintptr_t>(procedure) - reinterpret_cast<std::uintptr_t>(base) : 0;
    std::printf("PROVIDER %s pointer=%08X allocation=%08X offset=%08X machine=%04X image=%08X path=%s\n",
        name, static_cast<unsigned>(reinterpret_cast<std::uintptr_t>(procedure)),
        static_cast<unsigned>(reinterpret_cast<std::uintptr_t>(base)), static_cast<unsigned>(offset),
        pe32 ? nt->FileHeader.Machine : 0, pe32 ? nt->OptionalHeader.SizeOfImage : 0,
        queried ? Path(static_cast<HMODULE>(mbi.AllocationBase)).c_str() : "");
    Check(queried == sizeof(mbi) && pe32 && nt->FileHeader.Machine == IMAGE_FILE_MACHINE_I386 &&
        offset < nt->OptionalHeader.SizeOfImage && !Path(static_cast<HMODULE>(mbi.AllocationBase)).empty(), name);
}

LONG WINAPI FilterA(EXCEPTION_POINTERS*) { return EXCEPTION_CONTINUE_SEARCH; }
LONG WINAPI FilterB(EXCEPTION_POINTERS*) { return EXCEPTION_EXECUTE_HANDLER; }
LONG CALLBACK Veh(EXCEPTION_POINTERS*) { return EXCEPTION_CONTINUE_SEARCH; }
DWORD WINAPI StackThread(void*) {
    ULONG desired = 32768;
    BOOL ok = SetThreadStackGuarantee(&desired);
    ULONG current = 0;
    BOOL queried = ok ? SetThreadStackGuarantee(&current) : FALSE;
    std::printf("STACK normal=%u previous=%lu requested=32768 query=%u current=%lu isolated_child_thread=1\n",
        static_cast<unsigned>(ok), static_cast<unsigned long>(desired),
        static_cast<unsigned>(queried), static_cast<unsigned long>(current));
    return ok && queried && current >= 32768 ? 0 : 1;
}

struct Fixture {
    unsigned char* base = nullptr;
    DWORD* slot = nullptr;
    Fixture() {
        base = static_cast<unsigned char*>(VirtualAlloc(nullptr, 4096, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
        if (!base) return;
        auto* dos = reinterpret_cast<IMAGE_DOS_HEADER*>(base);
        dos->e_magic = IMAGE_DOS_SIGNATURE;
        dos->e_lfanew = 0x80;
        auto* nt = reinterpret_cast<IMAGE_NT_HEADERS32*>(base + 0x80);
        nt->Signature = IMAGE_NT_SIGNATURE;
        nt->FileHeader.Machine = IMAGE_FILE_MACHINE_I386;
        nt->OptionalHeader.Magic = IMAGE_NT_OPTIONAL_HDR32_MAGIC;
        nt->OptionalHeader.SizeOfImage = 4096;
        nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress = 0x200;
        nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].Size = 2 * sizeof(IMAGE_IMPORT_DESCRIPTOR);
        auto* desc = reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(base + 0x200);
        desc->Name = 0x300;
        desc->OriginalFirstThunk = 0x340;
        desc->FirstThunk = 0x360;
        std::memcpy(base + 0x300, "kernel32.dll", sizeof("kernel32.dll"));
        auto* originalThunk = reinterpret_cast<IMAGE_THUNK_DATA32*>(base + 0x340);
        originalThunk->u1.AddressOfData = 0x380;
        std::memcpy(base + 0x382, "SetUnhandledExceptionFilter", sizeof("SetUnhandledExceptionFilter"));
        slot = reinterpret_cast<DWORD*>(base + 0x360);
    }
    ~Fixture() { if (base) VirtualFree(base, 0, MEM_RELEASE); }
    bool Protect(DWORD mode) {
        DWORD old = 0;
        return base && VirtualProtect(base, 4096, mode, &old) != 0;
    }
    DWORD Protection() const {
        MEMORY_BASIC_INFORMATION mbi{};
        return slot && VirtualQuery(slot, &mbi, sizeof(mbi)) == sizeof(mbi) ? mbi.Protect : 0;
    }
};

void IatContracts(FARPROC setUef, FARPROC other, FARPROC foreign) {
    Fixture f;
    Check(f.base && f.slot, "fixture allocation");
    if (!f.base || !f.slot) return;
    const DWORD original = static_cast<DWORD>(reinterpret_cast<std::uintptr_t>(setUef));
    const DWORD replacement = static_cast<DWORD>(reinterpret_cast<std::uintptr_t>(&FilterA));
    *f.slot = original;
    Check(f.Protection() == PAGE_READWRITE, "fixture writable initial");
    Check(f.Protect(PAGE_READONLY), "fixture protect readonly");
    void* published = nullptr;
    Check(HookIat(reinterpret_cast<HMODULE>(f.base), "kernel32.dll", "SetUnhandledExceptionFilter",
        reinterpret_cast<void*>(&FilterA), &published), "actual helper positive");
    Check(*f.slot == replacement, "actual helper wrote exact slot");
    Check(published == reinterpret_cast<void*>(setUef), "actual helper publishes original");
    Check(f.Protection() == PAGE_READONLY, "actual helper restored readonly page");
    void* untouched = reinterpret_cast<void*>(static_cast<std::uintptr_t>(0x12345678));
    Check(HookIat(reinterpret_cast<HMODULE>(f.base), "kernel32.dll", "SetUnhandledExceptionFilter",
        reinterpret_cast<void*>(&FilterA), &untouched), "already hooked returns true");
    Check(untouched == reinterpret_cast<void*>(static_cast<std::uintptr_t>(0x12345678)) &&
        *f.slot == replacement && f.Protection() == PAGE_READONLY,
        "already hooked does not publish original");

    Check(f.Protect(PAGE_READWRITE), "fixture reset protection");
    *f.slot = original;
    auto reject = [&](HMODULE module, const char* dll, const char* function, void* hook, const char* name) {
        void* sentinel = reinterpret_cast<void*>(static_cast<std::uintptr_t>(0x12345678));
        const DWORD prior = *f.slot;
        const DWORD protect = f.Protection();
        std::array<unsigned char, 4096> before{};
        std::memcpy(before.data(), f.base, before.size());
        Check(!HookIat(module, dll, function, hook, &sentinel), name);
        Check(*f.slot == prior && sentinel == reinterpret_cast<void*>(static_cast<std::uintptr_t>(0x12345678)) &&
            f.Protection() == protect && std::memcmp(before.data(), f.base, before.size()) == 0,
            "reject preserves fixture/output/protection");
    };
    reject(nullptr, "kernel32.dll", "SetUnhandledExceptionFilter", reinterpret_cast<void*>(&FilterA), "null module");
    reject(reinterpret_cast<HMODULE>(f.base), nullptr, "SetUnhandledExceptionFilter", reinterpret_cast<void*>(&FilterA), "null dll");
    reject(reinterpret_cast<HMODULE>(f.base), "kernel32.dll", nullptr, reinterpret_cast<void*>(&FilterA), "null function");
    reject(reinterpret_cast<HMODULE>(f.base), "kernel32.dll", "SetUnhandledExceptionFilter", nullptr, "null hook");
    reject(reinterpret_cast<HMODULE>(f.base), "user32.dll", "SetUnhandledExceptionFilter", reinterpret_cast<void*>(&FilterA), "missing dll");
    reject(reinterpret_cast<HMODULE>(f.base), "kernel32.dll", "MissingFunction", reinterpret_cast<void*>(&FilterA), "missing function");
    auto* dos = reinterpret_cast<IMAGE_DOS_HEADER*>(f.base);
    const WORD magic = dos->e_magic;
    dos->e_magic = 0;
    reject(reinterpret_cast<HMODULE>(f.base), "kernel32.dll", "SetUnhandledExceptionFilter", reinterpret_cast<void*>(&FilterA), "invalid DOS");
    dos->e_magic = magic;
    auto* nt = reinterpret_cast<IMAGE_NT_HEADERS32*>(f.base + 0x80);
    nt->Signature = 0;
    reject(reinterpret_cast<HMODULE>(f.base), "kernel32.dll", "SetUnhandledExceptionFilter", reinterpret_cast<void*>(&FilterA), "invalid NT");
    nt->Signature = IMAGE_NT_SIGNATURE;
    nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress = 0;
    reject(reinterpret_cast<HMODULE>(f.base), "kernel32.dll", "SetUnhandledExceptionFilter", reinterpret_cast<void*>(&FilterA), "missing imports");
    nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress = 0x200;
    *f.slot = static_cast<DWORD>(reinterpret_cast<std::uintptr_t>(f.base + 0x3C0));
    reject(reinterpret_cast<HMODULE>(f.base), "kernel32.dll", "SetUnhandledExceptionFilter", reinterpret_cast<void*>(&FilterA), "self module original rejected");

    // Expected limitation: a wrong, mapped, foreign function is accepted.
    // The pointer is recorded and never invoked as SetUnhandledExceptionFilter.
    *f.slot = static_cast<DWORD>(reinterpret_cast<std::uintptr_t>(other));
    Check(f.Protect(PAGE_READONLY), "counterexample protect readonly");
    void* wrongPublished = nullptr;
    Check(HookIat(reinterpret_cast<HMODULE>(f.base), "kernel32.dll", "SetUnhandledExceptionFilter",
        reinterpret_cast<void*>(&FilterA), &wrongPublished), "wrong procedure accepted counterexample");
    Check(wrongPublished == reinterpret_cast<void*>(other) && *f.slot == replacement &&
        f.Protection() == PAGE_READONLY, "wrong procedure publication counterexample");
    Check(f.Protect(PAGE_READWRITE), "foreign counterexample reset protection");
    *f.slot = static_cast<DWORD>(reinterpret_cast<std::uintptr_t>(foreign));
    Check(f.Protect(PAGE_READONLY), "foreign counterexample protect readonly");
    void* foreignPublished = nullptr;
    Check(HookIat(reinterpret_cast<HMODULE>(f.base), "kernel32.dll", "SetUnhandledExceptionFilter",
        reinterpret_cast<void*>(&FilterA), &foreignPublished), "foreign provider accepted counterexample");
    Check(foreignPublished == reinterpret_cast<void*>(foreign) && *f.slot == replacement &&
        f.Protection() == PAGE_READONLY, "foreign provider publication counterexample");
    std::printf("IAT wrong_procedure_accepted=1 foreign_provider_accepted=1 already_hooked_without_original=1 restoration_failure_unproven=1\n");
}
}

int main() {
    static_assert(sizeof(void*) == 4, "Win32 only");
    HMODULE k32 = GetModuleHandleA("kernel32.dll");
    Check(k32 != nullptr, "kernel32 loaded");
    if (!k32) return 1;
    const char* names[] = {"SetUnhandledExceptionFilter", "AddVectoredExceptionHandler",
        "RemoveVectoredExceptionHandler", "SetThreadStackGuarantee"};
    FARPROC resolved[4]{};
    for (int i = 0; i != 4; ++i) {
        resolved[i] = GetProcAddress(k32, names[i]);
        Check(resolved[i] != nullptr, names[i]);
        if (resolved[i]) Provider(names[i], resolved[i]);
    }
    if (!resolved[0] || !resolved[1] || !resolved[2] || !resolved[3]) return 1;
    FARPROC wrong = GetProcAddress(k32, "GetTickCount");
    Check(wrong != nullptr, "wrong procedure control loaded");
    if (!wrong) return 1;
    Provider("wrong_GetTickCount", wrong);

    Check(FilterA != FilterB, "distinct UEF callbacks");
    LPTOP_LEVEL_EXCEPTION_FILTER prior = SetUnhandledExceptionFilter(FilterA);
    LPTOP_LEVEL_EXCEPTION_FILTER returnedA = SetUnhandledExceptionFilter(FilterB);
    LPTOP_LEVEL_EXCEPTION_FILTER returnedB = SetUnhandledExceptionFilter(prior);
    Check(returnedA == FilterA && returnedB == FilterB, "SetUEF previous and exact restoration");
    LPTOP_LEVEL_EXCEPTION_FILTER beforeNull = SetUnhandledExceptionFilter(nullptr);
    LPTOP_LEVEL_EXCEPTION_FILTER returnedNull = SetUnhandledExceptionFilter(FilterA);
    LPTOP_LEVEL_EXCEPTION_FILTER returnedRestore = SetUnhandledExceptionFilter(beforeNull);
    Check(beforeNull == prior && returnedNull == nullptr && returnedRestore == FilterA,
        "SetUEF NULL prior return and exact restoration");
    std::printf("UEF previous=%08X A_returned=%08X B_returned=%08X before_null=%08X null_returned=%08X restore_returned=%08X\n",
        static_cast<unsigned>(reinterpret_cast<std::uintptr_t>(prior)),
        static_cast<unsigned>(reinterpret_cast<std::uintptr_t>(returnedA)),
        static_cast<unsigned>(reinterpret_cast<std::uintptr_t>(returnedB)),
        static_cast<unsigned>(reinterpret_cast<std::uintptr_t>(beforeNull)),
        static_cast<unsigned>(reinterpret_cast<std::uintptr_t>(returnedNull)),
        static_cast<unsigned>(reinterpret_cast<std::uintptr_t>(returnedRestore)));

    PVOID handle = AddVectoredExceptionHandler(1, Veh);
    Check(handle != nullptr, "VEH registration");
    ULONG removed = handle ? RemoveVectoredExceptionHandler(handle) : 0;
    if (handle) Check(removed != 0, "VEH remove exact handle");
    std::printf("VEH registered=%u removed=%u fatal_path_tested=0\n", handle != nullptr, removed != 0);

    HANDLE thread = CreateThread(nullptr, 0, StackThread, nullptr, 0, nullptr);
    Check(thread != nullptr, "stack disposable child created");
    if (thread) {
        DWORD wait = WaitForSingleObject(thread, 10000);
        DWORD code = 1;
        Check(wait == WAIT_OBJECT_0 && GetExitCodeThread(thread, &code) && code == 0,
            "stack guarantee child normal return");
        CloseHandle(thread);
    }
    IatContracts(resolved[0], wrong, resolved[1]);
    std::printf("RESULT checks=%d failures=%d\n", checks, failures);
    return failures ? 1 : 0;
}
