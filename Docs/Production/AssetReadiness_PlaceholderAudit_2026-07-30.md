# Asset readiness audit (placeholder scan)

Date: 30 July 2026

## Command used

- `Get-ChildItem Content -Recurse -File | Where-Object { $_.Extension -in '.uasset','.umap' -and $_.Name -match 'temp|placeholder|tmp|default|scratch|_dflt' }`

## High-signal candidates to replace/verify

- `Content\BrokenHorizon\Presentation\Characters\ABP_FirstLight_EnemyPlaceholder.uasset`
- `Content\BrokenHorizon\Presentation\Characters\SKM_FirstLight_EnemyPlaceholder.uasset`
- `Content\BrokenHorizon\LevelPrototyping\Materials\MI_DefaultColorway.uasset`
- `Content\Weapons\GrenadeLauncher\Audio\FirstPersonTemplateWeaponFire02.uasset`
- `Content\__ExternalActors__\BrokenHorizon\Maps\L_BrokenHorizon_World\8\IO\... (external actor temp artifact)`

## Next actions

1. Confirm whether `ABP_FirstLight_EnemyPlaceholder` and `SKM_FirstLight_EnemyPlaceholder`
   are intended temporary shipping blockers.
2. Replace `MI_DefaultColorway` with the final material variant or explicitly exclude
   from final 1.0 if intentionally internal-only.
3. Verify grenade audio asset source and naming intent (`FirstPersonTemplateWeaponFire02`);
   relabel/reparent to final naming convention if this is production.
4. Remove accidental external actor references from shipped map artifacts.

## Evidence status

- Logged as a first-pass, filename-based audit only.  
- Needs content-review confirmation before 1.0 closeout.

## Editor-backed baseline — 1 August 2026

The read-only UE5.8 commandlet audit in
`Content/Python/validate_asset_readiness.py` now scans 234 assets under the
Broken Horizon, Characters, Weapons, and active Infima assault-rifle shipping
scopes. Direct class-default inspection confirmed that `BP_Rifle` currently
uses the owner-only Infima skeletal rifle; its legacy static-mesh component is
unassigned. Its machine-readable
report is `Saved/Reports/BHAssetReadiness.json`; the commandlet log is
`Saved/Logs/BHAssetReadiness.log`.

- 22 Texture2D assets were measured. None exceeded the audit-policy 4096-pixel
  maximum-dimension warning, and none exceeded 2048 pixels while marked
  non-streaming. These are explicit audit thresholds because GDD-08 does not
  prescribe numeric texture budgets.
- 15 base materials and 5 material instances were found (25% instance share).
  This is a measured baseline, not proof that shader complexity or material
  reuse is production-final.
- Three name-based placeholder candidates were found. Asset Registry reverse
  references prove that two reach shipping gameplay contracts:
  - `SKM_FirstLight_EnemyPlaceholder` -> `/Game/Characters/BP_EnemySoldier`
  - `FirstPersonTemplateWeaponFire02` -> `/Game/BP_Rifle`
- Both shipping-referenced candidates return `DataValidationResult.VALID`.
  Their names and provenance still require an art/audio decision; validity does
  not mean final-quality content.
- `ABP_FirstLight_EnemyPlaceholder` has no reverse reference chain to the
  audited shipping roots and is therefore not currently classified as a
  shipping blocker.
- Seven skeletal meshes exposed usable metadata and none reported a single LOD.
  Seven static meshes now expose authoritative UE5.8 LOD metadata and valid
  simple-collision data: none are unknown or collisionless. Six have one LOD,
  but only the small ejected assault-rifle casing is shipping-referenced; its
  visual/performance disposition remains a manual art review item.

The final audit in `BHActiveWeaponAssetReadiness-Assets.log` completed with
`errors=0` and no audit warnings. The audit is read-only and saved no binary
assets.

### Disposition metrics - 2 August 2026

The canonical audit now records bounds, LOD/material/collision/Nanite metadata,
and import provenance for mesh candidates plus duration, channels, sample rate,
streaming, and import provenance for sound candidates. This keeps production
decisions grounded in the asset rather than its filename alone.

- The shipping-referenced casing is a 3.6 cm transient prop (1.81 cm sphere
  radius) with one material. One LOD and no Nanite are proportionate at this
  scale; it is no longer treated as a 1.0 LOD blocker, though visible ejection
  quality remains part of the weapon look review.
- `SKM_FirstLight_EnemyPlaceholder` resolves to Epic's stock Quinn mannequin
  import (`UE5_Mannequin_Quinn_LOD1.fbx`), with a 1.80 m full height and two
  materials. It remains a genuine character-art replacement candidate.
- `FirstPersonTemplateWeaponFire02` is a 1.702-second stereo import from the
  StarterContent/template weapon source. It remains a genuine production audio
  replacement candidate, not merely a false-positive name.

`BHFirstLightNavAlignmentComplete-Assets.log` completed the enhanced read-only
audit with zero warnings/errors. `Saved/Reports/BHAssetReadiness.json` contains
the machine-readable metrics; no content asset was saved by the audit.
No `.uasset` or `.umap` was saved or modified.

The audit is also available as the optional canonical validator gate
`Validate-BrokenHorizon.cmd -Assets`. The accepted integrated run is
`Saved/Logs/BHAINavigationFallback-Final-Assets.log`; it required the completion
marker, a read-only JSON report, and zero per-asset audit errors while leaving
art-review findings non-blocking for gameplay iteration.

## Audio/FX contract baseline - 1 August 2026

The separate read-only audit in
`Content/Python/validate_audio_fx_readiness.py` measures player-rifle, base-enemy,
player movement, and First Light guard audio/FX assignments without modifying
their assets. Its
report is `Saved/Reports/BHAudioFXReadiness.json`, and the canonical
`Validate-BrokenHorizon.cmd -AudioFX` evidence is
`Saved/Logs/BHAudioFXAudit-Canonical-AudioFX.log`.

The audit completed with zero contract-inspection errors, but only 1 of 18
required assignments and 0 of 27 optional assignments currently resolve. The
existing player fire cue is the shipping-referenced template-named asset already
flagged above. Missing production content includes player dry-fire and reload,
enemy and First Light guard fire, player default and five surface-specific
footsteps, wind/rain and distant-war layers/events, distinct UI confirmation/
strategic/combat cues, player/enemy indoor and outdoor weapon tails, near-miss
audio, muzzle/impact FX, and all seven AI bark categories.
These are measured content blockers, not validator failures or a claim that
the audio pass is complete.
