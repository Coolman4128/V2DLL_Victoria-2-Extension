#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include "../../Source/V2/V2/Fork/StartupIdentity.h"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cwchar>

using namespace fork_startup;

namespace {
int checks = 0;

void Check(bool condition, const char* why) {
    ++checks;
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s (Win32 %lu)\n", why, GetLastError());
        std::exit(1);
    }
}

void Hex(const unsigned char digest[32], char output[65]) {
    constexpr char digits[] = "0123456789abcdef";
    for (unsigned i = 0; i < 32; ++i) {
        output[i * 2] = digits[digest[i] >> 4];
        output[i * 2 + 1] = digits[digest[i] & 15];
    }
    output[64] = 0;
}

void ExpectHash(const unsigned char* data, std::size_t count,
                const char* expected, const char* why) {
    unsigned char digest[32]{};
    HashBytes(data, count, digest);
    char actual[65]; Hex(digest, actual);
    Check(std::strcmp(actual, expected) == 0, why);
}

void CheckUnchanged(const FileIdentity& id, const char* why) {
    FileIdentity sentinel{};
    std::memset(&sentinel, 0xa5, sizeof(sentinel));
    Check(std::memcmp(&id, &sentinel, sizeof(id)) == 0, why);
}

void WriteFixture(const wchar_t* path, const unsigned char* bytes, DWORD size) {
    const HANDLE file = CreateFileW(path, GENERIC_WRITE, 0, nullptr,
                                    CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    Check(file != INVALID_HANDLE_VALUE, "create owned fixture");
    DWORD written = 0;
    const BOOL wrote = size == 0 || WriteFile(file, bytes, size, &written, nullptr);
    Check(wrote && written == size, "write whole owned fixture");
    Check(CloseHandle(file) != FALSE, "close owned fixture");
}

void ExpectFile(const wchar_t* path, std::uint64_t size,
                const char* expected, const char* why) {
    FileIdentity id{};
    std::memset(&id, 0xa5, sizeof(id));
    Check(ReadFileIdentity(path, size, id) == IdentityReadStatus::Ok, why);
    char actual[65]; Hex(id.sha256, actual);
    Check(id.size == size && std::strcmp(actual, expected) == 0, why);
}
} // namespace

