# Validation contract and test matrix

## Execution acceptance and speed priority

[Execution control](execution-control.md) is mandatory. A session result must report its estimate, actual elapsed time, family wall span, one timing review at most across the whole session, retrospective, next planned scope, and scoped Git state. There is no hard elapsed-time deadline. Timing reviews only calibrate future estimates and never add work or change scope; at 2.5x estimate, scope reduction is a separate execution decision. Native safety/fidelity and the performance thresholds below are unchanged.

P00 measures **original-game HPM**, independently of inactive fork hooks. Its native result is distinct from retained-fixes/candidate acceptance. An accurate first sample validates the procedure; five valid repetitions are needed for a median/spread and threshold comparison. Incomplete repetitions remain incomplete. A speed-first schedule never converts an unverified modified profile into an accepted one.

Each row needs a result and evidence path in `planning/results/acceptance.md` during execution. Until then it is NOT RUN. Unit tests alone cannot mark native behavior passed. User multiplayer testing remains pending; the MP checklist below tests new-feature integration when that testing becomes available, without authorizing an MP-improvement project.

## Baselines

Maintain four separately identified builds/profiles: native vanilla, native HPM 0.4.6.3, retained-fixes fork with new features off, and release candidate with the requested features on. The native-to-fixes comparison allows only documented bug/stability corrections. Optimization equivalence is tested against the same retained-fixes profile, not an incorrectly assumed identical vanilla simulation. Record executable/build/mod/config/save hashes for every comparison.

## Required checks

| ID | Scenario | Pass condition | Main sessions |
| --- | --- | --- | --- |
| B01 | Clean Win32 Release rebuild and staged launch | Current source produces loaded DLL; exports/real Lua forwarding and symbols work | S01–S02 |
| B02 | Unsupported/mismatched hook site | Entire dependent feature fails closed with explicit error; no partial install | S02 |
| V01 | Every setting/default/alias/mod override | Policy-canonical effective manifest; no ignored emitted keys | S03 |
| V02 | Stock economy/combat/diplomacy with features off | No unrelated V2DLL changes; retained corrections enumerated | S04 |
| V03 | Existing crash/reinforcement/modifier/UI fixes | Targeted reproductions fixed, valid native scenarios unchanged | S04, S26 |
| V04 | Frozen MP remedies with diagnostics off | Same effective FPU/quantization/savings behavior; reporting independent | S06 |
| P01 | Exact 1837 timing at speed 5 | Simulate 1836 untimed in same process; measure 1837-01-01 to 1838-01-01; all days execute; load/pause treatment disclosed | S05–S07 |
| P02 | HPM 1837 performance | Five repetitions; median <12 seconds primary or <30 fallback; spread disclosed; no later-date absolute-speed gate | S13, S28–S29 |
| P03 | Vanilla 1837 comparison and supplementary later regression checks | Same 1837 procedure reported separately; later fixtures only when available/useful, with no <12/<30 requirement | S28 |
| P04 | Optimization fidelity | Same required state/RNG/events/orders under identical setup; unexplained differences reject candidate | S08–S12 |
| P05 | Feature overhead | Report idle, 10 and 100 feasible jobs; no runaway scans, queue/memory growth or UI stalls | S27–S28 |
| T01 | Template CRUD and unit-type data | Persistent names/revisions/counts; unknown/locked types handled; HPM list supported; renamed/missing type after mod-version change blocks rather than remapping by numeric index | S19, S26 |
| T02 | Partial recruitment availability | Legal subset queued; remaining counts pending; normal money/goods/POP/time | S21 |
| T03 | Concurrent jobs plus manual recruitment | No over-reservation, duplicate ticket or commandeered unit | S21 |
| T04 | Gather and merge mixed composition | One correct final stack, exact assigned IDs/counts, no unrelated troops | S22 |
| T05 | Blocked rally, changed access, lost recruit | Visible blocker; no endless replacement or free recovery | S23 |
| T06 | Template edit/delete/cancel while building | Snapshot semantics; existing troops and paid queue preserved | S23 |
| T07 | Template recruits across sea | Prompted shared transport and final single army | S25 |
| Z01 | One brigade reaches zero in otherwise surviving army | Same brigade ID remains; no partial deletion | S20 |
| Z02 | Combat with zero support/recon/infantry brigades | No damage/support/defense/width/siege benefit or shielding; positive-only control preserves native RNG/outcomes; retreat/wipe and all-zero battle termination checked | S21 |
| Z03 | Zero brigade reinforces | Ordinary cost/manpower/supply rules; resumes under legal conditions | S20, S22 |
| Z04 | Entire army reaches zero without a wipe | Stack persists safely; no permanent battle stall or phantom victory | S21–S22 |
| Z05 | Genuine whole-stack wipe | Every brigade removed once; no survivor shells or leaked job references | S20–S22 |
| Z06 | Explicit disband/demobilization | User command works, including mixed mobilized/regular army | S22 |
| Z07 | POP vanishes, revolt/owner teardown/script cleanup | No dangling references or unauthorized partial deletion; semantics documented | S19, S22 |
| Z08 | Save/reload mixed zero/live stack; old save | IDs/composition/reinforcement restored; old saves load | S22, S25 |
| R01 | Land route exists; no transport available; prompt canceled | Native land command or clear blocked reason; no unintended orders | S20 |
| R02 | One ship and ten brigades | Ten capacity-safe trips if native capacity is one; all survive -> final 10-unit army | S22–S23 |
| R03 | Three slots and ten brigades | Legal capacity batches, multiple trips, one final army | S22 |
| R04 | Multiple fleets, mixed escorts, existing cargo | Correct free capacity, opted-out/busy fleets respected, no reservation overlap | S20–S22 |
| R05 | Coast-to-coast and inland-to-inland overseas move | Full rendezvous, boarding, sailing, unloading, inland move and merge | S21–S23 |
| R06 | Partial fleet loss, blocked sea route, battle/retreat | Native losses/access rules, pause/replan, no infinite retry or partial-delete exception | S24 |
| R07 | Manual move/split/merge during in-flight command | Manual precedence proven; no duplicate action or taking unrelated units | S23–S24 |
| R08 | Zero brigades and leader loss during transport | Correct cargo membership; no free capacity; no resurrected leader | S24 |
| R09 | Cancel with troops at sea and batches on both shores | Surviving troops safe under native rules; no silent deletion/teleport | S23–S24 |
| SAVE01 | Save in each job state, especially before receipt | Exact reconciliation; no double spending/building/boarding | S17, S25 |
| SAVE02 | Save As, autosave, compressed save, older branch | Correct campaign/save binding; stale future jobs never applied | S16–S17, S25 |
| SAVE03 | Missing/corrupt/foreign metadata | Native units load; automation safely suspended with recovery UX | S17, S25 |
| SAVE04 | Transfer save to another machine/controller | Explicit safe resume/recovery contract; sidecar assumptions disclosed | S25 / user test |
| M01 | Human host and human client both start jobs | Each order emitted once by correct owner; no AI automation | S27 / user test |
| M02 | Frozen stability behavior retained | No new MP enhancement claimed or silently changed | All |
| G01 | Vanilla and HPM UI/layout/mod-local overrides | Working feature UI, policy enforced, HPM content retained | S26 |
| D01 | Package/deploy/rollback | Known current binaries/assets, manifests/backups, reproducible build | S30 |

