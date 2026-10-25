"""Plays with real Slate input: camera wheel/WASD, then clicks a tile next to the current hero to move it.
Step is read from C:/ProjectsUE/_tools/input_step.txt: camera_before | wheel | key_w_down | key_w_up | camera_after | click_move | check_move"""
import unreal

gw = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
step = open(r'C:/ProjectsUE/_tools/input_step.txt').read().strip()
sim = unreal.KeepersInputSimulator
pawn = unreal.GameplayStatics.get_player_pawn(gw, 0)


def camera():
    arm = pawn.get_component_by_class(unreal.SpringArmComponent) if pawn else None
    return 'pawn %s arm %s' % (pawn.get_actor_location() if pawn else None, arm.get_editor_property('target_arm_length') if arm else None)


cs = unreal.GameplayStatics.get_all_actors_of_class(gw, unreal.load_class(None, '/Game/Blueprints/Core/Combat/BP_CombatSystem.BP_CombatSystem_C'))
cs = cs[0] if cs else None

if step == 'camera':
    print(camera())
elif step == 'wheel':
    print('wheel', sim.scroll_wheel(gw, -3.0))
elif step == 'w_down':
    print('W down', sim.set_key('W', True))
elif step == 'w_up':
    print('W up', sim.set_key('W', False))
elif step == 'focus':
    print('click empty sky', sim.click_viewport(gw, unreal.Vector2D(0.05, 0.95), False))
elif step == 'click_move':
    unit = cs.get_editor_property('CurrentUnit')
    tiles = cs.get_editor_property('Grid').get_editor_property('GridTiles')
    i = unit.get_editor_property('Index')
    occupied = set((u.get_editor_property('Index').x, u.get_editor_property('Index').y) for u in cs.get_editor_property('UnitsInCombat'))
    target = None
    for dx, dy in [(1, 0), (0, 1), (-1, 0), (0, -1), (2, 0), (0, 2)]:
        k = unreal.IntPoint(i.x + dx, i.y + dy)
        if k in tiles and (k.x, k.y) not in occupied and 'NORMAL' in str(tiles[k].get_editor_property('Type')):
            target = k
            break
    loc = tiles[target].get_editor_property('Transform').translation
    open(r'C:/ProjectsUE/_tools/move_target.txt', 'w').write('%s %d %d' % (unit.get_name(), target.x, target.y))
    print('unit', unit.get_name()[8:], 'at', (i.x, i.y), '-> click', (target.x, target.y), sim.click_world_location(gw, loc, False))
elif step == 'check_move':
    name, tx, ty = open(r'C:/ProjectsUE/_tools/move_target.txt').read().split()
    unit = [u for u in cs.get_editor_property('UnitsInCombat') if u.get_name() == name][0]
    i = unit.get_editor_property('Index')
    print('unit now at', (i.x, i.y), 'target', (int(tx), int(ty)))
    pc = unreal.GameplayStatics.get_player_controller(gw, 0)
    print('show cursor', pc.get_editor_property('show_mouse_cursor'))
elif step in ('hover_move', 'click_target'):
    name, tx, ty = open(r'C:/ProjectsUE/_tools/move_target.txt').read().split()
    tiles = cs.get_editor_property('Grid').get_editor_property('GridTiles')
    loc = tiles[unreal.IntPoint(int(tx), int(ty))].get_editor_property('Transform').translation
    pc = unreal.GameplayStatics.get_player_controller(gw, 0)
    ok, pix = pc.project_world_location_to_screen(loc) if False else (True, None)
    if step == 'hover_move':
        res = pc.project_world_location_to_screen(loc)
        size = pc.get_viewport_size()
        print('project', res, 'size', size)
        px = res[0] if isinstance(res, tuple) else res
        frac = unreal.Vector2D(px.x / size[0], px.y / size[1])
        print('hover', sim.move_mouse(gw, frac))
    else:
        print('click', sim.click_world_location(gw, loc, False))
elif step == 'hovered':
    pa = unreal.GameplayStatics.get_all_actors_of_class(gw, unreal.load_class(None, '/Game/Blueprints/Core/Player/BP_PlayerActions.BP_PlayerActions_C'))[0]
    h = pa.get_editor_property('HoveredTile')
    act = pa.get_editor_property('SelectedAction_LeftClick')
    print('hovered tile', (h.x, h.y), 'left action', act.get_class().get_name() if act else None, 'selected unit', pa.get_editor_property('SelectedUnit').get_name()[8:] if pa.get_editor_property('SelectedUnit') else None)
elif step == 'wheel_out':
    for _ in range(30):
        sim.scroll_wheel(gw, -3.0)
    print('wheel out x30')
elif step == 'mouse':
    pc = unreal.GameplayStatics.get_player_controller(gw, 0)
    print('pc mouse', pc.get_mouse_position(), 'viewport', pc.get_viewport_size())
    vp = unreal.WidgetLayoutLibrary.get_mouse_position_on_viewport(gw)
    print('widget-layout mouse', vp)
elif step == 'click_other_hero':
    cur = cs.get_editor_property('CurrentUnit')
    others = [u for u in cs.get_editor_property('UnitsInCombat') if u.get_editor_property('TeamIndex') == 0 and u != cur and not u.get_editor_property('Ai')]
    o = others[-1]
    print('current', cur.get_name()[8:], '-> click', o.get_name()[8:], sim.click_world_location(gw, o.get_actor_location(), False))
elif step == 'current':
    print('current now', cs.get_editor_property('CurrentUnit').get_name()[8:])
