#include "KeepersBattleScenery.h"

#include "Components/ExponentialHeightFogComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "TimerManager.h"
#include "Materials/MaterialInterface.h"

DEFINE_LOG_CATEGORY_STATIC(LogKeepersScenery, Log, All);

namespace KeepersScenery
{
	template <typename T>
	static T* Load(const TCHAR* Path)
	{
		return LoadObject<T>(nullptr, Path);
	}

	/** Backdrop plates of the old levels are wider than this (cm, half size). */
	static constexpr float BackdropHalfSize = 2500.f;
}

AKeepersBattleScenery::AKeepersBattleScenery()
{
	PrimaryActorTick.bCanEverTick = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

UStaticMeshComponent* AKeepersBattleScenery::AddMesh(UStaticMesh* Mesh, const FTransform& Transform, UMaterialInterface* Material)
{
	if (!Mesh)
	{
		return nullptr;
	}
	UStaticMeshComponent* Component = NewObject<UStaticMeshComponent>(this);
	Component->SetStaticMesh(Mesh);
	Component->SetupAttachment(RootComponent);
	Component->SetWorldTransform(Transform);
	// Scenery never stops the cursor or the line of sight on the field.
	Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if (Material)
	{
		for (int32 Index = 0; Index < Component->GetNumMaterials(); ++Index)
		{
			Component->SetMaterial(Index, Material);
		}
	}
	Component->RegisterComponent();
	return Component;
}

UInstancedStaticMeshComponent* AKeepersBattleScenery::AddInstanced(UStaticMesh* Mesh, UMaterialInterface* Material)
{
	if (!Mesh)
	{
		return nullptr;
	}
	UInstancedStaticMeshComponent* Component = NewObject<UInstancedStaticMeshComponent>(this);
	Component->SetStaticMesh(Mesh);
	Component->SetupAttachment(RootComponent);
	Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if (Material)
	{
		for (int32 Index = 0; Index < Component->GetNumMaterials(); ++Index)
		{
			Component->SetMaterial(Index, Material);
		}
	}
	Component->RegisterComponent();
	return Component;
}

void AKeepersBattleScenery::Build(const FVector& Center, float FieldRadius, float WaterZ, int32 Seed)
{
	using namespace KeepersScenery;
	FRandomStream Random(Seed);

	// Meadow to the horizon, under everything.
	AddMesh(Load<UStaticMesh>(TEXT("/Engine/BasicShapes/Plane.Plane")),
		FTransform(FRotator::ZeroRotator, FVector(Center.X, Center.Y, WaterZ - 60.f), FVector(900.f, 900.f, 1.f)),
		Load<UMaterialInterface>(TEXT("/Game/Art/Materials/Colors/MI_Green_01.MI_Green_01")));

	// The lake around the field.
	const float LakeRadius = FieldRadius + 2600.f;
	AddMesh(Load<UStaticMesh>(TEXT("/Engine/BasicShapes/Cylinder.Cylinder")),
		FTransform(FRotator::ZeroRotator, FVector(Center.X, Center.Y, WaterZ - 2.f), FVector(LakeRadius / 50.f, LakeRadius / 50.f, 0.02f)),
		Load<UMaterialInterface>(TEXT("/Game/Art/Materials/Colors/MI_Blue_01.MI_Blue_01")));

	// Shores: low-poly hexagon columns rising into hills away from the lake.
	UStaticMesh* Hexagon = Load<UStaticMesh>(TEXT("/Game/Art/Meshes/SM_Hexagon_Platform_01.SM_Hexagon_Platform_01"));
	UInstancedStaticMeshComponent* Hills = AddInstanced(Hexagon, nullptr);
	if (!Hills)
	{
		return;
	}
	// Darker grass than the field, so the field reads first; earth on the sides.
	Hills->SetMaterial(0, Load<UMaterialInterface>(TEXT("/Game/Art/Materials/Colors/MI_Green_02.MI_Green_02")));
	Hills->SetMaterial(1, Load<UMaterialInterface>(TEXT("/Game/Art/Materials/Colors/MI_Brown_05.MI_Brown_05")));
	const FBoxSphereBounds HexBounds = Hexagon->GetBounds();
	const float HexScale = 520.f / FMath::Max(HexBounds.BoxExtent.X * 2.f, 1.f);
	const float HexHeight = FMath::Max(HexBounds.BoxExtent.Z * 2.f, 1.f);
	const float HexTop = HexBounds.Origin.Z + HexBounds.BoxExtent.Z;
	const float Step = HexBounds.BoxExtent.X * 2.f * HexScale * 0.87f;
	const float InnerRadius = FieldRadius + 1100.f;
	const float OuterRadius = FieldRadius + 6500.f;

	UStaticMesh* Trees[] =
	{
		Load<UStaticMesh>(TEXT("/Game/Art/Meshes/SM_Tree_01.SM_Tree_01")),
		Load<UStaticMesh>(TEXT("/Game/Art/Meshes/SM_Tree_02.SM_Tree_02")),
		Load<UStaticMesh>(TEXT("/Game/Art/Meshes/SM_Tree_03.SM_Tree_03")),
		Load<UStaticMesh>(TEXT("/Game/Art/Meshes/SM_Bush_01.SM_Bush_01")),
	};
	UInstancedStaticMeshComponent* TreeSets[UE_ARRAY_COUNT(Trees)];
	for (int32 I = 0; I < UE_ARRAY_COUNT(Trees); ++I)
	{
		TreeSets[I] = AddInstanced(Trees[I], nullptr);
	}

	const float Phase = Random.FRandRange(0.f, 100.f);
	auto TopAt = [&](const FVector2D& Point, float Distance)
	{
		const float Rise = FMath::SmoothStep(InnerRadius, OuterRadius, Distance);
		const float Noise = FMath::PerlinNoise2D(Point / 2200.f + FVector2D(Phase, Phase));
		return WaterZ + 25.f + Rise * 650.f + Noise * 220.f + Rise * Noise * 300.f;
	};

	struct FSpot { FVector Location; float Distance; };
	TArray<FSpot> Spots;
	const int32 Count = FMath::CeilToInt(OuterRadius / Step) + 1;
	for (int32 Row = -Count; Row <= Count; ++Row)
	{
		for (int32 Column = -Count; Column <= Count; ++Column)
		{
			const FVector2D Point(Center.X + (Column + (Row & 1) * 0.5f) * Step, Center.Y + Row * Step * 0.866f);
			const float Distance = FVector2D::Distance(Point, FVector2D(Center));
			// A ragged shore line.
			const float Shore = InnerRadius + FMath::PerlinNoise2D(Point / 1500.f + FVector2D(Phase * 2.f, 3.f)) * 700.f;
			if (Distance < Shore || Distance > OuterRadius)
			{
				continue;
			}
			const float Top = TopAt(Point, Distance);
			// Columns reach well under the water, so no hill floats.
			const float Bottom = WaterZ - 250.f;
			const float ZScale = (Top - Bottom) / HexHeight;
			FTransform Transform(FRotator(0.f, 0.f, 0.f), FVector(Point.X, Point.Y, Top - HexTop * ZScale), FVector(HexScale, HexScale, ZScale));
			Hills->AddInstance(Transform, true);
			Spots.Add({ FVector(Point.X, Point.Y, Top), Distance });
		}
	}

	// Woods on the hills: clumps, denser further out.
	for (const FSpot& Spot : Spots)
	{
		const float Woodland = FMath::PerlinNoise2D(FVector2D(Spot.Location) / 1800.f + FVector2D(11.f, Phase));
		if (Woodland < -0.05f || Random.FRand() > 0.75f)
		{
			continue;
		}
		const int32 Trees2 = Random.RandRange(1, 3);
		for (int32 I = 0; I < Trees2; ++I)
		{
			const int32 Kind = Random.RandRange(0, UE_ARRAY_COUNT(Trees) - 1);
			if (!TreeSets[Kind])
			{
				continue;
			}
			const FVector Offset(Random.FRandRange(-150.f, 150.f), Random.FRandRange(-150.f, 150.f), 0.f);
			const float Scale = Random.FRandRange(1.1f, 1.8f) * (Kind == 3 ? 1.2f : 1.f);
			TreeSets[Kind]->AddInstance(FTransform(FRotator(0.f, Random.FRandRange(0.f, 360.f), 0.f), Spot.Location + Offset, FVector(Scale)), true);
		}
	}

	// A few buildings of the Toony Tiny RTS Set on the hills: a hamlet, towers, a castle far away.
	struct FBuilding { const TCHAR* Mesh; float Scale; float MinDistance; };
	const FBuilding Buildings[] =
	{
		{ TEXT("SM_House"), 1.6f, 1500.f }, { TEXT("SM_House"), 1.6f, 1600.f }, { TEXT("SM_Farm"), 1.6f, 1700.f },
		{ TEXT("SM_Granary"), 1.6f, 1900.f }, { TEXT("SM_Tower_A"), 1.7f, 2400.f }, { TEXT("SM_LumberMill"), 1.6f, 2000.f },
		{ TEXT("SM_Temple"), 1.6f, 3000.f }, { TEXT("SM_Castle"), 2.2f, 4200.f }, { TEXT("SM_Tower_B"), 1.7f, 3300.f },
	};
	UMaterialInterface* BuildingColor = Load<UMaterialInterface>(TEXT("/Game/Toony_Tiny_RTS_Set/Materials/Buildings/MI_Buildings_Brown.MI_Buildings_Brown"));
	const float StartAngle = Random.FRandRange(0.f, 360.f);
	for (int32 I = 0; I < UE_ARRAY_COUNT(Buildings); ++I)
	{
		const FBuilding& Building = Buildings[I];
		UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("/Game/Toony_Tiny_RTS_Set/Meshes/Buildings/%s.%s"), Building.Mesh, Building.Mesh));
		const float Angle = FMath::DegreesToRadians(StartAngle + I * (360.f / UE_ARRAY_COUNT(Buildings)) + Random.FRandRange(-12.f, 12.f));
		const float Distance = InnerRadius + Building.MinDistance + Random.FRandRange(0.f, 500.f);
		const FVector2D Point = FVector2D(Center) + FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * Distance;
		// Face the lake.
		const float Yaw = FMath::RadiansToDegrees(Angle) + 180.f + Random.FRandRange(-25.f, 25.f);
		AddMesh(Mesh, FTransform(FRotator(0.f, Yaw, 0.f), FVector(Point.X, Point.Y, TopAt(Point, Distance)), FVector(Building.Scale)), BuildingColor);
	}

	// Haze that blends the hills into the sky.
	UExponentialHeightFogComponent* Fog = NewObject<UExponentialHeightFogComponent>(this);
	Fog->SetupAttachment(RootComponent);
	Fog->SetWorldLocation(FVector(Center.X, Center.Y, WaterZ));
	Fog->SetFogDensity(0.02f);
	Fog->SetFogHeightFalloff(0.2f);
	Fog->SetFogInscatteringColor(FLinearColor(0.45f, 0.58f, 0.78f));
	Fog->SetStartDistance(5000.f);
	Fog->RegisterComponent();

	UE_LOG(LogKeepersScenery, Log, TEXT("Scenery: field radius %.0f, %d hill columns"), FieldRadius, Spots.Num());
}

