#include "KeepersRockActor.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

AKeepersRockActor::AKeepersRockActor()
{
	PrimaryActorTick.bCanEverTick = true;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	RootComponent = Mesh;
	Mesh->SetMobility(EComponentMobility::Movable);
	Mesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Mesh->SetCollisionResponseToAllChannels(ECR_Ignore);
	// Project trace channels: 1 GroundAndGridModifiers, 4 LineOfSight
	Mesh->SetCollisionResponseToChannel(ECC_GameTraceChannel4, ECR_Block);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Column(TEXT("/Game/Art/Meshes/SM_Hexagon_Platform_01.SM_Hexagon_Platform_01"));
	if (Column.Succeeded())
	{
		Mesh->SetStaticMesh(Column.Object);
	}
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Stone(TEXT("/Game/Art/Materials/Colors/MI_Grey_03.MI_Grey_03"));
	if (Stone.Succeeded())
	{
		Mesh->SetMaterial(0, Stone.Object);
		Mesh->SetMaterial(1, Stone.Object);
	}
}

void AKeepersRockActor::Rise(const FVector& TileSurface, float TargetTopZ)
{
	// The column's top is at its origin and it extends 15 m down, so it can start buried.
	StartZ = TileSurface.Z - 40.f;
	EndZ = TargetTopZ;
	Elapsed = 0.f;
	bRising = true;
	SetActorLocation(FVector(TileSurface.X, TileSurface.Y, StartZ));
}

void AKeepersRockActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bRising)
	{
		return;
	}
	Elapsed += DeltaSeconds;
	const float Alpha = FMath::Clamp(Elapsed / RiseDuration, 0.f, 1.f);
	const float Eased = FMath::InterpEaseOut(0.f, 1.f, Alpha, 3.f);
	FVector Location = GetActorLocation();
	Location.Z = FMath::Lerp(StartZ, EndZ, Eased);
	SetActorLocation(Location);
	if (Alpha >= 1.f)
	{
		bRising = false;
		SetActorTickEnabled(false);
	}
}
