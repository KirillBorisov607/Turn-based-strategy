import unreal
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
name = open(r'C:/ProjectsUE/_tools/gen_name.txt').read().strip()
lo = unreal.Vector(1e9, 1e9, 1e9); hi = unreal.Vector(-1e9, -1e9, -1e9)
for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor):
    if isinstance(a, unreal.StaticMeshActor):
        m = a.static_mesh_component.static_mesh
        if m and m.get_name().startswith('SM_Hexagon_Platform'):
            p = a.get_actor_location()
            lo = unreal.Vector(min(lo.x, p.x), min(lo.y, p.y), 0); hi = unreal.Vector(max(hi.x, p.x), max(hi.y, p.y), 0)
print(lo, hi)
cx = (lo.x + hi.x) / 2; cy = (lo.y + hi.y) / 2
unreal.KeepersCaptureLibrary.view_from(world, unreal.Vector(cx - 3400, cy, 7600), unreal.Rotator(roll=0, pitch=-66, yaw=0), 60.0)
unreal.SystemLibrary.execute_console_command(world, 'HighResShot 1600x900 filename=C:/ProjectsUE/_logs/gen_top_%s.png' % name)
