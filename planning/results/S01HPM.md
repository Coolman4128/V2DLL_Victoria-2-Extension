# S01HPM native workload baseline

State: **COMPLETE - all packet exits independently accepted by root**. Start2026-10-17 22:44:53 EDT; final acceptance recorded22:58:55.953 EDT,14m02.953s elapsed including manual assistance, validation and review. Original game/Lua only; no candidate, timing or R02b work.

| Check | Current result |
| --- | --- |
| Official HPM0.4.6.3 archive/install | Root reread:8,432files/267,006,715bytes, exact archive pin and zero missing/extra/hash mismatches; read-only2.279s |
| Prelaunch preservation |17baseline files copied/hashed,12existing saves, all backup bytes match; HPM userdata absent before this run |
| New HPM settings |Only new HPM/settings.txt seeded from known windowed1440x1080/fullScreen=no/borderless=no snapshot, SHA107719...87EA; vanilla/GHM untouched |
| Native launch |PID39492, exact original EXE from actual working directory, `-mod=mod/HPM.mod`, original11,264/167,936-byte Lua files |
| Native menu |PASS, root viewed actual private image: Heart of Darkness `V2v3.04(WOAN)`; no custom HPM logo claimed. Exact descriptor/process/new HPM userdata and pinned installation establish selection |
| Second monitor |PASS, actual frame(-2941,-115,2162,1668) wholly inside physical DISPLAY2(-3840,-274,3840,2160); no user's-game/display manipulation |
| Usable fresh1836setup |PASS, user manually entered Belgium after supported pointer input failed; actual capture shows Belgium,January1,1836,red paused bars. Root viewed and accepted actual image |
| Normal close/post-exit preservation |PASS, supported Alt+F4 normal close, owned process handle ExitCode0 at02:56:15.592Z; all17preexisting files and exact12save set unchanged, no restoration/overwrite required; no owned game/GDB remains |
| Simulation/performance/features |NOT RUN; no speed/feature/complete compatibility claim |

Private evidence root `%LOCALAPPDATA%/V2DLLFork/S01HPM/`: `prelaunch-identities.json` SHA-256 `BAD2DDAEF326CF7E362FCF03405E70C5370D5021E3D2FC162BA17EF9D3B2B489`; exact backups; `launch-process.json`; `native-menu.jpg` SHA-256 `B75238579A3FAC5C35A130C4DF1A9A34FFE165BA03DD70F70C44BBF22180114D`; `native-menu-capture.json`; `input-recovery.json`. Root installation evidence `root-install/verification.json` SHA-256 `B486AA2EF75A1524CA169C38005D842CD04FCE1370F2E0C1E79D15972C6E619A`.

The first pre-render capture showed the user's underlying game; it was not archived or used for coordinate input. After activating only the returned owned Victoria2 window, the actual menu/titlebar was visible. A cross-monitor drag reported an endpoint bounds error; fresh observation subsequently found the window on the second display, so no blind retry or causal assumption was made. Root's physical EnumDisplaySettings query corrected the earlier DPI-virtualized Screen.AllScreens width: physical second monitor is3840x2160, not2560x1440, with no actual coordinate gap. Physical inventory SHA-256 `74DAF2FCF73A8C22FC285E41418AE84BF3C01F8F58D9D838529A436CFC83C6DD`. Earlier R02 immutable logical inventory remains historical.

The supported input retry reselected the unique returned window, inspected accessibility, reactivated only that window and refreshed the owned frame. Both clicks at the observed SinglePlayer button failed with SendInput87. Ordinary Tab/Return did not change the menu; no shortcut was invented. All UI input stopped until the user explicitly reported DONE after manually entering fresh1836Belgium. No native input bypass, display change or move back onto the user's game was attempted. This is a manually assisted native baseline, not a claim that automated pointer input works on this display.

The same PID39492/window29427278 was revalidated before read-only setup capture. `native-1836-belgium.jpg` SHA-256 `4096AF842BBDD6B036C5DA0E53B752280B2D6A25BAFB1297FF95E8565B96767E` shows Belgium,January1,1836,paused; final frame(-3605,-143,2162,1668) is wholly on physical DISPLAY2. Root viewed the actual image and metadata. Supported Alt+F4 then closed the game normally; `normal-exit.json` records retained process handle WaitForExit/ExitCode0. No save, kill, restart, debugger or candidate was used.

`post-exit-identities.json` SHA-256 `1EC3CF7E86727F0B2CD672B065B886E0065F9DF7754D4BB8EBB6173220AC30CD` records17matches/0mismatches and exact12unchanged saves. All existing settings already matched prelaunch bytes; no stale backup was written over current files. Generated HPM settings also matched the seeded windowed settings. New HPM userdata, absent before launch, remains separate:897files/357,934,882bytes; `generated-userdata-manifest.json` SHA-256 `4459C1F17E28095384915A18C8324E6F684D54C619F8D25DD1F18B36D68E5D2B`.

Final private `artifact-index.json` has45stable anchors, SHA-256 `D81540F6072BBB79BAA7797963B9F388E864CB1C4CE2DC5FD6E2DE5F2EFA2A5C`. Its review-ready state records the instant before final root review; root subsequently verified all45anchors, all17current baseline files, exact12save set and every one of897generated-userdata entries and independently accepted all S01HPM exits. The earlier10anchor partial index is retained as historical evidence. No proprietary contents enter Git. S02/R02 remain incomplete; R02b remains planned and requires separate assignment.
