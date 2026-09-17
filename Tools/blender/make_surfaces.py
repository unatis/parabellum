"""
Тайлящиеся карты поверхностей бункера: бетон стен, бетон пола, крашеный металл.
Считаются числами (решётчатый шум с заворотом краёв), поэтому швов на стыке тайла нет
и результат воспроизводится скриптом, а не хранится как бинарь.

Каждая поверхность даёт три карты:
  <name>_BC.png   - базовый цвет (sRGB)
  <name>_R.png    - шероховатость (линейная)
  <name>_AO.png   - затенение полостей (линейная)
  <name>_N.png    - нормаль из карты высот (линейная)

Это заглушка до фотограмметрии (Megascans): мастер-материал один и тот же, карты подменяются.
Запуск: Tools/blender/make_surfaces.bat -> Import/Surfaces/*.png
"""
import os, sys, struct, zlib
import numpy as np

OUT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "Import", "Surfaces"))
os.makedirs(OUT, exist_ok=True)
RES = 1024
TILE_M = 2.0     # сколько метров занимает один тайл - от этого зависит масштаб деталей


def write_png(path, arr):
    """RGB uint8 (h, w, 3) -> PNG без сторонних библиотек."""
    h, w, _ = arr.shape
    raw = b"".join(b"\x00" + arr[y].tobytes() for y in range(h))

    def chunk(tag, data):
        body = tag + data
        return struct.pack(">I", len(data)) + body + struct.pack(">I", zlib.crc32(body) & 0xFFFFFFFF)

    png = (b"\x89PNG\r\n\x1a\n"
           + chunk(b"IHDR", struct.pack(">2I5B", w, h, 8, 2, 0, 0, 0))
           + chunk(b"IDAT", zlib.compress(raw, 9))
           + chunk(b"IEND", b""))
    with open(path, "wb") as f:
        f.write(png)


def lattice(freq, rng):
    """Решётчатый шум с заворотом: значения на краях тайла совпадают, поэтому тайл бесшовный."""
    g = rng.random((freq, freq))
    t = np.linspace(0, freq, RES, endpoint=False)
    i = np.floor(t).astype(int) % freq
    j = (i + 1) % freq
    f = t - np.floor(t)
    f = f * f * (3 - 2 * f)                      # сглаживание, иначе решётка видна квадратами
    fx, fy = f[:, None], f[None, :]
    g00, g10 = g[np.ix_(i, i)], g[np.ix_(j, i)]
    g01, g11 = g[np.ix_(i, j)], g[np.ix_(j, j)]
    return (g00 * (1 - fx) * (1 - fy) + g10 * fx * (1 - fy)
            + g01 * (1 - fx) * fy + g11 * fx * fy)


def fbm(freq, octaves, rng, gain=0.5):
    out, amp, norm = np.zeros((RES, RES)), 1.0, 0.0
    for k in range(octaves):
        out += amp * lattice(freq * 2 ** k, rng)
        norm += amp
        amp *= gain
    return out / norm


def normal_from_height(h, strength):
    """Нормаль из высот: центральные разности с заворотом, чтобы шва на стыке не было."""
    dx = (np.roll(h, -1, 0) - np.roll(h, 1, 0)) * strength
    dy = (np.roll(h, -1, 1) - np.roll(h, 1, 1)) * strength
    n = np.stack([-dx, -dy, np.ones_like(h)], axis=-1)
    n /= np.linalg.norm(n, axis=-1, keepdims=True)
    return n * 0.5 + 0.5


def to_srgb(lin):
    lin = np.clip(lin, 0.0, 1.0)
    return np.where(lin <= 0.0031308, lin * 12.92, 1.055 * lin ** (1 / 2.4) - 0.055)


