"""
Colt M1911, образец 1911 года - второй образец, собранный тем же способом, что и Glock 17:
деталь строится числами из таблицы SPEC, затем габариты сборки сверяются с этой же таблицей.
Модель принимается, только если сошлась с источником.

Зачем именно 1911: это проверка, переносится ли способ. Схема та же браунинговская с коротким
ходом ствола, но запирание другое (качающаяся серьга вместо проушины с перекосом), разборка
другая (втулка ствола и заглушка пружины), и рукоять другая. Если способ работает и здесь,
дальше он сводится к архетипу и таблице.

Отличия образца 1911 от последующего 1911A1 (их и моделируем):
  - плоская задняя стенка рукояти, не выгнутая;
  - длинный спусковой крючок;
  - короткая пятка автоматического предохранителя;
  - нет выемок под пальцы за спуском;
  - мелкие прицельные приспособления.

Запуск: Tools/blender/make_colt1911.bat -> Import/Colt1911Parts/Colt1911_M1911.fbx + .json
"""
import bpy, os, sys, json, math
import mathutils

sys.path.append(os.path.dirname(os.path.abspath(__file__)))
import importlib, weapon_parts_lib as L
importlib.reload(L)

OUT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "Import", "Colt1911Parts"))
os.makedirs(OUT, exist_ok=True)
GEN = "M1911"

# --- Размеры, мм. Источник у каждого значения:
#   spec  - ТТХ производителя или армейское руководство
#   deriv - вычислено из других известных размеров
#   est   - обоснованное предположение по фотографиям и пропорциям (подлежит уточнению)
SPEC = {
    "overall_length":  (210.0, "spec"),  "barrel_length":   (127.0, "spec"),
    "height_with_mag": (133.0, "spec"),  "overall_width":   (32.5,  "spec"),
    "slide_width":     (22.9,  "spec"),  "sight_radius":    (165.0, "spec"),
    "bore_diameter":   (11.43, "spec"),  "mag_capacity":    (7,     "spec"),
    "weight_empty_g":  (1105.0, "spec"), "grip_angle_deg":  (18.0,  "spec"),
    # Реконструкция по пропорциям и фотографиям: обмера нет.
    "slide_length":    (197.0, "est"),   "slide_height":    (24.0,  "est"),
    "bushing_od":      (15.9,  "est"),   "bushing_len":     (14.0,  "est"),
    "plug_od":         (11.5,  "est"),   "barrel_outer":    (14.2,  "est"),
    "hammer_len":      (34.0,  "est"),   "hammer_w":        (5.0,   "est"),
    "thumb_safety_l":  (30.0,  "est"),   "grip_safety_l":   (46.0,  "est"),
    "trigger_len":     (26.0,  "est"),   "mainspring_h":    (58.0,  "est"),
    "grip_panel_l":    (92.0,  "est"),   "grip_panel_t":    (5.5,   "est"),
    "slide_stop_len":  (58.0,  "est"),   "recoil_guide_l":  (56.0,  "est"),
}


class SpecView:
    def __init__(self, d): self.d = d
    def __getitem__(self, k): return self.d[k][0]
    def src(self, k): return self.d[k][1]


S = SpecView(SPEC)

bpy.ops.object.select_all(action="SELECT")
bpy.ops.object.delete()
bpy.context.scene.unit_settings.system = "METRIC"
bpy.context.scene.unit_settings.scale_length = 0.001   # 1 BU = 1 мм

# Ось канала ствола на z=0, срез дула на x=0, оружие уходит назад по -x - как у Glock.
BORE_Z = 0.0
SLIDE_TOP_Z = BORE_Z + 14.0


def load_traced_profile():
    """
    Контур, снятый с фотографии (Tools/blender/trace_profile.py). Даёт то, чего нет в ТТХ:
    обвод затвора, скруглённую спусковую скобу, изгиб рукояти. Привязка по высоте - линия
    верха затвора: контур снят от верхней точки объекта, модель считает от оси канала,
    и связать их больше нечем.

    Силуэт даёт только НАРУЖНУЮ границу. Где кончается затвор и начинается рамка, в нём
    не написано - эта граница остаётся расчётной.
    """
    path = os.path.join(os.path.dirname(OUT), "Reference", "Colt1911_photo2", "profile.json")
    if not os.path.exists(path):
        print("@@ контура с фотографии нет, беру обводы из таблицы")
        return None
    with open(path, encoding="utf-8") as f:
        d = json.load(f)
    if d.get("slide_top_mm") is None:
        print("@@ в контуре нет датума верха затвора - беру обводы из таблицы")
        return None
    dz = SLIDE_TOP_Z - d["slide_top_mm"]
    print(f"@@ контур с фотографии {d['source']}: {len(d['top'])}+{len(d['bottom'])} точек, "
          f"сдвиг по высоте {dz:+.2f} мм")
    return {"top": [(x, y + dz) for x, y in d["top"]],
            "bottom": [(x, y + dz) for x, y in d["bottom"]],
            "source": d["source"], "height_mm": d["measured_height_mm"]}


