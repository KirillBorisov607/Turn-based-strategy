#include "KeepersBattleObjective.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "KeepersReflection.h"
#include "KeepersStyle.h"
#include "KeepersHeroes.h"
#include "KeepersJuice.h"
#include "KeepersLevelKit.h"
#include "KeepersTerrainSubsystem.h"
#include "Components/PointLightComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"

DEFINE_LOG_CATEGORY_STATIC(LogKeepersObjective, Log, All);

namespace KeepersObjective
{
	static UObject* FindCombatSystem(UWorld* World)
	{
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			if (It->GetClass()->GetName() == TEXT("BP_CombatSystem_C"))
			{
				return *It;
			}
		}
		return nullptr;
	}

	static bool IsAlive(UObject* Unit)
	{
		bool bAlive = false;
		KeepersReflection::FCall Call(Unit, TEXT("IsAlive"));
		if (Call.Call())
		{
			Call.Get<bool>(TEXT("IsAlive"), bAlive);
		}
		return bAlive;
	}

	static EKeepersObjectiveKind Requested = EKeepersObjectiveKind::None;

	static int32 TeamOf(const UObject* Unit)
	{
		int32 Team = 0;
		KeepersReflection::GetInt(Unit->GetClass(), Unit, TEXT("TeamIndex"), Team);
		return Team;
	}

	struct FTile
	{
		uint8 Type = 0;
		FVector Location = FVector::ZeroVector;
		UObject* Unit = nullptr;
	};

	/** BP_Grid.GridTiles: type, place and unit of every cell. */
	static TMap<FIntPoint, FTile> ReadTiles(UObject* Grid)
	{
		TMap<FIntPoint, FTile> Tiles;
		FProperty* TilesProperty = nullptr;
		void* TilesValue = Grid ? KeepersReflection::FindValuePtr(Grid->GetClass(), Grid, TEXT("GridTiles"), &TilesProperty) : nullptr;
		FMapProperty* Map = CastField<FMapProperty>(TilesProperty);
		const FStructProperty* Value = Map ? CastField<FStructProperty>(Map->ValueProp) : nullptr;
		if (!Value)
		{
			return Tiles;
		}
		FScriptMapHelper Helper(Map, TilesValue);
		for (FScriptMapHelper::FIterator It(Helper); It; ++It)
		{
			void* Data = Helper.GetValuePtr(It);
			FTile Tile;
			KeepersReflection::GetEnumByte(Value->Struct, Data, TEXT("Type"), Tile.Type);
			if (const FTransform* Transform = static_cast<const FTransform*>(KeepersReflection::FindValuePtr(Value->Struct, Data, TEXT("Transform"))))
			{
				Tile.Location = Transform->GetLocation();
			}
			FProperty* UnitProperty = nullptr;
			if (void* UnitPtr = KeepersReflection::FindValuePtr(Value->Struct, Data, TEXT("UnitOnTile"), &UnitProperty))
			{
				if (const FObjectPropertyBase* Object = CastField<FObjectPropertyBase>(UnitProperty))
				{
					Tile.Unit = Object->GetObjectPropertyValue(UnitPtr);
				}
			}
			Tiles.Add(*reinterpret_cast<const FIntPoint*>(Helper.GetKeyPtr(It)), Tile);
		}
		return Tiles;
	}

	static UStaticMeshComponent* Shape(AActor* Owner, const TCHAR* Mesh, const FLinearColor& Color)
	{
		UStaticMeshComponent* Component = NewObject<UStaticMeshComponent>(Owner);
		Component->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, Mesh));
		Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Component->SetCastShadow(false);
		if (UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")), Owner))
		{
			Material->SetVectorParameterValue(TEXT("Color"), Color);
			Component->SetMaterial(0, Material);
		}
		Component->SetupAttachment(Owner->GetRootComponent());
		Component->RegisterComponent();
		return Component;
	}

	static FText Plural(int32 N, const TCHAR* One, const TCHAR* Few, const TCHAR* Many)
	{
		const int32 Mod10 = N % 10;
		const int32 Mod100 = N % 100;
		const TCHAR* Word = (Mod10 == 1 && Mod100 != 11) ? One : (Mod10 >= 2 && Mod10 <= 4 && (Mod100 < 12 || Mod100 > 14)) ? Few : Many;
		return FText::FromString(FString::Printf(TEXT("%d %s"), N, Word));
	}
}

