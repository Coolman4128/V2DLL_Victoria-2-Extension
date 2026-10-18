# Victoria 2 fork — execution plan

## Controlling revision: speed first, estimate-based reviews

The user's 2026-10-18 timing correction supersedes the former hard two-hour cap. **[Execution control](execution-control.md) is mandatory.** Estimates are advisory; each whole session gets at most one independent timing review across all its slices and checkpoints. After two hours, pause for one review and resume the same objective; a session that ends below half estimate gets one closeout review only if it was not already reviewed. A review only calibrates future estimates; it never creates work or changes/recommends scope. At 2.5 times its estimate, reduce scope as a separate execution decision and continue with a smaller deliverable.

**Current state:** P00 was skipped at the user's direction and remains NOT MEASURED against its five-run acceptance. S02/R02 remains incomplete and resumes under the new estimate/review process; its prior hard-deadline record is historical only. R02w remains an undeployed partial checkpoint. S03 stays gated until every S02 exit passes.

Research date: **2026-10-17**. Source snapshot: `4368f434cc33b00e7b3cc4ff2458e1f69a9ec97d`, V2DLL source version 5.83. Execution has started; current eligibility and session timing history are recorded above. Consult [status](status.md) and verified handoffs before selecting work.

**30 numbered sessions plus the independent P00 insertion, with the listed 90- or 120-minute figures treated as planning estimates rather than caps.** Assign independent work to parallel agents inside a session. Sessions 19–24 run the zero-brigade, template and transport tracks together after the shared engine/order/save/UI foundation. Only the integrator writes shared files; live game control and actual benchmarks are serialized.

The historical 30-session estimate was 48 required hours plus up to 10 conditional optimization hours. It is a planning estimate, not a stop condition. Reserve labels do not create new objectives; each genuine session retains its family ID and contributes to its cumulative elapsed record. P00 was skipped by user direction; its partial HPM intervals remain unaccepted. Historical S02 elapsed time and its former cap remain visible as history, not as an active limit.

## Start here

1. Read root [AGENTS.md](../AGENTS.md), [requirements](requirements.md) and [architecture](architecture.md).
2. Read [execution control](execution-control.md), check status and the durable session clock, then select the next eligible packet. Continue an incomplete parent with an explicitly scoped session; if timing review is due, resume the same objective after review. Never hide elapsed time with a reset or alias.
3. Use [acceptance](acceptance.md) as the completion contract and [risks](risks.md) for blocked discovery/continuation rules.
4. Complete the packet's exit checks, record evidence and actual time, then advance serially. Conditional performance packets can be skipped with a recorded reason; required features cannot.
5. Research documents contain candidate packet suggestions from individual investigations. **The 30-session table below supersedes their local packet numbering and totals.** Do not execute those suggestions as extra sessions.

For serial multi-agent execution, use the copyable [orchestrator prompt](orchestrator-prompt.md). It resumes from evidence in status/handoffs, assigns one session manager at a time, and checks completion before advancing.

## Scope locked from the user

- Existing multiplayer stabilization: **DONE — awaiting user testing. No further improvements planned.**
- Performance: <12 seconds for **1837-01-01 to 1838-01-01** at speed 5 on the Ryzen 7 9700X; fallback <30. Simulate 1836 normally as untimed warm-up in each fresh-process repetition. HPM is primary; report vanilla for the same year separately. Later dates are supplementary regression checks, not speed-target gates.
- Original engine behavior plus actual bug fixes. Remove unrelated V2DLL economy/diplomacy/combat/allied-transport changes; retain frozen stability remedies. Keep HPM's own content and useful opt-in scripting.
- Human-player army templates that recruit available troops, wait for shortages, gather and merge.
- Zero-man land brigades persist and reinforce normally, provide no combat value, and never cause automatic partial army deletion; manual disband and genuine whole-stack wipes remain.
- Human-player EU4-style prompted transport using own fleets, including one-ship shuttling, legal splitting/boarding/sailing/unloading and final recombination.

