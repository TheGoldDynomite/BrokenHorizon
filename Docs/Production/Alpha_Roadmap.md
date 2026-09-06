# Alpha roadmap

Updated: 2026-09-05. Target: **G5 Alpha**, a complete playable limited-region
resistance campaign (M6). **A1 development has explicitly resumed.** Q2 health/stamina
150% HUD layout is Verified for its bounded scope. Bounded First Light topology coverage is now Verified. A0/A1/G2 remain open;
resumption does not authorize the blocked lighting map edit. The package-aware
performance runner is Verified for bounded measurement: three valid captures,
all three runs failed at least one provisional budget. Full A1/S07 acceptance remains open.

## Document ownership and gate names

This file owns alpha execution order and acceptance gates. The unaccepted
[Alpha Manifest](Alpha_Manifest.md) supplies reviewable content/support candidates; A0 remains open. The
[Module Implementation Plan](ModuleImplementationPlan.md) retains the detailed
backlog; [Alpha Progress](Alpha_Progress.md) owns verified delivery evidence.
The [content inventory](Broken_Horizon_1.0_ContentInventory.md) and
[operation matrix](Broken_Horizon_1.0_OperationMatrix.md) remain production records.

Use the canonical [GDD-12 Production Roadmap](../GDD_Modules/GDD-12_Production_Roadmap.docx):
G0 foundations, G1 networked First Light, G2 vertical slice, G3 three-sector
prototype, G4 content production, G5 Alpha, G6 Beta, G7 release candidate.
The [legacy GDD](../Broken_Horizon_GDD_v2.md) uses G4 as release-candidate shorthand;
that conflicting shorthand is not used here. Older files remain unchanged.
A0-A6 below are work packages toward G5, not replacements for canonical gates.

## Starting evidence

- Delivered checkpoints: `4a77387` combat HUD readability, `7de8bf3` Defense A,
  and `9b4cd86` same-process reconnect. Reuse their working systems and scoped proof.
- Preceding roadmap baseline suite: 127 tests, comprising 125 successes and two expected-warning
  successes, with zero failed/not-run/in-process:
  `Saved/Logs/Codex/AutomationReport-20260905-103902/index.json`.
- The preceding Readability Development package passed the four-objective First Light
  route and an inspected packaged 720p HUD capture. Details and limits are in
  [Alpha Progress](Alpha_Progress.md).
- These are prior-checkpoint results, not new validation of the resumed A1 slice. They do not prove the
  complete campaign, all earlier canonical gates, packaged multiplayer, or physical play.
- Defense A lifecycle/Continue already passes its fixture. Remaining facility art
  and manual tactical quality should reuse that proof rather than rebuild the runtime.

## Required alpha player outcome

A squad must be able to start a fresh campaign in a connected limited region and
complete this loop using actual controls and included content:

**Prepare -> deploy -> traverse -> fight and handle casualties -> extract/debrief
-> treat, recruit, resupply, and save -> choose the next operation.**

- [ ] Attack, Defense, Raid, and Resupply are all playable operation families.
- [ ] Forces, gear, resources, transport, and world state persist across operations.
- [ ] World state changes the operations available and their consequences.
- [ ] Campaign victory and defeat/recovery are reachable without debug intervention;
      supply loss cannot create an unrecoverable accidental progression dead end.
- [ ] Every promised multiplayer configuration supports objectives and world-state
      convergence, late join, reconnect, host recovery, and travel.
- [ ] All included routes, facilities, interactions, and combat spaces are reachable,
      readable, and usable in actual play.

Authored content is an alpha dependency. The alpha manifest must enumerate included
world spaces, weapons and first-person arms, enemy assets, animation sets, audio,
VFX, and transport, with ownership/source, integration state, and acceptance proof.
Accept each row in its actual gameplay context: enemy/interaction silhouettes,
weapon handling, locomotion and casualty feedback, route readability, sound cues,
and effects must support player decisions. Use
[Art Direction](../../Documentation/Art/ART_DIRECTION.md) and First Light target
quality as the template. Explicitly documented nonblocking temporary art may remain;
missing functional feedback or unreadable enemies cannot be dismissed as polish.

Full 1.0 arsenal breadth, final M8 quantities, all eight A/B operation variants,
the broader 1.0 region, and optional escort/rescue are not
automatic alpha prerequisites. Their obligations remain in the
[1.0 Roadmap](Broken_Horizon_1.0_Roadmap.md) and inventories. Include them in alpha
only through an explicit manifest decision; retain all four required families.

