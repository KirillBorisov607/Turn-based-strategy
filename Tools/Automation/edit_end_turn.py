"""W_Combat: the End Turn button ends the whole player phase (UKeepersCombatLibrary::EndPlayerTurn)."""
import sys
from bpedit import add_call, call, find, link, nodes, unlink

sys.stdout.reconfigure(encoding="utf-8")
ASSET = "/Game/Blueprints/Core/Combat/Widgets/W_Combat"
GRAPH = "EventGraph"

graph = nodes(ASSET, GRAPH)
if any("End Player Turn" in n["name"] for n in graph):
    print("already patched")
    sys.exit(0)

clicked = find(graph, "On Clicked (Button_EndTurn)")
next_unit = next(n for n in graph if n["name"].startswith("Next Unit")
                 and any(c["nodeId"] == clicked["id"] for p in n["pins"] if p["name"] == "execute" for c in p["connections"]))
combat_get = next(c for p in next_unit["pins"] if p["name"] == "self" for c in p["connections"])

added = add_call(ASSET, GRAPH, "EndPlayerTurn", "KeepersCombatLibrary", next_unit["posX"], next_unit["posY"] + 120)
end_turn = added.get("nodeId")
unlink(ASSET, GRAPH, clicked, "then")
print(link(ASSET, GRAPH, clicked, "then", end_turn, "execute"))
print(link(ASSET, GRAPH, combat_get["nodeId"], combat_get["pinName"], end_turn, "CombatSystem"))
print(call("remove_node", asset_path=ASSET, graph_name=GRAPH, node_id=next_unit["id"]))
print(call("compile_blueprint", asset_path=ASSET))
