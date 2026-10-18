#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

// A pure installation boundary, not an instruction decoder or native ABI proof.
// Evidence fields are reviewable caller assertions. Populate production profiles
// only from independently verified executable/ABI evidence, never live bytes.
namespace fork_boundary {

struct ExecutableIdentity {
    std::string sha256;
    std::uint16_t machine = 0;
    std::uint32_t imageSize = 0;
};

enum class PatchKind { Data, Instructions, RawTrampoline };

struct NativeContract {
    std::string callingConvention;
    std::string registerAndStackPreservation;
    std::string fpuPreservation;
    std::string lifetimeAndPhase;
    std::string evidence;
};

struct PatchSite {
    std::string name;
    std::uint32_t rva = 0;
    std::vector<unsigned char> expected;
    std::vector<unsigned char> replacement;
    // Callable targets, resume boundaries and vtable observations can be
    // validation-only dependencies. They must have an empty replacement and
    // never acquire write protection, write, or flush.
    bool validationOnly = false;
    PatchKind kind = PatchKind::Data;
    // Exact whole-instruction lengths covering expected bytes for code sites.
    std::vector<unsigned char> instructionLengths;
    // Raw copying of PC-relative instructions is never permitted here.
    bool stolenInstructionsContainRelativeAddressing = false;
    // This module bounds-checks resumeRva but does not decode/prove its target.
    // Caller must include a separate validation-only expected-byte dependency
    // at each resume/call/vtable boundary required by the reviewed evidence.
    std::uint32_t resumeRva = 0;
    NativeContract contract;
};

struct AddressProfile {
    std::string name;
    ExecutableIdentity executable;
    std::string evidence;
};

struct FeaturePlan {
    std::string name;
    std::vector<PatchSite> dependencies;
};

// All callbacks run synchronously on the caller's verified exclusive safe phase.
// No process APIs or executable discovery live in this module. Callbacks must not
// throw. read succeeds only on a complete read; write may partially change memory
// even when returning false. beginWrite failure MUST leave protection unchanged.
// Tokens belong to the backend and restoreProtection must support the matching
// original region/token. No concurrent mutation is allowed during this call.
struct MemoryAccess {
    std::function<bool(std::uint32_t, unsigned char*, std::size_t)> read;
    std::function<bool(std::uint32_t, std::size_t, std::uintptr_t&)> beginWrite;
    std::function<bool(std::uint32_t, const unsigned char*, std::size_t)> write;
    std::function<bool(std::uint32_t, std::size_t, std::uintptr_t)> restoreProtection;
    std::function<bool(std::uint32_t, std::size_t)> flushInstructions;
};

enum class SiteState { Skipped, Validated, Applied, RolledBack, RollbackFailed };
struct SiteReport {
    std::string name;
    SiteState state = SiteState::Skipped;
    std::string reason;
};
enum class InstallState { Rejected, Validated, Applied, RolledBack, RollbackFailed };
struct InstallReport {
    std::string feature;
    InstallState state = InstallState::Rejected;
    std::string reason;
    std::vector<SiteReport> sites;
};

// Validates the ENTIRE feature before any write/protection callback is invoked.
// Unknown executable/profile or any invalid dependency rejects the whole feature.
// A failed write is journaled before invocation and is included in rollback.
// An all-validation-only feature returns Validated and requires only read.
InstallReport InstallFeature(const ExecutableIdentity& actual,
                             const AddressProfile& profile,
                             const FeaturePlan& feature,
                             const MemoryAccess& memory);

} // namespace fork_boundary
