import unreal
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
name = open(r'C:/ProjectsUE/_tools/load_name.txt').read().strip()
unreal.SystemLibrary.execute_console_command(world, 'HighResShot 1920x1080 filename=C:/ProjectsUE/_logs/load/T_Load_%s.png' % name)
