"""
Перенос ассетов из проекта Lyra в Parabellum с зависимостями (аналог Migrate, воспроизводимый).

Запускается В КОНТЕКСТЕ LYRA (см. migrate_from_lyra.bat): собирает рекурсивные зависимости корневых
ассетов через Asset Registry, оставляет только /Game/, и копирует .uasset/.ubulk файлы в
Parabellum/Content с сохранением относительных путей - ссылки остаются валидными.
Lyra-специфичные классы (GA_, GE_, ABP_, BP_) отбрасываются: нам нужны меши, материалы, текстуры, анимации, звуки.
"""
import os, shutil
import unreal

ROOTS = [
    "/Game/Weapons/Pistol/Mesh/SK_Pistol",
    "/Game/Weapons/Pistol/Mesh/SM_Pistol",
    "/Game/Weapons/Pistol/Animations/Weap_Pistol_Fire",
    "/Game/Weapons/Pistol/Animations/Weap_Pistol_Reload",
    "/Game/Audio/Sounds/Weapons/Pistol/Weapons_Pistol_Punch-Close_01",
    "/Game/Audio/Sounds/Weapons/Pistol/Weapons_Pistol_Punch-Close_02",
    "/Game/Audio/Sounds/Weapons/Pistol/Weapons_Pistol_Punch-Close_03",
    "/Game/Audio/Sounds/Weapons/Pistol/Weapons_Pistol_Mech_01",
    "/Game/Audio/Sounds/Weapons/Pistol/Weapons_Pistol_DryFire_01",
    "/Game/Audio/Sounds/Weapons/Pistol/Weapons_Pistol_ClipIn_01",
    "/Game/Audio/Sounds/Weapons/Pistol/Weapons_Pistol_ClipOut_01",
    "/Game/Audio/Sounds/Weapons/Pistol/Weapons_Pistol_Slide_01",
    # попадания / хит-маркер (полировка Э4.5)
    "/Game/Audio/Sounds/Impacts/Lyra_ImpactPlaster_01",
    "/Game/Audio/Sounds/Impacts/Lyra_ImpactPlaster_02",
    "/Game/Audio/Sounds/Impacts/Lyra_Plyr_BulletImpact_01",
    "/Game/Audio/Sounds/Impacts/Lyra_ImpactHeadshot_01",
    "/Game/Audio/Sounds/Impacts/Lyra_EnemyKilled_01",
    "/Game/Audio/Sounds/WhizBys/Lyra_BulletIn_Close_01",
]
SKIP_PREFIXES = ("GA_", "GE_", "ABP_", "BP_", "B_", "AM_", "DA_", "IA_", "IMC_", "W_", "MSS_")
SRC_CONTENT = os.environ["PBL_LYRA_CONTENT"]
DST_CONTENT = os.environ["PBL_DST_CONTENT"]

ar = unreal.AssetRegistryHelpers.get_asset_registry()


def log(m):
    unreal.log(f"[migrate] {m}")


def gather(root_pkgs):
    seen, stack = set(), list(root_pkgs)
    opts = unreal.AssetRegistryDependencyOptions(include_soft_package_references=False, include_hard_package_references=True,
                                                 include_searchable_names=False, include_soft_management_references=False,
                                                 include_hard_management_references=False)
    while stack:
        pkg = stack.pop()
        if pkg in seen or not pkg.startswith("/Game/"):
            continue
        if os.path.basename(pkg).startswith(SKIP_PREFIXES):
            log(f"skip {pkg}")
            continue
        seen.add(pkg)
        for dep in ar.get_dependencies(unreal.Name(pkg), opts) or []:
            stack.append(str(dep))
    return sorted(seen)


def main():
    pkgs = gather(ROOTS)
    log(f"{len(pkgs)} packages")
    copied = 0
    for pkg in pkgs:
        rel = pkg[len("/Game/"):]
        for ext in (".uasset", ".ubulk", ".uexp", ".uptnl"):
            src = os.path.join(SRC_CONTENT, rel + ext)
            if os.path.isfile(src):
                dst = os.path.join(DST_CONTENT, rel + ext)
                os.makedirs(os.path.dirname(dst), exist_ok=True)
                shutil.copy2(src, dst)
                copied += 1
                log(f"copy {rel}{ext}")
    log(f"copied {copied} files")
    log("DONE")


main()
