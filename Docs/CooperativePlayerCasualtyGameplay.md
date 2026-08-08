# Cooperative player casualty gameplay

Multiplayer lethal damage now creates a rescue opportunity before the existing
campaign casualty and field-redeployment path.

## Player loop

1. The first lethal event in a multiplayer life incapacitates the operator for
   45 seconds. Movement and weapon actions stop, but the player can still look.
2. Additional lethal damage or an expired timer causes final death immediately.
3. A teammate within interaction range spends one field dressing to stabilize
   the casualty and stop the bleed-out timer.
4. A second interaction spends one medkit and revives the player at 35 health.
5. A revived player retains the one-down-per-life limit and must reassess any
   remaining limb injuries. This prevents endless revive chains.
6. If rescue fails, the existing replacement-operator redeployment and
   persistent-war casualty consequence continue unchanged.

Standalone play retains the immediate death/respawn behavior so a solo player
is not forced to wait through an impossible rescue window.

## Multiplayer authority

Incapacitation, stabilization, bleed-out deadline, medical supply spending,
revive health, and final death are server-authoritative. Incapacitated,
stabilized, and deadline state replicate. Interaction is re-traced and distance
validated on the server before either medical item is consumed.

## Validation

`BrokenHorizon.Gameplay.Coop.PlayerCasualty` verifies entry rules, the
one-down-per-life invariant, health revival, and replicated state contracts.
The non-shipping `-BHTestPlayerCasualtyRuntime` probe performs the entire live
sequence: lethal damage, incapacitation, teammate spawn, field-dressing spend,
stabilization, medkit spend, and 35-health revival.

## Manual playtest gates

- Run a listen server plus remote client and complete both treatment stages in
  each ownership direction under simulated latency and packet loss.
- Confirm interaction prompts remain readable around overlapping bodies,
  fortifications, vehicles, and dropped equipment.
- Review downed posture, animation transitions, collision capsule, camera
  height, and third-person readability; source validation cannot approve feel.
- Verify disconnect, host migration/session loss, travel, and operation debrief
  while a player is incapacitated or stabilized.
- Tune the 45-second window and 35-health recovery against actual combat pace.
