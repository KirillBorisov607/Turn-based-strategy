import unreal
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor):
    figs = a.get_components_by_class(unreal.KeepersFigureComponent)
    for f in figs:
        print(a.get_name(), a.get_class().get_name(), f.get_world_location(), f.is_visible())
    if 'Unit' in a.get_class().get_name():
        m = a.get_components_by_class(unreal.SkeletalMeshComponent)
        print('  unit', a.get_name(), a.get_actor_location(), [ (x.get_name(), x.is_visible()) for x in m][:3])
