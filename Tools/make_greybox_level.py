"""
Ð“ÐµÐ½ÐµÑ€Ð¸Ñ€ÑƒÐµÑ‚ greybox-ÑƒÑ€Ð¾Ð²ÐµÐ½ÑŒ /Game/Maps/Greybox Ð¸Ð· Ð´Ð²Ð¸Ð¶ÐºÐ¾Ð²Ñ‹Ñ… Ð¿Ñ€Ð¸Ð¼Ð¸Ñ‚Ð¸Ð²Ð¾Ð².

Ð—Ð°Ð¿ÑƒÑÐº Ð±ÐµÐ· Ñ€ÐµÐ´Ð°ÐºÑ‚Ð¾Ñ€Ð°: Tools/make_greybox_level.bat
Ð£Ñ€Ð¾Ð²ÐµÐ½ÑŒ Ð¿ÐµÑ€ÐµÑÐ¾Ð·Ð´Ð°Ñ‘Ñ‚ÑÑ Ñ Ð½ÑƒÐ»Ñ Ð¿Ñ€Ð¸ ÐºÐ°Ð¶Ð´Ð¾Ð¼ Ð·Ð°Ð¿ÑƒÑÐºÐµ. Ð ÑƒÐºÐ°Ð¼Ð¸ Ð² Ñ€ÐµÐ´Ð°ÐºÑ‚Ð¾Ñ€Ðµ ÐµÐ³Ð¾ Ð½Ðµ Ð¿Ñ€Ð°Ð²Ð¸Ñ‚ÑŒ -
Ð¸Ð·Ð¼ÐµÐ½ÐµÐ½Ð¸Ñ Ð¿Ð¾Ñ‚ÐµÑ€ÑÑŽÑ‚ÑÑ. Ð’ÑÑ Ð³ÐµÐ¾Ð¼ÐµÑ‚Ñ€Ð¸Ñ Ð¾Ð¿Ð¸ÑÐ°Ð½Ð° Ð½Ð¸Ð¶Ðµ Ð² LAYOUT, ÐµÐ´Ð¸Ð½Ð¸Ñ†Ñ‹ - ÑÐ°Ð½Ñ‚Ð¸Ð¼ÐµÑ‚Ñ€Ñ‹.

ÐœÐ°ÑÑˆÑ‚Ð°Ð± Ð¾Ñ€Ð¸ÐµÐ½Ñ‚Ð¸Ñ€Ð¾Ð²Ð°Ð½ Ð½Ð° CS (1u = 1.905 ÑÐ¼): Ð¸Ð³Ñ€Ð¾Ðº 137 ÑÐ¼, Ð³Ð»Ð°Ð·Ð° 122 ÑÐ¼,
ÑƒÐºÑ€Ñ‹Ñ‚Ð¸Ðµ "Ð¿Ð¾ Ð³Ñ€ÑƒÐ´ÑŒ" 110 ÑÐ¼ (Ð¼Ð¾Ð¶Ð½Ð¾ Ð²Ñ‹Ð³Ð»ÑÐ½ÑƒÑ‚ÑŒ), "Ð¿Ð¾ Ð¿Ð¾ÑÑ" 80 ÑÐ¼, ÐºÐ¾Ñ€Ð¸Ð´Ð¾Ñ€ ~270 ÑÐ¼.
"""
import os
import unreal

ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))

LEVEL_PATH = "/Game/Maps"
LEVEL_NAME = "Greybox"
CUBE = "/Engine/BasicShapes/Cube"          # 100x100x100 ÑÐ¼, Ð¿Ð¸Ð²Ð¾Ñ‚ Ð² Ñ†ÐµÐ½Ñ‚Ñ€Ðµ
# ÐŸÐ¾Ð» - ÑÐ²Ð¾Ð¹ Ð¼Ð°Ñ‚ÐµÑ€Ð¸Ð°Ð» M_FloorGrid (ÑÐ¼. FLOOR_GRID_HLSL): WorldGridMaterial Ð´Ð²Ð¸Ð¶ÐºÐ° ÑˆÑƒÐ¼Ð¸Ñ‚ Ð¿Ñ€Ð¾Ñ†ÐµÐ´ÑƒÑ€Ð½Ð¾Ð¹
# ÐºÑ€Ð°Ð¿Ð¸Ð½Ð¾Ð¹, Ð·Ð°Ð²Ð¸ÑÑÑ‰ÐµÐ¹ Ð¾Ñ‚ Ð´Ð¸ÑÑ‚Ð°Ð½Ñ†Ð¸Ð¸, Ð¸ Ð² Ð±Ð¾ÐºÐ¾Ð²Ñ‹Ñ… Ð·Ð°Ñ…Ð²Ð°Ñ‚Ð°Ñ… Ð²Ñ‹Ð³Ð»ÑÐ´Ð¸Ñ‚ Ð¸Ð½Ð°Ñ‡Ðµ, Ñ‡ÐµÐ¼ Ð² Ñ†ÐµÐ½Ñ‚Ñ€Ðµ.

ARENA = 4000        # ÑÑ‚Ð¾Ñ€Ð¾Ð½Ð° ÐºÐ²Ð°Ð´Ñ€Ð°Ñ‚Ð°
WALL_H = 350
WALL_T = 30
CHEST_H = 135   # Ð¿Ð¾ Ð³Ñ€ÑƒÐ´ÑŒ Ð´Ð»Ñ Ñ€Ð¾ÑÑ‚Ð° 176 (Ð³Ð»Ð°Ð·Ð° 165)
WAIST_H = 95

