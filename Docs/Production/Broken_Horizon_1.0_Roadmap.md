# Broken Horizon — 1.0 Roadmap

**Purpose:** Build the game incrementally toward a playable 1.0 milsim campaign.  
**Strategy:** Every milestone extends the same persistent open-world resistance loop.  
**Rule:** A milestone is complete only when it is playable, persistent, multiplayer-safe where applicable, and supported by evidence.

## 1. The 1.0 target

Version 1.0 is a cooperative open-world milsim campaign in which players lead a
small resistance force against an active occupation. Players plan operations,
travel through one continuous region, use real-world tactical principles,
manage casualties and logistics, and return to a campaign whose territory,
force readiness, supplies, and enemy response have changed.

The unique identity is the living resistance force: the people, vehicles,
equipment, bases, routes, and supplies that survive one operation determine what
the squad can field next.

### 1.0 minimum player-facing breadth

The 1.0 target is not only a networked systems sandbox. It must feel like a
complete cooperative milsim with enough physical content to support planning,
movement, contact, sustainment, and recovery. The minimum release contract is:

- **Actual theater:** one authored regional map with a readable tactical map,
  named sectors, roads, trails, bridges, rivers or lakes, shoreline access,
  elevation, forests, rural land, settlements, industrial space, military
  sites, resistance bases, checkpoints, depots, and extraction areas. Water is
  gameplay space with crossings, concealment, shoreline movement, and at least
  one water-accessible operation or route decision.
- **Vehicles and mobility:** infantry movement plus drivable cars/light
  transports, a cargo or logistics vehicle, and at least one armored vehicle or
  tank-class threat. Vehicles need entry/exit, seats, inventory/cargo,
  condition/damage, fuel, traction, recovery/repair, passenger replication,
  recognizable audio/VFX, and a tactical reason to use or avoid them. Boats or
  watercraft are required if the final map makes water a meaningful route;
  otherwise the water scope must be explicitly reduced before content lock.
- **Weapons and equipment:** a coherent small arsenal rather than one rifle:
  at minimum a service rifle, carbine or SMG, light machine gun, designated
  marksman weapon, pistol, shotgun, fragmentation grenade, smoke grenade, and
  one anti-vehicle or engineering option. Each weapon needs authored mesh,
  materials, animation set, reload/fire behavior, recoil/audio/VFX, ammunition
  type, handling role, and multiplayer/HUD readability.
- **Inventory and loadout:** a real pre-deployment and in-operation inventory
  showing carried items, weapon slots, magazines, medical supplies, grenades,
  tools, batteries or mission items, weight/bulk, container capacity, and
  resupply. Inventory state must replicate correctly, persist when intended,
  affect movement/vehicle capacity, and explain what the player can field next.
- **Loot and salvage:** searchable or recoverable battlefield items with clear
  ownership/state rules. Loot must support pickup, transfer, discard, consume,
  resupply, persistence boundaries, and HUD feedback. The system should create
  tactical choices—recover ammunition, tools, intelligence, medical stock, or
  equipment—rather than function as a cosmetic pickup counter.
- **Animation and character presentation:** project-owned or intentionally
  licensed first-person arms, weapon handling, equip/swap, aim, fire, reload,
  sprint, crouch, prone or low-profile movement where supported, vault/obstacle
  interaction, casualty care, vehicle entry/exit, hit reactions, death,
  surrender/custody, and enemy combat locomotion. Animation state must agree
  with authoritative gameplay state under interruption and latency.
- **Milsim information layer:** briefing, map planning, grid or landmark-based
  navigation, compass/bearing, squad markers, contact reports, radio/bark
  discipline, uncertain intelligence, casualty status, logistics status, rules
  of engagement where applicable, and an after-action report that explains the
  operation's consequences.

These are 1.0 scope pillars, not optional polish tasks. A larger fleet, naval
warfare, aircraft, extensive weapon catalog, or fully simulated logistics
economy can remain post-1.0, but the minimum breadth above must be playable,
authored, replicated where shared, and represented in the content inventory.

