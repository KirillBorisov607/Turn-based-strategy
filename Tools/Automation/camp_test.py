import unreal
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
a = open(r'C:/ProjectsUE/_tools/camp_arg.txt').read().split()
unreal.KeepersCampaignWorldSubsystem.test_camp(world, a[0], int(a[1]))
print('camp', a)
