"""
Импорт пака BarcodeGames "G17 - FPS Weapon Animations Pack" (Import/Glock17/glock17.fbx: руки + Glock 17, один риг,
анимации Draw/Fire/Reload/ReloadEmpty/Holster) в /Game/Weapons/G17Arms.
Запуск: Tools/import_g17_arms.bat. Идемпотентен.
"""
import os
import unreal

ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
FBX = os.environ.get("PBL_G17_FBX", os.path.join(ROOT, "Import", "Glock17", "glock17.fbx"))
DEST = os.environ.get("PBL_G17_DEST", "/Game/Weapons/G17Arms")
USE_T0 = os.environ.get("PBL_G17_T0", "1") == "1"          # T0 как ref pose: лечит рваный скиннинг при несовпадении bind pose
USE_INTERCHANGE = os.environ.get("PBL_G17_INTERCHANGE", "0") == "1"
eal = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()


def log(m):
    unreal.log(f"[g17arms] {m}")


def run_task(filename, dest, options):
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", filename)
    task.set_editor_property("destination_path", dest)
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("save", True)
    task.set_editor_property("options", options)
    tools.import_asset_tasks([task])
    paths = list(task.get_editor_property("imported_object_paths"))
    log(f"{os.path.basename(filename)} -> {len(paths)} objects")
    for p in paths:
        log(f"   {p}")
    return paths


ui = unreal.FbxImportUI()
ui.set_editor_property("import_mesh", True)
ui.set_editor_property("import_as_skeletal", True)
ui.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_SKELETAL_MESH)
ui.set_editor_property("import_animations", True)
ui.set_editor_property("import_materials", True)
ui.set_editor_property("import_textures", True)
ui.set_editor_property("create_physics_asset", False)
sk = ui.get_editor_property("skeletal_mesh_import_data")
sk.set_editor_property("import_morph_targets", False)
sk.set_editor_property("use_t0_as_ref_pose", USE_T0)
if USE_INTERCHANGE:
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", FBX); task.set_editor_property("destination_path", f"{DEST}/Mesh")
    task.set_editor_property("automated", True); task.set_editor_property("replace_existing", True); task.set_editor_property("save", True)
    tools.import_asset_tasks([task])
    paths = list(task.get_editor_property("imported_object_paths"))
    for p_ in paths: log(f"   {p_}")
else:
    paths = run_task(FBX, f"{DEST}/Mesh", ui)

mesh = None
for p in paths:
    a = eal.load_asset(p)
    if isinstance(a, unreal.SkeletalMesh):
        mesh = a
if mesh:
    b = mesh.get_bounds()
    e, o = b.box_extent, b.origin
    log(f"MESH {mesh.get_path_name()}: size {2*e.x:.1f} x {2*e.y:.1f} x {2*e.z:.1f} cm, min ({o.x-e.x:.1f},{o.y-e.y:.1f},{o.z-e.z:.1f}) max ({o.x+e.x:.1f},{o.y+e.y:.1f},{o.z+e.z:.1f})")
    log(f"MATERIALS {[str(m.material_slot_name) for m in mesh.materials]}")
    skel = mesh.skeleton
    log(f"SKELETON {skel.get_path_name()}")
    names = []
    try:
        rs = mesh.get_editor_property("ref_skeleton") if False else None
    except Exception:
        pass
    # имена костей через анимацию/скелет недоступны напрямую в Python - печатаем через сокеты и bone tree размер
    pass
anims = [p for p in paths if eal.load_asset(p) and isinstance(eal.load_asset(p), unreal.AnimSequence)]
log(f"ANIMS {len(anims)}: {[p.split('.')[-1] for p in anims]}")
log("DONE")
