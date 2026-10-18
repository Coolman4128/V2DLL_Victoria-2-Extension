# Discovery gates, estimates and scope control

These are engineering uncertainty records, not reasons to stop useful independent work. Estimates are elapsed session budgets with parallel lanes, including integration/testing; they are not guaranteed finish dates for undocumented engine behavior.

| Risk | Evidence | Gate / response |
| --- | --- | --- |
| Different executable variant | Local MD5 differs; four sampled hook sites match | S02 verifies retained/new sites and ABI. R01/R02 if mapping needed; no guessed offsets |
| Toolset mismatch | Project v145; installed VS2022 tools14.44 | S01 build via explicitly supported toolset; document deviations |
| No measured speed baseline | Repository has no reproducible seconds/year results | S07 profile, then use phase fractions to decide candidates |
| Original behavior conflicts with advertised “fixes” | Some AI/economy changes implement policies | S04 evidence decision; leave proven bug fixes, remove other changes |
| Shared diagnostics and stabilization | StateSavingsNormalize called through timed daily hook | S06 must preserve correction while eliminating observation overhead |
| Native order APIs absent | Existing ArmyMove hook is instrumentation | S14/S15 trace actual command creation, owner checks and receipts |
| Zero brigades violate native assumptions | Damage/wipe/reinforcement/POP cleanup use different paths | S19–S22 prove classification; no global destructor suppression |
| Persistent IDs / transferred saves | Mod loader exposes no save API | S16/S17 decide metadata path, safe recovery; no sidecar-only guarantees |
| Exact EU4 parity unobserved | Localized prompt states known, runtime algorithms unknown | S19 observe controlled reference cases; S20–S24 implement and compare |
| GHM runtime dependency / own bugs | Archive installed; no launch test yet | S01 smoke; isolate native GHM failure from fork regression |
| Final threshold beyond patchable bottleneck | <12 s requires average <32.9 ms/day | S13/S28 quantify gap; S29/R06/R07 only useful candidates; don't skip work |
| Growing feature scope | AI automation, auto-replacement and new MP work tempting | Excluded unless user explicitly expands scope |

## Cumulative stop rule replaces renewable continuations

[Execution control](execution-control.md) overrides the former reserve policy. One objective has one hard cumulative cap. R01–R08 and uniquely named descendants consume parent time; they add no automatic hours. Do not create a fresh continuation when a deadline expires. Stop, preserve a concise incomplete checkpoint, and report the precise blocker, cost and speed impact. Only a direct human grant of named additional minutes permits more time.

The S02 incident exceeded ten wall-clock hours through automatic continuation and dependency growth. S02/R02 is exhausted. A new manager, packet name or chat cannot reopen it. Mandatory 30/60/90-minute decisions, final-15-minute scope freeze and admission checks apply to the entire family. Cap discovery to one question and at most two discriminating attempts; unchanged blockers stop immediately.

There is no automatic 16-hour reserve allowance. Earlier schedule totals are estimates, not renewed authority. P00 measures untouched native HPM independently, so unproved inactive fork hooks cannot hold the first speed baseline hostage. Modified builds still require all active native dependencies proved.

## Efficient order

Obtain the untouched native HPM baseline in P00 first, reusing the accepted build/environment. Establish the minimum safe active modified profile and canonical settings before candidate timing; do not expand into universal compatibility certification. Reject expensive optimizations whose theoretical maximum gain cannot close the observed gap. Keep one shared command/identity/persistence layer for both automation features. Develop pure planners/test fixtures in parallel with bounded native discovery, then integrate once. Defer broad refactors, new network features, custom renderer and 64-bit conversion. Stop performance experiments after measured primary success and concentrate on remaining feature acceptance.

## Decisions that may need user input during execution

Most defaults are specified in requirements.md and can be implemented without further questions. Ask only when evidence creates a material conflict: an unavoidable native forced deletion violating ZERO-1; a save format that cannot support the promised workflow; an observed EU4 behavior that conflicts with the user's explicit batching/recombine requirement; or a necessary unsupported executable dependency. Present the evidence and concrete alternatives. Elapsed time is not approval to relax a requirement.
