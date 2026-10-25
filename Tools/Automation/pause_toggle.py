import unreal
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
unreal.KeepersPauseSubsystem.toggle_pause_menu(world)
print('pause')
