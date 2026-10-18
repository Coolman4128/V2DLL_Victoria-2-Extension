# S02 callback and lifetime classification — not proven

Session: `S02-20261018-callback-lifetime-classification`; 20-minute estimate, **2.123 minutes actual**, S02 family wall **1172.329 minutes (19.539 hours)** at clock close. The single timing-only review found the short duration reflected a narrow read-only classification, accepted evidence reuse and one targeted source confirmation; similar evidence classifications can use low single-digit-minute estimates, but that does not apply to native/runtime validation. Review details are in `%LOCALAPPDATA%/V2DLLFork/S02/budget-ledger.json`. No product edits, tests, build, game/debugger run, deployment or benchmark occurred.

## Results

| Gate | State | Reason |
| --- | --- | --- |
| `HookPresent` transitive callback closure | NOT PROVEN | The reachable path includes fork UI/layout helpers, an indirect native UI callback, diagnostics/logging, the native Present method and frame-cap timer/wait behavior. Complete ABI, object lifetime, reentrancy, thread/phase and exception behavior are not certified. |
| M3 other-thread teardown/lifetime exclusion | NOT PROVEN | Provider pinning does not prove COM affinity, callback rundown or fork-DLL unload safety. Existing native Release evidence covers the ordinary matching-thread path and excludes wrong-thread, derived/foreign and other-refcount cases. |

## Source trace

`HookPresent` in `Source/V2/V2/V2TechButton.cpp` (lines 13294–13702) calls `UvFlushSinglePanel` before the provider Present call, then `PinFpu`, diagnostics and `IdleNotePresent`, and ends with `WaitFpsCap`. `UvFlushSinglePanel` (lines 17855–17884) reads cached `g_uvLastIdler`, `g_uvPendingPanel` and panel/idler fields, then reaches `UvShowDetail`, `UvCall393510` (an indirect saved native function) and `UvFinishPanelLayout`. Local SEH handling does not prove callback ABI, pointer lifetime, reentrancy or phase. The Present body also reads shared UI/idler state while collecting diagnostics and logging. `WaitFpsCap` may create a timer, call `SetWaitableTimer` and `WaitForSingleObject`, then spin; this is part of the hook's actual transitive behavior.

The D3D backend retains the provider module from an address in `D3dInstallTransaction.cpp` (lines 1199–1205). That protects the provider image reference while retained; it does not establish device COM thread affinity or drain in-flight hooks before unloading the fork DLL. `DllMain` process detach in `V2TechButton.cpp` (lines 26848–26854) requests clipboard-worker shutdown by posting `WM_QUIT`; this path shows no D3D callback rundown or join.

Accepted R02u evidence did not execute a fork callback and explicitly left HookPresent behavior and M3 teardown open. R02v exercised only the ordinary matching-thread zero-reference Release path and excluded wrong-thread/derived/foreign cases. R02w's manifested SDK run used unmodified provider slots and did not certify these callback or teardown boundaries. These records and this source trace support classification only.

## Parent state and next action

S02 remains INCOMPLETE; all production D3D profiles remain Rejected/WITHHELD; S03 remains gated; HPM speed remains NOT MEASURED because P00 was skipped by user direction. The quiescent template-patch barrier is separately BLOCKED as recorded in the [prior S02 decision](S02-quiescence-decision-20261018.md). No candidate or game launch is safe under the present evidence.

No further source-only enumeration of these two closed questions will certify runtime behavior. The required missing evidence is a safe, production-equivalent proof of complete route/phase exclusion, callback lifetime and other-thread teardown. Do not repeat the existing single-route capture, unpatched provider-copy fixture or matching-thread Release sample as substitutes. Continue only if a concrete evidence path can satisfy the existing gates without weakening them or adding unrelated scope.
