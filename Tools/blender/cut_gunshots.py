"""
Нарезка одиночных выстрелов из длинной записи (Freesound и т.п.). Запуск через Blender (в нём есть numpy):
  blender -b --python Tools/blender/cut_gunshots.py -- <in.wav> <out_dir> <prefix> [max_shots]
Детектирует фронты по огибающей, режет с 30 мс до фронта, длина 1.2 с, фейд 250 мс, нормализует к -1 дБFS, 16 бит.
"""
import sys, os, wave, numpy as np

argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
src, out_dir, prefix = argv[0], argv[1], argv[2]
max_shots = int(argv[3]) if len(argv) > 3 else 4
os.makedirs(out_dir, exist_ok=True)

w = wave.open(src, "rb"); n, sr, ch, sw = w.getnframes(), w.getframerate(), w.getnchannels(), w.getsampwidth()
raw = w.readframes(n); w.close()
assert sw == 2, "16-bit only"
a = np.frombuffer(raw, dtype=np.int16).reshape(-1, ch).astype(np.float32) / 32768.0
mono = np.abs(a).max(axis=1)
win = int(sr * 0.005)
env = mono[: len(mono) // win * win].reshape(-1, win).max(axis=1)
onsets, last = [], -1e9
for i, v in enumerate(env):
    t = i * win / sr
    if v > 0.3 and t - last > 0.3:
        onsets.append(t); last = t

# Предпочитаем неклипованные выстрелы (пик < 0.999), затем остальные по порядку.
def peak_of(t):
    s0 = int(t * sr); return float(mono[s0:s0 + int(0.2 * sr)].max())
ranked = sorted(onsets, key=lambda t: (peak_of(t) >= 0.999, onsets.index(t)))
chosen = ranked[:max_shots]
pre, length, fade = int(0.03 * sr), int(1.2 * sr), int(0.25 * sr)
for k, t in enumerate(sorted(chosen)):
    s0 = max(int(t * sr) - pre, 0)
    seg = a[s0:s0 + length].copy()
    ramp = np.linspace(1.0, 0.0, fade, dtype=np.float32)
    seg[-fade:] *= ramp[:, None]
    seg[:pre] *= np.linspace(0.0, 1.0, pre, dtype=np.float32)[:, None]
    seg *= (10 ** (-1 / 20)) / max(np.abs(seg).max(), 1e-6)
    out = os.path.join(out_dir, f"{prefix}_{k+1:02d}.wav")
    ww = wave.open(out, "wb"); ww.setnchannels(ch); ww.setsampwidth(2); ww.setframerate(sr)
    ww.writeframes((seg * 32767).astype(np.int16).tobytes()); ww.close()
    print(f"@@ {out}: from {t:.3f} s, peak {peak_of(t):.3f}{' (clipped)' if peak_of(t) >= 0.999 else ''}")
print(f"@@ done {len(chosen)} of {len(onsets)} shots")
