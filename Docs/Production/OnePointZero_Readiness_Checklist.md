# Broken Horizon 1.0 readiness checklist

## Purpose

This checklist converts all remaining launch blockers into executable gates.  
We only move to 1.0 when every item is marked complete with evidence.

## Core rule

No item is “done” unless:

- a test or review command was run,
- the result is logged,
- evidence is filed under `Saved/Logs/` (runtime) or docs (art/content),
- and a date is recorded.

## Track A — Gameplay systems (G1–G3 gates)

| Item | Owner | How to verify | Evidence | Status |
|---|---|---|---|---|
| Dedicated host/reconnect continuity | Gameplay/Netcode | Host-loss/rejoin flow for 2+ clients | `G3-HostCrashRecovery2-20260801-050238-*`, `G3-NetImpairment2-20260801-051559-*`, `G1-FirstLightSharedReconnect-20260801-083153-*` | Verified (automated bounded recovery/rejoin) |
| Host/client First Light completion | Gameplay/QA | 2-player First Light route + extraction | `G1-FirstLightSharedReconnect-20260801-083153-*`, `BHPostAccessibility-FirstLightMP-20260801-130833-*`, `G1-ProductionRouteLootFinal2-20260801-192318-*`, `G1_FirstLight_Multiplayer_Evidence_2026-08-01.md` | Verified with the production keycard, door, guard, enemy-ammo, and extraction actors on dedicated authority; both clients completed and a replacement inherited completion. Manual feel review pending |
| Current UE 5.8 release gate | QA/Release | Editor build, 82 automation tests, startup smoke, and First Light playable-route/navigation smoke | `Saved/Logs/BHValidation-Build.log`, `Saved/Logs/BHValidation-Tests.log`, `Saved/Logs/BHValidation-Smoke.log`, `Saved/Logs/BHValidation-FirstLight.log` | Verified on 2026-08-08 with all four gate stages passing; First Light recorded 8 navigation fallbacks against a limit of 12. Protected DDC access requires the elevated validation boundary; rendered presentation, multiplayer feel, packaging, and manual controller review remain open |
| G3 mission director routing | Gameplay/Design | Attack + defense + raid path executed | `G3-AttackRoute3-20260801-080404-*`, `G3-DefendRoute-20260801-080702-*`, `G3-RaidRoute-20260801-081000-*` | Verified (automated multiplayer routes) |
| Transport/supply persistence | Gameplay/Systems | Save continuity across travel + reconnect, with transient battlefield loot excluded from authored persistence contracts | `G3-TransportTravelVerified2-20260801-043741-*`, `BHEphemeralAmmoFinal-*`, `BHBattlefieldLootFinal3-*`, `G1-AmmoHUDReplication-20260801-194127-*`, `SaveResilience_Evidence_2026-08-01.md` | Verified (bounded); runtime classification/consumption reach both clients, the owning client's weapon and bound HUD reach 180 / `30 / 180`, the used drop remains absent for a replacement while two unused drops remain, and checkpoints emit no missing-ID warnings |
| Load-aware vehicle performance | Gameplay/Logistics | Field transport cargo load reduces available speed, increases fuel burn, and is reflected in estimated range and travel time so supply movement creates an explicit operational tradeoff | `BHFieldTransport.h`, `BHFieldTransport.cpp`, `BHTransportHandling-Tests.log`, focused transport and First Light logistics evidence | Cargo-weighted acceleration, braking, and steering response now have a passing contract and current-source package proof; authoritative multiplayer replication/latency, terrain interaction, and manual driving/resupply feel remain open |
| Enemy combat, custody, and conduct persistence | Gameplay/Systems | Stable hostile-operative identity preserves transform, health, ammunition, readiness, surrender custody, and escape state across checkpoint capture and reapply to matching current or late-spawned mission actors without fabricating stale hostiles; saved-level/sector mismatches are rejected; detention, escape, and lethal-conduct outcomes remain strategic war events | `BHSaveGame.h`, `BHSaveSubsystem.cpp`, `BHEnemySoldier.cpp`, focused save/load and First Light runtime evidence | Source-integrated; UHT, wounded/armed/surrendered/dead checkpoint round-trip, late-spawn timing, saved-level/sector mismatch, multiplayer authority, and manual custody-presentation review remain open |
| 2h multiplayer soak | QA | 2h no blocker errors | `G3-TwoHourCampaignSoak-20260801-055216-*` | Verified (7,200s + reconnect) |
| Tactical progression choices | Gameplay/Design | Every campaign support unlock creates an accountable combat-operation choice | `BHMedicalPreparation-*`, `BrokenHorizon.PersistentWar.Progression.AfterAction` | Verified (recon, medical, reinforcement; manual balance review pending) |
| Custom campaign difficulty | Gameplay/UI/Networking | Host can expose and tune every GDD-10 pressure axis and all clients retain the exact profile through reconnect | `BHCustomDifficultyAxes-Focused.log`, `GDD10-CustomDifficulty-20260801-113802-*`, `BHCustomDifficulty-Final-*`, `BHStrategicUIFinal-RenderedUI-20260801-183138-*` | Six-axis keyboard/gamepad contract, retained/rejoin replication, and static rendered usability at 720p/1080p/4K verified; physical-controller interaction feel and subjective balance pending |
| Zero-supply recovery path | Gameplay/Logistics | A friendly squad cannot be permanently soft-locked by ammunition or medical depletion | `BHEmergencyFallbackKit-Focused.log`, `BHEmergencyFallbackKit-Final-*` | Verified in code and 57-test suite; manual multiplayer station interaction/cooldown feel pending |
| Multiplayer medical/vehicle recovery | Gameplay/Networking | Server treatment and recovery reach only the correct owning medical HUD while disabled transport recovery remains authoritative | `GDD04-MedicalVehicleRecovery-20260801-112726-*`, `BHMedicalVehicleReplication-Final-*` | Bounded two-client owner isolation and zero-to-full vehicle recovery verified; manual interaction/placement feel pending |
| Systemic operation taxonomy | Gameplay/Design | Attack, defense, raid, resupply, escort, rescue, and recon resolve through authoritative campaign contracts | `BHFieldReconOperationFinal-*`, `BHEscortVerified-*`, `BHRescueVerified-*`, `BHRouteVariations-*`, `BHPlayableRouteFinal2-*`, `BHBattlefieldLootFinal3-*`, `G1-ProductionRouteLootFinal2-20260801-192318-*`, `FirstLight_LiveGameplay_Evidence_2026-08-01.md` | Verified in automation and dedicated multiplayer; First Light exercises real keycard, door, guard/defense-wave, battlefield-ammo pickup, extraction, and war-victory paths. Continuous player-controlled traversal and multi-client operation-feel review remain pending |
| Field-squad Context command | Gameplay/AI | One player can assign only their own operative to a supported ally/objective action with persistent owner-visible state and explicit cancellation/failure | `BHFieldSquadFullContext-Final-*`, `BHFieldSquadContextHUD-Final-*`, `GDD05-ContextOwnership-20260801-111217-*` | Casualty aid, raid sabotage, secure, defend, owner HUD state, and bounded two-client ownership isolation verified; manual navigation/feel review pending |
| Enemy injury and morale withdrawal | Gameplay/AI | Low combat readiness combined with sustained suppression causes an enemy to break contact and retreat, while healthy soldiers retain normal combat behavior and isolated overwhelmed soldiers can still surrender | `BHEnemySoldier.h`, `BHEnemySoldier.cpp`, `BHEnemyAIController.cpp`, focused AI contract and First Light combat evidence | Source-integrated with authorable readiness threshold; focused AI contract, First Light behavior, balance, navigation coverage, and manual casualty-reaction review remain open |
| Casualty-aware cover handoff | Gameplay/AI | A dead or incapacitated cover claimant no longer blocks another operative from claiming the same physical cover point, preserving tactical occupancy after casualties without fabricating a new cover actor | `BHCoverPoint.h`, `BHCoverPoint.cpp`, `BHEnemySoldier.h`, focused AI contract and First Light combat evidence | Source-integrated; UHT, multi-AI claim handoff, navigation coverage, and manual casualty-reaction review remain open |
| Reload interruption under fire | Gameplay/Combat | Incoming damage or deliberate firing input interrupts an active tactical or emergency reload through the authoritative weapon path; tactical magazine state and emergency discarded-round state remain coherent while the owning player receives an explicit damage cue | `BHWeaponComponent.h`, `BHWeaponComponent.cpp`, `BHCharacter.cpp`, `BHRifle.h`, `BHRifle.cpp`, focused combat and multiplayer evidence | Source-integrated; authoritative interruption now clears procedural, montage, and rifle single-node reload presentation before restoring idle; source and packaged First Light smoke passed; dedicated-client timing, latency behavior, visual/audio blending, and manual combat-feel review remain open |
| Weapon heat persistence | Gameplay/Persistence | Checkpoints preserve the player's current weapon heat and valid overheat lockout, so a hot weapon does not silently return to a cooled state after reload while legacy saves remain compatible | `BHSaveGame.h`, `BHSaveSubsystem.cpp`, `BHWeaponComponent.h`, `BHWeaponComponent.cpp`, focused save/load evidence | Source-integrated with schema-57 validity gating; UHT, checkpoint round-trip, legacy migration, owner HUD replication, and manual heat/recovery review remain open |
| Fire-mode persistence | Gameplay/Persistence | Checkpoints preserve the player's selected semi-automatic or automatic fire mode when the equipped role supports it, while legacy saves fall back safely to the role default | `BHSaveGame.h`, `BHSaveSubsystem.cpp`, `BHWeaponComponent.h`, `BHWeaponComponent.cpp`, focused save/load evidence | Source-integrated with schema-58 validity gating; UHT, role/mode round-trip, owner replication, and manual input/readability review remain open |
| Environmental weapon acoustics | Gameplay/AI/Audio | Authoritative gunfire hearing reports combine an oriented, short-lived cached muzzle-enclosure probe with weather masking to apply indoor and battlefield-condition loudness/range behavior, while indoor/outdoor presentation tails remain aligned with the same physical environment | `BHBattlefieldConditions.h`, `BHBattlefieldConditions.cpp`, `BHRifle.h`, `BHRifle.cpp`, focused AI-hearing and First Light combat evidence | Source-integrated; UHT, weather hearing-range behavior, multiplayer authority, cache invalidation/performance, audio assignment coverage, and manual indoor/outdoor audio review remain open |
| Fire-mode HUD readability | UI/UX/Combat | The ammo/heat HUD exposes the selected `AUTO` or `SEMI` mode from initial rifle equip and updates after deliberate mode changes, role changes, and replicated checkpoint restoration without hiding ammunition or heat state; Blueprint listeners receive the same authoritative transitions | `BHAmmoHUDWidget.h`, `BHAmmoHUDWidget.cpp`, `BHWeaponComponent.h`, `BHWeaponComponent.cpp`, `BHCharacter.cpp`, rendered HUD evidence | Source-integrated; UHT, safe-area wrapping, 720p/4K layout, controller readability, duplicate-notification cadence, and manual combat review remain open |

