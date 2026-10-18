#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <bcrypt.h>
#include "TbbImportTransaction.h"
#include "Win32Memory.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <future>
#include <string>
#include <vector>

using namespace fork_boundary;
static std::atomic<int> checks{0};
static std::atomic<int> failures{0};
static unsigned char* image = nullptr;
static std::uint32_t base = 0;
static constexpr std::uint32_t imageSize = kPinnedTbbImageSize;
static std::uint32_t provider = 0;
static std::uint32_t providerBase = 0;
static std::wstring syntheticPath;
static std::wstring crtPath;
static TbbImportExpectation expectation;
static const std::uint16_t noTextRelocations = 0;

static void Check(bool yes, const char* label)
{
    checks.fetch_add(1);
    if (!yes) { failures.fetch_add(1); printf("FAIL %s\n", label); }
}

static std::array<unsigned char, 32> Sha256(const unsigned char* data, std::size_t count)
{
    std::array<unsigned char, 32> result{};
    BCRYPT_ALG_HANDLE algorithm = nullptr;
    const NTSTATUS opened = BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM,
                                                        nullptr, 0);
    Check(opened >= 0, "bcrypt sha256 provider opened");
    if (opened < 0) return result;
    const NTSTATUS hashed = BCryptHash(algorithm, nullptr, 0,
        const_cast<PUCHAR>(data), static_cast<ULONG>(count), result.data(),
        static_cast<ULONG>(result.size()));
    Check(hashed >= 0, "bcrypt sha256 content hashed");
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

static bool WriteSyntheticDisk()
{
    HANDLE handle = CreateFileW(syntheticPath.c_str(), GENERIC_WRITE, 0, nullptr,
                                CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (handle == INVALID_HANDLE_VALUE) return false;
    DWORD written = 0;
    const bool ok = WriteFile(handle, image, imageSize, &written, nullptr) && written == imageSize;
    CloseHandle(handle);
    return ok;
}

static std::uint32_t ReadSlot()
{
    return *reinterpret_cast<volatile std::uint32_t*>(image + kPinnedTbbBeginThreadExIatRva);
}

static void ForceSlot(std::uint32_t value)
{
    auto* address = image + kPinnedTbbBeginThreadExIatRva;
    DWORD old = 0, ignored = 0;
    Check(VirtualProtect(address, 4, PAGE_READWRITE, &old) != FALSE,
          "owned IAT slot made writable");
    InterlockedExchange(reinterpret_cast<volatile LONG*>(address), static_cast<LONG>(value));
    Check(VirtualProtect(address, 4, old, &ignored) != FALSE,
          "owned IAT slot protection restored");
}

static bool OriginalProtection()
{
    MEMORY_BASIC_INFORMATION region{};
    return VirtualQuery(image + kPinnedTbbBeginThreadExIatRva, &region, sizeof(region)) ==
           sizeof(region) && region.Protect == PAGE_EXECUTE_READ;
}

static void ForceByte(std::uint32_t rva, unsigned char value)
{
    DWORD old = 0, ignored = 0;
    Check(rva < imageSize && VirtualProtect(image + rva, 1, PAGE_READWRITE, &old) != FALSE,
          "owned image byte temporarily writable");
    image[rva] = value;
    Check(VirtualProtect(image + rva, 1, old, &ignored) != FALSE,
          "owned image byte protection restored");
}

static void ResetImage()
{
    DWORD ignored = 0;
    Check(VirtualProtect(image, imageSize, PAGE_READWRITE, &ignored) != FALSE,
          "synthetic image made writable for reset");
    memset(image, 0, imageSize);
    auto* dos = reinterpret_cast<IMAGE_DOS_HEADER*>(image);
    dos->e_magic = IMAGE_DOS_SIGNATURE;
    dos->e_lfanew = 0x80;
    auto* nt = reinterpret_cast<IMAGE_NT_HEADERS32*>(image + 0x80);
    nt->Signature = IMAGE_NT_SIGNATURE;
    nt->FileHeader.Machine = IMAGE_FILE_MACHINE_I386;
    nt->FileHeader.NumberOfSections = 2;
    nt->FileHeader.SizeOfOptionalHeader = sizeof(IMAGE_OPTIONAL_HEADER32);
    nt->FileHeader.TimeDateStamp = 0x12345678;
    nt->OptionalHeader.Magic = IMAGE_NT_OPTIONAL_HDR32_MAGIC;
    nt->OptionalHeader.ImageBase = 0x10000000;
    nt->OptionalHeader.SizeOfImage = imageSize;
    nt->OptionalHeader.NumberOfRvaAndSizes = IMAGE_NUMBEROF_DIRECTORY_ENTRIES;
    nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress = 0x4000;
    nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].Size =
        2 * sizeof(IMAGE_IMPORT_DESCRIPTOR);
    IMAGE_SECTION_HEADER* section = IMAGE_FIRST_SECTION(nt);
    memcpy(section[0].Name, ".text", sizeof(".text") - 1);
    section[0].VirtualAddress = 0x1000;
    section[0].Misc.VirtualSize = 0x1000;
    section[0].SizeOfRawData = 0x1000;
    section[0].PointerToRawData = 0x1000;
    memcpy(section[1].Name, ".idata", sizeof(".idata") - 1);
    section[1].VirtualAddress = 0x4000;
    section[1].Misc.VirtualSize = 0x20000;
    section[1].SizeOfRawData = 0x20000;
    section[1].PointerToRawData = 0x4000;
    for (std::uint32_t i = 0; i < 0x1000; ++i)
        image[0x1000 + i] = static_cast<unsigned char>((i * 37 + 11) & 0xff);
    auto* descriptor = reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(image + 0x4000);
    descriptor[0].Name = 0x4100;
    descriptor[0].OriginalFirstThunk = 0x4200;
    descriptor[0].FirstThunk = kPinnedTbbBeginThreadExIatRva;
    memcpy(image + 0x4100, "MSVCR100.dll", sizeof("MSVCR100.dll"));
    auto* lookup = reinterpret_cast<IMAGE_THUNK_DATA32*>(image + 0x4200);
    lookup[0].u1.AddressOfData = 0x4300;
    *reinterpret_cast<WORD*>(image + 0x4300) = 0;
    memcpy(image + 0x4302, "_beginthreadex", sizeof("_beginthreadex"));
    *reinterpret_cast<std::uint32_t*>(image + kPinnedTbbBeginThreadExIatRva) = provider;
    Check(VirtualProtect(image, imageSize, PAGE_EXECUTE_READ, &ignored) != FALSE,
          "synthetic image execute-read protection");
}