TRACED = load_traced_profile()


def clip(pts, x_from, x_to):
    """Точки контура в пределах участка, по возрастанию x (от хвоста к дулу)."""
    return [(x, z) for x, z in pts if x_from <= x <= x_to]
parts = {}
bl = S["barrel_length"]

# --- Ствол: у 1911 он не с проушиной, а с серьгой. Патронник - утолщение у казённой части ---
CHAMBER = 24.0                                   # длина патронника чуть больше гильзы .45
barrel = L.revolve("Barrel", [(-bl, 11.6), (-bl + CHAMBER, 11.6), (-bl + CHAMBER + 4, 8.2),
                              (-26, 8.2), (-22, S["bushing_od"] / 2 - 0.3),
                              (-6, S["bushing_od"] / 2 - 0.3), (0, 7.4)], 24)
L.boolean(barrel, L.revolve("bore_cut", [(-bl - 2, S["bore_diameter"] / 2), (2, S["bore_diameter"] / 2)], 16))
# Проушина под серьгу: у 1911 ствол опускается на ней, а не на скосе.
barrel = L.boolean(barrel, L.box("lug", (18, 9, 12), (-bl + 30, 0, -10)), "UNION")
# Сама серьга - отдельная деталь, она и отличает схему от глоковской.
link = L.box("BarrelLink", (6, 4, 14), (-bl + 30, 0, -17))
barrel.name = "Barrel"
parts["Barrel"] = barrel
parts["BarrelLink"] = link

# --- Затвор: профиль сбоку, выдавленный на ширину; вырезаны окно выброса, паз рамки, канал ---
sl, sh = S["slide_length"], S["slide_height"]
top, bot = BORE_Z + 14.0, BORE_Z - 10.0
traced_top = clip(TRACED["top"], -sl, 0.0) if TRACED else []
if len(traced_top) >= 6:
    # Низ затвора остаётся расчётным - в силуэте его нет; верх идёт с фотографии.
    slide_profile = [(0, bot + 2), (-4, bot), (-sl + 6, bot), (-sl, bot + 5)] + traced_top
    print(f"@@ верх затвора: {len(traced_top)} точек с фотографии")
else:
    slide_profile = [(0, top - 3), (0, bot + 2), (-4, bot), (-sl + 6, bot), (-sl, bot + 5),
                     (-sl, top - 4), (-sl + 6, top), (-4, top)]
slide = L.profile_extrude("Slide", slide_profile, S["slide_width"])
# Окно выброса над патронником, справа; вырез уходит ниже оси канала, иначе гильзе не выйти.
L.boolean(slide, L.box("ejport", (46, 26, 18), (-bl + 19, 4, top - 7)))
L.boolean(slide, L.box("railcut", (sl + 4, S["slide_width"] - 8, 12), (-sl / 2, 0, bot + 5)))
L.boolean(slide, L.revolve("barrelcut", [(-bl - 2, 9.0), (6, 9.0)], 20))
# Гнездо под втулку ствола у дульного среза - характерная черта 1911.
L.boolean(slide, L.revolve("bushingseat", [(-S["bushing_len"], S["bushing_od"] / 2 + 0.2), (6, S["bushing_od"] / 2 + 0.2)], 20))
# Насечка под пальцы: у образца 1911 она вертикальная и в задней части.
for i in range(14):
    L.boolean(slide, L.box(f"serr{i}", (1.6, S["slide_width"] + 2, 10), (-sl + 22 + i * 3.6, 0, top - 8)))
