import unreal
src='/Game/Maps/Slice/L_Slice_Hex'; dst='/Game/Maps/Generated/L_Generated'
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
les.load_level(src)
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
removed = {}; keep = {}
for a in eas.get_all_level_actors():
    cls = a.get_class().get_name()
    if cls in ('StaticMeshActor', 'BP_GridModifier_C', 'Actor') or cls.startswith('BP_Wall') or cls.startswith('BP_Pillar') or cls.startswith('BP_RandomCubes'):
        removed[cls] = removed.get(cls, 0) + 1
        a.destroy_actor()
    else:
        keep[cls] = keep.get(cls, 0) + 1
print('removed', removed)
print('kept', keep)
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
print('saved as', unreal.EditorLoadingAndSavingUtils.save_map(world, dst))
