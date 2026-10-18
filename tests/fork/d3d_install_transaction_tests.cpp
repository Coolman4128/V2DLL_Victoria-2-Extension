#define WIN32_LEAN_AND_MEAN
#define CINTERFACE
#include <windows.h>
#include <bcrypt.h>
#include <d3d9.h>
#include "D3dInstallTransaction.h"
#include "Win32Memory.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <future>
#include <functional>
#include <string>
#include <vector>

using namespace fork_boundary;

static_assert(sizeof(void*) == 4, "Win32 pointer ABI");
static_assert(sizeof(D3DPRESENT_PARAMETERS) == 56, "D3D9 presentation ABI");
static_assert(offsetof(D3DPRESENT_PARAMETERS, PresentationInterval) == 52,
              "D3D9 presentation interval ABI");
static_assert(offsetof(IDirect3D9Vtbl, CreateDevice) / sizeof(void*) == 16,
              "ordinary IDirect3D9 CreateDevice slot");
static_assert(offsetof(IDirect3DDevice9Vtbl, Reset) / sizeof(void*) == 16,
              "ordinary IDirect3DDevice9 Reset slot");
static_assert(offsetof(IDirect3DDevice9Vtbl, Present) / sizeof(void*) == 17,
              "ordinary IDirect3DDevice9 Present slot");
static_assert(D3DCREATE_FPU_PRESERVE == 0x2, "FPU preserve flag");
static_assert(D3DPRESENT_INTERVAL_IMMEDIATE == 0x80000000u, "immediate interval");
#undef D3DPRESENT_INTERVAL_IMMEDIATE

static std::atomic<int> checks{0};
static std::atomic<int> failures{0};
static unsigned char* exeImage = nullptr;
static std::uint32_t exeBase = 0;
static constexpr std::uint32_t exeImageSize = 0x1092000;
static std::uint32_t providerBase = 0;
static std::uint32_t providerImageSize = 0;
static std::uint32_t providerCreate9 = 0;
static std::wstring providerPath;
static D3dInstallExpectation expectation;
static std::uint32_t* providerD3dTable = nullptr;
static std::uint32_t* providerD3dTableSame = nullptr;
static std::uint32_t* providerDeviceTable = nullptr;
static std::uint32_t* providerDeviceTableSame = nullptr;
static std::uint32_t* providerDeviceTableDifferent = nullptr;
static std::uint32_t* providerWritableTable = nullptr;

static void Check(bool yes, const char* name)
{
    checks.fetch_add(1);
    if (!yes) { failures.fetch_add(1); std::printf("FAIL %s\n", name); }
}

static std::array<unsigned char, 32> Sha256(const unsigned char* data, std::size_t count)
{
    std::array<unsigned char, 32> result{};
    BCRYPT_ALG_HANDLE algorithm = nullptr;
    const NTSTATUS opened = BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM,
                                                        nullptr, 0);
    Check(opened >= 0, "bcrypt sha256 opens");
    if (opened < 0) return result;
    const NTSTATUS hashed = BCryptHash(algorithm, nullptr, 0,
        const_cast<PUCHAR>(data), static_cast<ULONG>(count), result.data(),
        static_cast<ULONG>(result.size()));
    Check(hashed >= 0, "bcrypt sha256 hashes");
    BCryptCloseAlgorithmProvider(algorithm, 0);
    return result;
}

static bool FileBytes(const std::wstring& path, std::vector<unsigned char>& data)
{
    HANDLE handle = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                                OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (handle == INVALID_HANDLE_VALUE) return false;
    LARGE_INTEGER length{};
    bool ok = GetFileSizeEx(handle, &length) && length.QuadPart > 0 &&
              length.QuadPart <= 0x1000000;
    if (ok)
    {
        data.resize(static_cast<std::size_t>(length.QuadPart));
        DWORD read = 0;
        ok = ReadFile(handle, data.data(), static_cast<DWORD>(data.size()), &read, nullptr) &&
             read == data.size();
    }
    CloseHandle(handle);
    return ok;
}

static void ResetExeImage()
{
    DWORD old = 0;
    Check(VirtualProtect(exeImage, exeImageSize, PAGE_READWRITE, &old) != FALSE,
          "owned EXE image writable for reset");
    memset(exeImage, 0, exeImageSize);
    auto* dos = reinterpret_cast<IMAGE_DOS_HEADER*>(exeImage);
    dos->e_magic = IMAGE_DOS_SIGNATURE;
    dos->e_lfanew = 0x80;
    auto* nt = reinterpret_cast<IMAGE_NT_HEADERS32*>(exeImage + 0x80);
    nt->Signature = IMAGE_NT_SIGNATURE;
    nt->FileHeader.Machine = IMAGE_FILE_MACHINE_I386;
    nt->FileHeader.NumberOfSections = 1;
    nt->FileHeader.SizeOfOptionalHeader = sizeof(IMAGE_OPTIONAL_HEADER32);
    nt->OptionalHeader.Magic = IMAGE_NT_OPTIONAL_HDR32_MAGIC;
    nt->OptionalHeader.ImageBase = exeBase;
    nt->OptionalHeader.SizeOfImage = exeImageSize;
    nt->OptionalHeader.NumberOfRvaAndSizes = IMAGE_NUMBEROF_DIRECTORY_ENTRIES;
    nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress = 0x2000;
    nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].Size =
        2 * sizeof(IMAGE_IMPORT_DESCRIPTOR);
    auto* section = IMAGE_FIRST_SECTION(nt);
    memcpy(section->Name, ".text", sizeof(".text") - 1);
    section->VirtualAddress = 0x1000;
    section->Misc.VirtualSize = exeImageSize - 0x1000;
    section->SizeOfRawData = exeImageSize - 0x1000;
    section->PointerToRawData = 0x1000;
    auto* descriptor = reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(exeImage + 0x2000);
    descriptor[0].Name = 0x2100;
    descriptor[0].OriginalFirstThunk = 0x2200;
    descriptor[0].FirstThunk = kPinnedD3dCreate9IatRva;
    memcpy(exeImage + 0x2100, "d3d9.dll", sizeof("d3d9.dll"));
    auto* lookup = reinterpret_cast<IMAGE_THUNK_DATA32*>(exeImage + 0x2200);
    lookup[0].u1.AddressOfData = 0x2300;
    *reinterpret_cast<WORD*>(exeImage + 0x2300) = 0;
    memcpy(exeImage + 0x2302, "Direct3DCreate9", sizeof("Direct3DCreate9"));
    *reinterpret_cast<std::uint32_t*>(exeImage + kPinnedD3dCreate9IatRva) = providerCreate9;
    Check(VirtualProtect(exeImage, exeImageSize, PAGE_EXECUTE_READ, &old) != FALSE,
          "owned EXE image restored RX");
}

