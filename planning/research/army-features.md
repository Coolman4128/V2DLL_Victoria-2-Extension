# Army templates and retained zero-strength brigades: implementation research

Research date: 2026-10-17. Repository baseline: `4368f434cc33b00e7b3cc4ff2458e1f69a9ec97d`.
This document is an implementation specification and evidence map, not a claim that the features already work. No runtime hooks were installed during research.

User-confirmed workload: Historical Project Mod (HPM), with vanilla control fixtures. Official release 0.4.6.3 is selected for pinning; installation/native validation remain pending. Templates and transport operate for human players only. The root [architecture](../architecture.md) and serial sessions own canonical filenames and scheduling; packet names below are research decomposition, not a second independent plan.

## Scope and decisions

Victoria 2's constituent army units are called brigades/regiments in the existing research. The user's “division” means one of these units. An army is the stack containing them.

- Templates specify counts of available land unit types. A Build action recruits legally available brigades, waits for missing capacity/resources, moves completed brigades to a chosen rally province, and merges them into one army. Costs, soldier POP eligibility, technology requirements, recruitment time, supply and travel rules remain vanilla.
- A retained brigade can have exactly zero strength. Its identity and army membership survive automatic individual casualty cleanup, and it can reinforce through the normal reinforcement system when eligible. It supplies no combat value. Do not fake one soldier, reduce casualties, refill strength, duplicate a brigade or allocate free manpower.
- Explicit manual disband and a vanilla whole-army stack wipe remain valid deletion paths. Merely reaching zero strength is not sufficient evidence of a stack wipe. Losing one brigade must not delete part of the army automatically.
- Apply the simulation-level retention rule consistently to armies on every peer. Template automation is an explicitly requested player tool; do not add automatic AI recruitment policy changes.
- Existing multiplayer improvements remain frozen. Proving that these new features use normal orders and do not introduce desync is feature acceptance, not a new multiplayer-improvement project.
- Existing modified combat deployment and splash-damage routines must be disabled/removed from the fork's supported configuration before validating retention. Do not preserve those gameplay changes by reusing their replacements wholesale.

## Evidence and address discipline

All RVAs below are from the repository's reference executable. Its documented MD5 is `1D1A97E42BD2DD65DEABD3E7F707B27D`. The local Steam executable found by the main research task has a different MD5 (`63C5485FF00353F2BE8DAAA3F0DCF957`). None of the following addresses is permission to patch that local executable unchanged. Compare actual bytes, control flow, structures and ABI first. A matching short prologue alone is insufficient.

The historical Ghidra names include a VA. For that reference image only, `RVA = VA - 0x400000`. Source offsets below are object fields, not RVAs.

