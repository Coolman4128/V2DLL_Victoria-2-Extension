#include "StartupIdentity.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

namespace fork_startup {
namespace {

constexpr std::uint64_t MaxFileBytes = 64ull * 1024ull * 1024ull;
constexpr DWORD ChunkBytes = 4096;

constexpr std::uint32_t Initial[8] = {
    0x6a09e667u, 0xbb67ae85u, 0x3c6ef372u, 0xa54ff53au,
    0x510e527fu, 0x9b05688cu, 0x1f83d9abu, 0x5be0cd19u
};
constexpr std::uint32_t Round[64] = {
    0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u,
    0x3956c25bu, 0x59f111f1u, 0x923f82a4u, 0xab1c5ed5u,
    0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u,
    0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u, 0xc19bf174u,
    0xe49b69c1u, 0xefbe4786u, 0x0fc19dc6u, 0x240ca1ccu,
    0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau,
    0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u,
    0xc6e00bf3u, 0xd5a79147u, 0x06ca6351u, 0x14292967u,
    0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu, 0x53380d13u,
    0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u,
    0xa2bfe8a1u, 0xa81a664bu, 0xc24b8b70u, 0xc76c51a3u,
    0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u,
    0x19a4c116u, 0x1e376c08u, 0x2748774cu, 0x34b0bcb5u,
    0x391c0cb3u, 0x4ed8aa4au, 0x5b9cca4fu, 0x682e6ff3u,
    0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u,
    0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u
};

std::uint32_t Rotate(std::uint32_t value, unsigned bits) {
    return (value >> bits) | (value << (32u - bits));
}

struct Sha256 {
    std::uint32_t words[8];
    unsigned char block[64];
    std::uint64_t byteCount;
    unsigned used;

    Sha256() : words{}, block{}, byteCount(0), used(0) {
        for (unsigned i = 0; i < 8; ++i) words[i] = Initial[i];
    }

    void Compress() {
        std::uint32_t schedule[64];
        for (unsigned i = 0; i < 16; ++i) {
            const unsigned p = i * 4;
            schedule[i] = (std::uint32_t(block[p]) << 24) |
                (std::uint32_t(block[p + 1]) << 16) |
                (std::uint32_t(block[p + 2]) << 8) | std::uint32_t(block[p + 3]);
        }
        for (unsigned i = 16; i < 64; ++i) {
            const std::uint32_t x = schedule[i - 15], y = schedule[i - 2];
            schedule[i] = schedule[i - 16] +
                (Rotate(x, 7) ^ Rotate(x, 18) ^ (x >> 3)) +
                schedule[i - 7] + (Rotate(y, 17) ^ Rotate(y, 19) ^ (y >> 10));
        }
        std::uint32_t a = words[0], b = words[1], c = words[2], d = words[3];
        std::uint32_t e = words[4], f = words[5], g = words[6], h = words[7];
        for (unsigned i = 0; i < 64; ++i) {
            const std::uint32_t choose = (e & f) ^ (~e & g);
            const std::uint32_t majority = (a & b) ^ (a & c) ^ (b & c);
            const std::uint32_t t1 = h + (Rotate(e, 6) ^ Rotate(e, 11) ^ Rotate(e, 25)) +
                choose + Round[i] + schedule[i];
            const std::uint32_t t2 = (Rotate(a, 2) ^ Rotate(a, 13) ^ Rotate(a, 22)) + majority;
            h = g; g = f; f = e; e = d + t1;
            d = c; c = b; b = a; a = t1 + t2;
        }
        words[0] += a; words[1] += b; words[2] += c; words[3] += d;
        words[4] += e; words[5] += f; words[6] += g; words[7] += h;
    }

    void Add(const unsigned char* data, std::size_t count) {
        byteCount += static_cast<std::uint64_t>(count);
        for (std::size_t i = 0; i < count; ++i) {
            block[used++] = data[i];
            if (used == 64) { Compress(); used = 0; }
        }
    }

