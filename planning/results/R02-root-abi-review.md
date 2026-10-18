# R02 independent root review: compiled ABI and floating-state obligations

Read-only review on 2026-10-17. Root did not access/control a game process, call an engine helper, or change production source. This supplements the manager's runtime samples and the PE/lifetime reports. It does not certify a supported production profile or object lifetime.

## Compiled fork wrappers

The earlier pre-backend Release Win32 checkpoint DLL SHA-256 is `C5D01DE9081A305EBECD1F06700FC249482533923C62654EE7E2C50229EA3F2E`. Root built an external Win32 DIA reader against the locally installed VS2022 DIA SDK, loaded that build's PDB, then independently disassembled the identified DLL spans with Capstone.

| Symbol | RVA / length | Observed normal-path contract |
| --- | --- | --- |
| Update0 | 0x5B00 / 30 | Preserve ESI; run OnViewUpdate; restore ECX=view, clear EDX, restore ESI and tail-jump original, or plain ret if absent |
| Tip1 | 0x5BA0 / 75 | Preserve EBP/ESI/EDI; original receives ECX=view, EDX=0 and stacked retBuf/element; retain result; restore registers; ret8 |
| Tip2 | 0x5C20 / 75 | Same stack/register shape as Tip1 |
| Tip3 | 0x5CA0 / 159 | Same shape; optional name comparison and result mutation precede restoration/ret8 |

DIA reports calling-convention value4 for these wrappers. The actual machine code is the evidence for register forwarding/cleanup, rather than source spelling alone. EBX is unused in these wrapper bodies. Both Tip1 and Tip2 still execute the element vslot+0x44 getter after the original when result/element are nonnull: optimization did not eliminate that dependency. Missing custom button names cannot remove this lifetime obligation.

No FP instructions occur in the four wrapper spans themselves. OnViewUpdate (RVA0x5910, length494) and SetupButtons (0x1F30, length578) use XMM0 for zeroing/data copies and restore normal-path nonvolatile general registers. Their directly decoded bodies contain no x87 or control-mode write. They call logging, native lookup, memcpy and security-cookie helpers; this is not a transitive floating-state proof. XMM0 contents are demonstrably not preserved by setup.

## Actual native caller liveness

Exact executable SHA-256: `62D48C204364DD706584777C2E2B3C7AB3C5F1DD0170872554943575D53D6648`. Root independently decoded its CFGs, cross-checking the worker's rooted call sites and manager's observed return addresses.

- Selected-view tooltip dispatcher entry0x26A040 supplies ECX=view and pushes element then output storage at call0x26A091. From return0x26A093 it moves preserved EDI into EAX, restores ESI/EBX/frame and returns ret4. There is no subsequent ECX/EDX/EFLAGS or x87/XMM result read in that continuation. Its own output is the caller-supplied object pointer, not an FP result.
- Technology's observed slot11 call0x3ACBC2 is reached from entry0x3ABDA0. At return0x3ACBC4 it loads ECX from its stack, restores EDI/ESI/FS exception chain/EBX/frame, and returns ret4. No incoming original EAX/EDX/flags or floating-register result is consumed by that continuation.

These exact continuations do not require preserving every volatile machine register bit across the replaced virtual call. They do require correct stack shape, the relevant nonvolatile registers, output storage/results, and ambient floating-control behavior. Caller end-of-function code alone does not prove every possible indirect caller, outer caller floating liveness, or a nonstandard hidden native contract.

Microsoft's x86 [argument-passing documentation](https://learn.microsoft.com/en-us/cpp/cpp/argument-passing-and-naming-conventions?view=msvc-170) identifies ESI/EDI/EBX/EBP preservation. The [fastcall documentation](https://learn.microsoft.com/en-us/cpp/cpp/fastcall?view=msvc-170) describes ECX/EDX argument registers and callee stack cleanup; [thiscall](https://learn.microsoft.com/en-us/cpp/cpp/thiscall?view=msvc-170) describes ECX=this and callee cleanup. These explain the adapter shape; they do not establish this game's ABI by themselves. No x64 floating-state table is used as an x86 proof.

## Search-helper control-mode evidence

Independent rooted CFGs have no unresolved jumps or exhausted budgets for the following directly decoded bodies. No floating-point/SIMD/control-mode instruction occurs in these bodies.

