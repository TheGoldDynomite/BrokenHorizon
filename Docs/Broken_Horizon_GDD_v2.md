# Broken Horizon — Game Design Document v2

**Status:** Production direction and planning baseline  
**Date:** 2026-08-09  
**Engine:** Unreal Engine 5.8  
**Audience:** Design, engineering, art, audio, QA, and production

## 1. Purpose

This document is the single planning baseline for Broken Horizon. The modular
GDD suite remains useful as detailed design reference, and the production
evidence files remain the source of truth for what is implemented. This GDD
answers the higher-level questions those documents do not answer in one place:

- What experience are we building?
- What must every feature reinforce?
- What is the intended player loop?
- What is in scope for the first shippable campaign?
- What should be built next, and what is explicitly deferred?

When a small fix competes with a larger feature, the feature that strengthens
the core loop, closes a player-facing loop, or removes a release risk takes
priority.

## 2. Product definition

Broken Horizon is a cooperative multiplayer tactical FPS about a small squad
trying to change the outcome of a regional war over a persistent campaign.
Players do not simply clear disconnected levels. They prepare an operation,
make decisions under pressure, carry the consequences home, and return to a
world whose sectors, routes, supplies, casualties, and enemy pressure have
changed.

### Player promise

The squad should feel that survival and success come from preparation,
communication, disciplined violence, and mutual support—not from arcade
mobility or disposable lives.

### Experience targets

- **Tactical clarity:** Players can understand threats, objectives, supplies,
  casualties, and squad intent without losing immersion.
- **Consequential action:** Ammunition, injuries, vehicles, routes, and dead
  enemies matter after the immediate firefight.
- **Cooperative dependence:** Every player can contribute, but the best outcome
  requires sharing information, treating casualties, moving supplies, and
  covering one another.
- **Systemic operations:** Missions are authored around clear objectives but
  are reshaped by weather, AI behavior, logistics, transport, and war state.
- **Persistent continuity:** A checkpoint, disconnect, server travel, or late
  join should preserve the authoritative campaign state.

## 3. Design pillars

### 3.1 Squad before superhero

Movement, combat, medical care, resupply, and command decisions are tuned for a
team operating together. The game should reward bounding movement, covering
fire, casualty recovery, and deliberate positioning.

### 3.2 Information is a resource

The squad must discover and share information through sight, sound, map state,
objective knowledge, contextual pings, radio/AI cues, and battlefield events.
UI communicates critical state with redundant text, shape, position, and audio
where appropriate. Information must not reveal actors through cover or create
unbounded notification spam.

### 3.3 Logistics creates choices

Supplies, ammunition, medical capacity, fuel, transport condition, and route
risk convert the strategic map into gameplay. A successful firefight is not a
complete success if the squad cannot extract, resupply, or protect the next
operation.

### 3.4 Consequences persist

Injuries, casualties, depleted sectors, destroyed or damaged enemy forces,
consumed battlefield loot, transport state, and operation outcomes survive the
boundaries where they are designed to matter. Persistence must be authoritative
and resilient rather than merely convenient.

### 3.5 Realism serves play

Ballistics, heat, reload interruption, posture, suppression, weather, hearing,
smoke, transport handling, and medical states should create readable tactical
decisions. Realism that adds friction without a meaningful choice is out of
scope.

## 4. Core player loop

```text
Assess the war
    ↓
Choose an operation and prepare the squad
    ↓
Deploy, navigate, fight, communicate, and manage supplies
    ↓
Adapt to casualties, weather, AI response, and route events
    ↓
Complete objectives and extract
    ↓
Treat, resupply, debrief, and save
    ↓
Observe the changed regional war and choose the next operation
```

Every major feature should identify which part of this loop it improves. A
feature without a clear loop connection is a candidate for deferral.

## 5. Campaign structure

The campaign is a persistent regional war represented by sectors and routes.
Each sector has operational value and changing conditions such as supply,
population, escalation, control, and available staging capacity. The campaign
advances through completed operations and bounded strategic updates.

### Operation types

The first campaign supports four primary systemic operation families:

