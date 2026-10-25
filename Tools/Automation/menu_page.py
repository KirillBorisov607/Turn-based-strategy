import unreal
page = int(open(r'C:/ProjectsUE/_tools/menu_page.txt').read())
gw = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
unreal.GameplayStatics.get_all_actors_of_class(gw, unreal.KeepersMenuScene)[0].show_menu_page(page)
print('page', page)
