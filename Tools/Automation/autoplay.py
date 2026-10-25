# Hands every unit in the running PIE battle to the AI, so a whole battle plays by itself.
import unreal
gw = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
cs = unreal.GameplayStatics.get_all_actors_of_class(gw, unreal.load_class(None, '/Game/Blueprints/Core/Combat/BP_CombatSystem.BP_CombatSystem_C'))[0]
for u in cs.get_editor_property('UnitsInCombat'):
    if not u.get_editor_property('Ai'):
        cs.call_method('SetUnitUseAi', (u, True))
cur = cs.get_editor_property('CurrentUnit')
ai = cur.get_editor_property('Ai') if cur else None
print('current', cur.get_name() if cur else None, 'ai', ai.get_name() if ai else None)
if ai:
    ai.call_method('AiPlayTurn')