static std::uint32_t ExeSlot()
{
    return *reinterpret_cast<volatile std::uint32_t*>(exeImage + kPinnedD3dCreate9IatRva);
}

static void ForceExeSlot(std::uint32_t value)
{
    auto* address = exeImage + kPinnedD3dCreate9IatRva;
    DWORD old = 0, ignored = 0;
    Check(VirtualProtect(address, 4, PAGE_READWRITE, &old) != FALSE,
          "owned EXE IAT temporarily writable");
    InterlockedExchange(reinterpret_cast<volatile LONG*>(address), static_cast<LONG>(value));
    Check(VirtualProtect(address, 4, old, &ignored) != FALSE,
          "owned EXE IAT protection restored");
}

static void ForceExeByte(std::uint32_t rva, unsigned char value)
{
    auto* address = exeImage + rva;
    DWORD old = 0, ignored = 0;
    Check(rva < exeImageSize &&
          VirtualProtect(address, 1, PAGE_READWRITE, &old) != FALSE,
          "owned EXE metadata temporarily writable");
    *address = value;
    Check(VirtualProtect(address, 1, old, &ignored) != FALSE,
          "owned EXE metadata protection restored");
}

static void ForceProviderSlot(std::uint32_t* slot, std::uint32_t value)
{
    DWORD old = 0, ignored = 0;
    Check(VirtualProtect(slot, 4, PAGE_READWRITE, &old) != FALSE,
          "owned provider slot temporarily writable");
    InterlockedExchange(reinterpret_cast<volatile LONG*>(slot), static_cast<LONG>(value));
    Check(VirtualProtect(slot, 4, old, &ignored) != FALSE,
          "owned provider slot protection restored");
}

static bool PrepareProvider(HMODULE surrogate)
{
    if (!surrogate) return false;
    providerBase = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(surrogate));
    auto* bytes = reinterpret_cast<unsigned char*>(surrogate);
    auto* dos = reinterpret_cast<IMAGE_DOS_HEADER*>(bytes);
    auto* nt = reinterpret_cast<IMAGE_NT_HEADERS32*>(bytes + dos->e_lfanew);
    providerImageSize = nt->OptionalHeader.SizeOfImage;
    providerCreate9 = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(
        GetProcAddress(surrogate, "Direct3DCreate9")));
    if (!providerCreate9 || !providerImageSize) return false;
    std::vector<unsigned char> disk;
    if (!FileBytes(providerPath, disk)) return false;
    expectation.exeImageSize = exeImageSize;
    expectation.exeIatRva = kPinnedD3dCreate9IatRva;
    expectation.providerDiskSize = disk.size();
    expectation.providerDiskSha256 = Sha256(disk.data(), disk.size());
    expectation.providerImageSize = providerImageSize;
    expectation.providerTimestamp = nt->FileHeader.TimeDateStamp;
    expectation.providerPreferredBase = nt->OptionalHeader.ImageBase;
    expectation.create9ExportRva = providerCreate9 - providerBase;
    expectation.create9ExportOrdinal = 37;
    const auto* section = IMAGE_FIRST_SECTION(nt);
    bool foundText = false;
    for (unsigned i = 0; i < nt->FileHeader.NumberOfSections; ++i)
    {
        if (memcmp(section[i].Name, ".text", sizeof(".text") - 1) != 0) continue;
        expectation.providerTextRva = section[i].VirtualAddress;
        expectation.providerTextVirtualSize = section[i].Misc.VirtualSize;
        expectation.providerTextRawSize = section[i].SizeOfRawData;
        const auto raw = section[i].PointerToRawData;
        if (raw > disk.size() || section[i].SizeOfRawData > disk.size() - raw)
            return false;
        std::vector<unsigned char> text(section[i].Misc.VirtualSize, 0);
        memcpy(text.data(), disk.data() + raw,
               (std::min)(text.size(), static_cast<std::size_t>(section[i].SizeOfRawData)));
        expectation.normalizedTextSha256 = Sha256(text.data(), text.size());
        foundText = true;
        break;
    }
    return foundText;
}

struct OwnedObject { std::uint32_t vtable = 0; };

static std::uint32_t Address(FARPROC value)
{
    return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(value));
}

static void* WINAPI HookCreate9(UINT) { return nullptr; }
static void* WINAPI HookCreate9Alternate(UINT) { return reinterpret_cast<void*>(1); }
static HRESULT WINAPI HookCreateDevice(void*, UINT, UINT, HWND, DWORD, void*, void**)
{
    return E_FAIL;
}
static HRESULT WINAPI HookCreateDeviceAlternate(void*, UINT, UINT, HWND, DWORD, void*, void**)
{
    return E_NOTIMPL;
}
static HRESULT WINAPI HookReset(void*, void*) { return E_FAIL; }
static HRESULT WINAPI HookResetAlternate(void*, void*) { return E_NOTIMPL; }
static HRESULT WINAPI HookPresent(void*, const void*, const void*, HWND, const void*)
{
    return E_FAIL;
}

struct Probe
{
    D3dInstallBackend backend;
    int retains = 0, resolutions = 0, create9Publications = 0;
    int createDevicePublications = 0, pairPublications = 0;
    int exeOpens = 0, providerOpens = 0, exeCloses = 0, providerCloses = 0;
    int exchanges = 0, providerSlotReads = 0;
    int failProviderOpen = 0, failProviderClose = 0, failCas = 0;
    int failExeReadAfterCas = 0;
    int failProviderReadAfterCas = 0;
    bool failAfterSwap = false, failRollbackCas = false, foreignBeforeCas = false;
    std::vector<std::uint32_t> casAddresses;
    std::uint32_t publishedCreate9 = 0, publishedCreateDevice = 0;
    std::uint32_t publishedReset = 0, publishedPresent = 0;

