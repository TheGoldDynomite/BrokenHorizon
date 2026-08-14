# Broken Horizon Source Rules

These instructions apply to C++, target files, module rules, and source-controlled plugin code below `Source/`.

## Unreal reflection and lifetime

- Keep every `*.generated.h` include last in its reflected header include block.
- Use `UCLASS`, `USTRUCT`, `UENUM`, `UFUNCTION`, and `UPROPERTY` deliberately and with valid Unreal Header Tool syntax.
- Prefer forward declarations in headers and concrete includes in `.cpp` files when safe.
- Add direct module dependencies to the correct `Build.cs`; do not depend on accidental transitive includes.
- Use `TObjectPtr` for reflected UObject members where it fits the existing project style.
- Respect UObject/Actor/component lifetime, garbage collection, ownership, delegate cleanup, and world teardown.
- Validate pointers and world state before use.
- Keep gameplay work on the game thread unless the design explicitly supports safe asynchronous work.

## Broken Horizon safety invariants

- Before any `IBHInteractable` `Execute_*` call, verify the target is valid and implements `UBHInteractable`.
- Preserve working movement, interaction, door, keycard, prompt, and objective behavior unless the task explicitly changes them.
- Avoid placing unrelated systems directly in `ABHCharacter`. Prefer focused reusable Actor Components when ownership supports them.
- Do not create networking behavior accidentally. Replication requires an explicit authority, ownership, prediction, and validation design.
- Avoid renaming Blueprint-facing properties, functions, classes, or categories without a migration reason and handoff.

## Performance and architecture

- Avoid `Tick` unless the behavior truly requires continuous work.
- Prefer state changes, delegates, timers, bounded traces, cached references, and subsystem/component ownership.
- Keep C++ authoritative for durable state and gameplay rules; expose presentation hooks to Blueprint.
- Do not build speculative inventory, networking, attachment, AI, save, or animation systems inside a focused feature.
- Reuse existing project patterns unless they are the source of a verified defect.

## Tests

- Inspect existing test placement and module conventions before creating new tests.
- Prefer deterministic automation tests that do not depend on editor order, the current map, timing races, or final art assets.
- Tests must leave disk and global state as they found it.
- Prioritize state transitions and safety invariants before visual behavior.
- Do not add production-only public APIs solely for testing unless the seam is small, generally useful, and documented.
- Relevant initial candidates, after API inspection, include interaction interface guards, locked/unlocked door behavior, keycard acceptance, objective transitions, ammunition transfer, and reload invariants.

## Compile cadence

After a coherent compile-sized edit, run:

```powershell
.\Tools\BuildEditor.ps1
```

Before declaring a feature complete, run:

```powershell
.\Tools\Validate.ps1
```

Fix the first meaningful UHT/compiler/linker error. Do not delete caches as a first response.
