# Alpha progress

Updated: 2026-09-05. Repository evidence overrides this compact continuation record.
Current slice: combat HUD readability against bright First Light surfaces.
Status: **Verified** for the bounded combat HUD readability slice.
Physical input and full-alpha gates remain separate from this slice.

## Release boundary and preceding checkpoints

- Full alpha requires the complete campaign per
  [ModuleImplementationPlan.md](ModuleImplementationPlan.md), G4-G7.
  G1 Networked FirstLight is an earlier milestone; no release date is implied.
- Foundation `167893e` and same-process reconnect `9b4cd86` were pushed to
  `origin/codex/first-light-acceptance` before this slice.
- Reconnect acceptance retained one client PID/GameInstance/WarSubsystem through
  real LeaveSession -> menu -> host restart -> reconnect; all 18 assertions passed,
  including actual lower revision 4097 -> 2 on a new connection:
  `Saved/Logs/Codex/BH-Alpha-SameProcessReconnect-20260905-072735-cb2a86c5-Summary.json`.
  This was loopback acceptance with a fresh campaign, not crash/save recovery.
- The foundation/reconnect checkpoint passed 125 automation tests and packaged standalone
  First Light. Its package predates the current UI changes; current status is below.

## Current delivered behavior and evidence

- Medical/load text now has a padded, measured dark Charcoal backdrop. The latest
  correction measures width with active Slate scale so it spans the text at 4K;
  existing text, warning colors, wrapping, and bottom anchoring remain.
- The actual bound ammo text is retained inside a native border with safe anchors
  and immediate shared styling, adding contrast without replacing its binding.
- Final source correction build passed (29 seconds):
  `Saved/Logs/Codex/BuildEditor-20260905-103248.log`.
- The seven-case HUD run at 10:21:21 passed capture/assertion checks and retains
  ammo layout/contrast evidence. Visual review found a small 4K medical-width
  underestimate, corrected in the subsequent scale-aware measurement change.
- Final targeted medical rerenders passed at 720p default and 4K/75% HUD/80% safe area:
  `Saved/Reports/BH-Alpha-Readability-Medical-20260905-103421-Summary.json`.
  The coordinator inspected the full 720p frame and native 1:1 4K medical pixels;
  the full inventory line is inside the backdrop with padding.
- Scoped review cleared all three HUD files, including the final scale-aware
  measurement and Charcoal backdrop adjustment.
- Actual wounded Defense A multiplayer rerun passed (exit 0), with ten PNG
  assertions, both independent Continue acknowledgements, and no cleanup errors:
  `Saved/Logs/Codex/BH-Alpha-Readability-Medical-DefenseA-20260905-103702-1cc005a6-Summary.json`.
  The coordinator inspected the fresh ClientB/securing full frame: all seven real
  wounded lines fit the dark panel, red warnings remain intact, and ammo/tactical
  labels are clear. This does not claim visual inspection of all ten fresh frames.
- Consolidated visual review records seven matrix frames, final 720p/native 4K
  medical checks, selected multiplayer images, and remaining observations:
  `Saved/Reports/BH-Alpha-Readability-20260905-VisualReview.json`.
- Final `Tools/Validate.ps1 -RequireTests -SkipReview` passed (exit 0):
  Project Doctor reported zero errors/warnings and the up-to-date Editor build
  succeeded: `Saved/Logs/Codex/BuildEditor-20260905-103900.log`.
  Automation passed 127 tests (125 successes, 2 expected-warning successes), with
  zero failed/not-run/in-process:
  `Saved/Logs/Codex/AutomationReport-20260905-103902/index.json`.
- Fresh Development package passed (exit 0, 103.59 seconds):
  `Saved/Logs/Codex/PackageDevelopment-20260905-104035.log`.
  Archive: `Builds/Alpha-Development-20260905-Readability/Windows`;
  launcher `BrokenHorizon.exe`, runtime `BrokenHorizon/Binaries/Win64/BrokenHorizon.exe`.