## Track B — Art, textures, and world polish

| Item | Owner | How to verify | Evidence | Status |
|---|---|---|---|---|
| Placeholder sweep | Art | Replace checker/default/temporary materials in shipped paths | `AssetReadiness_PlaceholderAudit_2026-07-30.md`; enhanced editor-backed reverse-reference/metrics report in `Saved/Reports/BHAssetReadiness.json` | Measured: stock Quinn enemy presentation and 1.702-second StarterContent rifle sound are confirmed shipping-referenced production replacement candidates; provenance is no longer filename-only |
| Texture budget compliance | Art/Tech art | Texture sizes and stream/mem within target budgets | `Saved/Reports/BHAssetReadiness.json`, `BHRenderedPerformance-Final-RenderedPerformance-Summary.json`, `BHRenderedTraversal-Final2-RenderedTraversalPerformance-Summary.json`, `BHFullWorldTraversalCanonical-RenderedWorldPerformance-Summary.json`, `Performance_Budget_Evidence_2026-08-01.md` | Metadata, initial-view, objective-route, and canonical six-sector full-world baselines passed. Full-world local traversal held 100% desired texture data, zero pending stream-in frames, and 38% GPU-memory usage; visible mip-quality review remains open |
| LOD and collision pass | Art/Tech art | LODs exist for high-impact meshes; collision simplified for far objects | `Saved/Reports/BHAssetReadiness.json`, `BHFirstLightNavAlignmentComplete-Assets.log`, plus manual Editor/map review | Authoritative metadata measured for 7 skeletal and 7 static meshes: no unknown static LODs, no collisionless static meshes, and no single-LOD skeletal meshes. The only shipping-referenced single-LOD mesh is a 3.6 cm, one-material transient casing, accepted as proportionate; visible weapon-ejection quality remains manual |
| Material consistency pass | Art | Material instance usage and shader complexity bounded | `Saved/Reports/BHAssetReadiness.json` inventory; rendering review still required | Measured: 15 materials / 5 instances; visual/perf review required |
| Lighting/look-pass for shipped zones | Art | Final look in First Light + connected campaign sectors | Visual review notes | In progress |
| Audio/FX tie-in completion | Audio | Combat/state/mission cues in all shipped loops | `Saved/Reports/BHAudioFXReadiness.json`, `BHWeaponAudioSpace-Final-AudioFX.log`, `BHAIVoiceBarks-Final-*`, `BHMovementAcoustics-Final2-*`, `BHAmbientWarAudio-Final-*`, `BHUINotificationAudio-Final-*`, `BHWeaponAudioSpace-Final-*` | Measured: AI barks, movement acoustics, weather/war bed, semantic UI cues, indoor/outdoor weapon tails, and directional near misses verified in source, but only 1/18 required and 0/27 optional audited assignments resolve; production audio/FX authoring remains a blocker |

