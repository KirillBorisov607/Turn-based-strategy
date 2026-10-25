#include "KeepersDirector.h"

#include "Async/Async.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/PlayerController.h"
#include "HAL/PlatformFileManager.h"
#include "ImageUtils.h"
#include "Kismet/GameplayStatics.h"
#include "KeepersCampaign.h"
#include "KeepersCampaignUI.h"
#include "KeepersLoadingScreen.h"
#include "KeepersMainMenu.h"
#include "KeepersReflection.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Widgets/SViewport.h"

DEFINE_LOG_CATEGORY_STATIC(LogKeepersDirector, Log, All);

namespace KeepersDirector
{
	static std::atomic<int32> PendingWrites{ 0 };

	static AActor* FindByClassName(UWorld* World, const TCHAR* ClassName)
	{
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			if (It->GetClass()->GetName() == ClassName)
			{
				return *It;
			}
		}
		return nullptr;
	}

	static TArray<AActor*> UnitsInCombat(UObject* CombatSystem)
	{
		TArray<AActor*> Units;
		FProperty* Property = nullptr;
		void* Value = CombatSystem ? KeepersReflection::FindValuePtr(CombatSystem->GetClass(), CombatSystem, TEXT("UnitsInCombat"), &Property) : nullptr;
		FArrayProperty* Array = CastField<FArrayProperty>(Property);
		FObjectPropertyBase* Inner = Array ? CastField<FObjectPropertyBase>(Array->Inner) : nullptr;
		if (!Inner)
		{
			return Units;
		}
		FScriptArrayHelper Helper(Array, Value);
		for (int32 I = 0; I < Helper.Num(); ++I)
		{
			if (AActor* Unit = Cast<AActor>(Inner->GetObjectPropertyValue(Helper.GetRawPtr(I))))
			{
				Units.Add(Unit);
			}
		}
		return Units;
	}
}

void UKeepersDirectorSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	const TCHAR* CommandLine = FCommandLine::Get();
	FString StepsText;
	const bool bRecord = FParse::Value(CommandLine, TEXT("-KeepersRecord="), Folder);
	const bool bSteps = FParse::Value(CommandLine, TEXT("-KeepersSteps="), StepsText, false);
	if (!bRecord && !bSteps)
	{
		return;
	}
	bEnabled = true;
	FParse::Value(CommandLine, TEXT("-KeepersFps="), Fps);
	FParse::Value(CommandLine, TEXT("-KeepersDuration="), Duration);
	FParse::Value(CommandLine, TEXT("-KeepersWarmup="), Warmup);
	Fps = FMath::Clamp(Fps, 5.f, 60.f);
	StepsText.ReplaceInline(TEXT("\""), TEXT(""));
	TArray<FString> Parts;
	StepsText.ParseIntoArray(Parts, TEXT(";"));
	for (const FString& Part : Parts)
	{
		FString Time;
		FString Rest;
		if (!Part.Split(TEXT(":"), &Time, &Rest))
		{
			continue;
		}
		FStep& Step = Steps.AddDefaulted_GetRef();
		Step.Time = FCString::Atof(*Time);
		if (!Rest.Split(TEXT("="), &Step.Action, &Step.Argument))
		{
			Step.Action = Rest;
		}
	}
	Steps.StableSort([](const FStep& A, const FStep& B) { return A.Time < B.Time; });
	if (bRecord)
	{
		// Every frame moves the game by exactly 1/Fps: smooth footage however slow the capture is.
		FApp::SetUseFixedTimeStep(true);
		FApp::SetFixedDeltaTime(1.0 / Fps);
		IFileManager::Get().MakeDirectory(*Folder, true);
	}
	UE_LOG(LogKeepersDirector, Log, TEXT("Director: %d steps, recording %s, %.0f s at %.0f fps"), Steps.Num(), bRecord ? *Folder : TEXT("off"), Duration, Fps);
}

TStatId UKeepersDirectorSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UKeepersDirectorSubsystem, STATGROUP_Tickables);
}

void UKeepersDirectorSubsystem::Tick(float DeltaTime)
{
	UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
	if (!World)
	{
		return;
	}
	Clock += DeltaTime;
	while (NextStep < Steps.Num() && Steps[NextStep].Time <= Clock)
	{
		RunStep(Steps[NextStep++]);
	}
	if (bAutoplay)
	{
		UpdateAutoplay(World);
	}
	UpdateCamera(World, DeltaTime);
	if (!Folder.IsEmpty() && Clock >= Warmup)
	{
		CaptureFrame(World);
	}
	if (Clock >= Warmup + Duration)
	{
		bEnabled = false;
		while (KeepersDirector::PendingWrites.load() > 0)
		{
			FPlatformProcess::Sleep(0.01f);
		}
		UE_LOG(LogKeepersDirector, Log, TEXT("Director: done, %d frames"), Frame);
		FPlatformMisc::RequestExit(false);
	}
}

