# Broken Horizon - Unreal Editor Handoff

These items remain pending because C++ build and NullRHI automation cannot
prove Blueprint assignments, binary asset wiring, rendered layout, navigation,
interaction feel, or client/server presentation.

## Task: Verify the player Blueprint contract

**Why Editor work is required:**

ABHCharacter owns the gameplay components and creates widgets from
Blueprint-assigned class properties. The source and asset paths exist, but the
current text checkout does not prove the assignments inside
Content/Characters/BP_BHCharacter.

**Steps:**

1. Open Content/Characters/BP_BHCharacter in the Unreal Editor.
2. In Class Defaults, inspect the exact properties PlayerMappingContext,
   MissionData, InteractionPromptClass, ObjectiveWidgetClass,
   ObjectiveNotificationWidgetClass, AmmoHUDWidgetClass,
   CombatStatusWidgetClass, PauseMenuWidgetClass, InventoryWidgetClass,
   MissionCompleteWidgetClass, DeathWidgetClass, and SubtitleWidgetClass.
3. Where the project intends an assignment, verify the asset resolves to the
   existing content paths: Content/BrokenHorizon/Input/IMC_Player;
   Content/BrokenHorizon/UI/MBP_InteractionPrompt;
   WBP_Objective; WBP_ObjectiveNotification; WBP_AmmoHud;
   WBP_CombatStatus; WBP_PauseMenu; WBP_Settings; and the corresponding
   native/Blueprint classes present in the Content/BrokenHorizon/UI folder.
   The text asset listing does not show a WBP_Inventory asset, so verify
   InventoryWidgetClass against the actual class and do not guess an
   inventory assignment.
4. Do not invent or substitute a MissionData asset. If MissionData is null or
   points to a missing asset, record the exact property/path and stop for a
   content decision.
5. Compile and save BP_BHCharacter only if an existing assignment is corrected.

**PIE verification:**

1. Set the PIE map to /Game/BrokenHorizon/Maps/L_FirstLight_Graybox.
2. Possess the configured player and confirm the objective, interaction prompt,
   ammo, combat-status, subtitle, and notification widgets appear only for the
   local player.
3. Verify IMC_Player action assets drive movement, look, interact, fire, aim,
   reload, pause, jump, crouch, and sprint. Record any missing binding by
   action name.

**Status:** Pending

## Task: Verify the game-mode and editor-module contract

**Why Editor work is required:**

DefaultEngine.ini points to
/Game/BrokenHorizon/Core/BP_BHGameMode.BP_BHGameMode_C, while the descriptor
declares BrokenHorizonEditor and the editor target currently lists only
BrokenHorizon in ExtraModuleNames. The editor build passed, but Blueprint
availability of UBHWorldBuilderLibrary is not proven.

**Steps:**

1. Open Content/BrokenHorizon/Core/BP_BHGameMode and inspect the parent class,
   GameStateClass, PlayerControllerClass, and Default Pawn Class.
2. Confirm the configured gameplay shell uses ABHGameMode, ABHWarGameState,
   ABrokenHorizonPlayerController, and BP_BHCharacter as intended by the
   source/configuration.
3. Open a test Blueprint or the appropriate editor utility search and search
   for the functions exposed by UBHWorldBuilderLibrary from
   Source/BrokenHorizonEditor/Public/BHWorldBuilderLibrary.h.
4. Do not run destructive world-builder functions during this audit. Do not
   rebuild or overwrite a map.
5. Compile/save only assets that were actually changed by an intentional
   editor decision.

**PIE verification:** None until the editor module's function availability
and ownership are confirmed.

**Status:** Pending

## Task: Run the First Light interaction route

**Why Editor work is required:**

The production keycard, door, guard, extraction, navigation, UI, and audio
chain uses map actors, instance PersistenceID values, Blueprint presentation,
and server authority. C++ route automation can exercise contracts, but it
cannot prove player traversal or feel.

**Steps:**

1. Open /Game/BrokenHorizon/Maps/L_FirstLight_Graybox.
2. Confirm the placed keycard, door, guard/operation site, and extraction
   actors have stable instance PersistenceID values and that the map contains
   the intended navigation data.
3. Start a one-client PIE session and follow the objective order:
   FindRedKeycard, UnlockSecurityDoor, EliminateGuard, ReachExtraction.
4. Before collecting the card, attempt the locked door and record the prompt
   and rejection. If a wrong-card fixture exists, record that rejection too.
   Collect the real card, interact with the door, complete the guard step, and
   overlap extraction.
5. Start a two-client PIE session and verify the authority-owned objective,
   keycard, door, and completion presentation converges for both clients.
6. Record map actor labels/PersistenceID values, log markers, and any
   navigation, UI, audio, or timing issue. Do not modify binary assets in this
   handoff.

**PIE verification:** The four objective IDs advance exactly once in order;
the locked door does not open without the required credential; the correct
credential unlocks it; extraction completes; both clients show the same
authoritative result.

**Status:** Pending
