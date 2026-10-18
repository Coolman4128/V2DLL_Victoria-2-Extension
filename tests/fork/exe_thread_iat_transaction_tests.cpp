#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "ExeThreadIatTransaction.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <future>
#include <string>
#include <thread>
#include <vector>

using namespace fork_boundary;
static std::atomic<int> checks{0};
static std::atomic<int> failures{0};
static unsigned char* image = nullptr;
static std::uint32_t base = 0;
static constexpr std::uint32_t imageSize = 0x1092000;
static std::array<std::uint32_t, kExeThreadImportCount> providers{};

static void Check(bool result, const char* label)
{
    checks.fetch_add(1);
    if (!result) { failures.fetch_add(1); printf("FAIL %s\n", label); }
}

static HANDLE WINAPI HookCreateThread(LPSECURITY_ATTRIBUTES, SIZE_T,
                                      LPTHREAD_START_ROUTINE, LPVOID, DWORD, LPDWORD) {
    return reinterpret_cast<HANDLE>(static_cast<std::uintptr_t>(1));
}
static HMODULE WINAPI HookLoadLibraryA(LPCSTR) {
    return reinterpret_cast<HMODULE>(static_cast<std::uintptr_t>(2));
}
static HMODULE WINAPI HookLoadLibraryW(LPCWSTR) {
    return reinterpret_cast<HMODULE>(static_cast<std::uintptr_t>(3));
}
static DWORD WINAPI HookGetTickCount() { return 4; }

static ExeIatHooks Hooks()
{
    ExeIatHooks hooks;
    hooks.pointers = {
        static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&HookCreateThread)),
        static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&HookLoadLibraryA)),
        static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&HookLoadLibraryW)),
        static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&HookGetTickCount))
    };
    return hooks;
}

static std::uint32_t ReadSlot(std::size_t index)
{
    return *reinterpret_cast<volatile std::uint32_t*>(image + kExeThreadIatRvas[index]);
}

static void ForceSlot(std::size_t index, std::uint32_t value)
{
    auto* address = image + kExeThreadIatRvas[index];
    DWORD previous = 0, ignored = 0;
    Check(VirtualProtect(address, sizeof(value), PAGE_READWRITE, &previous) != FALSE,
          "fixture makes one slot writable");
    InterlockedExchange(reinterpret_cast<volatile LONG*>(address), static_cast<LONG>(value));
    Check(VirtualProtect(address, sizeof(value), previous, &ignored) != FALSE,
          "fixture restores one slot protection");
}

static void CorruptImportName()
{
    DWORD old = 0, ignored = 0;
    Check(VirtualProtect(image + 0x2480, 1, PAGE_READWRITE, &old) != FALSE,
          "fixture opens import metadata page");
    image[0x2480 + 2] = 'X';
    Check(VirtualProtect(image + 0x2480, 1, old, &ignored) != FALSE,
          "fixture restores import metadata protection");
}

static void ImportName(std::uint32_t rva, const char* name)
{
    *reinterpret_cast<WORD*>(image + rva) = 0;
    memcpy(image + rva + sizeof(WORD), name, strlen(name) + 1);
}

