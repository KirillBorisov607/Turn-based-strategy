#include "KeepersHeroes.h"

#include "Animation/AnimInstance.h"
#include "Components/PointLightComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/DataTable.h"
#include "Engine/GameInstance.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "Kismet/GameplayStatics.h"
#include "KeepersPartyBattle.h"
#include "KeepersReflection.h"

DEFINE_LOG_CATEGORY_STATIC(LogKeepersHeroes, Log, All);

namespace KeepersHeroes
{
	static const FString SaveSlot = TEXT("KeepersParty");
	static const TCHAR* UnitDataTable = TEXT("/Game/Blueprints/Core/Units/Utilities/DT_DefaultUnitDataPerType.DT_DefaultUnitDataPerType");

	static FText T(const TCHAR* Text)
	{
		return FText::FromString(Text);
	}

	static FKeepersLook MakeLook(const TCHAR* Name, EKeepersAnimSet Set, const TCHAR* Body, const TCHAR* Head,
		const TCHAR* WeaponR = TEXT(""), const TCHAR* Shield = TEXT(""), const TCHAR* WeaponL = TEXT(""), const TCHAR* Back = TEXT(""))
	{
		FKeepersLook Look;
		Look.Name = T(Name);
		Look.AnimSet = Set;
		Look.Body = Body;
		Look.Head = Head;
		Look.WeaponR = *WeaponR ? FName(WeaponR) : NAME_None;
		Look.WeaponL = *WeaponL ? FName(WeaponL) : NAME_None;
		Look.Shield = *Shield ? FName(Shield) : NAME_None;
		Look.Back = *Back ? FName(Back) : NAME_None;
		return Look;
	}

	static TArray<FKeepersSpellInfo> BuildSpells()
	{
		auto S = [](const TCHAR* Id, const TCHAR* Name, const TCHAR* Description, const TCHAR* Numbers, EKeepersPose Pose = EKeepersPose::Attack)
		{
			FKeepersSpellInfo Info;
			Info.Id = Id;
			Info.Name = T(Name);
			Info.Description = T(Description);
			Info.Numbers = T(Numbers);
			Info.Pose = Pose;
			return Info;
		};
		using P = EKeepersPose;
		return {
			S(TEXT("MeleeHit"), TEXT("Удар"), TEXT("Простой удар по соседней клетке."), TEXT("2–3 урона · рядом")),
			S(TEXT("SwordSlash"), TEXT("Взмах меча"), TEXT("Широкий удар мечом по соседнему врагу."), TEXT("4–6 урона · рядом")),
			S(TEXT("SwordThrow"), TEXT("Бросок меча"), TEXT("Меч летит по прямой. Нужна видимость цели."), TEXT("3–5 урона · 3–4 клетки")),
			S(TEXT("ShortRangeArrow"), TEXT("Меткий выстрел"), TEXT("Прицельная стрела. Нужна видимость цели."), TEXT("2–5 урона · 3–4 клетки")),
			S(TEXT("LongRangeArrow"), TEXT("Навесной выстрел"), TEXT("Стрела летит дугой через препятствия. Можно дважды за ход."), TEXT("2–3 урона · 3–6 клеток")),
			S(TEXT("DarkWave"), TEXT("Тёмная волна"), TEXT("Удар по всем вокруг себя. Стоит здоровья."), TEXT("4–6 урона · вокруг · −2 HP · перезарядка 3"), P::Cast),
			S(TEXT("Heal"), TEXT("Лечение"), TEXT("Возвращает силы союзнику или себе."), TEXT("+3–5 HP · до 3 клеток"), P::Cast),
			S(TEXT("SlimeBall"), TEXT("Слизь"), TEXT("Комок слизи замедляет цель."), TEXT("2–3 урона · −1–2 ОП · 3–4 клетки")),
			S(TEXT("RunChickenRun"), TEXT("Беги, курица!"), TEXT("Курица вспоминает, что умеет бегать."), TEXT("+4–6 ОП · перезарядка 2"), P::Cast),
			S(TEXT("WindGust"), TEXT("Вихрь"), TEXT("Вихрь бьёт по области и сбивает с толку."), TEXT("3–4 урона · −1 ОД · 3 клетки"), P::Cast),
			S(TEXT("RaiseRock"), TEXT("Поднять скалу"), TEXT("Пустая клетка становится скалой: стеной и укрытием от стрел."), TEXT("1–2 клетки · перезарядка 2"), P::Cast),
			S(TEXT("Gust"), TEXT("Порыв ветра"), TEXT("Толкает цель на клетку от героя. Удар о препятствие ранит, в пропасть — падение."), TEXT("1 урон о препятствие · 1–3 клетки")),
			S(TEXT("Sprout"), TEXT("Росток"), TEXT("Клетка зарастает лесом: идти вдвое дольше, дерево закрывает обзор."), TEXT("до 3 клеток · перезарядка 1"), P::Cast),
			S(TEXT("ShieldBash"), TEXT("Удар щитом"), TEXT("Бьёт щитом и отталкивает врага."), TEXT("2 урона + толчок · рядом · перезарядка 1")),
			S(TEXT("Roots"), TEXT("Корни"), TEXT("Корни держат врага: следующий ход он стоит на месте."), TEXT("1–3 клетки · перезарядка 2"), P::Cast),
			// Abilities of heroes still to come.
			S(TEXT("Fireball"), TEXT("Огненный шар"), TEXT("Взрыв по клетке и вокруг неё. Лес в огне сгорает дотла."), TEXT("3–5 урона · область · 2–4 клетки · перезарядка 2"), P::Cast),
			S(TEXT("IcePath"), TEXT("Ледяная дорожка"), TEXT("Вода под целью и за ней замерзает: по льду можно пройти."), TEXT("1–2 урона · −1 ОП · 1–3 клетки по прямой"), P::Cast),
			S(TEXT("ChainLightning"), TEXT("Цепная молния"), TEXT("Молния перескакивает ещё на двух врагов рядом с целью."), TEXT("3 урона · до 3 целей · 2–4 клетки"), P::Cast),
			S(TEXT("Dash"), TEXT("Рывок"), TEXT("Прыжок на пустую клетку; враги рядом с местом приземления получают удар."), TEXT("2 урона рядом · 2–3 клетки по прямой")),
			S(TEXT("Backstab"), TEXT("Удар в спину"), TEXT("Если рядом с врагом стоит союзник, удар вдвое сильнее."), TEXT("4 урона · ×2 в окружении · рядом")),
			S(TEXT("SmokeBomb"), TEXT("Дымовая шашка"), TEXT("Дым на клетке и вокруг закрывает обзор на два хода."), TEXT("1–3 клетки · перезарядка 3"), P::Cast),
			S(TEXT("Crush"), TEXT("Сокрушить"), TEXT("Мощный удар; скалы и деревья вокруг цели — в щепки."), TEXT("5–7 урона · рядом")),
			S(TEXT("Throw"), TEXT("Бросок врага"), TEXT("Хватает соседа и бросает через себя: в воду — насмерть."), TEXT("2 урона + бросок · рядом")),
			S(TEXT("Boulder"), TEXT("Валун"), TEXT("Катапульта бросает валун: он бьёт по области и остаётся на поле скалой."), TEXT("3–4 урона · область · 3–6 клеток")),
			S(TEXT("RamStrike"), TEXT("Удар тарана"), TEXT("Таран отбрасывает соседа на клетку."), TEXT("4–5 урона · толчок · рядом")),
			S(TEXT("Charge"), TEXT("Натиск"), TEXT("Всадник мчится по прямой и сбивает врагов у места остановки."), TEXT("2–3 урона · 2–4 клетки")),
			S(TEXT("Rage"), TEXT("Ярость"), TEXT("Платит здоровьем и сразу действует ещё раз, +1 к движению."), TEXT("−4 HP · раз за ход · перезарядка 3"), P::Cast),
		};
	}

