"""L_MainMenu: replace BP_MainMenu (old widget + cycling grey levels) with AKeepersMenuScene."""
import unreal

les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
les.load_level('/Game/Maps/L_MainMenu')
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()

removed = 0
has_scene = False
for actor in eas.get_all_level_actors():
    name = actor.get_class().get_name()
    if name.startswith('BP_MainMenu'):
        eas.destroy_actor(actor)
        removed += 1
    elif name == 'KeepersMenuScene':
        has_scene = True
print('removed', removed)
if not has_scene:
    scene = eas.spawn_actor_from_class(unreal.KeepersMenuScene, unreal.Vector(0, 0, 0))
    print('spawned', scene.get_name())
print('saved', unreal.EditorLoadingAndSavingUtils.save_map(world, '/Game/Maps/L_MainMenu'))