void UKeepersDirectorSubsystem::RunStep(const FStep& Step)
{
	UWorld* World = GetGameInstance()->GetWorld();
	UE_LOG(LogKeepersDirector, Log, TEXT("Director %.1f s: %s %s"), Clock, *Step.Action, *Step.Argument);
	TArray<FString> Args;
	Step.Argument.ParseIntoArray(Args, TEXT(","));
	AKeepersMenuScene* Menu = nullptr;
	for (TActorIterator<AKeepersMenuScene> It(World); It; ++It)
	{
		Menu = *It;
	}
	UKeepersCampaignSubsystem* Campaign = UKeepersCampaignSubsystem::Get(World);

	if (Step.Action == TEXT("autoplay"))
	{
		bAutoplay = true;
	}
	else if (Step.Action == TEXT("speed") && Args.Num() >= 1)
	{
		UGameplayStatics::SetGlobalTimeDilation(World, FMath::Clamp(FCString::Atof(*Args[0]), 0.1f, 5.f));
	}
	else if (Step.Action == TEXT("orbit"))
	{
		CameraMode = ECameraMode::Orbit;
		bHasFocus = false;
	}
	else if (Step.Action == TEXT("follow"))
	{
		CameraMode = ECameraMode::Follow;
		bHasFocus = false;
	}
	else if (Step.Action == TEXT("game"))
	{
		CameraMode = ECameraMode::Game;
		if (APlayerController* Player = World->GetFirstPlayerController())
		{
			Player->SetViewTarget(Player->GetPawn());
		}
	}
	else if (Step.Action == TEXT("view") && Args.Num() >= 6)
	{
		CameraMode = ECameraMode::Still;
		if (ACameraActor* Shot = GetCamera(World))
		{
			Shot->SetActorLocationAndRotation(FVector(FCString::Atof(*Args[0]), FCString::Atof(*Args[1]), FCString::Atof(*Args[2])),
				FRotator(FCString::Atof(*Args[3]), FCString::Atof(*Args[4]), 0.f));
			Shot->GetCameraComponent()->SetFieldOfView(FCString::Atof(*Args[5]));
		}
	}
	else if (Step.Action == TEXT("squad") && Menu)
	{
		Menu->OpenSquadScreen(false);
	}
	else if (Step.Action == TEXT("squadhero") && Menu && Args.Num() >= 1)
	{
		Menu->TestSquad(FName(*Args[0]), Args.Num() > 1 ? FCString::Atoi(*Args[1]) : -1);
	}
	else if (Step.Action == TEXT("look") && Menu && Args.Num() >= 1)
	{
		Menu->TestLook(FCString::Atoi(*Args[0]), false);
	}
	else if (Step.Action == TEXT("newcampaign") && Campaign)
	{
		Campaign->StartNewCampaign();
	}
	else if (Step.Action == TEXT("node") && Campaign && Args.Num() >= 1)
	{
		Campaign->EnterNode(FCString::Atoi(*Args[0]));
	}
	else if (Step.Action == TEXT("camp") && Args.Num() >= 1)
	{
		UKeepersCampaignWorldSubsystem::TestCamp(World, Args[0], Args.Num() > 1 ? FCString::Atoi(*Args[1]) : 0);
	}
	else if (Step.Action == TEXT("open") && !Step.Argument.IsEmpty())
	{
		KeepersLoading::OpenLevel(World, FName(*Step.Argument));
	}
	else
	{
		UE_LOG(LogKeepersDirector, Warning, TEXT("Director: unknown step %s"), *Step.Action);
	}
}

void UKeepersDirectorSubsystem::UpdateAutoplay(UWorld* World)
{
	UObject* Combat = KeepersDirector::FindByClassName(World, TEXT("BP_CombatSystem_C"));
	UObject* Current = Combat ? KeepersReflection::GetObject(Combat, TEXT("CurrentUnit")) : nullptr;
	if (!Combat || !Current || AutoplayedCombat.Get() == Combat)
	{
		return;
	}
	// Every unit plays by the AI; the unit whose turn it is now starts its AI turn at once.
	const bool bCurrentHadAi = KeepersReflection::GetObject(Current, TEXT("Ai")) != nullptr;
	for (AActor* Unit : KeepersDirector::UnitsInCombat(Combat))
	{
		if (!KeepersReflection::GetObject(Unit, TEXT("Ai")))
		{
			KeepersReflection::FCall UseAi(Combat, TEXT("SetUnitUseAi"));
			UseAi.Set<UObject*>(TEXT("Unit"), Unit);
			UseAi.Set<bool>(TEXT("UseAi"), true);
			UseAi.Call();
		}
	}
	if (!bCurrentHadAi)
	{
		if (UObject* Ai = KeepersReflection::GetObject(Current, TEXT("Ai")))
		{
			KeepersReflection::FCall(Ai, TEXT("AiPlayTurn")).Call();
		}
	}
	AutoplayedCombat = Combat;
	UE_LOG(LogKeepersDirector, Log, TEXT("Director: autoplay on %s"), *World->GetMapName());
}