void UKeepersObjectiveWidget::SetLines(const FText& Title, const FText& Progress)
{
	PendingTitle = Title;
	PendingProgress = Progress;
	if (TitleText)
	{
		TitleText->SetText(Title);
		ProgressText->SetText(Progress);
		ProgressText->SetVisibility(Progress.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
}

TSharedRef<SWidget> UKeepersObjectiveWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		UBorder* Box = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Box"));
		Box->SetBrush(KeepersStyle::Panel(KeepersStyle::Ink, 12.f));
		Box->SetPadding(FMargin(22.f, 6.f));

		UVerticalBox* Lines = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Lines"));
		TitleText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Title"));
		ProgressText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Progress"));
		TitleText->SetFont(KeepersStyle::Heading(24));
		TitleText->SetColorAndOpacity(FSlateColor(KeepersStyle::Gold));
		ProgressText->SetFont(KeepersStyle::Body(14));
		ProgressText->SetColorAndOpacity(FSlateColor(KeepersStyle::Text));
		if (UVerticalBoxSlot* TitleSlot = Lines->AddChildToVerticalBox(TitleText))
		{
			TitleSlot->SetHorizontalAlignment(HAlign_Center);
		}
		if (UVerticalBoxSlot* ProgressSlot = Lines->AddChildToVerticalBox(ProgressText))
		{
			ProgressSlot->SetHorizontalAlignment(HAlign_Center);
		}
		Box->SetContent(Lines);

		// Full-screen, click-through root that keeps the panel at the top centre.
		UBorder* Screen = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Screen"));
		Screen->SetBrushColor(FLinearColor::Transparent);
		Screen->SetHorizontalAlignment(HAlign_Center);
		Screen->SetVerticalAlignment(VAlign_Top);
		Screen->SetPadding(FMargin(0.f, 12.f, 0.f, 0.f));
		Screen->SetVisibility(ESlateVisibility::HitTestInvisible);
		Screen->SetContent(Box);
		WidgetTree->RootWidget = Screen;
		TitleText->SetText(PendingTitle);
		ProgressText->SetText(PendingProgress);
	}
	return Super::RebuildWidget();
}

AKeepersBattleObjective* AKeepersBattleObjective::Find(const UWorld* World)
{
	static TWeakObjectPtr<AKeepersBattleObjective> Cached;
	static TWeakObjectPtr<const UWorld> CachedWorld;
	static uint64 Frame = MAX_uint64;
	if (CachedWorld.Get() != World || Frame != GFrameCounter)
	{
		CachedWorld = World;
		Frame = GFrameCounter;
		Cached.Reset();
		for (TActorIterator<AKeepersBattleObjective> It(const_cast<UWorld*>(World)); It; ++It)
		{
			Cached = *It;
			break;
		}
	}
	return Cached.Get();
}

void AKeepersBattleObjective::RequestForNextBattle(EKeepersObjectiveKind Kind)
{
	KeepersObjective::Requested = Kind;
}

EKeepersObjectiveKind AKeepersBattleObjective::LastGiven = EKeepersObjectiveKind::None;

EKeepersObjectiveKind AKeepersBattleObjective::TakeRequest()
{
	const EKeepersObjectiveKind Kind = KeepersObjective::Requested;
	KeepersObjective::Requested = EKeepersObjectiveKind::None;
	return Kind;
}

