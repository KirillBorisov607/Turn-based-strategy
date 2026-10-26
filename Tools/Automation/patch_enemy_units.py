"""Cavalry, catapults, rams and the chicken shaman in C++: figures from the pack's prebuilt meshes, enemy defs,
spell effects and the AI that raises rocks and grows forest."""
import os
os.chdir(r'C:/ProjectsUE/Turn-based-strategy/The_battle_code/Plugins/KeepersGame/Source/KeepersGame')


def patch(p, pairs):
    s = open(p, 'rb').read().decode('utf-8-sig')
    crlf = '\r\n' in s
    s = s.replace('\r\n', '\n')
    for a, b in pairs:
        assert s.count(a) == 1, (p, a[:80])
        s = s.replace(a, b)
    open(p, 'w', encoding='utf-8', newline='\r\n' if crlf else '\n').write(s)


patch('Public/KeepersFigure.h', [
('''	Staff      // has cast animations
};''', '''	Staff,     // has cast animations
	// Prebuilt meshes of their own skeletons (Body names the mesh, no head or weapons).
	Cavalry,
	Catapult,
	Ram
};'''),
])
patch('Private/KeepersFigure.cpp', [
('''		default:                         OutFolder = TEXT("Infantry");  OutPrefix = TEXT("infantry"); break;
		}
	}
}''', '''		default:                         OutFolder = TEXT("Infantry");  OutPrefix = TEXT("infantry"); break;
		}
	}

	static bool IsPrebuilt(EKeepersAnimSet Set)
	{
		return Set == EKeepersAnimSet::Cavalry || Set == EKeepersAnimSet::Catapult || Set == EKeepersAnimSet::Ram;
	}

	/** Riders and machines: their own folders and shorter animation lists. */
	static UAnimSequence* FindPrebuiltAnimation(EKeepersAnimSet Set, EKeepersPose Pose)
	{
		if (Set == EKeepersAnimSet::Cavalry)
		{
			const TCHAR* Suffix = TEXT("01_idle");
			switch (Pose)
			{
			case EKeepersPose::Walk:   Suffix = TEXT("02_walk"); break;
			case EKeepersPose::Run:    Suffix = TEXT("03_run"); break;
			case EKeepersPose::Attack:
			case EKeepersPose::Cast:   Suffix = TEXT("04_attack"); break;
			case EKeepersPose::Hit:    Suffix = TEXT("05_take_damage"); break;
			case EKeepersPose::Death:  Suffix = TEXT("06_death_A"); break;
			default: break;
			}
			return Load<UAnimSequence>(TEXT("Animations/Cavalry/Cavalry"), FName(*FString::Printf(TEXT("Anim_cav_%s"), Suffix)));
		}
		const TCHAR* Machine = Set == EKeepersAnimSet::Catapult ? TEXT("catapult") : TEXT("ram");
		const TCHAR* Suffix = TEXT("01_idle");
		switch (Pose)
		{
		case EKeepersPose::Walk:
		case EKeepersPose::Run:    Suffix = TEXT("02_move"); break;
		case EKeepersPose::Attack:
		case EKeepersPose::Cast:   Suffix = TEXT("03_attack"); break;
		case EKeepersPose::Hit:    Suffix = TEXT("04_damage"); break;
		case EKeepersPose::Death:  Suffix = TEXT("05_death"); break;
		default: break;
		}
		return Load<UAnimSequence>(FString(TEXT("Animations/Units/")) + (Set == EKeepersAnimSet::Catapult ? TEXT("Catapult") : TEXT("Ram")),
			FName(*FString::Printf(TEXT("Anim_%s_%s"), Machine, Suffix)));
	}
}'''),
('''UAnimSequence* UKeepersFigureComponent::FindAnimation(EKeepersAnimSet Set, EKeepersPose Pose)
{
	FString Folder;''', '''UAnimSequence* UKeepersFigureComponent::FindAnimation(EKeepersAnimSet Set, EKeepersPose Pose)
{
	if (KeepersFigure::IsPrebuilt(Set))
	{
		return KeepersFigure::FindPrebuiltAnimation(Set, Pose);
	}
	FString Folder;'''),
('''	Body->SetSkeletalMeshAsset(KeepersFigure::Load<USkeletalMesh>(TEXT("Meshes/Characters_Parts/Units"), Look.Body));
	Head->SetSkeletalMeshAsset(KeepersFigure::Load<USkeletalMesh>(TEXT("Meshes/Characters_Parts/Units"), Look.Head));''',
 '''	const bool bPrebuilt = KeepersFigure::IsPrebuilt(Look.AnimSet);
	const TCHAR* BodyFolder = !bPrebuilt ? TEXT("Meshes/Characters_Parts/Units")
		: Look.AnimSet == EKeepersAnimSet::Cavalry ? TEXT("Meshes/Characters_Prebuilt/Cavalry") : TEXT("Meshes/Characters_Prebuilt/Machines");
	Body->SetSkeletalMeshAsset(KeepersFigure::Load<USkeletalMesh>(BodyFolder, Look.Body));
	Head->SetSkeletalMeshAsset(bPrebuilt ? nullptr : KeepersFigure::Load<USkeletalMesh>(TEXT("Meshes/Characters_Parts/Units"), Look.Head));'''),
])

