# Broken Horizon agent guide

## Project

- Use Unreal Engine 5.8 from `C:\Program Files\Epic Games\UE_5.8`.
- Treat `BrokenHorizon.uproject` as the project entry point.
- Keep runtime code in `Source/BrokenHorizon` and editor-only code in `Source/BrokenHorizonEditor`.
- Follow the existing `BH` naming convention and preserve Blueprint-facing compatibility.

## Safety

- Preserve unrelated working-tree changes and inspect Git status before editing.
- Never hand-edit, replace, or delete `.uasset` or `.umap` files.
- Treat reflected names, asset paths, objective IDs, and persistence IDs as content/save contracts.
- Give persistent world actors unique stable IDs.
- Keep editor Python automation idempotent, tagged, and narrowly scoped.
- Do not clear `Binaries`, `Intermediate`, `Saved`, `DerivedDataCache`, or packaged builds without explicit authorization.

## Validation

- Run `Validate-BrokenHorizon.cmd -Build -Tests -Smoke` after meaningful C++ gameplay changes.
- Use `-FirstLight` for mission, AI, navigation, combat, save, or presentation changes.
- Use `-Packaged` for startup, cooking, packaging, or release-facing changes.
- Read the generated log summary; process exit alone is not sufficient evidence.
- Require manual editor/playtesting review for visuals, UMG layout, animation, navigation coverage, Blueprint wiring, and interaction feel.

## Delivery

- Lead with the player-visible result.
- State exactly what compiled, launched, and passed.
- Identify remaining manual review.
- Do not commit, push, package, or publish unless requested.
