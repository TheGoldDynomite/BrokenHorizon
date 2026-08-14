# Branch Review Prompt

```text
Review the current Broken Horizon branch without editing files.

1. Inspect git status, the base comparison, and changed execution paths.
2. Use bh_code_explorer and bh_reviewer in parallel.
3. Prioritize concrete Unreal defects: UHT syntax, module dependencies, object lifetime, GC, delegate cleanup, interface guards, Blueprint compatibility, replication assumptions, expensive Tick work, test gaps, and accidental generated files.
4. Return findings by severity with exact file/symbol references and the smallest safe correction.
5. State explicitly when no blocking finding is supported by evidence.
```
