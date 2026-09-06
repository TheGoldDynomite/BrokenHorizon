# Alpha progress

Updated: 2026-09-05. Repository evidence overrides this compact continuation record.
Completed slice: bounded A1 session-menu failure recovery.
Status: **Verified** for bounded session recovery; intermittent reload reliability remains open.
Performance commit: `ae20a5238db84418836419dff2aa06a3776be118`; pushed and remote SHA verified.
Topology commit: `1df892b9e9c2b85929ae29cb586fdf3191790e96`; pushed and remote SHA verified.
Prior Q2 commit: `58d2380f524dff9812e204d42222d2d0d97f11b9`; pushed to
`origin/codex/first-light-acceptance` and remote SHA verified.
Physical input and full-alpha gates remain separate from this slice.

A1 development has explicitly resumed. [Alpha Roadmap](Alpha_Roadmap.md) owns the
G5 alpha order and gates; Q2 and the bounded topology matrix are verified.
The validated topology delivery is tracked by active development-branch history. A0/A1/G2 remain
open. The earlier 127-test suite and Readability package below are prior-checkpoint proof,
separate from the completed bounded session-recovery proof.

Q1 lighting remains approval-blocked: automatic approval review rejected the normal
escalated map-save launch after resumption, before any process started. Explicit
map-edit approval has been requested; the map is unchanged and script unexecuted.
Computer Use initialization, retry, and clean-reset third attempt all failed before
game input with `windows sandbox failed: helper_unknown_error: apply deny-read ACLs`.
No custom input workaround was used. Actual controls/human feel remain open;
automated source, rendered, and packaged checks can proceed. Current Q2 proof and remaining gates are below.

Draft scope artifact: [Alpha Manifest](Alpha_Manifest.md) combines content candidates
and proposed support coverage. Draft commit `4470616664ee517c117c0d7d5c6671ac80ab8715`
was pushed and its remote SHA verified. The working-scope question is unanswered;
A0 and all candidate decisions remain unaccepted.

## Verified bounded A1 session recovery

- **Implemented source scope:** recover rejected travel and owned asynchronous
  failures through existing API/Error state; reset pending intent and clean the old
  named session before LAN Join retry. Four-file review is clear; inputs are frozen.
- Editor build passed in 28.83 seconds:
  `Saved/Logs/Codex/BuildEditor-20260905-214100.log`.
  Six targeted tests passed (four successes, two warning-successes; zero
  failed/not-run/in-process):
  `Saved/Logs/Codex/AutomationReport-20260905-214147/index.json`.
  These are five new registered tests plus Contract. Warnings are the scoped tests'
  expected travel/network-failure messages, independently checked by the coordinator.
- Actual-AST runner parser checks passed 59/59:
  `Saved/Automation/SessionRecoveryParser/20260905-214529-8b509158/Summary.json`.
  Frozen runner SHA256:
  `EEBCFD615C5FB0A6EE3C7132BC5324CDF5EA7F179C4E38EA47E1CA2475855710`.
- Editor fixture passed; coordinator read the entire summary:
  `Saved/Automation/SessionRecovery/20260905-214812-15bec943/BH-A1-SessionRecovery-Editor-20260905-2147-Summary.json`.
  Same-machine LAN discovery succeeded at port 8791; local reports retain both
  resolved connect strings. No-match Join reached Error with actionable
  buttons; actual `ServerTravel(false)` retained the named session, then the same
  host retried through New Game. Host New Game/LAN Join succeeded before real owned
  host termination caused ConnectionTimeout (about 60 seconds). Client recovered
  to Error/nonpending with retained named session and actionable menu. Its new
  widget showed ACTION FAILED and generic Host/Join instructions, not the original
  specific failure detail. Restarted
  host plus direct Join retry succeeded without hidden Leave.
- Client PID `23220` and GameInstance `54590:4560` persisted across recovery;
  connection identity changed `59211:5934` -> `55617:8883`. Each host had two
  participants/one remote; cleanup-error array was empty.
