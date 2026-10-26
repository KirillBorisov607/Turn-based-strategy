import unreal, glob, os
tasks = []
for f in sorted(glob.glob(r'C:/ProjectsUE/_logs/icons/T_Portrait_[MRB][ao]*.png')):
    if 'test' in f: continue
    t = unreal.AssetImportTask()
    t.filename = f
    t.destination_path = '/Game/Keepers/Portraits'
    t.replace_existing = True
    t.automated = True
    t.save = False
    tasks.append(t)
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)
for t in tasks:
    for p in t.imported_object_paths:
        tex = unreal.load_asset(p)
        tex.set_editor_property('lod_group', unreal.TextureGroup.TEXTUREGROUP_UI)
        tex.set_editor_property('mip_gen_settings', unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
        tex.set_editor_property('srgb', True)
        unreal.EditorAssetLibrary.save_asset(p.split('.')[0], False)
        print(p, tex.blueprint_get_size_x(), tex.blueprint_get_size_y())
