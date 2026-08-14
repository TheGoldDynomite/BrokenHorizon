# Weapon Foundation Vertical Slice

```text
Implement the first playable weapon foundation vertical slice for Broken Horizon, but only after verifying that equivalent code does not already exist.

Goal:
The player can own/equip one basic hitscan weapon, fire once per valid request, consume magazine ammunition, and reload from reserve ammunition. The design must leave clean extension points for AI, presentation, recoil, attachments, and networking without implementing those systems yet.

In scope:
- Focused reusable weapon ownership, preferably a component when it fits the verified architecture.
- Base weapon state and configuration.
- CanFire, Fire, CanReload, and Reload behavior.
- Camera/aim-based hitscan trace using the project's real camera/input flow.
- Structured hit result or event.
- Intent forwarding from ABHCharacter without placing the complete weapon system there.
- Deliberate Blueprint presentation events for fire, reload, ammo change, and hit confirmation.
- Deterministic ammo/reload tests.
- Documentation and exact Editor handoff.

Out of scope:
- Final animations, sounds, VFX, meshes, recoil, sway, attachments, inventory, switching, projectile weapons, AI combat, enemy reactions, save/load, and multiplayer replication.

Constraints:
- Preserve movement, interaction, doors, keycards, prompts, and objectives.
- Do not invent asset or input-action names.
- Avoid Tick unless continuous behavior is proven necessary.
- Build after each coherent source step and run Tools/Validate.ps1 before completion.

Done when:
- The editor target builds.
- Ammo and reload tests pass.
- The diff contains no unrelated changes.
- docs/EDITOR_HANDOFF.md has exact asset/input/widget/PIE steps.
- The final report states what is verified versus still visual/manual.
```
