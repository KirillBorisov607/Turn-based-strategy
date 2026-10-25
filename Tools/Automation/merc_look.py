import unreal, math
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
units = [a for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Character) if a.get_name().startswith('BP_Unit_C_')]
pts = [u.get_actor_location() for u in units]
cx = sum(p.x for p in pts)/len(pts); cy = sum(p.y for p in pts)/len(pts); cz = sum(p.z for p in pts)/len(pts)
unreal.KeepersCaptureLibrary.view_from(world, unreal.Vector(cx-520, cy-520, cz+420), unreal.Rotator(roll=0, pitch=-30, yaw=45), 55.0)
unreal.SystemLibrary.execute_console_command(world, 'HighResShot 1280x720 filename=C:/ProjectsUE/_logs/merc_close.png')
