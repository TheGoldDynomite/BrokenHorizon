# Broken Horizon module implementation plan

This file turns the modular GDD into the production backlog. The authoritative
design modules live under `Docs/GDD_Modules`. Status is evidence-based:

- **Proven**: implemented and covered by passing automated/runtime evidence.
- **Playable**: implemented and exercised, but still requires manual quality
  review or broader content coverage.
- **In progress**: meaningful implementation exists, but a module exit
  criterion is still missing.
- **Planned**: direction exists in the GDD, but production implementation has
  not started.

## End goal

Ship a cooperative multiplayer tactical FPS in which a squad can host or join a
shared persistent campaign, deploy into systemic operations, fight with
authoritative replicated combat and medical state, move people and supplies
through a changing regional war, survive disconnects and server travel, and
finish the campaign without desynchronization, save loss, or unrecoverable
strategic failure.

## Module register

| Module | Production status | Current evidence | Next exit criterion |
|---|---|---|---|
| GDD-00 Product foundation | Proven | Multiplayer GDD v1.1 and module suite | Review only when product promise changes |
| GDD-01 Vision and fantasy | Playable | First Light and persistent-war systems express the core pillars | Full squad playtest confirms coordination fantasy |
| GDD-02 Multiplayer and campaign loop | Proven | Server RPCs, replicated war/operation snapshots, authority-owned saves, field respawn, host/join/leave shell, two-client active-operation seamless travel, completion, host-process recovery, and reconnect | Latency/loss and soak proof |
| GDD-03 Player combat | Playable | Replicated weapon/health state, recoil, spread, posture, traversal, grenade and feedback validators; owner-confirmed lethal/headshot/armor cues; enemy near misses create bounded authoritative player suppression; background campaign updates defer through local damage/suppression and coalesce to the latest strategic snapshot while tactical alerts remain immediate; authoritative actor-backed context pings reach retained/rejoining clients, have paired rendered-client `TRACKED`/`LAST KNOWN` occlusion proof, and occupy a lane separate from incoming-threat chevrons | Manual latency, physical-input conflict, subjective notification cadence, and combat-feel pass with multiple clients |
| GDD-04 Health and logistics | Proven foundation | Injury/armor treatment, owner-replicated medical state, supplies, sector stations, convoy, replicated field transport, bounded two-client medical/vehicle recovery, and a cooldown-limited zero-supply emergency fallback kit | Manual multi-client treatment/vehicle interaction-feel pass |
| GDD-05 AI and command | Playable | Perception, search, cover, suppression, grenade evasion, retreat, factions, follow/hold orders, state-specific safe navigation-failure fallback, lethal-hit reaction suppression before death cleanup, owner HUD average/weakest-operative readiness feedback, and First Light serialized Recast data aligned with the live tile configuration | Navigation coverage, readiness balance, casualty-reaction feel, and command-conflict playtest |
| GDD-06 Missions and First Light | Playable | Required objective chain, operation directors, persistence, two-client completion/rejoin evidence, and a canonical runtime smoke that uses the production keycard, locked door, grouped guard or defense-wave combat, extraction overlap, and war-victory paths | Continuous multi-client player-controlled traversal and operation-feel signoff |
| GDD-07 Persistent war | Proven foundation | Sector, route, supply, convoy, population, escalation and campaign round-trip automation | Long multiplayer soak with join/reconnect during war changes |
| GDD-08 World/art/audio | In progress | First Light target, region map, lighting/presentation automation, read-only UE metadata/reference-chain audit for 234 shipping-scope assets including the active rifle dependency, authoritative LOD/collision metadata, replicated AI voice barks, movement acoustics, weather/war bed, semantic UI cues, indoor/outdoor weapon tails, directional near misses, and canonical audio/FX assignment audit; required audio/FX coverage is complete at 18/18 and systemic distant artillery, aircraft, and small-arms event cues raise optional coverage to 9/27 | Assign or author the remaining 18 optional audio/FX cues; resolve 2 shipping-referenced named candidates; manually disposition the single-LOD ejected casing and complete material/look, audio, streaming and runtime performance gates |
| GDD-09 UI/UX/accessibility | Implemented; interactive review pending | Native widgets, war map, combat HUD, schema-8 settings with full keyboard/controller remapping and device-aware Auto/keyboard/controller/both prompts, independent look controls, toggle/hold modes, network-safe menus, camera/visual-comfort controls, native configurable subtitles, 80-100% safe-frame enforcement for all styled UI, 75-150% HUD scale, high contrast/color-independent cues, host-authoritative Recruit/custom difficulty assists, and a conflict-checked English runtime localization target; settings and UI validators; explicit multiplayer state headings/colors, anchored and wrapped rendered host/join feedback, strategic host/operation-lock feedback, single-delivery errors, priority/preemption for critical notifications, combat-aware deferral plus latest-state coalescing of background strategy, shape-redundant armored-hit feedback, and canonical war-map/custom-difficulty captures at 720p/1080p/4K | Interactive host/join and remap axis capture, visual-comfort/subtitle feel, subjective combat readability/cadence, translated cultures and pseudo-localized layout review, couch-distance, and physical-controller accessibility review |
| GDD-10 Progression/balance | Playable | Tuned baselines, long-session simulation, authoritative host-adjustable custom difficulty axes, after-action merit, tactical-option progression, bounded multi-client difficulty replication/reconnect, and static custom-control usability proof at 720p/1080p/4K | Campaign-scale subjective balance and physical-controller control-usability review |
| GDD-11 Technical/QA | In progress | Unified validation script with conflict-checked localization gathering, 66-test automation suite, smoke/First Light logs, NullRHI CPU/memory/entity evidence, 1080p static/First Light traversal and canonical six-sector full-world traversal plus eight-visit navigation proof on RTX 5060 Ti, simultaneous two-rendered-client evidence, representative four-player rendered-observer plus network scale, a strict ten-minute two-rendered-client combat-density soak, four-client 80 ms/3% impairment budgets, and canonical 33-capture `-RenderedUI` proof for HUD/briefing/pause/settings/remapping/war-map/custom-difficulty at 720p/1080p/4K, HUD/safe-area extremes, and four host/join states at 720p/4K | Packaged/lower-tier, visible-quality, two-hour rendered campaign/reconnect soak, interactive controller/UI, translated/pseudo-localized layout, couch-distance, and geographically representative online-service evidence; player voice is outside the current project contract |
| GDD-12 Production/release | In progress | G0 foundations largely implemented; First Light is the quality gate | Pass G1 with networked First Light completion |
| GDD-13 Canonical contracts | Proven | Objective IDs, persistence IDs, map paths and tuning reference centralized | Keep synchronized with every contract change |

## Roadmap gates

### GDD-03 combat-aware campaign notifications - 1 August 2026

- Background war-turn, route, event, and strategic-priority updates now use a
  dedicated reliable owner-client presentation path. They queue while the local
  player is suppressed or within an authorable eight-second post-damage quiet
  window, then release when immediate combat pressure clears.
- Active-operation briefings, squad Context state, route failures, casualty
  alarms, objective events, and other immediate tactical messages preserve
  their existing priority and are never delayed by this policy.
- The queue retains message deduplication, bounded capacity, priority ordering,
  and authored audio semantics. New background war snapshots now replace older
  deferred background snapshots, so a prolonged fight releases one current
  strategic state rather than a stale sequence. Non-deferred tactical messages
  can still pass the waiting strategic update during combat.
- `BrokenHorizon.UI.Notification.PriorityContract` covers combat deferral,
  latest-state coalescing with real queue state, quiet release,
  immediate-message behavior, Blueprint diagnostics, and the networked
  character entry point. `BHPostCombatCadence-Focused2.log` passed the focused
  contract; `BHPostCombatCadenceFinal-*` compiled and passed all 66 tests,
  startup, and canonical First Light with 8/12 bounded navigation fallbacks.
  Manual moving-combat review remains required for subjective cadence and
  occlusion/readability.

### GDD-03 shared squad context ping - 1 August 2026

- Middle mouse or controller D-pad up now issues the GDD-03 context ping through
  an authoritative server RPC. The server repeats a bounded 200 m visibility
  trace and classifies hostile, ally/casualty, transport, supply, objective, or
  open location; clients cannot submit arbitrary world coordinates.
- `ABHWarGameState` publishes one revisioned squad snapshot containing a
  quantized location, context, issuer, server-time expiry, and optional
  replicated target actor. A one-second authority cooldown bounds spam, and the
  marker expires for every client from the shared server deadline.
- The native combat HUD draws a separate amber direction marker with context,
  distance, and issuer, preserving the blue field-squad command marker when
  both are active.
- Moving actor-backed pings are now locally line-of-sight safe. An unobstructed
  client view follows the target and labels it `TRACKED`; occlusion freezes the
  last visible coordinate and labels it `LAST KNOWN`, preventing movement
  disclosure through cover while preserving useful squad context.
- `BrokenHorizon.Multiplayer.Coordination.SharedSquadPing` verifies expiry,
  distance formatting, replicated field metadata, and the server RPC contract.
  The final editor build and all 60 tests passed in
  `BHSharedSquadPing-Final-*`; startup, First Light, and the read-only asset gate
  also passed with zero strict failure markers.
- The bounded dedicated-host fixture passed in
  `GDD03-SharedSquadPingFinal-20260801-120234-Summary.json` on port 8523. Both
  retained clients and the reconnecting client received revision 1 with
  `HOSTILE`, issuer `HOST_FIXTURE`, and the same quantized location; all four
  runtime logs contain zero fatal, assertion, network, travel, PIE, or autosave
  failure markers.
- `BHMovingPingOcclusionNetFinal2-NetworkScale-20260801-213010-Summary.json`
  passed the four-client/rejoin tracked-actor contract under 19-AI load, and
  `BHMovingPingRendered2-RenderedMultiplayer-20260801-213300-Summary.json`
  passed paired D3D12 captures and performance/streaming gates. Both inspected
  images show `LAST KNOWN` behind cover. The editor build, all 66 tests,
  startup, and First Light also passed in `BHMovingPingOcclusionFinal-*`.
- `BHPingLaneFinal2-RenderedMultiplayer-20260801-213945-Summary.json` then
  verified the dedicated coordination lane on both clients. Direct inspection
  confirms full separation from the simultaneous red incoming-threat chevron;
  `BHPingLaneRegressionFinal-*` passed all 66 tests, startup, and First Light.
- Manual two-player review remains required for colorblind readability,
  controller conflicts, combat spam cadence, latency, and the
  usefulness of each context classification in First Light.

### GDD-04 ephemeral battlefield loot persistence - 1 August 2026

- Enemy death ammunition remains replicated, usable current-level loot but is
  explicitly excluded from authored world-item persistence validation.
- The save validator continues to reject missing or duplicate stable IDs on
  placed supplies while accepting actors marked through
  `ConfigureRuntimePickup` / `MarkAsRuntimeSupply`.
- `BHEphemeralAmmoFinal-*` built and passed all 65 tests, startup smoke, and the
  extended First Light route. Three enemy drops were present across four
  successful checkpoints with zero missing-persistence-ID warnings.
- `BHBattlefieldLootFinal3-*` closes the usability side of that contract. The
  canonical route consumed a real runtime enemy drop through `IBHInteractable`,
  raised the authoritative reserve from 150 to 180 rounds, marked the pickup
  consumed, then completed extraction. The editor build, all 66 tests, startup,
  and First Light gates passed with 8/12 bounded navigation fallbacks.

### GDD-01 production First Light multiplayer route - 1 August 2026

- The two-client completion harness now runs the production actor route instead
  of its synthetic shared-objective shortcut: authored keycard, locked door,
  guard deaths, enemy runtime ammunition, and extraction all execute on the
  dedicated server.
- The strict multiplayer gate exposed and corrected client-side runtime-drop
  misclassification: `bRuntimeSupply` now replicates, so clients no longer
  report enemy loot as an authored supply missing a persistence ID.
- `G1-ProductionRouteLootFinal2-20260801-192318-*` passed with two authoritative
  players completed, completion replicated to both clients, and a replacement
  client inheriting the completed mission. The server consumed a real drop and
  raised reserve ammunition from 150 to 180 rounds before extraction.
- The direct editor build and `BHReplicatedRuntimeSupplyFinal-*` regression
  passed all 66 tests, startup, and canonical First Light with 8/12 navigation
  fallbacks. Physical two-player route and interaction feel remain manual.
- `G1-ConsumedLootReplicationFinal-20260801-193303-*` further proves both live
  clients received the used drop's consumed transition. The replacement client
  received exactly the two unused drops and no consumed drop, so reconnect did
  not resurrect battlefield loot. `BHConsumedLootReplicationFinal-*` passed
  all 66 tests, startup, and canonical First Light after this gate.
- `G1-OwnerAmmoReplication-20260801-193727-*` closes the owning-player state
  path: Client A received magazine 30 / reserve 180 through `OnRep_Ammo`, while
  Client B received only the shared consumed-pickup transition. The summary
  retains all route, reconnect, and late-join loot-set assertions;
  `BHOwnerAmmoReplicationFinal-*` passed all 66 tests, startup, and First Light.
- `G1-AmmoHUDReplication-20260801-194127-*` carries that owner-only state through
  the production ammo delegate and bound widget. Client A's actual `AmmoText`
  became `30 / 180`; Client B retained shared world state without private ammo.
  `BHAmmoHUDReplicationFinal-*` passed all 66 tests, startup, and First Light.
