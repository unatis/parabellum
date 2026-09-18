"""Выгружает нынешние текстуры рук в Import/Hands/current для разбора. Одноразовый разбор."""
import os
import unreal

ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
OUT = os.path.join(ROOT, "Import", "Hands", "current")
os.makedirs(OUT, exist_ok=True)
eal = unreal.EditorAssetLibrary

for p in eal.list_assets("/Game/Weapons/G17Arms", recursive=True):
    a = eal.load_asset(p)
    if isinstance(a, unreal.Texture2D):
        unreal.log(f"[hands] texture {a.get_name()} {a.blueprint_get_size_x()}x{a.blueprint_get_size_y()} "
                   f"srgb={a.get_editor_property('srgb')} comp={a.get_editor_property('compression_settings')}")
        task = unreal.AssetExportTask()
        task.set_editor_property("object", a)
        task.set_editor_property("filename", os.path.join(OUT, a.get_name() + ".png"))
        task.set_editor_property("automated", True)
        task.set_editor_property("prompt", False)
        task.set_editor_property("exporter", unreal.TextureExporterPNG())
        unreal.Exporter.run_asset_export_task(task)
    elif isinstance(a, unreal.MaterialInterface):
        unreal.log(f"[hands] material {a.get_name()}")
    elif isinstance(a, unreal.SkeletalMesh):
        mats = a.get_editor_property("materials")
        unreal.log(f"[hands] mesh {a.get_name()}: {len(mats)} материалов")
        for m in mats:
            mi = m.get_editor_property("material_interface")
            unreal.log(f"[hands]   slot {m.get_editor_property('material_slot_name')} -> {mi.get_name() if mi else None}")
unreal.log("[hands] DONE")
