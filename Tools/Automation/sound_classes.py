import unreal
at = unreal.AssetToolsHelpers.get_asset_tools()
master = unreal.load_asset('/Engine/EngineSounds/Master')
print('master obj', master)
classes = {}
for n in ['SC_Music', 'SC_Ambient', 'SC_Effects', 'SC_UI']:
    p = '/Game/Keepers/Audio/' + n
    a = unreal.load_asset(p) if unreal.EditorAssetLibrary.does_asset_exist(p) else at.create_asset(n, '/Game/Keepers/Audio', unreal.SoundClass, unreal.SoundClassFactory())
    if master:
        try:
            a.set_editor_property('parent_class', master)
        except Exception as e:
            print('parent err', e)
    unreal.EditorAssetLibrary.save_loaded_asset(a, False)
    classes[n] = a
ar = unreal.AssetRegistryHelpers.get_asset_registry()
count = {}
for cls in ['SoundCue', 'SoundWave']:
    for d in ar.get_assets_by_class(unreal.TopLevelAssetPath('/Script/Engine', cls)):
        p = str(d.package_name)
        if not p.startswith('/Game/'): continue
        low = p.split('/')[-1].lower()
        if 'hover' in low or 'click' in low or '/ui' in low or 'menu' in low and 'background' not in low:
            target = 'SC_UI'
        elif 'background' in low or 'music' in low or 'theme' in low:
            target = 'SC_Music'
        elif 'fire' in low or 'ambient' in low or 'wind' in low or 'bird' in low:
            target = 'SC_Ambient'
        else:
            target = 'SC_Effects'
        s = unreal.load_asset(p)
        try:
            s.set_editor_property('sound_class_object', classes[target])
            unreal.EditorAssetLibrary.save_loaded_asset(s, False)
            count[target] = count.get(target, 0) + 1
        except Exception as e:
            print('err', p, e)
print(count)
