#include "KeepersTutorial.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "KeepersReflection.h"

bool UKeepersTutorialSubsystem::bPendingTutorial = false;

namespace KeepersTutorial
{
	struct FStepText
	{
		const TCHAR* Title;
		const TCHAR* Text;
		bool bWaitsForClick;
	};

	// Order matches UKeepersTutorialSubsystem::EStep.
	static const FStepText Steps[] = {
		{ TEXT("Добро пожаловать, Хранитель!"),
		  TEXT("Это ваш отряд: Страж, Следопыт и Друид. Против вас — слизни. Сейчас ходит герой, чей портрет подсвечен справа вверху."), true },
		{ TEXT("Камера"),
		  TEXT("Колёсико мыши — приблизить и отдалить. WASD или стрелки — двигать камеру. Q и E — повернуть."), true },
		{ TEXT("Ход"),
		  TEXT("Синие клетки — куда герой дойдёт за этот ход. Наведите курсор на синюю клетку и кликните."), false },
		{ TEXT("Действие"),
		  TEXT("Внизу — способности героя. Выберите одну и кликните по подсвеченной цели. За ход — одно перемещение и одно действие."), false },
		{ TEXT("Любой порядок"),
		  TEXT("Кликните по другому своему герою, чтобы ходить им. Герои ходят в любом порядке."), false },
		{ TEXT("Конец хода"),
		  TEXT("Когда отряд готов, нажмите «Закончить ход» справа внизу. Затем походят враги."), false },
		{ TEXT("Намерения врагов"),
		  TEXT("Красные круги — туда враг ударит в начале своего следующего хода. Уведите героя из-под удара, заслоните клетку скалой Стража или сдуйте врага Порывом ветра."), true },
		{ TEXT("Удачи!"),
		  TEXT("Слизни оставляют лужи и делятся, куры зовут подмогу. Используйте местность: скалы, лес, обрывы. Побеждайте в кампании и собирайте артефакты."), true },
	};

	static UTextBlock* Text(UWidgetTree* Tree, int32 Size, const FLinearColor& Color)
	{
		UTextBlock* Block = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		FSlateFontInfo Font = Block->GetFont();
		Font.Size = Size;
		Block->SetFont(Font);
		Block->SetColorAndOpacity(FSlateColor(Color));
		Block->SetAutoWrapText(true);
		return Block;
	}

	static FSlateBrush Rounded(const FLinearColor& Color)
	{
		FSlateBrush Brush;
		Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
		Brush.TintColor = FSlateColor(Color);
		Brush.OutlineSettings = FSlateBrushOutlineSettings(FVector4(10.f, 10.f, 10.f, 10.f));
		return Brush;
	}

	static UButton* MakeButton(UWidgetTree* Tree, const TCHAR* Label, const FLinearColor& Color)
	{
		UButton* Button = Tree->ConstructWidget<UButton>(UButton::StaticClass());
		FButtonStyle Style = Button->GetStyle();
		Style.Normal = Rounded(Color);
		Style.Hovered = Rounded(Color * 1.3f);
		Style.Pressed = Rounded(Color * 0.8f);
		Style.NormalPadding = FMargin(14.f, 6.f);
		Style.PressedPadding = FMargin(14.f, 7.f, 14.f, 5.f);
		Button->SetStyle(Style);
		UTextBlock* Caption = Text(Tree, 14, FLinearColor::White);
		Caption->SetAutoWrapText(false);
		Caption->SetText(FText::FromString(Label));
		Button->AddChild(Caption);
		return Button;
	}

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

	static int32 TeamOf(const UObject* Unit)
	{
		int32 Team = INDEX_NONE;
		if (Unit)
		{
			KeepersReflection::GetInt(Unit->GetClass(), Unit, TEXT("TeamIndex"), Team);
		}
		return Team;
	}

	/** AP the heroes have spent this phase (sum of AP_Max - AP_Current over team 0). */
	static int32 HeroApSpent(UObject* CombatSystem)
	{
		int32 Spent = 0;
		FArrayProperty* Property = CastField<FArrayProperty>(KeepersReflection::FindMember(CombatSystem->GetClass(), TEXT("UnitsInCombat")));
		FObjectPropertyBase* Inner = Property ? CastField<FObjectPropertyBase>(Property->Inner) : nullptr;
		if (!Inner)
		{
			return 0;
		}
		FScriptArrayHelper Helper(Property, Property->ContainerPtrToValuePtr<void>(CombatSystem));
		for (int32 I = 0; I < Helper.Num(); ++I)
		{
			UObject* Unit = Inner->GetObjectPropertyValue(Helper.GetRawPtr(I));
			int32 Max = 0;
			int32 Current = 0;
			if (TeamOf(Unit) == 0 && KeepersReflection::GetUnitStat(Unit, TEXT("AP_Max"), Max) && KeepersReflection::GetUnitStat(Unit, TEXT("AP_Current"), Current))
			{
				Spent += FMath::Max(0, Max - Current);
			}
		}
		return Spent;
	}
}