| Evidence | Established by checked-in material | Implementation implication |
| --- | --- | --- |
| `Source/V2/V2/V2TechButton.cpp:4174` | Existing reinforcement fixes at RVAs `0x1C809B` (army) and `0x1C7F1C` (navy). | Preserve actual bug fixes. These do not prove zero-strength retention or resurrection. |
| `V2TechButton.cpp:5552` | `RVA_ARMY_COUNT_BRIGADES = 0x1D0650`. | A candidate army-count reader, with its source-declared calling convention still requiring binary verification. Count is not stable identity. |
| `V2TechButton.cpp:5672`, `DeriveArmyTrueOwner` | Army brigade list at `army+0x38`; linked nodes have brigade pointer at `+0`, next at `+8`; brigade soldier POP pointer at `+0x30`; POP province pointer at `+0x64`. | Useful read-only starting point for membership and recruitment provenance. The existing helper caps traversal at 64; do not copy that cap into template correctness logic. |
| `V2TechButton.cpp:16942` and `InstallArmySelectDiag` | Selection diagnostic RVA `0x1CC530`; movement diagnostic RVA `0x1CCEC0`. | Useful trace entry points. Neither is proven to construct, send or execute an army move order. |
| `V2TechButton.cpp:19679` | `CUnitView` constructor candidate RVA `0x398A50`, `unitpanel.gui`, army binding at `+0x28`; existing UI reuse code disabled because of lifecycle issues. | Investigate native UI integration; do not reactivate the disabled pooling experiment. |
| `V2TechButton.cpp:5589` onward | Fleet loader candidate RVA `0x1D7B90`; nested army parsing, attach and registration described; `Country::AddArmy` candidate RVA `0x113C80`. | Save/load tracing leads, not a general persistence API. The old allied-transport sidecar was removed in upstream. |
| `V2TechButton.cpp:10822` and battle README | Regiment type `+0x38`, strength `+0x3C`, organization `+0x40`; accumulated strength damage `+0x6C`, organization damage `+0x70`. Research describes strength in fixed-point units, 1000 = 1.0. | Distinguish a displayed soldier count from raw strength; test exact zero, positive rounding-to-zero display, and negative overshoot. |
| Battle README and `FUN_0059b840_0059b840.c` | Initial battle deployment RVA `0x19B840`; weak-unit handling is strength/organization dependent and consumes random values. | Trace zero-strength entry explicitly. A threshold involving a random value that can be zero is not an unconditional exclusion. |
| Battle README and `FUN_0059b190_0059b190.c` | Reserve deployment RVA `0x19B190`; round maintenance `0x19AEF0`; rear/front movement `0x19B0E0`; attack loop `0x19ACC0`. | Known battle-flow leads. Prove empty brigades cannot occupy useful slots, attack or supply support/recon bonuses. Preserve vanilla rules for positive-strength units. |
| `FUN_0059ca40_0059ca40.c` | Battle update RVA `0x19CA40`, with calls to `FUN_005dc460`, outcome branches and whole-side cleanup candidates. | Trace to actual casualty application and deletion. `FUN_005dc460` / RVA `0x1DC460` is only a candidate; its implementation is not included in this decompilation set. |
| `FUN_00595660_00595660.c` | RVA `0x195660` marks side `+0x68` and, on one path, adds remaining regiment strength to damage accumulator `+0x6C`. | Strong lead for the whole-side wipe path; exact branch semantics and ABI still need runtime/binary proof. |
| `FUN_00595760_00595760.c` | RVA `0x195760` repeatedly invokes `FUN_0068aa10` / RVA `0x28AA10` on a side's first army until its count becomes zero. | Strong lead for whole-army destruction. Do not turn this into a global “never delete” hook. |
| `z_The Great Library/v2_mp_sync_re/README_СИНХРОНИЗАЦИЯ_МП.md` | Existing stamped commands are distributed through the host and executed at an agreed simulation tick; session command queue documented at `+0xBBC/+0xBC0`. | Submit vanilla commands through the proven ordinary player path. Do not call command execution locally or inject private network messages. |
| `Source/V2/V2/V2ModAPI.h` | Plugin API offers image base, logging and low-level patch helpers only. | No typed recruitment, orders, army-ID, save-extension or UI API currently exists. Budget those bridges explicitly. |

Source links: [main implementation](../../Source/V2/V2/V2TechButton.cpp), [plugin API](../../Source/V2/V2/V2ModAPI.h), [battle research](../../z_The%20Great%20Library/v2_battle_re/README_БОЕВЫЕ_ЛИНИИ.md), [battle decompilation](../../z_The%20Great%20Library/v2_battle_re/decomp), [multiplayer research](../../z_The%20Great%20Library/v2_mp_sync_re/README_СИНХРОНИЗАЦИЯ_МП.md).

## Mandatory discovery deliverables

Before writing feature hooks, produce a versioned address/ABI manifest with expected bytes, function boundaries, live registers, stack cleanup, ownership of allocated objects and call-site evidence. Record the actual executable hash. Fields must have observed types and validity rules. Retain disassembly and a minimal reproducible save for each claim.

The following remain UNKNOWN and must be proved, not invented:

1. Persistent IDs of army, brigade, recruitment queue item and soldier POP; how IDs survive save/load and whether IDs are recycled.
2. Construction, validation, submission and execution of recruit, move, split, merge and disband orders; sender-country authority and stamped order handling.
3. Recruitment completion association: matching one accepted recruitment request to its eventual brigade, without assuming a name/type/province tuple is unique.
4. Exact zero-strength deletion sites for combat, attrition, daily cleanup, soldier POP loss and load repair; which handlers are shared with ships.
5. Manual disband, demobilize, full wipe, country teardown, scripted removal and owner-change paths, including whether cleanup begins before the final destructor.
6. Whether zero-strength regiments are saved and loaded by vanilla; whether load validation discards them or changes supporting POP links.
7. The reinforcement path when raw strength is zero and when its original POP is absent or ineligible.
8. A safe periodic job callback and save/load completion boundary. Merely intercepting `.v2` open-read is not proof that the game state is ready.
9. Native UI button/list/dialog creation and lifetime, keyboard capture and localization. Existing D3D Present hooks are rendering hooks, not a complete interactive UI framework.

