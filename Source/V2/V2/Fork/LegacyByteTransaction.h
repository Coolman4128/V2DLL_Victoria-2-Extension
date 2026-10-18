#pragma once

#include "AddressProfile.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace fork_boundary {

// Intentional bounded byte edits of the authenticated current image. A site
// may cover only an opcode or operand inside an instruction; it is therefore
// deliberately separate from AddressProfile's whole-instruction PatchKind.
// The caller proves executable identity, native semantics, safe phase and
// resolves file offsets to RVAs before invoking this transaction.
struct LegacyByteSite {
    std::string name;
    std::uint32_t rva = 0;
    std::vector<unsigned char> expected;
    std::vector<unsigned char> replacement;
    bool enabled = false;
};

enum class LegacyByteState { Rejected, Skipped, Applied, RolledBack, RollbackFailed };
enum class LegacyByteSiteState { Skipped, Rejected, Validated, Applied, RolledBack, RollbackFailed };

struct LegacyByteSiteReport {
    std::string name;
    LegacyByteSiteState state = LegacyByteSiteState::Skipped;
    std::string reason;
};

struct LegacyByteReport {
    LegacyByteState state = LegacyByteState::Rejected;
    std::string reason;
    std::size_t enabledCount = 0;
    std::size_t skippedCount = 0;
    std::vector<LegacyByteSiteReport> sites;
};

constexpr std::size_t kLegacyByteMaxSites = 128;
constexpr std::size_t kLegacyByteMaxLength = 64;
constexpr std::size_t kLegacyByteMaxNameLength = 96;

// Validates every enabled site before any protection change. The backend's
// synchronous callbacks must not throw and must follow MemoryAccess's partial
// write and beginWrite-failure contracts. No concurrent image mutation is
// permitted. All scratch/report storage is allocated before protection opens;
// target callbacks are never used for logging or allocation. On any mutation
// failure, every touched site (including a failing partial write) is attempted
// in reverse, read back and flushed, and all held protection tokens are
// restored in reverse. Incomplete byte OR protection recovery is reported as
// RollbackFailed, never as clean RolledBack.
LegacyByteReport InstallLegacyByteTable(const std::vector<LegacyByteSite>& sites,
                                        std::uint32_t imageSize,
                                        const MemoryAccess& memory);

} // namespace fork_boundary
