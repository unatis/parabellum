"""
Glock 17 Gen5: детали неполной разборки, построенные по опубликованным размерам.
Каждая деталь строится числами из таблицы SPEC, затем габариты сборки проверяются против неё же -
та же логика, что у баллистики: модель принимается, только если сходится с источником.
Запуск: Tools/blender/make_glock17.bat -> Import/Glock17Parts/Glock17.fbx + .json + preview.png
"""
import bpy, os, sys, json, math

sys.path.append(os.path.dirname(os.path.abspath(__file__)))
import importlib, weapon_parts_lib as L
importlib.reload(L)

OUT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "Import", "Glock17Parts"))
os.makedirs(OUT, exist_ok=True)

# --- Размеры, мм. У каждого значения помечен источник:
#   spec  - официальные данные производителя или стандарт на патрон
#   blue  - размерный производственный чертёж
#   deriv - вычислено из других известных размеров
#   est   - обоснованное предположение по фотографиям и пропорциям (подлежит уточнению)
SPEC = {
    "overall_length":  (202.0, "spec"), "height_with_mag": (138.0, "spec"),
    "overall_width":   (34.0,  "spec"), "slide_width":     (25.5,  "spec"),
    "barrel_length":   (114.0, "spec"), "sight_radius":    (165.0, "spec"),
    "grip_angle_deg":  (22.0,  "spec"), "bore_diameter":   (9.02,  "spec"),
    "mag_capacity":    (17,    "spec"), "trigger_reach":   (72.0,  "spec"),
    "weight_empty_g":  (638.0, "spec"),
    "slide_length":    (186.0, "est"),  "slide_height":    (25.5,  "est"),
    "barrel_outer":    (15.5,  "est"),  "mag_length":      (108.0, "est"),
    "mag_width":       (27.0,  "est"),  "mag_thickness":   (10.5,  "est"),
    # Внутренние детали: расположение выведено из схемы Браунинга с перекосом ствола и патента
    # Гастона Глока US 4,539,889; размеры - реконструкция по пропорциям, подлежат обмеру.
    "striker_length":  (62.0,  "est"),  "striker_dia":      (5.5,  "est"),
    "striker_tip_dia": (2.4,   "est"),  "spacer_sleeve_od": (9.0,  "est"),
    "fp_spring_od":    (7.0,   "est"),  "cover_plate_h":    (16.0, "est"),
    "extractor_len":   (22.0,  "est"),  "fp_safety_dia":    (3.2,  "est"),
    "locking_block_w": (12.0,  "est"),  "slide_lock_w":     (34.0, "est"),
    "trigger_bar_len": (62.0,  "est"),  "housing_len":      (26.0, "est"),
    "slide_stop_len":  (52.0,  "est"),  "mag_catch_h":      (12.0, "est"),
}


class SpecView(dict):
    """S["ключ"] отдаёт значение, S.src("ключ") - источник."""
    def __getitem__(self, k):
        return SPEC[k][0]
    def src(self, k):
        return SPEC[k][1]


S = SpecView()

# --- Отличия поколений: то, что реально меняется в геометрии (открытые данные производителя) ---
GENERATIONS = {
    "Gen1": {"year": 1982, "rail": False, "finger_grooves": False, "magwell_flare": False,
             "ambi_slide_stop": False, "pins": 2, "thumb_rest": False},
    "Gen2": {"year": 1988, "rail": False, "finger_grooves": False, "magwell_flare": False,
             "ambi_slide_stop": False, "pins": 2, "thumb_rest": False},
    "Gen3": {"year": 1998, "rail": True, "finger_grooves": True, "magwell_flare": False,
             "ambi_slide_stop": False, "pins": 3, "thumb_rest": True},
    "Gen4": {"year": 2010, "rail": True, "finger_grooves": True, "magwell_flare": False,
             "ambi_slide_stop": False, "pins": 3, "thumb_rest": True},
    "Gen5": {"year": 2017, "rail": True, "finger_grooves": False, "magwell_flare": True,
             "ambi_slide_stop": True, "pins": 2, "thumb_rest": False},
}
GEN = os.environ.get("PBL_GEN", "Gen5")
G = GENERATIONS[GEN]

bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.context.scene.unit_settings.system = "METRIC"
bpy.context.scene.unit_settings.scale_length = 0.001   # 1 BU = 1 мм

