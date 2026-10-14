// V2DLL_ModLoader.cpp - loads the optional per-mod plugin "v2dll_mod.dll".
// V2TechButton.cpp only calls LoadV2Mods() at the end of Install().
//
// Every mod ships the same (empty) v2dll_mod.dll built from V2Mod_Template.cpp.
// A modder who wants custom logic rebuilds ONLY that DLL and drops it in the
// mod folder; lua51.dll itself is never rebuilt.
#include <windows.h>
#include <string.h>
#include <stdio.h>
#include "V2DLL_Shared.h"
#include "V2ModAPI.h"

static const char* const MOD_DLL_NAME = "v2dll_mod.dll";

typedef int (__cdecl* tV2ModInit)(const V2ModAPI*);

static int __cdecl ApiPatchSlot(uint32_t rvaVtable, int slotIndex,
                                void* replacement, void** outOriginal)
{
    return PatchSlot((DWORD)rvaVtable, slotIndex, replacement, outOriginal) ? 1 : 0;
}

static int __cdecl ApiStealToTrampoline(uint32_t rva, unsigned steal,
    unsigned char* tramp, int trampSize, const unsigned char* expect,
    void* hook, void** orig, const char* tag)
{
    return StealToTrampoline((DWORD)rva, steal, tramp, trampSize, expect,
                             hook, orig, tag) ? 1 : 0;
}

static bool CallInit(tV2ModInit init, const V2ModAPI* api, int* result)
{
    __try { *result = init(api); return true; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

void LoadV2Mods(const char* modFolderRaw)
{
    // Only the mod that is currently running (-mod=). The .mod file gives a
    // path relative to the game folder (e.g. "mod/MyMod"): make it absolute.
    if (!modFolderRaw || !modFolderRaw[0])
        return;

    static char modFolder[MAX_PATH];   // static: api.modFolder points here
    DWORD n = GetFullPathNameA(modFolderRaw, MAX_PATH, modFolder, NULL);
    if (!n || n >= MAX_PATH)
    {
        Log("ModLoader: cannot resolve mod folder '%s'", modFolderRaw);
        return;
    }

    char path[MAX_PATH];
    _snprintf_s(path, sizeof(path), _TRUNCATE, "%s\\%s", modFolder, MOD_DLL_NAME);
    if (GetFileAttributesA(path) == INVALID_FILE_ATTRIBUTES)
        return;                         // this mod has no plugin DLL: normal

    static V2ModAPI api;
    api.size              = sizeof(V2ModAPI);
    api.apiVersion        = V2MOD_API_VERSION;
    api.gameBase          = (uint32_t)g_base;
    api.modFolder         = modFolder;
    api.Log               = &Log;
    api.PatchSlot         = &ApiPatchSlot;
    api.StealToTrampoline = &ApiStealToTrampoline;

    HMODULE m = LoadLibraryA(path);
    if (!m)
    {
        Log("ModLoader: cannot load %s (error %lu)", path, GetLastError());
        return;
    }
    tV2ModInit init = (tV2ModInit)GetProcAddress(m, "V2Mod_Init");
    if (!init) init = (tV2ModInit)GetProcAddress(m, "_V2Mod_Init");
    if (!init)
    {
        Log("ModLoader: %s has no V2Mod_Init export - skipped", path);
        return;
    }
    int result = 0;
    if (!CallInit(init, &api, &result))
        Log("ModLoader: %s crashed in V2Mod_Init", path);
    else
        Log("ModLoader: %s -> %s", path, result ? "ok" : "refused (returned 0)");
}
