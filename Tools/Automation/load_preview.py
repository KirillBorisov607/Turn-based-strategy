import unreal
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
unreal.KeepersLoadingSubsystem.preview_loading_screen(world, open(r'C:/ProjectsUE/_tools/load_level.txt').read().strip(), 30.0)
print('preview')