# Ось канала ствола на z=0, срез дула на x=0, оружие уходит назад по -x.
BORE_Z = 0.0
parts = {}

# --- Ствол: тело вращения, канал высверлен, снизу проушина запирания ---
bl = S["barrel_length"]
barrel = L.revolve("Barrel", [(-bl, 7.0), (-bl + 6, 7.75), (-18, 7.75), (-14, 9.5), (0, 9.5)], 24)
L.boolean(barrel, L.revolve("bore_cut", [(-bl - 2, S["bore_diameter"] / 2), (2, S["bore_diameter"] / 2)], 16))
barrel = L.boolean(barrel, L.box("lug", (26, 11, 13), (-bl + 20, 0, -11)), "UNION")
barrel.name = "Barrel"
parts["Barrel"] = barrel

# --- Затвор: профиль сбоку, выдавленный на ширину затвора; вырезаны окно выброса, паз рамки, канал ствола ---
sl, sh = S["slide_length"], S["slide_height"]
top, bot = BORE_Z + 16.0, BORE_Z - 9.5
slide_profile = [(2, top - 4), (0, top - 9), (0, bot + 3), (-6, bot), (-sl + 8, bot),
                 (-sl, bot + 4), (-sl, top - 3), (-sl + 5, top), (-4, top)]
slide = L.profile_extrude("Slide", slide_profile, S["slide_width"])
L.boolean(slide, L.box("ejport", (52, 30, 13), (-58, 4, top - 5)))
L.boolean(slide, L.box("railcut", (sl + 4, S["slide_width"] - 9, 13), (-sl / 2, 0, bot + 5)))
L.boolean(slide, L.revolve("barrelcut", [(-bl - 2, 10.0), (2, 10.0)], 20))
slide = L.boolean(slide, L.box("fs", (3.5, 3.5, 5), (-6, 0, top + 1.5)), "UNION")
slide = L.boolean(slide, L.box("rs", (5, 16, 6), (-6 - S["sight_radius"], 0, top + 2)), "UNION")
slide.name = "Slide"
parts["Slide"] = slide

# --- Рамка: пылевая крышка, спусковая скоба, рукоять под углом 22 градуса ---
ga = math.radians(S["grip_angle_deg"])

TOP_Z = BORE_Z + 21.0                      # верх целика
GRIP_BOT_Z = TOP_Z - S["height_with_mag"]  # низ донышка магазина = полная высота
grip_top_x = -132.0
grip_bot_z = GRIP_BOT_Z + 5.0              # рамка на 5 мм выше донышка
dx = math.tan(ga) * abs(grip_bot_z - (BORE_Z - 34))
mag_x_pre = -166.0   # ось магазина внутри рукояти
frame_profile = [
    (-4, bot - 1), (-4, bot - 9), (-52, bot - 9), (-58, bot - 26),
    (-64, bot - 30), (-96, bot - 32), (-104, bot - 26), (-106, bot - 10),
    (grip_top_x + 8, bot - 12), (grip_top_x + 8 - dx, grip_bot_z), (grip_top_x - 22 - dx, grip_bot_z),
    (-196, bot - 10), (-202, bot + 2), (-202, bot + 12), (-6, bot + 12),   # хвостовик: полная длина 202 мм
]
frame = L.profile_extrude("Frame", frame_profile, 30.0)
L.boolean(frame, L.box("magwell", (24, 22, 100), (grip_top_x - 6 - dx / 2, 0, grip_bot_z + 48)))
L.boolean(frame, L.box("tguard", (40, 34, 26), (-82, 0, bot - 14)))

# --- Отличия поколения ---
if G["rail"]:
    # Планка Пикатинни на пылевой крышке: появилась в Gen3 (1998)
    rail = L.box("rail", (34, 21, 7), (-34, 0, bot - 12))
    frame = L.boolean(frame, rail, "UNION")
    for i in range(2):
        L.boolean(frame, L.box(f"railslot{i}", (4, 23, 4), (-26 - i * 12, 0, bot - 12)))
