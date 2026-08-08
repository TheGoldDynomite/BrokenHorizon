# Combat engineering gameplay

Combat engineers carry two reusable inventory slots for command-detonated
charges. Charges are finite, server-authoritative equipment rather than free
context actions.

## Breaching

- Interacting with a locked `BHDoor` without its keycard offers a breaching
  charge when one is carried.
- The charge visibly arms for two seconds. Its owner then uses the Engineering
  control (default `V`) to command detonate.
- A focused breach deals up to 180 damage in a 4.5-meter danger-close radius,
  unlocks and opens the door, and satisfies the established shared unlock
  objective contract.
- The blast does not exclude its owner or teammates. Both sides of a doorway
  must be cleared before firing.

## Area denial

- With no active charge, the Engineering control places a charge on the aimed
  surface within five meters.
- The wider area-denial profile deals up to 140 damage across 6.5 meters.
- Once any owned charge is active, the same control detonates armed charges;
  pressing it during the arming interval reports that the charge is not ready.
- Charges can be shot. Armed charges sympathetically detonate; unarmed charges
  are destroyed. Interaction disarms a charge, and its owner recovers one
  carried charge.
- Placement never sticks a charge directly to a pawn.

## Logistics and persistence

Carried count and active count replicate owner-only. Carried count is saved in
the campaign checkpoint. Placed charges are deliberately consumed if the
player travels away. Friendly sector resupply restores carried charges to two
and spends the station's existing strategic supply cost/cooldown transaction.

## World and AI consequences

Detonation uses visibility-aware radial damage, physical blast danger, and an
8.5-kilometer-equivalent Unreal hearing radius for AI explosion response. Raid
logistics targets can accept engineering-charge sabotage through their existing
operation completion path.

## Validation

`BrokenHorizon.Gameplay.Engineering.CommandCharges` verifies damage-profile
tradeoffs, arming/ownership rules, replicated charge/door/inventory state, and
the save-game contract. `-BHTestEngineeringRuntime` spawns a locked transient
door in First Light, places and arms a charge, command-detonates it, and checks
the door, carried inventory, and active-charge cleanup.

## Manual playtest gates

- Verify charge placement orientation and clipping on final authored door,
  floor, wall, vehicle, and logistics-cache meshes.
- Review charge model, arming indicator, explosion VFX/audio, door animation,
  and danger-close readability in rendered multiplayer.
- Test owner and remote-client placement, disarm, shot detonation, simultaneous
  charges, late join, packet loss, and server travel.
- Confirm AI routes through a breached door after dynamic navigation updates.
- Tune blast radii and damage against casualty/revive pacing and fortifications.
