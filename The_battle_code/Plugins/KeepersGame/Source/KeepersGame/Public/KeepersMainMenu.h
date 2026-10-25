#pragma once

#include "Blueprint/UserWidget.h"
#include "GameFramework/Actor.h"
#include "KeepersFigure.h"
#include "KeepersMainMenu.generated.h"

class UBorder;
class UButton;
class UCameraComponent;
class UMaterialInstanceDynamic;
class UPointLightComponent;
class USkeletalMeshComponent;
class UStaticMeshComponent;
class UTextureRenderTarget2D;
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
	void SetRowWidth(float InWidth) { RowWidth = InWidth; }

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;

private:
	UFUNCTION()
	void HandlePrevious();

	UFUNCTION()
	void HandleNext();

	void Step(int32 Delta);

	FText LabelText;
	float RowWidth = 560.f;
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

	/** Graphics, screen, resolution, VSync and volume rows (also used by the pause menu). */
	static void AddSettingRows(UUserWidget* Owner, UVerticalBox* Target, float LeftPadding);

	/** Opens the squad screen; bStartCampaign: its main button starts a new campaign. */
	void OpenSquad(bool bStartCampaign);

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

	UPROPERTY()
	TObjectPtr<class UKeepersSquadWidget> Squad;
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

	/** Camera shot for a menu page (0 main, 1 quick battle, 2 settings, 3 squad). */
	void SetShot(int32 Shot);

	/** Squad screen: the hero shown up close by the fire. */
	void ShowPreview(FName Hero);
	void PlayPreview(EKeepersPose Pose);
	/** Plays an ability on the preview hero: its animation and a small effect on the target slime. */
	void PlayAbility(FName Hero, FName Spell);
	void RotatePreview(float DeltaYaw);
	/** A hero's outfit or colour changed: updates the preview, the portrait and the hero at the fire. */
	void RefreshHero(FName Hero);
	/** Puts an outfit on the preview hero only (trying on a closed outfit). */
	void TryOnLook(FName Hero, int32 Look);
	/** The squad changed: the heroes sitting at the fire. */
	void RefreshCamp();
	/** Live portrait of a hero for the cards. */
	UTextureRenderTarget2D* GetPortrait(FName Hero) const;

	/** Opens the squad screen, for scripts and tests. */
	UFUNCTION(BlueprintCallable, Category = "Keepers")
	void OpenSquadScreen(bool bStartCampaign);

	/** Squad screen test hooks: choose a hero, play an ability by index. */
	UFUNCTION(BlueprintCallable, Category = "Keepers")
	void TestSquad(FName Hero, int32 Ability);

	/** Squad screen test hook: choose an outfit of the shown hero, and open it if it is closed. */
	UFUNCTION(BlueprintCallable, Category = "Keepers")
	void TestLook(int32 Look, bool bUnlock);

	/** Where the camp fire burns. */
	UPROPERTY(EditAnywhere, Category = "Keepers")
	FVector CampCenter = FVector(-250.f, 150.f, 0.f);

	/** Yaw that turns a unit mesh to look along +X. */
	UPROPERTY(EditAnywhere, Category = "Keepers")
	float MeshYawOffset = -90.f;

	/** Camera shot per menu page (main, quick battle, settings, squad), relative to CampCenter. */
	UPROPERTY(EditAnywhere, Category = "Keepers")
	TArray<FVector> ShotLocations = { FVector(980.f, -330.f, 390.f), FVector(2300.f, -900.f, 2000.f), FVector(520.f, -160.f, 220.f), FVector(520.f, -280.f, 100.f) };

	/** Where each shot looks, relative to CampCenter. The squad shot is placed around the podium at runtime. */
	UPROPERTY(EditAnywhere, Category = "Keepers")
	TArray<FVector> ShotTargets = { FVector(0.f, 150.f, 40.f), FVector(-300.f, 500.f, 0.f), FVector(0.f, 110.f, 50.f), FVector(330.f, -150.f, 55.f) };

	/** Squad shot: horizontal direction from the podium to the camera, distance and heights. */
	UPROPERTY(EditAnywhere, Category = "Keepers")
	FVector SquadCameraDirection = FVector(0.82f, -0.57f, 0.f);

	UPROPERTY(EditAnywhere, Category = "Keepers")
	float SquadCameraDistance = 420.f;

	UPROPERTY(EditAnywhere, Category = "Keepers")
	float SquadCameraHeight = 150.f;

	UPROPERTY(EditAnywhere, Category = "Keepers")
	float SquadLookHeight = 34.f;

	/** Where the squad screen's hero stands, relative to CampCenter. */
	UPROPERTY(EditAnywhere, Category = "Keepers")
	FVector PodiumOffset = FVector(330.f, -150.f, 0.f);

	/** Scale of the Toony figures in the camp. */
	UPROPERTY(EditAnywhere, Category = "Keepers")
	float FigureScale = 0.62f;

private:
	void BuildCamp();
	void BuildSquadStage();
	void SetEveningLight();
	UStaticMeshComponent* AddMesh(const TCHAR* MeshPath, const FTransform& Transform, UMaterialInterface* Material);
	USkeletalMeshComponent* AddUnit(const TCHAR* Name, const FVector& Offset, float Turn, float Scale);
	UKeepersFigureComponent* AddFigure(const FVector& Location, float Yaw, float Scale);
	/** Ground height under a point of the island. */
	float GroundZ(const FVector& Location) const;
	void TickEffects(float DeltaTime);

	FName PreviewHero;
	float PreviewYaw = 0.f;
	float PreviewBaseYaw = 0.f;
	FVector PodiumLocation;
	FVector TargetLocation;

	/** A short-lived effect of a shown ability. */
	struct FEffect
	{
		TWeakObjectPtr<UPrimitiveComponent> Component;
		TWeakObjectPtr<UPointLightComponent> Light;
		AActor* SpawnedActor = nullptr;
		FVector From;
		FVector To;
		float Delay = 0.f;
		float Age = 0.f;
		float Duration = 0.5f;
		bool bArc = false;
		bool bHitsTarget = false;
		float LightIntensity = 0.f;
	};
	TArray<FEffect> Effects;
	float TargetShake = 0.f;
	/** Seconds left of the preview hero facing the target slime (while showing an ability). */
	float FaceTargetTime = 0.f;
	float FaceTargetBlend = 0.f;
	/** Gust pushes the slime back: how far, decaying. */
	float TargetPush = 0.f;

	UPROPERTY()
	TObjectPtr<UKeepersFigureComponent> Preview;

	/** Key light on the podium, on only in the squad shot. */
	UPROPERTY()
	TObjectPtr<UPointLightComponent> PodiumLight;

	UPROPERTY()
	TObjectPtr<USkeletalMeshComponent> TargetSlime;

	UPROPERTY()
	TArray<TObjectPtr<UKeepersFigureComponent>> CampFigures;

	UPROPERTY()
	TMap<FName, TObjectPtr<UKeepersFigureComponent>> StudioFigures;

	UPROPERTY()
	TMap<FName, TObjectPtr<UTextureRenderTarget2D>> Portraits;

	UPROPERTY()
	TArray<TObjectPtr<AActor>> SpawnedEffects;

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
