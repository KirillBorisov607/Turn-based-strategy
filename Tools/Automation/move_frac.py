import unreal
x, y = map(float, open(r'C:/ProjectsUE/_tools/frac.txt').read().split()[:2])
gw = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
print(unreal.KeepersInputSimulator.move_mouse(gw, unreal.Vector2D(x, y)))
