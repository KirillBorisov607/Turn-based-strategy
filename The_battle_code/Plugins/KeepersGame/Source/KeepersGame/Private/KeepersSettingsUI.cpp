#include "KeepersSettingsUI.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ScrollBox.h"
#include "Components/Slider.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/GameUserSettings.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "KeepersBattleHud.h"
#include "KeepersMainMenu.h"
#include "KeepersReflection.h"
#include "KeepersSettings.h"
#include "KeepersSquad.h"
#include "KeepersStyle.h"

namespace KeepersSettingsUI
{
	static FText T(const FString& Text)
	{
		return FText::FromString(Text);
	}

	static TArray<FText> Texts(std::initializer_list<const TCHAR*> Lines)
	{
		TArray<FText> Result;
		for (const TCHAR* Line : Lines)
		{
			Result.Add(FText::FromString(Line));
		}
		return Result;
	}

	static TArray<FText> Quality()
	{
		return Texts({ TEXT("Низко"), TEXT("Средне"), TEXT("Высоко"), TEXT("Эпично") });
	}

	static TArray<FText> Percents(int32 From, int32 To, int32 Step)
	{
		TArray<FText> Result;
		for (int32 P = From; P <= To; P += Step)
		{
			Result.Add(FText::FromString(FString::Printf(TEXT("%d%%"), P)));
		}
		return Result;
	}

	static const float SpeedSteps[] = { 0.5f, 0.75f, 1.f, 1.25f, 1.5f, 2.f };

	static int32 SpeedIndex(float Value)
	{
		int32 Best = 2;
		for (int32 I = 0; I < UE_ARRAY_COUNT(SpeedSteps); ++I)
		{
			if (FMath::Abs(SpeedSteps[I] - Value) < FMath::Abs(SpeedSteps[Best] - Value))
			{
				Best = I;
			}
		}
		return Best;
	}

	/** A double variable of a Blueprint object. */
	static double* DoubleOf(UObject* Object, const TCHAR* Name)
	{
		FProperty* Property = nullptr;
		void* Ptr = Object ? KeepersReflection::FindValuePtr(Object->GetClass(), Object, Name, &Property) : nullptr;
		return CastField<FDoubleProperty>(Property) ? static_cast<double*>(Ptr) : nullptr;
	}
}

// ---------------------------------------------------------------------------------------------------------------------

TSharedRef<SWidget> UKeepersSettingsPanel::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		UHorizontalBox* TabRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		const TCHAR* Names[] = { TEXT("Графика"), TEXT("Звук"), TEXT("Управление"), TEXT("Игра") };
		TWeakObjectPtr<UKeepersSettingsPanel> WeakThis(this);
		for (int32 I = 0; I < UE_ARRAY_COUNT(Names); ++I)
		{
			UKeepersFnButton* Tab = CreateWidget<UKeepersFnButton>(this, UKeepersFnButton::StaticClass());
			Tab->Setup(KeepersSettingsUI::T(Names[I]), UKeepersFnButton::EKind::Secondary, 18, [WeakThis, I]() { if (WeakThis.IsValid()) { WeakThis->ShowTab(I); } }, 134.f);
			TabRow->AddChildToHorizontalBox(Tab)->SetPadding(FMargin(0.f, 0.f, 8.f, 0.f));
			Tabs.Add(Tab);
		}
		Column->AddChildToVerticalBox(TabRow)->SetPadding(FMargin(0.f, 0.f, 0.f, 10.f));
		// Long tabs scroll inside a fixed height, so the panel fits the menu and the pause screen.
		Rows = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		UScrollBox* Scroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass());
		Scroll->SetAlwaysShowScrollbar(false);
		Scroll->SetScrollbarThickness(FVector2D(6.f, 6.f));
		Scroll->AddChild(Rows);
		USizeBox* ScrollSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		ScrollSize->SetMaxDesiredHeight(400.f);
		ScrollSize->SetWidthOverride(580.f);
		ScrollSize->AddChild(Scroll);
		Column->AddChildToVerticalBox(ScrollSize);
		UKeepersFnButton* Reset = CreateWidget<UKeepersFnButton>(this, UKeepersFnButton::StaticClass());
		Reset->Setup(KeepersSettingsUI::T(TEXT("Сбросить раздел")), UKeepersFnButton::EKind::Secondary, 16, [WeakThis]() { if (WeakThis.IsValid()) { WeakThis->ResetTab(); } }, 220.f);
		UVerticalBoxSlot* ResetSlot = Column->AddChildToVerticalBox(Reset);
		ResetSlot->SetPadding(FMargin(0.f, 10.f, 0.f, 0.f));
		ResetSlot->SetHorizontalAlignment(HAlign_Left);
		WidgetTree->RootWidget = Column;
		ShowTab(CurrentTab);
	}
	return Super::RebuildWidget();
}

