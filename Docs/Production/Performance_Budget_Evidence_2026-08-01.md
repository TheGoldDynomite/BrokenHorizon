# Broken Horizon performance budget evidence - 1 August 2026

## Result

The canonical validator now has a repeatable `-Performance` gate for the First
Light vertical slice. It captures a UE5.8 CSV profile, discards boot/warmup
frames, evaluates explicit CPU-proxy and memory/entity budgets, rejects fatal
runtime markers, and writes machine-readable evidence.

Command:

`Validate-BrokenHorizon.cmd -Performance -LogPrefix BHPerformanceGate-Final`

Final evidence:

- Result: passed.
- Measured frames after warmup: 420 of 600 captured frames.
- Headless frame-time p95: 0.754 ms (budget: 8.0 ms).
- Headless frame-time p99: 0.804 ms (budget: 16.67 ms).
- Frames above 33.33 ms: 0 (budget: 1).
- Peak physical memory: 2177.4 MB (budget: 4096 MB).
- Peak actor count: 95 (budget: 500).
- Peak tick-function count: 52 (budget: 500).
- Summary: `Saved/Reports/BHPerformanceGate-Final-Performance-Summary.json`.
- Profile: `Saved/Reports/BHPerformanceGate-Final-Performance-Profile.csv`.
- Runtime log: `Saved/Logs/BHPerformanceGate-Final-Performance.log`.

The capture uses a project-local `-userdir`, so profiler, config, and save
writes do not depend on a developer's AppData permissions. UE CSV files with
duplicate statistic names and trailing header/metadata rows are parsed without
discarding valid frame samples.

## Budget definition

These are initial PC vertical-slice headless regression budgets, not final
shipping hardware targets. A regression fails when any measured threshold is
exceeded. The budget values and each individual check are serialized into the
JSON evidence so future changes remain reviewable.

## Evidence boundary

This is an Editor `NullRHI` CPU/memory/entity baseline. It does not prove GPU
frame time, draw calls, shader/material cost, lighting cost, texture quality,
texture-streaming presentation, or final packaged-build performance. Those
require a rendered representative-PC capture and manual visual review.

The standalone capture also does not prove multiplayer bandwidth, replication
saturation, packet-loss behavior, travel/reconnect stability, or long-session
soak. The JSON explicitly records `rendererProof=false` and
`networkProof=false`. The separate dedicated-server/two-client baseline is now
recorded in `Network_Budget_Evidence_2026-08-01.md`; shipping-scale network and
lower-tier and shipping-scale network evidence remains open.

## Rendered 1080p D3D12 baseline

The canonical validator now exposes a separate `-RenderedPerformance` gate.
It renders First Light offscreen through D3D12 at a forced 1920x1080 resolution,
enables UE GPU CSV stats, discards 180 warmup frames, and evaluates 420 measured
frames. Unlike the NullRHI gate, its JSON records `rendererProof=true` and
includes the exact GPU, driver, resolution, and RHI.

Final command:

`Validate-BrokenHorizon.cmd -RenderedPerformance -LogPrefix BHRenderedPerformance-Final`

Hardware:

- GPU: NVIDIA GeForce RTX 5060 Ti.
- Driver: 610.88.
- RHI/feature path: D3D12 SM6.
- Resolution: 1920x1080 offscreen.

Final measurements:

- Frame p95: 7.304 ms; p99: 7.596 ms (budgets: 16.67/16.67 ms).
- GPU p95: 6.342 ms; p99: 6.472 ms (budgets: 16.67/20.0 ms).
- Render-thread p95: 7.289 ms (budget: 16.67 ms).
- Game-thread p95: 2.639 ms.
- Frames above 33.33 ms: 0.
- Draw calls p95: 158; maximum: 164 (p95 budget: 2,000).
- Primitives drawn p95: 271,219 (budget: 1,000,000).
- Local GPU memory maximum: 3,321 MB of a minimum 7,123 MB budget, or 46.6%
  (budget: 80%).
- Desired texture data loaded minimum: 100% (budget: 99%).
- Pending stream-in data maximum: 0 (budget: 0).
- Physical memory maximum: 3,389 MB.
- Evidence: `Saved/Reports/BHRenderedPerformance-Final-RenderedPerformance-Summary.json`.
- Profile: `Saved/Reports/BHRenderedPerformance-Final-RenderedPerformance-Profile.csv`.
- Runtime log: `Saved/Logs/BHRenderedPerformance-Final-RenderedPerformance.log`.

The original project-local NullRHI path was rerun after the shared profiler
changes and passed in `BHPerformance-PostRendered-Performance-Summary.json`.

This rendered gate proves the initial First Light gameplay view on this
specific machine and driver. It does not prove lower hardware tiers, packaged
build behavior, multiplayer rendering, traversal across streaming boundaries,
visible mip quality, shader-compilation stutter, or subjective image quality.
Those remain separate capture/manual-review requirements.

