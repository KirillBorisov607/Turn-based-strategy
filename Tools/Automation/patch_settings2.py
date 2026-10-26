"""Settings, second pass: a screen resolution that never leaves the game unclickable, sliders for sound and camera,
and rebindable keys."""
import os
os.chdir(r'C:/ProjectsUE/Turn-based-strategy/The_battle_code/Plugins/KeepersGame/Source/KeepersGame')


def patch(p, pairs):
    s = open(p, 'rb').read().decode('utf-8-sig')
    crlf = '\r\n' in s
    s = s.replace('\r\n', '\n')
    for a, b in pairs:
        assert s.count(a) == 1, (p, a[:80])
        s = s.replace(a, b)
    open(p, 'w', encoding='utf-8', newline='\r\n' if crlf else '\n').write(s)


patch('Public/KeepersSettings.h', [
('''	/** Applies the saved volumes to the world's audio (call when a level starts or a volume changes). */
	KEEPERSGAME_API void ApplyVolume(const UObject* WorldContext);
}''', '''	/** Applies the saved volumes to the world's audio (call when a level starts or a volume changes). */
	KEEPERSGAME_API void ApplyVolume(const UObject* WorldContext);

	/**
	 * The picture size the player chose, applied so that the window always fits the screen and the mouse always
	 * hits what it points at: a window is never larger than the desktop; borderless covers the desktop and renders
	 * the chosen size inside it (screen percentage); fullscreen switches the display mode.
	 */
	KEEPERSGAME_API void ApplyScreen(FIntPoint Wanted, EWindowMode::Type Mode);
	/** The size the player chose (it differs from the window in borderless mode). */
	KEEPERSGAME_API FIntPoint GetWantedResolution();
	/** Fixes saved settings from older versions that put the window off the screen; once per game. */
	KEEPERSGAME_API void SanitizeScreen();

	/** A rebindable control. */
	struct FKeyBind
	{
		const TCHAR* Id;
		const TCHAR* Label;
		FKey Default;
		/** Legacy axis or action mapping it drives (empty: read by the game's own code). */
		const TCHAR* Mapping;
		float Scale;
	};
	KEEPERSGAME_API const TArray<FKeyBind>& KeyBinds();
	KEEPERSGAME_API FKey GetKey(const TCHAR* Id);
	KEEPERSGAME_API void SetKey(const TCHAR* Id, const FKey& Key);
	KEEPERSGAME_API void ResetKeys();
	/** Writes the bound keys into the input mappings (at start and after a change). */
	KEEPERSGAME_API void ApplyKeys(const UObject* WorldContext);
}'''),
('''#include "CoreMinimal.h"''', '''#include "CoreMinimal.h"
#include "GenericPlatform/GenericWindow.h"
#include "InputCoreTypes.h"'''),
])