- `BHRenderedLootHUD-20260801-194607-*` adds two-client D3D12 proof at 720p.
  Client B's production widget rendered `30 / 180` fully inside the lower-right
  safe area; both client profiles met frame/GPU, hitch, draw, memory, and
  streaming budgets. `BHRenderedLootHUDRegression-*` passed all 66 tests,
  startup, and First Light afterward.

### G0 - Multiplayer foundations

- Server-authoritative player, combat, vehicle, operation and war state.
- Replicated presentation for remote clients.
- Authority-owned campaign save/load and server travel.
- Host and join session flow from the main menu.
- Clean leave-session behavior.

### G1 - Networked First Light alpha

- Two or more clients join the same session.
- Late join receives the current war and operation snapshot.
- Keycard, door, objectives, supplies, combat and extraction synchronize.
- Individual death uses field respawn without resetting the operation.
- Host save/load retains connected clients.

### G1 status update - 30 July 2026

- Dedicated host-loss/reconnect checkpoint is complete for runtime evidence.
- Two-process dedicated host/client smoke loops completed with unique per-cycle ports.
- Host lifecycle and snapshot continuity logs are present (`BH_WAR_GAME_STATE_READY`,
  `BH_SHARED_MISSION_SNAPSHOT_PUBLISHED`, and restart/rejoin evidence in
  `Saved/Logs`).
- Remaining work: polish Phase 2 readiness and continue to full two-client First
  Light gameplay verification.

### G2 status update - 30 July 2026

- Phase 2 startup gate started and verified.
- Startup smoke and First Light smoke now pass using `-DDC-ForceMemoryCache` to
  avoid local Derived Data Cache write-path failures (`G2-Startup.log`,
  `G2-FirstLight.log`), both with process exit code `0`.
- No `Fatal error`, `Assertion failed`, or `Unhandled Exception` markers were
  detected in those logs.
- Remaining work for this phase: complete multiplayer presentation pass and
  accessibility/combat-readability validation with host-and-client sessions.

### G3 status update - 30 July 2026

- Campaign prototype is the new focus after completing G2 startup and basic
  vertical-slice verification.
- G3 campaign gate evidence is now present in:
  - `G3-PersistentWar.log` (28 tests; persistent-war validation),
  - `G3-CampaignLoop.log` (5 tests),
  - `G3-CampaignLoop2.log` (5 tests with `-DDC-ForceMemoryCache` rerun),
    all with `Result={Success}` and `Automation Test Queue Empty`.
- Current evidence confirms routed campaign dynamics, casualty/supply progression,
  victory/defeat resolution, and successful mission-loop continuity across the
  full automated pass.
- Remaining G3 work: execute the three-sector travel/reconnect runbook with
  host travel + client reconnect checkpoints and two-hour multiplayer soak.

### G3 acceptance runbook (active)

- Scenario: three connected sectors (attack, defense, raid) with:
  1. first-time mission routing and completion,
  2. transport/supply persistence across authoritative server travel,
  3. host crash/recover and reconnect continuity,
  4. long-session soak with normal `Automation Test Queue Empty` completion
     markers and no `Test Failed` entries.
- Success criteria:
  - no `Fatal error`/`Unhandled Exception` markers in runtime logs,
  - no save-loss markers after travel/reconnect checkpoints,
  - same `OperationId`/casualty/supply contract preserved across transitions,
  - no unresolved objective/state divergence in host and client snapshots.
- Evidence folder: `Saved/Logs/` (`G3-*.log`, `Phase1-*`, `BHValidation-*`,
  soak/session logs).

### Gameplay operation update - 1 August 2026

- Resupply is now a selectable fourth systemic operation alongside attack,
  defense, and raid.
- Operation Lifeline assigns a stocked friendly staging sector and an
  undersupplied friendly destination, preserves the staging reserve until the
  player physically loads cargo, and completes only after a full assigned load
  reaches the destination.
- The war map exposes cargo, route, source supply, and destination capacity;
  the field HUD switches from the pickup station to the delivery station after
  cargo is loaded.
- Authoritative delivery can be performed by a cooperating player, while the
  assigned character owns mission resolution and debrief state.
- Evidence: `BrokenHorizon.PersistentWar.Operations.ResupplyDelivery` passed as
  part of the 39-test `BHResupplyFinal-Tests.log` suite; editor compilation,
  main-menu smoke, and First Light smoke also completed cleanly.
- Remaining operation-content gap: implement escort/rescue and add route ambush,
  vehicle-damage, and time-pressure variations to resupply.

### Gameplay operation update - 1 August 2026 (escort)

- Escort is now a selectable systemic operation whenever an active friendly
  convoy is travelling to a friendly sector.
- Operation Shepherd pins the exact strategic convoy across replication,
  reconnect, and save/load, deploys from that convoy's source sector, and uses
  the existing ambient route encounter rather than spawning a duplicate
  mission convoy.
- The field HUD tracks the assigned convoy. Reaching the local route exit
  completes the shared operation; destruction of that exact convoy fails it,
  while unrelated convoys cannot resolve the mission.
- Success and failure preserve sector ownership, produce distinct strategic
  events and debriefs, and release the campaign operation lock.
- Evidence: `BrokenHorizon.PersistentWar.Operations.EscortConvoy` passed as
  part of the 40-test `BHEscortVerified-Tests.log` suite. The editor target
  compiled, and `BHEscortSmokeVerified-Smoke.log` plus
  `BHEscortSmokeVerified-FirstLight.log` passed clean-log validation.
- Remaining operation-content gap: add rescue/casualty evacuation as the second
  escort/rescue form, plus route ambush, vehicle-damage, route-choice, and
  time-pressure variations.

### Gameplay operation update - 1 August 2026 (casualty rescue)

- Rescue is now a distinct selectable operation when the player's persistent
  field squad contains an incapacitated or stabilized casualty.
- Operation Guardian assigns the exact operative by stable ID and a chosen
  friendly treatment sector. The casualty target survives save/load,
  replication, reconnect snapshots, transport embarkation, and server travel.
- Field stabilization now creates a replicated `requires evacuation` state
  instead of silently returning the operative to normal combat. Stabilized
  casualties cannot fire or throw grenades, can board while the squad holds,
  and remain marked `MEDEVAC` on the combat HUD until treated.
- Reaching a friendly support station completes treatment and the rescue
  objective. A cooperating player can activate treatment for the operation
  owner; supply cost includes every fireteam member serviced. If the assigned
  casualty expires first, the rescue fails with a dedicated debrief.
- Evidence: `BrokenHorizon.PersistentWar.Operations.CasualtyRescue`, replicated
  casualty/roster contracts, transport eligibility, HUD labeling, and save
  round-trip checks passed in the 41-test `BHRescueVerified-Tests.log` suite.
  `BHRescueVerified-Smoke.log` and `BHRescueVerified-FirstLight.log` also passed
  clean-log validation after a successful editor build.
- Manual gameplay review remains required for casualty pickup feel, vehicle
  attachment presentation, treatment radius, route pacing, and two-client
  cooperative station activation.
- Remaining operation-content gap: add route ambush, vehicle-damage,
  route-choice, and time-pressure variations.

### Gameplay operation update - 1 August 2026 (route variations)

- Strategic convoys now receive a deterministic route-operation profile:
  standard movement, heavy ambush, damaged vehicle, or time-critical escort.
- Heavy-ambush contracts add hostile attackers to the existing strategic force
  package. Damaged-vehicle contracts begin at 55 percent integrity, making
  protection and recovery materially more important. Time-critical escorts
  fail the assigned operation if the convoy does not clear its local route
  within the 150-second operational window.
- When the world contains multiple splines connecting the source and
  destination, the friendly convoy exposes an in-world interaction that lets
  the player commit it to the next compatible route. Candidate ordering is
  stable by connection quality and route ID.
- The combat HUD names the active contract, retains cargo/integrity/distance
  telemetry, and displays a minute/second countdown for urgent escorts.
- Evidence: `BrokenHorizon.PersistentWar.Operations.RouteVariations` passed as
  part of the 42-test `BHRouteVariations-Tests.log` suite. The editor target
  compiled, and `BHRouteVariations-Smoke.log` plus
  `BHRouteVariations-FirstLight.log` passed clean-log validation.
- Manual gameplay review remains required for alternate-route spline coverage,
  reroute interaction range, ambush pacing, damaged-convoy survivability, HUD
  readability, and two-client authoritative route selection.
- Route-operation continuity is now authority-owned rather than actor-local:
  profile, selected world-route ID, and urgent deadline remain in the strategic
  convoy state carried by replication and save schema 41. The war subsystem
  advances urgent time on the server and publishes it at the war snapshot rate;
  recreated convoy encounters resume the stored route and countdown. Older
  convoy saves migrate deterministically when loaded.
- Persistence evidence: the escort test now round-trips selected route and
  deadline through replicated snapshots and serialized campaign restoration.
  It and the route-variation contract passed in the 42-test
  `BHRoutePersistence-Tests.log`; `BHRoutePersistence-Smoke.log` and
  `BHRoutePersistence-FirstLight.log` loaded cleanly after a successful build.

### Gameplay progression update - 1 August 2026 (difficulty axes)

- Recruit, Operator, Veteran, and bounded Custom campaign profiles now form an
  authoritative GDD-10 contract. Operator preserves existing tuning; Recruit
  uses the specified 0.75 incoming-damage multiplier and Veteran uses 1.15.
- Independent axes now affect incoming player damage, enemy acquisition,
  hostile alert/reposition coordination, treatment time and fireteam service
  cost, strategic simulation tempo, and war-checkpoint cadence. Difficulty
  does not remove feedback, grant AI perfect information, or alter objectives.
- The selected profile is replicated in the war snapshot, serialized in save
  schema 42, restored on campaign load, and defaults old saves to Operator.
  Custom profiles expose every axis through the Blueprint-facing subsystem API
  and sanitize values to production-safe bounds.
- The strategic map displays the active profile and allows the host to cycle
  Recruit, Operator, Veteran, and Custom with F6 while no operation is
  committed, preventing mid-operation pressure switching. In Custom, F8 or
  gamepad left shoulder selects each of the six axes; minus/equals or gamepad
  D-pad adjusts the selected value in bounded 0.05 steps. The selected axis and
  live multiplier remain visible in the command header and the footer exposes
  the active control scheme.
- Evidence: `BrokenHorizon.PersistentWar.Progression.CampaignDifficulty`
  passed with the complete 43-test `BHDifficultyAxes-Tests.log` suite. The
  editor target compiled, and `BHDifficultyAxes-Smoke.log` plus
  `BHDifficultyAxes-FirstLight.log` passed clean-log validation.
- `BrokenHorizon.PersistentWar.Progression.CustomDifficultyAxes` verifies all
  six readable axis controls, independent adjustment, Custom conversion, and
  the distinct damage/checkpoint lower bounds. It passed alone in
  `BHCustomDifficultyAxes-Focused.log` and within the 58-test
  `BHCustomDifficulty-Final-Tests.log` suite.
- `GDD10-CustomDifficulty-20260801-113802-Summary.json` passed on port 8491.
  The dedicated host configured damage 0.85, perception 0.95, coordination
  1.10, medical 1.20, strategic 1.30, and checkpoint 0.75; both retained
  clients and the reconnecting client received the exact Custom profile.
  All four multiplayer logs and `BHCustomDifficulty-Final-*` contain zero
  strict failure markers; startup and First Light smokes passed and First Light
  reached `BH_WAR_GAME_STATE_READY`.
- Manual review remains required for Recruit/Veteran combat feel, AI acquisition
  fairness, medical pacing, strategic tempo, rendered F6/F8/gamepad
  discoverability, and subjective axis-tuning usability during a live session.

### Gameplay progression update - 1 August 2026 (after-action and support)

- Every resolved systemic operation now creates an authoritative after-action
  record across mission result, force preservation, hostile outcome, resource
  efficiency, and operational effect. Debriefs show the resulting grade,
  score award, and cumulative campaign merit instead of reducing performance
  to kill count alone.
- Campaign merit unlocks tactical resilience rather than damage inflation:
  - 100 merit: intelligence network establishes a limited 35-percent regional
    confidence floor while preserving uncertainty.
  - 250 merit: casualty recovery network reduces treatment time and fireteam
    medical-service cost by 15 percent.
  - 450 merit: transport support network reduces vehicle recovery cost by
    25 percent.
- Progression, latest AAR, operation counts, merit, and capability unlocks are
  replicated in the war snapshot and persisted from save schema 43. Older saves
  migrate to an empty progression state without changing campaign contracts.
- The strategic command header displays merit and unlocked-support count; the
  success/failure debriefs display AAR grade and the exact merit award.
- Evidence: `BrokenHorizon.PersistentWar.Progression.AfterAction` passed with
  the complete 44-test `BHAfterActionVerified-Tests.log` suite after the editor
  target compiled. `BHAfterActionVerified-Smoke.log` and
  `BHAfterActionVerified-FirstLight.log` passed clean-log validation.
- Manual review remains required for scoring readability, perceived fairness,
  unlock pacing across a real campaign, treatment/vehicle-support feel, and
  host/client debrief synchronization.

### Gameplay progression update - 1 August 2026 (tactical planning)

