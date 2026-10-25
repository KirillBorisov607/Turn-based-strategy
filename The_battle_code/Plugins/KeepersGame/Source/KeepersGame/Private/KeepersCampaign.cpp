#include "KeepersCampaign.h"
#include "KeepersHeroes.h"
#include "KeepersLoadingScreen.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

DEFINE_LOG_CATEGORY_STATIC(LogKeepersCampaign, Log, All);

const FName UKeepersCampaignSubsystem::MapLevel = TEXT("/Game/Maps/Campaign/L_CampaignMap");
const FName UKeepersCampaignSubsystem::MainMenuLevel = TEXT("/Game/Maps/L_MainMenu");
const FString UKeepersCampaignSubsystem::SaveSlot = TEXT("KeepersCampaign");

namespace KeepersCampaign
{
	static const TArray<FName> BattleLevels = {
		TEXT("/Game/Maps/Slice/L_Slice_Hex"),
		TEXT("/Game/Maps/Slice/L_Slice_Chicken"),
		TEXT("/Game/Maps/Combats/L_Combat_SoSlimy"),
		TEXT("/Game/Maps/Combats/L_Combat_ChickenHunt"),
		TEXT("/Game/Maps/Combats/L_Combat_FairDuel"),
	};
	static const TArray<FName> EliteLevels = {
		TEXT("/Game/Maps/Combats/L_Combat_TooManyEnemies"),
	};
	static const FName BossLevel = TEXT("/Game/Maps/Combats/L_Combat_FlyingIsCheating");

	static constexpr int32 ColumnCount = 7;
}

const TArray<FKeepersArtifact>& UKeepersCampaignSubsystem::GetAllArtifacts()
{
	static const TArray<FKeepersArtifact> Artifacts = [] {
		auto Make = [](const TCHAR* Id, const TCHAR* Name, const TCHAR* Description)
		{
			FKeepersArtifact Artifact;
			Artifact.Id = Id;
			Artifact.Name = FText::FromString(Name);
			Artifact.Description = FText::FromString(Description);
			return Artifact;
		};
		return TArray<FKeepersArtifact>{
			Make(TEXT("StormFeather"), TEXT("Перо бури"), TEXT("Каждый толчок наносит на 1 урон больше.")),
			Make(TEXT("AncestorStone"), TEXT("Камень предков"), TEXT("Скалы Стража выше, а удар о скалу ранит на 1 сильнее.")),
			Make(TEXT("WorldTreeRoot"), TEXT("Корень Мирового древа"), TEXT("В начале хода герои в лесу лечатся на 2.")),
			Make(TEXT("RangerBoots"), TEXT("Сапоги следопыта"), TEXT("+1 к очкам движения всех героев.")),
			Make(TEXT("ScaleMail"), TEXT("Кольчуга из чешуи"), TEXT("+5 к здоровью всех героев.")),
			Make(TEXT("ShepherdBell"), TEXT("Колокольчик пастуха"), TEXT("Вражеские куры больше не зовут подмогу.")),
			Make(TEXT("SaltyBiscuit"), TEXT("Солёная галета"), TEXT("Слизни больше не делятся. Они просто грустят.")),
			Make(TEXT("OakHeart"), TEXT("Сердце дуба"), TEXT("Росток лечит союзника на выбранной клетке на 3.")),
		};
	}();
	return Artifacts;
}

const FKeepersArtifact* UKeepersCampaignSubsystem::FindArtifact(FName Id)
{
	return GetAllArtifacts().FindByPredicate([Id](const FKeepersArtifact& Artifact) { return Artifact.Id == Id; });
}

UKeepersCampaignSubsystem* UKeepersCampaignSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	return GameInstance ? GameInstance->GetSubsystem<UKeepersCampaignSubsystem>() : nullptr;
}

void UKeepersCampaignSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Save = NewObject<UKeepersCampaignSave>(this);
}

bool UKeepersCampaignSubsystem::HasSavedCampaign()
{
	return UGameplayStatics::DoesSaveGameExist(SaveSlot, 0);
}

void UKeepersCampaignSubsystem::StartNewCampaign()
{
	Save = NewObject<UKeepersCampaignSave>(this);
	GenerateMap(FMath::Rand());
	bActive = true;
	WriteSave();
	UE_LOG(LogKeepersCampaign, Log, TEXT("New campaign, %d nodes"), Save->Nodes.Num());
	OpenLevel(MapLevel);
}

bool UKeepersCampaignSubsystem::ContinueCampaign()
{
	UKeepersCampaignSave* Loaded = Cast<UKeepersCampaignSave>(UGameplayStatics::LoadGameFromSlot(SaveSlot, 0));
	if (!Loaded || Loaded->Nodes.Num() == 0)
	{
		return false;
	}
	Save = Loaded;
	bActive = true;
	// A battle that was interrupted is played again.
	if (Save->CurrentNode != INDEX_NONE && !Save->VisitedNodes.Contains(Save->CurrentNode))
	{
		Save->CurrentNode = Save->VisitedNodes.Num() > 0 ? Save->VisitedNodes.Last() : INDEX_NONE;
	}
	OpenLevel(MapLevel);
	return true;
}