## Proposed working scope to lock before content multiplication

These are sourced planning defaults, **not already locked user decisions**. A0
records the accepted alpha manifest and supported configurations; any deviation
must state its effect on the complete player loop.

| Item | Proposed working target | Lock and acceptance record |
| --- | --- | --- |
| Region | 6-8 connected sectors | A0 names included sectors/routes; do not invent extra sites to satisfy a count. |
| First campaign | 4-6 hours | A0 states pacing assumptions; A4 measures an actual fresh campaign. |
| Squad | 2-4 players | A0 names promised player counts and settings; A5 proves each promised configuration. |
| Hosting | Listen and dedicated paths are required by G2 | A0 explicitly names the supported alpha topologies, recovery behavior, and travel settings; unresolved scope is a gate, not an implicit omission. |
| Performance | Hardware, resolution, preset, frame-time and memory budgets | Lock measured targets at A1; repeat representative worst-case measurements at A5. No FPS promise is inferred here. |
| Network endurance | Real remote runs, impairment cases, and two-hour soak | A0 defines scenarios; A5 records results. These are existing gate obligations, not current passes. |

The [Module Implementation Plan](ModuleImplementationPlan.md) and canonical
[GDD-12](../GDD_Modules/GDD-12_Production_Roadmap.docx) provide the planning context.
Do not publish a date or completion percentage until measured throughput supports it.

## Ordered milestones

Milestone gates remain open; A1 has an active bounded source slice. Existing scoped proof may
satisfy individual checks; writing the roadmap does not complete a gate.

| ID | Dependency and status | Concrete deliverable | Exit proof |
| --- | --- | --- | --- |
| A0 Scope and support lock | First; open | Named alpha content manifest, supported player/topology/settings matrix, four required families, save/recovery expectations, and explicit deferred 1.0 items. | Every included row has an owner, integration dependency, and player-facing acceptance check; working scope accepted before content multiplication. |
| A1 First Light quality and G2 acceptance | Q2 and bounded topology matrix verified; full gate open and A0 scope lock required | Accepted world/combat/UI template with readable lighting, actual controls, coherent weapon/arms/enemy/animation/audio/VFX, measured performance targets, and multiplayer vertical slice. | Two-player natural First Light play plus visual/input review; listen AND dedicated functional paths with packaged clients hosting/joining as appropriate, reconnecting, and completing an operation; first-quality hardware/preset/frame-time/memory record. |
| A2 Connected prototype and four families | A0 plus A1/G2 acceptance; open | Connected three-sector prototype with Attack, Defense, Raid, and Resupply proving the full preparation-to-next-operation loop. | Natural operation routes, traversal, casualty handling, extraction/debrief, logistics, saves, and world-state consequences; reuse existing Defense A fixture evidence. |
| A3 Included regional content and logistics | A2; open | Playable alpha manifest across the included region, authored facilities/routes and required content, functioning movement of personnel/supplies/transport. | Every included inventory row reachable and accepted; corridor delivery/casualty recovery, transport persistence, and no supply dead ends. |
| A4 Complete campaign progression | A3; open | World-state-driven progression from fresh campaign to victory and a meaningful defeat/recovery path. | Recorded complete campaign run, deliberate setbacks, persistence checks, measured first-campaign duration, and no debug intervention. |
| A5 Packaged acceptance | A4; open | Supported multiplayer/recovery/travel, performance and actual-player acceptance in a fresh package. | Every promised configuration, real remote/impairment cases, two-hour soak, late join/reconnect/host recovery, budget measurements, and clean-install play. |
| A6 Alpha candidate handoff | A5; open | Versioned candidate, final validation/review, known issues, tester instructions, save compatibility statement. | Release checklist below satisfied; candidate reproducible and ready for the explicitly authorized testing audience. |

Content production begins alongside A2 only after A1/G2 multiplayer vertical-slice
acceptance and approval of the first template and manifest. This preserves the
Module Implementation Plan prerequisite. Integrate independent manifest rows in parallel where safe;
A3 closes regional coverage rather than postponing all art/content until the end.
Do not require a full repository audit or repeat all previously green tests before
starting the next bounded task; rerun proof when changed dependencies warrant it.

