import unreal, json
enum = unreal.load_asset('/Game/Art/Units/E_UnitType')
for n in ['Bandit', 'BanditArcher', 'Brute']:
    print(n, unreal.KeepersEditorLibrary.add_user_enum_entry(enum, n))
unreal.EditorAssetLibrary.save_loaded_asset(enum, False)
dt = unreal.load_asset('/Game/Blueprints/Core/Units/Utilities/DT_DefaultUnitDataPerType')
rows = json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(dt) if hasattr(unreal.DataTableFunctionLibrary,'export_data_table_to_json_string') else dt.export_to_json_string())
byname = {r['Name']: r for r in rows}
import copy
def make(name, base, hp, mp, spells, icon):
    r = copy.deepcopy(byname[base])
    r['Name'] = name; r['Type'] = name
    r['Stats']['HP_Current'] = hp; r['Stats']['HP_Max'] = hp
    r['Stats']['MP_Current'] = mp; r['Stats']['MP_Max'] = mp
    r['Stats']['AvailableSpells'] = spells
    r['Assets']['Icon'] = "/Script/Engine.Texture2D'/Game/Art/Units/%s/%s.%s'" % (base, icon, icon)
    return r
src = '/Game/Art/Units/Warrior/TX_Icon_Warrior'
for n in ['Bandit', 'BanditArcher', 'Brute']:
    dst = '/Game/Art/Units/Warrior/TX_Icon_%s' % n
    if not unreal.EditorAssetLibrary.does_asset_exist(dst):
        unreal.EditorAssetLibrary.duplicate_asset(src, dst)
rows = [r for r in rows if r['Name'] not in ('Bandit','BanditArcher','Brute')]
rows.append(make('Bandit', 'Warrior', 18, 3, ['SwordSlash'], 'TX_Icon_Bandit'))
a = make('BanditArcher', 'Ranger', 14, 4, ['ShortRangeArrow'], 'TX_Icon_BanditArcher')
a['Assets']['Icon'] = "/Script/Engine.Texture2D'/Game/Art/Units/Warrior/TX_Icon_BanditArcher.TX_Icon_BanditArcher'"
rows.append(a)
rows.append(make('Brute', 'Warrior', 34, 2, ['SwordSlash'], 'TX_Icon_Brute'))
ok = unreal.DataTableFunctionLibrary.fill_data_table_from_json_string(dt, json.dumps(rows))
print('fill', ok, unreal.DataTableFunctionLibrary.get_data_table_row_names(dt))
unreal.EditorAssetLibrary.save_loaded_asset(dt, False)
for i in range(enum.get_editor_property('display_name_map').__len__() if False else 0): pass
