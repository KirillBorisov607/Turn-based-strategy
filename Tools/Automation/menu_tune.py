import unreal, json
cfg = json.load(open(r'C:/ProjectsUE/_tools/menu_tune.json'))
gw = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
scene = unreal.GameplayStatics.get_all_actors_of_class(gw, unreal.KeepersMenuScene)[0]
if 'locs' in cfg:
    scene.set_editor_property('shot_locations', [unreal.Vector(*v) for v in cfg['locs']])
    scene.set_editor_property('shot_targets', [unreal.Vector(*v) for v in cfg['targets']])
print('ok', scene.get_editor_property('shot_locations')[0])
