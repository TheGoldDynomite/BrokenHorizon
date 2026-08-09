# Packaged Release Evidence - 2026-08-08

## Scope

This record covers the current Windows Development package after the First
Light ambient audio import and the cooked Enhanced Input runtime-context fix.
The archived build is a local release-readiness artifact; packaged output is
not committed to the repository.

## Evidence collected

- First Light ambient assets were imported by
  `Content/Python/create_first_light_audio_assets.py`.
- Import completed with `0 error(s), 0 warning(s)` and created
  `SW_FirstLight_Wind`, `SW_FirstLight_Rain`, `SW_FirstLight_WindRain`, and
  `SW_FirstLight_DistantWar` as 48 kHz stereo, 12-second SoundWave assets.
- The full validation gate passed on retry after one transient MSVC internal
  compiler error:
  `Validate-BrokenHorizon.cmd -Build -Tests -Smoke -FirstLight -AudioFX`.
- The editor build passed, automation passed, startup smoke passed, First
  Light smoke passed, and the audio/FX audit passed.
- The automation log reported `85 tests performed`, including the focused
  First Light ambient asset contract.
- First Light smoke reported navigation fallbacks `8/12`.
- Audio/FX audit inventory now reports `required=3/18` and `optional=1/27`;
  this is an improved structural assignment pass, not proof that the entire
  presentation layer is fully authored.
- `RunUAT.bat BuildCookRun` completed successfully for the current archive at
  `Builds/FirstLight-AmbientAudio-Development/Windows`.
- The current packaged First Light map exited with code `0` with audio enabled,
  and the audio device initialized at 48 kHz on the local WASAPI endpoint.
- Packaged First Light logged
  `BH_AMBIENT_AUDIO_READY wind=1 rain=1 war=1 looping=1`.
- The current packaged logs contain no missing First Light audio object,
  navmesh-settings mismatch, fatal error, unhandled exception, or cooked input
  mapping ensure marker.
- The ambient war director now assigns the distinct wind, rain, and distant-war
  SoundWaves as native defaults, retains a legacy combined-loop fallback, and
  keeps all four imported SoundWaves loop-enabled for the three ambient audio
  components. The focused asset contract loads all four exact package paths and
  verifies their loop configuration.

## Runtime fix included

`ABHCharacter::RefreshPlayerInputMappings` now rebuilds a transient input
mapping context from the cooked source mappings instead of recursively
duplicating the cooked `UInputMappingContext`. Modifier and trigger references
are reused, preventing packaged builds from attempting to replace loaded
`InputModifierNegate` subobjects while retaining user remapping behavior.

## Remaining 1.0 gates

- Run the packaged validator against the new archive rather than the older
  `FirstLight-Development` artifact used by the initial packaged baseline.
- Perform visible rendered First Light review with audio enabled, including
  spatial attenuation, ambience layering, combat feedback, UI readability,
  animation, and navigation coverage.
- Author and wire the remaining required and optional audio/FX assignments;
  the audit inventory is not yet a finished presentation pass.
- Complete controller, networked First Light, lower-tier performance, and
  two-hour rendered soak checks.
- Reconcile the remaining navigation fallbacks before release G1 sign-off.
## Feedback Audio Expansion Evidence (2026-08-08)

The First Light feedback-audio pass expands the required audio contract beyond ambience with native defaults for rifle and enemy weapon fire, dry fire, reload, indoor and outdoor tails, material-specific concrete/dirt/grass/metal/water footsteps, near-miss feedback, and objective confirmation/warning/alarm notifications. The importer remains idempotent and configures the four ambient SoundWaves as looping while keeping the fourteen feedback cues one-shot.