void UKeepersSettingsPanel::AddSlider(const TCHAR* Label, float Value, TFunction<void(float)> OnChanged, TFunction<FString(float)> Format)
{
	UKeepersSliderRow* Row = CreateWidget<UKeepersSliderRow>(this, UKeepersSliderRow::StaticClass());
	Row->Setup(KeepersSettingsUI::T(Label), Value, MoveTemp(OnChanged), MoveTemp(Format));
	Rows->AddChildToVerticalBox(Row)->SetPadding(FMargin(0.f, 3.f));
}

void UKeepersSettingsPanel::AddKeyRow(const TCHAR* Id, const TCHAR* Label)
{
	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	USizeBox* LabelSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	LabelSize->SetWidthOverride(330.f);
	LabelSize->AddChild(KeepersStyle::MakeText(WidgetTree, KeepersSettingsUI::T(Label), KeepersStyle::Body(16)));
	Row->AddChildToHorizontalBox(LabelSize)->SetVerticalAlignment(VAlign_Center);
	const bool bWaiting = Rebinding == Id;
	const FString KeyName = bWaiting ? FString(TEXT("Нажмите клавишу…")) : KeepersSettings::GetKey(Id).GetDisplayName().ToString();
	UKeepersFnButton* Button = CreateWidget<UKeepersFnButton>(this, UKeepersFnButton::StaticClass());
	TWeakObjectPtr<UKeepersSettingsPanel> WeakThis(this);
	const FString Control = Id;
	Button->Setup(KeepersSettingsUI::T(KeyName), bWaiting ? UKeepersFnButton::EKind::Primary : UKeepersFnButton::EKind::Secondary, 15, [WeakThis, Control]()
	{
		if (WeakThis.IsValid())
		{
			WeakThis->Rebinding = Control;
			WeakThis->ShowTab(WeakThis->CurrentTab);
			WeakThis->SetKeyboardFocus();
		}
	}, 220.f);
	Row->AddChildToHorizontalBox(Button);
	Rows->AddChildToVerticalBox(Row)->SetPadding(FMargin(0.f, 2.f));
}

FReply UKeepersSettingsPanel::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (Rebinding.IsEmpty())
	{
		return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
	}
	// Esc cancels; any other key becomes the control's key (and leaves the control that had it).
	const FKey Key = InKeyEvent.GetKey();
	if (Key != EKeys::Escape)
	{
		for (const KeepersSettings::FKeyBind& Bind : KeepersSettings::KeyBinds())
		{
			if (Rebinding != Bind.Id && KeepersSettings::GetKey(Bind.Id) == Key)
			{
				KeepersSettings::SetKey(Bind.Id, KeepersSettings::GetKey(*Rebinding));
			}
		}
		KeepersSettings::SetKey(*Rebinding, Key);
		KeepersSettings::ApplyKeys(this);
	}
	Rebinding.Reset();
	ShowTab(CurrentTab);
	return FReply::Handled();
}

