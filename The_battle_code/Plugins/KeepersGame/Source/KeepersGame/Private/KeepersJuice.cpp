#include "KeepersJuice.h"

#include "Components/StaticMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "KeepersReflection.h"
#include "Materials/MaterialInstanceDynamic.h"

DEFINE_LOG_CATEGORY_STATIC(LogKeepersJuice, Log, All);

namespace KeepersJuice
{
	static UStaticMesh* Mesh(bool bCube)
	{
		static TWeakObjectPtr<UStaticMesh> Cube;
		static TWeakObjectPtr<UStaticMesh> Sphere;
		TWeakObjectPtr<UStaticMesh>& Slot = bCube ? Cube : Sphere;
		if (!Slot.IsValid())
		{
			Slot = LoadObject<UStaticMesh>(nullptr, bCube ? TEXT("/Engine/BasicShapes/Cube.Cube") : TEXT("/Engine/BasicShapes/Sphere.Sphere"));
		}
		return Slot.Get();
	}

	static UMaterialInterface* BaseMaterial()
	{
		static TWeakObjectPtr<UMaterialInterface> Material;
		if (!Material.IsValid())
		{
			Material = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
		}
		return Material.Get();
	}

	/** Colour of what flies off a unit: goo, feathers, fur, cloth and steel. */
	static FLinearColor ColorOf(const AActor* Unit, int32 Team)
	{
		const FString Name = Unit->GetName();
		if (Name.Contains(TEXT("Slime")))
		{
			return FLinearColor(0.35f, 0.9f, 0.3f);
		}
		if (Name.Contains(TEXT("Chicken")))
		{
			return FLinearColor(0.95f, 0.93f, 0.85f);
		}
		if (Name.Contains(TEXT("Bat")))
		{
			return FLinearColor(0.45f, 0.25f, 0.6f);
		}
		return Team == 0 ? FLinearColor(1.f, 0.75f, 0.25f) : FLinearColor(0.9f, 0.25f, 0.2f);
	}
}

AKeepersBurst::AKeepersBurst()
{
	PrimaryActorTick.bCanEverTick = true;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
}

void AKeepersBurst::Start(EKind InKind, const FLinearColor& Color, int32 Count, float Size)
{
	Kind = InKind;
	Life = Kind == EKind::Hit ? 0.9f : Kind == EKind::Heal ? 1.1f : 1.2f;
	UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(KeepersJuice::BaseMaterial(), this);
	if (Material)
	{
		Material->SetVectorParameterValue(TEXT("Color"), Color);
	}
	for (int32 I = 0; I < Count; ++I)
	{
		FPiece& Piece = Pieces.AddDefaulted_GetRef();
		Piece.Mesh = NewObject<UStaticMeshComponent>(this);
		Piece.Mesh->SetStaticMesh(KeepersJuice::Mesh(Kind == EKind::Hit));
		Piece.Mesh->SetMaterial(0, Material);
		Piece.Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Piece.Mesh->SetCastShadow(false);
		Piece.Mesh->SetupAttachment(GetRootComponent());
		Piece.Mesh->RegisterComponent();
		const FVector Direction = FMath::VRand();
		switch (Kind)
		{
		case EKind::Hit:
			Piece.Velocity = FVector(Direction.X, Direction.Y, FMath::Abs(Direction.Z) + 0.6f) * FMath::FRandRange(250.f, 520.f);
			Piece.Scale = FMath::FRandRange(0.15f, 0.27f) * Size;
			break;
		case EKind::Heal:
			Piece.Velocity = FVector(Direction.X * 40.f, Direction.Y * 40.f, FMath::FRandRange(90.f, 190.f));
			Piece.Mesh->SetRelativeLocation(FVector(Direction.X * 60.f, Direction.Y * 60.f, FMath::FRandRange(-40.f, 20.f)));
			Piece.Scale = FMath::FRandRange(0.12f, 0.2f) * Size;
			break;
		case EKind::Puff:
			Piece.Velocity = FVector(Direction.X, Direction.Y, FMath::Abs(Direction.Z) * 0.5f + 0.4f) * FMath::FRandRange(70.f, 160.f);
			Piece.Scale = FMath::FRandRange(0.25f, 0.45f) * Size;
			break;
		}
		Piece.Spin = FRotator(FMath::FRandRange(-400.f, 400.f), FMath::FRandRange(-400.f, 400.f), FMath::FRandRange(-400.f, 400.f));
		Piece.Mesh->SetRelativeScale3D(FVector(Piece.Scale));
	}
}

