#pragma once

#include "Subsystems/GameInstanceSubsystem.h"
#include "Tickable.h"
#include "KeepersDirector.generated.h"

class ACameraActor;

/**
 * Records footage for trailers and alpha-test videos in a standalone game. Off unless the command line asks:
 *   -KeepersRecord=<folder>  frames frame_00001.jpg… (with the interface), at a fixed step of 1/Fps game seconds;
 *   -KeepersFps=30  -KeepersDuration=<game seconds>  -KeepersWarmup=<seconds before the first frame>;
 *   -KeepersSteps="t:action;t:action"  what happens when (game seconds from the start):
 *      autoplay — every unit of the battle plays by the AI;
 *      speed=2 — the game runs faster (time dilation);
 *      orbit / follow / still — the camera circles the field, follows the unit whose turn it is, or stays;
 *      squad / squadhero=Warrior,1 / look=2 — the squad screen of the main menu;
 *      newcampaign / camp=open,0 / node=3 — the campaign;
 *      open=/Game/Maps/… — opens a level with the loading screen;
 *      view=x,y,z,pitch,yaw,fov — a fixed shot.
 * The game quits when the duration is over.
 */
UCLASS()
class KEEPERSGAME_API UKeepersDirectorSubsystem : public UGameInstanceSubsystem, public FTickableGameObject
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickable() const override { return bEnabled; }
	virtual bool IsTickableWhenPaused() const override { return true; }
	virtual ETickableTickType GetTickableTickType() const override { return ETickableTickType::Conditional; }

private:
	enum class ECameraMode : uint8
	{
		Game,
		Still,
		Orbit,
		Follow
	};

	struct FStep
	{
		float Time = 0.f;
		FString Action;
		FString Argument;
	};

	void RunStep(const FStep& Step);
	void UpdateAutoplay(UWorld* World);
	void UpdateCamera(UWorld* World, float DeltaTime);
	void CaptureFrame(UWorld* World);
	ACameraActor* GetCamera(UWorld* World);

	bool bEnabled = false;
	FString Folder;
	float Fps = 30.f;
	float Duration = 30.f;
	float Warmup = 2.f;
	float Clock = 0.f;
	int32 Frame = 0;
	TArray<FStep> Steps;
	int32 NextStep = 0;

	bool bAutoplay = false;
	TWeakObjectPtr<UObject> AutoplayedCombat;
	ECameraMode CameraMode = ECameraMode::Game;
	float OrbitAngle = 200.f;
	TWeakObjectPtr<ACameraActor> Camera;
	TWeakObjectPtr<UWorld> CameraWorld;
	FVector FocusPoint = FVector::ZeroVector;
	bool bHasFocus = false;
};
