#pragma once

#include "StartupIdentity.h"

namespace fork_startup {

constexpr std::uint64_t KnownExecutableSize = 12294656;
constexpr std::uint32_t KnownImageSize = 0x1092000;

// Authorized is reserved for a future independently completed active profile.
// No current production path returns it. Positive preflight remains Withheld.
enum class StartupState { Rejected, Withheld, Authorized };

struct StartupDecision {
    StartupState state;
    const char* reason;
    std::uint32_t failedRva = 0;
    const char* failedSite = nullptr;
};

struct StartupFacts {
    bool staticLoad = false;
    bool proxyRoleMatches = false;
    std::uint32_t mappedBase = 0;
    IdentityReadStatus identityStatus = IdentityReadStatus::PathUnavailable;
    FileIdentity executable{};
};

struct StartupSite {
    const char* name;
    std::uint32_t rva;
    unsigned size;
    unsigned char expected[16];
    // A reviewed HIGHLOW operand is materialized from mappedBase, never learned
    // from current memory. -1 means the span has no absolute relocation.
    int relocationOffset;
    std::uint32_t absoluteTargetRva;
};

using StartupRead = bool (*)(void* context, std::uint32_t rva,
    unsigned char* output, std::uint32_t size);

const StartupSite* StartupSites(std::size_t& count);
const unsigned char* KnownExecutableSha256();
StartupDecision CheckStartup(const StartupFacts& facts,
    StartupRead read, void* context);

// Fixed-buffer, read-only Kernel32 capture. Static load is a loader fact only.
// Matching disk/mapped checks never authorize native installation by themselves.
StartupDecision CheckProductionStartup(void* self, const void* reserved);
// Does not enter existing logging/config/handler initialization on rejection.
void ReportStartupDecision(const StartupDecision& decision);

} // namespace fork_startup
