# Broken Horizon 1.0 content-breadth inventory

**Purpose:** Track the player-facing content required by the 1.0 roadmap. This
inventory is a release gate, not a wishlist. A row reaches **Accepted** only
when it is authored, playable, replicated where shared, validated in a cooked
build, and reviewed in-game.

## Status vocabulary

- **Planned:** required by the roadmap; production work has not started.
- **In progress:** implementation or authored content exists, but the exit
  evidence is incomplete.
- **Playable:** usable in the current build, with manual/content review open.
- **Accepted:** all applicable source, runtime, multiplayer, packaged, and
  visual/content evidence is complete.
- **Deferred:** explicitly moved beyond 1.0 with a recorded scope decision.

## Actual theater

| Area | 1.0 requirement | Status | Acceptance evidence / next action |
|---|---|---|---|
| Regional map | One authored continuous region with named sectors | In progress | Replace First Light-scale graybox presentation with authored theater review |
| Tactical map | Readable sectors, routes, landmarks, grids or bearings | Playable | Manual planning and couch-distance review |
| Terrain and routes | Elevation, forests, rural land, settlements, industrial space | In progress | Asset and route inventory; authored traversal captures |
| Water | River/lake, crossings, concealment, shoreline access | In progress | Reusable `ABHWaterSurface` is authored into First Light as `FirstLightWaterRoute01`; its player-facing mesh is now a thin surface plane while the tall overlap volume retains wading detection; characters receive water footsteps and a 0.60 configurable wading-speed multiplier; shoreline art, material, traversal review, and gameplay operation remain |
| Facilities | Bases, checkpoints, depots, military sites, extraction areas | In progress | Assign each facility an authored kit, gameplay purpose, and screenshot owner |
| Navigation | Authored NavMesh across player-critical routes | Playable | First Light fallback classification now treats superseded path requests as intentional aborts; editor smoke, two-client Windows route, and packaged First Light all record `0/12` navigation fallbacks; manual traversal and broader campaign-route review remain |

## Vehicles and mobility

| Category | 1.0 requirement | Status | Acceptance evidence / next action |
|---|---|---|---|
| Cars/light transport | Drivable infantry transport with seats and entry/exit | Playable | Manual driving, terrain, replication, and controller review |
| Cargo/logistics vehicle | Cargo capacity, resupply, fuel, repair, recovery | Playable | Validate complete convoy and resupply loop in authored routes |
| Armored threat | At least one armored or tank-class enemy threat | In progress | First Light now contains authored `FirstLightArmoredThreat01`; native foundation provides replicated armor integrity, frontal/rear damage multipliers, mobility disable state, stable ID, contact acquisition, and state telemetry; both clients now have Windows-controlled replicated startup/contact evidence; a modular hull/turret/barrel silhouette uses the Broken Horizon military material; final authored vehicle art, anti-vehicle feel, VFX/audio, and manual presentation remain |
| Watercraft | Required only if water remains a meaningful route | In progress | Stable, deduplicated `FirstLightWatercraft01` is staged at `(5000,300,110)` near the west edge of `FirstLightWaterRoute01`, with waterborne mode and a 15.0-supply `WesternFOB` to `EasternDepot` cargo load; `ABHFieldTransport` exposes modular hull/deck/console presentation components, Windows-controlled boarding evidence, and a passed occupied-player authoritative delivery route (`BHWatercraftOccupiedFixture3-20260809-235051`); final boat mesh/material, shoreline access, natural crossing, VFX/audio, and manual water traversal remain |

## Weapons and equipment