// ---------------------------------------------------------------------------------------------------------------------

void UKeepersTutorialWidget::ShowStep(int32 Index, int32 Count, const FText& Title, const FText& Text, bool bShowNext)
{
	if (!TitleText)
	{
		TakeWidget();
	}
	CounterText->SetText(FText::FromString(FString::Printf(TEXT("Обучение · %d из %d"), Index + 1, Count)));
	TitleText->SetText(Title);
	BodyText->SetText(Text);
	NextButton->SetVisibility(bShowNext ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
}

TSharedRef<SWidget> UKeepersTutorialWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		// Full-screen, click-through root; only the panel takes clicks.
		UBorder* Screen = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Screen->SetBrushColor(FLinearColor::Transparent);
		Screen->SetHorizontalAlignment(HAlign_Left);
		Screen->SetVerticalAlignment(VAlign_Top);
		Screen->SetPadding(FMargin(24.f, 90.f, 0.f, 0.f));
		Screen->SetVisibility(ESlateVisibility::SelfHitTestInvisible);

		USizeBox* Size = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		Size->SetWidthOverride(380.f);

		UBorder* Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Panel->SetBrush(KeepersTutorial::Rounded(FLinearColor(0.02f, 0.03f, 0.05f, 0.88f)));
		Panel->SetPadding(FMargin(18.f, 14.f));

		UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		CounterText = KeepersTutorial::Text(WidgetTree, 11, FLinearColor(0.7f, 0.75f, 0.8f));
		TitleText = KeepersTutorial::Text(WidgetTree, 19, FLinearColor(1.f, 0.85f, 0.4f));
		BodyText = KeepersTutorial::Text(WidgetTree, 14, FLinearColor::White);
		Column->AddChildToVerticalBox(CounterText);
		if (UVerticalBoxSlot* TitleSlot = Column->AddChildToVerticalBox(TitleText))
		{
			TitleSlot->SetPadding(FMargin(0.f, 2.f, 0.f, 6.f));
		}
		Column->AddChildToVerticalBox(BodyText);

		UHorizontalBox* Buttons = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		UButton* Skip = KeepersTutorial::MakeButton(WidgetTree, TEXT("Пропустить обучение"), FLinearColor(0.2f, 0.2f, 0.25f));
		NextButton = KeepersTutorial::MakeButton(WidgetTree, TEXT("Далее"), FLinearColor(0.25f, 0.45f, 0.3f));
		Skip->OnClicked.AddDynamic(this, &UKeepersTutorialWidget::HandleSkip);
		NextButton->OnClicked.AddDynamic(this, &UKeepersTutorialWidget::HandleNext);
		Buttons->AddChildToHorizontalBox(Skip);
		if (UHorizontalBoxSlot* NextSlot = Buttons->AddChildToHorizontalBox(NextButton))
		{
			NextSlot->SetPadding(FMargin(10.f, 0.f, 0.f, 0.f));
		}
		if (UVerticalBoxSlot* ButtonsSlot = Column->AddChildToVerticalBox(Buttons))
		{
			ButtonsSlot->SetPadding(FMargin(0.f, 12.f, 0.f, 0.f));
			ButtonsSlot->SetHorizontalAlignment(HAlign_Right);
		}

		Panel->SetContent(Column);
		Size->AddChild(Panel);
		Screen->SetContent(Size);
		WidgetTree->RootWidget = Screen;
	}
	return Super::RebuildWidget();
}

void UKeepersTutorialWidget::HandleNext()
{
	if (OnNext)
	{
		TFunction<void()> Callback = OnNext;
		Callback();
	}
}

void UKeepersTutorialWidget::HandleSkip()
{
	if (OnSkip)
	{
		TFunction<void()> Callback = OnSkip;
		Callback();
	}
}

// ---------------------------------------------------------------------------------------------------------------------

void UKeepersTutorialSubsystem::StartTutorial(UObject* WorldContext)
{
	bPendingTutorial = true;
	if (UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr)
	{
		UGameplayStatics::OpenLevel(World, TEXT("/Game/Maps/Slice/L_Slice_Hex"));
	}
}

TStatId UKeepersTutorialSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UKeepersTutorialSubsystem, STATGROUP_Tickables);
}

