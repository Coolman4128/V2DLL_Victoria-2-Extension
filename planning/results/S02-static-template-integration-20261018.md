# S02 static-template source and compiled integration checkpoint

Session: `S02-20261018-static-template-integration`; estimate **45 minutes**, actual **5.124 minutes**, family wall span **1165.086 minutes (19.418 hours)** through session close. The one post-session timing review found the short execution reflected a prepared toolchain and finite existing harnesses; its calibration is to estimate owned source/compiled checks separately from proprietary runtime safety evidence. Parent S02 remains incomplete.

## Scope and result

The session verified the current undeployed R02w static A/B provider-template transaction source, its actual `HookCreateDevice` callback ordering, fault/recovery contracts on an owned x86 PE32 surrogate, and a clean current-source Release Win32 build. This does **not** close production D3D acceptance. The real system provider was not patched by this session, no candidate or game process was launched, and original-game route/phase, transitive `HookPresent` callbacks, and M3 other-thread lifetime remain unproved.

## Checks

- `py -3 tests/fork/r02w_test_static_templates.py`, using the already-installed S02 `pefile` directory: **23 cases / 655 checks / 0 failures**. Artifact: `%LOCALAPPDATA%/V2DLLFork/R02w/luna-tests/run-20261018-153046-465387`; manifest SHA-256 `8D1E0F24AACD56031CF1147261377E08AEB39B8271B811B6FF4EFA39ABC4A082`.
- `py -3 tests/fork/test_d3d_install_transaction.py`: **25 cases / 555 checks / 0 failures**. Artifact manifest SHA-256 `25B590FB8D185018A373F83CF2035643F84358D45ED95C39292908911B47FC06`.
- `py -3 tests/fork/test_d3d_hook.py`: **25 cases / 555 checks / 0 failures**. Artifact manifest SHA-256 `F357DC271F69B6D142821C14AE5335643F84358D45ED95C39292908911B47FC06`.
- `py -3 tests/fork/test_startup_gate.py --build-root %LOCALAPPDATA%/V2DLLFork/S02/startup-gate-20261018-1535`: **253 assertions passed**. Production positives remain explicitly withheld; rejects have zero dependent effects.
- `./tools/build-release-win32.ps1 -BuildRoot %LOCALAPPDATA%/V2DLLFork/S02/build-static-template-20261018-1534`: clean Release Win32 rebuild succeeded, **0 warnings / 0 errors**, 8.20 seconds. The 59 staged `.cpp/.h/.c/.vcxproj` inputs match current source hashes exactly.
- Compiled audit: all three DLLs are PE32/i386; all **128 Lua export names, ordinals and forwarders** match the forwarding header; all three PE CodeView GUID/age identities match their PDBs. DLL SHA-256: `lua51.dll` `AFF1EC0098E769AE0ED5C9AE9452030182467210F3DABDF1F9A7A994060C7EF4`; `eventmusic.dll` `175E0FA44E415370F81DECF08CC240E0E209B3937EAF3913BDAA81A19724BA90`; `v2dll_mod.dll` `7E726BDE22FD39B5A14556476760BE18C74FBF4783AA0C832C99766FE6E6CF8A`. The private compiled audit script and artifacts are under `%LOCALAPPDATA%/V2DLLFork/S02/`.
- `git diff --check`: passed. The first direct `py -3` static-template invocation stopped before compilation because system Python lacks `pefile`; rerunning with the existing S02 vendored package path passed. The first startup-gate invocation also stopped before build because its default private output directory already existed; rerunning with a fresh private build root passed. No existing directories were removed or overwritten.

## Independent technical review and remaining gates

The read-only source review found that `PatchGroup` writes four A/B slots sequentially while the transaction gate serializes only installer calls. It does not exclude a concurrent `HookCreateDevice` call from entering the native provider during patch or rollback. Therefore a concurrent native template copy could observe a mixed slot pair. The post-call copy check cannot prevent that. Current evidence does not establish a quiescent exclusive installation phase, and this session did not implement a new serialization behavior or assert it is safe to change native API semantics.

The source callback fixture stubs the install/copy callbacks; the transaction fixture exercises actual transaction code only against an owned surrogate image. Neither proves hook inheritance through real pinned `d3d9.dll` template writes. The prior SDK fixture used unmodified native templates. Thus real provider inheritance is **NOT RUN / NOT PROVEN**. All production profiles remain **Rejected/WITHHELD** and S03 remains gated.

No game, debugger, deployment, private game file, mod installation, or performance run occurred. No frozen settings or gameplay behavior were changed. The one timing-only review is recorded in the persistent S02 session ledger; it created no tasks or scope changes. See [the next-session handoff](../handoffs/S02-static-template-integration-20261018.md).
