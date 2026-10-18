# S01HPM ? Corrected HPM workload baseline

Status: COMPLETE; all exit checks independently accepted by root. Start2026-10-17 22:44:53 EDT; final acceptance recorded22:58:55.953 EDT,14m02.953s at root acceptance; final manager handoff22:59:51.443 EDT,14m58.443s including final documentation, within the90-minute ceiling. User-directed workload prerequisite correction; R02/S02 remain incomplete and R02b is not active. See [results](../results/S01HPM.md) and [handoff](../handoffs/S01HPM.md). No60-minute assessment was due before completion.
Parent: S01. Execute after the active R02 packet/checkpoint; before HPM-dependent acceptance or timing. S01's historical GHM results remain unchanged.
Estimated elapsed: <=90 minutes including independent review and handoff. Re-estimate after 60 minutes and reserve final 15 minutes for validation/handoff; create a unique continuation if native startup exceeds the budget.

## Objective and prerequisites

Establish the corrected Historical Project Mod workload using official release 0.4.6.3, tag commit `6819d3214d6787247ce7558fa7376fc591cadc92`. The user explicitly authorized its download. Root may prepare the additive download/installation while recording this correction; only one session manager/live owner runs the native baseline after the active packet is checkpointed. Read environment.md, S01 evidence, requirements and acceptance first.

## Ownership and execution

Manager owns the installation/native launch manifest, live game process, preservation/rollback and final environment/status/handoff updates. Independent worker may verify archive paths, identities and installed hashes read-only. Root independently reviews evidence and owns scoped Git operations. Do not replace GHM, game binaries, existing DLLs, saves or caches. Keep downloaded assets and generated evidence outside Git. Use original Lua only; candidate launch remains gated by S02's complete active profile.

Record the official release URL, tag commit, archive SHA-256/size, archive path validation, exact HPM.mod bytes/hash, installed relative-path SHA-256 manifest and source/destination count comparison. Fail on pre-existing HPM destinations until preservation/reconciliation is established. Pin the actual installed content, not the descriptor's shortened version label alone.

Use the S01-proven game launch context with `-mod=mod/HPM.mod`. Verify an identifiable HPM native menu and a usable 1836 setup, or record exact blocking dependencies. Back up exact settings and inventory existing saves and binaries before launch; exit normally, restore settings, verify preservation, and keep HPM-generated user data separate. Do not infer an authoritative performance result from loading or menu checks.

## Exit evidence

Official pinned package installed additively with zero mismatched files; native HPM identity/setup independently reviewed; original binaries/settings and pre-existing saves preserved; exact commands, results, identities, elapsed time and remaining limitations in `planning/handoffs/S01HPM.md` and linked environment/status records. If blocked, preserve the concrete evidence and leave HPM acceptance open. No claim that historical GHM tests certify HPM.