- Existing campaign capability unlocks now create selectable pre-operation
  choices instead of remaining passive bonuses only:
  - the intelligence network unlocks **Recon Planning**, which raises operation
    planning confidence to the confirmed-intelligence threshold and therefore
    removes avoidable surprise-force/wave pressure;
  - the transport support network unlocks **Reinforcement Priority**, which
    requests one additional friendly support operative while remaining bounded
    by the staging garrison, operation support cap, and raid cap;
  - the casualty recovery network unlocks **Medical Preparation**, which
    deploys one medkit and two field dressings for the operation owner so the
    squad can deliberately trade strategic supply for casualty resilience;
  - **Standard Planning** remains the no-bonus baseline.
- Commanders cycle unlocked choices with F7 on the strategic map before an
  operation is committed. The active plan is shown in the campaign header and
  is locked once deployment begins, preventing mid-operation force changes.
- Tactical plans now create logistics tradeoffs instead of free permanent
  upgrades on attack, defense, and raid operations: Standard Planning retains
  the 10-supply deployment cost, Recon Planning costs 16 total, Medical
  Preparation costs 18 total, and Reinforcement Priority costs 22 total.
  Rescue, escort, and resupply operations retain their established logistics
  costs because tactical preparation does not apply to them.
- Affordability uses the full selected-plan cost across the operation's actual
  supply route. The deployment preview and rejection feedback show that exact
  requirement; if stocked alternate staging bases exist routing may use them,
  while a theater-wide shortage blocks the expensive plan but still permits a
  cheaper Standard deployment where possible.
- The selected option is authoritative, included in replicated war snapshots,
  and persisted in save schema 44. Schema 43 saves preserve their existing
  progression and migrate to Standard Planning; invalid or no-longer-unlocked
  selections are sanitized to the baseline.
- Operation force packages record which tactical effect was applied, making
  recon, medical, and reinforcement outcomes inspectable and deterministic for
  tests, reconnects, and saved active-operation state.
- Operation resolution now carries that accountability into the authoritative
  after-action record. Schema 45 stores the selected plan, its additional
  supply commitment, and a 0-5 tactical execution score in the latest debrief:
  successful recon receives full execution credit; medical and reinforcement
  credit fall with friendly support casualties; failed plans receive little or
  no tactical credit; and tactical supply expenditure reduces
  resource-efficiency scoring. Standard and noncombat operations do not receive
  artificial tactical points or preparation benefits.
- Both success and failure debriefs display the plan, committed extra supply,
  and tactical-effect score alongside grade, total score, and campaign merit,
  giving players direct feedback on whether the more expensive preparation was
  worthwhile.
- Evidence: `BrokenHorizon.PersistentWar.Progression.AfterAction` passed its
  focused tactical-option, constrained-supply, scoring, applicability,
  replication, and save-round-trip checks and the complete 45-test
  `BHTacticalAAR-Tests.log` suite. The UE 5.8 editor target compiled cleanly;
  `BHTacticalAAR-Smoke.log` and `BHTacticalAAR-FirstLight.log` loaded
  their intended maps and exited with no fatal, assertion, unhandled, Windows,
  or Blueprint error markers.
- Manual review remains required for F7 discoverability, briefing clarity,
  unlock pacing, perceived value against Standard Planning, medical-loadout
  usefulness, and authoritative host/client selection visibility in a live
  multiplayer session.
- Medical Preparation completion evidence: `BHMedicalPreparation-Build.log`
  compiled the UE 5.8 editor target; all 49 tests passed with an empty queue in
  `BHMedicalPreparation-Tests.log`; main-menu and First Light loads passed in
  `BHMedicalPreparation-Smoke.log` and `BHMedicalPreparation-FirstLight.log`.
  The AfterAction contract covers unlock, +8 supply commitment, combat-only
  applicability, +1 medkit/+2 dressings, casualty-based execution scoring,
  replicated snapshot selection, and save round trip.

### Gameplay operation update - 1 August 2026 (field reconnaissance)

- Recon is now a distinct selectable systemic operation for adjacent
  non-friendly sectors whose intelligence is not yet confirmed. Operation
  Watchtower uses a stable `ObserveSector` objective and a four-supply field
  reporting kit rather than an assault force or occupation package.
- The deployed squad must enter the assigned sector, move between observation
  positions, and remain in the field long enough to file reports through the
  existing authoritative reconnaissance loop. The objective completes only at
  100-percent intelligence confidence, and authoritative completion uses the
  shared-objective path so the operation owner's mission resolves consistently.
- Recon does not capture territory, change garrisons, award battlefield
  salvage, apply tactical-combat preparation, or teach the enemy a repeated
  combat pattern. Success confirms intelligence; withdrawal preserves sector
  control and produces a dedicated failure/debrief path.
- The strategic map exposes current/target intelligence, low-risk loadout
  guidance, staging route, and exact supply commitment. The field HUD reuses
  its movement, observation, cooldown, and confidence telemetry.
- The append-only operation value and stable objective survive replicated war
  snapshots and serialized active-operation state. The multiplayer test
  command parser also accepts `BHTestOperationType=Recon` for future bounded
  host/reconnect scenarios.
- Evidence: `BrokenHorizon.PersistentWar.Operations.FieldRecon` covers
  viability, four-supply commitment, dedicated briefing/objective, replicated
  snapshot convergence, save round trip, territory-neutral resolution,
  operation-lock release, and noncombat AAR scoring in the 52-test
  `BHFieldReconOperationFinal-Tests.log` suite. Editor compilation, main-menu
  startup, and First Light smoke evidence use the same final log prefix.
- Manual gameplay review remains required for observation pacing, patrol
  visibility, avoidance-versus-contact incentives, HUD readability, and
  two-client cooperative report completion.

### 1.0 launch readiness gates

- **Primary 1.0 exit condition:** gated checklist complete across four streams:
  - Multiplayer and gameplay readiness (G1–G4),
  - Campaign persistence and long-session stability (G3),
  - Art/asset readiness (GDD-08),
  - Delivery hardening (GDD-11/12).
- 1.0 is blocked until each stream has explicit owner, test command, evidence
  artifact, and a date-stamped completion record.

### Art & texture readiness runbook

- The initial 1 August 2026 editor-backed baseline is recorded in
  `AssetReadiness_PlaceholderAudit_2026-07-30.md` and
  `Saved/Reports/BHAssetReadiness.json`. It initially measured 206 assets with zero audit
  item errors, no oversized or large non-streaming texture warnings, and two
  validated but still review-required shipping reference chains.
- The corrected active-weapon audit now measures 234 assets after direct CDO
  inspection identified the owner-only Infima skeletal rifle dependency.
  UE5.8 `StaticMesh.get_num_lods()` provides authoritative commandlet metadata:
  all seven static meshes have known LOD and simple-collision data. Six are
  single-LOD, but only the small ejected casing is shipping-referenced. The
  audit remains read-only; casing suitability and visible quality remain manual.

- Track and remove placeholders in all shipping assets:
  - Weapon/vehicle/soldier/prop materials in First Light and campaign sectors.
  - Replace default checker/default-normal placeholders and temporary map props.
- Texture discipline:
  - Standardized texel density by distance class.
  - LOD chains for all high-impact meshes and shared texture atlasing where safe.
  - Streaming and compression tuned by usage (normal/ORM/Roughness separation where used).
  - No duplicate texture copies for the same visual variant unless intentional.
- Visual quality gates:
  - No UV stretching and no visible mip gaps in first-contact gameplay paths.
  - No unresolved import warnings for textures/materials in key assets.
  - Final lighting/material pass completed for First Light and connected campaign
    sectors.
- Performance gates (must pass before 1.0):
  - texture memory and draw-call budgets stayed within documented targets,
  - no long hitches from texture streaming in multiplayer combat windows,
  - no regressions after two-hour soak.
  - The canonical `-Performance` gate now establishes the repeatable headless
    First Light CPU/memory/entity baseline documented in
    `Performance_Budget_Evidence_2026-08-01.md`. Its final capture passed at
    0.754 ms frame p95, 0.804 ms frame p99, zero >33.33 ms measured hitches,
    2177.4 MB peak physical memory, 95 actors, and 52 tick functions after
    warmup. Because it uses Editor NullRHI, rendered GPU, draw-call, texture
    streaming, and network budgets remain explicitly unverified.
  - The separate canonical `-RenderedPerformance` gate now establishes a real
    1920x1080 D3D12 baseline on an RTX 5060 Ti: 6.342 ms GPU p95, 7.304 ms
    frame p95, 7.289 ms render-thread p95, 158 draw calls p95, 271,219
    primitives p95, 46.6% local GPU-memory budget usage, 100% desired texture
    data loaded, and zero pending stream-in data. See
    `Performance_Budget_Evidence_2026-08-01.md`.
  - The canonical `-RenderedTraversalPerformance` gate now covers the First
    Light keycard, security-door, hostile-contact, and extraction route twice.
    The final 1,620-frame regression passed at 8.148 ms frame p95, 7.317 ms GPU
    p95, 24.350 ms maximum frame time, zero >33/>50 ms hitches, 100% desired
    texture-data p05/final recovery, and zero pending stream-in p95. The
    canonical `-RenderedWorldPerformance` gate additionally visits all six
    stable OpenWorld sector anchors and passed 152 marked local-traversal
    samples at 8.428 ms frame p95, 7.232 ms GPU p95, zero >33/>50 ms hitches,
    100% texture-data p05/final recovery, zero pending stream-in frames, 38%
    GPU-memory usage, 607 actors, and 79 tick functions. The gate also projects
    navigation successfully at all eight visits; the 9,216-UU Recast tiles now
    require 900,912 addresses against the 1,048,576 hard limit, resolving the
    prior capacity warning. The same static, route, and full-world gates now
    enforce graphics/compute PSO hitch counters and GPU-budget stability. The
    accepted six-sector run recorded zero PSO misses or hitch-associated misses
    at 9.385 ms frame p95 with one allowed >33 ms frame and zero >50 ms hitches.
    Cold-cache packaged PSO behavior, visible mip quality, and lower hardware
    tiers remain open. The separate
    `-RenderedMultiplayer` gate
    now proves two simultaneous 720p D3D12 clients against a dedicated server
    and 19 replicated AI: final frame p95 was 12.137/12.007 ms, GPU p95 was
    9.127/9.201 ms, both clients had zero >50 ms frames, and texture-data p05
    and final recovery were 100%. The `-RenderedMultiplayerScale` gate also
    proves the four-player ceiling with one rendered observer and three
    separate-machine stand-in peers: 7.035 ms frame p95, 4.115 ms GPU p95,
    zero >50 ms frames, 100% texture recovery, and the paired four-client
    network budget. The canonical `-RenderedMultiplayerSoak` gate additionally
    passed 61,900 measured frames per client over 601.8/589.7 seconds at
    12.845/12.488 ms frame p95 and 9.293/9.214 ms GPU p95, with zero >50 ms
    frames, 100% texture recovery, zero pending stream-in p95, and no
    deep-world stress-AI fallbacks. Visible multiplayer UI review, packaged
    rendering, lower hardware tiers, and the separate two-hour rendered
    campaign/reconnect soak remain open.

### 1.0 milestone execution order

1. **Lock 1.0 scope** (done incrementally)
   - Finalize module exit criteria for G1–G3 + GDD-08 assets track.
2. **Finish G3 gate**
   - Campaign routing + travel + reconnect + soak.
3. **Content hardening sprint**
   - Replace remaining placeholders, apply texture budget/perf fixes, finalize
     visual passes for all shipped zones.
4. **Production hardening sprint**
   - Protected campaign checkpoints now validate envelope size and CRC before
     Unreal object deserialization, fall back to the compatible backup, and
     rewrite a healed current-schema primary only after world-state application
     succeeds. `G3-CorruptPrimaryRecovery2-20260801-085022-*` verified the
     damaged-primary path with two retained clients, occupied transport and
     field-squad restoration, production operation completion, and reconnect
     convergence. `G3-LegacySchema41Migration-20260801-090437-*` additionally
     verified raw legacy `GVAS` schema-41 load and protected schema-45 rewrite
     across the same multiplayer continuity gates. Automation now locks the
     schema 41/42 difficulty and 42/43/44 progression migration boundaries.
   - Migration/save recovery, disconnect handling, replication fault tolerance,
   - Canonical command health is verified through
     `Validate-BrokenHorizon.cmd -Build -Tests -Smoke -FirstLight`: the build,
     46-test automation queue, default startup, and First Light smoke all pass.
     The validator now writes and inspects a project-local UBT build log so the
     build phase has the same evidence standard as runtime phases. Installed
     UE 5.8 still requires its normal AppData trace/config access during UBT
     startup; this is an execution-permission boundary, not a project failure.
   - Packaged-smoke policy now rejects cooked Broken Horizon package misses and
     navmesh data that loads empty. The existing July 29 package reaches First
     Light, but `BHExistingPackageStrict-Packaged.log` correctly fails it for a
     stale 8189/988 navmesh tile-size mismatch and absent legacy
     `SW_FirstLight_WindRain` / `SW_FirstLight_DistantWar` references. Neither
     reference exists in the current source content, and the package predates
     the current module; a fresh authorized package plus strict smoke is the
     remaining release gate.
     packaged build smoke + QA pass.
5. **1.0 RC**
   - Confirm all evidence logs/records, then freeze scope and publish RC candidate.

### G2 - Vertical slice

