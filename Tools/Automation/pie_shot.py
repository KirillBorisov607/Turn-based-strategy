import unreal
ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
gw = ues.get_game_world()
print('game world', gw.get_name() if gw else None)
if gw:
    unreal.SystemLibrary.execute_console_command(gw, 'HighResShot 1280x720 filename=C:/ProjectsUE/_logs/game_slice_regress.png')
