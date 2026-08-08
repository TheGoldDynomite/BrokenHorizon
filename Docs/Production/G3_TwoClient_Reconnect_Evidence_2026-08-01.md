# G3 two-client reconnect evidence - 1 August 2026

## Scope

Bounded dedicated-server validation of the First Light map with two concurrent
clients. Client A disconnects while Client B remains connected, then Client A
rejoins the same authoritative server.

## Command

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass `
  -File .\Scripts\Test-BrokenHorizonMultiplayer.ps1 `
  -LogPrefix G3-TwoClientReconnect
```

## Result

- Passed on port `7822`.
- Dedicated host reached `BH_WAR_GAME_STATE_READY`.
- Both initial clients were welcomed by the First Light server.
- Both clients applied the authoritative war snapshot.
- Client A rejoined and applied the same snapshot while Client B remained.
- Snapshot signature was `1:0:6:None` for both clients and the rejoin.
- No fatal error, assertion, unhandled exception, network failure, connection
  timeout, or snapshot-apply failure marker was present.

## Evidence

- `Saved/Logs/G3-TwoClientReconnect-20260801-012025-Summary.json`
- `Saved/Logs/G3-TwoClientReconnect-20260801-012025-Host.log`
- `Saved/Logs/G3-TwoClientReconnect-20260801-012025-ClientA.log`
- `Saved/Logs/G3-TwoClientReconnect-20260801-012025-ClientB.log`
- `Saved/Logs/G3-TwoClientReconnect-20260801-012025-RejoinA.log`

## Remaining G3 proof

This run proves two-client join and client reconnect continuity on a stable
First Light server. The seamless-travel follow-up below now proves server travel
with an active committed operation, and the transport follow-up proves occupied
vehicle persistence through the production checkpoint-load travel path. Host
crash/recovery, operation objective completion, latency/loss, and the two-hour
soak remain open.

## Active-operation follow-up

A second run used the development-only active-operation hook:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass `
  -File .\Scripts\Test-BrokenHorizonMultiplayer.ps1 `
  -LogPrefix G3-ActiveOperationReconnect `
  -RequireActiveOperation
```

The host committed operation
`Operation_3547F7FD427D9434D6A7E79153AD6AEA`. Client A, Client B, and the
rejoining Client A all applied snapshot signature
`2:0:6:Operation_3547F7FD427D9434D6A7E79153AD6AEA`. No failure marker was
detected. Evidence is in
`Saved/Logs/G3-ActiveOperationReconnect-20260801-012301-*`.

## Seamless server-travel follow-up

The multiplayer harness now supports `-RequireServerTravel`. Its first run
correctly exposed that non-seamless travel closed both clients. `ABHGameMode`
now enables seamless travel, and the deterministic rerun used:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass `
  -File .\Scripts\Test-BrokenHorizonMultiplayer.ps1 `
  -LogPrefix G3-SeamlessTravelVerified `
  -RequireActiveOperation `
  -RequireServerTravel `
  -ConnectionTimeoutSeconds 60
```

The host and both clients loaded First Light before and after seamless travel.
Both connected clients and the rejoining client converged on signature
`1:2:6:Operation_53C37137480161D5223E4A9B88464B44`. The passed summary and
four process logs are under
`Saved/Logs/G3-SeamlessTravelVerified-20260801-034127-*`.

## Occupied transport checkpoint-travel follow-up

The harness now also supports `-RequireTransportPersistence`, which selects the
open-world campaign map, creates an isolated per-run checkpoint, deploys the
authoritative player into a real active operation, boards a persistent field
transport with two living operatives, and invokes the production `SaveProgress`
plus `LoadProgress` server-travel path:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass `
  -File .\Scripts\Test-BrokenHorizonMultiplayer.ps1 `
  -LogPrefix G3-TransportTravelVerified2 `
  -RequireActiveOperation `
  -RequireServerTravel `
  -RequireTransportPersistence `
  -ConnectionTimeoutSeconds 120
```