- Audio import commandlet log: Saved/Logs/BHFirstLightAudioImport-feedback.log
- Import result: 14 new one-shot SoundWaves created, four existing ambient SoundWaves skipped, zero commandlet errors; the importer reported one nonfatal source-quality note for a DC offset at the start of SW_FirstLight_UIWarning, which remains a manual polish item.
- Focused/full gate: Validate-BrokenHorizon.cmd -Build -Tests -Smoke -FirstLight -AudioFX
- Full gate result: exit 0; editor build, automation, startup smoke, First Light smoke, and audio/FX audit passed.
- Audio/FX audit result: Assignments: required=18/18, optional=6/27.
- First Light navigation result: 8/12 fallback points remain, with no navmesh mismatch or empty-navmesh failure in the fresh packaged run.
- Packaged archive: Builds/FirstLight-FeedbackAudio-Development/Windows
- Package result: BuildCookRun completed successfully with exit code 0 and produced the archived Windows build.
- Direct packaged First Light smoke log: Saved/Logs/BHFeedbackAudioPackagedFirstLight-20260808b.log
- Audio-enabled runtime result: WASAPI initialized at 48 kHz; BH_AMBIENT_AUDIO_READY wind=1 rain=1 war=1 looping=1 appeared; the runtime recorded RequestExitWithStatus(0, 0).
- Fresh packaged error scan: missing object 0, missing package 0, navmesh mismatch 0, empty navmesh 0, fatal error 0, unhandled exception 0, ensure failure 0.

The generated feedback samples are deterministic development source cues and still need human mix/presentation review in the editor and in-game. The UI warning DC-offset note should be cleaned up when the final recorded or designed cue replaces the development sample. Remaining 1.0 gates include rendered visual review, controller/UI interaction review, networked two-client First Light, lower-tier performance, long-duration soak, and raising navigation coverage beyond the current fallback set.
## Release Candidate Packaged Gate Evidence (2026-08-08)

The packaged validator previously targeted the obsolete Builds/FirstLight-Development/Windows archive. Config/ProjectManifest.json now selects the canonical release-candidate executable at Builds/FirstLight-ReleaseCandidate-Development/Windows/BrokenHorizon.exe, leaving older archives intact.

- Release-candidate package log: Saved/Logs/BHPackage-ReleaseCandidate-20260808.log
- BuildCookRun result: exit 0; Windows Development archive completed successfully.
- Official packaged gate: Validate-BrokenHorizon.cmd -Packaged -FirstLight -AudioFX
- Official gate result: exit 0; Audio/FX readiness passed with required=18/18 and optional=6/27, First Light smoke passed with navigation fallbacks 8/12, and packaged smoke passed.
- Packaged validation log: Saved/Logs/BHValidation-Packaged.log
- Audio-enabled release-candidate smoke log: Saved/Logs/BHReleaseCandidatePackagedFirstLightAudio-20260808.log
- Audio-enabled result: process exit 0; WASAPI initialized at 48 kHz; BH_AMBIENT_AUDIO_READY wind=1 rain=1 war=1 looping=1 appeared.
- Audio-enabled failure scan: missing object 0, missing package 0, navmesh mismatch 0, empty navmesh 0, fatal error 0, unhandled exception 0, ensure failure 0.

This closes the stale-packaged-artifact validation defect. Multi-platform SDK validation remains limited to the installed Win64 SDK, and the broader manual presentation, multiplayer, performance, soak, and navigation-coverage gates remain open.
## Cooperative Squad Ping Resilience Evidence (2026-08-08)

The network impairment gate exposed a delayed-controller UI race: ClientA received the authoritative squad ping and tracked hostile, but its local CombatStatus widget had not been created when the pawn first began. The ping update path now lazily creates the local widget once the owning controller and LocalPlayer are available, initializes health/stamina/grenade/engineering state, and leaves replicated tracked or last-known presentation semantics unchanged.