void UKeepersCampaignSubsystem::GenerateMap(int32 Seed)
{
	FRandomStream Random(Seed);
	Save->Seed = Seed;
	Save->Nodes.Reset();

	TArray<TArray<int32>> Columns;
	for (int32 Column = 0; Column < KeepersCampaign::ColumnCount; ++Column)
	{
		const bool bLast = Column == KeepersCampaign::ColumnCount - 1;
		const int32 Count = bLast ? 1 : (Column == 0 ? 2 : Random.RandRange(2, 3));
		TArray<int32>& Indexes = Columns.AddDefaulted_GetRef();
		for (int32 Row = 0; Row < Count; ++Row)
		{
			FKeepersMapNode Node;
			Node.Column = Column;
			Node.Height = Count == 1 ? 0.5f : 0.15f + 0.7f * Row / (Count - 1) + Random.FRandRange(-0.05f, 0.05f);
			if (bLast)
			{
				Node.Type = EKeepersNodeType::Boss;
			}
			else if (Column == 0)
			{
				Node.Type = EKeepersNodeType::Battle;
			}
			else if (Column == KeepersCampaign::ColumnCount - 2)
			{
				Node.Type = Row == 0 ? EKeepersNodeType::Camp : (Random.FRand() < 0.5f ? EKeepersNodeType::Ruins : EKeepersNodeType::Battle);
			}
			else
			{
				const float Roll = Random.FRand();
				Node.Type = Roll < 0.5f ? EKeepersNodeType::Battle
					: Roll < 0.7f ? EKeepersNodeType::Ruins
					: Roll < 0.85f ? EKeepersNodeType::Camp
					: (Column >= 2 ? EKeepersNodeType::Elite : EKeepersNodeType::Battle);
			}
			switch (Node.Type)
			{
			case EKeepersNodeType::Battle:
				Node.Level = KeepersCampaign::BattleLevels[Random.RandRange(0, KeepersCampaign::BattleLevels.Num() - 1)];
				break;
			case EKeepersNodeType::Elite:
				Node.Level = KeepersCampaign::EliteLevels[Random.RandRange(0, KeepersCampaign::EliteLevels.Num() - 1)];
				break;
			case EKeepersNodeType::Boss:
				Node.Level = KeepersCampaign::BossLevel;
				break;
			default:
				break;
			}
			Indexes.Add(Save->Nodes.Add(Node));
		}
	}

	// Each node leads to the nearest node(s) of the next column; every node gets at least one way in.
	for (int32 Column = 0; Column + 1 < Columns.Num(); ++Column)
	{
		TSet<int32> Reached;
		for (int32 From : Columns[Column])
		{
			TArray<int32> Candidates = Columns[Column + 1];
			const float Height = Save->Nodes[From].Height;
			Candidates.Sort([this, Height](int32 A, int32 B)
			{
				return FMath::Abs(Save->Nodes[A].Height - Height) < FMath::Abs(Save->Nodes[B].Height - Height);
			});
			const int32 Links = FMath::Min(Candidates.Num(), Random.FRand() < 0.5f ? 1 : 2);
			for (int32 I = 0; I < Links; ++I)
			{
				Save->Nodes[From].Next.AddUnique(Candidates[I]);
				Reached.Add(Candidates[I]);
			}
		}
		for (int32 To : Columns[Column + 1])
		{
			if (!Reached.Contains(To))
			{
				// Link from the closest node of this column.
				int32 Best = Columns[Column][0];
				for (int32 From : Columns[Column])
				{
					if (FMath::Abs(Save->Nodes[From].Height - Save->Nodes[To].Height) < FMath::Abs(Save->Nodes[Best].Height - Save->Nodes[To].Height))
					{
						Best = From;
					}
				}
				Save->Nodes[Best].Next.AddUnique(To);
			}
		}
	}
}

TArray<int32> UKeepersCampaignSubsystem::GetAvailableNodes() const
{
	TArray<int32> Result;
	if (!Save || Save->Nodes.Num() == 0)
	{
		return Result;
	}
	if (Save->CurrentNode == INDEX_NONE)
	{
		for (int32 I = 0; I < Save->Nodes.Num(); ++I)
		{
			if (Save->Nodes[I].Column == 0)
			{
				Result.Add(I);
			}
		}
		return Result;
	}
	if (Save->VisitedNodes.Contains(Save->CurrentNode))
	{
		Result = Save->Nodes[Save->CurrentNode].Next;
	}
	return Result;
}

bool UKeepersCampaignSubsystem::IsInBattleNode() const
{
	if (!bActive || !Save || !Save->Nodes.IsValidIndex(Save->CurrentNode) || Save->VisitedNodes.Contains(Save->CurrentNode))
	{
		return false;
	}
	const EKeepersNodeType Type = Save->Nodes[Save->CurrentNode].Type;
	return Type == EKeepersNodeType::Battle || Type == EKeepersNodeType::Elite || Type == EKeepersNodeType::Boss;
}

bool UKeepersCampaignSubsystem::IsBossNode(int32 NodeIndex) const
{
	return Save && Save->Nodes.IsValidIndex(NodeIndex) && Save->Nodes[NodeIndex].Type == EKeepersNodeType::Boss;
}

