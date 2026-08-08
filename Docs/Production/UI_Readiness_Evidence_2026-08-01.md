# UI readiness evidence - 2026-08-01

## Combat-aware campaign notification deferral

Background campaign updates no longer compete with immediate combat. War-turn,
route, campaign-event, and strategic-priority messages use a dedicated reliable
owner-client path and remain queued while the local player has meaningful
suppression or is inside the authorable eight-second post-damage quiet window.
The queue releases when combat pressure clears. Active-operation briefings,
objective events, squad commands, casualty alarms, and other tactical messages
remain immediate and can pass a waiting background update.

Post-combat release is latest-state coalesced. If several background war
snapshots arrive during one engagement, each newer deferred strategic update
replaces the older deferred snapshot; one current strategic state releases
after the quiet policy instead of replaying a stale sequence. Tactical and
operation notifications are not coalesced and can still preempt or pass the
deferred background entry.

`BHPostCombatCadence-Focused2.log` verifies actual native queue state after two
distinct strategic updates during combat: one pending entry remains and it is
still classified as combat-deferred. The UE5.8 editor target compiled, and
`BHPostCombatCadenceFinal-*` passed all 66 automation tests, startup smoke, and
First Light at 8/12 bounded navigation fallbacks. Physical multi-client combat
review remains required for subjective dwell, motion occlusion, and latency
feel; the stale-message flood risk is covered by automation.

## Multiplayer main-menu clarity

The native main-menu layer now renders every multiplayer session state with a
stable, localized heading and detail line:

- `MULTIPLAYER // READY`
- `MULTIPLAYER // HOSTING`
- `MULTIPLAYER // SEARCHING`
- `MULTIPLAYER // JOINING`
- `MULTIPLAYER // CONNECTING`
- `MULTIPLAYER // CONNECTED`
- `MULTIPLAYER // LEAVING`
- `MULTIPLAYER // ACTION FAILED`

Ready, pending, connected, and failed states use distinct neutral, amber, green,
and red status colors. Session failures broadcast synchronously from the session
subsystem and are no longer delivered to the Blueprint error event a second time
by the click handler.

## Automated evidence

- `BHSessionClarity-Build.log`: UE 5.8 editor target compiled successfully.
- `BHSessionClarity-Tests.log`: 46 succeeded, 0 failed, automation queue empty.
- `BrokenHorizon.Multiplayer.Session.Contract` verifies every state heading,
  detail composition, Blueprint-facing controls, and distinct error color.
- `BHSessionClarity-Smoke.log`: `L_MainMenu` launched; native styling visited
  12 widgets, 6 text blocks, and 5 buttons; zero strict failure markers.
- `BHSessionClarity-FirstLight.log`: First Light launched with zero strict
  failure markers.
- `BHSessionRenderedFinal-RenderedUI-20260801-173048-Summary.json`: ready,
  searching, connected, and failed main-menu states passed at 720p and 4K as
  part of the canonical 27-capture gate. The matching 65-test queue, startup,
  and First Light logs passed.

## Manual boundary

The rendered gate now proves static line wrapping, anchored placement, disabled
pending controls, and neutral/amber/green/red appearance at 720p and 4K. Focus
order, gamepad navigation, live host/join transitions, and network-error recovery
remain part of interactive UI/UX review.

## Combat and objective notification priority

The objective notification layer now protects time-critical mission information
from routine status traffic. Critical objective completions, shared-operation
success/failure debriefs, and operative-loss events immediately preempt lower
priority messages. Strategic briefings, operation assignments, route failures,
and casualty stabilization use the high-priority path. Preempted messages return
to a bounded queue ordered by priority while equal-priority messages retain FIFO
presentation. Existing Blueprint calls to `ShowNotification` and character calls
to `ShowStatusNotification` remain compatible and default to normal priority.

Automated evidence:

- `BHNotificationPriorityPass-Build.log`: UE 5.8 editor target compiled.
- `BHNotificationPriorityPass-Tests.log`: 47 succeeded, 0 failed, queue empty;
  `BrokenHorizon.UI.Notification.PriorityContract` passed and verifies critical
  and high preemption, routine-message protection, equal-priority FIFO policy,
  and the Blueprint-callable priority API.
- `BHNotificationPriorityPass-Smoke.log` and
  `BHNotificationPriorityPass-FirstLight.log`: main menu and First Light loaded
  and exited cleanly under the validator's strict failure scan.

Manual multiplayer playtesting remains required to judge notification dwell
time, text wrapping, repeated-event density, legibility during combat, and host
versus client presentation with representative rendered HUD assets.

## Server-confirmed armored-hit feedback

Rifle hit confirmation now distinguishes actual enemy armor mitigation from a
normal body impact. The authoritative hit-zone calculation identifies helmet
or torso-armor protection only when that equipment materially reduces damage,
then sends the result to the owning shooter through an unreliable presentation
RPC. The native hit marker retains the established lethal and headshot cues and
adds a cyan square for protected impacts; the extra shape keeps the information
available without relying on color alone. Legacy Blueprint `ShowHitMarker`
calls remain unchanged, while `ShowDetailedHitMarker` exposes the new cue.

