# Active playable slice - Attack A

Selected 2026-09-06. Status: In progress; player-controlled Editor acceptance pending.

## Working constraint

The user requires Unreal Editor / Play in Editor validation. Do not cook or
package the game unless the user explicitly asks. Compile the Editor target
only when changes require it. Existing release-only packaging gates remain
unproven and deferred; Editor evidence does not satisfy those gates.

## Player outcome and scope

Complete the first loop in Broken_Horizon_1.0_Roadmap.md: resistance-base
preparation, deployment roster, travel and reconnaissance, Attack A combat,
ammunition/casualty/vehicle consequences, return or withdrawal, debrief/save,
and a next deployment that reflects those consequences. Fix failures within
this loop before selecting another feature or expanding water-route content.
Do not count the separate four-objective First Light tutorial as this slice.

Preserve existing dirty documentation/configuration and water/lighting scripts.
No production-code defect is established by the initial read-only inspection.

## Evidence inspected

- Saved/Logs/Codex/validation-latest.json: September 6 00:10 PDT gate passed
  Project Doctor, incremental Editor build, and full BrokenHorizon automation;
  task review was skipped.
- Saved/Logs/Codex/AutomationReport-20260906-001026/index.json: 135 successes,
  6 successes with warnings, 0 failures, 0 not run, 0 in process. This was
  NullRHI automation, not a rendered or human-controlled playtest.
- The report includes operation snapshot compatibility/identity, field-squad
  readiness/debrief, save-game round-trip, and persistent-war tests. These
  support individual contracts; they do not prove the full player loop.
- Docs/Production/ModuleImplementationPlan.md records the FirstLightAttackA
  authored checkpoint and operation-gated first-wave formation.
- Source/BrokenHorizon/BHCharacter.cpp: ToggleWarMap opens deployment mode
  when no runtime war operation is active and the campaign is unresolved.
- Source/BrokenHorizon/Private/BHWarMapWidget.cpp: A/D selects operations;
  Enter deploys; a readiness warning can require another Enter; M/Esc closes
  the strategic map; Backspace requests and confirms withdrawal.
- Source/BrokenHorizon/Private/BHOpenWorldOperationDirector.cpp: the normal
  variant is deterministic from sector and operation type. An arbitrary
  Attack choice is not guaranteed to be Attack A. Do not force a fixture
  completion or silently relabel Attack B as accepted Attack A evidence.

## First user PIE pass

1. Open /Game/BrokenHorizon/Maps/L_FirstLight_Graybox in Unreal Editor.
   Use Play in Editor with one player for the initial control/route pass.
2. Press M (or the saved War Map binding). Before deployment, inspect available
   force/readiness, loadout/ammunition, supplies, staging, and the operation
   briefing. Select an Attack with A/D; Enter deploys. Confirm another Enter
   only if choosing to accept the displayed readiness warning.
3. Record the selected sector and route label. The target is ROUTE A /
   BASELINE APPROACH. If deployment is unavailable, another operation is
   already active, or only another variant is reached, report that exact
   state before resetting any save or using test-only commands.
4. Follow the operation through normal player movement and combat. Check
   whether staging, reconnaissance, cover, objectives, and return/extraction
   are understandable without developer directions. Report missing steps as
   defects rather than skipping them. Observe ammo and casualty/vehicle
   changes where they occur; do not create casualties merely to finish a test.
5. Complete the operation and inspect its debrief and checkpoint feedback.
   Confirm the outcome explains resource/territory consequences and local
   fireteam readiness. Continue and inspect the next deployment. It must
   reflect the resulting state and clear the completed operation presentation.
6. Report the first failed step, its visible message, and whether it occurs
   before deployment, during travel/combat, at debrief, or on continuation.
   If it passes, report the route and the before/after consequences observed.

## Remaining acceptance after the first pass

- Base preparation, travel/reconnaissance, tactical objective, and return
  work through physical input and remain understandable in the rendered game.
- A successful operation changes the next deployment as designed.
- Withdrawal produces the intended consequence and allows continued play.
- The normal checkpoint reload/Continue flow restores the observed result.
- A two-player listen-server PIE pass proves host/client objective and outcome
  convergence, owner-specific inventory/readiness, and no stale presentation.
- Only after those observations and any necessary fixes/review may this slice
  be called accepted. Source/automation, rendering, player controls, and
  multiplayer/persistence observations must be recorded separately.

