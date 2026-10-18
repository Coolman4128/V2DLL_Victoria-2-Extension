# S02 no-op loader startup analysis — 2026-10-18

## Result

Session `S02-20261018-noop-loader-startup-analysis`: 20-minute estimate, 1.453 minutes actual, S02 family wall span 1,233.939 minutes. This source/artifact-only review used the private launch reports and generated logs, accepted D3D route map, and current startup-gate source. No game, debugger, D3D memory/breakpoint, product code, settings or profile actions occurred.

The owned-child report gives this sequence:

- `CreateProcessW` launched `v2game.exe` with `-mod=mod/HPM.mod` at 20:35:18.271433Z, suspended, PID 42772/TID 31688.
- The primary thread resumed exactly once at 20:35:51.568431Z; previous suspend count was 1.
- The helper records its retained process handle signaled at 20:38:13.746688Z and unsigned exit code `0xFFFFFFFF`, about 142 seconds after resume. Its bounded wait is 30 minutes, so its own timeout did not fire. The later D3D ledger `after-delete` note says the same PID was force-terminated after GDB interruption failed; it does not timestamp the termination call, and the reports do not reconcile exact mechanism/order. The exit cause is unknown. See [chronology correction](S02-noop-loader-exit-chronology-20261018.md).
- The fresh HPM `system.log` contains only system memory, pagefile, CPU, processor-count and OS information. `game.log`, `setup.log`, and `time.log` are empty. The four mapped D3D sites therefore were not reached in this run. The run ended before the mapped D3D sites; that does not prove those routes are absent.

A proxy-chain concern is plausible but unverified. The diagnostic proxy forwards exports through `lua51_real.dll`, which is a renamed copy of the captured 11,264-byte wrapper (SHA-256 `B2BA19343691F22D1AF1417F9A7CBAEAE74BA820A2A412F5A6CDEA271CC1A7B0`). Current source's startup gate requires its own module handle to be `lua51.dll` and the executable-adjacent path; a binary containing that gate would reject under the renamed role. However, the captured wrapper hash differs from the current repository build binaries, and there is no evidence tying its attach behavior to this exit. Do not report this as the cause.

## Timing review

Exactly one independent whole-session timing review completed. It found the short duration consistent with narrow evidence triage reusing exact, indexed artifacts and no new runtime setup. Incomplete native validation is not time saved. Comparable tightly scoped artifact explanations with evidence already at hand calibrate to 3–5 minutes; this does not transfer to capture or native validation. The review added no work and changed no scope.

## Disposition

No existing artifact supports a safe next step that closes the remaining S02 native route/phase or callback/lifetime gates. The R02u 480-second D3D read-stop remains exhausted; do not repeat the capture, read more D3D state, or infer route absence. Keep production profiles Rejected/WITHHELD and S03 gated. P00 remains skipped and HPM speed remains NOT MEASURED. S02 is incomplete.