## Performance procedure

The full procedure lives in [research/performance.md](research/performance.md). Required throughput fixtures are pinned 1836 starting setups for HPM and vanilla. Simulate the entire startup year normally, then measure **1837-01-01 to 1838-01-01**. Mid/late saves are supplementary regression fixtures only when already available or a specific suspected regression justifies them; do not spend sessions generating decades solely to certify the early-game goal.

Final timing: five fresh-process repetitions per required workload/candidate, each loading the same pinned 1836 starting save and simulating 1836 untimed before timing exactly 1837. Loading an 1837 save immediately before timing or adding a warm-up month inside 1837 is not this protocol. Use speed 5, identical visible view/render cap/UI, game not minimized, and no omitted simulation work. Report normal autosave-inclusive timing separately from the no-autosave raw throughput target. Correctness runs use diagnostics separately. Record OS/render settings consistently for baseline and candidate.

For a 365-day year the primary target implies about 30.4 game days/second or 32.9 ms/day; fallback about 12.2 days/second or 82.2 ms/day. These are total wall-time budgets, not promises about what the native engine can achieve. Report a full-year measurement, not extrapolation from one fast day.

## Regression and release decisions

Keep optimization-only comparisons separate from intentional requested feature changes. Daily hashes help locate divergence but do not cover every field or prove purity. Review targeted combat, recruitment and command traces and native world counts as well. If the identical baseline cannot reproduce its own output, resolve the fixture/measurement uncertainty before interpreting A/B differences.

No silent lowering of thresholds. The HPM 1837 target can be `PRIMARY MET`, `FALLBACK MET`, `MISSED`, or `NOT MEASURED`; vanilla is a separately reported comparison and later dates are supplementary regression results. A feature can be `PASSED`, `FAILED`, `BLOCKED`, or `NOT RUN`. A usable local package may still have a documented unmet performance target or pending user MP test; neither is relabeled complete.
