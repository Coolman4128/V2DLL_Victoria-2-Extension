#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "LegacyByteTransaction.h"
#include "Win32Memory.h"
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

using namespace fork_boundary;
static DWORD g_base = 0;
static DWORD g_imageSize = 0;
static unsigned char* image = nullptr;
static int checks = 0;
static int failures = 0;

// ACTUAL_LEGACY_BYTE_TABLE_INJECTED

static void Check(bool ok, const char* label)
{
    ++checks;
    if (!ok) { ++failures; printf("FAIL %s\n", label); }
}

static DWORD Resolve(const BytePatch& patch)
{
    return patch.rva ? patch.rva : FileOffsetToRVA(patch.fileOffset);
}

static std::vector<LegacyByteSite> Sites()
{
    std::vector<LegacyByteSite> sites;
    for (int i = 0; i < EXE_PATCH_COUNT; ++i)
    {
        const BytePatch& patch = EXE_PATCHES[i];
        sites.push_back({patch.name, Resolve(patch),
            std::vector<unsigned char>(patch.expect, patch.expect + patch.len),
            std::vector<unsigned char>(patch.replace, patch.replace + patch.len), patch.enabled});
    }
    return sites;
}

static void Seed()
{
    DWORD ignored = 0;
    VirtualProtect(image, g_imageSize, PAGE_READWRITE, &ignored);
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
        Check(rva && rva + patch.len <= g_imageSize, "resolved fixture span");
        if (rva && rva + patch.len <= g_imageSize)
            memcpy(image + rva, patch.expect, patch.len);
    }
    Check(VirtualProtect(image, g_imageSize, PAGE_EXECUTE_READ, &ignored) != FALSE,
          "seed image execute-read protection");
}

static bool Protected()
{
    for (int i = 0; i < EXE_PATCH_COUNT; ++i)
    {
        MEMORY_BASIC_INFORMATION region = {};
        if (VirtualQuery(image + Resolve(EXE_PATCHES[i]), &region, sizeof(region)) != sizeof(region) ||
            region.Protect != PAGE_EXECUTE_READ)
            return false;
    }
    return true;
}

struct Probe
{
    MemoryAccess real;
    int reads = 0, opens = 0, writes = 0, closes = 0, flushes = 0;
    int failRead = 0, failOpen = 0, failWrite = 0, failClose = 0, failFlush = 0;
    bool partial = false;
    bool failRollbackWrite = false;
    bool failCloseBefore = false;
    std::vector<std::string> events;
    std::vector<std::uint32_t> openedRvas, closedRvas;

    Probe() : real(MakeWin32ImageMemory(g_base, g_imageSize)) {}
    MemoryAccess Access()
    {
        return {
            [this](std::uint32_t rva, unsigned char* out, std::size_t count) {
                events.push_back("read");
                if (++reads == failRead) return false;
                return real.read(rva, out, count);
            },
            [this](std::uint32_t rva, std::size_t count, std::uintptr_t& token) {
                events.push_back("open");
                if (++opens == failOpen) return false;
                const bool result = real.beginWrite(rva, count, token);
                if (result) openedRvas.push_back(rva);
                return result;
            },
            [this](std::uint32_t rva, const unsigned char* data, std::size_t count) {
                events.push_back("write");
                ++writes;
                if (writes == failWrite)
                {
                    if (partial) Check(real.write(rva, data, 1), "injected partial byte changed");
                    return false;
                }
                if (failRollbackWrite && writes > failWrite) return false;
                return real.write(rva, data, count);
            },
            [this](std::uint32_t rva, std::size_t count, std::uintptr_t token) {
                events.push_back("close");
                ++closes;
                if (closes == failClose && failCloseBefore) return false;
                bool actual = real.restoreProtection(rva, count, token);
                closedRvas.push_back(rva);
                return actual && closes != failClose;
            },
            [this](std::uint32_t rva, std::size_t count) {
                events.push_back("flush");
                if (++flushes == failFlush) return false;
                return real.flushInstructions(rva, count);
            }
        };
    }
};

static void CheckPristine(const std::vector<unsigned char>& before, const char* label)
{
    Check(memcmp(before.data(), image, g_imageSize) == 0, label);
    Check(Protected(), "all site protections restored");
}

static void CheckNoWrites(const Probe& probe, const std::vector<unsigned char>& before)
{
    Check(probe.opens == 0 && probe.writes == 0 && probe.closes == 0 && probe.flushes == 0,
          "validation failure has zero mutation callbacks");
    CheckPristine(before, "validation failure leaves exact image");
}