// ---------------------------------------------------------------------------------------------------------------------

bool UKeepersBattleScenerySubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UKeepersBattleScenerySubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	// Battle levels are the ones with a grid; the campaign map shows the same island.
	bool bField = InWorld.GetMapName().Contains(TEXT("CampaignMap"));
	for (TActorIterator<AActor> It(&InWorld); It && !bField; ++It)
	{
		bField = It->GetClass()->GetName().StartsWith(TEXT("BP_Grid_C"));
	}
	if (bField)
	{
		SurroundField(InWorld);
	}
}

void UKeepersBattleScenerySubsystem::SurroundField(UWorld& InWorld, int32 Retries)
{
	if (TActorIterator<AKeepersBattleScenery>(&InWorld))
	{
		return;
	}

	// The field: every level mesh except the wide backdrop plates, which go out of sight.
	FBox Field(ForceInit);
	for (TActorIterator<AStaticMeshActor> It(&InWorld); It; ++It)
	{
		FVector Origin;
		FVector Extent;
		It->GetActorBounds(false, Origin, Extent);
		if (FMath::Max(Extent.X, Extent.Y) > KeepersScenery::BackdropHalfSize)
		{
			if (Origin.Z < -100.f)
			{
				It->SetActorHiddenInGame(true);
			}
			continue;
		}
		if (FVector2D(Origin).Size() < 6000.f)
		{
			Field += FBox(Origin - Extent, Origin + Extent);
		}
	}
	if (!Field.IsValid)
	{
		// The island of the main menu and the campaign map streams in after the level starts: try again soon.
		if (Retries > 0)
		{
			TWeakObjectPtr<UWorld> WeakWorld(&InWorld);
			FTimerHandle Handle;
			InWorld.GetTimerManager().SetTimer(Handle, FTimerDelegate::CreateLambda([WeakWorld, Retries]()
			{
				if (UWorld* World = WeakWorld.Get())
				{
					SurroundField(*World, Retries - 1);
				}
			}), 0.3f, false);
		}
		return;
	}
	const FVector Center = Field.GetCenter();
	const float Radius = FMath::Max(Field.GetExtent().X, Field.GetExtent().Y);
	AKeepersBattleScenery* Scenery = InWorld.SpawnActor<AKeepersBattleScenery>(AKeepersBattleScenery::StaticClass(), FTransform::Identity);
	if (Scenery)
	{
		Scenery->Build(FVector(Center.X, Center.Y, 0.f), Radius, -170.f, GetTypeHash(InWorld.GetMapName()));
	}
}
