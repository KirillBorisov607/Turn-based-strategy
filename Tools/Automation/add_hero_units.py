"""The Mage, the Rogue and the Barbarian as battle units: E_UnitType and E_Spell entries, spell animations,
DT_SpellData rows (9 abilities) and DT_DefaultUnitDataPerType rows. Run inside the editor (py.sh)."""
import copy
import json

import unreal

ANIMS = "/Game/Blueprints/Core/Combat/Spells/SpellAnimations/"
lib = unreal.KeepersEditorLibrary
assets = unreal.EditorAssetLibrary

# name: (animation to copy, cast pattern, cast min/max, line of sight, aoe pattern, aoe min/max, hp min/max, mp, cost hp, max per turn, cooldown)
SPELLS = {
    "Fireball": ("LongRangeArrow", "Diamond", (2, 4), False, "Diamond", (0, 1), (-3, -5), 0, 0, 999, 2),
    "IcePath": ("Sprout", "Line", (1, 3), False, "None", (0, 0), (-1, -2), -1, 0, 999, 1),
    "ChainLightning": ("ShortRangeArrow", "Diamond", (2, 4), True, "None", (0, 0), (-3, -3), 0, 0, 999, 1),
    "Dash": ("MeleeHit", "Line", (2, 3), False, "None", (0, 0), (0, 0), 0, 0, 999, 1),
    "Backstab": ("SwordSlash", "Diamond", (1, 1), False, "None", (0, 0), (-4, -4), 0, 0, 999, 0),
    "SmokeBomb": ("RaiseRock", "Diamond", (1, 3), False, "None", (0, 0), (0, 0), 0, 0, 999, 3),
    "Crush": ("SwordSlash", "Diamond", (1, 1), False, "None", (0, 0), (-5, -7), 0, 0, 999, 1),
    "Throw": ("ShieldBash", "Diamond", (1, 1), False, "None", (0, 0), (-2, -2), 0, 0, 999, 1),
    "Rage": ("Heal", "Diamond", (0, 0), False, "None", (0, 0), (0, 0), 0, 4, 1, 3),
}
UNITS = {
    # name: (base row, hp, mp, spells)
    "Mage": ("Priest", 22, 3, ["Fireball", "IcePath", "ChainLightning"]),
    "Rogue": ("Ranger", 26, 5, ["Dash", "Backstab", "SmokeBomb"]),
    "Barbarian": ("Warrior", 45, 3, ["Crush", "Throw", "Rage"]),
}

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
for name, (anim, pattern, rng, los, aoe, aoe_rng, hp, mp, cost_hp, per_turn, cooldown) in SPELLS.items():
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
    r["Impact"]["MinMaxModifier_Mp"] = {"X": mp, "Y": mp}
    r["Requirements"]["Cost_HP"] = cost_hp
    r["Requirements"]["MaxCastPerTurn"] = per_turn
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
