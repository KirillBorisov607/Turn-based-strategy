"""Roots: after the spell hits, the target's MP is set to exactly 0 (the -10 modifier would show negative MP)."""
import os

ROOT = r"C:\ProjectsUE\Turn-based-strategy\The_battle_code\Plugins\KeepersGame\Source\KeepersGame"


def patch(rel, old, new):
    path = os.path.join(ROOT, rel)
    text = open(path, encoding="utf-8-sig").read()
    if new in text:
        return
    assert old in text, f"{rel}: anchor not found: {old[:60]}"
    open(path, "w", encoding="utf-8").write(text.replace(old, new, 1))


patch("Public/KeepersTerrainSubsystem.h", """	Push,
	Sprout
};""", """	Push,
	Sprout,
	Roots
};""")
patch("Public/KeepersTerrainSubsystem.h", """ *    blocks line of sight; an occupied tile grows a bush.
 */""", """ *    blocks line of sight; an occupied tile grows a bush.
 *  - BP_SpellAnim_Roots: the target's MP (already cut by the spell) is set to exactly 0 for its next turn.
 */""")
patch("Private/KeepersTerrainSubsystem.cpp", """	static const FString SproutAnimation = TEXT("BP_SpellAnim_Sprout");""", """	static const FString SproutAnimation = TEXT("BP_SpellAnim_Sprout");
	static const FString RootsAnimation = TEXT("BP_SpellAnim_Roots");""")
patch("Private/KeepersTerrainSubsystem.cpp", """	else if (ClassName.StartsWith(KeepersTerrain::SproutAnimation))
	{
		PendingEffects.Add(Actor, EKeepersTerrainEffect::Sprout);
	}""", """	else if (ClassName.StartsWith(KeepersTerrain::SproutAnimation))
	{
		PendingEffects.Add(Actor, EKeepersTerrainEffect::Sprout);
	}
	else if (ClassName.StartsWith(KeepersTerrain::RootsAnimation))
	{
		PendingEffects.Add(Actor, EKeepersTerrainEffect::Roots);
	}""")
patch("Private/KeepersTerrainSubsystem.cpp", """	case EKeepersTerrainEffect::Sprout:""", """	case EKeepersTerrainEffect::Roots:
		if (UObject* Rooted = KeepersTerrain::GetUnitOnTile(Grid, Target))
		{
			KeepersReflection::FCall SetMp(Rooted, TEXT("SetCurrentMp"));
			SetMp.Set<int32>(TEXT("Mp"), 0);
			SetMp.Set<bool>(TEXT("ShowNumber"), false);
			SetMp.Call();
		}
		break;
	case EKeepersTerrainEffect::Sprout:""")
print("patched")
