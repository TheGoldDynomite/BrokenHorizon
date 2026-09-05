# Broken Horizon — Consolidated PIE Playtest Checklist

Generated 2026-08-17 from `Docs/EDITOR_HANDOFF.md` (26 pending tasks) and
`Docs/ROADMAP.md`.

**Purpose:** convert every "source complete / rendered check pending" item into a
recorded pass or fail in as few editor sessions as possible.

---

## Rules for this pass

1. **Do not fix anything during the run.** Record the failure, move on. The goal
   is a complete map of what's broken, not a fix. Fixing mid-run means you get
   one item resolved and twenty still unknown.
2. **Do not resave or modify binary assets.** Every handoff task says this.
   If a map/asset change is genuinely required, note it and stop that item.
3. Record for each item: `PASS` / `FAIL` / `BLOCKED` / `N/A`, plus the exact
   on-screen text you saw and the relevant log line.
4. If Block B fails, **stop and fix before running C–E.** Everything downstream
   assumes the core route works.

**Known risk going in:** the 2026-08-15 smoke run
(`Saved/Logs/BHInteractionPromptFixFinal-FirstLight.log`) reached the door and
`pre_guard_extraction_gate`, then stopped before the guard/ammo/extraction
markers after seven `BH_AI_NAVIGATION_FALLBACK` warnings. Expect Block B item
B7 to be where this pass breaks.

---

## Block A — Pre-flight, editor only, no PIE (~20 min)

If these are wrong, every PIE result below is noise. Do these first.

**A1. Player Blueprint contract** — `Content/Characters/BP_BHCharacter`, Class Defaults

- [ ] `PlayerMappingContext` → `Content/BrokenHorizon/Input/IMC_Player`
- [ ] `MissionData` → resolves (if null, **record path and stop** — content decision, do not invent an asset)
- [ ] `ObjectiveWidgetClass` → WBP_Objective
- [ ] `ObjectiveNotificationWidgetClass` → WBP_ObjectiveNotification
- [ ] `AmmoHUDWidgetClass` → WBP_AmmoHud
- [ ] `CombatStatusWidgetClass` → WBP_CombatStatus
- [ ] `PauseMenuWidgetClass` → WBP_PauseMenu
- [ ] `InventoryWidgetClass` → verify against actual class (no WBP_Inventory asset exists — do not guess)
- [ ] `MissionCompleteWidgetClass`, `DeathWidgetClass`, `SubtitleWidgetClass` → record whatever they are
- [ ] `InteractionPromptClass` may still show legacy `MBP_InteractionPrompt` — that's OK, runtime uses the native class

**A2. Game-mode contract** — `Content/BrokenHorizon/Core/BP_BHGameMode`

- [ ] Parent class, `GameStateClass`, `PlayerControllerClass`, `DefaultPawnClass`
      resolve to `ABHGameMode` / `ABHWarGameState` / `ABrokenHorizonPlayerController` / `BP_BHCharacter`
- [ ] `UBHWorldBuilderLibrary` functions are findable from a Blueprint search
      (editor target lists only `BrokenHorizon` in ExtraModuleNames — this is the check)
- [ ] Do **not** run world-builder functions. Do not rebuild a map.

**A3. Map actor sanity** — `/Game/BrokenHorizon/Maps/L_FirstLight_Graybox`

- [ ] Keycard, security door, `FL_Guard_*` ×3, extraction actors present with stable `PersistenceID` values
- [ ] `FirstLight Mission Cache` at ~`(4900, 450, 90)`, `PersistenceID = FirstLightMissionCache01`, `MissionItemID = RedKeycard`
- [ ] `FirstLightWatercraft01` present
- [ ] `FirstLightArmoredThreat01` present
- [ ] Navigation data present; toggle nav visualization and confirm green coverage
      over the route **and** the `FL_DefenseA_FacilityPad` / six `FL_DefenseA_Garrison_*`
- [ ] Six Defense A garrisons are hidden/inert, no First Light guard objective assignment

---

## Block B — Single-client First Light core route (THE GATE)

One PIE session on `L_FirstLight_Graybox`. This is Roadmap item #1.

- [ ] **B1.** Widgets appear for local player only: objective, native interaction prompt, ammo, combat status, subtitle, notification
- [ ] **B2.** IMC_Player drives: move, look, interact, fire, aim, reload, pause, jump, crouch, sprint. *Record any missing binding by action name.*
- [ ] **B3.** Open inventory **before** pickup → shows `MISSION ITEMS 0` and `NONE`
- [ ] **B4.** Aim at `FL_RedKeycard` → center-screen native text `Press [F] to Pick Up Red Keycard`.
      Look away → collapses.
      *(Temporary dev fallback shows `INTERACTION // ...` upper-left, overlapping the objective panel. That overlap is known and not a failure.)*
      - If missing, capture: `BH_INTERACTION_PROMPT_DEFERRED`, `BH_INTERACTION_PROMPT_NATIVE_RETRY`, `BH_INTERACTION_PROMPT_READY_NATIVE`, `BH_INTERACTION_PROMPT_TEXT`