	static TArray<FKeepersHeroDef> BuildHeroes()
	{
		using A = EKeepersAnimSet;
		TArray<FKeepersHeroDef> Heroes;

		FKeepersHeroDef& Warrior = Heroes.AddDefaulted_GetRef();
		Warrior.Id = TEXT("Warrior");
		Warrior.Name = T(TEXT("Страж"));
		Warrior.Class = T(TEXT("Защитник"));
		Warrior.Role = T(TEXT("Ближний бой · контроль местности"));
		Warrior.Description = T(TEXT("Стоит в первом ряду и не пускает врагов к своим. Поднимает скалы из земли: за ними прячутся от стрел, ими перекрывают проходы."));
		Warrior.UnitType = 0;
		Warrior.Spells = { TEXT("SwordSlash"), TEXT("RaiseRock"), TEXT("ShieldBash") };
		Warrior.LearnableSpells = { TEXT("Throw"), TEXT("Crush"), TEXT("Rage") };
		Warrior.Looks = {
			MakeLook(TEXT("Ветеран"), A::Infantry, TEXT("Unit_Body_05d"), TEXT("Unit_Head_05b"), TEXT("SM_w_sword"), TEXT("SM_shield_05")),
			MakeLook(TEXT("Рыцарь"), A::Infantry, TEXT("Unit_Body_10c"), TEXT("Unit_Head_09b"), TEXT("SM_w_broad_sword"), TEXT("SM_shield_14")),
			MakeLook(TEXT("Паладин"), A::Infantry, TEXT("Unit_Body_11a"), TEXT("Unit_Head_07a"), TEXT("SM_w_mace"), TEXT("SM_shield_20")),
			MakeLook(TEXT("Северянин"), A::Infantry, TEXT("Unit_Body_06d"), TEXT("Unit_Head_07d"), TEXT("SM_w_axe_B"), TEXT("SM_shield_09")),
		};

		FKeepersHeroDef& Ranger = Heroes.AddDefaulted_GetRef();
		Ranger.Id = TEXT("Ranger");
		Ranger.Name = T(TEXT("Следопыт"));
		Ranger.Class = T(TEXT("Стрелок"));
		Ranger.Role = T(TEXT("Дальний бой · толчки"));
		Ranger.Description = T(TEXT("Бьёт издалека и держится от врагов подальше. Порывом ветра сталкивает их с обрывов и друг на друга."));
		Ranger.UnitType = 1;
		Ranger.Spells = { TEXT("LongRangeArrow"), TEXT("Gust"), TEXT("ShortRangeArrow") };
		Ranger.LearnableSpells = { TEXT("Dash"), TEXT("SmokeBomb"), TEXT("ChainLightning") };
		Ranger.Looks = {
			MakeLook(TEXT("Лесной стрелок"), A::Archer, TEXT("Unit_Body_03a"), TEXT("Unit_Head_03c"), TEXT(""), TEXT(""), TEXT("SM_w_long_bow"), TEXT("SM_quiver_A")),
			MakeLook(TEXT("Охотник"), A::Archer, TEXT("Unit_Body_03b"), TEXT("Unit_Head_04c"), TEXT(""), TEXT(""), TEXT("SM_w_recurve_bow"), TEXT("SM_quiver_B")),
			MakeLook(TEXT("Арбалетчик"), A::Crossbow, TEXT("Unit_Body_04d"), TEXT("Unit_Head_05a"), TEXT("SM_w_crossbow"), TEXT(""), TEXT(""), TEXT("SM_quiver_A")),
		};

		FKeepersHeroDef& Priest = Heroes.AddDefaulted_GetRef();
		Priest.Id = TEXT("Priest");
		Priest.Name = T(TEXT("Друид"));
		Priest.Class = T(TEXT("Лекарь"));
		Priest.Role = T(TEXT("Лечение · лес и корни"));
		Priest.Description = T(TEXT("Лечит отряд и растит лес прямо посреди боя. Корнями держит самого опасного врага на месте."));
		Priest.UnitType = 2;
		Priest.Spells = { TEXT("Heal"), TEXT("Sprout"), TEXT("Roots") };
		Priest.LearnableSpells = { TEXT("IcePath"), TEXT("RaiseRock"), TEXT("SmokeBomb") };
		Priest.Looks = {
			MakeLook(TEXT("Отшельник"), A::Staff, TEXT("Unit_Body_12a"), TEXT("Unit_Head_03d"), TEXT("SM_w_staff_A")),
			MakeLook(TEXT("Старейшина"), A::Staff, TEXT("Unit_Body_12c"), TEXT("Unit_Head_03e"), TEXT("SM_w_staff_C")),
			MakeLook(TEXT("Знахарь"), A::Staff, TEXT("Unit_Body_12b"), TEXT("Unit_Head_01e"), TEXT("SM_w_staff_D")),
		};

		FKeepersHeroDef& Mage = Heroes.AddDefaulted_GetRef();
		Mage.Id = TEXT("Mage");
		Mage.Name = T(TEXT("Маг"));
		Mage.Class = T(TEXT("Маг стихий"));
		Mage.Role = T(TEXT("Урон по площади"));
		Mage.Description = T(TEXT("Огнём поджигает лес, льдом замораживает воду, молнией бьёт сразу нескольких."));
		Mage.Unlock = T(TEXT("Пройдите Солёные болота"));
		Mage.UnitType = 9;
		Mage.Spells = { TEXT("Fireball"), TEXT("IcePath"), TEXT("ChainLightning") };
		Mage.LearnableSpells = { TEXT("Gust"), TEXT("RaiseRock"), TEXT("Heal") };
		Mage.Looks = {
			MakeLook(TEXT("Чародей"), A::Staff, TEXT("Unit_Body_12d"), TEXT("Unit_Head_11a"), TEXT("SM_w_staff_B")),
			MakeLook(TEXT("Звездочёт"), A::Staff, TEXT("Unit_Body_12e"), TEXT("Unit_Head_11d"), TEXT("SM_w_staff_D")),
		};
		Mage.Hp = 22;
		Mage.Mp = 3;
		Mage.Ap = 3;

		FKeepersHeroDef& Rogue = Heroes.AddDefaulted_GetRef();
		Rogue.Id = TEXT("Rogue");
		Rogue.Name = T(TEXT("Плут"));
		Rogue.Class = T(TEXT("Ловкач"));
		Rogue.Role = T(TEXT("Мобильность · удары в спину"));
		Rogue.Description = T(TEXT("Проскакивает сквозь врагов и бьёт туда, где не ждут. Дымом прячет отряд от стрел."));
		Rogue.Unlock = T(TEXT("Событие «Таверна»"));
		Rogue.UnitType = 10;
		Rogue.Spells = { TEXT("Dash"), TEXT("Backstab"), TEXT("SmokeBomb") };
		Rogue.LearnableSpells = { TEXT("ShortRangeArrow"), TEXT("Throw"), TEXT("Roots") };
		Rogue.Looks = {
			MakeLook(TEXT("Карманник"), A::Infantry, TEXT("Unit_Body_02d"), TEXT("Unit_Head_02b"), TEXT("SM_w_dagger_A"), TEXT(""), TEXT("SM_w_dagger_B")),
			MakeLook(TEXT("Головорез"), A::Infantry, TEXT("Unit_Body_02b"), TEXT("Unit_Head_04a"), TEXT("SM_w_sabre"), TEXT(""), TEXT("SM_w_dagger_C")),
		};
		Rogue.Hp = 26;
		Rogue.Mp = 6;
		Rogue.Ap = 3;

		FKeepersHeroDef& Barbarian = Heroes.AddDefaulted_GetRef();
		Barbarian.Id = TEXT("Barbarian");
		Barbarian.Name = T(TEXT("Варвар"));
		Barbarian.Class = T(TEXT("Берсерк севера"));
		Barbarian.Role = T(TEXT("Ближний бой · разрушение"));
		Barbarian.Description = T(TEXT("Ломает скалы и деревья, бросает врагов через себя. Раненый только злее."));
		Barbarian.Unlock = T(TEXT("Пройдите Ледяной перевал"));
		Barbarian.UnitType = 11;
		Barbarian.Spells = { TEXT("Crush"), TEXT("Throw"), TEXT("Rage") };
		Barbarian.LearnableSpells = { TEXT("ShieldBash"), TEXT("RaiseRock"), TEXT("Dash") };
		Barbarian.Looks = {
			MakeLook(TEXT("Дикарь"), A::TwoHanded, TEXT("Unit_Body_13a"), TEXT("Unit_Head_12a"), TEXT("SM_w_TH_Axe_A")),
			MakeLook(TEXT("Вождь"), A::TwoHanded, TEXT("Unit_Body_13c"), TEXT("Unit_Head_12c"), TEXT("SM_w_TH_maul")),
		};
		Barbarian.Hp = 45;
		Barbarian.Mp = 3;
		Barbarian.Ap = 3;

		return Heroes;
	}

