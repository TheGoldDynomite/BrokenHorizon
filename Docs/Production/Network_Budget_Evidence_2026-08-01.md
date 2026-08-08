# Two-client network budget evidence - 1 August 2026

## Result

Broken Horizon now has a canonical `-NetworkBudget` gate. A dedicated First
Light server measures UE5.8 net-driver and per-connection counters while two
independent clients connect, receive the authoritative war snapshot, converge,
disconnect/rejoin, and converge again.

Final command:

`Validate-BrokenHorizon.cmd -Build -Tests -Smoke -FirstLight -NetworkBudget -LogPrefix BHNetworkBudget-Full`

Final measured window:

- Result: passed.
- Samples: 10 one-second samples with two connected clients.
- Aggregate server output p95: 10,082 bytes/second (budget: 65,536).
- Aggregate server output maximum: 10,082 bytes/second.
- Per-connection output maximum: 6,847 bytes/second (budget: 32,768).
- Per-connection open-channel maximum: 21 (budget: 64).
- Aggregate outgoing packet-rate maximum: 60 packets/second.
- Observed packet loss: 0 (budget: 0 for the clean baseline).
- Net-driver RPC total at the end of the measured window: 206.
- Both retained and rejoining clients converged on snapshot signature
  `1:2:6:Operation_7BE6835C42874D9DD03493AA988BF3C1`.
- Evidence: `Saved/Logs/BHNetworkBudget-Full-NetworkBudget-20260801-140711-Summary.json`.
- Host/client logs use the same dated prefix beside the summary.

The complete 65-test automation queue, editor build, startup smoke, First Light
smoke, and the canonical network gate passed in the same validation run.

## Implementation contract

`-BHTestNetworkBudgetTelemetry` enables non-shipping, server-authoritative
telemetry in `ABHWarGameState`. It turns on UE net-stat collection and records
actual driver and connection byte rates, packet rates, channel pressure,
observed loss, and RPC totals. Samples are accepted only while at least two
valid client connections exist. The harness requires ten samples and fails any
budget check before it can report success.

The initial two-client vertical-slice budgets are serialized with their checks
in the JSON summary:

- aggregate server output p95 <= 65,536 bytes/second,
- per-connection output maximum <= 32,768 bytes/second,
- per-connection channels <= 64,
- no packet loss in the clean localhost baseline.

## Evidence boundary

This proves a dedicated-server/two-client NullRHI localhost baseline and
replication convergence. It does not prove rendered-client cost, a production
dedicated-server build, geographically representative Internet conditions,
voice bandwidth, maximum shipping player count, or worst-case combat/entity
density. The summary explicitly records `rendererProof=false` and
`representativeInternetProof=false`.

Packet-lag/loss recovery and the two-hour multiplayer soak remain separately
verified harness scenarios. The next section extends these counters to the
current four-player hosted-session default. Representative combat density and
Internet load remain required before G4 performance budgets can be final.

## Current four-player session-capacity scale

The hosted-session code defaults to four public connections, so a distinct
canonical `-NetworkScale` gate now exercises one dedicated server and four
clients without redefining an unsupported larger target. The final scale run
also committed an active campaign operation and replicated a shared squad ping;
all four clients received the war snapshot, operation state, and command state,
and the disconnected client converged again after rejoin.

Final command:

`Validate-BrokenHorizon.cmd -Build -Tests -Smoke -FirstLight -NetworkScale -LogPrefix BHNetworkScale-Final`

Initial four-client measurements before the combat-density extension:

- Aggregate server output p95 and maximum: 15,792 bytes/second.
- Per-connection output maximum: 7,484 bytes/second.
- Per-connection open-channel maximum: 25.
- Aggregate outgoing packet-rate maximum: 120 packets/second.
- Observed packet loss: 0.
- Net-driver RPC total at the end of the window: 624.
- All byte/channel/loss budgets passed.
- Evidence: `Saved/Logs/BHNetworkScale-Final-NetworkScale-20260801-141309-Summary.json`.
- The editor build, 65-test queue, startup smoke, and First Light smoke passed
  in the same final run.

This proves the current default four-player session capacity under First Light
AI/world activity plus active operation and squad-command replication. It does
not prove voice traffic, WAN behavior, or a future session capacity different
from the current four-player default.

## Bounded combat-density scale

The canonical four-client scale gate now adds a stable, test-only overlapping
combat population derived from production system caps: 12 hostile AI and four
friendly AI alongside four connected players. The fixture actors use native
`ABHEnemySoldier` movement, targeting, firing, faction, and replication. They
are transient, invulnerable, always relevant, and lifespan-bounded so combat
traffic remains active without deaths reducing the late-join population.

The First Light map also retained its three existing hostile soldiers, so each
initial and rejoining client independently observed 15 hostiles plus four
friendlies. The gate rejects server-only evidence: all four clients and the
rejoining client must emit their own successful replicated-density marker.

