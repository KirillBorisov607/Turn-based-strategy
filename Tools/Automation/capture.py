import unreal
# capture(loc, rot, out_name, w, h, fov)
def capture(loc, rot, name, w=1920, h=1080, fov=60.0):
    eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    rt = unreal.RenderingLibrary.create_render_target2d(world, w, h, unreal.TextureRenderTargetFormat.RTF_RGBA8)
    cap = eas.spawn_actor_from_class(unreal.SceneCapture2D, loc, rot)
    c = cap.capture_component2d
    c.set_editor_property('texture_target', rt)
    c.set_editor_property('fov_angle', fov)
    c.set_editor_property('capture_source', unreal.SceneCaptureSource.SCS_BASE_COLOR if globals().get('BASE') else unreal.SceneCaptureSource.SCS_FINAL_COLOR_LDR)
    c.capture_scene()
    unreal.RenderingLibrary.export_render_target(world, rt, 'C:/ProjectsUE/_logs', name)
    cap.destroy_actor()
