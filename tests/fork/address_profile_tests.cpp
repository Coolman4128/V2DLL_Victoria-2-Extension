#include "../../Source/V2/V2/Fork/AddressProfile.h"

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <vector>

using namespace fork_boundary;
namespace {
int assertions = 0;
void Check(bool ok, const char* message) {
    ++assertions;
    if (!ok) { std::cerr << "FAIL: " << message << '\n'; std::exit(1); }
}
ExecutableIdentity Identity() { return {std::string(64, 'a'), 0x14c, 64}; }
AddressProfile Profile() { return {"synthetic only", Identity(), "tests/fork/address_profile_tests.cpp"}; }
PatchSite Site(const char* name, std::uint32_t rva) {
    PatchSite s;
    s.name = name; s.rva = rva; s.expected = {0x90, 0x90}; s.replacement = {0xcc, 0xcc};
    s.contract = {"synthetic/no native call", "synthetic/no register use", "synthetic/no FPU use",
                  "synthetic/exclusive test invocation", "test fixture only; no native ABI certification"};
    return s;
}
FeaturePlan Plan() { return {"synthetic feature", {Site("first", 4), Site("second", 16)}}; }
struct Backend {
    std::vector<unsigned char> bytes = std::vector<unsigned char>(64, 0x90);
    bool writable = false; // All sites intentionally share one protection page.
    int reads = 0, opens = 0, writes = 0, closes = 0, flushes = 0;
    int failRead = 0, failOpen = 0, failWrite = 0, failClose = 0, failFlush = 0;
    bool failRollbackWrites = false;
    MemoryAccess Access() {
        return {
            [this](std::uint32_t r, unsigned char* out, std::size_t n) {
                if (++reads == failRead) return false;
                std::copy_n(bytes.data() + r, n, out); return true;
            },
            [this](std::uint32_t, std::size_t, std::uintptr_t& token) {
                if (++opens == failOpen) return false;
                token = writable ? 1 : 0; writable = true; return true;
            },
            [this](std::uint32_t r, const unsigned char* data, std::size_t n) {
                ++writes;
                Check(writable, "backend writes require writable protection");
                if (writes == failWrite || (failRollbackWrites && writes > failWrite)) {
                    bytes[r] = data[0]; return false; // Deliberate partial write.
                }
                std::copy_n(data, n, bytes.data() + r); return true;
            },
            [this](std::uint32_t, std::size_t, std::uintptr_t token) {
                ++closes; writable = token != 0;
                return closes != failClose; // False can still change protection.
            },
            [this](std::uint32_t, std::size_t) { return ++flushes != failFlush; }
        };
    }
};
void CheckPristine(const Backend& b) {
    Check(std::all_of(b.bytes.begin(), b.bytes.end(), [](unsigned char c) { return c == 0x90; }),
          "every original byte restored");
    Check(!b.writable, "original page protection restored");
}
void CheckRejected(FeaturePlan plan, AddressProfile profile = Profile(), ExecutableIdentity id = Identity()) {
    Backend b;
    const auto report = InstallFeature(id, profile, plan, b.Access());
    Check(report.state == InstallState::Rejected, "invalid contract rejected");
    Check(b.opens == 0 && b.writes == 0 && b.closes == 0 && b.flushes == 0,
          "invalid dependency has zero mutations/protection changes");
    Check(report.sites.size() == plan.dependencies.size(), "all dependencies reported");
    for (const auto& site : report.sites) {
        Check(site.state == SiteState::Skipped && !site.reason.empty(), "rejected sites explicitly skipped");
    }
    CheckPristine(b);
}
} // namespace

