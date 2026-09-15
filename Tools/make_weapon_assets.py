"""
Ассеты оружия, которые нельзя написать текстом: материал декали попадания.
Запуск: Tools/make_weapon_assets.bat. Идемпотентен.
"""
import unreal

mel = unreal.MaterialEditingLibrary
eal = unreal.EditorAssetLibrary

DECAL_HLSL = """
// Тёмное пятно с мягким краем и щербинкой в центре. UV декали 0..1.
float2 d = UV - 0.5f;
float r = length(d) * 2.0f;
float soft = 1.0f - smoothstep(0.55f, 1.0f, r);
float core = 1.0f - smoothstep(0.0f, 0.35f, r);
return float4(soft * 0.85f, core, 0.0f, 0.0f);   // x = opacity, y = центр (темнее)
"""


def log(m):
    unreal.log(f"[weapon-assets] {m}")


def make_decal():
    path, name = "/Game/Weapons/FX", "M_BulletDecal"
    full = f"{path}/{name}"
    if eal.does_asset_exist(full):
        mat = eal.load_asset(full)
        mel.delete_all_material_expressions(mat)
    else:
        mat = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, path, unreal.Material, unreal.MaterialFactoryNew())
    mat.set_editor_property("material_domain", unreal.MaterialDomain.MD_DEFERRED_DECAL)
    mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)

    custom = mel.create_material_expression(mat, unreal.MaterialExpressionCustom, -500, 0)
    custom.set_editor_property("code", DECAL_HLSL)
    custom.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT4)
    ci = unreal.CustomInput(); ci.set_editor_property("input_name", "UV")
    custom.set_editor_property("inputs", [ci])
    uv = mel.create_material_expression(mat, unreal.MaterialExpressionTextureCoordinate, -800, 0)
    mel.connect_material_expressions(uv, "", custom, "UV")

    # opacity = custom.r ; base color = тёмно-серый, в центре почти чёрный
    mask_r = mel.create_material_expression(mat, unreal.MaterialExpressionComponentMask, -250, 0)
    mask_r.set_editor_property("r", True); mask_r.set_editor_property("g", False); mask_r.set_editor_property("b", False); mask_r.set_editor_property("a", False)
    mel.connect_material_expressions(custom, "", mask_r, "")
    mel.connect_material_property(mask_r, "", unreal.MaterialProperty.MP_OPACITY)

    mask_g = mel.create_material_expression(mat, unreal.MaterialExpressionComponentMask, -250, 150)
    mask_g.set_editor_property("r", False); mask_g.set_editor_property("g", True); mask_g.set_editor_property("b", False); mask_g.set_editor_property("a", False)
    mel.connect_material_expressions(custom, "", mask_g, "")
    lerp = mel.create_material_expression(mat, unreal.MaterialExpressionLinearInterpolate, -50, 150)
    ca = mel.create_material_expression(mat, unreal.MaterialExpressionConstant3Vector, -250, 250); ca.set_editor_property("constant", unreal.LinearColor(0.12, 0.11, 0.10, 1))
    cb = mel.create_material_expression(mat, unreal.MaterialExpressionConstant3Vector, -250, 350); cb.set_editor_property("constant", unreal.LinearColor(0.02, 0.02, 0.02, 1))
    mel.connect_material_expressions(ca, "", lerp, "A")
    mel.connect_material_expressions(cb, "", lerp, "B")
    mel.connect_material_expressions(mask_g, "", lerp, "Alpha")
    mel.connect_material_property(lerp, "", unreal.MaterialProperty.MP_BASE_COLOR)
    rough = mel.create_material_expression(mat, unreal.MaterialExpressionConstant, -50, 300); rough.set_editor_property("r", 0.9)
    mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)

    mel.recompile_material(mat)
    eal.save_loaded_asset(mat)
    log(f"saved {full}")


def make_unlit_emissive(name, color, opacity_from_uv=False):
    """Излучающий unlit-материал (трейсер, вспышка). Additive, чтобы складывался с фоном."""
    path = "/Game/Weapons/FX"
    full = f"{path}/{name}"
    if eal.does_asset_exist(full):
        mat = eal.load_asset(full)
        mel.delete_all_material_expressions(mat)
    else:
        mat = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, path, unreal.Material, unreal.MaterialFactoryNew())
    mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_ADDITIVE)
    mat.set_editor_property("two_sided", True)
    c = mel.create_material_expression(mat, unreal.MaterialExpressionConstant3Vector, -400, 0)
    c.set_editor_property("constant", unreal.LinearColor(*color, 1.0))
    mel.connect_material_property(c, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.recompile_material(mat)
    eal.save_loaded_asset(mat)
    log(f"saved {full}")


make_decal()
make_unlit_emissive("M_Tracer", (8.0, 6.0, 3.0))        # тёплый, яркий - additive даст свечение
make_unlit_emissive("M_MuzzleFlash", (30.0, 18.0, 6.0))
unreal.log("[weapon-assets] DONE")
