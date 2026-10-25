import unreal
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
scene = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.KeepersMenuScene)[0]
a = open(r'C:/ProjectsUE/_tools/look_arg.txt').read().split()
scene.test_look(int(a[0]), a[1] == '1')
print('look', a)