Automated evidence:

- `BHArmoredHitFeedback-Build.log`: UE 5.8 editor target compiled.
- `BHArmoredHitFeedback-Tests.log`: 48 succeeded, 0 failed, queue empty;
  `BrokenHorizon.Combat.Feedback.ArmoredHit` verifies helmet/torso mitigation,
  exposed-limb and non-mitigating exclusions, plus legacy and detailed
  Blueprint API availability.
- `BHArmoredHitFeedback-Smoke.log` and
  `BHArmoredHitFeedback-FirstLight.log`: main menu and First Light loaded and
  exited without strict project failure markers.

Manual gameplay review remains required for marker size, duration, cyan
legibility under every accessibility palette, rapid-fire readability, and
host/client perceived latency in a rendered multiplayer session.

## Player suppression pressure and recovery

Enemy near misses now create bounded gameplay pressure instead of presentation
only. The authoritative character accumulates suppression with diminishing
returns, applies it to the existing weapon-spread multiplier, and recovers at a
predictable rate when fire stops. Default tuning caps the accuracy multiplier at
1.45x; the system does not lock input, move the player's aim, hide information,
or alter damage. The owning client mirrors the same pressure for presentation,
and the native HUD reports `SUPPRESSED - ACCURACY` with an explicit percentage
in addition to the existing directional near-miss cue. Death and field
redeployment clear both authoritative and local pressure.

Automated evidence:

- `BHPlayerSuppressionVerified-Build.log`: UE 5.8 editor target compiled.
- `BHPlayerSuppressionVerified-Tests.log`: 49 succeeded, 0 failed, queue empty;
  `BrokenHorizon.Combat.Suppression.PlayerPressure` verifies accumulation,
  diminishing stacks, decay, clamping, underflow protection, and the
  Blueprint-callable HUD contract.
- `BHPlayerSuppressionVerified-Smoke.log` and
  `BHPlayerSuppressionVerified-FirstLight.log`: main menu and First Light loaded
  and exited without strict project failure markers.

Manual gameplay review remains required for suppression intensity and recovery
pacing, automatic-fire readability, crouch/prone tradeoffs, accessibility at
HUD scale extremes, and perceived host/client accuracy under latency.

## Rendered shared squad-ping readability

The canonical rendered multiplayer fixture now saves a synchronized 1280x720
image from both clients after each applies the same authoritative hostile ping.
The first images revealed that the coordination marker occupied the strategic
briefing header. The marker now uses a reserved upper-center band, retaining its
direction chevron, `HOSTILE` context, 14 m distance, and `HOST_FIXTURE` issuer
without covering critical briefing text.

Actor-backed hostile pings now retain a replicated live target. Each owning
client traces from its own view: clear line of sight updates the marker and adds
`TRACKED`; cover freezes the last visible position and adds `LAST KNOWN`, so
enemy movement cannot leak through geometry. The server still owns the hit
classification and accepts only replicated actors.

`BHMovingPingOcclusionNetFinal2-NetworkScale-20260801-213010-Summary.json`
passed four-client combat density, active-operation replication, tracked ping
replication/presentation, reconnect convergence, and network budgets. The
isolated fixture measured 48,504 B/s aggregate outbound p95, 12,349 B/s maximum
per-client outbound, and 41 channels. The harness now creates combat density
before slow client startup and isolates active-operation runs from player save
progress.

`BHPingLaneFinal2-RenderedMultiplayer-20260801-213945-Summary.json` passed
two-client D3D12 capture, 19 observed AI, streaming, GPU-memory, and frame/GPU
budgets after moving the squad marker into a dedicated lower coordination lane.
Both 1280x720 images were inspected directly: the red incoming-threat chevron
and amber 56-57 m `LAST KNOWN` marker are fully separated, while the label stays
clear of the objective panel and reticle. Client A/B frame p95 measured
11.660/11.915 ms and GPU p95 measured 9.170/9.139 ms.

`BHPingLaneRegressionFinal-*` passed the UE5.8 editor build, all 66 tests,
startup, and the four-objective First Light route at 8/12 bounded navigation
fallbacks after the final lane polish. Color perception, physical-controller
conflict, moving-target feel, and latency remain live playtest gates.

## Multiplayer strategic-control authority clarity

Difficulty and tactical-plan controls no longer fail silently outside their
valid authority window. Connected clients see `[F6/F7] HOST CONTROLS` before
input and receive `COMMAND LOCKED // HOST AUTHORITY REQUIRED` when they try to
change campaign policy. Once an operation is committed, the footer identifies
both controls as locked and the authoritative host receives
`COMMAND LOCKED // ACTIVE OPERATION IN PROGRESS`. Unexpected subsystem
rejections report that difficulty or the tactical plan remained unchanged.
The war subsystem exposes the same mutation-authority result through a
Blueprint-pure contract so authored UI can remain consistent with native UI.

Automated evidence:

