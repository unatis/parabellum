"""
Текстура кожи рук из фотографий пользователя (Import/Hands/*.jpg).

Что здесь можно и чего нельзя. Наложить фотографию на развёртку нельзя: развёртка чужая, а
снимков всего два, и с них не восстановить ни ладонь, ни бока пальцев. Зато с них берётся то,
из-за чего рука сейчас выглядит пластиковой: собственный тон кожи и микрорельеф - поры, волоски,
мелкие складки. Они кладутся поверх существующей текстуры, которая уже согласована с развёрткой.

Освещение на снимке тёплое, поэтому цвет нормируется по светлому столу в кадре как по белому -
иначе жёлтый свет комнаты запечётся в альбедо и будет драться с освещением бункера.

Запуск: Tools/blender/make_hand_texture.bat -> Import/Hands/Hand_D.png
"""
import os, sys, struct, zlib
import numpy as np
import bpy

ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
SRC = os.path.join(ROOT, "Import", "Hands")
BASE = os.path.join(SRC, "current", "Hand_D.png")
OUT = os.path.join(SRC, "Hand_D.png")

# Снимок с прицеливанием: тыльная сторона кисти крупно и ровно освещена.
PHOTO = os.path.join(SRC, "dda71343-630c-438f-bd85-bacaa8933de9.jpg")
# Области в долях от размера кадра: кожа и светлый стол как ориентир белого.
# Тыльная сторона кисти: ровно освещена, видны поры и мелкие складки. Рамка подобрана
# по пробным вырезкам - более широкая захватывала край кисти, и он тиражировался по всей
# текстуре тёмными галочками.
SKIN_BOX = (0.56, 0.63, 0.69, 0.76)
WHITE_BOX = (0.10, 0.72, 0.35, 0.92)

DETAIL_STRENGTH = 0.55     # сколько микрорельефа переносить
TONE_STRENGTH = 0.85       # насколько подтягивать тон базовой текстуры к фотографии


def load_rgb(path):
    """Читает изображение через Blender и отдаёт sRGB-значения 0..1 как (h, w, 3)."""
    img = bpy.data.images.load(path, check_existing=False)
    img.colorspace_settings.name = "Non-Color"    # нужны сырые значения, без пересчёта в линейное
    w, h = img.size
    a = np.array(img.pixels[:], dtype=np.float32).reshape(h, w, 4)[:, :, :3]
    bpy.data.images.remove(img)
    return np.flipud(a)                            # Blender отдаёт снизу вверх


def write_png(path, arr):
    h, w, _ = arr.shape
    raw = b"".join(b"\x00" + arr[y].tobytes() for y in range(h))

    def chunk(tag, data):
        body = tag + data
        return struct.pack(">I", len(data)) + body + struct.pack(">I", zlib.crc32(body) & 0xFFFFFFFF)

    png = (b"\x89PNG\r\n\x1a\n"
           + chunk(b"IHDR", struct.pack(">2I5B", w, h, 8, 2, 0, 0, 0))
           + chunk(b"IDAT", zlib.compress(raw, 6))
           + chunk(b"IEND", b""))
    with open(path, "wb") as f:
        f.write(png)


def crop(img, box):
    h, w, _ = img.shape
    x0, y0, x1, y1 = box
    return img[int(y0 * h):int(y1 * h), int(x0 * w):int(x1 * w)]


def box_blur(a, k):
    """Среднее по блоку k x k через уменьшение и обратное растягивание - дёшево и достаточно."""
    h, w = a.shape[:2]
    hh, ww = h // k, w // k
    small = a[:hh * k, :ww * k].reshape(hh, k, ww, k, -1).mean(axis=(1, 3))
    big = np.repeat(np.repeat(small, k, axis=0), k, axis=1)
    # Хвост от целочисленного деления добиваем краем, иначе размер не сойдётся с исходником.
    return np.pad(big, ((0, h - big.shape[0]), (0, w - big.shape[1]), (0, 0)), mode="edge")


