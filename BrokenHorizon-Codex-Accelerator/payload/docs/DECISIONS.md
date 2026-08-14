# Broken Horizon — Durable Decisions

Record new decisions with date, status, rationale, consequences, and affected systems.

## D-001 — Engine and platform

- **Status:** Accepted; verify descriptor.
- **Decision:** Target Unreal Engine 5.8.0 on Windows.
- **Consequence:** Build and tooling scripts must resolve a compatible UE 5.8 installation rather than hardcoding one machine path.

## D-002 — C++ and Blueprint split

- **Status:** Accepted.
- **Decision:** Keep durable gameplay state, rules, and reusable architecture in C++; use Blueprint for asset assignment, animation, audio, VFX, layout, and tuning.
- **Consequence:** C++ exposes deliberate presentation hooks while Blueprint remains thin.

## D-003 — Binary asset safety

- **Status:** Accepted.
- **Decision:** Never hand-edit `.uasset` or `.umap` files.
- **Consequence:** Binary changes require Unreal Editor automation that is actually available or exact manual handoff steps.

## D-004 — Incremental validation

- **Status:** Accepted.
- **Decision:** Use incremental editor builds during development and the full validation script before completion. Clean builds are evidence-driven exceptions.
- **Consequence:** Faster iteration without routinely destroying useful generated state.

## D-005 — Agent coordination

- **Status:** Accepted.
- **Decision:** Parallelize read-only exploration; allow only one writer on overlapping files.
- **Consequence:** Faster mapping with fewer conflicting edits.

## D-006 — Interaction dispatch invariant

- **Status:** Accepted from historical crash evidence; re-verify implementation.
- **Decision:** Validate object lifetime and `UBHInteractable` implementation before every interaction `Execute_*` call.
- **Consequence:** This becomes a code-review and test target.

## D-007 — Source control

- **Status:** Accepted.
- **Decision:** Track Unreal binary assets with Git LFS, ignore generated Unreal output, and never auto-commit from setup tooling.
- **Consequence:** Local checkpoints remain intentional and reviewable.
