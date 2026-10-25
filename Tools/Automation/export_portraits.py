import unreal, time
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
ids = [('Warrior', False), ('Ranger', False), ('Priest', False), ('Bandit', True), ('BanditArcher', True), ('Brute', True)]
for i, e in ids:
    print(i, unreal.KeepersCaptureLibrary.export_portrait(world, i, e, 'C:/ProjectsUE/_logs/portraits/T_Portrait_%s.png' % i))
