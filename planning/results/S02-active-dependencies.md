# S02 active installation dependency map

Observed 2026-10-17 from current source, manually reviewed by the root orchestrator and session integrator. This is a source activation map, **not a supported executable profile**. Every row remains blocked for candidate deployment until its complete native dependencies, instruction boundaries, ABI, object lifetime and phase have evidence. Literal byte matches do not remove that block.

No root `v2dll_settings.ini` currently exists in the game installation. An unstaged first candidate would therefore start compiled defaults and may generate the default file. `localModConfig=false` means GHM settings do not override it initially. The shipped INI is not evidence of effective settings. S03 owns policy/default corrections; several unrelated legacy gameplay changes remain enabled at this S02 checkpoint and must not be mistaken for the final retained-fixes baseline.

| Source activation | Installer/dependency closure |
| --- | --- |
| Before `Install`, DllMain | `InstallCrashWatch`, `PinFpu`; exception/watch handlers, IAT effects and initial floating-point state are outside any gate inserted only inside Install |
| Unconditional | `InstallEngineStability`, `InstallArmySelectDiag`, every `VIEWS` vtable replacement, `InstallExePatches` with per-record controls, `InstallFactoryDumpScan`, `InstallDestroyCmdNullCheck`, `InstallGoodsConsumption` parser, `InstallChecksumDiagnostic`, `InstallLobbyEntryHook`, `InstallOosWatch`, `InstallRngProbe`, `InstallEventRollProbe`, `InstallStateProbe`, clipboard worker creation; internal gates still matter |
| `patchPopQuantize` | `InstallPopQuantize` |
| `removeSleepers` | `InstallMpClientSleep`, `InstallWfsoHook` |
| `decisionFilter` | CDecision `IsValid` vtable slot |
| `armyBetterReinforceOnAlliedOwnedLand` | `InstallOccupiedReinforceSplit`, `InstallAllyOwnerCheck` |
| `navyAllowUseAllyTransports` | `InstallAllyEmbark`, `InstallPhysfsOpenReadHook`, `InstallEmbarkLoadRepair`, `InstallFleetPassengerTooltip`, `InstallFleetCaptureHook` |
| Forced `patchCivilizeNullCheck=true` | `InstallCivilizeNullCheck` |
| `patchSupplySourceNullCheck` | `InstallSupplySourceNullCheck` |
| `patchTechNullCheckFixes` | `InstallTechCompareNullCheck`, `InstallTechFolderIconNullCheck` |
| `patchGraphPointClamp=false` | `InstallGraphPointClamp` dormant under compiled defaults |
| `factoryAllowConstructionInColonial` | `InstallProdListVisibilityHook`, `InstallProdTypeGateHook` |
| `hideRawGoodsFilter` | `InstallGoodsFilterPosProbe` |
| `filterShowAllInState` or `factoryFilterByProducersOnly` | `InstallFilterShowAllInState` |
| `enableRestartGameButton` | `InstallMenuCloseObserverHook` |
| `enableMusicPlayer` | `InstallPlayerButtons` |
| `musicFairRandom` | `InstallMusicFairRandom` |
| `patchHideNoSupplyFactories` | `InstallHideNoSupplyFactoriesHook` |
| `factoryRefundSavingsOnClose` or `factoryAutoCloseUnprofitable` | `InstallFactoryClose` |
| `factoryAiSpamFix` | `InstallAiExpandStaffing` |
| `patchAiNavalBaseLimit=false` | `InstallAiNavalBaseLimit` dormant under compiled defaults |
| `factoryForceMinWage` | `InstallFactoryMinWage` |
| `patchNeedsHonestUi` | `InstallNeedsHonestUi` |
| `factoryAiFactoryPriorityRuleSwap` | `InstallPriorityByRule` |
| `popFixNeedsModifiers` | `InstallFixStrataNeedsMods` |
| `popCardNeedsAffectedByNeedsModifiersFix` | `InstallPopCardStrataMods` |
| `popBudgetPayoutNeedsScaled` | `InstallBudgetPayoutByNeeds` |
| `EventSounds=false` (`ENABLE_EVENT_SOUNDS`) | `LoadEventMusicDll` dormant under compiled defaults |
| `tradePriceDeltaExponential=false`, else `tradePriceDeltaPercentual=true` | Exponential installer dormant; percentage `InstallPriceDelta` active |
| `versionLabel` | `InstallVersionLabel` |
| `minting` | `InstallMinting` |
| `assimilateBlock` | `InstallAssimilateBlock` vtable and effect-factory detour; current legacy sequence can partially install |
| `goodsIconsAsSeparateFiles` | `InstallGoodsIcons`; shared PHYSFS dependencies also used by allied-transport save observation |
| `patchCompressFlagCache` | `InstallFlagCache` |
| `patchPopPrefetch` | `PopPrefetch::Install` |
| `patchFastAiQueue` | `FastAiQueue::Install`, before DecisionCull |
| `patchCullDecisionChecks` | `DecisionCull::Install` |
| `patchParallelDecisions` | Consumed through DecisionCull/FastAiQueue, no separate direct Install branch |
| `patchParallelEvents` | `ParallelEvents::Install` |
| `patchCullCbChecks` | `CbCull::Install` |
| `patchPrefetchRelations` | `RelationPrefetch::Install` |
| `patchPrefetchAiRelations` | `RelationPrefetch::InstallAi` |
| `combatEnableRollChanges` | `InstallCombatRoll`; local missing external cave means it must report skipped, not applied |
| `combatSecondLineSupportUnitPriority` | `InstallReserveFrontline` |
| `combatSecondLineDamageRedirect` | `InstallSecondLineSplash` |
| `loadDllFromModFolder` | `ResolveModFolder`, optional `LoadV2Mods`; absent plugin is inert, present native plugin has its own dependencies |

EngineStability expands to unconditional `PinFpu`, timer resolution, and `TryPatchLateModules`; SelectHook when `fixSfxMixerLag || removeSleepers`; HighPriority/LFH by their controls; four EXE kernel32 IAT replacements (`CreateThread`, `LoadLibraryA`, `LoadLibraryW`, `GetTickCount`) when thread pinning is enabled; main-loop FPU hook and sleep patches by their controls. These are additional native contracts, not just one validated InstallEngineStability call.

Late-module closure rehooks those four EXE IAT entries and, when applicable, `Direct3DCreate9`. It patches tbb.dll CreateThread and MSVCR100 `_beginthreadex`; `HookTbbInitOnExe` is inactive at `engineWorkerThreads=0`. `HookLoadLibraryA/W`, `HookGetTickCount`, and `MainLoopThunk` may trigger this closure later. Direct3D object/device vtable replacements happen after creation and require their own boundary evidence. Windows/imported module identity is separate from the game EXE's identity.

Frozen effective values remain unchanged: FPU/D3D/thread pinning enabled, worker thread override zero, POP quantization enabled with 12 retained bits, state savings cap enabled at 60,000 pounds, sleeper replacement at 1 ms, renderer cap 70 with no-vsync disabled. Disabling these to obtain a partially supported launch would not establish the required preserved baseline.

The new `Fork/AddressProfile` module is a pure transactional seam. It has **no production profile or native backend installed**, never runs legacy installers, and its synthetic acceptance cannot certify this map. Resume targets, callable entry points, global data, imported functions, vtable values and dynamic later writes must all be included in future explicit dependency sets. The legacy startup return is now labeled as sequence completion, not feature availability.
