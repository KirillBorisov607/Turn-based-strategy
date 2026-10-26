import unreal, json
dt = unreal.load_asset('/Game/Blueprints/Core/Units/Utilities/DT_DefaultUnitDataPerType')
rows = json.loads(dt.export_to_json_string())
out = {r['Name']: {'hp': r['Stats']['HP_Max'], 'mp': r['Stats']['MP_Max'], 'ap': r['Stats']['AP_Max'], 'spells': r['Stats']['AvailableSpells']} for r in rows}
open(r'C:/ProjectsUE/_logs/unit_stats.json', 'w', encoding='utf-8').write(json.dumps(out, ensure_ascii=False, indent=1))
print(out)
