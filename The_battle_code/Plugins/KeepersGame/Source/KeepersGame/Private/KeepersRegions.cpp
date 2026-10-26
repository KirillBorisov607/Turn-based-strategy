#include "KeepersRegions.h"

namespace KeepersRegions
{
	/** E_UnitType values of the enemies. */
	static constexpr uint8 Slime = 3;
	static constexpr uint8 Chicken = 4;
	static constexpr uint8 Bandit = 6;
	static constexpr uint8 Archer = 7;
	static constexpr uint8 Brute = 8;
	static constexpr uint8 Cavalry = 12;
	static constexpr uint8 Catapult = 13;
	static constexpr uint8 Ram = 14;
	static constexpr uint8 Shaman = 15;

	static FText T(const TCHAR* Text)
	{
		return FText::FromString(Text);
	}

	static TArray<FKeepersRegion> Build()
	{
		TArray<FKeepersRegion> Regions;

		FKeepersRegion& Dales = Regions.AddDefaulted_GetRef();
		Dales.Id = TEXT("Dales");
		Dales.Name = T(TEXT("Зелёные долы"));
		Dales.Numeral = T(TEXT("регион I"));
		Dales.Intro = T(TEXT("Холмы, мосты и подозрительно много кур. Первый Камень земель где-то у слизней."));
		Dales.Liberated = T(TEXT("Король слизней растёкся лужей, а в луже блестит первый Камень земель. Долы снова зеленеют."));
		Dales.Band = { Bandit, Archer };
		Dales.EliteBand = { Brute, Bandit, Archer };
		Dales.BossBand = { Archer, Bandit };
		Dales.BossName = T(TEXT("Король слизней"));
		Dales.BossType = Slime;
		Dales.BossScale = 1.8f;
		Dales.BossHp = 24;

		FKeepersRegion& Forest = Regions.AddDefaulted_GetRef();
		Forest.Id = TEXT("Forest");
		Forest.Name = T(TEXT("Шепчущий лес"));
		Forest.Numeral = T(TEXT("регион II"));
		Forest.Intro = T(TEXT("Деревья шепчут гадости и растут прямо на глазах. Шаманы кур знают, как их подгонять."));
		Forest.Liberated = T(TEXT("Старый Пень рассыпался трухой. Под корнями — второй Камень и свежие куриные следы."));
		Forest.Forests = 10;
		Forest.Band = { Shaman, Bandit };
		Forest.EliteBand = { Shaman, Brute, Archer };
		Forest.BossBand = { Shaman, Shaman, Bandit };
		Forest.BossName = T(TEXT("Старый Пень"));
		Forest.bBossTotem = true;
		Forest.bBossGrowsForest = true;

		FKeepersRegion& Marsh = Regions.AddDefaulted_GetRef();
		Marsh.Id = TEXT("Marsh");
		Marsh.Name = T(TEXT("Солёные болота"));
		Marsh.Numeral = T(TEXT("регион III"));
		Marsh.Intro = T(TEXT("Топь отнимает шаг и затягивает тех, кто стоит на месте. Ведьма болот что-то знает о курах."));
		Marsh.Liberated = T(TEXT("Болотная Мать сдалась и рассказала: камни собирает Генерал Клюв. На шум из тумана вышел Маг стихий."));
		Marsh.Surface = EKeepersSurface::Marsh;
		Marsh.Band = { Bandit, Archer, Slime };
		Marsh.EliteBand = { Shaman, Brute, Archer };
		Marsh.BossBand = { Archer, Archer, Brute };
		Marsh.BossName = T(TEXT("Болотная Мать"));
		Marsh.BossType = Shaman;
		Marsh.BossScale = 1.35f;
		Marsh.BossHp = 22;
		Marsh.Unlocks = TEXT("Mage");

		FKeepersRegion& Pass = Regions.AddDefaulted_GetRef();
		Pass.Id = TEXT("Pass");
		Pass.Name = T(TEXT("Ледяной перевал"));
		Pass.Numeral = T(TEXT("регион IV"));
		Pass.Intro = T(TEXT("Кто закончит ход на льду, тот скользит дальше. Всадники кур здесь как дома."));
		Pass.Liberated = T(TEXT("Йети-вождь проиграл и в знак уважения отдал камень. С ним пришёл Варвар — ему тут скучно."));
		Pass.Surface = EKeepersSurface::Ice;
		Pass.Band = { Cavalry, Archer };
		Pass.EliteBand = { Cavalry, Cavalry, Archer };
		Pass.BossBand = { Catapult, Archer, Cavalry };
		Pass.BossName = T(TEXT("Йети-вождь"));
		Pass.BossType = Brute;
		Pass.BossScale = 1.3f;
		Pass.BossHp = 26;
		Pass.Unlocks = TEXT("Barbarian");

		FKeepersRegion& City = Regions.AddDefaulted_GetRef();
		City.Id = TEXT("City");
		City.Name = T(TEXT("Город кур"));
		City.Numeral = T(TEXT("регион V"));
		City.Intro = T(TEXT("Баррикады, тараны и катапульты. Куры готовились к этой встрече давно."));
		City.Liberated = T(TEXT("Генерал Клюв бежал, бросив пятый камень. Дорога к Сердцу земель открыта."));
		City.Barricades = 6;
		City.Band = { Bandit, Archer, Cavalry };
		City.EliteBand = { Ram, Catapult, Bandit };
		City.BossBand = { Ram, Catapult, Cavalry, Archer };
		City.BossName = T(TEXT("Генерал Клюв"));
		City.BossType = Cavalry;
		City.BossScale = 1.2f;
		City.BossHp = 24;

		FKeepersRegion& Heart = Regions.AddDefaulted_GetRef();
		Heart.Id = TEXT("Heart");
		Heart.Name = T(TEXT("Сердце земель"));
		Heart.Numeral = T(TEXT("регион VI"));
		Heart.Intro = T(TEXT("Без камней земля плывёт: скалы встают и рушатся, лес растёт за два хода. Великий Петух ждёт."));
		Heart.Liberated = T(TEXT("Великий Петух повержен. Шесть Камней земель снова вместе — осталось решить, что с ними делать."));
		Heart.bShifting = true;
		Heart.Band = { Cavalry, Shaman, Brute };
		Heart.EliteBand = { Catapult, Ram, Cavalry, Shaman };
		Heart.BossBand = { Catapult, Ram, Cavalry, Shaman, Brute };
		Heart.BossName = T(TEXT("Великий Петух"));
		Heart.BossType = Chicken;
		Heart.BossScale = 2.4f;
		Heart.BossHp = 50;
		return Regions;
	}

	const TArray<FKeepersRegion>& All()
	{
		static const TArray<FKeepersRegion> Regions = Build();
		return Regions;
	}

	const FKeepersRegion& Get(int32 Index)
	{
		const TArray<FKeepersRegion>& Regions = All();
		return Regions[FMath::Clamp(Index, 0, Regions.Num() - 1)];
	}
}
