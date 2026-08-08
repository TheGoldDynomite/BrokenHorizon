# Cooperative Supply Sharing

Active teammates can aim at one another and hold the normal interaction input to redistribute ammunition and critical supplies without returning to a base.

## Transfer rules

- The server re-traces and validates the target, distance, player state, and authority.
- Ammunition transfers only between matching weapon roles, representing compatible magazines and cartridges.
- A transfer moves at most 30 reserve rounds and always leaves the donor a 30-round emergency reserve.
- One fragmentation grenade or engineering charge transfers when the donor has more than one and the receiver has capacity.
- One medkit or field dressing transfers when the donor has more than one and the receiver has a genuine shortfall.
- A single interaction can form a useful field bundle from every compatible shortage.
- If nothing can be transferred, the donor receives a clear explanation rather than losing equipment.

Both players receive a transfer summary. All changed inventories use their existing owner-replication paths, force a prompt network update, and immediately recalculate carried kilograms, endurance, speed, noise, and weapon-handling burden.

## Tactical value

- Support gunners can supply another support gunner while retaining emergency ammunition.
- Engineers can redistribute charges before splitting into breach and security elements.
- A medic or prepared rifleman can reinforce a teammate who has exhausted casualty-care supplies.
- The squad can deliberately shift weight onto a security player before an infiltration or casualty evacuation.

## Manual multiplayer gates

- Test listen-server and remote-client transfers in both directions.
- Confirm interaction trace and prompt reliability while players crouch, lean, and move.
- Confirm feedback reaches both owning clients and does not obscure urgent combat warnings.
- Test mismatched weapon roles, full receivers, emergency-reserve protection, incapacity, and out-of-range rejection.
- Evaluate whether a hold interaction is sufficiently resistant to accidental transfers under fire.