- `BHStrategicControlClarity-Build.log`: UE 5.8 editor target compiled.
- `BHStrategicControlClarity-Tests.log`: 50 succeeded, 0 failed, queue empty;
  `BrokenHorizon.Multiplayer.UI.StrategicControlClarity` verifies authority
  precedence, active-operation locks, the unlocked-host path, and the
  Blueprint-pure authority contract.
- `BHStrategicControlClarity-Smoke.log` and
  `BHStrategicControlClarity-FirstLight.log`: main menu and First Light loaded
  and exited without strict project failure markers.

Manual two-client rendered review remains required for footer fit, feedback
dwell time, keyboard discoverability, and synchronization of the selected plan
after the host changes it while a client has the map open.

## Field-squad combat-readiness feedback

The field-squad HUD now exposes the gameplay readiness state that already
drives operative spread and burst timing. The owning commander sees average
`COHESION` plus the `LOWEST` individual readiness beside the current Follow,
Hold, or mounted order. The weakest combat-effective operative escalates the
panel border through amber and red thresholds, so casualty shock is visible
before the squad's degraded fire becomes confusing. Incapacitated operatives
remain represented by the existing MEDEVAC count rather than distorting the
combat-effective average. The existing Blueprint-facing field-squad status API
was preserved; authored HUDs receive readiness through a separate callable.

Automated evidence:

- `BHSquadReadinessHUD2-Build.log`: UE 5.8 editor target compiled.
- `BHSquadReadinessHUD2-Tests.log`: 51 succeeded, 0 failed, queue empty;
  `BrokenHorizon.Gameplay.UI.FieldSquadReadiness` verifies readiness text,
  bounds, weakest-member precedence, and Blueprint-callable availability.
- `BHSquadReadinessHUD2-Smoke.log` and
  `BHSquadReadinessHUD2-FirstLight.log`: main menu and First Light loaded and
  entered play without strict project failure markers.

Manual rendered gameplay review remains required for the expanded panel's fit
at 75 and 150 percent HUD scale, readiness-color legibility in every palette,
casualty-shock pacing, and owner-only visibility in a two-client session.

## Independent look accessibility controls

Players now have independently persisted horizontal and vertical look
sensitivity, an ADS sensitivity multiplier, and vertical-look inversion. The
settings subsystem migrates legacy profiles into schema 3 without discarding
the old mouse, audio, display, graphics, or accessibility preferences. Live
character look input reads the profile and composes axis scaling, aiming, and
inversion deterministically.

The current `WBP_Settings` binary asset exposes only its legacy mouse slider.
To make the new settings player-facing without hand-editing that asset, the
native settings widget creates a right-anchored `LOOK CONTROLS` panel when the
four optional authored controls are absent. Authored controls using the
established widget names take precedence automatically.

Automated evidence:

- `BHIndependentLookSettings-Focused2.log`: the focused contract passed,
  covering independent axes, ADS plus inversion composition, Blueprint API
  availability, defaults, and the `SaveGame` flag on all four fields.
- `BHLookSettings-UIValidation2.log`: every current HUD/menu Blueprint asset
  loaded and the updated native UI contract passed with zero errors/warnings.
- `BHIndependentLookSettings-Retry-Tests.log`: 60 succeeded, 0 failed, queue
  empty. The matching asset-readiness, startup, and First Light gates passed.

Static 720p/1080p/4K settings placement is now covered by the canonical rendered
gate. Keyboard and gamepad focus order, visible value comprehension, inversion
behavior on a physical controller, and subjective hip-fire versus ADS
sensitivity feel remain manual review gates.

## Camera and visual comfort controls

Settings schema 6 adds independent camera-shake, recoil-motion, head-bob, and
damage-flash intensity controls, plus switches for motion blur, depth of field,
and chromatic aberration. The settings overlay creates native controls even
when the older binary settings Blueprint has not yet been rebuilt. Gameplay
consumers read the live subsystem values and post-process switches are applied
at game-setting priority.

Automated evidence is `BHVisualComfort-FocusedFinal.log` (settings contract),
`BHVisualComfort-UIValidation.log` (all UI contract fragments and assets), and
`BHVisualComfort-Final-*` (build, 60 automation tests, asset audit, startup, and
First Light). A rendered playtest remains required to judge motion comfort and
slider ergonomics.

## Native subtitle presentation

The local player now receives a native subtitle layer with a Blueprint-callable
mission/radio API. Settings schema 7 controls subtitle enablement, speaker
labels, direction glyphs, 75-200% text sizing, background opacity, and safe
width. Presentation is timed and network-neutral.

`BHSubtitles-Focused.log` passed schema, persistence, API, and direction-glyph
contracts. `BHSubtitles-UIValidation.log` passed the UI/source/asset validator.
`BHSubtitles-Final-*` passed build, all 60 tests, asset readiness, startup, and
First Light. Voice-asset authoring and rendered legibility remain manual.

Mission objectives now carry optional activation/completion radio text,
speaker, duration, and direction metadata. Local players queue those lines at
mission start and objective transitions. `BHObjectiveRadioSubtitles-Final-*`
passed the expanded 61-test suite; actual voice assets and final localized copy
remain content-authoring work.