slide = L.boolean(slide, L.box("rearsight", (4, 9, 4), (-6 - S["sight_radius"], 0, top + 1)), "UNION")
slide = L.boolean(slide, L.box("frontsight", (3, 2.5, 3.5), (-6, 0, top + 1)), "UNION")
slide.name = "Slide"
parts["Slide"] = slide

# --- Втулка ствола и заглушка возвратной пружины: по ним 1911 и разбирается ---
bushing = L.revolve("BarrelBushing", [(-S["bushing_len"], S["bushing_od"] / 2),
                                      (0, S["bushing_od"] / 2)], 20)
L.boolean(bushing, L.revolve("bush_bore", [(-S["bushing_len"] - 2, 7.6), (2, 7.6)], 16))
bushing.name = "BarrelBushing"
parts["BarrelBushing"] = bushing

plug = L.revolve("RecoilSpringPlug", [(-16, S["plug_od"] / 2), (-2, S["plug_od"] / 2), (0, S["plug_od"] / 2 - 1.5)], 18)
L.move(plug, (0, 0, -13.5))
parts["RecoilSpringPlug"] = plug

spring = L.helix("RecoilSpring", coil_r=4.6, wire_r=0.9, pitch=4.2, turns=13, segments=14, ring=7, axis_x=-72)
L.move(spring, (0, 0, -13.5))
parts["RecoilSpring"] = spring

guide = L.revolve("RecoilSpringGuide", [(-S["recoil_guide_l"], 3.0), (-8, 3.0), (-8, 5.6), (0, 5.6)], 16)
L.move(guide, (-14, 0, -13.5))
parts["RecoilSpringGuide"] = guide

STOP_X = -bl + 42
stop = L.box("SlideStop", (S["slide_stop_len"], 4.0, 11.0), (STOP_X, -S["slide_width"] / 2 - 1, -14))
# Ось задержки строится в нуле и переносится на место: revolve крутит вокруг мирового нуля.
pin = L.revolve("stop_pin", [(-6, 2.3), (6, 2.3)], 12, axis="Y")
L.move(pin, (STOP_X + 18, 0, -14))
stop = L.boolean(stop, pin, "UNION")
stop.name = "SlideStop"
parts["SlideStop"] = stop

# --- Рамка: пылевая крышка, спусковая скоба, рукоять под углом 18 градусов ---
ga = math.radians(S["grip_angle_deg"])
TOP_Z = BORE_Z + 18.0                              # верх целика
GRIP_BOT_Z = TOP_Z - S["height_with_mag"]
grip_top_x = -128.0
grip_bot_z = GRIP_BOT_Z + 4.0
dx = math.tan(ga) * abs(grip_bot_z - (BORE_Z - 30))
traced_bot = clip(TRACED["bottom"], -S["overall_length"], 0.0) if TRACED else []
if len(traced_bot) >= 10:
    # Весь наружный низ - с фотографии: спусковая скоба, изгиб рукояти, донце магазина.
    # Верх рамки под затвором в силуэт не попадает и остаётся расчётным, а вот хвостовик
    # позади затвора виден - его тоже берём с фотографии, иначе рамка выходит глухой плитой.
    tail = [p for p in TRACED["top"] if p[0] <= -sl]
    frame_profile = [(0, bot + 14), (-sl, bot + 14)] + tail[::-1] + traced_bot
    print(f"@@ низ рамки: {len(traced_bot)} точек с фотографии, хвостовик: {len(tail)}")
else:
    frame_profile = [
        (0, bot - 1), (0, bot - 10), (-56, bot - 10), (-62, bot - 26),
        (-70, bot - 30), (-100, bot - 32), (-108, bot - 26), (-110, bot - 10),
        (grip_top_x + 10, bot - 12), (grip_top_x + 10 - dx, grip_bot_z),
        (grip_top_x - 30 - dx, grip_bot_z),
        (-206, bot - 8), (-210, bot + 4), (-210, bot + 14), (-4, bot + 14),
    ]
frame = L.profile_extrude("Frame", frame_profile, S["overall_width"] - 4.0)