patch('Private/KeepersSettings.cpp', [
('''#include "Kismet/GameplayStatics.h"''', '''#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/GameUserSettings.h"
#include "GameFramework/InputSettings.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerInput.h"
#include "Kismet/GameplayStatics.h"'''),
('''	void Set(const TCHAR* Key, float Value)
	{
		GConfig->SetFloat(Section, Key, Value, GGameUserSettingsIni);
		GConfig->Flush(false, GGameUserSettingsIni);
	}''', '''	void Set(const TCHAR* Key, float Value)
	{
		GConfig->SetFloat(Section, Key, Value, GGameUserSettingsIni);
		GConfig->Flush(false, GGameUserSettingsIni);
	}

	FIntPoint GetWantedResolution()
	{
		UGameUserSettings* Settings = GEngine ? GEngine->GetGameUserSettings() : nullptr;
		FIntPoint Wanted = Settings ? Settings->GetScreenResolution() : FIntPoint(1920, 1080);
		int32 X = 0;
		int32 Y = 0;
		if (GConfig->GetInt(Section, TEXT("WantedResX"), X, GGameUserSettingsIni) && GConfig->GetInt(Section, TEXT("WantedResY"), Y, GGameUserSettingsIni) && X > 0 && Y > 0)
		{
			Wanted = FIntPoint(X, Y);
		}
		return Wanted;
	}

	void ApplyScreen(FIntPoint Wanted, EWindowMode::Type Mode)
	{
		UGameUserSettings* Settings = GEngine ? GEngine->GetGameUserSettings() : nullptr;
		if (!Settings)
		{
			return;
		}
		const FIntPoint Desktop = Settings->GetDesktopResolution();
		GConfig->SetInt(Section, TEXT("WantedResX"), Wanted.X, GGameUserSettingsIni);
		GConfig->SetInt(Section, TEXT("WantedResY"), Wanted.Y, GGameUserSettingsIni);
		FIntPoint Window = Wanted;
		float Percent = 100.f;
		if (Mode == EWindowMode::WindowedFullscreen && Desktop.X > 0)
		{
			// The window covers the desktop; the chosen size is how sharp the picture inside it is.
			Window = Desktop;
			Percent = FMath::Clamp(100.f * Wanted.Y / Desktop.Y, 50.f, 100.f);
		}
		else if (Mode == EWindowMode::Windowed && Desktop.X > 0)
		{
			// A window larger than the screen hangs off it and the clicks land beside the buttons.
			const float Fit = FMath::Min(1.f, FMath::Min((Desktop.X - 40.f) / Wanted.X, (Desktop.Y - 90.f) / Wanted.Y));
			Window = FIntPoint(FMath::RoundToInt(Wanted.X * Fit), FMath::RoundToInt(Wanted.Y * Fit));
		}
		Settings->SetFullscreenMode(Mode);
		Settings->SetScreenResolution(Window);
		Settings->SetResolutionScaleValueEx(Percent);
		Settings->ApplyResolutionSettings(false);
		Settings->ApplySettings(false);
		Settings->ConfirmVideoMode();
		Settings->SaveSettings();
		UE_LOG(LogTemp, Log, TEXT("Keepers screen: mode %d, window %dx%d, picture %dx%d (%.0f%%)"), static_cast<int32>(Mode), Window.X, Window.Y, Wanted.X, Wanted.Y, Percent);
	}

	void SanitizeScreen()
	{
		static bool bDone = false;
		UGameUserSettings* Settings = GEngine ? GEngine->GetGameUserSettings() : nullptr;
		if (bDone || !Settings || GIsEditor)
		{
			return;
		}
		bDone = true;
		const FIntPoint Desktop = Settings->GetDesktopResolution();
		const FIntPoint Screen = Settings->GetScreenResolution();
		const EWindowMode::Type Mode = Settings->GetFullscreenMode();
		const bool bOffScreen = Mode == EWindowMode::Windowed && (Screen.X > Desktop.X || Screen.Y > Desktop.Y);
		const bool bBorderlessMismatch = Mode == EWindowMode::WindowedFullscreen && Screen != Desktop;
		if (Desktop.X > 0 && (bOffScreen || bBorderlessMismatch))
		{
			ApplyScreen(GetWantedResolution(), Mode);
		}
	}

	const TArray<FKeyBind>& KeyBinds()
	{
		static const TArray<FKeyBind> Binds =
		{
			{ TEXT("CamForward"), TEXT("Камера вперёд"), EKeys::W, TEXT("InputAxis_Forward"), 1.f },
			{ TEXT("CamBack"), TEXT("Камера назад"), EKeys::S, TEXT("InputAxis_Forward"), -1.f },
			{ TEXT("CamLeft"), TEXT("Камера влево"), EKeys::A, TEXT("InputAxis_Right"), -1.f },
			{ TEXT("CamRight"), TEXT("Камера вправо"), EKeys::D, TEXT("InputAxis_Right"), 1.f },
			{ TEXT("RotLeft"), TEXT("Поворот влево"), EKeys::Q, TEXT("InputAction_RotationLeft"), 0.f },
			{ TEXT("RotRight"), TEXT("Поворот вправо"), EKeys::E, TEXT("InputAction_RotationRight"), 0.f },
			{ TEXT("Pause"), TEXT("Пауза"), EKeys::P, TEXT(""), 0.f },
			{ TEXT("Ability1"), TEXT("Способность 1"), EKeys::One, TEXT(""), 0.f },
			{ TEXT("Ability2"), TEXT("Способность 2"), EKeys::Two, TEXT(""), 0.f },
			{ TEXT("Ability3"), TEXT("Способность 3"), EKeys::Three, TEXT(""), 0.f },
			{ TEXT("Ability4"), TEXT("Способность 4"), EKeys::Four, TEXT(""), 0.f },
			{ TEXT("Ability5"), TEXT("Способность 5"), EKeys::Five, TEXT(""), 0.f },
			{ TEXT("Ability6"), TEXT("Способность 6"), EKeys::Six, TEXT(""), 0.f },
			{ TEXT("BattleSpeed"), TEXT("Скорость боя x1 / x3"), EKeys::Tab, TEXT(""), 0.f },
		};
		return Binds;
	}

	static const FKeyBind* FindBind(const TCHAR* Id)
	{
		return KeyBinds().FindByPredicate([Id](const FKeyBind& Bind) { return FCString::Strcmp(Bind.Id, Id) == 0; });
	}

	FKey GetKey(const TCHAR* Id)
	{
		const FKeyBind* Bind = FindBind(Id);
		FString Name;
		if (GConfig->GetString(Section, *FString::Printf(TEXT("Key_%s"), Id), Name, GGameUserSettingsIni))
		{
			const FKey Key(*Name);
			if (Key.IsValid())
			{
				return Key;
			}
		}
		return Bind ? Bind->Default : EKeys::Invalid;
	}

	void SetKey(const TCHAR* Id, const FKey& Key)
	{
		GConfig->SetString(Section, *FString::Printf(TEXT("Key_%s"), Id), *Key.GetFName().ToString(), GGameUserSettingsIni);
		GConfig->Flush(false, GGameUserSettingsIni);
	}

	void ResetKeys()
	{
		for (const FKeyBind& Bind : KeyBinds())
		{
			GConfig->RemoveKey(Section, *FString::Printf(TEXT("Key_%s"), Bind.Id), GGameUserSettingsIni);
		}
		GConfig->Flush(false, GGameUserSettingsIni);
	}

	void ApplyKeys(const UObject* WorldContext)
	{
		UInputSettings* Input = UInputSettings::GetInputSettings();
		if (!Input)
		{
			return;
		}
		// Each bound control replaces its default key in the camera's mappings; the arrows stay as a spare.
		for (const FKeyBind& Bind : KeyBinds())
		{
			if (!*Bind.Mapping)
			{
				continue;
			}
			const FName Mapping(Bind.Mapping);
			const FKey Key = GetKey(Bind.Id);
			if (Bind.Scale != 0.f)
			{
				TArray<FInputAxisKeyMapping> Existing;
				Input->GetAxisMappingByName(Mapping, Existing);
				for (const FInputAxisKeyMapping& Old : Existing)
				{
					const bool bArrow = Old.Key == EKeys::Up || Old.Key == EKeys::Down || Old.Key == EKeys::Left || Old.Key == EKeys::Right;
					if (Old.Scale == Bind.Scale && !bArrow)
					{
						Input->RemoveAxisMapping(Old, false);
					}
				}
				Input->AddAxisMapping(FInputAxisKeyMapping(Mapping, Key, Bind.Scale), false);
			}
			else
			{
				TArray<FInputActionKeyMapping> Existing;
				Input->GetActionMappingByName(Mapping, Existing);
				for (const FInputActionKeyMapping& Old : Existing)
				{
					Input->RemoveActionMapping(Old, false);
				}
				Input->AddActionMapping(FInputActionKeyMapping(Mapping, Key), false);
			}
		}
		Input->ForceRebuildKeymaps();
		const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
		if (APlayerController* Player = World ? World->GetFirstPlayerController() : nullptr)
		{
			if (Player->PlayerInput)
			{
				Player->PlayerInput->ForceRebuildingKeyMaps(true);
			}
		}
	}'''),
])