## Two-rendered-client production loot route

`BHRenderedLootHUD-20260801-194607-Summary.json` combines the production First
Light keycard/door/combat/loot/extraction route with two simultaneous 1280x720
D3D12 clients and the established 19-actor combat-density fixture. Across 600
measured frames per client, frame p95 was 11.976/12.028 ms, GPU p95 was
9.196/9.237 ms, maximum frame time was 15.131/13.962 ms, and no frame exceeded
50 ms. Draw-call p95 was 167/174, physical GPU-memory usage peaked at 37.9% of
the detected 8151 MB, desired texture data remained 100%, and pending stream-in
p95 was zero. Client B also captured the real rewarded `30 / 180` ammo HUD.
This is bounded localhost/editor evidence, not packaged, lower-tier,
geographic-Internet, or rendered-soak proof.

## Rendered objective-route traversal baseline

`Validate-BrokenHorizon.cmd -RenderedTraversalPerformance` now moves an
invulnerable test viewpoint through the First Light keycard, security door,
hostile-contact, and extraction landmarks twice. Movement is interpolated,
each landmark includes a streaming-recovery dwell, and the camera stays outside
interaction volumes. The gate requires exactly eight route markers, texture
recovery, and zero frames above 50 ms.

The full regression command
`Validate-BrokenHorizon.cmd -Build -Tests -Smoke -FirstLight -RenderedTraversalPerformance -LogPrefix BHRenderedTraversal-Final2`
passed build, 65 automation tests, startup smoke, First Light smoke, and 1,620
measured rendered frames. Frame p95/p99 were 8.148/9.080 ms, GPU p95/p99 were
7.317/7.546 ms, frame maximum was 24.350 ms, and there were zero >33 ms or
>50 ms hitches. Desired texture data p05 and final recovery were both 100%,
pending stream-in p95 was zero, and pending data appeared in 3.89% of measured
frames. GPU-memory use peaked at 46.7% (3,327.2/7,123 MB).

Evidence: `Saved/Reports/BHRenderedTraversal-Final2-RenderedTraversalPerformance-Summary.json`.
This proves the bounded First Light objective route on the recorded RTX 5060 Ti
editor/D3D12 configuration. A separate simultaneous two-client rendered
combat-density gate is documented in `Network_Budget_Evidence_2026-08-01.md`.

## Canonical full-world sector traversal baseline

`Validate-BrokenHorizon.cmd -RenderedWorldPerformance` now exercises the
canonical `/Game/BrokenHorizon/Maps/L_BrokenHorizon_World` map. The fixture
visits all six stable sector anchors, then repeats the first two anchors, for
eight independently logged traversal steps. Each cross-sector reposition is
treated as a loading transition: the fixture waits five seconds for recovery
and measures only the following local 12-metre movement segment. This prevents
fast-travel distance from being misrepresented as ordinary player traversal
while still requiring each streamed sector to recover before measurement.

Canonical command:

`Validate-BrokenHorizon.cmd -RenderedWorldPerformance -LogPrefix BHFullWorldTraversalCanonical`

The 152 marked local-traversal samples passed every strict check:

- Frame p95/p99: 9.119/13.809 ms; maximum: 16.156 ms.
- GPU p95/p99: 7.179/7.516 ms; render-thread p95: 8.134 ms.
- Frames above 33 ms and traversal hitches above 50 ms: 0.
- Draw calls p95: 353; primitives drawn p95: 897,239.
- Desired texture data p05/final: 100%/100%; pending stream-in p95 and frame
  incidence: 0/0%.
- GPU-memory usage maximum: 38% (2,703.3 MB used against a 7,123 MB budget).
- Physical memory maximum: 5,120.9 MB (budget: 6,144 MB).
- Actor/tick-function maxima: 602/73. The full-world actor ceiling is 700;
  First Light retains its separate 500-actor ceiling.

Evidence:

- Summary: `Saved/Reports/BHFullWorldTraversalCanonical-RenderedWorldPerformance-Summary.json`.
- Profile: `Saved/Reports/BHFullWorldTraversalCanonical-RenderedWorldPerformance-Profile.csv`.
- Runtime log: `Saved/Logs/BHFullWorldTraversalCanonical-RenderedWorldPerformance.log`.

An earlier diagnostic that interpolated continuously across 5-14 km sector
separations was rejected because it measured forced cross-world relocation as
walking and produced artificial 50-80 ms frame spikes. The corrected gate
retains loading recovery as a prerequisite but restricts accepted samples to
normal local movement.

The subsequent navigation-capacity hardening increased the OpenWorld Recast
tile size from 8,192 to 9,216 UU. With the authored 25.2 km bounds and 12
average layers, the strengthened region validator calculates 900,912 required
tile addresses against UE's 1,048,576 hard limit; the prior 1,133,368-tile
warning is gone. The runtime gate now additionally rejects that warning and
requires successful nav projection at every one of its eight sector visits.

