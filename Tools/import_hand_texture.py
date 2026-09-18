"""Текстуры кожи рук из фотографий + материал с рельефом и шероховатостью.

Без карты нормалей и шероховатости кожа освещается как гладкий пластик - это и было
главной причиной «мультяшности», а не цвет.
"""
import os
import unreal

ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
SRC = os.path.join(ROOT, "Import", "Hands", "Hand_D.png")
DEST = "/Game/Weapons/G17Arms/Mesh"
eal = unreal.EditorAssetLibrary

if not os.path.exists(SRC):
    raise RuntimeError(f"нет файла {SRC}")

mel = unreal.MaterialEditingLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()


def import_tex(name, srgb, comp):
    path = os.path.join(ROOT, "Import", "Hands", name + ".png")
    if not os.path.exists(path):
        raise RuntimeError(f"нет файла {path}")
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", path)
    task.set_editor_property("destination_path", DEST)
    task.set_editor_property("destination_name", name)
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("save", True)
    task.set_editor_property("factory", unreal.TextureFactory())
    tools.import_asset_tasks([task])
    tex = eal.load_asset(f"{DEST}/{name}")
    if tex is None:
        raise RuntimeError(f"импорт не дал ассета {name}")
    tex.set_editor_property("srgb", srgb)
    tex.set_editor_property("compression_settings", comp)
    eal.save_loaded_asset(tex)
    unreal.log(f"[hands] {name}: {tex.blueprint_get_size_x()}x{tex.blueprint_get_size_y()}")
    return tex


TC = unreal.TextureCompressionSettings
diffuse = import_tex("Hand_D", True, TC.TC_DEFAULT)
normal = import_tex("Hand_N", False, TC.TC_NORMALMAP)
rough = import_tex("Hand_R", False, TC.TC_MASKS)

# Материал кожи: цвет, рельеф, шероховатость. Собирается скриптом, руками не правится.
full = f"{DEST}/M_Hand"
if eal.does_asset_exist(full):
    mat = eal.load_asset(full)
    mel.delete_all_material_expressions(mat)
else:
    mat = tools.create_asset("M_Hand", DEST, unreal.Material, unreal.MaterialFactoryNew())

for tex, prop, x, y in ((diffuse, unreal.MaterialProperty.MP_BASE_COLOR, -400, -200),
                        (normal, unreal.MaterialProperty.MP_NORMAL, -400, 100),
                        (rough, unreal.MaterialProperty.MP_ROUGHNESS, -400, 400)):
    node = mel.create_material_expression(mat, unreal.MaterialExpressionTextureSample, x, y)
    node.set_editor_property("texture", tex)
    if not mel.connect_material_property(node, "", prop):
        raise RuntimeError(f"не соединилось: {prop}")

# Кожа отражает слабо: значение по умолчанию 0.5 даёт блик как у пластика.
spec = mel.create_material_expression(mat, unreal.MaterialExpressionConstant, -400, 700)
spec.set_editor_property("r", 0.28)
mel.connect_material_property(spec, "", unreal.MaterialProperty.MP_SPECULAR)
mel.recompile_material(mat)
eal.save_loaded_asset(mat)
unreal.log(f"[hands] материал {full}")

# Ставим материал в слот кожи у меша рук.
mesh = eal.load_asset("/Game/Weapons/G17Arms/Mesh/glock17")
if mesh:
    mats = mesh.get_editor_property("materials")
    for i, m in enumerate(mats):
        if str(m.get_editor_property("material_slot_name")) == "Hand_D":
            m.set_editor_property("material_interface", mat)
            unreal.log(f"[hands] слот {i} Hand_D -> M_Hand")
    mesh.set_editor_property("materials", mats)
    eal.save_loaded_asset(mesh)
unreal.log("[hands] DONE")
