import unreal
tasks = []
for name in ("Rooster", "Witch"):
    t = unreal.AssetImportTask()
    t.filename = "C:/ProjectsUE/_logs/card_icons/T_Card_%s.png" % name
    t.destination_path = "/Game/Keepers/Icons"
    t.replace_existing = True
    t.automated = True
    t.save = False
    tasks.append(t)
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)
for t in tasks:
    for p in t.imported_object_paths:
        tex = unreal.load_asset(p)
        tex.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI)
        tex.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
        unreal.EditorAssetLibrary.save_asset(p.split(".")[0], False)
        print("icon", p)
