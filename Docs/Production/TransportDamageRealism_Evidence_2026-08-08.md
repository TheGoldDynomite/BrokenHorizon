# Transport Damage Realism Evidence

Date: 2026-08-08

## Player-facing result

Field transport damage now has a continuous operational consequence instead of
being only a binary working or immobilized state. Below the critical hull
fraction, forward and reverse mobility degrade progressively and mechanical
strain increases fuel burn. A transport with zero hull still stops through the
existing immobilization path, while the existing crew-damage, repair,
replication, and save/load contracts remain intact.
The native transport label now reports the current mobility percentage so the
player can understand the handling penalty before the vehicle becomes
immobilized.
High-speed collisions now apply capped hull damage through the authoritative
transport damage path, so careless driving can injure the vehicle and its
occupants instead of stopping with no consequence.

## Runtime contract

- `ABHFieldTransport::CalculateHullMobilityMultiplier` keeps undamaged and
  threshold-condition vehicles at full mobility, then interpolates to the
  configured damaged mobility floor.
- `ABHFieldTransport::CalculateHullFuelBurnMultiplier` keeps normal fuel burn
  above the threshold, then interpolates to the configured critical-damage
  penalty.
- The live movement path applies both curves to forward speed, reverse speed,
  and distance-based fuel consumption.
- `GetMobilityPercentage` exposes the live mobility state to Blueprint and the
  native transport label, reporting `0%` for an immobilized vehicle.
- Movement blockage applies collision damage only above the tuned speed
  threshold, with a cooldown and cap to prevent repeated contact from creating
  runaway damage.
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
- That test covers no-damage low-speed contact, threshold scaling, and the
  capped severe-impact damage path.
- The full automation run completed with `83 tests performed`.
- Startup smoke passed.
- First Light smoke passed with
  `BH_TEST_FIRST_LIGHT_PLAYABLE_ROUTE_COMPLETE result=success`.
- First Light navigation fallbacks were `8/12`, within the configured limit.
- The first validation attempt encountered a transient MSVC internal compiler
  error in untouched `BHAmbientWarDirector.cpp`; the immediate retry passed
  without any ambient-war source change.

Evidence logs:

- `Saved/Logs/BHValidation-Build.log`
- `Saved/Logs/BHValidation-Tests.log`
- `Saved/Logs/BHValidation-Smoke.log`
- `Saved/Logs/BHValidation-FirstLight.log`

## Remaining release gap

The native behavior is covered; manual rendered driving, controller feel,
vehicle presentation, lower-tier performance, and packaged-release review are
still required before the transport feature can be marked fully 1.0-ready.