- [ ] **B5.** Late-possession variant: possess **after** map load, repeat B4. Prompt still appears.
- [ ] **B6a.** Attempt locked door **before** the card → locked prompt + `ACCESS DENIED // REQUIRED KEYCARD: RedKeycard`
- [ ] **B6b.** Collect card. Inventory (kept open if possible) updates to `MISSION ITEMS 1` / `RedKeycard`, not stale state. Log: `BH_KEYCARD_INVENTORY_REPLICATED mission_items=1`
- [ ] **B6c.** Interact with door holding card → `ACCESS GRANTED // SECURITY DOOR UNLOCKED`
- [ ] **B7.** ⚠️ **Expected break point.** After the door: three `FL_Guard_*` are active hostile targets, objective stays `EliminateGuard` until resolved. Guard damage/death presentation reads correctly.
      *Count `BH_AI_NAVIGATION_FALLBACK` warnings in the log.*
- [ ] **B8.** Ammo interaction works
- [ ] **B9.** Reach extraction → `ReachExtraction` completes, completion notification fires **exactly once**
- [ ] **B10.** All four objective IDs advanced exactly once, in order: FindRedKeycard → UnlockSecurityDoor → EliminateGuard → ReachExtraction
- [ ] **B11.** Mission-complete lockout hides the active objective widget while the result screen shows
- [ ] **B12.** End PIE → Output Log has no new Broken Horizon errors

**Log sweep after Block B:**

```
BH_INTERACTION_TRACE / BH_INTERACTION_PROMPT_TEXT / BH_KEYCARD_INVENTORY_REPLICATED
BH_AI_NAVIGATION_FALLBACK   (count them)
```

---

## Block C — Single-client presentation sweep (ride-alongs)

Same map, same session type. These are cheap once you're already in PIE and they
close nine handoff tasks. None of them gate anything.

- [ ] **C1. Mission cache.** With card in hand, aim at the cache →
      `Press [F] to STORE RedKeycard IN MISSION CACHE`. Interact →
      inventory `MISSION ITEMS 0`, cache label reads stored, log `BH_MISSION_CACHE_TRANSFER operation=store`.
      Interact again → `Press [F] to RETRIEVE RedKeycard FROM MISSION CACHE`, inventory back to 1.
      Checkpoint save + map reload → stored state survives.
      *(If `SaveProgressForCharacter` fails, transfer must roll back and show `CHECKPOINT NOT SAVED`.)*
- [ ] **C2. Live inventory refresh.** Panel open, use `DISCARD ONE FRAG` → visible `FRAG` count and carried-load update **without closing the panel**. Repeat with `DISCARD 30 AMMO`.
- [ ] **C3. Salvage prompt.** Aim at an `ABHSalvagePickup` → `Press [F] to RECOVER <type> // <quantity>`. Press once → `RECOVERED // <type> x<amount>`, inventory/HUD update, capacity-full path unchanged.
- [ ] **C4. Transport boarding prompt.** Empty land transport → `Press [F] to drive field transport`. Empty watercraft → `Press [F] to board waterborne cargo transport`. Loaded → same action **plus** `SUPPLY <amount> // DELIVER TO <destination>` (or `AID <amount> // ...`). Loaded with no route → `DESTINATION PENDING`. *Check text wrapping.*
- [ ] **C5. Transport world label.** Empty → `CARGO 0/<capacity>`. Loaded supply → `SUPPLY <n>/<cap> > EASTERN DEPOT`. Aid → `AID <n>/<cap> > <community>`. No route → `DESTINATION PENDING`. Delivered → back to empty form.
- [ ] **C6. Driver readiness panel.** Board transport. Empty → compact `FIELD TRANSPORT` + speed/fuel/hull. Loaded → third line `SUPPLY <amount> // TO <destination>` (or `AID ...`), **fuel and hull bars stay inside the panel**. No route → `DESTINATION PENDING`.
- [ ] **C7. Shotgun.** Equip `SHOTGUN`, fire one shell → **one** muzzle-flash/audio/recoil/first-person cycle, not eight. Spread and damage unchanged. One shell = one magazine round.
- [ ] **C8. Armored threat cue.** Approach `FirstLightArmoredThreat01` until line of sight → one amber/red directional chevron + readable `ARMORED CONTACT // <meters>`. Break LoS → cue clears.
- [ ] **C9. Raid sabotage prompt.** *(needs a raid with an enemy logistics cache — skip if not reachable this session)* → `Press [F] to PLANT DEMOLITION CHARGES`; after → label `DEMOLITION CHARGES ARMED`, prompt `Demolition Charges Armed`, no second-action text.

