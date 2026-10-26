#include "KeepersFigure.h"

#include "Animation/AnimSequence.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"

namespace KeepersFigure
{
	static const TCHAR* Root = TEXT("/Game/Toony_Tiny_RTS_Set");

	template <typename T>
	static T* Load(const FString& Folder, FName Name)
	{
		if (Name.IsNone())
		{
			return nullptr;
		}
		const FString Asset = Name.ToString();
		return LoadObject<T>(nullptr, *FString::Printf(TEXT("%s/%s/%s.%s"), Root, *Folder, *Asset, *Asset));
	}

	/** Folder and file prefix of an animation set ("Units/Infantry", "infantry"). */
	static void SetNames(EKeepersAnimSet Set, FString& OutFolder, FString& OutPrefix)
	{
		switch (Set)
		{
		case EKeepersAnimSet::TwoHanded: OutFolder = TEXT("TwoHanded"); OutPrefix = TEXT("twohanded"); break;
		case EKeepersAnimSet::Spear:     OutFolder = TEXT("Spear");     OutPrefix = TEXT("spear"); break;
		case EKeepersAnimSet::Polearm:   OutFolder = TEXT("Polearm");   OutPrefix = TEXT("polearm"); break;
		case EKeepersAnimSet::Archer:    OutFolder = TEXT("Archer");    OutPrefix = TEXT("archer"); break;
		case EKeepersAnimSet::Crossbow:  OutFolder = TEXT("Crossbow");  OutPrefix = TEXT("crossbow"); break;
		case EKeepersAnimSet::Staff:     OutFolder = TEXT("Staff");     OutPrefix = TEXT("staff"); break;
		default:                         OutFolder = TEXT("Infantry");  OutPrefix = TEXT("infantry"); break;
		}
	}

	static bool IsPrebuilt(EKeepersAnimSet Set)
	{
		return Set == EKeepersAnimSet::Cavalry || Set == EKeepersAnimSet::Catapult || Set == EKeepersAnimSet::Ram;
	}

	/** Riders and machines: their own folders and shorter animation lists. */
	static UAnimSequence* FindPrebuiltAnimation(EKeepersAnimSet Set, EKeepersPose Pose)
	{
		if (Set == EKeepersAnimSet::Cavalry)
		{
			const TCHAR* Suffix = TEXT("01_idle");
			switch (Pose)
			{
			case EKeepersPose::Walk:   Suffix = TEXT("02_walk"); break;
			case EKeepersPose::Run:    Suffix = TEXT("03_run"); break;
			case EKeepersPose::Attack:
			case EKeepersPose::Cast:   Suffix = TEXT("04_attack"); break;
			case EKeepersPose::Hit:    Suffix = TEXT("05_take_damage"); break;
			case EKeepersPose::Death:  Suffix = TEXT("06_death_A"); break;
			default: break;
			}
			return Load<UAnimSequence>(TEXT("Animations/Cavalry/Cavalry"), FName(*FString::Printf(TEXT("Anim_cav_%s"), Suffix)));
		}
		const TCHAR* Machine = Set == EKeepersAnimSet::Catapult ? TEXT("catapult") : TEXT("ram");
		const TCHAR* Suffix = TEXT("01_idle");
		switch (Pose)
		{
		case EKeepersPose::Walk:
		case EKeepersPose::Run:    Suffix = TEXT("02_move"); break;
		case EKeepersPose::Attack:
		case EKeepersPose::Cast:   Suffix = TEXT("03_attack"); break;
		case EKeepersPose::Hit:    Suffix = TEXT("04_damage"); break;
		case EKeepersPose::Death:  Suffix = TEXT("05_death"); break;
		default: break;
		}
		return Load<UAnimSequence>(FString(TEXT("Animations/Units/")) + (Set == EKeepersAnimSet::Catapult ? TEXT("Catapult") : TEXT("Ram")),
			FName(*FString::Printf(TEXT("Anim_%s_%s"), Machine, Suffix)));
	}
}

UKeepersFigureComponent::UKeepersFigureComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
	// Menus pause nothing, but the squad screen preview must animate while a battle is paused too.
	PrimaryComponentTick.bTickEvenWhenPaused = true;
}