ACameraActor* UKeepersDirectorSubsystem::GetCamera(UWorld* World)
{
	if (!Camera.IsValid() || CameraWorld.Get() != World)
	{
		Camera = World->SpawnActor<ACameraActor>(FVector::ZeroVector, FRotator::ZeroRotator);
		CameraWorld = World;
		bHasFocus = false;
		if (Camera.IsValid())
		{
			Camera->GetCameraComponent()->SetConstraintAspectRatio(false);
			Camera->GetCameraComponent()->SetFieldOfView(55.f);
		}
	}
	if (APlayerController* Player = World->GetFirstPlayerController())
	{
		if (Camera.IsValid() && Player->GetViewTarget() != Camera.Get())
		{
			Player->SetViewTarget(Camera.Get());
		}
	}
	return Camera.Get();
}

void UKeepersDirectorSubsystem::UpdateCamera(UWorld* World, float DeltaTime)
{
	if (CameraMode == ECameraMode::Game || CameraMode == ECameraMode::Still)
	{
		if (CameraMode == ECameraMode::Still)
		{
			GetCamera(World);
		}
		return;
	}
	UObject* Combat = KeepersDirector::FindByClassName(World, TEXT("BP_CombatSystem_C"));
	const TArray<AActor*> Units = KeepersDirector::UnitsInCombat(Combat);
	if (Units.Num() == 0)
	{
		return;
	}
	FVector Center = FVector::ZeroVector;
	for (const AActor* Unit : Units)
	{
		Center += Unit->GetActorLocation();
	}
	Center /= Units.Num();
	ACameraActor* Shot = GetCamera(World);
	if (!Shot)
	{
		return;
	}
	FVector Focus = Center;
	FVector Location;
	OrbitAngle += DeltaTime * (CameraMode == ECameraMode::Orbit ? 5.f : 2.f);
	const FVector Around = FRotator(0.f, OrbitAngle, 0.f).Vector();
	if (CameraMode == ECameraMode::Follow)
	{
		const AActor* Current = Cast<AActor>(Combat ? KeepersReflection::GetObject(Combat, TEXT("CurrentUnit")) : nullptr);
		// The unit whose turn it is and its nearest foe, both in the frame.
		Focus = Current ? Current->GetActorLocation() : Center;
		float Spread = 600.f;
		if (Current)
		{
			int32 Team = 0;
			KeepersReflection::GetInt(Current->GetClass(), Current, TEXT("TeamIndex"), Team);
			const AActor* Foe = nullptr;
			for (const AActor* Unit : Units)
			{
				int32 Other = 0;
				KeepersReflection::GetInt(Unit->GetClass(), Unit, TEXT("TeamIndex"), Other);
				if (Other != Team && (!Foe || FVector::DistSquared(Unit->GetActorLocation(), Focus) < FVector::DistSquared(Foe->GetActorLocation(), Focus)))
				{
					Foe = Unit;
				}
			}
			if (Foe)
			{
				Spread = FMath::Clamp(FVector::Dist2D(Foe->GetActorLocation(), Focus), 400.f, 1600.f);
				Focus = (Focus + Foe->GetActorLocation()) * 0.5f;
			}
		}
		Location = Focus - Around * (500.f + Spread * 0.9f) + FVector(0.f, 0.f, 380.f + Spread * 0.6f);
	}
	else
	{
		Location = Center - Around * 2100.f + FVector(0.f, 0.f, 1500.f);
	}
	FocusPoint = bHasFocus ? FMath::VInterpTo(FocusPoint, Focus, DeltaTime, 1.5f) : Focus;
	const FVector From = bHasFocus ? FMath::VInterpTo(Shot->GetActorLocation(), Location, DeltaTime, 1.2f) : Location;
	bHasFocus = true;
	Shot->SetActorLocationAndRotation(From, (FocusPoint - From).Rotation());
}

void UKeepersDirectorSubsystem::CaptureFrame(UWorld* World)
{
	UGameViewportClient* Client = World->GetGameViewport();
	TSharedPtr<SViewport> Viewport = Client ? Client->GetGameViewportWidget() : nullptr;
	if (!Viewport.IsValid() || !FSlateApplication::IsInitialized())
	{
		return;
	}
	TArray<FColor> Pixels;
	FIntVector Size;
	if (!FSlateApplication::Get().TakeScreenshot(Viewport.ToSharedRef(), Pixels, Size) || Pixels.Num() != Size.X * Size.Y)
	{
		return;
	}
	const FString Path = FString::Printf(TEXT("%s/frame_%05d.jpg"), *Folder, ++Frame);
	++KeepersDirector::PendingWrites;
	Async(EAsyncExecution::ThreadPool, [Pixels = MoveTemp(Pixels), Size, Path]() mutable
	{
		for (FColor& Pixel : Pixels)
		{
			Pixel.A = 255;
		}
		FImageView Image(Pixels.GetData(), Size.X, Size.Y);
		FImageUtils::SaveImageByExtension(*Path, Image, 92);
		--KeepersDirector::PendingWrites;
	});
}
