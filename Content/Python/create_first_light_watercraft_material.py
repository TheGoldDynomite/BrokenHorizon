"""Create the project-owned watercraft material used by the authored boat."""

import unreal


PACKAGE = "/Game/BrokenHorizon/Environment/Materials"
ASSET = PACKAGE + "/M_BH_WatercraftSurface"


def main():
    material = unreal.load_asset(ASSET)
    if not material:
        material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            "M_BH_WatercraftSurface", PACKAGE, unreal.Material,
            unreal.MaterialFactoryNew())
    if not material:
        unreal.log_error("[Watercraft Material] create failed")
        return False
    material.set_editor_property("two_sided", True)
    color = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionConstant3Vector, -360, -80)
    color.set_editor_property("constant", unreal.LinearColor(0.035, 0.075, 0.085, 1.0))
    unreal.MaterialEditingLibrary.connect_material_property(
        color, "", unreal.MaterialProperty.MP_BASE_COLOR)
    roughness = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionConstant, -360, 80)
    roughness.set_editor_property("r", 0.38)
    unreal.MaterialEditingLibrary.connect_material_property(
        roughness, "", unreal.MaterialProperty.MP_ROUGHNESS)
    unreal.MaterialEditingLibrary.recompile_material(material)
    unreal.EditorAssetLibrary.save_asset(ASSET, only_if_is_dirty=False)
    unreal.log("[Watercraft Material] PASS asset=%s" % ASSET)
    return True


main()