- New helper: Source/BrokenHorizon/Private/BHCharacterHUD.cpp
- Call-site declaration and invocation: Source/BrokenHorizon/BHCharacter.h and Source/BrokenHorizon/BHCharacter.cpp
- Dedicated helper build: Validate-BrokenHorizon.cmd -Build passed after the source-file split.
- Network scenario gate: Validate-BrokenHorizon.cmd -Tests -Smoke -FirstLight -NetworkBudget -NetworkScale -NetworkImpairment
- Network result: exit 0; automation, startup smoke, First Light smoke, multiplayer reconnect, impairment, and network budget evidence passed.
- Squad-ping summary: Saved/Logs/BHValidation-NetworkImpairment-20260808-185524-Summary.json reports squadPingReplicationRequired=true and squadPingReplicationVerified=true.
- ClientA evidence: Saved/Logs/BHValidation-NetworkImpairment-20260808-185524-ClientA.log records BH_SQUAD_PING_APPLIED followed by BH_SQUAD_PING_PRESENTATION revision=1 tracked=1 visible=0 mode=LAST_KNOWN.
- Network budget result: aggregate outbound p95 52758 B/s, per-client outbound maximum 13284 B/s, 44 channels.
- Updated package log: Saved/Logs/BHPackage-ReleaseCandidate-NetworkFix-20260808.log
- Updated packaged gate: Validate-BrokenHorizon.cmd -Packaged -FirstLight -AudioFX passed with exit 0.
- Updated audio-enabled package smoke: Saved/Logs/BHReleaseCandidate-PackagedAudio-NetworkFix-20260808.log; process exit 0, WASAPI 48 kHz, ambient marker present, and zero missing-object, missing-package, navmesh, fatal, exception, and ensure markers.

This closes the deterministic delayed-controller squad-ping presentation failure under impairment. Manual rendered UI review, longer multiplayer soak, lower-tier performance, and broader navigation coverage remain open 1.0 gates.
## War Map Teardown and Rendered Presentation Evidence (2026-08-08)

The rendered UI gate exposed a real teardown-safety defect in the strategic War Map. During World Partition teardown, `UBHWarMapWidget::NativePaint()` could dereference a `TActorIterator` before its current actor was valid, producing an `Assertion failed: CurrentActor` crash. Both fortification-summary iterators now use the iterator's safe boolean state before dereferencing the current actor; the existing per-actor validity check remains in place.

- Initial failure: `Saved/Logs/BHValidation-RenderedUI-20260808-190315-WAR_MAP-1280x720-DEFAULT.log`, with the callstack ending in `Source/BrokenHorizon/Private/BHWarMapWidget.cpp`.
- Rendered UI correction: `Validate-BrokenHorizon.cmd -Build -RenderedUI` passed with exit 0. The default matrix passed HUD, briefing, pause, settings, remapping, War Map, and custom difficulty at 1280x720, 1920x1080, and 3840x2160, plus HUD/safe-area extremes and session-state cases. Summary: `Saved/Reports/BHValidation-RenderedUI-20260808-190528-Summary.json`.
- Pseudo-localization correction coverage: `Validate-BrokenHorizon.cmd -RenderedPseudoLocalization` passed with exit 0 for all seven high-risk UI states at 1280x720 and 1920x1080 in LEET, including War Map and War Map deployment. Summary: `Saved/Reports/BHValidation-RenderedPseudoLocalization-20260808-191356-Summary.json`.
- Performance gate: `Validate-BrokenHorizon.cmd -Performance` passed 600 frames with frame p95 `0.992 ms`, p99 `1.126 ms`, zero hitches, peak memory `2012 MB`, 95 actors, and 53 ticking actors. Summary: `Saved/Reports/BHValidation-Performance-Summary.json`.
- Release archive: `BHPackage-ReleaseCandidate-WarMapFix-20260808.log` completed BuildCookRun successfully and refreshed `Builds/FirstLight-ReleaseCandidate-Development/Windows`.
- Official packaged gate: `Validate-BrokenHorizon.cmd -Packaged -FirstLight -AudioFX` passed with exit 0. Audio/FX readiness remained required `18/18` and optional `6/27`; First Light navigation fallbacks remained `8/12`; packaged smoke passed against the refreshed archive. Log: `Saved/Logs/BHValidation-WarMapFix-Packaged-20260808.log`.
- Audio-enabled packaged First Light smoke: `Saved/Logs/BHReleaseCandidate-PackagedFirstLightAudio-WarMapFix-20260808.log` exited 0, initialized WASAPI at 48 kHz, logged `BH_AMBIENT_AUDIO_READY wind=1 rain=1 war=1 looping=1`, and recorded zero missing-object, missing-package, navmesh, fatal, unhandled-exception, assertion, or ensure-failure markers.

The War Map capture was visually stable after the fix. Manual presentation review is still open for the current development-graybox lighting/material presentation and a bottom-right ammo HUD label that sits against or clips at the viewport edge in the 1280x720 capture; those are presentation polish items, not part of this teardown-safety correction.

