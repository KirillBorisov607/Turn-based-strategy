#pragma once

#include "GameFramework/Actor.h"
#include "Subsystems/WorldSubsystem.h"
#include "KeepersSurfaces.generated.h"

/** Ground that changes how a cell plays, on top of its tile type. */
UENUM(BlueprintType)
enum class EKeepersSurface : uint8
{
	None,
	/** Who ends a turn on ice slides on along the way it came, until the ice ends or something stops it. */
	Ice,
	/** Marsh: a turn that starts in it has 1 movement point less; two turns in a row on the same cell and the bog pulls (3 damage). */
	Marsh
};

/** Level kit: ice or marsh on the cells around this actor (Radius in cells, 0 = the cell it stands on). */
UCLASS()
class KEEPERSGAME_API AKeepersSurfaceZone : public AActor
{
	GENERATED_BODY()

public:
	AKeepersSurfaceZone();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Surface")
	EKeepersSurface Surface = EKeepersSurface::Ice;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Surface", meta = (ClampMin = "0", ClampMax = "6"))
	int32 Radius = 1;
};

/**
 * Ice and marsh in battle. Cells get a surface from AKeepersSurfaceZone actors of the level, from the region
 * of the campaign (RequestForNextBattle: a few random patches on open ground) and from the Mage's Ice Path.
 * Flat plates show them; the effects follow the turns of BP_CombatSystem.
 */
UCLASS()
class KEEPERSGAME_API UKeepersSurfaceSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	static UKeepersSurfaceSubsystem* Get(const UObject* WorldContext);

	/** The next battle gets patches of this surface. */
	static void RequestForNextBattle(EKeepersSurface Surface);

	EKeepersSurface GetSurface(const FIntPoint& Index) const;
	/** Gives a cell a surface; bShow adds its plate. */
	void Paint(const FIntPoint& Index, EKeepersSurface Surface, bool bShow = true);

	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

private:
	void SetUp();
	void OnTurnEnded(UObject* Unit);
	void OnTurnStarted(UObject* Unit);
	void TickSlides(float DeltaTime);

	static EKeepersSurface Requested;

	TMap<FIntPoint, EKeepersSurface> Surfaces;
	bool bSetUp = false;

	UPROPERTY()
	TObjectPtr<UObject> CombatSystem;

	UPROPERTY()
	TObjectPtr<UObject> Grid;

	UPROPERTY()
	TObjectPtr<AActor> Plates;

	TWeakObjectPtr<UObject> CurrentUnit;
	FIntPoint TurnStart = FIntPoint(-1, -1);
	/** Where each unit began its last turn in the marsh. */
	TMap<TWeakObjectPtr<UObject>, FIntPoint> InMarsh;

	struct FSlide
	{
		TWeakObjectPtr<AActor> Unit;
		FVector From = FVector::ZeroVector;
		FVector To = FVector::ZeroVector;
		float Time = 0.f;
	};
	TArray<FSlide> Slides;
};
