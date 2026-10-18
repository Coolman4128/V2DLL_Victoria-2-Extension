#pragma once

#include "AddressProfile.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>

namespace fork_boundary {

constexpr std::uint32_t kPinnedTbbImageSize = 0x26000;
constexpr std::uint32_t kPinnedTbbBeginThreadExIatRva = 0x18144;

struct TbbImportExpectation {
    std::uint32_t imageSize = 0;
    std::uint32_t timeDateStamp = 0;
    std::uint32_t preferredBase = 0;
    std::uint32_t textRva = 0;
    std::uint32_t textVirtualSize = 0;
    std::uint32_t textRawSize = 0;
    std::uint32_t beginThreadExIatRva = 0;
    std::uint64_t diskSize = 0;
    std::array<unsigned char, 32> diskSha256{};
    std::array<unsigned char, 32> normalizedTextSha256{};
    std::uint32_t crtImageSize = 0;
    std::uint32_t crtTimeDateStamp = 0;
    std::uint32_t crtBeginThreadExExportRva = 0;
    std::uint32_t crtBeginThreadExOrdinal = 0;
    std::uint64_t crtDiskSize = 0;
    std::array<unsigned char, 32> crtDiskSha256{};
    const std::uint16_t* textRelocationDeltas = nullptr;
    std::size_t textRelocationCount = 0;
};

const TbbImportExpectation& PinnedTbbImportExpectation() noexcept;

enum class TbbIatCasResult { Swapped, Mismatch, Failed };
enum class TbbIatState {
    Skipped, Rejected, Busy, Applied, AlreadyInstalled, RolledBack,
    RollbackFailed, Poisoned
};

struct TbbIatReport {
    TbbIatState state = TbbIatState::Rejected;
    std::string reason;
    bool tbbReferenceRetained = false;
    bool providerReferenceRetained = false;
    bool originalPublished = false;
    bool hookPossiblyExposed = false; // historical, including uncertain CAS outcome
    bool createThreadImportSkipped = false; // authenticated pinned TBB has no named import
    bool attempted = false;
    bool hookObservedBeforeRecovery = false;
    bool originalVerifiedAfterRecovery = false;
};

// All callbacks are synchronous and nonthrowing. retainFromAddress takes a
// real module reference via GetModuleHandleEx(FROM_ADDRESS), returning its
// base; the transaction retains at most one TBB and one resolved-provider
// reference for process lifetime. It never calls FreeLibrary in a loader
// callback. A retained wrong module/provider identity terminally poisons this
// instance; no third reference is acquired on retry. The production path uses real mapped MEM_IMAGE; owned tests can
// inject an equivalent bounded image and a private surrogate reference.
// publishOriginal must make the provider pointer visible to the wrapper
// before any hook CAS, with release ordering. Failed CAS can occur before or
// after a complete aligned DWORD exchange; observed is valid for Mismatch.
struct TbbImportBackend {
    MemoryAccess memory;
    std::function<bool(std::uint32_t address, std::uint32_t& retainedBase)> retainFromAddress;
    std::function<bool(std::uint32_t retainedBase, wchar_t* path,
                       std::size_t capacity)> modulePath;
    std::function<bool(std::uint32_t base, std::uint32_t imageSize)> isMappedImage;
    // Called only after the current IAT pointer's owner has been retained
    // and authenticated as the pinned concrete MSVCR100 export module.
    std::function<std::uint32_t(std::uint32_t authenticatedCrtBase)> resolveBeginThreadEx;
    std::function<bool(std::uint32_t base, std::uint32_t rva,
                       unsigned char* output, std::size_t count)> readProvider;
    std::function<void(std::uint32_t)> publishOriginal;
    std::function<TbbIatCasResult(std::uint32_t rva, std::uint32_t expected,
                                  std::uint32_t desired, std::uint32_t& observed)> compareExchange;
};

// Caller must independently authenticate the main EXE/profile and an
// exclusive safe install phase. The mapped TBB and resolved provider must
// remain loaded while wrappers may be in flight. A retained reference pins
// each module, but this transaction does not establish game-thread phase or
// make the whole process installation atomic. No TBB initialize/cap hook is
// managed here; ENGINE_WORKER_THREADS=0 remains the caller's off setting.
class TbbImportTransaction {
public:
    TbbImportTransaction() = default;
    TbbImportTransaction(const TbbImportTransaction&) = delete;
    TbbImportTransaction& operator=(const TbbImportTransaction&) = delete;

    TbbIatReport Run(std::uint32_t candidateTbbBase, std::uint32_t hook,
                     const TbbImportBackend& backend,
                     const TbbImportExpectation& expected);

private:
    volatile long gate_ = 0; // idle, busy, installed, poisoned
    std::uint32_t retainedTbbBase_ = 0;
    std::uint32_t retainedProviderBase_ = 0;
    std::uint32_t original_ = 0;
    std::uint32_t installedHook_ = 0;
    bool originalPublished_ = false;
    bool hookPossiblyExposed_ = false;
};

// Uses MakeWin32ImageMemory, GetModuleHandleEx(FROM_ADDRESS), module path and
// MEM_IMAGE queries, loaded pinned MSVCR100 GetProcAddress (concrete export),
// and aligned InterlockedCompareExchange. Does not force-load TBB or the CRT.
TbbImportBackend MakeWin32TbbImportBackend(
    std::uint32_t candidateTbbBase, std::uint32_t imageSize,
    std::function<void(std::uint32_t)> publishOriginal);

} // namespace fork_boundary