- The fixture exercised actual enabled, visible, bound `UButton.OnClicked` events,
  not ProcessEvent or physical clicks. Host used configured `L_BrokenHorizon_World`
  and menu `L_MainMenu`; no map asset edit. No new pixels, Continue/save-restoration,
  or internet acceptance is claimed. Save completion remains a separate gate.
- Fresh SessionRecovery package passed (exit 0, 98.73 seconds):
  `Saved/Logs/Codex/PackageDevelopment-20260905-215056.log`;
  archive `Builds/Alpha-Development-20260905-SessionRecovery/Windows`; executable SHA256
  `5BCCD322394F764AF4F8F2C61C9E8DBD5207A9B37AE7C69A7EA1FF39FCF5EBE6`.
  Packaged recovery passed:
  `Saved/Automation/SessionRecovery/20260905-215311-afd8140d/BH-A1-SessionRecovery-Packaged-20260905-2153-Summary.json`.
- **Unresolved regression:** SameProcessReconnect crashed in the retained client
  during the second First Light load:
  `Saved/Logs/Codex/BH-A1-SessionRecovery-Reconnect-20260905-2155-20260905-215522-1501fd76-Summary.json`.
  Crash artifacts: `Saved/Crashes/UECC-Windows-4CFC9FFE4C1A258EE88FD5AF3C9DA6AD_0000`.
  The matching `-RetainedClient.log`, line 1694, records
  `EXCEPTION_ILLEGAL_INSTRUCTION`. This remains an **open intermittent First Light
  reload reliability finding**. No supported cause or hardware attribution exists.
  A passing retry does not resolve the first failure.
- The exact unchanged SameProcessReconnect retry passed all 18 assertions;
  coordinator audited:
  `Saved/Logs/Codex/BH-A1-SessionRecovery-Reconnect-20260905-2155-20260905-220141-2f759b98-Summary.json`.
  Client PID `18252`, GameInstance `54590:4560`, and WarSubsystem `54720:26691`
  were retained; connection changed `57532:24486` -> `61442:27709`, revision
  `4097` -> `2`, and host/client converged. Cleanup-error array was empty.
- Final `Tools/Validate.ps1 -RequireTests -SkipReview` passed; coordinator independently
  read build/test evidence. Up-to-date Editor build completed in 0.89 seconds:
  `Saved/Logs/Codex/BuildEditor-20260905-220251.log`.
  Full automation: 128 successes plus four warning-successes (132 total), zero
  failed/not-run/in-process:
  `Saved/Logs/Codex/AutomationReport-20260905-220252/index.json`.
  No owned engine/build/game processes remain. Final scoped review found no supported
  blocking defect in the unchanged four files. The first failure occurred after eight
  assertions; the unchanged retry passed all 18. The reviewer cannot conclude that
  the crash is unrelated or resolved. The 132-test gate closes bounded recovery
  validation while the intermittent map-reload reliability finding remains open.
- Recovery is Verified within this bounded scope; delivery is tracked by scoped
  commit history on the active development branch. Full A1/G2 and the intermittent
  reload finding remain open. The next independent source task is Continue
  accepted-versus-applied completion; save restoration and physical-input acceptance
  remain separate. A successful unchanged retry does not close reload reliability.

Packaged dedicated-server distribution has an external requirement: the architect
verified installed UE 5.8 supports Editor/Game configurations only (`BaseEngine.ini`
3996-4007), and UBT rejects a Server target (1395-1403). It requires an approved
server-capable source/custom engine installation. No Server build was attempted and
no engine metadata workaround was applied. Existing hybrid proof remains valid.
Q1 map approval, unanswered A0 scope, and actual Computer Use acceptance remain open.

## Verified performance runner and measured budget failures

- Script scope is `Scripts/Test-BrokenHorizonPerformance.ps1`; frozen SHA256:
  `F1961CA66E8E46EB27B8544C7250329512003C9EDB876ACA5E204F36AA1D97BC`.
