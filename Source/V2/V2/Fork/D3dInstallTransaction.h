#pragma once

#include "AddressProfile.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace fork_boundary {

constexpr std::uint32_t kPinnedD3dCreate9IatRva = 0x88A39C;
constexpr std::uint32_t kPinnedD3dImageSize = 0x179000;
constexpr std::uint32_t kD3dCreateDeviceSlot = 16;
constexpr std::uint32_t kD3dDeviceResetSlot = 16;
constexpr std::uint32_t kD3dDevicePresentSlot = 17;
constexpr std::uint32_t kPinnedD3dDeviceTemplateA = 0x1008;
constexpr std::uint32_t kPinnedD3dDeviceTemplateB = 0x2034;

struct D3dInstallExpectation {
    std::uint32_t exeImageSize = 0;
    std::uint32_t exeIatRva = 0;
    std::uint64_t providerDiskSize = 0;
    std::array<unsigned char,32> providerDiskSha256{};
    std::uint32_t providerImageSize = 0;
    std::uint32_t providerTimestamp = 0;
    std::uint32_t providerPreferredBase = 0;
    std::uint32_t providerTextRva = 0;
    std::uint32_t providerTextVirtualSize = 0;
    std::uint32_t providerTextRawSize = 0;
    std::array<unsigned char,32> normalizedTextSha256{};
    std::uint32_t create9ExportRva = 0;
    std::uint32_t create9ExportOrdinal = 0;
};

const D3dInstallExpectation& PinnedD3dInstallExpectation() noexcept;

enum class D3dCasResult { Swapped, Mismatch, Failed };
enum class D3dInstallStage { ExeImport, CreateDeviceTable, DeviceTablePair,
                             StaticDeviceTemplates, ReturnedDeviceCopy };
enum class D3dInstallState {
    Skipped, Rejected, Busy, Applied, AlreadyInstalled, RolledBack,
    RollbackFailed, Poisoned
};

struct D3dInstallReport {
    D3dInstallStage stage = D3dInstallStage::ExeImport;
    D3dInstallState state = D3dInstallState::Rejected;
    std::string reason;
    bool providerReferenceRetained = false;
    bool originalsPublished = false;
    bool hookPossiblyExposed = false;
    std::size_t attemptedSlots = 0;
    std::size_t hookObservedBeforeRecovery = 0;
    std::size_t originalVerifiedAfterRecovery = 0;
};

// Synchronous callbacks. Prewrite snapshot/allocation callbacks may throw
// std::bad_alloc; the gate is released and no protection has been changed.
// Publication, memory/protection and CAS callbacks used after the first
// beginWrite must be strictly nonthrowing. readDiskSnapshot reads exact bytes
// from one retained-module path/file handle into a bounded vector; the
// transaction hashes AND parses that same buffer before any write. It must
// never invoke D3D engine code. immutableProviderStorage checks the table's
// current committed RX MEM_IMAGE storage belongs to the retained provider;
// writable .data/heap tables fail. readPointer makes a VQ-gated transient
// DWORD read from a caller-owned COM object or its copied table; it retains
// no object pointer. Publication callbacks expose
// originals before any associated hook slot CAS and never clear them after
// possible exposure. CAS atomically compares one aligned x86 DWORD.
struct D3dInstallBackend {
    MemoryAccess exeMemory;
    std::uint32_t exeMemoryBase = 0;
    MemoryAccess providerMemory;
    std::uint32_t providerMemoryBase = 0;
    std::function<bool(std::uint32_t address,std::uint32_t& retainedBase)> retainFromAddress;
    std::function<bool(std::uint32_t retainedBase,std::vector<unsigned char>& bytes)> readDiskSnapshot;
    std::function<bool(std::uint32_t base,std::uint32_t imageSize)> isMappedImage;
    std::function<bool(std::uint32_t objectAddress,std::uint32_t& vtableAddress)> readPointer;
    std::function<bool(std::uint32_t providerBase,std::uint32_t address,std::size_t count)>
        immutableProviderStorage;
    std::function<std::uint32_t(std::uint32_t authenticatedProviderBase)> resolveCreate9;
    std::function<void(std::uint32_t)> publishCreate9;
    std::function<void(std::uint32_t)> publishCreateDevice;
    std::function<void(std::uint32_t,std::uint32_t)> publishDevicePair;
    std::function<D3dCasResult(std::uint32_t absoluteAddress,std::uint32_t expected,
                                std::uint32_t desired,std::uint32_t& observed)> compareExchange;
};

