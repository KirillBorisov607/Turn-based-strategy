#include "KeepersSmokeActor.h"

#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "KeepersReflection.h"
#include "Materials/MaterialInstanceDynamic.h"

AKeepersSmokeActor::AKeepersSmokeActor()
{
	PrimaryActorTick.bCanEverTick = true;
	Blocker = CreateDefaultSubobject<UBoxComponent>(TEXT("Blocker"));
	SetRootComponent(Blocker);
	// Only shots and sight are stopped: the grid, units and the cursor pass through.
	Blocker->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Blocker->SetCollisionResponseToAllChannels(ECR_Ignore);
	Blocker->SetCollisionResponseToChannel(ECC_GameTraceChannel4, ECR_Block);
}

void AKeepersSmokeActor::Setup(float CellSize, int32 InRounds)
{
	RoundsLeft = InRounds;
	Blocker->SetBoxExtent(FVector(CellSize * 0.42f, CellSize * 0.42f, 160.f));
	Blocker->SetRelativeLocation(FVector::ZeroVector);
	UStaticMesh* Sphere = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(
		LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")), this);
	if (Material)
	{
		Material->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.55f, 0.56f, 0.6f));
	}
	for (int32 I = 0; I < 7; ++I)
	{
		UStaticMeshComponent* Puff = NewObject<UStaticMeshComponent>(this);
		Puff->SetStaticMesh(Sphere);
		Puff->SetMaterial(0, Material);
		Puff->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Puff->SetCastShadow(false);
		Puff->SetupAttachment(RootComponent);
		Puff->RegisterComponent();
		const float Angle = I * 51.f;
		const float Radius = I == 0 ? 0.f : CellSize * 0.25f;
		Puff->SetRelativeLocation(FVector(FMath::Cos(FMath::DegreesToRadians(Angle)) * Radius, FMath::Sin(FMath::DegreesToRadians(Angle)) * Radius,
			60.f + (I % 3) * 45.f));
		Puff->SetRelativeScale3D(FVector(CellSize / 200.f * (I == 0 ? 1.3f : 0.9f)));
		Puffs.Add(Puff);
		BaseScales.Add(Puff->GetRelativeScale3D().X);
	}
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		if (It->GetClass()->GetName() == TEXT("BP_CombatSystem_C"))
		{
			CombatSystem = *It;
			break;
		}
	}
}

void AKeepersSmokeActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Age += DeltaSeconds;
	// The cloud breathes, and shrinks away when it clears.
	if (bClearing)
	{
		Fade = FMath::Max(Fade - DeltaSeconds, 0.f);
	}
	for (int32 I = 0; I < Puffs.Num(); ++I)
	{
		const float Breath = 1.f + 0.06f * FMath::Sin(Age * 1.7f + I);
		Puffs[I]->SetRelativeScale3D(FVector(FMath::Max(BaseScales[I] * Breath * Fade, 0.001f)));
	}
	if (bClearing)
	{
		if (Fade <= 0.f)
		{
			Destroy();
		}
		return;
	}
	// Each new round of the heroes eats a bit of the smoke.
	UObject* Combat = CombatSystem.Get();
	const UObject* Current = Combat ? KeepersReflection::GetObject(Combat, TEXT("CurrentUnit")) : nullptr;
	int32 Team = INDEX_NONE;
	if (Current)
	{
		KeepersReflection::GetInt(Current->GetClass(), Current, TEXT("TeamIndex"), Team);
	}
	if (Current && Team == 0 && LastTeam != 0 && LastTeam != INDEX_NONE && --RoundsLeft <= 0)
	{
		bClearing = true;
		Blocker->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	if (Current)
	{
		LastTeam = Team;
	}
}