## 2. Development rules

- Build the smallest complete loop before adding breadth.
- Keep the open-world map as the shared foundation for every operation.
- Prioritize player-facing loops over isolated mechanics.
- Use real tactical principles: observation, fire and movement, suppression,
  cover, concealment, communication, sustainment, casualty care, and withdrawal.
- Abstract administrative detail when it does not create a meaningful decision.
- Preserve authoritative replication and persistence contracts from the start.
- Do not expand content until the current milestone survives a manual playtest.

## 3. Milestones

### M0 — Direction and contracts

**Goal:** Lock the game’s strategic and tactical identity.

Deliverables:

- Resistance-versus-occupation campaign specification
- Open-world sector, route, facility, and streaming model
- Persistent force model specification
- Tactical information and realism rules
- First 1.0 map scope and operational geography
- Canonical save, objective, persistence, and map contracts

**Exit gate:** The team can describe what happens from deployment through
debrief and explain what changes in the next deployment.

### M1 — Playable open-world base

**Goal:** Establish one continuous region that is fun to move through.

Deliverables:

- Resistance headquarters or hideout
- Friendly, contested, and enemy-controlled locations
- Roads, trails, observation positions, and fallback routes
- Enemy checkpoints and patrol areas
- Supply, medical, repair, and vehicle staging points
- Stable streamed sectors and authored navigation
- Basic reconnaissance and tactical map presentation
- Authored terrain, water, roads, bridges, landmarks, and navigable routes;
  the map cannot remain a First Light-scale graybox

**Exit gate:** Two players can deploy, travel, observe, contact an enemy,
withdraw, and return to base without a disconnected mission transition.

### M2 — First authentic operation

**Goal:** Prove the real-world tactical loop.

Deliverables:

- Briefing with objective, intelligence quality, force, time, and constraints
- Deployment roster and equipment selection
- Reconnaissance and approach choice
- Fire-and-movement combat
- Suppression, cover, concealment, and contact reporting
- Casualty stabilization and extraction decisions
- Objective completion, failure, withdrawal, and debrief

**Exit gate:** A two-player squad succeeds through communication and tactical
movement rather than rushing or exploiting enemy behavior.

### M3 — Living resistance force

**Goal:** Make the squad’s organization the persistent character.

Deliverables:

- Persistent operators and readiness
- Experience, specialties, injuries, recovery, and loss
- Vehicles and maintenance state
- Equipment, ammunition, fuel, and medical stock
- Bases, facilities, and staging capacity
- Deployment roster built from actual campaign resources
- Persistent inventory, loadout, equipment ownership, loot/salvage, and
  carried-load consequences
- Save/load, late join, reconnect, and server travel continuity

**Exit gate:** A completed or failed operation visibly changes the next
deployment roster, available equipment, or operational capability.

### M4 — Active occupation

**Goal:** Make the enemy a strategic opponent.

Deliverables:

- Garrisons, patrols, convoys, and reinforcement routes
- Enemy resupply and strategic readiness
- Search operations and counterattacks
- Fortification and retaking behavior
- Escalation based on resistance activity
- Far-field strategic simulation for distant sectors

**Exit gate:** The world changes while players are away, and enemy behavior is
understandable as a response to the campaign state.

### M5 — Logistics and sustainment

**Goal:** Make the open world and tactical plan depend on sustainment.

Deliverables:

- Resupply routes and cargo movement
- Fuel, repair, and medical logistics
- Convoy and route-security operations
- Forward bases and captured facilities
- Emergency fallback supplies
- Supply consequences for force readiness and operation selection
- Vehicle fleet loop covering transport, cargo, fuel, damage, repair, recovery,
  and at least one armored threat; watercraft are included when the theater
  requires water travel

**Exit gate:** Players must choose between speed, safety, force size, equipment,
and sustainment before committing to an operation.