// One retained provider and fixed-capacity vtable journal per process. The
// caller authenticates the pinned main EXE and exclusive native phase. The
// returned COM object/device is never AddRef'd or retained. A successful
// native provider result/object is the caller's result even if an installer
// stage reports RolledBack; these methods never fabricate HRESULTs.
class D3dInstallTransaction {
public:
    D3dInstallTransaction() = default;
    D3dInstallTransaction(const D3dInstallTransaction&) = delete;
    D3dInstallTransaction& operator=(const D3dInstallTransaction&) = delete;

    D3dInstallReport InstallExeImport(std::uint32_t activeIatProvider,
        std::uint32_t hookCreate9,const D3dInstallBackend& backend,
        const D3dInstallExpectation& expected);
    D3dInstallReport InstallCreateDeviceTable(std::uint32_t returnedD3dObject,
        std::uint32_t hookCreateDevice,const D3dInstallBackend& backend);
    // Called before the original CreateDevice. The four journal addresses are
    // authenticated provider-image A/B template slots, never returned COM memory.
    D3dInstallReport InstallStaticDeviceTemplates(std::uint32_t hookReset,
        std::uint32_t hookPresent,const D3dInstallBackend& backend);
    // Read-only check during the CreateDevice callback; no returned pointer is
    // stored across the call or save/load/Release boundary.
    D3dInstallReport VerifyReturnedStaticDevice(std::uint32_t returnedDevice,
        const D3dInstallBackend& backend);
    // Historical image-backed table contract retained for its owned tests.
    // The production HookCreateDevice uses static templates instead and never
    // journals a returned device's MEM_PRIVATE table.
    D3dInstallReport InstallDeviceTablePair(std::uint32_t returnedDevice,
        std::uint32_t hookReset,std::uint32_t hookPresent,
        const D3dInstallBackend& backend);

private:
    volatile long gate_ = 0; // idle, busy, poisoned
    std::uint32_t providerBase_ = 0;
    std::uint32_t originalCreate9_ = 0;
    std::uint32_t originalCreateDevice_ = 0;
    std::uint32_t originalReset_ = 0;
    std::uint32_t originalPresent_ = 0;
    std::uint32_t hookCreate9_ = 0;
    std::uint32_t hookCreateDevice_ = 0;
    std::uint32_t hookReset_ = 0;
    std::uint32_t hookPresent_ = 0;
    bool importInstalled_ = false;
    bool importPublished_ = false;
    bool createDevicePublished_ = false;
    bool devicePairPublished_ = false;
    bool staticTemplatesInstalled_ = false;
    bool anyHookPossiblyExposed_ = false;
    bool createDevicePossiblyExposed_ = false;
    bool devicePairPossiblyExposed_ = false;
    std::uint32_t providerImageSize_ = 0;
    std::uint32_t exeImageSize_ = 0;
    std::uint32_t exeIatRva_ = 0;
    std::uint32_t exeBase_ = 0;
    std::uint32_t providerTextRva_ = 0;
    std::uint32_t providerTextSize_ = 0;
    std::vector<unsigned char> diskBytes_;
    std::vector<std::uint32_t> highlowRelocations_;
    std::array<std::uint32_t,32> createDeviceTables_{};
    std::array<std::uint32_t,32> deviceTables_{};
    std::size_t createDeviceTableCount_ = 0;
    std::size_t deviceTableCount_ = 0;
};

// Production callback adapter over current-process mapped images and
// GetModuleHandleEx(FROM_ADDRESS), without calling Direct3DCreate9 itself.
D3dInstallBackend MakeWin32D3dInstallBackend(
    std::uint32_t exeBase,std::uint32_t exeImageSize,
    std::uint32_t providerBase,std::uint32_t providerImageSize,
    std::function<void(std::uint32_t)> publishCreate9,
    std::function<void(std::uint32_t)> publishCreateDevice,
    std::function<void(std::uint32_t,std::uint32_t)> publishDevicePair);

} // namespace fork_boundary
