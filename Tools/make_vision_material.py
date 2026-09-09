"""
Собирает Post Process Material /Game/Vision/M_FovealComposite из Shaders/FovealComposite.ush.

Запуск: Tools/make_vision_material.bat (без редактора). Идемпотентен: материал пересобирается.
Править HLSL - в .ush, не в материале. Параметры P0/P1/Debug задаёт UPBLVisionComponent.
"""
import os
import unreal

MAT_PATH = "/Game/Vision"
MAT_NAME = "M_FovealComposite"
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
    for name in ("UV", "SceneTex", "SideL", "SideR", "P0", "P1", "Debug"):
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

    def vec_param(name, y, default):
        v = mel.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -900, y)
        v.set_editor_property("parameter_name", name)
        v.set_editor_property("default_value", unreal.LinearColor(*default))
        return v

    def scalar_param(name, y, default):
        sp = mel.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -900, y)
        sp.set_editor_property("parameter_name", name)
        sp.set_editor_property("default_value", default)
        return sp

    side_l = tex_param("SideL", 0)
    side_r = tex_param("SideR", 150)
    p0 = vec_param("P0", 300, (103.0, 77.5, 90.0, 33.2))
    p1 = vec_param("P1", 450, (35.0, 50.0, 25.0, 1.5))
    dbg = scalar_param("Debug", 600, 0.0)

    links = ((uv, "", "UV"), (scene, "Color", "SceneTex"), (side_l, "", "SideL"), (side_r, "", "SideR"),
             (p0, "", "P0"), (p1, "", "P1"), (dbg, "", "Debug"))
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
