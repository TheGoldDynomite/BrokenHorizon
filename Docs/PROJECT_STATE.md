# Broken Horizon - Project State

This file records verified present-tense repository facts. It is updated from
the actual checkout, not from the historical bootstrap assumptions.

## Identity

| Field | Current value | Evidence |
|---|---|---|
| Project | Broken Horizon | BrokenHorizon.uproject |
| Engine | Unreal Engine 5.8.0 | BrokenHorizon.uproject EngineAssociation=5.8; Project Doctor; Engine/Build/Build.version |
| Platform | Windows | BrokenHorizonEditor Win64 Development build; Visual Studio 2022 toolchain |
| Architecture | C++ runtime/editor modules plus Blueprint/content assets | BrokenHorizon.uproject; Source/BrokenHorizon; Source/BrokenHorizonEditor; Content |
| Runtime module | BrokenHorizon | Source/BrokenHorizon/BrokenHorizon.Build.cs |
| Editor module | BrokenHorizonEditor | Source/BrokenHorizonEditor/BrokenHorizonEditor.Build.cs and Private/BrokenHorizonEditor.cpp |
| Git branch | codex/realistic-gameplay-expansion | git branch --show-current |
| HEAD at bootstrap | ff3ed94ae32dc83a1bf3062958780a8ea7119492 | git rev-parse HEAD |
| Working tree | Pre-existing dirty WIP; preserve unrelated changes | Project Doctor reported 133 changed/untracked entries before this bootstrap |
| Engine root | C:/Program Files/Epic Games/UE_5.8 | Project Doctor and BuildEditor.ps1 |
| Editor target | BrokenHorizonEditor Win64 Development | Source/BrokenHorizonEditor.Target.cs |
| Game target | BrokenHorizon Win64 Development | Source/BrokenHorizon.Target.cs |

## Verified baseline

Project Doctor passed on 2026-08-14 with zero errors and zero warnings. It
confirmed the descriptor, UE 5.8.0 installation, UnrealEditor-Cmd, both target
names, 223 C++/C# source files, all kit files, Git LFS, Unreal ignore/LFS
rules, and the Visual C++ toolchain.

The exact baseline editor command was:

    powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\Tools\BuildEditor.ps1

It passed before any source or documentation edit. The native build resolved:

    Build.bat BrokenHorizonEditor Win64 Development C:/UnrealProjects/BrokenHorizon/BrokenHorizon.uproject -WaitMutex -NoHotReloadFromIDE

The build completed successfully in 14.77 seconds. Its log is
Saved/Logs/Codex/BuildEditor-20260814-022802.log.

The first full automation invocation discovered 101 tests and reached
Automation Test Queue Empty. Its exported report contained 100 successful
tests, one successful-with-warning test, and zero failed tests. The wrapper
incorrectly returned Failed because the Unreal process exit code was null and
the wrapper treated null as nonzero before consulting the report. The wrapper
was corrected to refresh the process and use the exported report when present;
the focused OrderedSequence rerun then passed. After the objective tests were
added, the full suite discovered 103 tests and reported 101 successes, two
successes-with-warnings, zero failures, and zero tests not run. The report is
Saved/Logs/Codex/AutomationReport-20260814-023959. The strict report gate now
requires a complete exported summary (failed, notRun, and inProcess fields);
missing or incomplete reports fail instead of passing on log heuristics.

Validate.ps1 -RequireTests -SkipReview passed on 2026-08-14. It completed
Project Doctor, the incremental editor build, and the full 103-test suite with
testsStatus=Passed. The default Validate.ps1 -RequireTests gate reaches
ReviewChanges but remains blocked by the dirty-checkout baseline: the report
contains the nine pre-existing tracked new-blank-line-at-EOF findings plus
additional whitespace findings in pre-existing untracked/user files.
generatedPathCount is zero, and the bootstrap files themselves have no
diff-check findings. Those unrelated files were not normalized. The final
skip-review run report is Saved/Logs/Codex/AutomationReport-20260814-032009;
Saved/Logs/Codex/validation-latest.json records success=true,
testsStatus=Passed, and reviewStatus=Skipped. A separate default run at
02:59:15 reached ReviewChanges and failed as expected because of the
dirty-checkout findings; review-latest.json recorded 24 diff-check findings
and zero generated paths.

## Verified gameplay architecture

- The runtime module directly declares Core, CoreUObject, Engine, PhysicsCore,
  InputCore, EnhancedInput, AIModule, NavigationSystem, StateTreeModule,
  GameplayStateTreeModule, DeveloperSettings, UMG, Slate, SlateCore, Niagara,
  Json, OnlineSubsystem, and OnlineSubsystemUtils dependencies in
  Source/BrokenHorizon/BrokenHorizon.Build.cs.
- The editor module directly depends on BrokenHorizon, Landscape, and Foliage,
  with UnrealEd private dependency in
  Source/BrokenHorizonEditor/BrokenHorizonEditor.Build.cs.
- Source/BrokenHorizon.Target.cs and Source/BrokenHorizonEditor.Target.cs use
  BuildSettingsVersion.V7 and EngineIncludeOrderVersion.Unreal5_8.
- Config/DefaultEngine.ini sets the game default map to
  /Game/BrokenHorizon/Maps/L_MainMenu and the global game mode to
  /Game/BrokenHorizon/Core/BP_BHGameMode.BP_BHGameMode_C. Config/DefaultGame.ini
  includes L_MainMenu, L_FirstLight_Graybox, and L_BrokenHorizon_World in the
  gameplay/cook configuration.
- ABHCharacter is the main player integration owner. Its constructor creates
  the objective, health, injury, and weapon components. BeginPlay creates the
  local-player objective, interaction prompt, ammo, combat-status, subtitle,
  and related widgets when their class properties are assigned.
