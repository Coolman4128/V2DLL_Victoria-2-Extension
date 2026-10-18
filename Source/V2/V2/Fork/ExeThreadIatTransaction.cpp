#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include "ExeThreadIatTransaction.h"
#include "Win32Memory.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <utility>

#if !defined(_M_IX86)
#error The EXE IAT transaction supports Win32 only.
#endif

namespace fork_boundary {
namespace {

constexpr std::uint32_t kPinnedImageSize = 0x1092000;
constexpr std::size_t kMaxImportDescriptors = 64;
constexpr std::size_t kMaxThunksPerDescriptor = 512;
constexpr std::size_t kMaxImportName = 64;

struct SlotJournal {
    std::uintptr_t originalProtection = 0;
    bool protectionOpen = false;
    bool attempted = false;
    bool exposed = false;
    bool byteRecovered = true;
    bool protectionRecovered = true;
};

struct GateExit {
    volatile long* gate;
    long next = 0;
    ~GateExit() noexcept {
        InterlockedExchange(reinterpret_cast<volatile LONG*>(gate), static_cast<LONG>(next));
    }
};

bool InImage(std::uint32_t rva, std::size_t length, std::uint32_t imageSize) noexcept {
    return length != 0 && rva < imageSize && length <= imageSize-rva;
}

template <class T>
bool ReadObject(const MemoryAccess& m, std::uint32_t rva, std::uint32_t imageSize, T& out) {
    return InImage(rva, sizeof(T), imageSize) &&
        m.read(rva, reinterpret_cast<unsigned char*>(&out), sizeof(T));
}

bool ReadName(const MemoryAccess& m, std::uint32_t rva, std::uint32_t imageSize,
              char (&out)[kMaxImportName]) {
    for (std::size_t i=0;i<kMaxImportName;++i) {
        if (rva > std::numeric_limits<std::uint32_t>::max()-i ||
            !InImage(rva+static_cast<std::uint32_t>(i), 1, imageSize) ||
            !m.read(rva+static_cast<std::uint32_t>(i),
                    reinterpret_cast<unsigned char*>(&out[i]), 1)) return false;
        if (out[i]=='\0') return true;
    }
    return false;
}

char LowerAscii(char c) noexcept { return c>='A' && c<='Z' ? static_cast<char>(c-'A'+'a') : c; }
bool EqualAsciiNoCase(const char* a, const char* b) noexcept {
    for (;*a && *b;++a,++b) if (LowerAscii(*a)!=LowerAscii(*b)) return false;
    return *a==*b;
}

bool SameHooks(const ExeIatHooks& a, const ExeIatHooks& b) noexcept {
    return a.pointers == b.pointers;
}

// Resolve exactly the named PE32 imports into the four pinned slot RVAs.
// This validates the mapped import metadata, not just the current slot DWORDs.
bool ResolvePinnedSlots(const MemoryAccess& memory, std::uint32_t imageSize) {
    IMAGE_DOS_HEADER dos{};
    if (!ReadObject(memory, 0, imageSize, dos) || dos.e_magic!=IMAGE_DOS_SIGNATURE ||
        dos.e_lfanew<0) return false;
    IMAGE_NT_HEADERS32 nt{};
    if (!ReadObject(memory, static_cast<std::uint32_t>(dos.e_lfanew), imageSize, nt) ||
        nt.Signature!=IMAGE_NT_SIGNATURE || nt.FileHeader.Machine!=IMAGE_FILE_MACHINE_I386 ||
        nt.OptionalHeader.Magic!=IMAGE_NT_OPTIONAL_HDR32_MAGIC ||
        nt.OptionalHeader.SizeOfImage!=imageSize ||
        nt.FileHeader.SizeOfOptionalHeader<sizeof(IMAGE_OPTIONAL_HEADER32) ||
        nt.OptionalHeader.NumberOfRvaAndSizes<=IMAGE_DIRECTORY_ENTRY_IMPORT) return false;
    const auto& dir=nt.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    if (!InImage(dir.VirtualAddress, dir.Size, imageSize) ||
        dir.Size<sizeof(IMAGE_IMPORT_DESCRIPTOR) ||
        dir.Size>kMaxImportDescriptors*sizeof(IMAGE_IMPORT_DESCRIPTOR)) return false;
    bool found[kExeThreadImportCount]{};
    bool terminated=false;
    for (std::size_t di=0;di<dir.Size/sizeof(IMAGE_IMPORT_DESCRIPTOR);++di) {
        IMAGE_IMPORT_DESCRIPTOR desc{};
        const auto descRva=dir.VirtualAddress+static_cast<std::uint32_t>(di*sizeof(desc));
        if (!ReadObject(memory,descRva,imageSize,desc)) return false;
        if (!desc.Name && !desc.FirstThunk && !desc.OriginalFirstThunk) {terminated=true;break;}
        if (!desc.Name) return false;
        char dll[kMaxImportName]{};
        if (!ReadName(memory,desc.Name,imageSize,dll)) return false;
        if (!EqualAsciiNoCase(dll,"kernel32.dll")) continue;
        if (!desc.OriginalFirstThunk || !desc.FirstThunk) return false;
        bool thunkTerminated=false;
        for (std::size_t ti=0;ti<kMaxThunksPerDescriptor;++ti) {
            const auto width=static_cast<std::uint64_t>(ti)*sizeof(std::uint32_t);
            if (static_cast<std::uint64_t>(desc.OriginalFirstThunk)+width>0xffffffffULL ||
                static_cast<std::uint64_t>(desc.FirstThunk)+width>0xffffffffULL) return false;
            const auto lookupRva=desc.OriginalFirstThunk+static_cast<std::uint32_t>(width);
            const auto slotRva=desc.FirstThunk+static_cast<std::uint32_t>(width);
            std::uint32_t lookup=0,slotValue=0;
            if (!ReadObject(memory,lookupRva,imageSize,lookup) ||
                !ReadObject(memory,slotRva,imageSize,slotValue)) return false;
            if (!lookup) {thunkTerminated=true;break;}
            if (lookup&IMAGE_ORDINAL_FLAG32) continue;
            if (lookup>imageSize-2) return false;
            char name[kMaxImportName]{};
            if (!ReadName(memory,lookup+2,imageSize,name)) return false;
            for (std::size_t k=0;k<kExeThreadImportCount;++k) {
                if (std::strcmp(name,kExeThreadImportNames[k])!=0) continue;
                if (found[k] || slotRva!=kExeThreadIatRvas[k] || (slotRva&3) || !slotValue)
                    return false;
                found[k]=true;
            }
        }
        if (!thunkTerminated) return false;
    }
    if (!terminated) return false;
    for (bool hit:found) if (!hit) return false;
    for (std::size_t i=0;i<kExeThreadImportCount;++i)
        for (std::size_t j=0;j<i;++j)
            if (kExeThreadIatRvas[i]==kExeThreadIatRvas[j]) return false;
    return true;
}

bool ReadSlot(const MemoryAccess& memory, std::size_t index,
              std::uint32_t imageSize, std::uint32_t& value) {
    return ReadObject(memory,kExeThreadIatRvas[index],imageSize,value);
}

ExeIatCasResult Win32Cas(std::uint32_t base, std::uint32_t imageSize,
                        std::uint32_t rva, std::uint32_t expected,
                        std::uint32_t desired, std::uint32_t& observed) noexcept {
    if (!InImage(rva,4,imageSize) || (rva&3) ||
        base>std::numeric_limits<std::uint32_t>::max()-rva) return ExeIatCasResult::Failed;
    const auto address=static_cast<std::uintptr_t>(base)+rva;
    LONG old=0;
    __try {
        old=InterlockedCompareExchange(reinterpret_cast<volatile LONG*>(address),
                                       static_cast<LONG>(desired),static_cast<LONG>(expected));
    } __except(EXCEPTION_EXECUTE_HANDLER) {return ExeIatCasResult::Failed;}
    observed=static_cast<std::uint32_t>(old);
    return observed==expected ? ExeIatCasResult::Swapped : ExeIatCasResult::Mismatch;
}

} // namespace

ExeIatBackend MakeWin32ExeThreadIatBackend(
    std::uint32_t imageBase,std::uint32_t imageSize,
    std::function<void(const std::array<std::uint32_t,kExeThreadImportCount>&)> publish) {
    ExeIatBackend backend;
    backend.memory=MakeWin32ImageMemory(imageBase,imageSize);
    backend.resolveKernel32=[](const char* name) -> std::uint32_t {
        const auto module=GetModuleHandleA("kernel32.dll");
        if (!module || !name) return 0;
        const auto ptr=GetProcAddress(module,name);
        return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(ptr));
    };
    backend.publishOriginals=std::move(publish);
    backend.compareExchange=[imageBase,imageSize](std::uint32_t rva,std::uint32_t expected,
        std::uint32_t desired,std::uint32_t& observed) noexcept {
        return Win32Cas(imageBase,imageSize,rva,expected,desired,observed);
    };
    return backend;
}