FReply UKeepersSettingsPanel::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	// A mouse button can be a control's key too (except the left one, which presses the buttons).
	if (!Rebinding.IsEmpty() && InMouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
	{
		KeepersSettings::SetKey(*Rebinding, InMouseEvent.GetEffectingButton());
		KeepersSettings::ApplyKeys(this);
		Rebinding.Reset();
		ShowTab(CurrentTab);
		return FReply::Handled();
	}
	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

void UKeepersSettingsPanel::AddRow(const TCHAR* Label, TArray<FText> Options, int32 Current, TFunction<void(int32)> OnChanged)
{
	UKeepersSettingRow* Row = CreateWidget<UKeepersSettingRow>(this, UKeepersSettingRow::StaticClass());
	Row->SetRowWidth(560.f);
	Row->Setup(KeepersSettingsUI::T(Label), MoveTemp(Options), Current, MoveTemp(OnChanged));
	Rows->AddChildToVerticalBox(Row)->SetPadding(FMargin(0.f, 2.f));
}

void UKeepersSettingsPanel::ShowTab(int32 Tab)
{
	using namespace KeepersSettingsUI;
	CurrentTab = Tab;
	for (int32 I = 0; I < Tabs.Num(); ++I)
	{
		Tabs[I]->SetActive(I == Tab);
	}
	Rows->ClearChildren();
	UGameUserSettings* Settings = GEngine ? GEngine->GetGameUserSettings() : nullptr;
	TWeakObjectPtr<UKeepersSettingsPanel> WeakThis(this);
	auto Toggle = [](float Value) { return Value > 0.5f ? 1 : 0; };

	if (Tab == 0 && Settings)
	{
		AddRow(TEXT("Общее качество"), Quality(), FMath::Clamp(Settings->GetOverallScalabilityLevel(), 0, 3), [Settings, WeakThis](int32 Index)
		{
			Settings->SetOverallScalabilityLevel(Index);
			Settings->ApplySettings(false);
			// The detail lines follow the overall level.
			if (WeakThis.IsValid())
			{
				WeakThis->ShowTab(0);
			}
		});
		// Sizes the display supports, the desktop's own among them, without duplicates, small to large.
		TArray<FIntPoint> Resolutions;
		UKismetSystemLibrary::GetSupportedFullscreenResolutions(Resolutions);
		const FIntPoint Desktop = Settings->GetDesktopResolution();
		Resolutions.AddUnique(Desktop);
		Resolutions.RemoveAll([](const FIntPoint& R) { return R.X < 1024 || R.Y < 576; });
		if (Resolutions.Num() == 0)
		{
			Resolutions = { FIntPoint(1280, 720), FIntPoint(1600, 900), FIntPoint(1920, 1080) };
		}
		Resolutions.Sort([](const FIntPoint& A, const FIntPoint& B) { return A.X * A.Y < B.X * B.Y; });
		TArray<FText> ResolutionNames;
		const FIntPoint Wanted = KeepersSettings::GetWantedResolution();
		int32 CurrentResolution = Resolutions.Num() - 1;
		for (int32 I = 0; I < Resolutions.Num(); ++I)
		{
			const bool bNative = Resolutions[I] == Desktop;
			ResolutionNames.Add(T(FString::Printf(TEXT("%d × %d%s"), Resolutions[I].X, Resolutions[I].Y, bNative ? TEXT(" (экран)") : TEXT(""))));
			if (Resolutions[I] == Wanted)
			{
				CurrentResolution = I;
			}
		}
		AddRow(TEXT("Разрешение"), MoveTemp(ResolutionNames), CurrentResolution, [Settings, Resolutions](int32 Index)
		{
			KeepersSettings::ApplyScreen(Resolutions[Index], Settings->GetFullscreenMode());
		});
		const EWindowMode::Type Mode = Settings->GetFullscreenMode();
		AddRow(TEXT("Режим экрана"), Texts({ TEXT("Полный экран"), TEXT("Без рамки"), TEXT("Окно") }),
			Mode == EWindowMode::Fullscreen ? 0 : Mode == EWindowMode::WindowedFullscreen ? 1 : 2, [Settings](int32 Index)
		{
			KeepersSettings::ApplyScreen(KeepersSettings::GetWantedResolution(),
				Index == 0 ? EWindowMode::Fullscreen : Index == 1 ? EWindowMode::WindowedFullscreen : EWindowMode::Windowed);
		});
		AddRow(TEXT("Масштаб рендера"), Percents(50, 100, 10), FMath::Clamp(FMath::RoundToInt(Settings->GetResolutionScaleNormalized() * 5.f), 0, 5), [Settings](int32 Index)
		{
			Settings->SetResolutionScaleNormalized(Index / 5.f);
			Settings->ApplySettings(false);
		});
		AddRow(TEXT("Верт. синхронизация"), Texts({ TEXT("Выкл"), TEXT("Вкл") }), Settings->IsVSyncEnabled() ? 1 : 0, [Settings](int32 Index)
		{
			Settings->SetVSyncEnabled(Index == 1);
			Settings->ApplySettings(false);
		});
		static const float Limits[] = { 30.f, 60.f, 120.f, 144.f, 0.f };
		int32 CurrentLimit = 4;
		for (int32 I = 0; I < UE_ARRAY_COUNT(Limits); ++I)
		{
			if (FMath::IsNearlyEqual(Settings->GetFrameRateLimit(), Limits[I]))
			{
				CurrentLimit = I;
			}
		}
		AddRow(TEXT("Ограничение FPS"), Texts({ TEXT("30"), TEXT("60"), TEXT("120"), TEXT("144"), TEXT("Без ограничения") }), CurrentLimit, [Settings](int32 Index)
		{
			Settings->SetFrameRateLimit(Limits[Index]);
			Settings->ApplySettings(false);
		});
		struct FQualityLine { const TCHAR* Label; int32 (UGameUserSettings::*Get)() const; void (UGameUserSettings::*Set)(int32); };
		const FQualityLine Lines[] =
		{
			{ TEXT("Тени"), &UGameUserSettings::GetShadowQuality, &UGameUserSettings::SetShadowQuality },
			{ TEXT("Текстуры"), &UGameUserSettings::GetTextureQuality, &UGameUserSettings::SetTextureQuality },
			{ TEXT("Эффекты"), &UGameUserSettings::GetVisualEffectQuality, &UGameUserSettings::SetVisualEffectQuality },
			{ TEXT("Сглаживание"), &UGameUserSettings::GetAntiAliasingQuality, &UGameUserSettings::SetAntiAliasingQuality },
			{ TEXT("Постобработка"), &UGameUserSettings::GetPostProcessingQuality, &UGameUserSettings::SetPostProcessingQuality },
			{ TEXT("Дальность прорисовки"), &UGameUserSettings::GetViewDistanceQuality, &UGameUserSettings::SetViewDistanceQuality },
			{ TEXT("Растительность"), &UGameUserSettings::GetFoliageQuality, &UGameUserSettings::SetFoliageQuality },
			{ TEXT("Освещение"), &UGameUserSettings::GetGlobalIlluminationQuality, &UGameUserSettings::SetGlobalIlluminationQuality },
		};
		for (const FQualityLine& Line : Lines)
		{
			auto Setter = Line.Set;
			AddRow(Line.Label, Quality(), FMath::Clamp((Settings->*Line.Get)(), 0, 3), [Settings, Setter](int32 Index)
			{
				(Settings->*Setter)(Index);
				Settings->ApplySettings(false);
			});
		}
		AddRow(TEXT("Показывать FPS"), Texts({ TEXT("Нет"), TEXT("Да") }), Toggle(KeepersSettings::Get(KeepersSettings::ShowFps)), [WeakThis](int32 Index)
		{
			KeepersSettings::Set(KeepersSettings::ShowFps, Index);
			UKeepersSettingsSubsystem::ApplyNow(WeakThis.Get());
		});
	}
	else if (Tab == 1)
	{
		struct FVolumeLine { const TCHAR* Label; const TCHAR* Key; };
		const FVolumeLine Lines[] =
		{
			{ TEXT("Общая громкость"), KeepersSettings::Volume },
			{ TEXT("Музыка"), KeepersSettings::MusicVolume },
			{ TEXT("Окружение"), KeepersSettings::AmbientVolume },
			{ TEXT("Эффекты"), KeepersSettings::EffectsVolume },
			{ TEXT("Интерфейс"), KeepersSettings::UiVolume },
		};
		for (const FVolumeLine& Line : Lines)
		{
			const TCHAR* Key = Line.Key;
			AddSlider(Line.Label, FMath::Clamp(KeepersSettings::Get(Key), 0.f, 1.f), [WeakThis, Key](float Value)
			{
				KeepersSettings::Set(Key, Value);
				KeepersSettings::ApplyVolume(WeakThis.Get());
			});
		}
	}
	else if (Tab == 2)
	{
		const TArray<FText> Speeds = Texts({ TEXT("50%"), TEXT("75%"), TEXT("100%"), TEXT("125%"), TEXT("150%"), TEXT("200%") });
		struct FSpeedLine { const TCHAR* Label; const TCHAR* Key; };
		const FSpeedLine Lines[] =
		{
			{ TEXT("Скорость камеры"), KeepersSettings::CameraSpeed },
			{ TEXT("Скорость поворота"), KeepersSettings::RotateSpeed },
			{ TEXT("Скорость приближения"), KeepersSettings::ZoomSpeed },
		};
		for (const FSpeedLine& Line : Lines)
		{
			const TCHAR* Key = Line.Key;
			// 0..1 on the slider is ×0.5..×2.
			AddSlider(Line.Label, FMath::Clamp((KeepersSettings::Get(Key) - 0.5f) / 1.5f, 0.f, 1.f), [WeakThis, Key](float Value)
			{
				KeepersSettings::Set(Key, 0.5f + Value * 1.5f);
				UKeepersSettingsSubsystem::ApplyNow(WeakThis.Get());
			}, [](float Value) { return FString::Printf(TEXT("×%.2g"), 0.5f + Value * 1.5f); });
		}
		AddRow(TEXT("Инверсия колёсика"), Texts({ TEXT("Нет"), TEXT("Да") }), Toggle(KeepersSettings::Get(KeepersSettings::InvertZoom)), [WeakThis](int32 Index)
		{
			KeepersSettings::Set(KeepersSettings::InvertZoom, Index);
			UKeepersSettingsSubsystem::ApplyNow(WeakThis.Get());
		});
		AddRow(TEXT("Прокрутка у края"), Texts({ TEXT("Нет"), TEXT("Да") }), Toggle(KeepersSettings::Get(KeepersSettings::EdgeScroll)), [](int32 Index)
		{
			KeepersSettings::Set(KeepersSettings::EdgeScroll, Index);
		});
		Rows->AddChildToVerticalBox(KeepersStyle::MakeText(WidgetTree, T(TEXT("Клавиши")), KeepersStyle::Heading(20), KeepersStyle::Gold))->SetPadding(FMargin(0.f, 12.f, 0.f, 4.f));
		for (const KeepersSettings::FKeyBind& Bind : KeepersSettings::KeyBinds())
		{
			AddKeyRow(Bind.Id, Bind.Label);
		}
		USizeBox* KeysSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		KeysSize->SetWidthOverride(560.f);
		KeysSize->AddChild(KeepersStyle::MakeText(WidgetTree,
			T(TEXT("Щёлкните по клавише и нажмите новую (Esc — отмена). Стрелки всегда двигают камеру, Esc всегда открывает паузу.")),
			KeepersStyle::Body(13), KeepersStyle::TextDim, true));
		Rows->AddChildToVerticalBox(KeysSize)->SetPadding(FMargin(4.f, 8.f, 0.f, 0.f));
	}
	else
	{
		static const float BattleSpeeds[] = { 1.f, 2.f, 3.f };
		const float Current = KeepersSettings::Get(KeepersSettings::BattleSpeed);
		AddRow(TEXT("Скорость боя"), Texts({ TEXT("x1"), TEXT("x2"), TEXT("x3") }), Current >= 2.5f ? 2 : Current >= 1.5f ? 1 : 0, [WeakThis](int32 Index)
		{
			KeepersSettings::Set(KeepersSettings::BattleSpeed, BattleSpeeds[Index]);
			UKeepersSettingsSubsystem::ApplyNow(WeakThis.Get());
		});
		AddRow(TEXT("Реплики героев"), Texts({ TEXT("Выкл"), TEXT("Вкл") }), Toggle(KeepersSettings::Get(KeepersSettings::Barks)), [](int32 Index)
		{
			KeepersSettings::Set(KeepersSettings::Barks, Index);
		});
	}
}

void UKeepersSettingsPanel::ResetTab()
{
	UGameUserSettings* Settings = GEngine ? GEngine->GetGameUserSettings() : nullptr;
	auto ResetKeys = [](std::initializer_list<const TCHAR*> Keys)
	{
		for (const TCHAR* Key : Keys)
		{
			KeepersSettings::Set(Key, KeepersSettings::DefaultOf(Key));
		}
	};
	switch (CurrentTab)
	{
	case 0:
		if (Settings)
		{
			// Keep the screen as it is; reset the picture.
			const FIntPoint Resolution = Settings->GetScreenResolution();
			const EWindowMode::Type Mode = Settings->GetFullscreenMode();
			Settings->SetToDefaults();
			Settings->SetScreenResolution(Resolution);
			Settings->SetFullscreenMode(Mode);
			Settings->SetOverallScalabilityLevel(3);
			Settings->ApplySettings(false);
		}
		ResetKeys({ KeepersSettings::ShowFps });
		break;
	case 1:
		ResetKeys({ KeepersSettings::Volume, KeepersSettings::MusicVolume, KeepersSettings::AmbientVolume, KeepersSettings::EffectsVolume, KeepersSettings::UiVolume });
		KeepersSettings::ApplyVolume(this);
		break;
	case 2:
		ResetKeys({ KeepersSettings::CameraSpeed, KeepersSettings::RotateSpeed, KeepersSettings::ZoomSpeed, KeepersSettings::InvertZoom, KeepersSettings::EdgeScroll });
		KeepersSettings::ResetKeys();
		KeepersSettings::ApplyKeys(this);
		break;
	default:
		ResetKeys({ KeepersSettings::BattleSpeed, KeepersSettings::Barks });
		break;
	}
	UKeepersSettingsSubsystem::ApplyNow(this);
	ShowTab(CurrentTab);
}

// ---------------------------------------------------------------------------------------------------------------------

void UKeepersSliderRow::Setup(const FText& Label, float InValue, TFunction<void(float)> InOnChanged, TFunction<FString(float)> InFormat)
{
	LabelText = Label;
	Value = InValue;
	OnChanged = MoveTemp(InOnChanged);
	Format = MoveTemp(InFormat);
}

TSharedRef<SWidget> UKeepersSliderRow::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		UBorder* Back = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Back->SetBrush(KeepersStyle::Panel(FLinearColor(1.f, 1.f, 1.f, 0.04f), 8.f, FLinearColor::Transparent));
		Back->SetPadding(FMargin(10.f, 6.f));
		UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		USizeBox* LabelSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		LabelSize->SetWidthOverride(220.f);
		LabelSize->AddChild(KeepersStyle::MakeText(WidgetTree, LabelText, KeepersStyle::Body(16)));
		Row->AddChildToHorizontalBox(LabelSize)->SetVerticalAlignment(VAlign_Center);
		Slider = WidgetTree->ConstructWidget<USlider>(USlider::StaticClass());
		Slider->SetValue(Value);
		Slider->SetStepSize(0.01f);
		Slider->SetSliderBarColor(FLinearColor(0.35f, 0.3f, 0.22f));
		Slider->SetSliderHandleColor(KeepersStyle::Gold);
		Slider->OnValueChanged.AddDynamic(this, &UKeepersSliderRow::HandleChanged);
		USizeBox* SliderSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		SliderSize->SetWidthOverride(240.f);
		SliderSize->AddChild(Slider);
		Row->AddChildToHorizontalBox(SliderSize)->SetVerticalAlignment(VAlign_Center);
		ValueText = KeepersStyle::MakeText(WidgetTree, FText::GetEmpty(), KeepersStyle::Body(15, true), KeepersStyle::Gold);
		Row->AddChildToHorizontalBox(ValueText)->SetPadding(FMargin(14.f, 0.f, 0.f, 0.f));
		Back->SetContent(Row);
		WidgetTree->RootWidget = Back;
		HandleChanged(Value);
	}
	return Super::RebuildWidget();
}

