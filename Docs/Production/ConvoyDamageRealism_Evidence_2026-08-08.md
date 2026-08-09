# Convoy Damage Realism Evidence

Date: 2026-08-08

## Player-facing result

Supply convoys now lose route speed progressively as their integrity falls.
An ambushed but still-living convoy therefore takes longer to clear its
corridor, increasing pressure on time-critical escort operations before the
existing destruction, salvage, and deadline-resolution paths take over.

## Runtime contract

- `ABHSupplyConvoyTarget::CalculateRouteSpeedMultiplier` preserves full speed
  above the critical integrity threshold and interpolates to a configured
  damaged-speed floor below it.
- `GetRouteSpeedMultiplier` exposes the live route penalty to Blueprint and
  returns `0%` for a convoy with no remaining health.
- Both world-route and direct-destination movement use the effective damaged
  speed, so the consequence is consistent across route variants.
- Existing authority-owned convoy state, operation deadlines, escort
  resolution, destruction, salvage, and persistence remain the owning systems.

## Automated evidence

Command:

`Validate-BrokenHorizon.cmd -Build -Tests -Smoke -FirstLight`

Results:

- UE 5.8 `BrokenHorizonEditor Win64 Development` build passed.
- `BrokenHorizon.Gameplay.Convoy.DamageSpeed` completed with
  `Result={Success}`.
- The full automation run completed with `84 tests performed`.
- Startup smoke passed.
- First Light smoke passed with
  `BH_TEST_FIRST_LIGHT_PLAYABLE_ROUTE_COMPLETE result=success`.
- First Light navigation fallbacks were `8/12`, within the configured limit.

Evidence logs:

- `Saved/Logs/BHValidation-Build.log`
- `Saved/Logs/BHValidation-Tests.log`
- `Saved/Logs/BHValidation-Smoke.log`
- `Saved/Logs/BHValidation-FirstLight.log`

## Remaining release gap

The native route consequence is covered. Manual rendered convoy pacing,
controller/UI readability, long-session deadline balance, lower-tier
performance, and packaged-release review remain required before the convoy
operation can be marked fully 1.0-ready.