Shared context-ping gameplay now has automated, four-client, and paired-rendered
evidence in `BHMovingPingOcclusionNetFinal2-*` and `BHMovingPingRendered2-*`.
Actor-backed pings follow only with local line of sight and become a frozen
`LAST KNOWN` marker behind cover. `BHPingLaneFinal2-*` also proves the squad
marker remains separated from simultaneous incoming-threat chevrons on both
rendered clients. Colorblind readability, latency, and controller-conflict
review remain part of the
UI/accessibility gate rather than being inferred from automation.

## Track C — Accessibility and UI readiness

| Item | Owner | How to verify | Evidence | Status |
|---|---|---|---|---|
| Host/join/readiness clarity | UI/UX | UI readability and clarity in host/join and shared-command states | `BHSessionClarity-*`, `BHStrategicControlClarity-*`, `BHSessionRenderedFinal-RenderedUI-20260801-173048-*`, `UI_Readiness_Evidence_2026-08-01.md` | Static ready/searching/connected/failure states passed at 720p/4K with anchored Join and wrapped recovery guidance; physical focus/navigation and live online-session transition review remain pending |
| Combat readability | UI/UX | Hit feedback, objective/readout clarity with >1 client | `BHNotificationPriorityPass-*`, `BHCombatAwareStrategyFinal-*`, `BHPostCombatCadenceFinal-*`, `BHArmoredHitFeedback-*`, `BHSquadReadinessHUD2-*`, `BHBriefingResponsiveFinal2-20260801-180737-*`, `BHMovingPingOcclusionNetFinal2-*`, `BHPingLaneFinal2-*`, `G1-AmmoHUDReplication-20260801-194127-*`, `BHRenderedLootHUD-20260801-194607-*`, `BHOpenWorldUI1080Final3-HUD.png`, `UI_Readiness_Evidence_2026-08-01.md` | In progress (priority/preemption, combat-aware strategy deferral, latest-state post-combat coalescing, armor-impact cues, bounded suppression, responsive briefing, LOS-safe pings, owner-isolated ammo flow, and a packaged 1080p OpenWorld objective/health/strategic HUD collision review verified; physical controller, color perception, and latency/subjective cadence feel remain pending) |
| Accessibility baseline | UI/UX | Contrast, full remapping, toggle/hold modes, independent look tuning, visual-comfort controls, subtitles, safe-area/text scaling, difficulty assists, and readable HUD | `BHAccessibility-*`, `BHIndependentLookSettings-*`, `BHInputRemapping-*`, `BHDynamicInputPromptsComplete-*`, `BHDynamicInputPromptsRenderedFinal-*`, `BHDynamicInputPromptsUIFinal2-RenderedUI-20260802-052750-*`, `BHMenuFocusComplete-*`, `BHInteractiveMenuFocusFinal.log`, `BHInputPromptModeComplete-*`, `BHLocalizationComplete-*`, `BHWarMapLocalizationVisualFinal-*`, `BHInputPromptMode-SETTINGS-1920x1080.png`, `BHInputPromptMode-GAMEPAD-HUD-1920x1080.png`, `BHInputPromptMode-BOTH-HUD-1920x1080.png`, `BHToggleHoldModes-*`, `BHVisualComfort-*`, `BHSubtitles-*`, `BHSafeArea-*` | Implemented in source and automation; deterministic focus/back behavior, device-aware prompts, a conflict-free 4,072-word English catalog, and fourteen LEET expansion captures at 720p/1080p are verified. Expanded session errors, strategic cards, deployment tactical lines, and the custom footer remain bounded. Physical-gamepad switching, branded glyph art, professional translations, linguistic review, and couch-distance review remain pending |

