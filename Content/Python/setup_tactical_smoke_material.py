"""Create the source-backed tactical smoke material used by support zones."""

import unreal


PACKAGE_PATH = "/Game/BrokenHorizon/Effects/Materials"
ASSET_NAME = "M_BH_TacticalSmokeCardFinal"
ASSET_PATH = "%s/%s" % (PACKAGE_PATH, ASSET_NAME)


def create_expression(material, expression_class, x, y):
    return unreal.MaterialEditingLibrary.create_material_expression(
        material,
        expression_class,
        x,
        y,
    )


def setup_tactical_smoke_material():
    material = unreal.load_asset(ASSET_PATH)
    if material is not None:
        unreal.log(
            "BH_TACTICAL_SMOKE_MATERIAL_READY "
            "asset=/Game/BrokenHorizon/Effects/Materials/M_BH_TacticalSmokeCardFinal"
        )
        return
    material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        ASSET_NAME,
        PACKAGE_PATH,
        unreal.Material,
        unreal.MaterialFactoryNew(),
    )
    if material is None:
        raise RuntimeError("Tactical smoke material could not be created.")

    material.modify()
    material.set_editor_property(
        "blend_mode",
        unreal.BlendMode.BLEND_TRANSLUCENT,
    )
    material.set_editor_property("two_sided", True)
    material.set_editor_property(
        "shading_model",
        unreal.MaterialShadingModel.MSM_UNLIT,
    )
    material.set_editor_property("cast_ray_traced_shadows", False)

    tint = create_expression(
        material,
        unreal.MaterialExpressionVectorParameter,
        -760,
        -180,
    )
    tint.set_editor_property("parameter_name", "SmokeTint")
    tint.set_editor_property(
        "default_value",
        unreal.LinearColor(0.16, 0.18, 0.19, 1.0),
    )

    smoke_texture_asset = unreal.load_asset(
        "/Engine/Tutorial/SubEditors/TutorialAssets/T_soft_smoke"
    )
    if smoke_texture_asset is None:
        raise RuntimeError("Engine soft-smoke texture could not be loaded.")
    smoke_texture = create_expression(
        material,
        unreal.MaterialExpressionTextureSampleParameter2D,
        -560,
        120,
    )
    smoke_texture.set_editor_property("parameter_name", "SmokeTexture")
    smoke_texture.set_editor_property("texture", smoke_texture_asset)

    opacity_strength = create_expression(
        material,
        unreal.MaterialExpressionScalarParameter,
        -120,
        370,
    )
    opacity_strength.set_editor_property(
        "parameter_name",
        "OpacityStrength",
    )
    opacity_strength.set_editor_property("default_value", 0.58)

    opacity = create_expression(
        material,
        unreal.MaterialExpressionMultiply,
        100,
        220,
    )
    unreal.MaterialEditingLibrary.connect_material_expressions(
        smoke_texture,
        "A",
        opacity,
        "A",
    )
    unreal.MaterialEditingLibrary.connect_material_expressions(
        opacity_strength,
        "",
        opacity,
        "B",
    )

    unreal.MaterialEditingLibrary.connect_material_property(
        tint,
        "",
        unreal.MaterialProperty.MP_EMISSIVE_COLOR,
    )
    unreal.MaterialEditingLibrary.connect_material_property(
        opacity,
        "",
        unreal.MaterialProperty.MP_OPACITY,
    )
    unreal.MaterialEditingLibrary.recompile_material(material)
    if not unreal.EditorAssetLibrary.save_loaded_asset(material):
        raise RuntimeError("Tactical smoke material could not be saved.")
    unreal.log(
        "BH_TACTICAL_SMOKE_MATERIAL_READY "
        "asset=/Game/BrokenHorizon/Effects/Materials/M_BH_TacticalSmokeCardFinal"
    )


if __name__ == "__main__":
    setup_tactical_smoke_material()