### M6 — Campaign alpha

**Goal:** Run a complete limited-region resistance campaign.

Deliverables:

- Attack/liberation
- Defense
- Raid
- Resupply
- Recruitment/recovery
- World-state-driven operation generation
- Campaign victory and defeat conditions
- Strategic debrief and next-operation planning

**Exit gate:** A campaign can progress from weak resistance to a meaningful
victory or defeat without save loss, strategic dead ends, or repetitive forced
mission order.

### M7 — Multiplayer and scale hardening

**Goal:** Make the campaign reliable for real cooperative sessions.

Deliverables:

- Two-to-four-player sessions
- Late join and reconnect during operations
- Host recovery and server travel
- Replicated AI, vehicles, supplies, casualties, and world state
- Sector streaming under representative AI and player density
- Long-session and network-impairment validation
- Packaged build performance validation

**Exit gate:** A complete campaign segment survives multiplayer travel,
reconnect, save/load, and extended play without state divergence or corruption.

### M8 — Content, presentation, and accessibility

**Goal:** Turn the proven systems into a coherent 1.0 experience.

Deliverables:

- A shippable theater slice with a clear visual language: one resistance
  heartland, three contested sectors, one enemy stronghold, one settlement,
  one industrial/logistics site, one rural route, one dense built-up route,
  and one wilderness/observation route. Every location needs a landmark,
  gameplay purpose, readable approach routes, authored navigation, and a
  before/after world-state change.
- Faction identity visible in world materials, signage, uniforms, vehicles,
  props, lighting, UI treatment, radio voice, and battlefield effects. No
  critical player route may rely on graybox-only geometry or an unassigned
  placeholder material/audio cue at release.
- A minimum authored content set: four operation families (attack, defense,
  raid, resupply), two authored variations for each, one recruitment/recovery
  operation, three route-event types, three enemy site layouts, three base or
  facility layouts, and one campaign victory plus one defeat presentation.
  Systemic variation may extend this set but cannot substitute for the
  authored scenarios required for onboarding and marketing captures.
- A presentation pass covering sky, time-of-day intent, weather readability,
  terrain/material breakup, vegetation/prop density, decals and damage,
  landmark silhouettes, VFX, ambient life, distant war activity, weapon tails,
  radio/barks, UI feedback, and performance-aware streaming. Each shipping
  sector gets a signed visual review with screenshots and a defect list.
- A dedicated asset-production pass, tracked by asset rather than by feature:
  project-owned first-person arms and weapon presentation; final enemy and
  resistance character silhouettes with uniforms and equipment variants;
  modular building, road, fence, checkpoint, depot, base, and interior kits;
  terrain, rock, vegetation, prop, vehicle, supply, medical, and interaction
  assets; and authored muzzle, impact, smoke, weather, damage, and extraction
  VFX. The release inventory must identify the source, owner, license status,
  import status, material status, LOD/collision status, map usage, and review
  status for every shipping asset.
- A texture/material quality pass with a documented texel-density target,
  trim-sheet and tileable-material plan, packed masks, normal maps, roughness
  variation, physical-material assignments, decals, wetness/mud/damage states,
  and platform texture groups. Hero assets need unique authored materials;
  modular environment assets may share trim/tile materials only when they
  retain believable variation. No critical route may depend on engine cubes,
  default-color materials, stock mannequin presentation, or template-named
  weapon/audio assets.
- Asset acceptance evidence for each category: in-editor close, gameplay
  distance, low-light/weather, multiplayer, and packaged-performance captures;
  clean asset validation; correct sockets/animation/material slots; authored
  LODs and collision; and a documented decision for every remaining single-LOD,
  placeholder-named, or externally sourced candidate.
- World audio, radio, weather, distant combat, tactical cues, objective
  feedback, casualty feedback, and support effects assigned to real assets and
  reviewed in context. The assignment audit, mix pass, and representative
  rendered captures are release evidence; source-side cue declarations alone
  are not.
