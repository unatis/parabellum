"""
Импорт карт поверхностей (Import/Surfaces/*.png) в /Game/Surfaces и сборка мастер-материала.

Материал трипланарный: уровень собран из растянутых кубов, и обычные UV дали бы на каждой грани
свою плотность текселей. Здесь координаты берутся из мировой позиции, а три проекции смешиваются
по нормали, поэтому масштаб текстуры одинаков на полу, стене и торце - независимо от масштаба актора.

Карты Megascans кладутся сюда же под теми же именами - материал менять не нужно.
Запуск: Tools/import_surfaces.bat. Идемпотентен.
"""
import os
import unreal

ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
SRC = os.path.join(ROOT, "Import", "Surfaces")
DEST = "/Game/Surfaces"
eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()

SURFACES = ["Concrete_Wall", "Concrete_Floor", "Steel_Painted"]
# Масштаб тайла в сантиметрах мира: карты сделаны на 2 м.
TILE_CM = 200.0

TRIPLANAR = """
// Трипланарная проекция: три развёртки по мировым осям, смешанные по нормали.
// Степень 6 у весов делает переход узким, иначе на скосах видно «размазанный» шов.
float3 p = WorldPos / max(TileSize, 1.0f);
float3 w = pow(abs(WorldNormal), 6.0f);
w /= max(w.x + w.y + w.z, 1e-4f);
float3 cx = Texture2DSample(Tex, TexSampler, p.yz).rgb;
float3 cy = Texture2DSample(Tex, TexSampler, p.xz).rgb;
float3 cz = Texture2DSample(Tex, TexSampler, p.xy).rgb;
return cx * w.x + cy * w.y + cz * w.z;
"""


LINES = """
// Разметка дистанций: белая краска по бетону, ширина задана в МИРОВЫХ единицах, поэтому
// в перспективе линии сужаются, как настоящие, а не расползаются в полосы.
float2 q  = WorldPos.xy / 100.0f;
float2 fw = max(fwidth(q), 1e-5f);
float  w1 = 0.010f;
float2 d1 = 0.5f - abs(frac(q) - 0.5f);
float2 a1 = saturate(w1 / max(fw, w1));
float2 c1 = (1.0f - smoothstep(w1 - fw, w1 + fw, d1)) * a1;
float2 q5 = q / 5.0f, f5 = fw / 5.0f;
float  w5 = 0.005f;
float2 d5 = 0.5f - abs(frac(q5) - 0.5f);
float2 a5 = saturate(w5 / max(f5, w5));
float2 c5 = (1.0f - smoothstep(w5 - f5, w5 + f5, d5)) * a5;
return saturate(max(c1.x, c1.y) * 0.25f + max(c5.x, c5.y));
"""


def log(m):
    unreal.log(f"[surfaces] {m}")


def import_texture(name, srgb, compression):
    path = os.path.join(SRC, name + ".png")
    if not os.path.exists(path):
        raise RuntimeError(f"нет файла {path}")
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", path)
    task.set_editor_property("destination_path", DEST)
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("save", True)
    task.set_editor_property("factory", unreal.TextureFactory())
    tools.import_asset_tasks([task])
    tex = eal.load_asset(f"{DEST}/{name}")
    if tex is None:
        raise RuntimeError(f"импорт не дал ассета: {name}")
    tex.set_editor_property("srgb", srgb)
    tex.set_editor_property("compression_settings", compression)
    eal.save_loaded_asset(tex)
    return tex


def triplanar_node(mat, x, y, tex_param_name, world_pos, world_normal, tile):
    """Custom-нода с трипланарной выборкой; текстура приходит параметром, чтобы менять её в инстансах."""
    node = mel.create_material_expression(mat, unreal.MaterialExpressionCustom, x, y)
    node.set_editor_property("code", TRIPLANAR)
    node.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    ins = []
    for n in ("WorldPos", "WorldNormal", "TileSize", "Tex"):
        ci = unreal.CustomInput()
        ci.set_editor_property("input_name", n)
        ins.append(ci)
    node.set_editor_property("inputs", ins)

    tex = mel.create_material_expression(mat, unreal.MaterialExpressionTextureObjectParameter, x - 400, y + 200)
    tex.set_editor_property("parameter_name", tex_param_name)
    for src, dst in ((world_pos, "WorldPos"), (world_normal, "WorldNormal"), (tile, "TileSize"), (tex, "Tex")):
        if not mel.connect_material_expressions(src, "", node, dst):
            raise RuntimeError(f"не соединилось: {dst}")
    return node, tex