- Actual-AST/real-CSV parser checks passed 45/45, including negative cases:
  `Saved/Automation/PerformanceBaseline/20260905-210053-c62984e1/Summary.json`.
- Same local hardware: i9-14900K / RTX5060Ti, driver 616.64, Windows 11/D3D12.
  Observed settings were stable and identical across all three runs: 1920x1080,
  all quality groups 3, screen percentage 100%, VSync 0, MaxFPS 0. Requested viewport
  and caps matched; quality groups 3 were observed defaults, not explicitly requested.
  First Light traversal used eight steps/two loops. Runs use unique UserDirs and
  owned CSVs; boot capture/180-frame prefix does not establish cache warmth.

| Run | Capture / sample boundary | Frame p95 / p99 | GPU p95 / p99 | Other observations | Budget result |
| --- | --- | --- | --- | --- | --- |
| Editor compatibility | Valid; 1800 numeric frames, 180-frame prefix; diagnostic rows 175/1795 (one eligible excluded); 1619 measured, other exclusions 0 | 9.063 / 9.718 ms | 7.610 / 7.904 ms | Render-thread p95 9.044 ms; peak RAM 3226.3 MB; peak VRAM 3051.6 MB (42.8% runtime budget); no >33 ms frame hitches | Failed three unchanged provisional budgets; overall false, exit 1 |
| Packaged first | Valid; 1800 numeric, 180-prefix, one eligible diagnostic exclusion, 1619 measured | 8.503 / 9.127 ms | 7.766 / 8.176 ms | Render-thread p95 8.481 ms; peak RAM 1699.5 MB; VRAM 3084.1 MB (43.3% runtime budget) | Failed primitives p95 1,009,635 > 1,000,000; overall false, script exit 1 |
| Packaged repeat, same configuration | Valid; 1800 numeric, 180-prefix, one eligible diagnostic exclusion, 1619 measured | 8.530 / 9.192 ms | 7.760 / 8.133 ms | Render-thread p95 8.527 ms; peak RAM 1642.6 MB; VRAM 3097.3 MB (43.5% runtime budget) | Failed primitives p95 1,001,301 > 1,000,000; overall false, script exit 1 |

Editor report (independently read by coordinator):
`Saved/Automation/Performance/20260905-210216-f3bc8f88/BH-A1-Perf-Editor-20260905-2057-Summary.json`.
Capture validity and budget acceptance are separate: primitives p95 1,065,869 exceeds
1,000,000; pending-stream p95 0.062 exceeds 0; pending-stream frames 6.92% exceeds 5%.
Final texture percentage was 100; owned-runtime cleanup completed.

Packaged reports (both independently audited by coordinator):
`Saved/Automation/Performance/20260905-210331-688f86b3/BH-A1-Perf-Packaged-First-20260905-2103-Summary.json`
and
`Saved/Automation/Performance/20260905-210431-11e9298a/BH-A1-Perf-Packaged-Repeat-20260905-2104-Summary.json`.
Both use the FirstLightTopology Windows archive and actual executable SHA256
`CCF9E88A5ED65F7373BAB7D5551F6FFF592560CAB670261AF268F017F5589F45`.
Both have pending-stream p95 0 and pending-frame percentage 4.32% (pass), final
texture percentage 100, no >33 ms frame hitches, and completed owned cleanup.
Their script exit 1 is solely a primitives-budget failure. Native process exit -1
from an owned cleanup stop is distinct from that script result.

Earlier failures are preserved: a prelaunch queryFrames PowerShell-precedence error
(no game launched), and a real footer-parse failure under
`Saved/Automation/Performance/20260905-205554-f0f6817d`. Both were corrected,
tested, and reviewed. Offline recovery of the old CSV is not a full-run pass.