# --- Бункер: полигон - закрытый бетонный зал, а не открытая площадка ---
# Свет идёт только от потолочных светильников, поэтому перекрытие, балки и колонны
# нужны не для красоты: без них Lumen нечем отражать, и зал выглядит плоским.
CEIL_T = 30           # толщина перекрытия
BEAM_H, BEAM_W = 50, 60
BEAM_STEP = 500       # шаг балок в зале
COL_W = 60            # сечение колонны
LAMP_LUMENS = 6000    # светильник ~ промышленный светодиодный, 4000 лм
LAMP_STEP = 700       # шаг светильников в зале
LANE_LAMP_STEP = 2000 # шаг светильников в тире

# (label, center(x,y,z), size(x,y,z), rotation(pitch,yaw,roll))
LAYOUT = []

def box(label, center, size, rot=(0, 0, 0)):
    LAYOUT.append((label, center, size, rot))

# ÐŸÐ¾Ð»: Ð²ÐµÑ€Ñ… Ð½Ð° Z=0
box("Floor", (0, 0, -10), (ARENA, ARENA, 20))

# ÐŸÐµÑ€Ð¸Ð¼ÐµÑ‚Ñ€
half = ARENA / 2
box("Wall_N", (0,  half, WALL_H / 2), (ARENA + WALL_T, WALL_T, WALL_H))
box("Wall_S", (0, -half, WALL_H / 2), (ARENA + WALL_T, WALL_T, WALL_H))
box("Wall_E", ( half, 0, WALL_H / 2), (WALL_T, ARENA, WALL_H))
box("Wall_W", (-half, 0, WALL_H / 2), (WALL_T, ARENA, WALL_H))

# Ð’Ð½ÑƒÑ‚Ñ€ÐµÐ½Ð½Ð¸Ðµ ÑÑ‚ÐµÐ½Ñ‹
box("Wall_Long",      (0,   -600, WALL_H / 2), (1600, WALL_T, WALL_H))
box("Wall_Cross",     (800, -100, WALL_H / 2), (WALL_T, 1000, WALL_H))
box("Corridor_A",     (-1250, 800, WALL_H / 2), (WALL_T, 1200, WALL_H))
box("Corridor_B",     (-950,  800, WALL_H / 2), (WALL_T, 1200, WALL_H))

# Ð£ÐºÑ€Ñ‹Ñ‚Ð¸Ñ Ð¿Ð¾ Ð³Ñ€ÑƒÐ´ÑŒ (Ð²Ñ‹Ð³Ð»ÑÐ½ÑƒÑ‚ÑŒ Ð¼Ð¾Ð¶Ð½Ð¾, Ð³Ð»Ð°Ð·Ð° Ð½Ð° 122)
for i, (x, y) in enumerate([(300, 600), (600, 900), (-300, 300), (-1600, -200)]):
    box(f"Cover_Chest_{i}", (x, y, CHEST_H / 2), (120, 60, CHEST_H))

# Ð£ÐºÑ€Ñ‹Ñ‚Ð¸Ñ Ð¿Ð¾ Ð¿Ð¾ÑÑ
for i, (x, y) in enumerate([(1200, 1200), (1400, -1200), (-600, -1300), (200, 1500)]):
    box(f"Cover_Waist_{i}", (x, y, WAIST_H / 2), (150, 80, WAIST_H))

# Ð¯Ñ‰Ð¸ÐºÐ¸ Ð² Ð¿Ð¾Ð»Ð½Ñ‹Ð¹ Ñ€Ð¾ÑÑ‚
for i, (x, y) in enumerate([(1500, 400), (-1500, -600), (-200, 1100)]):
    box(f"Crate_{i}", (x, y, 100), (200, 200, 200))

# Ð›ÐµÑÑ‚Ð½Ð¸Ñ†Ð°: 10 ÑÑ‚ÑƒÐ¿ÐµÐ½ÐµÐ¹ Ð¿Ð¾ 20 ÑÐ¼ Ð²Ð²ÐµÑ€Ñ…, 30 ÑÐ¼ Ð²Ð³Ð»ÑƒÐ±ÑŒ; Ð²ÐµÐ´Ñ‘Ñ‚ Ð½Ð° Ð¿Ð»Ð°Ñ‚Ñ„Ð¾Ñ€Ð¼Ñƒ Ð²Ñ‹ÑÐ¾Ñ‚Ð¾Ð¹ 200
for i in range(10):
    h = 20 * (i + 1)
    box(f"Stair_{i}", (1000 + 15 + 30 * i, -1500, h / 2), (30, 200, h))
box("Platform", (1500, -1500, 100), (400, 400, 200))

# Ð Ð°Ð¼Ð¿Ð°: Ð¿Ð¾Ð´ÑŠÑ‘Ð¼ 200 Ð½Ð° Ð´Ð»Ð¸Ð½Ðµ 400
box("Ramp", (-1500, 1400, 100), (447, 200, 20), rot=(26.57, 0, 0))
box("Ramp_Top", (-1150, 1400, 100), (250, 200, 200))

# ÐœÐ¸ÑˆÐµÐ½Ð¸-Ð¼Ð°Ð½ÐµÐºÐµÐ½Ñ‹ (Ð­5): APBLTargetDummy. (x, y, yaw)
DUMMIES = [(-900, 1500, 180), (300, 200, -90), (1500, -300, 135)]

