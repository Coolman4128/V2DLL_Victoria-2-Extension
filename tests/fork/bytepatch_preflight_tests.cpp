#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cstdarg>
#include <vector>

static DWORD g_base = 0;
static DWORD g_imageSize = 0;
static unsigned char* image = nullptr;
static int checks = 0;
static int failures = 0;
static char lastLog[512] = {};

enum QueryMode { VALID, QUERY_FAIL, GUARDED, UNCOMMITTED, FOREIGN_BASE, PRIVATE_TYPE,
                 NO_ACCESS, BAD_REGION, SPLIT_REGION };
static QueryMode queryMode = VALID;
static DWORD splitRva = 0;

static void Log(const char* format, ...)
{
    va_list args;
    va_start(args, format);
    vsnprintf_s(lastLog, sizeof(lastLog), _TRUNCATE, format, args);
    va_end(args);
}

static SIZE_T TestVirtualQuery(const void* address, MEMORY_BASIC_INFORMATION* info, SIZE_T length)
{
    if (queryMode == QUERY_FAIL || length != sizeof(*info) || !image)
        return 0;
    const uintptr_t cursor = reinterpret_cast<uintptr_t>(address);
    const uintptr_t base = reinterpret_cast<uintptr_t>(image);
    if (cursor < base || cursor >= base + g_imageSize)
        return 0;
    memset(info, 0, sizeof(*info));
    info->BaseAddress = image;
    info->AllocationBase = image;
    info->RegionSize = g_imageSize;
    info->State = MEM_COMMIT;
    info->Type = MEM_IMAGE;
    info->Protect = PAGE_EXECUTE_READ;
    if (queryMode == GUARDED) info->Protect |= PAGE_GUARD;
    if (queryMode == UNCOMMITTED) info->State = MEM_RESERVE;
    if (queryMode == FOREIGN_BASE) info->AllocationBase = image + 0x1000;
    if (queryMode == PRIVATE_TYPE) info->Type = MEM_PRIVATE;
    if (queryMode == NO_ACCESS) info->Protect = PAGE_NOACCESS;
    if (queryMode == BAD_REGION) info->RegionSize = 0;
    if (queryMode == SPLIT_REGION)
    {
        const uintptr_t split = base + splitRva;
        if (cursor < split)
            info->RegionSize = splitRva;
        else
        {
            info->BaseAddress = reinterpret_cast<void*>(split);
            info->RegionSize = g_imageSize - splitRva;
        }
    }
    return sizeof(*info);
}

#define VirtualQuery TestVirtualQuery
// ACTUAL_BYTEPATCH_BODIES_INJECTED
#undef VirtualQuery

static void Check(bool condition, const char* label)
{
    ++checks;
    if (!condition)
    {
        ++failures;
        printf("FAIL %s: %s\n", label, lastLog);
    }
}

static DWORD Resolve(const BytePatch& patch)
{
    return patch.rva ? patch.rva : FileOffsetToRVA(patch.fileOffset);
}

static void Seed()
{
    memset(image, 0xCC, g_imageSize);
    IMAGE_DOS_HEADER* dos = reinterpret_cast<IMAGE_DOS_HEADER*>(image);
    dos->e_magic = IMAGE_DOS_SIGNATURE;
    dos->e_lfanew = 0x80;
    IMAGE_NT_HEADERS32* nt = reinterpret_cast<IMAGE_NT_HEADERS32*>(image + 0x80);
    nt->Signature = IMAGE_NT_SIGNATURE;
    nt->FileHeader.NumberOfSections = 1;
    nt->OptionalHeader.SizeOfImage = g_imageSize;
    IMAGE_SECTION_HEADER* section = IMAGE_FIRST_SECTION(nt);
    section->PointerToRawData = 0x1000;
    section->VirtualAddress = 0x1000;
    section->SizeOfRawData = 0x400000;
    for (int i = 0; i < EXE_PATCH_COUNT; ++i)
    {
        const BytePatch& patch = EXE_PATCHES[i];
        DWORD rva = Resolve(patch);
        if (!rva || rva + patch.len > g_imageSize)
        {
            Check(false, "fixture descriptor span");
            continue;
        }
        memcpy(image + rva, patch.expect, patch.len);
    }
    queryMode = VALID;
    splitRva = 0;
}

static void ExpectPreflight(bool expected, const char* label)
{
    std::vector<unsigned char> before(image, image + g_imageSize);
    bool actual = PreflightExePatches();
    Check(actual == expected, label);
    Check(memcmp(before.data(), image, g_imageSize) == 0, "preflight image unchanged");
}

int main()
{
    g_imageSize = 0xB00000;
    image = static_cast<unsigned char*>(VirtualAlloc(nullptr, g_imageSize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
    if (!image) return 2;
    g_base = static_cast<DWORD>(reinterpret_cast<uintptr_t>(image));

    Check(EXE_PATCH_COUNT == 21, "current descriptor count");
    int enabled = 0;
    for (int i = 0; i < EXE_PATCH_COUNT; ++i)
        enabled += EXE_PATCHES[i].enabled ? 1 : 0;
    Check(enabled == 18, "current default-enabled count");

    Seed();
    ExpectPreflight(true, "all enabled signatures match");
    for (int i = 0; i < EXE_PATCH_COUNT; ++i)
        if (!EXE_PATCHES[i].enabled)
            image[Resolve(EXE_PATCHES[i])] ^= 1;
    ExpectPreflight(true, "disabled signatures skipped");

    for (int i = 0; i < EXE_PATCH_COUNT; ++i)
    {
        if (!EXE_PATCHES[i].enabled) continue;
        Seed();
        image[Resolve(EXE_PATCHES[i])] ^= 1;
        ExpectPreflight(false, EXE_PATCHES[i].name);
    }

    const int first = 0;
    BytePatch saved = EXE_PATCHES[first];
    Seed(); EXE_PATCHES[first].len = 0;
    ExpectPreflight(false, "zero length"); EXE_PATCHES[first] = saved;
    Seed(); EXE_PATCHES[first].len = 17;
    ExpectPreflight(false, "oversized length"); EXE_PATCHES[first] = saved;
    Seed(); EXE_PATCHES[first].rva = g_imageSize - 1; EXE_PATCHES[first].len = 2;
    ExpectPreflight(false, "image edge span"); EXE_PATCHES[first] = saved;
    Seed(); EXE_PATCHES[first].rva = 0; EXE_PATCHES[first].fileOffset = 0x500000;
    ExpectPreflight(false, "unmapped file offset"); EXE_PATCHES[first] = saved;
    Seed(); EXE_PATCHES[first].rva = g_imageSize;
    ExpectPreflight(false, "rva beyond image"); EXE_PATCHES[first] = saved;

    const QueryMode rejected[] = { QUERY_FAIL, GUARDED, UNCOMMITTED, FOREIGN_BASE,
                                   PRIVATE_TYPE, NO_ACCESS, BAD_REGION };
    for (QueryMode mode : rejected)
    {
        Seed(); queryMode = mode;
        ExpectPreflight(false, "VirtualQuery rejection");
    }
    // The second enabled descriptor spans four bytes, forcing two queries.
    Seed(); queryMode = SPLIT_REGION; splitRva = Resolve(EXE_PATCHES[1]) + 1;
    ExpectPreflight(true, "valid split mapped regions");

    VirtualFree(image, 0, MEM_RELEASE);
    printf("RESULT checks=%d failures=%d descriptors=%d enabled=%d\n",
           checks, failures, EXE_PATCH_COUNT, enabled);
    return failures ? 1 : 0;
}
