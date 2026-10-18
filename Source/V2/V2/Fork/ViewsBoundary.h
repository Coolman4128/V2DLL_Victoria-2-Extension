#pragma once

#include "AddressProfile.h"
#include <array>

namespace fork_boundary {

constexpr std::size_t ViewSlotCount = 4;
// Source inventory order: technology/update, budget/tooltip,
// production/tooltip, politics/tooltip. These are leads, not a supported profile.
constexpr std::array<std::uint32_t, ViewSlotCount> ViewSlotRvas = {
    0xA17FD0, 0xA05A18, 0xA0FEF4, 0xA0E480
};

// The optional legacy family is unavailable on every current profile (R02b).
// Reserve its four PE32 pointer spans even for aliases through shared PatchSlot.
// This is a deny rule, not executable/phase authentication or a plugin sandbox.
bool BlocksLegacyViewsSlot(std::uint32_t rvaVtable, int slotIndex);
bool BlocksLegacyViewsWrite(std::uint32_t rva, std::uint32_t size);

struct ViewSlotEvidence {
    // Explicit reviewed target and code dependency; never learned from the
    // current slot's contents. Entry must be validation-only Instructions,
    // start at targetRva and carry the reviewed native callback contract.
    std::uint32_t targetRva = 0;
    PatchSite entry;
};

struct ViewsEvidence {
    std::array<ViewSlotEvidence, ViewSlotCount> slots;
    // All additional native reads/calls, including dynamic element-vtable
    // dependencies, must be accounted for by the reviewed closure evidence.
    // Dependency spans must be disjoint from each other and the four entries;
    // conflicting/duplicate spans fail closed rather than weakening validation.
    // This adapter cannot discover omitted dependencies or prove ABI/lifetime.
    std::vector<PatchSite> readDependencies;
    std::string closureEvidence;
};

// Pure transaction adapter with no production profile or call site. The complete
// active DLL runtime gate remains a prerequisite to any future wiring. R02b's
// separate legacy deny rule above does not authorize this transaction seam.
// loadedBase and replacement pointers describe PE32 mapped addresses. Only a
// completely successful transaction publishes originals; every failure leaves
// the caller's output array untouched. Caller owns a proved exclusive phase.
InstallReport InstallViewsFamily(const ExecutableIdentity& actual,
    const AddressProfile& profile, std::uint32_t loadedBase,
    const ViewsEvidence& evidence,
    const std::array<std::uint32_t, ViewSlotCount>& replacements,
    std::array<std::uint32_t, ViewSlotCount>& originals,
    const MemoryAccess& memory);

} // namespace fork_boundary
