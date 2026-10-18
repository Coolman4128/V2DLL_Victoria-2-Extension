# Requirements and decisions

Execution priority correction (2026-10-18): **measured HPM speed is the primary project goal**. [Execution control](execution-control.md) imposes one cumulative session cap including every reserve, worker and review; exhaustion stops work and requires an explicit human time extension. First obtain the untouched native HPM baseline in P00; do not spend unbounded time proving inactive fork hooks. This changes execution priority and budget control, not simulation semantics, frozen remedies or the remaining requested feature requirements.

Status: approved user requirements translated into an implementation contract; no fork code implemented by the planning task. Last updated 2026-10-17.

## Priorities and exceptions

| ID | Requirement | Completion rule |
| --- | --- | --- |
| MP-0 | Retain upstream multiplayer stability work; no further improvements until user tests | DONE — awaiting user testing; no planned MP-improvement sessions |
| PERF-12 | Under 12 seconds for 1837-01-01 to 1838-01-01, speed 5, after untimed normal simulation of 1836 | Five HPM repetitions, median <12 seconds; vanilla same-year results reported separately |
| PERF-30 | Backup under 30 seconds for that same 1837 year | Report as fallback only; does not erase unmet PERF-12 |
| VAN-1 | Original engine behavior plus bug fixes | No unrelated V2DLL economy, diplomacy, allied transport, damage or deployment changes activate |
| SCRIPT-1 | Keep useful additional scripting | Existing scripts unchanged; extensions inert unless content explicitly requests them |
| TMP-1 | Saved army templates and one-click construction | Exact requested types/counts recruited as legal, gathered and merged; pending shortages remain visible |
| ZERO-1 | Zero-man brigades survive and can reinforce | No automatic partial army deletion; zero combat value; manual disband and genuine full-stack wipes permitted |
| TRN-1 | EU4-style prompted automatic sea transport | Land-unreachable, sea-reachable destination; own transports; capacity-limited batches; complete move/board/sail/unload/recombine cycle |

User clarification: templates and transport control **human players only**. The selected mod is **Historical Project Mod (HPM)**, corrected by the user from the earlier GHM identification. Use vanilla as the original-behavior baseline and HPM as the main real-world performance and compatibility workload. HPM's intentional economic/content changes remain part of HPM; this fork removes its own unwanted changes.

Performance clarification (2026-10-17): the absolute speed goals apply to the beginning of the campaign, **measuring 1837 after the startup year has settled**. Each authoritative repetition starts from the pinned 1836 setup in a fresh process, simulates 1836 normally without timing it, and measures only the full 1837 year. Do not move the date forward or replace this warm-up with a month after loading an 1837 save. Later dates may be checked for regressions; their speed does not decide PERF-12/PERF-30, and missing later saves do not block those goals.

## Original behavior

The disposition of every current setting/byte patch is in [patch-policy.md](research/patch-policy.md). Removal is enforced in the installer as well as configuration, so an old mod INI cannot silently re-enable prohibited behavior. Keep unreachable upstream implementations temporarily if that saves merge effort; public settings must describe the fork's actual supported behavior.

Actual defect fixes are retained. An option called a fix is not sufficient evidence if it introduces a new economic/AI rule. Ambiguous cases, including AI factory staffing policy and needs-scaled budget payouts, get an evidence decision in S04. If the changed behavior is demonstrated to correct a defect, retain it and document the exact difference; otherwise restore native behavior. Do not purge all economy-related code indiscriminately.

Frozen multiplayer remedies (notably state-savings normalization and existing POP quantization) are explicit exceptions to strict vanilla numeric equivalence. Preserve them; do not use vanilla restoration as a reason to remove them. Diagnostic logging may be disabled or made cheaper while their simulation work remains identical.

The three requested army features are also explicit exceptions. Everything else must preserve the original game or selected mod's rules.

## Army templates

- A template has a name, stable template ID, revision and integer brigade counts keyed by engine unit-type identity/name. Counts are units, not men. Support the selected mod's available land unit types; display locked/unavailable types clearly.
- Player flow: create/edit/duplicate/delete template, select a rally province, preview composition and currently recruitable amount, click Build, then see per-job progress and blockers. UI deletion of a template does not silently disband units or cancel its snapshot jobs.
- A job snapshots the template revision, recruits through normal eligibility/cost/time/POP rules, tracks completed native units, moves them to the rally point, and merges them into one army of the specified composition. Existing unrelated armies are not commandeered.
- If only some troops can be recruited now, queue the legal subset and keep the remaining requirements pending. Retry on relevant game-state changes with bounded work; never spend twice or make free soldiers. Unknown or permanently unavailable types block those slots visibly.
- Repeated Build clicks create separate identifiable jobs, not duplicate commands for the same job. Native queue capacity and soldier POP availability are reserved across jobs.
- Revalidate against manual recruitment and other native queue changes immediately before submission. A completed recruit lost en route or wiped is recorded as lost; a Retry/replace action is explicit. Do not turn reconciliation into unlimited automatic replacement.
- Pause/resume/cancel a job. Cancellation stops future automation, releases reservations and leaves existing troops/paid recruitment intact. Explicit cancellation of native recruitment may use the game's normal refund rules only when the player requests it.
- Unreachable recruitment areas/rally points are reported. After TRN-1 is implemented, a template job can request the same prompted transport service for recruits needing a sea crossing; no hidden seizure of fleets.
- Templates and jobs survive normal save/reload according to the persistence contract. Jobs do not auto-rebuild battle casualties indefinitely; preserving zero brigades supplies the requested composition continuity. Continuous replacement after a full wipe is outside the request.

