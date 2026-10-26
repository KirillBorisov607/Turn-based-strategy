#include "KeepersReactions.h"

#include "Engine/World.h"
#include "EngineUtils.h"
#include "KeepersReflection.h"
#include "KeepersTerrainSubsystem.h"
#include "TimerManager.h"

namespace KeepersReactions
{
	using S = EKeepersBarkSituation;
	using FLines = TArray<const TCHAR*>;
	using FTable = TMap<S, FLines>;

	/** E_UnitType values. */
	enum EType : uint8 { Warrior, Ranger, Priest, Slime, Chicken, Bat, Bandit, Archer, Brute, Mage, Rogue, Barbarian, Cavalry, Catapult, Ram, Shaman, TypeCount };

	/** Lines every hero may say. */
	static const FTable& HeroLines()
	{
		static const FTable Table =
		{
			{ S::FriendlyFire, { TEXT("Эй! Я же свой!"), TEXT("Смотри, куда бьёшь!"), TEXT("Мы на одной стороне, помнишь?"), TEXT("За что?!"),
				TEXT("Я это запомню."), TEXT("Отличный удар. По мне."), TEXT("Друзья так не поступают!"), TEXT("Кажется, ты перепутал цель.") } },
			{ S::HitAlly, { TEXT("Ой. Прости!"), TEXT("Это был манёвр. Честно."), TEXT("Ты стоял не там!"), TEXT("Случайно! Почти."),
				TEXT("Зато врагу тоже досталось!"), TEXT("Никто не видел. Правда?") } },
			{ S::Kill, { TEXT("Готов!"), TEXT("Следующий!"), TEXT("Одним меньше."), TEXT("Так ему и надо."), TEXT("Видели? Видели?!"),
				TEXT("Отдыхай."), TEXT("Это за Камни земель!") } },
			{ S::AllyDown, { TEXT("Нет! Держись!"), TEXT("Мы отомстим!"), TEXT("Лекаря сюда!"), TEXT("Не смей падать!"), TEXT("Я вынесу тебя после боя!") } },
			{ S::LowHealth, { TEXT("Мне бы перевязку…"), TEXT("Ещё немного — и всё."), TEXT("Я в порядке. Почти."), TEXT("Прикройте меня!"),
				TEXT("Кажется, мне нужен привал.") } },
			{ S::Healed, { TEXT("Спасибо!"), TEXT("Как новенький!"), TEXT("Вот это другое дело."), TEXT("Чую силу земли!") } },
			{ S::BattleStart, { TEXT("К бою!"), TEXT("Строимся!"), TEXT("За Камни земель!"), TEXT("Покажем им!") } },
			{ S::Victory, { TEXT("Победа!"), TEXT("Земля спокойна. Пока что."), TEXT("Ещё один камень ближе."), TEXT("Куры, вы следующие!") } },
			{ S::TreeFalls, { TEXT("Берегись! Падает!"), TEXT("Лес на нашей стороне."), TEXT("Прости, дерево."), TEXT("Тимбе-е-ер!") } },
			{ S::RockRolls, { TEXT("Камень пошёл!"), TEXT("Лови булыжник!"), TEXT("Покатилось!") } },
			{ S::Slides, { TEXT("Лё-ё-ёд!"), TEXT("Не могу остановиться!"), TEXT("Кто полил тут?!") } },
			{ S::StuckInMarsh, { TEXT("Сапог! Где мой сапог?!"), TEXT("Засосало…"), TEXT("Фу, тина!") } },
			{ S::Shocked, { TEXT("Бз-з-з!"), TEXT("Волосы дыбом!") } },
			{ S::IceMelts, { TEXT("Лёд тает! Буль!"), TEXT("Кто растопил?!") } },
			{ S::Rockfall, { TEXT("Сверху!"), TEXT("Ай, голова!") } },
		};
		return Table;
	}

