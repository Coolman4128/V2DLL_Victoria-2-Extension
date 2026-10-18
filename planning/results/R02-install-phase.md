# R02 installation-phase constraints

This is a narrow phase review, not a production gate or candidate certification. `ViewsBoundary` remains uncalled. The exact disk EXE SHA-256 is `62D48C204364DD706584777C2E2B3C7AB3C5F1DD0170872554943575D53D6648`.

## Evidence

The pinned PE imports `lua51.dll` through its ordinary import directory (54 named Lua imports). Its entry RVA is `0x6B0F91`. The private independent parse `R02/load-time-phase.json` has SHA-256 `F4A32AA027A5604068682D9687DC88E2EA783BB42FAF5B8349DE64EB9EEC5528`. Original-Lua diagnostic `starti` observations stop in the Windows loader with the EXE already mapped and Lua dependencies listed before native application entry. They demonstrate the native baseline condition; no fork DLL attach was observed.

Windows initializes load-time dependencies during process initialization. A DLL can also receive PROCESS_ATTACH during a later LoadLibrary call; PROCESS_ATTACH alone therefore does not prove pre-game startup. Its reserved parameter distinguishes static startup from dynamic loading. See [Microsoft DLL entry-point documentation](https://learn.microsoft.com/en-us/windows/win32/dlls/dllmain), [entry-point timing](https://learn.microsoft.com/en-us/windows/win32/dlls/dynamic-link-library-entry-point-function), and [load-time linking](https://learn.microsoft.com/en-us/windows/win32/dlls/load-time-dynamic-linking).

Current source `DllMain` (line 26353) ignores the reserved argument and synchronously calls legacy `Install` on PROCESS_ATTACH. This is an existing source fact, not proof that every possible attach has an exclusive installation phase. It initializes logging/crash watch and pins FPU before Install; Install calls LoadSettings before obtaining engine identity, then multiple independent installers before VIEWS. `g_dllReady` is set after either success or failure, so it means the legacy sequence ended. It cannot authorize the pure family adapter. The legacy shared PatchSlot and optional native mod Init paths remain independent bypasses.

## Required future supported phase

A future gate must bind the exact executable and resolved proxy module identity to verified initial startup, reject dynamic/late attach unless a separately proved exclusive phase exists, and run complete active-closure validation before any candidate native mutation. It must not infer safety from a nonempty evidence string, a main-thread ID, `g_dllReady`, a matching current slot or the loader lock alone. The four replacement/static original code addresses have process-image lifetimes; they are distinct from native GUI object pointers and the preexisting cross-campaign caches. No new object pointer is stored by this pure installer.

Loader entry serialization does not establish the later callback element/descriptor lifetime, rule out nested GUI dispatch, or justify arbitrary DllMain transitive calls. Microsoft restricts complex entry-point operations; moving or certifying the whole legacy startup is outside this focused family result. The original frozen startup behavior remains unchanged. No production phase token or call site was added, and no candidate was deployed. A later separately tested, uncalled Win32 memory backend is recorded in the validation report; it supplies no production phase/profile binding.
