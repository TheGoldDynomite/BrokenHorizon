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
