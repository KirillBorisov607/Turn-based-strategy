import unreal
gw = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
cs = unreal.GameplayStatics.get_all_actors_of_class(gw, unreal.load_class(None, '/Game/Blueprints/Core/Combat/BP_CombatSystem.BP_CombatSystem_C'))[0]
for u in cs.get_editor_property('UnitsInCombat'):
    if u.get_editor_property('TeamIndex') != 0:
        u.call_method('ModifyCurrentHp', (-100, True))
        cs.call_method('ProcessUnitDeath', (u,))
print('enemies down, world', gw.get_name())