## Ammo HUD Safe-Area Evidence (2026-08-08)

The rendered HUD capture showed the weapon/ammunition block pressed against the lower-right viewport edge. The native `UBHAmmoHUDWidget` now applies a 32-point safe inset during `NativeConstruct`: canvas-backed text is anchored and aligned to the bottom-right with an auto-sized slot, while non-canvas layouts receive the same inset through render translation. This preserves existing Blueprint widget assets while preventing the functional ammo readout from clipping at common display edges.

- Native implementation: `Source/BrokenHorizon/Public/BHAmmoHUDWidget.h` and `Source/BrokenHorizon/Private/BHAmmoHUDWidget.cpp`.
- Build, automation, and startup smoke: `Validate-BrokenHorizon.cmd -Build -Tests -Smoke` passed with exit 0. Log: `Saved/Logs/BHValidation-AmmoHUDSafeArea-BuildTestsSmoke-20260808.log`.
- Rendered HUD coverage: the clean rendered run passed HUD, briefing, pause, settings, remapping, War Map, and custom difficulty at 1280x720, 1920x1080, and 3840x2160, plus HUD scale/safe-area profiles at 1280x720 and 3840x2160. The corrected 1280x720 capture is `Saved/Reports/BHValidation-RenderedUI-20260808-193225-HUD-1280x720-DEFAULT.png`; the ammo block is fully visible with a lower-right inset.
- Rendered gate limitation: the same run reached the session-state matrix and then timed out in the existing `SESSION_SEARCHING` fixture after the gameplay and HUD cases passed, leaving no validator-owned process after cleanup. The complete rendered matrix is therefore not marked green by this increment; the session-searching harness path remains an open release gate.
- Release archive: `BHPackage-ReleaseCandidate-AmmoHUDSafeArea-20260808.log` completed BuildCookRun successfully and refreshed `Builds/FirstLight-ReleaseCandidate-Development/Windows`.
- Official packaged gate: `Validate-BrokenHorizon.cmd -Packaged -FirstLight -AudioFX` passed with exit 0 against the refreshed archive. Required audio/FX assignments remained `18/18`, optional assignments `6/27`, First Light navigation fallbacks `8/12`, and packaged smoke passed. Log: `Saved/Logs/BHValidation-AmmoHUDSafeArea-Packaged-20260808.log`.
- Audio-enabled packaged First Light smoke: `Saved/Logs/BHReleaseCandidate-PackagedFirstLightAudio-AmmoHUDSafeArea-20260808.log` exited 0, initialized WASAPI at 48 kHz, logged `BH_AMBIENT_AUDIO_READY wind=1 rain=1 war=1 looping=1`, and recorded zero missing-object, missing-package, navmesh, fatal, unhandled-exception, assertion, or ensure-failure markers.

The HUD safe-area result is functionally ready for this increment. Manual presentation review remains open for the development-graybox lighting/material pass, and the session-searching rendered fixture must be repaired or rerun cleanly before the overall 1.0 presentation gate can be called complete.

## Deterministic Rendered-Evidence Gate (2026-08-08)

The rendered UI gate had a tooling-level race: timed-out Unreal child processes could write the shared screenshot filename after a newer case started, causing a valid 1280x720 session review to be evaluated as a stale 3840x2160 PNG. The release evidence path is now deterministic. The rendered harness gives each case a unique per-run/per-resolution screenshot path, waits for a valid PNG at the expected dimensions, retries the copy while the asynchronous screenshot writer releases the file, and recursively terminates descendants when a case times out. Runtime manual invocations retain their legacy screenshot filenames when no override is supplied.

