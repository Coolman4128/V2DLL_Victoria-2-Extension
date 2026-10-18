// Headless public-Windows-API sample in this owned console process only.
// No game/process-open/debugger/window APIs and no production integration.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <xmmintrin.h>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

namespace {
unsigned short ReadControlWord() {
    unsigned short value = 0;
    __asm fnstcw value
    return value;
}
void WriteControlWord(unsigned short value) {
    __asm fldcw value
}
struct RestoreFloatingState {
    unsigned short cw = ReadControlWord();
    unsigned int mxcsr = _mm_getcsr();
    ~RestoreFloatingState() { WriteControlWord(cw); _mm_setcsr(mxcsr); }
};
std::size_t checks = 0;
std::size_t failures = 0;
void Check(bool value, const char* contract) {
    ++checks;
    if (!value) { ++failures; std::fprintf(stderr, "FAIL %s\n", contract); }
}
std::string ModulePath(HMODULE module) {
    std::array<wchar_t, 32768> path{};
    const DWORD length = GetModuleFileNameW(module, path.data(), static_cast<DWORD>(path.size()));
    if (!length || length >= path.size()) return {};
    const int bytes = WideCharToMultiByte(CP_UTF8, 0, path.data(), static_cast<int>(length),
        nullptr, 0, nullptr, nullptr);
    if (bytes <= 0) return {};
    std::string result(static_cast<std::size_t>(bytes), '\0');
    if (WideCharToMultiByte(CP_UTF8, 0, path.data(), static_cast<int>(length),
        result.data(), bytes, nullptr, nullptr) != bytes) return {};
    return result;
}
void JsonString(const std::string& value) {
    std::putchar('"');
    for (unsigned char ch : value) {
        if (ch == '"' || ch == '\\') { std::putchar('\\'); std::putchar(ch); }
        else if (ch < 0x20) std::printf("\\u%04x", static_cast<unsigned>(ch));
        else std::putchar(ch);
    }
    std::putchar('"');
}

// Only this process's normally loaded USER32 image is inspected. This is not
// an engine resolver and has no reusable game/process memory capability.
FARPROC User32AnsiImport(HMODULE user32) {
    auto* base = reinterpret_cast<unsigned char*>(user32);
    auto* dos = reinterpret_cast<IMAGE_DOS_HEADER*>(base);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew <= 0) return nullptr;
    auto* pe = reinterpret_cast<IMAGE_NT_HEADERS32*>(base + dos->e_lfanew);
    if (pe->Signature != IMAGE_NT_SIGNATURE || pe->FileHeader.Machine != IMAGE_FILE_MACHINE_I386 ||
        pe->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR32_MAGIC) return nullptr;
    const auto directory = pe->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    const auto imageSize = pe->OptionalHeader.SizeOfImage;
    const auto span = [imageSize](DWORD rva, std::size_t length) {
        return rva < imageSize && length <= imageSize - rva;
    };
    const auto named = [base, &span, imageSize](DWORD rva, const char* expected) {
        if (!span(rva, 1)) return false;
        const char* value = reinterpret_cast<const char*>(base + rva);
        return std::memchr(value, '\0', imageSize - rva) && _stricmp(value, expected) == 0;
    };
    if (directory.Size < sizeof(IMAGE_IMPORT_DESCRIPTOR) ||
        !span(directory.VirtualAddress, directory.Size)) return nullptr;
    auto* imports = reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(base + directory.VirtualAddress);
    const std::size_t count = directory.Size / sizeof(IMAGE_IMPORT_DESCRIPTOR);
    for (std::size_t n = 0; n < count && imports[n].Name; ++n) {
        if (!named(imports[n].Name, "api-ms-win-core-stringansi-l1-1-0.dll")) continue;
        if (!span(imports[n].OriginalFirstThunk, sizeof(IMAGE_THUNK_DATA32)) ||
            !span(imports[n].FirstThunk, sizeof(IMAGE_THUNK_DATA32))) return nullptr;
        auto* names = reinterpret_cast<IMAGE_THUNK_DATA32*>(base + imports[n].OriginalFirstThunk);
        auto* resolved = reinterpret_cast<IMAGE_THUNK_DATA32*>(base + imports[n].FirstThunk);
        // Trusted loader-created system image, bounded additionally to its extent.
        const std::size_t nameLimit = (imageSize - imports[n].OriginalFirstThunk) / sizeof(IMAGE_THUNK_DATA32);
        const std::size_t addressLimit = (imageSize - imports[n].FirstThunk) / sizeof(IMAGE_THUNK_DATA32);
        const std::size_t limit = nameLimit < addressLimit ? nameLimit : addressLimit;
        for (std::size_t i = 0; i < limit && names[i].u1.AddressOfData; ++i) {
            if (IMAGE_SNAP_BY_ORDINAL32(names[i].u1.Ordinal)) continue;
            const auto nameRva = names[i].u1.AddressOfData;
            if (span(nameRva, sizeof(WORD) + 1) && named(nameRva + sizeof(WORD), "CharLowerBuffA"))
                return reinterpret_cast<FARPROC>(static_cast<std::uintptr_t>(resolved[i].u1.Function));
        }
    }
    return nullptr;
}
}

