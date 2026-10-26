#pragma once

#include "Components/SceneComponent.h"
#include "KeepersFigure.generated.h"

class UAnimSequence;
class USkeletalMeshComponent;
class UStaticMeshComponent;

/** Animation set of the Toony Tiny RTS Set, chosen by the weapon the figure holds. */
UENUM(BlueprintType)
enum class EKeepersAnimSet : uint8
{
	Infantry,  // one-handed weapon and shield
	TwoHanded,
	Spear,
	Polearm,
	Archer,
	Crossbow,
	Staff,     // has cast animations
	// Prebuilt meshes of their own skeletons (Body names the mesh, no head or weapons).
	Cavalry,
	Catapult,
	Ram
};

/** What a figure is doing. */
UENUM(BlueprintType)
enum class EKeepersPose : uint8
{
	Idle,
	Walk,
	Run,
	Attack,
	Cast,
	Hit,
	Death
};

/**
 * How a hero looks: parts of the Toony Tiny RTS Set by asset name ("Unit_Body_06d", "SM_w_sword", ...).
 * An empty name leaves the slot empty.
 */
USTRUCT(BlueprintType)
struct FKeepersLook
{
	GENERATED_BODY()

	/** Name shown in the outfit switcher ("Ветеран"). */
	UPROPERTY(BlueprintReadOnly)
	FText Name;

	UPROPERTY(BlueprintReadOnly)
	FName Body;

	UPROPERTY(BlueprintReadOnly)
	FName Head;

	/** Weapon in the right hand (Weapon_Socket_R). */
	UPROPERTY(BlueprintReadOnly)
	FName WeaponR;

	/** Weapon in the left hand (Weapon_Socket_L): bows, second daggers. */
	UPROPERTY(BlueprintReadOnly)
	FName WeaponL;

	UPROPERTY(BlueprintReadOnly)
	FName Shield;

	/** Quiver on the back (Backpack_Socket). */
	UPROPERTY(BlueprintReadOnly)
	FName Back;

	UPROPERTY(BlueprintReadOnly)
	EKeepersAnimSet AnimSet = EKeepersAnimSet::Infantry;
};

/**
 * A Toony Tiny RTS Set character assembled from parts: the body plays the animation, the head follows it
 * (leader pose), weapons, shield and quiver sit on the skeleton's sockets, everything wears one team colour.
 * Front of the figure is +Y, like the project's own unit meshes; it stands on its origin and is about
 * FigureHeight tall at scale 1.
 */
UCLASS(ClassGroup = Keepers, meta = (BlueprintSpawnableComponent))
class KEEPERSGAME_API UKeepersFigureComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	UKeepersFigureComponent();

	static constexpr float FigureHeight = 176.f;

	/** The pack's colours: Black, Blue, BlueB, Brown, Green, GreenB, Pink, Purple, Red, Tan, White, Yellow. */
	static const TArray<FName>& Colors();

	/** Russian name of a colour. */
	static FText ColorName(FName Color);

	void SetLook(const FKeepersLook& InLook);
	void SetColor(FName InColor);
	const FKeepersLook& GetLook() const { return Look; }

	/** Plays a pose. Attack, cast and hit play once and return to idle; death stays on the last frame. */
	void Play(EKeepersPose Pose);
	EKeepersPose GetPose() const { return CurrentPose; }

	/** Every primitive of the figure (for scene capture show-only lists). */
	TArray<UPrimitiveComponent*> GetParts() const;

	USkeletalMeshComponent* GetBody() const { return Body; }

	/** Animation of the set for a pose, or nullptr when the set has none (falls back to attack for cast). */
	static UAnimSequence* FindAnimation(EKeepersAnimSet Set, EKeepersPose Pose);

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	virtual void OnRegister() override;
	virtual void OnUnregister() override;

private:
	void Rebuild();
	void ApplyColor();
	UStaticMeshComponent* MakeAttachment(FName MeshName, FName Socket);

	FKeepersLook Look;
	FName Color = TEXT("Blue");
	EKeepersPose CurrentPose = EKeepersPose::Idle;
	float OneShotLeft = 0.f;

	UPROPERTY(Transient)
	TObjectPtr<USkeletalMeshComponent> Body;

	UPROPERTY(Transient)
	TObjectPtr<USkeletalMeshComponent> Head;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> Attachments;
};
