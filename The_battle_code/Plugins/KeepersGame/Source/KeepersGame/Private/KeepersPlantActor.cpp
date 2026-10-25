#include "KeepersPlantActor.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"

AKeepersPlantActor::AKeepersPlantActor()
{
	PrimaryActorTick.bCanEverTick = true;

	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	RootComponent = MeshComponent;
	MeshComponent->SetMobility(EComponentMobility::Movable);
	MeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	MeshComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
}

void AKeepersPlantActor::Grow(UStaticMesh* Mesh, float TargetScale, bool bBlocksLineOfSight)
{
	MeshComponent->SetStaticMesh(Mesh);
	// Project trace channel 4 is LineOfSight.
	MeshComponent->SetCollisionResponseToChannel(ECC_GameTraceChannel4, bBlocksLineOfSight ? ECR_Block : ECR_Ignore);
	FinalScale = TargetScale;
	Elapsed = 0.f;
	SetActorScale3D(FVector(0.05f));
	SetActorTickEnabled(true);
}

void AKeepersPlantActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Elapsed += DeltaSeconds;
	const float Alpha = FMath::Clamp(Elapsed / GrowDuration, 0.f, 1.f);
	// Ease out with a little overshoot ("pop").
	const float Back = 1.70158f;
	const float T = Alpha - 1.f;
	const float Eased = 1.f + (Back + 1.f) * T * T * T + Back * T * T;
	SetActorScale3D(FVector(FMath::Max(0.05f, FinalScale * Eased)));
	if (Alpha >= 1.f)
	{
		SetActorScale3D(FVector(FinalScale));
		SetActorTickEnabled(false);
	}
}