	/** Each hero's own voice. */
	static const TMap<uint8, FTable>& OwnLines()
	{
		static const TMap<uint8, FTable> Table =
		{
			{ Warrior, {
				{ S::FriendlyFire, { TEXT("Мой щит не для твоих ударов!"), TEXT("Я Страж, а не мишень!") } },
				{ S::HitAlly, { TEXT("Щитом задел. Бывает.") } },
				{ S::Kill, { TEXT("Стена не пропустит."), TEXT("Щит и сталь!") } },
				{ S::LowHealth, { TEXT("Доспех трещит…"), TEXT("Стражи не отступают!") } },
				{ S::TreeFalls, { TEXT("Щитом — по стволу!") } },
			} },
			{ Ranger, {
				{ S::FriendlyFire, { TEXT("Я тебе не мишень для тренировки!") } },
				{ S::HitAlly, { TEXT("Стрела свернула. Ветер!"), TEXT("Мой порыв — твоя проблема.") } },
				{ S::Kill, { TEXT("В яблочко!"), TEXT("Даже не прицеливался.") } },
				{ S::LowHealth, { TEXT("Стрелы кончаются, силы тоже.") } },
			} },
			{ Priest, {
				{ S::FriendlyFire, { TEXT("Ай! Лечить себя теперь…"), TEXT("Я же вас лечу!") } },
				{ S::HitAlly, { TEXT("Простите-простите-простите!"), TEXT("Сейчас подлечу, не сердись!") } },
				{ S::Kill, { TEXT("Прости, существо."), TEXT("Земля примет тебя.") } },
				{ S::Healed, { TEXT("Сам себя не похвалишь…") } },
				{ S::LowHealth, { TEXT("Даже лекарю нужен лекарь.") } },
			} },
			{ Mage, {
				{ S::FriendlyFire, { TEXT("Мантию прожёг!"), TEXT("Осторожнее, я хрупкий!") } },
				{ S::HitAlly, { TEXT("Площадь поражения. Ничего личного."), TEXT("Огонь не разбирает, кто свой.") } },
				{ S::Kill, { TEXT("Стихия решила."), TEXT("Хрустящая корочка.") } },
				{ S::IceMelts, { TEXT("Огонь плюс лёд — простая алхимия.") } },
			} },
			{ Rogue, {
				{ S::FriendlyFire, { TEXT("Обычно это я бью в спину!") } },
				{ S::HitAlly, { TEXT("Рука соскользнула. Специально."), TEXT("Ты бы не заметил, если б не кричал.") } },
				{ S::Kill, { TEXT("Тихо и аккуратно."), TEXT("Даже не увидел меня.") } },
				{ S::LowHealth, { TEXT("Пора бы исчезнуть…") } },
			} },
			{ Barbarian, {
				{ S::FriendlyFire, { TEXT("Щекотно. Но обидно."), TEXT("Ещё раз — и сам полетишь!") } },
				{ S::HitAlly, { TEXT("Ха! Крепче стой!"), TEXT("Сам виноват — под руку попал.") } },
				{ S::Kill, { TEXT("Хрясь!"), TEXT("Следующего мне!"), TEXT("Слабак!") } },
				{ S::LowHealth, { TEXT("Боль — это просто злость!"), TEXT("Сейчас разозлюсь по-настоящему.") } },
				{ S::TreeFalls, { TEXT("Дерево? Не помеха!") } },
				{ S::RockRolls, { TEXT("Камень — тоже оружие!") } },
			} },
		};
		return Table;
	}

	/** People of the chickens: bandits, archers, brutes, riders, shamans. */
	static const FTable& PeopleLines()
	{
		static const FTable Table =
		{
			{ S::FriendlyFire, { TEXT("Эй, нам за это не платили!"), TEXT("Свои! Свои же!"), TEXT("Я пожалуюсь курам!"), TEXT("Ты на чьей стороне?!") } },
			{ S::HitAlly, { TEXT("Ой. Вычтут из жалованья."), TEXT("Не стой на пути!"), TEXT("Он сам подставился!") } },
			{ S::Kill, { TEXT("Ха! Куры будут довольны."), TEXT("Один Хранитель — минус!"), TEXT("Премия моя!") } },
			{ S::AllyDown, { TEXT("Мы так не договаривались!"), TEXT("Кто теперь мне должен?"), TEXT("Отходим! Нет, стоим!") } },
			{ S::LowHealth, { TEXT("Это не стоит тех денег…"), TEXT("Я, пожалуй, в отпуск.") } },
			{ S::BattleStart, { TEXT("Хранители! Плата двойная!"), TEXT("Куры заплатили — работаем!"), TEXT("Окружай их!") } },
			{ S::TreeFalls, { TEXT("Дерево на меня?!") } },
			{ S::Rockfall, { TEXT("Камень?! Откуда камень?!"), TEXT("Сверху-у-у!") } },
			{ S::Slides, { TEXT("Держите меня!"), TEXT("Скользко-о!") } },
			{ S::StuckInMarsh, { TEXT("Нога! Застряла нога!") } },
			{ S::Shocked, { TEXT("Ай-яй-яй-яй!") } },
			{ S::IceMelts, { TEXT("Я не умею плавать!") } },
		};
		return Table;
	}

