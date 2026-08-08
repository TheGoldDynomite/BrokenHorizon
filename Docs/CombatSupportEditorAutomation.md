# Combat-support Editor automation

`Content/Python/setup_combat_support.py` is an editor-only UE 5.8 setup helper.
It is safe to run repeatedly: it reuses assets, never deletes assets, does not
open or save maps, and does not change Blueprint graphs or widget layouts.

## Run it

1. Restart Unreal Editor once after pulling this change so the project enables
   **Python Editor Script Plugin** and **Editor Scripting Utilities**.
2. Open **Tools > Execute Python Script**.
3. Select `Content/Python/setup_combat_support.py`.
4. Read the `[BH Combat Support Setup]` messages in the Output Log.

The script creates or reuses:

- `/Game/BrokenHorizon/Core/BP_AmmoSupply` (child of `BHAmmoSupply`)
- `/Game/BrokenHorizon/Core/BP_MedicalSupply` (child of `BHMedicalSupply`)
- `/Game/BrokenHorizon/UI/WBP_CombatStatus` (only if it was missing)

It applies sensible supply defaults (30 reserve ammo, 35 healing, consume on
use, interaction text) and attempts to assign Engine's basic cube as the
placeholder mesh. It also assigns `WBP_CombatStatus` to the known
`/Game/BrokenHorizon/Characters/MyBHCharacter` Blueprint's **Combat Status
Widget Class** when the property is available.

## Small manual widget step

The script deliberately never edits an existing UMG layout. Open
`WBP_CombatStatus` and add:

- `HealthBar` — Progress Bar (required)
- `StaminaBar` — Progress Bar (required)
- `HealthText` — Text Block (optional)
- `StaminaText` — Text Block (optional)

Compile and save the widget. Override `OnPlayerDamaged` only if you want a
cosmetic hit effect.

## Optional demo placement

The normal script run never edits a map. To deliberately place one ammo and
one medical supply near the first `PlayerStart` in the currently open map,
open **Output Log** and run:

```python
import setup_combat_support
setup_combat_support.place_demo_supplies()
```

It tags both actors `BH_AutoCombatSupportDemo`, gives each a generated unique
`Persistence ID`, and refuses to place another pair in a map that already has
that tag. Adjust their positions and save the map yourself. For hand-placed
supplies, set a unique instance-only Persistence ID such as `AmmoSupply_01` or
`MedicalSupply_01`.