| Category | Minimum 1.0 breadth | Status | Acceptance evidence / next action |
|---|---|---|---|
| Service rifle | Authored mesh, material, animation, audio/VFX, ammo, HUD role | Playable | `BP_Rifle` now uses project-owned `SW_FirstLight_WeaponFire` rather than the template-named fire cue; Windows-launched runtime confirms project-owned fire/dry/reload cues loaded; final mesh/material/LOD, muzzle/impact VFX, and manual feel/mix review remain |
| Carbine/SMG | Distinct close-range handling role | In progress | Native CARBINE role now has a distinct close-range rifle profile, cycle path, carried-load weighting, save-compatible enum append, and HUD/inventory role exposure; authored weapon mesh/material/animation/audio and manual feel review remain |
| Light machine gun | Sustained-fire and carried-load role | In progress | Native SUPPORT role provides sustained automatic fire, 60-round capacity, heat/reload tradeoffs, suppression tuning, and carried-load weighting; authored mesh/material/animation/audio and manual feel remain |
| Marksman weapon | Precision role and readable optics/ballistics | In progress | Native `EBHWeaponRole::Marksman` already has a distinct semi-automatic precision profile (15-round magazine, 42 damage, 80,000 UU range, tighter ADS spread, lower falloff); focused role-profile automation covers its capacity/damage/accuracy contract and the shared loadout path cycles through it; authored DMR mesh, optic/material, animation, audio/VFX, and Windows-controlled feel review remain |
| Pistol | Sidearm and emergency role | In progress | Native PISTOL role now has a distinct semi-automatic sidearm profile, cycle path, carried-load weighting, save-compatible enum append, and HUD/inventory role exposure; authored sidearm mesh/material/animation/audio and manual draw/reload feel remain |
| Shotgun | Close-range breaching/defensive role | In progress | Native SHOTGUN role now has a distinct low-capacity semi-automatic breaching profile, eight authoritative pellet traces per shell, close-range falloff, carried-load weighting, save-compatible enum append, and runtime role fixture; authored mesh/material/animation/audio and manual feel remain |
| Grenades | Fragmentation and smoke | Playable | Smoke visual density/wind and multiplayer readability remain open |
| Anti-vehicle/engineering | One option against armor or obstacles | In progress | Breach engineering charges and a player-facing `Y`-bound anti-vehicle projectile now connect to the armored damage contract; authored launcher presentation, impact VFX/audio, multiplayer, and Windows-control review remain |

## Inventory, loadout, and salvage

| System | 1.0 requirement | Status | Acceptance evidence / next action |
|---|---|---|---|
| Pre-deployment loadout | Slots, magazines, medical, grenades, tools, mission items | Playable | Native loadout panel, `I` toggle, and `F6` primary-role cycling now expose and change the live combat load; mission-item count and field-container capacity are visible; full slot editing and per-item mission controls remain open |
| In-operation inventory | Pickup, transfer, discard, consume, container capacity | Playable | Native salvage pickup supports authoritative partial recovery and persistence boundaries; inventory panel exposes server-authoritative discard controls for ammo, frag, smoke, engineering, and anti-vehicle items into timed runtime pickups with HUD feedback, plus mission-item count and current/remaining field-container capacity; two-client Windows route evidence proves authoritative transfer and consumed-state replication (`BHInventoryTransferRuntimeWindows2-20260810-034819`); camera-facing HUD, broader container coverage, and manual interaction review remain |
| Carried load | Weight/bulk affects movement and vehicle capacity | Playable | Runtime panel records live weight/load state; validate movement, cargo, persistence, and controller readability |
| Resupply | Bases, stations, cargo, and operation consequences | Playable | Complete end-to-end resupply decision review |
| Loot/salvage | Searchable battlefield recovery with ownership/state rules | In progress | First Light now has an authored `FirstLightSalvageCache01` ammunition cache; native pickup supports ammo, frag, smoke, and engineering recovery with stable IDs, partial-capacity consumption, and HUD feedback; the current two-client route proves both clients observe consumed state and a rejoining client receives the authored salvage state (`BHSalvageLateJoinCurrent-20260809-235221`); camera-facing HUD capture and broader transfer/container/manual review remain |
| Persistence | Intended inventory and equipment survive save/load/reconnect | Playable | Save-game round-trip passed in `BHCurrentInventoryPersistenceRoundTrip3`; two-client Windows continuity passed inventory transfer, consumed-loot state, rejoin salvage state, and inherited completed mission state; broader campaign soak remains |