def tile_mirror(patch, h, w):
    """Замощение с зеркалом: у кожи нет направления, шва так не видно."""
    ph, pw = patch.shape[:2]
    ny, nx = -(-h // ph) + 1, -(-w // pw) + 1
    rows = []
    for j in range(ny):
        row = [patch if (i + j) % 2 == 0 else patch[:, ::-1] for i in range(nx)]
        row = [r if j % 2 == 0 else r[::-1] for r in row]
        rows.append(np.concatenate(row, axis=1))
    return np.concatenate(rows, axis=0)[:h, :w]


photo = load_rgb(PHOTO)
skin = crop(photo, SKIN_BOX)
white = crop(photo, WHITE_BOX)

# Нормировка по светлому столу: убираем тёплый свет комнаты из цвета кожи.
illum = np.clip(white.reshape(-1, 3).mean(axis=0), 1e-3, None)
illum = illum / illum.max()
skin_lin = skin / illum
skin_tone = np.clip(skin_lin.reshape(-1, 3).mean(axis=0), 0.0, 1.0)
print(f"@@ свет в кадре {illum.round(3)}, тон кожи после нормировки {skin_tone.round(3)}")

# Микрорельеф: высокие частоты кожи, серые (цвет берём из тона, не из деталей).
gray = skin_lin.mean(axis=2)
detail = gray - box_blur(gray[:, :, None], 24)[:, :, 0]
detail -= detail.mean()
print(f"@@ микрорельеф: размах {detail.min():+.3f}..{detail.max():+.3f}, патч {detail.shape[1]}x{detail.shape[0]}")

base = load_rgb(BASE)
h, w, _ = base.shape
det = tile_mirror(detail[:, :, None], h, w)[:, :, 0]

# Тон: подтягиваем средний цвет базовой текстуры к тону с фотографии, сохраняя её светлотные
# переходы - они несут форму пальцев и согласованы с развёрткой.
base_mean = np.clip(base.reshape(-1, 3).mean(axis=0), 1e-3, None)
scale = np.clip(skin_tone / base_mean, 0.5, 1.6)
toned = base * (1.0 - TONE_STRENGTH + TONE_STRENGTH * scale)
out = np.clip(toned + det[:, :, None] * DETAIL_STRENGTH, 0.0, 1.0)

print(f"@@ база {base_mean.round(3)} -> {out.reshape(-1, 3).mean(axis=0).round(3)}, {w}x{h}")
write_png(OUT, (out * 255 + 0.5).astype(np.uint8))
print(f"@@ записано {OUT}")

# Карта нормалей из того же микрорельефа: без неё кожа освещается как гладкий пластик.
NORMAL_STRENGTH = 14.0
dx = (np.roll(det, -1, 0) - np.roll(det, 1, 0)) * NORMAL_STRENGTH
dy = (np.roll(det, -1, 1) - np.roll(det, 1, 1)) * NORMAL_STRENGTH
n = np.stack([-dx, -dy, np.ones_like(det)], axis=-1)
n /= np.linalg.norm(n, axis=-1, keepdims=True)
write_png(OUT.replace("Hand_D.png", "Hand_N.png"), ((n * 0.5 + 0.5) * 255 + 0.5).astype(np.uint8))
print(f"@@ записана карта нормалей, наклон до {np.degrees(np.arccos(n[:, :, 2].min())):.0f} град")

# Шероховатость: кожа не зеркало. Поры чуть матовее гладких участков.
rough = np.clip(0.52 - det * 0.6, 0.30, 0.75)
write_png(OUT.replace("Hand_D.png", "Hand_R.png"),
          (np.repeat(rough[:, :, None], 3, axis=2) * 255 + 0.5).astype(np.uint8))
print(f"@@ записана шероховатость, среднее {rough.mean():.2f}")