# The pause key and the ability keys follow the bindings.
patch('Private/KeepersPauseMenu.cpp', [
('''	if (Key == EKeys::Escape || Key == EKeys::P || Key == EKeys::F10)''', '''	if (Key == EKeys::Escape || Key == KeepersSettings::GetKey(TEXT("Pause")) || Key == EKeys::F10)'''),
('''	if (Player->WasInputKeyJustPressed(EKeys::Escape) || Player->WasInputKeyJustPressed(EKeys::P) || Player->WasInputKeyJustPressed(EKeys::F10))''',
 '''	if (Player->WasInputKeyJustPressed(EKeys::Escape) || Player->WasInputKeyJustPressed(KeepersSettings::GetKey(TEXT("Pause"))) || Player->WasInputKeyJustPressed(EKeys::F10))'''),
('''		Button->Setup(FText::FromString(TEXT("Меню · P")),''', '''		Button->Setup(FText::FromString(FString::Printf(TEXT("Меню · %s"), *KeepersSettings::GetKey(TEXT("Pause")).GetDisplayName().ToString())),'''),
])

# Settings screen: sliders, key rows, the safe resolution.
patch('Public/KeepersSettingsUI.h', [
('''class UHorizontalBox;
class UTextBlock;''', '''class UHorizontalBox;
class USlider;
class UTextBlock;'''),
('''private:
	void AddRow(const TCHAR* Label, TArray<FText> Options, int32 Current, TFunction<void(int32)> OnChanged);
	void ResetTab();

	int32 CurrentTab = 0;''', '''	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

private:
	void AddRow(const TCHAR* Label, TArray<FText> Options, int32 Current, TFunction<void(int32)> OnChanged);
	/** A slider from 0 to 1 with its value in percent. */
	void AddSlider(const TCHAR* Label, float Value, TFunction<void(float)> OnChanged, TFunction<FString(float)> Format = nullptr);
	/** A control and its key; a click waits for the new key. */
	void AddKeyRow(const TCHAR* Id, const TCHAR* Label);
	void ResetTab();

	int32 CurrentTab = 0;
	/** The control waiting for a key, or empty. */
	FString Rebinding;'''),
('''/** A small FPS counter in the top right corner. */''', '''/** A labelled slider of the settings. */
UCLASS()
class KEEPERSGAME_API UKeepersSliderRow : public UUserWidget
{
	GENERATED_BODY()

public:
	void Setup(const FText& Label, float InValue, TFunction<void(float)> InOnChanged, TFunction<FString(float)> InFormat);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;

private:
	UFUNCTION()
	void HandleChanged(float NewValue);

	FText LabelText;
	float Value = 0.f;
	TFunction<void(float)> OnChanged;
	TFunction<FString(float)> Format;

	UPROPERTY()
	TObjectPtr<USlider> Slider;

	UPROPERTY()
	TObjectPtr<UTextBlock> ValueText;
};

/** A small FPS counter in the top right corner. */'''),
])