## Global safe frame and difficulty assists

The saved UI safe-area value now applies to every widget registered through the
shared style system, including menus, gameplay HUD, overlays, and alerts. A
runtime-only canvas wrapper preserves authored binary trees and clamps the
visible frame to 80-100%; HUD scale remains independently adjustable.

Difficulty assistance is host-authoritative through Recruit and Custom campaign
profiles. The six custom axes affect incoming damage, perception, coordination,
medical pressure, strategic pressure, and checkpoint frequency and already have
replication, persistence, and consumer coverage.

`BHSafeArea-Focused.log`, `BHSafeArea-UIValidation.log`, and
`BHSafeArea-Final-*` passed, including all 60 project tests. The later canonical
rendered gate covers static 720p and 4K HUD layout at minimum and maximum scale
profiles; physical-input and couch-distance review remain required.

## Full keyboard and controller remapping

Settings schema 4 persists keyboard/mouse and controller overrides separately
for 27 stable gameplay binding IDs. The set includes movement/look axes,
posture, weapon controls, interaction, medical actions, war map, fireteam
orders, context action, and squad ping. Conflicts cannot silently orphan an
action: the capture UI swaps staged conflicts, and atomic application rejects
invalid devices or duplicate final keys.

The character duplicates the authored mapping context at runtime, preserving
its Enhanced Input modifiers and triggers while replacing keys from the local
profile. Controller mappings and native-only squad/gameplay actions are added
to that transient context. A settings change broadcasts locally and rebuilds
the mapping context immediately; no replicated gameplay or shared campaign
state is involved.

Automated evidence:

- `BHInputRemapping-Focused.log`: focused schema, persistence flags, stable IDs,
  unique defaults, device classification, action coverage, and Blueprint batch
  API passed.
- `BHInputRemapping-Final-Tests.log`: 60 succeeded, 0 failed, queue empty.
- `BHInputRemapping-Final-FirstLight.log`: First Light entered play and logged
  `BH_INPUT_BINDINGS_APPLIED mappings=43` without strict failures.
- `BHInputRemapping-UIValidation.log`: every current HUD/menu asset loaded and
  the remapping overlay source contract passed with zero errors/warnings.

Manual review remains required for scroll-row readability at supported
resolutions, mouse/gamepad focus navigation, physical analog-axis capture,
button-glyph comprehension, and a complete interactive rebind sweep.

### Dynamic remapped gameplay prompts

Gameplay prompts no longer advertise stale default keys after remapping.
`UBHUserSettingsSubsystem` now owns one keyboard/controller prompt formatter
and exposes it to Blueprint UI. Combat medical/grenade counts, bleeding action,
fireteam follow/hold status, squad-command waypoints, operative stabilization,
and every standard `IBHInteractable` prompt resolve the saved binding IDs at
display time. Legacy `[F]`, `[C]`, `[X]`, `[H]`, `[J]`, `[G]`, and `[M]` text
is migrated through the same formatter, preserving current content while
removing its hard-coded-control behavior.

Controller names use compact platform-neutral labels such as `FACE TOP`,
`L SHOULDER`, `R STICK`, and `DPAD UP`. Automation verifies dual-device,
single-device, compact-label, legacy-migration, and Blueprint contracts.
`BHDynamicInputPromptsRenderedFinal-RenderedMultiplayer-20260801-215148-*`
passed two-client D3D12, 19-AI, frame/GPU, memory, and streaming gates. Direct
inspection of both 1280x720 captures confirms `H / L SHOULDER`, `J / R STICK`,
and `G / R SHOULDER` remain fully visible under simultaneous injury/bleeding
pressure. Client A/B frame p95 was 11.807/11.765 ms and GPU p95 was
9.164/9.117 ms.

`BHDynamicInputPromptsUIFinal2-RenderedUI-20260802-052750-Summary.json` passed
the complete 33-capture matrix: seven gameplay/strategic states at
720p/1080p/4K, HUD/safe-area extremes, and four session states at 720p/4K.
The 720p strategic map was inspected directly and shows `[M / VIEW] CLOSE MAP`
fully inside the footer while fixed map-only shortcuts remain unchanged.

`BHDynamicInputPromptsComplete-*` passed the UE5.8 editor build, all 66 tests,
startup, and the four-objective First Light route at 8/12 bounded navigation
fallbacks. Physical controller glyph preference, last-input-device switching,
and a complete interactive rebind sweep remain human review gates.

## Network-safe menu behavior

Opening pause, settings, or the remapping overlay no longer requests a global
world pause in a network session. Listen-server hosts and clients receive local
UI focus plus local move/look suppression while authoritative simulation keeps
running. Standalone play retains conventional world pause behavior.

Automated evidence:

- `BHNetworkSafeMenu-Focused.log`: standalone pauses; listen server, dedicated
  server, and client modes explicitly do not.
- `BHNetworkSafeMenu-Final-Tests.log`: 60 succeeded, 0 failed, queue empty.
- Matching startup and First Light logs passed strict smoke validation.