Trace actions on a disposable game copy and fixture save. Never invoke a guessed function to see what happens. If a two-hour discovery packet ends without proof, commit its negative findings and a next investigation packet; dependent implementation stays blocked while independent work proceeds.

## Shared automation architecture

Implement small new translation units rather than adding the features to the 26,000-line implementation. Proposed boundaries: `GameOrders`, `GameIdentity`, `AutomationJobs`, `AutomationPersistence`, `ArmyTemplates`, `ArmyRetention`, and one shared UI integration layer. These are proposed filenames, not existing modules. The transport feature consumes the same order, identity and job services.

Use one job manager for templates and automatic transport. Its mutation surface is validated vanilla orders. Each order has a pending intent, an acknowledgement/completion observation and a generation. Retries re-check live state; a timed-out response must never cause duplicate recruitment or split twice. Use stable IDs, resolve them immediately before an order, and never serialize pointers or retain unvalidated pointers across ticks/load.

Distinguish a local unsent intent from an order already accepted by the native command queue. A cancellation generation can invalidate the former, but cannot be assumed to retract the latter. Prove command ordering and the user-override barrier: a delayed automation order must not later reverse a newer user command. Test this at split/board/merge boundaries and with real multiplayer latency. If native cancellation is unavailable, observe the outstanding order to completion and apply the newer user intent through a proven ordinary order sequence; never pretend an acknowledgement timeout recalled it.

Template definitions are local user preferences with a schema version, template ID, display name, unit-type keys and nonnegative counts. The type key must resolve against the currently loaded mod; do not serialize an in-memory type index as a portable identifier. Validate counts against integer overflow and UI/job resource limits. Template edit affects new builds by default; an existing job keeps an immutable snapshot so a rename/edit cannot silently alter recruitment already underway.

Build jobs contain owner country, campaign identity, template revision/snapshot, rally province, desired counts, accepted queue IDs, completed brigade IDs, chosen final army ID and a status/reason. Distinguish desired, queued, completed and assembled counts. Duplicate templates can run simultaneously without stealing each other's recruits.

Default Build behavior: start a new army; do not silently requisition existing troops or buy/build transport ships. Existing-unit matching or “refill this army to template” is optional future scope, not necessary for the requested build workflow. Recruitment uses legal sources able to reach the rally point; if overseas recruitment is required, the shared transport feature may carry those units after it is available. Before that integration exists, show the geographic constraint and keep the job incomplete rather than pretending all templates are supported.

Recruitment proceeds as legal capacity becomes available. A blocked POP, missing goods, unavailable unit technology, occupation or inaccessible rally point is a visible waiting condition. Re-check on relevant events or a bounded cadence, not a full-country scan every frame. Never reserve resources by directly modifying simulation state. Respect engine order rejection and recompute from accepted queues.

Local reservations do not block manual/native recruitment. Revalidate real POP capacity and queue state at submission and handle another action consuming the last slot. Track queue-to-brigade completion atomically enough that a save between queue removal and unit discovery cannot produce an extra recruit. Once an accepted job brigade is destroyed by a genuine wipe, mark that membership as lost; do not interpret it as a fresh unfilled slot and recruit endlessly. Show the loss and require an explicit user Retry/replace action for replacement work.

When brigades complete, move only job-owned units to the rally point. Merge by normal orders after arrival and legality checks. Track the survivor army after merge; do not assume either original ID survives. Preserve the intended final name where normal rename support is proven. User orders supersede automation for the affected units and pause/release them with a visible reason. Cancel stops future automation; it does not delete already recruited units or queued units unless the user separately requests ordinary queue cancellation.

