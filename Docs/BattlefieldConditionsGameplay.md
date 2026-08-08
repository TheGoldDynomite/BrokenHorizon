# Battlefield Conditions Gameplay

Broken Horizon now treats weather and time of day as synchronized battlefield state rather than decoration. The current condition is derived deterministically from the persistent-war turn, so save/load and multiplayer clients resolve the same tactical environment.

## Player-visible effects

- Rain reduces sight, masks footsteps, increases weapon dispersion, slows infantry, reduces vehicle traction, raises fuel use, and widens mortar patterns.
- Dense fog sharply reduces visual detection while making nearby movement comparatively more important.
- Severe storms combine poor visibility, heavy acoustic masking, unstable weapon handling, slow movement, low traction, high fuel demand, and the least accurate indirect fire.
- Dawn, dusk, and night apply additional low-light detection and handling penalties.
- A turn-change warning reports the condition and key tactical multipliers.
- Existing ambient-war wind and rain audio mixes follow the campaign condition.

The values model tactical consequences at game scale. They deliberately do not claim to be a meteorological or vehicle-physics simulation.

## Contracts

- `UBHBattlefieldConditions::BuildProfileForTurn` is deterministic and is the single gameplay source of truth.
- The replicated war turn is the synchronization and persistence contract.
- AI and players use the same environmental weapon penalty.
- Environmental penalties stack with injury, fatigue, posture, movement, suppression, and combat-readiness penalties.
- Weather never grants perfect concealment; normal collision, line-of-sight, smoke, hearing, and combat memory still apply.

## Manual review required

- Confirm wind/rain audio assets are assigned and balanced in the active map actor.
- Confirm each low-visibility condition remains readable on representative monitor brightness settings.
- Review AI combat at fog/night sight limits and vehicle steering in rain/storm.
- Review the turn-change notification for keyboard/controller and accessibility scaling.
- Production precipitation, wet surfaces, clouds, lightning, and fog presentation require authored assets and map lighting review; source gameplay does not fabricate those binary assets.