Manual two-client review remains required to watch remote movement, AI,
objective timers, and persistent-war time continue while each peer opens and
closes settings and the remapping overlay.

## Toggle and hold input modes

Aim, sprint, crouch, prone, lean, and interaction now expose persistent local
activation preferences. Existing players retain familiar behavior through the
schema-5 migration: aim/sprint/crouch/lean remain hold actions, prone remains a
toggle, and interaction remains tap-to-use unless changed.

Toggle and hold paths share deterministic press/release handlers for keyboard
and controller mappings. Hold-to-interact requires a completed 0.35-second hold
and never commits on cancellation or early release. The six checkboxes live at
the top of the scrollable Controls overlay and participate in defaults, cancel,
load, and Apply behavior.

Automated evidence:

- `BHToggleHoldModes-Focused.log`: schema defaults, persistence flags,
  transition truth table, interaction threshold, and Blueprint API passed.
- `BHToggleHoldModes-UIValidation.log`: all six controls and current interface
  assets passed with zero errors/warnings.
- `BHToggleHoldModes-Final-Tests.log`: 60 succeeded, 0 failed, queue empty;
  matching asset, startup, and First Light gates passed.

Manual input-feel review remains required for animation transitions, rapid
toggle changes, blocked prone exit feedback, sustained lean behavior, and a
visible hold-interaction progress treatment.

## Rendered HUD, briefing, and pause-menu review

Deterministic non-shipping review modes now capture the live First Light HUD,
strategic briefing, and pause menu through the production widget classes. The
briefing uses a viewport-relative width, centered canvas placement, automatic
wrapping, and an outlined font. Production-length copy fits without clipping at
1280x720 and 1920x1080.

The first pause capture exposed an active briefing layered through the modal
menu. Opening the pause menu now temporarily collapses objective notifications
without discarding their content or timers, and restores them on resume. A
full-screen charcoal backdrop keeps the four pause actions readable over both
bright exteriors and dark geometry.

Rendered evidence:

- `Saved/Reports/BHRenderedUI-BRIEFING-720p.png`
- `Saved/Reports/BHRenderedUI-BRIEFING-1080p.png`
- `Saved/Reports/BHRenderedUI-PAUSE-720p-Dimmed.png`
- `Saved/Reports/BHRenderedUI-PAUSE-1080p.png`
- `BHRenderedUI-Briefing-720p-Contrast.log`,
  `BHRenderedUI-BRIEFING-1080p.log`, and
  `BHRenderedUI-PAUSE-1080p-ForceRes.log` record the requested modes and exact
  screenshot dimensions with no fatal/assertion/unhandled marker.

This closes static 720p/1080p/4K placement and contrast review for the core HUD,
briefing, and pause states. It does not replace interactive controller-focus,
settings/remapping, safe-area extremes, couch-distance, localization, or
multiplayer menu-behavior review.

The review is now a repeatable release gate:

```powershell
Validate-BrokenHorizon.cmd -RenderedUI -LogPrefix <prefix>
```

`Scripts/Test-BrokenHorizonRenderedUI.ps1` launches nine isolated offscreen
D3D12 sessions covering HUD, briefing, and pause at all three review
resolutions. It rejects nonzero exits, fatal/assertion/unhandled and package
load markers, missing fixture/screenshot markers, stale captures, mismatched
Unreal render sizes, and mismatched PNG IHDR dimensions. Evidence is stored as
nine immutable run-ID images/logs plus a machine-readable summary.

The first 4K review exposed that HUD mode could overlap the automatic startup
briefing. The non-shipping fixture now collapses notification widgets only for
the isolated HUD case, preventing a notification-heavy frame from certifying
base-HUD placement. `BHRenderedUI4KFinal-RenderedUI-20260801-163705-Summary.json`
passed the canonical wrapper end to end across all nine captures, alongside a
clean editor build, all 65 automation tests, startup smoke, and First Light
smoke. Its proof flags deliberately claim rendered static-layout evidence, not
interactive-input or multiplayer evidence.

## Responsive settings and input-remapping presentation

Rendered review exposed the legacy settings asset placing look controls beyond
the usable 720p frame and allowing the remapping overlay to collide with both
settings and gameplay HUD layers. The native settings class now composes a
responsive, full-frame shell around the existing bound controls. Settings use
a scrollable hierarchy, persistent Remap Controls access, and an always-visible
Apply / Restore Defaults / Back action bar. Existing Blueprint controls retain
their bindings and authored controls still take precedence over native
fallbacks.

Input remapping is now a separate modal layer with wrapped instructions, a
safe-frame-relative viewport, independent scrolling, and persistent Reset
Bindings / Back to Settings actions. Opening it hides the underlying settings
shell; closing it restores that shell. Non-shipping review entry points traverse
the same pause -> settings -> remapping production flow without exposing a
shipping test API.