The user corrected the intended workload to **Historical Project Mod (HPM)** and authorized its download. Pin official release 0.4.6.3. Earlier GHM installation and native-launch results remain historical GHM evidence; HPM installed-file verification passed; its native baseline remains pending under S01HPM. See [environment evidence](research/environment.md).

## Research that materially changes the plan

- This is a 32-bit runtime patch layer, so feature APIs must be recovered/proved rather than assumed available.
- The local game EXE hash differs from the repository's reverse-engineering reference; four sampled sites match, but every required site still needs verification.
- Settings contain ignored emitted keys, unreachable handlers and a hidden enabled puppet-war change. Merely editing the INI cannot guarantee original behavior.
- Always-on diagnostic work may cost real performance even with DEBUG_LOG off; some diagnostic hooks also carry retained stability logic and cannot simply be removed.
- Templates/transport share missing command, identity, reservation and persistence infrastructure; build it once.
- Zero-brigade preservation needs reason-aware deletion and combat exclusion. Skipping a destructor or clamping strength to one is not acceptable.
- EU4 localization confirms prompt/split UI states; exact runtime parity needs controlled comparison before claiming it.

Details: [patch inventory/policy](research/patch-policy.md), [performance evidence and benchmark](research/performance.md), [army/zero-brigade research](research/army-features.md), [transport evidence/parity](research/transport.md), [local environment/install](research/environment.md).

The workload correction adds [S01HPM](sessions/S01HPM.md), a <=90-minute native HPM baseline continuation after the current R02 checkpoint and before HPM-dependent acceptance. S01HPM is now accepted; do not repeat it. It does not reopen completed generic build checks or reinterpret historical GHM evidence.

## Serial session schedule

Each packet specifies outcome, inputs, parallel lanes, integration, exit checks and handoff. The listed time includes validation, not just coding.

| Session | Work packet | Estimate | Execution |
| --- | --- | --- | --- |
| [S01](sessions/S01.md) | Reproducible build and launch baseline | 90 min | Required |
| [P00](sessions/P00.md) | Unmodified original-game HPM speed baseline | 120 min cumulative | Skipped by user direction; acceptance remains NOT MEASURED |
| [S02](sessions/S02.md) | Executable and patch-boundary verification | Estimate-based, focused continuation sessions | In progress on original family clock; S03 remains gated |
| [S03](sessions/S03.md) | Canonical settings and enforceable patch policy | 120 min | Required |
| [S04](sessions/S04.md) | Vanilla behavior and retained-fix acceptance | 120 min | Required |
| [S05](sessions/S05.md) | Benchmark harness and fixture specification | 120 min | Required |
| [S06](sessions/S06.md) | Remove measured diagnostic overhead safely | 90 min | Required |
| [S07](sessions/S07.md) | Baseline profiling and performance budget | 120 min | Required |
| [S08](sessions/S08.md) | Audit existing optimizations for behavior fidelity | 120 min | Required |
| [S09](sessions/S09.md) | Measured low-risk optimization A | 120 min | Conditional |
| [S10](sessions/S10.md) | Measured low-risk optimization B | 120 min | Conditional |
| [S11](sessions/S11.md) | Dominant native hotspot contract discovery | 120 min | Conditional |
| [S12](sessions/S12.md) | Native hotspot candidate and comparison | 120 min | Conditional |
| [S13](sessions/S13.md) | Performance checkpoint and feature budgets | 90 min | Required |
| [S14](sessions/S14.md) | Shared unit identity and native-order discovery | 120 min | Required |
| [S15](sessions/S15.md) | Command bridge, typed references and reservations | 120 min | Required |
| [S16](sessions/S16.md) | Save identity and lifecycle proof | 120 min | Required |
| [S17](sessions/S17.md) | Job scheduler and persistence implementation | 120 min | Required |
| [S18](sessions/S18.md) | Native UI integration and assets proof | 120 min | Required |
| [S19](sessions/S19.md) | Parallel feature discovery and template editor | 120 min | Required |
| [S20](sessions/S20.md) | Zero retention, template recruitment planner and transport prompt | 120 min | Required |
| [S21](sessions/S21.md) | Combat exclusion, recruitment tickets and single-batch voyage | 120 min | Required |
| [S22](sessions/S22.md) | Zero lifecycle, template assembly and multi-trip transport | 120 min | Required |
| [S23](sessions/S23.md) | Template controls and transport reassembly | 120 min | Required |
| [S24](sessions/S24.md) | Transport failures and overseas template integration | 120 min | Required |
| [S25](sessions/S25.md) | Combined feature persistence and overseas templates | 120 min | Required |
| [S26](sessions/S26.md) | Vanilla and HPM compatibility sweep | 120 min | Required |
| [S27](sessions/S27.md) | Feature stress and regression boundary | 120 min | Required |
| [S28](sessions/S28.md) | Final benchmark matrix | 120 min | Required |
| [S29](sessions/S29.md) | Final targeted performance candidate | 120 min | Conditional |
| [S30](sessions/S30.md) | Local release package and handoff | 90 min | Required |

