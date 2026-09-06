# Alpha progress

Updated: 2026-09-05. Repository evidence overrides this compact continuation record.
Completed slice: A1/Q2 health/stamina layout at 150% HUD scale.
Status: **Verified** for bounded Q2 HUD layout.
Delivered commit: `58d2380f524dff9812e204d42222d2d0d97f11b9`; pushed to
`origin/codex/first-light-acceptance` and remote SHA verified.
Physical input and full-alpha gates remain separate from this slice.

A1 development has explicitly resumed. [Alpha Roadmap](Alpha_Roadmap.md) owns the
G5 alpha order and gates; Q2 is complete. Packaged First Light functional runner
coverage is now in progress, with fresh validation pending. A0/A1/G2 remain
open. The earlier 127-test suite and Readability package below are prior-checkpoint proof,
not fresh validation of this layout work.

Q1 lighting remains approval-blocked: automatic approval review rejected the normal
escalated map-save launch after resumption, before any process started. Explicit
map-edit approval has been requested; the map is unchanged and script unexecuted.
Computer Use initialization, retry, and clean-reset third attempt all failed before
game input with `windows sandbox failed: helper_unknown_error: apply deny-read ACLs`.
No custom input workaround was used. Actual controls/human feel remain open;
automated source, rendered, and packaged checks can proceed. Current Q2 proof and remaining gates are below.

Draft scope artifact: [Alpha Manifest](Alpha_Manifest.md) combines content candidates
and proposed support coverage. It is unaccepted; A0 remains open.

## Active A1 task: packaged First Light functional coverage

- **Player outcome:** demonstrate direct-loopback First Light objective, loot, ammo,
  and rejoin state using packaged clients while retaining the Editor default.
- **Planned matrix:** fully packaged listen host plus one remote client; separately,
  a hybrid Editor dedicated server plus two packaged remote clients. The hybrid
  case is not proof of a packaged dedicated server.
- **Writer scope:** existing First Light multiplayer runner and
  `BHWarGameState.cpp/.h`, covering nonshipping readiness, standalone exit, and
  owner markers. One production writer owns this source/tooling work.
- **Status/proof:** implementation in progress; fresh matrix and final validation
  pending. Neither planned configuration is accepted yet; this runner does not prove
  session-menu flow or debrief acknowledgements.
- **Open dependencies:** Q1 explicit map approval, actual controls, full G2/A0,
  performance, and content acceptance remain open. Q2 evidence below is retained.

## Verified Q2 delivery and evidence

- Opt-in live probes demonstrate that the actual four vitals bindings remain grouped
  and the 150% right-edge overflow is fixed. Safe-area settings initialize before
  Slate construction; actual 80% bounds were checked.
- Strategic and field status stack below vitals. Long field context wraps and grows
  its background height; the transport label avoids the vitals block.
- Evidence chain under `Saved/Reports` (each prefix has `-Summary.json`):
  `BH-Q2-LiveVitals-20260905-112720`,
  `BH-Q2-GroupedVitals-20260905-113551`,
  `BH-Q2-LifecycleVitals-20260905-114140`,
  `BH-Q2-Column-20260905-115018`, and
  `BH-Q2-TextFollowup-20260905-115915`.
- The coordinator inspected the full 720p text-followup frame and native 4K
  column/transport crops; no blocking text occlusion was observed in those views.
  The test engineer independently checked widget identity/value/geometry earlier.
- Final scoped review cleared HUD diagnostic removal and the Defense A fixture
  readiness correction in `BHWarGameState.cpp`: phase 4 waits for
  `Snapshot.Phase == AwaitingWave`. The fixture edit is two insertions/one deletion.
- Failed Defense A runs `120841-e48a280b` and `121017-31770f76` are preserved.
  The exact corrected rendered run passed with ten PNG assertions, both owner
  Continue acknowledgements, two checkpoint confirmations, and no cleanup errors:
  `Saved/Logs/Codex/BH-A1-Vitals-DefenseA-20260905-192648-7e37cdce-Summary.json`.
  The coordinator inspected all ten fresh full frames. Native ammo crops of ClientA
  wave 2/securing and ClientB securing confirmed complete STABLE text with right
  padding; apparent clipping in the full-frame display was not present in those
  native pixels. Vitals, state, medical, and debrief are clear for this bounded scope.
- Final `Tools/Validate.ps1 -RequireTests -SkipReview` passed. Project Doctor:
  zero errors/warnings; Editor build:
  `Saved/Logs/Codex/BuildEditor-20260905-192833.log`.
  Automation: 125 successes plus two expected-warning successes, zero
  failed/not-run/in-process (127 total):
  `Saved/Logs/Codex/AutomationReport-20260905-192834/index.json`.