Final command:

`Validate-BrokenHorizon.cmd -Build -Tests -Smoke -FirstLight -NetworkScale -LogPrefix BHCombatDensity-Final3`

Final combat-density measurements:

- Four connected clients and 16 fixture AI, plus three existing map AI.
- Aggregate server output p95 and maximum: 49,836 bytes/second (budget: 65,536).
- Per-connection output maximum: 12,584 bytes/second (budget: 32,768).
- Per-connection open-channel maximum: 42 (budget: 64).
- Aggregate outgoing packet-rate maximum: 120 packets/second.
- Observed packet loss: 0.
- Net-driver RPC total at the end of the measured window: 920.
- Active operation, shared squad command, all client population checks, and
  reconnect convergence passed.
- Evidence: `Saved/Logs/BHCombatDensity-Final3-NetworkScale-20260801-142942-Summary.json`.
- The editor build, 65-test queue, startup smoke, and First Light smoke passed
  in the same final run.

An earlier fixed-delay client check correctly failed when the first clients
evaluated before the fourth client triggered server fixture creation. The
final verifier polls for bounded replication convergence and fails after 30
attempts; it does not accept a reduced population. Multiplayer teardown also
confirms owned process exit and observes a short barrier before subsequent UE
smokes, preventing shared-service teardown/relaunch races.

This is a strong bounded localhost stress baseline, but it remains NullRHI and
does not include voice data or WAN latency/loss/jitter. Those conditions and a
rendered-client run remain separate release gates.

## Simulated WAN impairment budget

The canonical `-NetworkImpairment` gate applies the previously established
Broken Horizon recovery profile—80 ms packet lag and 3% packet loss—to the
dedicated server, all four combat-density clients, and the rejoining client.
Unlike the clean gate, it permits a bounded loss burst instead of requiring
zero observed loss. Every other byte, channel, population, operation, squad
command, and reconnect assertion remains active.

Final command:

`Validate-BrokenHorizon.cmd -Build -Tests -Smoke -FirstLight -NetworkImpairment -LogPrefix BHWANBudget-Final`

Final impaired measurements:

- Simulation profile: 80 ms lag and 3% loss on all six process lifecycles.
- Four clients, 16 fixture AI, and three existing map AI.
- Aggregate server output p95 and maximum: 50,856 bytes/second (budget: 65,536).
- Per-connection output maximum: 12,710 bytes/second (budget: 32,768).
- Per-connection open-channel maximum: 42 (budget: 64).
- Aggregate outgoing packet-rate maximum: 120 packets/second.
- Worst observed one-second loss count: 12 (burst budget: 20).
- Net-driver RPC total at the end of the measured window: 898.
- All clients and the rejoin client received the full density, operation, and
  squad-command state and converged on snapshot
  `2:3:6:Operation_F2D354F042A953C3094C169BA715AD57`.
- Evidence: `Saved/Logs/BHWANBudget-Final-NetworkImpairment-20260801-143444-Summary.json`.
- The editor build, 65-test queue, startup smoke, and First Light smoke passed
  in the same final run.

This is repeatable UE packet simulation, not a geographically distributed WAN
test. It validates the game's bounded impairment behavior and retransmission
pressure but not ISP routing, jitter distributions, NAT variation, or platform
relay services.

No player voice-chat provider or voice interface is present in the current
project contract: the project uses `OnlineSubsystemNull`, and the only voice
content implemented is AI presentation audio. Voice bandwidth is therefore
excluded from these measurements. Adding player voice would be a product and
online-service scope decision requiring its own provider, privacy/moderation
requirements, UI, accessibility, and bandwidth gates.

## Two-client rendered combat-density baseline

The canonical `-RenderedMultiplayer` gate launches a NullRHI dedicated server
and two simultaneous 1280x720 offscreen D3D12 clients. The server adds 12
hostile and four friendly fixture AI to First Light, commits an active
operation and shared hostile ping, and records a 10-sample network window.
Both rendered clients must receive the war snapshot, ping, and complete
19-AI replicated population before their 1,800-frame captures are accepted.

Final command:

`Validate-BrokenHorizon.cmd -Build -Tests -Smoke -FirstLight -RenderedMultiplayer -LogPrefix BHRenderedMP-Final`

After 600 warmup frames, each client contributed 1,200 measured frames on an
RTX 5060 Ti with driver 610.88:

- Client A frame/GPU/render-thread p95: 12.137/9.127/12.151 ms; maximum frame
  20.618 ms; zero frames above 50 ms.
- Client B frame/GPU/render-thread p95: 12.007/9.201/11.999 ms; maximum frame
  14.964 ms; zero frames above 50 ms.
