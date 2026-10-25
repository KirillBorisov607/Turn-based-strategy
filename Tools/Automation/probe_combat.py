import unreal
gw = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
cs = unreal.GameplayStatics.get_all_actors_of_class(gw, unreal.load_class(None, '/Game/Blueprints/Core/Combat/BP_CombatSystem.BP_CombatSystem_C'))[0]
cur = cs.get_editor_property('CurrentUnit')
print('in combat', cs.get_editor_property('IsInCombat'), 'current', cur.get_name()[8:] if cur else None, 'winner', cs.get_editor_property('TeamIndexThatWonLastCombat'))
rows = []
for u in unreal.GameplayStatics.get_all_actors_of_class(gw, unreal.load_class(None, '/Game/Blueprints/Core/Units/BP_Unit.BP_Unit_C')):
    txt = u.get_editor_property('UnitData').export_text()
    import re
    hp = re.search(r'HP_Current_\w+=(-?\d+)', txt)
    rows.append('%s hp=%s' % (u.get_name()[8:], hp.group(1) if hp else '?'))
print(' | '.join(rows))
