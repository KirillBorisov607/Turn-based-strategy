#include "KeepersCaptureLibrary.h"

#include "Components/SceneCaptureComponent2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"
#include "Engine/GameViewportClient.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SViewport.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/PlayerController.h"

bool UKeepersCaptureLibrary::CaptureToPng(const UObject* WorldContextObject, FVector Location, FRotator Rotation, float Fov, int32 Width, int32 Height, const FString& FilePath, float ExposureBias)
{
	UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	if (!World)
	{
		return false;
	}
	UTextureRenderTarget2D* Target = NewObject<UTextureRenderTarget2D>(GetTransientPackage());
	// Final LDR colour is already gamma-corrected: store it as is.
	Target->InitCustomFormat(Width, Height, PF_B8G8R8A8, true);
	Target->UpdateResourceImmediate(true);

	USceneCaptureComponent2D* Capture = NewObject<USceneCaptureComponent2D>(World->GetWorldSettings());
	Capture->bCaptureEveryFrame = false;
	Capture->bCaptureOnMovement = false;
	Capture->CaptureSource = SCS_FinalColorLDR;
	Capture->FOVAngle = Fov;
	Capture->TextureTarget = Target;
	// A fresh capture has no eye adaptation history: fixed exposure, tuned by the caller.
	Capture->ShowFlags.SetEyeAdaptation(false);
	Capture->PostProcessSettings.bOverride_AutoExposureBias = true;
	Capture->PostProcessSettings.AutoExposureBias = ExposureBias;
	Capture->SetWorldLocationAndRotation(Location, Rotation);
	Capture->RegisterComponentWithWorld(World);
	Capture->CaptureScene();

	TArray<FColor> Pixels;
	FTextureRenderTargetResource* Resource = Target->GameThread_GetRenderTargetResource();
	const bool bRead = Resource && Resource->ReadPixels(Pixels);
	Capture->DestroyComponent();
	if (!bRead || Pixels.Num() != Width * Height)
	{
		return false;
	}
	for (FColor& Pixel : Pixels)
	{
		Pixel.A = 255;
	}
	TArray64<uint8> Png;
	FImageUtils::PNGCompressImageArray(Width, Height, TArrayView64<const FColor>(Pixels.GetData(), Pixels.Num()), Png);
	return FFileHelper::SaveArrayToFile(Png, *FilePath);
}

void UKeepersCaptureLibrary::ViewFrom(const UObject* WorldContextObject, FVector Location, FRotator Rotation, float Fov)
{
	UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	APlayerController* Player = World ? World->GetFirstPlayerController() : nullptr;
	if (!Player)
	{
		return;
	}
	static const FName Tag = TEXT("KeepersShotCamera");
	ACameraActor* Camera = nullptr;
	for (TActorIterator<ACameraActor> It(World); It; ++It)
	{
		if (It->ActorHasTag(Tag))
		{
			Camera = *It;
		}
	}
	if (!Camera)
	{
		Camera = World->SpawnActor<ACameraActor>(Location, Rotation);
		Camera->Tags.Add(Tag);
	}
	Camera->SetActorLocationAndRotation(Location, Rotation);
	Camera->GetCameraComponent()->SetFieldOfView(Fov);
	Camera->GetCameraComponent()->SetConstraintAspectRatio(false);
	Player->SetViewTarget(Camera);
}

bool UKeepersCaptureLibrary::CaptureViewport(const UObject* WorldContextObject, const FString& FilePath)
{
	UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	UGameViewportClient* Client = World ? World->GetGameViewport() : nullptr;
	TSharedPtr<SViewport> Viewport = Client ? Client->GetGameViewportWidget() : nullptr;
	if (!Viewport.IsValid() || !FSlateApplication::IsInitialized())
	{
		return false;
	}
	TArray<FColor> Pixels;
	FIntVector Size;
	if (!FSlateApplication::Get().TakeScreenshot(Viewport.ToSharedRef(), Pixels, Size) || Pixels.Num() != Size.X * Size.Y)
	{
		return false;
	}
	for (FColor& Pixel : Pixels)
	{
		Pixel.A = 255;
	}
	TArray64<uint8> Png;
	FImageUtils::PNGCompressImageArray(Size.X, Size.Y, TArrayView64<const FColor>(Pixels.GetData(), Pixels.Num()), Png);
	return FFileHelper::SaveArrayToFile(Png, *FilePath);
}

FString UKeepersCaptureLibrary::DescribeSlate()
{
	if (!FSlateApplication::IsInitialized())
	{
		return TEXT("no slate");
	}
	FSlateApplication& Slate = FSlateApplication::Get();
	FString Result;
	TSharedPtr<SWindow> Modal = Slate.GetActiveModalWindow();
	const FString Line = TEXT("\n");
	const TSharedPtr<SWidget> Focus = Slate.GetKeyboardFocusedWidget();
	Result += TEXT("modal=") + (Modal.IsValid() ? Modal->GetTitle().ToString() : FString(TEXT("none"))) + Line;
	Result += TEXT("captor=") + FString(Slate.HasAnyMouseCaptor() ? TEXT("yes") : TEXT("none")) + Line;
	Result += TEXT("focus=") + (Focus.IsValid() ? Focus->GetTypeAsString() : FString(TEXT("none"))) + Line;
	Result += TEXT("pressed=") + FString::FromInt(Slate.GetPressedMouseButtons().Num()) + TEXT(" cursor=") + Slate.GetCursorPos().ToString() + Line;
	TArray<TSharedRef<SWindow>> Windows;
	Slate.GetAllVisibleWindowsOrdered(Windows);
	for (const TSharedRef<SWindow>& Window : Windows)
	{
		Result += TEXT("window '") + Window->GetTitle().ToString() + TEXT("' ") + Window->GetRectInScreen().ToString() + Line;
	}
	FWidgetPath Path = Slate.LocateWindowUnderMouse(Slate.GetCursorPos(), Slate.GetInteractiveTopLevelWindows());
	for (int32 Index = FMath::Max(Path.Widgets.Num() - 6, 0); Index < Path.Widgets.Num(); ++Index)
	{
		Result += TEXT("  under: ") + Path.Widgets[Index].Widget->GetTypeAsString() + Line;
	}
	return Result;
}

AActor* UKeepersCaptureLibrary::FindActorByName(const UObject* WorldContextObject, const FString& Part)
{
	UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	if (!World)
	{
		return nullptr;
	}
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		if (It->GetName().Contains(Part))
		{
			return *It;
		}
	}
	return nullptr;
}