	static TArray<FKeepersHeroDef> BuildEnemies()
	{
		using A = EKeepersAnimSet;
		TArray<FKeepersHeroDef> Enemies;

		FKeepersHeroDef& Bandit = Enemies.AddDefaulted_GetRef();
		Bandit.Id = TEXT("Bandit");
		Bandit.Name = T(TEXT("Разбойник"));
		Bandit.Class = T(TEXT("Наёмник кур"));
		Bandit.UnitType = 6;
		Bandit.bEnemy = true;
		Bandit.Spells = { TEXT("SwordSlash") };
		Bandit.Looks = {
			MakeLook(TEXT("Головорез"), A::Infantry, TEXT("Unit_Body_02a"), TEXT("Unit_Head_01b"), TEXT("SM_w_axe_A"), TEXT("SM_shield_02")),
			MakeLook(TEXT("Дубинщик"), A::Infantry, TEXT("Unit_Body_04a"), TEXT("Unit_Head_02e"), TEXT("SM_w_club")),
			MakeLook(TEXT("Рубака"), A::Infantry, TEXT("Unit_Body_01b"), TEXT("Unit_Head_04b"), TEXT("SM_w_short_sword"), TEXT("SM_shield_03")),
			MakeLook(TEXT("Наёмник"), A::Infantry, TEXT("Unit_Body_05b"), TEXT("Unit_Head_05c"), TEXT("SM_w_mace_B"), TEXT("SM_shield_07")),
		};

		FKeepersHeroDef& Archer = Enemies.AddDefaulted_GetRef();
		Archer.Id = TEXT("BanditArcher");
		Archer.Name = T(TEXT("Стрелок"));
		Archer.Class = T(TEXT("Наёмник кур"));
		Archer.UnitType = 7;
		Archer.bEnemy = true;
		Archer.Spells = { TEXT("ShortRangeArrow") };
		Archer.Looks = {
			MakeLook(TEXT("Лучник"), A::Archer, TEXT("Unit_Body_03a"), TEXT("Unit_Head_03b"), TEXT(""), TEXT(""), TEXT("SM_w_short_bow"), TEXT("SM_quiver_B")),
			MakeLook(TEXT("Браконьер"), A::Archer, TEXT("Unit_Body_02b"), TEXT("Unit_Head_02d"), TEXT(""), TEXT(""), TEXT("SM_w_short_bow"), TEXT("SM_quiver_A")),
			MakeLook(TEXT("Арбалетчик"), A::Crossbow, TEXT("Unit_Body_04b"), TEXT("Unit_Head_04e"), TEXT("SM_w_crossbow_B")),
		};

		FKeepersHeroDef& Brute = Enemies.AddDefaulted_GetRef();
		Brute.Id = TEXT("Brute");
		Brute.Name = T(TEXT("Громила"));
		Brute.Class = T(TEXT("Наёмник кур"));
		Brute.UnitType = 8;
		Brute.bEnemy = true;
		Brute.Scale = 1.2f;
		Brute.Spells = { TEXT("SwordSlash"), TEXT("ShieldBash") };
		Brute.Looks = {
			MakeLook(TEXT("Громила"), A::TwoHanded, TEXT("Unit_Body_13b"), TEXT("Unit_Head_12b"), TEXT("SM_w_TH_maul_B")),
			MakeLook(TEXT("Дровосек"), A::TwoHanded, TEXT("Unit_Body_13a"), TEXT("Unit_Head_12d"), TEXT("SM_w_TH_Axe_B")),
			MakeLook(TEXT("Молотобоец"), A::TwoHanded, TEXT("Unit_Body_09d"), TEXT("Unit_Head_08b"), TEXT("SM_w_TH_warhammer")),
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
		return Enemies;
	}

	const TArray<FKeepersHeroDef>& Enemies()
	{
		static const TArray<FKeepersHeroDef> List = BuildEnemies();
		return List;
	}

	const FKeepersHeroDef* FindAny(FName Id)
	{
		const FKeepersHeroDef* Hero = Find(Id);
		return Hero ? Hero : Enemies().FindByPredicate([Id](const FKeepersHeroDef& Enemy) { return Enemy.Id == Id; });
	}

	const TArray<FKeepersHeroDef>& All()
	{
		static const TArray<FKeepersHeroDef> Heroes = BuildHeroes();
		return Heroes;
	}

	const FKeepersHeroDef* Find(FName Id)
	{
		return All().FindByPredicate([Id](const FKeepersHeroDef& Hero) { return Hero.Id == Id; });
	}

	const FKeepersHeroDef* FindByUnitType(int32 UnitType)
	{
		auto Matches = [UnitType](const FKeepersHeroDef& Hero) { return Hero.UnitType != INDEX_NONE && Hero.UnitType == UnitType; };
		const FKeepersHeroDef* Hero = All().FindByPredicate(Matches);
		return Hero ? Hero : Enemies().FindByPredicate(Matches);
	}

	const FKeepersSpellInfo* FindSpell(FName Id)
	{
		static const TArray<FKeepersSpellInfo> Spells = BuildSpells();
		return Spells.FindByPredicate([Id](const FKeepersSpellInfo& Spell) { return Spell.Id == Id; });
	}

	FKeepersHeroStats Stats(const FKeepersHeroDef& Hero)
	{
		FKeepersHeroStats Result{ Hero.Hp, Hero.Mp, Hero.Ap };
		const UDataTable* Table = Hero.UnitType != INDEX_NONE ? LoadObject<UDataTable>(nullptr, UnitDataTable) : nullptr;
		uint8* Row = Table ? Table->FindRowUnchecked(Hero.Id) : nullptr;
		if (!Row)
		{
			return Result;
		}
		FProperty* StatsProperty = nullptr;
		void* StatsPtr = KeepersReflection::FindValuePtr(Table->GetRowStruct(), Row, TEXT("Stats"), &StatsProperty);
		if (const FStructProperty* StatsStruct = CastField<FStructProperty>(StatsProperty))
		{
			KeepersReflection::GetInt(StatsStruct->Struct, StatsPtr, TEXT("HP_Max"), Result.Hp);
			KeepersReflection::GetInt(StatsStruct->Struct, StatsPtr, TEXT("MP_Max"), Result.Mp);
			KeepersReflection::GetInt(StatsStruct->Struct, StatsPtr, TEXT("AP_Max"), Result.Ap);
		}
		return Result;
	}

	uint8 UnitTypeOf(const UObject* Unit)
	{
		FProperty* Property = nullptr;
		void* Ptr = Unit ? KeepersReflection::FindValuePtr(Unit->GetClass(), const_cast<UObject*>(Unit), TEXT("UnitType"), &Property) : nullptr;
		const FByteProperty* Byte = CastField<FByteProperty>(Property);
		return Byte ? Byte->GetPropertyValue(Ptr) : 255;
	}

	/** Pose for a value of E_UnitAnimationState, by the entry's name. */
	static bool PoseForState(const FProperty* Property, uint8 Value, EKeepersPose& OutPose)
	{
		const UEnum* Enum = nullptr;
		if (const FByteProperty* Byte = CastField<FByteProperty>(Property))
		{
			Enum = Byte->Enum;
		}
		else if (const FEnumProperty* EnumProperty = CastField<FEnumProperty>(Property))
		{
			Enum = EnumProperty->GetEnum();
		}
		if (!Enum)
		{
			return false;
		}
		const FString Name = Enum->GetDisplayNameTextByValue(Value).ToString().ToLower();
		if (Name.Contains(TEXT("walk")) || Name.Contains(TEXT("move")) || Name.Contains(TEXT("run")))
		{
			OutPose = EKeepersPose::Walk;
		}
		else if (Name.Contains(TEXT("attack")) || Name.Contains(TEXT("cast")) || Name.Contains(TEXT("spell")))
		{
			OutPose = EKeepersPose::Attack;
		}
		else if (Name.Contains(TEXT("hit")) || Name.Contains(TEXT("damage")) || Name.Contains(TEXT("hurt")))
		{
			OutPose = EKeepersPose::Hit;
		}
		else if (Name.Contains(TEXT("death")) || Name.Contains(TEXT("dead")) || Name.Contains(TEXT("die")))
		{
			OutPose = EKeepersPose::Death;
		}
		else
		{
			OutPose = EKeepersPose::Idle;
		}
		return true;
	}
}

// ---------------------------------------------------------------------------------------------------------------------

UKeepersPartySubsystem* UKeepersPartySubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	return GameInstance ? GameInstance->GetSubsystem<UKeepersPartySubsystem>() : nullptr;
}

void UKeepersPartySubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Save = Cast<UKeepersPartySave>(UGameplayStatics::LoadGameFromSlot(KeepersHeroes::SaveSlot, 0));
	if (!Save)
	{
		Save = Cast<UKeepersPartySave>(UGameplayStatics::CreateSaveGameObject(UKeepersPartySave::StaticClass()));
	}
	// Drop heroes that no longer exist or are locked, then fill the squad up with the starting heroes.
	Save->Party.RemoveAll([this](FName Hero) { return !IsUnlocked(Hero); });
	for (const FKeepersHeroDef& Hero : KeepersHeroes::All())
	{
		if (Save->Party.Num() < SquadSize && IsUnlocked(Hero.Id) && !Save->Party.Contains(Hero.Id))
		{
			Save->Party.Add(Hero.Id);
		}
	}
}

bool UKeepersPartySubsystem::IsUnlocked(FName Hero) const
{
	// The first three are open from the start; the others open in the campaign (Unlock tells how).
	const FKeepersHeroDef* Def = KeepersHeroes::Find(Hero);
	return Def && Def->UnitType != INDEX_NONE && (Def->Unlock.IsEmpty() || Save->UnlockedHeroes.Contains(Hero));
}

void UKeepersPartySubsystem::TestSetParty(const UObject* WorldContextObject, const FString& Heroes)
{
	UKeepersPartySubsystem* Party = Get(WorldContextObject);
	if (!Party)
	{
		return;
	}
	TArray<FString> Names;
	Heroes.ParseIntoArray(Names, TEXT(","));
	for (const FName& Hero : TArray<FName>(Party->Save->Party))
	{
		Party->RemoveFromParty(Hero);
	}
	for (const FString& Name : Names)
	{
		Party->UnlockHero(FName(*Name));
		Party->AddToParty(FName(*Name));
	}
}

