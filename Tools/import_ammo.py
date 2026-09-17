"""
Импорт патрона, стреляной гильзы и пули (Import/Ammo/9x19.fbx) в /Game/Weapons/Ammo.
Масса и размеры остаются в Blender-скрипте и в 9x19.json - здесь только перенос мешей.
Запуск: Tools/import_ammo.bat. Идемпотентен.
"""
import os, json
import unreal

ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
SRC = os.path.join(ROOT, "Import", "Ammo")
DEST = "/Game/Weapons/Ammo"
eal = unreal.EditorAssetLibrary


def log(m):
    unreal.log(f"[ammo] {m}")


fbx = os.path.join(SRC, "9x19.fbx")
if not os.path.exists(fbx):
    raise RuntimeError(f"нет файла {fbx}")

ui = unreal.FbxImportUI()
ui.set_editor_property("import_mesh", True)
ui.set_editor_property("import_as_skeletal", False)
ui.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_STATIC_MESH)
ui.set_editor_property("import_animations", False)
ui.set_editor_property("import_materials", True)
ui.set_editor_property("import_textures", False)
sm = ui.get_editor_property("static_mesh_import_data")
sm.set_editor_property("combine_meshes", False)
sm.set_editor_property("auto_generate_collision", False)
# Пивот - в середине изделия (так его поставил Blender): гильза должна крутиться вокруг себя,
# а не вокруг общего нуля сборки, как детали оружия.
sm.set_editor_property("transform_vertex_to_absolute", False)

task = unreal.AssetImportTask()
task.set_editor_property("filename", fbx)
task.set_editor_property("destination_path", DEST)
task.set_editor_property("automated", True)
task.set_editor_property("replace_existing", True)
task.set_editor_property("save", True)
task.set_editor_property("options", ui)
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])

for p in list(task.get_editor_property("imported_object_paths")):
    a = eal.load_asset(p)
    if not isinstance(a, unreal.StaticMesh):
        continue
    # Импортёр UE добавляет к имени суффикс _001; в коде меш ищется по чистому имени.
    if a.get_name().endswith("_001"):
        want = f"{DEST}/{a.get_name()[:-4]}"
        if eal.does_asset_exist(want):
            eal.delete_asset(want)
        eal.rename_asset(f"{DEST}/{a.get_name()}", want)
        a = eal.load_asset(want)
    b = a.get_bounds()
    bs = a.get_editor_property("body_setup")
    if bs:
        # Гильза падает и скачет по полу - нужна выпуклая простая коллизия, а не сложная по треугольникам.
        bs.set_editor_property("collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_SIMPLE_AND_COMPLEX)
    a.set_editor_property("light_map_resolution", 8)
    unreal.EditorStaticMeshLibrary.remove_collisions(a)
    unreal.EditorStaticMeshLibrary.add_simple_collisions(a, unreal.ScriptingCollisionShapeType.CAPSULE)
    eal.save_loaded_asset(a)
    log(f"{a.get_name():12s} {2*b.box_extent.x:6.2f} x {2*b.box_extent.y:5.2f} x {2*b.box_extent.z:5.2f} cm, "
        f"{a.get_num_triangles(0)} tris")

meta = os.path.join(SRC, "9x19.json")
if os.path.exists(meta):
    with open(meta, encoding="utf-8") as f:
        m = json.load(f)
    log(f"mass, g: {m['parts'].get('_mass_g')}")
log("DONE")