1. **Attack:** seize or disrupt an enemy-held objective.
2. **Defense:** hold a position through escalating pressure.
3. **Raid:** enter hostile territory, achieve a limited objective, and extract.
4. **Resupply / Operation Lifeline:** load cargo at a stocked friendly sector,
   transport it along a risky route, and deliver it to an undersupplied sector.

Future operation families—escort, rescue, and specialized ambush variations—are
planned extensions, not prerequisites for the first stable campaign loop.

### First Light vertical slice

First Light is the quality gate and representative onboarding operation. Its
canonical objective order is:

1. `FindRedKeycard`
2. `UnlockSecurityDoor`
3. `EliminateGuard`
4. `ReachExtraction`

It must prove the complete player-facing route in multiplayer: join, briefing,
traversal, keycard interaction, locked-door state, combat, enemy drops,
casualty/medical readiness where applicable, extraction, completion replication,
save, late join, and reconnect continuity.

## 6. Player systems

### Combat

Combat is authoritative, lethal, and readable. Weapons use role-appropriate
handling, recoil, spread, posture, heat, magazine state, fire modes, reload
interruption, environmental acoustics, and physical exposure. Grenades,
engineering charges, smoke, suppression, near misses, armor, and hit feedback
expand tactical options without turning the game into an ability shooter.

### Health and casualties

Players and relevant AI can move through healthy, injured, incapacitated,
stabilized, evacuated, and dead states. Treatment consumes time and supplies.
Casualties affect movement, squad readiness, transport decisions, and the
debrief. A terminal death cannot return as an actionable casualty after load.

### Equipment and logistics

Inventory and carried load affect capability and mobility. Ammunition,
medical supplies, field resupply, runtime battlefield loot, cargo, fuel, hull
condition, and transport handling must form one coherent loop from preparation
through extraction and persistence.

### AI and command

Enemy AI uses perception, cover, suppression, search, retreat, grenade
evasion, faction behavior, and bounded navigation recovery. Squad commands are
simple, legible, and cooperative: follow, hold, and shared context pings are
more important than a complex command tree. AI should react to the same
conditions players understand, including weather, sound, casualties, and
support effects.

### Battlefield conditions

Weather and battlefield conditions modify sight, sound, explosive hearing,
smoke persistence, movement, transport traction, and artillery/munition
behavior through separate bounded contracts. Conditions must create choices and
remain understandable through briefings, HUD cues, and world presentation.

## 7. Multiplayer and persistence contracts

- The server owns combat outcomes, mission state, war state, inventory changes,
  casualty transitions, transport state, and campaign saves.
- Clients receive enough replicated state to make correct tactical decisions
  and render a consistent shared world.
- Late join and reconnect inherit the current operation and war snapshot.
- Server travel preserves the operation contract and authoritative save state.
- Stable objective IDs, persistence IDs, map paths, reflected names, and save
  fields are content contracts and require consumer searches before changes.
- Ephemeral enemy loot may be usable in the current level but must not become
  accidental authored-world persistence.

## 8. UX and presentation direction

The interface is a field instrument, not a separate strategy screen. The HUD
must prioritize immediate survival and objective information, defer low-priority
strategic updates during combat, coalesce stale updates, and preserve urgent
casualty or objective messages.

Accessibility is part of readability: safe-frame support, scalable HUD, high
contrast, color-independent cues, remappable keyboard/controller input,
toggle/hold options, camera comfort controls, and configurable subtitles are
baseline requirements.

World art, audio, and VFX should distinguish indoor/outdoor acoustics, weather,
near misses, distant war activity, smoke, support impacts, and faction
identity. Placeholder assets are acceptable during systems development but are
not release evidence.

## 9. Scope priorities

### Must ship

- Cooperative host/join campaign loop with authoritative save/load.
- First Light multiplayer operation and completion route.
- Attack, defense, raid, and resupply operation foundations.
- Combat, injury, casualty, medical, ammunition, and extraction consequences.
- Replicated war/operation snapshots, late join, reconnect, and server travel.
- Readable HUD, accessibility baseline, localization-safe UI, and debrief.
- AI perception, cover, suppression, search, retreat, and reliable authored
  navigation on shipping maps.