static void ResetImage()
{
    DWORD previous = 0;
    Check(VirtualProtect(image, imageSize, PAGE_READWRITE, &previous) != FALSE,
          "fixture writable reset");
    memset(image, 0, imageSize);
    auto* dos = reinterpret_cast<IMAGE_DOS_HEADER*>(image);
    dos->e_magic = IMAGE_DOS_SIGNATURE;
    dos->e_lfanew = 0x80;
    auto* nt = reinterpret_cast<IMAGE_NT_HEADERS32*>(image + 0x80);
    nt->Signature = IMAGE_NT_SIGNATURE;
    nt->FileHeader.Machine = IMAGE_FILE_MACHINE_I386;
    nt->FileHeader.NumberOfSections = 1;
    nt->FileHeader.SizeOfOptionalHeader = sizeof(IMAGE_OPTIONAL_HEADER32);
    nt->OptionalHeader.Magic = IMAGE_NT_OPTIONAL_HDR32_MAGIC;
    nt->OptionalHeader.ImageBase = base;
    nt->OptionalHeader.SizeOfImage = imageSize;
    nt->OptionalHeader.NumberOfRvaAndSizes = IMAGE_NUMBEROF_DIRECTORY_ENTRIES;
    nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress = 0x2000;
    nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].Size =
        4 * sizeof(IMAGE_IMPORT_DESCRIPTOR);
    auto* section = IMAGE_FIRST_SECTION(nt);
    section->VirtualAddress = 0x1000;
    section->Misc.VirtualSize = imageSize - 0x1000;
    section->SizeOfRawData = imageSize - 0x1000;
    section->PointerToRawData = 0x1000;
    auto* descriptor = reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(image + 0x2000);
    memcpy(image + 0x2200, "KERNEL32.dll", sizeof("KERNEL32.dll"));
    // GetTickCount and LoadLibraryW occupy adjacent IAT DWORDs. The other
    // fixed RVAs are separate single-name descriptors for the same provider.
    descriptor[0].Name = 0x2200;
    descriptor[0].OriginalFirstThunk = 0x2300;
    descriptor[0].FirstThunk = kExeThreadIatRvas[3];
    descriptor[1].Name = 0x2200;
    descriptor[1].OriginalFirstThunk = 0x2320;
    descriptor[1].FirstThunk = kExeThreadIatRvas[0];
    descriptor[2].Name = 0x2200;
    descriptor[2].OriginalFirstThunk = 0x2330;
    descriptor[2].FirstThunk = kExeThreadIatRvas[1];
    auto* original0 = reinterpret_cast<IMAGE_THUNK_DATA32*>(image + 0x2300);
    auto* original1 = reinterpret_cast<IMAGE_THUNK_DATA32*>(image + 0x2320);
    auto* original2 = reinterpret_cast<IMAGE_THUNK_DATA32*>(image + 0x2330);
    original0[0].u1.AddressOfData = 0x2400;
    original0[1].u1.AddressOfData = 0x2440;
    original1[0].u1.AddressOfData = 0x2480;
    original2[0].u1.AddressOfData = 0x24C0;
    ImportName(0x2400, kExeThreadImportNames[3]);
    ImportName(0x2440, kExeThreadImportNames[2]);
    ImportName(0x2480, kExeThreadImportNames[0]);
    ImportName(0x24C0, kExeThreadImportNames[1]);
    for (std::size_t i = 0; i < kExeThreadImportCount; ++i)
        *reinterpret_cast<std::uint32_t*>(image + kExeThreadIatRvas[i]) = providers[i];
    Check(VirtualProtect(image, imageSize, PAGE_EXECUTE_READ, &previous) != FALSE,
          "fixture execute-read mapped image");
}

static bool OriginalProtection()
{
    for (std::size_t i = 0; i < kExeThreadImportCount; ++i)
    {
        MEMORY_BASIC_INFORMATION info = {};
        if (VirtualQuery(image + kExeThreadIatRvas[i], &info, sizeof(info)) != sizeof(info) ||
            info.Protect != PAGE_EXECUTE_READ)
            return false;
    }
    return true;
}

struct Probe
{
    ExeThreadIatTransaction* transaction = nullptr;
    ExeIatHooks hooks = Hooks();
    ExeIatBackend backend;
    std::array<std::uint32_t, kExeThreadImportCount> published{};
    bool didPublish = false;
    bool reenterOnPublish = false;
    ExeIatState reentrantState = ExeIatState::Rejected;
    int opens = 0, closes = 0, exchanges = 0, forwardExchanges = 0, flushes = 0;
    int publicationCount = 0, providerResolutions = 0, slotReads = 0;
    int failOpen = 0, failClose = 0, failForward = 0, failReadback = 0, failRollback = 0;
    bool failAfterSwap = false;
    bool foreignBeforeCas = false;
    std::vector<std::uint32_t> openRvas, closeRvas, exchangeRvas;