# Ð¡Ñ‚Ñ€ÐµÐ»ÐºÐ¾Ð²Ð°Ñ Ð¿Ð¾Ð»Ð¾ÑÐ° (E10.4): Ð¾Ñ‚ Ð²Ð¾ÑÑ‚Ð¾Ñ‡Ð½Ð¾Ð¹ ÑÑ‚ÐµÐ½Ñ‹ Ð°Ñ€ÐµÐ½Ñ‹ Ð½Ð° Ð²Ð¾ÑÑ‚Ð¾Ðº, 8 Ð¼ ÑˆÐ¸Ñ€Ð¸Ð½Ð¾Ð¹, 320 Ð¼ Ð´Ð»Ð¸Ð½Ð¾Ð¹.
# ÐŸÑ€Ð¾Ñ‘Ð¼ Ð² Ð²Ð¾ÑÑ‚Ð¾Ñ‡Ð½Ð¾Ð¹ ÑÑ‚ÐµÐ½Ðµ, Ð¿Ð¾Ð», Ð±Ð¾ÐºÐ¾Ð²Ñ‹Ðµ ÑÑ‚ÐµÐ½Ñ‹, Ð¼ÐµÑ‚ÐºÐ¸ Ð´Ð¸ÑÑ‚Ð°Ð½Ñ†Ð¸Ð¸ ÐºÐ°Ð¶Ð´Ñ‹Ðµ 25 Ð¼, Ñ‰Ð¸Ñ‚Ñ‹-Ð¼Ð¸ÑˆÐµÐ½Ð¸.
LANE_START_X = half + WALL_T            # Ð½Ð°Ñ‡Ð°Ð»Ð¾ Ð¿Ð¾Ð»Ð¾ÑÑ‹ Ð·Ð° ÑÑ‚ÐµÐ½Ð¾Ð¹
LANE_LEN = 32000
LANE_W = 800
LANE_TARGETS = [25, 50, 100, 200, 300]  # Ð¼, Ñ‰Ð¸Ñ‚Ñ‹ 1x1.5 Ð¼
LANE_MARKS = list(range(25, 301, 25))

# Ð‘Ð»Ð¾Ðº Ð³ÐµÐ»Ñ (E10.6): Ð¿Ñ€Ð¾Ñ‚Ð¾ÐºÐ¾Ð» FBI - 10 ft (3 Ð¼) Ð¾Ñ‚ Ð´ÑƒÐ»Ð°. PS4 (2100,0) ÑÐ¼Ð¾Ñ‚Ñ€Ð¸Ñ‚ Ð½Ð° Ð²Ð¾ÑÑ‚Ð¾Ðº Ð²Ð´Ð¾Ð»ÑŒ Ð¿Ð¾Ð»Ð¾ÑÑ‹.
GEL_BLOCKS = [(2100 + 300 + 50, 150)]   # ближняя грань на 3 м от PS4, смещён вправо (y=+150), чтобы не закрывать куклу на 5 м; с PS4 - yaw ~27°
# Стойка материалов (E10.6c): панели 60x60 см на 10 м от PS4, центр на высоте дула. (материал CSV, толщина см, ширина см, вид)
PANEL_DIST_M = 10
PANELS = [("Drywall", 1.27, "/Game/Range/M_PanelDrywall"), ("Plywood", 1.8, "/Game/Range/M_PanelPlywood"),
          ("Pine", 10.0, "/Game/Range/M_PanelPine"), ("MildSteel", 0.5, "/Game/Range/M_PanelSteel")]
PANEL_W = 60
PANEL_GAP = 30
# Гелевая кукла (E10.6/E10.7-заготовка): человек 176 см из блоков геля 10%, на 5 м от PS4, лицом к стрелку.
# (имя, размер (глубина X, ширина Y, высота Z) см, центр (y, z) см, часть тела из BodyLayers.csv - слои внутри; None = однородный гель)
GEL_DUMMY_DIST_M = 5
GEL_DUMMY_PARTS = [
    ("Head",  (20, 16, 23), (0, 164), "Head"),
    ("Neck",  (11, 11, 8),  (0, 149), "Neck"),
    ("Torso", (24, 38, 60), (0, 115), "Torso"),
    ("Pelvis",(24, 36, 22), (0, 74),  "Pelvis"),
    ("ArmL",  (10, 10, 62), (26, 112), "Arm"),
    ("ArmR",  (10, 10, 62), (-26, 112), "Arm"),
    ("LegL",  (14, 15, 63), (10, 31), "Leg"),
    ("LegR",  (14, 15, 63), (-10, 31), "Leg"),
]
GEL_STAND_H = 158   # Ñ†ÐµÐ½Ñ‚Ñ€ Ð±Ð»Ð¾ÐºÐ° (+7.5) Ð½Ð° Ð²Ñ‹ÑÐ¾Ñ‚Ðµ Ð´ÑƒÐ»Ð° Ð¿Ñ€Ð¸ ÑÑ‚Ñ€ÐµÐ»ÑŒÐ±Ðµ Ð³Ð¾Ñ€Ð¸Ð·Ð¾Ð½Ñ‚Ð°Ð»ÑŒÐ½Ð¾ Ñ PS4 (Ð³Ð»Ð°Ð·Ð° 165, Ð´ÑƒÐ»Ð¾ ~165 ÑÐ¼)

PLAYER_STARTS = [(-1700, -1700, 100, 45), (1700, 1700, 100, -135), (-1700, 1700, 100, -45), (1700, -700, 100, 135), (2100, 0, 100, 0)]

