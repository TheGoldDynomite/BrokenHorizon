# Alpha progress

Updated: 2026-09-05. Repository evidence overrides this compact continuation record.
Current slice: Defense A multiplayer lifecycle and presentation corrections.
Status: **Verified** for bounded Defense A lifecycle/rendered acceptance.
Physical input and full-alpha gates remain separate from this verified slice.

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
- The preceding checkpoint passed 125 automation tests and packaged standalone
  First Light. Its package predates the current UI changes; current status is below.

## Current delivered behavior

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

## Verified current evidence

- Latest source build passed (38.68 seconds):
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
| Code/unit | Build, focused tests, final 127-test suite, and scoped review pass. |
| Live network | Two-client tactical lifecycle/Continue passes under controlled fixture damage/positioning and natural director clocks. Remote networking, campaign crash/save recovery, and soak remain. |
| Rendered/player | Ten actual 1280x720 frames inspected. Two-player Listen Server: use actual Enter/M Continue and scroll if needed. Combat feel and resolution/localization/safe-area coverage remain. |
| Presentation/performance | Graybox lighting is overbright and baseline white HUD contrast remains limited. Earlier local two-client runs reported VRAM budget warnings; no performance acceptance. |
| Campaign/content | Complete campaign and authored-content gates remain; this tactical acceptance slice is not full alpha. |
| Package | Fresh package creation and standalone First Light route pass. Packaged multiplayer and rendered play remain open. |

## Current task files

- `Source/BrokenHorizon/BHCharacter.cpp`
- `Source/BrokenHorizon/BHCharacter.h`
- `Source/BrokenHorizon/Private/BHCombatStatusWidget.cpp`
- `Source/BrokenHorizon/Private/BHMissionCompleteWidget.cpp`
- `Source/BrokenHorizon/Public/BHMissionCompleteWidget.h`
- `Source/BrokenHorizon/Private/BHObjectiveNotificationWidget.cpp`
- `Source/BrokenHorizon/Public/BHObjectiveNotificationWidget.h`
- `Source/BrokenHorizon/Private/BHOpenWorldOperationDirector.cpp`
- `Source/BrokenHorizon/Private/BHWarGameState.cpp`
- `Source/BrokenHorizon/Private/BHDefenseAMultiplayerTest.cpp`
- `Source/BrokenHorizon/Private/BHDefenseAMultiplayerTest.h`
- `Source/BrokenHorizon/Private/Tests/BHNotificationLifecycleTests.cpp`
- `Source/BrokenHorizon/Private/Tests/BHNotificationTestWidget.h`
- `Scripts/Test-BrokenHorizonDefenseAMultiplayer.ps1`
- `Docs/Production/Alpha_Progress.md`

## Reproduction and continuation

Use `powershell.exe -NoProfile -ExecutionPolicy Bypass -File` from the project root:

- `Tools/BuildEditor.ps1`
- `Tools/RunTests.ps1 -TestFilter BrokenHorizon.UI.Notification`
- `Scripts/Validate-BrokenHorizon.ps1 -DefenseAGarrison`
- `Scripts/Test-BrokenHorizonDefenseAMultiplayer.ps1 -Rendered`
- `Tools/Validate.ps1 -RequireTests -SkipReview`

Read this record and relevant ownership docs, inspect the current diff and named
reports, then select one playable slice. Keep one writer per bounded file set,
preserve unrelated work, and follow standing automatic commit/push delivery after
validation. Keep code, network, rendered, content, and package evidence distinct.
