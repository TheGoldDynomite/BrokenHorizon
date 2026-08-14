# Broken Horizon Tools

| Script | Purpose |
|---|---|
| `ProjectDoctor.ps1` | Checks project path, Unreal installation, targets, source tree, Git/LFS, kit files, and toolchain hints. |
| `SetupSourceControl.ps1` | Merges Unreal ignore/LFS rules and optionally initializes Git; never commits. |
| `BuildEditor.ps1` | Incremental Win64 Development editor-target build. |
| `RunTests.ps1` | Headless `BrokenHorizon` automation filter with timeout and separate no-tests status. |
| `ReviewChanges.ps1` | Git status, whitespace/conflict-marker checks, generated-output protection, and a JSON review summary. |
| `Validate.ps1` | Doctor + editor build + tests + change review, with `-RequireTests` available after test onboarding. |
| `PackageDevelopment.ps1` | UAT BuildCookRun Development package using project packaging settings. |
| `OpenFirstRunPrompt.ps1` | Copies the project bootstrap prompt to the clipboard. |

All logs go to `Saved\Logs\Codex`.