    void Finish(unsigned char digest[32]) {
        const std::uint64_t bitCount = byteCount * 8u;
        block[used++] = 0x80;
        if (used > 56) {
            while (used < 64) block[used++] = 0;
            Compress(); used = 0;
        }
        while (used < 56) block[used++] = 0;
        for (unsigned i = 0; i < 8; ++i)
            block[56 + i] = static_cast<unsigned char>(bitCount >> ((7u - i) * 8u));
        Compress();
        for (unsigned i = 0; i < 8; ++i) {
            digest[i * 4] = static_cast<unsigned char>(words[i] >> 24);
            digest[i * 4 + 1] = static_cast<unsigned char>(words[i] >> 16);
            digest[i * 4 + 2] = static_cast<unsigned char>(words[i] >> 8);
            digest[i * 4 + 3] = static_cast<unsigned char>(words[i]);
        }
    }
};

bool SameFileState(const BY_HANDLE_FILE_INFORMATION& a,
                   const BY_HANDLE_FILE_INFORMATION& b) {
    return a.dwVolumeSerialNumber == b.dwVolumeSerialNumber &&
        a.nFileIndexHigh == b.nFileIndexHigh && a.nFileIndexLow == b.nFileIndexLow &&
        a.nFileSizeHigh == b.nFileSizeHigh && a.nFileSizeLow == b.nFileSizeLow &&
        a.ftCreationTime.dwHighDateTime == b.ftCreationTime.dwHighDateTime &&
        a.ftCreationTime.dwLowDateTime == b.ftCreationTime.dwLowDateTime &&
        a.ftLastWriteTime.dwHighDateTime == b.ftLastWriteTime.dwHighDateTime &&
        a.ftLastWriteTime.dwLowDateTime == b.ftLastWriteTime.dwLowDateTime;
}

} // namespace

void HashBytes(const unsigned char* data, std::size_t count,
               unsigned char digest[32]) {
    Sha256 sha;
    sha.Add(data, count);
    sha.Finish(digest);
}

IdentityReadStatus ReadFileIdentity(const wchar_t* path,
                                    std::uint64_t expectedSize,
                                    FileIdentity& output) {
    if (!path || !path[0]) return IdentityReadStatus::PathUnavailable;
    // Win32's longest extended-length path is below this limit. A missing
    // terminator within the bound is rejected before passing it to CreateFileW.
    unsigned pathLength = 0;
    while (pathLength < 32767 && path[pathLength]) ++pathLength;
    if (pathLength == 32767) return IdentityReadStatus::PathUnavailable;
    if (expectedSize == 0 || expectedSize > MaxFileBytes)
        return IdentityReadStatus::SizeMismatch;

    // Denying write sharing prevents cooperating writers from opening this
    // handle during the read. Existing or noncooperating changes remain subject
    // to metadata/length checks; this is not an atomic filesystem snapshot.
    const HANDLE file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ,
                                    nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return IdentityReadStatus::OpenFailed;

    IdentityReadStatus status = IdentityReadStatus::Ok;
    BY_HANDLE_FILE_INFORMATION before{}, after{};
    LARGE_INTEGER initialSize{}, finalSize{};
    if (GetFileType(file) != FILE_TYPE_DISK ||
        !GetFileInformationByHandle(file, &before) || !GetFileSizeEx(file, &initialSize)) {
        status = IdentityReadStatus::ReadFailed;
    } else if (initialSize.QuadPart <= 0 ||
               static_cast<std::uint64_t>(initialSize.QuadPart) != expectedSize) {
        status = IdentityReadStatus::SizeMismatch;
    } else if ((static_cast<std::uint64_t>(before.nFileSizeHigh) << 32 | before.nFileSizeLow)
               != expectedSize) {
        status = IdentityReadStatus::ChangedDuringRead;
    }

    FileIdentity candidate{};
    Sha256 sha;
    unsigned char chunk[ChunkBytes];
    std::uint64_t remaining = expectedSize;
    while (status == IdentityReadStatus::Ok && remaining != 0) {
        const DWORD want = remaining < ChunkBytes ? static_cast<DWORD>(remaining) : ChunkBytes;
        DWORD read = 0;
        if (!ReadFile(file, chunk, want, &read, nullptr)) {
            status = IdentityReadStatus::ReadFailed;
        } else if (read != want) {
            status = IdentityReadStatus::ChangedDuringRead;
        } else {
            sha.Add(chunk, read);
            remaining -= read;
        }
    }
    if (status == IdentityReadStatus::Ok) {
        unsigned char extra = 0;
        DWORD read = 0;
        if (!ReadFile(file, &extra, 1, &read, nullptr))
            status = IdentityReadStatus::ReadFailed;
        else if (read != 0)
            status = IdentityReadStatus::ChangedDuringRead;
    }
    if (status == IdentityReadStatus::Ok) {
        if (!GetFileInformationByHandle(file, &after) || !GetFileSizeEx(file, &finalSize))
            status = IdentityReadStatus::ReadFailed;
        else if (finalSize.QuadPart != initialSize.QuadPart || !SameFileState(before, after))
            status = IdentityReadStatus::ChangedDuringRead;
    }
    if (!CloseHandle(file) && status == IdentityReadStatus::Ok)
        status = IdentityReadStatus::ReadFailed;
    if (status == IdentityReadStatus::Ok) {
        sha.Finish(candidate.sha256);
        candidate.size = expectedSize;
        output = candidate;
    }
    return status;
}

const char* IdentityReadReason(IdentityReadStatus status) {
    switch (status) {
    case IdentityReadStatus::Ok: return "file identity read and stable";
    case IdentityReadStatus::PathUnavailable: return "executable path unavailable";
    case IdentityReadStatus::OpenFailed: return "executable file could not be opened read-only";
    case IdentityReadStatus::SizeMismatch: return "executable file size unexpected or outside startup bound";
    case IdentityReadStatus::ReadFailed: return "executable file read or metadata query failed";
    case IdentityReadStatus::ChangedDuringRead: return "executable file changed during identity read";
    }
    return "unknown file identity status";
}

} // namespace fork_startup
