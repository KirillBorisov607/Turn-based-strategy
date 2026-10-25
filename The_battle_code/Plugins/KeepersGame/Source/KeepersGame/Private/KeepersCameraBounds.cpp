#include "KeepersCameraBounds.h"

#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "KeepersReflection.h"

TStatId UKeepersCameraBounds::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UKeepersCameraBounds, STATGROUP_Tickables);
}

bool UKeepersCameraBounds::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

bool UKeepersCameraBounds::UpdateBounds()
{
	if (!Grid.IsValid())
	{
		for (TActorIterator<AActor> It(GetWorld()); It; ++It)
		{
			if (It->GetClass()->GetName() == TEXT("BP_Grid_C"))
			{
				Grid = *It;
				break;
			}
		}
	}
	AActor* GridActor = Grid.Get();
	FMapProperty* TilesProperty = GridActor ? CastField<FMapProperty>(KeepersReflection::FindMember(GridActor->GetClass(), TEXT("GridTiles"))) : nullptr;
	const FStructProperty* TileProperty = TilesProperty ? CastField<FStructProperty>(TilesProperty->ValueProp) : nullptr;
	if (!TileProperty)
	{
		return false;
	}
	FScriptMapHelper Tiles(TilesProperty, TilesProperty->ContainerPtrToValuePtr<void>(GridActor));
	if (Tiles.Num() == TileCount)
	{
		return Bounds.bIsValid;
	}
	TileCount = Tiles.Num();

	// Bounds of the tiles units can use (type None marks holes and off-map cells).
	Bounds = FBox2D(ForceInit);
	for (FScriptMapHelper::FIterator It(Tiles); It; ++It)
	{
		const uint8* Tile = Tiles.GetValuePtr(It);
		uint8 Type = 0;
		KeepersReflection::GetEnumByte(TileProperty->Struct, Tile, TEXT("Type"), Type);
		const void* Transform = KeepersReflection::FindValuePtr(TileProperty->Struct, const_cast<uint8*>(Tile), TEXT("Transform"));
		if (Type != 0 && Transform)
		{
			const FVector Location = static_cast<const FTransform*>(Transform)->GetLocation();
			Bounds += FVector2D(Location.X, Location.Y);
		}
	}
	if (!Bounds.bIsValid)
	{
		return false;
	}
	Bounds = Bounds.ExpandBy(Margin);
	MaxZoom = FMath::Max(MinMaxZoom, Bounds.GetSize().GetMax() * 0.9f);
	return true;
}

void UKeepersCameraBounds::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	APlayerController* Player = GetWorld()->GetFirstPlayerController();
	APawn* Pawn = Player ? Player->GetPawn() : nullptr;
	if (!Pawn || Pawn->GetClass()->GetName() != TEXT("BP_Pawn_C") || !UpdateBounds())
	{
		return;
	}

	// BP_Pawn eases towards Location_Desired / Zoom_Desired; bounding the targets keeps the motion smooth.
	FProperty* LocationProperty = nullptr;
	if (FVector* Desired = static_cast<FVector*>(KeepersReflection::FindValuePtr(Pawn->GetClass(), Pawn, TEXT("Location_Desired"), &LocationProperty)))
	{
		Desired->X = FMath::Clamp(Desired->X, Bounds.Min.X, Bounds.Max.X);
		Desired->Y = FMath::Clamp(Desired->Y, Bounds.Min.Y, Bounds.Max.Y);
	}
	auto SetDouble = [Pawn](const TCHAR* Name, double Value)
	{
		FProperty* Property = nullptr;
		void* Ptr = KeepersReflection::FindValuePtr(Pawn->GetClass(), Pawn, Name, &Property);
		if (FDoubleProperty* DoubleProperty = CastField<FDoubleProperty>(Property))
		{
			DoubleProperty->SetPropertyValue(Ptr, Value);
		}
	};
	SetDouble(TEXT("Zoom_Min"), MinZoom);
	SetDouble(TEXT("Zoom_Max"), MaxZoom);
	FProperty* ZoomProperty = nullptr;
	if (FDoubleProperty* Zoom = CastField<FDoubleProperty>(KeepersReflection::FindMember(Pawn->GetClass(), TEXT("Zoom_Desired"))))
	{
		void* Ptr = Zoom->ContainerPtrToValuePtr<void>(Pawn);
		Zoom->SetPropertyValue(Ptr, FMath::Clamp(Zoom->GetPropertyValue(Ptr), static_cast<double>(MinZoom), static_cast<double>(MaxZoom)));
	}
}