bool UKeepersObjectiveSpawner::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UKeepersObjectiveSpawner::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	const EKeepersObjectiveKind Kind = AKeepersBattleObjective::TakeRequest();
	AKeepersBattleObjective::LastGiven = Kind;
	if (Kind == EKeepersObjectiveKind::None || !KeepersObjective::FindCombatSystem(&InWorld))
	{
		return;
	}
	// A level with an objective of its own keeps it.
	for (TActorIterator<AKeepersBattleObjective> It(&InWorld); It; ++It)
	{
		return;
	}
	AKeepersBattleObjective* Objective = InWorld.SpawnActor<AKeepersBattleObjective>();
	if (!Objective)
	{
		return;
	}
	Objective->bReachBeacon = Kind == EKeepersObjectiveKind::Reach;
	Objective->bDefeatLeader = Kind == EKeepersObjectiveKind::Leader || Kind == EKeepersObjectiveKind::Totem;
	Objective->bTotem = Kind == EKeepersObjectiveKind::Totem;
	Objective->SurviveTurns = Kind == EKeepersObjectiveKind::Survive ? 4 : 0;
	UE_LOG(LogKeepersObjective, Log, TEXT("Objective %d given to %s"), static_cast<int32>(Kind), *InWorld.GetMapName());
}

void AKeepersBattleObjective::SetUp()
{
	bSetUp = true;
	UObject* Grid = KeepersReflection::GetObject(CombatSystem, TEXT("Grid"));
	const TMap<FIntPoint, KeepersObjective::FTile> Tiles = KeepersObjective::ReadTiles(Grid);
	FKeepersLevelGrid Shape;
	if (const uint8* GridShape = Grid ? static_cast<const uint8*>(KeepersReflection::FindValuePtr(Grid->GetClass(), Grid, TEXT("GridShape"))) : nullptr)
	{
		Shape.Shape = *GridShape;
	}
	float Step = 100.f;
	if (const FVector* Size = Grid ? static_cast<const FVector*>(KeepersReflection::FindValuePtr(Grid->GetClass(), Grid, TEXT("GridTileSize"))) : nullptr)
	{
		Step = Size->Z;
	}
	auto Walkable = [](uint8 Type) { return Type == 1 || Type == 3 || Type == 4; };

	if (bReachBeacon && BeaconIndex.X < 0)
	{
		// The farthest cell the heroes can walk to: a breadth-first walk from all of them.
		TMap<FIntPoint, int32> Steps;
		TArray<FIntPoint> Queue;
		for (const TPair<FIntPoint, KeepersObjective::FTile>& Pair : Tiles)
		{
			if (Pair.Value.Unit && KeepersObjective::TeamOf(Pair.Value.Unit) == 0)
			{
				Steps.Add(Pair.Key, 0);
				Queue.Add(Pair.Key);
			}
		}
		for (int32 Head = 0; Head < Queue.Num(); ++Head)
		{
			const FIntPoint Index = Queue[Head];
			const KeepersObjective::FTile& From = Tiles.FindChecked(Index);
			for (const FIntPoint& Next : KeepersLevelKit::Neighbors(Shape, Index))
			{
				const KeepersObjective::FTile* To = Tiles.Find(Next);
				if (To && Walkable(To->Type) && !Steps.Contains(Next) && FMath::Abs(To->Location.Z - From.Location.Z) <= Step + 1.f)
				{
					Steps.Add(Next, Steps[Index] + 1);
					Queue.Add(Next);
				}
			}
		}
		int32 Best = -1;
		for (const TPair<FIntPoint, int32>& Pair : Steps)
		{
			if (Pair.Value > Best && !Tiles[Pair.Key].Unit && Tiles[Pair.Key].Type == 1)
			{
				Best = Pair.Value;
				BeaconIndex = Pair.Key;
			}
		}
	}
	if (bReachBeacon && Tiles.Contains(BeaconIndex))
	{
		const FVector Place = Tiles[BeaconIndex].Location;
		Beacon = KeepersObjective::Shape(this, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"), FLinearColor(1.f, 0.78f, 0.25f));
		Beacon->SetWorldLocation(Place + FVector(0.f, 0.f, 200.f));
		Beacon->SetWorldScale3D(FVector(0.3f, 0.3f, 4.f));
		// A golden plate on the cell and a gem turning over the pillar: seen from anywhere on the field.
		UStaticMeshComponent* Plate = KeepersObjective::Shape(this, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"), FLinearColor(1.f, 0.85f, 0.35f));
		Plate->SetWorldLocation(Place + FVector(0.f, 0.f, 3.f));
		Plate->SetWorldScale3D(FVector(1.6f, 1.6f, 0.04f));
		BeaconGem = KeepersObjective::Shape(this, TEXT("/Engine/BasicShapes/Cube.Cube"), FLinearColor(1.f, 0.6f, 0.15f));
		BeaconGem->SetWorldScale3D(FVector(0.45f));
		BeaconLight = NewObject<UPointLightComponent>(this);
		BeaconLight->SetupAttachment(RootComponent);
		BeaconLight->RegisterComponent();
		BeaconLight->SetWorldLocation(Place + FVector(0.f, 0.f, 260.f));
		BeaconLight->SetLightColor(FLinearColor(1.f, 0.75f, 0.3f));
		BeaconLight->SetIntensity(9000.f);
		BeaconLight->SetAttenuationRadius(700.f);
		BeaconLight->SetCastShadows(false);
		UE_LOG(LogKeepersObjective, Log, TEXT("Beacon at (%d,%d)"), BeaconIndex.X, BeaconIndex.Y);
	}
	else
	{
		bReachBeacon = false;
	}

	if (bDefeatLeader && bTotem && !Leader)
	{
		// The totem stands where the enemies are, as far from the heroes as it gets.
		float Best = -1.f;
		for (const TPair<FIntPoint, KeepersObjective::FTile>& Pair : Tiles)
		{
			// A walker: fliers hover over water.
			if (!Pair.Value.Unit || KeepersObjective::TeamOf(Pair.Value.Unit) == 0 || UKeepersTerrainSubsystem::CanUnitStandOn(Pair.Value.Unit, 5))
			{
				continue;
			}
			float Nearest = MAX_flt;
			for (const TPair<FIntPoint, KeepersObjective::FTile>& Other : Tiles)
			{
				if (Other.Value.Unit && KeepersObjective::TeamOf(Other.Value.Unit) == 0)
				{
					Nearest = FMath::Min(Nearest, FVector::Dist2D(Other.Value.Location, Pair.Value.Location));
				}
			}
			if (Nearest > Best)
			{
				Best = Nearest;
				Leader = Cast<AActor>(Pair.Value.Unit);
			}
		}
	}
	if (bDefeatLeader && !Leader)
	{
		// The toughest enemy leads.
		int32 BestHp = -1;
		for (const TPair<FIntPoint, KeepersObjective::FTile>& Pair : Tiles)
		{
			int32 Hp = 0;
			if (Pair.Value.Unit && KeepersObjective::TeamOf(Pair.Value.Unit) != 0 && KeepersReflection::GetUnitStat(Pair.Value.Unit, TEXT("HP_Max"), Hp) && Hp > BestHp)
			{
				BestHp = Hp;
				Leader = Cast<AActor>(Pair.Value.Unit);
			}
		}
		if (Leader)
		{
			// Tougher than its kind.
			const int32 Bonus = FMath::Max(BestHp / 2, 4);
			KeepersReflection::AddUnitStat(Leader, TEXT("HP_Max"), Bonus);
			KeepersReflection::AddUnitStat(Leader, TEXT("HP_Current"), Bonus);
		}
	}
	if (bDefeatLeader && bTotem && Leader)
	{
		MakeTotem();
	}
	else if (bDefeatLeader && Leader)
	{
		const FKeepersHeroDef* Def = KeepersHeroes::FindByUnitType(KeepersHeroes::UnitTypeOf(Leader));
		LeaderName = Def ? Def->Name.ToString() : FString();
		Crown = KeepersObjective::Shape(this, TEXT("/Engine/BasicShapes/Cone.Cone"), FLinearColor(1.f, 0.8f, 0.2f));
		Crown->SetWorldScale3D(FVector(0.55f, 0.55f, -0.5f));
		UE_LOG(LogKeepersObjective, Log, TEXT("Leader %s"), *Leader->GetName());
	}
	else
	{
		bDefeatLeader = false;
	}
}

void AKeepersBattleObjective::MakeTotem()
{
	// Still and tough: no movement, no actions, 26 health whatever the unit was.
	auto SetStat = [this](const TCHAR* Stat, int32 Value)
	{
		int32 Old = 0;
		if (KeepersReflection::GetUnitStat(Leader, Stat, Old))
		{
			KeepersReflection::AddUnitStat(Leader, Stat, Value - Old);
		}
	};
	SetStat(TEXT("HP_Max"), 26);
	SetStat(TEXT("HP_Current"), 26);
	SetStat(TEXT("MP_Max"), 0);
	SetStat(TEXT("MP_Current"), 0);
	SetStat(TEXT("AP_Max"), 0);
	SetStat(TEXT("AP_Current"), 0);
	// The unit's figure hides under the pole.
	TArray<UMeshComponent*> Meshes;
	Leader->GetComponents<UMeshComponent>(Meshes);
	for (UMeshComponent* Mesh : Meshes)
	{
		if (Mesh->IsA<UStaticMeshComponent>() || Mesh->IsA<USkeletalMeshComponent>())
		{
			Mesh->SetVisibility(false, true);
		}
	}
	TArray<AActor*> Attached;
	Leader->GetAttachedActors(Attached, true, true);
	for (AActor* Actor : Attached)
	{
		Actor->SetActorHiddenInGame(true);
	}

	// A carved pole: a stone base, three painted rings, wings, a beak, eyes and a red comb.
	const FVector Base = Leader->GetActorLocation() - FVector(0.f, 0.f, Leader->GetSimpleCollisionHalfHeight());
	auto Part = [this, &Base](const TCHAR* Mesh, const FLinearColor& Color, const FVector& Offset, const FVector& Scale, const FRotator& Rotation = FRotator::ZeroRotator)
	{
		UStaticMeshComponent* Component = KeepersObjective::Shape(this, Mesh, Color);
		Component->SetWorldLocation(Base + Offset);
		Component->SetWorldRotation(Rotation);
		Component->SetWorldScale3D(Scale);
		TotemParts.Add(Component);
	};
	const TCHAR* Cylinder = TEXT("/Engine/BasicShapes/Cylinder.Cylinder");
	const TCHAR* Cube = TEXT("/Engine/BasicShapes/Cube.Cube");
	const TCHAR* Sphere = TEXT("/Engine/BasicShapes/Sphere.Sphere");
	const TCHAR* Cone = TEXT("/Engine/BasicShapes/Cone.Cone");
	Part(Cylinder, FLinearColor(0.35f, 0.33f, 0.3f), FVector(0.f, 0.f, 10.f), FVector(0.75f, 0.75f, 0.2f));
	Part(Cylinder, FLinearColor(0.45f, 0.25f, 0.1f), FVector(0.f, 0.f, 50.f), FVector(0.45f, 0.45f, 0.6f));
	Part(Cylinder, FLinearColor(0.85f, 0.65f, 0.15f), FVector(0.f, 0.f, 100.f), FVector(0.42f, 0.42f, 0.4f));
	Part(Cylinder, FLinearColor(0.75f, 0.2f, 0.12f), FVector(0.f, 0.f, 140.f), FVector(0.4f, 0.4f, 0.4f));
	Part(Cube, FLinearColor(0.9f, 0.88f, 0.8f), FVector(0.f, 32.f, 115.f), FVector(0.1f, 0.5f, 0.3f), FRotator(0.f, 0.f, -25.f));
	Part(Cube, FLinearColor(0.9f, 0.88f, 0.8f), FVector(0.f, -32.f, 115.f), FVector(0.1f, 0.5f, 0.3f), FRotator(0.f, 0.f, 25.f));
	Part(Sphere, FLinearColor(0.95f, 0.95f, 0.9f), FVector(0.f, 0.f, 185.f), FVector(0.45f));
	Part(Cone, FLinearColor(1.f, 0.6f, 0.1f), FVector(26.f, 0.f, 185.f), FVector(0.14f, 0.14f, 0.22f), FRotator(-90.f, 0.f, 0.f));
	Part(Sphere, FLinearColor(0.05f, 0.05f, 0.05f), FVector(18.f, 10.f, 196.f), FVector(0.06f));
	Part(Sphere, FLinearColor(0.05f, 0.05f, 0.05f), FVector(18.f, -10.f, 196.f), FVector(0.06f));
	Part(Cube, FLinearColor(0.9f, 0.1f, 0.08f), FVector(0.f, 0.f, 215.f), FVector(0.3f, 0.08f, 0.18f));
	TotemLight = NewObject<UPointLightComponent>(this);
	TotemLight->SetupAttachment(RootComponent);
	TotemLight->RegisterComponent();
	TotemLight->SetWorldLocation(Base + FVector(0.f, 0.f, 160.f));
	TotemLight->SetLightColor(FLinearColor(1.f, 0.35f, 0.2f));
	TotemLight->SetIntensity(6000.f);
	TotemLight->SetAttenuationRadius(500.f);
	TotemLight->SetCastShadows(false);
	LeaderName = BossName.IsEmpty() ? FString(TEXT("тотем кур")) : BossName.ToString();
	UE_LOG(LogKeepersObjective, Log, TEXT("Totem %s at %s"), *Leader->GetName(), *Base.ToString());
}

void AKeepersBattleObjective::TotemHeals()
{
	if (!Leader || !KeepersObjective::IsAlive(Leader))
	{
		return;
	}
	const FVector Center = Leader->GetActorLocation();
	UObject* Grid = KeepersReflection::GetObject(CombatSystem, TEXT("Grid"));
	float Reach = 450.f;
	if (const FVector* Size = Grid ? static_cast<const FVector*>(KeepersReflection::FindValuePtr(Grid->GetClass(), Grid, TEXT("GridTileSize"))) : nullptr)
	{
		Reach = Size->X * 2.3f;
	}
	int32 Healed = 0;
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		if (*It == Leader || It->GetClass()->GetName() != TEXT("BP_Unit_C") || KeepersObjective::TeamOf(*It) == 0 || !KeepersObjective::IsAlive(*It)
			|| FVector::Dist2D(It->GetActorLocation(), Center) > Reach)
		{
			continue;
		}
		int32 Hp = 0;
		int32 HpMax = 0;
		KeepersReflection::GetUnitStat(*It, TEXT("HP_Current"), Hp);
		KeepersReflection::GetUnitStat(*It, TEXT("HP_Max"), HpMax);
		if (Hp >= HpMax)
		{
			continue;
		}
		KeepersReflection::FCall Heal(*It, TEXT("ModifyCurrentHp"));
		Heal.Set<int32>(TEXT("Modifier"), FMath::Min(2, HpMax - Hp));
		Heal.Set<bool>(TEXT("ShowNumber"), true);
		Heal.Call();
		UKeepersJuiceSubsystem::Burst(GetWorld(), It->GetActorLocation(), AKeepersBurst::EKind::Heal, FLinearColor(1.f, 0.45f, 0.25f), 10, 0.8f);
		++Healed;
	}
	if (Healed > 0)
	{
		UKeepersJuiceSubsystem::Burst(GetWorld(), Center + FVector(0.f, 0.f, 200.f), AKeepersBurst::EKind::Heal, FLinearColor(1.f, 0.35f, 0.2f), 16, 1.2f);
		UE_LOG(LogKeepersObjective, Log, TEXT("The totem heals %d enemies"), Healed);
	}
}

AKeepersBattleObjective::AKeepersBattleObjective()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void AKeepersBattleObjective::BeginPlay()
{
	Super::BeginPlay();
	CombatSystem = KeepersObjective::FindCombatSystem(GetWorld());

	if (APlayerController* Player = GetWorld()->GetFirstPlayerController())
	{
		Widget = CreateWidget<UKeepersObjectiveWidget>(Player, UKeepersObjectiveWidget::StaticClass());
		if (Widget)
		{
			Widget->SetVisibility(ESlateVisibility::HitTestInvisible);
			Widget->AddToViewport(5);
		}
	}
	UpdateWidget();
}

void AKeepersBattleObjective::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (Widget)
	{
		Widget->RemoveFromParent();
	}
	Super::EndPlay(EndPlayReason);
}