The canonical `-RenderedUI` gate now covers five states at three resolutions:
HUD, briefing, pause, settings, and remapping at 1280x720, 1920x1080, and
3840x2160. `BHResponsiveSettingsFinal2-RenderedUI-20260801-165332-Summary.json`
passed all 15 captures. `BHResponsiveRemapLayer-Build.log` compiled the final
modal-layer correction; `BHResponsiveSettingsFinal2-Tests.log` passed all 65
tests; `BHResponsiveSettingsFinal2-UIValidation.log` passed the responsive
source and current-asset contract; and matching startup/First Light smoke logs
loaded their intended maps.

This proves static placement and legibility, including scroll/action-bar
availability. Physical mouse/gamepad focus order, scrolling feel, key capture,
conflict recovery, Apply/Cancel behavior, and couch-distance readability remain
interactive review gates.

## HUD scale and safe-area extremes

HUD scaling now applies by UI context: gameplay and overlay groups honor the
75-150% preference, while full-screen menu shells remain at authored scale.
Canvas-anchored HUD groups scale inward around their screen-edge anchors, so
health, stamina, objectives, medical status, and ammunition remain visible at
150%. Reapplying 100% explicitly resets previously scaled groups during live
settings changes. Non-shipping `-BHTestHUDScale` and
`-BHTestUISafeAreaScale` overrides exercise this behavior without modifying the
saved player profile.

The canonical rendered gate now contains 19 captures: its existing five states
at 720p, 1080p, and 4K, plus HUD profiles `HUD75-SAFE80` and
`HUD150-SAFE100` at 720p and 4K. The direct UE5.8 editor build succeeded;
`BHUIScaleExtremesFinal3-Tests.log` passed all 65 tests with an empty queue; and
`BHUIScaleExtremesFinal5-RenderedUI-20260801-171726-Summary.json` passed all 19
captures with matching startup and First Light smoke logs. Static placement at
the supported scale extremes is therefore covered. Physical controller focus,
localized text expansion, multiplayer interaction, and couch-distance
readability remain manual gates.

## Rendered multiplayer main-menu states

The native fallback for legacy `WBP_MainMenu` assets now assigns explicit canvas
geometry to the inserted Join Campaign button and places session feedback in an
anchored, padded, high-contrast panel. Status copy wraps at a bounded width, so
long recovery guidance no longer collapses into a narrow strip at the upper-left
screen edge. Pending states continue to disable campaign and settings actions;
color remains redundant with the explicit state heading.

A non-shipping main-menu fixture drives four representative states through the
production widget update path: `SESSION_READY`, `SESSION_SEARCHING`,
`SESSION_CONNECTED`, and `SESSION_ERROR`. Each is captured at 1280x720 and
3840x2160 on `L_MainMenu`. Together with the 19 existing gameplay/settings
captures, `BHSessionRenderedFinal-RenderedUI-20260801-173048-Summary.json`
passed all 27 images. `BHSessionRenderedFinal-Tests.log` passed all 65 tests with
an empty queue; startup and First Light smokes loaded their intended maps. Static
host/join clarity is covered; physical focus/navigation and live online-session
behavior remain interactive gates.

## Live First Light briefing and exposure review

A visible 1280x720 First Light playtest exposed two presentation defects that
the static source checks did not catch: the long strategic briefing inherited
an oversized asset font and extended beyond the bottom of the viewport, while
UE5.8 reported that the measured -8.3 EV scene exposure was just outside the
default cached-Lumen-lighting range. Long-form notifications now cap typography
at 20 px below 900p (24 px at larger viewports) and tighten line spacing while
short combat notifications retain their authored sizing. The project cached
lighting pre-exposure is now 3 EV, giving First Light a safe range that includes
the observed exposure without suppressing renderer warnings.

`BHBriefingPresentationRetest.log` produced a valid 1280x720
`BHRenderedUI-BRIEFING.png` with the complete title and briefing inside the
frame and no cached-lighting, fatal, assertion, or unhandled-exception marker.
The canonical `Validate-BrokenHorizon.cmd -Build -Tests -Smoke -FirstLight`
gate also passed. A second visible session showed the exposure warning absent.
`BHBriefingResponsiveFinal2-20260801-180737-Summary.json` then passed the full
27-capture rendered matrix: HUD, briefing, pause, settings, and remapping at
720p/1080p/4K; HUD/safe-area extremes at 720p/4K; and four main-menu session
states at 720p/4K.
The graybox's final lighting/material look and extended traversal readability
remain art-direction and manual gameplay gates.

## Strategic war-map and custom-difficulty rendered review

The canonical rendered fixture now opens the production war map in both the
default campaign state and a deliberately asymmetric six-axis custom difficulty
profile. An initial 1280x720 review exposed the long campaign header drawing
under the recent-event panel. When that panel is present, the header now uses a
compact strategic summary while retaining the selected custom axis and value;
the logistics line remains separate and readable.

