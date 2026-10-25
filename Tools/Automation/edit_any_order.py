"""Heroes in any order: Action_Cbt_Move.ExecuteAction first tries to hand control to a clicked hero."""
import sys
from bpedit import add_call, call, find, link, nodes, unlink

sys.stdout.reconfigure(encoding="utf-8")
ASSET = "/Game/Blueprints/Core/Player/Actions/Combat/Action_Cbt_Move"
GRAPH = "ExecuteAction"


def node_id(result):
    return result.get("nodeId") or result.get("node_id") or result.get("id")


graph = nodes(ASSET, GRAPH)
if any("Try Switch to Unit At" in n["name"] for n in graph):
    print("already patched")
    sys.exit(0)

entry = next(n for n in graph if n["class"] == "K2Node_FunctionEntry")
parent_call = next(n for n in graph if n["class"] == "K2Node_CallParentFunction")
then_target = next(p for p in parent_call["pins"] if p["name"] == "then")["connections"][0]

switch = node_id(add_call(ASSET, GRAPH, "TrySwitchToUnitAt", "KeepersCombatLibrary", parent_call["posX"] + 260, parent_call["posY"] + 180))
branch = node_id(call("add_branch_node", asset_path=ASSET, graph_name=GRAPH,
                      node_x=parent_call["posX"] + 560, node_y=parent_call["posY"] + 180))
unlink(ASSET, GRAPH, parent_call, "then")
print(link(ASSET, GRAPH, parent_call, "then", switch, "execute"))
print(link(ASSET, GRAPH, entry, "Index", switch, "Index"))
print(link(ASSET, GRAPH, switch, "then", branch, "execute"))
print(link(ASSET, GRAPH, switch, "ReturnValue", branch, "Condition"))
print(link(ASSET, GRAPH, branch, "else", then_target["nodeId"], then_target["pinName"]))
print(call("compile_blueprint", asset_path=ASSET))