if G["finger_grooves"]:
    # Выемки под пальцы на передней поверхности рукояти: Gen3 и Gen4
    # Передняя поверхность рукояти идёт от (grip_top_x+8, bot-12) к (grip_top_x+8-dx, grip_bot_z)
    fs_x0, fs_z0 = grip_top_x + 8, bot - 12
    fs_x1, fs_z1 = grip_top_x + 8 - dx, grip_bot_z
    for i, t in enumerate((0.22, 0.46, 0.70)):
        gx = fs_x0 + (fs_x1 - fs_x0) * t
        gz = fs_z0 + (fs_z1 - fs_z0) * t
        g = L.revolve(f"fg{i}", [(-20, 6.0), (20, 6.0)], 14, axis="Y")
        L.move(g, (gx + 3.0, 0, gz))
        L.boolean(frame, g)
if G["magwell_flare"]:
    # Расширенная горловина магазина: Gen5
    # Горловина расширяется вниз и назад, но не выходит за габаритную ширину 34 мм
    flare = L.box("flare", (38, 32, 10), (mag_x_pre, 0, grip_bot_z + 3))
    frame = L.boolean(frame, flare, "UNION")
# --- Эргономика рукояти: вместо плоской плиты - оболочка по сечениям (перед-зад 45 мм, бок 30 мм, талия уже) ---
GRIP_TOP_Z = bot - 14
L.boolean(frame, L.box("grip_slab_cut", (120, 40, abs(GRIP_TOP_Z - grip_bot_z) + 2),
                       (-162.0, 0, (GRIP_TOP_Z + grip_bot_z) / 2)))
# Сечения рукояти: передняя и задняя стенки наклонены на 22 градуса, талия чуть уже (мм от дула).
SECTIONS = ((0.00, -124.0, -196.0, 30.0), (0.25, -133.0, -198.0, 29.2), (0.50, -142.0, -200.0, 28.6),
            (0.75, -149.0, -201.0, 29.0), (1.00, -155.0, -202.0, 30.0))
rings = []
for (t, front, rear, d) in SECTIONS:
    cz = GRIP_TOP_Z + (grip_bot_z - GRIP_TOP_Z) * t
    rings.append(L.rrect_ring((front + rear) / 2.0, 0.0, cz, abs(rear - front), d, 0, n=7))
grip = L.loft("grip", rings)
frame = L.boolean(frame, grip, "UNION")
frame.name = "Frame"
parts["Frame"] = frame

# =====================================================================================
# ВНУТРЕННИЕ ДЕТАЛИ - реконструкция. Взаимное расположение следует из схемы запирания
# с перекосом ствола (Браунинг) и патента US 4,539,889; размеры помечены как est.
# =====================================================================================
BREECH_X = -114.0 - 4.0          # казённый срез ствола, за ним зеркало затвора

# --- Ударник с пружиной, муфтой и полукольцами ---
striker = L.revolve("Striker", [(-S["striker_length"], 3.2), (-S["striker_length"] + 10, 4.2),
                                (-46, 4.2), (-44, S["striker_dia"] / 2), (-14, S["striker_dia"] / 2),
                                (-12, 3.0), (-2, 3.0), (0, S["striker_tip_dia"] / 2)], 16)
L.move(striker, (BREECH_X - 2, 0, 0))
parts["Striker"] = striker

fp_spring = L.helix("FiringPinSpring", coil_r=S["fp_spring_od"] / 2, wire_r=0.65, pitch=3.4, turns=11)
L.move(fp_spring, (BREECH_X - 56, 0, 0))
parts["FiringPinSpring"] = fp_spring

sleeve = L.revolve("SpacerSleeve", [(0, S["spacer_sleeve_od"] / 2), (30, S["spacer_sleeve_od"] / 2)], 16)
L.boolean(sleeve, L.revolve("sl_bore", [(-2, 3.8), (32, 3.8)], 16))
L.move(sleeve, (BREECH_X - 54, 0, 0))
parts["SpacerSleeve"] = sleeve

cups = L.box("SpringCups", (6, 9, 9), (BREECH_X - S["striker_length"] + 6, 0, 0))
L.boolean(cups, L.revolve("cups_bore", [(-4, 2.2), (4, 2.2)], 12))
parts["SpringCups"] = cups

# --- Крышка затвора: держит всю сборку ударника сзади ---
plate = L.box("SlideCoverPlate", (2.5, 17.0, S["cover_plate_h"]), (-183.0, 0, 3.0))
parts["SlideCoverPlate"] = plate