- Enhanced Input flows through ABrokenHorizonPlayerController default mapping
  contexts and ABHCharacter runtime action/mapping setup. The authored content
  includes Content/BrokenHorizon/Input/IMC_Player and IA_Move, IA_Look,
  IA_Jump, IA_Crouch, IA_Sprint, IA_Interact, IA_Fire, IA_Aim, IA_Reload, and
  IA_Pause.
- ABHMainMenuGameMode owns the main-menu widget. The player controller owns
  optional mobile controls. ABHCharacter owns the gameplay widget instances.
- UBHInteractable and IBHInteractable define the interaction contract.
  ABHCharacter::UpdateInteractionPrompt and
  ABHCharacter::ExecuteInteraction validate actor validity and interface
  implementation before calling an IBHInteractable Execute_* function.
- ABHDoor is a replicated interactable. Its authority-only interaction handles
  locked/unlocked/open state, RequiredKeycard, breaching-charge state, the
  UnlockSecurityDoor objective, and PersistenceID-based save/restore.
- ABHKeycard is a replicated authority-only interactable. It calls
  ABHCharacter::CollectKeycard for player-controlled characters, completes the
  FindRedKeycard shared objective, and destroys itself after a successful
  collection.
- UBHObjectiveComponent is a replicated, no-Tick actor component owned by
  ABHCharacter. StartRuntimeMission, CompleteObjectiveByID, FailMission,
  RestoreRuntimeMissionState, and ClearMissionState provide deterministic
  state-transition APIs. The source test
  BrokenHorizon.Gameplay.Objectives.OrderedSequence already covers ordered
  completion, out-of-order rejection, replicated fields, mission completion,
  and clearing. The added
  BrokenHorizon.Gameplay.Objectives.FailureState and
  BrokenHorizon.Gameplay.Objectives.RuntimeRestore tests cover the safe
  terminal-failure and runtime-restore seams.
- UBHWeaponComponent and ABHRifle already implement the weapon foundation;
  weapon implementation is not a planned-empty system. Existing code covers
  authoritative magazine/reserve state, reload, fire mode, heat, hitscan,
  replication, and presentation hooks. ABHRifle references the First Light
  weapon-fire, indoor/outdoor tail, dry-fire, and reload sound assets.

## Known gaps and boundaries

- The generic interaction guard is source-verified but has no direct
  deterministic test because the dispatch helper is private and the normal
  path depends on an authoritative camera trace. Do not add a test-only public
  API or fake a dispatch assertion.
- There is no safe direct unit test for ABHDoor or ABHKeycard missing/wrong/
  correct keycard behavior. The current behavior depends on actor authority,
  player-controlled actors in a UWorld, map PersistenceID values, transforms,
  and shared objective dispatch. Keep this as First Light integration/manual
  coverage until a generally useful access-policy seam is intentionally
  designed.
- Reflection flags prove that objective and door fields replicate; they do not
  prove packet delivery, reconnect convergence, client HUD refresh, or
  multiplayer timing.
- The editor target build passes, but the target lists BrokenHorizon in
  ExtraModuleNames while the descriptor declares BrokenHorizonEditor. Whether
  editor-only Blueprint library functions are loaded in the editor remains an
  explicit Editor verification item.
- C++ and asset-path existence do not prove Blueprint assignments, map actor
  placement, navigation coverage, widget layout, audio balance, interaction
  feel, or visual quality. Those remain PIE/editor gates.
- The checkout includes pre-existing modified source, tests, scripts, maps,
  localization, binary assets, and untracked accelerator/world-kit material.
  None of those files were reset, cleaned, or replaced.
- ReviewChanges checks both tracked diffs and untracked text files. The current
  dirty checkout has the nine pre-existing tracked EOF findings plus
  pre-existing untracked/user whitespace findings; the bootstrap files pass
  that check and generatedPathCount remains zero. This is a dirty-checkout
  review blocker, not a build or test failure.

## Current active milestone

**Verified Codex development baseline.** The repository is buildable and its
headless automation workflow now understands the real project test report.
The next slice is First Light interaction/operation-route acceptance and
manual editor verification, not a new weapon foundation.

## Validation record

| Date | Commit/branch | Project Doctor | Editor build | Automation | PIE/manual | Notes |
|---|---|---|---|---|---|---|
| 2026-08-14 | codex/realistic-gameplay-expansion / ff3ed94a... | Passed, 0 errors, 0 warnings | Passed before and after the test addition | Final full suite: 103 discovered, 101 success, 2 success-with-warning, 0 failed, 0 not run | Pending | Validate -RequireTests -SkipReview passed; default review is blocked by pre-existing tracked and untracked whitespace findings |

## Progress log

- 2026-08-14: Verified UE 5.8.0 descriptor, targets, modules, source
  ownership, input/UI/interaction/objective/weapon paths, and existing
  automation conventions.
- 2026-08-14: Project Doctor passed; pre-edit editor build passed.
- 2026-08-14: Fixed the tooling-only null-process-exit/report classification
  in Tools/BH.Common.ps1 and Tools/RunTests.ps1.
- 2026-08-14: Added deterministic objective failure and runtime-restore
  automation tests in
  Source/BrokenHorizon/Private/Tests/BHObjectiveStateTransitionTests.cpp;
  the focused objective filter passed.
- 2026-08-14: Full post-change automation passed with 103 discovered tests,
  zero failures, and zero tests not run.
- 2026-08-14: Hardened the test runner to reject missing/incomplete exported
  reports and hardened change review to inspect untracked text files while
  preserving the existing dirty-checkout findings.
