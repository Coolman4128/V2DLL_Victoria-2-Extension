// V2DLL_MpSync.cpp - performance + multiplayer-sync patches for v2game.exe 3.04/3.05
// (same exe; the "3.05" mod only rewrites the version label).
//
//   skip_network_wait   poll the network without blocking (vanilla: 15-35 ms per frame)
//   skip_clean_resets   skip Country_PrePass's per-country table reset when it writes nothing new
//   state_checksum      append state hashes to the daily sync vector (silent desyncs become visible)
//   oos_log             name the sync-vector element that differed (written to the DLL log)
//   fix_rng_resync      on a sync mismatch realign the RNG to the host's position, not to the seed
//   sync_world_rng      lockstep draws of the clock-seeded world RNG come from the shared, counted one
//   mp_host_wait_days   the host waits for the slowest player instead of racing ahead
//   pin_fpu_every_tick  re-apply the DLL's FPU settings at the start of every hourly tick
//
// Multiplayer options must have the SAME value for every player of a game.
//
// Adapted from "The Grand Loader" (The Grand Combination), whose reverse engineering of v2game.exe
// these patches follow. Addresses are RVAs (VA - 0x00400000).
//
// Entry point: InstallMpSync() (see V2DLL_Shared.h). Called once from Install().
#include <windows.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include "V2DLL_Shared.h"