patch('Private/KeepersHeroes.cpp', [
('''			MakeLook(TEXT("Молотобоец"), A::TwoHanded, TEXT("Unit_Body_09d"), TEXT("Unit_Head_08b"), TEXT("SM_w_TH_warhammer")),
		};
		return Enemies;''', '''			MakeLook(TEXT("Молотобоец"), A::TwoHanded, TEXT("Unit_Body_09d"), TEXT("Unit_Head_08b"), TEXT("SM_w_TH_warhammer")),
		};

		// The elite: riders.
		FKeepersHeroDef& Cavalry = Enemies.AddDefaulted_GetRef();
		Cavalry.Id = TEXT("Cavalry");
		Cavalry.Name = T(TEXT("Всадник"));
		Cavalry.Class = T(TEXT("Элита кур"));
		Cavalry.UnitType = 12;
		Cavalry.bEnemy = true;
		Cavalry.Scale = 0.75f;
		Cavalry.Spells = { TEXT("SwordSlash"), TEXT("Charge") };
		Cavalry.Looks = {
			MakeLook(TEXT("Рыцарь"), A::Cavalry, TEXT("SK_Knight"), TEXT("")),
			MakeLook(TEXT("Лёгкий всадник"), A::Cavalry, TEXT("SK_Light_Cavalry"), TEXT("")),
			MakeLook(TEXT("Латник"), A::Cavalry, TEXT("SK_Heavy_Cavalry"), TEXT("")),
		};

		// Siege machines of the bosses.
		FKeepersHeroDef& Catapult = Enemies.AddDefaulted_GetRef();
		Catapult.Id = TEXT("Catapult");
		Catapult.Name = T(TEXT("Катапульта"));
		Catapult.Class = T(TEXT("Осадная машина"));
		Catapult.UnitType = 13;
		Catapult.bEnemy = true;
		Catapult.Scale = 0.6f;
		Catapult.Spells = { TEXT("Boulder") };
		Catapult.Looks = {
			MakeLook(TEXT("Катапульта"), A::Catapult, TEXT("SK_Catapult_lvl1"), TEXT("")),
			MakeLook(TEXT("Окованная катапульта"), A::Catapult, TEXT("SK_Catapult_lvl2"), TEXT("")),
			MakeLook(TEXT("Боевая катапульта"), A::Catapult, TEXT("SK_Catapult_lvl3"), TEXT("")),
		};

		FKeepersHeroDef& Ram = Enemies.AddDefaulted_GetRef();
		Ram.Id = TEXT("Ram");
		Ram.Name = T(TEXT("Таран"));
		Ram.Class = T(TEXT("Осадная машина"));
		Ram.UnitType = 14;
		Ram.bEnemy = true;
		Ram.Scale = 0.6f;
		Ram.Spells = { TEXT("RamStrike") };
		Ram.Looks = {
			MakeLook(TEXT("Таран"), A::Ram, TEXT("SK_Ram_lvl1"), TEXT("")),
			MakeLook(TEXT("Окованный таран"), A::Ram, TEXT("SK_Ram_lvl2"), TEXT("")),
			MakeLook(TEXT("Таран с навесом"), A::Ram, TEXT("SK_Ram_lvl3"), TEXT("")),
		};

		// The shaman raises rocks and grows forest for the chickens.
		FKeepersHeroDef& Shaman = Enemies.AddDefaulted_GetRef();
		Shaman.Id = TEXT("Shaman");
		Shaman.Name = T(TEXT("Шаман кур"));
		Shaman.Class = T(TEXT("Колдун кур"));
		Shaman.UnitType = 15;
		Shaman.bEnemy = true;
		Shaman.Spells = { TEXT("RaiseRock"), TEXT("Sprout"), TEXT("Roots") };
		Shaman.Looks = {
			MakeLook(TEXT("Шаман"), A::Staff, TEXT("Unit_Body_12b"), TEXT("Unit_Head_10b"), TEXT("SM_w_staff_D")),
			MakeLook(TEXT("Гадатель"), A::Staff, TEXT("Unit_Body_12d"), TEXT("Unit_Head_06c"), TEXT("SM_w_staff_A")),
		};
		return Enemies;'''),
('''			S(TEXT("Rage"), TEXT("Ярость"),''', '''			S(TEXT("Boulder"), TEXT("Валун"), TEXT("Катапульта бросает валун: он бьёт по области и остаётся на поле скалой."), TEXT("3–4 урона · область · 3–6 клеток")),
			S(TEXT("RamStrike"), TEXT("Удар тарана"), TEXT("Таран отбрасывает соседа на клетку."), TEXT("4–5 урона · толчок · рядом")),
			S(TEXT("Charge"), TEXT("Натиск"), TEXT("Всадник мчится по прямой и сбивает врагов у места остановки."), TEXT("2–3 урона · 2–4 клетки")),
			S(TEXT("Rage"), TEXT("Ярость"),'''),
])