## Zero-strength brigades

Victoria 2's individual land units are brigades/regiments; the user's “division” means that individual unit, and “army/stack” means their containing army.

- Apply the rule consistently to normal national land brigades, including AI counterparts, so it is not a local-player advantage or a local-only simulation mutation. Human-only scope applies to automation, not this simulation rule. Rebel-specific handling must follow the same preservation invariant where the native object model supports it; document any required special case before accepting the feature.
- A brigade reduced to exactly zero retains identity, type, army membership, appropriate POP linkage, and save representation. No permanent deletion due to combat casualties, attrition, or automatic depleted-POP cleanup is allowed to remove just part of a surviving army.
- Zero brigades provide no attack, defense, support, reconnaissance, combat frontage, siege/occupation or other combat-derived contribution. They do not become targets that absorb useful combat hits and do not keep a battle alive by themselves. Positive-strength brigades retain native deployment and damage.
- Reinforcement uses ordinary native availability and costs. A missing or exhausted soldier POP may leave a zero brigade waiting; it must not dereference a dead POP, invent manpower or select an unrelated POP to evade recruitment restrictions. Prove what conditions can restore its normal linkage/reinforcement.
- Player-directed brigade/army disband remains effective. Player-directed demobilization counts as explicit disband of mobilized brigades; it can remove those brigades from a mixed stack. Unrequested automatic partial cleanup is not a substitute for demobilization.
- A genuine engine-confirmed full-stack wipe may remove the entire physical army, including its zero brigades. Preserve native defeat/wipe detection; an all-zero stack outside a wipe stays present and can replenish. A zero remainder must not evade a wipe because the destruction loop no longer sees active regiments.
- Whole-owner teardown, annexation, rebellion/defection and scripted removal need explicit lifecycle classification. These are research gates, not permission to silently broaden the two allowed deletion cases. If there is no valid owner, halt a candidate implementation and document the smallest necessary behavior decision before it ships.
- Naval ship deletion remains native. During automated transport, a lost transport may invoke unavoidable native sea-loss rules: investigate whether it removes a whole physical embarked stack or only some brigades. Do not silently allow partial deletion under a new exception. The convoy's logical grouping does not make geographically separate armies immune to genuine local stack wipes.

## Automatic transport and EU4 parity

- With a human player's army selected, a move order to a land-unreachable destination uses native movement/access rules to test for a valid own-fleet sea route. Legal land moves proceed normally. Canceling the prompt changes no orders or units.
- If the sea route exists and at least one usable own transport is available, show a confirmation naming source army, destination and selected eligible fleet(s), including the need for multiple trips. Offer per-fleet participation control so busy/opted-out fleets are not taken without consent.
- Fleet capacity is measured by native brigade slots/free cargo space, not men. One transport must be sufficient to shuttle an arbitrarily larger legal army in successive batches. Preserve mixed escort fleets, existing cargo and already reserved capacity.
- Issue normal split, move, board, sail, unload and merge commands. Bring armies and ships to valid pickup points, move through legal sea routes, land at an accessible destination coast and continue inland if needed. Every leg must respect access, blockades, hostility, battle/retreat restrictions, capacity and travel time.
- Shuttle all required batches automatically and reassemble surviving transported brigades into one army at the final target. Do not merge unrelated troops. Preserve intended army name/leader where native rules allow, with recorded split/merge mapping.
- Handle fleet destruction, lost capacity, war/access changes, route loss, manual orders, cancellation, simultaneous jobs and load/resume. Prompt or show a blocked state instead of spinning, teleporting or silently abandoning a batch. Manual player commands win and cannot be overwritten on the next tick.
- Already-submitted native commands may be in flight when a player redirects. Prove the game's ordering/receipt behavior and establish an override barrier before further automation. Never claim a submitted command can be recalled without evidence. Manual split/merge with unrelated units requires reconciliation or suspension, not taking those units back.
- Empty brigades remain part of the composition and consume whatever capacity the verified native rule requires; never give extra capacity because they have zero men.
- If a leader dies or becomes unavailable during a mission, preserve the surviving units and native leader rules; final reassembly never recreates the leader.
- “Exactly like EU4” means reproduce the observable requested flow and verify it against installed EU4 1.37.5. Fleet selection, multi-trip timing, cancellation and recombination are comparison cases, not assumptions inferred from localization. Use equivalent V2 legal rules instead of importing EU4-specific naval mechanics. Resolve observed parity differences in the parity matrix before claiming completion.

## Save and multiplayer compatibility of new features

No new multiplayer stability project is authorized. Nevertheless, new features must not create duplicate commands or local-only simulation changes. Each human controller submits standard synchronized orders once; zero-strength hooks execute consistently on all matching clients. No new network packet format is planned.

Template definitions may live in user-local files. Active job state needs reliable campaign/save identity, atomic persistence, native IDs and command reconciliation. Normal local save/reload must resume without duplication. A transferred MP save with no controller metadata must load safely and explicitly suspend/recover automation; never claim automatic recovery solely from a local sidecar. S16/S17 prove whether embedded metadata is safely supported, otherwise implement an explicit recovery/import workflow. Existing troops always remain native units.

Existing stability remains marked done. User multiplayer playtesting stays a recorded release caveat, not an extra improvement phase. If testing exposes a regression caused by our changes, fix that regression within the responsible feature packet.