    explicit Probe(ExeThreadIatTransaction& group) : transaction(&group)
    {
        backend = MakeWin32ExeThreadIatBackend(base, imageSize,
            [this](const std::array<std::uint32_t, kExeThreadImportCount>& values) {
                published = values;
                didPublish = true;
                ++publicationCount;
                if (reenterOnPublish)
                    reentrantState = transaction->Run(imageSize, hooks, backend).state;
            });
        const auto rawResolve = backend.resolveKernel32;
        backend.resolveKernel32 = [this, rawResolve](const char* name) {
            ++providerResolutions;
            return rawResolve(name);
        };
        const auto rawOpen = backend.memory.beginWrite;
        backend.memory.beginWrite = [this, rawOpen](std::uint32_t rva, std::size_t size,
                                                    std::uintptr_t& token) {
            ++opens;
            if (opens == failOpen) return false;
            bool result = rawOpen(rva, size, token);
            if (result) openRvas.push_back(rva);
            return result;
        };
        const auto rawClose = backend.memory.restoreProtection;
        backend.memory.restoreProtection = [this, rawClose](std::uint32_t rva, std::size_t size,
                                                            std::uintptr_t token) {
            ++closes;
            closeRvas.push_back(rva);
            bool result = rawClose(rva, size, token);
            return result && closes != failClose;
        };
        const auto rawRead = backend.memory.read;
        backend.memory.read = [this, rawRead](std::uint32_t rva, unsigned char* out,
                                             std::size_t count) {
            for (std::size_t i = 0; i < kExeThreadImportCount; ++i)
                if (rva == kExeThreadIatRvas[i] && count == 4) ++slotReads;
            if (failReadback && forwardExchanges >= failReadback &&
                rva == kExeThreadIatRvas[failReadback - 1])
            {
                failReadback = 0;
                return false;
            }
            return rawRead(rva, out, count);
        };
        const auto rawFlush = backend.memory.flushInstructions;
        backend.memory.flushInstructions = [this, rawFlush](std::uint32_t rva,
                                                            std::size_t count) {
            ++flushes;
            return rawFlush(rva, count);
        };
        const auto rawCas = backend.compareExchange;
        backend.compareExchange = [this, rawCas](std::uint32_t rva, std::uint32_t expected,
                                                 std::uint32_t desired, std::uint32_t& observed) {
            ++exchanges;
            exchangeRvas.push_back(rva);
            std::size_t index = kExeThreadImportCount;
            for (std::size_t i = 0; i < kExeThreadImportCount; ++i)
                if (rva == kExeThreadIatRvas[i]) index = i;
            Check(index < kExeThreadImportCount, "CAS targets fixed quartet slot");
            if (index >= kExeThreadImportCount) return ExeIatCasResult::Failed;
            const bool forward = desired == hooks.pointers[index];
            if (forward)
            {
                ++forwardExchanges;
                Check(didPublish && published == providers,
                      "all four exact providers published before first exposed hook");
                Check(opens == 4, "all four protection tokens acquired before first CAS");
                if (foreignBeforeCas && forwardExchanges == 2)
                {
                    ForceSlot(index, 0x12345678);
                    return rawCas(rva, expected, desired, observed);
                }
                if (forwardExchanges == failForward && !failAfterSwap)
                    return ExeIatCasResult::Failed;
                const auto result = rawCas(rva, expected, desired, observed);
                if (forwardExchanges == failForward && failAfterSwap)
                    return ExeIatCasResult::Failed;
                return result;
            }
            if (failRollback && desired == providers[index]) return ExeIatCasResult::Failed;
            return rawCas(rva, expected, desired, observed);
        };
    }
};

static void CheckProviders()
{
    HMODULE kernel32 = GetModuleHandleA("kernel32.dll");
    Check(kernel32 != nullptr, "kernel32 loaded in owned Win32 test");
    for (std::size_t i = 0; i < kExeThreadImportCount; ++i)
    {
        providers[i] = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(
            GetProcAddress(kernel32, kExeThreadImportNames[i])));
        Check(providers[i] != 0, "GetProcAddress resolves provider or forwarder");
    }
}

static void CheckSlots(const std::array<std::uint32_t, kExeThreadImportCount>& values,
                       const char* label)
{
    bool match = true;
    for (std::size_t i = 0; i < kExeThreadImportCount; ++i)
        match &= ReadSlot(i) == values[i];
    Check(match, label);
}