void UKeepersSliderRow::HandleChanged(float NewValue)
{
	const bool bChanged = !FMath::IsNearlyEqual(NewValue, Value);
	Value = NewValue;
	if (ValueText)
	{
		ValueText->SetText(FText::FromString(Format ? Format(Value) : FString::Printf(TEXT("%d%%"), FMath::RoundToInt(Value * 100.f))));
	}
	if (bChanged && OnChanged)
	{
		OnChanged(Value);
	}
}

// ---------------------------------------------------------------------------------------------------------------------

TSharedRef<SWidget> UKeepersFpsWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
		WidgetTree->RootWidget = Canvas;
		UBorder* Pill = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Pill->SetBrush(KeepersStyle::Panel(FLinearColor(0.f, 0.f, 0.f, 0.55f), 8.f, FLinearColor::Transparent));
		Pill->SetPadding(FMargin(8.f, 2.f));
		Text = KeepersStyle::MakeText(WidgetTree, FText::GetEmpty(), KeepersStyle::Body(12, true), KeepersStyle::Good);
		Pill->SetContent(Text);
		UCanvasPanelSlot* PillSlot = Canvas->AddChildToCanvas(Pill);
		PillSlot->SetAnchors(FAnchors(1.f, 0.f));
		PillSlot->SetAlignment(FVector2D(1.f, 0.f));
		PillSlot->SetOffsets(FMargin(-12.f, 8.f, 0.f, 0.f));
		PillSlot->SetAutoSize(true);
	}
	return Super::RebuildWidget();
}

void UKeepersFpsWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	const float Raw = FApp::GetDeltaTime() > 0.0 ? 1.f / static_cast<float>(FApp::GetDeltaTime()) : 0.f;
	Smoothed = FMath::Lerp(Smoothed, Raw, 0.1f);
	Timer -= InDeltaTime;
	if (Text && Timer <= 0.f)
	{
		Timer = 0.5f;
		Text->SetText(FText::FromString(FString::Printf(TEXT("%d FPS"), FMath::RoundToInt(Smoothed))));
	}
}

// ---------------------------------------------------------------------------------------------------------------------

TStatId UKeepersSettingsSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UKeepersSettingsSubsystem, STATGROUP_Tickables);
}

bool UKeepersSettingsSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UKeepersSettingsSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	KeepersSettings::ApplyVolume(&InWorld);
	KeepersSettings::SanitizeScreen();
	KeepersSettings::ApplyKeys(&InWorld);
	bApplied = false;
}

void UKeepersSettingsSubsystem::ApplyNow(const UObject* WorldContext)
{
	UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (UKeepersSettingsSubsystem* Self = World ? World->GetSubsystem<UKeepersSettingsSubsystem>() : nullptr)
	{
		Self->bApplied = false;
	}
}

void UKeepersSettingsSubsystem::ApplyToPawn(APawn* Pawn)
{
	using namespace KeepersSettingsUI;
	const UObject* Defaults = Pawn->GetClass()->GetDefaultObject();
	struct FLine { const TCHAR* Variable; const TCHAR* Key; bool bInvertable; };
	const FLine Lines[] =
	{
		{ TEXT("Location_Speed"), KeepersSettings::CameraSpeed, false },
		{ TEXT("Rotation_Speed"), KeepersSettings::RotateSpeed, false },
		{ TEXT("Zoom_Speed"), KeepersSettings::ZoomSpeed, true },
	};
	for (const FLine& Line : Lines)
	{
		double* Value = DoubleOf(Pawn, Line.Variable);
		const double* Base = DoubleOf(const_cast<UObject*>(Defaults), Line.Variable);
		if (Value && Base)
		{
			const double Sign = Line.bInvertable && KeepersSettings::Get(KeepersSettings::InvertZoom) > 0.5f ? -1.0 : 1.0;
			*Value = *Base * KeepersSettings::Get(Line.Key) * Sign;
		}
	}
}