`BHStrategicUIFinal-RenderedUI-20260801-183138-Summary.json` passed all 33
captures. WAR_MAP and CUSTOM_DIFFICULTY are covered at 1280x720, 1920x1080,
and 3840x2160 alongside the prior gameplay, settings, scale/safe-area, and
session states. Visual spot checks of the 720p and 4K custom captures confirmed
that the campaign log, strategic summary, custom DAMAGE 0.85x value, logistics,
sector cards, and command footer do not overlap or clip. The same canonical run
passed the 65-test queue, startup smoke, and First Light navigation-grenade
smoke with four fallbacks against the allowed maximum of eight. Physical
controller navigation and campaign-scale subjective balance remain manual
gates.

## Multiplayer battlefield-loot HUD endpoint

The dedicated First Light acceptance route now traces an enemy ammunition drop
through the complete UI path: server interaction, owner-only weapon replication,
the `OnAmmoChanged` delegate, `ABHCharacter::HandleAmmoChanged`, and the bound
`UBHAmmoHUDWidget` text block. `G1-AmmoHUDReplication-20260801-194127-*`
recorded Client A as the rewarded owner, owner reserve 180, and bound HUD text
`30 / 180`. Client B received the pickup's consumed world state without Client
A's private ammo values. The same run retained both-client consumption,
late-join two-drop availability, mission completion, and rejoin inheritance.
`BHAmmoHUDReplicationFinal-*` passed the editor build, all 66 tests, startup,
and canonical First Light. NullRHI proves widget binding/data flow, while
visible pickup animation, typography under combat motion, sound, and dwell feel
remain rendered/manual review items.

## Rendered multiplayer battlefield-loot HUD

The existing D3D12 rendered-multiplayer harness now has an optional production
route mode. It keeps two real 1280x720 clients connected to dedicated authority,
runs the authored First Light route, and requests a screenshot only from the
client whose bound ammo widget receives the enemy-drop reward.

`BHRenderedLootHUD-20260801-194607-Summary.json` passed with two rendered
clients, 19 replicated combat actors, production-route and battlefield-loot HUD
proof, and Client B identified as the rewarded owner. Its 1280x720 capture
shows the gold `30 / 180` counter fully inside the lower-right safe area with no
clipping or collision with the medical panel, objective list, center
notification, weapon, or viewport edge. Both RTX 5060 Ti client profiles passed:
frame p95 was 11.976/12.028 ms, GPU p95 9.196/9.237 ms, no frame exceeded 50 ms,
texture desired-data p05/final was 100%, pending stream-in p95 was zero, and
peak physical GPU-memory usage was 37.9%.

The fixture advances objectives every half-second, so the captured center
notification reflects the intentionally compressed queue rather than normal
player pacing. `BHRenderedLootHUDRegression-*` subsequently passed all 66
tests, startup, and canonical First Light. Moving-combat occlusion, pickup
animation/audio, normal notification cadence, and physical-input feel remain
manual review gates.

## Packaged OpenWorld pause and 1080p safe-frame review

The 2 August packaged playtest exposed four issues that headless checks could
not prove: Escape did not reach the pause action after viewport capture, the
health values collided with the strategic panel, the objective card was too
large and clipped its line, and legacy top-left menu text could escape the safe
frame. Runtime pause binding now includes the native fallback mapping, while
the objective, strategic, and menu layouts enforce compact 1080p-safe bounds.

`BHOpenWorldUI1080Final3-HUD.png`, `BHOpenWorldUI1080Final2-PAUSE.png`, and
`BHOpenWorldUI1080Final2-MAIN_MENU.png` are rendered 1920x1080 evidence. A fresh
Development package at `Builds/OpenWorld-Development-20260802-UIFix` then passed
the player-real route: Host New Campaign, gameplay mouse capture, weapon fire,
Escape pause with visible cursor, and second-Escape resume. Its log recorded
`BH_INPUT_BINDINGS_APPLIED mappings=43`, pause open with `cursor=1` and
`world_paused=0` for the listen host, and return to game-only input. The same
revision passed build, all 66 tests, startup, First Light, and a packaged
OpenWorld startup/exit smoke in `BHOpenWorldUIComplete-*`.

The prior no-nav ambient route is a bounded local fallback rather than a world
startup failure; this visible run reached `BH_AMBIENT_WAR_READY` without the
fallback. The `UCrowdManager` Recast warning occurred only after viewport-close
`RequestExit`, so it remains classified as teardown-only pending any evidence
of a warning during active play.

## Deterministic menu focus and back navigation

The main menu, pause menu, settings screen, and remapping overlay now place
keyboard/controller focus on an actionable visible control after each modal
transition. Main menu starts on Host New Campaign, pause starts on Resume,
settings starts on mouse sensitivity, and remapping starts on its visible
Toggle Aim control. Returning from settings or remapping restores focus to the
button that opened that layer rather than leaving focus on a collapsed widget.

Settings also handles Escape and the virtual gamepad Back action directly:
the first back closes remapping, while the next closes settings without
applying. `BHInteractiveMenuFocusFinal.log` and direct 1920x1080 observation
verified arrow-only main-menu navigation, Enter activation, Escape from
settings, restored Settings-button activation, remapping close, and visible
focus restoration. `BHMenuFocusRemappingRendered.log` records pause, settings,
and `toggle_aim` focus markers; `BHMenuFocus-REMAPPING-1920x1080.png` is the
rendered overlay evidence. `BHMenuFocusComplete-*` passed the editor build,
all 66 tests, startup, and First Light before the final visible-focus selection
adjustment; the final source revision then compiled and passed the targeted
rendered remapping runtime with no strict failure marker. Physical gamepad
hardware and platform-specific glyph preference remain separate review gates.

