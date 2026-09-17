"""
Раскладка архивов Megascans (Fab) под имена, которые ждёт import_surfaces.py.

Megascans кладёт в архив свой набор карт с длинными именами; здесь из него берутся только нужные
роли и раскладываются как <Surface>_<ROLE>.jpg. Ничего не пережимается - файлы копируются как есть.

Архивы класть в Import/Surfaces/megascans/*.zip. Соответствие архива и поверхности - в MAP ниже.
Запуск: python Tools/megascans_prepare.py
"""
import os, sys, zipfile, shutil

sys.stdout.reconfigure(encoding="utf-8")
ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
SRC = os.path.join(ROOT, "Import", "Surfaces", "megascans")
DST = os.path.join(ROOT, "Import", "Surfaces")

# Какой архив какой поверхностью становится. Ключ - кусок имени файла архива.
MAP = {
    "industrial-concrete-floor": "Concrete_Floor",
    "smooth-precast-concrete": "Concrete_Wall",
    "stained-concrete-ceiling": "Concrete_Ceiling",
}

# Роль -> имена карт Megascans по убыванию предпочтения. Cavity берётся вместо AO там,
# где AO в наборе нет: роль у них близкая, и это лучше, чем ровная единица.
ROLES = {
    "BC": ["BaseColor"],
    "R": ["Roughness"],
    "AO": ["AO", "Cavity"],
    "N": ["Normal"],
}


def main():
    if not os.path.isdir(SRC):
        print(f"нет папки {SRC}")
        return
    total = 0
    for fn in sorted(os.listdir(SRC)):
        if not fn.lower().endswith(".zip"):
            continue
        surface = next((v for k, v in MAP.items() if k in fn.lower()), None)
        if not surface:
            print(f"пропуск {fn}: не задано, какой поверхностью он становится")
            continue
        z = zipfile.ZipFile(os.path.join(SRC, fn))
        names = z.namelist()
        got = []
        for role, wanted in ROLES.items():
            pick = None
            for w in wanted:
                pick = next((n for n in names if n.endswith(f"_{w}.jpg") or n.endswith(f"_{w}.png")), None)
                if pick:
                    break
            if not pick:
                print(f"  {surface}_{role}: в архиве нет ни одной из карт {wanted}")
                continue
            out = os.path.join(DST, f"{surface}_{role}{os.path.splitext(pick)[1]}")
            with z.open(pick) as f, open(out, "wb") as o:
                shutil.copyfileobj(f, o)
            got.append(f"{role}<-{os.path.basename(pick).split('_')[-1]}")
            total += 1
        print(f"{surface:18s} {fn}: {', '.join(got)}")
    print(f"разложено карт: {total}")


main()
