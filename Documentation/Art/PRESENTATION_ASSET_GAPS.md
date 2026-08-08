# Operation First Light — Presentation Asset Audit

Audit date: 2026-07-24. This is an asset-readiness audit, not an instruction to replace the working graybox. Epic sample/template content remains reference or placeholder content and is not a dependency target for Broken Horizon gameplay code.

## Current presentation foundation

| Area | What exists | Current status |
|---|---|---|
| Player | `ABHCharacter`, `BP_BHCharacter`, first-person camera, movement, stamina, health, death, interaction, weapon input | Gameplay complete. Native optional `FirstPersonArms` skeletal mesh hook now attaches to the camera; no mesh is required. |
| Rifle | `ABHRifle`, `UBHWeaponComponent`, `BP_Rifle`, camera-origin hitscan, muzzle obstruction trace, ammo/reload/ADS/recoil, gunfire hearing | Gameplay complete. Static and optional skeletal weapon mesh components, socket/fallback muzzle transform, optional montage/audio/Niagara/decal/camera-shake fields, and Blueprint cosmetic events are available. |
| Enemy | `ABHEnemySoldier`, `BP_EnemySoldier`, health/death, patrol/perception/combat, muzzle fallback, objective completion | Gameplay complete. Inherited skeletal mesh/AnimBP support plus optional hit/fire/death events, fire montage, sound, Niagara muzzle effect, and socket/fallback muzzle transform are available. |
| Animation | UE mannequin locomotion, rifle, hit-react, and death sample assets under `/Game/Characters/Mannequins`; first-person and Variant sample animation content | A project-owned copy of the self-contained third-person rifle AnimBP now drives the Quinn enemy placeholder after the unarmed AnimBP remained visually idle. A dedicated first-person arms set and final authored enemy combat animation remain gaps. |
| Weapon meshes | Rifle static/skeletal prototype assets under `/Game/Weapons/Rifle` | A project-owned static rifle copy is now assigned to `BP_Rifle` with the existing proven camera-relative placement and aligned `MuzzlePoint` fallback. It remains prototype art; a persistent mesh socket and animated magazine/bolt presentation still need a compatible skeletal weapon and arms set. |
| UI | Native-backed objective, notification, ammo, combat status, interaction, death, pause, settings, main-menu, and mission-complete widgets | Functional. Layout, type hierarchy, icons, reticles, animation, accessibility, and visual consistency remain a later UI pass. |
| Audio | A small amount of Epic template weapon audio exists outside Broken Horizon-specific folders | No cohesive First Light sound library. Current gameplay remains safe when all new sound fields are empty. |
| VFX | A few Epic Variant effects exist outside Broken Horizon-specific folders | No project-owned muzzle, impact, weather, atmospheric, or interaction VFX set. Current gameplay remains safe when all new Niagara/decal fields are empty. |
| Environment | `L_FirstLight_Graybox`, collision-enabled Engine cubes, gameplay actors, routes, patrol, extraction, nav bounds | Functionally verified graybox. No final modular facility, forest, road, fence, prop, decal, wetness, or vista kit. |
| Lighting | Generated Directional Light and Sky Light in the graybox | Visibility baseline only. `apply_first_light_lighting.py` provides an opt-in, tagged cold-dawn baseline without touching geometry or custom lights. |

## Asset categories still needed

### First-person player

- **Arms skeletal mesh:** dedicated first-person arms or a correctly cropped/hidden-body UE5-compatible mesh. Import as Skeletal Mesh from FBX; use centimeter scale, Z-up conversion through Unreal import, and a stable skeleton.
- **Arms skeleton and AnimBP:** one project-owned skeleton, an Animation Blueprint for idle/locomotion sway, and montages for fire and reload. Montages must target the same skeleton as the assigned arms mesh and use a slot exposed by its AnimGraph.
- **Optional aim presentation:** an AnimBP variable or the rifle Blueprint event `On Aim Presentation Changed`; no ADS montage is required by code.
- **Camera shake:** one subtle `CameraShakeBase` Blueprint for firing. Keep it additive to the existing controller recoil.

### Rifle