# Ð¡Ð²ÐµÑ‚ Ð¸ ÑÐºÑÐ¿Ð¾Ð·Ð¸Ñ†Ð¸Ñ. Ð­ÐºÑÐ¿Ð¾Ð·Ð¸Ñ†Ð¸Ñ Ñ„Ð¸ÐºÑÐ¸Ñ€Ð¾Ð²Ð°Ð½Ð° (ÑÐ¿ÐµÐºÐ° Ð·Ñ€ÐµÐ½Ð¸Ñ: Ð°Ð²Ñ‚Ð¾-ÑÐºÑÐ¿Ð¾Ð·Ð¸Ñ†Ð¸Ñ Ð´Ð°ÑÑ‚ ÑˆÐ²Ñ‹ Ð½Ð° Ð­6).
SKY_FILL = 0.25   # Ð½ÐµÐ±Ð¾ ÑÑ€Ñ‡Ðµ: Ñ‚ÐµÐ½Ð¸ Ð½Ðµ Ñ‡Ñ‘Ñ€Ð½Ñ‹Ðµ (Ñ€ÐµÐ°Ð»ÑŒÐ½Ð¾Ðµ Ð¾Ñ‚Ð½Ð¾ÑˆÐµÐ½Ð¸Ðµ ÑÐ¾Ð»Ð½Ñ†Ðµ/Ñ‚ÐµÐ½ÑŒ ~5-10:1; Ð´Ð»Ñ Ð¿Ð¾Ð»Ð¸Ð³Ð¾Ð½Ð° Ð½ÑƒÐ¶Ð½Ð° Ñ€Ð¾Ð²Ð½Ð°Ñ Ð¾ÑÐ²ÐµÑ‰Ñ‘Ð½Ð½Ð¾ÑÑ‚ÑŒ)
EXPOSURE_BIAS = 3.5   # Ñ€ÑƒÑ‡Ð½Ð°Ñ ÐºÐ°Ð¼ÐµÑ€Ð° f/4 1/60 ISO100 = EV~10; ÑÐ¾Ð»Ð½Ñ†Ðµ 10 lux Ð´Ð°Ñ‘Ñ‚ EV~4 -> ÐºÐ¾Ð¼Ð¿ÐµÐ½ÑÐ¸Ñ€ÑƒÐµÐ¼

# ---------------------------------------------------------------------------

ell = unreal.EditorLevelLibrary
eal = unreal.EditorAssetLibrary


def log(msg):
    unreal.log(f"[greybox] {msg}")


def spawn(cls, loc, rot=(0, 0, 0), label=None, folder=None):
    actor = ell.spawn_actor_from_class(cls, unreal.Vector(*loc), unreal.Rotator(rot[2], rot[0], rot[1]))
    if actor is None:
        raise RuntimeError(f"spawn failed: {cls} {label}")
    if label:
        actor.set_actor_label(label)
    if folder:
        actor.set_folder_path(folder)
    return actor


FLOOR_GRID_HLSL = """
// Метровая сетка по мировым координатам. Ширина линий - в МИРОВЫХ единицах (2 см / 5 см): в перспективе линии
// сужаются как настоящая разметка. Антиалиасинг - по осям отдельно (fwidth.x для линий x=const, fwidth.y для y=const),
// и с сохранением «количества краски»: когда пиксель шире линии, линия становится шире, но бледнее (амплитуда w/fw),
// а не расползается в серую полосу. Раньше ширина была экранной (1.5 px min) - вдали линии не сужались и давали полосы.
float2 p  = WorldPos.xy / 100.0f;
float2 fw = max(fwidth(p), 1e-5f);
float  w1 = 0.010f;                                   // полуширина 1-м линии в клетках: 2 см
float2 d1 = 0.5f - abs(frac(p) - 0.5f);               // расстояние до ближайшей линии по каждой оси, в клетках
float2 a1 = saturate(w1 / max(fw, w1));                // амплитуда: сохраняем интеграл яркости
float2 c1 = (1.0f - smoothstep(w1 - fw, w1 + fw, d1)) * a1;
float  l1 = max(c1.x, c1.y);
float2 p5 = p / 5.0f;
float2 fw5 = fw / 5.0f;
float  w5 = 0.005f;                                    // 5-м линия: 5 см
float2 d5 = 0.5f - abs(frac(p5) - 0.5f);
float2 a5 = saturate(w5 / max(fw5, w5));
float2 c5 = (1.0f - smoothstep(w5 - fw5, w5 + fw5, d5)) * a5;
float  l5 = max(c5.x, c5.y);
// Тёмный бетон, разметка - светлая краска поверх: так читается пол цеха, а не белая плита.
float  base = 0.055f;
float  c = lerp(lerp(base, 0.10f, l1), 0.32f, l5);
return float3(c, c, c);
"""


