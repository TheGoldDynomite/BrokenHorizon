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