The command-emitting controller runs once for the owning human country, not once on every peer. Other peers receive ordinary commands. Do not make the host also issue the same orders for a human client. Country switching, loss of human control, disconnect and reload suspend a controller until ownership is re-established. Running the entire job independently on every client would duplicate commands.

## Persistence contract and feasibility gate

The feature must survive ordinary save/load without losing template composition or recruiting duplicates. Template definitions can use local preference storage. Active job metadata is separate from authoritative simulation state; all accepted vanilla orders and units remain in the game save.

Preferred path: prove an embedded, versioned save extension for active job metadata that survives the actual game's save/load and multiplayer save transfer. Do not assume unknown save keys are retained or ignored safely. If native extension support is absent, a companion metadata file is permissible only with explicit campaign/save fingerprint pairing, atomic replacement, a schema version and post-load reconciliation. A file keyed solely by save filename is unsafe. Never read stale sidecar data as instructions for a different campaign.

Sidecar-only persistence cannot automatically resume jobs after another computer receives a multiplayer save without those metadata. This is a real limitation, not something to hide under “save supported.” If the native route fails, the implementation session must document and resolve a reliable metadata delivery/resume design before declaring full multiplayer/save acceptance. Do not respond by inventing a new networking protocol within this fork's scope.

Every loaded job first reconciles against actual queues, units, country control and tick. Missing metadata must leave the vanilla save loadable and its units intact; it must not regenerate guessed recruitment requests. Corrupt/foreign metadata pauses automation visibly. Track deliberate disband so a reload cannot resurrect it. A battle stack wipe removes all associated brigade IDs and completes/cancels relevant membership tracking; retention does not rebuild wiped units.

## Retention design and invariants

Patch the narrow automatic individual-removal decision before identity/list ownership is destroyed. Preserve ordinary casualties, POP losses, battle reporting and zero strength. Do not globally hook `operator delete`, suppress destructors, detach a doomed object and reinsert it, or rescue a unit after the engine has invalidated references.

Build a reason map from real call sites. At minimum distinguish automatic individual exhaustion, manual disband, full-army wipe, load restoration and world teardown. A full-wipe context must be scoped to the specific army IDs and operation; it cannot be a global flag left set across callbacks. Whole-side wipe code may contain multiple armies: validate which army memberships vanish together. Do not treat “all members are zero” alone as permission for deletion outside an actual wipe.

Zero-strength brigades must not attack, contribute support/recon/engineering or useful combat width, shield positive-strength brigades through targeting, prevent battle termination, change retreat eligibility solely through their count, or make an all-empty force capable of occupation. Audit strength-aware force valuation and related combat/siege calculations; introduce only the minimum zero-strength guards needed. Verify zero-strength cavalry/support edge cases. Existing random-strength thresholds can admit a zero-strength unit when the threshold is zero; preserve necessary vanilla RNG consumption while excluding the empty unit. For positive-strength-only scenarios, the retention patch must preserve the vanilla combat path and random draw sequence.

Retained brigades continue occupying their real structural slot in the army and recruitment accounting wherever ordinary existing-brigade rules require it. They must not let the country mint unlimited manpower or bypass soldier POP limits. A depleted or missing supporting POP leaves the brigade visibly unable to reinforce until a legitimate engine path permits it. Do not silently bind it to another POP. If the engine cannot safely retain that association, this is a design gate needing a real lifecycle solution, not an excuse to delete the brigade.

Outside a stack wipe, an all-empty army remains representable and may recover if eligible. It cannot lock battle progress or repeatedly engage with useful combat effects. Verify movement, transport capacity accounting, selection, splitting and merging still work with empty units. Transport batching must count units using the same capacity rules as vanilla and must preserve every retained brigade ID.

Forced world events need an explicit policy recorded during discovery: soldier POP disappearance, owner revolt/defection, annexation, scripted removal, automatic demobilization and country elimination. The user's rule allows manual deletion and whole-stack wipes only; do not silently add a third exemption. A user's manual demobilize action can be classified as explicit disband, but automated demobilization is not automatically authorized. Required structural teardown (e.g. a country ceases to exist) must be demonstrated and resolved as a product decision before acceptance if it conflicts with that rule. Continue independent work while the decision is pending.

## Focused acceptance matrix