# --- Выбрасыватель: в правой стенке затвора, зацеп за закраину гильзы ---
extr = L.box("Extractor", (S["extractor_len"], 5.0, 8.0), (BREECH_X - 9, 9.5, 1.0))
extr = L.boolean(extr, L.box("extr_hook", (4.0, 5.0, 4.0), (BREECH_X + 1, 8.0, -1.0)), "UNION")
parts["Extractor"] = extr

plunger = L.revolve("ExtractorPlunger", [(0, S["fp_safety_dia"] / 2), (26, S["fp_safety_dia"] / 2)], 12)
L.move(plunger, (BREECH_X - 46, 9.5, 1.0))
parts["ExtractorPlunger"] = plunger

# --- Предохранитель ударника: плунжер поперёк канала ударника, снимается спусковой тягой ---
fps = L.revolve("FiringPinSafety", [(0, S["fp_safety_dia"] / 2), (11, S["fp_safety_dia"] / 2)], 12, axis="Z")
L.move(fps, (BREECH_X - 14, 0, -6.0))
parts["FiringPinSafety"] = fps

# --- Блок запирания: принимает проушину ствола, задаёт перекос при откате ---
lb = L.box("LockingBlock", (20.0, S["locking_block_w"], 16.0), (-104.0, 0, bot - 6.0))
L.boolean(lb, L.box("lb_slot", (12.0, 12.5, 9.0), (-104.0, 0, bot + 1.0)))
parts["LockingBlock"] = lb

# --- Защёлка разборки: поперечный сухарь, опускается при снятии затвора ---
sl = L.box("SlideLock", (5.0, S["slide_lock_w"], 7.0), (-98.0, 0, bot - 13.0))
parts["SlideLock"] = sl

# --- Затворная задержка: рычаг по левой стороне рамки ---
ss = L.box("SlideStop", (S["slide_stop_len"], 3.0, 6.0), (-86.0, -15.5, bot - 4.0))
ss = L.boolean(ss, L.box("ss_tab", (7.0, 4.0, 9.0), (-64.0, -15.5, bot - 1.0)), "UNION")
parts["SlideStop"] = ss

# --- Спусковой крючок с тягой и коннектором ---
trig = L.box("Trigger", (7.0, 6.0, 15.0), (-133.0, 0, bot - 24.0))
bar = L.box("trigger_bar", (S["trigger_bar_len"], 2.2, 5.0), (-133.0 - S["trigger_bar_len"] / 2, 5.0, bot - 20.0))
trig = L.boolean(trig, bar, "UNION")
parts["Trigger"] = trig

conn = L.box("Connector", (9.0, 2.0, 7.0), (-172.0, 5.0, bot - 17.0))
parts["Connector"] = conn

housing = L.box("TriggerHousing", (S["housing_len"], 16.0, 20.0), (-172.0, 0, bot - 28.0))
L.boolean(housing, L.box("hs_hollow", (20.0, 11.0, 14.0), (-172.0, 0, bot - 28.0)))
parts["TriggerHousing"] = housing

# --- Защёлка магазина ---
# Защёлка справа и затворная задержка слева вместе дают габаритную ширину 34 мм
mc = L.box("MagCatch", (8.0, 6.0, S["mag_catch_h"]), (-140.0, 14.0, bot - 30.0))
parts["MagCatch"] = mc

# --- Возвратная пружина в сборе: направляющий стержень и витая пружина ---
rod = L.revolve("rod", [(-92, 2.0), (-8, 2.0), (-6, 4.5), (0, 4.5)], 16)
rsa = L.boolean(rod, L.helix("spring", coil_r=5.2, wire_r=0.9, pitch=6.5, turns=12, axis_x=-88), "UNION")
rsa.name = "RecoilSpring"
rsa.location = (-6, 0, BORE_Z - 11)
bpy.context.view_layer.objects.active = rsa
bpy.ops.object.transform_apply(location=True)
parts["RecoilSpring"] = rsa

# --- Магазин на 17 патронов с донышком ---
ml, mw, mt = S["mag_length"], S["mag_width"], S["mag_thickness"]
mag_x = mag_x_pre
mag = L.box("Magazine", (mw, mt, ml), (mag_x, 0, GRIP_BOT_Z + 4 + ml / 2))
mag = L.boolean(mag, L.box("floor", (mw + 4, mt + 5, 8), (mag_x, 0, GRIP_BOT_Z + 4)), "UNION")
mag.name = "Magazine"
parts["Magazine"] = mag