static std::uint32_t __cdecl HookBeginThreadEx(void*, unsigned,
    LPTHREAD_START_ROUTINE, void*, unsigned, unsigned*) { return 7; }

static void PrepareProvider()
{
    wchar_t wow[MAX_PATH]{};
    UINT length = GetSystemWow64DirectoryW(wow, MAX_PATH);
    Check(length > 0 && length < MAX_PATH - 15, "physical x86 SysWOW64 path available");
    if (!length || length >= MAX_PATH - 15) return;
    crtPath = wow;
    crtPath += L"\\msvcr100.dll";
    HMODULE crt = LoadLibraryW(crtPath.c_str());
    Check(crt != nullptr, "pinned x86 MSVCR100 loaded for owned test");
    if (!crt) return;
    providerBase = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(crt));
    provider = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(
        GetProcAddress(crt, "_beginthreadex")));
    Check(provider != 0, "real legacy CRT _beginthreadex resolved");
    auto* nt = reinterpret_cast<IMAGE_NT_HEADERS32*>(
        reinterpret_cast<unsigned char*>(crt) + reinterpret_cast<IMAGE_DOS_HEADER*>(crt)->e_lfanew);
    expectation.crtImageSize = nt->OptionalHeader.SizeOfImage;
    expectation.crtTimeDateStamp = nt->FileHeader.TimeDateStamp;
    expectation.crtBeginThreadExExportRva = provider - providerBase;
    expectation.crtBeginThreadExOrdinal = 469;
    std::vector<unsigned char> disk;
    Check(FileBytes(crtPath, disk), "physical x86 CRT disk file read");
    if (!disk.empty())
    {
        expectation.crtDiskSize = disk.size();
        expectation.crtDiskSha256 = Sha256(disk.data(), disk.size());
    }
    Check(expectation.crtBeginThreadExExportRva == 0x5C601,
          "resolved real CRT export matches pinned x86 RVA");
    Check(GetProcAddress(crt, MAKEINTRESOURCEA(469)) ==
          reinterpret_cast<FARPROC>(static_cast<std::uintptr_t>(provider)),
          "real CRT ordinal 469 resolves exact beginthreadex target");
}