    Probe()
    {
        casAddresses.reserve(16);
        backend = MakeWin32D3dInstallBackend(exeBase, exeImageSize,
            providerBase, providerImageSize,
            [this](std::uint32_t value) {
                ++create9Publications;
                publishedCreate9 = value;
            },
            [this](std::uint32_t value) {
                ++createDevicePublications;
                publishedCreateDevice = value;
            },
            [this](std::uint32_t reset, std::uint32_t present) {
                ++pairPublications;
                publishedReset = reset;
                publishedPresent = present;
            });
        const auto retain = backend.retainFromAddress;
        backend.retainFromAddress = [this, retain](std::uint32_t address,
                                                  std::uint32_t& retained) {
            ++retains;
            return retain(address, retained);
        };
        const auto resolve = backend.resolveCreate9;
        backend.resolveCreate9 = [this, resolve](std::uint32_t authenticatedBase) {
            ++resolutions;
            Check(authenticatedBase == providerBase,
                  "Create9 resolver receives authenticated provider base");
            return resolve(authenticatedBase);
        };
        backend.immutableProviderStorage = [](std::uint32_t owner,
                                              std::uint32_t address, std::size_t count) {
            if (owner != providerBase || !address || !count || address < owner ||
                address - owner < expectation.providerTextRva ||
                address - owner - expectation.providerTextRva >=
                    expectation.providerTextVirtualSize ||
                count > expectation.providerTextVirtualSize -
                    (address - owner - expectation.providerTextRva)) return false;
            std::uintptr_t cursor = address;
            const auto end = cursor + count;
            while (cursor < end)
            {
                MEMORY_BASIC_INFORMATION info{};
                if (VirtualQuery(reinterpret_cast<void*>(cursor), &info, sizeof(info)) !=
                        sizeof(info) ||
                    reinterpret_cast<std::uintptr_t>(info.AllocationBase) != owner ||
                    info.Type != MEM_IMAGE || info.State != MEM_COMMIT ||
                    (info.Protect & PAGE_GUARD)) return false;
                const auto protect = info.Protect & 0xff;
                if (protect != PAGE_EXECUTE_READ && protect != PAGE_READONLY)
                    return false;
                const auto next = reinterpret_cast<std::uintptr_t>(info.BaseAddress) +
                                  info.RegionSize;
                if (next <= cursor) return false;
                cursor = next;
            }
            return true;
        };
        const auto exeOpen = backend.exeMemory.beginWrite;
        backend.exeMemory.beginWrite = [this, exeOpen](std::uint32_t rva,
                                                       std::size_t count,
                                                       std::uintptr_t& token) {
            ++exeOpens;
            return exeOpen(rva, count, token);
        };
        const auto exeRead = backend.exeMemory.read;
        backend.exeMemory.read = [this, exeRead](std::uint32_t rva,
                                                unsigned char* output,
                                                std::size_t count) {
            if (failExeReadAfterCas && exchanges >= failExeReadAfterCas &&
                rva == kPinnedD3dCreate9IatRva && count == sizeof(std::uint32_t)) {
                failExeReadAfterCas = 0;
                return false;
            }
            return exeRead(rva, output, count);
        };
        const auto exeClose = backend.exeMemory.restoreProtection;
        backend.exeMemory.restoreProtection = [this, exeClose](std::uint32_t rva,
                                                               std::size_t count,
                                                               std::uintptr_t token) {
            ++exeCloses;
            return exeClose(rva, count, token);
        };
        const auto providerOpen = backend.providerMemory.beginWrite;
        backend.providerMemory.beginWrite = [this, providerOpen](std::uint32_t rva,
                                                                 std::size_t count,
                                                                 std::uintptr_t& token) {
            ++providerOpens;
            if (providerOpens == failProviderOpen) return false;
            return providerOpen(rva, count, token);
        };
        const auto providerClose = backend.providerMemory.restoreProtection;
        backend.providerMemory.restoreProtection = [this, providerClose](
            std::uint32_t rva, std::size_t count, std::uintptr_t token) {
            ++providerCloses;
            const bool restored = providerClose(rva, count, token);
            return restored && providerCloses != failProviderClose;
        };
        const auto providerRead = backend.providerMemory.read;
        backend.providerMemory.read = [this, providerRead](std::uint32_t rva,
                                                           unsigned char* output,
                                                           std::size_t count) {
            if (count == 4) ++providerSlotReads;
            if (failProviderReadAfterCas && exchanges >= failProviderReadAfterCas &&
                count == 4)
            {
                failProviderReadAfterCas = 0;
                return false;
            }
            return providerRead(rva, output, count);
        };
        const auto cas = backend.compareExchange;
        backend.compareExchange = [this, cas](std::uint32_t address,
                                              std::uint32_t expectedValue,
                                              std::uint32_t desired,
                                              std::uint32_t& observed) {
            ++exchanges;
            casAddresses.push_back(address);
            const bool forward = desired == Address(reinterpret_cast<FARPROC>(&HookCreate9)) ||
                desired == Address(reinterpret_cast<FARPROC>(&HookCreateDevice)) ||
                desired == Address(reinterpret_cast<FARPROC>(&HookReset)) ||
                desired == Address(reinterpret_cast<FARPROC>(&HookPresent));
            if (desired == Address(reinterpret_cast<FARPROC>(&HookCreate9)))
                Check(create9Publications == 1 && publishedCreate9 == providerCreate9,
                      "Create9 original published before IAT exposure");
            if (desired == Address(reinterpret_cast<FARPROC>(&HookCreateDevice)))
                Check(createDevicePublications == 1 && publishedCreateDevice != 0,
                      "CreateDevice original published before table exposure");
            if (desired == Address(reinterpret_cast<FARPROC>(&HookReset)) ||
                desired == Address(reinterpret_cast<FARPROC>(&HookPresent)))
                Check(pairPublications == 1 && publishedReset && publishedPresent,
                      "Reset and Present originals published before either exposure");
            if (!forward && failRollbackCas) return D3dCasResult::Failed;
            if (forward && foreignBeforeCas && exchanges == failCas)
                InterlockedExchange(reinterpret_cast<volatile LONG*>(
                    static_cast<std::uintptr_t>(address)), 0x12345678);
            if (exchanges == failCas && !failAfterSwap)
                return D3dCasResult::Failed;
            const auto result = cas(address, expectedValue, desired, observed);
            if (exchanges == failCas && failAfterSwap)
                return D3dCasResult::Failed;
            return result;
        };
    }
};

static bool ApplyImport(D3dInstallTransaction& group, Probe& probe)
{
    const auto report = group.InstallExeImport(providerCreate9,
        Address(reinterpret_cast<FARPROC>(&HookCreate9)), probe.backend, expectation);
    Check(report.state == D3dInstallState::Applied,
          "prerequisite EXE import applied");
    return report.state == D3dInstallState::Applied;
}