# Магазин единственный однорядный: у .45 два ряда не ставят, отсюда узкая рукоять.
CART = {"round_len": 32.39, "case_dia": 12.19}
MAG_WALL = 1.0
MAG_IN_W = CART["round_len"] + 1.8
MAG_IN_T = CART["case_dia"] + 1.2                  # один ряд: ширина чуть больше гильзы
MAG_OUT_W = MAG_IN_W + 2 * MAG_WALL
MAG_OUT_T = MAG_IN_T + 2 * MAG_WALL
MAG_PITCH = CART["case_dia"]                       # однорядная укладка: патрон на патроне
MAG_LIP = 3.0
MAG_TILT = math.degrees(math.atan2(dx, abs(grip_bot_z - (bot - 12))))
MAG_TOP_Z = bot - 2.0
MAG_H = ((MAG_TOP_Z - GRIP_BOT_Z) - (MAG_OUT_W + 2.0) / 2 * math.sin(math.radians(MAG_TILT))) \
    / math.cos(math.radians(MAG_TILT))
MAG_TOP_X = (grip_top_x + 10 - dx + (-206.0)) / 2 + math.sin(math.radians(MAG_TILT)) * MAG_H
MAG_PLACE = (MAG_TILT, (MAG_TOP_X, 0, MAG_TOP_Z))

L.boolean(frame, L.place(L.box("magwell", (MAG_OUT_W + 1.0, MAG_OUT_T + 1.0, MAG_H), (0, 0, -MAG_H / 2)), *MAG_PLACE))
L.boolean(frame, L.box("tguard", (44, 28, 26), (-88, 0, bot - 16)))
# Образец 1911: за спуском выемок под пальцы нет - они появились только на 1911A1.
frame.name = "Frame"
parts["Frame"] = frame

# --- Наружные органы управления ---
CAP = S["mag_capacity"]
MAG_STACK_H = (CAP - 1) * MAG_PITCH + CART["case_dia"]
MAG_TOP_ROUND_Z = -(MAG_LIP + CART["case_dia"] / 2)
FOLLOWER_H = 9.0

mag = L.box("Magazine", (MAG_OUT_W, MAG_OUT_T, MAG_H), (0, 0, -MAG_H / 2))
L.boolean(mag, L.box("magcav", (MAG_IN_W, MAG_IN_T, MAG_H - MAG_LIP - 4.0), (0, 0, -MAG_LIP - (MAG_H - MAG_LIP - 4.0) / 2)))
L.boolean(mag, L.box("maglips", (MAG_IN_W, CART["case_dia"], MAG_LIP + 2.0), (0, 0, -(MAG_LIP + 2.0) / 2 + 1.0)))
mag = L.boolean(mag, L.box("magfloor", (MAG_OUT_W + 2.0, MAG_OUT_T + 2.0, 4.0), (0, 0, -MAG_H + 2.0)), "UNION")
mag.name = "Magazine"
L.place(mag, *MAG_PLACE)
parts["Magazine"] = mag

fol_z = MAG_TOP_ROUND_Z - (CAP - 1) * MAG_PITCH - CART["case_dia"] / 2 - FOLLOWER_H / 2
fol = L.box("MagFollower", (MAG_IN_W - 0.6, MAG_IN_T - 0.6, FOLLOWER_H), (0, 0, fol_z))
L.place(fol, *MAG_PLACE)
parts["MagFollower"] = fol

# Курок: у 1911 он открытый, это одно из его лиц.
# Курок в спущенном положении стоит вровень с линией затвора, а не торчит над ней:
# паспортная высота 133 мм меряется по верху прицельных, и курок в неё укладывается.
HAM_X, HAM_Z = -196.0, bot + 7
hammer = L.box("Hammer", (S["hammer_len"] * 0.55, S["hammer_w"], S["hammer_len"]), (HAM_X, 0, HAM_Z))
ring = L.revolve("hammer_ring", [(-3, 7.0), (3, 7.0)], 16, axis="Y")
L.move(ring, (HAM_X, 0, HAM_Z + S["hammer_len"] / 2 - 4))
hammer = L.boolean(hammer, ring, "UNION")
hole = L.revolve("hammer_hole", [(-4, 3.4), (4, 3.4)], 14, axis="Y")
L.move(hole, (HAM_X, 0, HAM_Z + S["hammer_len"] / 2 - 4))
L.boolean(hammer, hole)
hammer.name = "Hammer"
parts["Hammer"] = hammer

safety = L.box("ThumbSafety", (S["thumb_safety_l"], 3.0, 9.0), (-176, -S["overall_width"] / 2 + 1, bot - 4))
parts["ThumbSafety"] = safety

grip_safety = L.box("GripSafety", (16.0, 20.0, S["grip_safety_l"]), (-196, 0, bot - 26))
parts["GripSafety"] = grip_safety

