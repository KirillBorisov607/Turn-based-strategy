"""Cavalry, siege machines and the chicken shaman: E_UnitType and E_Spell entries, spell animations, DT_SpellData
rows (Boulder, RamStrike, Charge), DT_DefaultUnitDataPerType rows and the three new card icons. Run inside the editor."""
import copy
import json

import unreal

ANIMS = "/Game/Blueprints/Core/Combat/Spells/SpellAnimations/"
lib = unreal.KeepersEditorLibrary
assets = unreal.EditorAssetLibrary

# name: (animation to copy, cast pattern, cast min/max, line of sight, aoe pattern, aoe min/max, hp min/max, cooldown)
SPELLS = {
    "Boulder": ("LongRangeArrow", "Diamond", (3, 6), False, "Diamond", (0, 1), (-3, -4), 2),
    "RamStrike": ("ShieldBash", "Diamond", (1, 1), False, "None", (0, 0), (-4, -5), 1),
    "Charge": ("MeleeHit", "Line", (2, 4), False, "None", (0, 0), (-2, -3), 2),
}
UNITS = {
    # name: (base row, hp, mp, spells)
    "Cavalry": ("Warrior", 28, 5, ["SwordSlash", "Charge"]),
    "Catapult": ("Ranger", 18, 1, ["Boulder"]),
    "Ram": ("Warrior", 32, 2, ["RamStrike"]),
    "Shaman": ("Priest", 16, 3, ["RaiseRock", "Sprout", "Roots"]),
}

# The three new medallions only.
tasks = []
for name in SPELLS:
    t = unreal.AssetImportTask()
    t.filename = "C:/ProjectsUE/_logs/card_icons/T_Card_%s.png" % name
    t.destination_path = "/Game/Keepers/Icons"
    t.replace_existing = True
    t.automated = True
    t.save = False
    tasks.append(t)
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)
for t in tasks:
    for p in t.imported_object_paths:
        tex = unreal.load_asset(p)
        tex.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI)
        tex.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
        tex.set_editor_property("srgb", True)
        assets.save_asset(p.split(".")[0], False)
        print("icon", p)

spell_enum = unreal.load_asset("/Game/Blueprints/Core/Combat/Spells/Utilities/E_Spell")
for name in SPELLS:
    print("spell", name, lib.add_user_enum_entry(spell_enum, name))
assets.save_loaded_asset(spell_enum, False)
unit_enum = unreal.load_asset("/Game/Art/Units/E_UnitType")
for name in UNITS:
    print("unit", name, lib.add_user_enum_entry(unit_enum, name))
assets.save_loaded_asset(unit_enum, False)

for name, spec in SPELLS.items():
    target = ANIMS + "BP_SpellAnim_" + name
    if not assets.does_asset_exist(target):
        assets.duplicate_asset(ANIMS + "BP_SpellAnim_" + spec[0], target)
        assets.save_asset(target, False)

spells_dt = unreal.load_asset("/Game/Blueprints/Core/Combat/Spells/Utilities/DT_SpellData")
rows = json.loads(spells_dt.export_to_json_string())
template = next(r for r in rows if r["Name"] == "ShieldBash")
rows = [r for r in rows if r["Name"] not in SPELLS]
for name, (anim, pattern, rng, los, aoe, aoe_rng, hp, cooldown) in SPELLS.items():
    r = copy.deepcopy(template)
    r["Name"] = name
    r["Spell"] = name
    r["Assets"]["Animation"] = "/Script/Engine.BlueprintGeneratedClass'%sBP_SpellAnim_%s.BP_SpellAnim_%s_C'" % (ANIMS, name, name)
    r["Assets"]["Icon"] = "/Script/Engine.Texture2D'/Game/Keepers/Icons/T_Card_%s.T_Card_%s'" % (name, name)
    r["Cast"]["RangePattern"] = pattern
    r["Cast"]["RangeMinMax"] = {"X": rng[0], "Y": rng[1]}
    r["Cast"]["LineOfSight"]["RequireLineOfSight"] = los
    r["CastAoe"]["RangePattern"] = aoe
    r["CastAoe"]["RangeMinMax"] = {"X": aoe_rng[0], "Y": aoe_rng[1]}
    r["Impact"]["MinMaxModifier_Hp"] = {"X": hp[0], "Y": hp[1]}
    r["Impact"]["MinMaxModifier_Mp"] = {"X": 0, "Y": 0}
    r["Requirements"]["Cost_HP"] = 0
    r["Requirements"]["MaxCastPerTurn"] = 999
    r["Requirements"]["CooldownTurnCount"] = cooldown
    rows.append(r)
print("spells", unreal.DataTableFunctionLibrary.fill_data_table_from_json_string(spells_dt, json.dumps(rows)))
assets.save_loaded_asset(spells_dt, False)

units_dt = unreal.load_asset("/Game/Blueprints/Core/Units/Utilities/DT_DefaultUnitDataPerType")
rows = json.loads(units_dt.export_to_json_string())
by_name = {r["Name"]: r for r in rows}
rows = [r for r in rows if r["Name"] not in UNITS]
for name, (base, hp, mp, spells) in UNITS.items():
    r = copy.deepcopy(by_name[base])
    r["Name"] = name
    r["Type"] = name
    r["Stats"]["HP_Current"] = hp
    r["Stats"]["HP_Max"] = hp
    r["Stats"]["MP_Current"] = mp
    r["Stats"]["MP_Max"] = mp
    r["Stats"]["AvailableSpells"] = spells
    rows.append(r)
print("units", unreal.DataTableFunctionLibrary.fill_data_table_from_json_string(units_dt, json.dumps(rows)))
assets.save_loaded_asset(units_dt, False)
print(unreal.DataTableFunctionLibrary.get_data_table_row_names(units_dt))
enum_names = [unit_enum.get_display_name_by_index(i) for i in range(unit_enum.num_enums() - 1)] if hasattr(unit_enum, "num_enums") else []
print("unit types", enum_names)
