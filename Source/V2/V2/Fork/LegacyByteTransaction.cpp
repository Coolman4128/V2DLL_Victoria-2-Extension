#include "LegacyByteTransaction.h"

#include <algorithm>
#include <cstdint>
#include <utility>

namespace fork_boundary {
namespace {

struct Journal {
    std::vector<unsigned char> original;
    std::vector<unsigned char> check;
    std::uintptr_t token = 0;
    bool open = false;
    bool touched = false;
    bool bytesRecovered = true;
    bool protectionRecovered = true;
};

bool Overlap(const LegacyByteSite& a, const LegacyByteSite& b) noexcept {
    const auto aEnd = static_cast<std::uint64_t>(a.rva) + a.expected.size();
    const auto bEnd = static_cast<std::uint64_t>(b.rva) + b.expected.size();
    return a.rva < bEnd && b.rva < aEnd;
}

} // namespace

LegacyByteReport InstallLegacyByteTable(const std::vector<LegacyByteSite>& sites,
                                        std::uint32_t imageSize,
                                        const MemoryAccess& memory) {
    LegacyByteReport report;
    report.reason.reserve(512);
    if (sites.empty() || sites.size() > kLegacyByteMaxSites || imageSize == 0) {
        report.reason = "empty/oversized table or missing image extent";
        return report;
    }
    report.sites.reserve(sites.size());
    for (const auto& site : sites) {
        LegacyByteSiteReport entry;
        entry.name = site.name;
        entry.reason.reserve(512);
        entry.reason = site.enabled ? "not yet validated" : "disabled";
        report.sites.push_back(std::move(entry));
        if (site.enabled) ++report.enabledCount;
        else ++report.skippedCount;
    }
    auto reject = [&](const char* why, std::size_t row = static_cast<std::size_t>(-1)) {
        report.state = LegacyByteState::Rejected;
        for (std::size_t i = 0; i < sites.size(); ++i) {
            if (!sites[i].enabled) continue;
            report.sites[i].state = LegacyByteSiteState::Rejected;
            report.sites[i].reason = "table rejected before mutation";
        }
        if (row < sites.size()) {
            report.reason.assign(sites[row].name);
            report.reason.append(": ");
            report.reason.append(why);
            report.sites[row].reason = why;
        } else report.reason = why;
        return report;
    };
    if (!report.enabledCount) {
        report.state = LegacyByteState::Skipped;
        report.reason = "all rows disabled";
        return report;
    }
    if (!memory.read || !memory.beginWrite || !memory.write ||
        !memory.restoreProtection || !memory.flushInstructions)
        return reject("incomplete memory backend");

    // Only effective-enabled rows are dependencies. Disabled rows have no
    // resolution, range, signature or protection requirement.
    for (std::size_t i = 0; i < sites.size(); ++i) {
        const auto& site = sites[i];
        if (!site.enabled) continue;
        if (site.name.empty() || site.name.size() > kLegacyByteMaxNameLength ||
            site.expected.empty() ||
            site.expected.size() > kLegacyByteMaxLength ||
            site.replacement.size() != site.expected.size())
            return reject("invalid enabled row name or byte length", i);
        if (site.rva >= imageSize || site.expected.size() > imageSize - site.rva)
            return reject("enabled row outside image", i);
        for (std::size_t j = 0; j < i; ++j) {
            if (!sites[j].enabled) continue;
            if (site.name == sites[j].name) return reject("duplicate enabled row name", i);
            if (Overlap(site, sites[j])) return reject("overlapping enabled rows", i);
        }
    }

    // All allocations and report capacities are ready before beginWrite.
    std::vector<Journal> journal(sites.size());
    for (std::size_t i = 0; i < sites.size(); ++i) {
        if (!sites[i].enabled) continue;
        journal[i].original.resize(sites[i].expected.size());
        journal[i].check.resize(sites[i].expected.size());
    }
    for (std::size_t i = 0; i < sites.size(); ++i) {
        const auto& site = sites[i];
        if (!site.enabled) continue;
        auto& entry = journal[i];
        if (!memory.read(site.rva, entry.original.data(), entry.original.size()))
            return reject("enabled row read failed", i);
        if (entry.original != site.expected)
            return reject("enabled row signature mismatch", i);
        report.sites[i].state = LegacyByteSiteState::Validated;
        report.sites[i].reason = "validated";
    }

    std::size_t failedAt = sites.size();
    const char* failure = nullptr;
    // Acquiring all tokens before the first byte write also handles rows that
    // share a page: the tokens must be restored in reverse acquisition order.
    for (std::size_t i = 0; i < sites.size(); ++i) {
        if (!sites[i].enabled) continue;
        auto& entry = journal[i];
        if (!memory.beginWrite(sites[i].rva, sites[i].expected.size(), entry.token)) {
            failedAt = i;
            failure = "write protection acquisition failed";
            break;
        }
        entry.open = true;
    }
    if (!failure) {
        for (std::size_t i = 0; i < sites.size(); ++i) {
            const auto& site = sites[i];
            if (!site.enabled) continue;
            auto& entry = journal[i];
            // MemoryAccess::write may partially mutate before returning false.
            entry.touched = true;
            if (!memory.write(site.rva, site.replacement.data(), site.replacement.size())) {
                failedAt = i;
                failure = "write failed or was partial";
                break;
            }
            if (!memory.read(site.rva, entry.check.data(), entry.check.size()) ||
                entry.check != site.replacement) {
                failedAt = i;
                failure = "replacement readback failed";
                break;
            }
            if (!memory.flushInstructions(site.rva, site.replacement.size())) {
                failedAt = i;
                failure = "instruction flush failed";
                break;
            }
        }
    }
    if (!failure) {
        for (std::size_t n = sites.size(); n > 0; --n) {
            const auto i = n - 1;
            if (!sites[i].enabled) continue;
            auto& entry = journal[i];
            if (!memory.restoreProtection(sites[i].rva, sites[i].expected.size(), entry.token)) {
                failedAt = i;
                failure = "final protection restoration failed";
                break;
            }
            entry.open = false;
        }
    }
    if (!failure) {
        report.state = LegacyByteState::Applied;
        report.reason = "all enabled rows applied and verified";
        for (std::size_t i = 0; i < sites.size(); ++i) {
            if (!sites[i].enabled) continue;
            report.sites[i].state = LegacyByteSiteState::Applied;
            report.sites[i].reason = "applied and verified";
        }
        return report;
    }

    // Recovery continues after any individual error. Every touched region is
    // reacquired and attempted, even the row whose write failed partially.
    for (std::size_t n = sites.size(); n > 0; --n) {
        const auto i = n - 1;
        auto& entry = journal[i];
        const auto& site = sites[i];
        if (!site.enabled || !entry.touched) continue;
        std::uintptr_t rollbackToken = 0;
        const bool reacquired = memory.beginWrite(site.rva, site.expected.size(), rollbackToken);
        if (reacquired) {
            // Even if a callback returns false after modifying all bytes,
            // treat recovery as incomplete: its write contract failed.
            const bool wrote = memory.write(site.rva, entry.original.data(), entry.original.size());
            const bool read = memory.read(site.rva, entry.check.data(), entry.check.size());
            const bool flushed = memory.flushInstructions(site.rva, entry.original.size());
            entry.bytesRecovered = wrote && read && entry.check == entry.original && flushed;
            if (!memory.restoreProtection(site.rva, site.expected.size(), rollbackToken))
                entry.protectionRecovered = false;
        } else {
            // This failed reacquisition cannot certify byte recovery, even if
            // a read happens to match the original after a partial write.
            const bool read = memory.read(site.rva, entry.check.data(), entry.check.size());
            entry.bytesRecovered = false;
            entry.protectionRecovered = false;
            (void)read;
        }
    }
    // Restore every still-open original token, including rows never written.
    for (std::size_t n = sites.size(); n > 0; --n) {
        const auto i = n - 1;
        auto& entry = journal[i];
        if (!sites[i].enabled || !entry.open) continue;
        if (!memory.restoreProtection(sites[i].rva, sites[i].expected.size(), entry.token))
            entry.protectionRecovered = false;
        else entry.open = false;
    }
    // A final read after all restoration attempts verifies the whole touched
    // set, not only each site's immediate rollback write.
    bool complete = true;
    for (std::size_t i = 0; i < sites.size(); ++i) {
        const auto& site = sites[i];
        if (!site.enabled) continue;
        auto& entry = journal[i];
        if (entry.touched) {
            const bool read = memory.read(site.rva, entry.check.data(), entry.check.size());
            entry.bytesRecovered = entry.bytesRecovered && read && entry.check == entry.original;
        }
        if (entry.open || !entry.bytesRecovered || !entry.protectionRecovered) complete = false;
    }
    // All protection/byte recovery work is complete before report text changes.
    report.state = complete ? LegacyByteState::RolledBack : LegacyByteState::RollbackFailed;
    report.reason.assign(sites[failedAt].name);
    report.reason.append(": ");
    report.reason.append(failure);
    report.reason.append(complete ? "; rollback verified" :
        "; byte or protection recovery incomplete");
    for (std::size_t i = 0; i < sites.size(); ++i) {
        if (!sites[i].enabled) continue;
        const auto& entry = journal[i];
        const bool siteOk = !entry.open && entry.bytesRecovered && entry.protectionRecovered;
        report.sites[i].state = !siteOk ? LegacyByteSiteState::RollbackFailed :
            (entry.touched ? LegacyByteSiteState::RolledBack : LegacyByteSiteState::Validated);
        report.sites[i].reason = !siteOk ? "byte or protection recovery incomplete" :
            (entry.touched ? "original bytes restored and verified" : "validated; not written");
    }
    report.sites[failedAt].reason = failure;
    return report;
}

} // namespace fork_boundary
