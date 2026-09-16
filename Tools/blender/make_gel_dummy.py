"""
Гелевая кукла: гуманоид 176 см (мужчина ~75 кг) из эллиптических сечений, разрезанный на части с именами частей тела
из Content/Data/BodyLayers.csv. Каждая часть - отдельный объект с origin в геометрическом центре (нужно PBLMaterialBlock:
высота от центра для полос рёбер, ось для кости конечности). Единицы - сантиметры.
Запуск: Tools/blender/make_gel_dummy.bat -> Import/GelDummy/GelDummy.fbx + GelDummy.json (центры частей, см).
"""
import bpy, bmesh, json, math, os, sys

OUT_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "Import", "GelDummy")
OUT_DIR = os.path.abspath(OUT_DIR)
os.makedirs(OUT_DIR, exist_ok=True)

bpy.ops.wm.read_factory_settings(use_empty=True)
scene = bpy.context.scene
scene.unit_settings.system = "METRIC"
scene.unit_settings.scale_length = 0.01   # 1 BU = 1 см (как в Unreal)


def lofted(name, rings, segments=24, part=None, cap=True):
    """rings: список (z, cx, cy, rx, ry) - эллиптическое сечение на высоте z (см). Строит замкнутую поверхность."""
    bm = bmesh.new()
    loops = []
    for (z, cx, cy, rx, ry) in rings:
        loop = []
        for i in range(segments):
            a = 2 * math.pi * i / segments
            loop.append(bm.verts.new((cx + rx * math.cos(a), cy + ry * math.sin(a), z)))
        loops.append(loop)
    for a, b in zip(loops[:-1], loops[1:]):
        for i in range(segments):
            bm.faces.new((a[i], a[(i + 1) % segments], b[(i + 1) % segments], b[i]))
    if cap:
        bm.faces.new(loops[0][::-1])
        bm.faces.new(loops[-1])
    bm.normal_update()
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    bm.free()
    ob = bpy.data.objects.new(name, me)
    scene.collection.objects.link(ob)
    ob["BodyPart"] = part or name
    return ob


def ellipsoid(name, cx, cy, cz, rx, ry, rz, part=None, n=10):
    rings = []
    for k in range(n + 1):
        t = -math.pi / 2 + math.pi * k / n
        f = math.cos(t)
        rings.append((cz + rz * math.sin(t), cx, cy, max(rx * f, 0.05), max(ry * f, 0.05)))
    return lofted(name, rings, part=part)


parts = []
# Голова: эллипсоид 19 (глубина X) x 15.5 (ширина Y) x 23 (высота), центр 164 см
parts.append(ellipsoid("Head", 0, 0, 164, 9.5, 7.75, 11.5, "Head"))
# Шея: r 6, 145..153
parts.append(lofted("Neck", [(145, 0, 0, 6, 6), (153, 0, 0, 6, 6)], part="Neck"))
# Торс 85..145: плечи широкие (грудь 24 глубина, 38 ширина), талия уже (20 x 30)
parts.append(lofted("Torso", [(85, 0, 0, 10.5, 15.5), (100, 0, 0, 11, 16), (120, 0, 0, 12, 18), (135, 0, 0, 12, 19), (145, 0, 0, 10, 17)], part="Torso"))
# Таз 63..85: 24 x 36
parts.append(lofted("Pelvis", [(63, 0, 0, 10, 15), (72, 0, 0, 12, 18), (85, 0, 0, 10.5, 16)], part="Pelvis"))
# Руки: от плеча (z 145) до кисти (z 80), y = ±26 (сужение 5 -> 4 см), ладонь как эллипсоид
for side, sy in (("L", 1), ("R", -1)):
    parts.append(lofted(f"Arm{side}", [(80, 0, sy * 26, 4, 4), (115, 0, sy * 26, 4.5, 4.5), (147, 0, sy * 25, 5.5, 5.5)], part="Arm"))
    parts.append(ellipsoid(f"Hand{side}", 0, sy * 26, 72, 4.5, 2.5, 9, "Arm"))
# Ноги: бедро r 8 (z 85 -> 45), голень r 6 -> 4.5 у лодыжки (z 8), y = ±10; стопа - коробка 26 x 9 x 7
for side, sy in (("L", 1), ("R", -1)):
    parts.append(lofted(f"Leg{side}", [(8, 0, sy * 10, 4.5, 4.5), (45, 0, sy * 10, 6, 6), (60, 0, sy * 10, 7, 7), (85, 0, sy * 10, 8, 8)], part="Leg"))
    parts.append(lofted(f"Foot{side}", [(0, 6, sy * 10, 13, 4.5), (8, 6, sy * 10, 12, 4.5)], part="Leg"))

meta = {}
for ob in parts:
    bpy.context.view_layer.objects.active = ob
    ob.select_set(True)
    bpy.ops.object.origin_set(type="ORIGIN_GEOMETRY", center="BOUNDS")
    c = ob.location
    meta[ob.name] = {"part": ob["BodyPart"], "center_cm": [round(c.x, 2), round(c.y, 2), round(c.z, 2)]}
    ob.select_set(False)

for ob in parts:
    ob.select_set(True)
fbx = os.path.join(OUT_DIR, "GelDummy.fbx")
bpy.ops.export_scene.fbx(filepath=fbx, use_selection=True, object_types={"MESH"}, mesh_smooth_type="FACE",
                         apply_unit_scale=True, apply_scale_options="FBX_SCALE_NONE", axis_forward="-Y", axis_up="Z",
                         use_mesh_modifiers=True, bake_space_transform=True, add_leaf_bones=False)
with open(os.path.join(OUT_DIR, "GelDummy.json"), "w", encoding="utf-8") as f:
    json.dump(meta, f, indent=1, ensure_ascii=False)
print("@@GD@@ exported", fbx, len(parts), "parts")
for k, v in meta.items():
    print("@@GD@@", k, v)