void UKeepersSettingsSubsystem::EdgeScroll(APlayerController* Player, float DeltaTime)
{
	APawn* Pawn = Player->GetPawn();
	float X = 0.f;
	float Y = 0.f;
	int32 Width = 0;
	int32 Height = 0;
	if (!Pawn || UGameplayStatics::IsGamePaused(this) || !Player->GetMousePosition(X, Y))
	{
		return;
	}
	Player->GetViewportSize(Width, Height);
	if (Width <= 0 || Height <= 0)
	{
		return;
	}
	constexpr float Margin = 10.f;
	FVector2D Push(0.f, 0.f);
	Push.X = X < Margin ? -1.f : X > Width - Margin ? 1.f : 0.f;
	Push.Y = Y < Margin ? 1.f : Y > Height - Margin ? -1.f : 0.f;
	if (Push.IsZero())
	{
		return;
	}
	FProperty* LocationProperty = nullptr;
	void* LocationPtr = KeepersReflection::FindValuePtr(Pawn->GetClass(), Pawn, TEXT("Location_Desired"), &LocationProperty);
	const FStructProperty* Struct = CastField<FStructProperty>(LocationProperty);
	if (!LocationPtr || !Struct || Struct->Struct != TBaseStructure<FVector>::Get())
	{
		return;
	}
	// Along the camera's yaw.
	const float Yaw = Player->GetControlRotation().Yaw;
	FRotator CameraYaw(0.f, Yaw, 0.f);
	if (const FRotator* Desired = static_cast<const FRotator*>(KeepersReflection::FindValuePtr(Pawn->GetClass(), Pawn, TEXT("Rotation_Desired"))))
	{
		CameraYaw.Yaw = Desired->Yaw;
	}
	const FVector Forward = CameraYaw.Vector();
	const FVector Right = FRotationMatrix(CameraYaw).GetScaledAxis(EAxis::Y);
	FVector& Location = *static_cast<FVector*>(LocationPtr);
	Location += (Forward * Push.Y + Right * Push.X) * 1600.f * KeepersSettings::Get(KeepersSettings::CameraSpeed) * DeltaTime;
}

