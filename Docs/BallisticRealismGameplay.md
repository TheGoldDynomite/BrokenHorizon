# Ballistic realism gameplay

Broken Horizon's rifle fire remains server-authoritative while modeling the
combat consequences of distance, gravity, impact angle, and cover material.

## Player-visible behavior

- Projectiles accumulate gravity drop from muzzle velocity and travel distance.
- Damage energy falls from a full close-range value to a role-specific retained
  value at long range.
- Marksman weapons retain velocity and energy farther than assault weapons;
  support weapons trade terminal performance for sustained suppression.
- Concrete and structural metal stop direct fire. Shallow strikes may ricochet,
  while head-on impacts do not.
- Thin metal, timber, glass, flesh, and light untagged construction have distinct
  penetration limits and energy loss. Material that exceeds the penetration
  depth stops the projectile.
- A successful penetration or ricochet can damage a secondary actor on the
  server. Near misses continue feeding the existing AI suppression model.

## Surface contract

Physical surfaces are named in `DefaultEngine.ini`: Concrete, Dirt, Grass,
Metal, Water, Wood, Glass, and Flesh. Environment assets need an appropriate
physical material for deliberate behavior. Untagged construction uses a
conservative light-cover fallback so missing metadata is not treated like
armor plate.

## Validation

Automation verifies 500-meter drop, distance energy loss, material resistance,
penetration thickness, and glancing-versus-head-on ricochet decisions. The
non-shipping `-BHTestBallisticsRuntime` probe confirms the active rifle profile
loads these contracts in a real map.

## Manual playtest gates

- Confirm sight holds at 100, 300, and 500 meters feel learnable for each role.
- Shoot authored wood, glass, concrete, and metal assets and verify their
  physical materials are assigned correctly.
- Verify two-client hit, penetration, ricochet, hit-marker, and lethal-hit
  feedback under latency.
- Tune ricochet audio/VFX and ensure reflected rounds never look like tracers
  originating from the wrong impact point.
- Confirm suppression remains readable without excessive camera effects.