bool UKeepersPartySubsystem::UnlockHero(FName Hero)
{
	if (!KeepersHeroes::Find(Hero) || Save->UnlockedHeroes.Contains(Hero))
	{
		return false;
	}
	Save->UnlockedHeroes.Add(Hero);
	WriteSave();
	return true;
}

bool UKeepersPartySubsystem::AddToParty(FName Hero)
{
	if (!IsUnlocked(Hero) || Save->Party.Contains(Hero) || Save->Party.Num() >= SquadSize)
	{
		return false;
	}
	Save->Party.Add(Hero);
	WriteSave();
	return true;
}

void UKeepersPartySubsystem::RemoveFromParty(FName Hero)
{
	if (Save->Party.Remove(Hero) > 0)
	{
		WriteSave();
	}
}

FKeepersHeroChoice& UKeepersPartySubsystem::Choice(FName Hero)
{
	if (FKeepersHeroChoice* Found = Save->Choices.FindByPredicate([Hero](const FKeepersHeroChoice& C) { return C.Hero == Hero; }))
	{
		return *Found;
	}
	FKeepersHeroChoice& Added = Save->Choices.AddDefaulted_GetRef();
	Added.Hero = Hero;
	return Added;
}

const FKeepersHeroChoice* UKeepersPartySubsystem::FindChoice(FName Hero) const
{
	return Save->Choices.FindByPredicate([Hero](const FKeepersHeroChoice& C) { return C.Hero == Hero; });
}

