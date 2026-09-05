# Alpha progress

Updated: 2026-09-05. Compact continuation record; repository evidence overrides it.
Status: **Verified** for the bounded same-process reconnect acceptance slice.
Full alpha and the player-facing release gates below remain open.

## Release boundary

- Full alpha requires the complete campaign, per
  [ModuleImplementationPlan.md](ModuleImplementationPlan.md), G4-G7.
- G1 Networked FirstLight is an earlier milestone, not the alpha release.
- Continue one playable slice at a time; no new scope or release date is implied.

## Preceding foundation - 2026-09-05

- Checkpoint `167893eab9b1206ea72e30e3203ead1968b6556c` was pushed to
  `origin/codex/first-light-acceptance` before this slice.
- Foundation work fixed connection-scoped snapshot ordering, restored development
  weapon-role fixtures, and checked exact loot identities through consumption/rejoin.
- Two-client First Light completed four objectives with inventory transfer, owner
  ammo HUD text `30 / 180`, and exact surviving loot IDs inherited on rejoin:
  `Saved/Logs/BH-Alpha-FirstLight-Coop-20260905-064139-Summary.json`.
- Foundation package and standalone route passed; the current package below
  supersedes that earlier build. Logged HUD text is not rendered proof.

## Delivered and verified current slice

- Added a non-shipping, opt-in acceptance fixture through real `LeaveSession`,
  main-menu travel, host restart, and reconnect in the retained client process.
  It does not change snapshot receive logic or ordinary game behavior.
- All 18 reconnect assertions passed in real NullRHI loopback networking:
  `Saved/Logs/Codex/BH-Alpha-SameProcessReconnect-20260905-072735-cb2a86c5-Summary.json`.
- Client PID `36624`, GameInstance `54588:4560`, and WarSubsystem `54718:26720`
  survived. Connection changed `57530:24486` -> none -> `61440:27738`.
  Actual accepted revision fell from `4097` to `2` on the new connection.
- Accepted revision, turn, sector count, and operation ID matched the restarted
  host's publication. The original host stopped only after the client reached
  the menu. The restarted host used a fresh isolated campaign, not save recovery.
- Editor build passed: `Saved/Logs/Codex/BuildEditor-20260905-072110.log`.
- Focused tests: 3 successes, zero warnings/failures/not-run/in-process:
  `Saved/Logs/Codex/AutomationReport-20260905-073231/index.json`.
- Existing strategic Defend/seamless-travel regression passed server travel and
  initial/second/rejoin snapshot application:
  `Saved/Logs/BH-Alpha-SameProcess-TravelRegression-20260905-073320-Summary.json`.
  This does not activate or complete tactical Defense A.
- Full `Tools/Validate.ps1 -RequireTests -SkipReview` passed: 125 tests
  (123 successes, 2 expected-warning successes), zero failed/not-run/in-process;
  Project Doctor had zero errors/warnings and the final Editor build was current:
  `Saved/Logs/Codex/AutomationReport-20260905-073426/index.json`.
- Scoped source/script review and recovery review found no supported blocker;
  the script correction preserved all acceptance assertions.

## Current package

- Fresh Development package passed (exit 0, 154.49 seconds):
  `Saved/Logs/Codex/PackageDevelopment-20260905-073510.log`.
- Archive: `Builds/Alpha-Development-20260905-SameProcess/Windows`.
  Launcher: `BrokenHorizon.exe`; runtime: `BrokenHorizon/Binaries/Win64/BrokenHorizon.exe`
  relative to that archive.
- Packaged standalone First Light passed all four objectives for one player:
  keycard, cache store/retrieve, door, pre-guard extraction gate, three guards,
  ammo reserve 150 -> 180, and extraction. The reconnect fixture remained inactive.
- Exit 0, no failure markers, isolated UserDir/checkpoint/backup; NullRHI gives
  no rendered or packaged multiplayer proof:
  `Saved/Logs/BH-Alpha-Packaged-FirstLight-SameProcess-20260905-073926-Summary.json`.
  The generated summary retains the exact standalone launch arguments.

## Remaining full-alpha gates

| Gate | Next required evidence |
| --- | --- |
| Code/unit | Current bounded slice passes build, focused/full automation, and scoped review. Repeat for subsequent changes. |
| Live network | Real same-process loopback acceptance is verified. Remote-network conditions, tactical Defense A activation/completion, crash/save recovery, and campaign soak remain. |
| Rendered/player | First Light interaction, inventory, shared objective/door behavior, HUD/debrief visibility after reconnect, and combat feel. No new rendered or HUD proof. |
| Campaign/content | Complete campaign and authored-content gates from the module plan/content inventory; G1 alone cannot satisfy alpha. |
| Package | Current creation and standalone route verified. Packaged host/join/reconnect, multiplayer operation completion, and rendered play remain. |

## Current task files

- `Source/BrokenHorizon/Private/BHSessionSubsystem.cpp`
- `Source/BrokenHorizon/Public/BHSessionSubsystem.h`
- `Source/BrokenHorizon/Private/BHWarGameState.cpp`
- `Scripts/Test-BrokenHorizonSameProcessReconnect.ps1`
- `Docs/Production/Alpha_Progress.md`

## Reproduction commands

Run from the project root with `powershell.exe -NoProfile -ExecutionPolicy Bypass -File`:

- `Tools/BuildEditor.ps1`
- `Scripts/Test-BrokenHorizonSameProcessReconnect.ps1`
- `Tools/RunTests.ps1 -TestFilter BrokenHorizon.Multiplayer.PersistentWar`
- `Scripts/Test-BrokenHorizonMultiplayer.ps1 -RequireActiveOperation -RequireServerTravel -OperationType Defend -ConnectionTimeoutSeconds 120 -LogPrefix BH-Alpha-SameProcess-TravelRegression`
- `Tools/Validate.ps1 -RequireTests -SkipReview`
- `Tools/PackageDevelopment.ps1 -ArchiveDirectory C:\UnrealProjects\BrokenHorizon\Builds\Alpha-Development-20260905-SameProcess`

## Efficient continuation

1. Read this record and relevant ownership docs; inspect the current diff and
   newest named evidence before selecting the next playable slice.
2. Keep one writer per bounded file set and preserve unrelated work in progress.
3. Use focused checks during edits, then one final gate for the coherent slice.
4. Record exact evidence limits; keep code, network, rendered, content, and
   package status distinct. Follow the standing GitHub delivery authorization.

World keycard pickup grants each player a credential; inventory replicates
owner-only. Mission-cache transfers affect the intended player only. See
[First Light checklist](../FIRSTLIGHT_PLAYTEST_CHECKLIST.md), D2/D6.
