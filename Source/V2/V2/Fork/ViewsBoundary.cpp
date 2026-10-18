#include "ViewsBoundary.h"
#include <limits>

namespace fork_boundary {

bool BlocksLegacyViewsSlot(std::uint32_t rvaVtable, int slotIndex) {
    // Match the legacy Win32 address arithmetic without dereferencing an alias.
    const auto target = rvaVtable + static_cast<std::uint32_t>(slotIndex) * 4u;
    return BlocksLegacyViewsWrite(target, 4);
}

bool BlocksLegacyViewsWrite(std::uint32_t rva, std::uint32_t size) {
    if (size == 0) return false;
    for (const auto reserved : ViewSlotRvas) {
        // Unsigned distances also cover a PE32 interval wrapping through zero.
        if (reserved - rva < size || rva - reserved < 4u)
            return true;
    }
    return false;
}

namespace {
std::vector<unsigned char> PointerBytes(std::uint32_t pointer) {
    return {static_cast<unsigned char>(pointer), static_cast<unsigned char>(pointer >> 8),
        static_cast<unsigned char>(pointer >> 16), static_cast<unsigned char>(pointer >> 24)};
}
InstallReport Unavailable(const char* reason) {
    InstallReport report;
    report.feature = "four VIEWS callbacks";
    report.reason = reason;
    return report;
}
}

InstallReport InstallViewsFamily(const ExecutableIdentity& actual,
    const AddressProfile& profile, std::uint32_t loadedBase,
    const ViewsEvidence& evidence,
    const std::array<std::uint32_t, ViewSlotCount>& replacements,
    std::array<std::uint32_t, ViewSlotCount>& originals,
    const MemoryAccess& memory) {
    if (loadedBase == 0 || profile.executable.imageSize == 0 ||
        profile.executable.imageSize - 1 > std::numeric_limits<std::uint32_t>::max() - loadedBase)
        return Unavailable("feature unavailable: invalid PE32 mapped image extent");
    if (evidence.closureEvidence.empty() || evidence.readDependencies.empty())
        return Unavailable("feature unavailable: native read/call closure evidence missing");

    FeaturePlan plan;
    plan.name = "four VIEWS callbacks";
    std::array<std::uint32_t, ViewSlotCount> pendingOriginals{};
    for (std::size_t i = 0; i < ViewSlotCount; ++i) {
        const auto& proof = evidence.slots[i];
        if (replacements[i] == 0 || proof.targetRva == 0 ||
            proof.targetRva >= profile.executable.imageSize ||
            !proof.entry.validationOnly || proof.entry.kind != PatchKind::Instructions ||
            proof.entry.rva != proof.targetRva)
            return Unavailable("feature unavailable: incomplete VIEWS target/entry evidence");
        pendingOriginals[i] = loadedBase + proof.targetRva;
        if (replacements[i] == pendingOriginals[i])
            return Unavailable("feature unavailable: callback replacement equals native target");
        PatchSite slot;
        slot.name = "VIEWS slot " + std::to_string(i);
        slot.rva = ViewSlotRvas[i];
        slot.expected = PointerBytes(pendingOriginals[i]);
        slot.replacement = PointerBytes(replacements[i]);
        slot.contract = proof.entry.contract;
        plan.dependencies.push_back(slot);
        plan.dependencies.push_back(proof.entry);
    }
    for (const auto& dependency : evidence.readDependencies) {
        if (!dependency.validationOnly)
            return Unavailable("feature unavailable: dependent reads must be validation-only");
        plan.dependencies.push_back(dependency);
    }
    auto report = InstallFeature(actual, profile, plan, memory);
    if (report.state == InstallState::Applied) originals = pendingOriginals;
    return report;
}
} // namespace fork_boundary
