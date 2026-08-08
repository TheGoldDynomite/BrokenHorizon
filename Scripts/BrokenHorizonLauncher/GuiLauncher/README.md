# Broken Horizon GUI Launcher

Build it by running `Build-GuiLauncher.ps1`. Give players the generated
`Builds/Launcher/GUI/BrokenHorizonLauncher-Win64.zip`. They extract it once and
double-click `BrokenHorizonLauncher.exe`.

The launcher checks the stable GitHub release asset URL in `launcher-config.json`.
When that ZIP changes, it downloads the complete packaged game into the player's
local application-data folder, validates the real Windows executable, and launches it.

For each game update, package Broken Horizon and replace this release asset:

`BrokenHorizon-FirstLight-Development.zip`

under the GitHub release/tag `Update`. Keep the asset name unchanged. Players do
not need a new launcher unless the launcher itself changes.