No engine process, cook, package, source edit, or map mutation was started by
this selection pass. Read-only explorer: bh_code_explorer. Engine execution
remains unassigned until an actual validation run is needed.

## Door approach obstruction repair - 2026-09-06

User reported a large solid green rectangle beside the First Light door.
Fresh Editor inspection identified FirstLightWaterShoreWest: a 700 x 1000 x
80 cm terrain cube at (4300,300,80), overlapping the security door and reaching
120 cm above ground. The scoped repair lowered only its center Z to -41,
putting its top 1 cm below FL_Ground and avoiding a coplanar surface.
The shoreline generator now uses the same west-bank position.

Verified: read-only inspection, backed-up Editor mutation, save/unload/reload,
125-actor recorded identity/transform/mesh/material/collision preservation, and
read-only First Light keycard/door/guard/extraction contract PASS. Evidence:
Saved/Logs/BH-DoorShoreline-Inspect.log,
Saved/Logs/BH-DoorShoreline-Apply2.log,
Saved/FirstLightDoorShorelineRepair/20260906-135452-607429b6/report.json,
and Saved/Logs/BH-DoorShoreline-Contract.log.
The validator's nav-settings warning belongs to its separate L_Prototype
comparison, not the repaired First Light map.

Agents: bh_code_explorer, bh_cpp_implementer, bh_reviewer. No C++ rebuild,
cooking or packaging was needed. Editor commandlet execution is complete.
Pending player check: reopen First Light in Editor, collect the keycard, unlock
the security door and walk through; confirm the raised green block is gone.
This fixes one reported obstruction; Attack A acceptance remains pending.

## Door presentation and bleeding reload repair - 2026-09-06

User confirmed the raised green block is gone. Their next PIE findings were a
cube-shaped security door and bleeding repeatedly preventing reload completion.

The door now uses the small authored SM_FirstLightSecurityDoor asset: an 8 cm
steel leaf with panels, trim, a warning plate, hinges and lever handles on both
faces. Hardware bounds are 18 x 200 x 200 cm. Its pivot moved to the leaf edge
while retaining the opening footprint, keycard, persistence ID and opening
settings. One simple box collider is saved with the mesh. The generator uses
the same asset/hinge placement; the original prototype asset is unchanged.
Authoring uses full UnrealEditor with -nullrhi; the script rejects unsupported
apply environments before mutation. No cook or package was generated.

Periodic injury damage now carries a scoped ongoing-damage context. Character
and owning-client damage handling preserve reload for that damage, while direct
hits still interrupt. Health loss, feedback, original damage causer and death
remain active. Tests cover repeated bleed ticks completing a reload exactly
once, direct-hit interruption, owner-notification policy, nested damage context,
and lethal bleeding ending the reload.

Validation evidence:
- Editor C++ build and full Tools/Validate.ps1 -RequireTests -SkipReview passed.
  Saved/Logs/Codex/AutomationReport-20260906-073712/index.json contains 136
  successes, 9 successes with warnings, 0 failures/not-run/in-process (145 total).
  Three new warnings concern transient test-world rifle teardown without a world
  context; the other six warning cases predate this fix. No production failure
  was reported. Focused BleedingReload tests also passed independently.
- Saved/FirstLightSecurityDoor/20260906-144211-27c555bb/report.json verifies
  backed-up apply, map save/reload, 125 actors and unchanged unrelated inventory.
- Saved/FirstLightSecurityDoor/20260906-144301-d0740a07/report.json verifies the
  saved mesh in a fresh process, including bounds, materials and one collider.
  Saved/Logs/BH-SecurityDoor-SerializedInspect.log records route-contract PASS.
- Saved/Reports/DoorPresentation-After.png was visually inspected in the rendered
  Editor. The leaf, panels and hardware are visible; existing bright graybox
  lighting remains. This does not prove physical interaction or navigation.
- Saved/Logs/BH-DoorReload-FirstLightSmoke.log records First Light loaded for play
  and exited normally with no logged errors. This is unattended startup only.
- Scoped agents used: bh_code_explorer, bh_architect, bh_cpp_implementer,
  bh_test_engineer, bh_build_debugger and bh_reviewer. Final review found no
  supported blockers; coordinator owned engine execution.

Minimum next PIE checks: inspect the new door, collect the keycard, unlock it
and walk through. Fire some rounds, then reload while actively bleeding; the
reload should finish while health continues to drain. Physical-input and
listen-server host/client proof remain pending; local RPC implementation tests
do not prove network delivery. Attack A acceptance remains in progress.