void UKeepersSettingsSubsystem::Tick(float DeltaTime)
{
	UWorld* World = GetWorld();
	APlayerController* Player = World ? World->GetFirstPlayerController() : nullptr;
	if (!Player || !World->HasBegunPlay())
	{
		return;
	}
	APawn* Pawn = Player->GetPawn();
	const bool bBattlePawn = Pawn && Pawn->GetClass()->GetName().StartsWith(TEXT("BP_Pawn"));
	if (!bApplied)
	{
		bApplied = true;
		if (bBattlePawn)
		{
			ApplyToPawn(Pawn);
			// Battle speed: the HUD's x1 / x3 buttons still change it during a battle.
			bool bBattle = false;
			for (TActorIterator<AActor> It(World); It && !bBattle; ++It)
			{
				bBattle = It->GetClass()->GetName() == TEXT("BP_CombatSystem_C");
			}
			if (bBattle)
			{
				Player->ConsoleCommand(FString::Printf(TEXT("SloMo %g"), KeepersSettings::Get(KeepersSettings::BattleSpeed)));
			}
		}
		else if (!Pawn)
		{
			// No pawn yet: try again next tick.
			bApplied = false;
		}
		const bool bShowFps = KeepersSettings::Get(KeepersSettings::ShowFps) > 0.5f;
		if (bShowFps && !Fps)
		{
			Fps = CreateWidget<UKeepersFpsWidget>(Player, UKeepersFpsWidget::StaticClass());
			Fps->AddToViewport(60);
			Fps->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
		else if (!bShowFps && Fps)
		{
			Fps->RemoveFromParent();
			Fps = nullptr;
		}
	}
	if (bBattlePawn && KeepersSettings::Get(KeepersSettings::EdgeScroll) > 0.5f)
	{
		EdgeScroll(Player, DeltaTime);
	}
	if (bBattlePawn && !UGameplayStatics::IsGamePaused(this))
	{
		// Rebound ability keys (the digits keep working through the Blueprint).
		for (int32 I = 0; I < 6; ++I)
		{
			const FKey Key = KeepersSettings::GetKey(*FString::Printf(TEXT("Ability%d"), I + 1));
			const bool bDigit = Key == EKeys::One || Key == EKeys::Two || Key == EKeys::Three || Key == EKeys::Four || Key == EKeys::Five || Key == EKeys::Six;
			if (!bDigit && Player->WasInputKeyJustPressed(Key))
			{
				UKeepersBattleHudSubsystem::TestAbility(this, I);
			}
		}
		if (Player->WasInputKeyJustPressed(KeepersSettings::GetKey(TEXT("BattleSpeed"))))
		{
			const float Speed = KeepersSettings::Get(KeepersSettings::BattleSpeed) >= 2.5f ? 1.f : 3.f;
			KeepersSettings::Set(KeepersSettings::BattleSpeed, Speed);
			bApplied = false;
		}
	}
}