static bool ApplyCreate(D3dInstallTransaction& group, Probe& probe,
                        OwnedObject& object)
{
    const auto report = group.InstallCreateDeviceTable(
        static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&object)),
        Address(reinterpret_cast<FARPROC>(&HookCreateDevice)), probe.backend);
    Check(report.state == D3dInstallState::Applied,
          "prerequisite CreateDevice table applied");
    return report.state == D3dInstallState::Applied;
}

static void TestPairFault(const std::wstring& name)
{
    D3dInstallTransaction group;
    Probe probe;
    OwnedObject d3dObject{static_cast<std::uint32_t>(
        reinterpret_cast<std::uintptr_t>(providerD3dTable))};
    OwnedObject deviceObject{static_cast<std::uint32_t>(
        reinterpret_cast<std::uintptr_t>(providerDeviceTable))};
    if (!ApplyImport(group, probe) || !ApplyCreate(group, probe, d3dObject)) return;
    const auto resetOriginal = providerDeviceTable[kD3dDeviceResetSlot];
    const auto presentOriginal = providerDeviceTable[kD3dDevicePresentSlot];
    const int casBefore = probe.exchanges;
    const std::size_t casIndex = probe.casAddresses.size();
    if (name == L"pair_begin_second")
        probe.failProviderOpen = probe.providerOpens + 2;
    if (name == L"pair_second_fail" || name == L"pair_after_swap" ||
        name == L"pair_tuple_after_rollback" ||
        name == L"pair_rollback_close" || name == L"pair_rollback_cas" ||
        name == L"pair_foreign_mid_cas")
        probe.failCas = probe.exchanges + 2;
    if (name == L"pair_after_swap" || name == L"pair_tuple_after_rollback" ||
        name == L"pair_rollback_cas")
        probe.failAfterSwap = true;
    if (name == L"pair_readback_fail")
        probe.failProviderReadAfterCas = probe.exchanges + 1;
    if (name == L"pair_final_restore" || name == L"pair_rollback_close")
        probe.failProviderClose = probe.providerCloses + 1;
    if (name == L"pair_rollback_cas") probe.failRollbackCas = true;
    if (name == L"pair_foreign_mid_cas") probe.foreignBeforeCas = true;
    auto report = group.InstallDeviceTablePair(
        static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&deviceObject)),
        Address(reinterpret_cast<FARPROC>(&HookReset)),
        Address(reinterpret_cast<FARPROC>(&HookPresent)), probe.backend);
    const bool terminal = name == L"pair_rollback_close" ||
                          name == L"pair_rollback_cas" ||
                          name == L"pair_foreign_mid_cas";
    if (report.state != (terminal ? D3dInstallState::RollbackFailed :
                                    D3dInstallState::RolledBack))
        std::printf("PAIR FAULT state=%d reason=%s opens=%d closes=%d CAS=%d\n",
                    static_cast<int>(report.state), report.reason.c_str(),
                    probe.providerOpens, probe.providerCloses, probe.exchanges);
    Check(report.state == (terminal ? D3dInstallState::RollbackFailed :
                                      D3dInstallState::RolledBack) &&
          probe.pairPublications == 1 &&
          probe.publishedReset == resetOriginal &&
          probe.publishedPresent == presentOriginal,
          "pair failure state and originals remain truthful");
    if (name == L"pair_rollback_cas")
        Check(providerDeviceTable[kD3dDeviceResetSlot] ==
                  Address(reinterpret_cast<FARPROC>(&HookReset)) &&
              providerDeviceTable[kD3dDevicePresentSlot] ==
                  Address(reinterpret_cast<FARPROC>(&HookPresent)),
              "failed rollback CAS leaves exposed hooks with originals published");
    else
        Check(providerDeviceTable[kD3dDeviceResetSlot] == resetOriginal &&
              providerDeviceTable[kD3dDevicePresentSlot] ==
                  (name == L"pair_foreign_mid_cas" ? 0x12345678u : presentOriginal),
              "pair recovery restores owned slots without clobbering foreign value");
    if (name == L"pair_begin_second")
        Check(probe.exchanges == casBefore && report.attemptedSlots == 0,
              "both protections acquired before first pair CAS");
    if (name == L"pair_after_swap")
    {
        const auto resetAddress = static_cast<std::uint32_t>(
            reinterpret_cast<std::uintptr_t>(providerDeviceTable + kD3dDeviceResetSlot));
        const auto presentAddress = static_cast<std::uint32_t>(
            reinterpret_cast<std::uintptr_t>(providerDeviceTable + kD3dDevicePresentSlot));
        Check(probe.casAddresses.size() >= casIndex + 4 &&
              probe.casAddresses[casIndex] == resetAddress &&
              probe.casAddresses[casIndex + 1] == presentAddress &&
              probe.casAddresses[casIndex + 2] == presentAddress &&
              probe.casAddresses[casIndex + 3] == resetAddress,
              "post-exchange pair recovery CAS runs in reverse slot order");
    }
    if (name == L"pair_tuple_after_rollback")
    {
        const int cas = probe.exchanges, opens = probe.providerOpens;
        auto retry = group.InstallDeviceTablePair(
            static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&deviceObject)),
            Address(reinterpret_cast<FARPROC>(&HookResetAlternate)),
            Address(reinterpret_cast<FARPROC>(&HookPresent)), probe.backend);
        Check(retry.state == D3dInstallState::Poisoned &&
              probe.exchanges == cas && probe.providerOpens == opens &&
              providerDeviceTable[kD3dDeviceResetSlot] == resetOriginal &&
              providerDeviceTable[kD3dDevicePresentSlot] == presentOriginal,
              "changed paired hook tuple after full-CAS clean recovery is terminal");
    }
    if (terminal)
    {
        auto retry = group.InstallDeviceTablePair(
            static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&deviceObject)),
            Address(reinterpret_cast<FARPROC>(&HookReset)),
            Address(reinterpret_cast<FARPROC>(&HookPresent)), probe.backend);
        Check(retry.state == D3dInstallState::Poisoned,
              "incomplete pair recovery is terminal");
    }
}