int main()
{
    Check(kExeThreadIatRvas == std::array<std::uint32_t, 4>{0x88A1A8, 0x88A1F8, 0x88A12C, 0x88A128},
          "fixed current EXE quartet RVAs and order");
    const std::array<const char*, 4> expectedNames = {
        "CreateThread", "LoadLibraryA", "LoadLibraryW", "GetTickCount"
    };
    for (std::size_t i = 0; i < kExeThreadImportCount; ++i)
        Check(strcmp(kExeThreadImportNames[i], expectedNames[i]) == 0,
              "fixed current EXE import-by-name order");
    CheckProviders();
    image = static_cast<unsigned char*>(VirtualAlloc(nullptr, imageSize,
        MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
    if (!image) return 2;
    base = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(image));
    const auto hooks = Hooks();
    for (auto hook : hooks.pointers) Check(hook != 0, "four distinct nonzero hook pointers");

    {
        ResetImage();
        ExeThreadIatTransaction group;
        Probe probe(group);
        auto report = group.Run(imageSize, hooks, probe.backend);
        if (report.state != ExeIatState::Applied)
            printf("BASELINE state=%d reason=%s failed=%zu opens=%d exchanges=%d\n",
                   static_cast<int>(report.state), report.reason.c_str(), report.failedIndex,
                   probe.opens, probe.exchanges);
        Check(report.state == ExeIatState::Applied, "real Win32 quartet applies");
        Check(report.originalsPublished && report.anyHookExposed && probe.didPublish,
              "originals published before exposed hook");
        CheckSlots(hooks.pointers, "exact four hook pointers installed");
        Check(OriginalProtection(), "all original IAT protections restored");
        Check(probe.openRvas.size() == 4 && probe.closeRvas.size() == 4,
              "exactly four IAT protections opened and closed");
        Check(probe.flushes == 0, "IAT pointer data does not invoke instruction flush");
        Check(probe.closeRvas == std::vector<std::uint32_t>(probe.openRvas.rbegin(), probe.openRvas.rend()),
              "IAT protections restored in reverse order");
        const int exchanges = probe.exchanges;
        const int opens = probe.opens;
        const int reads = probe.slotReads;
        const int resolutions = probe.providerResolutions;
        const int publications = probe.publicationCount;
        auto again = group.Run(imageSize, hooks, probe.backend);
        Check(again.state == ExeIatState::AlreadyInstalled && probe.exchanges == exchanges,
              "already-installed retry is idempotent without slot CAS");
        Check(probe.slotReads - reads == 4 && probe.providerResolutions - resolutions == 4 &&
              probe.opens == opens && probe.publicationCount == publications,
              "installed fastpath performs four slot reads/provider resolutions, zero CAS/protection/publication");
    }

    for (std::size_t bad : {std::size_t(0), std::size_t(3)})
    {
        ResetImage();
        ForceSlot(bad, 0x12345678);
        const std::vector<unsigned char> before(image, image + imageSize);
        ExeThreadIatTransaction group;
        Probe probe(group);
        auto report = group.Run(imageSize, hooks, probe.backend);
        Check(report.state == ExeIatState::Rejected, "early/later slot mismatch rejected");
        Check(probe.exchanges == 0 && probe.opens == 0,
              "all-four prepare mismatch prevents any slot mutation");
        Check(memcmp(before.data(), image, imageSize) == 0, "mismatch leaves owned image exact");
    }

    {
        ResetImage();
        CorruptImportName();
        const std::vector<unsigned char> before(image, image + imageSize);
        ExeThreadIatTransaction group;
        Probe probe(group);
        auto report = group.Run(imageSize, hooks, probe.backend);
        Check(report.state == ExeIatState::Rejected && !probe.didPublish,
              "initial import-name mismatch rejects before original publication");
        Check(probe.opens == 0 && probe.exchanges == 0 &&
              memcmp(before.data(), image, imageSize) == 0,
              "initial parser mismatch performs zero IAT writes/protection changes");
    }

    for (int mode = 0; mode < 5; ++mode)
    {
        ResetImage();
        ExeThreadIatTransaction group;
        Probe probe(group);
        if (mode == 0) probe.failOpen = 2;
        if (mode == 1) probe.failForward = 2;
        if (mode == 2) { probe.failForward = 2; probe.failAfterSwap = true; }
        if (mode == 3) probe.failReadback = 2;
        if (mode == 4) probe.failClose = 2;
        auto report = group.Run(imageSize, hooks, probe.backend);
        Check(report.state == ExeIatState::RolledBack,
              "acquire/CAS-before/CAS-after/readback/protection failure clean rollback");
        CheckSlots(providers, "failure restores all four original providers");
        Check(OriginalProtection(), "failure restores all IAT protections");
        if (mode != 0) Check(probe.didPublish && report.originalsPublished,
                             "original providers stay published after hook exposure");
        if (mode == 0) Check(probe.exchanges == 0,
                             "later acquisition failure prevents all slot CAS");
        if (mode == 2)
        {
            const std::vector<std::uint32_t> expected = {
                kExeThreadIatRvas[0], kExeThreadIatRvas[1],
                kExeThreadIatRvas[1], kExeThreadIatRvas[0]
            };
            Check(probe.exchangeRvas == expected,
                  "post-swap failure recovers attempted slots in reverse CAS order");
            const bool tokensAvailable = probe.openRvas.size() >= 4 && probe.closeRvas.size() >= 4;
            std::vector<std::uint32_t> reverseOriginals;
            if (tokensAvailable)
            {
                reverseOriginals.assign(probe.openRvas.begin(), probe.openRvas.begin() + 4);
                std::reverse(reverseOriginals.begin(), reverseOriginals.end());
            }
            Check(tokensAvailable &&
                  std::equal(reverseOriginals.begin(), reverseOriginals.end(),
                             probe.closeRvas.end() - 4),
                  "failure restores original protection tokens in reverse order");
        }
    }

    {
        ResetImage();
        ExeThreadIatTransaction group;
        Probe probe(group);
        probe.foreignBeforeCas = true;
        auto report = group.Run(imageSize, hooks, probe.backend);
        Check(report.state == ExeIatState::RollbackFailed || report.state == ExeIatState::Poisoned,
              "foreign mutation during CAS is terminal, not clean rollback");
        Check(ReadSlot(1) == 0x12345678, "foreign slot never overwritten by transaction");
        Check(ReadSlot(0) == providers[0], "earlier exposed hook recovered in reverse");
        Check(probe.didPublish && probe.published == providers,
              "original provider publication retained despite foreign mutation");
        auto retry = group.Run(imageSize, hooks, probe.backend);
        Check(retry.state == ExeIatState::Poisoned, "incomplete recovery blocks retry");
    }

    {
        ResetImage();
        ExeThreadIatTransaction group;
        Probe probe(group);
        probe.failForward = 2;
        probe.failAfterSwap = true;
        probe.failRollback = 1;
        auto report = group.Run(imageSize, hooks, probe.backend);
        Check(report.state == ExeIatState::RollbackFailed,
              "failed rollback CAS classified separately");
        Check(probe.didPublish && probe.published == providers,
              "originals remain published after failed recovery");
        Check(group.Run(imageSize, hooks, probe.backend).state == ExeIatState::Poisoned,
              "failed rollback terminally poisons retry");
    }

    {
        ResetImage();
        ExeThreadIatTransaction group;
        Probe probe(group);
        probe.failForward = 2;
        probe.failAfterSwap = true;
        probe.failClose = 1;
        auto report = group.Run(imageSize, hooks, probe.backend);
        Check(report.state == ExeIatState::RollbackFailed,
              "rollback protection callback failure is terminal even if OS restored page");
        Check(report.originalsPublished && probe.published == providers,
              "published originals survive uncertain protection recovery");
        Check(group.Run(imageSize, hooks, probe.backend).state == ExeIatState::Poisoned,
              "protection rollback failure blocks subsequent retry");
    }

    {
        ResetImage();
        ExeThreadIatTransaction group;
        Probe probe(group);
        probe.failForward = 2;
        Check(group.Run(imageSize, hooks, probe.backend).state == ExeIatState::RolledBack,
              "clean rollback setup for parser retry");
        const int casBefore = probe.exchanges;
        CorruptImportName();
        auto retry = group.Run(imageSize, hooks, probe.backend);
        Check(retry.state == ExeIatState::Rejected && probe.exchanges == casBefore,
              "clean-rollback retry reauthenticates import names before any CAS");
        Check(probe.didPublish && probe.published == providers,
              "clean-rollback parser rejection retains published originals");
    }

    {
        ResetImage();
        ExeThreadIatTransaction group;
        Probe probe(group);
        probe.failForward = 2;
        probe.failAfterSwap = true;
        probe.failReadback = 2;
        auto report = group.Run(imageSize, hooks, probe.backend);
        Check(report.state == ExeIatState::RolledBack,
              "post-exchange Failed plus unreadable immediate slot can recover cleanly");
        Check(report.anyHookExposed && report.originalsPublished && probe.published == providers,
              "uncertain post-exchange exposure retains all published originals");
        CheckSlots(providers, "uncertain exposure rollback restores quartet");
        const auto rawResolve = probe.backend.resolveKernel32;
        probe.backend.resolveKernel32 = [rawResolve](const char* name) {
            std::uint32_t pointer = rawResolve(name);
            return strcmp(name, "CreateThread") == 0 ? pointer + 4 : pointer;
        };
        auto retry = group.Run(imageSize, hooks, probe.backend);
        Check(retry.state == ExeIatState::Poisoned && probe.published == providers,
              "changed-provider retry poisons without replacing exposed originals");
    }

    for (int mode = 0; mode < 4; ++mode)
    {
        ResetImage();
        ExeThreadIatTransaction group;
        Probe probe(group);
        Check(group.Run(imageSize, hooks, probe.backend).state == ExeIatState::Applied,
              "installed-state failure setup applies quartet");
        if (mode == 0)
        {
            CorruptImportName();
        }
        if (mode == 1)
        {
            const auto rawResolve = probe.backend.resolveKernel32;
            probe.backend.resolveKernel32 = [rawResolve](const char* name) {
                std::uint32_t pointer = rawResolve(name);
                return strcmp(name, "CreateThread") == 0 ? pointer + 4 : pointer;
            };
        }
        if (mode == 2)
        {
            const auto rawRead = probe.backend.memory.read;
            probe.backend.memory.read = [rawRead](std::uint32_t rva, unsigned char* out,
                                                 std::size_t count) {
                return rva != kExeThreadIatRvas[0] && rawRead(rva, out, count);
            };
        }
        if (mode == 3) ForceSlot(2, 0x12345678);
        auto retry = group.Run(imageSize, hooks, probe.backend);
        const ExeIatState expected = mode == 0 ? ExeIatState::AlreadyInstalled :
                                                 ExeIatState::Poisoned;
        Check(retry.state == expected,
              "installed fastpath skips metadata reparse but poisons provider/slot changes");
        if (mode == 0)
            Check(retry.originalsPublished && retry.anyHookExposed,
                  "fast idempotent report retains publication/exposure flags");
        Check(group.Run(imageSize, hooks, probe.backend).state == expected,
              "installed fastpath result persists on repeat");
    }

    {
        ResetImage();
        ExeThreadIatTransaction group;
        Probe probe(group);
        probe.reenterOnPublish = true;
        auto report = group.Run(imageSize, hooks, probe.backend);
        Check(report.state == ExeIatState::Applied && probe.reentrantState == ExeIatState::Busy,
              "reentrant retry immediately reports busy");
    }

    {
        ResetImage();
        ExeThreadIatTransaction group;
        Probe probe(group);
        std::promise<void> entered;
        std::promise<void> release;
        auto enteredFuture = entered.get_future();
        auto releaseFuture = release.get_future().share();
        std::atomic<bool> signaled{false};
        const auto raw = probe.backend.compareExchange;
        probe.backend.compareExchange = [&](std::uint32_t rva, std::uint32_t expected,
                                            std::uint32_t desired, std::uint32_t& observed) {
            if (rva == kExeThreadIatRvas[0] && desired == hooks.pointers[0] &&
                !signaled.exchange(true))
            {
                entered.set_value();
                releaseFuture.wait();
            }
            return raw(rva, expected, desired, observed);
        };
        auto worker = std::async(std::launch::async, [&] {
            return group.Run(imageSize, hooks, probe.backend);
        });
        const bool insideCas = enteredFuture.wait_for(std::chrono::seconds(3)) ==
                               std::future_status::ready;
        Check(insideCas, "first installer reached held atomic CAS");
        if (insideCas)
        {
            auto start = std::chrono::steady_clock::now();
            auto retry = group.Run(imageSize, hooks, probe.backend);
            auto elapsed = std::chrono::steady_clock::now() - start;
            Check(retry.state == ExeIatState::Busy && elapsed < std::chrono::seconds(1),
                  "concurrent retry reports Busy without waiting for first CAS");
        }
        release.set_value();
        Check(worker.wait_for(std::chrono::seconds(3)) == std::future_status::ready,
              "held installer finishes after release");
        Check(worker.get().state == ExeIatState::Applied,
              "concurrent first installer applies complete quartet");
    }

    VirtualFree(image, 0, MEM_RELEASE);
    printf("RESULT checks=%d failures=%d slots=%zu\n", checks.load(), failures.load(),
           kExeThreadImportCount);
    return failures.load() ? 1 : 0;
}
