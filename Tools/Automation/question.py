import unreal
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
unreal.KeepersPauseSubsystem.test_question(world, True)