trigger = L.box("Trigger", (S["trigger_len"], 9.0, 15.0), (-106, 0, bot - 20))
parts["Trigger"] = trigger

housing = L.box("MainspringHousing", (9.0, 20.0, S["mainspring_h"]), (-205, 0, bot - 44))
parts["MainspringHousing"] = housing

# Щёчки рукояти: у 1911 они съёмные и деревянные - ещё одно его лицо.
for side, y in (("L", 1), ("R", -1)):
    # Строим в нуле, поворачиваем, ставим на место: L.rotate крутит вокруг мирового нуля,
    # и поворот детали, уже отнесённой на 140 мм, уводил её на сотню миллиметров вбок.
    g = L.box(f"GripPanel{side}", (30.0, S["grip_panel_t"], S["grip_panel_l"]), (0, 0, 0))
    L.place(g, -S["grip_angle_deg"],
            (grip_top_x - 14 - dx / 2, y * (S["overall_width"] / 2 - S["grip_panel_t"] / 2 - 1.0),
             grip_bot_z + S["grip_panel_l"] / 2 + 6), "Y")
    parts[f"GripPanel{side}"] = g

# --- Материалы и фаски ---
BLUED = L.material("Steel_Blued", (0.045, 0.045, 0.050), 1.0, 0.30)
BRIGHT = L.material("Steel_Bright", (0.36, 0.36, 0.38), 1.0, 0.24)
WOOD = L.material("Walnut", (0.13, 0.065, 0.032), 0.0, 0.45)
SPRING = L.material("Spring_Steel", (0.42, 0.42, 0.44), 1.0, 0.22)
MATS = {"GripPanelL": WOOD, "GripPanelR": WOOD, "RecoilSpring": SPRING, "MagFollower": BRIGHT}
BEVEL_W = {"Slide": 0.5, "Frame": 0.6, "Barrel": 0.4, "Magazine": 0.4, "RecoilSpring": 0.15,
           "GripPanelL": 0.6, "GripPanelR": 0.6}
for n, ob in parts.items():
    L.bevel(ob, width=BEVEL_W.get(n, 0.25), segments=2)
    L.shade_smooth(ob)
    L.assign(ob, MATS.get(n, BLUED))

# --- Проверка габаритов против таблицы источника ---
print(f"@@ === Colt M1911 ({GEN}) ===")
report = {}
for n, ob in parts.items():
    d = L.dims(ob)
    report[n] = {"size_mm": list(d)}
    print(f"@@ {n:18s} {d[0]:6.1f} x {d[1]:6.1f} x {d[2]:6.1f}")

pts = [ob.matrix_world @ mathutils.Vector(c) for ob in parts.values() for c in ob.bound_box]
lo = [min(p[k] for p in pts) for k in range(3)]
hi = [max(p[k] for p in pts) for k in range(3)]
meas = {
    "overall_length": hi[0] - lo[0],
    "height_with_mag": hi[2] - lo[2],
    "overall_width": hi[1] - lo[1],
    "barrel_length": L.dims(parts["Barrel"])[0],
    "slide_length": L.dims(parts["Slide"])[0],
}
print("@@ --- assembly vs spec ---")
ok = True
for k, v in meas.items():
    ref = S[k]
    err = (v - ref) / ref * 100.0
    good = abs(err) <= 2.0
    ok = ok and good
    print(f"@@ {k:18s} model {v:6.1f}  spec {ref:6.1f}  {err:+5.1f}%  {'OK' if good else 'FAIL'}  [{S.src(k)}]")
report["_check"] = {k: {"model": round(v, 1), "spec": S[k], "source": S.src(k)} for k, v in meas.items()}

by_src = {}
for k, (_, src) in SPEC.items():
    by_src.setdefault(src, []).append(k)
documented = len(by_src.get("spec", [])) + len(by_src.get("blue", []))
print(f"@@ --- data provenance: {documented}/{len(SPEC)} dimensions documented "
      f"({100.0 * documented / len(SPEC):.0f}%) ---")
report["_provenance"] = {"sources": {k: v[1] for k, v in SPEC.items()}, "documented": documented,
                         "total": len(SPEC), "completeness_pct": round(100.0 * documented / len(SPEC), 1),
                         "traced_contour": (TRACED or {}).get("source"),
                         "traced_points": (len(TRACED["top"]) + len(TRACED["bottom"])) if TRACED else 0}

