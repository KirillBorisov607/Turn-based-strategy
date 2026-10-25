import unreal
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
scene = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.KeepersMenuScene)[0]
for c in scene.get_components_by_class(unreal.KeepersFigureComponent):
    print(c.get_name(), c.get_world_location(), c.get_world_rotation(), c.is_visible())
cam = scene.get_components_by_class(unreal.CameraComponent)[0]
print('cam', cam.get_world_location(), cam.get_world_rotation())