const TArray<FName>& UKeepersFigureComponent::Colors()
{
	static const TArray<FName> All = { TEXT("Blue"), TEXT("BlueB"), TEXT("Green"), TEXT("GreenB"), TEXT("Purple"), TEXT("Yellow"),
		TEXT("Brown"), TEXT("Tan"), TEXT("White"), TEXT("Black"), TEXT("Pink"), TEXT("Red") };
	return All;
}

FText UKeepersFigureComponent::ColorName(FName InColor)
{
	static const TMap<FName, const TCHAR*> Names =
	{
		{ TEXT("Blue"), TEXT("Синий") }, { TEXT("BlueB"), TEXT("Лазурный") }, { TEXT("Green"), TEXT("Зелёный") },
		{ TEXT("GreenB"), TEXT("Травяной") }, { TEXT("Purple"), TEXT("Фиолетовый") }, { TEXT("Yellow"), TEXT("Жёлтый") },
		{ TEXT("Brown"), TEXT("Бурый") }, { TEXT("Tan"), TEXT("Песочный") }, { TEXT("White"), TEXT("Белый") },
		{ TEXT("Black"), TEXT("Чёрный") }, { TEXT("Pink"), TEXT("Розовый") }, { TEXT("Red"), TEXT("Красный") },
	};
	const TCHAR* const* Name = Names.Find(InColor);
	return FText::FromString(Name ? *Name : *InColor.ToString());
}

UAnimSequence* UKeepersFigureComponent::FindAnimation(EKeepersAnimSet Set, EKeepersPose Pose)
{
	if (KeepersFigure::IsPrebuilt(Set))
	{
		return KeepersFigure::FindPrebuiltAnimation(Set, Pose);
	}
	FString Folder;
	FString Prefix;
	KeepersFigure::SetNames(Set, Folder, Prefix);
	const TCHAR* Suffix = TEXT("01_idle");
	switch (Pose)
	{
	case EKeepersPose::Walk:   Suffix = TEXT("02_walk"); break;
	case EKeepersPose::Run:    Suffix = TEXT("03_run"); break;
	case EKeepersPose::Attack: Suffix = TEXT("04_attack_A"); break;
	case EKeepersPose::Cast:   Suffix = TEXT("07_cast_A"); break;
	case EKeepersPose::Hit:    Suffix = TEXT("05_take_damage"); break;
	case EKeepersPose::Death:  Suffix = TEXT("06_death_A"); break;
	default: break;
	}
	UAnimSequence* Sequence = KeepersFigure::Load<UAnimSequence>(TEXT("Animations/Units/") + Folder, FName(*FString::Printf(TEXT("Anim_%s_%s"), *Prefix, Suffix)));
	if (!Sequence && Pose == EKeepersPose::Cast)
	{
		return FindAnimation(Set, EKeepersPose::Attack);
	}
	return Sequence;
}

void UKeepersFigureComponent::OnRegister()
{
	Super::OnRegister();
	if (!Body)
	{
		Body = NewObject<USkeletalMeshComponent>(this, TEXT("FigureBody"), RF_Transient);
		Body->SetupAttachment(this);
		Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Body->SetAnimationMode(EAnimationMode::AnimationSingleNode);
		Body->PrimaryComponentTick.bTickEvenWhenPaused = true;
		Body->RegisterComponent();

		Head = NewObject<USkeletalMeshComponent>(this, TEXT("FigureHead"), RF_Transient);
		Head->SetupAttachment(Body);
		Head->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Head->RegisterComponent();
		Head->SetLeaderPoseComponent(Body);
	}
	Rebuild();
}

void UKeepersFigureComponent::OnUnregister()
{
	for (UStaticMeshComponent* Attachment : Attachments)
	{
		if (Attachment)
		{
			Attachment->DestroyComponent();
		}
	}
	Attachments.Reset();
	if (Head)
	{
		Head->DestroyComponent();
		Head = nullptr;
	}
	if (Body)
	{
		Body->DestroyComponent();
		Body = nullptr;
	}
	Super::OnUnregister();
}

void UKeepersFigureComponent::SetLook(const FKeepersLook& InLook)
{
	Look = InLook;
	Rebuild();
}

void UKeepersFigureComponent::SetColor(FName InColor)
{
	Color = InColor;
	ApplyColor();
}