int wmain(int argc, wchar_t** argv) {
    Check(argc == 3, "arguments: owned test directory, pinned installed EXE");
    const wchar_t* directory = argv[1];
    const wchar_t* gameExe = argv[2];
    wchar_t fixture[MAX_PATH];
    wchar_t missing[MAX_PATH];
    Check(swprintf_s(fixture, L"%ls\\identity-fixture.bin", directory) > 0,
          "owned fixture path");
    Check(swprintf_s(missing, L"%ls\\nonexistent-identity-fixture.bin", directory) > 0,
          "missing fixture path");

    ExpectHash(nullptr, 0,
        "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
        "SHA-256 empty vector");
    constexpr unsigned char abc[] = {'a', 'b', 'c'};
    ExpectHash(abc, sizeof(abc),
        "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",
        "SHA-256 abc vector");
    constexpr unsigned char multi[] =
        "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
    ExpectHash(multi, sizeof(multi) - 1,
        "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1",
        "SHA-256 standard multi-block vector");
    static unsigned char million[1000000];
    std::memset(million, 'a', sizeof(million));
    ExpectHash(million, sizeof(million),
        "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0",
        "SHA-256 million-a vector");

    static unsigned char binary[8193];
    for (std::size_t i = 0; i < sizeof(binary); ++i)
        binary[i] = static_cast<unsigned char>((i * 37u + 11u) & 255u);
    struct Vector { std::size_t size; const char* sha; };
    constexpr Vector vectors[] = {
        {1, "e7cf46a078fed4fafd0b5e3aff144802b853f8ae459a4f0c14add3314b7cc3a6"},
        {55, "2900465fcb533e05a158fd2b3be0e5e3b03740d83060aa3580e0d98a96bf2384"},
        {56, "31454ff48ef36af2f08fd511bdc37d9d5855ac23e992e5ff5445cb6b7674a674"},
        {63, "5f6401b96532c36de4e65beec0409b69b1d181864c8009b7a04f43e5d56350d1"},
        {64, "94eb5de4943613fd048dc93393ab06877405faa39c11f53e9386083339833e7e"},
        {65, "fc518669b6eb4b4dd91827ecacef86689c725bd5bab888fd3b26dbb196eec954"},
        {127, "0fe729ff19257bd6fec853acc2ea355f6b34b58e6c0f684c3e188fcdfcd9baae"},
        {4096, "4e441a3533bb2c10cd5649981d395744213e09a336746b5a3458fee4057205ec"},
        {4097, "042a02b55ba342cdd30322331816b6e4bca073f33bdb80149dc74a6f949802f4"},
        {8193, "d37426b71db09954cacd4d2be0059f5dbd7ad5d32fc42ab0eeda4ed1a4e1bebc"}
    };
    for (const auto& v : vectors) {
        ExpectHash(binary, v.size, v.sha, "SHA-256 varied binary length");
        WriteFixture(fixture, binary, static_cast<DWORD>(v.size));
        ExpectFile(fixture, v.size, v.sha, "streamed file identity matches independent hashlib");
    }

    FileIdentity output{};
    std::memset(&output, 0xa5, sizeof(output));
    Check(ReadFileIdentity(nullptr, 1, output) == IdentityReadStatus::PathUnavailable,
          "null path rejected");
    CheckUnchanged(output, "null path withholds output");
    Check(ReadFileIdentity(L"", 1, output) == IdentityReadStatus::PathUnavailable,
          "empty path rejected");
    CheckUnchanged(output, "empty path withholds output");
    static wchar_t unterminated[32768];
    for (auto& character : unterminated) character = L'x';
    Check(ReadFileIdentity(unterminated, 1, output) == IdentityReadStatus::PathUnavailable,
          "overlong path rejected before OS call");
    CheckUnchanged(output, "overlong path withholds output");
    Check(ReadFileIdentity(missing, 1, output) == IdentityReadStatus::OpenFailed,
          "missing file rejected");
    CheckUnchanged(output, "missing file withholds output");
    Check(ReadFileIdentity(fixture, 8192, output) == IdentityReadStatus::SizeMismatch,
          "wrong exact size rejected");
    CheckUnchanged(output, "wrong size withholds output");
    Check(ReadFileIdentity(fixture, 0, output) == IdentityReadStatus::SizeMismatch,
          "zero expected size rejected before open");
    CheckUnchanged(output, "zero expected size withholds output");
    Check(ReadFileIdentity(fixture, 64ull * 1024ull * 1024ull + 1,
                           output) == IdentityReadStatus::SizeMismatch,
          "oversized expected size rejected before open");
    CheckUnchanged(output, "oversized expected size withholds output");
    WriteFixture(fixture, binary, 0);
    Check(ReadFileIdentity(fixture, 1, output) == IdentityReadStatus::SizeMismatch,
          "empty file rejected");
    CheckUnchanged(output, "empty file withholds output");
    const HANDLE held = CreateFileW(fixture, GENERIC_READ, 0, nullptr,
                                    OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    Check(held != INVALID_HANDLE_VALUE, "open owned fixture exclusively");
    Check(ReadFileIdentity(fixture, 1, output) == IdentityReadStatus::OpenFailed,
          "sharing violation reported as open failure");
    CheckUnchanged(output, "sharing violation withholds output");
    Check(CloseHandle(held) != FALSE, "close exclusive fixture handle");
    Check(DeleteFileW(fixture) != FALSE, "delete only owned fixture");

    ExpectFile(gameExe, 12294656ull,
        "62d48c204364dd706584777c2e2b3c7ab3c5f1dd0170872554943575d53d6648",
        "pinned installed executable disk identity, read-only");
    Check(std::strcmp(IdentityReadReason(IdentityReadStatus::ChangedDuringRead),
                      "executable file changed during identity read") == 0,
          "changed-file reason is explicit");
    std::printf("PASS startup identity: %d checks; pinned installed EXE read-only\n", checks);
    return 0;
}