---

## Block D — Two-client listen server, First Light

New session type. Use the **non-host client** wherever the step allows it — that's
where the ownership bugs live.

- [ ] **D1. Route convergence.** Run the four-objective route. Authority-owned objective, keycard, door, and completion presentation converge for both clients.
- [ ] **D2. Keycard parity.** Collect the world card on the non-host client. The server grants the card to each player; both local inventories show the granted count and identity through their own owner-only `OwnedKeycards` replication (`ABHKeycard::Interact_Implementation`). This world-pickup grant is separate from the owner-specific cache transfers in D6. **Locked door does not unlock from a client-only local change.**
- [ ] **D3. Shared completion/debrief.** Finish the guard objective → both clients show the same completed objective state and debrief result **before either presses CONTINUE**. Neither client still shows an active `EliminateGuard` beside the result screen.
- [ ] **D4. Inventory transfers (5 controls).** Both characters within 250 cm. On the non-host, press `I` → panel exposes all five transfer buttons. Click each once:
      - [ ] `TRANSFER 30 AMMO TO ALLY` → `TRANSFERRED // ... AMMO ...` + `RECEIVED // ... AMMO ...`
      - [ ] `TRANSFER ONE SMOKE TO ALLY` → matching pair, both counts + carried load update
      - [ ] `TRANSFER ONE TOOL TO ALLY` → `... ENGINEERING ...` pair
      - [ ] `TRANSFER ONE AT TO ALLY` → `... ANTI-VEHICLE ...` pair
      - [ ] `TRANSFER FRAG TO NEAREST ALLY` → matching pair
      - [ ] Counts update **without reopening the panel**
      - [ ] Move beyond 250 cm, repeat one action → no transfer, **no stale success notification**
      - [ ] Check the three-plus-two button layout reads correctly
- [ ] **D5. Live inventory refresh, remote.** Recipient's panel stays open while the other player transfers one frag → recipient panel changes after replication. Remote pawns do not create local UI or refresh the other player's panel.
- [ ] **D6. Mission cache, two-client.** Non-host stores/retrieves. Server owns both transfers, cache state replicates, **only the intended player's owner-only keycard inventory changes**, both converge after reconnect/reload.
- [ ] **D7. Armored threat target selection.** Both clients in contact range. Keep one visible, put the other closer but behind cover → threat picks the **nearest visible** player, not the first connected one. Only the affected local HUD shows the cue. Then swap exposure → target switches, old cue clears, new cue appears once, damage applied authoritatively with no duplicate fire events.
- [ ] **D8. Shotgun, listen-server.** Fire once as host, once as non-host → both observe one shell-level presentation. No duplicated gunfire in logs.
- [ ] **D9.** End PIE → compare server and **both** client logs for RPC/network errors, save failures, duplicate actions, new warnings.

---

## Block E — Persistent war / War Map flow

Separate entry path (War Map deployment, not the First Light route). Some items
need a specific fixture state — set up what you can, mark the rest `BLOCKED`.

**E1. Deployment preview (single client, healthy fireteam)**

- [ ] War Map deployment mode → preview shows `NEXT DEPLOYMENT // EFFECTIVE`, `DOWN`, `MEDEVAC`, healthy roster in ready color
- [ ] Repeat with a downed / evac-required / service-needed operative → line changes to matching counts, amber/red state
- [ ] Read-only: deployment eligibility, supply cost, and selected operation **do not change** because the line displays

**E2. Operation debrief**

- [ ] Finish or fail an operation → result screen includes `NEXT DEPLOYMENT // EFFECTIVE` / `DOWN` / `MEDEVAC` matching the local fireteam
- [ ] With a casualty or serviced-but-not-ready member → recommendation changes to recovery/stabilization/service guidance
- [ ] Healthy roster → `FIRETEAM READY FOR NEXT DEPLOYMENT`
- [ ] Complete an ordinary First Light route → this block is **not** appended to the non-war mission-complete message
- [ ] Two-client: each player sees **its own** fireteam counts

**E3. Fireteam service waypoint**

- [ ] Damage a squad member to create a service need → owning HUD waypoint reads `RESUPPLY // FIRETEAM SERVICE` with count, points to nearest friendly station
- [ ] Use the station → service notification appears, waypoint clears or reports only remaining need
- [ ] Two-client: only the affected player's waypoint reports the count

**E4. Rescue casualty identity**