UStaticMeshComponent* UKeepersFigureComponent::MakeAttachment(FName MeshName, FName Socket)
{
	UStaticMesh* Mesh = KeepersFigure::Load<UStaticMesh>(TEXT("Meshes/Weapons"), MeshName);
	if (!Mesh || !Body)
	{
		return nullptr;
	}
	UStaticMeshComponent* Component = NewObject<UStaticMeshComponent>(this, NAME_None, RF_Transient);
	Component->SetStaticMesh(Mesh);
	Component->SetupAttachment(Body, Socket);
	Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Component->RegisterComponent();
	Attachments.Add(Component);
	return Component;
}

void UKeepersFigureComponent::Rebuild()
{
	if (!Body || !Head)
	{
		return;
	}
	const bool bPrebuilt = KeepersFigure::IsPrebuilt(Look.AnimSet);
	const TCHAR* BodyFolder = !bPrebuilt ? TEXT("Meshes/Characters_Parts/Units")
		: Look.AnimSet == EKeepersAnimSet::Cavalry ? TEXT("Meshes/Characters_Prebuilt/Cavalry") : TEXT("Meshes/Characters_Prebuilt/Machines");
	Body->SetSkeletalMeshAsset(KeepersFigure::Load<USkeletalMesh>(BodyFolder, Look.Body));
	Head->SetSkeletalMeshAsset(bPrebuilt ? nullptr : KeepersFigure::Load<USkeletalMesh>(TEXT("Meshes/Characters_Parts/Units"), Look.Head));
	Head->SetLeaderPoseComponent(Body, true);

	for (UStaticMeshComponent* Attachment : Attachments)
	{
		if (Attachment)
		{
			Attachment->DestroyComponent();
		}
	}
	Attachments.Reset();
	MakeAttachment(Look.WeaponR, TEXT("Weapon_Socket_R"));
	MakeAttachment(Look.WeaponL, TEXT("Weapon_Socket_L"));
	MakeAttachment(Look.Shield, TEXT("Shield_Socket"));
	MakeAttachment(Look.Back, TEXT("Backpack_Socket"));

	ApplyColor();
	const EKeepersPose Pose = CurrentPose;
	CurrentPose = EKeepersPose::Idle;
	Play(Pose == EKeepersPose::Death ? Pose : EKeepersPose::Idle);
}

void UKeepersFigureComponent::ApplyColor()
{
	UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr,
		*FString::Printf(TEXT("%s/Materials/Units/MI_Units_%s.MI_Units_%s"), KeepersFigure::Root, *Color.ToString(), *Color.ToString()));
	if (!Material)
	{
		return;
	}
	for (UPrimitiveComponent* Part : GetParts())
	{
		for (int32 Slot = 0; Slot < Part->GetNumMaterials(); ++Slot)
		{
			Part->SetMaterial(Slot, Material);
		}
	}
}

TArray<UPrimitiveComponent*> UKeepersFigureComponent::GetParts() const
{
	TArray<UPrimitiveComponent*> Parts;
	if (Body)
	{
		Parts.Add(Body);
	}
	if (Head)
	{
		Parts.Add(Head);
	}
	for (UStaticMeshComponent* Attachment : Attachments)
	{
		if (Attachment)
		{
			Parts.Add(Attachment);
		}
	}
	return Parts;
}

void UKeepersFigureComponent::Play(EKeepersPose Pose)
{
	if (!Body)
	{
		return;
	}
	UAnimSequence* Sequence = FindAnimation(Look.AnimSet, Pose);
	if (!Sequence)
	{
		return;
	}
	const bool bLoop = Pose == EKeepersPose::Idle || Pose == EKeepersPose::Walk || Pose == EKeepersPose::Run;
	CurrentPose = Pose;
	OneShotLeft = bLoop || Pose == EKeepersPose::Death ? 0.f : Sequence->GetPlayLength();
	Body->PlayAnimation(Sequence, bLoop);
}

void UKeepersFigureComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (OneShotLeft > 0.f)
	{
		OneShotLeft -= DeltaTime;
		if (OneShotLeft <= 0.f)
		{
			Play(EKeepersPose::Idle);
		}
	}
}
