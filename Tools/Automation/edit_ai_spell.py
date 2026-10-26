"""BP_UnitAi: the plan's spell goes into KeepersAiLibrary.AdjustPlanScore (pin Spell)."""
import sys
from bpedit import call, find, link, nodes

sys.stdout.reconfigure(encoding="utf-8")
A = "/Game/Blueprints/Core/Units/AI/BP_UnitAi"
G = "CalculateSpellScoreAndUpdateSpellCasting"
print(call("compile_blueprint", asset_path=A))
g = nodes(A, G)
adj = find(g, "Adjust Plan Score")
print([p["name"] for p in adj["pins"]])
brk = find(g, "Break S Spell Data")
spell_pin = next(p["name"] for p in brk["pins"] if p["name"].startswith("Spell_"))
print(link(A, G, brk, spell_pin, adj, "Spell"))
print(call("compile_blueprint", asset_path=A))