- Draw-call p95: 170 and 175.
- Per-process reported local GPU-budget usage: 79.7% and 79.8%, within the 80%
  simultaneous-client gate.
- Both clients held 100% desired texture data at p05 and final recovery, with
  zero pending stream-in data at p95.
- Evidence: `Saved/Reports/BHRenderedMP-Final-RenderedMultiplayer-20260801-151338-Summary.json`.
- The editor build, 65-test queue, startup smoke, and First Light smoke passed
  in the same final run.

This closes the bounded local two-client concurrent-renderer path. The later
shared-ping visual gate adds synchronized client screenshots; geographically
distributed services, packaged-build rendering, and a rendered soak remain
open.

### Shared-ping rendered readability extension

`BHRenderedPingFinal6-20260801-175228-Summary.json` extends the same dedicated
server plus two-rendered-client fixture with per-client PNG proof. Both clients
must apply revision 1 of the authoritative `HOSTILE` ping from `HOST_FIXTURE`,
render its issuer/context/distance marker, produce a valid 1280x720 screenshot,
and still pass the 19-AI, war-snapshot, texture-streaming, frame/GPU, draw-call,
and network checks. Screenshot allocation occurs after the measured 1,800-frame
window so image evidence cannot contaminate the performance sample.

The paired images exposed and closed a real collision with the critical
strategic-briefing header. Shared pings now occupy the upper-center coordination
band instead. Client A/B frame p95 measured 11.814/11.765 ms, GPU p95
9.138/9.080 ms, and both recorded zero frames above 50 ms, 100% desired texture
data, and zero pending stream-in p95. Peak local usage was 3,055.9/3,088.1 MB,
37.5/37.9% of the detected 8,151 MB physical GPU capacity; fluctuating WDDM
local-budget pressure remains recorded separately at 78.6/79.9%.

`BHRenderedPingRegression-Tests.log` passed all 65 tests with an empty queue;
matching startup and First Light smoke logs loaded their intended maps.
Physical marker-occlusion judgment during moving combat and geographic latency
remain manual/online gates.

## Four-player rendered-observer scale baseline

The representative `-RenderedMultiplayerScale` gate runs the shipping player
ceiling as a dedicated server, one profiled 1280x720 D3D12 observer, and three
NullRHI peers. This models clients running on separate player machines without
mischaracterizing four editor processes contending for one physical GPU as a
shipping performance target. Every client must still receive the shared war
snapshot, hostile ping, active combat density, and full 19-AI population.

Final integrated command:

`Validate-BrokenHorizon.cmd -Build -Tests -NetworkScale -RenderedMultiplayerScale -Smoke -FirstLight -LogPrefix BHRenderedMPScale-Final`

- Four-client network output p95: 49,289 B/s; per-client maximum 12,641 B/s;
  channel maximum 42.
- Rendered observer frame p95/p99/maximum: 7.035/7.502/8.399 ms.
- GPU p95/p99: 4.115/4.161 ms; render-thread p95: 7.045 ms.
- Zero frames above 50 ms; draw-call p95: 174.
- GPU-memory usage: 43.5% (3,100.8/7,123 MB).
- Desired texture data p05 and final recovery: 100%; pending stream-in p95: 0.
- Rendered evidence: `Saved/Reports/BHRenderedMPScale-Final-RenderedMultiplayerScale-20260801-152047-Summary.json`.
- Network evidence: `Saved/Logs/BHRenderedMPScale-Final-NetworkScale-20260801-151955-Summary.json`.
- The editor build, 65-test queue, startup smoke, and First Light smoke passed
  in the same command.

This closes the bounded four-player renderer-plus-network scale path. Visible
multi-client UI/readability, geographic online services, packaged rendering,
and a sustained rendered multiplayer soak remained separate gates at that
checkpoint.

## Sustained rendered multiplayer extension

The canonical `-RenderedMultiplayerSoak` gate now keeps a dedicated server and
two D3D12 clients inside the 12-hostile/4-friendly combat-density scenario for
at least 570 seconds of measured render work per client. The final
`BHRenderedSoakCleanFinal` run passed with 61,900 measured frames on each
client, 835 authority network samples, the complete 19-AI replicated
population, and live actor-backed squad-ping presentation on both clients.
There were no rejected stress spawns, deep-world AI fallbacks, fatal markers,
or frames above 50 ms. The navigation-safe fixture now projects every stress
actor onto reachable navigation and waits for dynamic navigation readiness,
preventing off-map replicated actors from invalidating a long-session load.

Evidence:
`Saved/Reports/BHRenderedSoakCleanFinal-RenderedMultiplayerSoak-20260802-061652-Summary.json`.
This is a bounded localhost UE Editor soak. The separate two-hour campaign,
travel/reconnect continuity, packaged multiplayer, and geographic-service gates
remain open.
