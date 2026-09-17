"""
Патрон 9x19 Parabellum: боевой патрон, стреляная гильза и пуля отдельно.
Размеры - из стандарта на патрон (SAAMI/CIP), а не с фотографий: гильза обязана совпадать
с чертежом стандарта, иначе она не ляжет ни в патронник, ни в магазин.
Запуск: Tools/blender/make_cartridge.bat -> Import/Ammo/9x19.fbx + .json + preview.png
"""
import bpy, os, sys, json, math
import mathutils

sys.path.append(os.path.dirname(os.path.abspath(__file__)))
import importlib, weapon_parts_lib as L
importlib.reload(L)

OUT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "Import", "Ammo"))
os.makedirs(OUT, exist_ok=True)

bpy.ops.object.select_all(action="SELECT")
bpy.ops.object.delete()
bpy.context.scene.unit_settings.system = "METRIC"
bpy.context.scene.unit_settings.scale_length = 0.001   # 1 BU = 1 мм, как в моделях оружия

# --- Размеры, мм. spec - стандарт на патрон (SAAMI/CIP), est - обоснованная оценка.
SPEC = {
    "case_len":       (19.15, "spec"),   # длина гильзы
    "overall_len":    (29.69, "spec"),   # длина патрона
    "rim_dia":        (9.96,  "spec"),   # диаметр закраины
    "rim_thick":      (1.14,  "spec"),
    "base_dia":       (9.93,  "spec"),   # диаметр у донца
    "neck_dia":       (9.65,  "spec"),   # диаметр у дульца: гильза слегка коническая
    "bullet_dia":     (9.02,  "spec"),   # .355 дюйма
    "primer_dia":     (4.45,  "spec"),   # капсюль Small Pistol, .175 дюйма
    "bullet_mass_g":  (8.03,  "spec"),   # 124 грана FMJ
    "groove_dia":     (8.80,  "est"),    # дно проточки под выбрасыватель
    "groove_width":   (1.45,  "est"),
    "wall_mouth":     (0.30,  "est"),    # толщина стенки у дульца
    "web_top":        (3.70,  "est"),    # где начинается пороховая камора
    "primer_depth":   (3.00,  "est"),
    "bullet_len":     (15.30, "est"),    # полная длина пули 124 грана FMJ RN
    "nose_len":       (5.50,  "est"),    # оживальная часть
    "case_mass_g":    (4.00,  "est"),    # латунная гильза 9x19
    "fired_neck_dia": (9.85,  "est"),    # после выстрела дульце раздаётся по патроннику
}


class SpecView:
    def __init__(self, d):
        self.d = d

    def __getitem__(self, k):
        return self.d[k][0]

    def src(self, k):
        return self.d[k][1]


S = SpecView(SPEC)
SEG = 32


def case_profile(neck_dia, mouth_bell):
    """Наружный контур гильзы (вдоль_оси, радиус) от донца до дульца."""
    rim, base, neck = S["rim_dia"] / 2, S["base_dia"] / 2, neck_dia / 2
    gr, gw, rt, cl = S["groove_dia"] / 2, S["groove_width"], S["rim_thick"], S["case_len"]
    x_out = rt + 0.25 + gw + 0.6
    return [
        (0.0, rim),
        (rt, rim),
        (rt + 0.25, gr),
        (rt + 0.25 + gw, gr),
        (x_out, base),
        (cl - 1.0, neck + (base - neck) * 1.0 / (cl - x_out)),
        (cl, neck + mouth_bell),
    ]


def build_case(name, fired):
    """Гильза: тело вращения минус пороховая камора, гнездо капсюля и запальное отверстие."""
    neck = S["fired_neck_dia"] if fired else S["neck_dia"]
    ob = L.revolve(name, case_profile(neck, 0.08 if fired else 0.0), SEG)
    r_mouth = neck / 2 - S["wall_mouth"]
    ob = L.boolean(ob, L.revolve(name + "_bore", [(S["web_top"], r_mouth - 0.55), (S["case_len"] + 1.0, r_mouth)], SEG))
    ob = L.boolean(ob, L.revolve(name + "_pocket", [(-0.5, S["primer_dia"] / 2), (S["primer_depth"], S["primer_dia"] / 2)], SEG))
    ob = L.boolean(ob, L.revolve(name + "_flash", [(S["primer_depth"] - 0.1, 0.75), (S["web_top"] + 0.2, 0.75)], 12))
    ob.name = name
    return ob


