# Speed-first execution and estimate-based review

Effective 2026-10-18 by direct user instruction. This policy replaces the previous hard two-hour cumulative cap and deadline-based stop behavior. Historical clocks, overrun records, and handoffs remain evidence; they do not prohibit continuing work. The project has no automatic reserve or packet restart, but a genuine new session may begin inside the same family after its prior session is closed and any triggered timing review is recorded.

## Priority and next work

Measured HPM simulation speed is the primary project goal. Preserve correctness, frozen multiplayer remedies and the other requested features; do not invent shortcuts to obtain a faster number. Work must either produce a speed measurement, implement a measured improvement, or satisfy the smallest demonstrated safety/correctness dependency of that improvement. Record which before starting. General compatibility completeness, renderer work, cleanup and speculative native research are not performance deliverables.

P00 was **SKIPPED BY USER DIRECTION** on 2026-10-18. Its partial OBS intervals remain evidence only; required five-run HPM throughput acceptance is **NOT MEASURED**. S02/R02 remains incomplete; its prior 120-minute estimates and later continuation are preserved as historical timing data. The user now directs us to continue under estimate-based reviews. S03 starts only after all S02 exit checks genuinely pass.

## Session clock and review cadence

- There is **no hard two-hour cap** and no time-based refusal to continue. Estimate each focused session, track elapsed work and total family wall time, and keep one cumulative family record across agents, reviews, context changes, and sessions. Do not reset or rename a family to hide elapsed time.
- Record both session elapsed time and family wall span. Include setup, reading, discovery, failed runs, build, tests, agent coordination/waiting, review, documentation, commit and delivery in task elapsed time. Annotate external approval waits or unrelated idle gaps separately when known; never silently omit them.
- At **more than two hours in one session**, pause implementation and obtain one independent timing review of the session's elapsed work, repeated effort, blockers, and estimate accuracy. Record timing lessons, then resume the same objective. There is at most one timing review for the entire session, regardless of how many slices, checkpoints, or handoffs it contains; do not review every slice or split a session to trigger another review. The review's only output is timing calibration for future estimates: it must not create tasks, recommend scope changes, or delay resumption.
- At **2.5 times the original session estimate**, require a scope cut and move on to a smaller, concrete deliverable. This is a session boundary, not a project stop: checkpoint the incomplete parent, write the reduced next-session scope, and continue it in the same family. For a two-hour estimate this checkpoint is five hours.
- When a session finishes at **less than half its estimate** and it has not already had its independent review, obtain one independent speed review. Identify reusable timing lessons and distinguish them from work that was not performed. Update future estimates from those lessons; the review does not create tasks, expand scope, or block continuation.
- After every session, record a concise retrospective and the next planned scope/estimate as ordinary execution planning. A session receives no more than one independent timing review, whether triggered during execution or at closeout; that review changes timing estimates only, not task scope. Any scope reduction at 2.5x is an execution rule applied independently of the review.
- Use `tools/planning/session_clock.py` to persist session estimate, start/end times, the single review if triggered, retrospective, and next scope. Its output is advisory and must never prevent work solely due elapsed time. Keep the historical schema-1 hard-deadline ledger intact inside the migrated record; do not treat its expired deadline as current authorization state.
- Time checkpoints are for scope management, not safety waiver. A reduced session may leave a parent incomplete; do not mark an exit passed, launch an unverified profile, or start a dependent packet until its actual acceptance conditions pass.

## Progress decisions

An independent timing review reports the session's elapsed work by activity where available, why it took that long or finished quickly, and the timing lesson for future estimates. It does not assign work, propose scope changes, or modify the current objective. Continue to report concrete artifacts and unresolved exits in normal status/handoffs. Cut repeated evidence collection when it does not change a decision. Reuse accepted artifacts when exact inputs and dependencies match.

If a proof is blocked by unavailable runtime access, write the exact blocker and identify the smallest feasible next scope; continue with independent product work where the dependency graph allows it. Do not start a dependent packet until its preconditions pass. Time overrun calls for one timing review per session; the 2.5x scope reduction rule is separate. Neither permits a safety waiver or an automatic project stop.

## Bounded research and evidence

Before discovery, write one question, one discriminating experiment, its estimated runtime, and outcomes that decide proceed/reject/block. Allow at most two attempts on the same hypothesis; a second attempt needs an identified changed condition. An unchanged blocker ends the lane immediately. A different name for the same uncertainty is not a new hypothesis. New independent questions require ordinary task planning and an updated estimate, not an automatic research chain.