void AKeepersBurst::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Age += DeltaSeconds;
	const float T = FMath::Clamp(Age / Life, 0.f, 1.f);
	for (FPiece& Piece : Pieces)
	{
		if (Kind == EKind::Hit)
		{
			Piece.Velocity.Z -= 1400.f * DeltaSeconds;
		}
		else
		{
			Piece.Velocity *= FMath::Pow(0.2f, DeltaSeconds);
		}
		Piece.Mesh->AddRelativeLocation(Piece.Velocity * DeltaSeconds);
		Piece.Mesh->AddRelativeRotation(Piece.Spin * DeltaSeconds);
		// Pieces shrink away at the end; a puff swells first.
		const float Grow = Kind == EKind::Puff ? 1.f + T * 0.8f : 1.f;
		const float Fade = T < 0.6f ? 1.f : 1.f - (T - 0.6f) / 0.4f;
		Piece.Mesh->SetRelativeScale3D(FVector(FMath::Max(Piece.Scale * Grow * Fade, 0.001f)));
	}
	if (Age >= Life)
	{
		Destroy();
	}
}

bool UKeepersJuiceSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

TStatId UKeepersJuiceSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UKeepersJuiceSubsystem, STATGROUP_Tickables);
}

void UKeepersJuiceSubsystem::Burst(UWorld* World, const FVector& Location, AKeepersBurst::EKind Kind, const FLinearColor& Color, int32 Count, float Size)
{
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	if (AKeepersBurst* Burst = World ? World->SpawnActor<AKeepersBurst>(Location, FRotator::ZeroRotator, Params) : nullptr)
	{
		UE_LOG(LogKeepersJuice, Log, TEXT("Burst %d at %s"), static_cast<int32>(Kind), *Location.ToString());
		Burst->Start(Kind, Color, Count, Size);
	}
}

void UKeepersJuiceSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	UWorld* World = GetWorld();
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Unit = *It;
		if (Unit->GetClass()->GetName().StartsWith(TEXT("BP_UnitAi")))
		{
			// The template's robot badge over AI units does not belong in this world.
			TArray<UWidgetComponent*> Badges;
			Unit->GetComponents(Badges);
			for (UWidgetComponent* Badge : Badges)
			{
				if (Badge->IsVisible())
				{
					Badge->SetVisibility(false);
				}
			}
			continue;
		}
		if (Unit->GetClass()->GetName() != TEXT("BP_Unit_C"))
		{
			continue;
		}
		int32 Hp = 0;
		if (!KeepersReflection::GetUnitStat(Unit, TEXT("HP_Current"), Hp))
		{
			continue;
		}
		int32* Known = Health.Find(Unit);
		if (!Known)
		{
			UE_LOG(LogKeepersJuice, Log, TEXT("Watching %s, %d HP"), *Unit->GetName(), Hp);
			Health.Add(Unit, Hp);
			continue;
		}
		const int32 Change = Hp - *Known;
		*Known = Hp;
		if (Change == 0)
		{
			continue;
		}
		int32 Team = 0;
		KeepersReflection::GetInt(Unit->GetClass(), Unit, TEXT("TeamIndex"), Team);
		const FVector Chest = Unit->GetActorLocation() + FVector(0.f, 0.f, 40.f);
		if (Change < 0)
		{
			Burst(World, Chest, AKeepersBurst::EKind::Hit, KeepersJuice::ColorOf(Unit, Team), FMath::Clamp(8 - Change * 2, 10, 24));
			if (Hp <= 0)
			{
				Burst(World, Unit->GetActorLocation(), AKeepersBurst::EKind::Puff, FLinearColor(0.85f, 0.82f, 0.78f), 12, 1.f);
			}
		}
		else
		{
			Burst(World, Chest, AKeepersBurst::EKind::Heal, FLinearColor(0.4f, 1.f, 0.45f), FMath::Clamp(4 + Change, 6, 16));
		}
	}
}
