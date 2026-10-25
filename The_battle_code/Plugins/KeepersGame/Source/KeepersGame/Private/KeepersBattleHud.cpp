#include "KeepersBattleHud.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "KeepersAbilityBar.h"
#include "KeepersHeroes.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "KeepersReflection.h"
#include "KeepersSettings.h"
#include "KeepersStyle.h"
#include "UObject/UObjectIterator.h"

DEFINE_LOG_CATEGORY_STATIC(LogKeepersHud, Log, All);

namespace KeepersHud
{
	static UObject* FindCombatSystem(UWorld* World)
	{
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			if (It->GetClass()->GetName() == TEXT("BP_CombatSystem_C"))
			{
				return *It;
			}
		}
		return nullptr;
	}

	/** Every widget of a user widget, nested user widgets included. */
	static void ForEachWidgetDeep(UUserWidget* Root, TFunctionRef<void(UWidget*)> Visit)
	{
		if (!Root || !Root->WidgetTree)
		{
			return;
		}
		Root->WidgetTree->ForEachWidget([&Visit](UWidget* Widget)
		{
			Visit(Widget);
			if (UUserWidget* Nested = Cast<UUserWidget>(Widget))
			{
				ForEachWidgetDeep(Nested, Visit);
			}
		});
	}
}

// ---------------------------------------------------------------------------------------------------------------------

TSharedRef<SWidget> UKeepersPhaseWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
		WidgetTree->RootWidget = Canvas;

		// Round pill, under the battle goal.
		RoundPill = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		RoundPill->SetBrush(KeepersStyle::Panel(KeepersStyle::Ink, 12.f));
		RoundPill->SetPadding(FMargin(14.f, 3.f));
		RoundText = KeepersStyle::MakeText(WidgetTree, FText::GetEmpty(), KeepersStyle::Heading(18), KeepersStyle::TextDim);
		RoundPill->SetContent(RoundText);
		RoundPill->SetVisibility(ESlateVisibility::Collapsed);
		if (UCanvasPanelSlot* PillSlot = Canvas->AddChildToCanvas(RoundPill))
		{
			PillSlot->SetAnchors(FAnchors(0.5f, 0.f));
			PillSlot->SetAlignment(FVector2D(0.5f, 0.f));
			PillSlot->SetPosition(FVector2D(0.f, 62.f));
			PillSlot->SetAutoSize(true);
		}

		// Big banner across the middle.
		Banner = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Banner->SetBrush(KeepersStyle::Panel(FLinearColor(0.f, 0.f, 0.f, 0.55f), 0.f, FLinearColor::Transparent));
		Banner->SetPadding(FMargin(80.f, 10.f));
		Banner->SetHorizontalAlignment(HAlign_Center);
		BannerText = KeepersStyle::MakeText(WidgetTree, FText::GetEmpty(), KeepersStyle::Heading(60), KeepersStyle::Gold);
		Banner->SetContent(BannerText);
		Banner->SetRenderOpacity(0.f);
		Banner->SetVisibility(ESlateVisibility::HitTestInvisible);
		if (UCanvasPanelSlot* BannerSlot = Canvas->AddChildToCanvas(Banner))
		{
			BannerSlot->SetAnchors(FAnchors(0.f, 0.38f, 1.f, 0.38f));
			BannerSlot->SetAlignment(FVector2D(0.f, 0.5f));
			BannerSlot->SetAutoSize(true);
		}
	}
	return Super::RebuildWidget();
}

void UKeepersPhaseWidget::ShowPhase(int32 Round, bool bPlayer)
{
	if (!RoundText)
	{
		return;
	}
	RoundText->SetText(FText::FromString(FString::Printf(TEXT("Раунд %d · %s"), Round, bPlayer ? TEXT("ваш ход") : TEXT("ход врагов"))));
	RoundText->SetColorAndOpacity(FSlateColor(bPlayer ? KeepersStyle::Text : FLinearColor(1.f, 0.6f, 0.5f)));
	RoundPill->SetVisibility(ESlateVisibility::HitTestInvisible);
	BannerText->SetText(FText::FromString(bPlayer ? TEXT("Ваш ход") : TEXT("Ход врагов")));
	BannerText->SetColorAndOpacity(FSlateColor(bPlayer ? KeepersStyle::Gold : FLinearColor(1.f, 0.35f, 0.25f)));
	BannerTime = 0.f;
}

void UKeepersPhaseWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (!Banner)
	{
		return;
	}
	// Fade in, hold, fade out; the text drifts a little while visible.
	BannerTime += InDeltaTime;
	const float In = FMath::Clamp(BannerTime / 0.2f, 0.f, 1.f);
	const float Out = FMath::Clamp((1.5f - BannerTime) / 0.4f, 0.f, 1.f);
	Banner->SetRenderOpacity(FMath::Min(In, Out));
	BannerText->SetRenderTranslation(FVector2D(FMath::Lerp(-30.f, 30.f, FMath::Clamp(BannerTime / 1.5f, 0.f, 1.f)), 0.f));
}

// ---------------------------------------------------------------------------------------------------------------------

TStatId UKeepersBattleHudSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UKeepersBattleHudSubsystem, STATGROUP_Tickables);
}

bool UKeepersBattleHudSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UKeepersBattleHudSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	KeepersSettings::ApplyVolume(&InWorld);
}

void UKeepersBattleHudSubsystem::TestAbility(const UObject* WorldContextObject, int32 Index)
{
	UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	UKeepersBattleHudSubsystem* Hud = World ? World->GetSubsystem<UKeepersBattleHudSubsystem>() : nullptr;
	if (Hud && Hud->AbilityBar)
	{
		Hud->AbilityBar->Press(Index);
	}
}

FText UKeepersBattleHudSubsystem::LocalUnitName(const FString& EnglishName)
{
	static const TMap<FString, FString> Names =
	{
		{ TEXT("warrior"), TEXT("Страж") },
		{ TEXT("ranger"), TEXT("Следопыт") },
		{ TEXT("priest"), TEXT("Друид") },
		{ TEXT("slime"), TEXT("Слизень") },
		{ TEXT("chicken"), TEXT("Курица") },
		{ TEXT("bat"), TEXT("Летучая мышь") },
		{ TEXT("bandit"), TEXT("Разбойник") },
		{ TEXT("banditarcher"), TEXT("Стрелок") },
		{ TEXT("brute"), TEXT("Громила") },
	};
	const FString* Name = Names.Find(EnglishName.TrimStartAndEnd().ToLower());
	return Name ? FText::FromString(*Name) : FText::GetEmpty();
}

void UKeepersBattleHudSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	UWorld* World = GetWorld();
	if (!World || !World->HasBegunPlay())
	{
		return;
	}

	if (!SkinnedHud.IsValid())
	{
		for (TObjectIterator<UUserWidget> It; It; ++It)
		{
			if (It->GetWorld() == World && It->GetClass()->GetName() == TEXT("W_Combat_C") && It->IsInViewport())
			{
				Skin(*It);
				break;
			}
		}
	}
	// The ability bar follows the Blueprint spell list closely: selection must feel instant.
	AbilityTimer -= DeltaTime;
	if (SkinnedHud.IsValid() && AbilityTimer <= 0.f)
	{
		AbilityTimer = 0.05f;
		if (!AbilityBar)
		{
			if (APlayerController* Player = World->GetFirstPlayerController())
			{
				AbilityBar = CreateWidget<UKeepersAbilityBar>(Player, UKeepersAbilityBar::StaticClass());
				AbilityBar->AddToViewport(14);
			}
		}
		if (AbilityBar)
		{
			AbilityBar->Sync(SkinnedHud.Get());
		}
	}

	RefreshTimer -= DeltaTime;
	if (SkinnedHud.IsValid() && RefreshTimer <= 0.f)
	{
		RefreshTimer = 0.25f;
		RefreshDynamic(SkinnedHud.Get());
		// The HP bars over the units live in widget components, outside the HUD.
		for (TObjectIterator<UUserWidget> It; It; ++It)
		{
			if (It->GetWorld() == World && It->GetClass()->GetName() == TEXT("W_HpBar_C"))
			{
				RefreshDynamic(*It);
			}
		}
	}

	// Round and phase.
	UObject* CombatSystem = KeepersHud::FindCombatSystem(World);
	bool bInCombat = false;
	bool bFinishing = false;
	if (CombatSystem)
	{
		KeepersReflection::GetBool(CombatSystem->GetClass(), CombatSystem, TEXT("IsInCombat"), bInCombat);
		KeepersReflection::GetBool(CombatSystem->GetClass(), CombatSystem, TEXT("IsFinishingCombat"), bFinishing);
	}
	const UObject* Current = bInCombat && !bFinishing ? KeepersReflection::GetObject(CombatSystem, TEXT("CurrentUnit")) : nullptr;
	if (!Current)
	{
		if (Phase && bFinishing)
		{
			Phase->SetVisibility(ESlateVisibility::Collapsed);
		}
		return;
	}
	int32 Team = INDEX_NONE;
	KeepersReflection::GetInt(Current->GetClass(), Current, TEXT("TeamIndex"), Team);
	const bool bPlayer = Team == 0;
	const int32 LastSide = LastTeam == INDEX_NONE ? INDEX_NONE : (LastTeam == 0 ? 0 : 1);
	if (LastSide != (bPlayer ? 0 : 1))
	{
		if (bPlayer)
		{
			++Round;
		}
		if (!Phase)
		{
			if (APlayerController* Player = World->GetFirstPlayerController())
			{
				Phase = CreateWidget<UKeepersPhaseWidget>(Player, UKeepersPhaseWidget::StaticClass());
				Phase->AddToViewport(15);
				Phase->SetVisibility(ESlateVisibility::HitTestInvisible);
			}
		}
		if (Phase)
		{
			Phase->ShowPhase(FMath::Max(Round, 1), bPlayer);
		}
	}
	LastTeam = Team;
}

void UKeepersBattleHudSubsystem::Skin(UUserWidget* Hud)
{
	SkinnedHud = Hud;
	UWidgetTree* Tree = Hud->WidgetTree;
	if (!Tree)
	{
		return;
	}
	const bool bTestMap = GetWorld()->GetOutermost()->GetName().Contains(TEXT("L_Debug"));

	auto PanelOf = [Tree](const TCHAR* Name, float Radius, const FMargin& Padding)
	{
		if (UBorder* Border = Cast<UBorder>(Tree->FindWidget(FName(Name))))
		{
			Border->SetBrush(KeepersStyle::Panel(KeepersStyle::Ink, Radius));
			Border->SetPadding(Padding);
		}
	};
	PanelOf(TEXT("Border"), 12.f, FMargin(14.f, 8.f));
	PanelOf(TEXT("Border_2"), 12.f, FMargin(6.f));
	PanelOf(TEXT("Border_Spells"), 14.f, FMargin(10.f, 6.f));
	// The Blueprint spell list keeps working out of sight; UKeepersAbilityBar shows and presses it.
	if (UWidget* Spells = Tree->FindWidget(TEXT("Border_Spells")))
	{
		Spells->SetVisibility(ESlateVisibility::Hidden);
	}
	PanelOf(TEXT("Border_0"), 12.f, FMargin(8.f));
	if (UBorder* EndTurnBorder = Cast<UBorder>(Tree->FindWidget(TEXT("Border_EndTurn"))))
	{
		// The button carries its own look.
		FSlateBrush None;
		None.DrawAs = ESlateBrushDrawType::NoDrawType;
		EndTurnBorder->SetBrush(None);
	}

	if (UButton* EndTurn = Cast<UButton>(Tree->FindWidget(TEXT("Button_EndTurn"))))
	{
		FButtonStyle Style = KeepersStyle::ButtonStyle(KeepersStyle::EButton::Primary);
		Style.NormalPadding = FMargin(28.f, 10.f);
		Style.PressedPadding = FMargin(28.f, 11.f, 28.f, 9.f);
		EndTurn->SetStyle(Style);
		// The Blueprint tints its buttons; the style carries the colour now.
		EndTurn->SetBackgroundColor(FLinearColor::White);
	}
	if (UTextBlock* EndTurnText = Cast<UTextBlock>(Tree->FindWidget(TEXT("Text_1"))))
	{
		EndTurnText->SetText(FText::FromString(TEXT("Закончить ход")));
		EndTurnText->SetColorAndOpacity(FSlateColor(FLinearColor(1.f, 0.97f, 0.9f)));
	}
	for (const TCHAR* Name : { TEXT("Button_EndCombat"), TEXT("Button_x1"), TEXT("Button_x3") })
	{
		if (UButton* Button = Cast<UButton>(Tree->FindWidget(FName(Name))))
		{
			Button->SetStyle(KeepersStyle::ButtonStyle(KeepersStyle::EButton::Secondary));
			Button->SetBackgroundColor(FLinearColor::White);
		}
	}
	// Debug controls only on the test map; the speed buttons stay.
	if (!bTestMap)
	{
		for (const TCHAR* Name : { TEXT("Button_EndCombat"), TEXT("CheckBox_IsTactical") })
		{
			if (UWidget* Widget = Tree->FindWidget(FName(Name)))
			{
				Widget->SetVisibility(ESlateVisibility::Collapsed);
			}
		}
	}
	RefreshDynamic(Hud);
}