# --- Неполная разборка 1911: она заметно отличается от глоковской ---
report["_fieldstrip"] = [
    {"part": "Magazine", "order": 1, "dir": [0, 0, -1], "dist_mm": 130, "name": "Магазин",
     "desc": "Однорядный магазин на 7 патронов .45 ACP. Один ряд - оттого рукоять узкая, а ёмкость вдвое меньше современных."},
    {"part": "RecoilSpringPlug", "order": 2, "dir": [1, 0, 0], "dist_mm": 70, "name": "Заглушка возвратной пружины",
     "desc": "Заглушка держит пружину под давлением; при разборке её утапливают и проворачивают втулку."},
    {"part": "BarrelBushing", "order": 3, "dir": [1, 0, 0], "dist_mm": 60, "name": "Втулка ствола",
     "desc": "Втулка центрирует дульную часть ствола в затворе. У Глока её нет - там ствол центрируется самим затвором."},
    {"part": "SlideStop", "order": 4, "dir": [0, -1, 0], "dist_mm": 60, "name": "Затворная задержка",
     "desc": "Ось затворной задержки проходит через серьгу ствола и держит затвор на рамке."},
    {"part": "Slide", "order": 5, "dir": [1, 0, 0], "dist_mm": 210, "name": "Затвор в сборе",
     "desc": "Затвор снимается вперёд после выхода задержки."},
    {"part": "RecoilSpring", "order": 6, "dir": [1, 0, 0], "dist_mm": 110, "name": "Возвратная пружина",
     "desc": "Возвратная пружина 1911 работает под стволом, а не вокруг него."},
    {"part": "RecoilSpringGuide", "order": 7, "dir": [1, 0, 0], "dist_mm": 90, "name": "Направляющий стержень",
     "desc": "Стержень задаёт пружине направление и упирается в рамку."},
    {"part": "BarrelLink", "order": 8, "dir": [0, 0, -1], "dist_mm": 50, "name": "Серьга ствола",
     "desc": "Серьга опускает казённую часть при откате и отпирает затвор - главное отличие схемы от глоковской с перекосом."},
    {"part": "Barrel", "order": 9, "dir": [1, 0, 0], "dist_mm": 150, "name": "Ствол",
     "desc": "Ствол с патронником; запирается двумя боевыми упорами в затвор."},
    {"part": "Frame", "order": 10, "dir": [0, 0, 0], "dist_mm": 0, "name": "Рамка",
     "desc": "Стальная рамка со спусковым механизмом; щёчки рукояти съёмные."},
]
report["_fullstrip"] = [
    {"part": "GripPanelL", "group": "frame", "order": 1, "dir": [0, 1, 0], "dist_mm": 50, "name": "Щёчка левая",
     "desc": "Деревянные щёчки крепятся винтами - у 1911 они съёмные, в отличие от цельной полимерной рукояти Глока."},
    {"part": "GripPanelR", "group": "frame", "order": 2, "dir": [0, -1, 0], "dist_mm": 50, "name": "Щёчка правая",
     "desc": "Правая щёчка рукояти."},
    {"part": "MainspringHousing", "group": "frame", "order": 3, "dir": [0, 0, -1], "dist_mm": 70, "name": "Задняя стенка рукояти",
     "desc": "У образца 1911 она плоская; выгнутой она стала только на 1911A1 в 1924 году."},
    {"part": "GripSafety", "group": "frame", "order": 4, "dir": [-1, 0, 0], "dist_mm": 60, "name": "Автоматический предохранитель",
     "desc": "Выключается сам при охвате рукояти. У образца 1911 пятка короткая - длинную ввели на A1."},
    {"part": "Hammer", "group": "frame", "order": 5, "dir": [-1, 0, 0], "dist_mm": 60, "name": "Курок",
     "desc": "Открытый курок: видно и состояние оружия, и то, что механизм курковый, а не ударниковый."},
    {"part": "ThumbSafety", "group": "frame", "order": 6, "dir": [0, -1, 0], "dist_mm": 50, "name": "Флажковый предохранитель",
     "desc": "Запирает затвор и шептало при взведённом курке."},
    {"part": "Trigger", "group": "frame", "order": 7, "dir": [1, 0, 0], "dist_mm": 60, "name": "Спусковой крючок",
     "desc": "У образца 1911 спуск длинный; короткий появился на A1."},
    {"part": "MagFollower", "group": "mag", "order": 8, "dir": [0, 0, -1], "dist_mm": 60, "name": "Подаватель",
     "desc": "Подаватель однорядного магазина."},
]

