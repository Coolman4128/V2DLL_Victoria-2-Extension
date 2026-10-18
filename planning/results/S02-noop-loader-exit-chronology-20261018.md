# S02 launch exit chronology correction — 2026-10-18

## Result

Session `S02-20261018-launch-exit-chronology-correction`: 10-minute estimate, 0.654 minutes actual, S02 family wall span 1,238.919 minutes. Its single whole-session review found this was quick because the helper source, exit report and ledger timestamps were already located. Comparable read-only chronology corrections with exact artifacts ready calibrate to 1–3 minutes; this does not apply to native diagnosis. The review added no work and changed no scope.

Read-only correction to the S02 no-op loader capture record. The private helper source `suspended_launcher_v3.py` waits on its retained process handle after the one-time resume. It writes `.exited.json` only after `WaitForSingleObject` returns `WAIT_OBJECT_0` and then successfully reads the exit code. In `finally`, it calls `TerminateProcess(...,199)` only if a zero-time wait shows the process is still unsignaled.

`launch2.exited.json` records PID 42772 exit status `0xFFFFFFFF` at 20:38:13.746688Z. The D3D JSONL ledger's `after-delete` event is later, 20:38:31.121938Z, and says that PID was force-terminated after GDB interruption failed. This is a contradiction in the recorded mechanism/order. The `after-delete` timestamp identifies when the ledger event was written; it does not give the time of a `TerminateProcess` call. The launcher proves the process handle was signaled before its exit report, but does not prove whether exit was natural or externally forced. The exit code does not explain why. Preserve the raw append-only ledger unchanged; describe the cause and exact termination chronology as unresolved.

No native process or D3D state was accessed in this correction. No code, game files or settings changed. R02u's D3D dwell remains 502.4676403 seconds against a 480-second read-stop; no further reads/resumes or cap reset are allowed. S02 remains incomplete, profiles Rejected/WITHHELD, S03 gated, and HPM throughput NOT MEASURED.

## Next scope

No further work is authorized by this correction. Continue the orchestrator only if an already-supported, safety-compliant S02 evidence path becomes available without additional D3D reads/resumes. Otherwise retain the S02 blocker; do not start S03.