static void PrepareExpectation()
{
    expectation.imageSize = imageSize;
    expectation.timeDateStamp = 0x12345678;
    expectation.preferredBase = 0x10000000;
    expectation.textRva = 0x1000;
    expectation.textVirtualSize = 0x1000;
    expectation.textRawSize = 0x1000;
    expectation.beginThreadExIatRva = kPinnedTbbBeginThreadExIatRva;
    expectation.textRelocationDeltas = &noTextRelocations;
    expectation.textRelocationCount = 0;
    expectation.normalizedTextSha256 = Sha256(image + 0x1000, 0x1000);
    Check(WriteSyntheticDisk(), "synthetic TBB disk fixture written privately");
    std::vector<unsigned char> disk;
    Check(FileBytes(syntheticPath, disk), "synthetic TBB disk fixture read");
    if (!disk.empty())
    {
        expectation.diskSize = disk.size();
        expectation.diskSha256 = Sha256(disk.data(), disk.size());
    }
}

struct Probe
{
    TbbImportTransaction* transaction = nullptr;
    TbbImportBackend backend;
    int tbbRetains = 0, providerRetains = 0, publications = 0;
    int opens = 0, closes = 0, exchanges = 0, slotReads = 0;
    int modulePaths = 0, metadataReads = 0, providerReads = 0;
    int resolverCalls = 0;
    std::uint32_t lastResolverBase = 0;
    int failOpen = 0, failClose = 0, failCas = 0, failReadback = 0, failRollback = 0;
    bool failAfterSwap = false;
    bool foreignBeforeCas = false;
    bool reenterOnPublish = false;
    TbbIatState reentrantState = TbbIatState::Rejected;
    std::uint32_t published = 0;

