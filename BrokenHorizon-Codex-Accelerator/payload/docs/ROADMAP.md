# Broken Horizon — Near-Term Roadmap

This is a prioritized queue, not permission to implement every item at once.

## 0. Verified development baseline — active

- Run Project Doctor.
- Map the current modules, classes, assets referenced by code, and input flow.
- Establish an incremental editor build.
- Add low-risk automation tests that match the actual APIs.
- Record exact Editor-only validation still needed.

## 1. Weapon foundation — likely next; verify first

One playable hitscan weapon with focused component ownership, magazine/reserve ammunition, reload state, hit results, Blueprint presentation hooks, tests, and exact Editor wiring.

Out of scope for the first slice: attachments, inventory, weapon switching, final animations/audio, projectile weapons, AI combat, and multiplayer replication.

## 2. Damage and health foundation

Only after the weapon API is stable: damage application, health state, death/downed policy, and testable transitions.

## 3. Inventory and equipment

Only after ownership requirements are known: items, keycards, weapons, ammunition, and UI boundaries.

## 4. AI combat vertical slice

Perception, target selection, weapon use, cover/suppression behavior, and bounded performance validation.

## Roadmap rule

Complete and validate one vertical slice before promoting the next. Update this file when priorities actually change.
