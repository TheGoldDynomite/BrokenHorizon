# Weapon Heat Gameplay

Sustained fire now heats each weapon and makes fire discipline a practical combat skill. Heat is authoritative, replicated, and tuned by weapon role instead of being a purely visual effect.

## Player-visible behavior

- Every shot adds role-specific heat; support weapons tolerate longer bursts while precision weapons reward deliberate pacing.
- Rising heat progressively increases dispersion and recoil before the weapon reaches its hard limit.
- At maximum heat, firing locks until the weapon cools below a safe recovery threshold. This hysteresis prevents rapid lock/unlock flicker at the limit.
- Cooling begins after a short firing delay and continues automatically while the weapon is held.
- The ammunition HUD displays weapon role, magazine and reserve ammunition, and current heat with a clear `OVERHEATED` warning.
- Heat resets when the authoritative weapon role changes or the character respawns.

## Multiplayer and tuning contracts

- The server owns heat accumulation, cooling, overheat state, and shot acceptance.
- Normalized heat and the overheat flag replicate to clients and drive Blueprint-facing feedback events.
- Heat penalties multiply existing movement, posture, suppression, injury, fatigue, readiness, and battlefield-condition handling penalties.
- Role profiles own heat-per-shot and cooling-rate values alongside their existing ammunition, cadence, damage, spread, and recoil characteristics.

## Manual playtest gates

- Verify burst length, recovery time, and warning readability for assault, marksman, and support roles.
- Confirm remote clients see heat and overheat transitions without distracting HUD jitter under latency and packet loss.
- Review recoil and spread at low, medium, and high heat using representative mouse and controller settings.
- Pair the source-driven state with final barrel heat shimmer, smoke, material response, sound, and animation assets before presentation sign-off.
- Confirm cooling balance supports suppression without enabling uninterrupted automatic fire.
