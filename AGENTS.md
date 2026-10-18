# Fork implementation rules

## Controlling priority and budget rule (2026-10-18)

**Measured HPM speed is the primary project goal. Read `planning/execution-control.md` before any execution. It overrides older automatic reserve/continuation instructions in every packet and handoff.** One top-level objective has one cumulative cap (at most 120 minutes), including all descendants, workers, tests, root review, handoff and Git. No agent, renamed packet or fresh chat can reset it. Check the persistent budget guard before every work phase. Assess at 30/60/90 minutes for a 120-minute cap; final 15 minutes are wrap-up only. At exhaustion stop and report incomplete; additional time requires explicit human authorization for a named objective and additional minutes. Blanket permission to continue is not an extension.

S02/R02 is already BUDGET EXHAUSTED / INCOMPLETE; no further native investigation is active. The revised independent next packet is P00, unmodified original-game HPM speed measurement. It does not permit an unverified fork launch or relax native safety. This plan revision does not itself start execution.

Read `planning/README.md`, `planning/requirements.md`, `planning/architecture.md`, and the current packet in `planning/sessions/` before implementation. Read the relevant research document rather than redoing completed discovery. The planning snapshot is 2026-10-17; current source remains the authority after edits.

## Scope

- Multiplayer stabilization is **DONE — awaiting user testing**. Preserve existing stabilization behavior, including FPU controls, state-savings normalization, POP quantization and their agreed effective settings. Do not add a networking overhaul, OOS investigation, checksum protocol, or stabilization workstream. Correct regressions introduced by this fork; report pre-existing issues without expanding scope.
- Vanilla is the engine-behavior baseline. Greater History Mod (GHM, explicitly not GFM) 0.6 is the main mod/performance workload. Preserve GHM's own content choices. Remove unrelated V2DLL balance changes according to `planning/research/patch-policy.md`.
- Templates and automatic transport are human-player automation. Zero-strength preservation is a simulation rule applied consistently to affected land brigades on every machine, with the exact scope and exceptions in `planning/requirements.md`.
- No new gameplay shortcuts to meet performance targets: no skipped days, reduced POPs/events/AI, changed economic formulas, free recruitment, teleportation, extra transport capacity, or altered RNG/rounding.

## Session execution

1. Run packets serially in the revised README order, including the explicit independent P00 insertion. A packet is complete only when its exit checks pass; partial research is not a completed feature. Conditional packets can be marked SKIPPED with their stated reason.
2. The user explicitly authorizes parallel agents within a packet. Use the named independent lanes, at most three workers plus the integrator. Do not start a dependent packet concurrently. One agent owns each file; only the integrator edits `V2TechButton.cpp`, shared headers, project files, settings and install order unless ownership is explicitly reassigned.
3. Prefer new focused modules over a monolith rewrite. Workers submit patches or implement owned new files; the integrator merges and tests once. Parallel agents never control the same live game process or debugger.
4. Enforce the cumulative family cap in `planning/execution-control.md`; reserves consume remaining parent time and never add time. Use `tools/planning/budget_guard.py` and preserve its ledger across resumes. Stop implementation with 15 minutes left; deadline exhaustion requires an honest checkpoint, not another continuation. Safe cleanup is mandatory and any overrun is recorded.
5. Update `planning/status.md` and create `planning/handoffs/SNN.md` after each packet: commit/worktree state, changed paths, checks and exact results, artifact/save hashes, decisions, remaining unknowns, and the next packet. Keep private game files/logs outside Git.
6. Implement authorized reversible work without repeatedly asking permission. Ask only for missing decisions that materially block correctness, actions beyond authorization, or an explicit extension of an exhausted cumulative budget. Do not contact mod authors, publish, or push releases without user authorization.

## Engine work

- Prove executable identity, instruction boundaries, byte signatures, calling conventions, register/FPU preservation, object lifetime and safe phase before calling or patching native code. The local executable differs in MD5 from the research reference; four matching sites do not establish global compatibility.
- New adapters fail closed on unknown executables or missing signatures and report which feature is unavailable. Validate all dependent sites before installation, and restore prior writes if installing the feature fails. Never reuse a UI timing hook as a command sender without tracing it.
- Use normal game command submission for recruitment, move, split, merge, embark and disembark. A helper may construct and enqueue a vanilla command; do not directly rewrite world lists to simulate those commands. Each local player's automation is the sole sender of their jobs; remote peers apply received commands through the game.
- Never hold native pointers across a tick/save/load. Use verified stable native IDs plus campaign/owner/generation checks; refresh through resolvers. Revalidate before issuing a command. Jobs must be idempotent and cancellation must not delete units or duplicate queued orders.
- Keep engine calls on their verified thread/phase. Background workers may operate on immutable snapshots for proven-pure work. UI prompts must not block the simulation thread or trigger a new global MP pause.
- Zero-strength brigades must stay exactly zero, remain associated with their stack and eligible for ordinary reinforcement, and contribute no combat/siege value. Never clamp to one man, suppress all destructors, or modify positive-strength damage/deployment rules.
- Respect save/load and full-stack wipe semantics. Do not invent reinforcement manpower when a soldier POP disappears. Missing/corrupt automation metadata must fail safely without inventing ownership or replaying stale orders.

## Validation and performance

- Follow `planning/acceptance.md` and `planning/research/performance.md`. Build Release Win32; do not claim x64 support. Keep `/fp:fast` and arbitrary parallel reductions out of simulation code.
- Record game/mod/build/config/save identities with results. Separate diagnostic runs from timing runs. Report each early/mid/late fixture, actual completed game dates, median/spread, autosave status, visible/render settings and job load.
- The goal is <12 seconds/year at speed 5 on the user's Ryzen 7 9700X; fallback <30 seconds/year. Do not promise or mark either achieved without the required measurements. Do not hide a slow late-game result behind an early-game average.
- Test meaningful contracts and regressions. Pure planners/state machines can use synthetic tests; native ABI and save lifecycle require actual-game evidence. Mocks do not certify those boundaries.
- Verify no unrelated behavior patches activate via compiled defaults, generated/shipped INI, legacy aliases, mod-local overrides or hidden installers. Preserve proven bug fixes. Ambiguous fixes get a documented evidence decision before release.
- Stop optional optimization when the primary target is met. If a candidate cannot plausibly close the measured gap or cannot preserve semantics, reject it and keep the best verified build.

## Workspace and delivery

- Preserve user edits. Use isolated build/staging/test folders and back up exact DLL/config files before deploying a test build. Do not modify the Steam executable or distribute proprietary executables/DLLs as part of the fork.
- GHM is already installed as documented in `planning/research/environment.md`; do not redownload or replace it without a reason. Keep its downloaded archive and generated caches outside this repository.
- Deliver source, build instructions, optional mod assets, rollback instructions, measured results and a known-limitations list. Package only verified dependencies; do not copy old committed build outputs and call them current.
