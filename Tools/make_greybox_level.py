"""
Генерирует greybox-уровень /Game/Maps/Greybox из движковых примитивов.

Запуск без редактора: Tools/make_greybox_level.bat
Уровень пересоздаётся с нуля при каждом запуске. Руками в редакторе его не править -
изменения потеряются. Вся геометрия описана ниже в LAYOUT, единицы - сантиметры.

Масштаб ориентирован на CS (1u = 1.905 см): игрок 137 см, глаза 122 см,
укрытие "по грудь" 110 см (можно выглянуть), "по пояс" 80 см, коридор ~270 см.
"""
import unreal

LEVEL_PATH = "/Game/Maps"
LEVEL_NAME = "Greybox"
CUBE = "/Engine/BasicShapes/Cube"          # 100x100x100 см, пивот в центре

ARENA = 4000        # сторона квадрата
WALL_H = 350
WALL_T = 30
CHEST_H = 110
WAIST_H = 80

# (label, center(x,y,z), size(x,y,z), rotation(pitch,yaw,roll))
LAYOUT = []

def box(label, center, size, rot=(0, 0, 0)):
    LAYOUT.append((label, center, size, rot))

# Пол: верх на Z=0
box("Floor", (0, 0, -10), (ARENA, ARENA, 20))

# Периметр
half = ARENA / 2
box("Wall_N", (0,  half, WALL_H / 2), (ARENA + WALL_T, WALL_T, WALL_H))
box("Wall_S", (0, -half, WALL_H / 2), (ARENA + WALL_T, WALL_T, WALL_H))
box("Wall_E", ( half, 0, WALL_H / 2), (WALL_T, ARENA, WALL_H))
box("Wall_W", (-half, 0, WALL_H / 2), (WALL_T, ARENA, WALL_H))

# Внутренние стены
box("Wall_Long",      (0,   -600, WALL_H / 2), (1600, WALL_T, WALL_H))
box("Wall_Cross",     (800, -100, WALL_H / 2), (WALL_T, 1000, WALL_H))
box("Corridor_A",     (-1250, 800, WALL_H / 2), (WALL_T, 1200, WALL_H))
box("Corridor_B",     (-950,  800, WALL_H / 2), (WALL_T, 1200, WALL_H))

# Укрытия по грудь (выглянуть можно, глаза на 122)
for i, (x, y) in enumerate([(300, 600), (600, 900), (-300, 300), (-1600, -200)]):
    box(f"Cover_Chest_{i}", (x, y, CHEST_H / 2), (120, 60, CHEST_H))

# Укрытия по пояс
for i, (x, y) in enumerate([(1200, 1200), (1400, -1200), (-600, -1300), (200, 1500)]):
    box(f"Cover_Waist_{i}", (x, y, WAIST_H / 2), (150, 80, WAIST_H))

# Ящики в полный рост
for i, (x, y) in enumerate([(1500, 400), (-1500, -600), (-200, 1100)]):
    box(f"Crate_{i}", (x, y, 100), (200, 200, 200))

# Лестница: 10 ступеней по 20 см вверх, 30 см вглубь; ведёт на платформу высотой 200
for i in range(10):
    h = 20 * (i + 1)
    box(f"Stair_{i}", (1000 + 15 + 30 * i, -1500, h / 2), (30, 200, h))
box("Platform", (1500, -1500, 100), (400, 400, 200))

# Рампа: подъём 200 на длине 400
box("Ramp", (-1500, 1400, 100), (447, 200, 20), rot=(26.57, 0, 0))
box("Ramp_Top", (-1150, 1400, 100), (250, 200, 200))

PLAYER_STARTS = [(-1700, -1700, 100, 45), (1700, 1700, 100, -135), (-1700, 1700, 100, -45), (1700, -700, 100, 135)]

# Свет и экспозиция. Экспозиция фиксирована (спека зрения: авто-экспозиция даст швы на Э6).
SUN_ROTATION = (-50, 35, 0)
SUN_INTENSITY_LUX = 10.0
EXPOSURE_BIAS = -1.0

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


def main():
    full = f"{LEVEL_PATH}/{LEVEL_NAME}"
    if eal.does_asset_exist(full):
        # Не удаляем (уровень может быть текущим/стартовым) - загружаем и вычищаем.
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

    for label, center, size, rot in LAYOUT:
        a = spawn(unreal.StaticMeshActor, center, rot, label, "Geometry")
        a.static_mesh_component.set_static_mesh(cube)
        a.set_actor_scale3d(unreal.Vector(size[0] / 100.0, size[1] / 100.0, size[2] / 100.0))
    log(f"geometry: {len(LAYOUT)} boxes")

    for i, (x, y, z, yaw) in enumerate(PLAYER_STARTS):
        spawn(unreal.PlayerStart, (x, y, z), (0, yaw, 0), f"PlayerStart_{i}", "Spawns")
    log(f"player starts: {len(PLAYER_STARTS)}")

    # Свет полностью динамический (Lumen): без Movable он ждёт запечённых лайтмап,
    # и уровень рендерится чёрным с надписью LIGHTING NEEDS TO BE REBUILT.
    sun = spawn(unreal.DirectionalLight, (0, 0, 1000), SUN_ROTATION, "Sun", "Lighting")
    sun_c = sun.get_component_by_class(unreal.DirectionalLightComponent)
    sun_c.set_mobility(unreal.ComponentMobility.MOVABLE)
    sun_c.set_intensity(SUN_INTENSITY_LUX)
    sky = spawn(unreal.SkyLight, (0, 0, 1000), label="SkyLight", folder="Lighting")
    sky_c = sky.get_component_by_class(unreal.SkyLightComponent)
    sky_c.set_mobility(unreal.ComponentMobility.MOVABLE)
    sky_c.set_editor_property("real_time_capture", True)

    ws = ell.get_editor_world().get_world_settings()
    ws.set_editor_property("force_no_precomputed_lighting", True)
    spawn(unreal.SkyAtmosphere, (0, 0, 0), label="SkyAtmosphere", folder="Lighting")

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
