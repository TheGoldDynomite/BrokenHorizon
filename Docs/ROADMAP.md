# Broken Horizon - Near-Term Codex Roadmap

This is a prioritized queue for repository work. It is not permission to
implement every item at once. The production-scale direction remains in
Docs/Production/Broken_Horizon_1.0_Roadmap.md and the module implementation
plan.

## 0. Verified development baseline - complete 2026-08-14

- Project Doctor passes with zero errors and zero warnings.
- UE 5.8.0, runtime/editor modules, targets, Build.cs dependencies, source
  ownership, input flow, UI ownership, interaction, door/keycard, objective,
  weapon, and test conventions are documented from the checkout.
- The pre-edit incremental editor build passes.
- Tools/RunTests.ps1 now distinguishes report-backed success from a null
  Unreal process exit code and retains a separate NoTests result.
- Existing automation is documented by its real registrations rather than the
  old planned-only matrix.
- Exact editor/PIE gaps are recorded in EDITOR_HANDOFF.md.

## 1. First Light interaction and operation-route acceptance - next

This is an acceptance/validation slice, not a new gameplay implementation.

- Verify BP_BHCharacter assignments for IMC_Player, MissionData, interaction
  prompt, objective, ammo, combat-status, pause, inventory, and notification
  widget classes.
- Run PIE on L_FirstLight_Graybox through the real objective order:
  FindRedKeycard, UnlockSecurityDoor, EliminateGuard, ReachExtraction.
- Confirm missing/wrong/correct keycard behavior, door prompts, objective
  notifications, extraction completion, and authority behavior with one
  player and then two clients.
- If direct deterministic door tests remain necessary, design a small
  reusable access-policy seam before adding production APIs. Do not use
  reflection or a map asset as a fake unit fixture.

Exit evidence: exact Editor/PIE result, route log markers, and any remaining
PersistenceID/navigation/UI/audio issues recorded in EDITOR_HANDOFF.md.

## 2. Campaign prototype hardening

Use the current production focus in
Docs/Production/ModuleImplementationPlan.md:

- validate attack/defense/raid director routing across sectors;
- verify stable operation IDs and world PersistenceID values across travel;
- validate transport/supply/casualty persistence through host travel,
  reconnect, and failure/recovery;
- close the two-hour multiplayer soak gate.

## 3. Content and presentation readiness

- Replace remaining placeholder meshes/materials and review First Light
  authored assets, audio, navigation, and UI at runtime.
- Validate the already-present weapon foundation with assigned meshes,
  animation, audio, and manual fire/reload/feel review.
- Maintain the restrained HUD/audio baseline and record couch-distance,
  controller, safe-frame, and accessibility review results.

## Roadmap rule

Complete and validate one vertical slice before promoting the next. Builds and
automation prove source and contract behavior; they do not close visual,
interaction-feel, navigation, or multiplayer soak gates.
