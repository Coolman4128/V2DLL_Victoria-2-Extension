# S02 first-factory route closure — result

- Session: `S02-20261018-first-factory-route-closure`; estimate 45 minutes; elapsed and family wall span are recorded in the session clock and handoff. One whole-session timing review only; timing calibration does not add work or alter scope.
- Question: does the pinned executable and available dependency evidence prove that installing A/B templates on the first intercepted `Direct3DCreate9` call, before forwarding, precedes every factory/provider use and excludes partial-template observation?
- Verdict: **FAIL for the complete-process guarantee; PASS only for the EXE's named normal-import route.** The first-factory design remains a conditional proposal, not an implemented or accepted profile.

## Evidence

- Pinned R02u `v2game.exe`: 12,294,656 bytes, SHA-256 `62D48C204364DD706584777C2E2B3C7AB3C5F1DD0170872554943575D53D6648`. Its PE has one named `d3d9.dll!Direct3DCreate9` import at IAT RVA `0x88A39C`, no delay-import directory, and no TLS directory. The exact IAT operand occurs at two `.text` calls: preferred VAs `0x9946A2` and `0x9B81BC`; existing dataflow work maps the known first-factory route and conditional second-factory `CreateDevice` candidates. See [accepted route map](S02-static-route-reachability-20261018.md) and [second-route dataflow](S02-second-factory-device-flow-20261018.md).
- The EXE also imports `GetProcAddress` and `LoadLibraryA/W`. Seven direct GetProcAddress references were classified; none was an evident D3D lookup, but branch/dataflow reuse was not exhaustively decoded. Ten inspected game-adjacent DLLs had no named Direct3DCreate9 import or literal string in the inspected disk copies. Those copies are not a process-bound census or proof of runtime identity.
- R02u observed one of the two EXE callsites reaching the pinned `C:\Windows\SysWOW64\d3d9.dll` export in one original-game run without the fork. The capture did not inventory loaded modules and cannot show fork installation timing. The existing no-op Lua proxy hit attach but did not reach the D3D callsites.
- The EXE imports `d3dx9_41.dll`; another inspected adjacent module imports D3DX helpers from `d3dx9_42.dll`. Transitive initializers, runtime-loaded modules, constructed export names, and other D3D provider users remain unclosed. A dependency initializer could use the provider before the Lua proxy's attach hook; no evidence excludes that path.

## Decision and next evidence

The first-factory redesign could serialize callers that pass through the intercepted EXE IAT and install templates before forwarding that call. It cannot establish global safety until every relevant caller is mediated and no earlier/foreign provider use can copy the templates. Unknown callers therefore fail the gate. This route audit does not solve the separate `HookPresent` callback-closure or M3 other-thread teardown gates.

The smallest evidence that can change the route verdict is an exact-process startup record tying module paths and hashes to load order, plus a complete import/delay-import and dynamic-resolution audit for those loaded images through the first provider/device use. Any unresolved factory acquisition or earlier provider use is FAIL. A passive trace can establish observed ordering only; it cannot replace the source/ownership proof for synchronization. Current R02u evidence has no module census, and its D3D read-stop is exhausted. No runtime, debugger, D3D read/resume, build, test, source edit, or profile launch occurred in this session.

## Exits

- Pinned EXE named normal-import routes: **PASS**, bounded to the two mapped IAT callsites.
- EXE delay-import/TLS routes: **PASS absent** in the pinned PE.
- Complete process factory-route closure and pre-first-use ordering: **FAIL / UNKNOWN paths remain**.
- First-factory design implementation/fixture: **NOT RUN**; route guarantee failed, so no code was changed.
- `HookPresent`, M3 lifetime, production profile, S02 acceptance and S03 eligibility: **NOT PASSED**.
- P00 remains user-skipped; HPM performance remains **NOT MEASURED**.


## Timing review

Session elapsed 2.769 minutes against 45 minutes; S02 family wall span 1,275.558 minutes. Exactly one independent closeout review found the estimate was high because accepted route artifacts were ready and the bounded unknown-route verdict came before any runtime work. Similar artifact-reusing static PE feasibility audits calibrate to 5–10 minutes; this does not apply to process-bound evidence collection. The review changed timing estimates only, adding no work and changing no scope.
