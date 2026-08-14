# Broken Horizon - Durable Decisions

Record new choices with date, status, rationale, consequences, and affected
systems. Repository evidence can update an existing decision without creating
a new one.

## D-001 - Engine and platform

- **Status:** Accepted and verified 2026-08-14.
- **Decision:** Target Unreal Engine 5.8.0 on Windows.
- **Evidence:** BrokenHorizon.uproject EngineAssociation=5.8,
  Unreal5_8 target include order, Project Doctor, and the successful
  BrokenHorizonEditor Win64 Development build.
- **Consequence:** Build and tooling scripts resolve the project-associated
  UE 5.8 installation. The historical UE 5.6 wording is no longer current.

## D-002 - C++ and Blueprint split

- **Status:** Accepted.
- **Decision:** Keep durable gameplay state, rules, and reusable architecture
  in C++; use Blueprint for asset assignment, animation, audio, VFX, layout,
  and tuning.
- **Consequence:** C++ exposes deliberate presentation hooks while Blueprint
  remains thin.

## D-003 - Binary asset safety

- **Status:** Accepted.
- **Decision:** Never hand-edit .uasset or .umap files.
- **Consequence:** Binary changes require Unreal Editor automation that is
  actually available or exact manual handoff steps.

## D-004 - Incremental validation

- **Status:** Accepted.
- **Decision:** Use incremental editor builds during development and the full
  validation script before completion. Clean builds are evidence-driven
  exceptions.
- **Consequence:** Faster iteration without routinely destroying useful
  generated state.

## D-005 - Agent coordination

- **Status:** Accepted.
- **Decision:** Parallelize read-only exploration; allow only one writer on
  overlapping files.
- **Consequence:** Faster mapping with fewer conflicting edits.

## D-006 - Interaction dispatch invariant

- **Status:** Accepted and source-reverified 2026-08-14.
- **Decision:** Validate object lifetime and UBHInteractable implementation
  before every interaction Execute_* call.
- **Evidence:** ABHCharacter::UpdateInteractionPrompt,
  ABHCharacter::ServerInteract, and ABHCharacter::ExecuteInteraction.
- **Consequence:** The invariant remains a code-review target. Direct test
  coverage waits for a generally useful seam or a real world-backed fixture.

## D-007 - Source control

- **Status:** Accepted.
- **Decision:** Track Unreal binary assets with Git LFS, ignore generated
  Unreal output, and never auto-commit from setup tooling.
- **Consequence:** Local checkpoints remain intentional and reviewable.

## D-008 - Automation report authority

- **Status:** Accepted 2026-08-14.
- **Decision:** When UnrealEditor-Cmd completes and exports a complete
  automation report but Windows exposes no usable process exit code,
  RunTests.ps1 uses the report's failed/test/notRun/inProcess counts instead
  of treating a null exit code as a failure. A missing or incomplete report is
  a failed run; an exported report with zero tests is NoTests.
- **Rationale:** The pre-test-addition project completed all 101 discovered
  tests, but the original wrapper false-failed on a null process exit code.
- **Consequence:** The report and logs must be retained and inspected; actual
  nonzero process exits, timeouts, incomplete reports, and report failures
  still fail the gate.

## D-009 - Untracked-file review

- **Status:** Accepted 2026-08-14.
- **Decision:** The default ReviewChanges.ps1 path checks untracked text files
  with the same whitespace/conflict-marker rules as tracked diffs while
  excluding generated folders and Unreal binary assets from that text pass.
- **Rationale:** Codex bootstrap documents, scripts, and tests can be
  untracked in an inherited checkout, so tracked-only diff checks would miss
  defects in the files being prepared.
- **Consequence:** A dirty checkout can expose additional pre-existing
  untracked whitespace findings. They remain review blockers until the owner
  intentionally normalizes or scopes that unrelated work.