static void TestBusy(bool objectStage)
{
    D3dInstallTransaction group;
    Probe probe;
    OwnedObject object{static_cast<std::uint32_t>(
        reinterpret_cast<std::uintptr_t>(providerD3dTable))};
    if (objectStage && !ApplyImport(group, probe)) return;
    std::promise<void> entered;
    auto enteredFuture = entered.get_future();
    std::promise<void> release;
    auto releaseFuture = release.get_future().share();
    if (objectStage)
    {
        const auto publish = probe.backend.publishCreateDevice;
        probe.backend.publishCreateDevice = [publish, &entered, releaseFuture](
            std::uint32_t value) {
            publish(value);
            entered.set_value();
            releaseFuture.wait();
        };
    }
    else
    {
        const auto publish = probe.backend.publishCreate9;
        probe.backend.publishCreate9 = [publish, &entered, releaseFuture](
            std::uint32_t value) {
            publish(value);
            entered.set_value();
            releaseFuture.wait();
        };
    }
    auto install = [&] {
        return objectStage ? group.InstallCreateDeviceTable(
            static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&object)),
            Address(reinterpret_cast<FARPROC>(&HookCreateDevice)), probe.backend) :
            group.InstallExeImport(providerCreate9,
                Address(reinterpret_cast<FARPROC>(&HookCreate9)),
                probe.backend, expectation);
    };
    auto worker = std::async(std::launch::async, install);
    const bool reached = enteredFuture.wait_for(std::chrono::seconds(2)) ==
                         std::future_status::ready;
    Check(reached, "first D3D transaction reaches held publication callback");
    if (reached)
    {
        const auto begin = std::chrono::steady_clock::now();
        const auto second = install();
        const auto elapsed = std::chrono::steady_clock::now() - begin;
        Check(second.state == D3dInstallState::Busy &&
              elapsed < std::chrono::milliseconds(200),
              "concurrent D3D retry reports Busy without waiting");
    }
    release.set_value();
    Check(worker.get().state == D3dInstallState::Applied,
          "held D3D transaction completes after Busy retry");
}

namespace source_hooks {
struct Settings { bool patchD3dFpuPreserve = true; bool patchD3dNoVsync = false; };
static Settings g_settings;
static const DWORD D3DCREATE_FPU_PRESERVE_FLAG = 0x00000002;
static const DWORD D3DPRESENT_INTERVAL_IMMEDIATE = 0x80000000;
static const int D3DPRESENT_INTERVAL_OFF = 52;
using tDirect3DCreate9 = void* (WINAPI*)(UINT);
using tD3D9CreateDevice = HRESULT (WINAPI*)(void*, UINT, UINT, HWND, DWORD, void*, void**);
static tDirect3DCreate9 g_realDirect3DCreate9 = nullptr;
static tD3D9CreateDevice g_realCreateDevice = nullptr;
static int objectInstallCalls = 0, deviceInstallCalls = 0;
static std::function<bool(void*)> deviceInstaller;
static void Log(const char*, ...) {}
static bool InstallD3dCreateDeviceTable(void*) { ++objectInstallCalls; return false; }
static bool InstallD3dDeviceTablePair(void* device) {
    ++deviceInstallCalls;
    return deviceInstaller ? deviceInstaller(device) : false;
}
// ACTUAL_D3D_CALLBACKS
} // namespace source_hooks

static UINT observedSdk = 0;
static DWORD observedFlags = 0;
static void* providerReturnedObject = nullptr;
static void* providerReturnedDevice = nullptr;
static HRESULT providerDeviceResult = S_FALSE;

static void* WINAPI ObjectProvider(UINT sdk)
{
    observedSdk = sdk;
    return providerReturnedObject;
}

static HRESULT WINAPI DeviceProvider(void*, UINT, UINT, HWND, DWORD flags,
                                     void*, void** outDevice)
{
    observedFlags = flags;
    if (outDevice) *outDevice = providerReturnedDevice;
    return providerDeviceResult;
}

