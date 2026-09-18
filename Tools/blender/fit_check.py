"""
Сверка силуэта модели с фотографией.

Пять габаритов говорят, что оружие нужной длины и высоты. Они ничего не говорят о форме:
модель может сойтись по всем пяти и выглядеть коробкой. Здесь силуэт модели рендерится
ортогонально с той же стороны, что снята фотография, накладывается на обведённый контур,
и расхождение считается площадью - в процентах и в миллиметрах.

Это и есть та обратная связь, которой не хватало: «похоже ли» перестаёт быть делом вкуса.

Мера - пересечение к объединению (IoU): 1.0 - силуэты совпали, 0.8 - каждая пятая часть
площади лишняя или недостающая. Рядом печатается, где именно модель толще или тоньше
фотографии, по участкам вдоль ствола.

Запуск: blender -b -P fit_check.py -- <скрипт_модели.py> <profile.json> <длина_мм>
"""
import os, sys, json, struct, zlib
import numpy as np
import bpy

ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
ARGS = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
MODEL_SCRIPT = ARGS[0] if ARGS else os.path.join(os.path.dirname(os.path.abspath(__file__)), "make_colt1911.py")
PROFILE = ARGS[1] if len(ARGS) > 1 else os.path.join(ROOT, "Reference", "Contours", "Colt1911_photo2.json")
LENGTH_MM = float(ARGS[2]) if len(ARGS) > 2 else 210.0

RES_X = 1600
MARGIN = 1.15          # запас кадра: силуэт не должен упираться в край


def write_png(path, arr):
    h, w, _ = arr.shape
    raw = b"".join(b"\x00" + arr[y].tobytes() for y in range(h))

    def chunk(tag, data):
        body = tag + data
        return struct.pack(">I", len(data)) + body + struct.pack(">I", zlib.crc32(body) & 0xFFFFFFFF)

    with open(path, "wb") as f:
        f.write(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">2I5B", w, h, 8, 2, 0, 0, 0))
                + chunk(b"IDAT", zlib.compress(raw, 6)) + chunk(b"IEND", b""))


# --- Строим модель тем же скриптом, что и всегда: сверять надо то, что идёт в игру ---
src = open(MODEL_SCRIPT, encoding="utf-8").read().split("# --- Превью ---")[0]
scope = {"__file__": MODEL_SCRIPT, "__name__": "__fit__"}
sys.argv = ["blender", "--"]
exec(compile(src, MODEL_SCRIPT, "exec"), scope)
parts = scope["parts"]
print(f"@@ модель собрана: {len(parts)} деталей")

# --- Силуэт модели: ортогональный рендер сбоку, всё белым на чёрном ---
sc = bpy.context.scene
sc.render.engine = "BLENDER_WORKBENCH"
sc.display.shading.light = "FLAT"
sc.display.shading.color_type = "SINGLE"
sc.display.shading.single_color = (1.0, 1.0, 1.0)
sc.world.color = (0.0, 0.0, 0.0) if sc.world else None
sc.render.film_transparent = False

import mathutils
pts = [ob.matrix_world @ mathutils.Vector(c) for ob in parts.values() for c in ob.bound_box]
lo = mathutils.Vector([min(p[k] for p in pts) for k in range(3)])
hi = mathutils.Vector([max(p[k] for p in pts) for k in range(3)])
cx, cz = (lo.x + hi.x) / 2, (lo.z + hi.z) / 2
span = max(hi.x - lo.x, hi.z - lo.z) * MARGIN

bpy.ops.object.camera_add(location=(cx, -900, cz), rotation=(np.pi / 2, 0, 0))
cam = bpy.context.active_object
cam.data.type = "ORTHO"
cam.data.ortho_scale = span
sc.camera = cam
sc.render.resolution_x = RES_X
sc.render.resolution_y = RES_X
sc.render.resolution_percentage = 100
OUT = os.path.join(ROOT, "Import", "Reference", "fit")
os.makedirs(OUT, exist_ok=True)
sc.render.filepath = os.path.join(OUT, "model_silhouette.png")
bpy.ops.render.render(write_still=True)

img = bpy.data.images.load(sc.render.filepath + ".png" if not sc.render.filepath.endswith(".png") else sc.render.filepath)
w, h = img.size
a = np.array(img.pixels[:], dtype=np.float32).reshape(h, w, 4)
model = np.flipud(a[:, :, 0] > 0.5)
bpy.data.images.remove(img)
mm_per_px = span / w
print(f"@@ силуэт модели: {model.sum()} px, масштаб {mm_per_px:.4f} мм/px")

