"""Player phase then enemy phase: sort UnitsInCombat by team at the start of combat."""
import sys
from bpedit import add_call, call, find, link, nodes, unlink

sys.stdout.reconfigure(encoding="utf-8")
ASSET = "/Game/Blueprints/Core/Combat/BP_CombatSystem"
GRAPH = "StartCombat"

graph = nodes(ASSET, GRAPH)
if any("Sort Units in Combat by Team" in n["name"] for n in graph):
    print("already patched")
    sys.exit(0)

set_starter = find(graph, "Set ObjectThatStartedCombat")
set_in_combat = find(graph, "Set IsInCombat")

added = add_call(ASSET, GRAPH, "SortUnitsInCombatByTeam", "KeepersCombatLibrary",
                 set_starter["posX"] + 120, set_starter["posY"] + 160)
print("added", added)
sort_id = added.get("nodeId") or added.get("node_id") or added.get("id")

unlink(ASSET, GRAPH, set_starter, "then")
print(link(ASSET, GRAPH, set_starter, "then", sort_id, "execute"))
print(link(ASSET, GRAPH, sort_id, "then", set_in_combat, "execute"))
print(call("compile_blueprint", asset_path=ASSET))
