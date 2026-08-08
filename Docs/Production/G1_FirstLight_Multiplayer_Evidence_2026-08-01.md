# G1 First Light multiplayer completion evidence - 1 August 2026

## Player-visible result

First Light now uses squad-shared mission progression in multiplayer. When one
player secures the red keycard, unlocks the security door, eliminates the guard,
or reaches extraction, every authoritative player pawn at that same objective
advances in canonical order. The four mission-state fields replicate to each
client, refresh the objective presentation, and present completion without
reapplying authority-only campaign results.

The red keycard is also granted to every connected player pawn so an arbitrary
authoritative checkpoint retains the shared access item. First Light is now an
explicit supported gameplay save map; intermediate and completion checkpoints
no longer fail compatibility validation.

## Two-client acceptance scenario

Command:

```powershell
Scripts/Test-BrokenHorizonFirstLightMultiplayer.ps1 `
  -TimeoutSeconds 180 `
  -LogPrefix G1-FirstLightSharedReconnect
```

Accepted evidence:

- `Saved/Logs/G1-FirstLightSharedReconnect-20260801-083153-Summary.json`
- matching `Host`, `ClientA`, and `ClientB` logs

The dedicated host completed `FindRedKeycard`, `UnlockSecurityDoor`,
`EliminateGuard`, and `ReachExtraction` in order for two authoritative player
pawns. It logged `result=success players=2 completed=2`. Both clients logged a
replicated state with `completed=4 complete=1 failed=0`.

The harness then disconnected Client A and connected a fresh replacement. Its
new authoritative pawn adopted all four completed objectives and shared
inventory from retained Client B, and RejoinA received the same completed state.
The summary records `rejoinCompletionInherited=true`.

The host completed eight isolated checkpoint writes (three intermediate plus
completion for each authoritative pawn). Host and client strict failure counts
were zero. The runner stopped only its owned processes and removed its isolated
primary/backup saves.

The direct `BrokenHorizonEditor Win64 Development` build succeeded. All 45
automation tests passed in `BHFirstLightReconnect-Tests.log`, including the expanded
ordered-objective replication contract. Main-menu and First Light smokes passed
in `BHFirstLightReconnect-Smoke.log` and
`BHFirstLightReconnect-FirstLight.log` with no
fatal, assertion, Blueprint, travel, network, or PIE error markers.

## Remaining manual review

Automation proves authority, canonical ordering, checkpoint compatibility,
client replication, and completion presentation state. A live networked editor
playtest is still required for physical interaction timing, guard combat,
navigation coverage, objective discoverability, UMG layout, audio/animation,
and extraction feel.
# Post-accessibility regression run

`BHPostAccessibility-FirstLightMP-20260801-130833-Summary.json` passed after the
schema-7 accessibility, global safe-frame, native subtitle, and objective-radio
changes. A dedicated host completed the four canonical objectives; completion
replicated to both clients and was inherited by a rejoining client. The four
process logs contain no fatal/assert/unhandled, network/travel, autosave, or
stale First Light ambient-audio package markers.

## Production actor route and battlefield loot regression

The two-client acceptance harness no longer relies on its synthetic
`CompleteSharedObjective` loop. It now delays the same production route fixture
used by the canonical First Light smoke until both clients are connected. On
dedicated authority that fixture interacts with the authored red keycard and
locked security door, kills the real guard group through
`UBHHealthComponent`, consumes an enemy runtime ammunition drop through
`IBHInteractable`, and enters the authored extraction zone. Completion is
accepted only when every authoritative player has all four objectives.

The first strict rerun exposed that `bRuntimeSupply` was not replicated: client
`BeginPlay` therefore misclassified replicated enemy drops as authored supplies
and emitted missing-persistence-ID warnings. Runtime classification now
replicates with the consumed state, preserving the same server/client semantic
contract instead of filtering the warning.

`G1-ProductionRouteLootFinal2-20260801-192318-Summary.json` passed with
`productionActorRoute=true`, `battlefieldLootConsumed=true`, two connected
players, replicated completion on Client A and Client B, and completed-state
inheritance on Rejoin A. The host recorded the canonical keycard, door, guard,
ammo-drop, and completion markers; the drop raised reserve ammunition from 150
to 180 rounds and the final marker reported `players=2 completed=2`. The four
process logs contained no fatal, assertion, unhandled, network, travel,
checkpoint, or autosave failure markers.

The matching editor target compiled, and
`BHReplicatedRuntimeSupplyFinal-*` passed all 66 automation tests, startup
smoke, and the standalone canonical First Light route with 8/12 bounded
navigation fallbacks. Physical two-player interaction timing, combat tactics,
pickup presentation, navigation feel, and extraction presentation remain
manual review gates.

The supply synchronization gate was then extended beyond warning absence.
`G1-ConsumedLootReplicationFinal-20260801-193303-Summary.json` proves Client A
and Client B each received all three available enemy drops and then the
consumed transition for the used drop. Because hidden consumed supplies are
intentionally no longer relevant to a new connection, the replacement client
correctly received exactly two available drops and zero consumed drops; the
used pickup did not reappear. The summary records both live-client consumed
transitions, `rejoinAvailableLootCount=2`, and
`rejoinConsumedLootAbsent=true`. All host/client logs remained free of route,
network, travel, persistence-ID, fatal, assertion, and unhandled markers.
`BHConsumedLootReplicationFinal-*` passed all 66 tests, startup, and canonical
First Light after the final evidence instrumentation.

The last player-visible gap was the owner-only weapon state. The server log
proved the selected pawn received 30 rounds, but that alone did not prove the
owning client's ammo delegate and HUD input observed the result.
`G1-OwnerAmmoReplication-20260801-193727-Summary.json` now records
`ammoOwnerClient=ClientA` and `ownerAmmoReserveReplicated=180`. Client A emitted
`BH_BATTLEFIELD_LOOT_AMMO_REPLICATED` from the real `OnRep_Ammo` path with
magazine 30 and reserve 180; Client B correctly received the world pickup's
consumed transition without receiving another pawn's owner-only ammo state.
The run retained both-client consumption, two-drop late-join availability,
mission completion, and rejoin inheritance gates. The editor target compiled
and `BHOwnerAmmoReplicationFinal-*` passed all 66 tests, startup, and canonical
First Light with 8/12 navigation fallbacks.

`G1-AmmoHUDReplication-20260801-194127-Summary.json` extends the same proof to
the bound gameplay widget. On Client A, `UBHAmmoHUDWidget::SetAmmo` updated the
real `AmmoText` binding to `30 / 180` before emitting
`BH_BATTLEFIELD_LOOT_HUD_UPDATED`; the summary records
`ownerAmmoHUDText="30 / 180"`. Client B still received only the shared
consumed-pickup transition. All production-route, two-client, reconnect, and
late-join loot-set assertions remained true. `BHAmmoHUDReplicationFinal-*`
passed all 66 tests, startup, and canonical First Light with 8/12 fallbacks.

Rendered proof followed in `BHRenderedLootHUD-20260801-194607-*`. Two D3D12
clients remained connected to dedicated authority while the production route
completed for both players. Client B was the rewarded owner and produced a
1280x720 screenshot of the bound `30 / 180` HUD state; the counter remained
inside the lower-right safe area without clipping or overlap. Both clients also
passed the bounded frame/GPU, hitch, draw-call, GPU-memory, and texture-streaming
budgets. `BHRenderedLootHUDRegression-*` passed all 66 tests, startup, and
canonical First Light afterward. Normal-paced two-player interaction and
moving-combat feel remain manual review.