def make_simple_material(name, base_color, roughness, emissive=None):
    """Материал-константа: цвет, шероховатость и по желанию свечение."""
    path = "/Game/Greybox"
    full = f"{path}/{name}"
    mel = unreal.MaterialEditingLibrary
    if eal.does_asset_exist(full):
        mat = eal.load_asset(full)
        mel.delete_all_material_expressions(mat)
    else:
        mat = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, path, unreal.Material, unreal.MaterialFactoryNew())
    col = mel.create_material_expression(mat, unreal.MaterialExpressionConstant3Vector, -400, 0)
    col.set_editor_property("constant", unreal.LinearColor(*base_color, 1.0))
    mel.connect_material_property(col, "", unreal.MaterialProperty.MP_BASE_COLOR)
    r = mel.create_material_expression(mat, unreal.MaterialExpressionConstant, -400, 200)
    r.set_editor_property("r", roughness)
    mel.connect_material_property(r, "", unreal.MaterialProperty.MP_ROUGHNESS)
    if emissive:
        e = mel.create_material_expression(mat, unreal.MaterialExpressionConstant3Vector, -400, 400)
        e.set_editor_property("constant", unreal.LinearColor(*emissive, 1.0))
        mel.connect_material_property(e, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.recompile_material(mat)
    eal.save_loaded_asset(mat)
    log(f"material {full}")
    return mat


def make_floor_material():
    """ÐœÐ°Ñ‚ÐµÑ€Ð¸Ð°Ð» Ð¿Ð¾Ð»Ð°: ÑÐµÑ€Ñ‹Ð¹ Ñ Ð¼ÐµÑ‚Ñ€Ð¾Ð²Ð¾Ð¹ ÑÐµÑ‚ÐºÐ¾Ð¹ (Custom node, Ð¼Ð¸Ñ€Ð¾Ð²Ñ‹Ðµ ÐºÐ¾Ð¾Ñ€Ð´Ð¸Ð½Ð°Ñ‚Ñ‹)."""
    path, name = "/Game/Greybox", "M_FloorGrid"
    full = f"{path}/{name}"
    mel = unreal.MaterialEditingLibrary
    if eal.does_asset_exist(full):
        mat = eal.load_asset(full)
        mel.delete_all_material_expressions(mat)
    else:
        mat = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, path, unreal.Material, unreal.MaterialFactoryNew())
    custom = mel.create_material_expression(mat, unreal.MaterialExpressionCustom, -400, 0)
    custom.set_editor_property("code", FLOOR_GRID_HLSL)
    custom.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    ci = unreal.CustomInput()
    ci.set_editor_property("input_name", "WorldPos")
    custom.set_editor_property("inputs", [ci])
    wp = mel.create_material_expression(mat, unreal.MaterialExpressionWorldPosition, -800, 0)
    if not mel.connect_material_expressions(wp, "", custom, "WorldPos"):
        raise RuntimeError("floor: connect WorldPos failed")
    if not mel.connect_material_property(custom, "", unreal.MaterialProperty.MP_BASE_COLOR):
        raise RuntimeError("floor: connect BaseColor failed")
    rough = mel.create_material_expression(mat, unreal.MaterialExpressionConstant, -400, 300)
    rough.set_editor_property("r", 0.85)
    mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.recompile_material(mat)
    eal.save_loaded_asset(mat)
    log(f"floor material {full}")
    return mat


def add_bunker_shell():
    """Перекрытие, балки и колонны. Колонны, попадающие на уже стоящую геометрию, пропускаются."""
    half_ = ARENA / 2
    box("Ceiling", (0, 0, WALL_H + CEIL_T / 2), (ARENA + WALL_T, ARENA + WALL_T, CEIL_T))
    box("Lane_Ceiling", (LANE_START_X + LANE_LEN / 2, 0, WALL_H + CEIL_T / 2), (LANE_LEN, LANE_W, CEIL_T))
    n_beams = 0
    y = -half_ + BEAM_STEP
    while y < half_ - 1:
        box(f"Beam_{int(y)}", (0, y, WALL_H - BEAM_H / 2), (ARENA, BEAM_W, BEAM_H))
        y += BEAM_STEP
        n_beams += 1
    x = LANE_START_X + 1000
    while x < LANE_START_X + LANE_LEN:
        box(f"Lane_Beam_{int(x)}", (x, 0, WALL_H - BEAM_H / 2), (BEAM_W, LANE_W, BEAM_H))
        x += 1000
        n_beams += 1

    solid = [(c[0], c[1], sz[0], sz[1]) for (lbl, c, sz, _r) in LAYOUT
             if not lbl.startswith(("Floor", "Ceiling", "Beam", "Lane_"))]
    n_cols = 0
    for cx in (-1500, -500, 500, 1500):
        for cy in (-1500, -500, 500, 1500):
            if any(abs(cx - ox) < (osx + COL_W) / 2 + 50 and abs(cy - oy) < (osy + COL_W) / 2 + 50
                   for (ox, oy, osx, osy) in solid):
                continue
            box(f"Column_{cx}_{cy}", (cx, cy, WALL_H / 2), (COL_W, COL_W, WALL_H))
            n_cols += 1
    log(f"bunker shell: ceiling, {n_beams} beams, {n_cols} columns")


