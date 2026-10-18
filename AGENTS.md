# Fork implementation rules

## Controlling priority and timing/review rule (2026-10-18)

**Measured HPM speed is the primary project goal. Read `planning/execution-control.md` before any execution.** Time estimates guide scope and trigger reviews; they are not hard deadlines. Track task and session elapsed time persistently. Each whole session gets at most one independent timing review, regardless of its slices or checkpoints: pause once after two hours and resume the same objective, or review at closeout if it finished under half its estimate. Do not review individual slices. The review only calibrates future estimates; it never adds work, changes or recommends scope, or delays resumption. At 2.5 times its estimate, reduce the session scope and move to a smaller deliverable while continuing the project; this is a separate execution rule. Keep native safety and packet acceptance requirements intact.

P00 was skipped by user direction and HPM throughput remains NOT MEASURED. S02/R02 is incomplete and resumes under the updated timing/review process; prior elapsed time and overruns remain historical data, not a work prohibition. S03 remains gated on genuine S02 exits. No unverified fork launch or native safety relaxation is authorized.

Read `planning/README.md`, `planning/requirements.md`, `planning/architecture.md`, and the current packet in `planning/sessions/` before implementation. Read the relevant research document rather than redoing completed discovery. The planning snapshot is 2026-10-17; current source remains the authority after edits.

## Scope

- Multiplayer stabilization is **DONE — awaiting user testing**. Preserve existing stabilization behavior, including FPU controls, state-savings normalization, POP quantization and their agreed effective settings. Do not add a networking overhaul, OOS investigation, checksum protocol, or stabilization workstream. Correct regressions introduced by this fork; report pre-existing issues without expanding scope.
- Vanilla is the engine-behavior baseline. Historical Project Mod (HPM) 0.4.6.3 is the main mod/performance workload. Preserve HPM's own content choices. Remove unrelated V2DLL balance changes according to `planning/research/patch-policy.md`.
- Templates and automatic transport are human-player automation. Zero-strength preservation is a simulation rule applied consistently to affected land brigades on every machine, with the exact scope and exceptions in `planning/requirements.md`.
- No new gameplay shortcuts to meet performance targets: no skipped days, reduced POPs/events/AI, changed economic formulas, free recruitment, teleportation, extra transport capacity, or altered RNG/rounding.

## Session execution

1. Run packets serially in the revised README order and honor user-directed skips. P00 is skipped by direction; its HPM measurement remains unaccepted. A packet is complete only when its exit checks pass; partial research is not a completed feature. Conditional packets can be marked SKIPPED with their stated reason.
2. The user explicitly authorizes parallel agents within a packet. Use the named independent lanes, at most three workers plus the integrator. Do not start a dependent packet concurrently. One agent owns each file; only the integrator edits `V2TechButton.cpp`, shared headers, project files, settings and install order unless ownership is explicitly reassigned.

   Count the root orchestrator against the global agent limit. With four active slots, root + session manager permits two additional workers; the root may own a third independent lane, or the manager must reuse workers sequentially and split/re-estimate when necessary. Only one session manager's packet may be active. See `planning/orchestrator-prompt.md` for execution and review rules.
3. Prefer new focused modules over a monolith rewrite. Workers submit patches or implement owned new files; the integrator merges and tests once. Parallel agents never control the same live game process or debugger.
4. Use `tools/planning/session_clock.py` to record estimates, elapsed time and at most one timing review per session. It is an advisory clock and must never block work solely because time passed. After two hours, pause once for review and resume the same objective; at 2.5 times the session estimate reduce scope as a separate execution decision. Record retrospectives and planned scope at closeout. Safe cleanup and honest reporting remain mandatory.
5. Update `planning/status.md` and create `planning/handoffs/SNN.md` after each packet: commit/worktree state, changed paths, checks and exact results, artifact/save hashes, decisions, remaining unknowns, and the next packet. Keep private game files/logs outside Git.
6. Implement authorized reversible work without repeatedly asking permission. Ask only for missing decisions that materially block correctness or actions beyond authorization. Do not contact mod authors, publish, or push releases without user authorization.

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
- Record game/mod/build/config/save identities with results. Separate diagnostic runs from timing runs. The target sample is **1837-01-01 through 1838-01-01**, after simulating all of 1836 normally as untimed warm-up in the same process. Report exact boundaries, median/spread, autosave status, visible/render settings and job load.
- The goal is <12 seconds for that 1837 year at speed 5 on the user's Ryzen 7 9700X; fallback <30 seconds. HPM is the primary workload; report vanilla separately. Mid/late-game measurements are supplementary regression checks, not additional absolute-speed targets or prerequisites for target acceptance. Do not promise or mark success without the required 1837 measurements.
- Test meaningful contracts and regressions. Pure planners/state machines can use synthetic tests; native ABI and save lifecycle require actual-game evidence. Mocks do not certify those boundaries.
- Verify no unrelated behavior patches activate via compiled defaults, generated/shipped INI, legacy aliases, mod-local overrides or hidden installers. Preserve proven bug fixes. Ambiguous fixes get a documented evidence decision before release.
- Stop optional optimization when the primary target is met. If a candidate cannot plausibly close the measured gap or cannot preserve semantics, reject it and keep the best verified build.

## Workspace and delivery

- Preserve user edits. Use isolated build/staging/test folders and back up exact DLL/config files before deploying a test build. Do not modify the Steam executable or distribute proprietary executables/DLLs as part of the fork.
- HPM download/installation is authorized by the user; pin and verify the official release as documented in `planning/research/environment.md`. Preserve the existing GHM installation. Keep its downloaded archive and generated caches outside this repository.
- Deliver source, build instructions, optional mod assets, rollback instructions, measured results and a known-limitations list. Package only verified dependencies; do not copy old committed build outputs and call them current.