patch('Private/KeepersBattleHud.cpp', [
('''		{ TEXT("barbarian"), TEXT("Варвар") },''', '''		{ TEXT("barbarian"), TEXT("Варвар") },
		{ TEXT("cavalry"), TEXT("Всадник") },
		{ TEXT("catapult"), TEXT("Катапульта") },
		{ TEXT("ram"), TEXT("Таран") },
		{ TEXT("shaman"), TEXT("Шаман кур") },'''),
])

patch('Public/KeepersTerrainSubsystem.h', [
('''	Throw,
	Rage
};''', '''	Throw,
	Rage,
	// Enemies.
	Boulder
};'''),
('''/**
 * Terrain and push effects of the Keepers' abilities.''', '''/**
 * Terrain and push effects of the Keepers' abilities (and of the enemies': Charge works as Dash, RamStrike pushes,
 * a catapult's Boulder stays on an empty target cell as a rock).'''),
])
patch('Private/KeepersTerrainSubsystem.cpp', [
('''	static const TArray<FString> PushAnimations = { TEXT("BP_SpellAnim_Gust"), TEXT("BP_SpellAnim_ShieldBash") };''',
 '''	static const TArray<FString> PushAnimations = { TEXT("BP_SpellAnim_Gust"), TEXT("BP_SpellAnim_ShieldBash"), TEXT("BP_SpellAnim_RamStrike") };'''),
('''		{ TEXT("BP_SpellAnim_Rage"), EKeepersTerrainEffect::Rage },''', '''		{ TEXT("BP_SpellAnim_Rage"), EKeepersTerrainEffect::Rage },
		{ TEXT("BP_SpellAnim_Charge"), EKeepersTerrainEffect::Dash },
		{ TEXT("BP_SpellAnim_Boulder"), EKeepersTerrainEffect::Boulder },'''),
('''	case EKeepersTerrainEffect::Rage:
		ApplyHeroAbility(Effect, Grid, Origin, Target);
		break;''', '''	case EKeepersTerrainEffect::Rage:
		ApplyHeroAbility(Effect, Grid, Origin, Target);
		break;
	case EKeepersTerrainEffect::Boulder:
		// The boulder stays where it fell when the cell was empty.
		if (!KeepersTerrain::GetUnitOnTile(Grid, Target) && RaiseRock(Grid, Target, 1) && bHasLocation)
		{
			UKeepersJuiceSubsystem::Burst(GetWorld(), TargetLocation + FVector(0.f, 0.f, 20.f), AKeepersBurst::EKind::Puff, FLinearColor(0.62f, 0.55f, 0.45f), 16, 1.f);
			UKeepersJuiceSubsystem::Sound(this, TEXT("Rock"));
		}
		break;'''),
])

