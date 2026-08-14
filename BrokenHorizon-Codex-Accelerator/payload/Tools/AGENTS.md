# Broken Horizon Tooling Rules

Scripts in this folder must remain compatible with Windows PowerShell 5.1 unless the project intentionally changes that baseline.

- Default to non-destructive, incremental operations.
- Never clean, delete caches, kill unrelated processes, commit, push, or change the engine installation automatically.
- Resolve the `.uproject`, target names, and engine root instead of hardcoding the user's path.
- Return nonzero exit codes on real failure and write useful logs under `Saved\Logs\Codex`.
- Keep command output visible and record the exact native command used.
- Treat "no tests found" separately from "tests failed."
- Update command wrappers and `docs/CODEX_KIT.md` when public script parameters change.