## Character, animation, and presentation

| Category | 1.0 requirement | Status | Acceptance evidence / next action |
|---|---|---|---|
| First-person arms | Project-owned or intentionally licensed arms | In progress | Resolve current asset ownership and presentation review |
| Weapon handling | Equip, swap, aim, fire, reload, interruption | Playable | Dedicated-client timing and manual feel review |
| Movement | Sprint, crouch, prone/low-profile, vault where supported | Playable | Full traversal and animation continuity review |
| Casualty care | Treatment, stabilization, vehicle entry/exit, recovery | Playable | Manual two-ally interaction and animation handoff review |
| Enemy presentation | Combat locomotion, hit reaction, surrender, death, custody | In progress | `ABHEnemySoldier` now adds project-material plate/radio/helmet silhouettes with Rifleman, lighter Scout, and heavier Gunner archetype proportions; Windows-launched First Light confirms the authored Rifleman kit; the Quinn-derived/placeholder skeletal mesh remains the single shipping-referenced asset blocker, with final uniform/material variants, animation, LOD/collision, hit-reaction, and manual review open |
| Materials and textures | Texel density, trim/tile plan, masks, normals, roughness, wetness/mud/damage | In progress | Current asset audit covers 259 assets with zero errors, zero oversized/non-streaming texture violations, zero unknown static-mesh LODs, and zero collisionless static meshes; authored texel-density/material/look pass and visible mip review remain open |
| Meshes and environment kit | Buildings, roads, fences, checkpoints, depots, bases, interiors, props | In progress | `ABHWorldKitModule` provides a stable-ID, Blueprint-facing modular contract with distinct compact checkpoint, wide depot, and large resistance-base proportions; idempotent authoring places `FirstLightWesternFOBKit01`, `FirstLightDovrenCheckpoint01`, and `FirstLightEasternDepotKit01`, and the Windows-launched authored-map session confirms all three variants with six valid components; project-owned mesh/material/LOD/collision review, signage fidelity, interiors, and visual capture remain |
| VFX and audio | Muzzle, impact, smoke, weather, damage, extraction, radio/war layer | In progress | Current readiness audit passes with zero errors and required coverage 18/18; optional authored coverage is 9/27. Water-route characters resolve to the project water footstep cue through the authoritative footstep path. Remaining optional muzzle/impact and enemy/guard bark assignments, authored VFX set, packaged mix, and manual spatial review remain open |

## Operations and information layer

| Category | 1.0 requirement | Status | Acceptance evidence / next action |
|---|---|---|---|
| Operation families | Attack, defense, raid, resupply | In progress | `Docs/Production/Broken_Horizon_1.0_OperationMatrix.md` now defines two authored rows per family, including the water-crossing resupply route; native variation contract is ready, authored sites/routes and review evidence remain |
| Briefing/planning | Map, route, objectives, ROE, logistics, uncertain intelligence | In progress | Operation status now identifies `ROUTE A // BASELINE APPROACH` or `ROUTE B // OFFSET APPROACH`; manual planning-to-debrief continuity and authored route review remain |
| Tactical information | Compass, contacts, radio/barks, casualty and logistics status | Playable | Subjective cadence/readability review |
| After-action report | Explains operation consequences and next deployment | In progress | Verify supplies, casualties, territory, and enemy response are explained |

## Release gate summary

The current highest-risk breadth blockers are the authored theater and water
decision, the armored threat, the broader weapon roster, the complete inventory
and salvage loop, project-owned environment/character/weapon presentation, and
asset-level acceptance evidence. Systems-only validation does not close these
rows. Each completed row must link its build, runtime, multiplayer, packaged,
and manual review evidence before 1.0 signoff.
