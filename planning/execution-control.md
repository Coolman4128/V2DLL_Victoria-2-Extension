# Speed-first execution and cumulative limits

Effective 2026-10-18, after the user stopped the S02 overrun. This policy supersedes every older instruction to automatically activate a reserve, create a fresh continuation, or resume an incomplete parent. Historical handoffs remain evidence, not authority to spend more time.

## Priority and next work

Measured HPM simulation speed is the primary project goal. Preserve correctness, frozen multiplayer remedies and the other requested features; do not invent shortcuts to obtain a faster number. Work must either produce a speed measurement, implement a measured improvement, or satisfy the smallest demonstrated safety/correctness dependency of that improvement. Record which before starting. General compatibility completeness, renderer work, cleanup and speculative native research are not performance deliverables.

The next eligible execution packet is [P00](sessions/P00.md): an **unmodified original-game HPM throughput baseline**, using the already accepted S01HPM environment. P00 is an explicit independent insertion before further S02 work, not permission to run an unverified fork. The full S03–S30 dependency gates remain unless a later written schedule revision identifies an actually independent task. This plan update does not start P00 or resume game control.

S02 and its R02 descendants are **BUDGET EXHAUSTED / INCOMPLETE**. No R02x, renamed D3D packet, fresh manager, reserve activation or new chat may restart the same objective. Additional S02 time requires a direct human instruction specifying the objective and additional minutes. Earlier blanket permission to continue the plan is not an extension grant. The present instruction authorizes this plan repair, not renewed S02 investigation.

## One clock for the entire objective

- Each top-level session has one durable family ID, original UTC start, authorized minutes and absolute deadline. Its listed estimate is a hard cap, at most 120 minutes unless the human explicitly grants an extension.
- Every child, reserve, retry, agent, review, context continuation and renamed packet inherits that family deadline. Workers run concurrently; their CPU minutes are not added together, but their waiting and root coordination do consume the same elapsed clock.
- Include setup, discovery, failed runs, build, tests, independent review, documentation, commit and delivery. No silent pause, reset, excluded integration time or new clock after a handoff. A deadline that passes while idle stays expired on resume.
- Before starting, record the family, current packet, original start, deadline, remaining minutes, finite acceptance checks, exclusions, ownership and intended speed/safety result in status. Root owns the clock and checks it before delegation and each tool/work phase; managers cannot grant themselves time.
- Use `python tools/planning/budget_guard.py --ledger <private-ledger.json> start --family <ID> --minutes <cap>` once. Then use `... check --family <ID> --packet <child-or-ID> --operation-minutes <bounded-estimate>` before each phase and immediately after each long call. Existing families cannot be restarted. Keep the ledger across chats and mirror its deadline in status/handoffs. An absent or inconsistent ledger on resume means stop and reconcile existing timestamps, not start over.
- The guard checks admission and returns nonzero when work must stop. It does not kill processes or make an agent incapable of ignoring instructions. Root must enforce its result; an automated check is not a claim of physical prevention.

## Mandatory decisions and cutoff

For a 120-minute family, assess at 30, 60 and 90 elapsed minutes; for shorter sessions assess at the corresponding quarter points. At each assessment report the actual artifact/result, remaining time, unresolved required checks and predicted completion cost. “Another investigation finished” is not progress toward a feature or measured speed result by itself.

At 30 minutes, reject an objective that cannot plausibly fit. At 60 minutes, choose the smallest safe deliverable or record a blocker. At 90 minutes, freeze scope: no new hook family, dependency, adapter design or experiment. These decisions may happen earlier when evidence is decisive.

The final 15 minutes are **wrap-up only**. Stop implementation and new experiments; finish bounded required checks, rollback/cleanup, record the result and make the scoped commit. Do not launch a check or run that cannot finish and clean up before the deadline. With a shorter remaining window, checkpoint earlier. At the deadline, stop optional work and report BUDGET EXHAUSTED / INCOMPLETE. Only necessary safe termination/rollback may continue; record any overrun honestly. Do not abandon a live process or corrupt user files just to claim compliance.

If blocked, report: what passed, the precise blocking dependency, evidence, time spent, the smallest safe alternative, expected additional cost and impact on the speed goal. Independent work is allowed only if explicitly present in the revised schedule with its own distinct deliverable. Never fill time with a dependent session or silently mark a required feature skipped.

