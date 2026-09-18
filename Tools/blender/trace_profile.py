"""
Обмер контура оружия по боковой фотографии.

Зачем. Опубликованные ТТХ дают пять-десять чисел - длину, высоту, ствол. Они задают габарит,
но не форму: обвод затвора, скруглённую спусковую скобу, изгиб рукояти в них не найти. Снимок
сбоку даёт вместо пяти чисел полсотни точек контура, и это ровно та разница, которой не хватает
моделям, собранным по одной таблице.

Что делает:
  1. отделяет оружие от фона по яркости (метод Оцу - порог берётся из самой картинки);
  2. выпрямляет наклон: прямая линия верха затвора - самый длинный ровный участок контура,
     по ней и выравнивается горизонт;
  3. берёт масштаб из одного известного размера - длины оружия;
  4. снимает верхний и нижний контур по каждому столбцу и прореживает их (Дуглас-Пекер),
     чтобы на выходе была полусотня осмысленных точек, а не две тысячи пикселей.

Выход - Import/Reference/<name>/profile.json (точки в миллиметрах, готовые в profile_extrude)
и overlay.png, где обвод нарисован поверх снимка: без этой картинки результату верить нельзя.

Запуск: blender -b -P trace_profile.py -- <фото> <длина_мм> [имя]
"""
import os, sys, json, struct, zlib
import numpy as np
import bpy

ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
ARGS = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
PHOTO = ARGS[0] if ARGS else ""
KNOWN_LENGTH_MM = float(ARGS[1]) if len(ARGS) > 1 else 210.0
NAME = ARGS[2] if len(ARGS) > 2 else os.path.splitext(os.path.basename(PHOTO))[0]
# Рамка поиска в долях кадра: x0 y0 x1 y1. Нужна, когда рядом лежит что-то ещё или фон пёстрый.
BOX = [float(v) for v in ARGS[3:7]] if len(ARGS) >= 7 else None
OUT = os.path.join(ROOT, "Import", "Reference", NAME)
os.makedirs(OUT, exist_ok=True)

SIMPLIFY_MM = 0.4      # допуск прореживания: точки ближе этого к прямой отбрасываются
FLAT_SLOPE = 0.06      # что считать ровным участком при поиске линии затвора (пикселей на пиксель)


def load_rgb(path):
    img = bpy.data.images.load(path, check_existing=False)
    img.colorspace_settings.name = "Non-Color"
    w, h = img.size
    a = np.array(img.pixels[:], dtype=np.float32).reshape(h, w, 4)[:, :, :3]
    bpy.data.images.remove(img)
    return np.flipud(a), w, h


def otsu(gray):
    """Порог из самой картинки: тот, что сильнее всего разводит два класса по яркости."""
    hist, edges = np.histogram(gray, bins=256, range=(0.0, 1.0))
    total = hist.sum()
    w0 = np.cumsum(hist)
    w1 = total - w0
    mids = (edges[:-1] + edges[1:]) / 2
    s0 = np.cumsum(hist * mids)
    s1 = s0[-1] - s0
    ok = (w0 > 0) & (w1 > 0)
    var = np.zeros(256)
    var[ok] = w0[ok] * w1[ok] * ((s0[ok] / w0[ok]) - (s1[ok] / w1[ok])) ** 2
    return mids[int(np.argmax(var))]


def foreground(rgb):
    """
    Отделяем по ЦВЕТУ от фона, а не по яркости. У оружия на одном снимке уживаются светлая
    сталь и тёмные щёчки: любой порог по яркости разрежет его пополам, а половину фона
    прихватит. Цвет фона берём с рамки кадра - там всегда фон, - и объектом считаем всё,
    что от этого цвета далеко.
    """
    border = np.concatenate([rgb[0], rgb[-1], rgb[:, 0], rgb[:, -1]])
    bg = np.median(border, axis=0)
    dist = np.linalg.norm(rgb - bg, axis=2)
    t = otsu(dist / max(dist.max(), 1e-6)) * max(dist.max(), 1e-6)
    print(f"@@ фон {np.round(bg, 3)}, порог удалённости {t:.3f}")
    return dist > t


