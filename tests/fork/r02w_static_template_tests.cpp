#define wmain r02t_fixture_main
#include "r02w_basis.cpp"
#undef wmain

// This test uses the R02t owned PE32 surrogate and its actual-source D3D
// authentication fixture. The four additional slots are fixed at the pinned
// A/B RVAs and reside in that surrogate's RX MEM_IMAGE .text section.

static constexpr std::array<std::uint32_t, 4> kTemplateSlots{{
    kPinnedD3dDeviceTemplateA + 4 * kD3dDeviceResetSlot,
    kPinnedD3dDeviceTemplateA + 4 * kD3dDevicePresentSlot,
    kPinnedD3dDeviceTemplateB + 4 * kD3dDeviceResetSlot,
    kPinnedD3dDeviceTemplateB + 4 * kD3dDevicePresentSlot
}};

static std::uint32_t TemplateValue(std::size_t index)
{
    return *reinterpret_cast<volatile std::uint32_t*>(
        static_cast<std::uintptr_t>(providerBase + kTemplateSlots[index]));
}

static bool Prerequisites(D3dInstallTransaction& transaction, Probe& probe)
{
    OwnedObject factory{static_cast<std::uint32_t>(
        reinterpret_cast<std::uintptr_t>(providerD3dTable))};
    return ApplyImport(transaction, probe) && ApplyCreate(transaction, probe, factory);
}

static bool OriginalsIntact(std::uint32_t reset, std::uint32_t present)
{
    return TemplateValue(0) == reset && TemplateValue(1) == present &&
           TemplateValue(2) == reset && TemplateValue(3) == present;
}

