# Broken Horizon Documentation Rules

Documentation under `docs/` is project memory, not marketing copy.

- `PROJECT_STATE.md`: verified present-tense facts, build/test status, known blockers, and the next active milestone.
- `ARCHITECTURE.md`: ownership, data flow, dependencies, extension points, and evidence-backed diagrams.
- `DECISIONS.md`: durable choices with date, status, rationale, and consequences.
- `ROADMAP.md`: only prioritized near-term vertical slices; do not silently expand scope.
- `EDITOR_HANDOFF.md`: exact outstanding Unreal Editor/Blueprint work. Remove or mark entries complete after verification.
- `TEST_MATRIX.md`: existing tests, intended coverage, gaps, and the command that runs them.

Do not replace verified user notes with assumptions. Label provisional information. Cite exact files/classes inside the repository when updating architecture or state.