- Validator/runtime implementation: `Scripts/Test-BrokenHorizonRenderedUI.ps1`, `Source/BrokenHorizon/Private/BHWarGameState.cpp`, and `Source/BrokenHorizon/Private/BHMainMenuGameMode.cpp`.
- Full rendered UI gate: `Validate-BrokenHorizon.cmd -Build -RenderedUI` passed with exit 0. All 33 cases passed: HUD, briefing, pause, settings, remapping, War Map, and custom difficulty at 1280x720, 1920x1080, and 3840x2160; HUD scale/safe-area extremes; and ready/searching/connected/error session states at 720p and 4K. Summary: `Saved/Reports/BHValidation-RenderedUI-20260808-194620-Summary.json`. Log: `Saved/Logs/BHValidation-RenderedUI-ProcessSafe-20260808.log`.
- Pseudo-localization gate: `Validate-BrokenHorizon.cmd -RenderedPseudoLocalization` passed with exit 0 for all 14 LEET high-risk UI cases at 1280x720 and 1920x1080. Summary: `Saved/Reports/BHValidation-RenderedPseudoLocalization-20260808-195403-Summary.json`. Log: `Saved/Logs/BHValidation-RenderedPseudoLocalization-ProcessSafe-20260808.log`.
- Release archive: `BHPackage-ReleaseCandidate-RenderedProcessSafe-20260808.log` completed BuildCookRun successfully and refreshed `Builds/FirstLight-ReleaseCandidate-Development/Windows`.
- Official packaged gate: `Validate-BrokenHorizon.cmd -Packaged -FirstLight -AudioFX` passed with exit 0 against the refreshed archive. Required audio/FX assignments remained `18/18`, optional assignments `6/27`, First Light navigation fallbacks `8/12`, and packaged smoke passed. Log: `Saved/Logs/BHValidation-RenderedProcessSafe-Packaged-20260808.log`.
- Audio-enabled packaged First Light smoke: `Saved/Logs/BHReleaseCandidate-PackagedFirstLightAudio-RenderedProcessSafe-20260808.log` exited 0, initialized WASAPI at 48 kHz, logged `BH_AMBIENT_AUDIO_READY wind=1 rain=1 war=1 looping=1`, and recorded zero missing-object, missing-package, navmesh, fatal, unhandled-exception, assertion, or ensure-failure markers.

The release-facing presentation evidence is now deterministic and complete for the tested Win64 development candidate. Human review remains open for the development-graybox lighting/material quality and overall interaction feel; those are content/polish gates, not rendered-harness failures.

## Renderer Budget and Cooperative Soak Evidence (2026-08-08)

The lower-tier renderer gates and sustained cooperative test now have current evidence. The full-world primitive counter produced one near-threshold sample at `1,039,154` p95 primitives against a `1,000,000` budget while frame/GPU/memory/streaming/navigation checks passed; an immediate clean repeat passed the complete world budget. This is retained as a machine-variance watch item rather than masking it by raising the budget.

- Rendered First Light performance passed: frame p95 `7.315 ms`, p99 `7.753 ms`, GPU p95 `6.370 ms`, render-thread p95 `7.293 ms`, zero hitches, 3283 MB peak memory, and zero PSO misses. Summary: `Saved/Reports/BHValidation-RenderedPerformance-Summary.json`.
- Rendered sector traversal performance passed: frame p95 `8.002 ms`, p99 `8.419 ms`, GPU p95 `7.341 ms`, render-thread p95 `8.002 ms`, zero hitches, 3395.3 MB peak memory, and five warm-cache PSO misses with zero hitch-associated misses. Summary: `Saved/Reports/BHValidation-RenderedTraversalPerformance-Summary.json`.
- Full-world traversal: the first capture reported `primitivesDrawnP95=1039154` and failed only that check; the immediate retry passed with frame p95 `8.026 ms`, GPU p95 `7.242 ms`, zero hitches, 5217 MB peak memory, 656 actors, 97 ticks, complete streaming recovery, and all eight navigation steps. Logs: `Saved/Logs/BHValidation-RenderedWorldPerformance-20260808.log` and `Saved/Logs/BHValidation-RenderedWorldPerformance-Retry-20260808.log`. Current summary: `Saved/Reports/BHValidation-RenderedWorldPerformance-Summary.json`.
- Sustained soak false negative: the initial ten-minute rendered two-client run passed all per-client renderer/network checks but ended ClientB at `569.8 s`, just below the unchanged `570.0 s` contract. `Scripts/Validate-BrokenHorizon.ps1` now requests `63000` capture frames instead of `62500`, providing a small timing buffer without relaxing the acceptance floor.
- Buffered cooperative soak passed: `Saved/Reports/BHValidation-RenderedMultiplayerSoak-20260808-201905-Summary.json` reports `sustainedSoakProof=true`, 62400 measured frames per client, ClientA/ClientB durations `581.5/577.3 s`, frame p95 `11.564/11.512 ms`, GPU p95 `8.854/8.793 ms`, zero frames over 50 ms, two connected/rendered clients, dedicated authority, 19 observed AI, and full war/combat/squad-ping replication proof. Log: `Saved/Logs/BHValidation-RenderedMultiplayerSoak-Buffered-20260808.log`.

