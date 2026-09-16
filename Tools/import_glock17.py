"""
Импорт модели Glock 17 (Import/Glock17/unpacked: source/*.fbx + textures/*.png) в /Game/Weapons/Glock17.
Статический FBX импортируется как скелетный меш (корневая кость) - оружие использует USkeletalMeshComponent.
Запуск: Tools/import_glock17.bat. Идемпотентен (replace_existing).
"""
import os, glob
import unreal

ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
SRC = os.path.join(ROOT, "Import", "Glock17", "unpacked")
DEST = "/Game/Weapons/Glock17"
eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()


def log(m):
    unreal.log(f"[glock17] {m}")


def run_task(filename, dest, options=None):
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", filename)
    task.set_editor_property("destination_path", dest)
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("save", True)
    if options is not None:
        task.set_editor_property("options", options)
    tools.import_asset_tasks([task])
    paths = list(task.get_editor_property("imported_object_paths"))
    log(f"{os.path.basename(filename)} -> {paths}")
    return paths


def import_mesh():
    fbx = glob.glob(os.path.join(SRC, "source", "*.fbx"))[0]
    ui = unreal.FbxImportUI()
    ui.set_editor_property("import_mesh", True)
    ui.set_editor_property("import_as_skeletal", True)
    ui.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_SKELETAL_MESH)
    ui.set_editor_property("import_animations", False)
    ui.set_editor_property("import_materials", False)
    ui.set_editor_property("import_textures", False)
    ui.set_editor_property("create_physics_asset", False)
    paths = run_task(fbx, f"{DEST}/Mesh", ui)
    mesh = None
    for p in paths:
        a = eal.load_asset(p)
        if isinstance(a, unreal.SkeletalMesh):
            mesh = a
    if mesh is None:
        raise RuntimeError("skeletal mesh not imported")
    b = mesh.get_bounds()
    e, o = b.box_extent, b.origin
    log(f"MESH {mesh.get_path_name()}: size {2*e.x:.1f} x {2*e.y:.1f} x {2*e.z:.1f} cm, origin ({o.x:.1f},{o.y:.1f},{o.z:.1f}), min ({o.x-e.x:.1f},{o.y-e.y:.1f},{o.z-e.z:.1f}) max ({o.x+e.x:.1f},{o.y+e.y:.1f},{o.z+e.z:.1f})")
    log(f"MATERIALS {[str(m.material_slot_name) for m in mesh.materials]}")
    return mesh


def import_textures():
    tex = {}
    for f in sorted(glob.glob(os.path.join(SRC, "textures", "G17_*.png"))):
        kind = os.path.splitext(os.path.basename(f))[0].split("_")[-1]   # BaseColor, Normal, ...
        paths = run_task(f, f"{DEST}/Textures")
        if not paths:
            continue
        t = eal.load_asset(paths[0])
        if kind == "Normal":
            t.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
            t.set_editor_property("srgb", False)
        elif kind in ("Metalness", "Roughness", "Height"):
            t.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_GRAYSCALE)
            t.set_editor_property("srgb", False)
        eal.save_loaded_asset(t)
        tex[kind] = t
    log(f"TEXTURES {sorted(tex)}")
    return tex


def make_material(tex):
    name, path = "M_Glock17", f"{DEST}/Materials"
    full = f"{path}/{name}"
    if eal.does_asset_exist(full):
        mat = eal.load_asset(full)
        mel.delete_all_material_expressions(mat)
    else:
        mat = tools.create_asset(name, path, unreal.Material, unreal.MaterialFactoryNew())
    y = 0
    def sample(kind, prop, sampler=None, channel=None):
        nonlocal y
        if kind not in tex:
            return
        s = mel.create_material_expression(mat, unreal.MaterialExpressionTextureSample, -500, y)
        s.set_editor_property("texture", tex[kind])
        if sampler is not None:
            s.set_editor_property("sampler_type", sampler)
        mel.connect_material_property(s, channel or "", prop)
        y += 250
    sample("BaseColor", unreal.MaterialProperty.MP_BASE_COLOR)
    sample("Normal", unreal.MaterialProperty.MP_NORMAL, unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    sample("Metalness", unreal.MaterialProperty.MP_METALLIC, unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_GRAYSCALE, "R")
    sample("Roughness", unreal.MaterialProperty.MP_ROUGHNESS, unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_GRAYSCALE, "R")
    mat.set_editor_property("used_with_skeletal_mesh", True)
    mel.recompile_material(mat)
    eal.save_loaded_asset(mat)
    log(f"MATERIAL {full}")
    return mat


mesh = import_mesh()
tex = import_textures()
mat = make_material(tex)
mats = list(mesh.materials)
for i in range(len(mats)):
    mats[i].material_interface = mat
mesh.set_editor_property("materials", mats)
eal.save_loaded_asset(mesh)
log("DONE")
