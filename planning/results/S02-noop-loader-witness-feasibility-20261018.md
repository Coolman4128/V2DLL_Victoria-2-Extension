# S02 no-op loader witness feasibility — 2026-10-18

## Question and result

Can a no-op diagnostic DLL loaded through the executable's existing Lua import path observe process attach and known D3D route order without running fork installers?

**Result: feasible for one bounded, candidate-free diagnostic capture, subject to identity and restoration preflight.** The executable statically imports `lua51.dll`; the production StartupGate requires static attach and the executable-adjacent path. The existing proxy exports forward 128 named/ordinal entries to `lua51_real.dll`. A diagnostic proxy can occupy that same role while omitting `Install()`, and record its process attach plus the known factory/CreateDevice callsites.

This would establish only observed event ordering and route hits in that run. It would not prove complete route coverage, pre-entry/TLS absence, quiescence during production installation, production callback safety, `HookPresent` closure, or M3 teardown/lifetime. Therefore S02 remains incomplete and all production profiles remain Rejected/WITHHELD.

## Evidence and constraints

- Source checked: `Source/V2/V2/V2DLL_ModLoader.cpp`, `V2ModAPI.h`, `V2Mod_Template.cpp`, `Fork/StartupGate.cpp`, and `lua51_exports.h`.
- Accepted evidence checked: `planning/results/R02-install-phase.md` and the existing compiled export audit. The optional `v2dll_mod.dll` path runs after built-in installation, so it cannot witness the primary attach; the static Lua import is the relevant loader role.
- Before any capture, pin and verify the exact EXE, proxy and original Lua identities; verify the complete export table/forwarding; prepare exact backup and restoration of the exe-adjacent Lua files; use isolated staging where possible; make DllMain capture bounded and free of loader-lock I/O; and honor the existing R02u D3D dwell/read-stop/hard caps and cleanup protocol.
- Do not launch a fork candidate or call fork installers. Do not interpret a missed route as proof of absence.
- If exact file identity, safe backup/restore, or bounded capture cannot be established, stop before launch and report the specific blocker.

## Session record

Estimate: 20 minutes. Actual: 4.070 minutes, based on the persistent session clock. Its one independent whole-session timing review found that reused source and accepted evidence made this source-only feasibility decision fast, and calibrated similar checks to 8–10 minutes. It did not add work or change scope; the separately estimated 45-minute live capture remained unchanged.

## Next bounded session

Estimate 45 minutes: perform preflight and, only if every identity/restoration/cap check passes, one no-op Lua proxy capture of attach time and the known D3D route events. Exit with exact hashes, event order/hits, restoration audit and cleanup, or BLOCKED before launch with the failed precondition. No candidate, production patch, broad route scan, profile launch, build-policy change, or expansion into callback/lifetime proof.