int main()
{
    g_imageSize = 0xB00000;
    image = static_cast<unsigned char*>(VirtualAlloc(nullptr, g_imageSize,
        MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
    if (!image) return 2;
    g_base = static_cast<DWORD>(reinterpret_cast<std::uintptr_t>(image));
    Seed();
    const auto sites = Sites();
    Check(EXE_PATCH_COUNT == 21 && sites.size() == 21, "all current rows extracted");
    int enabled = 0;
    for (const auto& site : sites) enabled += site.enabled ? 1 : 0;
    Check(enabled == 18, "18 compiled enabled defaults");

    {
        Seed();
        for (int i = 0; i < EXE_PATCH_COUNT; ++i)
            if (!EXE_PATCHES[i].enabled)
            {
                DWORD old = 0; VirtualProtect(image + Resolve(EXE_PATCHES[i]), 1, PAGE_READWRITE, &old);
                image[Resolve(EXE_PATCHES[i])] ^= 1;
                VirtualProtect(image + Resolve(EXE_PATCHES[i]), 1, old, &old);
            }
        const std::vector<unsigned char> before(image, image + g_imageSize);
        std::vector<unsigned char> expected = before;
        for (const auto& site : sites)
            if (site.enabled)
                memcpy(expected.data() + site.rva, site.replacement.data(), site.replacement.size());
        Probe probe;
        const auto report = InstallLegacyByteTable(sites, g_imageSize, probe.Access());
        Check(report.state == LegacyByteState::Applied, "real Win32 backend applies table");
        Check(report.enabledCount == 18 && report.skippedCount == 3, "enabled/skipped count");
        Check(memcmp(expected.data(), image, g_imageSize) == 0, "exact successful replacement image");
        Check(Protected(), "successful transaction restores protections");
        Check(probe.opens == 18 && probe.writes == 18 && probe.closes == 18 && probe.flushes == 18,
              "enabled rows alone invoke writes and protection");
        Check(probe.closedRvas.size() == probe.openedRvas.size() &&
              std::equal(probe.closedRvas.begin(), probe.closedRvas.end(),
                         probe.openedRvas.rbegin()), "shared-page tokens restored in reverse acquisition order");
        auto firstWrite = std::find(probe.events.begin(), probe.events.end(), "write");
        Check(std::count(probe.events.begin(), firstWrite, "open") == 18,
              "all enabled protection tokens acquired before first write");
        for (int i = 0; i < EXE_PATCH_COUNT; ++i)
            Check(report.sites[i].state == (sites[i].enabled ? LegacyByteSiteState::Applied : LegacyByteSiteState::Skipped),
                  "per-site success or disabled classification");
    }

    int lastEnabled = -1;
    for (int i = 0; i < EXE_PATCH_COUNT; ++i)
        if (sites[i].enabled) lastEnabled = i;
    Check(lastEnabled > 0, "later enabled site identified");
    for (int bad : {0, lastEnabled})
    {
        Seed();
        const std::vector<unsigned char> before(image, image + g_imageSize);
        DWORD old = 0; VirtualProtect(image + sites[bad].rva, 1, PAGE_READWRITE, &old);
        image[sites[bad].rva] ^= 1;
        VirtualProtect(image + sites[bad].rva, 1, old, &old);
        const std::vector<unsigned char> mismatched(image, image + g_imageSize);
        Probe probe;
        auto report = InstallLegacyByteTable(sites, g_imageSize, probe.Access());
        Check(report.state == LegacyByteState::Rejected, "early/later signature mismatch rejected");
        Check(report.sites[bad].state == LegacyByteSiteState::Rejected,
              "mismatched row distinctly rejected");
        Check(report.reason.find(sites[bad].name + ": enabled row signature mismatch") != std::string::npos,
              "mismatch report identifies actual row and stage");
        CheckNoWrites(probe, mismatched);
        Check(memcmp(before.data(), mismatched.data(), g_imageSize) != 0, "mismatch changed one fixture byte");
    }

    for (int mode = 0; mode < 5; ++mode)
    {
        Seed();
        const std::vector<unsigned char> before(image, image + g_imageSize);
        Probe probe;
        if (mode == 0) probe.failOpen = 2;
        if (mode == 1) { probe.failWrite = 2; probe.partial = true; }
        if (mode == 2) probe.failRead = enabled + 2;
        if (mode == 3) probe.failFlush = 2;
        if (mode == 4) probe.failClose = 2;
        auto report = InstallLegacyByteTable(sites, g_imageSize, probe.Access());
        Check(report.state == LegacyByteState::RolledBack, "injected failure rolls back cleanly");
        const char* stage[] = {"write protection acquisition failed", "write failed or was partial",
                               "replacement readback failed", "instruction flush failed",
                               "final protection restoration failed"};
        Check(report.reason.find(stage[mode]) != std::string::npos &&
              report.reason.find("rollback verified") != std::string::npos,
              "failed mutation report identifies stage and clean recovery");
        CheckPristine(before, "injected failure restores all original bytes");
        for (const auto& site : report.sites)
            Check(site.state != LegacyByteSiteState::Applied, "failed transaction reports no applied sites");
        if (mode == 0) Check(probe.writes == 0, "acquisition failure prevents all writes");
        if (mode == 1) Check(report.sites[1].state == LegacyByteSiteState::RolledBack,
                             "failed partial-write site is recovered");
    }

    for (int mode = 0; mode < 4; ++mode)
    {
        Seed();
        Probe probe;
        probe.failWrite = 2; probe.partial = true;
        if (mode == 0) probe.failRollbackWrite = true;
        if (mode == 1) probe.failRead = enabled + 2;
        if (mode == 2) probe.failFlush = 2;
        if (mode == 3) { probe.failClose = enabled + 2; probe.failCloseBefore = true; }
        auto report = InstallLegacyByteTable(sites, g_imageSize, probe.Access());
        Check(report.state == LegacyByteState::RollbackFailed,
              "rollback write/readback/flush/protection failure distinct");
        Check(report.reason.find("write failed or was partial") != std::string::npos &&
              report.reason.find("recovery incomplete") != std::string::npos,
              "failed recovery reports both initiating stage and incomplete state");
        Check(probe.closes >= enabled, "recovery continues closing all held tokens");
        bool attributed = false;
        for (const auto& site : report.sites)
            attributed |= site.state == LegacyByteSiteState::RollbackFailed;
        Check(attributed, "rollback failure attributed to at least one site");
        if (mode != 3) Check(Protected(), "rollback failure still attempts protection restoration");
        else Check(!Protected(), "unrestored original page protection remains observable");
    }

    {
        Seed();
        auto renamed = sites;
        renamed[0].name = std::string(96, 'L');
        Probe probe;
        auto report = InstallLegacyByteTable(renamed, g_imageSize, probe.Access());
        Check(report.state == LegacyByteState::Applied, "maximum allowed enabled name applies");
        Check(report.sites[0].name == renamed[0].name, "maximum name reported without truncation");
        Check(Protected(), "maximum-name success restores protections");
    }

    for (int mode = 0; mode < 2; ++mode)
    {
        Seed();
        const std::vector<unsigned char> before(image, image + g_imageSize);
        Probe probe;
        if (mode == 0) probe.failWrite = enabled;
        else probe.failClose = enabled;
        auto report = InstallLegacyByteTable(sites, g_imageSize, probe.Access());
        Check(report.state == LegacyByteState::RolledBack,
              "last write or last protection restore rolls back cleanly");
        CheckPristine(before, "late failure restores all original bytes");
        const char* stage = mode == 0 ? "write failed or was partial" :
                                        "final protection restoration failed";
        Check(report.reason.find(sites[lastEnabled].name + ": " + stage) != std::string::npos ||
              (mode == 1 && report.reason.find(sites[0].name + ": " + stage) != std::string::npos),
              "late failure names exact row and stage");
    }

    for (int mode = 0; mode < 4; ++mode)
    {
        Seed();
        const std::vector<unsigned char> before(image, image + g_imageSize);
        auto invalid = sites;
        if (mode == 0) invalid[1].name = invalid[0].name;
        if (mode == 1) invalid[1].rva = invalid[0].rva;
        if (mode == 2) invalid[1].rva = g_imageSize;
        if (mode == 3) invalid[0].name = std::string(97, 'X');
        Probe probe;
        auto report = InstallLegacyByteTable(invalid, g_imageSize, probe.Access());
        Check(report.state == LegacyByteState::Rejected, "duplicate/overlap/range rejected");
        CheckNoWrites(probe, before);
    }

    VirtualFree(image, 0, MEM_RELEASE);
    printf("RESULT checks=%d failures=%d descriptors=%d enabled=%d\n",
           checks, failures, EXE_PATCH_COUNT, enabled);
    return failures ? 1 : 0;
}
