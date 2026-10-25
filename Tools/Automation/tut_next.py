import unreal
gw = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
w = [x for x in unreal.ObjectIterator(unreal.KeepersTutorialWidget) if x.get_world() == gw][0]
w.call_method('HandleNext')
print('next')
