# S02 no-op loader capture — 2026-10-18

## Result

Session `S02-20261018-noop-loader-capture`, 45-minute estimate. The fixed-path, fixed-base x86 no-op `lua51.dll` proxy loaded through the executable's static import. A hardware breakpoint hit its `DllMain` entry at `0x10001000` (module base `0x10000000`) on the primary thread before game initialization. The proxy forwarded the same 128 names and ordinals to a byte-identical copy of the existing `lua5.1.dll`, ran no fork installer, imported no other DLLs, and its entry body was only `mov eax,1; ret 0xc`.

Four hardware breakpoints were then armed at the two previously mapped Direct3DCreate9 IAT callsites and two known slot-16 CreateDevice callsites using the fresh executable base `0x210000`:

| Route | Static RVA | Runtime address | Result |
| --- | ---: | ---: | --- |
| Direct3DCreate9 callsite A | `0x5946A2` | `0x7A46A2` | No hit during the bounded wait |
| Direct3DCreate9 callsite B | `0x5B81BC` | `0x7C81BC` | No hit during the bounded wait |
| CreateDevice callsite A | `0x5950E0` | `0x7A60E0` | No hit during the bounded wait |
| CreateDevice callsite B | `0x595269` | `0x7A5269` | No hit during the bounded wait |

No route absence or order conclusion follows from these misses. The process had only written the short HPM `system.log` system-settings header; `game.log`, `setup.log`, and `time.log` were empty. The observed run therefore did not establish that startup reached the mapped D3D routes. Computer Use app inventory returned no Windows apps, so a startup prompt or menu could not be observed or operated. The owned-child launcher report records a natural process exit at 2026-10-18T20:38:13.746688Z with exit code `0xFFFFFFFF`, about 142 seconds after its one resume; it did not hit the helper timeout. The cause is not established. GDB cleanup and the D3D family read-stop are separate observations; the process report does not support saying the game child was terminated after the read-stop.

## Timing and limits

The persistent clock records **13.687 minutes actual** against a 45-minute estimate and **1,230.927 minutes S02 family wall span**. Because it finished below half estimate, exactly one independent whole-session closeout timing review was completed. The reviewer found that accepted artifacts and a narrow one-capture objective kept the session short; the missed routes and incomplete startup are not work saved. Comparable one-shot bounded diagnostics with exact identities/artifacts ready should be estimated at 15–20 minutes, or about 25 minutes when launcher/debugger recovery and restoration are uncertain. This review calibrates timing only; it adds no work and changes no scope. This was one session with several slices and has no slice-level reviews.

The R02u D3D hardware-breakpoint ledger now records **502.4676403 seconds cumulative** versus its **480-second read-stop** and **600-second hard cap**. The read-stop was exceeded by 22.4676403 seconds during bounded breakpoint/process cleanup after GDB interruption failed; no further memory reads or breakpoint resumes occurred after the cleanup boundary. The hard cap was not exceeded. This family must not perform more D3D breakpoint reads or reset the ledger.

No product source, settings, saves, or game executable changed. Exact restoration returned all 929 manifest files to their pinned bytes; four generated HPM logs were preserved privately. The temporary `lua51_real.dll` was removed, original `lua51.dll` and `lua5.1.dll` hashes were restored, and the game and GDB processes are absent. Private diagnostic source, binaries, launcher reports, raw logs, and restoration evidence are under `%LOCALAPPDATA%\V2DLLFork\S02-noop-loader-witness`.

S02 remains **INCOMPLETE**. D3D route phase/quiescence remains **NOT PROVEN**; `HookPresent` callback closure and M3 other-thread teardown/lifetime remain **NOT PROVEN**. Profiles remain Rejected/WITHHELD; S03 stays gated. P00 remains skipped and HPM speed remains NOT MEASURED.

## Follow-up

The planned 20-minute source/artifact analysis completed as `S02-20261018-noop-loader-startup-analysis`; see its [result](S02-noop-loader-startup-analysis-20261018.md) and [handoff](../handoffs/S02-noop-loader-startup-analysis-20261018.md). It established an early child exit but not its cause, and found no artifact-supported safe action that closes the remaining native gates. The R02u 480-second D3D read-stop remains exhausted.
