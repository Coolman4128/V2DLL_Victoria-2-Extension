// Actual Win32 scratch-memory/protection tests. Never opens a game/process or
// executes synthetic code. This backend is test-only, not production wiring.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include "../../Source/V2/V2/Fork/ViewsBoundary.h"
#include "../../Source/V2/V2/Fork/Win32Memory.h"
#include <array>
#include <cstring>
#include <cstdlib>
#include <iostream>
#include <limits>

using namespace fork_boundary;
namespace {
int checks = 0, cases = 0;
void Check(bool ok, const char* why) {
    ++checks;
    if (!ok) { std::cerr << "FAIL: " << why << " Win32=" << GetLastError() << '\n'; std::exit(1); }
}
constexpr std::uint32_t Size = 0xB00000;
constexpr std::array<std::uint32_t, 4> Sentinel = {11, 22, 33, 44};
NativeContract Contract() {
    return {"synthetic only", "synthetic only", "no executable code called",
        "single-thread owned scratch allocation", "OS memory seam; no native game proof"};
}
ExecutableIdentity Identity() { return {std::string(64, 'a'), 0x14C, Size}; }
ViewsEvidence Evidence() {
    ViewsEvidence e; e.closureEvidence = "Synthetic fixture only; never a supported game profile";
    for (std::size_t i = 0; i < ViewSlotCount; ++i) {
        auto& s = e.slots[i]; s.targetRva = 0x1000 + static_cast<std::uint32_t>(i)*16;
        s.entry.name = "synthetic entry " + std::to_string(i); s.entry.rva = s.targetRva;
        s.entry.expected = {0x90}; s.entry.kind = PatchKind::Instructions;
        s.entry.instructionLengths = {1}; s.entry.resumeRva = s.targetRva+1;
        s.entry.validationOnly = true; s.entry.contract = Contract();
    }
    PatchSite d; d.name = "synthetic read-only data"; d.rva = 0x2000;
    d.expected = {0x90, 0x90}; d.validationOnly = true; d.contract = Contract();
    e.readDependencies.push_back(d); return e;
}
struct Scratch {
    unsigned char* image = nullptr;
    std::uint32_t base = 0;
    std::array<std::uint32_t, 4> native{}, replacement{};
    const std::array<std::uint32_t, 4>* outputs = nullptr;
    int opens = 0, writes = 0, restores = 0, flushes = 0;
    int failWriteAt = 0, failRestoreAt = 0;
    bool restoreBeforeFailure = false, permanentRestoreFailure = false;
    std::uint32_t failedRestoreRva = 0;
    Scratch() {
        static_assert(sizeof(void*) == 4, "This fixture requires Release Win32");
        image = static_cast<unsigned char*>(VirtualAlloc(nullptr, Size, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE));
        Check(image != nullptr, "VirtualAlloc scratch image");
        const auto address = reinterpret_cast<std::uintptr_t>(image);
        Check(address <= std::numeric_limits<std::uint32_t>::max()-Size, "PE32 image extent");
        base = static_cast<std::uint32_t>(address);
        std::memset(image, 0x90, Size);
        // Synthetic PE32 headers are deliberately unrelated to the game. The
        // adapter consumes supplied identity; this fixture does not discover it.
        auto dos = reinterpret_cast<IMAGE_DOS_HEADER*>(image);
        dos->e_magic = IMAGE_DOS_SIGNATURE; dos->e_lfanew = 0x80;
        auto nt = reinterpret_cast<IMAGE_NT_HEADERS32*>(image+0x80);
        *nt = {}; nt->Signature = IMAGE_NT_SIGNATURE;
        nt->FileHeader.Machine = IMAGE_FILE_MACHINE_I386;
        nt->OptionalHeader.Magic = IMAGE_NT_OPTIONAL_HDR32_MAGIC;
        nt->OptionalHeader.ImageBase = base; nt->OptionalHeader.SizeOfImage = Size;
        for (std::size_t i = 0; i < ViewSlotCount; ++i) {
            native[i] = base+Evidence().slots[i].targetRva;
            replacement[i] = base+0x8000+static_cast<std::uint32_t>(i)*0x20;
            std::memcpy(image+ViewSlotRvas[i], &native[i], 4);
        }
        DWORD old = 0; Check(VirtualProtect(image, Size, PAGE_READONLY, &old) != FALSE, "initial read-only image");
        AssertProtection(PAGE_READONLY);
    }
    ~Scratch() { Check(VirtualFree(image, 0, MEM_RELEASE) != FALSE, "release only own scratch allocation"); }
    bool Range(std::uint32_t r, std::size_t n) const { return r < Size && n <= Size-r; }
    DWORD Protection(std::uint32_t r) const {
        MEMORY_BASIC_INFORMATION info{};
        Check(VirtualQuery(image+r, &info, sizeof(info)) == sizeof(info), "query actual OS protection");
        Check(info.AllocationBase == image && info.State == MEM_COMMIT, "query belongs to committed scratch allocation");
        return info.Protect;
    }
    void AssertProtection(DWORD protection) const {
        for (auto r : ViewSlotRvas) Check(Protection(r) == protection, "slot actual page protection");
        Check(Protection(0x1000) == PAGE_READONLY && Protection(0x2000) == PAGE_READONLY,
            "validation-only dependencies stay read-only");
    }
    void Unpublished() const { if (outputs) Check(*outputs == Sentinel, "no outputs published inside memory callbacks"); }
    std::array<std::uint32_t, 4> Slots() const {
        std::array<std::uint32_t, 4> result{};
        for (std::size_t i = 0; i < ViewSlotCount; ++i) std::memcpy(&result[i], image+ViewSlotRvas[i], 4);
        return result;
    }
    void Mutate(std::uint32_t r) {
        DWORD old = 0, discard = 0;
        Check(VirtualProtect(image+r, 1, PAGE_READWRITE, &old) != FALSE, "fixture mutation protection");
        image[r] ^= 1;
        Check(VirtualProtect(image+r, 1, old, &discard) != FALSE, "fixture mutation restores exact protection");
    }
    MemoryAccess Access() {
        const auto backend = MakeWin32ImageMemory(base, Size);
        return {
            [this,backend](std::uint32_t r, unsigned char* out, std::size_t n) {
                Unpublished(); Check(Range(r,n), "bounded scratch read"); return backend.read(r,out,n);
            },
            [this,backend](std::uint32_t r, std::size_t n, std::uintptr_t& token) {
                Unpublished(); ++opens; Check(Range(r,n), "bounded scratch protection acquisition");
                return backend.beginWrite(r,n,token);
            },
            [this,backend](std::uint32_t r, const unsigned char* in, std::size_t n) {
                Unpublished(); ++writes; Check(Range(r,n), "bounded scratch write");
                Check(Protection(r) == PAGE_READWRITE, "OS page actually writable before memcpy");
                if (writes == failWriteAt) { Check(n >= 2, "partial write fixture span"); Check(backend.write(r,in,2), "real partial write before injected failure"); return false; }
                return backend.write(r,in,n);
            },
            [this,backend](std::uint32_t r, std::size_t n, std::uintptr_t token) {
                Unpublished(); ++restores; Check(Range(r,n), "bounded scratch protection restore");
                const bool failure = restores == failRestoreAt ||
                    (permanentRestoreFailure && failedRestoreRva == r && failedRestoreRva != 0);
                if (failure && !restoreBeforeFailure) { failedRestoreRva = r; return false; }
                const bool ok = backend.restoreProtection(r,n,token);
                if (failure) { failedRestoreRva = r; return false; }
                return ok;
            },
            [this,backend](std::uint32_t r, std::size_t n) {
                Unpublished(); ++flushes; Check(Range(r,n), "bounded scratch flush");
                return backend.flushInstructions(r,n);
            }
        };
    }
    InstallReport Run(std::array<std::uint32_t, 4>& out) {
        ++cases; outputs = &out;
        AddressProfile profile{"synthetic Win32 scratch", Identity(), "test-only supplied identity"};
        auto report = InstallViewsFamily(Identity(),profile,base,Evidence(),replacement,out,Access());
        outputs = nullptr; return report;
    }
};
}
int main() {
    {
        ++cases;
        Scratch s; const auto backend = MakeWin32ImageMemory(s.base,Size);
        SYSTEM_INFO info{}; GetSystemInfo(&info); const auto page = info.dwPageSize;
        unsigned char data[4]{}; const unsigned char bytes[4] = {1,2,3,4};
        std::uintptr_t token = 77;
        Check(backend.read(0x1000,data,4), "real backend reads committed read-only bytes");
        Check(!backend.write(0x1000,bytes,4), "write refuses read-only page");
        Check(!backend.beginWrite(page-1,2,token) && token == 77, "cross-page acquisition fails without token mutation");
        Check(!backend.write(page-1,bytes,2), "cross-page write rejects");
        Check(!backend.restoreProtection(page-1,2,PAGE_READONLY), "cross-page restore rejects");
        Check(backend.read(page-1,data,2), "read-only validation may span readable pages");
        Check(!backend.read(Size-1,data,2) && !backend.read(0xffffffff,data,1), "range overflow rejects");
        Check(!backend.read(0,data,0) && !backend.read(0,nullptr,1), "empty/null reads reject");
        Check(!backend.read(0,reinterpret_cast<unsigned char*>(1),1), "invalid output buffer caught by SEH");
        Check(!MakeWin32ImageMemory(0,Size).read(0,data,1), "null base rejects");
        Check(!MakeWin32ImageMemory(0xfffff000,0x2000).read(0,data,1), "mapped extent overflow rejects");
        Check(!MakeWin32ImageMemory(s.base+page,Size-page).read(0,data,1), "interior base cannot authenticate different allocation");
        Check(backend.beginWrite(0x1000,4,token) && token == PAGE_READONLY, "acquire exact read-only protection token");
        Check(s.Protection(0x1000) == PAGE_READWRITE, "data receives no execute permission");
        Check(!backend.write(0x1000,reinterpret_cast<const unsigned char*>(1),4), "invalid source caught by SEH");
        Check(backend.write(0x1000,bytes,4) && backend.flushInstructions(0x1000,4), "real backend write and cache flush");
        Check(!backend.restoreProtection(0x1000,4,PAGE_NOACCESS), "invalid restoration token rejected");
        Check(backend.restoreProtection(0x1000,4,token) && backend.restoreProtection(0x1000,4,token), "protection restore supports retry after already restored");
        Check(s.Protection(0x1000) == PAGE_READONLY, "exact restored data protection");
        std::uintptr_t first=0,second=0;
        Check(backend.beginWrite(0x1000,4,first) && backend.beginWrite(0x1004,4,second), "same-page nested acquisition");
        Check(first == PAGE_READONLY && second == PAGE_READWRITE, "same-page tokens preserve acquisition state");
        Check(backend.restoreProtection(0x1004,4,second) && backend.restoreProtection(0x1000,4,first), "reverse same-page restoration");
        DWORD old = 0;
        Check(VirtualProtect(s.image+0x3000,page,PAGE_READONLY|PAGE_GUARD,&old) != FALSE, "fixture installs guard page");
        token=77;
        Check(!backend.read(0x3000,data,1) && !backend.beginWrite(0x3000,1,token) && token==77, "guard rejects before touch");
        Check(s.Protection(0x3000) == (PAGE_READONLY|PAGE_GUARD), "guard flag remains intact");
        data[0]=0x55; data[1]=0x66;
        Check(!backend.read(0x2fff,data,2) && data[0]==0x55 && data[1]==0x66,
            "cross-page validation checks guard before copying any bytes");
        Check(VirtualProtect(s.image+0x3000,page,PAGE_NOACCESS,&old) != FALSE, "fixture installs inaccessible page");
        Check(!backend.read(0x3000,data,1) && !backend.beginWrite(0x3000,1,token) &&
            !backend.flushInstructions(0x3000,1), "inaccessible page rejects");
        Check(VirtualProtect(s.image+0x3000,page,PAGE_EXECUTE_READ,&old) != FALSE, "fixture installs executable read page without executing it");
        Check(backend.beginWrite(0x3000,4,token) && token == PAGE_EXECUTE_READ, "code acquisition preserves original execute token");
        Check(s.Protection(0x3000) == PAGE_EXECUTE_READWRITE, "existing execute permission retained during write");
        Check(backend.restoreProtection(0x3000,4,token) && s.Protection(0x3000)==PAGE_EXECUTE_READ, "exact code protection restored");
        Check(VirtualProtect(s.image+0x3000,page,PAGE_READONLY|PAGE_NOCACHE,&old) != FALSE, "fixture installs noncached readable page");
        // Windows can normalize an unsupported requested cache modifier. Test
        // the actual queried protection, never pretend the request proves it.
        const DWORD actualCache = s.Protection(0x3000);
        const bool acquiredCache = backend.beginWrite(0x3000,4,token);
        Check(acquiredCache && token==actualCache, "actual cache protection captured exactly");
        Check(s.Protection(0x3000)==(PAGE_READWRITE|(actualCache&(PAGE_NOCACHE|PAGE_WRITECOMBINE))), "write retains actual cache flags");
        Check(backend.restoreProtection(0x3000,4,token) && s.Protection(0x3000)==actualCache, "actual cache protection restored exactly");
        Check(VirtualFree(s.image+0x3000,page,MEM_DECOMMIT) != FALSE, "fixture decommits only its own page");
        Check(!backend.read(0x3000,data,1) && !backend.beginWrite(0x3000,1,token), "uncommitted page rejects");
        s.AssertProtection(PAGE_READONLY);
    }
    {
        Scratch s; auto out = Sentinel; const auto r = s.Run(out);
        Check(r.state == InstallState::Applied, "successful OS transaction");
        Check(out == s.native && s.Slots() == s.replacement, "all outputs and replacements published after success");
        Check(s.opens == 4 && s.writes == 4 && s.restores == 4 && s.flushes == 4, "exact four mutation operations");
        s.AssertProtection(PAGE_READONLY);
    }
    for (std::size_t i = 0; i < ViewSlotCount; ++i) {
        Scratch s; s.Mutate(ViewSlotRvas[i]); const auto before = s.Slots(); auto out = Sentinel;
        const auto r = s.Run(out);
        Check(r.state == InstallState::Rejected && !r.reason.empty(), "each slot mismatch explicitly rejects");
        Check(s.opens == 0 && s.writes == 0 && s.restores == 0 && s.flushes == 0, "mismatch has zero protection/write/flush effects");
        Check(out == Sentinel && s.Slots() == before, "mismatch preserves outputs and all slot bytes");
        s.AssertProtection(PAGE_READONLY);
    }
    for (int at = 1; at <= 4; ++at) {
        Scratch s; s.failWriteAt = at; auto out = Sentinel; const auto r = s.Run(out);
        Check(r.state == InstallState::RolledBack && r.reason.find("partial") != std::string::npos,
            "each partially failing slot write recovers explicitly");
        Check(out == Sentinel && s.Slots() == s.native, "partial failure restores all slot bytes and withholds outputs");
        s.AssertProtection(PAGE_READONLY);
    }
    for (bool alreadyRestored : {false,true}) for (int at = 1; at <= 4; ++at) {
        Scratch s; s.failRestoreAt = at; s.restoreBeforeFailure = alreadyRestored;
        auto out = Sentinel; const auto r = s.Run(out);
        Check(r.state == InstallState::RolledBack && r.reason.find("protection") != std::string::npos,
            "restore failure recovered whether page remained writable or read-only");
        Check(out == Sentinel && s.Slots() == s.native, "restore failure withholds outputs and restores all slots");
        s.AssertProtection(PAGE_READONLY);
    }
    {
        Scratch s; s.failRestoreAt = 1; s.permanentRestoreFailure = true;
        auto out = Sentinel; const auto r = s.Run(out);
        Check(r.state == InstallState::RollbackFailed && r.reason.find("protection") != std::string::npos,
            "persistent protection restoration failure reported explicitly");
        Check(out == Sentinel && s.Slots() == s.native, "persistent restore failure still restores bytes and withholds outputs");
        Check(s.Protection(s.failedRestoreRva) == PAGE_READWRITE, "persistent failed page stays observably writable");
        for (auto address : ViewSlotRvas) if (address != s.failedRestoreRva)
            Check(s.Protection(address) == PAGE_READONLY, "other pages restored despite one persistent restore failure");
        DWORD old = 0;
        Check(VirtualProtect(s.image,Size,PAGE_READONLY,&old) != FALSE, "test harness explicitly recovers own failed protection");
        s.AssertProtection(PAGE_READONLY);
    }
    std::cout << "PASS " << cases << " Win32 scratch-memory scenarios; " << checks
        << " checks; synthetic OS memory seam only, no native ABI/lifetime certification\n";
}
