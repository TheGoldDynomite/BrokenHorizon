# First Light live gameplay evidence - 2026-08-01

## Scope

A visible native First Light session was exercised through the production input
mapping at 1280x720. This was an observed control and runtime-health review, not
a substitute for a human feel/balance signoff or complete objective-route run.

## Observed controls and feedback

- Prone entered and exited with the expected camera-height transition.
- Rifle fire produced recoil/impact feedback and changed ammunition from
  `30/90` to `29/90`.
- Reload transferred one reserve round and restored `30/89`.
- Fire-mode switching displayed `FIRE MODE // SEMI`.
- A frag grenade spawned visibly, decremented the HUD from 2 to 1, and produced
  `BH_FRAG_THROWN` plus `BH_FRAG_EXPLODED` runtime markers.
- The strategic map opened and closed with six sector cards, current priority,
  campaign log, loadout, route state, and keyboard guidance visible.
- Squad follow/hold without a recruited operative returned explicit recovery
  guidance instead of silently failing.
- Squad Context without a supported target returned explicit valid-target
  guidance.
- Pause and settings were previously opened through the same visible First
  Light process; the responsive settings hierarchy and resume path rendered.

Hold-duration movement, sprint, crouch, lean, ADS, physical middle-mouse ping,
continuous route traversal, and live enemy combat feel were not proven by this
input-injection pass and remain manual gameplay gates. The production
interaction and objective-completion path is now covered separately by the
canonical runtime route fixture described below.

## Navigation defect and correction

`BHManualFirstLightControlsReview.log` exposed repeated identical
`BH_AI_NAVIGATION_FALLBACK` warnings and a per-frame Recast failure burst while
a grenade was moving. The map uses 8192 cm dynamic tiles but only the default
three-layer allocation; the grenade's moving physics collision also remained
navigation relevant.

The runtime correction:

- marks both grenade visual/collision primitives as unable to affect navigation;
- keeps the scalable 1024-tile non-fixed pool explicit in project config; and
- raises `AverageLayersPerTile` to 12 for the vertically layered graybox.

In `BHManualFirstLightNavigationRetest.log`, tile-limit warnings fell to zero
before, during, and after a live grenade throw/explosion. Only two bounded
startup fallbacks and two bounded grenade-response fallbacks occurred; none
repeated. Enemy positions changed from their initial locations (for example,
soldier C_2 moved from `6650,450` to approximately `6403,163`), providing direct
runtime evidence that patrol movement recovered. No fatal, assertion, or
unhandled-exception marker occurred.

## Regression validation

`Validate-BrokenHorizon.cmd -Build -Tests -Smoke -FirstLight` passed after the
correction:

- editor target compiled;
- the automation queue passed;
- startup smoke passed; and
- First Light smoke passed.

Final navigation coverage, patrol quality, grenade-evade feel, and the complete
mission route still require hands-on play by a human operator.

## Permanent First Light gameplay and navigation gate

The canonical `-FirstLight` smoke no longer exits immediately after map load.
In non-shipping builds it now runs two production-map fixtures. The first
spawns and throws a real replicated physics grenade and waits through
detonation. The second drives the real `BHKeycard` interaction, locked `BHDoor`
interaction, authoritative `BHHealthComponent` death path for the authored
guard group (or strategic defense waves), and `BHExtractionZone` overlap. It
requires each live shared objective to advance in canonical order and verifies
four completed objectives plus mission victory. It does not call
`CompleteSharedObjective` to bypass those actors.

The wrapper requires successful keycard, door, combat, extraction, grenade,
and explosion markers; rejects `tile limit reached` and either fixture's
failure marker; and caps navigation fallbacks at 16 for the extended combat
window. `BHPlayableRouteFinal2-FirstLight.log` passed the four production
interaction stages, applied mission victory to the war sector, produced one
grenade explosion, reported zero Recast tile-limit warnings, and remained
bounded at 11/16 navigation fallbacks. Matching `BHPlayableRouteFinal2-Build`,
`-Tests`, and `-Smoke` logs passed, including all 65 automation tests. This is
repeatable runtime proof of the objective mechanics and campaign handoff;
continuous player-controlled traversal, aiming, combat tactics, animation
feel, and multi-client operation feel remain manual gameplay review.

## Serialized navigation alignment

The extended route later exposed a load-time Recast warning: the First Light
map still serialized the earlier six-tile capacity while project runtime
settings required 24 addressable tiles. Runtime reconstruction kept the route
playable, but added avoidable load work and meant the map/config navigation
contract had drifted.

The documented, narrowly scoped
`Content/Python/repair_first_light_navigation.py` editor automation rebuilt and
saved only `L_FirstLight_Graybox`; its SHA-256 changed from
`D3F14B51E92F5F4DF28EE6DB8E502EC6080EF5E035CF719A465F1459B6419BF2` to
`2D9F5DF8787C3168A56F92A213837AB0ED947A87537971FED053465CCF1A1B45`.
The read-only validator then retained one player start, keycard, door,
extraction and navigation volume, three guards, the stable door ID, and all
three `EliminateGuard` assignments.

`BHSerializedNavFinal-FirstLight.log` passed the grenade and complete
keycard/door/combat/extraction/war-victory route with zero serialized-capacity
mismatch, empty-navmesh, tile-limit, fatal, assertion, or unhandled markers.
The canonical wrapper now rejects both stale-nav serialization warning forms.
Matching build, all 65 tests, and startup smoke passed; the extended encounter
reported 13/16 bounded AI fallbacks. Hands-on navigation coverage and combat
path quality remain manual review.

## Lethal-hit AI transition ordering

Route logs showed several same-frame navigation fallbacks as the objective
guards died. `UBHHealthComponent` broadcasts damage after reducing health to
zero but before its death broadcast; the enemy damage listener therefore sent
the controller a normal live-damage reaction immediately before death cleanup.
That reaction cancelled movement and caused stale combat/search fallbacks.

`ABHEnemySoldier` now preserves lethal hit presentation but notifies its AI
controller only when positive health remains and the soldier is not already
dead. `BrokenHorizon.Gameplay.AI.LethalDamageReaction` covers wounded,
zero-health, and already-dead decisions. `BHLethalAIReactionFinal-Tests.log`
passed the expanded 66-test queue, and the matching build, startup, and full
First Light route passed. Navigation fallbacks dropped from 13 to 8; the
remaining combat-time transitions were emitted by living squadmates reacting
to allied casualties. `BHLethalAIReactionTightGate-FirstLight.log` passed again
at 8/12 after tightening the canonical ceiling. Subjective hit reaction,
casualty morale, and movement feel remain manual gameplay review.

## Battlefield loot interaction gate

The permanent route now verifies the supply loop between combat and
extraction, rather than proving only that enemy drops spawned. After the real
guard/defense-wave death path creates runtime ammunition, the fixture locates
an unconsumed `ABHAmmoSupply` marked as runtime loot, prepares an authoritative
30-round reserve deficit, and invokes its production `IBHInteractable` path.
It requires the supply to become consumed and the weapon reserve to increase
by the pickup amount before allowing extraction.

`BHBattlefieldLootFinal3-FirstLight.log` recorded
`step=ammo_drop result=success before=150 after=180 rounds=30`, completed all
four objectives and mission victory, and remained at 8/12 navigation
fallbacks. The matching editor target compiled; all 66 automation tests,
startup smoke, and the complete First Light route passed. The wrapper now
requires this marker, so an unusable, zero-value, non-consuming, or
non-authoritative battlefield drop fails the canonical gameplay gate. Manual
pickup placement, readability, sound/VFX, and combat-pressure feel remain
hands-on review items.
