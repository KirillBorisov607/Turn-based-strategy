import unreal
t = unreal.AssetImportTask()
t.filename = r'C:/ProjectsUE/_logs/sfx/MUS_Battle.wav'
t.destination_path = '/Game/Keepers/Audio/Music'
t.replace_existing = True
t.automated = True
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([t])
w = unreal.load_asset('/Game/Keepers/Audio/Music/MUS_Battle')
w.set_editor_property('looping', True)
w.set_editor_property('sound_class_object', unreal.load_asset('/Game/Keepers/Audio/SC_Music'))
unreal.EditorAssetLibrary.save_asset('/Game/Keepers/Audio/Music/MUS_Battle', False)
print(w.get_editor_property('duration'))
