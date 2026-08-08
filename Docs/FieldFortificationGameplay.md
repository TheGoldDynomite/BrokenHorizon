# Field fortification gameplay

OpenWorld contains two tactical fortification positions in each of its six
war sectors. The positions are intentionally fixed so construction remains
compatible with navigation, multiplayer authority, and persistent saves.

## Player loop

1. Secure friendly control of a sector.
2. Approach a gold `FORTIFICATION BUILD POSITION` marker.
3. Hold the normal interact input to spend 12 sector supply and construct it.
4. Use the completed barricade as physical and AI-recognized cover.
5. Revisit damaged barricades to repair them. Repair cost scales from zero to
   six supply based on missing integrity.
6. Destroyed barricades return to an inactive build position and can be rebuilt.

Construction, repair, damage, and destruction are server-authoritative and
replicated. Construction state, integrity, transform, sector, and stable
persistence ID are recorded in campaign schema 46.

## Editor automation

- `setup_field_fortifications.py` adds or updates only the twelve positions.
- `validate_field_fortifications.py` performs read-only count, sector, and
  persistence-ID validation.
- The broader `create_broken_horizon_world.py` remains idempotent and also
  ensures these positions when rebuilding the initial OpenWorld region.

Visual placement, navigation around the completed collision, interaction feel,
and two-client construction/destruction still require manual playtesting after
automated validation.