int32 UKeepersPartySubsystem::GetLookIndex(FName Hero) const
{
	const FKeepersHeroChoice* Found = FindChoice(Hero);
	return Found ? Found->Look : 0;
}

void UKeepersPartySubsystem::SetLookIndex(FName Hero, int32 Look)
{
	Choice(Hero).Look = Look;
	WriteSave();
}

FName UKeepersPartySubsystem::GetColor(FName Hero) const
{
	const FKeepersHeroChoice* Found = FindChoice(Hero);
	return Found ? Found->Color : FName(TEXT("Blue"));
}

void UKeepersPartySubsystem::SetColor(FName Hero, FName Color)
{
	Choice(Hero).Color = Color;
	WriteSave();
}

FKeepersLook UKeepersPartySubsystem::GetLook(FName Hero) const
{
	const FKeepersHeroDef* Def = KeepersHeroes::Find(Hero);
	if (!Def || Def->Looks.Num() == 0)
	{
		return FKeepersLook();
	}
	const int32 Look = FMath::Clamp(GetLookIndex(Hero), 0, Def->Looks.Num() - 1);
	return Def->Looks[IsLookUnlocked(Hero, Look) ? Look : 0];
}

void UKeepersPartySubsystem::AddGlory(int32 Amount)
{
	Save->Glory = FMath::Max(Save->Glory + Amount, 0);
	WriteSave();
}

bool UKeepersPartySubsystem::IsLookUnlocked(FName Hero, int32 Look) const
{
	return Look <= 0 || Save->UnlockedLooks.Contains(FName(*FString::Printf(TEXT("%s:%d"), *Hero.ToString(), Look)));
}

bool UKeepersPartySubsystem::UnlockLook(FName Hero, int32 Look)
{
	if (IsLookUnlocked(Hero, Look))
	{
		return true;
	}
	if (Save->Glory < LookCost(Look))
	{
		return false;
	}
	Save->Glory -= LookCost(Look);
	Save->UnlockedLooks.Add(FName(*FString::Printf(TEXT("%s:%d"), *Hero.ToString(), Look)));
	WriteSave();
	return true;
}