- First Light meets target art, audio, combat readability and accessibility.
- Listen and dedicated server paths pass functional tests.
- Acceptable frame time, bandwidth and replication stability under target load.
  The two-client dedicated-server baseline is now measured by the canonical
  `-NetworkBudget` gate: 10,082 B/s aggregate output p95, 6,847 B/s maximum
  per connection, 21 maximum channels, zero observed loss, and retained/rejoin
  snapshot convergence passed. See
  `Network_Budget_Evidence_2026-08-01.md`. The current four-player
  hosted-session default also passes `-NetworkScale`: 15,792
  B/s aggregate output p95, 7,484 B/s maximum per connection, 25 maximum
  channels, zero observed loss, active-operation and squad-command delivery to
  all four clients, and reconnect convergence. That initial window did not
  include bounded battle density. The
  canonical scale gate now additionally holds 12 hostile plus four friendly
  native AI active and always relevant during measurement. Including the
  three map hostiles, every client and the rejoining client observed 19 AI.
  The final bounded-density result remained within budget at 49,836 B/s
  aggregate p95, 12,584 B/s per-client maximum, and 42 channels. Rendered
  clients, geographic WAN conditions, and capacity beyond four remain open. The separate
  `-NetworkImpairment` gate now passes the same density at 80 ms lag and 3%
  loss: 50,856 B/s aggregate p95, 12,710 B/s per-client maximum, 42 channels,
  and a worst one-second observed loss count of 12 against a burst budget of
  20. Every client and rejoin still converged. This is deterministic UE packet
  simulation, not a geographically distributed online-service test. Player
  voice has no provider/interface in the current GDD-backed project contract
  and is not included in bandwidth evidence.
- Packaged clients can host, join, reconnect and complete the operation.

### G3 - Campaign prototype

- Three connected sectors support attack, defense and raid.
- Squad transport, supply route and persistent casualties survive server travel.
- The strategic map explains priorities and shared consequences.
- A two-hour multiplayer soak completes without blocking desync or save loss.

### G4-G7 - Production through release

Content production starts only after the multiplayer vertical slice passes.
Alpha requires the complete campaign. Beta locks content and focuses on
balance, accessibility, compatibility and performance. Release candidate
requires full multiplayer regression, migration, disconnect, recovery and soak
coverage with no critical defects.

## Current implementation focus

1. Start campaign prototype hardening:
   - validate mission director routing across sectors (attack/defense/raid),
   - verify persistence IDs and operation IDs remain stable across travel,
   - confirm squad transport/supply logic persists through server transitions.
2. Add G3 acceptance runbook:
   - scenario: 3-sector linked operation,
   - smoke/replay across host travel + reconnect,
   - two-hour multiplayer soak gate.
3. Capture first regression evidence for shared casualties and convoy persistence
   in runtime logs and save artifacts.
4. Keep G2 follow-through running in parallel for any remaining accessibility
   and combat-readability issues that could block campaign delivery.
5. Execute and close the 1.0 readiness checklist in
   `OnePointZero_Readiness_Checklist.md` every milestone.

## Latest completed milestone

### G0 session shell - 29 July 2026

- Added a `UGameInstanceSubsystem` that creates, advertises, searches, joins,
  travels, and destroys Broken Horizon campaign sessions.
- Added LAN development service support through `OnlineSubsystemNull`.
- Main-menu actions now host new/continued campaigns and expose a runtime join
  button plus session status without requiring a binary UMG edit.
- Returning to command saves only on authority, then leaves the online session
  cleanly for hosts and clients.
- `BrokenHorizonEditor Win64 Development` compiled successfully.
- All 38 `BrokenHorizon` automation tests passed, including
  `BrokenHorizon.Multiplayer.Session.Contract`.
- Startup and First Light unattended smoke tests passed with no fatal,
  assertion, Blueprint, network, load, or streaming error markers.

Manual review still required: verify the inserted join control in the rendered
  main menu and complete an actual two-process host/join/leave playtest.

### G2 accessibility and combat-readability baseline - 1 August 2026

- Extended the existing user-settings save to schema 2 with preserved migration
  for mouse, audio, window, and quality values plus persistent HUD scale
  (75-150%), Standard/Deuteranopia/Protanopia/Tritanopia palettes, high-contrast
  HUD, and reduced-motion preferences.
- Kept the existing Blueprint `ApplySettings` contract intact and added a
  separate accessibility application API. Optional settings-widget bindings
  allow the shipping UMG to expose the controls without renaming old widgets.
- Shared native UI styling now applies scale and palette-safe friendly/danger
  colors, strengthens text shadows and panel separation in high-contrast mode,
  stops active widget animations in reduced-motion mode, and refreshes already
  open native widgets immediately after settings change.
- `BrokenHorizonEditor Win64 Development` compiled successfully. All 45
  `BrokenHorizon` automation tests passed in
  `BHAccessibility-Tests.log`, including
  `BrokenHorizon.UI.Accessibility.SettingsContract`. Main-menu startup passed in
  `BHAccessibility-Smoke.log`; First Light loaded and exited cleanly in
  `BHAccessibility-FirstLightVerified.log`.
- Manual review remains required for UMG control binding/layout, 75% and 150%
  safe-zone clipping, palette legibility on representative displays, reduced
  motion coverage in Blueprint-authored animations, and multiplayer combat
  readability. Color remains redundant with existing labels/readouts rather
  than becoming the only carrier of state.

### GDD-09 dynamic remapped gameplay prompts - 1 August 2026

- Added one authoritative keyboard/controller prompt formatter to the saved
  input-binding subsystem. The Blueprint-facing API returns current overrides,
  omits empty device separators, and compacts controller names into readable,
  platform-neutral labels.
- Combat medical/grenade state, bleeding response, fireteam orders, squad hold
  waypoints, operative stabilization, and standard interactables now resolve
  binding IDs at display time. Existing content with legacy bracket tokens is
  migrated without changing its asset or interface contract.
- `BHDynamicInputPromptsRenderedFinal-RenderedMultiplayer-20260801-215148-*`
  passed paired 1280x720 D3D12 capture under 19-AI combat density. Direct image
  inspection confirms dual-device prompts fit without clipping; frame p95 was
  11.807/11.765 ms and GPU p95 was 9.164/9.117 ms.
- The strategic-map footer also resolves the saved `WarMap` binding without
  rewriting unrelated fixed map shortcuts. The complete 33-capture
  `BHDynamicInputPromptsUIFinal2-RenderedUI-20260802-052750-*` matrix passed;
  direct 720p inspection confirms `[M / VIEW] CLOSE MAP` remains legible.
- The UE5.8 editor target compiled and `BHDynamicInputPromptsComplete-*` passed
  all 66 tests, startup, and the four-objective First Light route with 8/12
  bounded navigation fallbacks. Physical-device switching, controller glyph
  preference, and a complete interactive rebind sweep remain manual.

### GDD-09 independent look controls - 1 August 2026

- Advanced the user-settings save contract to schema 3. Existing schema-1/2
  profiles migrate their legacy mouse sensitivity into both independent axes;
  horizontal sensitivity, vertical sensitivity, ADS multiplier, and vertical
  inversion are then persisted separately.
- Character look input applies the saved horizontal and vertical scales on
  every look event, composes the ADS multiplier while aiming, and applies
  inversion only to the vertical axis.
- The existing settings Blueprint contains only its legacy mouse slider, so
  `UBHSettingsWidget` now supplies a native right-anchored `LOOK CONTROLS`
  panel at runtime without editing or renaming the binary UMG asset. Future
  authored controls with the established names replace this fallback through
  the existing optional bindings.
- `BrokenHorizonEditor Win64 Development` compiled. Focused settings automation
  passed in `BHIndependentLookSettings-Focused2.log`; the canonical retry ran
  all 60 tests with zero failures and queue empty in
  `BHIndependentLookSettings-Retry-Tests.log`. Asset readiness, main-menu
  startup, and First Light smoke gates also passed under the same prefix.
- `BHLookSettings-UIValidation2.log` proves the current HUD/menu assets load and
  the native UI source contract is present. A rendered mouse/controller
  playtest remains required for panel placement, focus navigation, displayed
  numeric clarity, and subjective hip-fire/ADS feel.

### GDD-09 full input remapping - 1 August 2026

- Advanced user settings to schema 4 with separate persistent keyboard/mouse
  and controller binding maps. Twenty-seven stable binding IDs cover movement,
  look, posture, combat, interaction, medical actions, war map, and squad
  coordination; schema-3 profiles migrate without losing prior preferences.
- Locally controlled characters duplicate `IMC_Player` into a transient runtime
  mapping context, preserve authored modifiers/triggers, apply saved overrides,
  add controller coverage, and rebuild the context immediately after settings
  change. No input `.uasset` was edited.
- Settings now expose a native scrollable keyboard/controller capture overlay.
  Device-invalid inputs are rejected, conflicts swap in the staged UI, defaults
  can be restored, and the complete map is validated and saved atomically on
  Apply.
- `BHInputRemapping-Focused.log` passed the schema, `SaveGame`, stable-ID,
  uniqueness, device, coverage, and Blueprint API contract. The canonical
  `BHInputRemapping-Final-*` run compiled, passed all 60 tests, passed the asset
  audit, and passed startup/First Light smoke. First Light logged
  `BH_INPUT_BINDINGS_APPLIED mappings=43`, proving installation on the local
  gameplay character. `BHInputRemapping-UIValidation.log` completed with zero
  errors and zero warnings.
- Manual rendered review remains required for row sizing, focus navigation,
  physical-controller axis capture, glyph clarity, and an interactive rebind
  sweep of every action.

### GDD-09 network-safe menus - 1 August 2026

- Pause/settings/controls menus now pause the world only in standalone play.
  Listen servers, dedicated servers, and connected clients keep the shared
  simulation running while the local player receives UI focus and local
  move/look input suppression.
- Resume and character teardown only clear global pause when the world was
  eligible for standalone pause, preventing one client's menu lifecycle from
  mutating shared session time.
- `BHNetworkSafeMenu-Focused.log` passed explicit standalone, listen-server,
  dedicated-server, and client policy assertions. The canonical
  `BHNetworkSafeMenu-Final-*` build, 60-test suite, startup smoke, and First
  Light smoke all passed with zero strict failures.
- A manual two-client review remains required to confirm that the remote player,
  AI, objectives, and war clock visibly continue while either peer has the menu
  or remapping overlay open.

### GDD-09 toggle and hold input modes - 1 August 2026

- Advanced local settings to schema 5 with persistent activation preferences
  for aim, sprint, crouch, prone, lean, and interaction. Migration preserves the
  established defaults: hold aim/sprint/crouch/lean, toggle prone, and tap
  interaction.
- Keyboard and controller actions share the same press/release policy. Toggle
  actions remain active across release and reverse on the next press; hold
  actions stop on release/cancel. Opposite toggle-lean input clears the prior
  side before activating the new side.
- Hold-to-interact requires 0.35 seconds and commits only on a completed release;
  canceled or short holds cannot issue a local or server interaction.
- The remapping overlay now includes all six mode controls. Defaults, cancel,
  saved-value restore, and atomic Apply behavior are integrated without editing
  the binary settings asset.
- `BHToggleHoldModes-Focused.log` passed migration defaults, `SaveGame` flags,
  toggle/hold transitions, interaction threshold, and Blueprint API coverage.
- `BHToggleHoldModes-UIValidation.log` completed with zero errors/warnings, and
  canonical `BHToggleHoldModes-Final-*` build, 60-test suite, asset, startup,
  and First Light gates passed.
- Manual keyboard/controller review remains required for rapid mode changes,
  animation transitions, blocked prone exits, sustained lean comfort, and hold
  interaction feedback timing.

### GDD-09 camera and visual comfort controls - 1 August 2026

- User-settings schema 6 persists independent 0-100% camera-shake,
  recoil-motion, head-bob, and damage-flash intensity plus explicit motion
  blur, depth-of-field, and chromatic-aberration switches.
- Rifle fire shake, local recoil presentation, locomotion head bob, and damage
  tint read the live settings. Post-process switches apply through game-setting
  console-variable priority without changing weapon spread or authoritative
  combat state.
- The native settings/remapping overlay exposes all seven controls and restores,
  defaults, applies, and saves them with the existing settings transaction.
- Evidence: `BHVisualComfort-FocusedFinal.log` passed the focused settings
  contract; `BHVisualComfort-UIValidation.log` passed the read-only UI validator;
  `BHVisualComfort-Final-*` passed editor build, all 60 automation tests, asset
  readiness, startup smoke, and First Light smoke. Visual feel still requires
  a rendered playtest.

### GDD-09 subtitle presentation - 1 August 2026

- User-settings schema 7 persists subtitle enablement, speaker labels,
  directional indicators, 75-200% subtitle size, background opacity, and an
  80-100% subtitle safe-width control.
- Every locally controlled character owns a native timed subtitle layer. Its
  Blueprint-callable gameplay API accepts speaker, line, duration, and optional
  relative direction, so mission and radio content can use the same accessible
  presentation without a binary widget dependency.
- Direction is redundant with speaker/text and uses front/right/back/left
  glyphs. The layer auto-wraps, centers, applies the current profile, and never
  pauses or mutates the network session.
- `BHSubtitles-Focused.log` and `BHSubtitles-UIValidation.log` passed. Canonical
  `BHSubtitles-Final-*` passed editor build, all 60 automation tests, asset
  readiness, startup smoke, and First Light smoke. Authored voice-line hookup
  and rendered safe-frame/legibility review remain manual content gates.