# --- Раскладка боеприпаса ---
_rot = mathutils.Matrix.Rotation(math.radians(MAG_TILT), 4, "Y")
_off = mathutils.Vector((MAG_TOP_X, 0.0, MAG_TOP_Z))
_first = _rot @ mathutils.Vector((0.0, 0.0, MAG_TOP_ROUND_Z)) + _off
_pitch = _rot @ mathutils.Vector((0.0, 0.0, -MAG_PITCH))
report["_ammo"] = {
    "round": "Round45ACP",
    "capacity": CAP,
    "chamber": [round(-bl + CART["round_len"] / 2, 3), 0.0, BORE_Z],
    "chamber_pitch_deg": 0.0,
    "stack_first": [round(v, 3) for v in _first],
    "stack_pitch": [round(v, 3) for v in _pitch],
    "stack_lateral": [0.0, 0.0, 0.0],       # один ряд: смещения между рядами нет
    "round_pitch_deg": round(-MAG_TILT, 3),
    "geometry": {"tilt_deg": round(MAG_TILT, 2), "pitch_mm": round(MAG_PITCH, 3),
                 "stack_height_mm": round(MAG_STACK_H, 1),
                 "inner_mm": [round(MAG_IN_W, 2), round(MAG_IN_T, 2), round(MAG_H, 1)]},
}
_inside = MAG_H - MAG_LIP - 4.0
print(f"@@ магазин: однорядный, наклон {MAG_TILT:.1f} град, шаг {MAG_PITCH:.2f} мм")
print(f"@@ столб {CAP} патронов {MAG_STACK_H:.1f} + подаватель {FOLLOWER_H:.0f} = "
      f"{MAG_STACK_H + FOLLOWER_H:.1f} мм из {_inside:.1f} внутри  "
      f"{'OK' if MAG_STACK_H + FOLLOWER_H <= _inside else 'НЕ ВЛЕЗАЕТ'}")

bpy.ops.object.select_all(action="DESELECT")
for ob in parts.values():
    ob.select_set(True)
bpy.ops.export_scene.fbx(filepath=os.path.join(OUT, f"Colt1911_{GEN}.fbx"), use_selection=True,
                         object_types={"MESH"}, apply_unit_scale=True, apply_scale_options="FBX_SCALE_NONE",
                         axis_forward="-Y", axis_up="Z", bake_space_transform=True, mesh_smooth_type="FACE")
with open(os.path.join(OUT, f"Colt1911_{GEN}.json"), "w", encoding="utf-8") as f:
    json.dump({"generation": GEN, "spec": {k: v[0] for k, v in SPEC.items()}, "parts": report}, f,
              indent=1, ensure_ascii=False)

# --- Превью ---
sc = bpy.context.scene
sc.render.engine = "CYCLES"
sc.cycles.samples = 64
sc.cycles.use_denoising = True
world = bpy.data.worlds.new("W")
sc.world = world
world.use_nodes = True
world.node_tree.nodes["Background"].inputs[0].default_value = (0.05, 0.055, 0.065, 1.0)
world.node_tree.nodes["Background"].inputs[1].default_value = 1.2
for (loc, rot, size, power) in (((-90, -260, 260), (math.radians(40), 0, 0), 400, 900000),
                                ((160, -200, 60), (math.radians(78), 0, math.radians(38)), 260, 350000),
                                ((-330, 160, 120), (math.radians(65), 0, math.radians(200)), 300, 500000)):
    bpy.ops.object.light_add(type="AREA", location=loc, rotation=rot)
    lt = bpy.context.active_object
    lt.data.size = size
    lt.data.energy = power
sc.render.resolution_x, sc.render.resolution_y = 1400, 700
sc.render.filepath = os.path.join(OUT, f"preview_{GEN}.png")
bpy.ops.object.camera_add(location=(-105, -460, -45), rotation=(math.radians(90), 0, 0))
sc.camera = bpy.context.active_object
sc.camera.data.lens = 50
bpy.ops.render.render(write_still=True)
print(f"@@ exported to {OUT}; dimension check {'PASSED' if ok else 'FAILED'}")