The verified run on port `8439` retained both clients through seamless travel
and preserved operation
`Operation_E44C6BF640FDE6FCD870DCA197262E70`. The destination restored the
driver, cargo `9.0`, fuel `0.63`, hull `0.71`, and both embarked field-squad
passengers on transport `WesternFOBFieldTransport01`. Both seamless-travel
controllers recovered valid character pawns, Client A disconnected, and its
replacement converged with retained Client B. The passed summary is
`Saved/Logs/G3-TransportTravelVerified2-20260801-043741-Summary.json`; matching
Host, ClientA, ClientB, and RejoinA logs share that prefix. The isolated save and
backup were removed by the harness, leaving normal player checkpoint slots
untouched.

This work also corrected dedicated-server player resolution during post-travel
checkpoint application, bounded the player-ready retry, handed transport
drivers back to their controllers before travel, and restarted controllers that
arrived pawnless after seamless travel. The fixture waits for both editor-backed
clients and compares snapshots at actual convergence points so ambient war turns
cannot create false divergence.

`BrokenHorizonEditor Win64 Development` compiled successfully. All 45
`BrokenHorizon` automation tests passed in `BHTransportTravel-Tests.log`, and
main-menu plus First Light smokes passed in `BHTransportTravel-Smoke.log` and
`BHTransportTravel-FirstLight.log`. No fatal, assertion, unhandled exception,
automation failure, package-load failure, or travel/network failure marker was
present in the accepted evidence.

## Active objective completion after travel

A follow-up run added `-RequireOperationCompletion` to the occupied-transport
scenario. The authoritative player deployed into attack operation
`Operation_8295D0AF40CE5FA43EE661AA1F9331E3`; its approach-phase director state,
mission objective, vehicle, and squad were checkpointed and restored after
seamless travel. The restored director then completed through the production
mission-objective path, saved the result checkpoint, and released the strategic
operation lock.

The retained clients and rejoin converged on snapshot `9:1:6:None`, proving the
completed operation did not reappear for a reconnecting player. Evidence is in
`Saved/Logs/G3-OperationCompletionTravel-20260801-044653-Summary.json` and its
Host, ClientA, ClientB, and RejoinA logs. The summary records all transport and
completion requirements as true, and its isolated checkpoint files were removed
after the run.

After this extension compiled successfully, all 45 automation tests passed in
`BHOperationTravel-Tests.log`; main-menu and First Light smokes passed in
`BHOperationTravel-Smoke.log` and `BHOperationTravel-FirstLight.log`.

## Dedicated host-process crash recovery

The harness now supports `-RequireHostRecovery`. Phase one deploys a live
operation, boards the persistent transport and squad, writes an isolated
checkpoint, and then terminates only the processes owned by the test. Phase two
starts a new dedicated-server process on the same port and save suffix, connects
two new clients, restores the checkpoint through server travel, then performs a
further disconnect/rejoin convergence check.

The first recovery attempt exposed a real save-loss race: normal war and field
autosaves on the replacement host could overwrite the crash checkpoint while
the server waited for clients. Recovery startup now defers those autosaves until
`LoadProgress` begins; normal autosaving resumes afterward.

The verified run recovered operation
`Operation_8D36241B480609501A406A81B7668D3F`, transport
`WesternFOBFieldTransport01`, cargo `9.0`, fuel `0.63`, hull `0.71`, the driver,
and two embarked squad members in a fresh host process. Retained and rejoining
clients converged on `7:0:6:Operation_8D36241B480609501A406A81B7668D3F`.
Evidence is `Saved/Logs/G3-HostCrashRecovery2-20260801-050238-Summary.json` plus
its CrashedHost, CrashedClientA, CrashedClientB, Host, ClientA, ClientB, and
RejoinA logs. The isolated primary and backup saves were removed after the run.

The final editor target compiled successfully. All 45 automation tests passed
in `BHHostRecovery-Tests.log`; main-menu and First Light smokes passed in
`BHHostRecovery-Smoke.log` and `BHHostRecovery-FirstLight.log`.

## Latency and packet-loss travel/reconnect

The harness now accepts bounded UE5.8 packet-simulation settings. A verified
First Light run applied `PktLag=80` and `PktLoss=3` to the dedicated host, both
retained clients, and the rejoin client. Both initial clients connected before
travel, survived seamless travel, and applied the same active-operation state;
the replacement Client A then rejoined under the same impairment.

