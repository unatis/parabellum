"""
Импорт карт поверхностей (Import/Surfaces) в /Game/Surfaces и сборка мастер-материала.

Материал трипланарный: уровень собран из растянутых кубов, и обычные UV дали бы на каждой грани
свою плотность текселей. Координаты берутся из мировой позиции, три проекции смешиваются по
нормали - масштаб текстуры одинаков на полу, стене и торце независимо от масштаба актора.
Нормали тоже смешиваются трипланарно, поэтому материал работает в МИРОВЫХ нормалях
(tangent_space_normal = False) - иначе рельеф разъехался бы по граням.

Карты ждутся как <Surface>_BC / _R / _AO / _N, .jpg или .png. Megascans раскладывает
Tools/megascans_prepare.py; если их нет, берутся процедурные из Tools/blender/make_surfaces.py.
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

# Поверхности и то, наносится ли на них разметка дистанций.
SURFACES = [("Concrete_Wall", False), ("Concrete_Floor", True), ("Concrete_Ceiling", False),
            ("Steel_Painted", False)]
# Карты Megascans сняты с площадки 2x2 м - тайл в сантиметрах мира ровно такой же.
TILE_CM = 200.0

TRIPLANAR = """
// Три развёртки по мировым осям, смешанные по нормали. Степень 6 делает переход узким,
// иначе на скосах видно размазанный шов.
float3 p = WorldPos / max(TileSize, 1.0f);
float3 w = pow(abs(WorldNormal), 6.0f);
w /= max(w.x + w.y + w.z, 1e-4f);
float3 cx = Texture2DSample(Tex, TexSampler, p.yz).rgb;
float3 cy = Texture2DSample(Tex, TexSampler, p.xz).rgb;
float3 cz = Texture2DSample(Tex, TexSampler, p.xy).rgb;
return cx * w.x + cy * w.y + cz * w.z;
"""

TRIPLANAR_N = """
// Трипланарные нормали: каждая проекция распаковывается и разворачивается в мир по своей оси,
// знак берётся от нормали грани. Результат - нормаль В МИРОВЫХ координатах.
float3 p = WorldPos / max(TileSize, 1.0f);
float3 wn = WorldNormal;
float3 w = pow(abs(wn), 6.0f);
w /= max(w.x + w.y + w.z, 1e-4f);
float3 sgn = sign(wn);
// Карта нормалей лежит в двух каналах (BC5): синий в текстуре не хранится и восстанавливается
// здесь. Если брать .b как есть, получается -1 - нормали разворачиваются внутрь и всё чернеет.
float2 rx = Texture2DSample(Tex, TexSampler, p.yz).rg * 2.0f - 1.0f;
float2 ry = Texture2DSample(Tex, TexSampler, p.xz).rg * 2.0f - 1.0f;
float2 rz = Texture2DSample(Tex, TexSampler, p.xy).rg * 2.0f - 1.0f;
float3 tx = float3(rx, sqrt(saturate(1.0f - dot(rx, rx))));
float3 ty = float3(ry, sqrt(saturate(1.0f - dot(ry, ry))));
float3 tz = float3(rz, sqrt(saturate(1.0f - dot(rz, rz))));
float3 nx = float3(tx.z * sgn.x, tx.x, tx.y);
float3 ny = float3(ty.x, ty.z * sgn.y, ty.y);
float3 nz = float3(tz.x, tz.y, tz.z * sgn.z);
return normalize(nx * w.x + ny * w.y + nz * w.z);
"""

LINES = """
// Разметка дистанций: краска по бетону. Ширина в МИРОВЫХ единицах, поэтому вдали линии
// сужаются как настоящие, а не расползаются в полосы.
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


def find_map(surface, role):
    for ext in (".jpg", ".png"):
        p = os.path.join(SRC, f"{surface}_{role}{ext}")
        if os.path.exists(p):
            return p
    return None


def import_texture(path, srgb, compression):
    name = os.path.splitext(os.path.basename(path))[0]
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


def triplanar(mat, x, y, param, code, out_type, wp, wn, tile):
    node = mel.create_material_expression(mat, unreal.MaterialExpressionCustom, x, y)
    node.set_editor_property("code", code)
    node.set_editor_property("output_type", out_type)
    ins = []
    for n in ("WorldPos", "WorldNormal", "TileSize", "Tex"):
        ci = unreal.CustomInput()
        ci.set_editor_property("input_name", n)
        ins.append(ci)
    node.set_editor_property("inputs", ins)
    tex = mel.create_material_expression(mat, unreal.MaterialExpressionTextureObjectParameter, x - 400, y + 150)
    tex.set_editor_property("parameter_name", param)
    for src, dst in ((wp, "WorldPos"), (wn, "WorldNormal"), (tile, "TileSize"), (tex, "Tex")):
        if not mel.connect_material_expressions(src, "", node, dst):
            raise RuntimeError(f"не соединилось: {param}/{dst}")
    return node


def red(mat, node, x, y):
    m = mel.create_material_expression(mat, unreal.MaterialExpressionComponentMask, x, y)
    m.set_editor_property("r", True)
    m.set_editor_property("g", False)
    m.set_editor_property("b", False)
    mel.connect_material_expressions(node, "", m, "")
    return m


