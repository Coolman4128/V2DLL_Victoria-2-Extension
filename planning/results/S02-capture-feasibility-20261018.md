# S02 — read-only route-capture feasibility

Session: `S02-20261018-capture-feasibility` (20-minute estimate). Question: can a read-only original-game startup/menu capture, without loading or patching the fork, establish the D3D installation timing required by the current profile?

## Existing route and install path

Accepted R02u evidence contains one original-game factory callsite/continuation pair and one CreateDevice pair on Thread 1. The pinned EXE inventory lists two factory IAT callsites and two indirect CreateDevice callsites. It does not establish that the runtime observations cover all paths.

Current source puts the D3D import installation inside the fork's attach-time legacy install sequence:

- `DllMain` enters `Install()` on `DLL_PROCESS_ATTACH` (`Source/V2/V2/V2TechButton.cpp:26817-26846`).
- `Install()` calls `InstallEngineStability()` (`:25525`), which calls `TryPatchLateModules()` (`:20491-20527`).
- `TryPatchLateModules()` checks the currently loaded `d3d9.dll` and, when the frozen D3D FPU setting is enabled, installs the EXE `Direct3DCreate9` import hook (`:14112-14165`).
- The later factory and device-table work happens only when the hooked factory/CreateDevice callbacks execute (`:13773-13875`).

This identifies the source-level phase as process attach followed by the then-loaded provider check; it does not identify when/how the production fork DLL would be loaded relative to original-game D3D initialization, nor whether the EXE is quiescent at attach. The production profile is still withheld, and there is no safe candidate launch from which to observe that ordering.

## Feasibility result

**A read-only original-game capture without the fork cannot prove the required safe installation window.** It could count observed factory/CreateDevice routes during that particular startup, and might establish first-device ordering for that run. It cannot observe the fork's `DLL_PROCESS_ATTACH`, the provider state at that attach, whether the importer is executing concurrently, or whether all creation paths are covered. Loading the withheld candidate to observe those facts would violate the current launch gate; a read-only stock-game run does not close it.

Therefore this additional capture is not approved as the next experiment: likely output would duplicate one-run route evidence but still leave the decisive phase gate open. The exact missing evidence is a separately authenticated production load/injection timing plus proof that the EXE import and provider-template writes occur before any affected D3D object use and during an exclusive, quiescent phase, with all active routes covered. No available evidence supplies that observation safely. Keep the production profile Rejected/WITHHELD and S02 incomplete; do not start S03.

## Session exit

- Feasibility decision and criteria: **RECORDED**.
- Native run, debugger, build, tests, launch, deployment, and benchmark: **NOT RUN**.
- Product behavior/source: **UNCHANGED**.
- S02: **INCOMPLETE / PHASE GATE BLOCKED**. P00 remains skipped by user direction; HPM speed remains NOT MEASURED.
- Timing lesson: this 20-minute feasibility decision took 0.728 minutes because it compared existing route evidence with a short source call path. That estimate applies to similar static feasibility checks, not native route/phase validation. The review creates no additional work.