bool UKeepersTutorialSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UKeepersTutorialSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	UWorld* World = GetWorld();
	if (!World || !World->HasBegunPlay())
	{
		return;
	}
	if (!bActive)
	{
		if (!bPendingTutorial)
		{
			return;
		}
		UObject* CombatSystem = KeepersTutorial::FindCombatSystem(World);
		bool bInCombat = false;
		if (CombatSystem)
		{
			KeepersReflection::GetBool(CombatSystem->GetClass(), CombatSystem, TEXT("IsInCombat"), bInCombat);
		}
		if (bInCombat && KeepersReflection::GetObject(CombatSystem, TEXT("CurrentUnit")))
		{
			bPendingTutorial = false;
			Begin(CombatSystem);
		}
		return;
	}

	UObject* CombatSystem = Combat.Get();
	if (!CombatSystem)
	{
		Finish();
		return;
	}
	StepTime += DeltaTime;
	if (!KeepersTutorial::Steps[static_cast<int32>(Current)].bWaitsForClick && IsStepDone(CombatSystem))
	{
		Advance();
	}
}

void UKeepersTutorialSubsystem::Begin(UObject* CombatSystem)
{
	APlayerController* Player = GetWorld()->GetFirstPlayerController();
	if (!Player)
	{
		return;
	}
	Combat = CombatSystem;
	bActive = true;
	Widget = CreateWidget<UKeepersTutorialWidget>(Player, UKeepersTutorialWidget::StaticClass());
	TWeakObjectPtr<UKeepersTutorialSubsystem> WeakThis(this);
	Widget->OnNext = [WeakThis]() { if (WeakThis.IsValid()) { WeakThis->Advance(); } };
	Widget->OnSkip = [WeakThis]() { if (WeakThis.IsValid()) { WeakThis->Finish(); } };
	Widget->AddToViewport(40);
	Show(EStep::Welcome);
}

void UKeepersTutorialSubsystem::Show(EStep Step)
{
	Current = Step;
	if (Step == EStep::Done)
	{
		Finish();
		return;
	}
	UObject* CombatSystem = Combat.Get();
	StepUnit = KeepersReflection::GetObject(CombatSystem, TEXT("CurrentUnit"));
	StepUnitIndex = FIntPoint(-999, -999);
	if (UObject* Unit = StepUnit.Get())
	{
		KeepersReflection::GetIntPoint(Unit->GetClass(), Unit, TEXT("Index"), StepUnitIndex);
	}
	StepApSpent = CombatSystem ? KeepersTutorial::HeroApSpent(CombatSystem) : 0;
	StepTime = 0.f;
	bSawEnemyPhase = false;

	const int32 Count = UE_ARRAY_COUNT(KeepersTutorial::Steps);
	const KeepersTutorial::FStepText& Text = KeepersTutorial::Steps[static_cast<int32>(Step)];
	Widget->ShowStep(static_cast<int32>(Step), Count, FText::FromString(Text.Title), FText::FromString(Text.Text), Text.bWaitsForClick);
}

void UKeepersTutorialSubsystem::Advance()
{
	Show(static_cast<EStep>(static_cast<uint8>(Current) + 1));
}

bool UKeepersTutorialSubsystem::IsStepDone(UObject* CombatSystem)
{
	UObject* CurrentUnit = KeepersReflection::GetObject(CombatSystem, TEXT("CurrentUnit"));
	switch (Current)
	{
	case EStep::Move:
	{
		FIntPoint Index(-999, -999);
		UObject* Unit = StepUnit.Get();
		if (Unit)
		{
			KeepersReflection::GetIntPoint(Unit->GetClass(), Unit, TEXT("Index"), Index);
		}
		return Unit && Index != StepUnitIndex;
	}
	case EStep::Ability:
		return KeepersTutorial::HeroApSpent(CombatSystem) > StepApSpent;
	case EStep::SwitchHero:
		return CurrentUnit && CurrentUnit != StepUnit.Get() && KeepersTutorial::TeamOf(CurrentUnit) == 0;
	case EStep::EndTurn:
		// Wait for the enemy phase to end, so the intents are on the board.
		if (CurrentUnit && KeepersTutorial::TeamOf(CurrentUnit) != 0)
		{
			bSawEnemyPhase = true;
		}
		return bSawEnemyPhase && CurrentUnit && KeepersTutorial::TeamOf(CurrentUnit) == 0;
	default:
		return false;
	}
}

void UKeepersTutorialSubsystem::Finish()
{
	bActive = false;
	Current = EStep::Done;
	if (Widget)
	{
		Widget->RemoveFromParent();
		Widget = nullptr;
	}
}