The bounded A1 session-recovery slice is Verified with Editor and packaged evidence: rejected travel,
no-match Join, actual button events, real host loss, and same-client direct Join
retry without hidden Leave. Build, six targeted tests (two expected-warning
successes), 59 parser cases, and four-file review pass; see [Alpha Progress](Alpha_Progress.md).
Fresh package and packaged recovery pass. SameProcessReconnect crashed during the
retained client's second First Light load with EXCEPTION_ILLEGAL_INSTRUCTION;
the exact unchanged retry passed all 18 assertions, but the first crash remains an
open intermittent reload-reliability finding with no supported cause or hardware
attribution. Final Validate passed with 132 tests (128 successes/four warning-successes)
and an up-to-date build; final review found no supported blocker in the unchanged
four files. The crash cannot be declared unrelated or resolved; its reliability
finding and full A1/G2 remain open. Continue accepted-versus-applied completion is
the next independent source task; save restoration, physical input, rendered
appearance and internet acceptance remain separate, with no save API change in this delivery.
Performance checkpoint `ae20a5238db84418836419dff2aa06a3776be118` is pushed and
remote-SHA verified; its three valid captures still fail provisional budgets.
Installed UE 5.8 lacks Server distribution support; packaged dedicated delivery
requires an approved server-capable source/custom engine installation, not a metadata
workaround. No Server build was attempted. A0/Q1/actual-control gates remain open.

## Active A1 task queue

No item below starts from this document alone. Priority and readiness are separate:
Q1 lighting is high priority but approval-blocked; Q2 is verified. Unlocked scope budgets do not block contained
existing-map/HUD repairs. A0 scope lock and G2 acceptance precede multiplying
content; the following bounded tasks provide the nearest player-facing proof.

| ID | Scope and dependency | Done when |
| --- | --- | --- |
| Q1 First Light lighting | Review the prepared narrow repair for black sky/overbright world. Map remains unchanged; automatic approval review rejected the normal escalated map-save launch again after A1 resumed, before any process started. Explicit map-edit approval has been requested; the script remains unexecuted. Author only once applicable approval is resolved. | Approved in-Editor authoring preserves gameplay actors/IDs; before/after rendered comparison accepts sky, surfaces, enemies, and HUD at the recorded settings. |
| Q2 HUD scale edge | Verified bounded delivery: final source/rendered/package evidence confirms four actual vitals bindings grouped, 150% edge overflow fixed, 80% safe bounds initialized before Slate, and strategic/field/transport text clear of vitals. Long field context wraps with its background. Independent of Q1 approval. | Actual bars remain within the safe area at supported scales/resolutions; existing medical/ammo readability remains intact. |
| Q3 Actual First Light controls | Open: Computer Use failed before game input on initialization, retry, and a clean-reset third attempt. Two-player play using Enter/M Continue, scrolling, firing/reloading, keycard/door interaction, combat, and extraction. Use working runtime systems. | Both players complete the natural route and actual input/debrief flow; record failures and minimum remaining manual steps, not delegate-only proof. |
| Q4 Natural Attack A | FirstLightAttackA has five guards and eight cover points. The strategic North Pass and physical marker are distinct concepts; inspect the actual deployment route. | Players deploy, navigate, fight naturally, secure, and debrief with correct world-state consequence; the older deterministic completion hook does not count as natural combat proof. |
| Q5 Logistics corridor | Western FOB -> Eastern Depot, including FirstLightWaterRoute01 and FirstLightWatercraft01; use existing transport/logistics paths. | Boarding, crossing, delivery, casualty recovery, and subsequent save/reload work through actual play without supply/progression dead ends. |
| Q6 Natural Raid A | Depot patrol, sabotage, and exfiltration through the authored route. | Natural patrol/combat or avoidance, sabotage result, extraction/debrief, and campaign persistence are demonstrated with readable facility/interaction cues. |

