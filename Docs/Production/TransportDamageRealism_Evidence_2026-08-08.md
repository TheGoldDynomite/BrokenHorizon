# Transport Damage Realism Evidence

Date: 2026-08-08

## Player-facing result

Field transport damage now has a continuous operational consequence instead of
being only a binary working or immobilized state. Below the critical hull
fraction, forward and reverse mobility degrade progressively and mechanical
strain increases fuel burn. A transport with zero hull still stops through the
existing immobilization path, while the existing crew-damage, repair,
replication, and save/load contracts remain intact.

## Runtime contract

- `ABHFieldTransport::CalculateHullMobilityMultiplier` keeps undamaged and
  threshold-condition vehicles at full mobility, then interpolates to the
  configured damaged mobility floor.
- `ABHFieldTransport::CalculateHullFuelBurnMultiplier` keeps normal fuel burn
  above the threshold, then interpolates to the configured critical-damage
  penalty.
- The live movement path applies both curves to forward speed, reverse speed,
  and distance-based fuel consumption.
- The calculation functions clamp invalid fractions and multipliers so
  Blueprint-authored tuning cannot create negative mobility or reduced fuel
  burn from damage.

## Automated evidence

Command:

`Validate-BrokenHorizon.cmd -Build -Tests -Smoke -FirstLight`

Results:

- UE 5.8 `BrokenHorizonEditor Win64 Development` build passed.
- `BrokenHorizon.Gameplay.Transport.DamageMobility` completed with
  `Result={Success}`.
- The full automation run completed with `83 tests performed`.
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

The native behavior is covered; manual rendered driving, controller feel,
vehicle presentation, lower-tier performance, and packaged-release review are
still required before the transport feature can be marked fully 1.0-ready.