- Stable performance, packaged startup, save resilience, and release QA gates.

### Should ship if stable

- Escort and rescue operations.
- Route ambushes, vehicle damage events, and time-pressure variations.
- Broader weather/audio/FX coverage and authored world presentation.
- Campaign-scale balance and long-session multiplayer polish.

### Explicitly deferred

- Player voice communication as a project contract.
- Large class/hero ability systems.
- Competitive PvP.
- Unbounded open-world simulation that compromises network or save reliability.
- Content expansion before the First Light and campaign loop are manually
  signed off.

## 10. Production roadmap

### Gate G0 — foundations

Maintain the authoritative game shell, combat/health foundations, replicated
presentation, session flow, save/load, and clean leave behavior.

### Gate G1 — networked First Light alpha

Complete the production First Light route with two or more clients, late join,
reconnect, synchronized interaction/combat/extraction, field respawn, and
authoritative save continuity.

### Gate G2 — presentation and accessibility

Replace critical placeholders, complete HUD readability and accessibility
review, validate controller/remapping flows, and perform rendered multiplayer
playtests at target resolutions and couch distance.

### Gate G3 — persistent campaign

Run the three-sector attack/defense/raid campaign scenario with travel,
resupply, host recovery, reconnect, and long-session soak. Confirm no save loss,
objective divergence, or strategic-state corruption.

### Gate G4 — release candidate

Pass fresh packaged build validation, lower-tier performance, two-hour rendered
campaign/reconnect soak, controller/UI review, translated/pseudo-localized
layout review, audio/FX assignment review, and manual content/navigation gates.

## 11. Immediate plan: next three production increments

1. **Close the First Light quality gate.** Run a human two-player route and
   record issues in traversal, interaction feel, combat readability, casualty
   response, extraction, late join, and reconnect.
2. **Turn open gates into owners and evidence.** For each remaining module,
   record one acceptance scenario, one automated or runtime evidence target,
   and one explicit manual signoff.
3. **Build the campaign-content layer.** Add authored operation variations only
   after the base four operation families survive the G3 travel/reconnect/soak
   scenario. Prioritize escort/rescue and route events over isolated mechanics.

## 12. Definition of done for future work

A feature is not complete when its class compiles. It is complete when:

- the player-facing loop is clear;
- authority, replication, persistence, resupply/carry-load, and HUD paths are
  addressed where relevant;
- the feature has a focused contract or test where practical;
- the editor build and relevant runtime smoke pass;
- the production map or packaged route is checked when in scope; and
- remaining visual, interaction, multiplayer, balance, or content review is
  stated explicitly.

## 13. Decision filter

Before starting work, ask:

1. Which pillar and loop stage does this improve?
2. Does it close a complete player-facing loop or only add an isolated detail?
3. Does it affect a canonical contract such as an ID, save field, map path, or
   reflected Blueprint name?
4. What is the smallest evidence needed to call it proven or playable?
5. Does it advance the next production gate more than the alternatives?

If the answer to questions 1, 2, and 5 is unclear, the work should be refined,
deferred, or replaced with a plan item before implementation begins.

## 14. Source references

- `Docs/Production/ModuleImplementationPlan.md`
- `Docs/GDD_Modules/Broken_Horizon_Modular_GDD_Index.docx`
- `Docs/GDD_Modules/GDD-00_Product_Foundation.docx` through
  `Docs/GDD_Modules/GDD-13_Canonical_Contracts.docx`
- `Docs/Production/OnePointZero_Readiness_Checklist.md`
- `Docs/FirstLightGrayboxAutomation.md`


## 15. Direction revision — milsim total-war campaign

Broken Horizon is a cooperative milsim and total-war campaign about a small resistance force fighting to liberate a region from an occupying power. It combines Arma-style tactical authenticity—terrain, weapons, communication, logistics, suppression, medical states, vehicles, and disciplined movement—with Antistasi-style persistence: the war continues between operations, territory has strategic meaning, and players build an increasingly capable force inside a hostile theater.

