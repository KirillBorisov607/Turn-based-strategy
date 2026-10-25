// Level kit: the "Хранители" menu of the level editor. Tools for building battle levels by hand:
// a new level from the template, a hex field to sculpt, raise / lower / snap, and a check of the level
// the way the grid will see it when combat starts.

#include "KeepersLevelTools.h"
#include "KeepersLevelKit.h"
#include "KeepersReflection.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "DrawDebugHelpers.h"
#include "Editor.h"
#include "Engine/Selection.h"
#include "Engine/StaticMeshActor.h"
#include "EngineUtils.h"
#include "FileHelpers.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Misc/MessageDialog.h"
#include "Misc/PackageName.h"
#include "Modules/ModuleManager.h"
#include "ScopedTransaction.h"
#include "Styling/AppStyle.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "ToolMenus.h"
#include "UObject/SavePackage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SWindow.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "KeepersLevelTools"

namespace KeepersLevelTools
{
	static const TCHAR* Template = TEXT("/Game/Maps/Generated/L_Generated");
	static const TCHAR* LevelFolder = TEXT("/Game/Maps/Battles");
	static const TCHAR* Column = TEXT("/Game/Art/Meshes/SM_Hexagon_Platform_01.SM_Hexagon_Platform_01");
	static const TCHAR* Tops[] =
	{
		TEXT("/Game/Art/Materials/ColorsWithTexture/MI_Hexagon_Checker_01.MI_Hexagon_Checker_01"),
		TEXT("/Game/Art/Materials/ColorsWithTexture/MI_Hexagon_Checker_02.MI_Hexagon_Checker_02"),
	};
	static const TCHAR* Side = TEXT("/Game/Art/Materials/Colors/MI_Brown_08.MI_Brown_08");
	static const FName FieldFolder(TEXT("Поле"));

	UWorld* EditorWorld()
	{
		return GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	}

	void Notify(const FString& Text, bool bGood = true)
	{
		FNotificationInfo Info(FText::FromString(Text));
		Info.ExpireDuration = 5.f;
		Info.bUseSuccessFailIcons = true;
		Info.Image = nullptr;
		if (TSharedPtr<SNotificationItem> Item = FSlateNotificationManager::Get().AddNotification(Info))
		{
			Item->SetCompletionState(bGood ? SNotificationItem::CS_Success : SNotificationItem::CS_Fail);
		}
	}

	bool FindGrid(FKeepersLevelGrid& Out)
	{
		UWorld* World = EditorWorld();
		if (World && KeepersLevelKit::PrepareGrid(World, Out))
		{
			// The scenario's construction script centres the grid on the scenario and reads where units stand.
			Out.Scenario->RerunConstructionScripts();
			KeepersLevelKit::PrepareGrid(World, Out);
			return true;
		}
		Notify(TEXT("В уровне нет BP_Grid и BP_CombatScenario — это не уровень боя."), false);
		return false;
	}

	TArray<AActor*> Selected()
	{
		TArray<AActor*> Actors;
		for (FSelectionIterator It(GEditor->GetSelectedActorIterator()); It; ++It)
		{
			if (AActor* Actor = Cast<AActor>(*It))
			{
				Actors.Add(Actor);
			}
		}
		return Actors;
	}

	bool IsUnit(const AActor* Actor)
	{
		return Actor && Actor->GetClass()->GetName() == TEXT("BP_Unit_C");
	}

