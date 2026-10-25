import unreal
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
scene = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.KeepersMenuScene)[0]
scene.open_squad_screen(False)
print('opened')
