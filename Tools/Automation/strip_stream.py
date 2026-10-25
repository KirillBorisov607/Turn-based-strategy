import unreal
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
if world.get_name() != 'L_Generated':
    les.load_level('/Game/Maps/Generated/L_Generated')
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
for l in unreal.EditorLevelUtils.get_levels(world):
    if l.get_outer().get_name() == 'L_Hexagon':
        print('remove', unreal.EditorLevelUtils.remove_level_from_world(l))
print('save', les.save_current_level())
print([l.get_outer().get_name() for l in unreal.EditorLevelUtils.get_levels(world)])