namespace {

// ---------------------------------------------------------------------------------------------
// Memory helpers
// ---------------------------------------------------------------------------------------------

inline uintptr_t exe_addr(uint32_t rva) { return (uintptr_t)g_base + rva; }
inline uint32_t read32(uintptr_t at) { return *reinterpret_cast<const uint32_t*>(at); }

bool expect_bytes(uintptr_t at, const void* expected, size_t size)
{
    return memcmp(reinterpret_cast<const void*>(at), expected, size) == 0;
}

bool write_bytes(uintptr_t at, const void* src, size_t size)
{
    void* dst = reinterpret_cast<void*>(at);
    DWORD old = 0;
    if (!VirtualProtect(dst, size, PAGE_EXECUTE_READWRITE, &old))
        return false;
    memcpy(dst, src, size);
    VirtualProtect(dst, size, old, &old);
    FlushInstructionCache(GetCurrentProcess(), dst, size);
    return true;
}

bool replace_pointer(uintptr_t slot, uintptr_t expected, uintptr_t replacement)
{
    if (!expect_bytes(slot, &expected, sizeof(expected)))
        return false;
    return write_bytes(slot, &replacement, sizeof(replacement));
}

// Redirects the 5-byte CALL rel32 at `site` to `target` when it currently calls `expected_target`.
bool redirect_call(uintptr_t site, uintptr_t expected_target, uintptr_t target)
{
    unsigned char expected[5] = { 0xE8 };
    int32_t rel = (int32_t)(expected_target - (site + 5));
    memcpy(expected + 1, &rel, sizeof(rel));
    if (!expect_bytes(site, expected, sizeof(expected)))
        return false;
    unsigned char patched[5] = { 0xE8 };
    rel = (int32_t)(target - (site + 5));
    memcpy(patched + 1, &rel, sizeof(rel));
    return write_bytes(site, patched, sizeof(patched));
}

// Target of the CALL rel32 at `site`, or 0 when there is no CALL there.
uintptr_t call_target(uintptr_t site)
{
    if (*reinterpret_cast<const unsigned char*>(site) != 0xE8)
        return 0;
    int32_t rel;
    memcpy(&rel, reinterpret_cast<const void*>(site + 1), sizeof(rel));
    return site + 5 + (uintptr_t)rel;
}

// ---------------------------------------------------------------------------------------------
// Game layout (docs/re/systems/multiplayer.md of The Grand Loader)
// ---------------------------------------------------------------------------------------------

const uint32_t kWorldPtrRva = 0xE588E8;           // CWorld*
const uint32_t kCountryManagerPtrRva = 0xE587E4;  // +0 count, +4 CCountry* array
const uint32_t kOperatorNewRva = 0x6AE9AF;        // cdecl
const uint32_t kOperatorDeleteRva = 0x6AE91B;     // cdecl
const uint32_t kRngCallsRva = 0xB20C14;           // g_rngCalls (draw counter of the global generator)

const uint32_t kWorldProvinces = 0xACC;           // vector<CProvince*> begin
const uint32_t kWorldProvincesEnd = 0xAD0;
const uint32_t kWorldDate = 0xB0C;                // hours
const uint32_t kWorldSyncVector = 0xB74;          // vector<int>: the sync check
const uint32_t kWorldIdler = 0xB24;
const uint32_t kWorldPlayers = 0x18;

const uint32_t kProvinceOwner = 0x12C;
const uint32_t kProvinceController = 0x134;
const uint32_t kProvincePopLists = 0x194;
const uint32_t kProvincePopListsEnd = 0x198;

const uint32_t kCountryMoney = 0xE78;             // int64
const uint32_t kCountryProvinces = 0x9D8;
const uint32_t kCountryProvincesEnd = 0x9DC;

const uint32_t kPopId = 0x0C;
const uint32_t kPopSize = 0x58;
const uint32_t kPopNext = 0x27C;

const uint32_t kInGameIdlerVtableRva = 0xA0A34C;
const uint32_t kIdlerSessionManagerSlot = 0x44;
const uint32_t kSessionManagerIsServer = 0x34;
const uint32_t kSessionManagerExecuting = 0x234;

inline uintptr_t world() { return read32(exe_addr(kWorldPtrRva)); }
inline uintptr_t country_manager() { return read32(exe_addr(kCountryManagerPtrRva)); }

// Session manager of the in-game idler, or 0 outside the game.
uintptr_t session_manager(uintptr_t w)
{
    uintptr_t idler = read32(w + kWorldIdler);
    if (!idler || read32(idler) != exe_addr(kInGameIdlerVtableRva))
        return 0;
    typedef uintptr_t(__thiscall* GetFn)(uintptr_t);
    return reinterpret_cast<GetFn>(read32(read32(idler) + kIdlerSessionManagerSlot))(idler);
}

// ---------------------------------------------------------------------------------------------
// Hourly tick hook (runs callbacks at the start of every game hour, game main thread)
//   CHourlyTickCommand::vftable 0xA12414, slot 6 (+0x18) = Execute 0x32ECF0
// ---------------------------------------------------------------------------------------------

const uint32_t kHourlySlotRva = 0xA12414 + 0x18;
const uint32_t kHourlyExecuteRva = 0x32ECF0;
const int kMaxHourlyCallbacks = 4;

void (*g_hourlyCallbacks[kMaxHourlyCallbacks])() = {};
int g_hourlyCount = 0;
uintptr_t g_originalHourlyExecute = 0;

void hourly_dispatch()
{
    for (int i = 0; i < g_hourlyCount; i++)
        g_hourlyCallbacks[i]();
}

__declspec(naked) void hourly_thunk()
{
    __asm {
        pushad
        pushfd
        call hourly_dispatch
        popfd
        popad
        jmp dword ptr [g_originalHourlyExecute]
    }
}

bool on_hourly_tick(void (*fn)())
{
    if (g_hourlyCount >= kMaxHourlyCallbacks)
        return false;
    if (g_originalHourlyExecute == 0)
    {
        uintptr_t slot = exe_addr(kHourlySlotRva);
        uintptr_t current = *reinterpret_cast<const uintptr_t*>(slot);
        if (current != exe_addr(kHourlyExecuteRva) ||
            !replace_pointer(slot, current, reinterpret_cast<uintptr_t>(&hourly_thunk)))
        {
            Log("MpSync: CHourlyTickCommand::Execute slot not recognised");
            return false;
        }
        g_originalHourlyExecute = current;
    }
    g_hourlyCallbacks[g_hourlyCount++] = fn;
    return true;
}

// ---------------------------------------------------------------------------------------------
// skip_network_wait
//   CommandQueue_Process -> ... -> network class: CALL enet_host_service (0x689D00, cdecl, 3 args)
//   at 0x6618CC. The base constructor sets the timeout to 15 ms, client/server to 35 ms. The call is
//   forwarded with a timeout of 0; every packet is still received, a frame later at most.
// ---------------------------------------------------------------------------------------------

const uint32_t kServiceCallRva = 0x6618CC;
const uint32_t kHostServiceRva = 0x689D00;

typedef int(__cdecl* HostServiceFn)(void* host, void* event, uint32_t timeout);
HostServiceFn g_hostService = 0;
bool g_netLogged = false;

int __cdecl non_blocking_host_service(void* host, void* event, uint32_t timeout)
{
    if (!g_netLogged)
    {
        g_netLogged = true;
        Log("skip_network_wait: first poll, the game asked for a %u ms wait, polling without one", timeout);
    }
    return g_hostService(host, event, 0);
}

void install_network_wait()
{
    g_hostService = reinterpret_cast<HostServiceFn>(exe_addr(kHostServiceRva));
    if (!redirect_call(exe_addr(kServiceCallRva), exe_addr(kHostServiceRva),
                       reinterpret_cast<uintptr_t>(&non_blocking_host_service)))
    {
        Log("skip_network_wait: call site %08X not recognised or already hooked, off", kServiceCallRva + 0x400000);
        return;
    }
    Log("skip_network_wait: active, the per-frame network poll no longer blocks");
}

// ---------------------------------------------------------------------------------------------
// skip_clean_resets
//   Country_PrePass runs for all 683 tags every day. At 0x10861B it calls FUN_0051FD10(country), which
//   resets two per-country tables (country+0x1514 and country+0x14F4) to the constant pair at
//   0xFF3688/0xFF368C, resizing them first when their sizes do not match: ~30 KB written per tag per
//   day. For a tag whose tables already hold the constant everywhere (a dead tag nothing wrote to),
//   the call changes nothing, so it is skipped: same memory, same results.
// ---------------------------------------------------------------------------------------------

const uint32_t kResetCallRva = 0x10861B;
const uint32_t kResetRva = 0x11FD10;
const uint32_t kResetValueRva = 0xFF3688;
const uint32_t kGoodsSourceRva = 0xE587F0;
const uint32_t kTypesSourceRva = 0xE5CE80;
const uint32_t kCountryRow = 0x1514;
const uint32_t kCountryMatrix = 0x14F4;
const uint32_t kEntrySize = 0x18;
const uint64_t kResetReportEvery = 683 * 30;

uintptr_t g_reset = 0;
uintptr_t g_resetValue = 0;
uintptr_t g_goodsSource = 0;
uintptr_t g_typesSource = 0;
uint8_t g_skip = 0;
uint64_t g_resetCalls = 0;
uint64_t g_resetSkipped = 0;

inline int32_t vector_count(uintptr_t vec, uint32_t element)
{
    return (int32_t)((read32(vec + 4) - read32(vec)) / element);
}

// True when every entry of `count` 0x18-byte entries at `row` already holds the reset pair.
bool row_clean(uintptr_t row, int32_t count, uint32_t lo, uint32_t hi)
{
    const uint32_t* p = reinterpret_cast<const uint32_t*>(row);
    for (int32_t i = 0; i < count; i++, p += 6)
    {
        if (p[0] != lo || p[1] != hi || p[2] != lo || p[3] != hi || p[4] != lo || p[5] != hi)
            return false;
    }
    return true;
}

// Whether FUN_0051FD10(country) would leave memory exactly as it is.
bool tables_clean(uintptr_t country)
{
    uintptr_t goodsSource = read32(g_goodsSource);
    uintptr_t typesSource = read32(g_typesSource);
    if (!goodsSource || !typesSource)
        return false;   // the call would create the lazy singleton
    int32_t goods = (int32_t)((read32(goodsSource + 0x10) - read32(goodsSource + 0xC)) >> 2);
    int32_t types = (int32_t)((read32(typesSource + 0x10) - read32(typesSource + 0xC)) >> 2);
    if (vector_count(country + kCountryRow, kEntrySize) != goods ||
        vector_count(country + kCountryMatrix, 16) != types)
        return false;   // the call would resize
    uint32_t lo = read32(g_resetValue), hi = read32(g_resetValue + 4);
    if (!row_clean(read32(country + kCountryRow), goods, lo, hi))
        return false;
    uintptr_t headers = read32(country + kCountryMatrix);
    for (int32_t t = 0; t < types; t++)
    {
        if (!row_clean(read32(headers + (uint32_t)t * 16), goods, lo, hi))
            return false;
    }
    return true;
}

// Returns 1 when the reset can be skipped.
uint8_t __stdcall before_reset(uintptr_t country)
{
    if (!country)
        return 0;
    uint8_t skip = tables_clean(country) ? 1 : 0;
    g_resetSkipped += skip;
    if (++g_resetCalls % kResetReportEvery == 0)
        Log("skip_clean_resets: %llu of %llu per-country table resets skipped (already clean)",
            (unsigned long long)g_resetSkipped, (unsigned long long)g_resetCalls);
    return skip;
}

// Stack at entry: [ret][country]. After pushad (32) + pushfd (4), country is at [esp+40].
__declspec(naked) void reset_thunk()
{
    __asm {
        pushad
        pushfd
        push dword ptr [esp + 40]
        call before_reset
        mov byte ptr [g_skip], al
        popfd
        popad
        cmp byte ptr [g_skip], 0
        jne skip
        jmp dword ptr [g_reset]
    skip:
        ret 4                           ; stdcall, as the original
    }
}

void install_clean_resets()
{
    g_reset = exe_addr(kResetRva);
    g_resetValue = exe_addr(kResetValueRva);
    g_goodsSource = exe_addr(kGoodsSourceRva);
    g_typesSource = exe_addr(kTypesSourceRva);
    if (!expect_bytes(g_reset, "\x55\x8B\xEC\x6A\xFF", 5) ||
        !redirect_call(exe_addr(kResetCallRva), g_reset, reinterpret_cast<uintptr_t>(&reset_thunk)))
    {
        Log("skip_clean_resets: call site %08X not recognised or already hooked, off", kResetCallRva + 0x400000);
        return;
    }
    Log("skip_clean_resets: active");
}

// ---------------------------------------------------------------------------------------------
// Sync vector: names, oos_log report, sync_check (compare + resync hooks), state_checksum
//
//   Update_World_and_Advance_Time (0x282EC0), on every machine, per hourly tick command:
//     0x283036  CALL Vector_Equals(local world+0xB74, host command+0x3C)  returns 1 when they differ
//     mismatch path: g_rngCalls = host[0], seed = host[1], g_rngCalls = 0,
//     0x28364D  CALL Rng_Resync (0x5B7C80)   reseeds from the seed and advances g_rngCalls draws
//     0x283AB1  CALL DayTick (0x2859C0)
//   The game's own vector holds two elements (draw count, seed). state_checksum appends five more
//   after DayTick; the game's comparison is limited to its own two (so the extra ones never trigger
//   the OOS reaction), and the rest is compared and logged here.
// ---------------------------------------------------------------------------------------------

const char* const kNames[] = { "rng_calls", "rng_seed", "rng_calls_end", "provinces", "countries", "pops", "date" };

const char* sync_element_name(size_t index)
{
    return index < sizeof(kNames) / sizeof(kNames[0]) ? kNames[index] : "extra";
}

size_t sync_element_index(const char* name)
{
    for (size_t i = 0; i < sizeof(kNames) / sizeof(kNames[0]); i++)
        if (strcmp(kNames[i], name) == 0)
            return i;
    return (size_t)-1;
}

// ---- oos_log ----

const int kFullLogs = 20;        // mismatching ticks logged in full
const int kThenEveryDays = 30;   // afterwards one line per this many days

bool g_oosEnabled = false;
int g_mismatches = 0;
int32_t g_lastLoggedDay = -1000000;
bool g_fpuLogged = false;

int32_t current_day()
{
    uintptr_t w = world();
    return w ? (int32_t)read32(w + kWorldDate) / 24 : -1;
}

void log_fpu_once()
{
    if (g_fpuLogged)
        return;
    g_fpuLogged = true;
    uint16_t cw = 0;
    uint32_t csr = 0;
    __asm {
        fnstcw cw
        stmxcsr csr
    }
    const char* precision = (cw & 0x300) == 0x000 ? "24-bit" : (cw & 0x300) == 0x200 ? "53-bit" : "64-bit";
    Log("oos_log: game thread x87 control word %04X (%s precision), MXCSR %08X", cw, precision, csr);
}

void report_sync_vectors(const uint32_t* a, size_t nLocal, const uint32_t* b, size_t nHost)
{
    if (!g_oosEnabled)
        return;
    log_fpu_once();
    bool differ = nLocal != nHost;
    bool gameSeesIt = false;
    for (size_t i = 0; !differ && i < nLocal; i++)
        differ = a[i] != b[i];
    if (!differ)
        return;
    for (size_t i = 0; i < 2 && i < nLocal && i < nHost; i++)
        gameSeesIt = gameSeesIt || a[i] != b[i];
    int32_t day = current_day();
    g_mismatches++;
    bool full = g_mismatches <= kFullLogs;
    if (!full && day - g_lastLoggedDay < kThenEveryDays)
        return;
    g_lastLoggedDay = day;
    if (nLocal != nHost)
    {
        Log("oos_log: day %d: sync vector sizes differ, local %u elements, host %u: the host and this client do "
            "not run the same patch settings (STATE_CHECKSUM must be the same for everyone)",
            day, (unsigned)nLocal, (unsigned)nHost);
        return;
    }
    Log("oos_log: day %d: %s (mismatch #%d)", day,
        gameSeesIt ? "OUT OF SYNC (the game reacts: RNG realign, dialog, pause)"
                   : "SILENT DIVERGENCE (state differs, the game does not react)",
        g_mismatches);
    if (!full)
        return;
    for (size_t i = 0; i < nLocal; i++)
        if (a[i] != b[i])
            Log("oos_log:   %s: local %08X, host %08X", sync_element_name(i), a[i], b[i]);
}

// ---- sync_check ----

const uint32_t kEqualsCallRva = 0x283036;
const uint32_t kEqualsRva = 0x28F340;
const uint32_t kResyncCallRva = 0x28364D;
const uint32_t kResyncRva = 0x5B7C80;
const size_t kGameElements = 2;

struct IntVector { uint32_t begin, end, capacity; };

uintptr_t g_originalEquals = 0;
uintptr_t g_originalResync = 0;
bool g_compareHooked = false;
uint32_t g_hostCopy[64];
size_t g_hostSize = 0;
IntVector* g_local = 0;
IntVector* g_host = 0;
uint32_t g_savedLocalEnd = 0;
uint32_t g_savedHostEnd = 0;

// Before the game's compare: keep a copy of the host vector for the resync and the report, then
// shorten both vectors to the two game elements so that Vector_Equals only sees those.
void __cdecl before_compare(IntVector* local, IntVector* host)
{
    g_local = local;
    g_host = host;
    size_t nHost = (host->end - host->begin) / 4;
    g_hostSize = nHost < 64 ? nHost : 64;
    memcpy(g_hostCopy, reinterpret_cast<const void*>(host->begin), g_hostSize * 4);
    report_sync_vectors(reinterpret_cast<const uint32_t*>(local->begin), (local->end - local->begin) / 4,
                        g_hostCopy, g_hostSize);
    g_savedLocalEnd = local->end;
    g_savedHostEnd = host->end;
    size_t nLocal = (local->end - local->begin) / 4;
    if (nLocal >= kGameElements && nHost >= kGameElements)
    {
        local->end = local->begin + kGameElements * 4;
        host->end = host->begin + kGameElements * 4;
    }
}

void __cdecl after_compare()
{
    if (g_local)
    {
        g_local->end = g_savedLocalEnd;
        g_host->end = g_savedHostEnd;
    }
}

__declspec(naked) void equals_thunk()
{
    __asm {
        pushad
        pushfd
        push ecx
        push eax
        call before_compare
        add esp, 8
        popfd
        popad
        call dword ptr [g_originalEquals]
        push eax
        pushad
        pushfd
        call after_compare
        popfd
        popad
        pop eax
        ret
    }
}

// In the mismatch path, right before Rng_Resync: the game has just set g_rngCalls = 0. Replace it
// with the host's position: `rng_calls_end` when the host runs state_checksum, else the count it sent
// (one day behind, but not zero).
void __cdecl before_resync()
{
    uint32_t* calls = reinterpret_cast<uint32_t*>(exe_addr(kRngCallsRva));
    uint32_t target = 0;
    const char* source = "nothing (vanilla: restart from the seed)";
    size_t endIndex = sync_element_index("rng_calls_end");
    if (g_hostSize > endIndex && endIndex != (size_t)-1)
    {
        target = g_hostCopy[endIndex];
        source = "host rng_calls_end";
    }
    else if (g_hostSize > 0)
    {
        target = g_hostCopy[0];
        source = "host rng_calls at its day start (no rng_calls_end in its vector)";
    }
    Log("fix_rng_resync: realigning the global RNG to %u draws from the seed (%s)", target, source);
    *calls = target;
}

__declspec(naked) void resync_thunk()
{
    __asm {
        pushad
        pushfd
        call before_resync
        popfd
        popad
        jmp dword ptr [g_originalResync]
    }
}

bool hook_call(uint32_t siteRva, uint32_t expectedRva, uintptr_t& original, void* thunk, const char* what)
{
    uintptr_t site = exe_addr(siteRva);
    uintptr_t current = call_target(site);
    if (!current)
    {
        Log("sync_check: no CALL at %08X (%s), off", siteRva + 0x400000, what);
        return false;
    }
    if (current != exe_addr(expectedRva))
        Log("sync_check: the %s call site is already redirected; chaining to it", what);
    original = current;
    if (!redirect_call(site, current, reinterpret_cast<uintptr_t>(thunk)))
    {
        Log("sync_check: could not patch the %s call site, off", what);
        return false;
    }
    return true;
}

// ---- state_checksum ----

const uint32_t kDayTickCallRva = 0x283AB1;
const uint32_t kDayTickRva = 0x2859C0;
const size_t kMinCapacity = 16;
const uint32_t kMaxDepth = 8;

typedef void*(__cdecl* AllocFn)(size_t);
typedef void(__cdecl* FreeFn)(void*);
AllocFn g_gameNew = 0;
FreeFn g_gameDelete = 0;

uintptr_t g_originalDayTick = 0;
uintptr_t g_savedReturn[kMaxDepth];
uint32_t g_depth = 0;
bool g_loggedFirst = false;

inline uint32_t fnv(uint32_t h, uint32_t v)
{
    for (int i = 0; i < 4; i++)   // FNV-1a over the four bytes of v
    {
        h ^= (v >> (8 * i)) & 0xFF;
        h *= 16777619u;
    }
    return h;
}

uint32_t hash_provinces(uintptr_t w)
{
    uint32_t h = 2166136261u;
    uintptr_t it = read32(w + kWorldProvinces);
    uintptr_t end = read32(w + kWorldProvincesEnd);
    for (; it < end; it += 4)
    {
        uintptr_t p = read32(it);
        if (!p)
        {
            h = fnv(h, 0xFFFFFFFFu);
            continue;
        }
        h = fnv(h, read32(p + kProvinceOwner));
        h = fnv(h, read32(p + kProvinceController));
    }
    return h;
}

uint32_t hash_countries()
{
    uint32_t h = 2166136261u;
    uintptr_t cm = country_manager();
    if (!cm)
        return h;
    uint32_t count = read32(cm);
    uintptr_t arr = read32(cm + 4);
    for (uint32_t i = 0; i < count; i++)
    {
        uintptr_t c = read32(arr + 4 * i);
        if (!c)
            continue;
        h = fnv(h, i);
        h = fnv(h, read32(c + kCountryMoney));
        h = fnv(h, read32(c + kCountryMoney + 4));
        h = fnv(h, (read32(c + kCountryProvincesEnd) - read32(c + kCountryProvinces)) / 4);
    }
    return h;
}

uint32_t hash_pops(uintptr_t w)
{
    uint32_t h = 2166136261u;
    uintptr_t it = read32(w + kWorldProvinces);
    uintptr_t end = read32(w + kWorldProvincesEnd);
    for (; it < end; it += 4)
    {
        uintptr_t p = read32(it);
        if (!p)
            continue;
        uintptr_t list = read32(p + kProvincePopLists);
        uintptr_t listEnd = read32(p + kProvincePopListsEnd);
        for (; list < listEnd; list += 16)
        {
            for (uintptr_t pop = read32(list); pop; pop = read32(pop + kPopNext))
            {
                h = fnv(h, read32(pop + kPopId));
                h = fnv(h, read32(pop + kPopSize));
            }
        }
    }
    return h;
}

// MSVC std::vector<int>, grown with the game's own operator new / delete.
void push_back(uintptr_t vec, uint32_t value)
{
    uint32_t* begin = reinterpret_cast<uint32_t*>(read32(vec));
    uint32_t* end = reinterpret_cast<uint32_t*>(read32(vec + 4));
    uint32_t* cap = reinterpret_cast<uint32_t*>(read32(vec + 8));
    if (end == cap)
    {
        size_t size = (size_t)(end - begin);
        size_t capacity = (size_t)(cap - begin);
        size_t wanted = capacity * 2 < kMinCapacity ? kMinCapacity : capacity * 2;
        uint32_t* fresh = static_cast<uint32_t*>(g_gameNew(wanted * sizeof(uint32_t)));
        if (size)
            memcpy(fresh, begin, size * sizeof(uint32_t));
        if (begin)
            g_gameDelete(begin);
        begin = fresh;
        end = fresh + size;
        cap = fresh + wanted;
        *reinterpret_cast<uint32_t**>(vec) = begin;
        *reinterpret_cast<uint32_t**>(vec + 8) = cap;
    }
    *end++ = value;
    *reinterpret_cast<uint32_t**>(vec + 4) = end;
}

void after_day_tick()
{
    uintptr_t w = world();
    if (!w)
        return;
    uintptr_t vec = w + kWorldSyncVector;
    uint32_t values[5] = {
        read32(exe_addr(kRngCallsRva)),   // global RNG position after this day: the host's position when a client compares
        hash_provinces(w),
        hash_countries(),
        hash_pops(w),
        read32(w + kWorldDate),
    };
    for (int i = 0; i < 5; i++)
        push_back(vec, values[i]);
    if (!g_loggedFirst)
    {
        g_loggedFirst = true;
        size_t n = (read32(vec + 4) - read32(vec)) / 4;
        Log("state_checksum: first day hashed, the sync vector now has %u elements", (unsigned)n);
    }
}

// Replaces the CALL DayTick. The site pushes the world and DayTick pops it (RET 4), so the return
// address is swapped for `after` and saved on a small stack of ours; `after` runs the hashing with
// every register preserved and returns to the site.
__declspec(naked) void day_tick_thunk()
{
    __asm {
        // stack: [return to site][world]
        push eax
        push ecx
        mov eax, [esp + 8]                 // return address
        mov ecx, g_depth
        mov [g_savedReturn + ecx * 4], eax
        inc ecx
        mov g_depth, ecx
        pop ecx
        pop eax
        mov dword ptr [esp], offset after  // DayTick returns here
        jmp dword ptr [g_originalDayTick]
    after:
        // DayTick's RET 4 popped the world; the site's return address is on our stack
        pushad
        pushfd
        call after_day_tick
        popfd
        popad
        push eax
        push ecx
        mov ecx, g_depth
        dec ecx
        mov g_depth, ecx
        mov eax, [g_savedReturn + ecx * 4]
        pop ecx
        xchg eax, [esp]                    // restore eax, leave the site's return address on top
        ret
    }
}

bool install_state_checksum()
{
    g_gameNew = reinterpret_cast<AllocFn>(exe_addr(kOperatorNewRva));
    g_gameDelete = reinterpret_cast<FreeFn>(exe_addr(kOperatorDeleteRva));
    uintptr_t site = exe_addr(kDayTickCallRva);
    uintptr_t current = call_target(site);
    if (!current)
    {
        Log("state_checksum: no CALL at %08X, off", kDayTickCallRva + 0x400000);
        return false;
    }
    // Chain-friendly: whatever the site calls now is what we call (your own hook sits at the
    // entry of DayTick itself, which this call still reaches).
    if (current != exe_addr(kDayTickRva))
        Log("state_checksum: the DayTick call site is already redirected (to %08X); chaining to it",
            (unsigned)(current - (uintptr_t)g_base + 0x400000));
    g_originalDayTick = current;
    if (!redirect_call(site, current, reinterpret_cast<uintptr_t>(&day_tick_thunk)))
    {
        Log("state_checksum: could not patch the DayTick call site, off");
        return false;
    }
    Log("state_checksum: active, 5 state hashes appended to the daily sync vector");
    return true;
}

// ---------------------------------------------------------------------------------------------
// sync_world_rng
//   The world object carries its own MT19937 (world+0xB4), seeded from the clock and never saved, so
//   two machines never share it. Ten sites draw from it with `MOV EAX,[g_world]; ADD EAX,0xB4;
//   CALL Rng_Next`. While the session manager executes a command every machine runs the same code in
//   the same order: the draw comes from the global generator and is counted like the game's own draws.
//   Outside that (AI frame jobs, tooltips) it comes from a private MT19937.
// ---------------------------------------------------------------------------------------------

const uint32_t kRngNextRva = 0x5B7610;
const uint32_t kMtStateRva = 0xB0ECF0;   // g_mtState
const uint32_t kWorldRngSiteRvas[] = { 0x420D18, 0x421ECF, 0x421F32, 0x43381E, 0x438955,
                                       0x43C722, 0x44401D, 0x446408, 0x45CFBF, 0x4687EA };

uintptr_t g_rngNext = 0;
uintptr_t g_mtState = 0;
uint32_t* g_rngCalls = 0;
int g_rngSites = 0;

struct Mt   // private MT19937, seeded from the clock once
{
    uint32_t state[624];
    int index;
    Mt() : index(625) {}
    void seed(uint32_t s)
    {
        state[0] = s;
        for (int i = 1; i < 624; i++)
            state[i] = 1812433253u * (state[i - 1] ^ (state[i - 1] >> 30)) + (uint32_t)i;
        index = 624;
    }
    uint32_t next()
    {
        if (index >= 624)
        {
            if (index == 625)
                seed((uint32_t)GetTickCount() ^ (uint32_t)GetCurrentProcessId() * 2654435761u);
            for (int i = 0; i < 624; i++)
            {
                uint32_t y = (state[i] & 0x80000000u) | (state[(i + 1) % 624] & 0x7fffffffu);
                state[i] = state[(i + 397) % 624] ^ (y >> 1) ^ ((y & 1u) ? 0x9908b0dfu : 0u);
            }
            index = 0;
        }
        uint32_t y = state[index++];
        y ^= y >> 11;
        y ^= (y << 7) & 0x9d2c5680u;
        y ^= (y << 15) & 0xefc60000u;
        return y ^ (y >> 18);
    }
};
Mt g_private;

bool in_command_execution()
{
    uintptr_t w = world();
    if (!w)
        return false;
    uintptr_t mgr = session_manager(w);
    return mgr && *reinterpret_cast<const uint8_t*>(mgr + kSessionManagerExecuting) != 0;
}

uint32_t __cdecl world_rng_draw()
{
    if (in_command_execution())
    {
        (*g_rngCalls)++;
        uint32_t value;
        uintptr_t state = g_mtState;
        uintptr_t fn = g_rngNext;
        __asm {
            mov eax, state
            call fn
            mov value, eax
        }
        return value;
    }
    return g_private.next();
}

// Stands in for `CALL Rng_Next` at the ten sites: EAX (the world state) is ignored, the result goes
// back in EAX, ECX/EDX are preserved in case the caller keeps something there.
__declspec(naked) void world_rng_thunk()
{
    __asm {
        push ecx
        push edx
        call world_rng_draw
        pop edx
        pop ecx
        ret
    }
}

void install_world_rng()
{
    g_rngNext = exe_addr(kRngNextRva);
    g_mtState = exe_addr(kMtStateRva);
    g_rngCalls = reinterpret_cast<uint32_t*>(exe_addr(kRngCallsRva));
    const int total = (int)(sizeof(kWorldRngSiteRvas) / sizeof(kWorldRngSiteRvas[0]));
    for (int i = 0; i < total; i++)
    {
        uint32_t rva = kWorldRngSiteRvas[i];
        if (redirect_call(exe_addr(rva), g_rngNext, reinterpret_cast<uintptr_t>(&world_rng_thunk)))
            g_rngSites++;
        else
            Log("sync_world_rng: site %08X does not call Rng_Next as expected, left alone", rva + 0x400000);
    }
    Log("sync_world_rng: active on %d of %d world-RNG draw sites", g_rngSites, total);
}

// ---------------------------------------------------------------------------------------------
// mp_host_wait_days
//   CInGameIdler_Update: CALL ProcessGameTimeAdvance (0x282BD0, __fastcall(world), delta in XMM0) at
//   0x25571C. On the host it issues the next CHourlyTickCommand. Every tick each machine pings the
//   host with its date; the host stores it per player (world+0x18 list: +0x1C id, -1 = unused, +0x34
//   date in hours, +0x3C next). Vanilla warns at 6 days of lag and pauses at 11. Here the host skips
//   ProcessGameTimeAdvance while any player is more than N days behind, with a timeout so a
//   disconnected player cannot hold it forever.
// ---------------------------------------------------------------------------------------------

const uint32_t kAdvanceCallRva = 0x25571C;
const uint32_t kAdvanceRva = 0x282BD0;
const uint32_t kPlayerId = 0x1C;
const uint32_t kPlayerDate = 0x34;
const uint32_t kPlayerNext = 0x3C;
const uint32_t kReleaseAfterMs = 15000;

uintptr_t g_originalAdvance = 0;
int g_maxDays = 0;
bool g_waiting = false;
uint32_t g_waitStarted = 0;
bool g_timedOut = false;
uint32_t g_episodes = 0;

bool is_server(uintptr_t w)
{
    uintptr_t mgr = session_manager(w);
    return mgr && *reinterpret_cast<const uint8_t*>(mgr + kSessionManagerIsServer) != 0;
}

// Days the slowest player is behind the host's date, by the last ping received from each.
int max_lag_days(uintptr_t w)
{
    int32_t now = (int32_t)read32(w + kWorldDate) / 24;
    int worst = 0;
    for (uintptr_t p = read32(w + kWorldPlayers); p; p = read32(p + kPlayerNext))
    {
        if ((int32_t)read32(p + kPlayerId) == -1)
            continue;
        uint32_t date = read32(p + kPlayerDate);
        if (date == 0)
            continue;   // never reported
        int lag = now - (int32_t)date / 24;
        if (lag > worst)
            worst = lag;
    }
    return worst;
}

// Returns non-zero when the time advance should be skipped this frame.
int __cdecl should_wait(uintptr_t w)
{
    if (!w || !is_server(w))
        return 0;
    int lag = max_lag_days(w);
    uint32_t now = GetTickCount();
    if (lag <= g_maxDays)
    {
        if (g_waiting)
        {
            g_waiting = false;
            g_timedOut = false;
        }
        return 0;
    }
    if (!g_waiting)
    {
        g_waiting = true;
        g_waitStarted = now;
        g_episodes++;
        if (g_episodes <= 10)
            Log("mp_host_wait: a player is %d day(s) behind, holding the next tick", lag);
    }
    else if (now - g_waitStarted > kReleaseAfterMs)
    {
        if (!g_timedOut)
        {
            g_timedOut = true;
            Log("mp_host_wait: still %d day(s) behind after %u ms without catching up, releasing (disconnected player?)",
                lag, kReleaseAfterMs);
        }
        return 0;
    }
    return 1;
}

// ProcessGameTimeAdvance is __fastcall with the world in ECX and the frame delta in XMM0; nothing on
// the stack, so skipping it is a plain return.
__declspec(naked) void advance_thunk()
{
    __asm {
        pushad
        pushfd
        sub esp, 16
        movdqu [esp], xmm0
        push ecx
        call should_wait
        add esp, 4
        mov [esp + 48], eax          // saved eax of pushad: 16 (xmm0) + 4 (eflags) + 28 (edi..ecx)
        movdqu xmm0, [esp]
        add esp, 16
        popfd
        popad
        test eax, eax
        jnz skip
        jmp dword ptr [g_originalAdvance]
    skip:
        ret
    }
}

void install_host_wait(int days)
{
    g_maxDays = days;
    g_originalAdvance = exe_addr(kAdvanceRva);
    if (!redirect_call(exe_addr(kAdvanceCallRva), g_originalAdvance, reinterpret_cast<uintptr_t>(&advance_thunk)))
    {
        Log("mp_host_wait: call site %08X not recognised or already hooked, off", kAdvanceCallRva + 0x400000);
        return;
    }
    Log("mp_host_wait: active, the host holds its ticks while a player is more than %d day(s) behind", g_maxDays);
}

// ---------------------------------------------------------------------------------------------
// pin_fpu_every_tick: runs the DLL's own FPU pin (same values as for its threads) at the start of
// every hourly tick, before the day is simulated, so a stray change of the control registers
// (overlay, driver, another DLL) cannot make one machine compute differently from the others.
// ---------------------------------------------------------------------------------------------

void (*g_pinFn)() = 0;
bool g_pinLogged = false;

void pin_tick()
{
    g_pinFn();
    if (!g_pinLogged)
    {
        g_pinLogged = true;
        Log("pin_fpu_every_tick: first tick, FPU control registers pinned");
    }
}

}  // namespace

