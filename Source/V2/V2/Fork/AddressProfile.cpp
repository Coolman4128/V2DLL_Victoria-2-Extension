#include "AddressProfile.h"

#include <algorithm>

namespace fork_boundary {
namespace {
bool ValidIdentity(const ExecutableIdentity& id) {
    return id.sha256.size() == 64 &&
        std::all_of(id.sha256.begin(), id.sha256.end(), [](char c) {
            return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
        }) && id.machine == 0x14c && id.imageSize != 0;
}
bool SameIdentity(const ExecutableIdentity& a, const ExecutableIdentity& b) {
    return a.sha256 == b.sha256 && a.machine == b.machine && a.imageSize == b.imageSize;
}
bool CompleteContract(const NativeContract& c) {
    return !c.callingConvention.empty() && !c.registerAndStackPreservation.empty() &&
        !c.fpuPreservation.empty() && !c.lifetimeAndPhase.empty() && !c.evidence.empty();
}
std::string InvalidSite(const PatchSite& s, const AddressProfile& p) {
    if (s.name.empty()) return "missing site name";
    if (s.expected.empty()) return "expected bytes are required (nullable expect rejected)";
    if (s.validationOnly && !s.replacement.empty()) return "validation-only dependency has replacement bytes";
    if (!s.validationOnly && s.replacement.size() != s.expected.size()) return "replacement/expected size mismatch";
    if (s.rva >= p.executable.imageSize || s.expected.size() > p.executable.imageSize - s.rva)
        return "site outside image";
    if (!CompleteContract(s.contract)) return "missing native contract/evidence";
    if (s.kind != PatchKind::Data) {
        std::size_t covered = 0;
        for (auto length : s.instructionLengths) {
            if (length == 0 || length > 15 || covered > s.expected.size() ||
                length > s.expected.size() - covered) return "invalid instruction boundaries";
            covered += length;
        }
        if (covered != s.expected.size()) return "incomplete instruction boundaries";
        if (s.resumeRva >= p.executable.imageSize || s.resumeRva == 0)
            return "missing/out-of-image resume target";
        if (s.resumeRva >= s.rva && s.resumeRva < s.rva + s.expected.size())
            return "resume target inside overwritten instructions";
    }
    if (s.kind == PatchKind::RawTrampoline && s.stolenInstructionsContainRelativeAddressing)
        return "relative stolen instruction requires a verified relocating backend";
    return {};
}
struct Journal {
    std::vector<unsigned char> original;
    std::vector<unsigned char> check;
    std::uintptr_t token = 0;
    bool open = false;
    bool touched = false;
    bool rollbackOk = true;
};
} // namespace

InstallReport InstallFeature(const ExecutableIdentity& actual, const AddressProfile& profile,
                            const FeaturePlan& feature, const MemoryAccess& memory) {
    InstallReport report;
    report.feature = feature.name;
    for (const auto& site : feature.dependencies) report.sites.push_back({site.name, SiteState::Skipped, {}});
    auto reject = [&](const std::string& why) {
        report.reason = why;
        for (auto& site : report.sites) site.reason = "feature unavailable: " + why;
        return report;
    };
    if (profile.name.empty() || profile.evidence.empty() || !ValidIdentity(profile.executable) ||
        !ValidIdentity(actual) || !SameIdentity(actual, profile.executable))
        return reject("unknown executable or unsupported address profile");
    if (feature.name.empty() || feature.dependencies.empty()) return reject("empty feature/dependency set");
    const bool hasWrites = std::any_of(feature.dependencies.begin(), feature.dependencies.end(),
        [](const PatchSite& site) { return !site.validationOnly; });
    if (!memory.read || (hasWrites && (!memory.beginWrite || !memory.write || !memory.restoreProtection || !memory.flushInstructions)))
        return reject("incomplete memory backend");
    std::vector<Journal> journal(feature.dependencies.size());
    for (std::size_t i = 0; i < feature.dependencies.size(); ++i) {
        const auto& site = feature.dependencies[i];
        auto invalid = InvalidSite(site, profile);
        if (!invalid.empty()) return reject(site.name + ": " + invalid);
        for (std::size_t j = 0; j < i; ++j) {
            const auto& prior = feature.dependencies[j];
            if (prior.name == site.name) return reject("duplicate site name: " + site.name);
            if (static_cast<std::uint64_t>(site.rva) < static_cast<std::uint64_t>(prior.rva) + prior.expected.size() &&
                static_cast<std::uint64_t>(prior.rva) < static_cast<std::uint64_t>(site.rva) + site.expected.size())
                return reject("overlapping dependencies: " + prior.name + "/" + site.name);
        }
        journal[i].original.resize(site.expected.size());
        journal[i].check.resize(site.expected.size());
        if (!memory.read(site.rva, journal[i].original.data(), site.expected.size()))
            return reject(site.name + ": read failed");
        if (journal[i].original != site.expected) return reject(site.name + ": signature mismatch");
    }
    if (!hasWrites) {
        report.state = InstallState::Validated;
        report.reason = "all dependencies validated; no writes requested";
        for (auto& site : report.sites) { site.state = SiteState::Validated; site.reason = "validated only"; }
        return report;
    }
    auto rollback = [&](std::size_t failedSite, const char* why) {
        bool complete = true;
        for (std::size_t n = journal.size(); n > 0; --n) {
            const auto i = n - 1;
            auto& entry = journal[i];
            const auto& site = feature.dependencies[i];
            bool ok = true;
            if (entry.touched) {
                // Reacquire even after a failed protection restore: its resulting
                // protection cannot be assumed writable.
                std::uintptr_t rollbackToken = 0;
                if (!memory.beginWrite(site.rva, site.expected.size(), rollbackToken)) ok = false;
                else {
                    const bool wrote = memory.write(site.rva, entry.original.data(), entry.original.size());
                    const bool flushed = memory.flushInstructions(site.rva, entry.original.size());
                    const bool restored = memory.restoreProtection(site.rva, entry.original.size(), rollbackToken);
                    ok = wrote && flushed && restored;
                    const bool read = memory.read(site.rva, entry.check.data(), entry.check.size());
                    ok = ok && read && entry.check == entry.original;
                }
            }
            if (entry.open) {
                const bool restored = memory.restoreProtection(site.rva, site.expected.size(), entry.token);
                ok = restored && ok;
            }
            complete = complete && ok;
            entry.rollbackOk = ok;
        }
        // Complete recovery for EVERY region before constructing report strings.
        // All scratch buffers were allocated before any protection changes.
        report.reason = feature.dependencies[failedSite].name + ": " + why;
        for (std::size_t i = 0; i < journal.size(); ++i) {
            const auto& entry = journal[i];
            const bool ok = entry.rollbackOk;
            report.sites[i].state = !ok ? SiteState::RollbackFailed :
                (entry.touched ? SiteState::RolledBack : SiteState::Skipped);
            report.sites[i].reason = (ok ? "feature unavailable: " : "rollback/protection restoration failed: ") + report.reason;
        }
        report.state = complete ? InstallState::RolledBack : InstallState::RollbackFailed;
        return report;
    };
    // Prepare success strings before any mutation, too. After protection opens,
    // the transaction/recovery paths use only already allocated storage until
    // all memory/protection operations have completed.
    report.reason = "all dependencies validated and applied";
    for (std::size_t i = 0; i < report.sites.size(); ++i)
        report.sites[i].reason = feature.dependencies[i].validationOnly ? "validated only" : "applied";
    // Obtain every region before writing. Separate sites can share protection
    // pages; restore their tokens in reverse acquisition order.
    for (std::size_t i = 0; i < journal.size(); ++i) {
        const auto& site = feature.dependencies[i];
        if (site.validationOnly) continue;
        if (!memory.beginWrite(site.rva, site.expected.size(), journal[i].token))
            return rollback(i, "write protection acquisition failed");
        journal[i].open = true;
    }
    for (std::size_t i = 0; i < journal.size(); ++i) {
        const auto& site = feature.dependencies[i];
        if (site.validationOnly) continue;
        journal[i].touched = true;
        if (!memory.write(site.rva, site.replacement.data(), site.replacement.size()))
            return rollback(i, "write failed (possibly partial)");
        auto& check = journal[i].check;
        if (!memory.read(site.rva, check.data(), check.size()) || check != site.replacement)
            return rollback(i, "replacement readback failed");
        if (!memory.flushInstructions(site.rva, site.replacement.size()))
            return rollback(i, "instruction flush failed");
    }
    for (std::size_t n = journal.size(); n > 0; --n) {
        const auto i = n - 1;
        const auto& site = feature.dependencies[i];
        if (site.validationOnly) continue;
        if (!memory.restoreProtection(site.rva, site.expected.size(), journal[i].token))
            return rollback(i, "final protection restoration failed");
        journal[i].open = false;
    }
    report.state = InstallState::Applied;
    for (std::size_t i = 0; i < report.sites.size(); ++i) {
        report.sites[i].state = feature.dependencies[i].validationOnly ? SiteState::Validated : SiteState::Applied;
    }
    return report;
}
} // namespace fork_boundary
