# Operation First Light graybox automation

Run `Content/Python/build_first_light_graybox.py` from **Tools > Execute Python
Script**. It creates the new map:

`/Game/BrokenHorizon/Maps/L_FirstLight_Graybox`

It refuses to overwrite an existing map by default and never opens or edits
`L_Prototype`. All generated actors carry the `BH_Auto_FirstLight` tag and are
organized into `FirstLight/...` World Outliner folders.

Generated gameplay wiring:

- Keycard: `RedKeycard`, persistence `FirstLightRedKeycard`
- Door: locked, requires `RedKeycard`, persistence `FirstLightSecurityDoor`
- Guard death objective: `EliminateGuard`
- Extraction prerequisite: `EliminateGuard`
- Extraction objective: `ReachExtraction`
- Supplies: `FirstLightAmmoSupply`, `FirstLightMedicalSupply`
- Three patrol points are assigned to the guard.

The First Light guard uses `/Game/Characters/BP_EnemySoldier`, the same
Blueprint verified in `L_Prototype`. To repair an older generated map that
uses the temporary child guard Blueprint, run
`Content/Python/repair_first_light_guard.py` from **Tools > Execute Python
Script**. It replaces only the tagged `FL_Guard` and preserves its transform
and patrol route.

After generation, run `Content/Python/validate_first_light_graybox.py` using
the same menu. It is read-only, prints actor counts plus the objective IDs to
Output Log, and fails if the keycard, locked door, guard death objective, or
extraction objective does not match the canonical First Light contract.

## Required final editor review

1. The generator attempts to build navigation automatically. If Output Log
   reports a navigation-build failure, run
   `Content/Python/repair_first_light_navigation.py` from **Tools > Execute
   Python Script**, or press **P** (**Build > Build Paths**) with the map open.
2. Verify the NavMeshBoundsVolume covers every floor/walkway the guard needs.
3. In World Settings, confirm `BP_BHGameMode` is selected.
4. Ensure your mission data asset has this exact ordered sequence:
   `FindRedKeycard`, `UnlockSecurityDoor`, `EliminateGuard`, `ReachExtraction`.
5. Play in Editor: keycard → locked door → guard → extraction. If an asset was
   customized after the map script was written, inspect the generated actor
   details once before playtesting.

To intentionally rebuild only this map's previously generated content, set
`REBUILD_EXISTING = True` inside the generator and run it while the map exists.
It deletes only actors marked `BH_Auto_FirstLight`; it never replaces the map
asset or touches untagged user actors.