- Packaged canonical First Light passed all four objectives with isolated saves:
  `Saved/Reports/BH-Alpha-Packaged-Readability-20260905-104326-FirstLight-Summary.json`.
  This NullRHI run proves the packaged route only.
- Packaged 1280x720 HUD capture passed native fixture and decoded-PNG assertions:
  `Saved/Reports/BH-Alpha-Packaged-Readability-20260905-104508-HUD720-Summary.json`
  and its matching PNG. The coordinator inspected the fresh full frame and accepted
  medical/ammo backing and layout. The scene is actual First Light with synthetic
  HUD data; this does not prove packaged multiplayer or physical input.
- The initial 10:43:26 HUD probe falsely failed because it required an optional
  engine trace. That failure was retained; only the local Saved probe was corrected,
  and the fresh 10:45:08 run passed. All owned processes closed cleanly.
- `Content/Python/repair_first_light_lighting.py` was prepared and reviewed, but
  automatic approval review denied the map-save launch. Specific user approval is
  pending; the script has not been executed or committed and
  `Content/BrokenHorizon/Maps/L_FirstLight_Graybox.umap` is unchanged.
- The sky remains black and the world overbright. Separately, authored health/stamina
  bars extend past the right edge at 150% HUD scale; investigate their slot/pivot
  ownership as a subsequent bounded correction.

## Preceding checkpoint: Defense A (`7de8bf3`)

The following behavior and evidence describe the preceding committed checkpoint,
not validation of the current HUD readability changes.

- Real two-client Defense A deployment runs authored first-wave activation,
  natural inter-wave delay, runtime second-wave spawning, physical securing,
  completion, actual HUD/debrief presentation, then Continue back to the War Map.
- Tactical objective text no longer overlaps generic objectives or health readouts;
  stale keyed strategic briefings are selectively cancelled at transitions.
- Tactical text stays fixed at the upper left while bearing chevrons remain
  separate. The observed wounded medical/load block fits above the bottom edge.
- Debrief body is bounded and readable with a separate Continue footer.
  Notification presentation is suppressed during debrief; unrelated queued alerts
  remain available to resume afterward.
- Activation notice describes the dynamic expected total waves (two in this fixture)
  rather than aging wave-one state. Continue exercises the widget delegate, save boundary, owner acknowledgement,
  and War Map transition. Physical input and scroll interaction remain untested.

## Verified preceding Defense A evidence

- Defense A source build passed (38.68 seconds):
  `Saved/Logs/Codex/BuildEditor-20260905-092709.log`.
- Focused notification coverage passed 3 tests, zero warnings/failures:
  `Saved/Logs/Codex/AutomationReport-20260905-091633/index.json`.
  Two new deterministic queue lifecycle tests cover this change.
- Final source/UI/fixture/script review, including the final two-file adjustment,
  found no supported blocking issue.
- Final rendered two-client lifecycle plus Continue passed:
  `Saved/Logs/Codex/BH-Alpha-DefenseA-Multiplayer-20260905-092832-e9e3e40a-Summary.json`.
  Actual HUD/debrief markers and both owner Continue acknowledgements passed;
  terminal waypoints hide on both clients. The fixture deliberately skips reload.
- The coordinator inspected all ten actual 1280x720 captures (five phases/client).
  Tactical text/chevrons clear health at both bearings, the observed medical/load
  block fits, stale overlays clear, and both debrief bodies/footers are readable:
  `Saved/Logs/Codex/BH-Alpha-DefenseA-Multiplayer-20260905-092832-e9e3e40a-VisualReview.json`.
  PNGs: `Saved/Automation/DefenseAMultiplayer/20260905-092832-e9e3e40a`.
- Separate standalone Defense A save/reload regression passed: reload retained
  five living guards/one casualty, second wave spawned two, terminal casualties eight:
  `Saved/Logs/BH-Alpha-DefenseA-UI-Standalone-20260905-0920-DefenseAGarrison.log`.