def make_master():
    name, path = "M_Surface", DEST
    full = f"{path}/{name}"
    if eal.does_asset_exist(full):
        mat = eal.load_asset(full)
        mel.delete_all_material_expressions(mat)
    else:
        mat = tools.create_asset(name, path, unreal.Material, unreal.MaterialFactoryNew())
    # Нормали приходят из трипланарного смешения уже в мировых координатах.
    mat.set_editor_property("tangent_space_normal", False)

    wp = mel.create_material_expression(mat, unreal.MaterialExpressionWorldPosition, -1400, 0)
    wn = mel.create_material_expression(mat, unreal.MaterialExpressionVertexNormalWS, -1400, 150)
    tile = mel.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -1400, 300)
    tile.set_editor_property("parameter_name", "TileSize")
    tile.set_editor_property("default_value", TILE_CM)

    F3 = unreal.CustomMaterialOutputType.CMOT_FLOAT3
    bc = triplanar(mat, -700, -300, "BaseColorMap", TRIPLANAR, F3, wp, wn, tile)
    rg = triplanar(mat, -700, 100, "RoughnessMap", TRIPLANAR, F3, wp, wn, tile)
    ao = triplanar(mat, -700, 450, "AOMap", TRIPLANAR, F3, wp, wn, tile)
    nm = triplanar(mat, -700, 800, "NormalMap", TRIPLANAR_N, F3, wp, wn, tile)
    # PBL_NO_NORMAL=1 - собрать материал без рельефа: так проверяется, что тёмная картинка
    # идёт от нормалей, а не от альбедо сканов.
    if os.environ.get("PBL_NO_NORMAL") != "1":
        mel.connect_material_property(nm, "", unreal.MaterialProperty.MP_NORMAL)
    mel.connect_material_property(red(mat, ao, -350, 450), "", unreal.MaterialProperty.MP_AMBIENT_OCCLUSION)
    rough = red(mat, rg, -350, 100)

    # Разметка поверх бетона: включается параметром, поэтому пол и стены - один материал.
    lines = mel.create_material_expression(mat, unreal.MaterialExpressionCustom, -1000, 1200)
    lines.set_editor_property("code", LINES)
    lines.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT1)
    ci = unreal.CustomInput()
    ci.set_editor_property("input_name", "WorldPos")
    lines.set_editor_property("inputs", [ci])
    mel.connect_material_expressions(wp, "", lines, "WorldPos")
    strength = mel.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -1000, 1400)
    strength.set_editor_property("parameter_name", "LineStrength")
    strength.set_editor_property("default_value", 0.0)
    alpha = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, -700, 1300)
    mel.connect_material_expressions(lines, "", alpha, "A")
    mel.connect_material_expressions(strength, "", alpha, "B")
    paint = mel.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -700, 1500)
    paint.set_editor_property("parameter_name", "LineColor")
    paint.set_editor_property("default_value", unreal.LinearColor(0.34, 0.34, 0.33, 1.0))

    bc_mix = mel.create_material_expression(mat, unreal.MaterialExpressionLinearInterpolate, -350, -300)
    mel.connect_material_expressions(bc, "", bc_mix, "A")
    mel.connect_material_expressions(paint, "", bc_mix, "B")
    mel.connect_material_expressions(alpha, "", bc_mix, "Alpha")
    mel.connect_material_property(bc_mix, "", unreal.MaterialProperty.MP_BASE_COLOR)

    paint_r = mel.create_material_expression(mat, unreal.MaterialExpressionConstant, -350, 250)
    paint_r.set_editor_property("r", 0.55)
    r_mix = mel.create_material_expression(mat, unreal.MaterialExpressionLinearInterpolate, -100, 150)
    mel.connect_material_expressions(rough, "", r_mix, "A")
    mel.connect_material_expressions(paint_r, "", r_mix, "B")
    mel.connect_material_expressions(alpha, "", r_mix, "Alpha")
    mel.connect_material_property(r_mix, "", unreal.MaterialProperty.MP_ROUGHNESS)

    mel.recompile_material(mat)
    eal.save_loaded_asset(mat)
    log(f"master {full} (мировые нормали, тайл {TILE_CM:.0f} см)")
    return mat


TC = unreal.TextureCompressionSettings
ROLES = [("BC", True, TC.TC_DEFAULT, "BaseColorMap"),
         ("R", False, TC.TC_MASKS, "RoughnessMap"),
         ("AO", False, TC.TC_MASKS, "AOMap"),
         ("N", False, TC.TC_NORMALMAP, "NormalMap")]

master = make_master()
for surface, lines in SURFACES:
    params = {}
    for role, srgb, comp, param in ROLES:
        p = find_map(surface, role)
        if not p:
            log(f"{surface}: нет карты {role} - пропуск поверхности")
            params = None
            break
        params[param] = import_texture(p, srgb, comp)
    if not params:
        continue
    name = f"MI_{surface}"
    full = f"{DEST}/{name}"
    if eal.does_asset_exist(full):
        eal.delete_asset(full)
    mi = tools.create_asset(name, DEST, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    mel.set_material_instance_parent(mi, master)
    for param, tex in params.items():
        mel.set_material_instance_texture_parameter_value(mi, param, tex)
    if lines:
        mel.set_material_instance_scalar_parameter_value(mi, "LineStrength", 1.0)
    eal.save_loaded_asset(mi)
    log(f"instance {full}{' + разметка' if lines else ''}")
log("DONE")
