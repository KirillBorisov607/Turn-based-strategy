import unreal, glob, os
tasks = []
for f in sorted(glob.glob(r'C:/ProjectsUE/_logs/sfx/SFX_*.wav')):
    t = unreal.AssetImportTask()
    t.filename = f
    t.destination_path = '/Game/Keepers/Audio/SFX'
    t.replace_existing = True
    t.automated = True
    t.save = False
    tasks.append(t)
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)
effects = unreal.load_asset('/Game/Keepers/Audio/SC_Effects')
ui = unreal.load_asset('/Game/Keepers/Audio/SC_UI')
for t in tasks:
    for p in t.imported_object_paths:
        w = unreal.load_asset(p)
        name = p.split('.')[-1]
        w.set_editor_property('sound_class_object', ui if name in ('SFX_Card', 'SFX_Coin', 'SFX_LevelUp', 'SFX_Victory', 'SFX_Defeat') else effects)
        unreal.EditorAssetLibrary.save_asset(p.split('.')[0], False)
        print(p, w.get_editor_property('duration'))