# --- Фаски по рёбрам и материалы ---
MATS = {
    "Slide":        L.material("Steel_nDLC", (0.055, 0.055, 0.060), 1.0, 0.34),   # сталь с покрытием nDLC
    "Barrel":       L.material("Steel_Nitride", (0.085, 0.082, 0.080), 1.0, 0.26),
    "Frame":        L.material("Polymer", (0.030, 0.030, 0.032), 0.0, 0.62),      # полимер: не металл, матовый
    "Magazine":     L.material("Polymer_Mag", (0.035, 0.035, 0.038), 0.0, 0.55),
    "RecoilSpring": L.material("Spring_Steel", (0.42, 0.42, 0.44), 1.0, 0.22),
}
BEVEL_W = {"Slide": 0.5, "Barrel": 0.4, "Frame": 0.6, "Magazine": 0.4, "RecoilSpring": 0.15}
# Внутренние детали: сталь, кроме полимерных корпуса УСМ, спуска и защёлки магазина.
POLYMER_PARTS = {"TriggerHousing", "Trigger", "MagCatch", "Magazine"}
STEEL_BRIGHT = L.material("Steel_Bright", (0.36, 0.36, 0.38), 1.0, 0.24)
for n, ob in parts.items():
    L.bevel(ob, width=BEVEL_W.get(n, 0.2), segments=2)
    L.shade_smooth(ob)
    L.assign(ob, MATS.get(n, MATS["Frame"] if n in POLYMER_PARTS else STEEL_BRIGHT))

# --- Проверка габаритов против таблицы источника ---
print(f"@@ === {GEN} ({G['year']}): rail={G['rail']} grooves={G['finger_grooves']} flare={G['magwell_flare']} pins={G['pins']} ===")
print("@@ --- parts (mm) ---")
report = {}
for n, ob in parts.items():
    d = L.dims(ob)
    report[n] = {"size_mm": list(d)}
    print(f"@@ {n:13s} {d[0]:6.1f} x {d[1]:6.1f} x {d[2]:6.1f}")

import mathutils
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
    print(f"@@ {k:18s} model {v:6.1f}  spec {ref:6.1f}  {err:+5.1f}%  {'OK' if good else 'FAIL'}")
report["_check"] = {k: {"model": round(v, 1), "spec": S[k], "source": S.src(k)} for k, v in meas.items()}

# --- Полнота данных: сколько размеров из документов, сколько реконструировано ---
by_src = {}
for k, (_, src) in SPEC.items():
    by_src.setdefault(src, []).append(k)
documented = len(by_src.get("spec", [])) + len(by_src.get("blue", []))
completeness = 100.0 * documented / len(SPEC)
print(f"@@ --- data provenance: {documented}/{len(SPEC)} dimensions documented ({completeness:.0f}%) ---")
for src in ("spec", "blue", "deriv", "est"):
    if by_src.get(src):
        print(f"@@ {src:6s} {len(by_src[src]):2d}: {', '.join(sorted(by_src[src]))}")
report["_provenance"] = {"sources": {k: v[1] for k, v in SPEC.items()},
                         "documented": documented, "total": len(SPEC),
                         "completeness_pct": round(completeness, 1)}

