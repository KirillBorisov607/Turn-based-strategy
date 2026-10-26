#include "KeepersSurfaces.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "KeepersCampaign.h"
#include "KeepersJuice.h"
#include "KeepersLevelKit.h"
#include "KeepersReflection.h"
#include "KeepersTerrainSubsystem.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogKeepersSurfaces, Log, All);

EKeepersSurface UKeepersSurfaceSubsystem::Requested = EKeepersSurface::None;

namespace KeepersSurfaces
{
	static const FLinearColor IceColor(0.78f, 0.92f, 1.f);
	static const FLinearColor MarshColor(0.13f, 0.24f, 0.05f);
	/** Damage of a bog that pulls a unit standing in it for a second turn. */
	static constexpr int32 BogDamage = 3;
	/** Cells a unit slides on ice at most. */
	static constexpr int32 MaxSlide = 4;
	static constexpr float SlideSecondsPerCell = 0.22f;

	static bool IsGround(uint8 Type)
	{
		return Type == 1 || Type == 3 || Type == 4;
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

	static FVector TileSize(UObject* Grid)
	{
		const void* Size = Grid ? KeepersReflection::FindValuePtr(Grid->GetClass(), Grid, TEXT("GridTileSize")) : nullptr;
		return Size ? *static_cast<const FVector*>(Size) : FVector(200.f, 200.f, 100.f);
	}
}

AKeepersSurfaceZone::AKeepersSurfaceZone()
{
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

UKeepersSurfaceSubsystem* UKeepersSurfaceSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UKeepersSurfaceSubsystem>() : nullptr;
}

void UKeepersSurfaceSubsystem::RequestForNextBattle(EKeepersSurface Surface)
{
	Requested = Surface;
}

bool UKeepersSurfaceSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

TStatId UKeepersSurfaceSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UKeepersSurfaceSubsystem, STATGROUP_Tickables);
}

EKeepersSurface UKeepersSurfaceSubsystem::GetSurface(const FIntPoint& Index) const
{
	const EKeepersSurface* Surface = Surfaces.Find(Index);
	return Surface ? *Surface : EKeepersSurface::None;
}