def build_primer(name, struck):
    """Капсюль в гнезде. У стреляной гильзы - с наколом от ударника."""
    r = S["primer_dia"] / 2 - 0.02
    ob = L.revolve(name, [(0.02, r), (S["primer_depth"] - 0.1, r)], SEG)
    if struck:
        bpy.ops.mesh.primitive_uv_sphere_add(radius=0.8, segments=16, ring_count=8, location=(0.0, 0.0, 0.0))
        dent = bpy.context.active_object
        dent.name = name + "_dent"
        ob = L.boolean(ob, dent)
    ob.name = name
    return ob


def build_bullet(name, x_base):
    """Пуля 124 грана FMJ: цилиндрическая ведущая часть и касательная огива."""
    R, Ln, BL = S["bullet_dia"] / 2, S["nose_len"], S["bullet_len"]
    rho = (R * R + Ln * Ln) / (2 * R)
    prof = [(x_base, R - 0.35), (x_base + 0.35, R)]
    x_nose = x_base + BL - Ln
    prof.append((x_nose, R))
    for i in range(1, 13):
        d = Ln * i / 12.0
        r = math.sqrt(max(rho * rho - d * d, 0.0)) - (rho - R)
        prof.append((x_nose + d, max(r, 0.30)))
    return L.revolve(name, prof, SEG)


BRASS = L.material("Brass", (0.62, 0.45, 0.16), 1.0, 0.24)
BRASS_FIRED = L.material("Brass_Fired", (0.50, 0.37, 0.15), 1.0, 0.38)
JACKET = L.material("Jacket_Cupronickel", (0.68, 0.47, 0.32), 1.0, 0.28)
PRIMER_M = L.material("Primer_Brass", (0.66, 0.52, 0.22), 1.0, 0.30)

parts, groups = {}, {}

# Боевой патрон: донце пули стоит так, чтобы носик пришёлся ровно на длину патрона по стандарту.
# Отсюда и глубина посадки: case_len - x_base = 4.8 мм в гильзе.
x_bullet_base = S["overall_len"] - S["bullet_len"]
case_live = build_case("Round9x19", False)
bullet_live = build_bullet("Round9x19_bullet", x_bullet_base)
parts["Round9x19"] = case_live
groups["Round9x19"] = [(case_live, BRASS), (build_primer("Round9x19_primer", False), PRIMER_M), (bullet_live, JACKET)]

# Стреляная гильза: раздутое дульце, потемневшая латунь, наколотый капсюль.
case_fired = build_case("Case9x19", True)
parts["Case9x19"] = case_fired
groups["Case9x19"] = [(case_fired, BRASS_FIRED), (build_primer("Case9x19_primer", True), PRIMER_M)]

# Пуля отдельно: и как снаряд, и как деталь в разрезе патрона.
bullet_solo = build_bullet("Bullet9x19", 0.0)
parts["Bullet9x19"] = bullet_solo
groups["Bullet9x19"] = [(bullet_solo, JACKET)]

# --- Проверка против стандарта ---
print("@@ --- 9x19 vs SAAMI/CIP (mm) ---")
meas = {
    "case_len": L.dims(case_live)[0],
    "rim_dia": max(L.dims(case_live)[1], L.dims(case_live)[2]),
    "bullet_dia": max(L.dims(bullet_solo)[1], L.dims(bullet_solo)[2]),
    "bullet_len": L.dims(bullet_solo)[0],
}
pts = [mathutils.Vector(c) for ob in (case_live, bullet_live) for c in ob.bound_box]
meas["overall_len"] = max(p[0] for p in pts) - min(p[0] for p in pts)
ok = True
for k in sorted(meas):
    v, ref = meas[k], S[k]
    err = (v - ref) / ref * 100.0
    good = abs(err) <= 2.0
    ok = ok and good
    print(f"@@ {k:13s} model {v:6.2f}  spec {ref:6.2f}  {err:+5.1f}%  {'OK' if good else 'FAIL'}  [{SPEC[k][1]}]")

by_src = {}
for k, (_, src) in SPEC.items():
    by_src.setdefault(src, []).append(k)
