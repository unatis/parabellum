"""
Собирает Post Process Material /Game/Vision/M_FovealComposite из Shaders/FovealComposite.ush.

Запуск: Tools/make_vision_material.bat (без редактора). Идемпотентен: материал пересобирается.
Править HLSL - в .ush, не в материале. Скалярные параметры (см. SCALARS) задаёт UPBLVisionComponent.
"""
import os
import unreal

MAT_PATH = "/Game/Vision"
MAT_NAME = "M_FovealComposite"
# Скалярные параметры Custom-ноды (имя, дефолт). VectorParameter в Custom приходит как float3 - не годится.
SCALARS = (("Aspect", 16.0 / 9.0), ("CenterFOV", 103.0), ("SideYaw", 60.0), ("SideFOV", 80.0), ("TotalFOV", 160.0), ("PaniniD", 2.2), ("CamPitch", 0.0), ("PitchAlignStart", 40.0), ("PitchAlignEnd", 70.0), ("SideExposureFix", 1.0), ("SideMip", 2.5),
           ("BlendStart", 35.0), ("BlendEnd", 50.0), ("BlurStartYaw", 50.0), ("BlurMaxDeg", 0.3), ("Debug", 0.0))
HLSL = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "Shaders", "FovealComposite.ush")

mel = unreal.MaterialEditingLibrary
eal = unreal.EditorAssetLibrary


def log(m):
    unreal.log(f"[vision-mat] {m}")


def main():
    full = f"{MAT_PATH}/{MAT_NAME}"
    if eal.does_asset_exist(full):
        mat = eal.load_asset(full)
        mel.delete_all_material_expressions(mat)
        log(f"reuse {full}")
    else:
        mat = unreal.AssetToolsHelpers.get_asset_tools().create_asset(MAT_NAME, MAT_PATH, unreal.Material, unreal.MaterialFactoryNew())
        log(f"create {full}")

    mat.set_editor_property("material_domain", unreal.MaterialDomain.MD_POST_PROCESS)
    mat.set_editor_property("blendable_location", unreal.BlendableLocation.BL_SCENE_COLOR_BEFORE_BLOOM)
    mat.set_editor_property("blendable_priority", 0)

    with open(HLSL, encoding="utf-8") as f:
        code = f.read()

    custom = mel.create_material_expression(mat, unreal.MaterialExpressionCustom, -400, 0)
    custom.set_editor_property("code", code)
    custom.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    custom.set_editor_property("description", "FovealComposite (from Shaders/FovealComposite.ush)")
    inputs = []
    for name in ("UV", "SceneTex", "SideL", "SideR") + tuple(n for n, _ in SCALARS):
        ci = unreal.CustomInput()
        ci.set_editor_property("input_name", name)
        inputs.append(ci)
    custom.set_editor_property("inputs", inputs)

    uv = mel.create_material_expression(mat, unreal.MaterialExpressionScreenPosition, -900, -300)
    scene = mel.create_material_expression(mat, unreal.MaterialExpressionSceneTexture, -900, -150)
    scene.set_editor_property("scene_texture_id", unreal.SceneTextureId.PPI_POST_PROCESS_INPUT0)

    def tex_param(name, y):
        t = mel.create_material_expression(mat, unreal.MaterialExpressionTextureObjectParameter, -900, y)
        t.set_editor_property("parameter_name", name)
        return t

    def scalar_param(name, y, default):
        sp = mel.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -900, y)
        sp.set_editor_property("parameter_name", name)
        sp.set_editor_property("default_value", default)
        return sp

    side_l = tex_param("SideL", 0)
    side_r = tex_param("SideR", 150)
    links = [(uv, "", "UV"), (scene, "Color", "SceneTex"), (side_l, "", "SideL"), (side_r, "", "SideR")]
    for i, (name, default) in enumerate(SCALARS):
        links.append((scalar_param(name, 300 + 120 * i, default), "", name))
    for src, out, pin in links:
        if not mel.connect_material_expressions(src, out, custom, pin):
            raise RuntimeError(f"connect {pin} failed")

    if not mel.connect_material_property(custom, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR):
        raise RuntimeError("connect emissive failed")

    mel.recompile_material(mat)
    eal.save_loaded_asset(mat)
    log(f"saved {full}")
    log("DONE")


main()
