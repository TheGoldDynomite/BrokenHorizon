# Broken Horizon — Codex Kit

## Installed version

`2.0.1` — 2026-08-14

## Installed capabilities

- Root, source, docs, and tooling `AGENTS.md` instruction layers.
- Eight project-scoped Codex agents.
- Persistent project state, architecture, decision, roadmap, handoff, and test documents.
- Unreal Engine discovery from project association, registry, Launcher metadata, environment variables, and common install paths.
- Incremental editor build command.
- Headless automation test command with a distinct no-tests result.
- Git change review with whitespace/conflict-marker checks, generated-output protection, and JSON summaries.
- Full validation gate and JSON summaries.
- Development packaging command.
- Git initialization, Unreal ignore rules, Git LFS rules, and tracked-generated-file warnings.
- Project Doctor environment report.

## Logs

Commands write timestamped logs and latest-result JSON files under:

```text
Saved\Logs\Codex
```

## Exit-code conventions

- `0`: success.
- `1`: failure or blocker.
- `2`: no matching automation tests, or an optional setup capability is unavailable.
- `124`: spawned Unreal test process timed out.

## Safety

The kit never performs clean builds, deletes caches, commits, pushes, upgrades the engine, changes plugins, or edits binary Unreal assets automatically.
