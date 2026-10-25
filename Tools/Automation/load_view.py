import unreal, math
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
chars = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Character)
pts = [c.get_actor_location() for c in chars]
cx = sum(p.x for p in pts)/len(pts); cy = sum(p.y for p in pts)/len(pts); cz = sum(p.z for p in pts)/len(pts)
yaw, dist, h, pitch = [float(x) for x in open(r'C:/ProjectsUE/_tools/load_yaw.txt').read().split()]
r = math.radians(yaw)
loc = unreal.Vector(cx - math.cos(r)*dist, cy - math.sin(r)*dist, cz + h)
unreal.KeepersCaptureLibrary.view_from(world, loc, unreal.Rotator(roll=0, pitch=pitch, yaw=yaw), 58.0)
# hide the HUD widgets for the shot
for w in unreal.WidgetBlueprintLibrary.get_all_widgets_of_class(world, unreal.UserWidget, False) if hasattr(unreal,'WidgetBlueprintLibrary') else []:
    pass
name = open(r'C:/ProjectsUE/_tools/load_name.txt').read().strip()
unreal.SystemLibrary.execute_console_command(world, 'HighResShot 1920x1080 filename=C:/ProjectsUE/_logs/load/T_Load_%s.png' % name)
print('shot', name)
