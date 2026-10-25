"""L_Slice_Chicken: the slice battle plus a friendly chicken to protect and a 5-round survival goal."""
import unreal

les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)

dst = '/Game/Maps/Slice/L_Slice_Chicken'
if not unreal.EditorAssetLibrary.does_asset_exist(dst):
    les.load_level('/Game/Maps/Slice/L_Slice_Hex')
    print('saved copy', unreal.EditorLoadingAndSavingUtils.save_map(ues.get_editor_world(), dst))
les.load_level(dst)

actors = eas.get_all_level_actors()
units = [a for a in actors if a.get_class().get_name() == 'BP_Unit_C']
if not any('Chicken' in a.get_actor_label() for a in units):
    grid = [a for a in actors if a.get_class().get_name() == 'BP_Grid_C'][0]
    tiles = grid.get_editor_property('GridTiles')
    warrior = [a for a in units if 'Warrior' in a.get_actor_label()][0]
    wloc = warrior.get_actor_location()
    occupied = [a.get_actor_location() for a in units]

    # Tile types are traced from the environment at game start, so pick a neighbouring tile at the warrior's height.
    def dist2d(a, b):
        return ((a.x - b.x) ** 2 + (a.y - b.y) ** 2) ** 0.5

    best = None
    for k in tiles:
        loc = tiles[k].get_editor_property('Transform').translation
        d = dist2d(loc, wloc)
        if 150 < d < 320 and abs(loc.z - (wloc.z - 100)) < 60 and all(dist2d(loc, o) > 150 for o in occupied):
            if best is None or d < best[0]:
                best = (d, loc)
    print('spot', best)

    unit_class = unreal.load_class(None, '/Game/Blueprints/Core/Units/BP_Unit.BP_Unit_C')
    chicken = eas.spawn_actor_from_class(unit_class, best[1] + unreal.Vector(0, 0, 100), unreal.Rotator(0, 0, 0))
    chicken.set_actor_label('BP_Unit_Team0_Chicken')
    chicken.set_editor_property('UnitType', type(warrior.get_editor_property('UnitType')).CHICKEN)

    scen = [a for a in actors if a.get_class().get_name() == 'BP_CombatScenario_C'][0]
    scen.call_method('BuildTeamsBasedOnUnitNames')
    scen.call_method('BuildAiListBasedOnTeamIndexes')
    scen.call_method('BuildDesiredGridIndexesBasedOnUnitLocations')

    objective = eas.spawn_actor_from_class(unreal.KeepersBattleObjective, unreal.Vector(0, 0, 0), unreal.Rotator(0, 0, 0))
    objective.set_actor_label('BattleObjective')
    objective.set_editor_property('SurviveTurns', 5)
    objective.set_editor_property('ProtectedUnit', chicken)
    objective.set_editor_property('ProtectedName', 'курицу Пеструшку')

print('saved', les.save_current_level())