def clean(mask, passes=2):
    """Чистка: одиночные пиксели убираем большинством соседей, дыры внутри объекта заливаем."""
    m = mask.copy()
    for _ in range(passes):
        acc = np.zeros(m.shape, dtype=np.int16)
        for dy in (-1, 0, 1):
            for dx in (-1, 0, 1):
                acc += np.roll(np.roll(m, dy, axis=0), dx, axis=1).astype(np.int16)
        m = acc >= 5
    # Дыры: то, до чего не дотянулась заливка фона от края кадра.
    h, w = m.shape
    bg = ~m
    seen = np.zeros_like(bg)
    stack = [(y, x) for y in (0, h - 1) for x in range(w) if bg[y, x]]
    stack += [(y, x) for x in (0, w - 1) for y in range(h) if bg[y, x]]
    for y, x in stack:
        seen[y, x] = True
    while stack:
        y, x = stack.pop()
        for dy, dx in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            ny, nx = y + dy, x + dx
            if 0 <= ny < h and 0 <= nx < w and bg[ny, nx] and not seen[ny, nx]:
                seen[ny, nx] = True
                stack.append((ny, nx))
    return m | (bg & ~seen)


def largest_component(mask):
    """Оставляем один самый крупный объект: рядом с оружием на снимке обычно лежат
    магазин и патроны, и обмер по всему тёмному дал бы контур по ним."""
    h, w = mask.shape
    label = np.zeros((h, w), dtype=np.int32)
    best, best_size, cur = 0, 0, 0
    ys, xs = np.nonzero(mask)
    for sy, sx in zip(ys, xs):
        if label[sy, sx]:
            continue
        cur += 1
        size = 0
        stack = [(sy, sx)]
        label[sy, sx] = cur
        while stack:
            y, x = stack.pop()
            size += 1
            for dy, dx in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                ny, nx = y + dy, x + dx
                if 0 <= ny < h and 0 <= nx < w and mask[ny, nx] and not label[ny, nx]:
                    label[ny, nx] = cur
                    stack.append((ny, nx))
        if size > best_size:
            best, best_size = cur, size
    print(f"@@ объектов в кадре: {cur}, самый крупный занимает {100.0 * best_size / mask.sum():.0f}% переднего плана")
    return label == best


def columns(mask):
    """Верх и низ объекта в каждом столбце; там, где объекта нет, - NaN."""
    h, w = mask.shape
    top = np.full(w, np.nan)
    bot = np.full(w, np.nan)
    for x in range(w):
        ys = np.flatnonzero(mask[:, x])
        if ys.size >= 3:                       # одиночные пиксели - шум, не контур
            top[x] = ys[0]
            bot[x] = ys[-1]
    return top, bot


def longest_flat_run(top):
    """Самый длинный ровный участок верхнего контура: у пистолета это верх затвора."""
    d = np.abs(np.diff(top))
    flat = np.isfinite(d) & (d < FLAT_SLOPE)
    best_len = best_start = cur_start = 0
    cur = 0
    for i, f in enumerate(flat):
        if f:
            if cur == 0:
                cur_start = i
            cur += 1
            if cur > best_len:
                best_len, best_start = cur, cur_start
        else:
            cur = 0
    return best_start, best_start + best_len


def rotate_points(xs, ys, ang, cx, cy):
    c, s = np.cos(ang), np.sin(ang)
    x, y = xs - cx, ys - cy
    return x * c - y * s + cx, x * s + y * c + cy


def simplify(pts, tol):
    """Дуглас-Пекер: оставляем точки, без которых ломаная уходит дальше допуска."""
    if len(pts) < 3:
        return pts
    a, b = np.array(pts[0]), np.array(pts[-1])
    ab = b - a
    n = np.linalg.norm(ab)
    if n < 1e-9:
        d = np.array([np.linalg.norm(np.array(p) - a) for p in pts])
    else:
        d = np.abs(np.cross(np.array(pts) - a, ab)) / n
    i = int(np.argmax(d))
    if d[i] <= tol:
        return [pts[0], pts[-1]]
    return simplify(pts[:i + 1], tol)[:-1] + simplify(pts[i:], tol)


def write_png(path, arr):
    h, w, _ = arr.shape
    raw = b"".join(b"\x00" + arr[y].tobytes() for y in range(h))

    def chunk(tag, data):
        body = tag + data
        return struct.pack(">I", len(data)) + body + struct.pack(">I", zlib.crc32(body) & 0xFFFFFFFF)

    with open(path, "wb") as f:
        f.write(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">2I5B", w, h, 8, 2, 0, 0, 0))
                + chunk(b"IDAT", zlib.compress(raw, 6)) + chunk(b"IEND", b""))


if not PHOTO or not os.path.exists(PHOTO):
    raise RuntimeError(f"нет файла {PHOTO}")