## Track D — Delivery hardening

| Item | Owner | How to verify | Evidence | Status |
|---|---|---|---|---|
| Automated command health | Technical | Validation script passes (`Validate-BrokenHorizon.cmd -Build -Tests -Localization -Smoke -FirstLight`) | UE5.8 `BrokenHorizonEditor` build plus `BHFirstLightNavAlignmentComplete-*` and `BHWarMapLocalizationComplete2-*` | Verified: editor target compiled, all 66 tests passed, startup and First Light completed at 8/12 allowed navigation fallbacks, serialized Recast settings now align at 9,216 UU, and task-specific regex failure markers are enforced. The latest localization gate remains a 4,072-word zero-conflict catalog |
| First Light and full-world performance regression | Technical | Headless, static/traversal rendered, canonical full-world, two-rendered-client, four-player rendered-observer, and ten-minute sustained rendered gates pass (`-Performance`, `-RenderedPerformance`, `-RenderedTraversalPerformance`, `-RenderedWorldPerformance`, `-RenderedMultiplayer`, `-RenderedMultiplayerScale`, `-RenderedMultiplayerSoak`) | `BHShaderStutterAccepted-*`, `BHShaderTraversalAccepted-*`, `BHShaderWorldTraversalAccepted-*`, `BHRenderedPingFinal6-*`, `BHRenderedMPScale-Final-*`, `BHRenderedSoakCleanFinal-*`, performance/network evidence docs | Warm-cache static, route, and six-sector PSO/hitch gates passed with stable GPU budgets and zero hitch-associated misses; the accepted world run was 9.385 ms frame p95 with zero PSO misses, one allowed >33 ms frame, and zero >50 ms hitches. The sustained two-client soak passed 61,900 measured frames per client with complete texture recovery. Packaged/lower-tier, cold-cache packaged PSO, two-hour rendered campaign/reconnect soak, and visible-quality evidence remain open |
| Two-client network budget | Technical/Networking | Dedicated host plus two clients remain within byte/channel budgets and converge after reconnect (`Validate-BrokenHorizon.cmd -NetworkBudget`) | `BHNetworkBudget-Full-NetworkBudget-20260801-140711-Summary.json`, `Network_Budget_Evidence_2026-08-01.md` | Verified at localhost vertical-slice load: 10,082 B/s aggregate p95, 6,847 B/s per-client max, 21 channels, zero observed loss; shipping scale/Internet profile remains open |
| Four-client combat-density scale | Technical/Networking | Current default four-player capacity carries 16 additional native AI, active operation, and squad-command replication within budget; every client and rejoin proves population convergence (`Validate-BrokenHorizon.cmd -NetworkScale`) | `BHMovingPingOcclusionNetFinal2-NetworkScale-20260801-213010-Summary.json`, `Network_Budget_Evidence_2026-08-01.md` | Bounded localhost density verified with isolated campaign state: every client observed 19 AI and received the actor-backed hostile ping; 48,504 B/s aggregate p95, 12,349 B/s per-client max, 41 channels, zero loss; voice, WAN, and any future capacity increase remain open |
| Migration/save resilience | Systems | Save/load/reconnect and campaign continuity | `G3-CorruptPrimaryRecovery2-20260801-085022-*`, `G3-LegacySchema41Migration-20260801-090437-*`, `BHLegacyMigration-FullTests.log`, `SaveResilience_Evidence_2026-08-01.md` | Verified (protected corruption recovery + synthetic legacy migration) |
| Packet-loss and recovery behavior | Technical | Four-client combat density remains within network budgets and converges through reconnect at 80 ms lag/3% loss (`Validate-BrokenHorizon.cmd -NetworkImpairment`) | `BHWANBudget-Final-NetworkImpairment-20260801-143444-Summary.json`, `G3-HostCrashRecovery2-20260801-050238-*`, `Network_Budget_Evidence_2026-08-01.md` | Verified in bounded UE simulation: 50,856 B/s aggregate p95, 12,710 B/s per-client max, 42 channels, worst loss burst 12/20; geographic online-service validation remains open |
| Package/release smoke | Build/Platform | Packaged launch smoke for target builds | `BHOpenWorldUIComplete-PackagedOpenWorld.log`, fresh package `Builds/OpenWorld-Development-20260802-UIFix`, and packaged visible log | Fresh Development package verified for menu-to-listen-host OpenWorld travel, gameplay input, weapon fire, Escape pause/resume after mouse capture, and clean shutdown. Shipping configuration, installer/update path, and RC signing remain open |
| Final release notes lock | Production | No unresolved release blockers | PM tracker | In progress |

