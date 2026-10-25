import unreal, re
names = ['Icon_RaiseRock', 'Icon_ShieldBash', 'Icon_Sprout', 'Icon_Roots']
tasks = []
for n in names:
    t = unreal.AssetImportTask()
    t.filename = 'C:/ProjectsUE/_icons/%s.png' % n
    t.destination_path = '/Game/Art/Textures/SpellIcons'
    t.replace_existing = True
    t.automated = True
    t.save = True
    tasks.append(t)
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)
for n in names:
    tex = unreal.load_asset('/Game/Art/Textures/SpellIcons/' + n)
    tex.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_EDITOR_ICON)
    tex.set_editor_property('mip_gen_settings', unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
    tex.set_editor_property('lod_group', unreal.TextureGroup.TEXTUREGROUP_UI)
    unreal.EditorAssetLibrary.save_loaded_asset(tex, False)
lib = unreal.DataTableFunctionLibrary
spells = unreal.load_asset('/Game/Blueprints/Core/Combat/Spells/Utilities/DT_SpellData')
csv = spells.export_to_csv_string() if hasattr(spells, 'export_to_csv_string') else lib.export_data_table_to_csv_string(spells)
icon = lambda n: "Icon=\"\"/Script/Engine.Texture2D'/Game/Art/Textures/SpellIcons/%s.%s'\"\"" % (n, n)
out = []
for l in csv.split('\n'):
    for spell, n in [('RaiseRock', 'Icon_RaiseRock'), ('ShieldBash', 'Icon_ShieldBash'), ('Sprout', 'Icon_Sprout'), ('Roots', 'Icon_Roots')]:
        if l.startswith(spell + ','):
            l = re.sub(r'Icon=""[^"]*""', icon(n), l)
    out.append(l)
lib.fill_data_table_from_csv_string(spells, '\n'.join(out))
unreal.EditorAssetLibrary.save_loaded_asset(spells, False)
check = spells.export_to_csv_string() if hasattr(spells, 'export_to_csv_string') else lib.export_data_table_to_csv_string(spells)
print([(l.split(',')[0], re.findall(r'SpellIcons/(\w+)\.', l)) for l in check.split('\n') if l.split(',')[0] in ('RaiseRock', 'ShieldBash', 'Sprout', 'Roots', 'Gust')])
