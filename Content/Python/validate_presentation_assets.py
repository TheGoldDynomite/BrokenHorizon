"""Validate only the Broken Horizon assets touched by presentation setup."""

import unreal


ASSET_PATHS = (
    "/Game/BrokenHorizon/Presentation/Weapons/SM_FirstLight_Rifle",
    "/Game/BrokenHorizon/Presentation/Characters/"
    "SKM_FirstLight_EnemyPlaceholder",
    "/Game/BrokenHorizon/Presentation/Characters/"
    "ABP_FirstLight_EnemyRifle",
    "/Game/BP_Rifle",
    "/Game/Characters/BP_EnemySoldier",
)


def validate_presentation_assets():
    asset_data_list = []
    for asset_path in ASSET_PATHS:
        asset = unreal.EditorAssetLibrary.load_asset(asset_path)
        if not asset:
            raise RuntimeError("Missing presentation asset: " + asset_path)
        asset_data = unreal.EditorAssetLibrary.find_asset_data(asset_path)
        if not asset_data.is_valid():
            raise RuntimeError(
                "Asset Registry data is missing: " + asset_path
            )
        asset_data_list.append(asset_data)

    validator = unreal.get_editor_subsystem(
        unreal.EditorValidatorSubsystem
    )
    invalid_assets = []
    for asset_path, asset_data in zip(ASSET_PATHS, asset_data_list):
        validation_output = validator.is_asset_valid(
            asset_data,
            unreal.DataValidationUsecase.COMMANDLET,
        )
        if isinstance(validation_output, tuple):
            result, errors, warnings = validation_output
        else:
            result = validation_output
            errors = ()
            warnings = ()
        unreal.log(
            "[BH Presentation Validation] %s -> %s "
            "(errors=%d, warnings=%d)"
            % (
                asset_path,
                str(result),
                len(errors),
                len(warnings),
            )
        )
        if result != unreal.DataValidationResult.VALID:
            invalid_assets.append(asset_path)

    unreal.log(
        "[BH Presentation Validation] Validated %d assets; invalid=%d"
        % (len(asset_data_list), len(invalid_assets))
    )
    if invalid_assets:
        raise RuntimeError(
            "Broken Horizon presentation validation found invalid assets: %s"
            % ", ".join(invalid_assets)
        )


if __name__ == "__main__":
    validate_presentation_assets()
