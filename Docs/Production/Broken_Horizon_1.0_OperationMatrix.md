# Broken Horizon 1.0 authored operation matrix

This matrix is the content gate for the required 1.0 operation breadth. The
native operation variation contract supplies deterministic selection,
save/load, replication, force pressure, approach geometry, and player-facing
route labels. A row is not accepted until its approach, objective layout,
enemy site, briefing, debrief, multiplayer route, and manual review evidence
are complete.

| Family | Variation | Player-facing route and objective | Required authored content | Status |
|---|---|---|---|---|
| Attack | A - Baseline approach | Direct approach to secure an occupied checkpoint | Checkpoint site, two approach lanes, enemy garrison, extraction fallback, briefing/debrief capture | In progress |
| Attack | B - Offset approach | Flanking approach through the rural/water-edge lane before securing the checkpoint | Alternate objective lane, concealment route, water-route decision, adjusted enemy site composition | In progress |
| Defense | A - Hold the facility | Hold a resistance facility through reinforcement waves | Base/facility layout, perimeter positions, fallback line, casualty recovery point | In progress |
| Defense | B - Broken perimeter | Recover a breached outer line and re-establish the defense ring | Alternate breach lane, damaged position state, reinforcement approach, recovery route | In progress |
| Raid | A - Clean sabotage | Recon and sabotage a logistics target without early detection | Depot layout, patrol pattern, sabotage target, quiet exfiltration route | In progress |
| Raid | B - Contested sabotage | Sabotage after contact and break through a reaction force | Alternate target approach, reaction-force site, casualty/exfiltration route, AAR consequence | In progress |
| Resupply | A - Secure convoy | Escort cargo along the protected rural route to a friendly facility | Cargo vehicle staging, route-security contacts, resupply station, delivery consequence | In progress |
| Resupply | B - Water crossing | Move cargo through the authored First Light water route when the direct road is compromised | `FirstLightWaterRoute01`, waterborne transport, shoreline access, boat presentation, alternate delivery route; briefing/status identifies `ROUTE B // WATER CROSSING REQUIRED`; cargo delivery blocks non-waterborne transport | In progress |

## Evidence required for acceptance

- Authored map placement and stable IDs for every site, route, objective, and
  extraction point.
- Briefing identifies the selected route, intelligence quality, force,
  logistics, and major tactical risk.
- Operation status and after-action report explain the selected variation and
  its consequences.
- Two-client completion, late join, reconnect, save/load, and server-travel
  evidence for representative rows.
- In-game captures at gameplay distance, low light/weather, multiplayer, and
  packaged-performance conditions.
- Manual Windows-control review of approach choice, water crossing, contact,
  withdrawal, and debrief readability.

## Current implementation links

- `FBHOpenWorldOperationState.OperationVariationIndex` and `OperationCenter`
  carry the selected variation and objective lane through persistence.
- `ABHOpenWorldOperationDirector::ResolveOperationVariationIndex` provides the
  deterministic selection contract.
- `ROUTE A // BASELINE APPROACH` and `ROUTE B // OFFSET APPROACH` are visible
  in operation status text.
- `ABHWaterSurface`, `FirstLightWaterRoute01`, and the authored stable
  `FirstLightWatercraft01` provide the current water-route foundation.
- `ABHOperationSiteMarker` and `Content/Python/add_first_light_operation_sites.py`
  author the eight stable matrix-site contracts in First Light, with family,
  variation, purpose, and approach labels. These are site/content contracts,
  not final facility geometry or combat acceptance.
- `Content/Python/add_first_light_attack_garrison.py` now applies an idempotent
  five-member Attack A checkpoint garrison at `FirstLightAttackA`, using the
  verified `BP_EnemySoldier` class, stable actor labels/tags, and the
  `FirstLight_AttackA_Checkpoint` objective completion contract. The garrison
  is authored into the map but remains subject to operation-gated activation
  and full combat acceptance.
- `Content/Python/add_first_light_attack_layout.py` now applies the bounded
  eight-point cover/approach layout around the checkpoint. Focused First Light
  smoke after applying the garrison and layout passed with `0/12` navigation
  fallbacks. Final theater art, tactical cover quality, active-operation
  combat, and manual route review remain open.
- `Content/Python/add_first_light_attack_ground.py` now applies an idempotent
  Attack A collision footprint and local navigation volume at the checkpoint;
  `repair_first_light_navigation.py` rebuilds the serialized Recast data after
  authoring. The current focused First Light smoke remains at `0/12`
  navigation fallbacks. This closes only the ground/navigation foundation; it
  does not accept the final terrain, cover, garrison, or operation route.
- The current map test showed that live authored defenders must not be active
  during the canonical free-roam smoke. The accepted design is
  operation-gated garrison activation/spawn from the Attack A site contract;
  the map now retains the authored garrison and bounded cover layout, while
  free-roam remains marker-only and the active-operation director owns the
  combat wave.
- The native operation director now applies the authored Attack A baseline
  formation only during the active first hostile wave: five close candidates
  around the marker are navigation-projected and tied to
  `FirstLight_AttackA_Checkpoint`; free-roam remains marker-only.
- `RoadmapAttackAActiveMP-20260810-051235-Summary.json` and its host log prove
  a requested Attack operation reaches the active-operation multiplayer path
  with client, additional-client, and reconnect snapshots. The strategic
  sector remains `NorthPass`, while the director resolves the authored
  `FirstLightAttackA` marker by family/variation and records
  `BH_OPERATION_AUTHORED_SITE center=V(X=6100.00, Z=40.00)` plus the authored
  formation/objective marker. This accepts authored-site activation and
  replicated startup; objective completion, extraction, persistence, and
  manual combat review remain open.
- `RoadmapAttackACompletionTravelPassing-20260810-052304-Summary.json` now
  proves the expanded Attack continuity fixture: authored First Light
  activation, active Attack commit, First Light-to-open-world server travel,
  transport persistence restoration, automated operation completion after
  travel, and initial/additional/rejoin client snapshots. The fixture uses a
  deterministic completion hook; it does not replace manual approach,
  combat, extraction, or debrief review.