- Mission objective definitions now expose optional activation/completion radio
  lines, speaker, duration, and direction. The character automatically queues
  authored lines at mission start and objective transitions, so a completion
  line cannot overwrite the following instruction. The focused
  `BHObjectiveRadioSubtitles-Focused.log` contract, UI validator, and canonical
  `BHObjectiveRadioSubtitles-Final-*` gate passed all 61 project tests, asset
  readiness, startup, and First Light.

### GDD-09 global safe area and difficulty assists - 1 August 2026

- The saved 80-100% safe-frame setting now wraps every interface registered
  through the shared UI style: menus, gameplay HUD, overlays, and alerts. It is
  independent from the existing 75-150% HUD scale and refreshes live.
- Deterministic normalized-inset math is clamped and automation-covered. The
  wrapper is created once per widget and updated idempotently, preserving the
  authored binary widget tree beneath a runtime-only canvas root.
- Difficulty assists were already implemented as host-authoritative Recruit
  and Custom campaign modes. Custom exposes incoming damage, enemy perception,
  enemy coordination, medical pressure, strategic pressure, and checkpoint
  interval axes; the profile saves, replicates, and is consumed by combat, AI,
  injury, logistics, and campaign-pressure systems.
- `BHSafeArea-Focused.log` and `BHSafeArea-UIValidation.log` passed. Canonical
  `BHSafeArea-Final-*` passed editor build, all 60 automation tests, asset
  readiness, startup, and First Light smoke. Multi-resolution rendered review
  remains required before the GDD-09 production gate can be signed off.

### G1 host-loss checkpoint - 30 July 2026

- Ran the dedicated host-loss/rejoin runtime path with unique ports per cycle to
  prevent socket drift and false join failures.
- Host readiness and continuity markers observed on dedicated host and restarted
  host runs: `BH_WAR_GAME_STATE_READY`, `BH_SHARED_MISSION_SNAPSHOT_PUBLISHED`.
- Two-cycle and additional repeatable checks recorded in `Saved/Logs` under
  `Phase1-*` and `BHPhase1-*` naming.

### GDD-04 emergency fallback logistics - 1 August 2026

- Closed the GDD-04 soft-lock gap at friendly sector stations. If a sector
  cannot pay the normal strategic resupply cost and the player has zero reserve
  ammunition or no medical resources, the station now issues a cooldown-limited
  emergency kit: up to 30 reserve rounds, one medkit, and one field dressing.
- The emergency path intentionally does not repair armor, replenish grenades,
  service the fireteam, recover a transport, or repair/refuel a vehicle. A
  combat-effective player cannot repeatedly farm the free kit.
- Depleted friendly stations advertise `[F] Emergency Fallback Kit` instead of
  presenting a dead interaction. Issuance updates the checkpoint and records
  `BH_EMERGENCY_FALLBACK_KIT` evidence.
- `BrokenHorizon.PersistentWar.Logistics.EmergencyFallbackKit` verifies the
  critical-load qualification and capacity-clamped ammunition contract. It
  passed alone in `BHEmergencyFallbackKit-Focused.log` and as part of the
  57-test `BHEmergencyFallbackKit-Final-Tests.log` suite.
- `BHEmergencyFallbackKit-Final-Smoke.log` and
  `BHEmergencyFallbackKit-Final-FirstLight.log` passed; First Light reached
  `BH_WAR_GAME_STATE_READY`, and all three final logs contain zero strict
  fatal/assertion/unhandled/test-failure markers.
- Manual multiplayer play remains required to judge station discoverability,
  notification clarity, and whether the 60-second fallback cooldown feels
  appropriately restrictive during an actual supply collapse.

### GDD-04 multiplayer medical and vehicle recovery - 1 August 2026

- Fixed a multiplayer correctness gap in `UBHInjuryComponent`: bleeding,
  limb injuries, medical inventory, armor durability, and treatment state now
  replicate owner-only and rebroadcast the existing Blueprint/HUD delegates on
  receipt. Server-authoritative station resupply and treatment can no longer
  leave the owning client's medical HUD on stale local values.
- Player-initiated field dressing and medkit requests now pass through reliable
  character server RPCs before changing medical inventory, bleeding, treatment,
  or checkpoint state. The client receives the existing owner-only state
  transition instead of authoring a local replicated medical spend.
- Added a non-shipping `-BHTestMedicalRecoveryReplication` fixture. It prepares
  two distinct depleted player states, applies authoritative ammunition,
  medical, and armor recovery, and recovers a spawned zero-fuel/zero-hull
  transport through `RecoverAndService`.
- Extended `Test-BrokenHorizonMultiplayer.ps1` with
  `-RequireMedicalRecoveryReplication`. The harness requires the two retained
  clients to observe different owner-only medical inventories and requires the
  host to report a recovered transport at full fuel and hull.
- `GDD04-MedicalVehicleRecovery-20260801-112726-Summary.json` passed on port
  7918 in First Light. Client A received one medkit/one dressing, Client B
  received two medkits/one dressing, and neither client observed the other's
  terminal owner state. The host recorded 30 and 45 reserve rounds respectively
  plus `vehicle_recovered=1 fuel=1.00 hull=1.00`.
- The direct editor build and focused casualty-aid component contract passed.
  The post-fixture 57-test suite, startup smoke, and First Light smoke passed in
  `BHMedicalVehicleReplication-Final-*`; First Light reached
  `BH_WAR_GAME_STATE_READY` and all final/runtime logs contain zero strict
  failure markers.
- This proves authority, owner delivery/isolation, and the transport recovery
  contract. Manual play remains required for three-second treatment feel,
  station line-of-sight interaction, passenger presentation, and vehicle
  recovery placement quality.

### GDD-02 open-world operation authority hardening - 8 August 2026

- Open-world operation directors now advance their tick, restore operation
  state, accept support orders, react to raid sabotage, resolve completion or
  failure, and destroy tracked operation actors only on the authoritative
  server. Replicated clients retain read-only operation state and presentation
  without running a second simulation.
- Editor compilation, multi-client operation-order coverage, reconnect/restore,
  and First Light route evidence remain required before this authority pass can
  be treated as verified.
- Ambient patrols and convoy security now apply the same server-only rule to
  their tick, load-time reset, salvage-security restore, and teardown paths;
  replicated weather and audio mix values remain available to clients.

### GDD-06 defense operation authority hardening - 8 August 2026

- Defense wave initialization, tracked defender capture, reinforcement spawning,
  wave timing, teardown, and objective resolution now run only on the
  authoritative defense director. A client-side director disables its tick
  instead of creating local enemies or advancing a local operation state.
- Wave notifications continue through the existing character presentation path;
  this source change still requires editor compilation and a two-client First
  Light defense run to prove replication and notification timing.

### GDD-05 field command hardening - 1 August 2026

- Confirmed that field operatives and operation support already accept the
  authoritative Follow, Hold, and aimed Move-and-Hold command path, including
  persistent command location/yaw and an owner-only replicated field roster.
- Hardened Move-and-Hold navigation so a successful arrival faces the squad at
  the commander's issued yaw. Rejected or failed paths retry a bounded three
  times, emit `BH_SQUAD_MOVE_HOLD_FAILED` telemetry, and fall back to a safe
  hold at the operative's current location instead of retrying forever or
  becoming silently immobile.
- Route failures now propagate from each AI controller to its authoritative
  owning character. The commander receives a deduplicated `SQUAD // ROUTE
  BLOCKED` notification identifying the operative, failed distance, safe-hold
  behavior, and redirect control; `BH_SQUAD_MOVE_HOLD_REPORTED` records the
  player-facing delivery path. Operation support uses the same owner route.
- `BrokenHorizonEditor Win64 Development` compiled successfully. All 45
  `BrokenHorizon` automation tests passed in `BHMoveHold-Tests.log`; main-menu
  startup and First Light loaded and exited in `BHMoveHold-Smoke.log` and
  `BHMoveHold-FirstLight.log`.
- The player-facing failure-report extension compiled successfully and all 45
  tests passed again in `BHSquadFailureReport-Tests.log`. Main-menu and First
  Light smokes passed in `BHSquadFailureReport-Smoke.log` and
  `BHSquadFailureReport-FirstLight.log`, with no fatal, assertion, unhandled,
  Blueprint-error, or Windows-error markers.
- Manual multiplayer PIE review remains required for command ownership
  visibility, NavMesh edge/failure feedback, formation spacing, final facing,
  and command feel under simultaneous players.

### GDD-05 general navigation failure fallback - 1 August 2026

- General AI path-following failure now has state-specific safe behavior rather
  than advancing as though the destination was reached. Failed investigation
  or explosive evasion becomes a bounded search at the last-known location;
  repeated search failure holds safely until its search timer expires; failed
  return falls back to patrol with a delayed retry.
- Combat movement failure releases an unreachable cover claim and delays the
  next pursuit/reposition request instead of silently occupying the anchor.
  Retreat failure holds at the last reachable location while preserving morale
  and ammunition-withdrawal resolution deadlines.
- Every runtime fallback emits `BH_AI_NAVIGATION_FALLBACK` with the failed and
  selected states for navigation-coverage diagnosis.
- `BrokenHorizon.Gameplay.AI.NavigationFailureFallback` passed alone and as
  part of all 59 `BrokenHorizon` tests. The canonical
  `BHAINavigationFallback-Final-*` gate compiled the editor target, passed the
  full queue, passed the read-only asset audit, launched the main menu, and
  loaded First Light through `BH_WAR_GAME_STATE_READY`; all five final logs
  contain zero strict failure markers.
- Manual First Light and campaign-sector play remains required to inspect real
  NavMesh gaps, cover choices, fallback telegraphing, and recovery pacing.

### GDD-05 field-squad Context action - 1 August 2026

- Added a native `X` Context command without changing binary input assets.
  Aiming at a downed friendly field operative now lets the authoritative
  commander assign the nearest combat-effective operative from their own
  three-person roster to casualty aid at command range.
- The server independently repeats the visibility trace, validates the target,
  roster ownership, responder health, embarked state, and available field
  dressing before redirecting AI. Other players' responders are never
  selected, so one commander cannot silently overwrite another commander's AI
  order.
- The responder moves through the existing bounded follow/navigation path,
  consumes one commander field dressing only on arrival, stabilizes the
  casualty, and resumes the prior Follow/Hold/Move-and-Hold formation order.
  Pressing `C` explicitly cancels the assignment; invalidation and a 35-second
  timeout also restore the formation with player-facing feedback.
- Raid sabotage now uses the same Context command and the existing authoritative
  `ABHRaidSabotageTarget` contract. An owned operative approaches the active
  cache, arms charges only after reaching it, triggers the established reaction
  force/exfiltration transition, and then resumes the previous formation order.
  A second player's pending assignment observes the resolved target and ends
  safely rather than overwriting either squad.
- Sector anchors now expose a visibility-only, non-blocking Context target at
  their operation center. During the matching active attack or defense, an
  owned operative can be sent to secure/hold that line; mismatched sectors,
  inactive operations, raids, foreign responders, and embarked squads reject
  the order explicitly.
- Objective progress recognizes living recruited operatives through their
  authoritative character owner. Generic operation-support AI does not satisfy
  attack/defense completion automatically. Once the assigned operative reaches
  the line, it remains on Move-and-Hold until cancellation or operation end and
  receives distinct secure/defend confirmation feedback.
- Unsupported targets still produce explicit feedback rather than fabricated
  interactions.
- `BrokenHorizon.PersistentWar.FieldSquad.ContextCasualtyAid` covers supported,
  foreign-responder, unsupported-target, embarked, no-supply, and authoritative
  RPC contracts. `BHFieldSquadContext-Focused.log` records its focused pass.
  The editor target compiled successfully; all 53 `BrokenHorizon` tests reached
  `Automation Test Queue Empty`, and main-menu plus First Light smokes passed in
  the `BHFieldSquadContext-CoopFinal-*` evidence set with clean strict markers.
- `BrokenHorizon.PersistentWar.FieldSquad.ContextSabotage` covers owned versus
  foreign responders, active versus resolved raid targets, embarked rejection,
  and preservation of direct player interaction. Both Context tests passed in
  `BHFieldSquadObjectiveContext-Focused.log`.
- The final editor build succeeded; all 54 `BrokenHorizon` tests reached
  `Automation Test Queue Empty`, and main-menu plus First Light smokes passed
  with no strict failure markers in `BHFieldSquadObjectiveContext-Final-*`.
- `BrokenHorizon.PersistentWar.FieldSquad.ContextObjectivePresence` covers
  attack/defend eligibility, raid exclusion, active-operation and sector
  matching, ownership, embarkation, and the sector-anchor target contract. All
  three Context tests passed in `BHFieldSquadFullContext-Focused.log`.
- The final editor target compiled successfully; all 55 `BrokenHorizon` tests
  reached `Automation Test Queue Empty`, and main-menu plus First Light smokes
  passed with zero strict failure markers in `BHFieldSquadFullContext-Final-*`.
- Active Context ownership now persists in the native fireteam HUD after the
  transient notification fades. The owner-only replicated action, target label,
  and arrival phase render `CONTEXT <ACTION> // MOVING|ACTIVE // <TARGET>` for
  aid, sabotage, secure, and defend without exposing another player's order.
  The panel expands only while the line is present and retains existing
  readiness, service, medevac, Follow, Hold, and embarked information.
- `BrokenHorizon.Gameplay.UI.FieldSquadContextOwnership` verifies label
  composition, clear state, panel integration, replicated properties, and the
  Blueprint-callable HUD extension. It and the existing squad HUD/readiness
  contracts passed in `BHFieldSquadContextHUD-Focused.log`.