Unsupported optional behavior stays explicitly unavailable; do not repair every historical hook to unlock an unrelated task. Required frozen behavior cannot be silently disabled to make a lean profile launch. A modified build needs proof for every active dependency, but an untouched native measurement does not require proof of inactive fork hooks. Native calls, ABI/lifetime boundaries and installation phases still require real evidence; speed priority never weakens those rules.

Reuse accepted artifacts when the exact inputs and relevant dependencies are unchanged. Repeat only checks affected by a change or an unresolved concern. Keep one concise decision/result and one focused immutable index per session. No evidence-count targets, broad rehashes of unrelated trees, duplicate worker reviews or repeated source audits merely to fill a packet. Root reviews decisive raw evidence, not just an agent's summary.

## Measurement before optimization or feature expansion

P00 establishes original HPM throughput first; one correctly bounded run can validate the procedure, but only five valid repetitions support its median/spread and threshold comparison. If fewer fit, report the samples and incomplete status; never extrapolate success. Report vanilla separately when obtained. Do not hold HPM results for supplementary late-game fixtures or optional tools.

Use **UI speed 5**, a fresh process with the pinned 1836 setup, all of 1836 simulated normally and untimed in that same process, then exactly **1837-01-01 through 1838-01-01**. Fix/report autosave, visible view, render settings and job load. No profilers, builds or competing workers during timing. Diagnostic runs remain separate. The <12-second primary and <30-second fallback targets and fidelity requirements are unchanged.

After measurements, rank candidates by measured cost fraction, plausible absolute seconds saved, preservation risk and implementation-plus-validation cost. A research proposal without such evidence must identify the specific measurement it will obtain within its current estimate and review cadence. Do not spend a session proving a negligible or unmeasured subsystem merely because its internals are accessible. Complete the performance checkpoint before starting the army/transport feature phase. Stop optional optimization after the primary target passes.

## Handoff and acceptance fields

Every session handoff must state: family ID; session ID and estimate; session elapsed and family wall span; independent review triggers/results; retrospective and next session scope; required exits PASSED/FAILED/BLOCKED/NOT RUN; measured performance or NOT MEASURED; exact changed inputs and decisive evidence; safe process/file state; commit and delivery state; next eligible action. Keep prior hard-deadline data as history only. A checkpoint may be committed with an INCOMPLETE label; a commit never certifies acceptance.

Documentation writes must use explicit UTF-8, validate the complete prospective contents, retain exact pre-edit bytes, and atomically replace the file only after encoding succeeds. Stage only task changes; preserve unrelated user edits. A broken evidence index or status file is a blocker to claims of a complete handoff, not permission to reconstruct silently from a stale committed version.

## S02 incident and recovery state

S02 began 2026-10-17 20:09:43 EDT. The sequence through R02w occupied more than ten wall-clock hours; this is an actual cumulative overrun, not a series of compliant fresh budgets. The independent review found uncontrolled dependency growth and repeated narrow packet handoffs, with no early parent-level scope reduction. The new response is review and scope reduction, then continued work; native safety gates remain mandatory.

S01 and S01HPM are accepted. Through R02v, bounded results were committed; R02w source/tests are an **uncommitted incomplete checkpoint**. The owned provider-template hook inheritance experiment passed, and source/build consistency passed; compiled acceptance is incomplete. All fork startup profiles remain Rejected/WITHHELD. No speed result or requested army feature is claimed. Earlier agents stopped at the historical checkpoint.

The prior checkpoint script truncated `planning/status.md`; it was restored to HEAD, which did not restore its prior uncommitted edits. Recovery records remain outside Git under `%LOCALAPPDATA%/V2DLLFork/R02w/root-review/`. Reconcile that missing working-copy content as a separately bounded documentation repair before claiming the historical status is fully recovered. This update records current truth without claiming that repair happened.

Current user-directed change: P00 was skipped with baseline acceptance still NOT MEASURED. The independent timing review found S02 scope expanded into serial native proof packets without a parent-level stop/re-scope decision; S01 was materially faster because it reused the installed game/toolchain and had finite setup checks, not native engine proof. The user replaced the hard two-hour cap with the review cadence above and authorized continued development. An owned supported-OS SDK fixture passed provider-template copying. Original-game phase, callbacks, M3 lifetime and full-profile launch remain open; S02 is still incomplete and S03 is not active.
