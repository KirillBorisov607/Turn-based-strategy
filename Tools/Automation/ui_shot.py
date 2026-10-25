import unreal
name = open(r'C:/ProjectsUE/_tools/ui_shot.txt').read().strip()
gw = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
unreal.SystemLibrary.execute_console_command(gw, 'HighResShot 1280x720 filename=C:/ProjectsUE/_logs/g_%s.png' % name)
print('ui', unreal.KeepersInputSimulator.save_viewport_ui(gw, 'C:/ProjectsUE/_logs/u_%s.png' % name, 1280, 720))