# --- Контур с фотографии в ту же сетку ---
with open(PROFILE, encoding="utf-8") as f:
    prof = json.load(f)
if prof.get("slide_top_mm") is None:
    raise RuntimeError("в контуре нет датума верха затвора")
dz = 14.0 - prof["slide_top_mm"]          # тот же датум, что в модели: верх затвора на +14 мм


def to_px(x_mm, z_mm):
    return (x_mm - cx) / mm_per_px + w / 2.0, h / 2.0 - (z_mm - cz) / mm_per_px


photo = np.zeros_like(model)
top = [(x, z + dz) for x, z in prof["top"]]
bot = [(x, z + dz) for x, z in prof["bottom"]]
xs_mm = np.array([p[0] for p in top])
top_z = np.array([p[1] for p in top])
bot_z = np.array([p[1] for p in bot])
bot_x = np.array([p[0] for p in bot])
for px in range(w):
    x_mm = (px - w / 2.0) * mm_per_px + cx
    if x_mm < xs_mm.min() or x_mm > xs_mm.max():
        continue
    t = np.interp(x_mm, xs_mm, top_z)
    b = np.interp(x_mm, bot_x, bot_z)
    y0 = int(h / 2.0 - (t - cz) / mm_per_px)
    y1 = int(h / 2.0 - (b - cz) / mm_per_px)
    if y1 > y0:
        photo[max(y0, 0):min(y1, h), px] = True
print(f"@@ силуэт с фотографии: {photo.sum()} px")

# --- Мера расхождения ---
# Контур с фотографии - это ОБВОД: он заливается между верхней и нижней границей и про дырки
# внутри детали не знает. У оружия дырки настоящие - окно спусковой скобы прежде всего. Сравнивать
# надо обвод с обводом, иначе правильно прорезанная скоба считается недостачей материала и мера
# наказывает за верную работу. Поэтому силуэт модели тоже приводится к обводу; сырое число
# печатается рядом, чтобы разница была видна.
raw_model = model.copy()
model = np.zeros_like(raw_model)
for px in range(w):
    col = np.flatnonzero(raw_model[:, px])
    if col.size:
        model[col[0]:col[-1] + 1, px] = True
holes = int(model.sum() - raw_model.sum())
print(f"@@ обвод модели: {model.sum()} px, из них проёмов внутри детали {holes} px "
      f"({100.0 * holes / max(model.sum(), 1):.1f}%)")

inter = (model & photo).sum()
union = (model | photo).sum()
iou = inter / max(union, 1)
only_model = (model & ~photo).sum()
only_photo = (photo & ~model).sum()
print(f"@@ IoU {iou:.3f}  |  лишнего у модели {100.0 * only_model / max(photo.sum(), 1):.1f}%, "
      f"недостаёт {100.0 * only_photo / max(photo.sum(), 1):.1f}%")

# Где именно расходится: по участкам вдоль ствола.
print("@@ расхождение по участкам (от дула к хвосту):")
for i in range(8):
    x0, x1 = int(w * i / 8), int(w * (i + 1) / 8)
    m, ph = model[:, x0:x1], photo[:, x0:x1]
    if ph.sum() + m.sum() == 0:
        continue
    seg_iou = (m & ph).sum() / max((m | ph).sum(), 1)
    dz_mm = (m.sum(axis=0).mean() - ph.sum(axis=0).mean()) * mm_per_px
    x_from = ((x0 - w / 2.0) * mm_per_px + cx)
    print(f"@@   {x_from:7.0f} мм: IoU {seg_iou:.2f}, модель {'толще' if dz_mm > 0 else 'тоньше'} на {abs(dz_mm):.1f} мм")

rgb = np.zeros((h, w, 3), dtype=np.uint8)
rgb[photo & model] = (70, 90, 70)          # совпало
rgb[photo & ~model] = (40, 120, 230)       # есть на фото, нет в модели
rgb[model & ~photo] = (230, 90, 40)        # есть в модели, нет на фото
write_png(os.path.join(OUT, "fit_overlay.png"), rgb)
print(f"@@ записано {OUT}: синее - недостаёт, оранжевое - лишнее")
