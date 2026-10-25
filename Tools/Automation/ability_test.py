import unreal
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
unreal.KeepersBattleHudSubsystem.test_ability(world, int(open(r'C:/ProjectsUE/_tools/ability_arg.txt').read()))
