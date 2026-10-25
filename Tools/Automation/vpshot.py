import unreal
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
name = open(r'C:/ProjectsUE/_tools/ui_shot.txt').read().strip()
print(unreal.KeepersCaptureLibrary.capture_viewport(world, 'C:/ProjectsUE/_logs/v_%s.png' % name))
