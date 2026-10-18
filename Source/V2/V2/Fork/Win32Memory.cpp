#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include "Win32Memory.h"
#include <cstring>
#include <limits>

#if !defined(_M_IX86)
#error This backend supports Release Win32 only.
#endif

namespace fork_boundary {
namespace {
bool CopyBytes(void* destination, const void* source, std::size_t count) noexcept {
    __try { std::memcpy(destination, source, count); return true; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

bool Readable(DWORD protection) noexcept {
    if (protection & ~(0xffUL | PAGE_NOCACHE | PAGE_WRITECOMBINE)) return false;
    if (protection & (PAGE_GUARD | PAGE_NOACCESS)) return false;
    switch (protection & 0xff) {
    case PAGE_READONLY: case PAGE_READWRITE: case PAGE_WRITECOPY:
    case PAGE_EXECUTE_READ: case PAGE_EXECUTE_READWRITE: case PAGE_EXECUTE_WRITECOPY:
        return true;
    default: return false;
    }
}
bool Writable(DWORD protection) noexcept {
    if (!Readable(protection)) return false;
    const auto kind = protection & 0xff;
    return kind == PAGE_READWRITE || kind == PAGE_WRITECOPY ||
        kind == PAGE_EXECUTE_READWRITE || kind == PAGE_EXECUTE_WRITECOPY;
}
bool Executable(DWORD protection) noexcept {
    const auto kind = protection & 0xff;
    return kind == PAGE_EXECUTE_READ || kind == PAGE_EXECUTE_READWRITE ||
        kind == PAGE_EXECUTE_WRITECOPY;
}
struct Image {
    std::uint32_t base, size;
    DWORD pageSize;
    bool Range(std::uint32_t rva, std::size_t count) const noexcept {
        return base != 0 && size != 0 && size <= std::numeric_limits<std::uint32_t>::max()-base &&
            count != 0 && rva < size && count <= size-rva;
    }
    unsigned char* Address(std::uint32_t rva) const noexcept {
        return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(base)+rva);
    }
    bool Pages(std::uint32_t rva, std::size_t count, bool requireWritable) const noexcept {
        if (!Range(rva, count)) return false;
        auto cursor = reinterpret_cast<std::uintptr_t>(Address(rva));
        const auto end = cursor+count;
        while (cursor < end) {
            MEMORY_BASIC_INFORMATION info{};
            if (VirtualQuery(reinterpret_cast<void*>(cursor), &info, sizeof(info)) != sizeof(info) ||
                reinterpret_cast<std::uintptr_t>(info.AllocationBase) != base ||
                info.State != MEM_COMMIT || !Readable(info.Protect) ||
                (requireWritable && !Writable(info.Protect))) return false;
            const auto region = reinterpret_cast<std::uintptr_t>(info.BaseAddress);
            if (info.RegionSize > std::numeric_limits<std::uintptr_t>::max()-region) return false;
            const auto next = region+info.RegionSize;
            if (next <= cursor) return false;
            cursor = next;
        }
        return true;
    }
    bool SinglePage(std::uint32_t rva, std::size_t count) const noexcept {
        if (!Range(rva, count) || !pageSize) return false;
        const auto address = static_cast<std::uintptr_t>(base)+rva;
        return address/pageSize == (address+count-1)/pageSize;
    }
};
}

MemoryAccess MakeWin32ImageMemory(std::uint32_t base, std::uint32_t imageSize) {
    SYSTEM_INFO system{}; GetSystemInfo(&system);
    const Image image{base, imageSize, system.dwPageSize};
    return {
        [image](std::uint32_t rva, unsigned char* out, std::size_t count) noexcept {
            return out && image.Pages(rva, count, false) && CopyBytes(out, image.Address(rva), count);
        },
        [image](std::uint32_t rva, std::size_t count, std::uintptr_t& token) noexcept {
            if (!image.SinglePage(rva, count) || !image.Pages(rva, count, false)) return false;
            MEMORY_BASIC_INFORMATION info{};
            if (VirtualQuery(image.Address(rva), &info, sizeof(info)) != sizeof(info)) return false;
            // Data never becomes executable; code retains its execute permission.
            const DWORD protection = (Executable(info.Protect) ? PAGE_EXECUTE_READWRITE : PAGE_READWRITE) |
                (info.Protect & (PAGE_NOCACHE | PAGE_WRITECOMBINE));
            DWORD prior = 0;
            if (!VirtualProtect(image.Address(rva), count, protection, &prior)) return false;
            token = prior;
            return true;
        },
        [image](std::uint32_t rva, const unsigned char* in, std::size_t count) noexcept {
            return in && image.SinglePage(rva, count) && image.Pages(rva, count, true) &&
                CopyBytes(image.Address(rva), in, count);
        },
        [image](std::uint32_t rva, std::size_t count, std::uintptr_t token) noexcept {
            if (!image.SinglePage(rva, count) || !image.Pages(rva, count, false) ||
                token > std::numeric_limits<DWORD>::max() || !Readable(static_cast<DWORD>(token))) return false;
            DWORD prior = 0;
            return VirtualProtect(image.Address(rva), count, static_cast<DWORD>(token), &prior) != FALSE;
        },
        [image](std::uint32_t rva, std::size_t count) noexcept {
            return image.Pages(rva, count, false) &&
                FlushInstructionCache(GetCurrentProcess(), image.Address(rva), count) != FALSE;
        }
    };
}
} // namespace fork_boundary