// ---------------------------------------------------------------------------------------------
// Entry point
// ---------------------------------------------------------------------------------------------

void InstallMpSync(const MpSyncConfig& cfg)
{
    if (cfg.skipNetworkWait)
        install_network_wait();

    if (cfg.skipCleanResets)
        install_clean_resets();

    // The comparison hook owns the daily sync check: needed by oos_log and state_checksum.
    // state_checksum is only installed when it is in place: without it the game would compare (and
    // react to) the extra elements itself.
    g_oosEnabled = cfg.oosLog;
    bool wantCompare = cfg.stateChecksum || cfg.oosLog;
    if (wantCompare &&
        hook_call(kEqualsCallRva, kEqualsRva, g_originalEquals, (void*)&equals_thunk, "Vector_Equals"))
    {
        g_compareHooked = true;
        Log("sync_check: the game compares its two elements; the rest is compared and logged here");
    }
    if (cfg.stateChecksum)
    {
        if (g_compareHooked)
            install_state_checksum();
        else
            Log("state_checksum: the comparison hook is unavailable, off");
    }

    if (cfg.fixRngResync &&
        hook_call(kResyncCallRva, kResyncRva, g_originalResync, (void*)&resync_thunk, "Rng_Resync"))
        Log("fix_rng_resync: active, a sync mismatch realigns the RNG to the host's position instead of the seed");

    if (cfg.syncWorldRng)
        install_world_rng();

    if (cfg.hostWaitDays > 0)
        install_host_wait(cfg.hostWaitDays);

    if (cfg.pinFpuFn)
    {
        g_pinFn = cfg.pinFpuFn;
        if (on_hourly_tick(&pin_tick))
            Log("pin_fpu_every_tick: active");
    }
}
