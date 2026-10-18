# S02 quiescence decision — blocked

Session: `S02-20261018-quiescence-decision`; 30-minute estimate, **2.374 minutes actual**, S02 family wall **1169.604 minutes (19.493 hours)** at clock close. Its one timing-only review found the narrow source decision took 12.6x less than estimated because it reused indexed evidence and required only source tracing plus one confirmation; this calibrates future estimates to separate source/evidence review from runtime validation and adds no work or scope. Full record: `%LOCALAPPDATA%/V2DLLFork/S02/budget-ledger.json`. No implementation, tests, build, game/debugger run, deployment or benchmark occurred.

## Decision

**BLOCKED — the current production path does not establish race-free, quiescent installation of the four static A/B template slots.** Keep every production D3D profile Rejected/WITHHELD. This is an S02 native safety gate; it does not authorize a candidate launch or S03.

`D3dInstallTransaction::InstallStaticDeviceTemplates` uses a nonblocking transaction-local CAS gate (`Source/V2/V2/Fork/D3dInstallTransaction.cpp`, lines 875–891), which serializes calls mutating this transaction object. It then patches four DWORD slots individually (`lines 948–997`); each CAS is atomic, but the four-slot transition is not one atomic publication. Neither mechanism excludes a native provider read or a second `CreateDevice` call during patch or rollback.

Production calls this installer from `HookCreateDevice` before forwarding the current call (`Source/V2/V2/V2TechButton.cpp`, lines 13841–13867). A concurrent caller that encounters the transaction's Busy result is marked unavailable by the wrapper, but the wrapper still forwards its own call to `g_realCreateDevice`. That preserves the current call's HRESULT/output path; it does not prove exclusion of concurrent provider use. The available synchronization here is nonblocking; adding waits or relying on loader-lock behavior is not supported by current evidence or constraints.

## Accepted evidence and exact gap

- R02u observed one factory/CreateDevice route, not complete route coverage or an exclusive/quiescent installation phase.
- R02v established the bounded static template producer/copy mechanics, not original-game runtime phase.
- R02w's owned manifested SDK run confirmed native layout/copy behavior using unmodified provider slots. It did not test inheritance after patching those slots or prove original-game phase.
- The existing capture-feasibility result explains why a stock-game capture cannot reveal the withheld fork's attach/install timing.

The missing evidence is a production-equivalent exclusive phase covering all active device routes and excluding concurrent provider reads/constructors during all four writes and possible rollback, without waits, loader-lock dependence or changed successful native behavior. No currently accepted evidence supplies it. This decision introduces no new task or scope.

## Exit state

| Item | State |
| --- | --- |
| Existing-path quiescence proof | BLOCKED / NOT PROVEN |
| Production D3D profile | Rejected/WITHHELD |
| S02 | INCOMPLETE |
| S03 | GATED |
| HPM speed | NOT MEASURED; P00 remains skipped by user direction |
| Runtime/build/test/deployment work in this session | NOT RUN |

Next action: preserve this precise blocker and do not repeat stock-game capture or unpatched provider-copy checks as substitutes. A production-equivalent safe install-phase proof is a hard precondition before any candidate/profile launch. The next bounded S02 session may classify, from existing source/evidence only, the two already-open gates for transitive `HookPresent` callback closure and M3 other-thread lifetime. It must record PASS or NOT PROVEN per gate with exact limits; no build, test, game/debugger run, candidate launch, or new fix scope.
