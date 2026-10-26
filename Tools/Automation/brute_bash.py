import unreal, json
dt = unreal.load_asset('/Game/Blueprints/Core/Units/Utilities/DT_DefaultUnitDataPerType')
rows = json.loads(dt.export_to_json_string())
for r in rows:
    if r['Name'] == 'Brute':
        r['Stats']['AvailableSpells'] = ['SwordSlash', 'ShieldBash']
        print('Brute', r['Stats']['AvailableSpells'])
print(unreal.DataTableFunctionLibrary.fill_data_table_from_json_string(dt, json.dumps(rows)))
unreal.EditorAssetLibrary.save_loaded_asset(dt, False)
