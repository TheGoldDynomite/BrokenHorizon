# Broken Horizon — Project State

> Replace provisional entries with verified repository evidence during the first bootstrap session. Do not claim a baseline build until it has run.

## Identity

| Field | Current value | Evidence |
|---|---|---|
| Project | Broken Horizon | Project history; verify `.uproject` |
| Engine | Unreal Engine 5.8.0 | Provisional until descriptor/installation check |
| Platform | Windows | Project history |
| Architecture | C++ + Blueprint | Project history; verify modules/assets |
| Primary module | BrokenHorizon | Provisional |
| Git branch | Unknown | Run `git status` |
| Last editor build | Not yet verified by this kit | Run `Tools/BuildEditor.ps1` |
| Last automation run | Not yet verified by this kit | Run `Tools/RunTests.ps1` |

## Provisional working baseline

The following came from prior development history and must be confirmed against source and PIE behavior:

- first-person camera and movement;
- Enhanced Input movement bindings;
- jump and crouch;
- trace-based interaction and prompt display;
- `IBHInteractable` / `UBHInteractable` integration;
- door open/close and lock/keycard behavior;
- objective component, HUD, and notification UI.

## Historical safety issue

A previous editor crash was caused by invoking an interaction `Execute_*` function on an actor that did not implement `UBHInteractable`. The expected invariant is object validity plus an interface implementation check before every interface dispatch.

## Current active milestone

**Repository bootstrap and verified baseline.**

After the baseline is documented, the likely next vertical slice is a basic weapon foundation unless the current repository shows it already exists or another task has priority.

## Known blockers

- None verified yet.
- Warn if the local project path contains `OneDrive`.

## Validation record

| Date | Commit/branch | Editor build | Tests | PIE/manual | Notes |
|---|---|---|---|---|---|
| 2026-08-14 | Unknown | Not run in source project | Not run | Not run | Codex Accelerator installed as a drop-in package |

## Progress log

Add dated entries only after direct inspection or successful validation.
