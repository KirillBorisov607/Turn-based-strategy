#pragma once

#include "Subsystems/WorldSubsystem.h"
#include "KeepersCameraBounds.generated.h"

/**
 * Keeps the battle camera (BP_Pawn) over the battlefield: its position stays inside the grid's bounds
 * plus a margin, and zoom (the spring arm length) stays between MinZoom and a limit that fits the grid.
 */
UCLASS()
class KEEPERSGAME_API UKeepersCameraBounds : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

	/** Extra room around the outermost tiles, in cm. */
	static constexpr float Margin = 600.f;
	static constexpr float MinZoom = 900.f;
	static constexpr float MinMaxZoom = 4500.f;

private:
	bool UpdateBounds();

	TWeakObjectPtr<AActor> Grid;
	FBox2D Bounds = FBox2D(ForceInit);
	float MaxZoom = MinMaxZoom;
	int32 TileCount = 0;
};