def main():
    full = f"{LEVEL_PATH}/{LEVEL_NAME}"
    if eal.does_asset_exist(full):
        # ÐÐµ ÑƒÐ´Ð°Ð»ÑÐµÐ¼ (ÑƒÑ€Ð¾Ð²ÐµÐ½ÑŒ Ð¼Ð¾Ð¶ÐµÑ‚ Ð±Ñ‹Ñ‚ÑŒ Ñ‚ÐµÐºÑƒÑ‰Ð¸Ð¼/ÑÑ‚Ð°Ñ€Ñ‚Ð¾Ð²Ñ‹Ð¼) - Ð·Ð°Ð³Ñ€ÑƒÐ¶Ð°ÐµÐ¼ Ð¸ Ð²Ñ‹Ñ‡Ð¸Ñ‰Ð°ÐµÐ¼.
        if not ell.load_level(full):
            raise RuntimeError("load_level failed")
        old = ell.get_all_level_actors()
        for a in old:
            ell.destroy_actor(a)
        log(f"reuse {full}: removed {len(old)} actors")
    else:
        if not ell.new_level(full):
            raise RuntimeError("new_level failed")
        log(f"created {full}")

    cube = eal.load_asset(CUBE)
    if cube is None:
        raise RuntimeError(f"no mesh {CUBE}")

    # ÐŸÐ¾Ð»Ð¾ÑÐ°: Ð¿Ñ€Ð¾Ñ‘Ð¼ Ð² ÑÑ‚ÐµÐ½Ðµ
    box("Lane_Floor", (LANE_START_X + LANE_LEN / 2, 0, -10), (LANE_LEN, LANE_W, 20))
    box("Lane_Wall_N", (LANE_START_X + LANE_LEN / 2,  LANE_W / 2, WALL_H / 2), (LANE_LEN, WALL_T, WALL_H))
    box("Lane_Wall_S", (LANE_START_X + LANE_LEN / 2, -LANE_W / 2, WALL_H / 2), (LANE_LEN, WALL_T, WALL_H))
    box("Lane_Wall_End", (LANE_START_X + LANE_LEN, 0, WALL_H / 2), (WALL_T, LANE_W, WALL_H))
    for i, d in enumerate(LANE_MARKS):
        x = LANE_START_X + d * 100
        box(f"Lane_Mark_{d}", (x, LANE_W / 2 - 40, 60), (10, 10, 120))
        box(f"Lane_Mark2_{d}", (x, -LANE_W / 2 + 40, 60), (10, 10, 120))
    for d in LANE_TARGETS:
        x = LANE_START_X + d * 100
        box(f"Lane_Target_{d}", (x, 0, 150), (10, 100, 150))   # Ñ‰Ð¸Ñ‚ 1 Ð¼ x 1.5 Ð¼, Ñ†ÐµÐ½Ñ‚Ñ€ Ð½Ð° 1.5 Ð¼
    # ÐŸÑ€Ð¾Ñ‘Ð¼: Ð²Ð¾ÑÑ‚Ð¾Ñ‡Ð½ÑƒÑŽ ÑÑ‚ÐµÐ½Ñƒ Ñ€Ð¸ÑÑƒÐµÐ¼ Ð´Ð²ÑƒÐ¼Ñ Ð¿Ð¾Ð»Ð¾Ð²Ð¸Ð½Ð°Ð¼Ð¸ Ð²Ð¼ÐµÑÑ‚Ð¾ Ñ†ÐµÐ»Ð¾Ð¹
    LAYOUT[:] = [item for item in LAYOUT if item[0] != "Wall_E"]
    box("Wall_E_a", (half, (LANE_W / 2 + ARENA / 2) / 2 + 0, WALL_H / 2), (WALL_T, ARENA / 2 - LANE_W / 2, WALL_H))
    box("Wall_E_b", (half, -((LANE_W / 2 + ARENA / 2) / 2), WALL_H / 2), (WALL_T, ARENA / 2 - LANE_W / 2, WALL_H))

    add_bunker_shell()

    floor_mat = make_floor_material()
    # Бетон: альбедо ~0.2, как у настоящего, чуть тёплый и очень шероховатый.
    concrete = make_simple_material("M_Concrete", (0.200, 0.195, 0.185), 0.92)
    for label, center, size, rot in LAYOUT:
        a = spawn(unreal.StaticMeshActor, center, rot, label, "Geometry")
        a.static_mesh_component.set_static_mesh(cube)
        a.set_actor_scale3d(unreal.Vector(size[0] / 100.0, size[1] / 100.0, size[2] / 100.0))
        if label in ("Floor", "Lane_Floor") and floor_mat:
            a.static_mesh_component.set_material(0, floor_mat)
        elif concrete:
            a.static_mesh_component.set_material(0, concrete)
    log(f"geometry: {len(LAYOUT)} boxes")

    # ÐœÐ¸ÑˆÐµÐ½Ð¸ - C++ Ð°ÐºÑ‚Ð¾Ñ€ APBLTargetDummy (Ð¼ÐµÑˆ/Ð°Ð½Ð¸Ð¼Ð°Ñ†Ð¸Ð¸ Ð±ÐµÑ€Ñ‘Ñ‚ Ð¸Ð· DefaultGame.ini).
    for i, (x, y, yaw) in enumerate(DUMMIES):
        spawn(unreal.PBLTargetDummy, (x, y, 0), (0, yaw, 0), f"Dummy_{i}", "Targets")
    log(f"dummies: {len(DUMMIES)}")

    for i, (x, y) in enumerate(GEL_BLOCKS):
        st = spawn(unreal.StaticMeshActor, (x, y, GEL_STAND_H / 2), (0, 0, 0), f"Gel_Stand_{i}", "Range")
        st.static_mesh_component.set_static_mesh(cube)
        st.set_actor_scale3d(unreal.Vector(0.3, 0.3, GEL_STAND_H / 100.0))
        spawn(unreal.PBLMaterialBlock, (x, y, GEL_STAND_H + 7.5), (0, 0, 0), f"GelBlock_{i}", "Range")
    log(f"gel blocks: {len(GEL_BLOCKS)}")

    # Панели материалов: в ряд поперёк полосы, каждая на своей стойке, подпись над панелью.
    px = 2100 + PANEL_DIST_M * 100
    total = len(PANELS) * PANEL_W + (len(PANELS) - 1) * PANEL_GAP
    for i, (mat, thick, look) in enumerate(PANELS):
        py = -total / 2 + PANEL_W / 2 + i * (PANEL_W + PANEL_GAP)
        st = spawn(unreal.StaticMeshActor, (px, py, (GEL_STAND_H - 30) / 2), (0, 0, 0), f"Panel_Stand_{mat}", "Range")
        st.static_mesh_component.set_static_mesh(cube)
        st.set_actor_scale3d(unreal.Vector(0.1, 0.1, (GEL_STAND_H - 30) / 100.0))
        blk = spawn(unreal.PBLMaterialBlock, (px, py, GEL_STAND_H + 7.5), (0, 0, 0), f"Panel_{mat}", "Range")
        blk.set_editor_property("material_name", unreal.Name(mat))
        blk.set_editor_property("size", unreal.Vector(thick, PANEL_W, PANEL_W))
        lm = eal.load_asset(look)
        if lm:
            blk.set_editor_property("gel_material", lm)
        t = spawn(unreal.TextRenderActor, (px, py, GEL_STAND_H + 7.5 + PANEL_W / 2 + 12), (0, 180, 0), f"Panel_Label_{mat}", "Range")
        tr = t.text_render
        tr.set_text(unreal.Text(f"{mat} {thick*10:g} mm"))
        tr.set_editor_property("world_size", 10.0)
        tr.set_editor_property("horizontal_alignment", unreal.HorizTextAligment.EHTA_CENTER)
    log(f"material panels: {len(PANELS)} at {PANEL_DIST_M} m")

    # Оружейный стенд (F3): в арене, на высоте глаз, лицом к северу.
    bench = spawn(unreal.PBLWeaponBench, (-1700, -1200, 150), (0, 0, 0), "WeaponBench", "Range")
    log("weapon bench at (-1700, -1200, 150)")

    # Гелевая кукла: если есть меши из Blender (/Game/Range/GelDummy + Import/GelDummy/GelDummy.json) - реальная форма,
    # иначе коробки GEL_DUMMY_PARTS. Каждая часть - PBLMaterialBlock Gel10 со стеком слоёв BodyPart.
    gx = 2100 + GEL_DUMMY_DIST_M * 100
    meta_path = os.path.join(ROOT, "Import", "GelDummy", "GelDummy.json")
    used_mesh = False
    if os.path.exists(meta_path):
        import json
        with open(meta_path, encoding="utf-8") as f:
            meta = json.load(f)
        for name, info in meta.items():
            mesh = eal.load_asset(f"/Game/Range/GelDummy/{name}")
            if not mesh:
                continue
            cx, cy, cz = info["center_cm"]
            blk = spawn(unreal.PBLMaterialBlock, (gx + cx, cy, cz), (0, 0, 0), f"GelDummy_{name}", "Range")
            blk.set_editor_property("material_name", unreal.Name("Gel10"))
            blk.set_editor_property("block_mesh", mesh)
            blk.set_editor_property("body_part", unreal.Name(info["part"]))
            used_mesh = True
        log(f"gel dummy (Blender): {len(meta)} parts at {GEL_DUMMY_DIST_M} m")
    if not used_mesh:
        for name, size, (py, pz), part in GEL_DUMMY_PARTS:
            blk = spawn(unreal.PBLMaterialBlock, (gx, py, pz), (0, 0, 0), f"GelDummy_{name}", "Range")
            blk.set_editor_property("material_name", unreal.Name("Gel10"))
            blk.set_editor_property("size", unreal.Vector(*size))
            if part:
                blk.set_editor_property("body_part", unreal.Name(part))
        log(f"gel dummy (boxes): {len(GEL_DUMMY_PARTS)} parts at {GEL_DUMMY_DIST_M} m")

    for d in LANE_MARKS:
        t = spawn(unreal.TextRenderActor, (LANE_START_X + d * 100, LANE_W / 2 - 60, 200), (0, -90, 0), f"Lane_Label_{d}", "Lane")
        tr = t.text_render
        tr.set_text(unreal.Text(f"{d} m"))
        tr.set_editor_property("world_size", 60.0)
        tr.set_editor_property("horizontal_alignment", unreal.HorizTextAligment.EHTA_CENTER)
    log(f"lane: {LANE_LEN/100:.0f} m, marks {len(LANE_MARKS)}, targets {LANE_TARGETS}")

    for i, (x, y, z, yaw) in enumerate(PLAYER_STARTS):
        ps = spawn(unreal.PlayerStart, (x, y, z), (0, yaw, 0), f"PlayerStart_{i}", "Spawns")
        # Ð¢ÐµÐ³ Ð½ÑƒÐ¶ÐµÐ½, Ñ‡Ñ‚Ð¾Ð±Ñ‹ -PBLPlayerStart=PS<i> Ð´Ð°Ð²Ð°Ð» Ð²Ð¾ÑÐ¿Ñ€Ð¾Ð¸Ð·Ð²Ð¾Ð´Ð¸Ð¼ÑƒÑŽ Ñ‚Ð¾Ñ‡ÐºÑƒ Ð´Ð»Ñ ÑÐºÑ€Ð¸Ð½ÑˆÐ¾Ñ‚Ð¾Ð².
        ps.set_editor_property("player_start_tag", unreal.Name(f"PS{i}"))
    log(f"player starts: {len(PLAYER_STARTS)}")

    # Ð¡Ð²ÐµÑ‚ Ð¿Ð¾Ð»Ð½Ð¾ÑÑ‚ÑŒÑŽ Ð´Ð¸Ð½Ð°Ð¼Ð¸Ñ‡ÐµÑÐºÐ¸Ð¹ (Lumen): Ð±ÐµÐ· Movable Ð¾Ð½ Ð¶Ð´Ñ‘Ñ‚ Ð·Ð°Ð¿ÐµÑ‡Ñ‘Ð½Ð½Ñ‹Ñ… Ð»Ð°Ð¹Ñ‚Ð¼Ð°Ð¿,
    # Ð¸ ÑƒÑ€Ð¾Ð²ÐµÐ½ÑŒ Ñ€ÐµÐ½Ð´ÐµÑ€Ð¸Ñ‚ÑÑ Ñ‡Ñ‘Ñ€Ð½Ñ‹Ð¼ Ñ Ð½Ð°Ð´Ð¿Ð¸ÑÑŒÑŽ LIGHTING NEEDS TO BE REBUILT.
    # Бункер закрыт сверху, солнца и неба нет: светят только потолочные светильники,
    # остальное доносит отражённый свет Lumen. Каждый светильник - прямоугольная лампа
    # плюс видимая эмиссивная панель, иначе источник висит в воздухе невидимкой.
    lamp_mat = make_simple_material("M_Lamp", (0.02, 0.02, 0.02), 0.4, emissive=(9.0, 8.6, 7.6))
    lamp_z = WALL_H - BEAM_H - 6

    def lamp(x, y, width=200.0, label="Lamp"):
        rl = spawn(unreal.RectLight, (x, y, lamp_z), (-90, 0, 0), label, "Lighting")
        c = rl.get_component_by_class(unreal.RectLightComponent)
        c.set_mobility(unreal.ComponentMobility.MOVABLE)
        c.set_editor_property("intensity_units", unreal.LightUnits.LUMENS)
        c.set_intensity(LAMP_LUMENS)
        c.set_editor_property("source_width", width)
        c.set_editor_property("source_height", 18.0)
        c.set_attenuation_radius(1600.0)
        c.set_light_color(unreal.LinearColor(1.0, 0.955, 0.88, 1.0))
        c.set_editor_property("barn_door_angle", 88.0)
        # Промышленный светильник светит и вверх: без этой доли потолок остаётся чёрным,
        # а зал читается как открытая площадка под ночным небом.
        up = spawn(unreal.RectLight, (x, y, lamp_z + 10), (90, 0, 0), label + "_Up", "Lighting")
        uc = up.get_component_by_class(unreal.RectLightComponent)
        uc.set_mobility(unreal.ComponentMobility.MOVABLE)
        uc.set_editor_property("intensity_units", unreal.LightUnits.LUMENS)
        uc.set_intensity(LAMP_LUMENS * 0.3)
        uc.set_editor_property("source_width", width)
        uc.set_editor_property("source_height", 18.0)
        uc.set_attenuation_radius(700.0)
        uc.set_editor_property("cast_shadows", False)

        panel = spawn(unreal.StaticMeshActor, (x, y, lamp_z + 4), (0, 0, 0), label + "_Panel", "Lighting")
        panel.static_mesh_component.set_static_mesh(cube)
        panel.set_actor_scale3d(unreal.Vector(width / 100.0, 0.2, 0.06))
        panel.static_mesh_component.set_material(0, lamp_mat)
        panel.static_mesh_component.set_cast_shadow(False)

    n = 0
    y = -ARENA / 2 + LAMP_STEP / 2
    while y < ARENA / 2:
        x = -ARENA / 2 + LAMP_STEP / 2
        while x < ARENA / 2:
            lamp(x, y, 200.0, f"Lamp_{int(x)}_{int(y)}")
            x += LAMP_STEP
            n += 1
        y += LAMP_STEP
    x = LANE_START_X + 500
    while x < LANE_START_X + LANE_LEN:
        lamp(x, 0, 300.0, f"Lane_Lamp_{int(x)}")
        # Первые 100 м - рабочая зона, дальше свет реже: там только дальние щиты.
        x += 1000 if (x - LANE_START_X) < 10000 else LANE_LAMP_STEP
        n += 1
    log(f"lamps: {n} x {LAMP_LUMENS} lm")

    # Skylight нет намеренно: снаружи неба не видно, а пустой захват даёт только предупреждение.
    # Заполняющий свет в бункере даёт отражённый Lumen от бетона.

    ws = ell.get_editor_world().get_world_settings()
    ws.set_editor_property("force_no_precomputed_lighting", True)

    ppv = spawn(unreal.PostProcessVolume, (0, 0, 0), label="PP_FixedExposure", folder="Lighting")
    ppv.set_editor_property("unbound", True)
    st = ppv.get_editor_property("settings")
    st.set_editor_property("override_auto_exposure_method", True)
    st.set_editor_property("auto_exposure_method", unreal.AutoExposureMethod.AEM_MANUAL)
    st.set_editor_property("override_auto_exposure_bias", True)
    st.set_editor_property("auto_exposure_bias", EXPOSURE_BIAS)
    ppv.set_editor_property("settings", st)
    log("lighting + fixed exposure")

    if not ell.save_current_level():
        raise RuntimeError("save_current_level failed")
    log(f"saved {full}")
    log("DONE")


main()