| Case | Required observation |
| --- | --- |
| Small domestic template | Exactly requested type counts; normal recruitment cost/time; one final army at rally point. |
| Short soldier POP capacity / absent goods | Legal subset queued; missing counts visibly waiting; no busy loop or duplicate queues. |
| Duplicate template jobs / same province/type | Each completion assigned once to its true job; final armies have independent identities. |
| Edit/delete template after Build | Existing immutable job remains coherent; no accidental new recruitment. |
| User moves/splits/merges a job army | Controller relinquishes or explicitly pauses affected ownership; never fights the user's orders. |
| User command while automation order is already in flight | No delayed automation command reverses the newer player intent; accepted-order handling uses proved native ordering. |
| Manual recruitment/cancel consumes reserved POP capacity | Reject/reconcile correctly; no oversubscription or false completion. |
| Brigade wiped before template assembly | Job reports loss; target-count accounting does not silently recruit replacements forever. |
| Cancel midway | Accepted units remain usable; no hidden deletion or resource refund. |
| Occupied source / rally lost / war/access change | Normal validation respected; visible waiting/replanning; bounded retries. |
| Overseas recruits | Shared transport workflow assembles them without allied-transport permission changes. |
| Save during recruitment, movement or merge | No duplicate orders; exact identities reconcile; progress resumes. |
| Save after submission but before receipt / queue-to-unit handoff | Pending commands are neither repeated nor forgotten; queue/unit mapping remains exactly once. |
| Host/client Save transfer and reload | Controller authority remains unique; metadata and live queues reconcile or report a defined unresolved limitation. |
| Exactly zero / overshoot negative / display rounds to zero | Real zero retained; overshoot clamped only according to proved damage application semantics; positive raw strength not misclassified. |
| One of N brigades exhausted in battle | N identities remain after battle; exhausted member contributes zero combat value and can later reinforce. |
| Attrition exhausts a brigade | No automatic partial deletion; same identity survives. |
| All brigades zero without wipe | Stack remains representable; no endless combat, phantom occupation or invalid UI. |
| True full-army wipe / mixed armies on one side | Every brigade of the wiped army removed; no rescued fragment; unrelated armies handled normally. |
| Manual disband individual / entire stack | Requested identities removed permanently, including after save/load. |
| Soldier POP dwindles, disappears or changes owner | No dangling pointer, automatic forbidden partial deletion, free replenishment or silent POP reassignment. |
| Zero infantry / cavalry / artillery / engineer | No combat bonuses, useful frontage, shielding or fake strength. |
| All-positive control battle | Same vanilla deployment/damage results and RNG consumption as baseline with identical inputs. |
| Fleet loss, embark/split/merge, automated batching | Army membership is neither duplicated nor partly discarded by interaction between features; genuine whole-stack loss remains possible. |
| Retention disabled on a save with empty brigades | Defined behavior documented and tested; do not imply that switching the patch off preserves retention. |
| Two-peer feature smoke test | Both peers apply the same submitted commands; no new feature-attributable divergence. Existing MP patches unchanged. |
| Large jobs and many empty brigades | Memory bounded, scheduling cost measured, no per-frame global scan; benchmark overhead included in final performance results. |

## Bounded work packets for the serial session plan

These are candidate packets to integrate into the root session sequence. Each session has at most 120 minutes of active work. A range estimates a packet's first pass, not guaranteed reverse-engineering success. Unfinished investigations become separately numbered follow-up sessions with concrete evidence and the same cap. Parallel lanes only run when file ownership and prerequisites are independent.

