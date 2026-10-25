import unreal
gw = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
cs = unreal.GameplayStatics.get_all_actors_of_class(gw, unreal.load_class(None, '/Game/Blueprints/Core/Combat/BP_CombatSystem.BP_CombatSystem_C'))[0]
unreal.KeepersCombatLibrary.end_player_turn(cs)
cur = cs.get_editor_property('CurrentUnit')
print('after end turn current', cur.get_name()[8:], 'team', cur.get_editor_property('TeamIndex'))