- Briefing, intelligence, deployment, operation selection, debrief, campaign
  overview, and next-deployment UX that explains what changed and why. A new
  player must understand the objective, available force, key risks, and
  campaign consequence without reading external documentation.
- Controller support, scalable HUD, subtitles, localization-safe layout,
  comfort options, high-contrast/color-independent cues, and a couch-distance
  review at 720p, 1080p, and 4K.
- Two balance passes: one for ordinary cooperative players learning the loop
  and one for experienced milsim players seeking pressure. Both passes must
  record completion rate, casualty rate, resource state, operation duration,
  and qualitative findings.

**Exit gate:** Manual playtests find problems in quality and balance rather
than missing core systems or broken campaign continuity. The content owner
must also sign off the theater inventory, the authored operation matrix, the
visual/audio assignment audit, and the player-facing briefing-to-debrief loop.

### M9 — 1.0 release candidate

**Goal:** Prove the shipped product.

Required evidence:

- Fresh packaged build
- Content lock for the release theater, operation matrix, faction set, and
  player-facing onboarding/debrief flow
- Final-art/placeholder audit with zero unresolved critical-route placeholders
- Visual review captures for every shipping sector, base/facility type, and
  operation family
- Audio/FX assignment and in-context mix review for every required player loop
- Full campaign completion and defeat paths
- Two-hour or longer cooperative campaign soak
- Save/load, travel, late join, reconnect, and host recovery
- Navigation and streaming review
- Lower-tier performance review
- Controller and accessibility review
- Audio, visual, localization, and couch-distance review
- No unresolved blocker defects in the canonical 1.0 route

**Release bar:** 1.0 is not a systems-complete build with a graybox map. It is
the smallest complete campaign that looks intentional, sounds authored, gives
players enough distinct places and operations to form memories, and clearly
shows the consequences of their decisions. Any item deferred from this bar
must be named explicitly as post-1.0 content rather than silently counted as
roadmap breadth.

**Current art warning:** The existing readiness evidence still identifies a
stock Quinn-derived enemy mesh, a template-named rifle fire asset, no complete
project-owned environment kit, no complete project-owned VFX set, and major
missing audio assignments. Those are content-production blockers even when the
assets pass technical validation. They must be resolved or explicitly removed
from the 1.0 contract before release-candidate signoff.

## 4. The first build slice

The first implementation slice after M0 is:

```text
Resistance base
    → deployment roster
    → open-world travel
    → reconnaissance and contact
    → tactical objective
    → casualty / ammunition / vehicle consequences
    → return or withdrawal
    → debrief and save
    → changed next deployment
```

This slice should be implemented before adding large numbers of weapons,
operation types, enemy archetypes, or map landmarks.

## 5. Working cadence

Each increment should follow this cycle:

1. Select one milestone slice and define its player-visible outcome.
2. Inspect existing implementation and preserve unrelated worktree changes.
3. Implement the smallest complete source/content path.
4. Add focused automation or validation evidence.
5. Run build, relevant smoke, and First Light/open-world checks.
6. Manually play the affected route.
7. Record what is proven, playable, in progress, and still blocked.
8. Only then select the next increment.

## 6. Immediate next increment

Before adding another isolated mechanic, create a 1.0 content inventory and
visual-quality board, then map the persistent resistance-force specification to
the current systems. The first concrete contracts should cover:

- Operator identity, status, specialty, and availability
- Vehicle identity, condition, location, and readiness
- Equipment and supply ownership
- Facility ownership and function
- Deployment roster composition
- Operation outcome and debrief changes
- Save/load and replicated campaign representation

The content inventory should list every release sector, landmark, facility,
operation variation, enemy site, base, authored event, UI screen, required
audio/FX cue, and screenshot/review owner. Each row must have a status of
`placeholder`, `playable`, `art-complete`, `audio-complete`, or `release-signed`.