	static const FTable& SlimeLines()
	{
		static const FTable Table =
		{
			{ S::FriendlyFire, { TEXT("Буль?! Своих не трогай!"), TEXT("Я тоже слизь, вообще-то!") } },
			{ S::HitAlly, { TEXT("Буль. Прости, брат."), TEXT("Мы все из одной лужи…") } },
			{ S::Kill, { TEXT("Буль-буль! Победа!"), TEXT("Поглощён!") } },
			{ S::AllyDown, { TEXT("Брат растёкся…"), TEXT("Мы отомстим лужами!") } },
			{ S::LowHealth, { TEXT("Я подсыхаю…"), TEXT("Слишком мало меня!") } },
			{ S::BattleStart, { TEXT("Буль! Еда пришла!"), TEXT("Обнимашки!") } },
			{ S::Rockfall, { TEXT("Сплющило-о!") } },
			{ S::Slides, { TEXT("Скольжу! Я и так скольжу!") } },
			{ S::IceMelts, { TEXT("Буль… я и так мокрый.") } },
		};
		return Table;
	}

	static const FTable& ChickenLines()
	{
		static const FTable Table =
		{
			{ S::FriendlyFire, { TEXT("Ко! Своих не клюют!"), TEXT("Перья! Мои перья!") } },
			{ S::HitAlly, { TEXT("Ко-ко… случайно.") } },
			{ S::Kill, { TEXT("Ко-ко-конец!"), TEXT("Великий Петух гордится мной!") } },
			{ S::AllyDown, { TEXT("Ко-о-о… сестра!"), TEXT("Отомстим! Ко!") } },
			{ S::LowHealth, { TEXT("Ко… перья кончаются."), TEXT("Мама, я в суп не хочу!") } },
			{ S::BattleStart, { TEXT("Ко-ко-ко! Тревога!"), TEXT("Зовите всех!") } },
			{ S::Rockfall, { TEXT("Небо падает! Ко-о!") } },
			{ S::Slides, { TEXT("Ко-ко-конёк!") } },
			{ S::StuckInMarsh, { TEXT("Ко! Лапки!") } },
		};
		return Table;
	}

	static const FTable& BatLines()
	{
		static const FTable Table =
		{
			{ S::FriendlyFire, { TEXT("Пи! Слепой, что ли?!") } },
			{ S::Kill, { TEXT("Пи-пи-пи! (торжествующе)") } },
			{ S::AllyDown, { TEXT("Пи… (грустно)") } },
			{ S::LowHealth, { TEXT("Пи… крыло болит.") } },
			{ S::BattleStart, { TEXT("Пи! Пи! Чужаки!") } },
		};
		return Table;
	}

	static const FTable& MachineLines()
	{
		static const FTable Table =
		{
			{ S::FriendlyFire, { TEXT("*скрип возмущения*") } },
			{ S::Kill, { TEXT("*довольный скрип*") } },
			{ S::LowHealth, { TEXT("*треск досок*"), TEXT("*колесо отваливается*") } },
			{ S::BattleStart, { TEXT("*скрип колёс*") } },
		};
		return Table;
	}

	static const FTable* GroupOf(uint8 Type)
	{
		switch (Type)
		{
		case Warrior: case Ranger: case Priest: case Mage: case Rogue: case Barbarian: return &HeroLines();
		case Bandit: case Archer: case Brute: case Cavalry: case Shaman: return &PeopleLines();
		case Catapult: case Ram: return &MachineLines();
		case Slime: return &SlimeLines();
		case Chicken: return &ChickenLines();
		case Bat: return &BatLines();
		default: return nullptr;
		}
	}