## Why these boundaries save time

S01–S08 establish a trustworthy build, behavior baseline and measurements before optimization. S09–S12 are conditional: skip if the primary target is met or no viable/proved candidate exists. S13 records the best baseline. S14–S18 establish services used by all three features. Their native mutation boundaries share the same executable and integration points, so their proofs precede feature workers.

S19–S24 overlap independent feature work across up to three workers. There is a serial integration barrier after each session; if one proof fails, finish only independent work that remains useful and checkpoint the blocker. Continuations retain the same family clock and follow estimate-based reviews and scope cuts; elapsed time alone does not stop the project. S25 combines save/feature interactions, S26–S28 validate compatibility, stress and performance, S29 is one last justified candidate, and S30 packages a reviewable local result.

The queue does not authorize global monolith cleanup, a new renderer, 64-bit conversion, AI automation, automatic replacement after full army wipes or new multiplayer stabilization. These would consume time without being required.

## Conditional reserve packets

| Packet | Trigger | Available time |
| --- | --- | --- |
| [R01](sessions/R01.md) | Build/native launch dependency unresolved | Parent remainder only |
| [R02](sessions/R02.md) | Signature/ABI/call boundary unproved | Parent remainder only |
| [R03](sessions/R03.md) | Bug classification or optimization fidelity unresolved | Parent remainder only |
| [R04](sessions/R04.md) | Native orders/deletion/routing proof or narrow adapter blocked | Parent remainder only |
| [R05](sessions/R05.md) | Save identity/lifecycle/reconciliation failure | Parent remainder only |
| [R06](sessions/R06.md) | Additional measured hotspot needs contract discovery | Parent remainder only |
| [R07](sessions/R07.md) | One additional proved performance candidate | Parent remainder only |
| [R08](sessions/R08.md) | Normal fixture generation/final benchmark needs more runtime | Parent remainder only |

Reserve packets are subdivisions of their actual parent objective, not parallel projects. They use the same family clock and require an explicit scope/estimate update; they do not reset elapsed time. Apply the review and scope-reduction cadence in execution-control.md. No automatic repeats.

## Definition of a reviewable completion

All requested functional cases pass against vanilla and HPM; the effective patch policy retains demonstrated bug fixes and excludes unrelated behavior; source builds reproducibly; save/load/rollback are documented; performance results include every required workload and no altered simulation workload. Multiplayer stabilization remains done awaiting the user's playtest, and new-feature MP checks are honestly marked pending if unavailable.

If performance misses 12 seconds but meets 30, report fallback achieved and primary unmet. If both miss, report actual best result and bottleneck evidence. Exact EU4 parity is not called done until its observed comparison cases pass. Packaging does not override incomplete requirements.
