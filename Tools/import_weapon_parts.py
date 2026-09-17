"""
Импорт деталей оружия из Blender (Import/Glock17Parts/Glock17_<Gen>.fbx + .json) в /Game/Weapons/<Name>Parts
и генерация Content/Data/WeaponParts.csv - порядка и направлений разборки для оружейной комнаты.
Единый источник правды - скрипт Blender: здесь только перенос.
Запуск: Tools/import_weapon_parts.bat. Идемпотентен.
"""
import os, json, glob
import unreal

ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
SRC = os.path.join(ROOT, "Import", "Glock17Parts")
WEAPON = os.environ.get("PBL_WEAPON", "Glock17")
GEN = os.environ.get("PBL_GEN", "Gen5")
DEST = f"/Game/Weapons/{WEAPON}Parts"
eal = unreal.EditorAssetLibrary


def log(m):
    unreal.log(f"[parts] {m}")


fbx = os.path.join(SRC, f"{WEAPON}_{GEN}.fbx")
meta_path = os.path.join(SRC, f"{WEAPON}_{GEN}.json")
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
# Пивот каждой детали - в нуле сборки: детали встают на свои места сами, а разбор идёт смещением.
sm.set_editor_property("transform_vertex_to_absolute", True)

task = unreal.AssetImportTask()
task.set_editor_property("filename", fbx)
task.set_editor_property("destination_path", DEST)
task.set_editor_property("automated", True)
task.set_editor_property("replace_existing", True)
task.set_editor_property("save", True)
task.set_editor_property("options", ui)
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])

imported = {}
for p in list(task.get_editor_property("imported_object_paths")):
    a = eal.load_asset(p)
    if isinstance(a, unreal.StaticMesh):
        b = a.get_bounds()
        # Коллизия по геометрии: нужна, чтобы наводить курсор на деталь в оружейной комнате.
        bs = a.get_editor_property("body_setup")
        if bs:
            bs.set_editor_property("collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
            eal.save_loaded_asset(a)
        imported[a.get_name()] = a
        log(f"{a.get_name():18s} {2*b.box_extent.x:6.1f} x {2*b.box_extent.y:5.1f} x {2*b.box_extent.z:6.1f} cm")
log(f"imported {len(imported)} parts")

# --- Данные разборки в CSV проекта ---
with open(meta_path, encoding="utf-8") as f:
    meta = json.load(f)
rows = ["Weapon,Generation,Part,DisplayName,Stage,Group,Order,DirX,DirY,DirZ,Dist_cm,Description"]
for stage, key in (("Field", "_fieldstrip"), ("Full", "_fullstrip")):
    for it in meta["parts"].get(key, []):
        d = it["dir"]
        rows.append(f'{WEAPON},{GEN},{it["part"]},"{it["name"]}",{stage},{it.get("group","")},'
                    f'{it["order"]},{d[0]},{d[1]},{d[2]},{it["dist_mm"] / 10.0:.1f},"{it.get("desc", "")}"')
out_csv = os.path.join(ROOT, "Content", "Data", "WeaponParts.csv")
with open(out_csv, "w", encoding="utf-8", newline="\n") as f:
    f.write("\n".join(rows) + "\n")
log(f"wrote {out_csv}: {len(rows) - 1} steps")

# --- Раскладка боеприпаса: где лежит патрон в патроннике и патроны в магазине ---
am = meta["parts"].get("_ammo")
if am:
    cols = ("Weapon,Generation,RoundMesh,Capacity,ChamberX_mm,ChamberY_mm,ChamberZ_mm,ChamberPitch_deg,"
            "StackX_mm,StackY_mm,StackZ_mm,PitchX_mm,PitchY_mm,PitchZ_mm,LatX_mm,LatY_mm,LatZ_mm,RoundPitch_deg")
    vals = [WEAPON, GEN, am["round"], am["capacity"], *am["chamber"], am["chamber_pitch_deg"],
            *am["stack_first"], *am["stack_pitch"], *am["stack_lateral"], am["round_pitch_deg"]]
    out_ammo = os.path.join(ROOT, "Content", "Data", "AmmoLayout.csv")
    with open(out_ammo, "w", encoding="utf-8", newline="\n") as f:
        f.write(cols + "\n" + ",".join(str(v) for v in vals) + "\n")
    g = am.get("geometry", {})
    log(f"wrote {out_ammo}: {am['capacity']} патронов, наклон {g.get('tilt_deg')} град, шаг {g.get('pitch_mm')} мм")

prov = meta["parts"].get("_provenance", {})
log(f"provenance: {prov.get('documented')}/{prov.get('total')} documented ({prov.get('completeness_pct')}%)")
log("DONE")
