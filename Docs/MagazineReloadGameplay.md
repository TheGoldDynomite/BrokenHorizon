# Magazine-Aware Reload Gameplay

Reloading now creates an explicit combat decision instead of treating every reload identically.

## Controls and behavior

- Press reload once for a tactical reload. The current partial magazine is retained and its cartridges remain in the player's total carried ammunition.
- Double-tap reload within 0.35 seconds to convert to an emergency reload.
- Emergency reloads take 65% of the weapon role's normal reload time but immediately discard every round remaining in the inserted magazine.
- An empty weapon automatically uses the faster emergency procedure without losing ammunition.
- Assault, marksman, and support weapons retain their authored role-specific base timing.

The existing ammunition model stores current-magazine rounds separately from all carried spare/retained magazine rounds. A tactical reload conserves the combined total; an emergency reload removes the inserted magazine's remaining rounds before drawing the replacement.

## Feedback and multiplayer

- Tactical feedback reports how many rounds were retained.
- Emergency feedback reports exactly how many rounds were dropped.
- Magazine and reserve counts update immediately, including carried-weight consequences.
- Reload type replicates so remote presentation uses the same shortened or normal procedural motion.
- Reliable reload RPC ordering allows a rapid second press to convert a server-side tactical reload already in progress.

## Manual playtest gates

- Validate double-tap recognition at low and high frame rates and under client latency.
- Confirm tactical and emergency procedural motions match their actual authoritative completion times.
- Confirm sound timing remains convincing when a tactical reload converts to emergency.
- Test each weapon role with full, partial, one-round, and empty magazines.
- Check notification readability and ensure repeated reload input cannot produce excessive message noise.
- Confirm ammunition loss feels consequential without making the emergency option unusable.
