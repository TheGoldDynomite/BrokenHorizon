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