Final hardened command:

`Validate-BrokenHorizon.cmd -RenderedWorldPerformance -LogPrefix BHOpenWorldNavigationFinal3`

The final capture records `navigationProof=true`, `navigationSteps=8`, and all
strict performance checks passed: frame p95/p99 were 8.428/14.778 ms, maximum
frame time was 18.636 ms, GPU p95/p99 were 7.232/7.315 ms, desired texture data
remained 100%, pending stream-in incidence was 0%, and there were zero >33 ms
or >50 ms hitches. Peak physical memory was 5,254.8 MB, GPU-memory use was 38%,
and actor/tick-function maxima were 607/79. Evidence is in
`Saved/Reports/BHOpenWorldNavigationFinal3-RenderedWorldPerformance-Summary.json`
and `Saved/Logs/BHOpenWorldNavigationFinal3-RenderedWorldPerformance.log`.

This is offscreen UE5.8 Editor D3D12 evidence on the recorded RTX 5060 Ti. It
does not prove packaged-build behavior, lower hardware tiers, visible mip
quality, cold-cache packaged PSO behavior, fast-travel presentation, subjective
image quality, or packaged multiplayer behavior. Those remain separate
validation and manual-review requirements.

## Warm-cache shader and PSO hitch evidence

The rendered performance harness now requires Unreal's graphics and compute PSO
CSV counters, reports total misses and loaded shader/map counts, and fails if a
PSO miss is associated with a hitch. UE's `-1` not-a-hitch sentinel is retained
as applicability evidence and never summed as a negative miss. A separate GPU
budget-stability check rejects runs where Windows reduces the process-local
budget below 80% of its observed maximum, preventing externally constrained
captures from being accepted as project performance.

- `BHShaderStutterAccepted-RenderedPerformance-Summary.json`: 420 measured
  static First Light frames, 7.941 ms frame p95, zero hitches, zero graphics PSO
  misses, one compute miss, and zero hitch-associated misses.
- `BHShaderTraversalAccepted-RenderedTraversalPerformance-Summary.json`: 1,620
  measured route frames, 8.722 ms frame p95, zero hitches, six graphics misses,
  one compute miss, and zero hitch-associated misses.
- `BHShaderWorldTraversalAccepted-RenderedWorldPerformance-Summary.json`: all
  eight six-sector navigation visits, 9.385/12.401 ms frame p95/p99, one allowed
  >33 ms frame, zero >50 ms traversal hitches, stable 100% GPU-budget p05/max,
  and zero graphics, compute, or hitch-associated PSO misses.

Two intermediate full-world attempts are retained as rejected diagnostics: one
exceeded the primitive p95 ceiling and two experienced a transient Windows GPU
budget collapse. They are not acceptance evidence. This closes the warm-cache
editor PSO/hitch measurement gap; a cold-cache packaged pipeline-cache run is
still required for release.

## Sustained two-client rendered combat soak

`Validate-BrokenHorizon.cmd -RenderedMultiplayerSoak` now runs a dedicated
server and two simultaneous 1280x720 D3D12 clients under the 12-hostile,
4-friendly combat-density fixture. It requires at least 36,000 post-warmup
frames and 570 seconds of measured render work per client, live actor-backed
squad-ping presentation, the 19-AI replicated population, network telemetry,
texture recovery, and every existing frame/GPU/VRAM budget.

Final command:

`Validate-BrokenHorizon.cmd -RenderedMultiplayerSoak -LogPrefix BHRenderedSoakCleanFinal`

- Client A measured 61,900 frames over 601.8 seconds: frame p95/p99/maximum
  12.845/14.466/19.362 ms and GPU p95/p99 9.293/10.423 ms.
- Client B measured 61,900 frames over 589.7 seconds: frame p95/p99/maximum
  12.488/14.144/18.894 ms and GPU p95/p99 9.214/10.333 ms.
- Both clients recorded zero frames above 50 ms, 100% desired texture data,
  zero pending stream-in p95, and 37.9% peak use of the detected 8,151 MB
  physical GPU capacity.
- The authority configured all 16 stress actors once, emitted 835 network
  samples, and recorded no deep-world AI fallback or fatal marker.
- Evidence:
  `Saved/Reports/BHRenderedSoakCleanFinal-RenderedMultiplayerSoak-20260802-061652-Summary.json`.

The harness deliberately omits synchronous PNG capture during soak profiling;
the preceding short `-RenderedMultiplayer` gate retains per-client screenshot
proof. Two failed diagnostics established that the PNG request itself caused
the only synchronized 0.40-0.50 second frame on both local clients. The soak
still requires each rendered client to log the live tracked-ping presentation.
This closes the bounded ten-minute editor-rendered soak, not the separate
two-hour campaign/reconnect soak, packaged behavior, WAN behavior, or lower-tier
hardware qualification.