| Entry | Instruction count | Remaining calls |
| --- | --- | --- |
| FindWindow0x5E4A20 | 106 | 13 call instructions, including branches beyond a successful lookup |
| FindChild0x5E4C80 | 33 | 3 calls; fallback is virtual |
| key search0x5E6B30 | 92 | stack-alignment/probe helper and USER32!CharLowerBuffA |
| existence0x5E68A0 | 65 | byte comparison |
| access0x5E6960 | 98 | comparison plus insertion/allocation/release branches |
| byte comparison0x14BD0 | 50 | none |
| alloca alignment0x6C0440 | 28 including tail probe | none |
| stack probe0x6B10F0 | 19 | none |

The lookup import is IAT RVA0x88A2C8, USER32!CharLowerBuffA. It is a separate system-API boundary, not resolved merely by finding its import name. Successful pinned window lookups and missing custom-control branches may exclude insertion/observer branches only under a validated runtime asset/profile constraint. These static bodies plus baseline native control-word/MXCSR samples do not certify all imported, logging, CRT or exceptional paths. On x86 both x87 and SSE control modes matter; Microsoft's [controlfp documentation](https://learn.microsoft.com/en-us/cpp/c-runtime-library/reference/controlfp-s?view=msvc-170) describes their separate state. Preserve the frozen fork controls; no mode change is proposed here.

On this machine, on-disk x86 `SysWOW64/user32.dll` SHA-256 `56F3BC77020E21EBEEF825965929DC0D0BD87F9CAFBB08DBCD477043E4342D2E` exports CharLowerBuffA at RVA0xBAA80 as a short integer-only prologue/tail jump through IAT0xEE4DC, imported from `api-ms-win-core-stringansi-l1-1-0.dll!CharLowerBuffA`. This narrows the external boundary to an API-set target; it does not establish its runtime resolution or callee behavior. Root did not invoke this API or begin a Windows-library compatibility workstream. Private `charlower-thunk.json` SHA-256 `46A5C2D7B05E83C1C7F97BDDACD8BE30BE92484727AB5C3C27C24D0CF44CB61F` records the exact thunk.

The relevant obligation is ABI/liveness preservation plus unchanged required ambient control modes, rather than bitwise equality of every native FP status/result across a function that normally performs arithmetic. The exact extra helper paths still need closure; post-original element/descriptor lifetime and reentrancy remain independent open requirements.

## Evidence and limits

Private files: `%LOCALAPPDATA%/V2DLLFork/R02/root-wrapper-symbols/`.

- Original `symbols.txt` SHA-256 `99BC2DFAA313CB1FCF7D04F15C721E3C11F6B3686DF153844B0585B18E9CC099`.
- `wrapper-disassembly.txt` SHA-256 `B50D22CEC53BA549C9BE7C08339F69773382F9D69C247C087B4970B825AF295A`.
- `native-abi-liveness.json` SHA-256 `D0853DE67C176574C67E7AE2D6E25DF9015AAB23073831E4CB85D0E0EB9227D4`, generated by external `abi_liveness.py` after exact executable identity check. CFG calls are recorded without traversing callees; indirect jumps are explicit. This is normal direct CFG evidence, not exception-edge discovery.
- Extended `helper-symbols.txt` / `helper-disassembly.txt` record compiled setup. The external DIA helper source was extended for extra names; original wrapper outputs above are retained unchanged.

No full active-profile gate, production backend/caller, candidate launch or HPM runtime acceptance follows. Root's static HPM GUI inventory is separately retained as `HPM-static-interface.json`, SHA-256 `725B67B3518400325FB6DF8AE066651BAECD5B500A63119ECEAA2D02B4BDCF4E`: 20 files / 6 GUI files, four custom keys absent. HPM overrides budget/politics; GHM runtime observations cannot substitute for HPM receivers.

## Independent retained-borrow review after resumed work

Root independently read the current static artifact `tooltip-lifetime/probe-call-audit-current.json`, SHA-256 `3D3B02DFB4D8B5DDDAAB73AE892F94150ED48F4E8BB98E8B938265BFC405572E`. Its 38 focused CFGs include output-copy helper0x115D0 (23 instructions), temporary-output destructor0x59820 (31), native string copy0x8ED0 (103) and element method0x240B40 (2), each without unresolved CFG edges or exhausted budgets. Current checked-in reader SHA-256 is `0A42FC2942BCCC3C92A0A62B53BDC70030B69371102DFAFD05F815F527FD30AF` at review; historical artifact identities remain distinct.