### Unique twist: the force is the character

The squad is the campaign’s living strategic asset. The game remembers more than territory: it remembers the people who fight, their experience and injuries, the vehicles they maintain, the supplies they capture, the routes they secure, the specialists they recover, and the leaders they keep alive. These directly determine what the resistance can deploy next.

A victory expands capability; a careless operation can remove experienced personnel, consume scarce materiel, expose a route, or force the next mission to be fought with a weaker force. The player is therefore managing both a fireteam and the continuity of a fighting organization.

### Strategic questions before every deployment

1. What does the resistance need: territory, supplies, intelligence, transport, safe routes, or recovered personnel?
2. What can the resistance afford: available operators, equipment, fuel, medical stock, and acceptable casualty risk?
3. What will the occupation do in response: reinforce, patrol, raid, cut a route, retake a position, or escalate the sector?

### Revised operation families

- Attack/liberation: seize or disrupt an enemy-held objective and open the next strategic opportunity.
- Defense: hold a position while protecting the force-generation infrastructure behind it.
- Raid: steal intelligence or materiel, achieve a limited objective, and extract before the reaction force closes in.
- Resupply: move scarce cargo along a risky route to an undersupplied friendly sector.
- Recruitment/recovery: protect or extract people whose survival adds specialists, local knowledge, or replacement capacity.

### New production priority

Before broad content expansion, define the minimum persistent resistance-force model for personnel, specialists, vehicles, equipment, supplies, facilities, intelligence, and enemy reaction. Connect it to deployment, operation generation, debrief, and save/load. The campaign should visibly change what the squad can field next, not merely record a score after each mission.

## 16. Open-world theater

Broken Horizon takes place on one large, continuous open-world map. The map is both the tactical battlefield and the total-war board. Players should be able to move from a resistance hideout to a supply route, village, checkpoint, objective, airfield, depot, or extraction area without loading into a disconnected mission space.

### Open-world principles

- **One persistent theater:** Strategic ownership, patrols, supplies, facilities, casualties, vehicles, and operation state exist in the same regional world.
- **Player-chosen approach:** Players can reconnoiter, infiltrate, assault, ambush, bypass, or withdraw based on terrain, intelligence, time, weather, and force readiness.
- **Travel is gameplay:** Movement between objectives creates risk and opportunity through fuel, road security, convoy protection, observation, enemy patrols, breakdowns, and unexpected contact.
- **The map generates operations:** Operations originate from conditions in the theater—an exposed convoy, an isolated garrison, a cut route, a vulnerable depot, a threatened settlement, or an opportunity discovered through intelligence.
- **Bases matter:** Hideouts, safehouses, staging areas, captured facilities, medical points, vehicle parks, and supply depots form the physical logistics network of the resistance.
- **The occupation is active:** Enemy forces patrol, reinforce, search, resupply, counterattack, fortify, and retake ground according to strategic pressure and available resources.
- **Persistence is visible:** Players should return to the same roads, settlements, bases, and battlefields and see the consequences of earlier operations.

### Open-world gameplay loop

```text
Observe the theater
    ↓
Gather intelligence and identify an opportunity or threat
    ↓
Choose force, route, timing, supplies, and rules of engagement
    ↓
Travel through or operate inside contested territory
    ↓
Fight, sabotage, rescue, resupply, capture, or withdraw
    ↓
Return to a base or establish a forward foothold
    ↓
Recover personnel, repair vehicles, redistribute supplies, and plan again
```

### Map-scale production requirements

The open world must be divided into streamed sectors and bounded simulation regions so that the campaign can remain persistent without simulating every actor at full fidelity everywhere. Near the squad, the game prioritizes physical AI, navigation, vehicles, combat, sound, and interaction. Far from the squad, the war uses strategic snapshots and summarized movement, supply, reinforcement, and combat outcomes.

