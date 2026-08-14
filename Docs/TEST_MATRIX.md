# Broken Horizon - Test Matrix

## Commands

Run PowerShell scripts with the existing Windows policy bypass form:

    powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\Tools\ProjectDoctor.ps1
    powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\Tools\BuildEditor.ps1
    powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\Tools\RunTests.ps1 -TestFilter BrokenHorizon
    powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\Tools\Validate.ps1 -RequireTests

RunTests.ps1 launches UnrealEditor-Cmd with NullRHI by default, exports an
automation report under Saved/Logs/Codex, and distinguishes Passed, Failed,
TimedOut, and NoTests. When Unreal returns no usable process exit code but the
report exists, the report's test states are authoritative. Validate.ps1 runs
Project Doctor, the incremental editor build, tests, and ReviewChanges.ps1.
The build/test portion of Validate.ps1 -RequireTests passed. The default
review step remains blocked by pre-existing tracked and untracked whitespace
findings; the bootstrap files themselves pass the same check. Use -SkipReview
only to isolate the verified build/test gate until that unrelated work is
intentionally normalized.

## Current automation inventory

The current source contains 12 C++ test files and 103
IMPLEMENT_SIMPLE_AUTOMATION_TEST registrations under
Source/BrokenHorizon/Private/Tests. Tests are in the runtime module and
normally use FBH...Test names, BrokenHorizon.* paths, and
EditorContext | EngineFilter.

The pre-test-addition baseline report on 2026-08-14 discovered all 101 tests.
The exported report had 100 successes, one success with warning, zero failures,
and zero tests not run. The warning is the expected logged out-of-order
objective completion inside OrderedSequence, not a failed assertion.

The post-addition objective filter discovered FailureState,
OrderedSequence, and RuntimeRestore. It reported one success, two
successes-with-warnings, zero failures, and zero tests not run.

The post-change full filter then discovered 103 tests and reported 101
successes, two successes-with-warnings, zero failures, and zero tests not run.
The report is Saved/Logs/Codex/AutomationReport-20260814-023959. The runner
now rejects missing or incomplete exported summaries rather than certifying a
run from a null process exit code or partial report.
The final Validate -RequireTests -SkipReview run repeated the same 103-test
result in Saved/Logs/Codex/AutomationReport-20260814-032009. The default
Validate -RequireTests run reached ReviewChanges, which reported the dirty
checkout's pre-existing tracked and untracked whitespace findings and zero
generated paths.

The two expected warning-bearing tests in the final report are:

- `BrokenHorizon.Gameplay.Objectives.FailureState` (`FailureState`):
  `LogTemp: Ignored objective completion for FailureState_First; the expected objective is None.`
  This is the intentional post-failure completion rejection assertion.
- `BrokenHorizon.Gameplay.Objectives.OrderedSequence` (`OrderedSequence`):
  `LogTemp: Ignored objective completion for UnlockSecurityDoor; the expected objective is FindRedKeycard.`
  This is the intentional out-of-order completion rejection assertion.

Both warnings are expected evidence of rejected invalid transitions, not test
failures, and should not be suppressed merely to make the report quiet.

## Coverage matrix

| Area | Candidate behavior | Current evidence | Status | Test or gap |
|---|---|---|---|---|
| Interaction | Non-interactable actor is never dispatched through Execute_* | ABHCharacter prompt and ExecuteInteraction guard source | Source verified; direct deterministic test missing | Private trace/dispatch seam; do not add a test-only public API |
| Interaction | Invalid target is rejected before interface dispatch | IsValid checks in ABHCharacter::ExecuteInteraction and ServerInteract | Source verified; authority/trace delivery pending | Requires world-backed or small reusable predicate test |
| Door | Unlocked door toggles open/closed | ABHDoor source and First Light route harness | Integration/manual only | No direct deterministic actor test; transform/tick needs PIE |
| Door | Locked door rejects missing keycard | ABHDoor::Interact_Implementation source | Source verified; behavior test missing | Requires authority character/world and breaching-charge policy |
| Door | Locked door rejects wrong keycard | ABHDoor::Interact_Implementation source | Source verified; behavior test missing | Same world/policy gap |
| Door/keycard | Correct keycard unlocks and advances objective | ABHKeycard, ABHDoor, CompleteSharedObjective source; First Light route | Integration/manual only | PersistenceID and player-controlled actor fixture required |
| Objectives | Ordered current-only completion and final completion | FBHObjectiveSequenceTest | Passed | BrokenHorizon.Gameplay.Objectives.OrderedSequence |
| Objectives | FailMission is terminal and rejects later completion | UBHObjectiveComponent::FailMission and new focused test | Passed | BrokenHorizon.Gameplay.Objectives.FailureState |
| Objectives | Runtime restore filters invalid/duplicate IDs and restores first incomplete | UBHObjectiveComponent::RestoreRuntimeMissionState and new focused test | Passed | BrokenHorizon.Gameplay.Objectives.RuntimeRestore |
| Weapons | Magazine/reserve/reload invariants | Existing source and tests | Passed | BrokenHorizon.Gameplay.Combat.MagazineReloads; PersistentWar.WeaponStatePersistence |
| Weapons | Reload interruption presentation | Existing source/test | Passed | BrokenHorizon.Gameplay.Combat.ReloadInterruptionPresentation |
| Startup | Editor target launches without fatal initialization error | Project Doctor and editor build | Build passed; runtime smoke pending | Use task-specific UnrealEditor log and inspect markers |
| First Light | Production keycard/door/guard/extraction route | ABHWarGameState::RunFirstLightPlayableRouteTest and Scripts/Validate-BrokenHorizon.ps1 | Integration smoke/manual | NullRHI does not prove visuals or interaction feel |
| Blueprint/content | Widget/input/mission assignments, map actors, nav, materials/audio | Asset tree only | Pending Editor/PIE | See EDITOR_HANDOFF.md |
| Multiplayer | Authority, travel, reconnect, client HUD convergence | Existing source/contract tests | Partial | Requires two-client and soak evidence |

## Safe test rules

- Keep tests deterministic, independent of map order, timing races, and final
  art.
- Do not mutate disk or global settings.
- Prefer transient state components and pure rules.
- Do not instantiate a production Blueprint or use reflection to set private
  actor state solely to make a test pass.
- Treat property replication flags as contract checks, not as proof of network
  delivery.
- Keep First Light route and PIE tests separate from unit/state tests.

## Explicit gaps

The interaction and door/keycard candidates cannot be safely expanded into
direct unit coverage with the current public APIs. The exact missing seams are
the authoritative camera trace/dispatch predicate, actor authority/world
ownership, player-controlled iteration, stable map PersistenceID values, and
door transform/tick behavior. Documenting those gaps is preferable to faking
coverage. Objective failure and runtime-restore coverage now use the existing
transient component API; further interaction coverage should wait for a
production-owned predicate or world-fixture seam rather than adding test-only
public APIs.
