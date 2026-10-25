import unreal, glob, json
tasks = []
for f in sorted(glob.glob(r'C:/ProjectsUE/_logs/portraits/T_Portrait_*.png')):
    t = unreal.AssetImportTask(); t.filename = f; t.destination_path = '/Game/Keepers/Portraits'
    t.replace_existing = True; t.automated = True; t.save = False
    tasks.append(t)
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)
for t in tasks:
    for p in t.imported_object_paths:
        tex = unreal.load_asset(p)
        tex.set_editor_property('lod_group', unreal.TextureGroup.TEXTUREGROUP_UI)
        tex.set_editor_property('mip_gen_settings', unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
        unreal.EditorAssetLibrary.save_asset(p.split('.')[0], False)
dt = unreal.load_asset('/Game/Blueprints/Core/Units/Utilities/DT_DefaultUnitDataPerType')
rows = json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(dt))
for r in rows:
    n = r['Name']
    if n in ('Warrior','Ranger','Priest','Bandit','BanditArcher','Brute'):
        r['Assets']['Icon'] = "/Script/Engine.Texture2D'/Game/Keepers/Portraits/T_Portrait_%s.T_Portrait_%s'" % (n, n)
print(unreal.DataTableFunctionLibrary.fill_data_table_from_json_string(dt, json.dumps(rows)))
print(unreal.EditorAssetLibrary.save_asset('/Game/Blueprints/Core/Units/Utilities/DT_DefaultUnitDataPerType', False))
for r in json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(dt)): print(r['Name'], r['Assets']['Icon'][-40:])
