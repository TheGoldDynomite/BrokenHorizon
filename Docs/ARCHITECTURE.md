# Broken Horizon - Architecture

## Evidence key

- **Verified:** confirmed in the current descriptor, source, configuration,
  asset tree, or completed command output.
- **Pending:** requires Unreal Editor, PIE, a real map, or a network harness.
- **Planned:** not implemented or not selected for the current slice.

## System map

    BrokenHorizon.uproject
    |
    +-- BrokenHorizon runtime module
    |   +-- ABHCharacter
    |   |   +-- Objective, health, injury, weapon components
    |   |   +-- Enhanced Input runtime setup
    |   |   +-- Interaction trace and authority dispatch
    |   |   +-- Local gameplay widgets
    |   +-- ABHGameMode / ABHWarGameState
    |   |   +-- seamless travel and shared campaign shell
    |   |   +-- persistent war and operation state
    |   +-- UBHObjectiveComponent
    |   +-- ABHDoor / ABHKeycard / IBHInteractable
    |   +-- UBHWeaponComponent / ABHRifle
    |   +-- save/session/war/AI/logistics systems
    |   +-- Source/BrokenHorizon/Private/Tests automation suite
    |
    +-- BrokenHorizonEditor module
        +-- UBHWorldBuilderLibrary
        +-- editor-only world construction helpers

The runtime module is declared in BrokenHorizon.uproject and
Source/BrokenHorizon/BrokenHorizon.Build.cs. The editor module is declared in
the descriptor and has source under Source/BrokenHorizonEditor. The editor
target build passes, but the editor target's ExtraModuleNames currently lists
only BrokenHorizon; module loading of BrokenHorizonEditor is a pending editor
verification item.

## Ownership and data flow

### Player shell, input, and UI

1. ABrokenHorizonPlayerController adds its Blueprint-configured
   DefaultMappingContexts in SetupInputComponent.
2. ABHCharacter::EnsureRuntimeInputActions creates transient fallback actions
   for the expanded gameplay action set.
3. ABHCharacter::RefreshPlayerInputMappings clones the authored
   PlayerMappingContext, applies user remaps, adds fallback bindings, and
   installs the runtime context.
4. ABHCharacter::SetupPlayerInputComponent binds movement, look, posture,
   interaction, weapon, grenade/smoke, medical, engineering, squad, map, and
   menu actions to character handlers.
5. ABHCharacter::BeginPlay creates local-player widgets from its class
   properties. ABHMainMenuGameMode creates the main-menu widget and switches
   the local player to UI-only input.

The C++ widget classes are native. The current content tree contains
Content/BrokenHorizon/UI/MBP_InteractionPrompt,
WBP_Objective, WBP_ObjectiveNotification, WBP_AmmoHud, WBP_CombatStatus,
WBP_PauseMenu, WBP_Settings, and Content/BHAmmoHUDWidget. Blueprint property
assignments and visual layout remain pending Editor/PIE evidence.

### Interaction, doors, and keycards

UBHInteractable/IBHInteractable is the polymorphic contract. The prompt path
checks IsValid(HitActor) and interface implementation before
Execute_GetInteractionText. The authoritative path in
ABHCharacter::ExecuteInteraction rejects non-authority/invalid targets,
handles explicit character/enemy cases, then checks
TargetActor->Implements<UBHInteractable>() before Execute_Interact.

ABHDoor owns replicated bIsOpen, bLocked, TargetOpenRotation, bBreached, and
bBreachChargePlanted. RequiredKeycard and instance-only PersistenceID are
content contracts. ABHDoor::Interact_Implementation is authority-only and
uses ABHCharacter::HasKeycard, TryPlaceBreachingCharge,
CompleteSharedObjective, and the open/close state.

ABHKeycard owns KeycardID, KeycardName, and instance-only PersistenceID.
Its authority-only interaction iterates valid player-controlled ABHCharacter
actors, calls CollectKeycard, completes FindRedKeycard, and destroys the
pickup after the interacting character receives the credential.

This design makes a map-backed interaction route useful integration coverage,
but not a clean unit-test seam. A future policy extraction must remain a
generally useful access rule, not a test-only getter for protected actor state.

### Mission and persistence state

UBHObjectiveComponent is replicated and has no Tick. It is created and owned
by ABHCharacter, so objective fields are per-character replicated state even
when CompleteSharedObjective keeps player progress aligned on the authority.
StartRuntimeMission activates the first definition. CompleteObjectiveByID
accepts only the current objective, advances in order, broadcasts completion,
and marks the mission complete after the final definition. FailMission is
terminal until state is restored or cleared. RestoreRuntimeMissionState
filters invalid/duplicate completed IDs and reconstructs the first incomplete
objective when needed.

ABHGameMode owns ABHWarGameState, enables seamless travel, adopts the most
progressed mission state for joining players, and restarts missing pawns after
travel. BHSaveSubsystem persists campaign/player state and resolves world
actors through stable instance PersistenceID values. Unique IDs are therefore
save/content contracts, not optional labels.

### Weapons and combat