void UKeepersPartySubsystem::ResetHeroes()
{
	Save->UnlockedHeroes.Reset();
	Save->Party = { TEXT("Warrior"), TEXT("Ranger"), TEXT("Priest") };
	UGameplayStatics::SaveGameToSlot(Save, KeepersHeroes::SaveSlot, 0);
}

void UKeepersPartySubsystem::WriteSave() const
{
	if (bTestSquad)
	{
		return;
	}
	UGameplayStatics::SaveGameToSlot(Save, KeepersHeroes::SaveSlot, 0);
}

// ---------------------------------------------------------------------------------------------------------------------

TStatId UKeepersHeroLookSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UKeepersHeroLookSubsystem, STATGROUP_Tickables);
}

bool UKeepersHeroLookSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UKeepersHeroLookSubsystem::Tick(float DeltaTime)
{
	UWorld* World = GetWorld();
	if (!World || !World->HasBegunPlay())
	{
		return;
	}

	// New units appear during battles (slimes split, chickens call for help), so look for them now and then.
	ScanTimer -= DeltaTime;
	if (ScanTimer <= 0.f)
	{
		ScanTimer = 0.5f;
		for (TActorIterator<ACharacter> It(World); It; ++It)
		{
			ACharacter* Unit = *It;
			if (!Seen.Contains(Unit))
			{
				Seen.Add(Unit);
				Dress(Unit);
			}
		}
	}

	// Mirror the hidden mesh's animation state on the figure.
	for (int32 I = Dressed.Num() - 1; I >= 0; --I)
	{
		FDressed& Entry = Dressed[I];
		ACharacter* Unit = Entry.Unit.Get();
		UKeepersFigureComponent* Figure = Entry.Figure.Get();
		if (!Unit || !Figure)
		{
			Dressed.RemoveAtSwap(I);
			continue;
		}
		UAnimInstance* Anim = Unit->GetMesh() ? Unit->GetMesh()->GetAnimInstance() : nullptr;
		FProperty* Property = nullptr;
		void* Ptr = Anim ? KeepersReflection::FindValuePtr(Anim->GetClass(), Anim, TEXT("AnimationState"), &Property) : nullptr;
		if (!Ptr)
		{
			continue;
		}
		const uint8 State = *static_cast<const uint8*>(Ptr);
		if (State == Entry.LastState)
		{
			continue;
		}
		Entry.LastState = State;
		EKeepersPose Pose;
		if (KeepersHeroes::PoseForState(Property, State, Pose))
		{
			if (Pose == EKeepersPose::Attack && Figure->GetLook().AnimSet == EKeepersAnimSet::Staff)
			{
				Pose = EKeepersPose::Cast;
			}
			Figure->Play(Pose);
		}
	}
}

void UKeepersHeroLookSubsystem::LookFor(const FKeepersHeroDef& Hero, int32 Team, int32 Variant, FKeepersLook& OutLook, FName& OutColor) const
{
	static const FName EnemyColors[] = { TEXT("Red"), TEXT("Purple"), TEXT("Yellow") };
	if (Hero.bEnemy)
	{
		OutLook = Hero.Looks[Variant % Hero.Looks.Num()];
		OutColor = Team == 0 ? FName(TEXT("Blue")) : EnemyColors[(FMath::Max(Team, 1) - 1) % UE_ARRAY_COUNT(EnemyColors)];
		return;
	}
	const UKeepersPartySubsystem* Party = UKeepersPartySubsystem::Get(GetWorld());
	if (Team == 0 && Party)
	{
		OutLook = Party->GetLook(Hero.Id);
		OutColor = Party->GetColor(Hero.Id);
		return;
	}
	// Enemy heroes: another outfit than the player's and a colour per enemy team.
	const int32 PlayerLook = Party ? Party->GetLookIndex(Hero.Id) : 0;
	OutLook = Hero.Looks[(PlayerLook + 1) % Hero.Looks.Num()];
	OutColor = EnemyColors[(FMath::Max(Team, 1) - 1) % UE_ARRAY_COUNT(EnemyColors)];
}