ExeIatReport ExeThreadIatTransaction::Run(std::uint32_t imageSize,
                                           const ExeIatHooks& hooks,
                                           const ExeIatBackend& backend) {
    ExeIatReport report;
    const auto prior=InterlockedCompareExchange(reinterpret_cast<volatile LONG*>(&gate_),1,0);
    if (prior==1) {report.state=ExeIatState::Busy;report.reason="busy";return report;}
    if (prior==3) {
        report.state=ExeIatState::Poisoned;report.reason="poisoned";
        report.originalsPublished=originalsPublished_;
        report.anyHookExposed=anyHookExposed_;
        return report;
    }
    if (prior==2) {
        const auto locked=InterlockedCompareExchange(reinterpret_cast<volatile LONG*>(&gate_),1,2);
        if (locked!=2) {report.state=ExeIatState::Busy;report.reason="busy";return report;}
    }
    if (prior!=0 && prior!=2) {
        report.state=ExeIatState::Busy;report.reason="invalid or changing gate";return report;
    }
    GateExit gate{&gate_,prior==2 ? 2L : 0L};
    // Any allocation failure from reserve occurs under RAII before a page is
    // opened. Busy/reentrant calls return above without allocator work.
    report.reason.reserve(512);
    report.originalsPublished=originalsPublished_;
    report.anyHookExposed=anyHookExposed_;
    auto reject=[&](const char* reason,ExeIatState state=ExeIatState::Rejected) {
        // Once installed, failure to revalidate any part of the fixed group
        // cannot leave the instance in a trusted installed state.
        if (prior==2) state=ExeIatState::Poisoned;
        report.state=state;report.reason=reason;
        if (state==ExeIatState::Poisoned) gate.next=3;
        return report;
    };
    if (imageSize!=kPinnedImageSize || !backend.memory.read || !backend.memory.beginWrite ||
        !backend.memory.restoreProtection || !backend.resolveKernel32 ||
        !backend.publishOriginals || !backend.compareExchange)
        return reject("unsupported image extent or incomplete IAT backend");
    if (prior==2) {
        // The first install already authenticated PE import names. Under the
        // caller's pinned-image and provider-lifetime prerequisites, hot
        // retries only check cached identities and current aligned DWORDs.
        // Any failure poisons the group; no installed retry modifies it.
        if (!originalsPublished_ || !anyHookExposed_ || !SameHooks(hooks,installedHooks_))
            return reject("installed group has missing publication or changed hooks");
        for (std::size_t i=0;i<kExeThreadImportCount;++i) {
            const auto provider=backend.resolveKernel32(kExeThreadImportNames[i]);
            std::uint32_t current=0;
            if (!provider || provider!=originals_[i] ||
                !ReadSlot(backend.memory,i,imageSize,current) ||
                current!=installedHooks_.pointers[i]) {
                report.failedIndex=i;
                return reject("installed provider or IAT slot changed or unreadable");
            }
        }
        report.state=ExeIatState::AlreadyInstalled;
        report.reason="all four installed slots and providers verified";
        report.hookObservedBeforeRecovery=kExeThreadImportCount;
        return report;
    }
    if (!ResolvePinnedSlots(backend.memory,imageSize))
        return reject("pinned kernel32 import-by-name quartet mismatch");
    std::array<std::uint32_t,kExeThreadImportCount> providers{};
    std::array<std::uint32_t,kExeThreadImportCount> current{};
    for (std::size_t i=0;i<kExeThreadImportCount;++i) {
        providers[i]=backend.resolveKernel32(kExeThreadImportNames[i]);
        if (!providers[i] || !hooks.pointers[i] || hooks.pointers[i]==providers[i] ||
            !ReadSlot(backend.memory,i,imageSize,current[i])) {
            report.failedIndex=i;
            return reject("provider/hook target unavailable or slot unreadable");
        }
        for (std::size_t j=0;j<i;++j)
            if (hooks.pointers[i]==hooks.pointers[j]) {
                report.failedIndex=i;return reject("duplicate hook target");
            }
    }
    if (anyHookExposed_ && providers!=originals_)
        return reject("provider changed after hook exposure",ExeIatState::Poisoned);
    for (std::size_t i=0;i<kExeThreadImportCount;++i) {
        if (current[i]!=providers[i]) {
            report.failedIndex=i;
            return reject("foreign or already-hooked IAT slot before mutation",
                          anyHookExposed_ ? ExeIatState::Poisoned : ExeIatState::Rejected);
        }
    }
    // No target hook can run yet. Never replace originals after exposure;
    // retries require the same live GetProcAddress provider values.
    if (!originalsPublished_ || !anyHookExposed_) {
        originals_=providers;
        backend.publishOriginals(originals_);
        originalsPublished_=true;
    }
    report.originalsPublished=true;
    std::array<SlotJournal,kExeThreadImportCount> journal{};
    std::size_t failed=kExeThreadImportCount;
    const char* failure=nullptr;
    for (std::size_t i=0;i<kExeThreadImportCount;++i) {
        if (!backend.memory.beginWrite(kExeThreadIatRvas[i],4,journal[i].originalProtection)) {
            failed=i;failure="IAT protection acquisition failed";break;
        }
        journal[i].protectionOpen=true;
    }
    if (!failure) {
        for (std::size_t i=0;i<kExeThreadImportCount;++i) {
            auto& slot=journal[i];
            slot.attempted=true;
            ++report.attemptedSlots;
            std::uint32_t observed=0;
            const auto outcome=backend.compareExchange(kExeThreadIatRvas[i],providers[i],
                hooks.pointers[i],observed);
            // Failed may be reported after a complete exchange. Mismatch
            // may also observe an already-installed hook. In either case,
            // a failed immediate readback cannot disprove prior exposure.
            if (outcome==ExeIatCasResult::Swapped ||
                outcome==ExeIatCasResult::Failed ||
                (outcome==ExeIatCasResult::Mismatch && observed==hooks.pointers[i])) {
                slot.exposed=true;anyHookExposed_=true;report.anyHookExposed=true;
            }
            std::uint32_t after=0;
            if (!ReadSlot(backend.memory,i,imageSize,after)) {
                failed=i;failure="IAT slot readback failed";break;
            }
            if (after==hooks.pointers[i]) {
                slot.exposed=true;anyHookExposed_=true;report.anyHookExposed=true;
                ++report.hookObservedBeforeRecovery;
            }
            if (outcome!=ExeIatCasResult::Swapped || after!=hooks.pointers[i]) {
                failed=i;failure=outcome==ExeIatCasResult::Mismatch ?
                    "IAT CAS mismatch or foreign clobber" : "IAT CAS failed or readback mismatched";
                break;
            }
        }
    }
    if (!failure) {
        for (std::size_t n=kExeThreadImportCount;n>0;--n) {
            const auto i=n-1;
            if (!backend.memory.restoreProtection(kExeThreadIatRvas[i],4,
                                                   journal[i].originalProtection)) {
                failed=i;failure="final IAT protection restoration failed";break;
            }
            journal[i].protectionOpen=false;
        }
    }
    if (!failure) {
        installedHooks_=hooks;
        gate.next=2;
        report.state=ExeIatState::Applied;
        report.reason="all four imports installed and verified";
        return report;
    }

    // Every attempted slot is inspected and restored in reverse; a CAS
    // mismatch never overwrites a foreign pointer. Reacquire because some
    // original protection tokens may already have been restored.
    bool recovered=true;
    for (std::size_t n=kExeThreadImportCount;n>0;--n) {
        const auto i=n-1;
        auto& slot=journal[i];
        if (!slot.attempted) continue;
        std::uintptr_t rollbackToken=0;
        if (!backend.memory.beginWrite(kExeThreadIatRvas[i],4,rollbackToken)) {
            slot.protectionRecovered=false;
            slot.byteRecovered=false;
        } else {
            std::uint32_t at=0;
            if (!ReadSlot(backend.memory,i,imageSize,at)) slot.byteRecovered=false;
            else if (at==hooks.pointers[i]) {
                std::uint32_t observed=0;
                const auto restored=backend.compareExchange(kExeThreadIatRvas[i],
                    hooks.pointers[i],providers[i],observed);
                if (restored!=ExeIatCasResult::Swapped) slot.byteRecovered=false;
            } else if (at!=providers[i]) slot.byteRecovered=false;
            std::uint32_t check=0;
            if (!ReadSlot(backend.memory,i,imageSize,check) || check!=providers[i])
                slot.byteRecovered=false;
            else if (slot.byteRecovered) ++report.originalVerifiedAfterRecovery;
            if (!backend.memory.restoreProtection(kExeThreadIatRvas[i],4,rollbackToken))
                slot.protectionRecovered=false;
        }
    }
    for (std::size_t n=kExeThreadImportCount;n>0;--n) {
        const auto i=n-1;
        auto& slot=journal[i];
        if (!slot.protectionOpen) continue;
        if (!backend.memory.restoreProtection(kExeThreadIatRvas[i],4,slot.originalProtection))
            slot.protectionRecovered=false;
        else slot.protectionOpen=false;
    }
    for (std::size_t i=0;i<kExeThreadImportCount;++i) {
        std::uint32_t at=0;
        if (!ReadSlot(backend.memory,i,imageSize,at) || at!=providers[i] ||
            journal[i].protectionOpen || !journal[i].protectionRecovered ||
            !journal[i].byteRecovered) recovered=false;
    }
    report.failedIndex=failed;
    report.reason.assign(kExeThreadImportNames[failed]);
    report.reason.append(": ");
    report.reason.append(failure);
    report.reason.append(recovered ? "; all slots restored" : "; recovery incomplete");
    if (recovered) {
        report.state=ExeIatState::RolledBack;
        gate.next=0;
    } else {
        report.state=ExeIatState::RollbackFailed;
        gate.next=3;
    }
    return report;
}

} // namespace fork_boundary