	/** A hexagon column standing on a cell, its top on Level. */
	AStaticMeshActor* SpawnColumn(UWorld* World, const FKeepersLevelGrid& Grid, const FIntPoint& Index, int32 Level)
	{
		UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, Column);
		if (!Mesh)
		{
			return nullptr;
		}
		const FBoxSphereBounds Bounds = Mesh->GetBounds();
		const float Scale = Grid.TileSize.X / FMath::Max(Bounds.BoxExtent.X * 2.f, 1.f);
		const float Top = Bounds.Origin.Z + Bounds.BoxExtent.Z;
		const FVector Cell = KeepersLevelKit::TileLocation(Grid, Index);
		FActorSpawnParameters Params;
		Params.ObjectFlags = RF_Transactional;
		AStaticMeshActor* Actor = World->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(),
			FTransform(FRotator::ZeroRotator, FVector(Cell.X, Cell.Y, Cell.Z + Level * Grid.TileSize.Z - Top), FVector(Scale, Scale, 1.f)), Params);
		if (!Actor)
		{
			return nullptr;
		}
		UStaticMeshComponent* Component = Actor->GetStaticMeshComponent();
		Component->SetStaticMesh(Mesh);
		Component->SetCollisionProfileName(TEXT("BlockAll"));
		// GroundAndGridModifiers: the grid finds its tiles on it; LineOfSight: a column blocks shots past its edge.
		Component->SetCollisionResponseToChannel(ECC_GameTraceChannel1, ECR_Block);
		Component->SetCollisionResponseToChannel(ECC_GameTraceChannel4, ECR_Block);
		// A checker of two tones, like the hand-made maps.
		Component->SetMaterial(0, LoadObject<UMaterialInterface>(nullptr, Tops[(Index.X / 2 + Index.Y / (Grid.IsHex() ? 4 : 2)) % 2]));
		Component->SetMaterial(1, LoadObject<UMaterialInterface>(nullptr, Side));
		Actor->SetActorLabel(FString::Printf(TEXT("Hex_%d_%d"), Index.X, Index.Y));
		Actor->SetFolderPath(FieldFolder);
		return Actor;
	}

	int32 FillField(UWorld* World, const FKeepersLevelGrid& Grid)
	{
		int32 Spawned = 0;
		for (const FIntPoint& Index : KeepersLevelKit::AllIndexes(Grid))
		{
			FVector Ground;
			if (KeepersLevelKit::TraceTile(Grid, Index, Ground) == 0 && SpawnColumn(World, Grid, Index, 0))
			{
				++Spawned;
			}
		}
		return Spawned;
	}

	/** Puts a unit on the cell centre, standing on the ground there. */
	void PlaceUnit(const FKeepersLevelGrid& Grid, AActor* Unit, const FIntPoint& Index)
	{
		FVector Ground = KeepersLevelKit::TileLocation(Grid, Index);
		KeepersLevelKit::TraceTile(Grid, Index, Ground);
		Unit->Modify();
		Unit->SetActorLocation(FVector(Ground.X, Ground.Y, Ground.Z + 100.f));
	}

	// --- the table of levels -------------------------------------------------------------------------------------

	UDataTable* TableForEdit()
	{
		if (UDataTable* Table = KeepersLevelKit::GetTable())
		{
			return Table;
		}
		const FString PackagePath = FPackageName::ObjectPathToPackageName(FString(KeepersLevelKit::TablePath));
		UPackage* Package = CreatePackage(*PackagePath);
		UDataTable* Table = NewObject<UDataTable>(Package, *FPackageName::GetShortName(PackagePath), RF_Public | RF_Standalone | RF_Transactional);
		Table->RowStruct = FKeepersLevelRow::StaticStruct();
		FAssetRegistryModule::AssetCreated(Table);
		return Table;
	}

	void SaveTable(UDataTable* Table)
	{
		Table->MarkPackageDirty();
		UEditorLoadingAndSavingUtils::SavePackages({ Table->GetOutermost() }, false);
	}

	/** Adds the level to the game's table (campaign, quick battles, loading screen); false if it is there. */
	bool Register(const FString& LevelPackage, const FString& Title)
	{
		UDataTable* Table = TableForEdit();
		const FName RowName(*FPackageName::GetShortName(LevelPackage));
		if (Table->FindRow<FKeepersLevelRow>(RowName, TEXT("KeepersLevelTools"), false))
		{
			return false;
		}
		Table->Modify();
		FKeepersLevelRow Row;
		Row.Level = TSoftObjectPtr<UWorld>(FSoftObjectPath(LevelPackage + TEXT(".") + FPackageName::GetShortName(LevelPackage)));
		Row.Title = FText::FromString(Title);
		Row.Subtitle = FText::FromString(TEXT("Новое поле боя."));
		Table->AddRow(RowName, Row);
		SaveTable(Table);
		return true;
	}

	// --- commands ------------------------------------------------------------------------------------------------

	FString AskTitle()
	{
		FString Result;
		TSharedPtr<SEditableTextBox> Box;
		TSharedRef<SWindow> Window = SNew(SWindow)
			.Title(LOCTEXT("NewLevelTitle", "Новый уровень боя"))
			.SizingRule(ESizingRule::Autosized)
			.SupportsMinimize(false)
			.SupportsMaximize(false);
		TWeakPtr<SWindow> WeakWindow = Window;
		auto Accept = [&Result, &Box, WeakWindow]()
		{
			Result = Box->GetText().ToString().TrimStartAndEnd();
			if (WeakWindow.IsValid())
			{
				WeakWindow.Pin()->RequestDestroyWindow();
			}
			return FReply::Handled();
		};
		Window->SetContent(SNew(SBox).Padding(16.f).MinDesiredWidth(420.f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 8.f)
				[
					SNew(STextBlock).Text(LOCTEXT("NewLevelPrompt", "Название боя (его увидят в меню и на экране загрузки):"))
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 12.f)
				[
					SAssignNew(Box, SEditableTextBox)
					.HintText(LOCTEXT("NewLevelHint", "Например: Мельница у брода"))
					.OnTextCommitted_Lambda([Accept](const FText&, ETextCommit::Type Commit) mutable { if (Commit == ETextCommit::OnEnter) { Accept(); } })
				]
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right)
				[
					SNew(SButton).Text(LOCTEXT("NewLevelCreate", "Создать")).OnClicked_Lambda(Accept)
				]
			]);
		GEditor->EditorAddModalWindow(Window);
		return Result;
	}

	FString CreateLevel(const FString& Title)
	{
		FString Package;
		for (int32 Number = 1; Number < 1000; ++Number)
		{
			Package = FString::Printf(TEXT("%s/L_Battle_%02d"), LevelFolder, Number);
			if (!FPackageName::DoesPackageExist(Package))
			{
				break;
			}
		}
		UWorld* World = UEditorLoadingAndSavingUtils::NewMapFromTemplate(Template, true);
		FKeepersLevelGrid Grid;
		if (!World || !KeepersLevelKit::PrepareGrid(World, Grid))
		{
			Notify(TEXT("Не удалось открыть шаблон уровня."), false);
			return FString();
		}
		// The template's grid: hexagons, 16 × 12 cells around the origin (the grid is centred on the scenario).
		Grid.Scenario->SetActorLocation(FVector(0.f, 0.f, Grid.Scenario->GetActorLocation().Z));
		FProperty* GridDataProperty = nullptr;
		void* GridData = KeepersReflection::FindValuePtr(Grid.Scenario->GetClass(), Grid.Scenario, TEXT("GridData"), &GridDataProperty);
		if (const FStructProperty* GridDataStruct = CastField<FStructProperty>(GridDataProperty))
		{
			const FIntPoint Count(16, 12);
			const uint8 Hexagon = 2;
			const bool bTrue = true;
			auto Set = [&](const TCHAR* Name, const void* Value)
			{
				FProperty* Property = nullptr;
				if (void* Ptr = KeepersReflection::FindValuePtr(GridDataStruct->Struct, GridData, Name, &Property))
				{
					Property->CopyCompleteValue(Ptr, Value);
				}
			};
			Set(TEXT("TileCount"), &Count);
			Set(TEXT("Shape"), &Hexagon);
			Set(TEXT("GenerateGridBeforeStartingCombat"), &bTrue);
			Set(TEXT("UseEnvironment"), &bTrue);
			Grid.Scenario->RerunConstructionScripts();
			KeepersLevelKit::PrepareGrid(World, Grid);
		}
		FillField(World, Grid);
		// Heroes on the left edge, enemies on the right; the enemies are tagged, so new ones can be told apart.
		const TArray<TArray<AActor*>> Teams = KeepersLevelKit::GetTeams(Grid);
		for (int32 Team = 0; Team < Teams.Num() && Team < 2; ++Team)
		{
			for (int32 I = 0; I < Teams[Team].Num(); ++I)
			{
				const int32 X = Team == 0 ? 1 + I % 2 : Grid.Count.X - 2 - I % 2;
				const int32 Y = 6 + I * 4;
				PlaceUnit(Grid, Teams[Team][I], FIntPoint(X, Y + (X + Y) % 2));
				if (Team == 1)
				{
					Teams[Team][I]->Tags.AddUnique(TEXT("Enemy"));
				}
			}
		}
		Grid.Scenario->RerunConstructionScripts();
		if (!UEditorLoadingAndSavingUtils::SaveMap(World, Package))
		{
			Notify(TEXT("Не удалось сохранить уровень ") + Package, false);
			return FString();
		}
		Register(Package, Title);
		Notify(FString::Printf(TEXT("Создан уровень «%s» (%s). Он уже есть в быстрых боях и кампании."), *Title, *Package));
		return Package;
	}

	void NewLevel()
	{
		const FString Title = AskTitle();
		if (!Title.IsEmpty())
		{
			CreateLevel(Title);
		}
	}

	void Fill()
	{
		FKeepersLevelGrid Grid;
		if (!FindGrid(Grid))
		{
			return;
		}
		const FScopedTransaction Transaction(LOCTEXT("Fill", "Залить поле гексами"));
		const int32 Spawned = FillField(EditorWorld(), Grid);
		Notify(FString::Printf(TEXT("Добавлено гексов: %d."), Spawned));
	}

	void Raise(int32 Levels)
	{
		FKeepersLevelGrid Grid;
		const float Step = KeepersLevelKit::PrepareGrid(EditorWorld(), Grid) ? Grid.TileSize.Z : 100.f;
		const FScopedTransaction Transaction(Levels > 0 ? LOCTEXT("Raise", "Поднять на уровень") : LOCTEXT("Lower", "Опустить на уровень"));
		for (AActor* Actor : Selected())
		{
			Actor->Modify();
			Actor->AddActorWorldOffset(FVector(0.f, 0.f, Step * Levels));
		}
	}

	void Snap()
	{
		FKeepersLevelGrid Grid;
		if (!FindGrid(Grid))
		{
			return;
		}
		const FScopedTransaction Transaction(LOCTEXT("Snap", "Привязать к клеткам"));
		int32 Snapped = 0;
		for (AActor* Actor : Selected())
		{
			FIntPoint Index;
			if (!KeepersLevelKit::TileIndex(Grid, Actor->GetActorLocation(), Index))
			{
				continue;
			}
			if (IsUnit(Actor))
			{
				PlaceUnit(Grid, Actor, Index);
				++Snapped;
				continue;
			}
			// Into the cell centre, its top exactly on a height level.
			FVector Origin, Extent;
			Actor->GetActorBounds(true, Origin, Extent);
			const FVector Cell = KeepersLevelKit::TileLocation(Grid, Index);
			const float Top = Origin.Z + Extent.Z;
			const float Level = FMath::RoundToFloat((Top - Cell.Z) / Grid.TileSize.Z);
			Actor->Modify();
			Actor->AddActorWorldOffset(FVector(Cell.X - Origin.X, Cell.Y - Origin.Y, Cell.Z + Level * Grid.TileSize.Z - Top));
			++Snapped;
		}
		Notify(FString::Printf(TEXT("Привязано: %d."), Snapped));
	}

	void Paint(const TCHAR* Material)
	{
		const FScopedTransaction Transaction(LOCTEXT("Paint", "Покрасить верх"));
		UMaterialInterface* Top = LoadObject<UMaterialInterface>(nullptr, Material);
		for (AActor* Actor : Selected())
		{
			if (AStaticMeshActor* Mesh = Cast<AStaticMeshActor>(Actor))
			{
				Mesh->GetStaticMeshComponent()->Modify();
				Mesh->GetStaticMeshComponent()->SetMaterial(0, Top);
			}
		}
	}

	void ClearMarks()
	{
		if (UWorld* World = EditorWorld())
		{
			FlushPersistentDebugLines(World);
		}
	}

	/** Checks the level the way the grid sees it when combat starts, and marks the cells in the viewport. */
	FString CheckReport()
	{
		FKeepersLevelGrid Grid;
		if (!FindGrid(Grid))
		{
			return FString();
		}
		UWorld* World = EditorWorld();
		struct FTile
		{
			uint8 Type = 0;
			FVector Ground = FVector::ZeroVector;
		};
		TMap<FIntPoint, FTile> Tiles;
		int32 Counts[6] = {};
		for (const FIntPoint& Index : KeepersLevelKit::AllIndexes(Grid))
		{
			FTile Tile;
			Tile.Type = KeepersLevelKit::TraceTile(Grid, Index, Tile.Ground);
			++Counts[FMath::Min<int32>(Tile.Type, 5)];
			if (Tile.Type != 0)
			{
				Tiles.Add(Index, Tile);
			}
		}
		auto Walkable = [](uint8 Type) { return Type == 1 || Type == 3 || Type == 4; };

		TArray<FString> Problems;
		const TArray<TArray<AActor*>> Teams = KeepersLevelKit::GetTeams(Grid);
		const FKeepersLevelRow* Row = KeepersLevelKit::FindRow(World->GetOutermost()->GetFName());
		const bool bFromPlacement = Row && Row->bStartsFromPlacement;
		TSet<AActor*> InTeams;
		TMap<FIntPoint, AActor*> Occupied;
		TArray<FIntPoint> PlayerCells;
		TArray<TPair<AActor*, FIntPoint>> EnemyCells;
		for (AActor* Unit : KeepersLevelKit::GetLevelUnits(World))
		{
			int32 Team = INDEX_NONE;
			for (int32 T = 0; T < Teams.Num(); ++T)
			{
				Team = Teams[T].Contains(Unit) ? T : Team;
			}
			if (Team == INDEX_NONE)
			{
				if (!bFromPlacement)
				{
					Problems.Add(FString::Printf(TEXT("%s не входит ни в одну команду сценария (UnitsData → UnitTeams)."), *Unit->GetActorLabel()));
					continue;
				}
				Team = Unit->ActorHasTag(TEXT("Enemy")) ? 1 : 0;
			}
			FIntPoint Index;
			const FTile* Tile = KeepersLevelKit::TileIndex(Grid, Unit->GetActorLocation(), Index) ? Tiles.Find(Index) : nullptr;
			if (!Tile)
			{
				Problems.Add(FString::Printf(TEXT("%s стоит не на клетке поля."), *Unit->GetActorLabel()));
				continue;
			}
			if (!Walkable(Tile->Type))
			{
				Problems.Add(FString::Printf(TEXT("%s стоит на непроходимой клетке (%d, %d)."), *Unit->GetActorLabel(), Index.X, Index.Y));
			}
			if (AActor** Other = Occupied.Find(Index))
			{
				Problems.Add(FString::Printf(TEXT("%s и %s на одной клетке (%d, %d)."), *Unit->GetActorLabel(), *(*Other)->GetActorLabel(), Index.X, Index.Y));
			}
			Occupied.Add(Index, Unit);
			if (Team == 0)
			{
				PlayerCells.Add(Index);
			}
			else
			{
				EnemyCells.Emplace(Unit, Index);
			}
		}
		if (PlayerCells.Num() == 0)
		{
			Problems.Add(TEXT("Нет героев игрока на поле."));
		}
		if (EnemyCells.Num() == 0)
		{
			Problems.Add(TEXT("Нет врагов на поле (враги — вторая команда сценария или юниты с тегом Enemy)."));
		}

		// Where the heroes can walk: neighbours with a step of at most one level.
		TSet<FIntPoint> Reached;
		TArray<FIntPoint> Queue = PlayerCells;
		Reached.Append(PlayerCells);
		while (Queue.Num() > 0)
		{
			const FIntPoint Index = Queue.Pop();
			const FTile& From = Tiles.FindChecked(Index);
			for (const FIntPoint& Next : KeepersLevelKit::Neighbors(Grid, Index))
			{
				const FTile* To = Tiles.Find(Next);
				if (To && Walkable(To->Type) && !Reached.Contains(Next) && FMath::Abs(To->Ground.Z - From.Ground.Z) <= Grid.TileSize.Z + 1.f)
				{
					Reached.Add(Next);
					Queue.Add(Next);
				}
			}
		}
		int32 Cut = 0;
		for (const TPair<FIntPoint, FTile>& Pair : Tiles)
		{
			Cut += Walkable(Pair.Value.Type) && !Reached.Contains(Pair.Key) ? 1 : 0;
		}
		for (const TPair<AActor*, FIntPoint>& Enemy : EnemyCells)
		{
			if (PlayerCells.Num() > 0 && !Reached.Contains(Enemy.Value))
			{
				Problems.Add(FString::Printf(TEXT("До %s герои не дойдут пешком (обрыв выше уровня или вода)."), *Enemy.Key->GetActorLabel()));
			}
		}
		if (Cut > 0)
		{
			Problems.Add(FString::Printf(TEXT("%d проходимых клеток (жёлтые) героям недоступны: к ним нет подъёма в один уровень. Если это задуманная скала — всё хорошо."), Cut));
		}
		if (!Row)
		{
			Problems.Add(TEXT("Уровня нет в таблице уровней: «Хранители → Добавить уровень в игру», чтобы он появился в меню и кампании."));
		}

		// Marks: green — walkable, yellow — walkable but cut off, red — obstacle, blue — only flyers.
		FlushPersistentDebugLines(World);
		// Hexagons: flat sides face the neighbours along Y, corners point along X.
		const float Radius = Grid.TileSize.X * (Grid.IsHex() ? 0.53f : 0.46f);
		for (const TPair<FIntPoint, FTile>& Pair : Tiles)
		{
			const uint8 Type = Pair.Value.Type;
			const FColor Color = Type == 2 ? FColor::Red : Type == 5 ? FColor(60, 140, 255)
				: Reached.Contains(Pair.Key) ? FColor(40, 220, 60) : FColor::Yellow;
			const FVector Centre = Pair.Value.Ground + FVector(0.f, 0.f, 6.f);
			const int32 Sides = Grid.IsHex() ? 6 : 4;
			for (int32 Edge = 0; Edge < Sides; ++Edge)
			{
				const float A = FMath::DegreesToRadians(360.f / Sides * Edge + (Grid.IsHex() ? 0.f : 45.f));
				const float B = FMath::DegreesToRadians(360.f / Sides * (Edge + 1) + (Grid.IsHex() ? 0.f : 45.f));
				DrawDebugLine(World, Centre + FVector(FMath::Cos(A), FMath::Sin(A), 0.f) * Radius, Centre + FVector(FMath::Cos(B), FMath::Sin(B), 0.f) * Radius, Color, true, -1.f, 0, 4.f);
			}
		}

		FString Report = FString::Printf(TEXT("Клеток поля: %d. Обычных: %d, медленных (×2/×3): %d, препятствий: %d, только для летающих: %d.\n")
			TEXT("Героев: %d, врагов: %d. Отрезанных проходимых клеток: %d.\n\n"),
			Tiles.Num(), Counts[1], Counts[3] + Counts[4], Counts[2], Counts[5], PlayerCells.Num(), EnemyCells.Num(), Cut);
		Report += Problems.Num() == 0 ? TEXT("Всё в порядке: уровень готов к бою.") : TEXT("Что поправить:\n• ") + FString::Join(Problems, TEXT("\n• "));
		Report += TEXT("\n\nРазметка в окне: зелёный — проходимо, жёлтый — не дойти, красный — препятствие, синий — только летающим. Убрать: «Хранители → Убрать разметку».");
		return Report;
	}

	void Check()
	{
		const FString Report = CheckReport();
		if (!Report.IsEmpty())
		{
			FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(Report), FText::FromString(TEXT("Проверка уровня")));
		}
	}

	void AddToGame()
	{
		UWorld* World = EditorWorld();
		const FString Package = World ? World->GetOutermost()->GetName() : FString();
		if (Package.IsEmpty() || Package.StartsWith(TEXT("/Temp/")))
		{
			Notify(TEXT("Сначала сохраните уровень."), false);
			return;
		}
		Notify(Register(Package, FPackageName::GetShortName(Package))
			? TEXT("Уровень добавлен в игру. Название и описание — в таблице уровней.")
			: TEXT("Уровень уже есть в таблице уровней."));
	}

	void OpenTable()
	{
		UDataTable* Table = TableForEdit();
		if (Table->GetRowMap().Num() == 0)
		{
			SaveTable(Table);
		}
		GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAsset(Table);
	}

	void FillMenu(UToolMenu* Menu)
	{
		auto Add = [](FToolMenuSection& Section, const TCHAR* Name, const TCHAR* Label, const TCHAR* Tip, TFunction<void()> Run)
		{
			Section.AddMenuEntry(Name, FText::FromString(Label), FText::FromString(Tip), FSlateIcon(),
				FUIAction(FExecuteAction::CreateLambda([Run]() { Run(); })));
		};
		FToolMenuSection& Level = Menu->AddSection(TEXT("KeepersLevel"), LOCTEXT("LevelSection", "Уровень"));
		Add(Level, TEXT("KeepersNewLevel"), TEXT("Новый уровень боя…"), TEXT("Уровень из шаблона: сетка, бой, свет, поле гексов, отряды по краям. Сразу попадает в быстрые бои и кампанию."), &NewLevel);
		Add(Level, TEXT("KeepersCheck"), TEXT("Проверить уровень"), TEXT("Как увидит поле игра: клетки, высоты, проходы, где стоят отряды. Размечает клетки цветом."), &Check);
		Add(Level, TEXT("KeepersClearMarks"), TEXT("Убрать разметку"), TEXT("Стереть цветные клетки проверки."), &ClearMarks);
		Add(Level, TEXT("KeepersAddToGame"), TEXT("Добавить уровень в игру"), TEXT("Записать открытый уровень в таблицу уровней: быстрые бои, кампания, экран загрузки."), &AddToGame);
		Add(Level, TEXT("KeepersOpenTable"), TEXT("Таблица уровней"), TEXT("Названия, описания, картинки загрузки, где используется уровень."), &OpenTable);

		FToolMenuSection& Field = Menu->AddSection(TEXT("KeepersField"), LOCTEXT("FieldSection", "Поле"));
		Add(Field, TEXT("KeepersFill"), TEXT("Залить поле гексами"), TEXT("Поставить гекс на каждую пустую клетку сетки (уровень 0). Лишние удалить — будут ямы и вода."), &Fill);
		Add(Field, TEXT("KeepersRaise"), TEXT("Поднять выделенное на уровень"), TEXT("+100 см: холм. Разница больше одного уровня — обрыв."), []() { Raise(1); });
		Add(Field, TEXT("KeepersLower"), TEXT("Опустить выделенное на уровень"), TEXT("−100 см."), []() { Raise(-1); });
		Add(Field, TEXT("KeepersSnap"), TEXT("Привязать выделенное к клеткам"), TEXT("В центр клетки, верх ровно на уровень высоты; юнитов — на землю клетки."), &Snap);
		Add(Field, TEXT("KeepersPaintA"), TEXT("Покрасить верх: клетка 1"), TEXT("Узор MI_Hexagon_Checker_01."), []() { Paint(TEXT("/Game/Art/Materials/ColorsWithTexture/MI_Hexagon_Checker_01.MI_Hexagon_Checker_01")); });
		Add(Field, TEXT("KeepersPaintB"), TEXT("Покрасить верх: клетка 2"), TEXT("Узор MI_Hexagon_Checker_02."), []() { Paint(TEXT("/Game/Art/Materials/ColorsWithTexture/MI_Hexagon_Checker_02.MI_Hexagon_Checker_02")); });
		Add(Field, TEXT("KeepersPaintC"), TEXT("Покрасить верх: клетка 3"), TEXT("Узор MI_Hexagon_Checker_03."), []() { Paint(TEXT("/Game/Art/Materials/ColorsWithTexture/MI_Hexagon_Checker_03.MI_Hexagon_Checker_03")); });
	}

	void RegisterMenus()
	{
		UToolMenus* Menus = UToolMenus::Get();
		// Main menu: "Хранители".
		UToolMenu* MainMenu = Menus->ExtendMenu(TEXT("LevelEditor.MainMenu"));
		MainMenu->AddSubMenu(TEXT("KeepersOwner"), NAME_None, TEXT("Keepers"), LOCTEXT("MenuLabel", "Хранители"),
			LOCTEXT("MenuTip", "Конструктор уровней «Хранителей земель»"));
		FillMenu(Menus->RegisterMenu(TEXT("LevelEditor.MainMenu.Keepers"), NAME_None, EMultiBoxType::Menu, false));

		// Toolbar: the same menu and quick buttons for heights.
		UToolMenu* Toolbar = Menus->ExtendMenu(TEXT("LevelEditor.LevelEditorToolBar.User"));
		FToolMenuSection& Section = Toolbar->FindOrAddSection(TEXT("Keepers"));
		Section.AddEntry(FToolMenuEntry::InitComboButton(TEXT("KeepersCombo"), FUIAction(),
			FNewToolMenuDelegate::CreateStatic(&FillMenu), LOCTEXT("ToolbarLabel", "Хранители"), LOCTEXT("MenuTip", "Конструктор уровней «Хранителей земель»"),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), TEXT("LevelEditor.Tabs.Viewports"))));
		Section.AddEntry(FToolMenuEntry::InitToolBarButton(TEXT("KeepersRaiseButton"), FUIAction(FExecuteAction::CreateLambda([]() { Raise(1); })),
			LOCTEXT("RaiseLabel", "▲"), LOCTEXT("RaiseTip", "Поднять выделенное на уровень (+100 см)")));
		Section.AddEntry(FToolMenuEntry::InitToolBarButton(TEXT("KeepersLowerButton"), FUIAction(FExecuteAction::CreateLambda([]() { Raise(-1); })),
			LOCTEXT("LowerLabel", "▼"), LOCTEXT("LowerTip", "Опустить выделенное на уровень (−100 см)")));
		Section.AddEntry(FToolMenuEntry::InitToolBarButton(TEXT("KeepersCheckButton"), FUIAction(FExecuteAction::CreateStatic(&Check)),
			LOCTEXT("CheckLabel", "Проверить"), LOCTEXT("CheckTip", "Проверить уровень боя")));
	}
}

class FKeepersDevToolsModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateLambda([]()
		{
			FToolMenuOwnerScoped Owner(TEXT("KeepersDevTools"));
			KeepersLevelTools::RegisterMenus();
		}));
	}

	virtual void ShutdownModule() override
	{
		if (UToolMenus* Menus = UToolMenus::TryGet())
		{
			Menus->UnregisterOwner(TEXT("KeepersDevTools"));
		}
	}
};

IMPLEMENT_MODULE(FKeepersDevToolsModule, KeepersDevTools)

#undef LOCTEXT_NAMESPACE
