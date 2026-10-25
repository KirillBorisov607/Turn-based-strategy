#pragma once

#include "Blueprint/UserWidget.h"
#include "GameFramework/Actor.h"
#include "KeepersMainMenu.generated.h"

class UBorder;
class UButton;
class UCameraComponent;
class UMaterialInstanceDynamic;
class UPointLightComponent;
class UStaticMeshComponent;
class UTextBlock;
class UVerticalBox;
class AKeepersMenuScene;

/** One line of the main menu: an upper-case caption that lights up and slides when hovered. */
UCLASS()
class KEEPERSGAME_API UKeepersMenuItem : public UUserWidget
{
	GENERATED_BODY()

public:
	void Setup(const FText& Label, const FText& Description, TFunction<void()> InOnClicked, bool bEnabled = true);

	/** Called when the mouse enters the item, with its description. */
	TFunction<void(const FText&)> OnHoveredDescription;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	UFUNCTION()
	void HandleClicked();

	UFUNCTION()
	void HandleHovered();

	FText LabelText;
	FText DescriptionText;
	TFunction<void()> OnClicked;
	bool bItemEnabled = true;
	float Highlight = 0.f;

	UPROPERTY()
	TObjectPtr<UButton> Button;

	UPROPERTY()
	TObjectPtr<UTextBlock> Caption;

	UPROPERTY()
	TObjectPtr<UBorder> Bar;

	UPROPERTY()
	TObjectPtr<UBorder> Glow;
};

/** A settings line: label on the left, "‹ value ›" on the right. */
UCLASS()
class KEEPERSGAME_API UKeepersSettingRow : public UUserWidget
{
	GENERATED_BODY()

public:
	void Setup(const FText& Label, TArray<FText> InOptions, int32 InCurrent, TFunction<void(int32)> InOnChanged);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;

private:
	UFUNCTION()
	void HandlePrevious();

	UFUNCTION()
	void HandleNext();

	void Step(int32 Delta);

	FText LabelText;
	TArray<FText> Options;
	int32 Current = 0;
	TFunction<void(int32)> OnChanged;

	UPROPERTY()
	TObjectPtr<UTextBlock> ValueText;
};

/**
 * The main menu in the style of the Ultimate Menu System reference: title and a list of pages on the left
 * over a dark fade, the 3D camp scene on the right, a hint bar at the bottom.
 * Pages: main, quick battle, settings. Esc goes back.
 */
UCLASS()
class KEEPERSGAME_API UKeepersMainMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	enum class EPage : uint8
	{
		Main,
		QuickBattle,
		Settings
	};

	TWeakObjectPtr<AKeepersMenuScene> Scene;

	void ShowPage(EPage Page);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

private:
	UKeepersMenuItem* AddItem(const TCHAR* Label, const TCHAR* Description, TFunction<void()> OnClicked, bool bEnabled = true);
	void AddSettings();
	void SetDescription(const FText& Text);

	/** Fades the screen out, then runs Action (opening a level). */
	void Leave(TFunction<void()> Action);

	void AskNewCampaign();

	UFUNCTION()
	void HandleConfirmYes();

	UFUNCTION()
	void HandleConfirmNo();

	EPage CurrentPage = EPage::Main;
	bool bLeaving = false;

	UPROPERTY()
	TObjectPtr<UVerticalBox> List;

	UPROPERTY()
	TObjectPtr<UTextBlock> PageTitle;

	UPROPERTY()
	TObjectPtr<UTextBlock> Description;

	UPROPERTY()
	TObjectPtr<UBorder> Confirm;
};

/**
 * The main menu level director: streams the green hexagon island, builds a little camp (fire, logs,
 * smoke, the three Keepers and a chicken), turns the light to evening, shows the menu widget and flies
 * the camera between a shot per menu page.
 */
UCLASS()
class KEEPERSGAME_API AKeepersMenuScene : public AActor
{
	GENERATED_BODY()

public:
	AKeepersMenuScene();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	/** Opens a menu page (0 main, 1 quick battle, 2 settings), for scripts and tests. */
	UFUNCTION(BlueprintCallable, Category = "Keepers")
	void ShowMenuPage(int32 Page);

	/** Camera shot for a menu page (0 main, 1 quick battle, 2 settings). */
	void SetShot(int32 Shot);

	/** Where the camp fire burns. */
	UPROPERTY(EditAnywhere, Category = "Keepers")
	FVector CampCenter = FVector(-250.f, 150.f, 0.f);

	/** Yaw that turns a unit mesh to look along +X. */
	UPROPERTY(EditAnywhere, Category = "Keepers")
	float MeshYawOffset = -90.f;

	/** Camera shot per menu page (main, quick battle, settings), relative to CampCenter. */
	UPROPERTY(EditAnywhere, Category = "Keepers")
	TArray<FVector> ShotLocations = { FVector(980.f, -330.f, 390.f), FVector(2300.f, -900.f, 2000.f), FVector(520.f, -160.f, 220.f) };

	/** Where each shot looks, relative to CampCenter. */
	UPROPERTY(EditAnywhere, Category = "Keepers")
	TArray<FVector> ShotTargets = { FVector(0.f, 150.f, 40.f), FVector(-300.f, 500.f, 0.f), FVector(0.f, 110.f, 50.f) };

private:
	void BuildCamp();
	void SetEveningLight();
	UStaticMeshComponent* AddMesh(const TCHAR* MeshPath, const FTransform& Transform, UMaterialInterface* Material);
	void AddUnit(const TCHAR* Name, const FVector& Offset, float Turn, float Scale);

	int32 CurrentShot = 0;
	FVector CameraLocation;
	FVector CameraTarget;
	float Time = 0.f;

	UPROPERTY(VisibleAnywhere, Category = "Keepers")
	TObjectPtr<UCameraComponent> Camera;

	UPROPERTY()
	TObjectPtr<UPointLightComponent> FireLight;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> Flames;

	UPROPERTY()
	TObjectPtr<UKeepersMainMenuWidget> Menu;
};