## Device-aware input prompt preference

Settings schema 8 now persists four prompt modes: Auto (Last Input), Keyboard
+ Mouse, Controller, and Show Both. Auto follows the last meaningful local
gameplay input; controller-axis detection uses a dead-zone threshold so ordinary
stick drift does not churn the HUD. Explicit modes remain stable, and a missing
preferred-device binding falls back to the available binding instead of showing
an empty prompt.

`BHInputPromptMode-SETTINGS-1920x1080.png` visibly verifies the new setting at
1080p. `BHInputPromptMode-GAMEPAD-HUD-1920x1080.png` shows controller-only
medical and grenade labels, while `BHInputPromptMode-BOTH-HUD-1920x1080.png`
shows the paired keyboard/controller labels without clipping. The focused
settings contract passed, then `BHInputPromptModeComplete-*` passed the editor
build, all 66 tests, startup smoke, and First Light with 8/12 bounded navigation
fallbacks. A physical-controller pass for live Auto switching and final
platform-branded glyph artwork remain manual production gates.

## Runtime localization pipeline

`Config/Localization/Game.ini` now defines the shippable `Game` target for
runtime C++/config text plus Broken Horizon UI and mission assets. Unreal's
gather pipeline generates the English manifest, archive, portable-object file,
metadata, and runtime `Game.locres` under `Content/Localization/Game`.

The initial gather exposed two namespace/key collisions where the war map and
ambient director reused `FriendlySectorControl` and `EnemySectorControl` for
different source strings. The war-map keys are now unique, preventing culture
data from resolving to whichever source happened to gather first.

`Validate-BrokenHorizon.cmd -Localization` regenerates the target, rejects text
conflicts and missing/empty artifacts, and enforces a non-trivial word catalog.
`BHLocalizationComplete-Localization.log` passed with 3,539 English words and
zero conflicts. The same revision built, passed all 66 tests, started cleanly,
and completed the First Light fixture with 8/12 bounded navigation fallbacks.
Professional translated cultures and pseudo-localized rendered layout review
remain release-content gates; the extraction, conflict detection, and runtime
resource pipeline are now established.

## Pseudo-localized responsive UI

`Validate-BrokenHorizon.cmd -RenderedPseudoLocalization` now runs a focused
ten-capture LEET matrix over Settings, Remapping, War Map, multiplayer ready,
and long session-error states at 1280x720 and 1920x1080. Every capture proves
the requested pseudo-culture, fixture marker, renderer resolution, and output
dimensions; rendered evidence is then inspected because those checks alone do
not prove readable layout.

The first visual pass exposed raw settings option strings, a session-error card
whose expanded text escaped its backing at 720p, and untranslated/overwide War
Map labels. Settings combo values now use stable localization keys, the session
card has expansion-safe height, and the War Map's title, faction/site labels,
logistics/supply/route states, campaign-log heading, legend, and empty state are
gatherable. Compact strategic card lines are measured against their actual
card width and elide at the boundary instead of drawing into adjacent sectors.

`BHPseudoLocalizationFix-RenderedPseudoLocalization-20260802-072841-*` passed
all ten renders. Direct inspection confirms Settings, Remapping, ready/error
session states, and the localized War Map labels remain inside their panels;
`BHPseudoLocalization-WAR_MAP-FIT-1280x720.png` is the focused narrow-layout
proof after text fitting. `BHPseudoLocalizationComplete-*` then passed the
editor build, all 66 tests, a 3,609-word zero-conflict gather, startup, and
First Light with 8/12 bounded navigation fallbacks. Professional cultures and
the War Map's remaining raw numerical/tactical format sentences still require
conversion and translated-layout review.

### War Map formatted-text completion

The LEET matrix now covers fourteen captures: Settings, Remapping, strategic
War Map, deployment planning, custom difficulty, multiplayer ready, and long
session error at both 1280x720 and 1920x1080. Deployment and custom review
fixtures exercise their real lower tactical layers even after the deterministic
campaign fixture reaches victory.

Campaign headings, sector telemetry, force and supply previews, operation
readiness, transport ETA/range, player loadout risks, command footers, custom
difficulty controls, and the remaining deployment fallbacks now use stable
localized text and culture-aware numeric formatting. Direct inspection of
`BHWarMapLocalizationVisualFinal-RenderedPseudoLocalization-20260802-080143-*`
confirms the expanded tactical lines and custom footer remain visible at 720p
and 1080p. `BHWarMapLocalizationComplete2-*` passed all 66 tests, produced a
4,072-word catalog with zero conflicts, passed startup, and completed First
Light with 8/12 bounded navigation fallbacks. Professional translations and
translated-culture linguistic review remain open.
