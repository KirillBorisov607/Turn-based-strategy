#pragma once

#include "Blueprint/UserWidget.h"
#include "Subsystems/WorldSubsystem.h"
#include "KeepersBattleHud.generated.h"

class UBorder;
class UTextBlock;

/** Round counter under the battle goal and a big "your turn" / "enemy turn" banner when the phase changes. */
UCLASS()
class KEEPERSGAME_API UKeepersPhaseWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void ShowPhase(int32 Round, bool bPlayer);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	UPROPERTY()
	TObjectPtr<UTextBlock> RoundText;

	UPROPERTY()
	TObjectPtr<UBorder> RoundPill;

	UPROPERTY()
	TObjectPtr<UTextBlock> BannerText;

	UPROPERTY()
	TObjectPtr<UBorder> Banner;

	float BannerTime = 10.f;
};

/**
 * Battle HUD skin: gives the Blueprint combat HUD (W_Combat) the Keepers look, hides the debug controls
 * outside the test map, restyles unit portraits and HP bars, names units in Russian, and shows the
 * round and phase.
 */
UCLASS()
class KEEPERSGAME_API UKeepersBattleHudSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

	/** Russian name of a unit type as the HUD shows it ("Priest" -> "Друид"); empty if unknown. */
	static FText LocalUnitName(const FString& EnglishName);

private:
	void Skin(UUserWidget* Hud);

	/** Styles the widgets the Blueprints create during the battle (portraits, HP bars) and names units in Russian. */
	void RefreshDynamic(UUserWidget* Hud);

	static void SoftenTeamColor(class UProgressBar* Bar);

	TWeakObjectPtr<UUserWidget> SkinnedHud;
	float RefreshTimer = 0.f;

	/** Widgets already given the Keepers look. */
	TSet<TWeakObjectPtr<UWidget>> StyledWidgets;

	UPROPERTY()
	TObjectPtr<UKeepersPhaseWidget> Phase;

	int32 Round = 0;
	int32 LastTeam = INDEX_NONE;
};