- The final editor build succeeded; all 56 `BrokenHorizon` tests reached
  `Automation Test Queue Empty`, and main-menu plus First Light smokes passed
  with zero strict failure markers in `BHFieldSquadContextHUD-Final-*`.
- Added a development-only bounded multiplayer fixture for owner-only Context
  replication. A dedicated host creates two independent player-owned field
  squads with distinct action, target, and arrival states; each retained client
  must observe exactly one unique owner label, never both, before the standard
  reconnect convergence check proceeds.
- `GDD05-ContextOwnership-20260801-111217-Summary.json` passed on port 8292.
  Client A received only `CONTEXT_OWNER_A` (Secure/Moving), Client B received
  only `CONTEXT_OWNER_B` (Defend/Active), the host recorded two separate
  configured characters, and the rejoining client converged on the same war
  snapshot. No fatal, assertion, network, travel, PIE, or autosave failure
  marker appeared in the four runtime logs.
- This fixture proves the network ownership boundary and persistent replicated
  HUD state. Manual two-client play remains required for simultaneous live
  targeting, navigation around combat, animation, and command feel.
- Manual two-client PIE review remains required for long-range target
  readability, navigation around live combat, cancellation feel, simultaneous
  casualty assignments, and dressing ownership clarity.

### G3 seamless active-operation travel - 1 August 2026

- Extended the bounded multiplayer harness with a development-only scheduled
  server-travel phase. It requires both clients to apply the pre-travel war
  snapshot, remain connected through a complete map lifecycle, apply the
  post-travel snapshot, and then verifies a disconnected/rejoining client
  converges on the same signature and operation ID.
- The first run exposed a real defect: ordinary server travel closed both
  client connections even though the host retained campaign state. Enabled
  seamless travel on `ABHGameMode` and added an authority contract assertion
  so future defaults cannot silently regress.
- The verified run on port 8224 retained both clients through First Light
  travel and produced the matching post-travel/rejoin signature
  `1:2:6:Operation_53C37137480161D5223E4A9B88464B44`. Evidence is in
  `G3-SeamlessTravelVerified-20260801-034127-Summary.json` and its Host,
  ClientA, ClientB, and RejoinA logs. No fatal, assertion, unhandled,
  network-failure, travel-failure, or snapshot-apply-failure marker appeared.
- `BrokenHorizonEditor Win64 Development` compiled successfully. All 45 tests
  passed in `BHSeamlessTravel-Tests.log`; main-menu and First Light smokes
  passed in `BHSeamlessTravel-Smoke.log` and
  `BHSeamlessTravel-FirstLight.log`.
- Remaining proof: host-process crash/recovery, operation objective completion
  across travel, join timing under simulated latency/loss, and the two-hour
  multiplayer soak.

### G3 occupied transport checkpoint travel - 1 August 2026

- Extended the isolated multiplayer harness to deploy a real active operation,
  create a per-run checkpoint, board a stable-ID field transport with two squad
  passengers, and execute the production checkpoint-load seamless-travel path
  with two retained clients plus one reconnect.
- Fixed dedicated authority player resolution after travel, bounded pending-save
  retries until a server pawn exists, explicitly hands an occupied transport back
  to its controller before travel, and recovers pawnless controllers in
  `PostSeamlessTravel`.
- The verified run restored operation
  `Operation_E44C6BF640FDE6FCD870DCA197262E70`, transport
  `WesternFOBFieldTransport01`, driver possession, cargo `9.0`, fuel `0.63`, hull
  `0.71`, and two embarked field operatives. Both original clients survived the
  map lifecycle and the rejoin converged with the retained client. Evidence is
  `G3-TransportTravelVerified2-20260801-043741-Summary.json` and its four process
  logs; isolated test saves were removed automatically.
- `BrokenHorizonEditor Win64 Development` compiled successfully. All 45 tests
  passed in `BHTransportTravel-Tests.log`; main-menu and First Light smokes
  passed in `BHTransportTravel-Smoke.log` and
  `BHTransportTravel-FirstLight.log`.
- A second run restored attack operation
  `Operation_8295D0AF40CE5FA43EE661AA1F9331E3` in its approach phase and completed
  it after travel through the production director/objective/debrief path. The
  result checkpoint saved, the strategic operation lock cleared, and retained
  plus rejoining clients converged on `9:1:6:None`. Evidence is
  `G3-OperationCompletionTravel-20260801-044653-Summary.json` and its four logs.
  All 45 tests plus startup and First Light smokes passed again in the
  `BHOperationTravel-*` logs.
- Full process-replacement recovery now passes in
  `G3-HostCrashRecovery2-20260801-050238-Summary.json`: an isolated checkpoint
  created by the first host restored operation
  `Operation_8D36241B480609501A406A81B7668D3F`, its occupied transport, cargo,
  vehicle condition, and two passengers in a fresh host process. The first run
  found and fixed an autosave race that could overwrite the recovery checkpoint
  while waiting for clients. All 45 tests and both smokes passed in the
  `BHHostRecovery-*` logs.
- Bounded impairment validation now passes with 80 ms outgoing latency and
  three-percent packet loss applied to the host, both retained clients, and the
  rejoin client. They survived active-operation seamless travel and converged on
  `1:2:6:Operation_6343CAB74C3E2F36E49160B5383A1839`; evidence is
  `G3-NetImpairment2-20260801-051559-Summary.json` and its four logs.
- The full two-hour campaign soak passed in
  `G3-TwoHourCampaignSoak-20260801-055216-Summary.json`. The dedicated host and
  two retained clients remained alive for all 7,200 monitored seconds after
  seamless open-world travel; war/field checkpoints continued while the
  campaign advanced to turn 40. Client A, Client B, and the fresh rejoin client
  converged on
  `42:40:6:Operation_27204FAE4E104A5C4A28C0B23617001C`. All four logs were
  clean under the strict fatal/assertion/network/travel/UI/autosave marker set,
  all owned processes exited, and the isolated checkpoint was removed.
- Attack, defense, and raid routing now each pass the same production
  deployment/checkpoint-travel/completion/reconnect path. Evidence is
  `G3-AttackRoute3-20260801-080404-*` (`NorthPass`, type 1),
  `G3-DefendRoute-20260801-080702-*` (`DovrenVillage`, type 2), and
  `G3-RaidRoute-20260801-081000-*` (`KoronaCrossroads`, type 3). Each restored
  the occupied transport and squad, completed through the real director and
  objective path, cleared the operation, and converged retained/rejoining
  clients on a `None` operation snapshot. The editor target built successfully;
  all 45 tests and both smokes passed in `BHCampaignRoutes-*`.
- Automated G3 routing, travel, reconnect, recovery, impairment, and two-hour
  stability proof is complete. Manual networked playtesting remains required
  for entry/exit feel, passenger presentation, collision, animation continuity,
  and subjective mission pacing/readability.

### G1 two-player First Light completion - 1 August 2026

- First Light objectives now progress as authoritative squad-shared state and
  replicate current objective, completed IDs, completion, and failure to each
  client. Client replication refreshes presentation without reapplying
  authority-only mission/campaign results.
- Keycard access is shared across connected player pawns, and First Light is an
  explicit supported save map so its intermediate/final checkpoints pass the
  same compatibility validation as the campaign world.
- The accepted dedicated-host/two-client run is
  `G1-FirstLightSharedReconnect-20260801-083153-Summary.json` plus its host,
  two initial-client, and rejoin logs. Authority completed all four canonical
  objectives for both players; both initial clients received
  `completed=4 complete=1 failed=0`. A fresh replacement pawn then inherited
  the completed squad state from retained Client B, and RejoinA converged.
  Eight isolated checkpoint writes succeeded, strict failure counts were zero,
  and cleanup removed every owned process and test save.
- The editor target built successfully. All 45 tests passed in
  `BHFirstLightReconnect-Tests.log`; main-menu and First Light smokes passed in
  the corresponding `BHFirstLightReconnect-*` logs.
- Manual two-player review remains required for physical route execution,
  navigation, combat and extraction feel, rendered UMG, audio, and animation.

### GDD-08 AI voice bark and audio/FX readiness audit - 1 August 2026

- Enemy soldiers now expose seven Blueprint-authorable positional voice-bark
  contracts: alert, contact, reload, grenade, casualty, retreat, and search.
  Authority selects barks from real AI state, reload, grenade, and death paths;
  an unreliable multicast delivers them to clients and a configurable cooldown
  prevents non-casualty spam.
- `BrokenHorizon.Gameplay.AI.VoiceBarkContract` verifies cooldown behavior,
  reflected authoring properties, the Blueprint-callable trigger, and the
  multicast contract. The canonical `BHAIVoiceBarks-Final-*` run built the
  editor target and passed all 62 tests, asset readiness, startup smoke, and
  First Light smoke.
- `Content/Python/validate_audio_fx_readiness.py` and
  `Validate-BrokenHorizon.cmd -AudioFX` provide a read-only canonical audit of
  the player weapon, base enemy, and First Light guard contracts. The accepted
  `BHAudioFXAudit-Canonical-AudioFX.log` run completed with zero audit errors.
- The report deliberately keeps content readiness open: only 1 of 18 required
  assignments and 0 of 27 optional assignments currently resolve. Player dry
  fire/reload, enemy fire, muzzle/impact FX, and all seven bark categories still
  need production assets and authored Blueprint assignments. Source contracts
  and validation are implemented; final audio content is not claimed complete.

### GDD-08 movement acoustics - 1 August 2026

- Player movement now emits server-authoritative hearing stimuli that the
  existing enemy hearing sense can investigate. Cadence and loudness respond
  deterministically to horizontal speed, sprint, crouch, prone posture,
  physical surface, and an authorable equipment-load multiplier, making quiet
  movement a real stealth decision rather than presentation only.
- Each authoritative step multicasts positional presentation to clients and
  resolves authorable default, concrete, dirt, grass, metal, and water cues.
  Dedicated servers skip playback while retaining AI-hearing simulation.
- `BrokenHorizon.Gameplay.Movement.AcousticsContract` verifies posture ordering,
  surface/equipment amplification, cadence, reflection, and unreliable
  multicast presentation. `BHMovementAcoustics-Final2-*` built successfully,
  passed all 63 tests, both read-only audits, startup smoke, and First Light
  smoke with clean canonical log checks.
- Audible footstep assets are not yet assigned. The expanded readiness report
  therefore records 1 of 18 required and 0 of 27 optional assignments; gameplay
  noise behavior is active, while final sound authoring and subjective mix/
  stealth-distance playtesting remain open.

### GDD-08 weather and wider-war audio - 1 August 2026

- The ambient war director now replicates a shared quiet, tense, frontline, or
  combat audio state. Frontline adjacency, active operations, physical hostile
  presence, and strategic response pressure drive the state, so the wider-war
  layer follows campaign conditions rather than playing indiscriminately.
- Authoritative weather systems can set clamped wind and rain intensities
  through a Blueprint API. Clients crossfade authorable wind, rain, and distant
  war loops; dedicated servers skip playback while retaining state ownership.
- Frontline and combat states schedule bounded, spatially offset artillery,
  aircraft, or small-arms one-shots and multicast the chosen event/location to
  all clients. Quiet areas retain only a restrained bed and do not generate
  intermittent spectacle.
- `BrokenHorizon.Gameplay.World.AmbientWarAudioContract` verifies state
  precedence, monotonic war-bed intensity, reflected authoring slots, weather
  control, and multicast events. `BHAmbientWarAudio-Final-*` built and passed
  all 64 tests, both audits, startup smoke, and First Light smoke.
- Production loops and one-shots are not assigned. The expanded audit records
  1 of 18 required and 0 of 27 optional assignments, so final sound creation,
  attenuation/mix tuning, and rendered multiplayer listening review remain
  explicit content gates.

### GDD-08 UI confirmation and warning audio - 1 August 2026

- Objective notifications now carry an explicit audio semantic independent of
  visual priority: no cue, quiet confirmation, strategic warning, or immediate
  combat alarm. Legacy Normal/High/Critical APIs remain Blueprint-compatible
  and map to confirmation/strategic/combat defaults.
- Explicit cue selection propagates through a reliable owner-client RPC, so
  dedicated-host notifications retain the same meaning as listen-server and
  standalone presentation. Queued/preempted notifications preserve their
  original cue and play only when actually presented.
- Objective completion deliberately uses a quiet confirmation despite critical
  visual priority; operative loss uses a strategic warning. Unclassified
  critical danger retains the immediate combat-alarm default.
- `BrokenHorizon.UI.Notification.PriorityContract` now verifies semantic
  mapping, reflected sound slots, Blueprint override APIs, and the networked
  character entry point. `BHUINotificationAudio-Final-*` built and passed all
  64 tests, both audits, startup smoke, and First Light smoke.
- Quiet confirmation, strategic warning, and combat alarm assets remain
  unassigned. The readiness report therefore records 1 of 18 required and 0 of
  27 optional assignments; final content, loudness hierarchy, and rendered
  controller/multiplayer listening review remain open.

### GDD-08 weapon acoustic space and near misses - 1 August 2026

- Player and enemy weapon presentation now probes the ceiling and four lateral
  directions around each muzzle. A majority-enclosed result selects the indoor
  report tail; open or weakly occluded spaces select the outdoor tail. The
  existing mechanical fire cue remains independent so designers can mix the
  weapon body and environment response separately.