patch('Private/KeepersSettingsUI.cpp', [
('''#include "Components/ScrollBox.h"''', '''#include "Components/ScrollBox.h"
#include "Components/Slider.h"'''),
('''void UKeepersSettingsPanel::AddRow(const TCHAR* Label, TArray<FText> Options, int32 Current, TFunction<void(int32)> OnChanged)
{''', '''void UKeepersSettingsPanel::AddSlider(const TCHAR* Label, float Value, TFunction<void(float)> OnChanged, TFunction<FString(float)> Format)
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
{'''),
# Resolution: the safe way.
('''		TArray<FIntPoint> Resolutions;
		UKismetSystemLibrary::GetSupportedFullscreenResolutions(Resolutions);
		if (Resolutions.Num() == 0)
		{
			Resolutions = { FIntPoint(1280, 720), FIntPoint(1600, 900), FIntPoint(1920, 1080) };
		}
		TArray<FText> ResolutionNames;
		int32 CurrentResolution = Resolutions.Num() - 1;
		for (int32 I = 0; I < Resolutions.Num(); ++I)
		{
			ResolutionNames.Add(T(FString::Printf(TEXT("%d × %d"), Resolutions[I].X, Resolutions[I].Y)));
			if (Resolutions[I] == Settings->GetScreenResolution())
			{
				CurrentResolution = I;
			}
		}
		AddRow(TEXT("Разрешение"), MoveTemp(ResolutionNames), CurrentResolution, [Settings, Resolutions](int32 Index)
		{
			Settings->SetScreenResolution(Resolutions[Index]);
			Settings->ApplySettings(false);
		});
		const EWindowMode::Type Mode = Settings->GetFullscreenMode();
		AddRow(TEXT("Режим экрана"), Texts({ TEXT("Полный экран"), TEXT("Без рамки"), TEXT("Окно") }),
			Mode == EWindowMode::Fullscreen ? 0 : Mode == EWindowMode::WindowedFullscreen ? 1 : 2, [Settings](int32 Index)
		{
			Settings->SetFullscreenMode(Index == 0 ? EWindowMode::Fullscreen : Index == 1 ? EWindowMode::WindowedFullscreen : EWindowMode::Windowed);
			Settings->ApplySettings(false);
		});''', '''		// Sizes the display supports, the desktop's own among them, without duplicates, small to large.
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
		});'''),
# Volumes as sliders.
('''		for (const FVolumeLine& Line : Lines)
		{
			const TCHAR* Key = Line.Key;
			AddRow(Line.Label, Percents(0, 100, 10), FMath::RoundToInt(FMath::Clamp(KeepersSettings::Get(Key), 0.f, 1.f) * 10.f), [WeakThis, Key](int32 Index)
			{
				KeepersSettings::Set(Key, Index / 10.f);
				KeepersSettings::ApplyVolume(WeakThis.Get());
			});
		}''', '''		for (const FVolumeLine& Line : Lines)
		{
			const TCHAR* Key = Line.Key;
			AddSlider(Line.Label, FMath::Clamp(KeepersSettings::Get(Key), 0.f, 1.f), [WeakThis, Key](float Value)
			{
				KeepersSettings::Set(Key, Value);
				KeepersSettings::ApplyVolume(WeakThis.Get());
			});
		}'''),
# Camera speeds as sliders (0.5 .. 2), key rows.
('''		for (const FSpeedLine& Line : Lines)
		{
			const TCHAR* Key = Line.Key;
			AddRow(Line.Label, Speeds, SpeedIndex(KeepersSettings::Get(Key)), [WeakThis, Key](int32 Index)
			{
				KeepersSettings::Set(Key, SpeedSteps[Index]);
				UKeepersSettingsSubsystem::ApplyNow(WeakThis.Get());
			});
		}''', '''		for (const FSpeedLine& Line : Lines)
		{
			const TCHAR* Key = Line.Key;
			// 0..1 on the slider is ×0.5..×2.
			AddSlider(Line.Label, FMath::Clamp((KeepersSettings::Get(Key) - 0.5f) / 1.5f, 0.f, 1.f), [WeakThis, Key](float Value)
			{
				KeepersSettings::Set(Key, 0.5f + Value * 1.5f);
				UKeepersSettingsSubsystem::ApplyNow(WeakThis.Get());
			}, [](float Value) { return FString::Printf(TEXT("×%.2g"), 0.5f + Value * 1.5f); });
		}'''),
('''		USizeBox* KeysSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		KeysSize->SetWidthOverride(560.f);
		KeysSize->AddChild(KeepersStyle::MakeText(WidgetTree,
			T(TEXT("WASD или стрелки — камера · Q / E — поворот · колёсико — приближение · 1–6 — способности · P или Esc — пауза")),
			KeepersStyle::Body(13), KeepersStyle::TextDim, true));
		Rows->AddChildToVerticalBox(KeysSize)->SetPadding(FMargin(4.f, 8.f, 0.f, 0.f));''', '''		Rows->AddChildToVerticalBox(KeepersStyle::MakeText(WidgetTree, T(TEXT("Клавиши")), KeepersStyle::Heading(20), KeepersStyle::Gold))->SetPadding(FMargin(0.f, 12.f, 0.f, 4.f));
		for (const KeepersSettings::FKeyBind& Bind : KeepersSettings::KeyBinds())
		{
			AddKeyRow(Bind.Id, Bind.Label);
		}
		USizeBox* KeysSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		KeysSize->SetWidthOverride(560.f);
		KeysSize->AddChild(KeepersStyle::MakeText(WidgetTree,
			T(TEXT("Щёлкните по клавише и нажмите новую (Esc — отмена). Стрелки всегда двигают камеру, Esc всегда открывает паузу.")),
			KeepersStyle::Body(13), KeepersStyle::TextDim, true));
		Rows->AddChildToVerticalBox(KeysSize)->SetPadding(FMargin(4.f, 8.f, 0.f, 0.f));'''),
('''		ResetKeys({ KeepersSettings::CameraSpeed, KeepersSettings::RotateSpeed, KeepersSettings::ZoomSpeed, KeepersSettings::InvertZoom, KeepersSettings::EdgeScroll });''',
 '''		ResetKeys({ KeepersSettings::CameraSpeed, KeepersSettings::RotateSpeed, KeepersSettings::ZoomSpeed, KeepersSettings::InvertZoom, KeepersSettings::EdgeScroll });
		KeepersSettings::ResetKeys();
		KeepersSettings::ApplyKeys(this);'''),
('''void UKeepersSettingsSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	KeepersSettings::ApplyVolume(&InWorld);''', '''void UKeepersSettingsSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	KeepersSettings::ApplyVolume(&InWorld);
	KeepersSettings::SanitizeScreen();
	KeepersSettings::ApplyKeys(&InWorld);'''),
# Ability keys and the battle speed key.
('''	if (bBattlePawn && KeepersSettings::Get(KeepersSettings::EdgeScroll) > 0.5f)
	{
		EdgeScroll(Player, DeltaTime);
	}''', '''	if (bBattlePawn && KeepersSettings::Get(KeepersSettings::EdgeScroll) > 0.5f)
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
	}'''),
('''#include "KeepersMainMenu.h"''', '''#include "KeepersBattleHud.h"
#include "KeepersMainMenu.h"'''),
('''// ---------------------------------------------------------------------------------------------------------------------

TSharedRef<SWidget> UKeepersFpsWidget::RebuildWidget()''', '''// ---------------------------------------------------------------------------------------------------------------------

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

TSharedRef<SWidget> UKeepersFpsWidget::RebuildWidget()'''),
])
print('ok')