# --- Порядок и направление съёма при неполной разборке ---
# Неполная разборка (field strip) и полная (armorer). dir - направление съёма, dist - на сколько отводится.
report["_fieldstrip"] = [
    {"part": "Magazine", "order": 1, "dir": [0, 0, -1], "dist_mm": 120, "name": "Магазин"},
    {"part": "Slide", "order": 2, "dir": [1, 0, 0], "dist_mm": 200, "name": "Затвор в сборе"},
    {"part": "RecoilSpring", "order": 3, "dir": [1, 0, 0], "dist_mm": 120, "name": "Возвратная пружина в сборе"},
    {"part": "Barrel", "order": 4, "dir": [1, 0, 0.3], "dist_mm": 140, "name": "Ствол"},
    {"part": "Frame", "order": 5, "dir": [0, 0, 0], "dist_mm": 0, "name": "Рамка"},
]
report["_fullstrip"] = [
    {"part": "SlideCoverPlate",  "group": "slide", "order": 1, "dir": [0, 0, -1],  "dist_mm": 40, "name": "Крышка затвора"},
    {"part": "SpringCups",       "group": "slide", "order": 2, "dir": [0, 0, 1],   "dist_mm": 45, "name": "Полукольца пружины"},
    {"part": "FiringPinSpring",  "group": "slide", "order": 3, "dir": [-1, 0, 0],  "dist_mm": 70, "name": "Пружина ударника"},
    {"part": "SpacerSleeve",     "group": "slide", "order": 4, "dir": [-1, 0, 0],  "dist_mm": 55, "name": "Муфта"},
    {"part": "Striker",          "group": "slide", "order": 5, "dir": [-1, 0, 0],  "dist_mm": 90, "name": "Ударник"},
    {"part": "FiringPinSafety",  "group": "slide", "order": 6, "dir": [0, 0, -1],  "dist_mm": 35, "name": "Предохранитель ударника"},
    {"part": "ExtractorPlunger", "group": "slide", "order": 7, "dir": [0, 1, 0],   "dist_mm": 45, "name": "Толкатель выбрасывателя"},
    {"part": "Extractor",        "group": "slide", "order": 8, "dir": [0, 1, 0],   "dist_mm": 40, "name": "Выбрасыватель"},
    {"part": "SlideLock",        "group": "frame", "order": 9, "dir": [0, -1, 0],  "dist_mm": 55, "name": "Защёлка разборки"},
    {"part": "LockingBlock",     "group": "frame", "order": 10, "dir": [0, 0, 1],  "dist_mm": 50, "name": "Блок запирания"},
    {"part": "SlideStop",        "group": "frame", "order": 11, "dir": [0, -1, 0], "dist_mm": 45, "name": "Затворная задержка"},
    {"part": "TriggerHousing",   "group": "frame", "order": 12, "dir": [0, 0, -1], "dist_mm": 60, "name": "Корпус УСМ с отражателем"},
    {"part": "Connector",        "group": "frame", "order": 13, "dir": [0, 1, 0],  "dist_mm": 40, "name": "Коннектор"},
    {"part": "Trigger",          "group": "frame", "order": 14, "dir": [0, 0, -1], "dist_mm": 70, "name": "Спусковой крючок с тягой"},
    {"part": "MagCatch",         "group": "frame", "order": 15, "dir": [0, 1, 0],  "dist_mm": 40, "name": "Защёлка магазина"},
]

for ob in parts.values():
    ob.select_set(True)
bpy.ops.export_scene.fbx(filepath=os.path.join(OUT, f"Glock17_{GEN}.fbx"), use_selection=True, object_types={"MESH"},
                         apply_unit_scale=True, apply_scale_options="FBX_SCALE_NONE", axis_forward="-Y", axis_up="Z",
                         bake_space_transform=True, mesh_smooth_type="FACE")
with open(os.path.join(OUT, f"Glock17_{GEN}.json"), "w", encoding="utf-8") as f:
    json.dump({"generation": GEN, "gen_features": G, "spec": {k: v[0] for k, v in SPEC.items()}, "parts": report}, f, indent=1, ensure_ascii=False)

# --- Разнесённый вид: детали раздвигаются по тем же осям, что заданы для разборки в игре ---
EXPLODE = float(os.environ.get("PBL_EXPLODE", "0"))
if EXPLODE > 0:
    import mathutils as MU
    for item in report["_fieldstrip"] + report["_fullstrip"]:
        ob = parts.get(item["part"])
        if ob and item["dist_mm"]:
            d = MU.Vector(item["dir"]).normalized()
            L.move(ob, d * item["dist_mm"] * EXPLODE)

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
# Рамка кадра под габарит изделия: длина 204, высота 138 мм.
if EXPLODE > 0:
    # Детали расходятся на 400 мм по длине - кадр под весь набор
    bpy.ops.object.camera_add(location=(60, -900, 120), rotation=(math.radians(82), 0, math.radians(5)))
    sc.camera = bpy.context.active_object
    sc.camera.data.lens = 40
    sc.render.filepath = os.path.join(OUT, f"exploded_{GEN}.png")
else:
    bpy.ops.object.camera_add(location=(-100, -450, -45), rotation=(math.radians(90), 0, 0))
    sc.camera = bpy.context.active_object
    sc.camera.data.lens = 50
bpy.ops.render.render(write_still=True)
print(f"@@ exported to {OUT}; dimension check {'PASSED' if ok else 'FAILED'}")