Computer Use initialization, retry, and clean-reset third attempt all failed before
any game input: `windows sandbox failed: helper_unknown_error: apply deny-read ACLs`.
No custom input workaround was used. Actual controls and human-feel acceptance
remain open; automated source, rendered, and packaged validation can proceed.
Q2 is Verified: final build/127-test suite/review, seven normal HUD frames, First
Light/Defense A regression, all ten fresh Defense A images, and the fresh Vitals
package with four-objective route and inspected 720p HUD150 pass; see
[Alpha Progress](Alpha_Progress.md). Q2 commit
`58d2380f524dff9812e204d42222d2d0d97f11b9` was pushed to
`origin/codex/first-light-acceptance`; its remote SHA was verified.
Bounded First Light topology coverage is Verified: preserved Editor default,
hybrid Editor dedicated server with packaged remotes, fully packaged listen, and
packaged standalone. The final 127-test suite, scoped review, fresh topology package,
and eight negative cases pass; exact reports are in [Alpha Progress](Alpha_Progress.md).
The validated delivery is tracked by active development-branch history.

This is NullRHI same-machine direct-loopback objective/loot/ammo/rejoin and actual
widget-state proof, not new pixels, physical input, session-menu/debrief flow,
remote-internet play, or a packaged dedicated-server executable. Optional inventory
transfer and weapon-role modes were not rerun. Q2 visual evidence remains separate.
The manifest support rows remain candidates; unanswered A0 scope, full A1/G2,
performance/content, actual controls, and Q1 explicit map approval remain open.
The recorded six-sector graph does not accept physical traversal or regional scope.

The topology update was delivered as `1df892b9e9c2b85929ae29cb586fdf3191790e96`
(pushed and remote SHA verified). The completed bounded measurement slice covers Editor compatibility
and packaged first/repeat runs on the same local hardware/configuration: First Light
eight-step, two-loop traversal, 1080p D3D12, unique UserDirs, owned CSVs, and observed
effective settings. All captures are valid: Editor fails primitives and two streaming
budgets; each packaged run fails primitives only. Final review and 45 parser cases
pass; final Validate also passed with 127 tests and an up-to-date build. Exact reports/commands are in Alpha Progress. Provisional budgets are
not accepted promises; boot capture/180-frame prefix does not establish cache warmth,
and differing August reports cannot support an old-versus-new improvement claim.

The [operation matrix](Broken_Horizon_1.0_OperationMatrix.md) still has all eight
A/B rows in progress for authored/manual criteria. Do not schedule already
implemented runtime systems again or require all eight variants for alpha without
manifest adoption. Defense A needs facility art/manual tactical quality, not a
repeat implementation of its accepted lifecycle.

## Working cadence and task card

During active development, select the next unpassed gate and one playable
slice. Assign read-only exploration and architecture review when needed, one
production writer, and one execution owner. Build a coherent change, collect
scoped proof, then run the applicable final automated gate and scoped review.
Record exact evidence and remaining manual limits in [Alpha Progress](Alpha_Progress.md).
Commit and push each completed scoped update under standing user permission;
preserve unrelated WIP. No recurring automation or build is started by this plan.

If asset approval blocks a task, choose an independent ready task. Do not assume elapsed time or planning approval grants asset authorization.
Review this plan after each gate and weekly while actively working; adjust sequence
using evidence without silently changing the accepted alpha manifest.

Use this compact task card for execution:

- **ID / gate / player outcome:**
- **Owned files / writer / execution owner:**
- **Dependencies / accepted scope / unresolved approvals:**
- **Proof:** exact command, report/capture, pass/fail, and evidence boundary.
- **Remaining manual checks / known issues:**
- **Commit / pushed branch:** completed scoped delivery only.

## Alpha candidate release checklist

- [ ] Fresh complete campaign reaches victory and exercises defeat/recovery.
- [ ] Every included alpha inventory row is playable, reachable, and accepted.
- [ ] Inventory, forces/casualties, resources, transport/logistics, and world state persist.
- [ ] Every promised multiplayer configuration passes real remote play, impairment,
      late join, reconnect, host recovery, travel, and the defined two-hour soak.
- [ ] Actual controls, UI/scrolling, navigation, low-light readability, and agreed
      hardware/resolution/preset/frame-time/memory budgets are accepted.
- [ ] No critical campaign stopper, save loss, desynchronization, or crash remains.
- [ ] Final registered tests and scoped review pass for the stable candidate.
- [ ] Fresh versioned package passes clean-install startup and the required campaign route.
- [ ] Known issues, tester instructions, supported configurations, and save compatibility
      are recorded with the candidate and exact evidence references.

Beta (G6) and release candidate (G7) follow alpha. This roadmap grants no public
release, packaged-build distribution, or default-branch merge permission.
