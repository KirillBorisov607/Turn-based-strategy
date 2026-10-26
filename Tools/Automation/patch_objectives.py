"""Beacon and leader objectives, the spawner of requested objectives."""
import os
os.chdir(r'C:/ProjectsUE/Turn-based-strategy/The_battle_code/Plugins/KeepersGame/Source/KeepersGame')


def patch(p, pairs):
    s = open(p, 'rb').read().decode('utf-8-sig')
    crlf = '\r\n' in s
    s = s.replace('\r\n', '\n')
    for a, b in pairs:
        assert s.count(a) == 1, (p, a[:70])
        s = s.replace(a, b)
    open(p, 'w', encoding='utf-8', newline='\r\n' if crlf else '\n').write(s)


patch('Public/KeepersBattleObjective.h', [
('''#include "GameFramework/Actor.h"
''', '''#include "GameFramework/Actor.h"
#include "Subsystems/WorldSubsystem.h"
'''),
])
s = open('Public/KeepersBattleObjective.h', encoding='utf-8').read()
s += '''
/** Spawns the requested objective (AKeepersBattleObjective::RequestForNextBattle) in a battle level without one. */
UCLASS()
class KEEPERSGAME_API UKeepersObjectiveSpawner : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
};
'''
open('Public/KeepersBattleObjective.h', 'w', encoding='utf-8').write(s)

patch('Private/KeepersBattleObjective.cpp', [
('''#include "KeepersStyle.h"
''', '''#include "KeepersStyle.h"
#include "KeepersHeroes.h"
#include "KeepersLevelKit.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
'''),
('''	static FText Plural(''', '''	static EKeepersObjectiveKind Requested = EKeepersObjectiveKind::None;

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

	static FText Plural('''),
('''AKeepersBattleObjective::AKeepersBattleObjective()''', '''AKeepersBattleObjective* AKeepersBattleObjective::Find(const UWorld* World)
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
	Objective->bDefeatLeader = Kind == EKeepersObjectiveKind::Leader;
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
		Beacon->SetWorldLocation(Place + FVector(0.f, 0.f, 110.f));
		Beacon->SetWorldScale3D(FVector(0.22f, 0.22f, 2.2f));
		BeaconLight = NewObject<UPointLightComponent>(this);
		BeaconLight->SetupAttachment(RootComponent);
		BeaconLight->RegisterComponent();
		BeaconLight->SetWorldLocation(Place + FVector(0.f, 0.f, 160.f));
		BeaconLight->SetLightColor(FLinearColor(1.f, 0.75f, 0.3f));
		BeaconLight->SetIntensity(9000.f);
		BeaconLight->SetAttenuationRadius(420.f);
		BeaconLight->SetCastShadows(false);
		UE_LOG(LogKeepersObjective, Log, TEXT("Beacon at (%d,%d)"), BeaconIndex.X, BeaconIndex.Y);
	}
	else
	{
		bReachBeacon = false;
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
	if (bDefeatLeader && Leader)
	{
		const FKeepersHeroDef* Def = KeepersHeroes::FindByUnitType(KeepersHeroes::UnitTypeOf(Leader));
		LeaderName = Def ? Def->Name.ToString() : FString();
		Crown = KeepersObjective::Shape(this, TEXT("/Engine/BasicShapes/Cone.Cone"), FLinearColor(1.f, 0.8f, 0.2f));
		Crown->SetWorldScale3D(FVector(0.35f, 0.35f, -0.35f));
		UE_LOG(LogKeepersObjective, Log, TEXT("Leader %s"), *Leader->GetName());
	}
	else
	{
		bDefeatLeader = false;
	}
}

AKeepersBattleObjective::AKeepersBattleObjective()'''),
('''	// The protected unit fights on the heroes' side but is played by the AI.''', '''	if (!bSetUp)
	{
		SetUp();
		UpdateWidget();
	}
	// The beacon pulses, the crown floats over the leader.
	const float Time = GetWorld()->GetTimeSeconds();
	if (BeaconLight)
	{
		BeaconLight->SetIntensity(7000.f + 3000.f * FMath::Sin(Time * 3.f));
	}
	if (Crown && Leader)
	{
		Crown->SetWorldLocation(Leader->GetActorLocation() + FVector(0.f, 0.f, 150.f + 10.f * FMath::Sin(Time * 2.5f)));
		Crown->SetWorldRotation(FRotator(0.f, Time * 90.f, 0.f));
	}
	if (bDefeatLeader && Leader && !KeepersObjective::IsAlive(Leader))
	{
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
	// The protected unit fights on the heroes' side but is played by the AI.'''),
('''	if (Goals.Num() == 0)
	{
		Goals.Add(TEXT("победите всех врагов"));
	}''', '''	if (bReachBeacon)
	{
		Goals.Add(TEXT("дойдите до маяка"));
	}
	if (bDefeatLeader)
	{
		Goals.Add(LeaderName.IsEmpty() ? FString(TEXT("победите вожака")) : FString::Printf(TEXT("победите вожака — %s"), *LeaderName.ToLower()));
	}
	if (Goals.Num() == 0)
	{
		Goals.Add(TEXT("победите всех врагов"));
	}'''),
])
print('ok')