The first shippable map should prioritize a coherent playable region over maximum size. It needs a meaningful resistance heartland, several contested settlements, connected roads and secondary routes, enemy military sites, supply infrastructure, extraction spaces, varied terrain, and enough distance for travel decisions to matter.

Open-world acceptance requires more than loading the map successfully. It must prove continuous traversal, sector streaming, navigation coverage, transport and resupply routes, persistent world-state round trips, late join/reconnect, strategic updates while the squad is traveling, and stable performance at representative player and AI density.

## 17. Realism and real-world tactical doctrine

Realism is a core product requirement, not an art direction. Broken Horizon should model the reasons real units make tactical decisions: limited information, exposure, terrain, time, fatigue, communications, ammunition, casualty risk, and the need to coordinate fire and movement. The game takes inspiration from real-world small-unit doctrine while remaining a playable cooperative campaign rather than a training simulator.

### Tactical authenticity pillars

- **Mission planning:** Players receive an objective, intelligence quality, time window, rules of engagement, available force, and logistics constraints. The plan is their decision, not a scripted route.
- **Terrain and observation:** Elevation, concealment, cover, dead ground, visibility, lighting, weather, vegetation, structures, and likely avenues of approach affect detection and engagement.
- **Fire and movement:** Teams are rewarded for suppressing, maneuvering, bounding, maintaining sectors, using overwatch, and avoiding unsupported advances.
- **Information discipline:** The squad acts on incomplete and sometimes stale information. Markers, pings, radio reports, and AI callouts communicate uncertainty rather than granting omniscience.
- **Communications:** Contact reports, bearings, landmarks, range, unit status, and concise squad commands matter. Communication failure, distance, terrain, and combat pressure may reduce coordination quality.
- **Logistics and sustainment:** Ammunition, medical supplies, fuel, batteries, vehicle condition, spare parts, and transport capacity limit operational tempo.
- **Casualty care:** A casualty creates a tactical problem. Stabilization, security, movement, evacuation, and abandonment decisions have time and force consequences.
- **Withdrawal and preservation:** Breaking contact, delaying an enemy, abandoning equipment, or returning later are valid tactical outcomes. The game should not require every operation to end in total enemy destruction.
- **Rules of engagement:** Identification, collateral risk, civilian presence where designed, and escalation consequences distinguish a disciplined operation from indiscriminate fire.

### Realism rules for gameplay systems

1. The game must not provide information the squad has not reasonably observed, received, or inferred.
2. Enemy behavior must be tactically understandable even when it is not predictable.
3. Weapons, vehicles, medical states, and movement should communicate limitations through handling and consequences rather than arbitrary cooldowns.
4. A superior plan, position, or information advantage should often matter more than faster reflexes.
5. Every realism mechanic must create a meaningful decision, readable consequence, or cooperative role.
6. Failure should create a new tactical situation rather than silently resetting the world.

### Real-world tactics without needless friction

The game will use authentic principles, not administrative busywork. Players should not be forced to reproduce every real military procedure or perform repetitive maintenance unless it changes the tactical problem. Detailed systems are justified when they affect planning, coordination, survival, or campaign capability.

Examples of acceptable abstraction include simplified radio handling that preserves communication discipline, contextual medical actions that preserve triage decisions, and strategic logistics that models supply flow without requiring players to manually count every item.

### Tactical AI expectations

AI should use the same battlefield logic the player can understand: seek cover, observe before exposing itself, communicate contact, suppress before maneuvering, react to casualties, protect important sites, withdraw when its mission or force condition requires it, and exploit known routes or terrain. AI should not rely on omniscient targeting, impossible reaction times, or spawning directly into contact to create difficulty.

### Realism acceptance tests

Future operations and features should include manual review for:

- whether terrain supports real observation, concealment, and maneuver;
- whether players can identify why contact occurred and what information was available;
- whether fire-and-movement tactics are more effective than unsupported rushing;
- whether casualty and resupply decisions create believable tradeoffs;
- whether AI reactions are tactically credible and readable;
- whether the open-world travel and logistics network supports planning rather than busywork; and
- whether the experience remains playable for a squad that communicates normally, not only for expert military players.
