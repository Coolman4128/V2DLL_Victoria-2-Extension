#pragma once

#include "AddressProfile.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>

namespace fork_boundary {

// Fixed order is the existing EXE installation order. These are mapped-image
// RVAs, not file offsets. Each slot is a four-byte aligned PE32 IAT pointer.
enum class ExeThreadImport : std::size_t {
    CreateThread = 0, LoadLibraryA = 1, LoadLibraryW = 2, GetTickCount = 3
};
constexpr std::size_t kExeThreadImportCount = 4;
constexpr std::array<std::uint32_t, kExeThreadImportCount> kExeThreadIatRvas = {
    0x88A1A8, 0x88A1F8, 0x88A12C, 0x88A128
};
constexpr std::array<const char*, kExeThreadImportCount> kExeThreadImportNames = {
    "CreateThread", "LoadLibraryA", "LoadLibraryW", "GetTickCount"
};

enum class ExeIatCasResult { Swapped, Mismatch, Failed };
enum class ExeIatState {
    Rejected, Busy, Applied, AlreadyInstalled, RolledBack, RollbackFailed, Poisoned
};

struct ExeIatReport {
    ExeIatState state = ExeIatState::Rejected;
    std::string reason;
    bool originalsPublished = false;
    bool anyHookExposed = false; // historically observed or possibly exposed
    std::size_t attemptedSlots = 0;
    std::size_t hookObservedBeforeRecovery = 0; // this Run's hook readbacks; not final installed count
    std::size_t originalVerifiedAfterRecovery = 0; // attempted rows verified original during rollback; may never have swapped
    std::size_t failedIndex = kExeThreadImportCount;
};

// The callback backend is synchronous/noexcept. MemoryAccess.read must be a
// complete read; beginWrite failure leaves protection unchanged. The CAS
// callback atomically compares one aligned DWORD and never tears a pointer.
// It reports the prior DWORD in observed. Failed may mean failure before OR
// after a complete exchange; the transaction re-reads and journals the slot.
// publishOriginals MUST synchronously publish all four provider pointers to
// the existing wrapper globals before returning, with release ordering. It
// must not invoke target code or throw. It is never called to clear originals.
struct ExeIatBackend {
    MemoryAccess memory;
    std::function<std::uint32_t(const char* name)> resolveKernel32;
    std::function<void(const std::array<std::uint32_t, kExeThreadImportCount>&)> publishOriginals;
    std::function<ExeIatCasResult(std::uint32_t rva, std::uint32_t expected,
                                   std::uint32_t desired, std::uint32_t& observed)> compareExchange;
};

struct ExeIatHooks {
    std::array<std::uint32_t, kExeThreadImportCount> pointers{};
};

// Caller prerequisites: authenticate the pinned main EXE and mapped MEM_IMAGE
// extent, ensure the providers and wrappers live for the EXE lifetime, and
// call only in a verified exclusive installation phase. PE import metadata
// and MemoryAccess do not establish those external facts.
// One instance is retained for the EXE lifetime. Concurrent/reentrant Run
// returns Busy immediately; no lock waiting occurs under loader callbacks.
// First install and clean-rollback retries parse pinned PE import metadata;
// installed retries verify only cached provider identity and four live IAT
// DWORDs under the caller's stable mapped-image/provider prerequisites.
// Poisoned is terminal after incomplete byte/protection recovery. Published
// providers remain valid after any hook exposure, even a clean rollback.
class ExeThreadIatTransaction {
public:
    ExeThreadIatTransaction() = default;
    ExeThreadIatTransaction(const ExeThreadIatTransaction&) = delete;
    ExeThreadIatTransaction& operator=(const ExeThreadIatTransaction&) = delete;

    ExeIatReport Run(std::uint32_t imageSize, const ExeIatHooks& hooks,
                     const ExeIatBackend& backend);

private:
    volatile long gate_ = 0; // 0 idle, 1 busy, 2 installed, 3 poisoned
    bool originalsPublished_ = false;
    bool anyHookExposed_ = false;
    std::array<std::uint32_t, kExeThreadImportCount> originals_{};
    ExeIatHooks installedHooks_{};
};

// Production adapter: bounded current-process image reads/protection through
// MakeWin32ImageMemory, GetProcAddress(kernel32) including legitimate export
// forwarding, and aligned InterlockedCompareExchange on the mapped IAT DWORD.
// The caller supplies the publication callback for its existing wrapper globals.
ExeIatBackend MakeWin32ExeThreadIatBackend(
    std::uint32_t imageBase, std::uint32_t imageSize,
    std::function<void(const std::array<std::uint32_t, kExeThreadImportCount>&)> publishOriginals);

} // namespace fork_boundary
