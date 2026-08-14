# Broken Horizon Repository Bootstrap

```text
Prepare the actual Broken Horizon repository for fast, reliable Codex development. This is an implementation task, not a general explanation. Follow AGENTS.md and all nested instructions.

Goal:
Establish a verified repository baseline, make the installed build/test workflow work against the real project, update the project-memory documents with evidence, and add the safest useful automation coverage that matches the current source. Do not implement a new gameplay feature in this task.

Phase 1 — Inspect without changing gameplay:

1. Read AGENTS.md, Source/AGENTS.md, docs/PROJECT_STATE.md, docs/ARCHITECTURE.md, docs/DECISIONS.md, docs/TEST_MATRIX.md, the .uproject, all target files, and relevant module Build.cs files.
2. Run Tools/ProjectDoctor.ps1.
3. Inspect git status and the current diff. Preserve unrelated user work.
4. Spawn bh_code_explorer to map the real modules, key classes, input flow, UI ownership, interaction/door/keycard/objective systems, existing weapon code, and existing tests.
5. In parallel, spawn bh_architect to identify ownership risks, test seams, and the smallest safe baseline improvements. Both agents are read-only.
6. Wait for both and reconcile their evidence before any source edit.

Phase 2 — Verify the baseline:

1. Run Tools/BuildEditor.ps1 before changing production code.
2. If the baseline fails, use bh_build_debugger as the only writer and fix only a clearly project-local build blocker. Do not delete caches, upgrade the engine, or change plugins as a generic response.
3. Record the exact baseline command, result, first meaningful error if any, engine root, editor target, branch, and commit state.

Phase 3 — Update durable project memory:

Update docs/PROJECT_STATE.md, docs/ARCHITECTURE.md, docs/ROADMAP.md, docs/TEST_MATRIX.md, and docs/EDITOR_HANDOFF.md using verified files, symbols, assets referenced by code, build results, and remaining unknowns. Preserve existing verified notes. Record durable new choices in docs/DECISIONS.md only when a real choice was made.

Phase 4 — Source-aware low-risk tests:

1. Inspect the existing test pattern and module structure before adding anything.
2. Use bh_test_engineer as the only writer for test files and any minimal test-module wiring.
3. Add only deterministic tests supported by the real APIs. Prioritize, in this order:
   - interaction dispatch/interface safety;
   - door unlocked/locked transitions;
   - wrong/missing/correct keycard behavior;
   - objective state transitions.
4. Do not invent signatures, asset names, or public APIs. Do not add broad production refactors merely to make tests possible.
5. When a candidate cannot be tested safely without a larger design change, document the exact gap in TEST_MATRIX.md instead of faking coverage.

Phase 5 — Validate and review:

1. Run Tools/BuildEditor.ps1 after each coherent compile-sized change.
2. Run Tools/Validate.ps1. Use -RequireTests only if tests were successfully added and should be discoverable.
3. Spawn bh_reviewer for a read-only review of the final diff, Unreal reflection/lifetime safety, module dependencies, regression risk, and validation gaps.
4. Fix blocking findings with exactly one writer, then rerun the affected validation.
5. Inspect git diff --check and confirm no generated folders or unrelated files are included.

Completion report:

- Verified project/module/engine architecture.
- Exact files changed.
- Baseline and final build commands/results.
- Tests added, names, and results—or exact reasons a candidate could not be added safely.
- Remaining Unreal Editor/PIE steps from EDITOR_HANDOFF.md.
- Risks or blockers.
- The next single vertical slice, based on the real current project rather than prior assumptions.
```
