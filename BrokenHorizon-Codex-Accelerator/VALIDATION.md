# Package Validation

Validated on **2026-08-14** before archive creation.

| Result | Check | Details |
|---|---|---|
| Pass | Required files | 26 required files present |
| Pass | Package file inventory | 52 files |
| Pass | UTF-8 text decoding | 52 text files decoded |
| Pass | Non-empty package files | all files non-empty |
| Pass | No NUL bytes in text | none |
| Pass | TOML parsing | 9/9 parsed |
| Pass | Codex agent configuration schema | {"default_subagent_model": "gpt-5.6-terra", "default_subagent_reasoning_effort": "medium", "enabled": true, "interrupt_message": true, "max_concurrent_threads_per_session": 4} |
| Pass | Project agent schemas | 8 agents valid and uniquely named |
| Pass | Default subagent model | gpt-5.6-terra |
| Pass | PowerShell lexical structure | 10 scripts balanced |
| Pass | PowerShell safety policy | strict mode/error policy present |
| Pass | Windows command wrappers | 8 wrappers verified; the new top-level launcher targets FIRST_RUN_PROMPT.txt |
| Pass | Installer root-command wiring | all six wrappers referenced |
| Pass | Capability and safety markers | no binary asset hand editing, interface guard, incremental build, test timeout, no-tests status, change review gate, git lfs assets, state preservation, timestamped backup |
| Pass | No embedded local user paths | none |

## What this proves

- The installer payload is complete and internally cross-referenced.
- All Codex TOML files parse, the eight agent definitions have the required fields, and agent names are unique.
- All PowerShell files pass static UTF-8, string/comment, and delimiter-balance checks.
- The seven script command wrappers point to included PowerShell scripts and propagate exit codes; the top-level prompt launcher points to the included top-level prompt file.
- The package contains the intended safety controls, backup behavior, Git/LFS rules, incremental build flow, automation timeout/no-tests handling, and final change-review gate.

## Runtime validation boundary

The actual `BrokenHorizon.uproject`, Unreal Engine installation, and Windows PowerShell runtime were not available in this chat environment. Therefore, this report does **not** claim that the user's current project compiles or that project-specific tests pass. The installer runs `ProjectDoctor.ps1` on the real machine, and `FIRST_RUN_PROMPT.txt` directs Codex to establish the real baseline build, inspect the actual APIs, add only source-compatible tests, run validation, and record the results.
