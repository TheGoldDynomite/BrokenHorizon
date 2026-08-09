"""Create the two required First Light ambience SoundWave assets once.

This script is intentionally narrow and idempotent. It imports only the
matching WAV files from Content/BrokenHorizon/Audio and never saves maps or
rewrites an existing asset.
"""

import os

import unreal


AUDIO_ASSETS = (
    "SW_FirstLight_WindRain",
    "SW_FirstLight_DistantWar",
)


def main():
    project_dir = os.path.normpath(unreal.Paths.project_dir())
    source_dir = os.path.join(
        project_dir,
        "Content",
        "BrokenHorizon",
        "Audio",
    )
    destination_path = "/Game/BrokenHorizon/Audio"
    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()

    os.makedirs(source_dir, exist_ok=True)
    created = []
    skipped = []

    for asset_name in AUDIO_ASSETS:
        asset_path = f"{destination_path}/{asset_name}"
        source_path = os.path.join(source_dir, f"{asset_name}.wav")

        if unreal.EditorAssetLibrary.does_asset_exist(asset_path):
            skipped.append(asset_name)
            continue

        if not os.path.isfile(source_path):
            unreal.log_error(
                f"BH_FIRST_LIGHT_AUDIO_IMPORT missing_source={source_path}"
            )
            raise RuntimeError(f"Missing audio source: {source_path}")

        task = unreal.AssetImportTask()
        task.filename = source_path
        task.destination_path = destination_path
        task.destination_name = asset_name
        task.replace_existing = False
        task.automated = True
        task.save = True
        asset_tools.import_asset_tasks([task])

        if not unreal.EditorAssetLibrary.does_asset_exist(asset_path):
            unreal.log_error(
                f"BH_FIRST_LIGHT_AUDIO_IMPORT import_failed={asset_path}"
            )
            raise RuntimeError(f"Import failed: {asset_path}")

        created.append(asset_name)

    unreal.log(
        "BH_FIRST_LIGHT_AUDIO_IMPORT result=complete "
        f"created={','.join(created) or 'none'} "
        f"skipped={','.join(skipped) or 'none'}"
    )


if __name__ == "__main__":
    main()
