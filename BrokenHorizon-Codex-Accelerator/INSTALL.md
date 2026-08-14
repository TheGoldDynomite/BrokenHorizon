# Broken Horizon Codex Accelerator

This is the one-click project upgrade for **Broken Horizon**. It installs the permanent Codex context, specialized project agents, source-control rules, project-state documents, and Windows build/test/package tools directly into the folder containing `BrokenHorizon.uproject`.

It does **not** blindly inject gameplay code or tests into unseen classes. Instead, the first-run bootstrap prompt makes Codex inspect the real project, establish a baseline build, update the documentation with verified facts, and then add only tests that match the actual APIs.

## Get the prompt immediately

After extracting the ZIP, `Open-First-Run-Prompt.cmd` is now visible in the main package folder. Double-click it to copy `FIRST_RUN_PROMPT.txt` to the clipboard and open it in Notepad. This works even before installation.

The installer also places another `Open-First-Run-Prompt.cmd` inside the actual Broken Horizon project root for later use.

## Install

1. Extract this ZIP anywhere outside the Unreal project.
2. Double-click `Install-BrokenHorizonCodexKit.cmd`.
3. Select the folder that directly contains `BrokenHorizon.uproject`.
4. The installer will:
   - back up any existing Codex kit files;
   - install or upgrade the agent team and instructions;
   - preserve existing project-state documents unless they are missing;
   - merge Unreal `.gitignore` and Git LFS rules without deleting existing rules;
   - initialize Git when needed, without committing anything;
   - run a non-destructive project doctor;
   - copy the first-run prompt to the Windows clipboard.
5. Open that same project folder in Codex, trust the project when prompted so its project-scoped `.codex` configuration and agents can load, and paste the clipboard contents.

The installer does not need administrator access. It warns if the project is still inside OneDrive.

## Useful commands after installation

Run these from the project root or double-click their command wrappers:

```powershell
.\Tools\ProjectDoctor.ps1
.\Tools\BuildEditor.ps1
.\Tools\RunTests.ps1
.\Tools\ReviewChanges.ps1
.\Tools\Validate.ps1
.\Tools\PackageDevelopment.ps1
```

The normal fast loop is:

```text
focused change -> BuildEditor.ps1 -> continue -> Validate.ps1 -> review diff
```

Do not use clean builds as a normal troubleshooting step. The installed tools use Unreal's incremental build path by default.

## What is backed up

Existing `AGENTS.md`, `.codex`, kit-owned nested instructions, workflow files, tools, source-control rules, and state documents are copied to a timestamped `CodexKitBackup-*` folder before replacement or merging.

The installer never commits, pushes, deletes Unreal caches, changes engine versions, edits `.uasset` or `.umap` files, or modifies gameplay source.
