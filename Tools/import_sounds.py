"""
Импорт звуков выстрела (Import/Sounds/cut/*.wav) в /Game/Audio/Glock17 и создание затухания ATT_Gunshot.
Запуск: Tools/import_sounds.bat. Идемпотентен.
"""
import os, glob
import unreal

ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
SRC = os.path.join(ROOT, "Import", "Sounds", "cut")
DEST = "/Game/Audio/Glock17"
eal = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()


def log(m):
    unreal.log(f"[sounds] {m}")


for f in sorted(glob.glob(os.path.join(SRC, "*.wav"))):
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", f)
    task.set_editor_property("destination_path", DEST)
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("save", True)
    tools.import_asset_tasks([task])
    log(f"{os.path.basename(f)} -> {list(task.get_editor_property('imported_object_paths'))}")

# Затухание: естественный спад, ближняя зона 3 м, слышно до 800 м (реальный выстрел 9 мм ~160 дБ у дула), поглощение воздухом.
full = f"{DEST}/ATT_Gunshot"
att = eal.load_asset(full) if eal.does_asset_exist(full) else tools.create_asset("ATT_Gunshot", DEST, unreal.SoundAttenuation, None)
settings = att.get_editor_property("attenuation")
settings.set_editor_property("attenuation_shape", unreal.AttenuationShape.SPHERE)
settings.set_editor_property("distance_algorithm", unreal.AttenuationDistanceModel.NATURAL_SOUND)
settings.set_editor_property("attenuation_shape_extents", unreal.Vector(300.0, 0.0, 0.0))
settings.set_editor_property("falloff_distance", 80000.0)
settings.set_editor_property("attenuate_with_lpf", True)   # поглощение воздухом: НЧ-фильтр с дистанцией
settings.set_editor_property("lpf_radius_min", 5000.0)
settings.set_editor_property("lpf_radius_max", 60000.0)
settings.set_editor_property("lpf_frequency_at_max", 1500.0)
settings.set_editor_property("spatialize", True)
att.set_editor_property("attenuation", settings)
eal.save_loaded_asset(att)
log(f"saved {full}")
log("DONE")