- **Weapon mesh:** static mesh for a non-animated weapon or skeletal mesh if bolt/magazine motion is required. FBX is preferred for skeletal import; static FBX or glTF is acceptable for a static prototype.
- **Orientation and scale:** Unreal forward is +X, dimensions are centimeters, and the mesh origin should support stable camera attachment.
- **Muzzle socket:** socket name `Muzzle` by default. Put it at the bore exit with +X pointing down the barrel. If absent, gameplay and effects use the native `MuzzlePoint` component.
- **Animation:** weapon animation can be driven in `BP_Rifle` through `On Fire Presentation`, `On Reload Presentation`, and `On Aim Presentation Changed`. First-person montages assigned on the rifle must match the arms skeleton.
- **Materials/textures:** physically plausible metal/polymer/paint, restrained roughness variation, readable values in low light, and scalable texture sizes.

### Enemy soldier

- **Skeletal mesh:** UE5 Manny skeleton compatibility is the simplest path, or retarget a licensed soldier to a project-owned skeleton. Provide a physics asset for editor correctness even though gameplay does not require ragdoll.
- **Animation Blueprint:** idle/walk/run, direction/aim offset, and combat state presentation driven by normal Character velocity and controller rotation.
- **Montages:** fire, optional hit reaction, and optional death animation. Each must match the enemy mesh skeleton and use a configured AnimGraph slot.
- **Muzzle socket:** `Muzzle` on the weapon/character skeletal hierarchy when available. The native `MuzzlePoint` remains the safe fallback.
- **Cosmetics:** implement `On Enemy Fire Cosmetics`, `On Enemy Hit Cosmetics`, and the existing `On Enemy Death Cosmetics` in the enemy Blueprint. Do not put damage, AI state, or objective completion into those events.

### Weapon VFX and decals

- **Muzzle flash:** Niagara System sized for first-person use and a separate scalable system or user parameter for enemy/world presentation.
- **Impact:** one generic Niagara impact first, followed later by physical-material-specific concrete, metal, wood, and dirt effects.
- **Decal:** deferred-decal material with transparent edges, modest size, short lifetime, and a strict on-screen budget.
- **Format:** use Niagara System assets, not required Blueprint actors. Texture sources should normally be lossless PNG/TGA/EXR as appropriate.

### Audio

- **Required set:** player fire, dry fire, reload mechanical layer, enemy/world fire, concrete/metal/wood/dirt impacts, keycard, locked/unlocked door, checkpoint, supplies, injury/death, objective update, and restrained facility ambience.
- **Format:** import clean WAV sources, commonly 48 kHz and 16- or 24-bit. Prefer mono sources for spatial one-shots and stereo only for non-spatial UI/ambience where appropriate.
- **Implementation:** assign `SoundBase` assets directly or use Sound Cues/MetaSounds. Keep master routing through the existing configurable Sound Class/Sound Mix.

### First Light environment

- Modular weathered-concrete walls, floors, ceilings, door frames, steps, and barriers
- Steel fencing, gates, railings, poles, antennas, dishes, cable runs, conduit, vents, and rooftop equipment
- Administration/security furniture and communications consoles
- Road/asphalt, mud, drainage, curb, wetness, puddle, leak, moss, rust, dirt, and signage materials/decals
- Conifer trees, understory, rocks, terrain materials, and low-cost distant mountain/vista treatment
- Burnt-orange practical security fixtures with non-shadowing fill variants
- Neutral fictional safety/security markings; no real faction insignia

## Recommended integration order

1. Apply and tune the cold-dawn lighting baseline on a reviewed copy/state of First Light.
2. Establish an environment texel-density/material/trim-sheet standard, then replace the largest graybox silhouettes.
3. Integrate first-person arms, rifle mesh/socket, and fire/reload/ADS presentation.
4. Integrate the enemy mesh, AnimBP, muzzle socket, and combat/death cosmetic events.
5. Add weapon audio, muzzle flashes, impacts, and bounded decals.
6. Add wetness, signs, props, forest, and distant vistas while protecting gameplay readability.
7. Style and animate the functional UI.
8. Profile packaged builds; tune LODs, shadows, fog, reflections, Niagara counts, texture groups, and scalability.

## Null-safe integration rule

Every new mesh, montage, audio, Niagara, decal, and camera-shake field is optional. Empty fields must remain valid. Blueprint cosmetic events may animate or play presentation, but must never become responsible for firing, damage, ammo, AI decisions, death state, objectives, saving, or respawning.
