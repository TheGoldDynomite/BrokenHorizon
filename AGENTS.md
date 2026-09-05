# Broken Horizon — Codex Operating Contract

## Mission

Act as the engineering team for **Broken Horizon**, an Unreal Engine 5.8 first-person military-simulation project built with C++ and Blueprint. Preserve working behavior, finish one testable vertical slice at a time, and validate every meaningful source change.

The user prefers plain language, exact actions, and complete files when manual copy/paste is genuinely required.

## Autonomy and approval boundary

For requests to change, build, fix, test, or review the local project, make the requested in-scope local edits and run relevant non-destructive checks without repeatedly asking for permission.

Require approval before:

- deleting caches or user content;
- changing engine association or plugin versions;
- adding external dependencies;
- moving or renaming binary Unreal assets;
- broad refactors unrelated to the request;
- commits, pushes, pull requests, or other external writes unless explicitly requested.

Never hand-edit `.uasset` or `.umap` files.

## Project facts: provisional until verified

- Intended engine: Unreal Engine 5.8 on Windows.
- Likely primary module: `BrokenHorizon`; confirm from the descriptor and targets.
- Historically working systems are believed to include:
  - `ABHCharacter` first-person movement, camera, jump, crouch, interaction trace, and UI hooks;
  - `IBHInteractable` / `UBHInteractable`;
  - `ABHDoor` open/close and lock/keycard behavior;
  - interaction prompt UI;
  - `UBHObjectiveComponent`, objective HUD, and objective notification UI.
- A historical editor crash came from calling an interface `Execute_*` method on an actor that did not implement `UBHInteractable`.
- The project should remain outside OneDrive-controlled folders.

Repository evidence overrides this list. Put verified results in `docs/PROJECT_STATE.md`.

## Fast startup

Before editing:

1. Read `docs/PROJECT_STATE.md`, `docs/DECISIONS.md`, and the relevant section of `docs/ARCHITECTURE.md`.
2. Inspect the `.uproject`, target files, and module `Build.cs` only when the task requires them.
3. Inspect `git status` and the current diff. Never overwrite unrelated work.
4. Search narrowly for the actual symbols and files involved; do not read the entire repository by default.
5. State the implementation boundary and validation command before writing.

On the first project session, or after an environment change, run:

```powershell
.\Tools\ProjectDoctor.ps1
```

## Choose the smallest workflow

### Routine, local, or documentation-only work

Handle directly or use `bh_routine_worker`. Do not spawn architecture agents for mechanical edits.

### Focused gameplay feature or refactor

When ownership or more than one system is involved, run `bh_code_explorer` and `bh_architect` in parallel. Wait for both, then use exactly one writer (`bh_cpp_implementer`). Use `bh_reviewer` after behavior, public APIs, lifecycle, or module dependencies change.

### Build, UHT, linker, startup, or crash failure

Use `bh_code_explorer` to isolate the execution path when needed, then make `bh_build_debugger` the only writer. Fix the first meaningful error rather than downstream noise.

### Tests

Use `bh_test_engineer` after the production API and expected behavior are understood. Tests must match the real project rather than invented signatures.

Read-only agents may work in parallel. Never run overlapping write-capable agents on the same files.

## Scope and file boundaries

Normal editable source:

- `Source/`
- `Config/`
- source-controlled plugin source
- `Tools/`
- `docs/`
- project-level text configuration

Generated output is not source:

- `Binaries/`
- `DerivedDataCache/`
- `Intermediate/`
- `Saved/`
- `.vs/`
- generated IDE solutions and databases

Do not clean, regenerate, or delete generated output as a generic fix. Use evidence.

More specific C++ rules are in `Source/AGENTS.md`. Documentation rules are in `docs/AGENTS.md`. Tooling rules are in `Tools/AGENTS.md`.

## Productivity rules

- Implement one vertical slice at a time.
- Prefer incremental builds. Do not perform clean builds unless stale generated state is proven.
- Keep plans short and executable.
- Avoid unrelated formatting, file moves, and speculative systems.
- Put durable gameplay state and reusable rules in C++; keep Blueprint focused on assets, animation, audio, effects, layout, and tuning.
- Prefer events, delegates, timers, and cached references over unnecessary `Tick` work.
- When editor work is unavoidable, record exact steps in `docs/EDITOR_HANDOFF.md`.
- Update project documents only when verified behavior, ownership, decisions, or milestones change.

## Validation ladder

Use the cheapest check that proves the current step, then run the full gate before completion.

1. Text/config-only: inspect diff and run the relevant script syntax or consistency check.
2. C++ compile-sized step:

   ```powershell
   .\Tools\BuildEditor.ps1
   ```

3. Completed feature or bug fix:

   ```powershell
   .\Tools\Validate.ps1
   ```

   The validation gate also runs `ReviewChanges.ps1` to catch whitespace/conflict markers and generated-output files.

4. Release candidate or handoff package:

   ```powershell
   .\Tools\PackageDevelopment.ps1
   ```

Use `Validate.ps1 -RequireTests` once Broken Horizon automation tests exist and are expected to run.

## Definition of done

A source task is complete only when:

- the requested behavior is implemented without silent scope expansion;
- the editor target builds, or the exact pre-existing blocker is documented;
- relevant tests pass when available;
- the final diff contains no unrelated changes;
- required Unreal Editor work is written with exact assets, panels, values, connections, compile/save steps, and PIE expectations;
- `docs/PROJECT_STATE.md` and other durable documents reflect verified changes.

Use one completion status:

- **Verified:** build/test/reproduction passed.
- **Code-complete, editor validation needed:** source passed available checks but a visual or asset step remains.
- **Blocked:** exact blocker and evidence are recorded.

## Final handoff

Report:

- behavior delivered;
- exact files changed;
- validation commands and results;
- Unreal Editor handoff steps;
- remaining risks or unverified behavior;
- next single vertical slice.

## Standing GitHub delivery - 2026-09-05

The user explicitly authorizes automatic commit and push of every completed,
validated game update or fix to the active development branch and its upstream.
This standing permission supersedes earlier requirements to ask before each
commit or push; do not request repeated approval for this authorized delivery.

- Stage only the completed task's scoped changes and required dependencies;
  preserve unrelated work in progress.
- Write meaningful commits that describe the change, validation performed, and
  any remaining manual Editor/PIE evidence.
- Do not force push, merge into the default branch, publish releases, or include
  generated packages, caches, saves, or credentials unless specifically authorized.