# The AI: enemies with terrain spells wall the heroes in and grow forest under their feet.
patch('Private/KeepersAiLibrary.cpp', [
('''		TMap<FIntPoint, FTile> Tiles;
		float Step = 100.f;''', '''		TMap<FIntPoint, FTile> Tiles;
		/** Cells of team 0. */
		TArray<FIntPoint> Heroes;
		float Step = 100.f;'''),
('''		Cache.Tiles.Reset();''', '''		Cache.Tiles.Reset();
		Cache.Heroes.Reset();'''),
('''			Cache.Tiles.Add(Index, Tile);''', '''			Cache.Tiles.Add(Index, Tile);
			if (Tile.Unit)
			{
				int32 Team = 1;
				if (KeepersReflection::GetInt(Tile.Unit->GetClass(), Tile.Unit, TEXT("TeamIndex"), Team) && Team == 0)
				{
					Cache.Heroes.Add(Index);
				}
			}'''),
('''	/** UnitData.Stats.ValidTileTypes has FlyingUnitsOnly. */''', '''	/** E_Spell value of an entry name, MAX_uint8 when there is none. */
	static uint8 SpellValue(const TCHAR* Name)
	{
		static TMap<FString, uint8> Values;
		if (const uint8* Found = Values.Find(Name))
		{
			return *Found;
		}
		uint8 Value = MAX_uint8;
		if (const UEnum* Spells = LoadObject<UEnum>(nullptr, TEXT("/Game/Blueprints/Core/Combat/Spells/Utilities/E_Spell.E_Spell")))
		{
			for (int32 I = 0; I < Spells->NumEnums() - 1; ++I)
			{
				if (Spells->GetDisplayNameTextByIndex(I).ToString() == Name)
				{
					Value = static_cast<uint8>(Spells->GetValueByIndex(I));
				}
			}
		}
		Values.Add(Name, Value);
		return Value;
	}

	/** Steps from a cell to the nearest hero, 99 when there is none. */
	static int32 StepsToHeroes(const FGridCache& Map, const FIntPoint& Cell)
	{
		int32 Best = 99;
		for (const FIntPoint& Hero : Map.Heroes)
		{
			Best = FMath::Min(Best, StepsBetween(Map.bHex, Cell, Hero));
		}
		return Best;
	}

	/**
	 * Worth of an enemy's rock or forest (they deal no damage, so the plan's own score is 0):
	 * a rock next to heroes walls them in, a rock between the caster and the heroes is cover,
	 * forest under a hero's feet or next to it slows them down.
	 */
	static double TerrainPlanScore(const FGridCache& Map, uint8 Spell, const FIntPoint& Origin, const FIntPoint& Target)
	{
		const bool bRock = Spell == SpellValue(TEXT("RaiseRock"));
		const bool bForest = Spell == SpellValue(TEXT("Sprout"));
		const FTile* At = Map.Tiles.Find(Target);
		if ((!bRock && !bForest) || !At || At->Type != 1)
		{
			return 0.0;
		}
		int32 HeroesNext = 0;
		for (const FIntPoint& Hero : Map.Heroes)
		{
			HeroesNext += StepsBetween(Map.bHex, Hero, Target) == 1 ? 1 : 0;
		}
		if (bRock)
		{
			if (At->Unit)
			{
				return 0.0;
			}
			double Value = HeroesNext > 0 ? 1.3 + 0.3 * (HeroesNext - 1) : 0.0;
			const int32 FromOrigin = StepsToHeroes(Map, Origin);
			if (StepsBetween(Map.bHex, Origin, Target) == 1 && FromOrigin <= 5 && StepsToHeroes(Map, Target) < FromOrigin)
			{
				Value = FMath::Max(Value, 1.1);
			}
			return Value;
		}
		const bool bUnderHero = At->Unit && Map.Heroes.Contains(Target);
		return bUnderHero ? 1.2 : HeroesNext > 0 && !At->Unit ? 0.8 : 0.0;
	}

	/** UnitData.Stats.ValidTileTypes has FlyingUnitsOnly. */'''),
('''	if (!Grid || BaseScore <= 0.0)
	{
		return BaseScore;
	}''', '''	if (!Grid)
	{
		return BaseScore;
	}'''),
('''	const FGridCache& Map = ReadGrid(Grid);
	const FTile* From = Map.Tiles.Find(Origin);
	const FTile* At = Map.Tiles.Find(Target);
	if (!From)
	{
		return BaseScore;
	}
	const int32 Team = TeamOf(Unit);''', '''	const FGridCache& Map = ReadGrid(Grid);
	const FTile* From = Map.Tiles.Find(Origin);
	const FTile* At = Map.Tiles.Find(Target);
	if (!From)
	{
		return BaseScore;
	}
	const int32 Team = TeamOf(Unit);
	// Rocks and forest of the enemies (the shaman's): scored here, they deal no damage.
	if (Team != 0)
	{
		const double Terrain = TerrainPlanScore(Map, Spell, Origin, Target);
		if (Terrain > 0.0)
		{
			return FMath::Max(BaseScore, 0.0) + Terrain;
		}
	}
	if (BaseScore <= 0.0)
	{
		return BaseScore;
	}'''),
])
print('ok')