def make_master():
    name, path = "M_Surface", "/Game/Surfaces"
    full = f"{path}/{name}"
    if eal.does_asset_exist(full):
        mat = eal.load_asset(full)
        mel.delete_all_material_expressions(mat)
    else:
        mat = tools.create_asset(name, path, unreal.Material, unreal.MaterialFactoryNew())

    wp = mel.create_material_expression(mat, unreal.MaterialExpressionWorldPosition, -1200, 0)
    wn = mel.create_material_expression(mat, unreal.MaterialExpressionVertexNormalWS, -1200, 200)
    tile = mel.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -1200, 400)
    tile.set_editor_property("parameter_name", "TileSize")
    tile.set_editor_property("default_value", TILE_CM)

    bc, _ = triplanar_node(mat, -500, -200, "BaseColorMap", wp, wn, tile)

    msk, _ = triplanar_node(mat, -500, 400, "MaskMap", wp, wn, tile)
    rough = mel.create_material_expression(mat, unreal.MaterialExpressionComponentMask, -200, 400)
    rough.set_editor_property("r", True)
    rough.set_editor_property("g", False)
    rough.set_editor_property("b", False)
    mel.connect_material_expressions(msk, "", rough, "")

    ao = mel.create_material_expression(mat, unreal.MaterialExpressionComponentMask, -200, 600)
    ao.set_editor_property("r", False)
    ao.set_editor_property("g", True)
    ao.set_editor_property("b", False)
    mel.connect_material_expressions(msk, "", ao, "")
    mel.connect_material_property(ao, "", unreal.MaterialProperty.MP_AMBIENT_OCCLUSION)

    # Разметка поверх бетона: включается параметром, поэтому пол и стены - один материал.
    lines = mel.create_material_expression(mat, unreal.MaterialExpressionCustom, -800, 900)
    lines.set_editor_property("code", LINES)
    lines.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT1)
    ci = unreal.CustomInput()
    ci.set_editor_property("input_name", "WorldPos")
    lines.set_editor_property("inputs", [ci])
    mel.connect_material_expressions(wp, "", lines, "WorldPos")

    strength = mel.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -800, 1100)
    strength.set_editor_property("parameter_name", "LineStrength")
    strength.set_editor_property("default_value", 0.0)
    alpha = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, -500, 1000)
    mel.connect_material_expressions(lines, "", alpha, "A")
    mel.connect_material_expressions(strength, "", alpha, "B")

    paint = mel.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -500, 1200)
    paint.set_editor_property("parameter_name", "LineColor")
    paint.set_editor_property("default_value", unreal.LinearColor(0.32, 0.32, 0.31, 1.0))

    bc_mix = mel.create_material_expression(mat, unreal.MaterialExpressionLinearInterpolate, -200, 0)
    mel.connect_material_expressions(bc, "", bc_mix, "A")
    mel.connect_material_expressions(paint, "", bc_mix, "B")
    mel.connect_material_expressions(alpha, "", bc_mix, "Alpha")
    mel.connect_material_property(bc_mix, "", unreal.MaterialProperty.MP_BASE_COLOR)

    paint_r = mel.create_material_expression(mat, unreal.MaterialExpressionConstant, -200, 300)
    paint_r.set_editor_property("r", 0.55)
    r_mix = mel.create_material_expression(mat, unreal.MaterialExpressionLinearInterpolate, 0, 400)
    mel.connect_material_expressions(rough, "", r_mix, "A")
    mel.connect_material_expressions(paint_r, "", r_mix, "B")
    mel.connect_material_expressions(alpha, "", r_mix, "Alpha")
    mel.connect_material_property(r_mix, "", unreal.MaterialProperty.MP_ROUGHNESS)

    mel.recompile_material(mat)
    eal.save_loaded_asset(mat)
    log(f"master {full}")
    return mat


def make_instance(master, surface, bc_tex, msk_tex, lines=False, name=None):
    name = name or f"MI_{surface}"
    full = f"{DEST}/{name}"
    if eal.does_asset_exist(full):
        eal.delete_asset(full)
    mi = tools.create_asset(name, DEST, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    mel.set_material_instance_parent(mi, master)
    mel.set_material_instance_texture_parameter_value(mi, "BaseColorMap", bc_tex)
    mel.set_material_instance_texture_parameter_value(mi, "MaskMap", msk_tex)
    if lines:
        mel.set_material_instance_scalar_parameter_value(mi, "LineStrength", 1.0)
    eal.save_loaded_asset(mi)
    log(f"instance {full}")
    return mi


master = make_master()
for s in SURFACES:
    bc = import_texture(f"{s}_BC", True, unreal.TextureCompressionSettings.TC_DEFAULT)
    msk = import_texture(f"{s}_MSK", False, unreal.TextureCompressionSettings.TC_MASKS)
    import_texture(f"{s}_N", False, unreal.TextureCompressionSettings.TC_NORMALMAP)
    make_instance(master, s, bc, msk)
    if s == "Concrete_Floor":
        # Пол тира и зала: тот же бетон, но с нанесённой разметкой дистанций.
        make_instance(master, s, bc, msk, lines=True, name="MI_Concrete_FloorLines")
log("DONE")
