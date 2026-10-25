import unreal
x, y, down = open(r'C:/ProjectsUE/_tools/frac.txt').read().split()
gw = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
print(down, unreal.KeepersInputSimulator.set_mouse_button(gw, unreal.Vector2D(float(x), float(y)), down == 'down'), gw.get_name())