static int TestCase(const std::wstring& name)
{
    const auto reset = Address(GetProcAddress(
        reinterpret_cast<HMODULE>(static_cast<std::uintptr_t>(providerBase)),
        "_FixtureReset@8"));
    const auto present = Address(GetProcAddress(
        reinterpret_cast<HMODULE>(static_cast<std::uintptr_t>(providerBase)),
        "_FixturePresent@20"));
    const auto resetHook = Address(reinterpret_cast<FARPROC>(&HookReset));
    const auto presentHook = Address(reinterpret_cast<FARPROC>(&HookPresent));
    Check(reset && present && reset != present,
          "owned provider Reset/Present exports are distinct");
    Check(OriginalsIntact(reset, present),
          "all four A/B slots begin with exact authenticated originals");
    D3dInstallTransaction transaction;
    Probe probe;
    if (name == L"pre_import")
    {
        const auto result = transaction.InstallStaticDeviceTemplates(
            resetHook, presentHook, probe.backend);
        Check(result.state == D3dInstallState::Rejected &&
              probe.exchanges == 0 && probe.providerOpens == 0 &&
              probe.pairPublications == 0 && OriginalsIntact(reset, present),
              "static stage refuses missing authenticated import without writes");
        return failures.load() ? 1 : 0;
    }
    if (name == L"pre_create")
    {
        if (!ApplyImport(transaction, probe)) return 1;
        const int cas = probe.exchanges, opens = probe.providerOpens;
        const auto result = transaction.InstallStaticDeviceTemplates(
            resetHook, presentHook, probe.backend);
        Check(result.state == D3dInstallState::Rejected &&
              probe.exchanges == cas && probe.providerOpens == opens &&
              probe.pairPublications == 0 && OriginalsIntact(reset, present),
              "static stage refuses absent factory table without template writes");
        return failures.load() ? 1 : 0;
    }
    if (!Prerequisites(transaction, probe)) return 1;
    std::vector<std::uint32_t> openEvents, closeEvents;
    int heapReads = 0;
    const auto begin = probe.backend.providerMemory.beginWrite;
    probe.backend.providerMemory.beginWrite = [&](std::uint32_t rva,
        std::size_t count, std::uintptr_t& token) {
        openEvents.push_back(rva);
        return begin(rva, count, token);
    };
    const auto restore = probe.backend.providerMemory.restoreProtection;
    probe.backend.providerMemory.restoreProtection = [&](std::uint32_t rva,
        std::size_t count, std::uintptr_t token) {
        closeEvents.push_back(rva);
        return restore(rva, count, token);
    };
    const auto readPointer = probe.backend.readPointer;
    probe.backend.readPointer = [&](std::uint32_t address, std::uint32_t& value) {
        ++heapReads;
        return readPointer(address, value);
    };
    const auto originalCas = probe.backend.compareExchange;
    probe.backend.compareExchange = [&](std::uint32_t address,
        std::uint32_t expected, std::uint32_t desired,
        std::uint32_t& observed) {
        if (desired == resetHook || desired == presentHook)
            Check(openEvents.size() == 4,
                  "all four protection tokens acquired before first template CAS");
        return originalCas(address, expected, desired, observed);
    };
    const int beforeCas = probe.exchanges;
    const int beforeOpens = probe.providerOpens;
    const int beforePublishes = probe.pairPublications;
    const int beforeReads = probe.providerSlotReads;
    if (name == L"success" || name == L"aslr" ||
        name == L"already" || name == L"foreign_retry")
    {
        const auto result = transaction.InstallStaticDeviceTemplates(
            resetHook, presentHook, probe.backend);
        Check(result.stage == D3dInstallStage::StaticDeviceTemplates &&
              result.state == D3dInstallState::Applied &&
              result.attemptedSlots == 4 && result.originalsPublished,
              "static A/B quartet applies as one named stage");
        Check(probe.pairPublications == beforePublishes + 1 &&
              probe.publishedReset == reset && probe.publishedPresent == present,
              "both exact originals published before any template hook");
        Check(probe.exchanges == beforeCas + 4 &&
              probe.providerOpens == beforeOpens + 4,
              "all four static slots acquired and exchanged");
        Check(openEvents == std::vector<std::uint32_t>(kTemplateSlots.begin(),
                                                      kTemplateSlots.end()) &&
              closeEvents == std::vector<std::uint32_t>(kTemplateSlots.rbegin(),
                                                       kTemplateSlots.rend()),
              "successful shared-image protections open forward and close reverse");
        Check(probe.casAddresses.size() == static_cast<std::size_t>(beforeCas + 4) &&
              std::equal(kTemplateSlots.begin(), kTemplateSlots.end(),
                  probe.casAddresses.begin() + beforeCas,
                  [](std::uint32_t rva, std::uint32_t address) {
                      return providerBase + rva == address;
                  }),
              "forward CAS addresses are exact A/B Reset/Present slots");
        Check(TemplateValue(0) == resetHook && TemplateValue(1) == presentHook &&
              TemplateValue(2) == resetHook && TemplateValue(3) == presentHook,
              "all four template slots contain exact hook pointers");
        if (name == L"already")
        {
            const int cas = probe.exchanges, opens = probe.providerOpens;
            const int publications = probe.pairPublications;
            const auto again = transaction.InstallStaticDeviceTemplates(
                resetHook, presentHook, probe.backend);
            Check(again.state == D3dInstallState::AlreadyInstalled &&
                  probe.exchanges == cas && probe.providerOpens == opens &&
                  probe.pairPublications == publications,
                  "identical installed retry is read-only and unpublished");
        }
        if (name == L"foreign_retry")
        {
            ForceProviderSlot(reinterpret_cast<std::uint32_t*>(
                static_cast<std::uintptr_t>(providerBase + kTemplateSlots[3])),
                0x12345678);
            const int cas = probe.exchanges, opens = probe.providerOpens;
            const auto changed = transaction.InstallStaticDeviceTemplates(
                resetHook, presentHook, probe.backend);
            Check(changed.state == D3dInstallState::Poisoned &&
                  probe.exchanges == cas && probe.providerOpens == opens &&
                  TemplateValue(3) == 0x12345678,
                  "foreign installed slot poisons without clobber");
        }
    }
    else if (name == L"prevalidate")
    {
        ForceProviderSlot(reinterpret_cast<std::uint32_t*>(
            static_cast<std::uintptr_t>(providerBase + kTemplateSlots[3])),
            0x12345678);
        const auto result = transaction.InstallStaticDeviceTemplates(
            resetHook, presentHook, probe.backend);
        Check(result.state == D3dInstallState::Rejected &&
              probe.exchanges == beforeCas &&
              probe.providerOpens == beforeOpens &&
              probe.pairPublications == beforePublishes &&
              TemplateValue(0) == reset && TemplateValue(1) == present &&
              TemplateValue(2) == reset && TemplateValue(3) == 0x12345678,
              "late B slot mismatch rejects before any write or publication");
    }
    else if (name == L"immutable_reject")
    {
        const auto immutable = probe.backend.immutableProviderStorage;
        probe.backend.immutableProviderStorage = [immutable](std::uint32_t base,
            std::uint32_t address, std::size_t count) {
            if (address == base + kPinnedD3dDeviceTemplateB) return false;
            return immutable(base, address, count);
        };
        const auto result = transaction.InstallStaticDeviceTemplates(
            resetHook, presentHook, probe.backend);
        Check(result.state == D3dInstallState::Rejected &&
              probe.exchanges == beforeCas &&
              probe.providerOpens == beforeOpens &&
              probe.pairPublications == beforePublishes &&
              OriginalsIntact(reset, present),
              "non-immutable B storage rejects before any template write");
    }
    else if (name == L"open_fail")
    {
        probe.failProviderOpen = beforeOpens + 4;
        const auto result = transaction.InstallStaticDeviceTemplates(
            resetHook, presentHook, probe.backend);
        Check(result.state == D3dInstallState::RolledBack &&
              probe.exchanges == beforeCas && OriginalsIntact(reset, present),
              "fourth protection acquire fails before first CAS");
    }
    else if (name == L"cas_before" || name == L"cas_after" ||
             name == L"readback" || name == L"failed_readback" ||
             name == L"foreign_mid" || name == L"rollback_close" ||
             name == L"final_restore")
    {
        probe.failCas = beforeCas + 4;
        probe.failAfterSwap = name == L"cas_after" ||
                              name == L"failed_readback";
        probe.foreignBeforeCas = name == L"foreign_mid";
        if (name == L"readback" || name == L"failed_readback")
        {
            if (name == L"readback") probe.failCas = 0;
            probe.failProviderReadAfterCas = beforeCas + 4;
        }
        if (name == L"rollback_close")
            probe.failProviderClose = probe.providerCloses + 1;
        if (name == L"final_restore")
        {
            probe.failCas = 0;
            probe.failProviderClose = probe.providerCloses + 1;
        }
        const auto result = transaction.InstallStaticDeviceTemplates(
            resetHook, presentHook, probe.backend);
        if (name == L"rollback_close")
            Check(result.state == D3dInstallState::RollbackFailed,
                  "incomplete rollback protection poisons static stage");
        else if (name == L"foreign_mid")
            Check(result.state == D3dInstallState::RollbackFailed &&
                  TemplateValue(3) == 0x12345678,
                  "CAS mismatch leaves foreign slot unmodified");
        else
            Check(result.state == D3dInstallState::RolledBack &&
                  OriginalsIntact(reset, present),
                  "late CAS/readback failure reverses all touched slots");
        Check(probe.exchanges >= beforeCas + 4 &&
              probe.pairPublications == beforePublishes + 1 &&
              probe.publishedReset == reset && probe.publishedPresent == present,
              "failure still published originals before possible exposure");
        if (name == L"cas_before")
        {
            const int cas = probe.exchanges;
            probe.failCas = 0;
            openEvents.clear();
            closeEvents.clear();
            const auto retry = transaction.InstallStaticDeviceTemplates(
                resetHook, presentHook, probe.backend);
            Check(retry.state == D3dInstallState::Applied &&
                  probe.exchanges == cas + 4,
                  "same hook tuple retries after clean rollback");
        }
        if (name == L"cas_after" || name == L"failed_readback")
        {
            const int cas = probe.exchanges, protections = probe.providerOpens;
            const auto changed = transaction.InstallStaticDeviceTemplates(
                Address(reinterpret_cast<FARPROC>(&HookResetAlternate)),
                presentHook, probe.backend);
            Check(changed.state == D3dInstallState::Poisoned &&
                  probe.exchanges == cas && probe.providerOpens == protections &&
                  OriginalsIntact(reset, present) &&
                  probe.publishedReset == reset && probe.publishedPresent == present,
                  "changed hook after possible exposure poisons without republishing");
        }
        if (name == L"rollback_close" || name == L"foreign_mid")
        {
            const int cas = probe.exchanges, protections = probe.providerOpens;
            const auto terminal = transaction.InstallStaticDeviceTemplates(
                resetHook, presentHook, probe.backend);
            Check(terminal.state == D3dInstallState::Poisoned &&
                  probe.exchanges == cas && probe.providerOpens == protections,
                  "incomplete static rollback is terminal and write-free");
        }
    }
    else if (name == L"copy_a" || name == L"copy_b" ||
             name == L"copy_c" || name == L"copy_bad_self" ||
             name == L"copy_foreign" || name == L"copy_unreadable")
    {
        const auto installed = transaction.InstallStaticDeviceTemplates(
            resetHook, presentHook, probe.backend);
        Check(installed.state == D3dInstallState::Applied,
              "static quartet installed before returned-copy check");
        auto* memory = static_cast<unsigned char*>(VirtualAlloc(nullptr, 0x4000,
            MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
        Check(memory != nullptr, "owned returned-device buffer allocated");
        if (!memory) return 1;
        memset(memory, 0xA5, 0x4000);
        const auto device = static_cast<std::uint32_t>(
            reinterpret_cast<std::uintptr_t>(memory));
        const auto copiedTable = device + 0x2F5C;
        *reinterpret_cast<std::uint32_t*>(memory) = copiedTable;
        *reinterpret_cast<std::uint32_t*>(memory + 0x2F58) =
            providerBase + (name == L"copy_b" ? kPinnedD3dDeviceTemplateB :
            name == L"copy_c" ? 0x2D44u : kPinnedD3dDeviceTemplateA);
        *reinterpret_cast<std::uint32_t*>(memory + 0x2F5C + 4 * 16) = resetHook;
        *reinterpret_cast<std::uint32_t*>(memory + 0x2F5C + 4 * 17) =
            name == L"copy_foreign" ? 0x12345678 : presentHook;
        if (name == L"copy_bad_self")
            *reinterpret_cast<std::uint32_t*>(memory) = copiedTable + 4;
        const std::vector<unsigned char> before(memory, memory + 0x4000);
        const int cas = probe.exchanges, opens = probe.providerOpens;
        if (name == L"copy_unreadable")
        {
            DWORD old = 0;
            Check(VirtualProtect(memory, 0x4000, PAGE_NOACCESS, &old) != FALSE,
                  "returned device page made unreadable for VQ fault");
        }
        const auto result = transaction.VerifyReturnedStaticDevice(device,
            probe.backend);
        if (name == L"copy_unreadable")
        {
            DWORD old = 0;
            Check(VirtualProtect(memory, 0x4000, PAGE_READWRITE, &old) != FALSE,
                  "returned device page restored after VQ fault");
        }
        const bool supported = name == L"copy_a" || name == L"copy_b";
        Check(result.stage == D3dInstallStage::ReturnedDeviceCopy &&
              result.state == (supported ? D3dInstallState::AlreadyInstalled :
                                           D3dInstallState::Rejected),
              "returned copy supports only intact A/B tuple");
        Check(probe.exchanges == cas && probe.providerOpens == opens &&
              memcmp(memory, before.data(), before.size()) == 0,
              "returned-copy verifier performs no heap or image write");
        VirtualFree(memory, 0, MEM_RELEASE);
    }
    else if (name == L"busy")
    {
        auto publish = probe.backend.publishDevicePair;
        D3dInstallState nested = D3dInstallState::Rejected;
        probe.backend.publishDevicePair = [&](std::uint32_t a, std::uint32_t b) {
            nested = transaction.InstallStaticDeviceTemplates(
                resetHook, presentHook, probe.backend).state;
            publish(a, b);
        };
        const auto result = transaction.InstallStaticDeviceTemplates(
            resetHook, presentHook, probe.backend);
        Check(nested == D3dInstallState::Busy &&
              result.state == D3dInstallState::Applied,
              "same-thread reentry returns Busy without waiting");
    }
    else
        Check(false, "recognized R02w static-template case");
    Check(probe.providerSlotReads > beforeReads,
          "transaction reads provider slots through owned backend");
    if (name.rfind(L"copy_", 0) != 0)
        Check(heapReads == 0,
              "static template stage never reads or retains a COM heap object");
    return failures.load() ? 1 : 0;
}

int wmain(int argc, wchar_t** argv)
{
    if (argc != 3) return 2;
    const std::wstring name = argv[2];
    providerPath = argv[1];
    providerPath += L"\\d3d_surrogate.dll";
    void* preferredReservation = nullptr;
    if (name == L"aslr")
    {
        preferredReservation = VirtualAlloc(reinterpret_cast<void*>(0x68000000u),
            0x400000, MEM_RESERVE, PAGE_NOACCESS);
        Check(preferredReservation == reinterpret_cast<void*>(0x68000000u),
              "preferred surrogate base reserved before owned loader relocation");
        if (!preferredReservation) return 2;
    }
    HMODULE surrogate = LoadLibraryW(providerPath.c_str());
    Check(surrogate != nullptr, "private patched provider surrogate loads");
    if (!surrogate || !PrepareProvider(surrogate)) return 2;
    Check((name == L"aslr") ?
              providerBase != expectation.providerPreferredBase :
              providerBase == expectation.providerPreferredBase,
          "owned surrogate map matches requested preferred/relocated condition");
    if (preferredReservation)
        Check(VirtualFree(preferredReservation, 0, MEM_RELEASE) != FALSE,
              "preferred-base reservation released after provider map");
    exeImage = static_cast<unsigned char*>(VirtualAlloc(nullptr, exeImageSize,
        MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
    Check(exeImage != nullptr, "owned EXE PE32 image allocated");
    if (!exeImage) return 2;
    exeBase = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(exeImage));
    ResetExeImage();
    providerD3dTable = reinterpret_cast<std::uint32_t*>(
        GetProcAddress(surrogate, "FixtureD3dTable"));
    Check(providerD3dTable != nullptr, "private factory table export available");
    if (providerD3dTable) TestCase(name);
    VirtualFree(exeImage, 0, MEM_RELEASE);
    std::printf("RESULT checks=%d failures=%d\n", checks.load(), failures.load());
    return failures.load() ? 1 : 0;
}