void UKeepersSurfaceSubsystem::Paint(const FIntPoint& Index, EKeepersSurface Surface, bool bShow)
{
	Surfaces.Add(Index, Surface);
	FVector Location;
	if (!bShow || !Grid || !UKeepersTerrainSubsystem::TileLocation(Grid, Index, Location))
	{
		return;
	}
	if (!Plates)
	{
		Plates = GetWorld()->SpawnActor<AActor>();
		USceneComponent* Root = NewObject<USceneComponent>(Plates);
		Plates->SetRootComponent(Root);
		Root->RegisterComponent();
	}
	const float Size = KeepersSurfaces::TileSize(Grid).X / 100.f;
	const bool bIce = Surface == EKeepersSurface::Ice;
	UStaticMeshComponent* Plate = KeepersSurfaces::Shape(Plates, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"), bIce ? KeepersSurfaces::IceColor : KeepersSurfaces::MarshColor);
	Plate->SetWorldLocation(Location + FVector(0.f, 0.f, 3.f));
	Plate->SetWorldScale3D(FVector(Size * 0.47f, Size * 0.47f, 0.05f));
	// Cracks of light on the ice, bubbles of mud in the marsh.
	FRandomStream Random(GetTypeHash(Index));
	for (int32 I = 0; I < 2; ++I)
	{
		const FVector Offset(Random.FRandRange(-0.25f, 0.25f) * Size * 100.f, Random.FRandRange(-0.25f, 0.25f) * Size * 100.f, 7.f);
		UStaticMeshComponent* Detail = bIce
			? KeepersSurfaces::Shape(Plates, TEXT("/Engine/BasicShapes/Cube.Cube"), FLinearColor(0.95f, 0.99f, 1.f))
			: KeepersSurfaces::Shape(Plates, TEXT("/Engine/BasicShapes/Sphere.Sphere"), FLinearColor(0.12f, 0.16f, 0.06f));
		Detail->SetWorldLocation(Location + Offset);
		Detail->SetWorldRotation(FRotator(0.f, Random.FRandRange(0.f, 180.f), 0.f));
		Detail->SetWorldScale3D(bIce ? FVector(Size * 0.3f, 0.03f, 0.02f) : FVector(Size * 0.09f));
	}
}

void UKeepersSurfaceSubsystem::Tick(float DeltaTime)
{
	UWorld* World = GetWorld();
	if (!World || !World->HasBegunPlay())
	{
		return;
	}
	if (!CombatSystem)
	{
		CombatSystem = UKeepersTerrainSubsystem::CombatSystemOf(World);
		if (!CombatSystem)
		{
			return;
		}
	}
	bool bInCombat = false;
	KeepersReflection::GetBool(CombatSystem->GetClass(), CombatSystem, TEXT("IsInCombat"), bInCombat);
	if (!bInCombat)
	{
		return;
	}
	if (!bSetUp)
	{
		SetUp();
	}
	TickSlides(DeltaTime);
	if (Surfaces.Num() == 0)
	{
		return;
	}
	UObject* Current = KeepersReflection::GetObject(CombatSystem, TEXT("CurrentUnit"));
	if (Current != CurrentUnit.Get())
	{
		if (CurrentUnit.IsValid())
		{
			OnTurnEnded(CurrentUnit.Get());
		}
		CurrentUnit = Current;
		if (Current)
		{
			OnTurnStarted(Current);
		}
	}
}

void UKeepersSurfaceSubsystem::SetUp()
{
	bSetUp = true;
	Grid = KeepersReflection::GetObject(CombatSystem, TEXT("Grid"));
	FKeepersLevelGrid Level;
	if (!Grid || !KeepersLevelKit::PrepareGrid(GetWorld(), Level))
	{
		return;
	}
	const float Size = KeepersSurfaces::TileSize(Grid).X;
	const TArray<FIntPoint> All = KeepersLevelKit::AllIndexes(Level);
	auto Around = [&](const FVector& Center, float Radius, EKeepersSurface Surface)
	{
		for (const FIntPoint& Index : All)
		{
			FVector Location;
			uint8 Type = 0;
			if (UKeepersTerrainSubsystem::TileLocation(Grid, Index, Location) && UKeepersTerrainSubsystem::TileType(Grid, Index, Type)
				&& KeepersSurfaces::IsGround(Type) && FVector::Dist2D(Location, Center) <= Radius)
			{
				Paint(Index, Surface);
			}
		}
	};

	// Zones placed in the level.
	for (TActorIterator<AKeepersSurfaceZone> It(GetWorld()); It; ++It)
	{
		Around(It->GetActorLocation(), (It->Radius + 0.5f) * Size, It->Surface);
	}

	// Patches for the region: on open ground, away from the heroes' first cells.
	const EKeepersSurface Patch = Requested;
	Requested = EKeepersSurface::None;
	if (Patch != EKeepersSurface::None)
	{
		const UKeepersCampaignSubsystem* Campaign = UKeepersCampaignSubsystem::Get(this);
		FRandomStream Random(GetTypeHash(GetWorld()->GetMapName()) ^ (Campaign ? Campaign->GetSeed() + Campaign->GetCurrentNode() * 7919 : 0));
		TArray<FVector> Heroes;
		TArray<FVector> Candidates;
		for (const FIntPoint& Index : All)
		{
			FVector Location;
			uint8 Type = 0;
			if (!UKeepersTerrainSubsystem::TileLocation(Grid, Index, Location) || !UKeepersTerrainSubsystem::TileType(Grid, Index, Type))
			{
				continue;
			}
			const UObject* Unit = UKeepersTerrainSubsystem::UnitOnTile(Grid, Index);
			int32 Team = 1;
			if (Unit && KeepersReflection::GetInt(Unit->GetClass(), Unit, TEXT("TeamIndex"), Team) && Team == 0)
			{
				Heroes.Add(Location);
			}
			else if (!Unit && Type == 1)
			{
				Candidates.Add(Location);
			}
		}
		const int32 Count = FMath::Clamp(Candidates.Num() / 30, 2, 5);
		TArray<FVector> Centers;
		for (int32 Try = 0; Try < 200 && Centers.Num() < Count && Candidates.Num() > 0; ++Try)
		{
			const FVector Center = Candidates[Random.RandRange(0, Candidates.Num() - 1)];
			const bool bNearHero = Heroes.ContainsByPredicate([&](const FVector& Hero) { return FVector::Dist2D(Hero, Center) < Size * 1.6f; });
			const bool bNearPatch = Centers.ContainsByPredicate([&](const FVector& Other) { return FVector::Dist2D(Other, Center) < Size * 3.f; });
			if (!bNearHero && !bNearPatch)
			{
				Centers.Add(Center);
				Around(Center, Size * (Patch == EKeepersSurface::Ice ? 1.6f : 1.1f), Patch);
			}
		}
	}
	UE_LOG(LogKeepersSurfaces, Log, TEXT("Surfaces: %d cells"), Surfaces.Num());
}

void UKeepersSurfaceSubsystem::OnTurnStarted(UObject* Unit)
{
	FIntPoint Index(-1, -1);
	KeepersReflection::GetIntPoint(Unit->GetClass(), Unit, TEXT("Index"), Index);
	TurnStart = Index;
	if (GetSurface(Index) != EKeepersSurface::Marsh || UKeepersTerrainSubsystem::CanUnitStandOn(Unit, 5))
	{
		InMarsh.Remove(Unit);
		return;
	}
	const bool bSecondTurn = InMarsh.FindRef(Unit) == Index;
	InMarsh.Add(Unit, Index);
	// After the combat system has given the unit its points for the turn.
	TWeakObjectPtr<UObject> WeakUnit(Unit);
	FTimerHandle Timer;
	GetWorld()->GetTimerManager().SetTimer(Timer, FTimerDelegate::CreateWeakLambda(this, [this, WeakUnit, bSecondTurn]()
	{
		UObject* Stuck = WeakUnit.Get();
		if (!Stuck || !CombatSystem)
		{
			return;
		}
		KeepersReflection::FCall Mp(Stuck, TEXT("ModifyCurrentMp"));
		Mp.Set<int32>(TEXT("Modifier"), -1);
		Mp.Set<bool>(TEXT("ShowNumber"), true);
		Mp.Call();
		if (const AActor* Actor = Cast<AActor>(Stuck))
		{
			UKeepersJuiceSubsystem::Burst(GetWorld(), Actor->GetActorLocation(), AKeepersBurst::EKind::Puff, FLinearColor(0.25f, 0.3f, 0.12f), bSecondTurn ? 18 : 8, 0.8f);
			UKeepersJuiceSubsystem::Sound(this, TEXT("Splash"), bSecondTurn ? 0.9f : 0.4f);
		}
		if (bSecondTurn)
		{
			UE_LOG(LogKeepersSurfaces, Log, TEXT("The bog pulls %s"), *Stuck->GetName());
			UKeepersTerrainSubsystem::DamageUnit(CombatSystem, Stuck, KeepersSurfaces::BogDamage);
		}
	}), 0.25f, false);
}

void UKeepersSurfaceSubsystem::OnTurnEnded(UObject* Unit)
{
	FIntPoint Index(-1, -1);
	KeepersReflection::GetIntPoint(Unit->GetClass(), Unit, TEXT("Index"), Index);
	bool bAlive = false;
	KeepersReflection::FCall IsAlive(Unit, TEXT("IsAlive"));
	// Fliers (they may hover over water) do not touch the ice.
	if (!IsAlive.Call() || !IsAlive.Get<bool>(TEXT("IsAlive"), bAlive) || !bAlive || UKeepersTerrainSubsystem::CanUnitStandOn(Unit, 5))
	{
		return;
	}
	FVector Start;
	FVector End;
	if (GetSurface(Index) != EKeepersSurface::Ice || Index == TurnStart || !UKeepersTerrainSubsystem::TileLocation(Grid, TurnStart, Start)
		|| !UKeepersTerrainSubsystem::TileLocation(Grid, Index, End))
	{
		return;
	}
	// On along the way it came while the ice lasts: free, walkable and level cells only.
	const FVector Direction = (End - Start).GetSafeNormal2D();
	const float Step = KeepersSurfaces::TileSize(Grid).Z;
	FIntPoint Cell = Index;
	FVector CellLocation = End;
	int32 Cells = 0;
	while (Cells < KeepersSurfaces::MaxSlide)
	{
		const TOptional<FIntPoint> Next = UKeepersTerrainSubsystem::NextAlong(Grid, Cell, Direction);
		FVector NextLocation;
		uint8 Type = 0;
		if (!Next || !UKeepersTerrainSubsystem::TileLocation(Grid, Next.GetValue(), NextLocation) || !UKeepersTerrainSubsystem::TileType(Grid, Next.GetValue(), Type)
			|| !KeepersSurfaces::IsGround(Type) || !UKeepersTerrainSubsystem::CanUnitStandOn(Unit, Type) || UKeepersTerrainSubsystem::UnitOnTile(Grid, Next.GetValue())
			|| FMath::Abs(NextLocation.Z - CellLocation.Z) > Step * 0.5f)
		{
			break;
		}
		Cell = Next.GetValue();
		CellLocation = NextLocation;
		++Cells;
		if (GetSurface(Cell) != EKeepersSurface::Ice)
		{
			break;
		}
	}
	AActor* Actor = Cast<AActor>(Unit);
	if (Cells == 0 || !Actor)
	{
		return;
	}
	const FVector From = Actor->GetActorLocation();
	KeepersReflection::FCall Move(CombatSystem, TEXT("SetUnitIndexOnGrid"));
	Move.Set<UObject*>(TEXT("Unit"), Unit);
	Move.Set<FIntPoint>(TEXT("Index"), Cell);
	Move.Set<bool>(TEXT("Force"), true);
	Move.Set<bool>(TEXT("SnapLocation"), true);
	Move.Call();
	// The grid already has it at the end; the figure glides there.
	FSlide Slide;
	Slide.Unit = Actor;
	Slide.From = From;
	Slide.To = Actor->GetActorLocation();
	Slide.Time = -Cells * KeepersSurfaces::SlideSecondsPerCell;
	Actor->SetActorLocation(From);
	Slides.Add(Slide);
	UKeepersJuiceSubsystem::Burst(GetWorld(), From, AKeepersBurst::EKind::Puff, KeepersSurfaces::IceColor, 10, 0.6f);
	UKeepersJuiceSubsystem::Sound(this, TEXT("Gust"), 0.6f);
	UE_LOG(LogKeepersSurfaces, Log, TEXT("%s slides %d cells on ice to (%d,%d)"), *Unit->GetName(), Cells, Cell.X, Cell.Y);
}

void UKeepersSurfaceSubsystem::TickSlides(float DeltaTime)
{
	for (int32 I = Slides.Num() - 1; I >= 0; --I)
	{
		FSlide& Slide = Slides[I];
		AActor* Actor = Slide.Unit.Get();
		if (!Actor)
		{
			Slides.RemoveAt(I);
			continue;
		}
		// Time runs from -Duration up to 0.
		Slide.Time = FMath::Min(Slide.Time + DeltaTime, 0.f);
		const float Duration = FMath::Max(FVector::Dist2D(Slide.From, Slide.To) / 200.f * KeepersSurfaces::SlideSecondsPerCell, 0.05f);
		const float Alpha = FMath::Clamp(1.f + Slide.Time / Duration, 0.f, 1.f);
		Actor->SetActorLocation(FMath::Lerp(Slide.From, Slide.To, 1.f - FMath::Square(1.f - Alpha)));
		if (Slide.Time >= 0.f)
		{
			Actor->SetActorLocation(Slide.To);
			Slides.RemoveAt(I);
		}
	}
}
