import unreal
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
unreal.KeepersCaptureLibrary.view_from(world, unreal.Vector(-3600, -2400, 2300), unreal.Rotator(roll=0, pitch=-32, yaw=30), 60.0)
unreal.SystemLibrary.execute_console_command(world, 'HighResShot 1920x1080 filename=C:/ProjectsUE/_logs/load/T_Load_L_Generated.png')
