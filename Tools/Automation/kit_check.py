import unreal
print(unreal.KeepersEditorLibrary.check_battle_level())
w = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
grid = [a for a in unreal.GameplayStatics.get_all_actors_of_class(w, unreal.Actor) if a.get_class().get_name()=='BP_Grid_C'][0]
print('bl', grid.get_editor_property('GridBottomLeftCornerLocation'), grid.get_editor_property('GridCenterLocation'), grid.get_editor_property('GridTileSize'))
sc = [a for a in unreal.GameplayStatics.get_all_actors_of_class(w, unreal.Actor) if a.get_class().get_name()=='BP_CombatScenario_C'][0]
print(sc.get_editor_property('GridData').export_text())