rgb_in, W, H = load_rgb(PHOTO)
gray = rgb_in.mean(axis=2)
if BOX:
    x0b, y0b, x1b, y1b = int(BOX[0] * W), int(BOX[1] * H), int(BOX[2] * W), int(BOX[3] * H)
    keep = np.zeros((H, W), dtype=bool)
    keep[y0b:y1b, x0b:x1b] = True
    print(f"@@ рамка поиска {x0b},{y0b}..{x1b},{y1b}")
else:
    keep = np.ones((H, W), dtype=bool)

mask = foreground(rgb_in) & keep
print(f"@@ снимок {W}x{H}, передний план {100.0 * mask.mean():.1f}% кадра")
raw_fg = mask.sum()
mask = clean(mask)
mask = largest_component(mask)

# Признак негодного снимка: объект рассыпался на куски или занимает подозрительную долю кадра.
share = mask.sum() / max(raw_fg, 1)
if share < 0.8:
    print(f"@@ ВНИМАНИЕ: крупнейший объект - только {100 * share:.0f}% переднего плана. Фон недостаточно")
    print("@@ контрастен, объект распался на куски. Смотрите overlay.png: числам верить нельзя.")
    print("@@ Помогает однотонный контрастный фон или рамка поиска четырьмя долями кадра.")

top, bot = columns(mask)
valid = np.flatnonzero(np.isfinite(top))
if valid.size < 20:
    raise RuntimeError("объект не выделился - нужен контрастный однотонный фон")

# --- Выпрямление наклона по линии затвора ---
a, b = longest_flat_run(top)
if b - a > 30:
    xs = np.arange(a, b)
    k, _ = np.polyfit(xs, top[a:b], 1)
    angle = -np.arctan(k)
    print(f"@@ линия затвора: {b - a} px, наклон {np.degrees(np.arctan(k)):+.2f} град - выпрямляем")
else:
    angle = 0.0
    print("@@ ровного участка не нашлось, снимок принят как есть")

cx, cy = W / 2.0, H / 2.0
pts_top = np.array([rotate_points(x, top[x], angle, cx, cy) for x in valid])
pts_bot = np.array([rotate_points(x, bot[x], angle, cx, cy) for x in valid])

# --- Масштаб по известной длине ---
x_all = np.concatenate([pts_top[:, 0], pts_bot[:, 0]])
span_px = x_all.max() - x_all.min()
mm_per_px = KNOWN_LENGTH_MM / span_px
x0 = x_all.max()                              # ноль по дульному срезу: как в скриптах моделей
y_all = np.concatenate([pts_top[:, 1], pts_bot[:, 1]])
height_mm = (y_all.max() - y_all.min()) * mm_per_px
print(f"@@ масштаб {mm_per_px:.4f} мм/px ({span_px:.0f} px = {KNOWN_LENGTH_MM} мм)")
print(f"@@ высота по контуру {height_mm:.1f} мм - это проверка: сверьте с паспортной")

# Ось канала неизвестна, поэтому за ноль по высоте берём верх объекта; модель сдвинет сама.
y_ref = y_all.min()


def to_mm(p):
    return [round(float((p[0] - x0) * mm_per_px), 2), round(float(-(p[1] - y_ref) * mm_per_px), 2)]


prof_top = simplify([to_mm(p) for p in pts_top], SIMPLIFY_MM)
prof_bot = simplify([to_mm(p) for p in pts_bot], SIMPLIFY_MM)
print(f"@@ контур: верх {len(pts_top)} -> {len(prof_top)} точек, низ {len(pts_bot)} -> {len(prof_bot)}")

with open(os.path.join(OUT, "profile.json"), "w", encoding="utf-8") as f:
    json.dump({"source": os.path.basename(PHOTO), "known_length_mm": KNOWN_LENGTH_MM,
               "mm_per_px": round(mm_per_px, 5), "rotation_deg": round(float(np.degrees(angle)), 3),
               "measured_height_mm": round(float(height_mm), 2),
               "note": "x от дульного среза назад отрицательный, y вниз от верхней точки отрицательный",
               "top": prof_top, "bottom": prof_bot}, f, indent=1, ensure_ascii=False)

# --- Картинка для проверки: без неё цифрам верить нельзя ---
rgb = np.repeat((np.clip(gray, 0, 1) * 255).astype(np.uint8)[:, :, None], 3, axis=2)
rgb[mask] = (rgb[mask] * 0.55 + np.array([0, 60, 0])).astype(np.uint8)
for x in valid:
    for y, col in ((int(top[x]), (255, 90, 40)), (int(bot[x]), (60, 160, 255))):
        rgb[max(y - 1, 0):y + 2, x] = col
write_png(os.path.join(OUT, "overlay.png"), rgb)
print(f"@@ записано {OUT}")