- [ ] War Map preview → `MEDEVAC // CASUALTY <ID> // TREATMENT <DESTINATION> // FOOT OR VEHICLE EXTRACTION`
- [ ] Commit → `STRATEGIC BRIEFING` repeats `ASSIGNED CASUALTY // <same ID>`, same destination, stays tied to that casualty
- [ ] Two-client: other client's War Map shows no stale casualty identity

**E5. Rescue treatment destination gate**

- [ ] At a **wrong** friendly station with casualty in service radius → ordinary service works, prompt names the committed destination, `EvacuateCasualty` does **not** complete
- [ ] At the **committed** destination → prompt names the exact casualty and treatment action
- [ ] Interact → log `BH_RESCUE_TREATMENT_COMPLETED` with matching casualty ID + destination
- [ ] Objective notification names that casualty, objective advances **exactly once**, debrief reports same casualty as evacuated
- [ ] Two-client convergence on the locked-destination rule

**E6. Defense A operation**

- [ ] Garrisons inert before activation: hidden, no damage, no movement block
- [ ] Activate Defense A variation-0 from normal War Map flow → garrisons become visible/colliding/damageable, AI reacts after director alert
- [ ] Log contains `BH_OPERATION_AUTHORED_DEFENSE_GARRISON_TRACKED`
- [ ] Kill one authored guard → `BH_OPERATION_HOSTILE_CASUALTY` / wave state advances **once**
- [ ] Host and non-host waypoints show the same operation label, wave/total, living hostiles, losses, friendly support
- [ ] During inter-wave wait: reinforcement countdown and support values converge, do **not** revert to generic phase text
- [ ] Both clients receive activation, inter-wave, and hostile-casualty **notifications** (separate from the waypoint — don't treat the waypoint as proof of delivery)
- [ ] Save/reload during inter-wave wait → living/dead set preserved, garrison stays dormant, later wave uses normal dynamic spawn

**E7. Operation failure debrief + reconnect**

- [ ] Trigger one authoritative failure (expired approach / Defense A breach / convoy escort deadline / convoy destruction)
- [ ] Server log: **one** war-result application + `BH_SHARED_OPERATION_FAILURE_PROPAGATED`, participant count matches active players
- [ ] Before CONTINUE: both clients show same failure debrief, no active route objective
- [ ] After CONTINUE: failure checkpoint persists through reconnect
- [ ] Reconnecting client without the original debrief RPC → same lockout with fallback `MISSION FAILED`, no active route objective left visible
- [ ] Escort failure specifically: `BH_SHARED_OPERATION_FAILURE_DEBRIEF` present, and **no** `BH_SHARED_OPERATION_FAILURE_CHECKPOINT_FAILED`

**E8. Convoy route choice**

- [ ] Committed EscortRescue with >1 authored `ABHWorldRoute` → prompt includes `Press [F] to reroute convoy`, `CURRENT:`, `NEXT:` with authored display names
- [ ] Press `[F]` → `CONVOY REROUTED` names the same corridor shown as `NEXT`, route marker/HUD converges

**E9. Ownership / checkpoint checks (two-client, `L_BrokenHorizon_World`)**

- [ ] **Sector resupply station:** non-host interacts → server applies resupply once, interacting client's owner-only inventory/checkpoint is the one saved, other client does **not** receive that private loadout. Test cooldown / loadout-full path.
- [ ] **Field armory:** non-host interacts at `FieldArmory_<SectorID>` → loadout notification reports new role, full ammo, sector supply, checkpoint result. **Host's role and ammo unchanged.** Reconnect → non-host keeps its chosen role.
- [ ] **Owner-scoped field checkpoints:** non-host uses checkpoint + support relay (squad-ping) + build/repair fortification → each notification correct, host's owner-only role/ammo/medical/loadout unchanged. Reconnect → non-host's saved state restored.
- [ ] **Military transport:** two-client `Resupply` op, variation B / water route, board `FirstLightWatercraft01` as non-host → delivery message fires, cargo reaches zero **exactly once**, server log has `BH_FIELD_LOGISTICS_DELIVERED`, reconnect does not restore delivered cargo.
- [ ] **Civilian aid transport:** non-host occupies transport at a friendly connected sector with an aid request → `V` control, route update + load + delivery notifications + cargo change. Server applies route and delivery once, community support changes, occupying driver's checkpoint updated, reconnect does not duplicate cargo.

---

## Results log

| Block | Item | Result | Notes / exact text seen / log line |
|---|---|---|---|
| A | A1 | | |
| A | A2 | | |
| A | A3 | | |
| B | B1–B12 | | |
| C | C1–C9 | | |
| D | D1–D9 | | |
| E | E1–E9 | | |

**After the run, update:**

- `Docs/EDITOR_HANDOFF.md` — flip each task `Status:` to the real result
- `Docs/ROADMAP.md` — item #1 exit evidence
- Then decide what to build next, based on what actually broke.
