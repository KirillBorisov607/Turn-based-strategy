"""BP_UnitAi.CalculateSpellScoreAndUpdateSpellCasting: the plan's score goes through
KeepersAiLibrary.AdjustPlanScore (terrain, spread of threats, distance, finishing blows)."""
import sys
from bpedit import add_call, call, find, link, nodes, unlink

sys.stdout.reconfigure(encoding="utf-8")
A = "/Game/Blueprints/Core/Units/AI/BP_UnitAi"
G = "CalculateSpellScoreAndUpdateSpellCasting"
g = nodes(A, G)
if any("Adjust Plan Score" in n["name"] for n in g):
    print("already patched"); sys.exit(0)
total = find(g, "Get TotalSpellScore", 1)   # the one feeding "float > float"
compare = find(g, "float > float")
make = find(g, "Make S Unit Ai Spell Casting Data")
unit = find(g, "Get Unit", 0)
origin = [n for n in g if n["name"].startswith("Get Origin")][-1]
target = [n for n in g if n["name"].startswith("Get Target")][-1]
assert any(c["nodeId"] == compare["id"] for p in total["pins"] for c in p["connections"]), total
r = add_call(A, G, "AdjustPlanScore", "KeepersAiLibrary", compare["posX"] - 300, compare["posY"] + 160)
adj = r.get("nodeId") or r.get("node_id") or r.get("id")
score_pin = next(p["name"] for p in make["pins"] if p["name"].startswith("SpellScore"))
unlink(A, G, compare, "A")
unlink(A, G, make, score_pin)
print(link(A, G, unit, "Unit", adj, "Unit"))
print(link(A, G, origin, "Origin", adj, "Origin"))
print(link(A, G, target, "Target", adj, "Target"))
print(link(A, G, total, "TotalSpellScore", adj, "BaseScore"))
print(link(A, G, adj, "ReturnValue", compare, "A"))
print(link(A, G, adj, "ReturnValue", make, score_pin))
print(call("compile_blueprint", asset_path=A))