int main() {
    static_assert(sizeof(void*) == 4, "Win32 sample only");
    RestoreFloatingState restore;
    const HMODULE user32 = GetModuleHandleW(L"user32.dll");
    if (!user32) { std::fprintf(stderr, "USER32 not loaded\n"); return 1; }
    const FARPROC exported = GetProcAddress(user32, "CharLowerBuffA");
    const HMODULE apiSet = LoadLibraryExW(L"api-ms-win-core-stringansi-l1-1-0.dll",
        nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (!apiSet || !exported) { std::fprintf(stderr, "Public export/provider unavailable\n"); return 1; }
    const FARPROC provider = GetProcAddress(apiSet, "CharLowerBuffA");
    const FARPROC importedProvider = User32AnsiImport(user32);
    HMODULE owner = nullptr;
    const bool ownerFound = provider && GetModuleHandleExA(
        GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        reinterpret_cast<LPCSTR>(provider), &owner) != 0;
    const std::string user32Path = ModulePath(user32);
    const std::string providerPath = ownerFound ? ModulePath(owner) : std::string{};
    Check(exported == reinterpret_cast<FARPROC>(&CharLowerBuffA), "import equals USER32 public export");
    Check(provider && importedProvider == provider, "USER32 API-set IAT equals resolved provider");
    Check(ownerFound && !providerPath.empty() && !user32Path.empty(), "provider owner and paths resolved");
    if (failures) { FreeLibrary(apiSet); return 1; }

    struct Key { const char* source; const char* expected; };
    const std::array<Key, 7> keys{{
        {"FE_ACADEMIES_BDSM", "fe_academies_bdsm"},
        {"FE_RPROJECTS_BDSM", "fe_rprojects_bdsm"},
        {"FE_BUDGET_DIPLO_BDSM", "fe_budget_diplo_bdsm"},
        {"hide_colonial_states", "hide_colonial_states"},
        {"selected_tech_window", "selected_tech_window"},
        {"factory_buttons", "factory_buttons"},
        {"start", "start"},
    }};
    struct Mode { const char* name; unsigned short cw; unsigned int mxcsr; };
    const std::array<Mode, 3> modes{{
        {"process-entry", restore.cw, restore.mxcsr},
        {"native-sample-controls", 0x007F, 0x1F80},
        {"masked-round-toward-zero", 0x0C7F, 0x7F80},
    }};
    constexpr unsigned int MxcsrControlMask = 0xFFC0; // status bits0..5 are not control bits.
    std::size_t calls = 0;
    for (const auto& mode : modes) {
        for (const auto& key : keys) {
            std::array<char, 128> buffer{};
            const auto length = std::strlen(key.source);
            buffer[0] = '[';
            std::memcpy(buffer.data() + 1, key.source, length);
            buffer[length + 1] = ']';
            WriteControlWord(mode.cw);
            _mm_setcsr(mode.mxcsr);
            const auto beforeCw = ReadControlWord();
            const auto beforeMxcsr = _mm_getcsr();
            const DWORD changed = CharLowerBuffA(buffer.data() + 1, static_cast<DWORD>(length));
            const auto afterCw = ReadControlWord();
            const auto afterMxcsr = _mm_getcsr();
            // Restore before assertions/stdio, outside the measured API interval.
            WriteControlWord(restore.cw);
            _mm_setcsr(restore.mxcsr);
            ++calls;
            Check(beforeCw == mode.cw, "requested x87 control installed in owned process");
            Check((beforeMxcsr & MxcsrControlMask) == (mode.mxcsr & MxcsrControlMask),
                "requested MXCSR controls installed in owned process");
            Check(changed == length, "native lowercase reports exact span");
            Check(std::memcmp(buffer.data() + 1, key.expected, length) == 0, "pinned ASCII lowercase result");
            Check(buffer[0] == '[' && buffer[length + 1] == ']' && buffer[length + 2] == '\0',
                "outside-span guards preserved");
            Check(beforeCw == afterCw, "x87 control word preserved across public API sample");
            Check((beforeMxcsr & MxcsrControlMask) == (afterMxcsr & MxcsrControlMask),
                "MXCSR control bits preserved across public API sample");
        }
    }
    std::printf("{\n  \"scope\": \"Owned headless Win32 public OS API; pinned ASCII samples only\",\n");
    std::printf("  \"user32_path\": "); JsonString(user32Path);
    std::printf(",\n  \"provider_path\": "); JsonString(providerPath);
    std::printf(",\n  \"api_set\": \"api-ms-win-core-stringansi-l1-1-0.dll\",\n");
    std::printf("  \"modes\": [\n");
    for (std::size_t n = 0; n < modes.size(); ++n) {
        std::printf("    {\"name\": "); JsonString(modes[n].name);
        std::printf(", \"x87_cw\": \"0x%04X\", \"mxcsr\": \"0x%08X\"}%s\n",
            static_cast<unsigned>(modes[n].cw), modes[n].mxcsr, n + 1 == modes.size() ? "" : ",");
    }
    std::printf("  ],\n");
    std::printf("  \"provider_matches_user32_iat\": true,\n  \"calls\": %zu,\n  \"checks\": %zu,\n  \"failures\": %zu,\n",
        calls, checks, failures);
    std::printf("  \"mxcsr_control_mask\": \"0xFFC0\",\n  \"status_bits_equivalence_claimed\": false,\n");
    std::printf("  \"runtime_profile_status\": \"unavailable\",\n  \"candidate_loaded\": false\n}\n");
    FreeLibrary(apiSet);
    return failures ? 1 : 0;
}
