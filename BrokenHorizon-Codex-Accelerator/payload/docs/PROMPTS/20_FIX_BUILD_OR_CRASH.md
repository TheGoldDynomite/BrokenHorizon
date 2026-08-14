# Build or Crash Investigation Prompt

```text
Diagnose and fix the current Broken Horizon build, startup, or editor crash. Follow AGENTS.md.

Evidence:
[Paste the first meaningful error, call stack, or reproduction]

Required workflow:
1. Reproduce or run the failing command when possible.
2. Use bh_code_explorer to map the exact declaration/execution path.
3. Use bh_build_debugger as the only writer.
4. Fix the smallest evidence-backed root cause. Do not delete caches unless stale generated state is proven.
5. Run Tools/BuildEditor.ps1 and the focused reproduction.
6. Use bh_reviewer on the final diff.
7. Report root cause, exact files, validation, and remaining Editor steps.
```
