#pragma once

#include "GameFramework/Actor.h"
#include "Subsystems/WorldSubsystem.h"
#include "KeepersJuice.generated.h"

class UStaticMeshComponent;

/** A short burst of little low-poly pieces: sparks of a hit, a heal, a puff of a fallen unit. */
UCLASS(NotPlaceable, Transient)
class KEEPERSGAME_API AKeepersBurst : public AActor
{
	GENERATED_BODY()

public:
	AKeepersBurst();

	enum class EKind : uint8
	{
		/** Pieces fly out and fall. */
		Hit,
		/** Glowing pieces rise and fade. */
		Heal,
		/** Big soft pieces swell and drift up. */
		Puff
	};

	void Start(EKind InKind, const FLinearColor& Color, int32 Count, float Size);

	virtual void Tick(float DeltaSeconds) override;

private:
	struct FPiece
	{
		TObjectPtr<UStaticMeshComponent> Mesh;
		FVector Velocity = FVector::ZeroVector;
		FRotator Spin = FRotator::ZeroRotator;
		float Scale = 1.f;
	};

	EKind Kind = EKind::Hit;
	TArray<FPiece> Pieces;
	float Age = 0.f;
	float Life = 0.7f;
};

/**
 * Makes the fight read at a glance: watches every unit's health and answers each change on the field —
 * sparks in the colour of the one hit, green motes of a heal, a puff when a unit falls.
 */
UCLASS()
class KEEPERSGAME_API UKeepersJuiceSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

	/** Spawns a burst at a place; also for other code (abilities, objectives). */
	static void Burst(UWorld* World, const FVector& Location, AKeepersBurst::EKind Kind, const FLinearColor& Color, int32 Count, float Size = 1.f);

private:
	TMap<TWeakObjectPtr<AActor>, int32> Health;
	float Scan = 0.f;
};
