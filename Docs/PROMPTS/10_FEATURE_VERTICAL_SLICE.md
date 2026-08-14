# Feature Vertical Slice Prompt

```text
Implement one vertical slice in Broken Horizon. Follow AGENTS.md and nested instructions.

Goal:
[Visible player outcome]

Context:
[Current verified systems/classes/assets]

In scope:
[Exact behavior and files/systems allowed]

Out of scope:
[Adjacent systems that must not be added]

Constraints:
- Preserve unrelated working behavior.
- Do not edit .uasset or .umap files as text.
- Keep reusable gameplay rules in C++ and presentation in Blueprint.
- Use the smallest compatible ownership design.
- Build incrementally with Tools/BuildEditor.ps1.

Done when:
- The editor target builds.
- Relevant tests pass or a precise test gap is documented.
- The final diff has no unrelated changes.
- docs/EDITOR_HANDOFF.md contains exact remaining Editor steps.
- Tools/Validate.ps1 passes to the extent supported by the project.
```