void AKeepersBattleObjective::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!CombatSystem || bFinished)
	{
		return;
	}
	bool bInCombat = false;
	bool bFinishing = false;
	KeepersReflection::GetBool(CombatSystem->GetClass(), CombatSystem, TEXT("IsInCombat"), bInCombat);
	KeepersReflection::GetBool(CombatSystem->GetClass(), CombatSystem, TEXT("IsFinishingCombat"), bFinishing);
	if (!bInCombat || bFinishing)
	{
		return;
	}

	if (!bSetUp)
	{
		SetUp();
		UpdateWidget();
	}
	// The beacon pulses, the crown floats over the leader.
	const float Time = GetWorld()->GetTimeSeconds();
	if (BeaconLight)
	{
		BeaconLight->SetIntensity(14000.f + 6000.f * FMath::Sin(Time * 3.f));
	}
	if (BeaconGem && Beacon)
	{
		BeaconGem->SetWorldLocation(Beacon->GetComponentLocation() + FVector(0.f, 0.f, 260.f + 15.f * FMath::Sin(Time * 2.f)));
		BeaconGem->SetWorldRotation(FRotator(45.f, Time * 80.f, 45.f));
	}
	if (Crown && Leader)
	{
		Crown->SetWorldLocation(Leader->GetActorLocation() + FVector(0.f, 0.f, 175.f + 10.f * FMath::Sin(Time * 2.5f)));
		Crown->SetWorldRotation(FRotator(0.f, Time * 90.f, 0.f));
	}
	if (TotemLight)
	{
		TotemLight->SetIntensity(5000.f + 2500.f * FMath::Sin(Time * 2.f));
	}
	if (bDefeatLeader && Leader && !KeepersObjective::IsAlive(Leader))
	{
		if (bTotem && !bTotemFallen)
		{
			// The pole breaks into splinters.
			bTotemFallen = true;
			for (UStaticMeshComponent* Part : TotemParts)
			{
				UKeepersJuiceSubsystem::Burst(GetWorld(), Part->GetComponentLocation(), AKeepersBurst::EKind::Hit, FLinearColor(0.6f, 0.35f, 0.15f), 4, 1.f);
				Part->SetVisibility(false);
			}
			if (TotemLight)
			{
				TotemLight->SetVisibility(false);
			}
			UKeepersJuiceSubsystem::Sound(this, TEXT("Rock"));
		}
		Finish(true);
		return;
	}
	if (bReachBeacon)
	{
		for (TActorIterator<AActor> It(GetWorld()); It; ++It)
		{
			FIntPoint Index;
			if (It->GetClass()->GetName() == TEXT("BP_Unit_C") && KeepersObjective::TeamOf(*It) == 0 && *It != ProtectedUnit
				&& KeepersReflection::GetIntPoint(It->GetClass(), *It, TEXT("Index"), Index) && Index == BeaconIndex && KeepersObjective::IsAlive(*It))
			{
				Finish(true);
				return;
			}
		}
	}
	// The protected unit fights on the heroes' side but is played by the AI.
	if (ProtectedUnit && !bProtectedSetUp)
	{
		bProtectedSetUp = true;
		KeepersReflection::FCall SetTeam(CombatSystem, TEXT("SetUnitTeamIndex"));
		SetTeam.Set<UObject*>(TEXT("Unit"), ProtectedUnit.Get());
		SetTeam.Set<int32>(TEXT("TeamIndex"), 0);
		SetTeam.Call();
		KeepersReflection::FCall UseAi(CombatSystem, TEXT("SetUnitUseAi"));
		UseAi.Set<UObject*>(TEXT("Unit"), ProtectedUnit.Get());
		UseAi.Set<bool>(TEXT("UseAi"), true);
		UseAi.Call();
	}

	// A new round starts each time play returns to team 0.
	const UObject* Current = KeepersReflection::GetObject(CombatSystem, TEXT("CurrentUnit"));
	int32 Team = INDEX_NONE;
	if (Current)
	{
		KeepersReflection::GetInt(Current->GetClass(), Current, TEXT("TeamIndex"), Team);
	}
	if (Current && Team == 0 && LastTeam != 0)
	{
		++Round;
		UE_LOG(LogKeepersObjective, Log, TEXT("Round %d"), Round);
		if (SurviveTurns > 0 && Round > SurviveTurns)
		{
			Finish(true);
			return;
		}
		UpdateWidget();
	}
	if (Current && bTotem && Team != 0 && LastTeam == 0)
	{
		TotemHeals();
	}
	if (Current)
	{
		LastTeam = Team;
	}

	if (ProtectedUnit && !KeepersObjective::IsAlive(ProtectedUnit))
	{
		Finish(false);
	}
}

