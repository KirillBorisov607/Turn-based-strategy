import unreal
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
scene = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.KeepersMenuScene)[0]
arg = open(r'C:/ProjectsUE/_tools/squad_arg.txt').read().split()
if arg[0] == 'open':
    scene.open_squad_screen(True)
else:
    scene.test_squad(arg[1], int(arg[2]))
print('ok')