At outer entry0x269B60, let P be ESP before the dispatch argument pushes. Native0x269CD2 stores the hovered element at P+0x0C. Two pushes precede call0x269CF7; its native ret8 restores P at0x269CF9. On the zero composite-field branch,0x269D2B loads that same retained local,0x269D2F dereferences its vptr, and0x269D37 calls virtual+0x9C. The other branch reloads the same local0x269E21 and publishes it0x269E3D. No fresh hover resolution substitutes a potentially different element here. The intermediate normal output-string copy/destruction helpers use disjoint P+0x28/P+0xBC storage; the retained-local address is not supplied to them, and explicit unwind-state stores use P+0x124. Valid native string/allocator invariants remain preconditions.

This establishes an existing native borrowed-element contract across the selected-view dispatch, materially stronger than merely finding no known destructor in a bounded call graph. The immediate post-original name getter occurs inside that expected borrowing interval. The separate type-specific descriptor initialization/owner evidence and earlier stable Budget identity captures support its descriptor validity for the reviewed subtypes. This does not authorize retaining that pointer beyond the native activation, unknown GUI subclasses/plugins, or generic deferred observers.

The direct-call audit is corroboration rather than a requirement to re-prove all vanilla tooltip arithmetic. Root independently verified depth6 artifact `D9122C1ACCF31300D560E25CD9CFAC913487B40CA07096A85A4A5423C5188C68`:741 visited bodies/90,698 instructions;448 indirect sites;9 unresolved bodies;37 frontier records,33 distinct recorded targets and31 still unvisited. Bounded absence of a known teardown target is not universal exclusion.

The manager's subsequent PID31912 retained-element capture is pending independent raw-log review and post-exit preservation. At the user's renewed desktop-control stop, the process/debugger were left untouched; `native-retained-element/gdb.log` was still zero bytes when root read it, consistent with unflushed logging. Root did not flush/poll/control that debugger. Manager-reported observations from that unfinished experiment are not promoted to independently accepted raw evidence. Background code/evidence work remains authorized; current native experiment closure awaits desktop approval.

## Current background backend build review

After adding the uncalled Win32 memory backend, the clean Release Win32 build at `R02/build-win32-backend-final/` passed in6.31 seconds with0 warnings/errors. Root independently compared all47 manifest entries against both current source and staged source: exact hashes and file set, zero omitted/extra/mismatched files. New lua51.dll SHA-256 is `54088298AB06631C6C56BCD54493E9EE9A870F87DCA8DC6383D8079BF7FF5579`; matching PDB SHA-256 `B1F4E0F86C2A75ACEA9456ADD50801C724560EFFD9C9565F8589C68937E3E826`.

Root re-read the new PDB using DIA and compared the six previously reviewed machine-code spans: Update0, Tip1, Tip2, Tip3, OnViewUpdate and SetupButtons have identical RVAs, lengths and exact bytes to the earlier checkpoint. The compiled ABI observations above therefore apply to these unchanged spans in the new build. Private `backend-final-symbols.txt` retains the new symbol output; this does not extend proof to unreviewed helpers or production invocation.

The final Win32Memory source uses value-captured base/extent, allocation ownership and committed-page checks, single-page mutations, SEH-bounded copies and exact protection tokens. No code issue was found during independent root review. The focused tests delegate actual operations to this module and exercise restore-after-already-restored, nested same-page tokens, read-only/guard/noaccess/uncommitted boundaries, invalid copy pointers, range overflow and execute-permission preservation. Guard rejection verifies the guard flag is not consumed. The cache-modifier case records the actual OS protection; Windows normalized the request, so no execution evidence for retained PAGE_NOCACHE is claimed.

Root read actual test/build artifacts:19 scratch scenarios/2,404 checks passed, focused build0 warnings/errors in1.34 seconds. The code creates no production call site; source search finds only declaration/definition and owned-memory test use of MakeWin32ImageMemory. Identity/phase authentication remains external, and no game process was touched by this background work. Exact native experiment closure and independent raw-log acceptance remain pending.

## Conditional Politics text-filter follow-up

Root extended the external DIA reader against the current54088298... build/PDB; no product source or running process was changed. Tip3's exact `plurality` name comparison is inlined integer byte comparison at RVA0x5D07..0x5D2C. Tip1/Tip2 call the name getter but discard its result. The only extra Tip3 post-getter function is StripBareNumberLines0x1CE0, symbol length589.

