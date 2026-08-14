# Broken Horizon 1.0 Roadmap Evidence — 2026-08-09

This note records the current M8/M9 evidence against
`Broken_Horizon_1.0_Roadmap.md`. It is supplemental to the readiness checklist;
it does not close the remaining manual release gates by itself.

## Verified this pass

- Fresh cooked build: `Builds/RoadmapM8-Cooked/Windows` launched successfully.
- Windows-control packaged review: gameplay HUD, objective state, first-person
  weapon presentation, pause menu focus, and Settings navigation were visible.
- Settings input-prompt selector exposed `Auto (Last Input)`,
  `Keyboard + Mouse`, `Controller`, and `Show Both`.
- Selecting `Controller` and applying settings updated the visible selector and
  the contextual gameplay hints.
- Asset audit passed: 259 assets scanned, zero oversized/non-streaming texture
  violations, zero unknown static-mesh LODs, and zero collisionless static meshes.
- Required audio/FX assignment audit passed: 18/18 required assignments.
- Localization audit passed: 4,736 catalog words and zero conflicts.
- Rendered UI gate passed at 720p, 1080p, and 4K, including safe-area extremes
  and the required session states.
- Clean rendered performance gate passed: frame p95 7.459 ms, GPU p95
  6.253 ms, zero hitches, zero PSO misses, and 41% VRAM usage.
- Final rendered two-client soak passed: two connected players, complete CSV
  capture/finalization, client frame p95 11.587/11.547 ms, and GPU p95
  8.792/8.765 ms.
- Consolidated current-source gate passed: build, automation, localization,
  startup smoke, First Light smoke, and packaged smoke.
- Fresh network-impairment validation passed: aggregate outbound p95 49,756
  B/s, per-client outbound maximum 12,418 B/s, and 41 replicated channels,
  with reconnect convergence verified under the configured impairment profile.
- A fresh Windows-control launch of the cooked build reached the multiplayer
  main menu with Host New Campaign, Host Continue, Join Campaign, and Settings
  visible and the multiplayer status card reporting ready.

## Two-hour soak result

The corrected two-hour run was launched with 432,000 frames and client command
lines confirmed at `-seconds=7205`. It reached active rendered gameplay, but
both client logs stopped advancing at approximately game time `21:42:06`
while the processes remained responsive and CPU-active. After repeated checks
with no log progress, the exact owned processes were stopped and the run was
classified as an inconclusive/stalled failure rather than a pass. The preserved
logs are:

- `Saved/Logs/BHRoadmapM9TwoHourSoak-20260809-143820-ClientA.log`
- `Saved/Logs/BHRoadmapM9TwoHourSoak-20260809-143820-ClientB.log`
- `Saved/Logs/BHRoadmapM9TwoHourSoak-20260809-143820-Host.log`

The two-hour cooperative soak remains an open M9 blocker and needs a focused
stall diagnosis before another full-duration attempt.

## Long-soak diagnosis update

The source fix in `BHUIStyle.cpp` now skips repeated full widget-tree styling
when a widget's style context is unchanged, while `RefreshAll` invalidates the
cache for settings-driven restyles. Build, automation, and First Light smoke
passed after the change.

The first post-fix rendered soak attempt failed with a D3D12 out-of-video-memory
error because a stale manually launched packaged process was still consuming
approximately 1.8 GB. That process was stopped, and a clean rerun produced
both large client CSV profiles and clean Unreal shutdown logs. Its JSON summary
was not produced after the validation wait was interrupted, so it is retained
as partial evidence rather than a pass and must be rerun through final summary
generation.

The clean post-fix rendered soak was rerun through final summary generation and
passed: two connected/rendered clients, 62,400 measured frames per client,
587.1/586.3 measured seconds, frame p95 11.817/11.807 ms, GPU p95
8.902/8.839 ms, zero frames over 50 ms, 32.4% GPU-memory usage, 100% desired
texture data, and zero pending stream-in data. Evidence:
`Saved/Reports/BHRoadmapM9UIStyleCacheSoakSummary-20260809-181552-Summary.json`.
This closes the corrected ten-minute rendered soak regression check, but does
not close the separate two-hour M9 requirement.

## Lower-tier qualification result

The performance harness now supports an explicit rendered lower-tier profile
(canonical scalability level 1, reduced quality settings, 75% screen
percentage, and VSync disabled for measurement). An initial capture exposed a
harness configuration issue: without the explicit `-NoVSync` and
`-ScalabilityQuality=1` startup flags, the capture clustered at 33.3 ms and did
not represent the intended profile. That run is superseded, not used as a
game-performance verdict.

The corrected lower-tier capture passed all current budgets:

- Frame p95 7.420 ms; p99 7.674 ms; zero hitches.
- GPU p95 6.250 ms; p99 6.363 ms; render-thread p95 7.422 ms.
- Zero PSO misses, 40.9% VRAM usage, 3,380.6 MB physical memory.

Evidence: `Saved/Reports/BHRoadmapM9LowerTierNoVsync-Summary.json`.

## Still open for M8/M9 signoff

- Physical gamepad input and couch-distance readability with hardware.
- Subjective combat, audio, lighting/look, balance, and navigation review.
- Final disposition of the two shipping-referenced placeholder candidates.
- Lower-tier and cold-cache performance qualification.
- Two-hour-or-longer cooperative campaign soak, including reconnect and host
  recovery during the extended run.
- Geographic/WAN service profile, shipping configuration, installer/update,
  signing, release notes, and final gate-owner signoff.

## Interpretation

The current evidence proves the packaged build and the automated/rendered
quality gates are progressing, but the M9 exit gate is not yet complete because
the manual hardware, extended soak, lower-tier, service, and release-delivery
reviews remain open.