void UKeepersBattleHudSubsystem::SoftenTeamColor(UProgressBar* Bar)
{
	// The Blueprint fills HP bars with pure team colours; use the palette's softer ally / enemy tones.
	const FLinearColor Fill = Bar->GetFillColorAndOpacity();
	if (Fill.Equals(KeepersStyle::Ally) || Fill.Equals(KeepersStyle::Enemy))
	{
		return;
	}
	if (Fill.B > Fill.R)
	{
		Bar->SetFillColorAndOpacity(KeepersStyle::Ally);
	}
	else if (Fill.R > Fill.G && Fill.R > Fill.B)
	{
		Bar->SetFillColorAndOpacity(KeepersStyle::Enemy);
	}
}

void UKeepersBattleHudSubsystem::RefreshDynamic(UUserWidget* Hud)
{
	// Portraits, HP bars and unit names are created and recoloured by the Blueprints as the battle
	// goes, so they are styled here repeatedly; the Blueprints keep driving the team colours.
	const int32 StyledBefore = StyledWidgets.Num();
	UKeepersHeroLookSubsystem* Looks = GetWorld() ? GetWorld()->GetSubsystem<UKeepersHeroLookSubsystem>() : nullptr;
	KeepersHud::ForEachWidgetDeep(Hud, [this, Looks](UWidget* Widget)
	{
		const FName Name = Widget->GetFName();
		if (UImage* Image = Cast<UImage>(Widget))
		{
			// Hero icons (TX_Icon_Warrior...) become live portraits of the Toony figures.
			const UTexture2D* Icon = Cast<UTexture2D>(Image->GetBrush().GetResourceObject());
			FString Type;
			const bool bIcon = Icon && (Icon->GetName().Split(TEXT("TX_Icon_"), nullptr, &Type) || Icon->GetName().Split(TEXT("T_Portrait_"), nullptr, &Type));
			if (Looks && bIcon && KeepersHeroes::FindAny(*Type))
			{
				// The card's team ring tells friend from foe (the Blueprint tints it blue or red).
				bool bEnemy = false;
				if (const UUserWidget* Card = Image->GetTypedOuter<UUserWidget>())
				{
					if (const UBorder* Ring = Card->WidgetTree ? Cast<UBorder>(Card->WidgetTree->FindWidget(TEXT("Border_TeamColor"))) : nullptr)
					{
						const FLinearColor Tint = Ring->GetBrushColor();
						bEnemy = Tint.R > Tint.B;
					}
				}
				if (UTextureRenderTarget2D* Portrait = Looks->GetPortrait(*Type, bEnemy))
				{
					Image->SetBrushResourceObject(Portrait);
				}
			}
			return;
		}
		if (UTextBlock* Text = Cast<UTextBlock>(Widget))
		{
			if (Name == TEXT("Text_UnitName"))
			{
				// Unit names come from the data as English type names.
				const FText Local = LocalUnitName(Text->GetText().ToString());
				if (!Local.IsEmpty())
				{
					Text->SetText(Local);
				}
			}
			return;
		}
		if (UBorder* Border = Cast<UBorder>(Widget))
		{
			if (Name == TEXT("Border_IsSelected"))
			{
				// The Blueprint marks the selected portrait white; make it gold.
				const FLinearColor Tint = Border->GetBrushColor();
				if (Tint.A > 0.5f && Tint.R > 0.9f && Tint.G > 0.9f && Tint.B > 0.9f)
				{
					Border->SetBrushColor(KeepersStyle::Gold);
				}
			}
			if ((Name == TEXT("Border_IsSelected") || Name == TEXT("Border_TeamColor")) && !StyledWidgets.Contains(Widget))
			{
				StyledWidgets.Add(Widget);
				// Same tint (set by the Blueprint), rounded shape.
				FSlateBrush Brush = Border->Background;
				Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
				const float Radius = Name == TEXT("Border_IsSelected") ? 20.f : 16.f;
				Brush.OutlineSettings = FSlateBrushOutlineSettings(FVector4(Radius, Radius, Radius, Radius));
				Border->SetBrush(Brush);
				// Outer ring (selection, gold or clear) around the team colour ring.
				Border->SetPadding(FMargin(Name == TEXT("Border_TeamColor") ? 12.f : 9.f));
			}
			return;
		}
		if (StyledWidgets.Contains(Widget))
		{
			if (UProgressBar* StyledBar = Cast<UProgressBar>(Widget))
			{
				SoftenTeamColor(StyledBar);
			}
			return;
		}
		if (UButton* Button = Cast<UButton>(Widget))
		{
			if (Name == TEXT("Button_Unit") || Name == TEXT("Button_Spell"))
			{
				StyledWidgets.Add(Widget);
				FButtonStyle Style = Button->GetStyle();
				Style.Normal = KeepersStyle::Panel(FLinearColor(0.06f, 0.07f, 0.1f, 1.f), 8.f, FLinearColor::Transparent);
				Style.Hovered = KeepersStyle::Panel(FLinearColor(0.12f, 0.13f, 0.17f, 1.f), 8.f, FLinearColor::Transparent);
				Style.Pressed = KeepersStyle::Panel(FLinearColor(0.04f, 0.05f, 0.07f, 1.f), 8.f, FLinearColor::Transparent);
				Style.Disabled = KeepersStyle::Panel(FLinearColor(0.05f, 0.05f, 0.06f, 1.f), 8.f, FLinearColor::Transparent);
				Style.HoveredSlateSound = KeepersStyle::HoverSound();
				Button->SetStyle(Style);
				Button->SetBackgroundColor(FLinearColor::White);
			}
			return;
		}
		if (UProgressBar* Bar = Cast<UProgressBar>(Widget))
		{
			if (Name == TEXT("ProgressBar_Hp"))
			{
				StyledWidgets.Add(Widget);
				FProgressBarStyle Style = Bar->GetWidgetStyle();
				Style.BackgroundImage = KeepersStyle::Panel(FLinearColor(0.f, 0.f, 0.f, 0.7f), 12.f, FLinearColor(1.f, 1.f, 1.f, 0.2f), 2.f);
				// White fill: the Blueprint's team colour tints it.
				Style.FillImage = KeepersStyle::Panel(FLinearColor::White, 12.f, FLinearColor::Transparent);
				Bar->SetWidgetStyle(Style);
				SoftenTeamColor(Bar);
			}
		}
	});
	if (StyledWidgets.Num() != StyledBefore)
	{
		UE_LOG(LogKeepersHud, Log, TEXT("Styled %d HUD widgets in %s"), StyledWidgets.Num() - StyledBefore, *Hud->GetClass()->GetName());
	}
}
