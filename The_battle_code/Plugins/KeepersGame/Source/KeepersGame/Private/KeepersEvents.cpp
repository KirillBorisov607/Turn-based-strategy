// Event nodes of the campaign: encounters with a choice and its consequence.

#include "KeepersCampaign.h"
#include "KeepersCampaignUI.h"
#include "KeepersHeroes.h"

#include "Engine/World.h"

namespace KeepersEvents
{
	enum class EEffect : uint8
	{
		None,
		Gold,
		Artifact,
		Armor,
		Boots,
		Glory,
		Fight,
		FightMercenaries
	};

	struct FOption
	{
		const TCHAR* Label;
		/** Gold paid before anything happens. */
		int32 Cost = 0;
		/** Chance of the good outcome, percent. */
		int32 Chance = 100;
		EEffect Effect = EEffect::None;
		int32 Amount = 0;
		const TCHAR* Outcome = TEXT("");
		EEffect FailEffect = EEffect::None;
		int32 FailAmount = 0;
		const TCHAR* FailOutcome = TEXT("");
	};

	struct FEncounter
	{
		const TCHAR* Title;
		const TCHAR* Text;
		TArray<FOption> Options;
	};

	static const TArray<FEncounter>& All()
	{
		static const TArray<FEncounter> Events =
		{
			{ TEXT("Курица-беженка"),
				TEXT("Из кустов выбегает растрёпанная курица с узелком. «Они всё знают! — кудахчет она. — Прячьте меня!»"),
				{
					{ TEXT("Приютить"), 0, 100, EEffect::Glory, 10, TEXT("Курица пересказала сплетни курятника. Теперь о Хранителях судачат по всей долине.") },
					{ TEXT("Обыскать узелок"), 0, 60, EEffect::Gold, 30, TEXT("В узелке — золотые зёрнышки. Курица обиделась, но промолчала."),
						EEffect::FightMercenaries, 0, TEXT("Это ловушка! Из кустов выходят наёмники кур.") },
					{ TEXT("Пройти мимо") , 0, 100, EEffect::None, 0, TEXT("Курица смотрит вам вслед с глубоким осуждением.") },
				} },
			{ TEXT("Таверна «Пьяный гриб»"),
				TEXT("Под вывеской с подмигивающим грибом шумно и тепло. Хозяин протирает кружку и кивает на свободный стол."),
				{
					{ TEXT("Выпить за удачу (15 зол.)"), 15, 100, EEffect::Artifact, 0, TEXT("К утру у Хранителей откуда-то взялась реликвия. Откуда — не помнит никто.") },
					{ TEXT("Устроить драку"), 0, 100, EEffect::Fight, 0, TEXT("") },
					{ TEXT("Послушать слухи"), 0, 100, EEffect::Glory, 6, TEXT("Говорят, Великий Петух боится сквозняков. И что слизни делятся, если их ударить. Это вы и так знали.") },
				} },
			{ TEXT("Говорящий камень"),
				TEXT("Мшистый валун открывает один глаз: «Ну наконец-то. Триста лет никто не спрашивал дорогу»."),
				{
					{ TEXT("Поговорить"), 0, 100, EEffect::Glory, 8, TEXT("Камень рассказал дорогу, погоду на неделю и три анекдота про гранит.") },
					{ TEXT("Отколоть кусочек"), 0, 100, EEffect::Armor, 0, TEXT("Камень обиделся, а осколок лёг в броню как родной.") },
				} },
			{ TEXT("Ведьмин котёл"),
				TEXT("В котле над костром булькает что-то лиловое. Ведьмы нигде нет. Подозрительно."),
				{
					{ TEXT("Отхлебнуть"), 0, 50, EEffect::Boots, 0, TEXT("Ноги сами несут вперёд! Сапоги будто стали легче."),
						EEffect::Gold, -15, TEXT("Хранителей позеленило. Знахарь в деревне взял за лечение 15 золотых.") },
					{ TEXT("Продать котёл"), 0, 100, EEffect::Gold, 35, TEXT("Старьёвщик дал за котёл 35 золотых и не задал ни одного вопроса.") },
					{ TEXT("Опрокинуть"), 0, 100, EEffect::Fight, 0, TEXT("") },
				} },
			{ TEXT("Мост тролля"),
				TEXT("Под мостом храпит тролль. Табличка: «Проход — 20 золотых. Или загадка. Или в лоб»."),
				{
					{ TEXT("Заплатить (20 зол.)"), 20, 100, EEffect::None, 0, TEXT("Тролль пересчитал монеты дважды и пропустил, не просыпаясь до конца.") },
					{ TEXT("Отгадать загадку"), 0, 50, EEffect::Gold, 30, TEXT("«Что ходит, но не идёт?» — «Ход!» Тролль так растрогался, что подарил 30 золотых."),
						EEffect::Fight, 0, TEXT("Тролль не оценил ответ. Совсем.") },
					{ TEXT("В лоб"), 0, 100, EEffect::Fight, 0, TEXT("") },
				} },
			{ TEXT("Бродячий кузнец"),
				TEXT("Кузнец с переносной наковальней чинит подкову ослу. «Подковать и вас? Недорого»."),
				{
					{ TEXT("Подковать сапоги (25 зол.)"), 25, 100, EEffect::Boots, 0, TEXT("Новые подковы звенят по камням. Шаг стал шире.") },
					{ TEXT("Подлатать броню (25 зол.)"), 25, 100, EEffect::Armor, 0, TEXT("Пара заклёпок, и броня как новая.") },
					{ TEXT("Отказаться"), 0, 100, EEffect::None, 0, TEXT("Кузнец пожал плечами и вернулся к ослу.") },
				} },
			{ TEXT("Заброшенный колодец"),
				TEXT("Из колодца тянет холодом и почему-то запахом блинов."),
				{
					{ TEXT("Бросить монетку (5 зол.)"), 5, 40, EEffect::Artifact, 0, TEXT("Из колодца со звоном вылетает реликвия. Желание сбылось!"),
						EEffect::None, 0, TEXT("Плюх. Ничего. Колодец, кажется, хихикнул.") },
					{ TEXT("Спуститься"), 0, 55, EEffect::Gold, 40, TEXT("На дне — чей-то тайник: 40 золотых и холодный блин."),
						EEffect::Fight, 0, TEXT("Внизу кто-то живёт. И он не рад гостям.") },
					{ TEXT("Уйти"), 0, 100, EEffect::None, 0, TEXT("Блины подождут.") },
				} },
			{ TEXT("Телега наёмников"),
				TEXT("На обочине — сломанная телега с гербом курятника. Возницы спорят, кто виноват."),
				{
					{ TEXT("Помочь починить"), 0, 100, EEffect::Glory, 10, TEXT("Наёмники так удивились, что пообещали в следующий раз бить вполсилы. Наверное, врут.") },
					{ TEXT("Обчистить телегу"), 0, 100, EEffect::FightMercenaries, 0, TEXT("") },
					{ TEXT("Пройти мимо"), 0, 100, EEffect::None, 0, TEXT("Спор за спиной переходит в драку. Без вас.") },
				} },
			{ TEXT("Гнездо кур-шпионов"),
				TEXT("На дереве — гнездо с подзорной трубой. Оттуда кто-то старательно кудахчет в рупор."),
				{
					{ TEXT("Разорить гнездо"), 0, 100, EEffect::Fight, 0, TEXT("") },
					{ TEXT("Подслушать"), 0, 70, EEffect::Glory, 12, TEXT("Шпионы обсуждали планы Великого Петуха. Теперь их знаете и вы."),
						EEffect::FightMercenaries, 0, TEXT("Вас заметили! Шпионы зовут охрану.") },
					{ TEXT("Тихо уйти"), 0, 100, EEffect::None, 0, TEXT("Кудахтанье стихает за холмом.") },
				} },
			{ TEXT("Ярмарка"),
				TEXT("Пёстрые шатры, музыка, жареные орехи. Зазывала машет руками: «Реликвии! Напёрстки! Всё честно!»"),
				{
					{ TEXT("Купить реликвию (40 зол.)"), 40, 100, EEffect::Artifact, 0, TEXT("Торговец клянётся, что реликвия настоящая. Похоже, так и есть.") },
					{ TEXT("Сыграть в напёрстки (10 зол.)"), 10, 45, EEffect::Gold, 30, TEXT("Горошина под левым! Выигрыш — 30 золотых."),
						EEffect::None, 0, TEXT("Горошины не было ни под одним. Всё честно, как и обещали.") },
					{ TEXT("Поесть орехов"), 0, 100, EEffect::Glory, 4, TEXT("Хранители сыты, довольны и сплясали с местными.") },
				} },
			{ TEXT("Спящий великан"),
				TEXT("Поперёк дороги храпит великан. В его кармане что-то блестит, а в бороде запуталась курица."),
				{
					{ TEXT("Обшарить карман"), 0, 50, EEffect::Artifact, 0, TEXT("Великан почесался, но не проснулся. В кармане — реликвия."),
						EEffect::Fight, 0, TEXT("Великан открыл один глаз. Его друзья — оба."), },
					{ TEXT("Освободить курицу"), 0, 100, EEffect::Glory, 8, TEXT("Курица унеслась с криком «Я вам это припомню!». Кажется, это благодарность.") },
					{ TEXT("Обойти по полю"), 0, 100, EEffect::None, 0, TEXT("Крюк в полдня, зато без приключений.") },
				} },
			{ TEXT("Гадалка у дороги"),
				TEXT("Старушка в платке тасует карты с курами вместо мастей. «Позолоти ручку — скажу, что будет»."),
				{
					{ TEXT("Позолотить ручку (10 зол.)"), 10, 100, EEffect::Glory, 12, TEXT("«Вижу петуха. Большого. Злого. Не ешьте перед боем». Совет ценный.") },
					{ TEXT("Сыграть с ней в карты"), 0, 40, EEffect::Gold, 35, TEXT("Старушка проиграла и рассмеялась: «Давно никто не обыгрывал!»"),
						EEffect::Gold, -10, TEXT("Козырной курицей бьют всё. Минус 10 золотых.") },
					{ TEXT("Пройти мимо"), 0, 100, EEffect::None, 0, TEXT("«Ещё вернётесь», — говорит она в спину.") },
				} },
			{ TEXT("Грибная поляна"),
				TEXT("Поляна светится грибами всех цветов. Друид оживился."),
				{
					{ TEXT("Собрать светящиеся"), 0, 60, EEffect::Gold, 25, TEXT("Алхимик в деревне купил грибы за 25 золотых и долго жал руку."),
						EEffect::Glory, -5, TEXT("Грибы оказались говорящими и обиделись. Слава Хранителей пострадала.") },
					{ TEXT("Отдохнуть на поляне"), 0, 100, EEffect::Glory, 5, TEXT("Хранители проснулись бодрыми. Один гриб остался у Друида в шляпе.") },
					{ TEXT("Растоптать — вдруг ядовитые"), 0, 100, EEffect::Fight, 0, TEXT("") },
				} },
			{ TEXT("Застава кур"),
				TEXT("Шлагбаум, будка и курица в каске. «Предъявите пропуск или яйцо»."),
				{
					{ TEXT("Дать взятку (15 зол.)"), 15, 100, EEffect::None, 0, TEXT("Курица в каске козырнула крылом. Шлагбаум поднят.") },
					{ TEXT("Прорваться"), 0, 100, EEffect::FightMercenaries, 0, TEXT("") },
					{ TEXT("Притвориться курами"), 0, 35, EEffect::Glory, 15, TEXT("Стражник ничего не заподозрил. Это было унизительно и прекрасно."),
						EEffect::FightMercenaries, 0, TEXT("«Почему у вашей курицы меч?» Тревога!") },
				} },
			{ TEXT("Брошенный обоз"),
				TEXT("На обочине — перевёрнутая повозка торговца. Ни души, только следы куриных лап."),
				{
					{ TEXT("Забрать товар"), 0, 100, EEffect::Gold, 30, TEXT("Мешок соли и кошель. Торговцу, если найдётся, вернём. Наверное.") },
					{ TEXT("Пойти по следам"), 0, 100, EEffect::Fight, 0, TEXT("Следы привели к засаде. Курам нужен был не товар, а вы.") },
					{ TEXT("Поставить повозку на колёса"), 0, 100, EEffect::Glory, 8, TEXT("Мелочь, а приятно. Проезжие запомнят.") },
				} },
			{ TEXT("Старый оружейник"),
				TEXT("В покосившейся кузнице дед правит клинок. «Последний заказ отменили. Хотите — отдам дёшево»."),
				{
					{ TEXT("Купить доспех (30 зол.)"), 30, 100, EEffect::Armor, 0, TEXT("Доспех сел как влитой. Дед прослезился.") },
					{ TEXT("Помочь в кузне"), 0, 100, EEffect::Gold, 15, TEXT("Полдня у горна — и 15 золотых за работу.") },
					{ TEXT("Отказаться"), 0, 100, EEffect::None, 0, TEXT("Дед махнул рукой и вернулся к клинку.") },
				} },
			{ TEXT("Слизень-попрошайка"),
				TEXT("Маленький слизень сидит у дороги с табличкой «Не бейте, я хороший»."),
				{
					{ TEXT("Подать монетку (5 зол.)"), 5, 100, EEffect::Glory, 10, TEXT("Слизень радостно булькнул и поделился на двух благодарных слизней.") },
					{ TEXT("Не верить табличке"), 0, 100, EEffect::Fight, 0, TEXT("Табличка врала. Из-за кустов выползли его братья.") },
					{ TEXT("Пройти мимо"), 0, 100, EEffect::None, 0, TEXT("Слизень провожает вас грустным бульканьем.") },
				} },
			{ TEXT("Сокровищница мельника"),
				TEXT("Мельник шепчет: «Под жерновом клад деда. Достанете — поделимся. Только там крысы...»"),
				{
					{ TEXT("Достать клад"), 0, 65, EEffect::Gold, 45, TEXT("Клад нашёлся! Доля Хранителей — 45 золотых."),
						EEffect::Fight, 0, TEXT("«Крысы» оказались наёмниками кур, сторожившими клад для себя.") },
					{ TEXT("Отказаться"), 0, 100, EEffect::None, 0, TEXT("Мельник вздыхает и сыплет муку дальше.") },
				} },
			{ TEXT("Певчий дрозд"),
				TEXT("Дрозд поёт так, что Следопыт заслушался. Похоже, птица учёная."),
				{
					{ TEXT("Научиться песне"), 0, 100, EEffect::Glory, 10, TEXT("Теперь отряд насвистывает в походе. Бодрит.") },
					{ TEXT("Спросить дорогу"), 0, 100, EEffect::Boots, 0, TEXT("Дрозд показал короткую тропу. Ноги сказали спасибо.") },
				} },
			{ TEXT("Святилище земли"),
				TEXT("Камни в кругу, мох на алтаре, тишина. Здесь, говорят, спит дух этих мест."),
				{
					{ TEXT("Принести дар (20 зол.)"), 20, 100, EEffect::Artifact, 0, TEXT("Мох на алтаре расступился. Дух ответил даром на дар.") },
					{ TEXT("Помолиться"), 0, 100, EEffect::Glory, 6, TEXT("Тишина отвечает тишиной. Но на душе легче.") },
					{ TEXT("Поискать монеты паломников"), 0, 50, EEffect::Gold, 25, TEXT("В траве — 25 золотых. Дух, кажется, не заметил."),
						EEffect::Fight, 0, TEXT("Дух заметил.") },
				} },
		};
		return Events;
	}

