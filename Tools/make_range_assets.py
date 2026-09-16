"""
Ассеты стрельбища, которые нельзя написать текстом: материал геля (полупрозрачный) и материал канала.
Запуск: Tools/make_range_assets.bat. Идемпотентен.
"""
import unreal

mel = unreal.MaterialEditingLibrary
eal = unreal.EditorAssetLibrary
PATH = "/Game/Range"


def log(m):
    unreal.log(f"[range-assets] {m}")


def get_or_create(name):
    full = f"{PATH}/{name}"
    if eal.does_asset_exist(full):
        mat = eal.load_asset(full)
        mel.delete_all_material_expressions(mat)
    else:
        mat = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, PATH, unreal.Material, unreal.MaterialFactoryNew())
    return mat, full


def const3(mat, rgb, x, y):
    c = mel.create_material_expression(mat, unreal.MaterialExpressionConstant3Vector, x, y)
    c.set_editor_property("constant", unreal.LinearColor(rgb[0], rgb[1], rgb[2], 1.0))
    return c


def const1(mat, v, x, y):
    c = mel.create_material_expression(mat, unreal.MaterialExpressionConstant, x, y)
    c.set_editor_property("r", v)
    return c


def make_gel(name="M_Gel", translucent=True, lighting=None, unlit=False):
    mat, full = get_or_create(name)
    if translucent:
        mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
        if lighting is not None:
            mat.set_editor_property("translucency_lighting_mode", lighting)
    if unlit:
        mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    mat.set_editor_property("two_sided", False)
    color = const3(mat, (0.80, 0.62, 0.30), -400, 0)
    mel.connect_material_property(color, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR if unlit else unreal.MaterialProperty.MP_BASE_COLOR)
    if translucent:
        mel.connect_material_property(const1(mat, 0.42, -400, 150), "", unreal.MaterialProperty.MP_OPACITY)
    mel.connect_material_property(const1(mat, 0.12, -400, 250), "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.connect_material_property(const1(mat, 0.6, -400, 350), "", unreal.MaterialProperty.MP_SPECULAR)
    mel.recompile_material(mat)
    eal.save_loaded_asset(mat)
    log(f"saved {full}")


def make_channel():
    mat, full = get_or_create("M_WoundChannel")
    mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    mel.connect_material_property(const3(mat, (0.55, 0.03, 0.02), -400, 0), "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.recompile_material(mat)
    eal.save_loaded_asset(mat)
    log(f"saved {full}")


# Режим освещения полупрозрачности - по умолчанию (volumetric): TLM_SURFACE_PER_PIXEL_LIGHTING в этом проекте
# (5.8, Lumen, RenderOffScreen) не рисуется вовсе - проверено скриншотами 2026-09-15.
make_gel()
make_channel()


def make_panel(name, rgb, rough=0.8, metal=0.0):
    mat, full = get_or_create(name)
    mel.connect_material_property(const3(mat, rgb, -400, 0), "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(const1(mat, rough, -400, 150), "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.connect_material_property(const1(mat, metal, -400, 250), "", unreal.MaterialProperty.MP_METALLIC)
    mel.recompile_material(mat)
    eal.save_loaded_asset(mat)
    log(f"saved {full}")


make_panel("M_PanelDrywall", (0.85, 0.85, 0.82), 0.9)
make_panel("M_PanelPlywood", (0.65, 0.48, 0.28), 0.7)
make_panel("M_PanelPine", (0.78, 0.62, 0.38), 0.75)
make_panel("M_PanelSteel", (0.45, 0.46, 0.48), 0.35, 1.0)
log("done")