## 1.0 gate signoff

When every row is **Complete** with evidence, we are ready for 1.0 RC.

- Gameplay gate: in progress (automated routes complete; manual live feel/navigation review remains)
- Visual/content gate: in progress
- Accessibility gate: in progress
- Delivery gate: in progress

## Suggested 1.0 timeline (minimum)

1. Week 1: finish remaining G3 runbook + G3 soak
2. Week 2: art/texture cleanup and performance compliance
3. Week 3: accessibility and UX polish
4. Week 4: soak, packaging, migration/rollback pass, and RC freeze

## Latest realism increment (2026-08-09)

- Defeated enemy combat state: integrated. Stable field-operative IDs are saved at death and restored as inert zero-health enemies across save/load and corpse expiry.
- Evidence: focused persistence automation, editor build, source smoke, fresh package, and packaged First Light route passed. Aggregate automation remains partially blocked by the preserved unrelated DistantEventWeatherMix and SettingsContract failures.
- Manual/release follow-up: verify multiplayer load timing and corpse presentation in the editor; continue clearing the aggregate baseline before 1.0 sign-off.
## Latest weapon persistence increment (2026-08-09)

- Weapon heat and fire mode: contract hardened. SaveGame fields, owner replication flags, current-schema coverage, heat clamping, heat-spread behavior, and legacy validity gating are covered by BrokenHorizon.PersistentWar.WeaponStatePersistence.
- Evidence: editor build, focused automation, startup smoke, and First Light smoke passed; First Light recorded 3/12 navigation fallbacks.
- Remaining release review: live equipped-rifle checkpoint round-trip, dedicated-client timing, controller readability, heat/recovery feel, and the unrelated aggregate baseline failures remain open.
## Latest terminal death-state increment (2026-08-09)