- Canonical First Light and navigation/grenade regression passed; this proves
  normal objective behavior, not its rendered appearance:
  `Saved/Logs/BH-Alpha-DefenseA-UI-FirstLight-20260905-0921-FirstLight.log` and
  `Saved/Logs/BH-Alpha-DefenseA-UI-FirstLight-20260905-0921-FirstLight-NavigationGrenade.log`.
- Final `Tools/Validate.ps1 -RequireTests -SkipReview` passed: 127 tests
  (125 successes, 2 expected-warning successes), zero failed/not-run/in-process:
  `Saved/Logs/Codex/AutomationReport-20260905-093207/index.json`.
  Project Doctor: zero errors/warnings; final build current:
  `Saved/Logs/Codex/BuildEditor-20260905-093205.log`.
- Fresh Development package passed (exit 0, 171.01 seconds):
  `Saved/Logs/Codex/PackageDevelopment-20260905-093342.log`.
  Archive: `Builds/Alpha-Development-20260905-DefenseA/Windows`;
  launcher `BrokenHorizon.exe`, runtime `BrokenHorizon/Binaries/Win64/BrokenHorizon.exe`.
- Packaged canonical First Light standalone smoke passed (exit 0): four objectives,
  one player completed, no failure markers or cleanup errors, reconnect/Defense A
  multiplayer helpers inactive, isolated UserDir and unique saves verified:
  `Saved/Logs/BH-Alpha-Packaged-FirstLight-DefenseA-20260905-093756-Summary.json`.
  NullRHI proves packaged startup/route only, not packaged rendering or multiplayer.

## Remaining full-alpha gates

| Gate | Evidence boundary and remaining work |
| --- | --- |
| Code/unit | Current HUD build, scoped review, and final 127-test suite pass. Preceding Defense A passed 127 tests. |
| Live network | Two-client tactical lifecycle/Continue passes under controlled fixture damage/positioning and natural director clocks. Remote networking, campaign crash/save recovery, and soak remain. |
| Rendered/player | Current targeted medical rerenders and wounded runtime rerun pass; selected fresh multiplayer images inspected. Prior Defense A inspected ten 1280x720 frames. Actual Enter/M Continue, scrolling, combat feel, localization, and broader safe-area coverage remain. |
| Presentation/performance | HUD contrast correction built and targeted static visuals inspected. Black sky/overbright world and 150% health/stamina edge overflow remain. Lighting map-save launch was denied by automatic approval review. Earlier local two-client VRAM warnings do not constitute performance acceptance. |
| Campaign/content | Complete campaign and authored-content gates remain; this tactical acceptance slice is not full alpha. |
| Package | Fresh Readability package, packaged four-objective First Light route, and visually inspected 720p HUD pass. Packaged multiplayer and physical play remain open. |

## Current task files

- `Source/BrokenHorizon/Private/BHCombatStatusWidget.cpp`
- `Source/BrokenHorizon/Private/BHAmmoHUDWidget.cpp`
- `Source/BrokenHorizon/Public/BHAmmoHUDWidget.h`
- `Docs/Production/Alpha_Progress.md`

The prepared lighting repair script is outside the completed HUD delivery until
its map-authoring approval and execution evidence are resolved.

## Reproduction and continuation

Use `powershell.exe -NoProfile -ExecutionPolicy Bypass -File` from the project root:

- `Tools/BuildEditor.ps1`
- `Scripts/Test-BrokenHorizonRenderedUI.ps1` (use the scoped HUD case filter)
- `Scripts/Validate-BrokenHorizon.ps1 -DefenseAGarrison`
- `Scripts/Test-BrokenHorizonDefenseAMultiplayer.ps1 -Rendered`
- `Tools/Validate.ps1 -RequireTests -SkipReview`

Read this record and relevant ownership docs, inspect the current diff and named
reports, then select one playable slice. Keep one writer per bounded file set,
preserve unrelated work, and follow standing automatic commit/push delivery after
validation. Keep code, network, rendered, content, and package evidence distinct.