	/** A random hero of the party (or the Warrior). */
	static FName RandomHero(const UObject* Context)
	{
		const UKeepersPartySubsystem* Party = UKeepersPartySubsystem::Get(Context);
		const TArray<FName> Heroes = Party ? Party->GetParty() : TArray<FName>();
		return Heroes.Num() > 0 ? Heroes[FMath::RandRange(0, Heroes.Num() - 1)] : FName(TEXT("Warrior"));
	}

	/** Applies an effect; returns what changed, for the outcome card. Fights are started by the caller. */
	static FString Apply(UKeepersCampaignSubsystem* Campaign, const UObject* Context, EEffect Effect, int32 Amount)
	{
		switch (Effect)
		{
		case EEffect::Gold:
			if (Amount < 0)
			{
				Campaign->SpendGold(FMath::Min(-Amount, Campaign->GetGold()));
				return FString::Printf(TEXT("−%d золота"), -Amount);
			}
			Campaign->AddGold(Amount);
			return FString::Printf(TEXT("+%d золота"), Amount);
		case EEffect::Artifact:
		{
			const TArray<FName> Rolled = Campaign->RollArtifacts(1);
			if (Rolled.Num() == 0)
			{
				Campaign->AddGold(30);
				return TEXT("Реликвий больше нет — взяли золотом: +30");
			}
			Campaign->AddArtifact(Rolled[0]);
			const FKeepersArtifact* Artifact = UKeepersCampaignSubsystem::FindArtifact(Rolled[0]);
			return FString(TEXT("Реликвия: ")) + (Artifact ? Artifact->Name.ToString() : Rolled[0].ToString());
		}
		case EEffect::Armor:
		case EEffect::Boots:
		{
			const EKeepersGear Gear = Effect == EEffect::Armor ? EKeepersGear::Armor : EKeepersGear::Boots;
			const FName Hero = RandomHero(Context);
			const FKeepersHeroDef* Def = KeepersHeroes::Find(Hero);
			const FString Name = Def ? Def->Name.ToString() : Hero.ToString();
			if (!Campaign->GrantGear(Hero, Gear))
			{
				Campaign->AddGold(20);
				return Name + TEXT(": снаряжение уже лучшее — +20 золота");
			}
			return FString::Printf(TEXT("%s: %s, ранг %d"), *Name, Gear == EKeepersGear::Armor ? TEXT("броня") : TEXT("сапоги"),
				Campaign->GetGearRank(Hero, Gear));
		}
		case EEffect::Glory:
			if (UKeepersPartySubsystem* Party = UKeepersPartySubsystem::Get(Context))
			{
				Party->AddGlory(Amount);
			}
			return Amount < 0 ? FString::Printf(TEXT("−%d славы"), -Amount) : FString::Printf(TEXT("+%d славы"), Amount);
		default:
			return FString();
		}
	}
}