int wmain(int argc, wchar_t** argv)
{
    if (argc != 3) return 2;
    const std::wstring caseName = argv[2];
    providerPath = argv[1];
    providerPath += L"\\d3d_surrogate.dll";
    void* preferredBlock = nullptr;
    if (caseName == L"aslr_success")
    {
        preferredBlock = VirtualAlloc(reinterpret_cast<void*>(0x68000000u),
            0x400000, MEM_RESERVE, PAGE_NOACCESS);
        Check(preferredBlock == reinterpret_cast<void*>(0x68000000u),
              "owned reservation blocks surrogate preferred image base");
        if (!preferredBlock) return 2;
    }
    HMODULE surrogate = LoadLibraryW(providerPath.c_str());
    Check(surrogate != nullptr, "private D3D provider surrogate loads");
    if (!PrepareProvider(surrogate)) return 2;
    if (preferredBlock)
    {
        Check(providerBase != 0x68000000u &&
              expectation.providerPreferredBase == 0x68000000u,
              "surrogate really loads at relocated image base");
        Check(VirtualFree(preferredBlock, 0, MEM_RELEASE) != FALSE,
              "owned preferred-base reservation released after load");
    }
    exeImage = static_cast<unsigned char*>(VirtualAlloc(nullptr, exeImageSize,
        MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
    Check(exeImage != nullptr, "owned EXE PE32 image allocated");
    if (!exeImage) return 2;
    exeBase = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(exeImage));
    ResetExeImage();
    providerD3dTable = reinterpret_cast<std::uint32_t*>(
        GetProcAddress(surrogate, "FixtureD3dTable"));
    providerD3dTableSame = reinterpret_cast<std::uint32_t*>(
        GetProcAddress(surrogate, "FixtureD3dTableSame"));
    providerDeviceTable = reinterpret_cast<std::uint32_t*>(
        GetProcAddress(surrogate, "FixtureDeviceTable"));
    providerDeviceTableSame = reinterpret_cast<std::uint32_t*>(
        GetProcAddress(surrogate, "FixtureDeviceTableSame"));
    providerDeviceTableDifferent = reinterpret_cast<std::uint32_t*>(
        GetProcAddress(surrogate, "FixtureDeviceTableDifferent"));
    providerWritableTable = reinterpret_cast<std::uint32_t*>(
        GetProcAddress(surrogate, "FixtureWritableDeviceTable"));
    Check(providerD3dTable && providerD3dTableSame && providerDeviceTable &&
          providerDeviceTableSame &&
          providerDeviceTableDifferent && providerWritableTable,
          "all private provider vtable exports available");
    if (!providerD3dTable || !providerD3dTableSame || !providerDeviceTable ||
        !providerDeviceTableSame ||
        !providerDeviceTableDifferent || !providerWritableTable) return 2;
    Check(kPinnedD3dCreate9IatRva == 0x88A39C &&
          kD3dCreateDeviceSlot == 16 && kD3dDeviceResetSlot == 16 &&
          kD3dDevicePresentSlot == 17, "pinned import and ordinary COM slots");
    Check(ExeSlot() == providerCreate9, "owned EXE import starts at surrogate provider");
    ForceExeSlot(providerCreate9);
    Check(ExeSlot() == providerCreate9, "owned EXE slot supports bounded atomic reset");
    if (caseName == L"success" || caseName == L"aslr_success")
    {
        D3dInstallTransaction group;
        Probe probe;
        const auto hook = Address(reinterpret_cast<FARPROC>(&HookCreate9));
        auto report = group.InstallExeImport(providerCreate9, hook, probe.backend, expectation);
        if (report.state != D3dInstallState::Applied)
            std::printf("EXE BASELINE state=%d reason=%s retains=%d resolves=%d opens=%d CAS=%d\n",
                        static_cast<int>(report.state), report.reason.c_str(),
                        probe.retains, probe.resolutions, probe.exeOpens, probe.exchanges);
        Check(report.state == D3dInstallState::Applied,
              "authenticated EXE Direct3DCreate9 import applies");
        Check(ExeSlot() == hook && probe.publishedCreate9 == providerCreate9 &&
              probe.create9Publications == 1,
              "EXE IAT hook exact with original published");
        const int opensBeforeRetry = probe.exeOpens, casBeforeRetry = probe.exchanges;
        auto importRetry = group.InstallExeImport(ExeSlot(), hook, probe.backend, expectation);
        Check(importRetry.state == D3dInstallState::AlreadyInstalled &&
              probe.exeOpens == opensBeforeRetry && probe.exchanges == casBeforeRetry &&
              probe.create9Publications == 1,
              "live hooked IAT retry is idempotent without protection or CAS");
        auto* d3dTable = reinterpret_cast<std::uint32_t*>(
            GetProcAddress(surrogate, "FixtureD3dTable"));
        auto* deviceTable = reinterpret_cast<std::uint32_t*>(
            GetProcAddress(surrogate, "FixtureDeviceTable"));
        Check(d3dTable && deviceTable, "RX provider vtable exports available");
        if (d3dTable && deviceTable)
        {
            const auto createOriginal = d3dTable[kD3dCreateDeviceSlot];
            const auto resetOriginal = deviceTable[kD3dDeviceResetSlot];
            const auto presentOriginal = deviceTable[kD3dDevicePresentSlot];
            OwnedObject d3dObject{static_cast<std::uint32_t>(
                reinterpret_cast<std::uintptr_t>(d3dTable))};
            OwnedObject deviceObject{static_cast<std::uint32_t>(
                reinterpret_cast<std::uintptr_t>(deviceTable))};
            Check(probe.backend.immutableProviderStorage(providerBase, d3dObject.vtable,
                  17 * sizeof(std::uint32_t)) &&
                  probe.backend.immutableProviderStorage(providerBase, deviceObject.vtable,
                  18 * sizeof(std::uint32_t)),
                  "owned MEM_IMAGE RX provider vtables are immutable storage");
            auto create = group.InstallCreateDeviceTable(
                static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&d3dObject)),
                Address(reinterpret_cast<FARPROC>(&HookCreateDevice)), probe.backend);
            if (create.state != D3dInstallState::Applied)
                std::printf("CREATE BASELINE state=%d reason=%s opens=%d CAS=%d\n",
                            static_cast<int>(create.state), create.reason.c_str(),
                            probe.providerOpens, probe.exchanges);
            Check(create.state == D3dInstallState::Applied &&
                  d3dTable[kD3dCreateDeviceSlot] ==
                      Address(reinterpret_cast<FARPROC>(&HookCreateDevice)) &&
                  probe.publishedCreateDevice == createOriginal,
                  "returned D3D object CreateDevice table applies with original");
            auto pair = group.InstallDeviceTablePair(
                static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&deviceObject)),
                Address(reinterpret_cast<FARPROC>(&HookReset)),
                Address(reinterpret_cast<FARPROC>(&HookPresent)), probe.backend);
            if (pair.state != D3dInstallState::Applied)
                std::printf("PAIR BASELINE state=%d reason=%s opens=%d CAS=%d\n",
                            static_cast<int>(pair.state), pair.reason.c_str(),
                            probe.providerOpens, probe.exchanges);
            Check(pair.state == D3dInstallState::Applied &&
                  deviceTable[kD3dDeviceResetSlot] ==
                      Address(reinterpret_cast<FARPROC>(&HookReset)) &&
                  deviceTable[kD3dDevicePresentSlot] ==
                      Address(reinterpret_cast<FARPROC>(&HookPresent)) &&
                  probe.publishedReset == resetOriginal &&
                  probe.publishedPresent == presentOriginal,
                  "paired Reset and Present table applies with both originals");
            Check(probe.retains == 1,
                  "all three stages reuse one retained authenticated provider reference");
        }
    }
    else if (caseName == L"exe_recovery_same" ||
             caseName == L"exe_recovery_different")
    {
        D3dInstallTransaction group;
        Probe probe;
        const auto hook = Address(reinterpret_cast<FARPROC>(&HookCreate9));
        const auto otherHook = Address(reinterpret_cast<FARPROC>(&HookCreate9Alternate));
        Check(hook != otherHook, "replacement Create9 hook has distinct identity");
        probe.failCas = 1;
        probe.failAfterSwap = true;
        probe.failExeReadAfterCas = 1;
        auto first = group.InstallExeImport(providerCreate9, hook, probe.backend,
                                            expectation);
        Check(first.state == D3dInstallState::RolledBack &&
              first.hookPossiblyExposed && ExeSlot() == providerCreate9 &&
              probe.create9Publications == 1 && probe.publishedCreate9 == providerCreate9,
              "full exchanged IAT CAS plus failed readback recovers original while retaining publication");
        const int opens = probe.exeOpens, cas = probe.exchanges;
        const bool different = caseName == L"exe_recovery_different";
        auto retry = group.InstallExeImport(providerCreate9,
            different ? otherHook : hook, probe.backend, expectation);
        Check(retry.state == (different ? D3dInstallState::Poisoned :
                                          D3dInstallState::Applied) &&
              probe.create9Publications == 1 &&
              (different ? probe.exeOpens == opens && probe.exchanges == cas &&
                           ExeSlot() == providerCreate9 : ExeSlot() == hook),
              "historically exposed hook identity blocks different retry but permits same hook");
    }
    else if (caseName == L"exe_unrelated_import")
    {
        DWORD old = 0, ignored = 0;
        Check(VirtualProtect(exeImage, exeImageSize, PAGE_READWRITE, &old) != FALSE,
              "owned EXE import directory writable for unrelated descriptor");
        auto* nt = reinterpret_cast<IMAGE_NT_HEADERS32*>(exeImage + 0x80);
        auto* descriptor = reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(exeImage + 0x2000);
        descriptor[1] = descriptor[0];
        descriptor[0] = IMAGE_IMPORT_DESCRIPTOR{};
        descriptor[0].Name = 0x2400;
        descriptor[0].FirstThunk = 0x2500;
        memcpy(exeImage + 0x2400, "kernel32.dll", sizeof("kernel32.dll"));
        nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].Size =
            3 * sizeof(IMAGE_IMPORT_DESCRIPTOR);
        Check(VirtualProtect(exeImage, exeImageSize, PAGE_EXECUTE_READ, &ignored) != FALSE,
              "owned EXE import directory restored RX");
        D3dInstallTransaction group;
        Probe probe;
        auto report = group.InstallExeImport(providerCreate9,
            Address(reinterpret_cast<FARPROC>(&HookCreate9)), probe.backend, expectation);
        Check(report.state == D3dInstallState::Applied &&
              ExeSlot() == Address(reinterpret_cast<FARPROC>(&HookCreate9)),
              "unrelated import without OFT does not reject exact d3d9 named IAT");
    }
    else if (caseName == L"exe_foreign")
    {
        ForceExeSlot(0x12345678);
        D3dInstallTransaction group;
        Probe probe;
        auto report = group.InstallExeImport(ExeSlot(),
            Address(reinterpret_cast<FARPROC>(&HookCreate9)), probe.backend, expectation);
        Check(report.state == D3dInstallState::Rejected &&
              probe.resolutions == 0 && probe.create9Publications == 0 &&
              probe.exeOpens == 0 && probe.exchanges == 0 && ExeSlot() == 0x12345678,
              "foreign EXE import rejects before export resolution and mutation");
    }
    else if (caseName == L"exe_bad_import")
    {
        ForceExeByte(0x2302, 'X');
        D3dInstallTransaction group;
        Probe probe;
        auto report = group.InstallExeImport(ExeSlot(),
            Address(reinterpret_cast<FARPROC>(&HookCreate9)), probe.backend, expectation);
        Check(report.state == D3dInstallState::Poisoned &&
              probe.create9Publications == 0 && probe.exeOpens == 0 &&
              probe.exchanges == 0 && ExeSlot() == providerCreate9,
              "named import mismatch rejects before publication or writes");
    }
    else if (caseName == L"exe_retry_foreign")
    {
        D3dInstallTransaction group;
        Probe probe;
        if (ApplyImport(group, probe))
        {
            const int cas = probe.exchanges, opens = probe.exeOpens;
            ForceExeSlot(0x12345678);
            auto report = group.InstallExeImport(ExeSlot(),
                Address(reinterpret_cast<FARPROC>(&HookCreate9)), probe.backend, expectation);
            Check(report.state == D3dInstallState::Poisoned &&
                  probe.exchanges == cas && probe.exeOpens == opens &&
                  ExeSlot() == 0x12345678,
                  "foreign clobber of installed EXE import poisons without overwrite");
        }
    }
    else if (caseName == L"create_heap" || caseName == L"create_data")
    {
        D3dInstallTransaction group;
        Probe probe;
        if (ApplyImport(group, probe))
        {
            std::vector<std::uint32_t> heap(providerD3dTable,
                                            providerD3dTable + 17);
            auto* table = caseName == L"create_heap" ? heap.data() :
                          providerWritableTable;
            OwnedObject object{static_cast<std::uint32_t>(
                reinterpret_cast<std::uintptr_t>(table))};
            const int cas = probe.exchanges, opens = probe.providerOpens;
            auto report = group.InstallCreateDeviceTable(
                static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&object)),
                Address(reinterpret_cast<FARPROC>(&HookCreateDevice)), probe.backend);
            Check(report.state == D3dInstallState::Rejected &&
                  probe.createDevicePublications == 0 && probe.exchanges == cas &&
                  probe.providerOpens == opens,
                  "mutable heap or provider data vtable rejects without publication/write");
        }
    }
    else if (caseName == L"create_tuple_after_rollback")
    {
        D3dInstallTransaction group;
        Probe probe;
        OwnedObject object{static_cast<std::uint32_t>(
            reinterpret_cast<std::uintptr_t>(providerD3dTable))};
        if (ApplyImport(group, probe))
        {
            const auto original = providerD3dTable[kD3dCreateDeviceSlot];
            probe.failCas = probe.exchanges + 1;
            probe.failAfterSwap = true;
            auto failed = group.InstallCreateDeviceTable(
                static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&object)),
                Address(reinterpret_cast<FARPROC>(&HookCreateDevice)), probe.backend);
            Check(failed.state == D3dInstallState::RolledBack &&
                  failed.hookPossiblyExposed &&
                  providerD3dTable[kD3dCreateDeviceSlot] == original,
                  "CreateDevice full-CAS failure cleanly restores original");
            const int cas = probe.exchanges, opens = probe.providerOpens;
            auto retry = group.InstallCreateDeviceTable(
                static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&object)),
                Address(reinterpret_cast<FARPROC>(&HookCreateDeviceAlternate)),
                probe.backend);
            Check(retry.state == D3dInstallState::Poisoned &&
                  probe.exchanges == cas && probe.providerOpens == opens &&
                  providerD3dTable[kD3dCreateDeviceSlot] == original,
                  "changed CreateDevice hook after exposed clean recovery is terminal");
        }
    }
    else if (caseName == L"pair_second_fail" || caseName == L"pair_after_swap" ||
             caseName == L"pair_tuple_after_rollback" ||
             caseName == L"pair_readback_fail" || caseName == L"pair_final_restore" ||
             caseName == L"pair_rollback_close" || caseName == L"pair_begin_second" ||
             caseName == L"pair_rollback_cas" || caseName == L"pair_foreign_mid_cas")
        TestPairFault(caseName);
    else if (caseName == L"tuple_second")
    {
        D3dInstallTransaction group;
        Probe probe;
        OwnedObject d3dObject{static_cast<std::uint32_t>(
            reinterpret_cast<std::uintptr_t>(providerD3dTable))};
        OwnedObject firstDevice{static_cast<std::uint32_t>(
            reinterpret_cast<std::uintptr_t>(providerDeviceTable))};
        OwnedObject sameDevice{static_cast<std::uint32_t>(
            reinterpret_cast<std::uintptr_t>(providerDeviceTableSame))};
        OwnedObject differentDevice{static_cast<std::uint32_t>(
            reinterpret_cast<std::uintptr_t>(providerDeviceTableDifferent))};
        const auto resetHook = Address(reinterpret_cast<FARPROC>(&HookReset));
        const auto presentHook = Address(reinterpret_cast<FARPROC>(&HookPresent));
        if (ApplyImport(group, probe) && ApplyCreate(group, probe, d3dObject))
        {
            auto first = group.InstallDeviceTablePair(
                static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&firstDevice)),
                resetHook, presentHook, probe.backend);
            Check(first.state == D3dInstallState::Applied,
                  "first device pair applies before second-table tests");
            const int publication = probe.pairPublications;
            auto same = group.InstallDeviceTablePair(
                static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&sameDevice)),
                resetHook, presentHook, probe.backend);
            Check(same.state == D3dInstallState::Applied &&
                  probe.pairPublications == publication &&
                  providerDeviceTableSame[kD3dDeviceResetSlot] == resetHook &&
                  providerDeviceTableSame[kD3dDevicePresentSlot] == presentHook,
                  "second identical method tuple uses published originals");
            const int cas = probe.exchanges, opens = probe.providerOpens;
            const auto differentOriginal = providerDeviceTableDifferent[kD3dDevicePresentSlot];
            auto different = group.InstallDeviceTablePair(
                static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&differentDevice)),
                resetHook, presentHook, probe.backend);
            if (different.state != D3dInstallState::Poisoned)
                std::printf("DIFFERENT TUPLE state=%d reason=%s CAS=%d/%d opens=%d/%d\n",
                            static_cast<int>(different.state), different.reason.c_str(),
                            probe.exchanges, cas, probe.providerOpens, opens);
            Check(different.state == D3dInstallState::Poisoned &&
                  probe.exchanges == cas && probe.providerOpens == opens &&
                  providerDeviceTableDifferent[kD3dDevicePresentSlot] == differentOriginal,
                  "different second tuple rejects without altering published originals");
        }
    }
    else if (caseName == L"owned_clobber")
    {
        D3dInstallTransaction group;
        Probe probe;
        OwnedObject first{static_cast<std::uint32_t>(
            reinterpret_cast<std::uintptr_t>(providerD3dTable))};
        OwnedObject second{static_cast<std::uint32_t>(
            reinterpret_cast<std::uintptr_t>(providerD3dTableSame))};
        if (ApplyImport(group, probe) && ApplyCreate(group, probe, first))
        {
            ForceProviderSlot(providerD3dTable + kD3dCreateDeviceSlot, 0x12345678);
            const int cas = probe.exchanges, opens = probe.providerOpens;
            auto report = group.InstallCreateDeviceTable(
                static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&second)),
                Address(reinterpret_cast<FARPROC>(&HookCreateDevice)), probe.backend);
            Check(report.state == D3dInstallState::Poisoned &&
                  probe.exchanges == cas && probe.providerOpens == opens &&
                  providerD3dTableSame[kD3dCreateDeviceSlot] !=
                      Address(reinterpret_cast<FARPROC>(&HookCreateDevice)),
                  "foreign clobber at older owned table stops new-table write");
        }
    }
    else if (caseName == L"busy_import" || caseName == L"busy_object")
        TestBusy(caseName == L"busy_object");
    else if (caseName == L"callback_preserve")
    {
        OwnedObject returnedD3d{static_cast<std::uint32_t>(
            reinterpret_cast<std::uintptr_t>(providerD3dTable))};
        OwnedObject returnedDevice{static_cast<std::uint32_t>(
            reinterpret_cast<std::uintptr_t>(providerDeviceTable))};
        providerReturnedObject = &returnedD3d;
        providerReturnedDevice = &returnedDevice;
        source_hooks::g_realDirect3DCreate9 = &ObjectProvider;
        source_hooks::g_realCreateDevice = &DeviceProvider;
        D3dInstallTransaction group;
        Probe probe;
        Check(ApplyImport(group, probe) && ApplyCreate(group, probe, returnedD3d),
              "callback preservation starts after active D3D import/table install");
        source_hooks::deviceInstaller = [&group, &probe](void* device) {
            probe.failCas = probe.exchanges + 2;
            const auto report = group.InstallDeviceTablePair(
                static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(device)),
                Address(reinterpret_cast<FARPROC>(&HookReset)),
                Address(reinterpret_cast<FARPROC>(&HookPresent)), probe.backend);
            Check(report.state == D3dInstallState::RolledBack,
                  "actual paired table installer recovers before callback return");
            return report.state == D3dInstallState::Applied;
        };
        auto* object = source_hooks::HookDirect3DCreate9(32);
        Check(object == &returnedD3d && observedSdk == 32 &&
              source_hooks::objectInstallCalls == 1,
              "actual source preserves native Direct3DCreate9 object on clean installer failure");
        D3DPRESENT_PARAMETERS parameters{};
        parameters.PresentationInterval = 0x12345678;
        void* device = nullptr;
        providerDeviceResult = S_FALSE;
        const auto hr = source_hooks::HookCreateDevice(&returnedD3d, 1, 2,
            nullptr, 0x10, &parameters, &device);
        Check(hr == S_FALSE && device == &returnedDevice &&
              source_hooks::deviceInstallCalls == 1 &&
              (observedFlags & D3DCREATE_FPU_PRESERVE) != 0 &&
              parameters.PresentationInterval == 0x12345678,
              "actual source preserves native HRESULT/device/flags on clean table failure");
        providerDeviceResult = E_FAIL;
        device = nullptr;
        const auto failed = source_hooks::HookCreateDevice(&returnedD3d, 1, 2,
            nullptr, 0x10, &parameters, &device);
        Check(failed == E_FAIL && source_hooks::deviceInstallCalls == 1,
              "native device failure returns its HRESULT without installer attempt");
    }
    else
        Check(false, "recognized D3D fixture case");
    VirtualFree(exeImage, 0, MEM_RELEASE);
    if (surrogate) FreeLibrary(surrogate);
    std::printf("RESULT checks=%d failures=%d\n", checks.load(), failures.load());
    return failures.load() ? 1 : 0;
}
