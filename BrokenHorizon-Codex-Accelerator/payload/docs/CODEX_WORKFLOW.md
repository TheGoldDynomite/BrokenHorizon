# Broken Horizon — Fast Codex Workflow

## First project session

The installer copies `FIRST_RUN_PROMPT.txt` to the project root and attempts to place it on the clipboard. Paste it into Codex with the Broken Horizon folder open.

That task performs repository mapping, a baseline build, project documentation updates, source-control checks, and source-aware low-risk test work before recommending the next gameplay slice.

## Daily implementation loop

1. Create or confirm a Git checkpoint.
2. Give Codex one visible outcome, exact in-scope behavior, out-of-scope boundaries, and a definition of done.
3. Let Codex inspect only the affected code path.
4. Build after a coherent compile-sized change:

   ```powershell
   .\Tools\BuildEditor.ps1
   ```

5. Continue until the vertical slice is complete.
6. Run the full gate:

   ```powershell
   .\Tools\Validate.ps1
   ```

7. Review `git diff`, complete `docs/EDITOR_HANDOFF.md`, run PIE, then commit intentionally.

## Validation commands

```powershell
# Environment and source-control health
.\Tools\ProjectDoctor.ps1

# Fast incremental editor compile
.\Tools\BuildEditor.ps1

# Broken Horizon automation filter
.\Tools\RunTests.ps1

# Review the local change set without editing it
.\Tools\ReviewChanges.ps1

# Doctor + build + tests + change review
.\Tools\Validate.ps1

# Fail when the test filter matches nothing
.\Tools\Validate.ps1 -RequireTests

# Development package using project packaging settings
.\Tools\PackageDevelopment.ps1
```

Every command accepts `-ProjectRoot` and `-EngineRoot` when automatic discovery is not enough.


## Safe parallel worktrees

Use worktrees only for genuinely independent tasks. Read-only architecture/exploration agents may run together, but never let two write-capable agents modify the same source, module rules, input configuration, widget, Blueprint, map, or descriptor.

From the main project repository:

```powershell
New-Item -ItemType Directory -Force ..\BrokenHorizon-worktrees | Out-Null
git worktree add ..\BrokenHorizon-worktrees\weapon-foundation -b feature/weapon-foundation
git worktree add ..\BrokenHorizon-worktrees\docs-audit -b chore/docs-audit
```

Merge the validation/tooling branch before starting a feature that depends on those tools. Remove a completed worktree only after its branch is committed or otherwise preserved:

```powershell
git worktree remove ..\BrokenHorizon-worktrees\docs-audit
git worktree prune
```

Do not use worktrees to bypass review or to run overlapping writers faster.

## Prompt shape

Use:

```text
Goal:
Context:
In scope:
Out of scope:
Constraints:
Done when:
```

Do not ask Codex to "add the whole combat system." Ask for one playable and testable slice.

## Agent routing

- Small/mechanical: main agent or `bh_routine_worker`.
- Repository path mapping: `bh_code_explorer`.
- Cross-system ownership/design: `bh_architect`.
- Focused C++ implementation: `bh_cpp_implementer`.
- Build/crash diagnosis: `bh_build_debugger`.
- Automation tests: `bh_test_engineer`.
- Blueprint handoff: `bh_blueprint_guide`.
- Final safety review: `bh_reviewer`.

Read-only agents can run together. One writer owns the affected files.
