import unreal, sys
level = open(r'C:/ProjectsUE/_tools/pie_level.txt').read().strip()
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
ok = les.load_level(level)
print('loaded', level, ok)
les.editor_request_begin_play()
print('PIE requested')
