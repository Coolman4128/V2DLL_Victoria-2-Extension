# S02 returned-device installation window — feasibility result

- Session: `S02-20261018-returned-device-window-feasibility`; 15-minute estimate, 1.564 minutes actual, approximately 1,204.67 minutes cumulative S02 family wall at close. One bounded source/evidence question: can the existing `InstallDeviceTablePair` safely replace shared static-provider template writes by patching the new device during `HookCreateDevice`, before returning it?
- Decision: **NO — the current transaction cannot be used for the observed native object.** The transaction rejects `staticTemplatesInstalled_` mode (`D3dInstallTransaction.cpp:1089-1090`); otherwise it requires the returned vtable and all slots to be in immutable provider image storage (`:1114-1124`). The production backend is image-scoped (`D3dInstallTransaction.cpp:1195-1218`; `Win32Memory.cpp:76-111`). R02u observed the actual returned device vtable in writable `MEM_PRIVATE` heap storage. `InstallDeviceTablePair` therefore rejects it before writes.
- Root independently inspected `Source/V2/V2/V2TechButton.cpp` (`InstallD3dStaticDeviceTemplates`, `VerifyD3dStaticDeviceCopy`, `HookCreateDevice`), `Source/V2/V2/Fork/D3dInstallTransaction.cpp` (`InstallDeviceTablePair`) and `Win32Memory.cpp` (`MakeWin32ImageMemory`). The code at `V2TechButton.cpp:13794-13825,13841-13867` shows production installs static templates before calling the provider and performs only a read-only device-table check after success. The legacy table transaction serializes its own operations, publishes global originals, applies two sequential slot changes, and journals raw vtable addresses; it does not retain a COM object or prove quiescence from provider readers.
- Native evidence limits: R02u identifies the observed vtable as `MEM_PRIVATE`; R02v shows the provider can recopy Reset/Present entries from its saved static template during Reset and that ordinary zero-ref Release can free the embedded table (`R02v-d3d-heap-lifetime.md`, sections 2–4). R02u/v do not prove all driver/device paths or exclusive same-object/thread use during a post-create table rewrite.
- Conclusion: moving the current image-backed transaction to the returned-device callback is not a safe workaround. A heap-aware per-device transaction would be a new implementation contract and still needs proof of output publication, Reset recopy handling, address reuse/lifetime and concurrent use. This session did not implement or test one.
- No source, build, tests, game/debugger, candidate, deployment or benchmark changed. S02 remains incomplete; profiles remain Rejected/WITHHELD; S03 stays gated; HPM speed remains NOT MEASURED.

## Evidence references

- [R02u runtime device-table observation](R02u-d3d-native.md)
- [R02v heap-table lifetime and Reset recopy](R02v-d3d-heap-lifetime.md)
- [S02 safe-evidence-path result](S02-safe-evidence-path-20261018.md)


## Timing review

Exactly one independent whole-session closeout review was recorded because the session finished below half its estimate. It took 1.564 minutes versus 15 minutes because source guards and exact native evidence were already available, manager and root checks were parallel, and the question had a direct negative discriminator. Similar read-only source feasibility checks with accepted evidence merit 2–4 minute estimates. The review changed no work or scope.
