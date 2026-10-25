#include "KeepersInputSimulator.h"

#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Framework/Application/SlateApplication.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"
#include "RenderingThread.h"
#include "Slate/WidgetRenderer.h"
#include "TextureResource.h"
#include "GameFramework/PlayerController.h"
#include "Widgets/SViewport.h"

DEFINE_LOG_CATEGORY_STATIC(LogKeepersInputSim, Log, All);

namespace KeepersInputSim
{
	static FVector2D LastPosition = FVector2D::ZeroVector;

	static TSharedPtr<SViewport> GetViewportWidget(UObject* WorldContext)
	{
		UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
		UGameViewportClient* Client = World ? World->GetGameViewport() : nullptr;
		return Client ? Client->GetGameViewportWidget() : nullptr;
	}

	/** Viewport fraction (0..1) -> absolute desktop position. */
	static bool FractionToScreen(UObject* WorldContext, const FVector2D& Fraction, FVector2D& OutScreen)
	{
		TSharedPtr<SViewport> Widget = GetViewportWidget(WorldContext);
		if (!Widget.IsValid())
		{
			UE_LOG(LogKeepersInputSim, Warning, TEXT("No game viewport"));
			return false;
		}
		const FGeometry& Geometry = Widget->GetTickSpaceGeometry();
		OutScreen = FVector2D(Geometry.LocalToAbsolute(FVector2f(Geometry.GetLocalSize()) * FVector2f(Fraction)));
		return true;
	}

	static void Move(const FVector2D& Screen)
	{
		FPointerEvent Event(0, Screen, LastPosition, TSet<FKey>(), EKeys::Invalid, 0.f, FModifierKeysState());
		FSlateApplication::Get().ProcessMouseMoveEvent(Event);
		LastPosition = Screen;
	}

	static void Click(const FVector2D& Screen, bool bRight)
	{
		const FKey Button = bRight ? EKeys::RightMouseButton : EKeys::LeftMouseButton;
		Move(Screen);
		TSet<FKey> Pressed;
		Pressed.Add(Button);
		FPointerEvent Down(0, Screen, Screen, Pressed, Button, 0.f, FModifierKeysState());
		FSlateApplication::Get().ProcessMouseButtonDownEvent(nullptr, Down);
		FPointerEvent Up(0, Screen, Screen, TSet<FKey>(), Button, 0.f, FModifierKeysState());
		FSlateApplication::Get().ProcessMouseButtonUpEvent(Up);
	}
}

bool UKeepersInputSimulator::ClickViewport(UObject* WorldContext, FVector2D Fraction, bool bRightButton)
{
	FVector2D Screen;
	if (!KeepersInputSim::FractionToScreen(WorldContext, Fraction, Screen))
	{
		return false;
	}
	KeepersInputSim::Click(Screen, bRightButton);
	return true;
}

bool UKeepersInputSimulator::ClickWorldLocation(UObject* WorldContext, FVector Location, bool bRightButton)
{
	UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	APlayerController* Player = World ? World->GetFirstPlayerController() : nullptr;
	FVector2D Pixel;
	if (!Player || !Player->ProjectWorldLocationToScreen(Location, Pixel))
	{
		return false;
	}
	int32 Width = 0;
	int32 Height = 0;
	Player->GetViewportSize(Width, Height);
	if (Width <= 0 || Height <= 0)
	{
		return false;
	}
	return ClickViewport(WorldContext, FVector2D(Pixel.X / Width, Pixel.Y / Height), bRightButton);
}

bool UKeepersInputSimulator::SetMouseButton(UObject* WorldContext, FVector2D Fraction, bool bDown)
{
	FVector2D Screen;
	if (!KeepersInputSim::FractionToScreen(WorldContext, Fraction, Screen))
	{
		return false;
	}
	KeepersInputSim::Move(Screen);
	TSet<FKey> Pressed;
	if (bDown)
	{
		Pressed.Add(EKeys::LeftMouseButton);
		FPointerEvent Down(0, Screen, Screen, Pressed, EKeys::LeftMouseButton, 0.f, FModifierKeysState());
		FSlateApplication::Get().ProcessMouseButtonDownEvent(nullptr, Down);
	}
	else
	{
		FPointerEvent Up(0, Screen, Screen, Pressed, EKeys::LeftMouseButton, 0.f, FModifierKeysState());
		FSlateApplication::Get().ProcessMouseButtonUpEvent(Up);
	}
	return true;
}

bool UKeepersInputSimulator::MoveMouse(UObject* WorldContext, FVector2D Fraction)
{
	FVector2D Screen;
	if (!KeepersInputSim::FractionToScreen(WorldContext, Fraction, Screen))
	{
		return false;
	}
	KeepersInputSim::Move(Screen);
	return true;
}

bool UKeepersInputSimulator::ScrollWheel(UObject* WorldContext, float Delta)
{
	FVector2D Screen;
	if (!KeepersInputSim::FractionToScreen(WorldContext, FVector2D(0.5f, 0.5f), Screen))
	{
		return false;
	}
	KeepersInputSim::Move(Screen);
	FPointerEvent Wheel(0, Screen, Screen, TSet<FKey>(), EKeys::Invalid, Delta, FModifierKeysState());
	FSlateApplication::Get().ProcessMouseWheelOrGestureEvent(Wheel, nullptr);
	return true;
}

bool UKeepersInputSimulator::SaveViewportUI(UObject* WorldContext, const FString& Path, int32 Width, int32 Height)
{
	TSharedPtr<SViewport> Viewport = KeepersInputSim::GetViewportWidget(WorldContext);
	TSharedPtr<SWidget> Layer = Viewport.IsValid() ? Viewport->GetContent() : nullptr;
	if (!Layer.IsValid())
	{
		return false;
	}
	UTextureRenderTarget2D* Target = NewObject<UTextureRenderTarget2D>();
	Target->ClearColor = FLinearColor::Transparent;
	Target->InitCustomFormat(Width, Height, PF_B8G8R8A8, false);
	Target->UpdateResourceImmediate(true);

	FWidgetRenderer Renderer(false, false);
	Renderer.DrawWidget(Target, Layer.ToSharedRef(), FVector2D(Width, Height), 0.f);
	FlushRenderingCommands();

	TArray<FColor> Pixels;
	FTextureRenderTargetResource* Resource = Target->GameThread_GetRenderTargetResource();
	if (!Resource || !Resource->ReadPixels(Pixels) || Pixels.Num() != Width * Height)
	{
		return false;
	}
	TArray64<uint8> Png;
	FImageUtils::PNGCompressImageArray(Width, Height, TArrayView64<const FColor>(Pixels.GetData(), Pixels.Num()), Png);
	return FFileHelper::SaveArrayToFile(Png, *Path);
}

bool UKeepersInputSimulator::SetKey(FName KeyName, bool bDown)
{
	const FKey Key(KeyName);
	if (!Key.IsValid())
	{
		return false;
	}
	const FKeyEvent Event(Key, FModifierKeysState(), 0, false, 0, 0);
	return bDown ? FSlateApplication::Get().ProcessKeyDownEvent(Event) : FSlateApplication::Get().ProcessKeyUpEvent(Event);
}
