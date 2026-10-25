"""Enemy intents in BP_UnitAi.AiFlow.

1. Event_RunAiLogic -> RestoreIntent -> Branch: has an intent -> attack now (Cast Spell state),
   otherwise the usual Start Turn.
2. MoveToSpellCastingLocation_Done -> SaveIntent -> End Turn (no attack this turn).
3. CastSpell_Done -> PrepareToPlanNextIntent -> back to Decide Which Spell to Cast.
"""
import sys
from bpedit import add_call, call, find, link, nodes, unlink

sys.stdout.reconfigure(encoding="utf-8")
ASSET = "/Game/Blueprints/Core/Units/AI/BP_UnitAi"
GRAPH = "AiFlow"
LIB = "KeepersCombatLibrary"


def node_id(result):
    return result.get("nodeId") or result.get("node_id") or result.get("id")


graph = nodes(ASSET, GRAPH)
if any("Restore Intent" in n["name"] for n in graph):
    print("already patched")
    sys.exit(0)

run_logic = find(graph, "Event_RunAiLogic")
move_done = find(graph, "MoveToSpellCastingLocation_Done")
cast_done = find(graph, "CastSpell_Done")


def state_node_after(source, pin_name="then"):
    target = next(p for p in source["pins"] if p["name"] == pin_name)["connections"][0]
    return target["nodeId"], target["pinName"]


start_turn_id, start_turn_pin = state_node_after(run_logic)
cast_state_id, cast_state_pin = state_node_after(move_done)
decide_knot_id, decide_knot_pin = state_node_after(cast_done)
end_turn = next(n for n in graph if n["name"].startswith("Set Ai State with Delay")
                and any(c["nodeId"] == find(graph, "MoveToEndTurnLocation_Done")["id"]
                        for p in n["pins"] if p["name"] == "execute" for c in p["connections"]))

# 1. Restore at turn start
restore = node_id(add_call(ASSET, GRAPH, "RestoreIntent", LIB, run_logic["posX"] + 250, run_logic["posY"] - 220))
branch = node_id(call("add_branch_node", asset_path=ASSET, graph_name=GRAPH,
                      node_x=run_logic["posX"] + 560, node_y=run_logic["posY"] - 220))
unlink(ASSET, GRAPH, run_logic, "then")
print(link(ASSET, GRAPH, run_logic, "then", restore, "execute"))
print(link(ASSET, GRAPH, restore, "then", branch, "execute"))
print(link(ASSET, GRAPH, restore, "ReturnValue", branch, "Condition"))
print(link(ASSET, GRAPH, branch, "then", cast_state_id, cast_state_pin))
print(link(ASSET, GRAPH, branch, "else", start_turn_id, start_turn_pin))

# 2. Save instead of attacking
save = node_id(add_call(ASSET, GRAPH, "SaveIntent", LIB, move_done["posX"] + 250, move_done["posY"] + 140))
unlink(ASSET, GRAPH, move_done, "then")
print(link(ASSET, GRAPH, move_done, "then", save, "execute"))
print(link(ASSET, GRAPH, save, "then", end_turn, "execute"))

# 3. Refill AP after attacking so the next intent can be planned
prepare = node_id(add_call(ASSET, GRAPH, "PrepareToPlanNextIntent", LIB, cast_done["posX"] + 250, cast_done["posY"] + 140))
unlink(ASSET, GRAPH, cast_done, "then")
print(link(ASSET, GRAPH, cast_done, "then", prepare, "execute"))
print(link(ASSET, GRAPH, prepare, "then", decide_knot_id, decide_knot_pin))

print(call("compile_blueprint", asset_path=ASSET))
