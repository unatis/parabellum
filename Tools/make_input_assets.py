"""
Генерирует ассеты Enhanced Input для Parabellum.

Запуск без открытия редактора (см. Tools/make_input_assets.bat):
    UnrealEditor-Cmd.exe Parabellum.uproject -run=pythonscript -script="Tools/make_input_assets.py"

Идемпотентен: существующие ассеты перенастраиваются, а не дублируются.
Источник правды по раскладке — этот файл, не .uasset. Правь здесь и перегенерируй.
"""
import unreal

ACTIONS_PATH = "/Game/Input/Actions"
CONTEXT_PATH = "/Game/Input"
CONTEXT_NAME = "IMC_Default"

# name -> тип значения
ACTIONS = {
    "IA_Move":   unreal.InputActionValueType.AXIS2D,   # X = вправо, Y = вперёд
    "IA_Look":   unreal.InputActionValueType.AXIS2D,   # сырая дельта мыши, чувствительность в C++
    "IA_Jump":   unreal.InputActionValueType.BOOLEAN,
    "IA_Crouch": unreal.InputActionValueType.BOOLEAN,
    "IA_Fire":   unreal.InputActionValueType.BOOLEAN,
    "IA_Reload": unreal.InputActionValueType.BOOLEAN,
    "IA_Aim":    unreal.InputActionValueType.BOOLEAN,
}

# (action, key, [модификаторы]) — модификатор = (класс, {свойство: значение})
NEGATE_ALL = (unreal.InputModifierNegate, {})
TO_Y = (unreal.InputModifierSwizzleAxis, {"order": unreal.InputAxisSwizzle.YXZ})

MAPPINGS = [
    ("IA_Move", "W", [TO_Y]),
    ("IA_Move", "S", [NEGATE_ALL, TO_Y]),
    ("IA_Move", "D", []),
    ("IA_Move", "A", [NEGATE_ALL]),
    ("IA_Look", "Mouse2D", []),
    ("IA_Jump", "SpaceBar", []),
    ("IA_Crouch", "LeftControl", []),
    ("IA_Fire", "LeftMouseButton", []),
    ("IA_Reload", "R", []),
    ("IA_Aim", "RightMouseButton", []),
]

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
eal = unreal.EditorAssetLibrary


def get_or_create(name, path, asset_class, factory):
    full = f"{path}/{name}"
    if eal.does_asset_exist(full):
        asset = eal.load_asset(full)
        unreal.log(f"[input] reuse  {full}")
    else:
        asset = asset_tools.create_asset(name, path, asset_class, factory)
        unreal.log(f"[input] create {full}")
    if asset is None:
        raise RuntimeError(f"failed to create {full}")
    return asset


def make_key(key_name):
    # unreal.Key не принимает аргументов конструктора - имя задаём свойством.
    key = unreal.Key()
    key.set_editor_property("key_name", key_name)
    return key


def make_modifier(imc, spec):
    cls, props = spec
    mod = unreal.new_object(cls, outer=imc)
    for k, v in props.items():
        mod.set_editor_property(k, v)
    return mod


def main():
    actions = {}
    for name, value_type in ACTIONS.items():
        ia = get_or_create(name, ACTIONS_PATH, unreal.InputAction, unreal.InputAction_Factory())
        ia.set_editor_property("value_type", value_type)
        actions[name] = ia

    imc = get_or_create(CONTEXT_NAME, CONTEXT_PATH, unreal.InputMappingContext,
                        unreal.InputMappingContext_Factory())
    # С 5.7 UInputMappingContext.Mappings - deprecated и рантаймом не читается:
    # GetMappings() возвращает DefaultKeyMappings.Mappings. Пишем именно туда.
    # Модификаторы вешаем сразу, поэтому map_key() не используем - собираем
    # FEnhancedActionKeyMapping сами и кладём массив целиком.
    new_mappings = []
    for action_name, key_name, modifier_specs in MAPPINGS:
        m = unreal.EnhancedActionKeyMapping()
        m.set_editor_property("action", actions[action_name])
        m.set_editor_property("key", make_key(key_name))
        m.set_editor_property("modifiers", [make_modifier(imc, spec) for spec in modifier_specs])
        new_mappings.append(m)

    data = unreal.InputMappingContextMappingData()
    data.set_editor_property("mappings", new_mappings)
    imc.set_editor_property("default_key_mappings", data)
    # Старое поле чистим, чтобы в ассете не лежало два набора.
    imc.set_editor_property("mappings", [])

    for ia in actions.values():
        eal.save_loaded_asset(ia)
    eal.save_loaded_asset(imc)

    # Контроль: перечитать и напечатать, что реально записалось.
    for m in imc.get_editor_property("default_key_mappings").get_editor_property("mappings"):
        mods = [type(x).__name__ for x in m.get_editor_property("modifiers")]
        unreal.log(f"[input] {m.get_editor_property('key').get_editor_property('key_name')} -> "
                   f"{m.get_editor_property('action').get_name()} {mods}")
    unreal.log("[input] DONE")


main()