void UKeepersCampaignSubsystem::EnterNode(int32 NodeIndex)
{
	if (!GetAvailableNodes().Contains(NodeIndex))
	{
		return;
	}
	Save->CurrentNode = NodeIndex;
	WriteSave();
	const FKeepersMapNode& Node = Save->Nodes[NodeIndex];
	UE_LOG(LogKeepersCampaign, Log, TEXT("Enter node %d (%s)"), NodeIndex, *StaticEnum<EKeepersNodeType>()->GetNameStringByValue(static_cast<int64>(Node.Type)));
	if (!Node.Level.IsNone())
	{
		OpenLevel(Node.Level);
	}
	// Ruins and camps are resolved on the map screen (see UKeepersCampaignMapWidget).
}

bool UKeepersCampaignSubsystem::HasArtifact(FName Id) const
{
	return bActive && Save && Save->Artifacts.Contains(Id);
}

bool UKeepersCampaignSubsystem::HasArtifact(const UObject* WorldContext, FName Id)
{
	const UKeepersCampaignSubsystem* Campaign = Get(WorldContext);
	return Campaign && Campaign->HasArtifact(Id);
}

void UKeepersCampaignSubsystem::AddArtifact(FName Id)
{
	Save->Artifacts.AddUnique(Id);
	WriteSave();
}

TArray<FName> UKeepersCampaignSubsystem::RollArtifacts(int32 Count) const
{
	TArray<FName> Pool;
	for (const FKeepersArtifact& Artifact : GetAllArtifacts())
	{
		if (!Save->Artifacts.Contains(Artifact.Id))
		{
			Pool.Add(Artifact.Id);
		}
	}
	TArray<FName> Result;
	while (Result.Num() < Count && Pool.Num() > 0)
	{
		Result.Add(Pool[FMath::RandRange(0, Pool.Num() - 1)]);
		Pool.Remove(Result.Last());
	}
	return Result;
}

void UKeepersCampaignSubsystem::CompleteCurrentNode()
{
	if (!Save->Nodes.IsValidIndex(Save->CurrentNode))
	{
		return;
	}
	Save->VisitedNodes.AddUnique(Save->CurrentNode);
	// Glory for every won battle: more for elites and the boss.
	if (UKeepersPartySubsystem* Party = GetGameInstance()->GetSubsystem<UKeepersPartySubsystem>())
	{
		const EKeepersNodeType Type = Save->Nodes[Save->CurrentNode].Type;
		Party->AddGlory(Type == EKeepersNodeType::Boss ? 40 : Type == EKeepersNodeType::Elite ? 12 : Type == EKeepersNodeType::Battle ? 6 : 0);
	}
	if (IsBossNode(Save->CurrentNode))
	{
		UE_LOG(LogKeepersCampaign, Log, TEXT("Campaign won"));
		bActive = false;
		UGameplayStatics::DeleteGameInSlot(SaveSlot, 0);
		OpenLevel(MainMenuLevel);
		return;
	}
	WriteSave();
	OpenLevel(MapLevel);
}

void UKeepersCampaignSubsystem::FailCampaign()
{
	UE_LOG(LogKeepersCampaign, Log, TEXT("Campaign lost"));
	// A lost run still brings some glory: two for every place visited.
	if (UKeepersPartySubsystem* Party = GetGameInstance()->GetSubsystem<UKeepersPartySubsystem>())
	{
		Party->AddGlory(Save->VisitedNodes.Num() * 2);
	}
	bActive = false;
	UGameplayStatics::DeleteGameInSlot(SaveSlot, 0);
	OpenLevel(MainMenuLevel);
}

void UKeepersCampaignSubsystem::EnsureLoaded()
{
	if (bActive)
	{
		return;
	}
	UKeepersCampaignSave* Loaded = Cast<UKeepersCampaignSave>(UGameplayStatics::LoadGameFromSlot(SaveSlot, 0));
	if (Loaded && Loaded->Nodes.Num() > 0)
	{
		Save = Loaded;
	}
	else
	{
		Save = NewObject<UKeepersCampaignSave>(this);
		GenerateMap(FMath::Rand());
		WriteSave();
	}
	bActive = true;
}

void UKeepersCampaignSubsystem::RetryCurrentNode()
{
	if (Save->Nodes.IsValidIndex(Save->CurrentNode) && !Save->Nodes[Save->CurrentNode].Level.IsNone())
	{
		OpenLevel(Save->Nodes[Save->CurrentNode].Level);
	}
}

void UKeepersCampaignSubsystem::ReturnToMainMenu(const UObject* WorldContext)
{
	WriteSave();
	bActive = false;
	OpenLevel(MainMenuLevel);
}

void UKeepersCampaignSubsystem::WriteSave() const
{
	if (Save)
	{
		UGameplayStatics::SaveGameToSlot(Save, SaveSlot, 0);
	}
}

void UKeepersCampaignSubsystem::OpenLevel(FName Level) const
{
	if (UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr)
	{
		KeepersLoading::OpenLevel(World, Level);
	}
}