    explicit Probe(TbbImportTransaction& group) : transaction(&group)
    {
        backend.memory = MakeWin32ImageMemory(base, imageSize);
        backend.retainFromAddress = [this](std::uint32_t address, std::uint32_t& retained) {
            if (address == base)
            {
                ++tbbRetains;
                retained = base;
                return true;
            }
            if (address == provider)
            {
                ++providerRetains;
                HMODULE module = nullptr;
                bool ok = GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
                    reinterpret_cast<LPCWSTR>(static_cast<std::uintptr_t>(provider)), &module) != FALSE;
                retained = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(module));
                return ok;
            }
            return false;
        };
        backend.modulePath = [this](std::uint32_t retained, wchar_t* path, std::size_t capacity) {
            ++modulePaths;
            const std::wstring& source = retained == base ? syntheticPath : crtPath;
            if (source.size() + 1 > capacity) return false;
            wcscpy_s(path, capacity, source.c_str());
            return retained == base || retained == providerBase;
        };
        backend.isMappedImage = [](std::uint32_t mappedBase, std::uint32_t size) {
            if (mappedBase == base) return size == imageSize;
            MEMORY_BASIC_INFORMATION region{};
            return mappedBase == providerBase && size == expectation.crtImageSize &&
                VirtualQuery(reinterpret_cast<void*>(static_cast<std::uintptr_t>(mappedBase)),
                             &region, sizeof(region)) == sizeof(region) && region.Type == MEM_IMAGE;
        };
        backend.resolveBeginThreadEx = [this](std::uint32_t authenticatedCrtBase) {
            ++resolverCalls;
            lastResolverBase = authenticatedCrtBase;
            return authenticatedCrtBase == providerBase ? provider : 0;
        };
        backend.readProvider = [this](std::uint32_t providerImage, std::uint32_t rva,
                                  unsigned char* out, std::size_t count) {
            ++providerReads;
            if (providerImage != providerBase || !out ||
                rva >= expectation.crtImageSize || count > expectation.crtImageSize - rva)
                return false;
            memcpy(out, reinterpret_cast<const void*>(static_cast<std::uintptr_t>(providerImage) + rva),
                   count);
            return true;
        };
        backend.publishOriginal = [this](std::uint32_t value) {
            published = value;
            ++publications;
            if (reenterOnPublish)
                reentrantState = transaction->Run(base,
                    static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&HookBeginThreadEx)),
                    backend, expectation).state;
        };
        const auto rawOpen = backend.memory.beginWrite;
        backend.memory.beginWrite = [this, rawOpen](std::uint32_t rva, std::size_t count,
                                                    std::uintptr_t& token) {
            ++opens;
            if (opens == failOpen) return false;
            return rawOpen(rva, count, token);
        };
        const auto rawClose = backend.memory.restoreProtection;
        backend.memory.restoreProtection = [this, rawClose](std::uint32_t rva, std::size_t count,
                                                            std::uintptr_t token) {
            ++closes;
            bool ok = rawClose(rva, count, token);
            return ok && closes != failClose;
        };
        const auto rawRead = backend.memory.read;
        backend.memory.read = [this, rawRead](std::uint32_t rva, unsigned char* out,
                                             std::size_t count) {
            if (rva == kPinnedTbbBeginThreadExIatRva && count == 4)
            {
                ++slotReads;
                if (failReadback && exchanges >= failReadback)
                {
                    failReadback = 0;
                    return false;
                }
            }
            else ++metadataReads;
            return rawRead(rva, out, count);
        };
        backend.compareExchange = [this](std::uint32_t rva, std::uint32_t expected,
                                         std::uint32_t desired, std::uint32_t& observed) {
            ++exchanges;
            Check(rva == kPinnedTbbBeginThreadExIatRva, "atomic CAS targets active TBB IAT slot");
            const bool forward = desired == static_cast<std::uint32_t>(
                reinterpret_cast<std::uintptr_t>(&HookBeginThreadEx));
            if (forward)
            {
                Check(publications == 1 && published == provider,
                      "original CRT provider published before hook exposure");
                if (foreignBeforeCas) ForceSlot(0x12345678);
                if (exchanges == failCas && !failAfterSwap)
                    return TbbIatCasResult::Failed;
            }
            else if (failRollback) return TbbIatCasResult::Failed;
            LONG prior = InterlockedCompareExchange(
                reinterpret_cast<volatile LONG*>(image + rva), static_cast<LONG>(desired),
                static_cast<LONG>(expected));
            observed = static_cast<std::uint32_t>(prior);
            TbbIatCasResult result = observed == expected ? TbbIatCasResult::Swapped :
                                                           TbbIatCasResult::Mismatch;
            if (forward && exchanges == failCas && failAfterSwap)
                return TbbIatCasResult::Failed;
            return result;
        };
    }
};