- The existing authoritative enemy near-miss/suppression event now plays a
  directional cue beside the local player. Volume and pitch scale with miss
  intensity, while a short minimum interval prevents automatic fire from
  producing an unusable wall of overlapping transients.
- `BrokenHorizon.Gameplay.Combat.WeaponAudioSpaceContract` verifies enclosure
  classification, near-miss intensity clamping, and player/enemy reflected
  authoring slots. `BHWeaponAudioSpace-Final-*` built and passed all 65 tests,
  both audits, startup smoke, and First Light smoke.
- Indoor/outdoor tails and the near-miss cue are unassigned production content.
  The current audit records 1 of 18 required and 0 of 27 optional assignments;
  final assets, attenuation/reverb, occlusion, and multiplayer listening review
  remain open.

- Multiplayer main-menu clarity now exposes consistent READY, HOSTING,
  SEARCHING, JOINING, CONNECTING, CONNECTED, LEAVING, and ACTION FAILED
  headings with severity colors. Synchronous session errors reach the
  Blueprint-facing error event once rather than twice. `BHSessionClarity-*`
  compiled and passed the 46-test suite plus default and First Light smokes;
  rendered focus, wrapping, safe-zone, and gamepad review remains manual.
- The soak harness now performs continuous process and failure-marker monitoring
  between post-travel convergence and reconnect. Its 180-second shakedown passed
  in `G3-SoakShakedown2-20260801-052328-Summary.json`; this is harness evidence
  only and does not replace the pending 7,200-second acceptance run.
- The first full-duration attempt was rejected after detecting default-slot
  autosave failures and stale seamless-travel controller UI creation. Soak runs
  now isolate and clean their checkpoints, fail on autosave or `PIE: Error`
  markers, require a real attached `ULocalPlayer` before creating UI, and stagger
  editor-client startup. The hardened short proof passed in
  `G3-TravelUIFix2-20260801-053906-Summary.json`, followed by all 45 tests and
  both smokes in `BHSoakHardening-*`.

### Packaged OpenWorld pause and UI safe frame - 2 August 2026

- Added the native runtime Pause action to legacy mapping contexts so packaged
  listen hosts retain Escape after gameplay mouse capture. Open/close markers
  now record routing, cursor, network mode, and input-mode restoration.
- Compacted and inset the objective card, separated the strategic situation
  panel from health/stamina, bounded its 1080p copy, and clamped legacy
  main-menu top-left text into the safe frame.
- `BHOpenWorldUI1080Final3-HUD.png` and the pause/main-menu companions provide
  rendered 1920x1080 evidence. The fresh OpenWorld Development package passed
  Host New Campaign, captured-input fire, Escape pause with cursor release, and
  second-Escape resume; `BHOpenWorldUIComplete-*` passed the complete regression
  and packaged startup smoke.
- Ambient no-nav routing remains a bounded local fallback. The latest visible
  package reached ambient-war ready without it, while the CrowdManager/Recast
  warning appeared only after viewport-close exit and is tracked as teardown
  noise unless reproduced during active play.

### GDD-09 deterministic menu focus and back navigation - 2 August 2026

- Main menu, pause, settings, and remapping now focus a deterministic visible
  control when opened, and restore focus to the opening control when a nested
  layer closes. Runtime `BH_UI_FOCUS` markers identify every transition.
- Settings consumes Escape and virtual gamepad Back. Back closes remapping
  first, then closes settings without applying, matching modal-stack behavior.
- Direct 1080p control verified arrow navigation, Enter activation, nested
  Escape/back, and restored-button activation. `BHMenuFocusComplete-*` passed
  the editor build, all 66 tests, startup, and First Light; the final visible
  remapping-focus adjustment compiled and passed
  `BHMenuFocusRemappingRendered.log` with rendered evidence in
  `BHMenuFocus-REMAPPING-1920x1080.png`.
- Physical gamepad hardware, platform glyph preference, localization, and
  couch-distance review remain open; keyboard-equivalent navigation no longer
  depends on mouse focus.

### GDD-09 device-aware input prompt preference - 2 August 2026

- Settings schema 8 persists Auto (Last Input), Keyboard + Mouse, Controller,
  and Show Both. Auto detects meaningful local gameplay input and ignores small
  controller-axis drift; explicit modes remain fixed until the player changes
  them.
- Prompt resolution uses current remapped bindings and falls back to the other
  device when the preferred binding is absent. Binding and device changes
  refresh subscribed HUD widgets without modifying the saved preference.
- `BHInputPromptMode-SETTINGS-1920x1080.png`,
  `BHInputPromptMode-GAMEPAD-HUD-1920x1080.png`, and
  `BHInputPromptMode-BOTH-HUD-1920x1080.png` provide rendered 1080p evidence.
  `BHInputPromptModeComplete-*` passed the editor build, all 66 tests, startup,
  and First Light with 8/12 bounded navigation fallbacks.
- Physical-controller live Auto switching, platform-branded glyph artwork,
  localization, and couch-distance comprehension remain manual gates.

### GDD-09 runtime localization pipeline - 2 August 2026

- Added the standard `Game` localization target for runtime source/config text
  and shipped Broken Horizon UI/mission assets. It generates an English
  manifest, archive, PO file, metadata, and loadable `Game.locres`.
- The first authoritative gather found two reused `BrokenHorizon` keys with
  different source text. The war-map owner labels now use distinct keys, and
  the final catalog reports zero conflicts instead of depending on gather
  order.
- `Validate-BrokenHorizon.cmd -Localization` regenerates the catalog, rejects
  conflict warnings and non-empty conflict reports, verifies every required
  artifact, and rejects an unexpectedly small catalog.
- `BHLocalizationComplete-*` passed the editor build, all 66 tests, a
  3,539-word zero-conflict localization gather, startup, and First Light with
  8/12 bounded navigation fallbacks. Professional translations and
  pseudo-localized rendered layout checks remain release-content gates.

### GDD-09 pseudo-localized responsive layout - 2 August 2026

- Added `-RenderedPseudoLocalization`, a ten-capture LEET matrix for Settings,
  Remapping, War Map, multiplayer-ready, and long failure states at 720p and
  1080p. The summary requires active pseudo-culture plus the normal fixture,
  renderer, image-dimension, and clean-exit proofs.
- The first inspection found three real gaps: raw settings combo options,
  expanded session failure text outside its backing, and raw/overwide strategic
  labels. Combo options now use stable keys, the session card is expansion
  safe, and core War Map state labels are gatherable.
- Strategic card text is measured against the live card width and elides at
  its own boundary, preventing translated or authored long values from drawing
  across neighboring sectors. The focused 720p proof is
  `BHPseudoLocalization-WAR_MAP-FIT-1280x720.png`.
- `BHPseudoLocalizationFix-RenderedPseudoLocalization-20260802-072841-*`
  passed all ten captures with inspected 720p/1080p evidence.
  `BHPseudoLocalizationComplete-*` then passed the editor build, all 66 tests,
  a 3,609-word zero-conflict gather, startup, and First Light at 8/12 bounded
  navigation fallbacks. Professional cultures and remaining War Map formatted
  tactical sentences are still open localization work.

### GDD-09 War Map formatted-text completion - 2 August 2026

- Expanded pseudo-localized evidence from ten to fourteen captures by adding
  deployment planning and custom-difficulty states at 720p and 1080p.
- Converted campaign, sector, force, supply, readiness, transport, loadout,
  deployment, command-footer, and custom-control presentation to stable
  localization keys with culture-aware number formatting.
- A review-only resolved-campaign bypass makes the deployment fixture exercise
  the real tactical layer without changing production operation rules.
- Direct inspection of `BHWarMapLocalizationVisualFinal-*` confirms expanded
  tactical and custom footers fit at both resolutions. The editor target built;
  `BHWarMapLocalizationComplete2-*` passed all 66 tests, a 4,072-word
  zero-conflict gather, startup, and First Light with 8/12 bounded navigation
  fallbacks. Professional translations and linguistic review remain open.

### GDD-05/GDD-11 First Light navmesh settings alignment - 2 August 2026

- Found that First Light was still serialized with the legacy 8,192 UU tile
  setting while `DefaultEngine.ini` requires 9,216 UU. UE discarded the saved
  Recast data at load (`8189` versus expected `9215`), even though the bounded
  fallback fixture let the smoke gate continue.
- Aligned the idempotent First Light navigation builder with the project-wide
  9,216 UU tile size, 12 average layers, dynamic generation, and 1,024-tile
  pool, then rebuilt only navigation in the existing map.
- Fixed `Assert-CleanLog` so task-specific failure patterns are evaluated as
  regexes; the existing navmesh-mismatch and Recast-recreation checks now
  actually fail validation instead of searching for literal `.*` text.
- `BHFirstLightNavAlignmentComplete-*` passed the editor build, all 66 tests,
  the enhanced asset audit, startup, and First Light. The final First Light and
  asset logs contain no serialized-navmesh mismatch; the navigation/grenade
  route succeeds and AI fallbacks remain bounded at 8/12. The CrowdManager
  warning remains teardown-only after successful fixture completion.

### GDD-11 warm-cache shader/PSO stutter gate - 2 August 2026

- Rendered performance summaries now include graphics/compute PSO misses,
  hitch-associated misses, loaded shader/map counts, and a process-local GPU
  budget stability ratio. Any hitch-associated miss or budget collapse below
  80% of the run maximum fails acceptance.
- Static First Light and route traversal passed with stable GPU budgets, zero
  hitches, and zero hitch-associated misses. The finalized route exposed six
  graphics and one compute PSO miss without a stall.
- `BHShaderWorldTraversalAccepted-*` passed all eight campaign-sector visits at
  9.385/12.401 ms frame p95/p99, one allowed >33 ms frame, zero >50 ms hitches,
  stable GPU budget, and zero graphics/compute PSO misses.
- Rejected GPU-budget-collapse diagnostics remain preserved and are not cited
  as passes. Cold-cache packaged pipeline-cache behavior remains a separate RC
  gate.

### GDD-11 renderer memory stability and VSM pool guard - 8 August 2026

- The extended rendered multiplayer gate exposed a real D3D12 local-budget failure on the installed 8 GB RTX 5060 Ti after 13,859 frames. The RHI dump showed a 512 MB virtual-shadow physical page pool, 630 MB of UAV textures, and a 4,147 MB local budget.
- Bounded the VSM physical page cache to 1,024 pages and set the UE 5.8 dynamic VSM page-pool load threshold to 0.75. Lumen, Nanite, ray tracing, and virtual shadows remain enabled; the renderer can reduce shadow resolution before exhausting local memory.
- `Validate-BrokenHorizon.cmd -Build -Tests -Smoke -FirstLight` passed after the configuration change. The buffered two-client rendered soak passed with ClientA/ClientB frame/GPU p95 of `11.600/8.825 ms` and `11.577/8.822 ms`.
- The extended two-hour rendered acceptance run must still be rerun with this guard; visual shadow coverage and manual speaker/display review remain open.

### GDD-11 extended rendered soak acceptance - 9 August 2026

- The VSM page-pool guard completed the long rendered acceptance run without a D3D12 local-budget failure. The dedicated First Light host and two rendered 1280x720 clients measured `779400` frames each and `7232.9/7261.6 s` per-client duration against the `7080 s` requirement.
- The final summary reports `result=Passed`, `sustainedSoakProof=true`, frame p95 `11.560/11.586 ms`, GPU p95 `8.694/8.699 ms`, zero frames over 50 ms, and `32.4%` maximum GPU-memory usage per client.
- This closes the extended two-client renderer-memory gate for the installed Win64 environment. Packaged proof, navigation coverage, manual visual/audio/controller review, and non-Win64 platform validation remain open.

## VSM release-candidate packaged acceptance - 2026-08-09

The release archive and authoritative packaged validation gate passed after applying the virtual-shadow-map physical page cap for the tested 8 GB GPU path.

- Release archive refreshed successfully at Builds/FirstLight-ReleaseCandidate-Development with UE 5.8 BuildCookRun using Win64 Development build, cook, stage, pak, IoStore, compression, and archive steps.
- Packaged gate command: Validate-BrokenHorizon.cmd -Packaged -FirstLight -AudioFX -LogPrefix BHValidation-VSMPageCap-Packaged.
- Packaged gate result: exit code 0; packaged smoke, First Light smoke, and Audio/FX readiness all passed.
- Audio/FX readiness reported required coverage 18/18 and optional coverage 9/27.
- Packaged runtime confirmed ambient wind, rain, war ambience, looping ambience, artillery, aircraft, small-arms, natural-sound attenuation, LPF, and occlusion readiness.
- The accepted navigation result remains 8/12 fallback-covered checks; the remaining four require manual map/navigation coverage work rather than another controller fallback.
- No fatal, assertion, ensure, exception, or out-of-video-memory markers were reported by the packaged gate. The expected lower-priority scalability warning for r.Shadow.Virtual.MaxPhysicalPages confirms the project setting remains authoritative at 1024.
- Packaged logs: Saved/Logs/BHValidation-VSMPageCap-Packaged-AudioFX.log, Saved/Logs/BHValidation-VSMPageCap-Packaged-FirstLight.log, and Saved/Logs/BHValidation-VSMPageCap-Packaged-Packaged.log.

This closes the current release-candidate packaged acceptance gate for the tested Win64 environment. Manual editor/playtest review, navigation coverage completion, optional audio/content coverage, and non-Win64 release qualification remain open 1.0 work.

