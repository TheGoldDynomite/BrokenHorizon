"""Create the project-owned water material used by First Light's route."""

import unreal


PACKAGE = "/Game/BrokenHorizon/Environment/Materials"
ASSET = PACKAGE + "/M_BH_WaterSurface"


def main():
    material = unreal.load_asset(ASSET)
    if not material:
        tools = unreal.AssetToolsHelpers.get_asset_tools()
        material = tools.create_asset(
            "M_BH_WaterSurface",
            PACKAGE,
            unreal.Material,
            unreal.MaterialFactoryNew(),
        )
    if not material:
        unreal.log_error("[Water Material] create failed")
        return False

    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    material.set_editor_property("two_sided", True)
    expr = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionConstant3Vector, -420, -80
    )
    expr.set_editor_property("constant", unreal.LinearColor(0.015, 0.12, 0.18, 1.0))
    unreal.MaterialEditingLibrary.connect_material_property(
        expr, "", unreal.MaterialProperty.MP_BASE_COLOR
    )
    opacity = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionConstant, -420, 80
    )
    opacity.set_editor_property("r", 0.62)
    unreal.MaterialEditingLibrary.connect_material_property(
        opacity, "", unreal.MaterialProperty.MP_OPACITY
    )
    unreal.MaterialEditingLibrary.recompile_material(material)
    unreal.EditorAssetLibrary.save_asset(ASSET, only_if_is_dirty=False)
    unreal.log("[Water Material] PASS asset=%s" % ASSET)
    return True


main()