	const TArray<const TCHAR*>& Lines(uint8 UnitType, EKeepersBarkSituation Situation)
	{
		// A unit's own lines together with its group's.
		static TMap<uint32, FLines> Cache;
		const uint32 Key = UnitType * 256u + static_cast<uint32>(Situation);
		if (const FLines* Found = Cache.Find(Key))
		{
			return *Found;
		}
		FLines Result;
		if (const FTable* Own = OwnLines().Find(UnitType))
		{
			if (const FLines* Lines = Own->Find(Situation))
			{
				Result.Append(*Lines);
			}
		}
		if (const FTable* Group = GroupOf(UnitType))
		{
			if (const FLines* Lines = Group->Find(Situation))
			{
				Result.Append(*Lines);
			}
		}
		return Cache.Add(Key, MoveTemp(Result));
	}

	int32 Count()
	{
		int32 Total = 0;
		auto Add = [&Total](const FTable& Table) { for (const TPair<S, FLines>& Pair : Table) { Total += Pair.Value.Num(); } };
		Add(HeroLines());
		Add(PeopleLines());
		Add(SlimeLines());
		Add(ChickenLines());
		Add(BatLines());
		Add(MachineLines());
		for (const TPair<uint8, FTable>& Own : OwnLines())
		{
			Add(Own.Value);
		}
		return Total;
	}

	static uint8 TypeOf(const AActor* Unit)
	{
		FProperty* Property = nullptr;
		void* Ptr = KeepersReflection::FindValuePtr(Unit->GetClass(), const_cast<AActor*>(Unit), TEXT("UnitType"), &Property);
		const FByteProperty* Byte = CastField<FByteProperty>(Property);
		return Byte && Ptr ? Byte->GetPropertyValue(Ptr) : TypeCount;
	}
}

bool UKeepersReactionSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

TStatId UKeepersReactionSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UKeepersReactionSubsystem, STATGROUP_Tickables);
}

bool UKeepersReactionSubsystem::Say(AActor* Unit, EKeepersBarkSituation Situation, float Chance, bool bUrgent)
{
	if (!Unit || FMath::FRand() > Chance)
	{
		return false;
	}
	// Quiet spells between lines: a little for the field, more for the unit.
	if (!bUrgent && Clock - LastAny < 1.2f)
	{
		return false;
	}
	if (Clock - LastSaid.FindRef(Unit) < 4.f && LastSaid.Contains(Unit))
	{
		return false;
	}
	const TArray<const TCHAR*>& Lines = KeepersReactions::Lines(KeepersReactions::TypeOf(Unit), Situation);
	if (Lines.Num() == 0)
	{
		return false;
	}
	UKeepersBarkLibrary::Bark(Unit, FText::FromString(Lines[FMath::RandRange(0, Lines.Num() - 1)]));
	LastAny = Clock;
	LastSaid.Add(Unit, Clock);
	return true;
}

