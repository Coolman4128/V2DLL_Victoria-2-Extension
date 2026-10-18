#include "../../Source/V2/V2/Fork/ViewsBoundary.h"
#include <algorithm>
#include <cstdlib>
#include <iostream>

using namespace fork_boundary;
namespace {
int checks = 0;
void Check(bool ok, const char* why) {
    ++checks;
    if (!ok) { std::cerr << "FAIL: " << why << '\n'; std::exit(1); }
}
constexpr std::uint32_t Base = 0x400000, Size = 0xB00000;
const std::array<std::uint32_t, ViewSlotCount> Replacement = {
    0x60000100, 0x60000200, 0x60000300, 0x60000400
};
const std::array<std::uint32_t, ViewSlotCount> Sentinel = {11, 22, 33, 44};
ExecutableIdentity Identity() { return {std::string(64, 'a'), 0x14c, Size}; }
AddressProfile Profile() { return {"synthetic views", Identity(), "synthetic only"}; }
NativeContract Contract() {
    return {"synthetic", "synthetic", "synthetic", "exclusive synthetic invocation",
        "test fixture; no native ABI/lifetime/phase certification"};
}
ViewsEvidence Evidence() {
    ViewsEvidence result;
    result.closureEvidence = "synthetic fixture, not production proof";
    for (std::size_t i = 0; i < ViewSlotCount; ++i) {
        auto& proof = result.slots[i];
        proof.targetRva = 0x100 + static_cast<std::uint32_t>(i) * 16;
        proof.entry.name = "entry " + std::to_string(i);
        proof.entry.rva = proof.targetRva;
        proof.entry.expected = {0x90};
        proof.entry.validationOnly = true;
        proof.entry.kind = PatchKind::Instructions;
        proof.entry.instructionLengths = {1};
        proof.entry.resumeRva = proof.targetRva + 1;
        proof.entry.contract = Contract();
    }
    PatchSite read;
    read.name = "dependent read"; read.rva = 0x200;
    read.expected = {0x90}; read.validationOnly = true; read.contract = Contract();
    result.readDependencies.push_back(read);
    return result;
}
struct Backend {
    std::vector<unsigned char> bytes = std::vector<unsigned char>(Size, 0x90);
    std::vector<unsigned char> before;
    bool writable = false;
    int reads = 0, opens = 0, writes = 0, closes = 0, flushes = 0;
    int failRead = 0, failOpen = 0, failWrite = 0, failClose = 0, failFlush = 0;
    bool failRollback = false;
    bool failCloseLeavesOpen = false;
    const std::array<std::uint32_t, ViewSlotCount>* observedOutputs = nullptr;
    void CheckUnpublished() const {
        if (observedOutputs) Check(*observedOutputs == Sentinel, "outputs unpublished inside backend callbacks");
    }
    explicit Backend(std::uint32_t base = Base) {
        for (std::size_t i = 0; i < ViewSlotCount; ++i) {
            const auto value = base + Evidence().slots[i].targetRva;
            for (unsigned j = 0; j < 4; ++j)
                bytes[ViewSlotRvas[i] + j] = static_cast<unsigned char>(value >> (j * 8));
        }
        before = bytes;
    }
    MemoryAccess Access() {
        return {
            [this](std::uint32_t r, unsigned char* out, std::size_t n) {
                CheckUnpublished();
                if (++reads == failRead) return false;
                Check(r <= bytes.size() && n <= bytes.size() - r, "read within image");
                std::copy_n(bytes.data() + r, n, out); return true;
            },
            [this](std::uint32_t, std::size_t, std::uintptr_t& token) {
                CheckUnpublished();
                if (++opens == failOpen) return false;
                token = writable ? 1 : 0; writable = true; return true;
            },
            [this](std::uint32_t r, const unsigned char* data, std::size_t n) {
                CheckUnpublished();
                Check(writable, "write has protection");
                ++writes;
                if (writes == failWrite || (failRollback && writes > failWrite)) {
                    bytes[r] = data[0]; return false;
                }
                std::copy_n(data, n, bytes.data() + r); return true;
            },
            [this](std::uint32_t, std::size_t, std::uintptr_t token) {
                CheckUnpublished();
                ++closes;
                if (closes == failClose && failCloseLeavesOpen) return false;
                writable = token != 0; return closes != failClose;
            },
            [this](std::uint32_t, std::size_t) { CheckUnpublished(); return ++flushes != failFlush; }
        };
    }
};
InstallReport Run(Backend& b, ViewsEvidence evidence = Evidence(), std::uint32_t base = Base,
    ExecutableIdentity actual = Identity(), AddressProfile profile = Profile(),
    std::array<std::uint32_t, ViewSlotCount> replacements = Replacement) {
    auto outputs = Sentinel;
    b.observedOutputs = &outputs;
    auto report = InstallViewsFamily(actual, profile, base, evidence, replacements, outputs, b.Access());
    b.observedOutputs = nullptr;
    if (report.state == InstallState::Applied) {
        for (std::size_t i = 0; i < ViewSlotCount; ++i) {
            Check(outputs[i] == base + evidence.slots[i].targetRva, "all originals published");
            for (unsigned j = 0; j < 4; ++j)
                Check(b.bytes[ViewSlotRvas[i] + j] == static_cast<unsigned char>(replacements[i] >> (j * 8)),
                    "correct callback bytes");
        }
    } else Check(outputs == Sentinel, "failure leaves all outputs unchanged");
    return report;
}
void Rejected(ViewsEvidence evidence, std::uint32_t base = Base,
    ExecutableIdentity actual = Identity(), AddressProfile profile = Profile(),
    std::array<std::uint32_t, ViewSlotCount> replacements = Replacement) {
    Backend b;
    const auto report = Run(b, evidence, base, actual, profile, replacements);
    Check(report.state == InstallState::Rejected && !report.reason.empty(), "explicit rejection");
    Check(b.opens == 0 && b.writes == 0 && b.closes == 0 && b.flushes == 0, "zero mutations on mismatch");
    Check(b.bytes == b.before && !b.writable, "rejection pristine");
}
}
int main() {
    for (auto base : {Base, std::uint32_t(0x14000000)}) {
        Backend b(base);
        Check(Run(b, Evidence(), base).state == InstallState::Applied, "preferred and relocated image");
        Check(b.writes == 4 && !b.writable, "only four writes, protections restored");
    }
    Rejected(Evidence(), 0);
    Rejected(Evidence(), 0xFFFFFF00);
    Rejected(Evidence(), Base + 0x10000); // valid address extent, wrong relocation
    auto id = Identity(); id.sha256[0] = 'b'; Rejected(Evidence(), Base, id);
    auto profile = Profile(); profile.executable.imageSize = 0; Rejected(Evidence(), Base, Identity(), profile);
    auto evidence = Evidence(); evidence.closureEvidence.clear(); Rejected(evidence);
    evidence = Evidence(); evidence.readDependencies.clear(); Rejected(evidence);
    evidence = Evidence(); evidence.readDependencies[0].validationOnly = false; Rejected(evidence);
    evidence = Evidence(); evidence.readDependencies[0].expected[0] = 0xCC; Rejected(evidence);
    evidence = Evidence(); evidence.readDependencies.push_back(evidence.readDependencies[0]); Rejected(evidence);
    evidence = Evidence(); evidence.readDependencies[0] = evidence.slots[0].entry; Rejected(evidence);
    for (std::size_t i = 0; i < ViewSlotCount; ++i) {
        evidence = Evidence(); evidence.slots[i].targetRva = Size; Rejected(evidence);
        evidence = Evidence(); evidence.slots[i].entry.rva++; Rejected(evidence);
        evidence = Evidence(); evidence.slots[i].entry.validationOnly = false; Rejected(evidence);
        evidence = Evidence(); evidence.slots[i].entry.instructionLengths.clear(); Rejected(evidence);
        evidence = Evidence(); evidence.slots[i].entry.contract.fpuPreservation.clear(); Rejected(evidence);
        auto replacements = Replacement; replacements[i] = 0; Rejected(Evidence(), Base, Identity(), Profile(), replacements);
        replacements[i] = Base + Evidence().slots[i].targetRva; Rejected(Evidence(), Base, Identity(), Profile(), replacements);
        Backend b; b.bytes[ViewSlotRvas[i]] ^= 1;
        Check(Run(b).state == InstallState::Rejected && b.writes == 0 && b.opens == 0, "each original slot mismatch rejects whole group");
        Backend entry; entry.bytes[Evidence().slots[i].targetRva] = 0xCC;
        Check(Run(entry).state == InstallState::Rejected && entry.writes == 0, "each native entry mismatch rejects whole group");
    }
    // Every transaction step, including partially failing writes, must recover
    // the entire family and withhold original outputs.
    for (int kind = 0; kind < 5; ++kind) {
        const int count = kind == 0 ? 13 : 4; // 9 dependencies + 4 readbacks
        for (int at = 1; at <= count; ++at) {
            Backend b;
            if (kind == 0) b.failRead = at;
            if (kind == 1) b.failOpen = at;
            if (kind == 2) b.failWrite = at;
            if (kind == 3) b.failClose = at;
            if (kind == 4) b.failFlush = at;
            const auto result = Run(b);
            Check(result.state == InstallState::Rejected || result.state == InstallState::RolledBack,
                "injected failure never reports success");
            Check(b.bytes == b.before && !b.writable, "every original and protection restored");
        }
    }
    Backend broken; broken.failWrite = 2; broken.failRollback = true;
    Check(Run(broken).state == InstallState::RollbackFailed, "unrecoverable backend failure explicit");
    for (int at = 1; at <= 4; ++at) {
        Backend b; b.failClose = at; b.failCloseLeavesOpen = true;
        Check(Run(b).state == InstallState::RolledBack, "failed restore leaving RW recovers");
        Check(b.bytes == b.before && !b.writable, "RW failure restores all bytes/protections");
    }
    {
        Backend b;
        std::array<std::uint32_t, ViewSlotCount> outputs{};
        b.bytes[ViewSlotRvas[3]] ^= 1;
        Check(InstallViewsFamily(Identity(), Profile(), Base, Evidence(), Replacement, outputs, b.Access()).state == InstallState::Rejected,
            "null prior outputs rejected safely");
        Check(outputs == std::array<std::uint32_t, ViewSlotCount>{}, "null prior outputs untouched");
    }
    std::cout << checks << " assertions pass; synthetic only; native ABI/lifetime/phase NOT certified\n";
}
