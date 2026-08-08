# Save resilience evidence - 2026-08-01

## Player-visible result

Broken Horizon now validates newly written campaign checkpoints before Unreal
deserializes them. A damaged primary checkpoint is rejected by size/CRC checks,
the compatible backup is loaded, and the successfully restored world is written
back as a healthy current-schema primary.

Valid legacy Unreal `GVAS` checkpoints remain readable. After a successful
legacy load, the normal authority checkpoint capture rewrites the restored state
in the protected current format.

## Accepted multiplayer recovery run

- Summary: `G3-CorruptPrimaryRecovery2-20260801-085022-Summary.json`
- Result: `Passed`
- Damaged primary rejected before object deserialization:
  `BH_CHECKPOINT_INTEGRITY_REJECTED ... reason=crc`
- Backup selected during authority bootstrap and explicit campaign load.
- Healed checkpoint marker:
  `BH_CHECKPOINT_HEALED source=backup schema_from=45 schema_to=45 result=success`
- Occupied field transport restored with cargo, fuel, hull, driver, and two
  field-squad passengers.
- The restored attack operation completed through its production objective and
  cleared from the campaign.
- Two retained clients and one reconnecting client converged on campaign
  signature `9:1:6:None`.
- Host, ClientA, ClientB, and RejoinA each had zero fatal, assertion, unhandled,
  network/travel, checkpoint-save, and autosave failure markers.
- The timestamped primary/backup slots were removed and no Unreal process
  remained after the harness completed.

## Regression evidence

- `G3-LegacySchema41Migration-20260801-090437-Summary.json`: synthetic raw
  `GVAS` schema-41 authority checkpoint restored on a fresh host, migrated to
  protected schema 45, retained occupied transport plus two passengers,
  completed the production attack route, and converged two retained clients
  plus a reconnecting client.
- `BHLegacyMigration-FullTests.log`: 46 succeeded, 0 failed, automation exit
  code 0, no strict failure markers. This includes explicit schema 41/42
  difficulty and schema 42/43/44 progression migration boundaries.
- `BHSaveResilience-Smoke.log`: startup world launched and exited cleanly.
- `BHSaveResilience-FirstLight.log`: First Light launched and exited cleanly.
- `BHLegacyMigration-SmokeRetry.log` and
  `BHLegacyMigration-FirstLight.log`: post-migration-regression startup and
  First Light launches completed cleanly. The initial default-map smoke was
  rejected after a concurrent editor startup crashed before world creation;
  its isolated retry launched `L_MainMenu` with zero strict markers.
- `BrokenHorizonEditor Win64 Development`: direct UE 5.8 build succeeded.

## Remaining boundary

- The protected envelope applies to newly written checkpoints.
- Valid legacy `GVAS` checkpoints remain supported, but arbitrary corruption
  inside a historical unprotected payload cannot be CRC-checked retroactively.
- The automated legacy fixture is synthetic: it serializes the current save
  class in raw `GVAS` form while tagging schema 41 so version-aware restoration
  executes. Retaining real archived saves from shipped releases remains useful
  once external builds exist, but is not currently possible for this pre-release
  project.

## Ephemeral battlefield supply isolation

Enemy ammunition drops are intentionally current-level loot: they are marked
as runtime supplies before `FinishSpawningActor`, have no stable persistence
ID, and should not enter the consumed-world-item contract. Persistence
validation previously warned about every such drop during objective and
operation-result checkpoints despite that explicit runtime classification.

`ABHSupplyBase::IsRuntimeSupply` now exposes the classification read-only, and
`UBHSaveSubsystem::ValidatePersistenceIDs` excludes those actors while retaining
strict missing/duplicate-ID checks for authored supplies. The canonical
First Light gate also rejects any future `BHAmmoSupply_* has no persistence ID`
warning.

`BHEphemeralAmmoFinal-FirstLight.log` spawned three 30-round enemy drops, wrote
four successful checkpoints, completed all four production objectives, and
applied mission victory. It contained zero runtime-drop persistence warnings,
fatal errors, assertions, or unhandled exceptions. The matching editor build,
all 65 automation tests, startup smoke, and extended First Light smoke passed;
navigation fallbacks remained bounded at 11/16.

`BHBattlefieldLootFinal3-FirstLight.log` additionally proves the isolated loot
remains functional: the canonical route consumed one real runtime drop through
the production interaction interface, raised authoritative reserve ammunition
from 150 to 180 rounds, marked the drop consumed, and then completed
extraction. The matching editor build, all 66 tests, startup smoke, and First
Light smoke passed with 8/12 bounded navigation fallbacks. This closes the
previous evidence gap between runtime-drop persistence exclusion and actual
player-facing pickup use.

The dedicated two-client production-route gate then found that runtime supply
classification itself was server-only. Replicated drops reached client
`BeginPlay` with the default authored classification and produced false
missing-ID warnings. `bRuntimeSupply` now replicates alongside consumed state.
`G1-ProductionRouteLootFinal2-20260801-192318-*` passed the strict host,
two-client, and rejoin route with no runtime-drop persistence warning on any
process; `BHReplicatedRuntimeSupplyFinal-*` passed the editor build, all 66
tests, startup, and canonical First Light regression.

`G1-ConsumedLootReplicationFinal-20260801-193303-*` adds direct replicated
state evidence. Client A and Client B each observed three available drops and
the consumed transition for the selected pickup. A replacement client observed
exactly two available drops and zero consumed drops, matching Unreal relevancy
for the hidden used actor and proving it did not respawn for late join. The
summary records `clientAConsumedLootReplicated=true`,
`clientBConsumedLootReplicated=true`, `rejoinAvailableLootCount=2`, and
`rejoinConsumedLootAbsent=true`; all four process logs remained strict-clean.

`G1-OwnerAmmoReplication-20260801-193727-*` additionally proves that the
authoritative pickup result reaches the correct owner-only weapon state. Client
A's real ammo RepNotify reported magazine 30 and reserve 180, while Client B
received the consumed world-pickup transition without another pawn's private
ammo values. The same run retained exactly two available drops and no consumed
drop on the replacement client. `BHOwnerAmmoReplicationFinal-*` passed all 66
tests, startup, and canonical First Light afterward.