UBHWeaponComponent is an actor component on ABHCharacter. It owns magazine and
reserve ammunition, reload state/type, fire mode, aiming, role, heat,
overheat, timers, replication callbacks, server RPCs, and the equipped
ABHRifle. ABHRifle performs configurable hitscan/ballistic response and
presentation/audio hooks. Existing tests cover weapon state persistence,
reload interruption presentation, role profiles, and replication contracts.

The current source already contains the weapon foundation. The next Codex
slice should not recreate it; manual Blueprint/presentation review and the
broader authored arsenal remain roadmap work.

## Verified class inventory

| System | Class/file | Owner | Inputs | Outputs/events | Assets/contracts | Status |
|---|---|---|---|---|---|---|
| Game shell | ABHGameMode in Source/BrokenHorizon/BHGameMode.* | World/game mode | travel/player join | GameState/pawn setup | BP_BHGameMode | Verified in source; PIE pending |
| Player | ABHCharacter in Source/BrokenHorizon/BHCharacter.* | Player pawn | Enhanced Input actions | movement, interaction, UI delegates | BP_BHCharacter; IMC_Player | Verified in source; Blueprint assignment pending |
| Interaction | UBHInteractable, IBHInteractable, ABHCharacter | Character trace and target actor | visibility trace, Interact action | prompt text and authority dispatch | MBP_InteractionPrompt | Guard verified; direct test seam pending |
| Door/keycard | ABHDoor, ABHKeycard | World actor plus character credential set | interaction target, RequiredKeycard | lock/open/breach/objective/save | PersistenceID, FindRedKeycard, UnlockSecurityDoor | Source verified; map/PIE pending |
| Objectives | UBHObjectiveComponent | ABHCharacter | runtime/authored definitions | current/completed/failed/complete delegates | BHMissionData, objective IDs | Sequence, failure, and restore tests verified |
| Weapons | UBHWeaponComponent, ABHRifle | ABHCharacter/component | fire/aim/reload actions | ammo, damage, presentation delegates | BP_Rifle; First Light audio | Source and automation verified; feel pending |
| Campaign | ABHWarGameState, UBHWarSubsystem, operation directors | GameState/subsystems/directors | turns, operations, travel | replicated snapshots, events, saves | operation IDs/PersistenceID | Source/automation verified; multiplayer soak pending |
| Editor tools | UBHWorldBuilderLibrary | BrokenHorizonEditor module | editor calls | map/world construction helpers | editor-only assets/maps | Build target verified; module use pending |

## Automation inventory

Source/BrokenHorizon/Private/Tests currently contains 12 C++ test files and
103 IMPLEMENT_SIMPLE_AUTOMATION_TEST registrations. The registrations use
FBH...Test names, BrokenHorizon.* paths, and
EditorContext | EngineFilter. The objective registrations are:

- BrokenHorizon.Gameplay.Objectives.OrderedSequence
- BrokenHorizon.Gameplay.Objectives.FailureState
- BrokenHorizon.Gameplay.Objectives.RuntimeRestore

The runner is Tools/RunTests.ps1 and the full gate is Tools/Validate.ps1.
The exported automation report is authoritative when UnrealEditor-Cmd returns
no usable process exit code after a completed report.

## Test and extension seams

- Deterministic state tests can construct a transient UBHObjectiveComponent
  with StartRuntimeMission and exercise completion/failure/restore/clear
  without a map.
- BHObjectiveStateTransitionTests.cpp is the focused deterministic coverage
  for FailMission terminal behavior and RestoreRuntimeMissionState filtering.
- Generic interaction safety is currently embedded in private
  ABHCharacter trace/dispatch code. A direct test would require a small
  reusable predicate or a world-backed trace fixture; adding a public API
  solely for a test is out of scope.
- Door/keycard transitions depend on authority, UWorld actor iteration,
  player-controlled characters, map IDs, and transforms. Use the existing
  First Light route smoke/manual PIE for the complete chain unless an access
  policy is intentionally extracted.
- Replication property flags are cheap contract checks. Packet delivery,
  reconnect, OnRep presentation, and client/server authority require a
  multi-process or PIE network run.
- Native widget ownership can be checked from C++, but layout, safe frame,
  input glyphs, audio balance, navigation, and interaction feel require
  rendered/manual review.

## Direct evidence files

- BrokenHorizon.uproject
- Config/DefaultEngine.ini and Config/DefaultGame.ini
- Config/ProjectManifest.json
- Source/BrokenHorizon/BrokenHorizon.Build.cs
- Source/BrokenHorizon.Target.cs and Source/BrokenHorizonEditor.Target.cs
- Source/BrokenHorizon/BHCharacter.cpp and BHCharacter.h
- Source/BrokenHorizon/Private/BHDoor.cpp and Public/BHDoor.h
- Source/BrokenHorizon/Private/BHKeycard.cpp and Public/BHKeycard.h
- Source/BrokenHorizon/Private/BHObjectiveComponent.cpp and Public/BHObjectiveComponent.h
- Source/BrokenHorizon/Private/BHWeaponComponent.cpp and Public/BHWeaponComponent.h
- Source/BrokenHorizon/Private/Tests
- Tools/ProjectDoctor.ps1, BuildEditor.ps1, RunTests.ps1, Validate.ps1