void UKeepersCampaignMapWidget::ShowEvent(int32 NodeIndex)
{
	using namespace KeepersEvents;
	UKeepersCampaignSubsystem* Campaign = UKeepersCampaignSubsystem::Get(this);
	UKeepersCampaignWorldSubsystem* World = GetWorld() ? GetWorld()->GetSubsystem<UKeepersCampaignWorldSubsystem>() : nullptr;
	if (!Campaign || !World)
	{
		return;
	}
	// The same node always holds the same encounter (a reload shows it again).
	const TArray<FEncounter>& Events = All();
	const FEncounter& Event = Events[FMath::Abs(Campaign->GetSeed() * 7 + NodeIndex * 13) % Events.Num()];
	TWeakObjectPtr<UKeepersCampaignMapWidget> WeakThis(this);
	TWeakObjectPtr<UKeepersCampaignSubsystem> WeakCampaign(Campaign);

	TArray<UKeepersChoiceWidget::FOption> Options;
	for (const FOption& Choice : Event.Options)
	{
		UKeepersChoiceWidget::FOption Option;
		const bool bAffordable = Campaign->GetGold() >= Choice.Cost;
		Option.Title = FText::FromString(Choice.Label);
		Option.Description = FText::FromString(!bAffordable ? FString(TEXT("Не хватает золота."))
			: Choice.Chance < 100 ? FString::Printf(TEXT("Повезёт ли? Шанс %d%%."), Choice.Chance)
			: Choice.Effect == EEffect::Fight || Choice.Effect == EEffect::FightMercenaries ? FString(TEXT("Будет бой.")) : FString());
		Option.Color = !bAffordable ? FLinearColor(0.18f, 0.18f, 0.2f)
			: Choice.Effect == EEffect::Fight || Choice.Effect == EEffect::FightMercenaries ? FLinearColor(0.5f, 0.16f, 0.12f) : FLinearColor(0.2f, 0.38f, 0.3f);
		const FString Title = Event.Title;
		Option.OnPicked = [WeakThis, WeakCampaign, Choice, Title, NodeIndex, bAffordable]()
		{
			UKeepersCampaignSubsystem* Run = WeakCampaign.Get();
			UKeepersCampaignMapWidget* Map = WeakThis.Get();
			if (!Run || !Map)
			{
				return;
			}
			if (!bAffordable)
			{
				Map->ShowEvent(NodeIndex);
				return;
			}
			if (Choice.Cost > 0)
			{
				Run->SpendGold(Choice.Cost);
			}
			const bool bLucky = FMath::RandRange(1, 100) <= Choice.Chance;
			const EEffect Effect = bLucky ? Choice.Effect : Choice.FailEffect;
			const FString Text = bLucky ? Choice.Outcome : Choice.FailOutcome;
			if (Effect == EEffect::Fight || Effect == EEffect::FightMercenaries)
			{
				if (Text.IsEmpty())
				{
					Run->StartEventBattle(Effect == EEffect::FightMercenaries);
					return;
				}
				// A word first, then the fight.
				UKeepersChoiceWidget::FOption Go;
				Go.Title = FText::FromString(TEXT("К оружию!"));
				Go.Color = FLinearColor(0.5f, 0.16f, 0.12f);
				Go.OnPicked = [WeakCampaign, Effect]() { if (WeakCampaign.IsValid()) { WeakCampaign->StartEventBattle(Effect == EEffect::FightMercenaries); } };
				if (UKeepersCampaignWorldSubsystem* Screen = Map->GetWorld()->GetSubsystem<UKeepersCampaignWorldSubsystem>())
				{
					Screen->ShowChoice(FText::FromString(Title), FText::FromString(Text), { MoveTemp(Go) });
				}
				return;
			}
			const FString Change = Apply(Run, Map, Effect, bLucky ? Choice.Amount : Choice.FailAmount);
			UKeepersChoiceWidget::FOption Next;
			Next.Title = FText::FromString(TEXT("Дальше"));
			Next.Description = FText::FromString(Change);
			Next.Color = FLinearColor(0.3f, 0.25f, 0.1f);
			Next.OnPicked = [WeakCampaign]() { if (WeakCampaign.IsValid()) { WeakCampaign->CompleteCurrentNode(); } };
			if (UKeepersCampaignWorldSubsystem* Screen = Map->GetWorld()->GetSubsystem<UKeepersCampaignWorldSubsystem>())
			{
				Screen->ShowChoice(FText::FromString(Title), FText::FromString(Text), { MoveTemp(Next) });
			}
		};
		Options.Add(MoveTemp(Option));
	}
	World->ShowChoice(FText::FromString(Event.Title), FText::FromString(Event.Text), MoveTemp(Options));
}
