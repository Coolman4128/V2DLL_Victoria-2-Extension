# S02 frozen native boundaries — static evidence

Observed 2026-10-17. Scope: compatibility evidence for three existing frozen hooks only. No hook, stabilization setting, executable, running process, or game state was changed. This report does not certify the whole DLL or authorize using these hooks as new command phases.

## Identity and reproducibility

- Local `v2game.exe`: SHA-256 `62D48C204364DD706584777C2E2B3C7AB3C5F1DD0170872554943575D53D6648`, MD5 `63C5485FF00353F2BE8DAAA3F0DCF957`; machine `0x014C`, preferred image base `0x400000`, SizeOfImage `0x1092000`.
- Read-only probe: `tools/verify/frozen_boundary_probe.py`, SHA-256 `F3066EBCF265CAE1A59BA33A29B03ABC8A18E6D35EFDBD0D54F9429FC7B50826`; Python with Capstone `5.0.7` and pefile `2024.8.26`.
- Private decoded evidence: `%LOCALAPPDATA%/V2DLLFork/S02/frozen-boundaries/probe.json`, SHA-256 `EBB81EDF85222B4A1F45B4C68A29097CC4BE84B2137368C8DB601F8627A1E8CE`. No executable bytes/dumps were added to Git beyond the small source signatures and instruction facts already needed for patch review.
- Source snapshot inspected: `Source/V2/V2/V2TechButton.cpp`, SHA-256 `C0E8CA75E07DBE1400A0E5F3359CAD12E0821F31769F74DDFED0DCE673CF4F21`. This is the observed checkpoint, not a guarantee that the integrator's eventual source hash is unchanged.

Reproduce in PowerShell, keeping output outside Git:

```powershell
$env:PYTHONPATH = Join-Path $env:LOCALAPPDATA 'V2DLLFork/S02/python'
python tools/verify/frozen_boundary_probe.py --exe 'C:/Program Files (x86)/Steam/steamapps/common/Victoria 2/v2game.exe' --output (Join-Path $env:LOCALAPPDATA 'V2DLLFork/S02/frozen-boundaries/probe.json')
```

Direct CFG traversal decodes branches without entering callees; it does not resolve indirect destinations or prove exception/unwind paths. Each known caller address was independently reachable from the reference-metadata entry in local bytes: POP caller `0x84060` → `0x8468F` (877 decoded instructions), daily caller `0x32ECF0` → `0x32ECFA` (6), main caller `0x5796B0` → `0x57E0C8` (4,678). These traversals had no unresolved direct-CFG branches, outside-range branches, or exhausted budgets. This does not prove the listed caller is the only possible caller.

## Source signatures, stolen spans and resumes

All addresses below are RVAs; add the actual loaded image base for runtime addresses.

| Hook | Full source check | Stolen instruction lengths | Exact resume instruction | Local result |
| --- | --- | --- | --- | --- |
| POP daily `0x85E40` | `POP_DAILY_SIG`: `55 8B EC 64 A1 00 00 00 00` | `1+2+6`: `push ebp`; `mov ebp,esp`; `mov eax,fs:[0]` | `0x85E49`: `push -1` | Signature and whole-instruction extent match |
| Daily/savings entry `0x282EC0` | Installer prefix `55 8B EC 6A FF`; also all 11 `OOS_POST_SEH` bytes at `0x282ED8`: `81 EC 40 01 00 00 53 56 8B 75 08` | `1+2+2`: `push ebp`; `mov ebp,esp`; `push -1` | `0x282EC5`: push SEH handler address | Both source checks match; whole-instruction extent matches |
| Main loop `0x5DF550` | `MAIN_LOOP_SIG`: `55 8B EC 6A FF` | `1+2+2`: same three instructions | `0x5DF555`: push SEH handler address | Signature and whole-instruction extent match |

There are zero base-relocation entries starting in any stolen span, zero stolen relative branches/calls, and no split instruction. POP's `fs:[0]` access is the thread exception-chain slot, not a PC-relative reference. Daily and POP copy only these spans into their existing trampolines; main-loop thunk reproduces its three instructions inline. SEH handler immediates lie at the native resume locations and are not copied into these stolen spans. This is narrow relocation evidence, not a proof of every other trampoline in the DLL.

## POP stack and captured registers

The local caller at `0x8466B` loads two stack words into ECX/EDX, reserves eight bytes, stores those words there, derives a boolean using `setge al; movzx ecx,al`, then pushes ECX, ESI, EBX, EDI. Its call at `0x8468F` targets `0x85E40`. Thus six stack words (24 bytes) are supplied; ECX at this call is a **0/1 boolean**, not an established object pointer. The first stack argument is EDI. The reference decompiler's four-parameter declaration is not a reliable stack-size contract: its file explicitly warns that stack tracking failed.

