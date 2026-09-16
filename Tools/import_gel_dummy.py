"""
Импорт куклы из Blender (Import/GelDummy/GelDummy.fbx, части - отдельные объекты) в /Game/Range/GelDummy как статические меши
со сложной коллизией (трассы по реальной форме). Запуск: Tools/import_gel_dummy.bat. Идемпотентен.
"""
import os
import unreal

ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
FBX = os.path.join(ROOT, "Import", "GelDummy", "GelDummy.fbx")
DEST = "/Game/Range/GelDummy"
eal = unreal.EditorAssetLibrary


def log(m):
    unreal.log(f"[geldummy] {m}")


ui = unreal.FbxImportUI()
ui.set_editor_property("import_mesh", True)
ui.set_editor_property("import_as_skeletal", False)
ui.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_STATIC_MESH)
ui.set_editor_property("import_animations", False)
ui.set_editor_property("import_materials", False)
ui.set_editor_property("import_textures", False)
sm = ui.get_editor_property("static_mesh_import_data")
sm.set_editor_property("combine_meshes", False)
sm.set_editor_property("auto_generate_collision", False)
# Вершины относительно пивота объекта (центр части из Blender), а не в абсолютных координатах FBX: актор блока = центр части.
sm.set_editor_property("transform_vertex_to_absolute", False)
task = unreal.AssetImportTask()
task.set_editor_property("filename", FBX)
task.set_editor_property("destination_path", DEST)
task.set_editor_property("automated", True)
task.set_editor_property("replace_existing", True)
task.set_editor_property("save", True)
task.set_editor_property("options", ui)
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
paths = list(task.get_editor_property("imported_object_paths"))
for p in paths:
    a = eal.load_asset(p)
    if isinstance(a, unreal.StaticMesh):
        bs = a.get_editor_property("body_setup")
        if bs:
            bs.set_editor_property("collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
        eal.save_loaded_asset(a)
        b = a.get_bounds()
        log(f"{a.get_name()}: {2*b.box_extent.x:.1f} x {2*b.box_extent.y:.1f} x {2*b.box_extent.z:.1f} cm, complex-as-simple")
log(f"DONE {len(paths)} assets")