All three client lifecycles converged on
`1:2:6:Operation_6343CAB74C3E2F36E49160B5383A1839`. Engine logs explicitly
record both packet settings for every process. Evidence is
`Saved/Logs/G3-NetImpairment2-20260801-051559-Summary.json` plus its Host,
ClientA, ClientB, and RejoinA logs. No fatal, assertion, unhandled exception,
travel failure, network failure, or connection-timeout marker appeared.

## Multiplayer soak harness shakedown

The same harness now accepts `-SoakSeconds` up to `7200`. After seamless travel
it continuously checks that the dedicated host and both retained clients remain
alive and scans all three logs for fatal, assertion, unhandled exception,
travel-failure, and network-failure markers before allowing the disconnect and
rejoin phase.

A 180-second shakedown passed in
`Saved/Logs/G3-SoakShakedown2-20260801-052328-Summary.json` and its four process
logs. This proves the monitor and cleanup path, not the required two-hour
duration. The 7,200-second acceptance run remains open until a complete summary
is produced.

The first full-duration attempt was invalidated early by a deeper audit. It
exposed checkpoint autosave failures on the shared default slot and six client
widget errors from a seamless-travel controller that had no attached
`ULocalPlayer`. Soak runs now use an isolated per-run save suffix and treat
autosave failures plus `PIE: Error` as hard failures. Character UI creation now
requires `GetLocalPlayer()` rather than the weaker local-controller predicates,
and editor clients launch with a short stagger to avoid handshake starvation.

`G3-TravelUIFix2-20260801-053906-Summary.json` proves the hardened short run:
travel, 30-second monitoring, and reconnect passed with no UI, save, fatal,
assertion, travel, or network failure markers; the isolated save was removed.
All 45 automation tests and both smokes passed afterward in the
`BHSoakHardening-*` logs.

## Full two-hour campaign soak

The hardened acceptance run completed all 7,200 monitored seconds on the open
world map after seamless travel. The dedicated host and both retained clients
remained alive while field checkpoints continued and the campaign advanced to
turn 40. Client A, Client B, and the fresh rejoin client converged on
`42:40:6:Operation_27204FAE4E104A5C4A28C0B23617001C`.

Evidence is
`Saved/Logs/G3-TwoHourCampaignSoak-20260801-055216-Summary.json` plus its Host,
ClientA, ClientB, and RejoinA logs. The summary records `result=Passed`,
`soakSeconds=7200`, `soakCompleted=true`, server travel, all three client
snapshot applications, and final signature convergence. No fatal, assertion,
unhandled exception, network/travel failure, `PIE: Error`, checkpoint failure,
or war/field autosave failure marker appeared. The runner stopped all owned
Unreal processes and removed its isolated primary/backup checkpoint files.

## Attack, defense, and raid route completion

The multiplayer harness now accepts `-OperationType Attack`, `Defend`, or
`Raid` and verifies the authoritative selected type and viable sector. Each
route uses the real player deployment path, checkpoints an occupied transport
and two-person field squad, restores them through server travel, completes the
restored production operation director/objective/debrief path, then requires a
fresh client to converge with the retained client after the completed operation
has cleared.

- Attack: `G3-AttackRoute3-20260801-080404-Summary.json`, `NorthPass`, type 1,
  final snapshot `9:1:6:None`.
- Defense: `G3-DefendRoute-20260801-080702-Summary.json`, `DovrenVillage`, type
  2, final snapshot `9:1:6:None`.
- Raid: `G3-RaidRoute-20260801-081000-Summary.json`, `KoronaCrossroads`, type 3,
  final snapshot `10:1:6:None`.

All three summaries passed with type verification, travel, transport restore,
operation completion, and reconnect convergence true. Every matching Host,
ClientA, ClientB, and RejoinA log had zero strict failure markers, and each
isolated checkpoint was removed. `BrokenHorizonEditor Win64 Development` built
successfully. All 45 automation tests passed in `BHCampaignRoutes-Tests.log`;
main-menu and First Light smokes passed in `BHCampaignRoutes-Smoke.log` and
`BHCampaignRoutes-FirstLight.log`.