int main() {
    {
        Backend b;
        auto report = InstallFeature(Identity(), Profile(), Plan(), b.Access());
        Check(report.state == InstallState::Applied, "supported feature applied");
        Check(b.bytes[4] == 0xcc && b.bytes[16] == 0xcc, "both dependent patches written");
        Check(!b.writable, "same-page protection restored on success");
        for (const auto& site : report.sites) Check(site.state == SiteState::Applied, "applied report truthful");
    }
    auto id = Identity(); id.sha256[0] = 'b'; CheckRejected(Plan(), Profile(), id);
    auto profile = Profile(); profile.evidence.clear(); CheckRejected(Plan(), profile);
    id = Identity(); id.machine = 0x8664; CheckRejected(Plan(), Profile(), id);
    id = Identity(); id.sha256 = "unknown"; CheckRejected(Plan(), Profile(), id);
    auto plan = Plan(); plan.dependencies[1].expected[1] = 0x91; CheckRejected(plan);
    plan = Plan(); plan.dependencies[1].expected.clear(); CheckRejected(plan);
    plan = Plan(); plan.dependencies[1].contract.evidence.clear(); CheckRejected(plan);
    plan = Plan(); plan.dependencies[1].contract.registerAndStackPreservation.clear(); CheckRejected(plan);
    plan = Plan(); plan.dependencies[1].contract.fpuPreservation.clear(); CheckRejected(plan);
    plan = Plan(); plan.dependencies[1].contract.lifetimeAndPhase.clear(); CheckRejected(plan);
    plan = Plan(); plan.dependencies[1].rva = 5; CheckRejected(plan);
    plan = Plan(); plan.dependencies[1].rva = 63; CheckRejected(plan);
    plan = Plan(); plan.dependencies[1].replacement.clear(); CheckRejected(plan);
    plan = Plan(); plan.dependencies[1].name = "first"; CheckRejected(plan);
    plan = Plan(); plan.dependencies[1].kind = PatchKind::RawTrampoline;
    plan.dependencies[1].instructionLengths = {1, 1}; plan.dependencies[1].resumeRva = 18;
    plan.dependencies[1].stolenInstructionsContainRelativeAddressing = true; CheckRejected(plan);
    plan.dependencies[1].stolenInstructionsContainRelativeAddressing = false;
    plan.dependencies[1].instructionLengths = {1}; CheckRejected(plan);
    plan.dependencies[1].instructionLengths = {0, 2}; CheckRejected(plan);
    plan.dependencies[1].instructionLengths = {1, 1}; plan.dependencies[1].resumeRva = 17; CheckRejected(plan);
    {
        Backend b; b.failRead = 2;
        auto report = InstallFeature(Identity(), Profile(), Plan(), b.Access());
        Check(report.state == InstallState::Rejected && b.writes == 0 && b.opens == 0, "late dependency read failure installs nothing");
    }
    {
        Backend b; auto memory = b.Access(); memory.flushInstructions = {};
        auto report = InstallFeature(Identity(), Profile(), Plan(), memory);
        Check(report.state == InstallState::Rejected && b.opens == 0, "incomplete backend rejected");
    }
    // Every failure can occur after a previous successful write. Includes final
    // restore failure, instruction-flush failure and failed partial second write.
    for (int failure = 0; failure < 7; ++failure) {
        Backend b;
        if (failure == 0) b.failWrite = 2;
        if (failure == 1) b.failFlush = 2;
        if (failure == 2) b.failClose = 2;
        if (failure == 3) b.failOpen = 2;
        if (failure == 4) b.failRead = 4; // second replacement readback
        if (failure == 5) b.failWrite = 1;
        if (failure == 6) b.failClose = 1; // first final close changes protection then fails
        const auto report = InstallFeature(Identity(), Profile(), Plan(), b.Access());
        Check(report.state == InstallState::RolledBack, "failed installation rolls back");
        CheckPristine(b);
        for (const auto& site : report.sites) Check(site.state != SiteState::Applied, "rolled-back install reports no applied site");
        if (failure == 0) Check(report.sites[1].state == SiteState::RolledBack, "failed partial-write site itself rolled back");
    }
    {
        Backend b; b.failWrite = 2; b.failRollbackWrites = true;
        const auto report = InstallFeature(Identity(), Profile(), Plan(), b.Access());
        Check(report.state == InstallState::RollbackFailed, "rollback failure explicit at feature level");
        Check(report.sites[0].state == SiteState::RollbackFailed, "rollback failure explicit at site level");
        Check(!b.writable, "protection attempted even when rollback write fails");
    }
    {
        Backend b; b.failWrite = 2; b.failOpen = 3;
        const auto report = InstallFeature(Identity(), Profile(), Plan(), b.Access());
        Check(report.state == InstallState::RollbackFailed, "rollback protection failure explicit");
        Check(report.sites[1].state == SiteState::RollbackFailed, "unrestored failed-write site explicit");
        Check(!b.writable, "original protection restored after rollback acquisition failure");
    }
    {
        Backend b; b.failWrite = 2; b.failFlush = 2; // first rollback flush
        const auto report = InstallFeature(Identity(), Profile(), Plan(), b.Access());
        Check(report.state == InstallState::RollbackFailed, "rollback cache flush failure explicit");
        Check(report.sites[1].state == SiteState::RollbackFailed, "rollback flush failure attributed to site");
        CheckPristine(b);
    }
    {
        auto mixed = Plan();
        auto boundary = Site("observed resume boundary", 24);
        boundary.validationOnly = true; boundary.replacement.clear();
        mixed.dependencies.push_back(boundary);
        Backend b;
        const auto report = InstallFeature(Identity(), Profile(), mixed, b.Access());
        Check(report.state == InstallState::Applied, "mixed feature applies writes");
        Check(report.sites[2].state == SiteState::Validated, "read-only dependency distinctly validated");
        Check(b.opens == 2 && b.writes == 2 && b.closes == 2 && b.flushes == 2,
              "read-only dependency causes no mutation callbacks");
        Check(b.bytes[24] == 0x90 && !b.writable, "read-only bytes/protection untouched");
        mixed.dependencies[2].expected[1] = 0x91;
        CheckRejected(mixed);
        mixed.dependencies[2].expected[1] = 0x90;
        Backend failed; failed.failWrite = 2;
        const auto failedReport = InstallFeature(Identity(), Profile(), mixed, failed.Access());
        Check(failedReport.state == InstallState::RolledBack, "mixed feature rollback");
        Check(failedReport.sites[2].state == SiteState::Skipped, "read-only dependency skipped when feature fails");
        CheckPristine(failed);
        mixed.dependencies[2].replacement = {0xcc, 0xcc}; CheckRejected(mixed);
    }
    {
        auto observed = Plan();
        for (auto& site : observed.dependencies) { site.validationOnly = true; site.replacement.clear(); }
        Backend b;
        auto memory = b.Access();
        memory.beginWrite = {}; memory.write = {}; memory.restoreProtection = {}; memory.flushInstructions = {};
        const auto report = InstallFeature(Identity(), Profile(), observed, memory);
        Check(report.state == InstallState::Validated, "all-read-only feature validated without write backend");
        for (const auto& site : report.sites) Check(site.state == SiteState::Validated, "all-read-only sites distinctly validated");
        Check(b.reads == 2 && b.opens == 0 && b.writes == 0 && b.closes == 0 && b.flushes == 0,
              "all-read-only feature only reads dependencies");
        CheckPristine(b);
    }
    std::cout << "PASS: address profile boundary (" << assertions << " assertions; synthetic only)\n";
}
