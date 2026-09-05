# Alpha progress

Updated: 2026-09-05. Compact continuation record; repository evidence overrides it.
Status: Code-complete; quick Editor/PIE validation remains.

## Release boundary

- Full alpha requires the complete campaign, per
  [ModuleImplementationPlan.md](ModuleImplementationPlan.md), G4-G7.
- G1 Networked FirstLight is an earlier milestone, not the alpha release.
- Continue one playable slice at a time through the existing roadmap; no new
  scope or release date is implied by this record.

## Delivered slice

- Snapshot receive watermark tracks source connection identity, accepts a new
  connection's lower revision, and preserves ordering during seamless travel.
- Development weapon-role fixtures restore their original state; network loot
  telemetry identifies exact instances across consumption and rejoin.
- First Light harness derives remaining loot from the observed starting set:
  six drops minus one consumed leaves five, rather than assuming two drops.
- Task-scoped reviewer found no supported blocking issue in the source, tests,
  or script corrections. Rendered/player and full release gates remain below.

## Verified post-change evidence

- Editor compile: `Saved/Logs/Codex/BuildEditor-20260905-064003.log`.
  Final validation build required no action:
  `Saved/Logs/Codex/BuildEditor-20260905-064434.log`.
- Focused tests: 3/3 successes, no failures, warnings, not-run, or in-process:
  `Saved/Logs/Codex/AutomationReport-20260905-064028/index.json`.
- Full `Tools/Validate.ps1 -RequireTests -SkipReview`: 125 tests passed
  (123 successes, 2 expected-warning successes); 0 failed/not-run/in-process:
  `Saved/Logs/Codex/AutomationReport-20260905-064436/index.json`.
- Combined First Light co-op run passed two players, four canonical objectives,
  authoritative completion, inventory transfer, weapon-role restoration, and
  owner ammo HUD text `30 / 180`. Both clients received completion; rejoin
  inherited it. Six available loot IDs minus consumed `84` exactly matched
  rejoin IDs `74,76,78,80,82`, with the consumed instance absent:
  `Saved/Logs/BH-Alpha-FirstLight-Coop-20260905-064139-Summary.json`.
- Defend travel run passed server travel and initial/second/rejoin snapshot
  application with the same committed operation:
  `Saved/Logs/BH-Alpha-ConnectionRevision-20260905-064310-Summary.json`.
  It does not prove tactical director activation or a two-hour soak.
- `SnapshotConnectionRevision` uses a scoped engine world and net-driver fixture
  to prove receive acceptance logic. It does not exercise a real same-process
  socket reconnect; the network harness launches a separate rejoin process.

- Development package created successfully (exit 0, 118.9 seconds):
  `Saved/Logs/Codex/PackageDevelopment-20260905-064556.log`.
  Launcher: `Builds/Alpha-Development-20260905/Windows/BrokenHorizon.exe`;
  runtime: `Builds/Alpha-Development-20260905/Windows/BrokenHorizon/Binaries/Win64/BrokenHorizon.exe`.
- Packaged standalone First Light route passed (exit 0, no failure markers):
  keycard, cache store/retrieve, door, pre-guard extraction gate, three guards,
  ammo reserve 150 -> 180, and all four objectives completed for one player.
  `Saved/Logs/BH-Alpha-Packaged-FirstLight-20260905-064946-Summary.json`
  records isolated saves, NullRHI, no rendered proof, and no multiplayer proof.

## Remaining gates

| Gate | Next required evidence |
| --- | --- |
| Code/unit | This slice passes compile, focused/full automation, and scoped review. Repeat when new changes justify it. |
| Live network | Same client process: leave to menu, restart host, reconnect at lower revision; verify HUD updates and seamless travel. Actual tactical Defense A activation/completion and campaign recovery/soak gates remain. |
| Rendered/player | First Light checklist with real interaction, inventory, shared objective/door behavior, HUD/debrief visibility, and combat feel. Logged HUD text does not prove rendered layout. |
| Campaign/content | Complete campaign and authored-content gates from the module plan and content inventory; G1 alone cannot satisfy alpha. |
| Package | Creation and standalone First Light route verified. Packaged host/join/reconnect, multiplayer operation completion, and rendered play remain open. |

## Task files

Production:

- `Source/BrokenHorizon/Public/BHWarSubsystem.h`
- `Source/BrokenHorizon/Private/BHWarSubsystem.cpp`
- `Source/BrokenHorizon/Private/BHWarGameState.cpp` (development helper)
- `Source/BrokenHorizon/Private/BHSupplyBase.cpp` (replication diagnostics)

Tests and harness:

- `Source/BrokenHorizon/Private/Tests/BHReplicationSnapshotRevisionTests.cpp`
- `Source/BrokenHorizon/Private/Tests/BHReplicationTestGameInstance.h`
- `Scripts/Test-BrokenHorizonFirstLightMultiplayer.ps1`

Documentation:

- `Docs/Production/Alpha_Progress.md`
- `Docs/ROADMAP.md`
- `Docs/FIRSTLIGHT_PLAYTEST_CHECKLIST.md`

## Commands

Run from the project root with `powershell.exe -NoProfile -ExecutionPolicy Bypass -File`:

- `Tools/BuildEditor.ps1`
- `Tools/RunTests.ps1 -TestFilter BrokenHorizon.Multiplayer.PersistentWar`
- `Scripts/Test-BrokenHorizonFirstLightMultiplayer.ps1 -RequireInventoryTransfer -RequireWeaponRoleRuntime -LogPrefix BH-Alpha-FirstLight-Coop`
- `Scripts/Test-BrokenHorizonMultiplayer.ps1 -RequireActiveOperation -RequireServerTravel -OperationType Defend -ConnectionTimeoutSeconds 120 -LogPrefix BH-Alpha-ConnectionRevision`
- `Tools/Validate.ps1 -RequireTests -SkipReview`
- `Tools/PackageDevelopment.ps1 -ArchiveDirectory C:\UnrealProjects\BrokenHorizon\Builds\Alpha-Development-20260905` (passed)

## Efficient continuation

1. Read this record, the active roadmap slice, and only relevant ownership docs.
2. Inspect the current diff and newest named evidence before selecting work.
3. Keep one writer per bounded file set; preserve inherited changes.
4. Use focused checks during edits, then one final gate for the coherent slice.
5. Update exact evidence paths and limits; keep code/unit, live-network,
   rendered/player, content, and package status distinct.

World keycard pickup grants each player a credential on the server; inventories
replicate owner-only. Mission-cache transfers affect the intended player only.
See `ABHKeycard::Interact_Implementation`, `ABHCharacter::GetLifetimeReplicatedProps`,
and [First Light checklist](../FIRSTLIGHT_PLAYTEST_CHECKLIST.md), D2/D6.