## Bounded research and evidence

Before discovery, write one question, one discriminating experiment, its maximum runtime, and outcomes that decide proceed/reject/block. Allow at most two attempts on the same hypothesis; a second attempt needs an identified changed condition. An unchanged blocker ends the lane immediately. A different name for the same uncertainty is not a new hypothesis. New independent questions require remaining budget and a scope review, not an automatic research chain.

Unsupported optional behavior stays explicitly unavailable; do not repair every historical hook to unlock an unrelated task. Required frozen behavior cannot be silently disabled to make a lean profile launch. A modified build needs proof for every active dependency, but an untouched native measurement does not require proof of inactive fork hooks. Native calls, ABI/lifetime boundaries and installation phases still require real evidence; speed priority never weakens those rules.

Reuse accepted artifacts when the exact inputs and relevant dependencies are unchanged. Repeat only checks affected by a change or an unresolved concern. Keep one concise decision/result and one focused immutable index per session. No evidence-count targets, broad rehashes of unrelated trees, duplicate worker reviews or repeated source audits merely to fill a packet. Root reviews decisive raw evidence, not just an agent's summary.

## Measurement before optimization or feature expansion

P00 establishes original HPM throughput first; one correctly bounded run can validate the procedure, but only five valid repetitions support its median/spread and threshold comparison. If fewer fit, report the samples and incomplete status; never extrapolate success. Report vanilla separately when obtained. Do not hold HPM results for supplementary late-game fixtures or optional tools.

Use **UI speed 5**, a fresh process with the pinned 1836 setup, all of 1836 simulated normally and untimed in that same process, then exactly **1837-01-01 through 1838-01-01**. Fix/report autosave, visible view, render settings and job load. No profilers, builds or competing workers during timing. Diagnostic runs remain separate. The <12-second primary and <30-second fallback targets and fidelity requirements are unchanged.

After measurements, rank candidates by measured cost fraction, plausible absolute seconds saved, preservation risk and implementation-plus-validation cost. A research proposal without such evidence must identify the specific measurement it will obtain within its current cap. Do not spend a session proving a negligible or unmeasured subsystem merely because its internals are accessible. Complete the performance checkpoint before starting the army/transport feature phase. Stop optional optimization after the primary target passes.

## Handoff and acceptance fields

Every session handoff must state: family ID; original start/deadline; all child IDs; authorization reference; total elapsed/remaining minutes; required exits PASSED/FAILED/BLOCKED/NOT RUN; measured performance or NOT MEASURED; exact changed inputs and decisive evidence; safe process/file state; commit and delivery state; next eligible action. Record over-budget time rather than hiding it. A checkpoint may be committed with an INCOMPLETE label; a commit never certifies acceptance.

Documentation writes must use explicit UTF-8, validate the complete prospective contents, retain exact pre-edit bytes, and atomically replace the file only after encoding succeeds. Stage only task changes; preserve unrelated user edits. A broken evidence index or status file is a blocker to claims of a complete handoff, not permission to reconstruct silently from a stale committed version.

## S02 incident and recovery state

S02 began 2026-10-17 20:09:43 EDT. The sequence through R02w occupied more than ten wall-clock hours; this is an actual cumulative overrun, not a series of compliant fresh budgets. The failure was uncontrolled dependency growth plus automatic reserve renewal, amplified by repeated verification/reporting. Native uncertainty justified a blocked result, not unlimited repair work.

S01 and S01HPM are accepted. Through R02v, bounded results were committed; R02w source/tests are an **uncommitted incomplete checkpoint**. Real patched-provider hook inheritance and final compiled acceptance are unfinished; all fork startup profiles remain Rejected/WITHHELD. No speed result or requested army feature is claimed. Agents are stopped.

The prior checkpoint script truncated `planning/status.md`; it was restored to HEAD, which did not restore its prior uncommitted edits. Recovery records remain outside Git under `%LOCALAPPDATA%/V2DLLFork/R02w/root-review/`. Reconcile that missing working-copy content as a separately bounded documentation repair before claiming the historical status is fully recovered. This update records current truth without claiming that repair happened.