Remaining release gates are the known limitations: this is a localhost ten-minute rendered soak rather than a two-hour packaged soak or geographic-network proof; optional audio cue coverage is `6/27`; non-Win64 SDKs are not installed; and manual review remains open for graybox lighting/material quality and interaction feel.
 
## Distant battlefield event audio increment - 2026-08-08
 
The First Light ambient-war event path now has deterministic fallback assets for three systemic distant cues: artillery impacts, aircraft overflight, and small-arms activity. ABHAmbientWarDirector resolves the three Blueprint-facing sound properties from /Game/BrokenHorizon/Audio, preserves Blueprint overrides, and emits a separate readiness marker so release logs distinguish the event cues from the existing weather and war-bed loops. The source generator is deterministic and creates stereo 48 kHz PCM WAV sources; the Unreal import remains narrow and idempotent.
 
Evidence:
 
- Validate-BrokenHorizon.cmd -Build -Tests -Smoke -FirstLight -Assets -AudioFX passed: editor build, automation, startup smoke, First Light smoke, asset audit, and audio/FX audit. Audio coverage is required=18/18, optional=9/27.
- Unreal imported and saved SW_FirstLight_DistantArtillery (4.50 s), SW_FirstLight_DistantAircraft (8.00 s), and SW_FirstLight_DistantSmallArms (2.60 s) without errors.
- BuildCookRun refreshed Builds/FirstLight-ReleaseCandidate-Development; cook, stage, IoStore, archive, and Windows Development output completed successfully.
- Validate-BrokenHorizon.cmd -Packaged -FirstLight -AudioFX passed. The cooked First Light log emitted BH_AMBIENT_AUDIO_READY wind=1 rain=1 war=1 looping=1 and BH_AMBIENT_EVENT_AUDIO_READY artillery=1 aircraft=1 small_arms=1.
- Audio-enabled packaged First Light smoke passed through WASAPI at 48 kHz with the same readiness markers and no fatal, assertion, or ensure markers.
 
Remaining audio gate: 18 optional slots remain, primarily enemy/guard barks and rifle/ambient presentation candidates. Player voice is outside the current project contract. Manual audio mix, spatial falloff, and combat-feel review remain required.
 
## Distant event spatial realism increment - 2026-08-08
 
The systemic distant-war event path now uses a native attenuation profile rather than uniform world-volume playback. The profile is a natural-sound spherical field with a 2,500 cm near radius, 10,000 cm falloff, stereo spatialization, air-absorption low-pass filtering from 18 kHz to 1.8 kHz, and lightweight occlusion at 35% volume with 0.15 s interpolation. This keeps the existing server-selected event location and multicast behavior while making artillery, aircraft, and small-arms activity respond to listener distance and cover.
 
Evidence:
 
- Validate-BrokenHorizon.cmd -Build -Tests -Smoke -FirstLight -AudioFX passed after the attenuation change: editor build, 66 automation tests, startup smoke, First Light route/navigation smoke, and audio/FX readiness. Audio coverage remained required=18/18, optional=9/27.
- BuildCookRun refreshed Builds/FirstLight-ReleaseCandidate-Development successfully.
- Validate-BrokenHorizon.cmd -Packaged -FirstLight -AudioFX passed. The cooked package emitted BH_AMBIENT_EVENT_ATTENUATION_READY model=natural_sound radius_cm=2500 falloff_cm=10000 lpf=1 occlusion=1 alongside all loop and event asset readiness markers.
- Audio-enabled packaged First Light smoke initialized WASAPI at 48 kHz and emitted the attenuation marker, with no fatal, assertion, or ensure markers.
 
Manual review remains required for subjective event loudness, spatial positioning, occlusion audibility, and the final combat mix on physical speakers/headphones.