Independent forward CFG review reaches195 instructions in StripBareNumberLines and35 in GStrSet0x1A90, length70, with no unresolved edges/outside-symbol targets. The compiler inlines the line classifier, strlen loops and GStrSet work inside Strip. Normal Strip paths call memcpy0x33A60 three times and security-cookie check0x3088F. The range-failure call0x309C0 and cookie-mismatch branch are exceptional/error paths, not additional normal-return guarantees. Strip and GStrSet bodies contain no x87, SIMD, floating-control write, allocation, engine command, native GUI callback or message-pump call.

The current static CRT memcpy symbol is RVA0x33A60, length1396. Root followed its actual CFG rather than decoding embedded tables as instructions. The forward tail's CMP ECX,16 / subtract16 loop leaves0..15; conditional subtraction of bit2/bit3 leaves0..3 before the four-entry table. The other forward and backward table indexes are masked with3. Exact tables at0x33CC4 and0x33D70 select four in-symbol byte-copy tails each; the apparent linear-disassembly jump0x33CBC is not reached and its following bytes must not be treated as a second pointer table.

Including those guarded tails yields404 reached memcpy instructions, zero unresolved/outside-symbol targets, zero calls/imports and11 normal return sites. ESI/EDI are saved at entry and restored before every normal ret; EBP is unused, EBX has only a self-LEA nop. The extra ECX alignment push/pop balances. Backward REP copying brackets STD with CLD before return. SIMD operations are only MOVDQA/MOVDQU/MOVQ/PALIGNR; there are no x87 or floating-control instructions, floating arithmetic or hidden callees. It may clobber volatile SIMD/general values and condition flags; it does not require bitwise preservation of those values. The reviewed normal helper paths preserve ambient x87/SSE control/status rather than resetting the frozen modes.

The cookie's four-instruction body compares ECX with the module cookie and returns on equality; mismatch tail-jumps0x308C5. No full error/unwind contract is claimed. Valid native GStr layout, live owned retBuf storage, readable NUL-terminated input and capacity/data invariants remain preconditions. The existing filter first clamps its temporary copy to1023 bytes, removes selected bare-number lines and writes within existing capacity without allocating. That truncation and filtering are pre-existing behavior, not a newly certified policy/bug-fix decision; S04's classification remains separate and no behavior edit was made here.

Private evidence under `root-wrapper-symbols/`: `strip-symbols.txt`, `strip-helper-symbols.txt`, `strip-disassembly.txt`, `strip-helper-disassembly.txt`, `strip_cfg.py` and `strip-cfg.json`. Rooted CFG JSON SHA-256 `C7C67A117D02D6BEEBA2205596B555B96FC9FBC228631E99C169F03FFBB07DA1`; source/DIA identities and exact bytes remain private. The linear helper listing includes embedded data and is only a navigation lead; the rooted CFG is the instruction-boundary evidence. This closes the narrow normal compiled post-tooltip text-helper obligation, without certifying pre-original lookup/import/logging paths, unknown descriptors, exceptional behavior, production profile/invocation or the unfinished native experiment.

## Bounded pre-original logging review

Root independently decoded Log RVA0x18F0 (212 bytes) and InitLogDir RVA0x1390 (370 bytes) from the same current DLL/PDB. Log's `g_settings.log` test at0x1900 branches directly to0x19B6 when false: integer cookie validation, balanced EBP/ESP restoration and plain ret. This path performs no directory initialization, file operation, locking, formatting or Windows API call. It preserves the relevant normal ABI/control state under the existing cookie invariant. This is conditional evidence only: compiled `Settings::log` defaults true, so it cannot be assumed for every supported configuration or replaced silently to evade validation.

The enabled Log body calls InitLogDir, EnterCriticalSection, `_wfopen_s`, GetCurrentProcessId, fprintf/vfprintf, fclose and conditionally LeaveCriticalSection. Its direct instructions contain no floating arithmetic or control-mode write, and both direct normal returns balance their frame. InitLogDir's already-ready path similarly contains only its flag branch and cookie/epilogue; its initialization path additionally calls module/path/directory APIs and wide-string CRT helpers. Identifying integer-only direct bodies does not establish the control-state/error/reentrancy behavior of their callees. A complete logging-enabled or initialization/exception contract is not claimed. No real log was opened, no product module loaded and no existing native process inspected for this review.

