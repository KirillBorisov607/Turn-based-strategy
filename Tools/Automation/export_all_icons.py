import unreal
reg = unreal.EditorAssetLibrary
paths = [p for p in reg.list_assets('/Game/Art/Textures/SpellIcons', recursive=False)] + [p for p in reg.list_assets('/Game/Keepers/Portraits', recursive=False)]
n = 0
for p in paths:
    tex = unreal.load_asset(p.split('.')[0])
    if not isinstance(tex, unreal.Texture2D):
        continue
    task = unreal.AssetExportTask()
    task.object = tex
    task.filename = 'C:/ProjectsUE/_logs/icons/%s.png' % tex.get_name()
    task.automated = True
    task.prompt = False
    task.replace_identical = True
    task.exporter = unreal.TextureExporterPNG()
    n += unreal.Exporter.run_asset_export_task(task)
print('exported', n)
