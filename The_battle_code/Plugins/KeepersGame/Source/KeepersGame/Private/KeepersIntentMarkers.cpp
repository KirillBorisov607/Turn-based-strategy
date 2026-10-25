#include "KeepersIntentMarkers.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

AKeepersIntentMarkers::AKeepersIntentMarkers()
{
	PrimaryActorTick.bCanEverTick = true;

	Discs = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Discs"));
	RootComponent = Discs;
	Discs->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Discs->SetCastShadow(false);
	Discs->SetMobility(EComponentMobility::Movable);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (Cylinder.Succeeded())
	{
		Discs->SetStaticMesh(Cylinder.Object);
	}
}

void AKeepersIntentMarkers::SetMarkerLocations(const TArray<FVector>& Locations)
{
	MarkerLocations.Reset();
	for (const FVector& Location : Locations)
	{
		MarkerLocations.AddUnique(Location);
	}

	if (!Material)
	{
		if (UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")))
		{
			Material = UMaterialInstanceDynamic::Create(Base, this);
			Material->SetVectorParameterValue(TEXT("Color"), Color);
			Discs->SetMaterial(0, Material);
		}
	}

	Discs->ClearInstances();
	for (const FVector& Location : MarkerLocations)
	{
		Discs->AddInstance(FTransform(FRotator::ZeroRotator, Location + FVector(0.f, 0.f, HeightOffset), FVector(DiscScale, DiscScale, 0.03f)), true);
	}
}

void AKeepersIntentMarkers::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (MarkerLocations.Num() == 0)
	{
		return;
	}

	Time += DeltaSeconds;
	const float Pulse = 0.85f + 0.15f * FMath::Sin(Time * PulseSpeed * UE_TWO_PI);
	for (int32 Index = 0; Index < MarkerLocations.Num(); ++Index)
	{
		const FVector Scale(DiscScale * Pulse, DiscScale * Pulse, 0.03f);
		Discs->UpdateInstanceTransform(Index, FTransform(FRotator::ZeroRotator, MarkerLocations[Index] + FVector(0.f, 0.f, HeightOffset), Scale), true, Index == MarkerLocations.Num() - 1, true);
	}
}