- All seven normal HUD frames passed and were inspected by the coordinator:
  `Saved/Reports/BH-A1-FinalHUD-20260905-120509-Summary.json`.
  This evidence remains valid after the fixture-only readiness adjustment.
- Canonical First Light and navigation/grenade regression passed under log prefix
  `BH-A1-Vitals-FirstLight-20260905-1208`, with zero of twelve navigation fallbacks.
- Fresh Vitals Development package passed (exit 0, 84.05 seconds):
  `Saved/Logs/Codex/PackageDevelopment-20260905-192943.log`;
  archive `Builds/Alpha-Development-20260905-Vitals/Windows`.
- Packaged First Light passed all four objectives:
  `Saved/Reports/BH-A1-Packaged-Vitals-20260905-193217-FirstLight-Summary.json`.
  This NullRHI run proves the packaged route only.
- Packaged HUD at 1280x720, 150% scale, and 100% safe area passed native fixture
  and decoded-PNG assertions:
  `Saved/Reports/BH-A1-Packaged-Vitals-20260905-193217-HUD150-Summary.json`
  and its matching PNG. The coordinator inspected the actual frame and accepted
  bars, strategic, medical, and ammo layout. This is the actual First Light scene
  with synthetic HUD data, not packaged multiplayer or physical-input proof.
- Packaged runs reported no cleanup errors; all owned processes ended.
- Consolidated final visual review records seven normal HUD frames, ten fresh
  multiplayer frames, the packaged 150% HUD frame, native ammo-crop confirmation,
  and evidence limits:
  `Saved/Reports/BH-A1-Vitals-20260905-FinalVisualReview.json`.
  Independent review cleared the apparent ammo concern; no further fix or rerun
  was required.

| Final Q2 gate | Current evidence boundary |
| --- | --- |
| Stable source build and full automation | Passed: Doctor 0/0, build 192833, 127 tests at 192834. |
| Normal HUD matrix | Seven normal frames passed and visually inspected. |
| First Light and Defense A regression | Passed; all ten fresh Defense A frames and selected native ammo crops inspected. |
| Fresh package and packaged acceptance | Vitals package, four-objective route, and inspected 720p HUD150 capture passed. |
| Final scoped review | Passed, including diagnostic removal and fixture readiness delta. |

Q2 is Verified within this bounded HUD scope. A0/A1/G2 remain open for actual controls, packaged
listen/dedicated functional proof, performance acceptance, and the remaining
quality/content gates; a HUD correction cannot close those requirements.

## Release boundary and preceding checkpoints

- The alpha target is canonical G5 Alpha / M6: a complete playable limited-region
  resistance campaign, ordered by [Alpha Roadmap](Alpha_Roadmap.md);
  [ModuleImplementationPlan.md](ModuleImplementationPlan.md) retains the detailed backlog.
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

## Prior checkpoint: delivered HUD behavior and evidence

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
- At this prior checkpoint, authored health/stamina bars extended past the right
  edge at 150% HUD scale. The verified Q2 correction above resolves that recorded
  defect. Black sky/overbright world remains the separate Q1 lighting issue.

## Preceding checkpoint: Defense A (`7de8bf3`)

The following behavior and evidence describe the preceding committed checkpoint,
not validation of the later HUD readability or active A1/Q2 layout changes.

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
| Code/unit | Prior HUD checkpoint build/review/127-test suite pass. Q2 final source build/review/127-test suite and bounded package acceptance pass. |
| Live network | Two-client tactical lifecycle/Continue passes under controlled fixture damage/positioning and natural director clocks. Remote networking, campaign crash/save recovery, and soak remain. |
| Rendered/player | Prior targeted medical rerenders and wounded runtime rerun pass; selected fresh multiplayer images inspected. Prior Defense A inspected ten 1280x720 frames. Actual Enter/M Continue, scrolling, combat feel, localization, and broader safe-area coverage remain. |
| Presentation/performance | HUD contrast correction built and targeted static visuals inspected. Black sky/overbright world remains; Q2 150% vitals correction is verified for the recorded cases. Lighting map-save launch was denied by automatic approval review. Earlier local two-client VRAM warnings do not constitute performance acceptance. |
| Campaign/content | Complete campaign and authored-content gates remain; this tactical acceptance slice is not full alpha. |
| Package | Current Vitals package, four-objective First Light route, and inspected 720p HUD150 pass. Packaged multiplayer and physical play remain open. |

## Current Q2 delivery files

- `Source/BrokenHorizon/Private/BHCombatStatusWidget.cpp`
- `Source/BrokenHorizon/Public/BHCombatStatusWidget.h`
- `Source/BrokenHorizon/Private/BHWarGameState.cpp`
- `Docs/Production/Alpha_Progress.md`
- `Docs/Production/Alpha_Roadmap.md`

## Prior HUD checkpoint files

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
