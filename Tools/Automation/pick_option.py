import unreal
n = int(open(r'C:/ProjectsUE/_tools/pick.txt').read())
cards = [w for w in unreal.ObjectIterator(unreal.KeepersOptionButton) if w.get_world() and w.is_in_viewport() is False and w.get_outer() is not None]
cards = [w for w in unreal.ObjectIterator(unreal.KeepersOptionButton)]
gw = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
cards = [w for w in cards if w.get_world() == gw]
print('cards', len(cards))
cards[n].call_method('HandleClicked')