documented = len(by_src.get("spec", []))
print(f"@@ --- provenance: {documented}/{len(SPEC)} dimensions from the cartridge standard ---")

# --- Фаски, сглаживание, материалы; каждое изделие сводится в один меш ---
report = {}
for name, members in groups.items():
    for ob, mat in members:
        L.bevel(ob, width=0.12, segments=2)
        L.shade_smooth(ob, angle_deg=40.0)
        L.assign(ob, mat)
    keep = parts[name]
    bpy.ops.object.select_all(action="DESELECT")
    for ob, _ in members:
        ob.select_set(True)
    bpy.context.view_layer.objects.active = keep
    if len(members) > 1:
        bpy.ops.object.join()
    keep.name = name
    d = L.dims(keep)
    # Начало координат - в середине изделия: удобно и спавнить, и вращать в полёте.
    cen = sum((mathutils.Vector(c) for c in keep.bound_box), mathutils.Vector()) / 8.0
    L.move(keep, -cen)
    report[name] = {"size_mm": [round(x, 2) for x in d]}
    print(f"@@ {name:12s} {d[0]:6.2f} x {d[1]:5.2f} x {d[2]:5.2f} mm, {len(keep.data.polygons)} polys")

report["_mass_g"] = {"Case9x19": S["case_mass_g"], "Bullet9x19": S["bullet_mass_g"],
                     "Round9x19": round(S["case_mass_g"] + S["bullet_mass_g"] + 0.4, 2)}
report["_check"] = {k: {"model": round(v, 2), "spec": S[k], "source": SPEC[k][1]} for k, v in meas.items()}
report["_provenance"] = {"sources": {k: v[1] for k, v in SPEC.items()}, "documented": documented, "total": len(SPEC)}

bpy.ops.object.select_all(action="DESELECT")
for ob in parts.values():
    ob.select_set(True)
print("@@ exporting: " + ", ".join(sorted(f"{o.name}/{o.data.name}" for o in bpy.context.selected_objects)))
bpy.ops.export_scene.fbx(filepath=os.path.join(OUT, "9x19.fbx"), use_selection=True, object_types={"MESH"},
                         apply_unit_scale=True, apply_scale_options="FBX_SCALE_NONE", axis_forward="-Y", axis_up="Z",
                         bake_space_transform=True, mesh_smooth_type="FACE")
with open(os.path.join(OUT, "9x19.json"), "w", encoding="utf-8") as f:
    json.dump({"cartridge": "9x19", "spec": {k: v[0] for k, v in SPEC.items()}, "parts": report}, f, indent=1, ensure_ascii=False)

# --- Превью: три изделия в ряд ---
L.move(parts["Case9x19"], mathutils.Vector((-5.0, 0.0, 13.0)))
L.move(parts["Bullet9x19"], mathutils.Vector((-7.0, 0.0, -13.0)))
sc = bpy.context.scene
sc.render.engine = "CYCLES"
sc.cycles.samples = 96
sc.cycles.use_denoising = True
world = bpy.data.worlds.new("W")
sc.world = world
world.use_nodes = True
world.node_tree.nodes["Background"].inputs[0].default_value = (0.05, 0.055, 0.065, 1.0)
world.node_tree.nodes["Background"].inputs[1].default_value = 1.4
for (loc, rot, size, power) in (((-20, -60, 60), (math.radians(40), 0, 0), 80, 12000),
                                ((40, -50, 20), (math.radians(78), 0, math.radians(38)), 60, 6000),
                                ((-60, 40, 30), (math.radians(65), 0, math.radians(200)), 70, 9000)):
    bpy.ops.object.light_add(type="AREA", location=loc, rotation=rot)
    lt = bpy.context.active_object
    lt.data.size = size
    lt.data.energy = power
sc.render.resolution_x, sc.render.resolution_y = 1200, 700
sc.render.filepath = os.path.join(OUT, "preview.png")
bpy.ops.object.camera_add(location=(0, -140, 0), rotation=(math.radians(90), 0, 0))
sc.camera = bpy.context.active_object
sc.camera.data.lens = 75
bpy.ops.render.render(write_still=True)
print(f"@@ exported to {OUT}; dimension check {'PASSED' if ok else 'FAILED'}")