UTextureRenderTarget2D* UKeepersHeroLookSubsystem::GetPortrait(FName HeroId, bool bEnemy)
{
	const FName Key(*FString::Printf(TEXT("%s_%d"), *HeroId.ToString(), bEnemy ? 1 : 0));
	if (const TObjectPtr<UTextureRenderTarget2D>* Found = Portraits.Find(Key))
	{
		return Found->Get();
	}
	const FKeepersHeroDef* Hero = KeepersHeroes::FindAny(HeroId);
	UWorld* World = GetWorld();
	if (!Hero || Hero->Looks.Num() == 0 || !World)
	{
		return nullptr;
	}
	if (!Studio)
	{
		// High above the field, out of every battle camera.
		Studio = World->SpawnActor<AActor>(AActor::StaticClass(), FTransform(FVector(0.f, 0.f, 9000.f)));
		USceneComponent* Root = NewObject<USceneComponent>(Studio, TEXT("Root"));
		Studio->SetRootComponent(Root);
		Root->RegisterComponent();
		// A bare actor has no root at spawn, so its spawn location is lost: place it now.
		Studio->SetActorLocation(FVector(0.f, 0.f, 9000.f));
		UPointLightComponent* Light = NewObject<UPointLightComponent>(Studio);
		Light->SetupAttachment(Root);
		Light->SetRelativeLocation(FVector(-300.f, -500.f, 350.f));
		Light->SetIntensity(80000.f);
		Light->SetAttenuationRadius(6000.f);
		Light->SetCastShadows(false);
		Light->RegisterComponent();
	}
	const FVector Spot = Studio->GetActorLocation() + FVector(Portraits.Num() * 400.f, 0.f, 0.f);
	FKeepersLook Look;
	FName Color;
	LookFor(*Hero, bEnemy ? 1 : 0, 0, Look, Color);
	UKeepersFigureComponent* Figure = NewObject<UKeepersFigureComponent>(Studio);
	Figure->SetupAttachment(Studio->GetRootComponent());
	Figure->SetWorldLocation(Spot);
	// Facing -Y, towards the camera.
	Figure->SetWorldRotation(FRotator(0.f, 180.f, 0.f));
	Figure->RegisterComponent();
	Figure->SetLook(Look);
	Figure->SetColor(Color);

	UTextureRenderTarget2D* Target = NewObject<UTextureRenderTarget2D>(this);
	Target->InitCustomFormat(192, 192, PF_B8G8R8A8, true);
	USceneCaptureComponent2D* Capture = NewObject<USceneCaptureComponent2D>(Studio);
	Capture->SetupAttachment(Studio->GetRootComponent());
	// Head and shoulders; riders and machines whole, from further away and a little to the side.
	const bool bBig = Look.AnimSet == EKeepersAnimSet::Cavalry || Look.AnimSet == EKeepersAnimSet::Catapult || Look.AnimSet == EKeepersAnimSet::Ram;
	if (bBig)
	{
		Figure->SetWorldRotation(FRotator(0.f, 215.f, 0.f));
	}
	Capture->SetWorldLocationAndRotation(Spot + (bBig ? FVector(0.f, -560.f, 230.f) : FVector(0.f, -200.f, 140.f)), FRotator(bBig ? -12.f : -6.f, 90.f, 0.f));
	Capture->FOVAngle = 30.f;
	Capture->TextureTarget = Target;
	Capture->CaptureSource = SCS_FinalColorLDR;
	Capture->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
	Capture->ShowOnlyComponents.Append(Figure->GetParts());
	Capture->ShowFlags.SetFog(false);
	Capture->ShowFlags.SetEyeAdaptation(false);
	Capture->bCaptureEveryFrame = true;
	Capture->RegisterComponent();
	Portraits.Add(Key, Target);
	return Target;
}

void UKeepersHeroLookSubsystem::Dress(ACharacter* Unit)
{
	USkeletalMeshComponent* Mesh = Unit ? Unit->GetMesh() : nullptr;
	const FKeepersHeroDef* Hero = KeepersHeroes::FindByUnitType(KeepersHeroes::UnitTypeOf(Unit));
	if (!Mesh || !Mesh->GetSkeletalMeshAsset() || !Hero || Hero->Looks.Num() == 0)
	{
		return;
	}

	int32 Team = 0;
	KeepersReflection::GetInt(Unit->GetClass(), Unit, TEXT("TeamIndex"), Team);
	FKeepersLook Look;
	FName Color;
	// Enemies of one kind wear different outfits: pick by the unit.
	LookFor(*Hero, Team, static_cast<int32>(GetTypeHash(Unit->GetName()) % 97), Look, Color);
	// A recruit wears another outfit of the class than the hero does.
	const UKeepersPartySubsystem* Party = UKeepersPartySubsystem::Get(Unit);
	if (Party && Hero->Looks.Num() > 1 && UKeepersPartyBattleSubsystem::IsRecruit(Unit))
	{
		Look = Hero->Looks[(Party->GetLookIndex(Hero->Id) + 1 + FMath::RandRange(0, Hero->Looks.Num() - 2)) % Hero->Looks.Num()];
	}

	// The old mesh keeps animating out of sight: its Anim Blueprint holds the state the figure mirrors.
	Mesh->SetVisibility(false, false);
	Mesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;

	UKeepersFigureComponent* Figure = NewObject<UKeepersFigureComponent>(Unit, TEXT("KeepersFigure"));
	Figure->SetupAttachment(Mesh);
	// Same facing (+Y) as the old meshes; scale to their height.
	const FBoxSphereBounds Bounds = Mesh->GetSkeletalMeshAsset()->GetBounds();
	const float OldHeight = Bounds.Origin.Z + Bounds.BoxExtent.Z;
	Figure->SetRelativeScale3D(FVector(FMath::Max(OldHeight, 50.f) * 0.85f * Hero->Scale / UKeepersFigureComponent::FigureHeight));
	Figure->RegisterComponent();
	Figure->SetLook(Look);
	Figure->SetColor(Color);
	Unit->AddInstanceComponent(Figure);

	Dressed.Add({ Unit, Figure, 255 });
	UE_LOG(LogKeepersHeroes, Log, TEXT("Dressed %s as %s (%s, team %d)"), *Unit->GetName(), *Hero->Id.ToString(), *Look.Name.ToString(), Team);
}
