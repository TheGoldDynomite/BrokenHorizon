# Carried Load and Fatigue Gameplay

Carried equipment now has a continuous mass cost derived from the player's real replicated inventory. It is not a separate saved statistic: changing weapon role, firing ammunition, throwing grenades, using medical supplies, or placing charges immediately changes the burden.

## Included mass

- Armor, helmet, water, radio, and baseline fighting equipment.
- Assault, marksman, or support weapon mass.
- Magazine and reserve ammunition, with role-appropriate round mass.
- Fragmentation grenades and engineering charges.
- Medkits and field dressings.

## Tactical consequences

Below 24 kg, the soldier retains the full fighting-load profile. Effects increase gradually between 24 and 40 kg rather than switching abruptly.

- Movement speed falls by up to 28%.
- Sprint stamina drain rises by up to 70%.
- Stamina recovery falls by up to 35%.
- Vault and mantle stamina costs rise by up to 35%.
- Equipment noise rises by up to 25%.
- Weapon dispersion rises by up to 10%.

The HUD reports total kilograms, fighting/heavy/overloaded classification, retained movement speed, and relative sprint endurance. Environmental, injury, suppression, posture, and load penalties stack because they represent separate physical constraints.

## Design intent

The support role can sustain a squad but pays for its weapon and ammunition. Engineers carrying both charges lose endurance. A lightly supplied rifleman can move and infiltrate more efficiently. Consuming or deploying supplies removes their carried mass, so logistics decisions remain meaningful after leaving an armory.

## Manual playtest gates

- Compare a light assault load with a fully supplied support load over the same sprint route.
- Verify vault/mantle feel at fighting, heavy, and overloaded states.
- Confirm the HUD line remains legible at supported UI scales and safe zones.
- Confirm the penalties encourage squad specialization without making support or engineering roles unpleasant.
- Tune kilogram assumptions against final weapon, armor, and equipment art specifications.