void AKeepersBattleObjective::RestoreRound(int32 InRound)
{
	Round = InRound;
	LastTeam = 0;
	UpdateWidget();
}

void AKeepersBattleObjective::UpdateWidget()
{
	if (!Widget)
	{
		return;
	}
	const FString Title = FString::Printf(TEXT("Цель: %s"), *GoalText());
	const FString Progress = SurviveTurns > 0 ? FString::Printf(TEXT("Ход %d из %d"), FMath::Max(Round, 1), SurviveTurns) : FString();
	Widget->SetLines(FText::FromString(Title), FText::FromString(Progress));
}

FString AKeepersBattleObjective::GoalText() const
{
	TArray<FString> Goals;
	if (SurviveTurns > 0)
	{
		Goals.Add(FString::Printf(TEXT("продержитесь %s"), *KeepersObjective::Plural(SurviveTurns, TEXT("ход"), TEXT("хода"), TEXT("ходов")).ToString()));
	}
	if (ProtectedUnit)
	{
		Goals.Add(FString::Printf(TEXT("защитите %s"), ProtectedName.IsEmpty() ? TEXT("союзника") : *ProtectedName.ToString()));
	}
	if (bReachBeacon)
	{
		Goals.Add(TEXT("дойдите до маяка"));
	}
	if (bDefeatLeader && !BossName.IsEmpty())
	{
		Goals.Add(bTotem ? FString::Printf(TEXT("свалите босса — %s, он лечит врагов рядом"), *BossName.ToString())
			: FString::Printf(TEXT("победите босса — %s"), *BossName.ToString()));
	}
	else if (bDefeatLeader && bTotem)
	{
		Goals.Add(TEXT("разрушьте тотем кур — он лечит врагов рядом"));
	}
	else if (bDefeatLeader)
	{
		Goals.Add(LeaderName.IsEmpty() ? FString(TEXT("победите вожака")) : FString::Printf(TEXT("победите вожака — %s"), *LeaderName.ToLower()));
	}
	if (Goals.Num() == 0)
	{
		Goals.Add(TEXT("победите всех врагов"));
	}
	return FString::Join(Goals, TEXT(" и "));
}

void AKeepersBattleObjective::Finish(bool bHeroesWin)
{
	bFinished = true;
	UE_LOG(LogKeepersObjective, Log, TEXT("Objective %s"), bHeroesWin ? TEXT("complete") : TEXT("failed"));
	if (Widget)
	{
		Widget->SetLines(FText::FromString(bHeroesWin ? TEXT("Цель выполнена!") : TEXT("Цель провалена...")), FText::GetEmpty());
	}

	// Enemies of team 1 win when the heroes fail.
	KeepersReflection::FCall FinishCombat(CombatSystem, TEXT("FinishCombat"));
	FinishCombat.Set<int32>(TEXT("WiiningTeamIndex"), bHeroesWin ? 0 : 1);
	FinishCombat.Call();
}