def save(name, albedo_lin, rough, ao, height, normal_strength):
    bc = (to_srgb(albedo_lin) * 255 + 0.5).astype(np.uint8)
    r8 = (np.clip(rough, 0, 1) * 255 + 0.5).astype(np.uint8)
    a8 = (np.clip(ao, 0, 1) * 255 + 0.5).astype(np.uint8)
    nrm = (normal_from_height(height, normal_strength) * 255 + 0.5).astype(np.uint8)
    write_png(os.path.join(OUT, f"{name}_BC.png"), bc)
    write_png(os.path.join(OUT, f"{name}_R.png"), rgb(r8.astype(float) / 255.0 * 255).astype(np.uint8))
    write_png(os.path.join(OUT, f"{name}_AO.png"), rgb(a8.astype(float) / 255.0 * 255).astype(np.uint8))
    write_png(os.path.join(OUT, f"{name}_N.png"), nrm)
    print(f"@@ {name:16s} albedo {albedo_lin.mean():.3f}  rough {rough.mean():.2f}  {RES}x{RES}, тайл {TILE_M} м")


def rgb(v):
    """Серое значение -> три канала."""
    return np.repeat(v[:, :, None], 3, axis=2)


# --- Бетон стен: литой, со следами опалубки -------------------------------------------------------
rng = np.random.default_rng(1)
mott = fbm(4, 6, rng)                       # общая пятнистость
stain = fbm(2, 3, rng)                      # крупные подтёки
pores = (lattice(RES // 4, rng) > 0.90).astype(float)   # поры от пузырьков воздуха
pores = np.maximum(pores, np.roll(pores, 1, 0) * 0.6)
# Следы опалубки: доска 1 м, стык - тонкая тёмная линия с небольшим уступом.
v = np.linspace(0, TILE_M, RES, endpoint=False)[None, :]
seam = np.exp(-((v % 1.0) / 0.006) ** 2) + np.exp(-(((1.0 - v % 1.0)) / 0.006) ** 2)
seam = np.repeat(seam, RES, axis=0)

h = mott * 0.6 + stain * 0.4 - pores * 0.5 - seam * 0.35
alb = 0.205 * (0.80 + 0.40 * mott) * (0.88 + 0.24 * stain) - pores * 0.06 - seam * 0.05
alb = np.clip(alb, 0.02, 1.0)
tint = np.stack([alb * 1.00, alb * 0.985, alb * 0.955], axis=-1)   # бетон чуть тёплый
save("Concrete_Wall", tint, 0.90 + 0.06 * mott + pores * 0.06, 1.0 - pores * 0.45 - seam * 0.25, h, 3.0)

# --- Бетон пола: затёртый, с заполнителем и следами износа ---------------------------------------
rng = np.random.default_rng(2)
mott = fbm(3, 5, rng)
wear = fbm(1, 3, rng)                        # светлые вытертые дорожки
agg = (lattice(RES // 3, rng) > 0.93).astype(float)     # выступивший заполнитель
scratch = (fbm(2, 2, rng) * lattice(RES // 8, rng) > 0.55).astype(float) * 0.5

h = mott * 0.3 + agg * 0.25 - scratch * 0.2
alb = 0.150 * (0.85 + 0.30 * mott) * (0.85 + 0.35 * wear) + agg * 0.05
alb = np.clip(alb, 0.02, 1.0)
tint = np.stack([alb * 1.00, alb * 0.99, alb * 0.97], axis=-1)
save("Concrete_Floor", tint, 0.80 + 0.12 * mott - wear * 0.14, 1.0 - agg * 0.15, h, 2.0)

# --- Крашеный металл: стойки, двери, корпуса светильников ----------------------------------------
rng = np.random.default_rng(3)
peel = fbm(8, 4, rng)                        # шагрень краски
chips = (lattice(RES // 6, rng) > 0.965).astype(float)   # сколы до металла
h = peel * 0.2 - chips * 0.6
alb = 0.055 * (0.9 + 0.2 * peel)
col = np.stack([alb * 0.95, alb * 1.00, alb * 0.98], axis=-1)     # тёмно-зелёная эмаль
col = col * (1 - chips[:, :, None]) + chips[:, :, None] * 0.22    # скол - светлый металл
save("Steel_Painted", col, 0.42 + 0.20 * peel - chips * 0.2, 1.0 - chips * 0.3, h, 2.5)

print(f"@@ written to {OUT}")
