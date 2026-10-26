import unreal
n = 0
for p in unreal.EditorAssetLibrary.list_assets('/Game/Art/Units', recursive=True):
    name = p.split('.')[-1]
    if not name.startswith('TX_Icon_'):
        continue
    tex = unreal.load_asset(p.split('.')[0])
    task = unreal.AssetExportTask()
    task.object = tex
    task.filename = 'C:/ProjectsUE/_logs/icons/%s.png' % name
    task.automated = True
    task.prompt = False
    task.replace_identical = True
    task.exporter = unreal.TextureExporterPNG()
    n += unreal.Exporter.run_asset_export_task(task)
    print(name)
print('exported', n)