- Defeated enemy restore: hardened. Server-only restore now clears stale incapacitation, medical-evacuation, surrender, custody, and timer state before replicating the corpse state.
- Evidence: focused contract and canonical automation passed; startup/First Light smoke passed; fresh Win64 package and packaged First Light route passed with four objectives and zero fatal package markers.
- Remaining release review: multiplayer late-spawn/save-load timing, corpse presentation, authored navigation coverage, and unrelated aggregate baseline failures remain open.
## Latest live terminal death increment (2026-08-09)

- Live lethal death: aligned with persistence. Surrender, custody, medical-evacuation, incapacitation, and timer flags are cleared immediately after any conduct outcome is recorded.
- Evidence: source build, canonical persistence/death automation, startup/First Light smoke, fresh package, and packaged guard route passed; First Light completed four objectives with zero fatal package markers.
- Remaining release review: direct surrendered-hostile multiplayer timing and HUD/corpse presentation, authored navigation coverage, and unrelated aggregate baseline failures remain open.
### Environmental weapon acoustics probe freshness (2026-08-09)

- [x] Environment cache refreshes when no sample exists or world time regresses.
- [x] Stationary muzzle movement and orientation changes invalidate cached indoor/outdoor classification.
- [x] Moving shooters use shorter freshness and travel windows for doorway and traversal realism.
- [x] Focused WeaponAudioSpaceContract automation passes.
- [x] Source build, First Light smoke, package cook/stage/pak, and packaged First Light route pass.
- [ ] Manually review rendered indoor/outdoor tails during stationary fire, traversal, rotation, and multiplayer ownership/latency.
- [ ] Measure five-ray probe cost under sustained automatic fire with the final audio mix and performance budgets.