#pragma once

#include <cstddef>
#include <cstdint>

// Bounded disk-file identity for the startup gate. A matching disk hash alone
// does not authenticate mapped bytes, native ABI, or a supported address profile.
namespace fork_startup {

struct FileIdentity {
    unsigned char sha256[32];
    std::uint64_t size;
};

enum class IdentityReadStatus {
    Ok,
    PathUnavailable,
    OpenFailed,
    SizeMismatch,
    ReadFailed,
    ChangedDuringRead
};

// The expected size must be 1..64 MiB. On failure, output is untouched.
// This function uses bounded stack storage and read-only Kernel32 file calls.
IdentityReadStatus ReadFileIdentity(const wchar_t* path,
                                    std::uint64_t expectedSize,
                                    FileIdentity& output);

// Standard SHA-256. For count > 0, data must point to count readable bytes;
// digest must always point to 32 writable bytes.
void HashBytes(const unsigned char* data, std::size_t count,
               unsigned char digest[32]);

const char* IdentityReadReason(IdentityReadStatus status);

} // namespace fork_startup