The runner is Verified for bounded measurement; all three overall budget failures
remain recorded. Frozen-hash final review and 45 parser cases, including real CSV,
are clear. Final `Tools/Validate.ps1 -RequireTests -SkipReview` passed (exit 0):
Doctor zero errors/warnings; up-to-date Editor build in 0.96 seconds:
`Saved/Logs/Codex/BuildEditor-20260905-210539.log`.
Automation passed 125 successes plus two warning-successes (127 total), zero
failed/not-run/in-process:
`Saved/Logs/Codex/AutomationReport-20260905-210540/index.json`.
The coordinator independently read all three performance JSONs and final build/test
evidence. Script SHA256 remained unchanged; all sessions terminated, no owned
engine/build processes remain, and execution was released.

No C++/asset or new-package change was made in this slice. No overall performance
acceptance, FPS promise, or comparison with incompatible August runs is claimed.
A1/S07 performance acceptance, A0 scope, Q1 map approval, and actual controls remain open.

Exact measurements used these commands from the repository root:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\Scripts\Test-BrokenHorizonPerformance.ps1 -Rendered -RenderOffscreen -Traversal -CaptureFrames 1800 -WarmupFrames 180 -LogPrefix BH-A1-Perf-Editor-20260905-2057
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\Scripts\Test-BrokenHorizonPerformance.ps1 -Rendered -RenderOffscreen -Traversal -CaptureFrames 1800 -WarmupFrames 180 -LogPrefix BH-A1-Perf-Packaged-First-20260905-2103 -Runtime Packaged -PackageRoot Builds/Alpha-Development-20260905-FirstLightTopology/Windows
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\Scripts\Test-BrokenHorizonPerformance.ps1 -Rendered -RenderOffscreen -Traversal -CaptureFrames 1800 -WarmupFrames 180 -LogPrefix BH-A1-Perf-Packaged-Repeat-20260905-2104 -Runtime Packaged -PackageRoot Builds/Alpha-Development-20260905-FirstLightTopology/Windows
```

## Verified A1 First Light topology coverage

- **Delivered behavior:** direct-loopback First Light objective, loot, ammo, and
  fresh-remote rejoin proof across the preserved Editor default, hybrid Editor
  dedicated server with packaged remotes, and fully packaged listen configuration.
- Four source/tooling files include guarded nonshipping diagnostics in
  `BHWeaponComponent.cpp`; no new gameplay API, RPC, or asset change was introduced.
- Final validation passed: Project Doctor zero errors/warnings, Editor build
  `Saved/Logs/Codex/BuildEditor-20260905-201540.log`, and 127 tests (125 successes,
  two expected-warning successes, zero failed/not-run/in-process):
  `Saved/Logs/Codex/AutomationReport-20260905-201542/index.json`.
- Fresh package passed in 98.86 seconds:
  `Saved/Logs/Codex/PackageDevelopment-20260905-201649.log`;
  archive `Builds/Alpha-Development-20260905-FirstLightTopology/Windows`.
- Positive reports under `Saved/Logs`:
  `BH-A1-FirstLight-Editor-20260905-200942-76e70e9e-Summary.json`,
  `BH-A1-FirstLight-Hybrid-20260905-201943-70c3f786-Summary.json`, and
  `BH-A1-FirstLight-Listen-20260905-202144-6912d1ac-Summary.json`.
  Each records four objectives/two completed players and exact GUID convergence
  after fresh remote rejoin: six spawned minus one consumed equals five remaining.
- Default/hybrid selected remote ID 256 and observed real `OnRep` after the server
  transaction, with synchronous actual HUD-widget text `30 / 180`. Listen selected
  local-authority Host and its actual local HUD; no remote `OnRep` proof is claimed
  for that selected listen observer. The coordinator independently inspected all
  summaries and the raw hybrid owner/OnRep/HUD block.
- Fresh packaged standalone omitted ExpectedPlayers, completed four objectives for
  one player, and exited normally with code 0:
  `Saved/Reports/BH-A1-Packaged-FirstLightTopology-20260905-202304-Summary.json`.
- Eight negative cases passed expected reason/no-route assertions:
  `Saved/Automation/FirstLightTopology/Negatives/20260905-201145-1600c1e1/Summary.json`.
  All cleanup-error arrays are empty; no owned live processes remain and execution
  was released. Final scoped source review is clear.
- **Evidence limits:** NullRHI, same-machine direct-loopback functional/widget state.
  No new rendered pixels, physical input, session-menu/debrief flow, remote internet,
  or packaged dedicated-server executable proof. Optional inventory-transfer and
  weapon-role modes were not rerun. Prior Q2 visual proof remains separate below.
- **Open gates:** unanswered scope question/A0, full A1/G2, actual controls,
  performance, content, and Q1 explicit map approval remain open. A passing bounded
  matrix does not accept proposed support promises.

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

Q2 is Verified within its prior visual/HUD scope. The current topology evidence
above separately proves bounded packaged listen and hybrid dedicated functionality.
A0/A1/G2 remain open for wider promised configurations, remote/menu/physical-input
acceptance, performance, and quality/content; neither bounded result closes those gates.

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
| Package | Current FirstLightTopology package passes standalone and bounded two-player loopback configurations: fully packaged listen and hybrid Editor dedicated with packaged clients. Wider promised configurations, remote/menu/physical acceptance remain open; Q2 rendered proof is separate. |

## Current session-recovery delivery files

- `Source/BrokenHorizon/Private/BHSessionSubsystem.cpp`
- `Source/BrokenHorizon/Public/BHSessionSubsystem.h`
- `Source/BrokenHorizon/Private/Tests/BHSessionAutomationTests.cpp`
- `Scripts/Test-BrokenHorizonSessionRecovery.ps1`
- `Docs/Production/Alpha_Progress.md`
- `Docs/Production/Alpha_Roadmap.md`
- `Docs/Production/Alpha_Manifest.md`

## Prior performance delivery files

- `Scripts/Test-BrokenHorizonPerformance.ps1`
- `Docs/Production/Alpha_Progress.md`
- `Docs/Production/Alpha_Roadmap.md`
- `Docs/Production/Alpha_Manifest.md`

## Prior topology delivery files

- `Scripts/Test-BrokenHorizonFirstLightMultiplayer.ps1`
- `Source/BrokenHorizon/Private/BHWarGameState.cpp`
- `Source/BrokenHorizon/Public/BHWarGameState.h`
- `Source/BrokenHorizon/Private/BHWeaponComponent.cpp`
- `Docs/Production/Alpha_Progress.md`
- `Docs/Production/Alpha_Roadmap.md`
- `Docs/Production/Alpha_Manifest.md`

## Prior Q2 delivery files

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
- `Scripts/Test-BrokenHorizonFirstLightMultiplayer.ps1` (preserved Editor default)
- `Scripts/Test-BrokenHorizonFirstLightMultiplayer.ps1 -Runtime Packaged -Topology Dedicated -PackageRoot Builds/Alpha-Development-20260905-FirstLightTopology/Windows` (hybrid Editor dedicated host)
- `Scripts/Test-BrokenHorizonFirstLightMultiplayer.ps1 -Runtime Packaged -Topology Listen -PackageRoot Builds/Alpha-Development-20260905-FirstLightTopology/Windows` (fully packaged listen)
- `Scripts/Test-BrokenHorizonRenderedUI.ps1` (use the scoped HUD case filter)
- `Scripts/Validate-BrokenHorizon.ps1 -DefenseAGarrison`
- `Scripts/Test-BrokenHorizonDefenseAMultiplayer.ps1 -Rendered`
- `Tools/Validate.ps1 -RequireTests -SkipReview`

Read this record and relevant ownership docs, inspect the current diff and named
reports, then select one playable slice. Keep one writer per bounded file set,
preserve unrelated work, and follow standing automatic commit/push delivery after
validation. Keep code, network, rendered, content, and package evidence distinct.