void UKeepersReactionSubsystem::Tick(float DeltaTime)
{
	UWorld* World = GetWorld();
	if (!World || !World->HasBegunPlay())
	{
		return;
	}
	Clock += DeltaTime;
	if (!CombatSystem)
	{
		CombatSystem = UKeepersTerrainSubsystem::CombatSystemOf(World);
		if (!CombatSystem)
		{
			return;
		}
	}
	bool bInCombat = false;
	bool bFinishing = false;
	KeepersReflection::GetBool(CombatSystem->GetClass(), CombatSystem, TEXT("IsInCombat"), bInCombat);
	KeepersReflection::GetBool(CombatSystem->GetClass(), CombatSystem, TEXT("IsFinishingCombat"), bFinishing);
	if (!bInCombat)
	{
		return;
	}
	AActor* Current = Cast<AActor>(KeepersReflection::GetObject(CombatSystem, TEXT("CurrentUnit")));
	int32 CurrentTeam = INDEX_NONE;
	if (Current)
	{
		KeepersReflection::GetInt(Current->GetClass(), Current, TEXT("TeamIndex"), CurrentTeam);
	}

	// The end: the winners cheer once.
	if (bFinishing)
	{
		if (!bEnded)
		{
			bEnded = true;
			int32 Winner = INDEX_NONE;
			KeepersReflection::GetInt(CombatSystem->GetClass(), CombatSystem, TEXT("TeamIndexThatWonLastCombat"), Winner);
			for (const TPair<TWeakObjectPtr<AActor>, FWatched>& Pair : Watched)
			{
				if (Pair.Key.IsValid() && Pair.Value.bAlive && Pair.Value.Team == Winner)
				{
					Say(Pair.Key.Get(), EKeepersBarkSituation::Victory, 1.f, true);
					break;
				}
			}
		}
		return;
	}

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Unit = *It;
		if (Unit->GetClass()->GetName() != TEXT("BP_Unit_C"))
		{
			continue;
		}
		int32 Hp = 0;
		int32 HpMax = 1;
		int32 Team = 0;
		KeepersReflection::GetUnitStat(Unit, TEXT("HP_Current"), Hp);
		KeepersReflection::GetUnitStat(Unit, TEXT("HP_Max"), HpMax);
		KeepersReflection::GetInt(Unit->GetClass(), Unit, TEXT("TeamIndex"), Team);
		FWatched* Seen = Watched.Find(Unit);
		if (!Seen)
		{
			FWatched& Fresh = Watched.Add(Unit);
			Fresh.Hp = Hp;
			Fresh.HpMax = FMath::Max(HpMax, 1);
			Fresh.Team = Team;
			continue;
		}
		const int32 Change = Hp - Seen->Hp;
		Seen->HpMax = FMath::Max(HpMax, 1);
		Seen->Team = Team;
		if (Change < 0 && Seen->bAlive)
		{
			const bool bDead = Hp <= 0;
			// Who struck: the unit whose turn it is.
			const bool bFriend = Current && Current != Unit && CurrentTeam == Team;
			if (bFriend)
			{
				Say(Unit, EKeepersBarkSituation::FriendlyFire, 1.f, true);
				// The striker answers a moment later.
				TWeakObjectPtr<AActor> Striker(Current);
				TWeakObjectPtr<UKeepersReactionSubsystem> WeakThis(this);
				FTimerHandle Timer;
				World->GetTimerManager().SetTimer(Timer, FTimerDelegate::CreateLambda([WeakThis, Striker]()
				{
					if (WeakThis.IsValid() && Striker.IsValid())
					{
						WeakThis->Say(Striker.Get(), EKeepersBarkSituation::HitAlly, 1.f, true);
					}
				}), 1.3f, false);
			}
			if (bDead)
			{
				Seen->bAlive = false;
				if (Current && CurrentTeam != Team)
				{
					Say(Current, EKeepersBarkSituation::Kill, 0.6f);
				}
				// The nearest friend cries out.
				AActor* Nearest = nullptr;
				float Best = MAX_flt;
				for (const TPair<TWeakObjectPtr<AActor>, FWatched>& Pair : Watched)
				{
					AActor* Other = Pair.Key.Get();
					if (Other && Other != Unit && Pair.Value.bAlive && Pair.Value.Team == Team)
					{
						const float Distance = FVector::Dist(Other->GetActorLocation(), Unit->GetActorLocation());
						if (Distance < Best)
						{
							Best = Distance;
							Nearest = Other;
						}
					}
				}
				Say(Nearest, EKeepersBarkSituation::AllyDown, Team == 0 ? 1.f : 0.5f, Team == 0);
			}
			else if (!Seen->bWarnedLow && Hp * 10 <= HpMax * 3)
			{
				Seen->bWarnedLow = true;
				Say(Unit, EKeepersBarkSituation::LowHealth, 0.8f);
			}
		}
		else if (Change >= 3 && Seen->bAlive)
		{
			if (Hp * 10 > HpMax * 3)
			{
				Seen->bWarnedLow = false;
			}
			Say(Unit, EKeepersBarkSituation::Healed, 0.5f);
		}
		Seen->Hp = Hp;
	}

	// The start: one word from each side once the field is in view.
	if (!bStarted && Clock > 1.5f && Watched.Num() > 0)
	{
		bStarted = true;
		bool bHeroSpoke = false;
		bool bEnemySpoke = false;
		for (const TPair<TWeakObjectPtr<AActor>, FWatched>& Pair : Watched)
		{
			if (!Pair.Key.IsValid())
			{
				continue;
			}
			if (Pair.Value.Team == 0 && !bHeroSpoke)
			{
				bHeroSpoke = Say(Pair.Key.Get(), EKeepersBarkSituation::BattleStart, 1.f, true);
			}
			else if (Pair.Value.Team != 0 && !bEnemySpoke)
			{
				bEnemySpoke = Say(Pair.Key.Get(), EKeepersBarkSituation::BattleStart, 1.f, true);
			}
		}
	}
}