Local direct-CFG decoding finds a normal epilogue at `0x873F5` restoring ESP, the SEH chain, EDI/ESI/EBX/EBP, and `ret 0x18` at `0x8740B`. This agrees with the local caller's 24 bytes. Source `PopDailyEntryThunk` saves ECX and `[esp+4]`, replaces only the return address, and jumps through the nine-byte trampoline. If entry ESP is S, native `ret 0x18` reaches `PopDailyAfterThunk` with ESP S+28, exactly as ordinary return would reach the caller. Its `pushad`/`popad` and final jump preserve that cleanup. The entry thunk's EAX load does not destroy an incoming EAX requirement in the stolen sequence: the native `mov eax,fs:[0]` overwrites EAX before its native use.

`PopDailyAfterThunk` preserves general registers around `AfterPopDaily`; it does not save EFLAGS, x87 data registers, or XMM state. `AfterPopDaily` intentionally calls frozen `PinFpu`. Native POP's first condition codes are produced by native arithmetic before conditional use; the known caller does not consume returned flags before another call. This does not establish a general nonstandard floating-point return/live-register contract across every possible caller.

The source's `g_popDailyRet`, `g_popDailyArg0`, and `g_popDailyEcx` are process-global scratch slots. Static instruction boundaries do **not** prove that nested/concurrent invocations cannot overwrite them. The local caller's boolean ECX and the reference callee's first-argument member accesses (`+0x804`/`+0x808`) also do not establish that either captured value satisfies the source quantizer's CPop/vector/list heuristics. Object interpretation, complete object lifetime, and effective traversal remain **UNPROVED**, not newly diagnosed stabilization defects. Frozen code was left intact.

## Daily/savings wrapper stack

Local `0x32ECF0` loads the global session pointer into EAX, adds `0x3C` to ECX, pushes ECX and EAX, calls `0x282EC0` at `0x32ECFA`, then immediately returns. The callee loads its first argument from `[ebp+8]` into ESI at `0x282EE0`; the local normal epilogue has `ret 8` at `0x283CCE`. This directly supports two stack arguments with callee cleanup. The known caller supplies argument 0 from the session global; the entry does not require an undocumented ESI object argument.

Source `HookOosReport` saves incoming ECX/ESI/EDI/EBX across `AccChkEnter` and `DiagNow`, restores them before the original trampoline, forwards the same two original arguments, and relies on native `ret 8` to remove the forwarded pair. It saves native EAX across `AccChk`; its own `ret 8` removes the original pair. Explicit source stack accounting balances: an incoming ESP S returns to the original caller with S+12, as ordinary `ret 8` would. EBP and normal x86 nonvolatile registers are preserved subject to the compiled helpers honoring their declared calling conventions. Incoming EAX is overwritten by the native SEH prologue; ECX is explicitly restored before original entry. This is source-level wrapper analysis plus native caller/epilogue proof, not inspection of every generated helper instruction.

`AccChkEnter` calls `StateSavingsNormalize(a0)` independently of diagnostic logging before the original native body. Source normalization traverses the session-country/state fields (`session+0xADC`, country `+0xE44`, state savings `+0x228`, country money `+0xE78`). The native argument origin above supports the session argument identity; it does not by itself prove these field layouts, lifetime, safe mutation phase, or concurrency exclusion. No new OOS behavior, protocol, or investigation was performed.

## Main-loop entry

The local entry reads `[edi+0xD8]` at `0x5DF56B` and later repeatedly uses EDI as an object base. This agrees with the reference decompiler's `unaff_EDI` rather than an assumed ECX/thiscall contract. The known caller at `0x57E0C8` targets the entry with no immediately pushed arguments. Local direct-CFG traversal covers 350 instructions, finds no normal `ret`, and ends at a call followed by `int3` (`0x5DF9FB`/`0x5DFA00`); the reference describes `_exit(0)` at that point. A normal return convention is therefore not claimed.

Source `MainLoopThunk` preserves general registers, including required EDI, around `PinFpu` and `TryPatchLateModules`; then exactly reproduces the three stolen instructions and resumes at `0x5DF555`. It does not preserve EFLAGS/x87/XMM as complete machine state. The native prologue executes `sub esp,0x14` before any entry-condition-code consumer, so changed incoming flags are not used along this entry sequence. `PinFpu` intentionally changes control modes. This hook is at the outer function entry, not evidence of a per-tick safe command-submission point or permission to send native commands there.

## Decision and remaining gates

**PASS, narrow static compatibility:** all complete installer signatures examined, all three stolen instruction extents, absence of stolen relative control flow/relocation starts, exact resume boundaries, known local caller targets, POP 24-byte cleanup, daily 8-byte cleanup, and main-loop hidden EDI dependence.

**UNPROVED:** runtime thread identity/exclusion and reentrancy; object lifetimes/layouts beyond the observed accesses; compiled helper FPU/XMM/live-register behavior; native exception/unwind behavior with replaced returns/wrappers; installation atomicity and safe installation phase; the complete set of possible indirect callers. Existing installers' protection/flush handling is not made transactional by this static report.

Keep the frozen behavior unchanged. Carry these unknowns into the integrator's S02/R02 decision; do not turn matching entry bytes into global ABI compatibility or feature completion. No game launch, debugger attachment, build, or live process inspection was performed in this proof lane.
