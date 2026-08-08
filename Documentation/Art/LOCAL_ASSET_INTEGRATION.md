# Operation First Light — Local Asset Integration

Integration date: 2026-07-24.

Current-state correction (2026-08-01): the assignment described below is
historical. `BP_Rifle` currently renders the owner-only
`/Game/InfimaGames/FreeFPSTemplate/Art/AssaultRifle/Meshes/SK_AssaultRifle`
skeletal mesh; its legacy static-mesh component is unassigned. The canonical
asset-readiness audit includes that active dependency namespace.

This pass used only assets already present in the Broken Horizon project. No
asset was downloaded, purchased, or referenced from outside `/Game`.

## Integrated assets

### Player rifle

- Source: `/Game/Weapons/Rifle/Meshes/SM_Rifle`
- Project-owned copy:
  `/Game/BrokenHorizon/Presentation/Weapons/SM_FirstLight_Rifle`
- Assigned to: `/Game/BP_Rifle`, component `RifleMesh`
- Camera-relative transform: location `(50, 15, -15)`, rotation
  `(Pitch 0, Yaw -90, Roll 0)`, scale `(1, 1, 1)`
- `SM_Rifle` has no persistent socket. `BP_Rifle` retains its verified
  `MuzzlePoint` at camera-relative `(100, 15, -15)`, approximately at the
  barrel exit after the mesh rotation. `ABHRifle` uses that component as its
  null-safe gameplay/effect fallback.
- The skeletal weapon component remains unassigned. No incompatible weapon or
  arms animation was forced onto it.

The rifle's original material and texture dependencies remain ordinary
project assets under `/Game/Weapons/Rifle`. The Blueprint holds a hard
reference to the presentation copy so the rifle and its dependencies cook
with the game.

### Enemy placeholder

- Source mesh:
  `/Game/Characters/Mannequins/Meshes/SKM_Quinn_Simple`
- Source AnimBP:
  `/Game/Variant_Shooter/Anims/ABP_TP_Rifle`
- Project-owned mesh copy:
  `/Game/BrokenHorizon/Presentation/Characters/SKM_FirstLight_EnemyPlaceholder`
- Project-owned AnimBP copy:
  `/Game/BrokenHorizon/Presentation/Characters/ABP_FirstLight_EnemyRifle`
- Assigned to:
  `/Game/Characters/BP_EnemySoldier`, component `CharacterMesh0`
- Mesh transform: location `(-10, 0, -80)`, rotation
  `(Pitch 0, Yaw -90, Roll 0)`, scale `(1, 1, 1)`

The mesh and AnimBP share `/Game/Characters/Mannequins/Meshes/SK_Mannequin`.
The rifle AnimBP was selected after the original unarmed placeholder remained
visually idle during AI movement. Its serialized dependencies are limited to
Engine animation classes and local mannequin animation assets; the duplicated
project-owned asset has no dependency on Variant Shooter gameplay classes.
AI controller, auto-possession, collision, health, firing, objective, and
death settings were not changed.

## Intentionally not integrated

No dedicated first-person arms-only skeletal mesh exists in the project or
the installed UE 5.8 templates. The available Manny and Quinn assets are full
bodies. The template first-person setup attaches its camera to a full-body
character and uses copy-pose animation, while Broken Horizon's
`FirstPersonArms` component is camera-attached. Assigning that full body to
`FirstPersonArms` would cause clipping and an incompatible animation setup.

The next arms asset should provide:

- an arms-only first-person skeletal mesh at Unreal centimeter scale;
- a stable skeleton and matching idle, fire, and reload animations;
- an AnimBP with a montage slot used by the optional rifle montages;
- hands posed for a rifle and enough upper-arm geometry for the current FOV;
- no collision, owner-only visibility, and disabled dynamic shadows.

No Variant Shooter gameplay class, feature action, or source module is used
by this integration. The self-contained third-person rifle AnimBP was
duplicated into Broken Horizon's presentation namespace; the original sample
asset is not assigned to the enemy Blueprint.

## Automation

Run `Content/Python/integrate_first_light_presentation.py` from
**Tools → Execute Python Script**. It defaults to a dry run:

- `APPLY_CHANGES = False`
- `REPLACE_CUSTOM_ASSIGNMENTS = False`
- `REPAIR_GENERATED_ALIGNMENT = False`

It reuses existing presentation assets, never deletes assets or modifies maps,
and refuses to replace a custom Blueprint assignment unless the explicit
replacement flag is enabled. Keep the editor closed when applying it from the
command line, or save and close the affected Blueprints before running it
interactively.

UE 5.8's Python static-mesh socket edit did not serialize reliably during
commandlet testing, so the script deliberately does not create a fake
`Muzzle` socket. Add one manually later only if a replacement presentation
asset needs socket-driven effects; gameplay already uses `MuzzlePoint`.

`Content/Python/inspect_presentation_assets.py` is read-only and prints the
relevant assets, skeletons, sockets, Blueprint component assignments, and
transforms to the Output Log.

`Content/Python/validate_presentation_assets.py` runs UE's installed asset
validators only against the active presentation assets and the two Blueprints
changed by this pass. This targeted check avoids unrelated validation defects
inside the retained Epic Variant Shooter sample content.
