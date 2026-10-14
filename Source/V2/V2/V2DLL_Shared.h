// V2DLL_Shared.h - symbols that V2TechButton.cpp exposes to the other
// translation units of the DLL, and what those units export back.
#pragma once
#include <windows.h>

// --- defined in V2TechButton.cpp ---
extern DWORD g_base;                                  // game image base
void   Log(const char* fmt, ...);
bool   V2_DebugLogEnabled();                          // DEBUG_LOG setting
bool   PatchSlot(DWORD rvaVtable, int slotIndex, void* replacement, void** outOriginal);
bool   StealToTrampoline(DWORD rva, unsigned steal, unsigned char* tramp, int trampSize,
                         const unsigned char* expect, void* hook, void** orig, const char* tag);
double ConsParseNumber(const char* s);

// --- defined in V2DLL_AssimilateBlock.cpp ---
bool   InstallAssimilateBlock();

// --- defined in V2DLL_ModLoader.cpp ---
// Loads "<modFolder>\v2dll_mod.dll" if present (modFolder = running -mod= path).
void   LoadV2Mods(const char* modFolder);

// --- defined in V2DLL_MpSync.cpp ---
// Performance + multiplayer-sync patches. Multiplayer options must be equal for every player.
struct MpSyncConfig
{
    bool skipNetworkWait;     // poll the network without blocking
    bool skipCleanResets;     // skip no-op per-country table resets
    bool stateChecksum;       // append state hashes to the daily sync vector
    bool oosLog;              // report which sync-vector element differed (to the DLL log)
    bool fixRngResync;        // realign the RNG to the host's position on a mismatch
    bool syncWorldRng;        // lockstep draws of the world RNG from the shared generator
    int  hostWaitDays;        // host waits while a player is more than N days behind (0 = off)
    void (*pinFpuFn)();       // called at the start of every hourly tick (NULL = off)
};
void   InstallMpSync(const MpSyncConfig& cfg);