| Packet | Estimate | Work and output | Exit / dependency |
| --- | --- | --- | --- |
| A01: identity and order trace | 90–120 min | Trace manual recruit/move/merge and collect IDs/ABIs; separate agent designs pure job schema/fixtures. | Evidence manifest; shared executable compatibility gate must pass first. No assumed APIs. |
| A02: recruitment completion and UI trace | 90–120 min | Match queue-to-brigade identity; independently map one native template-button/dialog lifecycle. | Recorded completion relation and UI integration proof; follow-up required if either unknown. |
| A03: removal classification | 90–120 min | Trace single-brigade exhaustion, full wipe, manual disband; inspect candidates above. | Proven distinct branches, fixture saves and ABI notes; no patch yet. Can share a session with independent template schema work. |
| A04: extended lifetime and save trace | 90–120 min | Zero reinforcement, attrition/POP cleanup and zero-unit save round trip; separate read-only investigation of job metadata storage. | All lifecycle entry points catalogued; persistent-identity/save design selected or specifically blocked. |
| A05: typed adapter foundation | 90–120 min | Implement validated read-only identity/legality and ordinary order submission wrappers; test one action at a time. | Commands equal a manual action; adapters fail closed on incompatible binary. |
| A06: template model and scheduler | 90–120 min | Pure model, persistence for definitions, count reconciliation, bounded state machine and idempotency tests. | Deterministic fake-world tests pass; no raw pointer storage. Can run in parallel with A05 in separate files. |
| A07: native template UI | 90–120 min | Create/edit/delete, type counts, rally choice, Build button and progress/cancel. | Interactive in-game path with valid input/error handling; no mock UI counted complete. Requires A02. |
| A08: recruitment integration | 90–120 min | Legal source selection, resource/capacity waits, submission and queue completion tracking. | Domestic fixture recruits exact requested counts without duplicates. Requires A05/A06. |
| A09: rally and assembly | 90–120 min | Movement, merge completion, retained final identity, user overrides and cancellation. | One assembled army and passing override/cancel cases. Requires A08. |
| A10: job save/load integration | 90–120 min | Actual save boundaries, metadata persistence and reconciliation; missing/corrupt data handling. | Save at each transition resumes safely; unresolved multiplayer metadata route keeps that gate open. Requires A04/A09. |
| A11: zero-strength retention | 90–120 min | Narrow automatic cleanup interception, true-wipe/manual-disband context and exact-zero representation. | Single-loss/wipe/manual fixtures pass; no global destructor suppression. Requires A03/A04. |
| A12: zero combat contribution | 90–120 min | Initial/reserve/ongoing combat exclusion, zero strength valuation, termination/retreat and reinforcement integration. | Zero contributes nothing; positive-only control preserved. Requires A11; split additional unproven paths into new packets. |
| A13: retention lifecycle coverage | 90–120 min | Save/load, all-empty armies, missing POP, attrition and owner/teardown edge cases. | Invariant matrix passes or each failing case has explicit blocker; no silent exception expansion. |
| A14: templates plus transport | 90–120 min | Overseas recruitment, job reservations, shared split/merge identities, retained empty brigades in batches. | Requested army arrives as one stack; no double controller owns a brigade/fleet. Requires transport implementation. |
| A15: feature acceptance | 90–120 min | Two-peer ordinary-command smoke, save transfer, cancel/reload, representative long-run and overhead measurement. | Recorded results with exact build/settings/save/hardware; failures become focused follow-up packets. Existing MP improvements untouched. |

The fifteen first-pass packets total 22.5–30 hours before any failed reverse-engineering gates, shared foundation work or transport implementation. Parallel independent lanes can reduce elapsed time but not erase discovery uncertainty. Do not quote this as a guaranteed completion date or mark features done on unit tests alone.

## Agent execution rules specific to these features

1. Reserve file ownership before parallel edits. One integrator owns shared initialization/configuration and order/identity interfaces; feature agents write separate translation units.
2. The baseline's exact gameplay flags are part of every fixture. Do not run retention acceptance with upstream reserve-frontline or second-line-splash behavior enabled.
3. Record each binary claim as observed, inferred or unknown; include executable hash, bytes and calling convention. Ghidra decompilation alone cannot certify a hook ABI.
4. Failed byte/signature checks disable the entire dependent feature with a clear reason. Partial installation that preserves only some required deletion paths is unacceptable.
5. No private network protocol, MP thread changes, RNG redesign, economy changes or global combat rebalance. Feature regression tests are allowed and necessary.
6. No unit recreation, positive strength floor, free recruitment, ownership borrowing or transport capacity cheats. Preserve identity and use legal orders.
7. Keep controllers event-driven or bounded per tick. Profile proposed scans before allowing them into the fastest-speed path.
8. Never finish a session by claiming an unexecuted runtime scenario passed. Commit documentation and build results separately from fixture observations; append exact next action and blockers.
