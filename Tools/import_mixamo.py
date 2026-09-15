"""
Импорт персонажа и анимаций Mixamo в /Game/Characters/<Name>.

Запуск: Tools/import_mixamo.bat  (без редактора, через -run=pythonscript)
  Import/Mixamo/<Character>.fbx        -> скелетный меш + скелет + материалы/текстуры
  Import/Mixamo/Anims/**/*.fbx         -> анимации на этот скелет (Without Skin)

Параметры через переменные окружения (или правкой констант ниже):
  PBL_CHAR_FBX  - путь к FBX персонажа
  PBL_CHAR_NAME - имя (папка /Game/Characters/<Name>)
"""
import os, glob
import unreal

ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
CHAR_FBX = os.environ.get("PBL_CHAR_FBX", os.path.join(ROOT, "Import", "Mixamo", "Ch15_nonPBR.fbx"))
CHAR_NAME = os.environ.get("PBL_CHAR_NAME", "Ch15")
ANIM_DIR = os.path.join(ROOT, "Import", "Mixamo", "Anims")
DEST = f"/Game/Characters/{CHAR_NAME}"

eal = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()


def log(m):
    unreal.log(f"[mixamo] {m}")


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


def import_character():
    ui = unreal.FbxImportUI()
    ui.set_editor_property("import_mesh", True)
    ui.set_editor_property("import_as_skeletal", True)
    ui.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_SKELETAL_MESH)
    ui.set_editor_property("import_animations", False)
    ui.set_editor_property("import_materials", True)
    ui.set_editor_property("import_textures", True)
    ui.set_editor_property("create_physics_asset", True)
    sk = ui.get_editor_property("skeletal_mesh_import_data")
    sk.set_editor_property("import_morph_targets", False)
    # Mixamo экспортирует в сантиметрах при масштабе 1.0 - ничего не масштабируем.
    paths = run_task(CHAR_FBX, f"{DEST}/Mesh", ui)
    skeleton = None
    for p in paths:
        a = eal.load_asset(p)
        if isinstance(a, unreal.Skeleton):
            skeleton = a
        elif isinstance(a, unreal.SkeletalMesh) and skeleton is None:
            skeleton = a.get_editor_property("skeleton")
    if skeleton is None:
        raise RuntimeError("skeleton not found after character import")
    log(f"skeleton: {skeleton.get_path_name()}")
    return skeleton


def import_anims(skeleton):
    files = sorted(glob.glob(os.path.join(ANIM_DIR, "**", "*.fbx"), recursive=True))
    log(f"animations: {len(files)} files")
    for f in files:
        ui = unreal.FbxImportUI()
        ui.set_editor_property("import_mesh", False)
        ui.set_editor_property("import_as_skeletal", True)
        ui.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_ANIMATION)
        ui.set_editor_property("import_animations", True)
        ui.set_editor_property("import_materials", False)
        ui.set_editor_property("import_textures", False)
        ui.set_editor_property("skeleton", skeleton)
        run_task(f, f"{DEST}/Anims", ui)


def main():
    if not os.path.isfile(CHAR_FBX):
        raise RuntimeError(f"no file {CHAR_FBX}")
    skeleton = import_character()
    if os.path.isdir(ANIM_DIR):
        import_anims(skeleton)
    log("DONE")


main()
