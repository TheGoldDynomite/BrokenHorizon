# Weapon roles and field armories

Friendly OpenWorld sectors contain a field armory. Interacting spends four
sector supply, cycles to the next weapon role, refills that role's ammunition,
and saves the campaign checkpoint.

## Roles

- **Assault:** 30-round balanced rifle, selectable semi/automatic fire, mobile
  handling, and 180 rounds in reserve.
- **Marksman:** 15-round semi-automatic rifle with higher damage, longer range,
  tighter aimed accuracy, stronger recoil, and 90 rounds in reserve.
- **Support:** 60-round automatic rifle with higher fire rate and substantially
  stronger suppression, wider spread, a longer reload, and 240 reserve rounds.

The active role is replicated to other players and displayed above the ammo
count. Role, magazine ammunition, and reserve ammunition persist in campaign
schema 47.

Manual review is required for recoil feel, time-to-kill balance, armory visual
placement, two-client role visibility, animation timing, and audio suitability.