Private evidence: `root-wrapper-symbols/log-symbols.txt`, `log_direct.py` and `log-direct.json`; JSON SHA-256 `D51BC3649D33572ABBC5B1A0F8278AC591E8B6A2D5D40F28B4DD173F3D146421`. The complete short bodies were decoded within DIA symbol boundaries; their branches resolve inside those bodies, with calls recorded separately. This narrows the outstanding pre-original obligation without promoting generic CRT/API behavior or the unobserved HPM runtime path to accepted evidence.

## Independently reviewed headless public-import sample

The manager's owned Win32 console test `tests/fork/charlower_win32_tests.cpp` exercises public USER32!CharLowerBuffA without desktop, game, debugger or other-process access. Root read the code, project, build log and actual JSON result: Release Win32 /W4 /WX /O2 build passed with0 warnings/errors; seven fixed ASCII keys under three owned-process control modes gave21 calls /150 checks /0 failures. Tests verify exact converted spans, outside-span guards and x87 control-word/MXCSR control-bit preservation, restoring entry controls outside each measured API interval. They do not assert FP status-bit equality or universal locale/error behavior.

The test resolves the public export and the API-set provider, checks the USER32 API-set IAT equals that provider and obtains its owning KERNELBASE module. Reported System32 paths are logical WOW64 process paths; the physical x86 files are SysWOW64/USER32.dll (SHA-256 `56F3BC77020E21EBEEF825965929DC0D0BD87F9CAFBB08DBCD477043E4342D2E`) and SysWOW64/KERNELBASE.dll (`5BE784046DC9731D1C36E872A8022065ED4A733707B1E467B30A96588714B137`), both machine0x014C. The native System32 machine0x8664 comparison files have distinct hashes and are not substituted as tested provider identities.

Root independently verified all12 listed file sizes/hashes in `charlower-contract/evidence.json`, SHA-256 `5BDF3A5F039186D063B2AD80B197C4DFC333DCD8E20DD17A8562F9DDCC813DEC`. This accepts a sampled public-import prerequisite for pinned ASCII lookups, not game runtime invocation, receiver/lifetime/phase, generic external API preservation or the complete production profile. No existing native process or product build was changed.

## Renewed authorization: retained-element experiment closed

The user renewed computer-use authorization on2026-10-17 and subsequently requested our test windows use the second monitor away from their game. The earlier pending experiment is now closed. Read-only recovery by the Sol manager found no v2game process/window and an extant original GDB. Both Sol and root failed to access the saved PTY handle; the original agent alone recovered its scoped handle90104. Its narrow recovery verified inferior<null>, disabled logging and quit with exit0. This was original-handle capability recovery after two other agents failed, with no restart, attach, kill, new run or UI action. The manager's final OS inventory found neither owned game nor GDB remaining.

Root independently read the flushed raw log and verified SHA-256 `E5260F5BC81BDE71231811EFE378045799E095DBA19C90E7BD4BF16D3206AF90` (91,539 bytes). It explicitly records normal exit of inferior31912. The earlier tool-output-only retained sample is now independently supported by stable raw evidence: Budget element57E79098/descriptor1ECEF088/name tax_0_inc is passed at original entry, survives original return and remains at outerP+0C. At native269D37, ECX is that same element and EDX is base+240B40 before the native virtual+9C call. A second pre/post activation retains the same pointer. Normal stack/nonvolatile values and sampled CW007F, FSTAT4024, FTAGFFFF, MXCSR1FA1 match the reported sections; these remain native samples, not frozen fork settings or universal floating-state invariants.

Root independently rehashed all17 preserved paths against the accepted previous17-path baseline and confirmed exact executable, both original Lua DLLs, vanilla/GHM settings and12save hashes. It also checked all three private settings copies (before/current-at-resume/generated) equal SHA-256 `107719DA564543731EF6B1F13120F32F710ADCF0367044DE303CE4F1B6AA87EA`. No overwrite was necessary and no unattended settings difference was found. Final `native-retained-element/identities-after.json` SHA-256 `5824F99810D84603D6DAA334820EE960933FE5233F0FE26FC754DD6EE872CDE0` truthfully identifies the previous stable baseline and original prelaunch tool evidence; no synthetic prelaunch JSON was invented.

This supersedes only the pending raw-log/preservation status of this experiment. Its exact immediate borrowed-element observation is accepted. The production profile/phase, unsupported runtime types, enabled logging/error/reentrancy limits, HPM native baseline and broader active installer closure remain independent open gates. No candidate or HPM run was made during recovery; no1837 timing or requested-feature acceptance follows. Earlier95-anchor background index is historical once this report is extended; a later checkpoint index must record the new report and recovered evidence identities.
