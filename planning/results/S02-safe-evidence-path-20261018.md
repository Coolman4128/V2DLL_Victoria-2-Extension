# S02 safe evidence path — blocked

Session: `S02-20261018-safe-evidence-path`; 25-minute estimate, **1.983 minutes actual**, S02 family wall **1176.231 minutes (19.604 hours)** at session close. The single timing-only closeout review found a short, evidence-reuse decision; it is recorded in `%LOCALAPPDATA%/V2DLLFork/S02/budget-ledger.json` and calibrates timing only.

## Decision

**BLOCKED.** No accepted existing artifact or source path proves that four A/B template slots can be published before every affected provider use while excluding concurrent/reentrant use. Existing evidence also does not close `HookPresent`'s transitive callback requirements or M3 other-thread teardown/lifetime.

## Evidence and limits

- The local game's PE import table lists `d3d9.dll` and `lua51.dll`. This proves static imports, but not authenticated runtime initialization order, fork attach timing, provider state at attach, or a quiescent phase.
- `DllMain(DLL_PROCESS_ATTACH)` calls `Install()`. The late-module path checks for loaded `d3d9.dll` and installs an EXE factory-import hook. The LoadLibrary hooks cover TBB, not D3D. These paths do not prove that every factory/device route is intercepted before use.
- `HookDirect3DCreate9` installs the `CreateDevice` hook on the returned factory before returning it. `HookCreateDevice` installs templates before forwarding the current call. Neither excludes a second factory/device path or concurrent provider reads/use. The four slot updates are individually atomic, not a single publication.
- Accepted R02u observed one factory and one CreateDevice route while the pinned EXE inventory lists two sites of each; the fork hooks were not loaded. R02v proves bounded provider copy/Release mechanics, not full production route ordering or exclusivity. R02w exercised SDK fixtures with unmodified provider templates, not real hook inheritance. The accepted capture-feasibility result states a stock-game run cannot reveal fork attach timing or prove quiescence.
- `HookPresent` transitive callback closure and M3 other-thread teardown/lifetime remain NOT PROVEN, as recorded in the [callback/lifetime classification](S02-callback-lifetime-classification-20261018.md).

## Exit matrix

| Exit | State | Evidence |
| --- | --- | --- |
| Candidate-free route to safe quiescent installation | BLOCKED | No authenticated attach timing, full route coverage before use, or exclusive window spanning all template writes and rollback |
| Real provider hook inheritance | NOT PROVEN | Existing fixtures use unmodified templates |
| `HookPresent` callback and M3 lifetime gates | NOT PROVEN | Prior classification; no runtime closure evidence |
| S02 complete / S03 eligible | NO | Required gates remain open; production profiles stay Rejected/WITHHELD |
| Process safety | PASSED | No game, candidate, debugger, build, test, or deployment was run |

## Retrospective and next scope

The decision reused accepted artifacts and made only focused source/PE checks. This session does not create follow-on investigation: no existing candidate-free proof path was found. Continue the already-planned S02 objective only if a concrete, authorized evidence route can satisfy the existing native gates; otherwise retain the blocker and keep S03 closed. P00 remains skipped by user direction, so HPM speed is NOT MEASURED.
