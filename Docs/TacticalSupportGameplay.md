# Tactical support gameplay

Broken Horizon now connects cooperative target marking to persistent-war
logistics through physical field-support relays.

## Player loop

1. Observe a target and place the normal shared squad ping.
2. Return to or coordinate with a teammate at a friendly sector relay.
3. Choose the dedicated smoke or mortar console and interact.
4. The server validates sector control, target age, relay range, supply, and
   cooldown before committing the request.

Every sector has one smoke relay and one mortar relay. Smoke costs 6 sector
supply and is limited to 1.5 km from its relay. Mortar costs 10 sector supply
and is limited to 5 km. Both relays require a current shared ping and have a
45-second recharge period. The logistics expenditure is checkpointed as soon
as the call is accepted.

## Realism model

- Smoke lasts 25 gameplay seconds and blocks AI sight whenever the observer's
  sight line crosses the nine-meter smoke volume. It does not deal damage or
  apply an arbitrary suppression statistic.
- Mortar fire has a 2.5-second warning/flight delay and a deterministic
  three-round adjustment pattern rather than landing instantly at one point.
- The 6.5-meter beaten zone applies radial falloff damage to any exposed actor,
  including the requester, friendly players, and friendly operatives. Players
  already inside or near the grid receive a danger-close warning before impact,
  so careless targeting has consequences without becoming invisible punishment.
- Calls are server-authoritative. Relay type, active support type, and cooldown
  replicate for reconnect-safe multiplayer presentation.

Real-world smoke duration and mortar range are compressed to fit the current
regional gameplay scale. Their relationships, failure modes, delays, and
logistics constraints remain intentionally credible.

## Manual gates

- Verify the smoke volume is readable without looking like a solid wall.
- Confirm AI genuinely loses and reacquires targets across the smoke boundary.
- Tune mortar warning time, dispersion, lethality, and friendly-fire clarity.
- Run a two-client test where one player marks and another operates the relay.
- Review relay placement, cover, and navigation at all six sectors.