## Rendered profile streaming validator acceptance - 2026-08-09

The optimized typed-buffer rendered profile aggregation passed a complete buffered two-client rendered multiplayer soak.

- Command: Validate-BrokenHorizon.cmd -RenderedMultiplayerSoak -LogPrefix BHValidation-RenderedProfileStreaming.
- Result: exit code 0; two clients connected and the complete capture and post-run CSV aggregation completed successfully.
- ClientA frame/GPU p95: 11.771/9.095 ms.
- ClientB frame/GPU p95: 11.489/9.108 ms.
- Authoritative summary: Saved/Reports/BHValidation-RenderedProfileStreaming-RenderedMultiplayerSoak-20260809-010152-Summary.json.
- The pass confirms the typed numeric buffers, incremental hitch/memory counters, percentile arrays, and texture-streaming tail aggregation preserve the existing rendered-soak gate behavior.

The extended two-client soak remains the long-duration acceptance gate; this buffered pass closes the parser regression check.

## Current rendered UI presentation acceptance - 2026-08-09

The current non-pseudo rendered UI matrix passed with no failed captures.

- Command: Validate-BrokenHorizon.cmd -RenderedUI -LogPrefix BHValidation-RenderedUI-Current.
- Result: exit code 0; all 33 captures passed.
- Coverage: HUD, briefing, pause, settings, input remapping, war map, custom difficulty, session ready/searching/connected/error, 1280x720, 1920x1080, 3840x2160, and HUD scale/safe-area extremes.
- Authoritative summary: Saved/Reports/BHValidation-RenderedUI-Current-RenderedUI-20260809-012148-Summary.json.
- This is automated rendered-layout evidence. Manual editor/playtest review remains required for visual quality, animation, controller feel, and navigation coverage.

## Enemy navigation invoker increment - 2026-08-09

Enemy soldiers now register bounded navigation invokers with an 8,000 cm generation radius and 12,000 cm removal radius, matching the project’s invoker-driven runtime navigation strategy without forcing full-map nav generation.

- Source gate: editor build, platform validation, automation, startup smoke, and First Light smoke passed.
- Packaged release archive rebuilt successfully with the change.
- Packaged First Light and Audio/FX acceptance passed; required audio coverage remains 18/18 and optional coverage remains 9/27.
- Navigation fallback coverage remains 8/12 in both editor and packaged smoke. The same fixed-coordinate failures persist, so the remaining gap is treated as map/navmesh coverage and manual navigation review, not as closed by this code increment.
- The invoker change is retained as runtime support for streamed/open-world AI navigation once the affected map tiles and spawn routes are repaired.



## Enemy navigation local-search recovery - 2026-08-09

Enemy AI now treats unreachable investigation and combat destinations as a local search problem: it projects the soldier onto nearby navmesh, samples a bounded reachable search point, holds at the last reachable position, and faces/scans instead of immediately issuing another path request. Initial patrol also waits for invoker navigation initialization before requesting its first move.

- Source gate: Validate-BrokenHorizon.cmd -Build -Tests -Smoke -FirstLight -LogPrefix BHValidation-LocalSearchFinal-Retry.
- Source result: exit 0; build, automation, startup, First Light grenade, and full route checks passed.
- First Light navigation fallback coverage: 5/12, down from 8/12 before the startup defer.
- Packaged gate: Validate-BrokenHorizon.cmd -Packaged -FirstLight -AudioFX -LogPrefix BHValidation-LocalSearchFinal-Packaged.
- Packaged result: exit 0; packaged smoke and First Light passed; required audio/FX coverage 18/18 and optional coverage 9/27.
- Rendered hold-only soak: Validate-BrokenHorizon.cmd -RenderedMultiplayerSoak -LogPrefix BHValidation-LocalSearchHold-Rendered.
- Rendered result: exit 0; dedicated authority, two connected/rendered clients, 19 observed AI, 570 seconds and 36,000 measured frames, renderer/network proof, zero frames over 50 ms, 100% desired texture recovery, and no stuck-movement warnings. Client A frame/GPU p95 was 11.631/8.828 ms; Client B was 11.571/8.781 ms.
- The remaining bounded fallback events, including the dense-soak 2->2 combat failures, remain open for manual map/navmesh coverage review. This increment improves recovery behavior but does not claim navigation is fully closed.
## Enemy navigation invoker rendered multiplayer acceptance - 2026-08-09

The enemy navigation-invoker build passed the full buffered two-client rendered multiplayer gate.

- Command: Validate-BrokenHorizon.cmd -RenderedMultiplayerSoak -LogPrefix BHValidation-EnemyNavInvoker-Rendered.
- Result: exit code 0; dedicated authority, two connected players, two rendered clients, and 19 observed AI all passed.
- Sustained proof: 570 seconds and 36,000 measured frames per client.
- ClientA frame/GPU p95: 11.648/9.134 ms; ClientB frame/GPU p95: 11.675/9.147 ms.
- Both clients recorded zero frames over 50 ms, GPU memory maximum 32.4 percent, desired texture data p05 100 percent, final 30-minute texture tail 100 percent, and pending stream-in p95 0.
- Authoritative summary: Saved/Reports/BHValidation-EnemyNavInvoker-Rendered-RenderedMultiplayerSoak-20260809-014045-Summary.json.

This confirms the bounded invoker does not regress the tested multiplayer rendering, memory, streaming, or network gate. It does not replace the unresolved map/navmesh coverage review; First Light navigation fallback remains 8/12.


## Enemy casualty morale falloff - 2026-08-09

Enemy casualty morale now uses one shared response for lethal deaths and friendly incapacitations. Living same-faction allies within the morale radius receive a distance-scaled suppression event: full configured pressure at the casualty location, tapering to 35 percent at the edge of the configured radius. Incapacitated allies are excluded so a downed operative does not react as an active combatant.

- Source gate: Validate-BrokenHorizon.cmd -Build -Tests -Smoke -FirstLight -LogPrefix BHValidation-CasualtyMoraleFalloff.
- Source result: exit 0; the modified enemy soldier translation unit compiled, automation passed, startup passed, and First Light passed with 5/12 bounded navigation fallbacks.
- Tactical-AI runtime gate: validate_tactical_ai.py with log BHValidation-CasualtyMoraleFalloff-TacticalAI-Final.log.
- Tactical-AI result: ALL CHECKS PASSED, including friendly incapacitation persistence and stabilization, finite ammunition, grenade safety, withdrawal, casualty pressure contracts, persistent squad orders, and rally waypoint presentation.
- Validator maintenance: the tactical-AI source contract now matches the current canonical bracketed controller prompts used by the widget and C++ automation tests; no runtime UI string was changed.
- Release gate: Win64 Development BuildCookRun archive completed successfully, followed by Validate-BrokenHorizon.cmd -Packaged -FirstLight -AudioFX -LogPrefix BHValidation-CasualtyMoraleFalloff-Packaged.
- Packaged result: exit 0; packaged smoke and First Light passed. Audio/FX readiness reports required 18/18, optional 9/27, and errors 0.
- The automated runtime recovery check exercises friendly incapacitation entry and stabilization. A manual two-ally falloff and command-conflict feel pass remains open, along with the broader casualty readability and presentation review.



## Enemy casualty contact propagation - 2026-08-09

Nearby living same-faction allies now receive a bounded contact alert when a casualty has a valid damage source. The alert uses the existing AI controller squad-alert contract, so an ally resolves the real hostile actor and enters the appropriate combat response without fabricating a target for source-less incapacitation or touching allies already dead, retreating, or evading explosives.

- Source gate: Validate-BrokenHorizon.cmd -Build -Tests -Smoke -FirstLight -LogPrefix BHValidation-CasualtyAlertPropagation, with the isolated automation retry recorded at BHValidation-CasualtyAlertPropagation-TestsRetry.
- Source result: the initial automation process had an Unreal MassEntity startup access violation before project tests executed; the isolated retry passed. Startup and First Light then passed with 5/12 bounded navigation fallbacks.
- Tactical-AI gate: validate_tactical_ai.py with BHValidation-CasualtyAlertPropagation-TacticalAI-Final.log.
- Tactical-AI result: ALL CHECKS PASSED, including the new casualty-alert source contract, friendly incapacitation recovery, suppression/casualty contracts, finite ammunition, grenade safety, withdrawal, and persistent squad orders.
- Packaged gate: the alert-enabled Win64 Development archive rebuilt successfully, and Validate-BrokenHorizon.cmd -Packaged -FirstLight -AudioFX -LogPrefix BHValidation-CasualtyAlertPropagation-Packaged passed.
- Packaged result: First Light route and all four objectives completed; audio/FX readiness required 18/18, optional 9/27, errors 0.
- Rendered soak: BHValidation-CasualtyAlertPropagation-Rendered-RenderedMultiplayerSoak-20260809-034124-Summary.json passed with dedicated authority, two connected/rendered clients, 19 observed AI, 570 seconds and 36,000 minimum measured frames, renderer/network proof, zero frames over 50 ms, desired texture recovery 100 percent, pending stream-in p95 0, and no stuck-movement warnings. Client A frame/GPU p95 was 11.558/9.05 ms; Client B was 11.628/9.06 ms. Host telemetry recorded 2,605 navigation fallbacks, 2,462 local-search recoveries, and zero stuck-movement warnings.
- The soak contained no casualty event, so direct runtime observation of BH_AI_CASUALTY_ALERT remains open. Manual two-ally casualty, command-conflict, and casualty-readability playtesting is still required.

### AI visual-contact certainty on casualty alerts - 9 August 2026

- `ABHEnemyAIController::EnterCombat` now clears inherited visual-contact state when a new target or squad casualty alert is received. An ally can react to a last-known hostile location without firing as though it has already seen that target.
- `LastKnownTargetLocation` is still updated for direct damage and squad alerts, while `LastConfirmedTargetTime` is refreshed only after an unobscured line-of-sight check. This preserves realistic search and reacquisition pacing across target switches.
- Validation evidence: `BHValidation-Build.log` passed, `BHValidation-Tests.log` passed, `BHValidation-Smoke.log` passed, and `BHValidation-FirstLight.log` passed. First Light reported navigation fallback coverage at 9/12; the remaining fixed-coordinate cases are still a map/NavMesh and manual traversal gate.
- Direct two-ally casualty execution remains a manual/PIE item because the editor commandlet fixture does not bind normal pawn `BeginPlay` death delegates. The source contract and normal runtime casualty recovery checks remain passing.
### AI visual-contact policy regression contract - 9 August 2026

- The target-handoff visibility decision is now a pure AI policy function with coverage for new unseen targets, squad casualty alerts, retained contact, and direct line of sight. This protects the distinction between a last-known contact and a visually confirmed target.
- `BHValidation-Tests.log` executed `BrokenHorizon.Gameplay.AI.VisualContactHandoff` with `Result={Success}`. The full validation also passed build/UHT, startup smoke, and First Light smoke; navigation fallback coverage remains 9/12.
### Weather-aware distant-war audio - 9 August 2026

- Distant artillery, aircraft, and small-arms events now preserve their clear-weather combat/frontline levels while applying a bounded wind/rain mask. Severe weather lowers distant readability without making the battlefield silent, preserving an audible war layer during storms.
- The policy is authority-driven through the replicated weather mix and still uses the existing spatial attenuation and occlusion contract. No audio assets were changed.
- Evidence: `BHValidation-Build.log`, `BHValidation-Tests.log`, `BHValidation-Smoke.log`, and `BHValidation-FirstLight.log` passed. `BrokenHorizon.Presentation.Audio.DistantEventWeatherMix` and `BrokenHorizon.Presentation.Audio.FirstLightAmbientAssets` both completed with `Result={Success}`. First Light navigation fallback coverage remains 9/12.
### Urgency-aware casualty waypoint rendering - 9 August 2026

- The field casualty waypoint now grades from amber to red as the stabilization window closes instead of showing every downed operative at the same urgency level. The existing direction, distance, countdown, and treatment instruction remain unchanged.
- The change is local to the HUD renderer and preserves safe-area behavior. It does not alter casualty persistence, treatment cost, or recovery timing.
- Evidence: the retry of `BHValidation-Build.log`, `BHValidation-Tests.log`, `BHValidation-Smoke.log`, and `BHValidation-FirstLight.log` passed. The rendered UI gate passed all 33 required captures in `BHValidation-RenderedUI-20260809-042834-Summary.json`. Direct casualty-state visual review remains manual because the canonical capture set does not force a live casualty event.
### Pseudo-localized HUD qualification - 9 August 2026

- The pseudo-localized rendered UI gate passed all 14 high-risk captures at 1280x720 and 1920x1080 with LEET expansion across settings, remapping, war-map, deployment, custom difficulty, and session states.
- Evidence: `BHValidation-RenderedPseudoLocalization-20260809-043709-Summary.json`. This closes the automated expanded-text layout check; translated linguistic review and interactive controller/couch-distance review remain manual.
### Packaged smoke scope after presentation increments - 9 August 2026

- The packaged validator launched the manifest Win64 executable successfully after the audio/HUD increments, and First Light smoke passed with navigation fallback coverage at 9/12. The packaged log also confirmed ambient audio readiness and runtime HUD style application.
- `Validate-BrokenHorizon.cmd -Packaged` exercises the existing packaged executable; it does not cook or archive a new package. Fresh current-source cook/archive qualification therefore remains an explicit release gate rather than being claimed by this smoke result.