int wmain(int argc, wchar_t** argv)
{
    if (argc != 2) return 2;
    syntheticPath = argv[1];
    syntheticPath += L"\\tbb.dll";
    PrepareProvider();
    image = static_cast<unsigned char*>(VirtualAlloc(nullptr, imageSize,
        MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
    if (!image || !provider) return 2;
    base = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(image));
    ResetImage();
    PrepareExpectation();
    const auto hook = static_cast<std::uint32_t>(
        reinterpret_cast<std::uintptr_t>(&HookBeginThreadEx));
    Check(hook && hook != provider, "owned hook pointer differs from CRT provider");
    Check(kPinnedTbbBeginThreadExIatRva == 0x18144 && imageSize == 0x26000,
          "active pinned TBB slot and image extent");

    {
        std::wstring surrogatePath = argv[1];
        surrogatePath += L"\\surrogate.dll";
        HMODULE surrogate = LoadLibraryW(surrogatePath.c_str());
        Check(surrogate != nullptr, "benign owned surrogate DLL loads");
        FARPROC address = surrogate ? GetProcAddress(surrogate, "RetentionProbe") : nullptr;
        if (!address && surrogate) address = GetProcAddress(surrogate, "_RetentionProbe@0");
        Check(address != nullptr, "benign surrogate export available");
        if (address)
        {
            auto production = MakeWin32TbbImportBackend(base, imageSize,
                [](std::uint32_t) {});
            std::uint32_t retained = 0;
            Check(production.retainFromAddress(static_cast<std::uint32_t>(
                reinterpret_cast<std::uintptr_t>(address)), retained) &&
                retained == static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(surrogate)),
                "production GetModuleHandleEx FROM_ADDRESS retains benign DLL");
            wchar_t path[MAX_PATH]{};
            Check(production.modulePath(retained, path, MAX_PATH) &&
                  _wcsicmp(path, surrogatePath.c_str()) == 0,
                  "production retained module path is exact surrogate path");
            Check(production.resolveBeginThreadEx(providerBase) == provider,
                  "production backend resolves real pinned CRT provider by retained base");
            if (retained) FreeLibrary(reinterpret_cast<HMODULE>(
                static_cast<std::uintptr_t>(retained)));
        }
        if (surrogate) FreeLibrary(surrogate);
    }

    {
        TbbImportTransaction group;
        Probe probe(group);
        auto report = group.Run(0, hook, probe.backend, expectation);
        Check(report.state == TbbIatState::Skipped && probe.tbbRetains == 0 &&
              probe.providerRetains == 0 && probe.opens == 0 && probe.exchanges == 0,
              "not-yet-loaded TBB defers without references or writes");
    }

    {
        ResetImage();
        TbbImportTransaction group;
        Probe probe(group);
        auto report = group.Run(base, hook, probe.backend, expectation);
        if (report.state != TbbIatState::Applied)
            printf("BASELINE state=%d reason=%s retains=%d/%d opens=%d cas=%d\n",
                static_cast<int>(report.state), report.reason.c_str(),
                probe.tbbRetains, probe.providerRetains, probe.opens, probe.exchanges);
        Check(report.state == TbbIatState::Applied, "active CRT TBB import applies");
        Check(report.createThreadImportSkipped, "absent pinned CreateThread import reported Skipped");
        Check(report.originalPublished && report.hookPossiblyExposed && probe.published == provider,
              "original CRT target published before exposure");
        Check(ReadSlot() == hook && OriginalProtection(), "one exact hook pointer and original page protection");
        Check(probe.tbbRetains == 1 && probe.providerRetains == 1,
              "at most one TBB and one provider reference retained");
        const int opens = probe.opens, cas = probe.exchanges, publications = probe.publications;
        const int paths = probe.modulePaths, metadata = probe.metadataReads;
        const int providerReads = probe.providerReads, slotReads = probe.slotReads;
        const int resolverCalls = probe.resolverCalls;
        auto again = group.Run(base, hook, probe.backend, expectation);
        Check(again.state == TbbIatState::AlreadyInstalled && probe.exchanges == cas &&
              probe.opens == opens && probe.publications == publications,
              "already-installed retry does not republish or rewrite");
        Check(probe.tbbRetains == 1 && probe.providerRetains == 1,
              "idempotent retry acquires no third module reference");
        Check(probe.modulePaths == paths && probe.metadataReads == metadata &&
              probe.providerReads == providerReads && probe.slotReads == slotReads + 1 &&
              probe.resolverCalls == resolverCalls + 1 &&
              probe.lastResolverBase == providerBase,
              "installed fast path performs one slot read without rehashing or reparse");
        const int retainedTbb = probe.tbbRetains, retainedProvider = probe.providerRetains;
        auto reloaded = group.Run(base + 0x1000, hook, probe.backend, expectation);
        Check(reloaded.state == TbbIatState::Poisoned &&
              probe.tbbRetains == retainedTbb && probe.providerRetains == retainedProvider,
              "changed TBB base after install poisons without third reference");
    }

    {
        ResetImage();
        TbbImportTransaction group;
        Probe probe(group);
        Check(group.Run(base, hook, probe.backend, expectation).state == TbbIatState::Applied,
              "provider-change setup applies active import");
        const int cas = probe.exchanges, publication = probe.publications;
        probe.backend.resolveBeginThreadEx = [&probe](std::uint32_t retainedBase) {
            ++probe.resolverCalls;
            probe.lastResolverBase = retainedBase;
            return provider + 4;
        };
        auto changed = group.Run(base, hook, probe.backend, expectation);
        Check(changed.state == TbbIatState::Poisoned && probe.exchanges == cas &&
              probe.publications == publication && probe.tbbRetains == 1 &&
              probe.providerRetains == 1,
              "installed provider drift poisons without writes or new references");
    }

    for (int mode = 0; mode < 2; ++mode)
    {
        ResetImage();
        if (mode == 0) ForceByte(0x4302, 'X');
        else ForceByte(0x1000, static_cast<unsigned char>(image[0x1000] ^ 1));
        const std::vector<unsigned char> before(image, image + imageSize);
        TbbImportTransaction group;
        Probe probe(group);
        auto report = group.Run(base, hook, probe.backend, expectation);
        Check(report.state == TbbIatState::Poisoned && probe.resolverCalls == 0 &&
              probe.publications == 0 &&
              probe.opens == 0 && probe.exchanges == 0,
              "named import or normalized text mismatch rejects before publication and writes");
        Check(memcmp(before.data(), image, imageSize) == 0 &&
              probe.tbbRetains == 1 && probe.providerRetains == 0,
              "prevalidation preserves owned image and bounds retained references");
    }

    {
        ResetImage();
        ForceSlot(0x12345678);
        const std::vector<unsigned char> before(image, image + imageSize);
        TbbImportTransaction group;
        Probe probe(group);
        auto report = group.Run(base, hook, probe.backend, expectation);
        Check(report.state == TbbIatState::Poisoned && probe.resolverCalls == 0 &&
              probe.exchanges == 0 &&
              probe.opens == 0 && probe.publications == 0,
              "foreign slot fails all prevalidation before publication or writes");
        Check(memcmp(before.data(), image, imageSize) == 0,
              "foreign prevalidation leaves complete synthetic image unchanged");
        Check(probe.tbbRetains == 1 && probe.providerRetains == 0,
              "unowned foreign target retains no provider reference");
        Check(group.Run(base, hook, probe.backend, expectation).state == TbbIatState::Poisoned &&
              probe.tbbRetains == 1 && probe.providerRetains == 0,
              "terminal foreign slot retry takes no additional references");
    }

    {
        ResetImage();
        std::wstring foreignPath = argv[1];
        foreignPath += L"\\surrogate.dll";
        HMODULE foreign = LoadLibraryW(foreignPath.c_str());
        Check(foreign != nullptr, "foreign provider surrogate loads for owner test");
        FARPROC address = foreign ? GetProcAddress(foreign, "_RetentionProbe@0") : nullptr;
        if (!address && foreign) address = GetProcAddress(foreign, "RetentionProbe");
        Check(address != nullptr, "foreign provider surrogate export resolves");
        if (address)
        {
            const auto foreignBase = static_cast<std::uint32_t>(
                reinterpret_cast<std::uintptr_t>(foreign));
            const auto foreignTarget = static_cast<std::uint32_t>(
                reinterpret_cast<std::uintptr_t>(address));
            ForceSlot(foreignTarget);
            TbbImportTransaction group;
            Probe probe(group);
            const auto retain = probe.backend.retainFromAddress;
            probe.backend.retainFromAddress = [retain, foreignTarget, foreignBase, &probe](
                std::uint32_t target, std::uint32_t& retained) {
                if (target != foreignTarget) return retain(target, retained);
                ++probe.providerRetains;
                HMODULE module = nullptr;
                const bool ok = GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
                    reinterpret_cast<LPCWSTR>(static_cast<std::uintptr_t>(target)),
                    &module) != FALSE;
                retained = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(module));
                return ok && retained == foreignBase;
            };
            const auto modulePath = probe.backend.modulePath;
            probe.backend.modulePath = [modulePath, foreignBase, foreignPath](
                std::uint32_t retained, wchar_t* path, std::size_t capacity) {
                if (retained != foreignBase) return modulePath(retained, path, capacity);
                if (foreignPath.size() + 1 > capacity) return false;
                wcscpy_s(path, capacity, foreignPath.c_str());
                return true;
            };
            const auto mapped = probe.backend.isMappedImage;
            probe.backend.isMappedImage = [mapped, foreignBase](std::uint32_t candidate,
                                                              std::uint32_t extent) {
                return candidate == foreignBase || mapped(candidate, extent);
            };
            auto report = group.Run(base, hook, probe.backend, expectation);
            Check(report.state == TbbIatState::Poisoned && probe.resolverCalls == 0 &&
                  probe.publications == 0 && probe.opens == 0 && probe.exchanges == 0,
                  "foreign owned provider fails identity before export resolution or writes");
            Check(ReadSlot() == foreignTarget && probe.tbbRetains == 1 &&
                  probe.providerRetains == 1,
                  "forwarded-target surrogate slot remains untouched with bounded refs");
        }
        if (foreign) FreeLibrary(foreign);
    }

    {
        ResetImage();
        const auto* providerImage = reinterpret_cast<const unsigned char*>(
            static_cast<std::uintptr_t>(providerBase));
        const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(providerImage);
        const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS32*>(
            providerImage + dos->e_lfanew);
        const auto& directory = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
        const auto* exports = reinterpret_cast<const IMAGE_EXPORT_DIRECTORY*>(
            providerImage + directory.VirtualAddress);
        Check(expectation.crtBeginThreadExOrdinal >= exports->Base &&
              expectation.crtBeginThreadExOrdinal - exports->Base < exports->NumberOfFunctions,
              "physical CRT ordinal maps within its export table");
        const std::uint32_t functionEntry = exports->AddressOfFunctions + 4 *
            (expectation.crtBeginThreadExOrdinal - exports->Base);
        const std::uint32_t forwarderRva = directory.VirtualAddress;
        TbbImportTransaction group;
        Probe probe(group);
        const auto readProvider = probe.backend.readProvider;
        probe.backend.readProvider = [readProvider, functionEntry, forwarderRva](
            std::uint32_t owner, std::uint32_t rva, unsigned char* out, std::size_t count) {
            if (!readProvider(owner, rva, out, count)) return false;
            if (owner == providerBase && rva == functionEntry && count == 4)
                memcpy(out, &forwarderRva, 4);
            return true;
        };
        auto report = group.Run(base, hook, probe.backend, expectation);
        Check(report.state == TbbIatState::Poisoned && probe.resolverCalls == 0 &&
              probe.publications == 0 && probe.opens == 0 && probe.exchanges == 0,
              "injected mapped CRT forwarder metadata rejects before resolver or mutation");
        Check(ReadSlot() == provider && probe.tbbRetains == 1 &&
              probe.providerRetains == 1,
              "forwarder-metadata rejection preserves IAT and bounded references");
    }

    for (int mode = 0; mode < 5; ++mode)
    {
        ResetImage();
        TbbImportTransaction group;
        Probe probe(group);
        if (mode == 0) probe.failOpen = 1;
        if (mode == 1) probe.failCas = 1;
        if (mode == 2) { probe.failCas = 1; probe.failAfterSwap = true; }
        if (mode == 3) probe.failReadback = 1;
        if (mode == 4) probe.failClose = 1;
        auto report = group.Run(base, hook, probe.backend, expectation);
        Check(report.state == TbbIatState::RolledBack,
              "protection/CAS-before/CAS-after/readback/final-restore failure rolls back");
        Check(ReadSlot() == provider && OriginalProtection(),
              "clean rollback restores original slot and protection");
        Check(probe.publications == 1 && probe.published == provider,
              "original CRT provider remains published after failed install");
        Check(probe.tbbRetains == 1 && probe.providerRetains == 1,
              "clean rollback retains at most one reference to each module");
        if (mode == 0) Check(!report.attempted && probe.exchanges == 0,
                             "protection acquisition failure exposes no hook");
        if (mode == 2) Check(report.hookPossiblyExposed &&
                             report.originalVerifiedAfterRecovery,
                             "post-exchange failure preserves exposure and verifies recovery");
    }

    {
        ResetImage();
        TbbImportTransaction group;
        Probe probe(group);
        probe.failCas = 1;
        probe.failAfterSwap = true;
        probe.failReadback = 1;
        auto report = group.Run(base, hook, probe.backend, expectation);
        Check(report.state == TbbIatState::RolledBack && report.hookPossiblyExposed &&
              report.originalPublished && probe.published == provider,
              "uncertain complete exchange plus unreadable immediate slot retains original");
        Check(ReadSlot() == provider, "uncertain-exposure rollback restores provider slot");
        probe.backend.resolveBeginThreadEx = [&probe](std::uint32_t retainedBase) {
            ++probe.resolverCalls;
            probe.lastResolverBase = retainedBase;
            return provider + 4;
        };
        auto retry = group.Run(base, hook, probe.backend, expectation);
        Check(retry.state == TbbIatState::Poisoned && probe.publications == 1,
              "changed CRT provider after possible exposure terminally poisons without republishing");
    }

    {
        ResetImage();
        TbbImportTransaction group;
        Probe probe(group);
        probe.foreignBeforeCas = true;
        auto report = group.Run(base, hook, probe.backend, expectation);
        Check(report.state == TbbIatState::RollbackFailed && ReadSlot() == 0x12345678,
              "foreign clobber between prepare/CAS is never overwritten");
        Check(group.Run(base, hook, probe.backend, expectation).state == TbbIatState::Poisoned,
              "foreign mid-CAS incomplete recovery is terminal");
    }

    for (int mode = 0; mode < 2; ++mode)
    {
        ResetImage();
        TbbImportTransaction group;
        Probe probe(group);
        probe.failCas = 1;
        probe.failAfterSwap = true;
        if (mode == 0) probe.failRollback = 1;
        else probe.failClose = 1;
        auto report = group.Run(base, hook, probe.backend, expectation);
        Check(report.state == TbbIatState::RollbackFailed,
              "rollback CAS/protection failure distinct from clean rollback");
        Check(report.originalPublished && probe.published == provider,
              "incomplete recovery keeps original provider published");
        Check(group.Run(base, hook, probe.backend, expectation).state == TbbIatState::Poisoned,
              "rollback failure poisons future retry");
    }

    {
        ResetImage();
        TbbImportTransaction group;
        Probe probe(group);
        probe.reenterOnPublish = true;
        auto report = group.Run(base, hook, probe.backend, expectation);
        Check(report.state == TbbIatState::Applied && probe.reentrantState == TbbIatState::Busy,
              "reentrant retry reports Busy without waiting under callback");
    }

    {
        ResetImage();
        TbbImportTransaction group;
        Probe probe(group);
        std::promise<void> entered;
        auto enteredFuture = entered.get_future();
        std::promise<void> release;
        auto releaseFuture = release.get_future().share();
        auto publish = probe.backend.publishOriginal;
        probe.backend.publishOriginal = [publish, &entered, releaseFuture](std::uint32_t value) {
            publish(value);
            entered.set_value();
            releaseFuture.wait();
        };
        auto worker = std::async(std::launch::async, [&] {
            return group.Run(base, hook, probe.backend, expectation);
        });
        const bool atPublish = enteredFuture.wait_for(std::chrono::seconds(2)) ==
                               std::future_status::ready;
        Check(atPublish, "first transaction reaches held publication callback");
        if (atPublish)
        {
            const auto begin = std::chrono::steady_clock::now();
            auto second = group.Run(base, hook, probe.backend, expectation);
            const auto elapsed = std::chrono::steady_clock::now() - begin;
            Check(second.state == TbbIatState::Busy &&
                  elapsed < std::chrono::milliseconds(200) && probe.exchanges == 0,
                  "concurrent retry returns Busy before first CAS without waiting");
        }
        release.set_value();
        Check(worker.get().state == TbbIatState::Applied && ReadSlot() == hook,
              "held transaction completes after concurrent Busy retry");
    }

    VirtualFree(image, 0, MEM_RELEASE);
    printf("RESULT checks=%d failures=%d active=1 skipped=1 cap=0\n",
           checks.load(), failures.load());
    return failures.load() ? 1 : 0;
}
