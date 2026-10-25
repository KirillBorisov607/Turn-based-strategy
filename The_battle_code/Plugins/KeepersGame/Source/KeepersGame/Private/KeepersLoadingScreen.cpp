#include "KeepersLoadingScreen.h"

#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "KeepersLevelKit.h"
#include "KeepersMapGenerator.h"
#include "KeepersStyle.h"
#include "MoviePlayer.h"
#include "Styling/CoreStyle.h"
#include "TimerManager.h"
#include "UObject/UObjectGlobals.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Images/SThrobber.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"
#include <atomic>

namespace KeepersLoading
{
	struct FPlace
	{
		const TCHAR* Level;
		const TCHAR* Kicker;
		const TCHAR* Title;
		const TCHAR* Subtitle;
	};

	static const FPlace Places[] =
	{
		{ TEXT("L_MainMenu"), TEXT("привал"), TEXT("Хранители земель"), TEXT("Костёр горит, Хранители ждут нового похода.") },
		{ TEXT("L_CampaignMap"), TEXT("регион I"), TEXT("Зелёные долы"), TEXT("Холмы, мосты и подозрительно много кур. Выберите путь.") },
		{ TEXT("L_Slice_Hex"), TEXT("Зелёные долы · бой"), TEXT("Переправа"), TEXT("Слизни перекрыли мосты через ручей. Пробейтесь на тот берег.") },
		{ TEXT("L_Slice_Chicken"), TEXT("Зелёные долы · защита"), TEXT("Курятник Пеструшки"), TEXT("Пеструшка носится по полю. Продержитесь пять ходов и не дайте её съесть.") },
		{ TEXT("L_Combat_FairDuel"), TEXT("Зелёные долы · испытание"), TEXT("Честная дуэль"), TEXT("Три Хранителя против трёх таких же. Кто лучше использует местность?") },
		{ TEXT("L_Combat_SoSlimy"), TEXT("Зелёные долы · бой"), TEXT("Слишком скользко"), TEXT("Слизни оставляют лужи и делятся надвое. Не дайте им заполнить поле.") },
		{ TEXT("L_Combat_ChickenHunt"), TEXT("Зелёные долы · бой"), TEXT("Охота на кур"), TEXT("Куры зовут подмогу каждые два хода. Действуйте быстро.") },
		{ TEXT("L_Combat_FlyingIsCheating"), TEXT("Зелёные долы · босс"), TEXT("Летать — нечестно"), TEXT("Летучие мыши перелетают пропасти. Прижмите их скалами.") },
		{ TEXT("L_Combat_TooManyEnemies"), TEXT("Зелёные долы · элита"), TEXT("Слишком много врагов"), TEXT("Врагов втрое больше. Узкие проходы — ваши друзья.") },
		{ TEXT("L_Combat_TablesHaveTurned"), TEXT("Зелёные долы · элита"), TEXT("Всё наоборот"), TEXT("Сегодня в меньшинстве не вы. Не расслабляйтесь.") },
		{ TEXT("L_Combat_FourPlayersChess"), TEXT("Зелёные долы · испытание"), TEXT("Шахматы на четверых"), TEXT("Четыре отряда на одной доске. Пусть враги дерутся друг с другом.") },
		{ TEXT("L_Debug"), TEXT("мастерская"), TEXT("Полигон"), TEXT("Всё поле и все способности под рукой.") },
	};

	static const TCHAR* Tips[] =
	{
		TEXT("Скала закрывает линию видимости: прячьтесь за ней от стрел."),
		TEXT("Порыв ветра сталкивает врагов в воду и пропасти."),
		TEXT("Красные диски — туда враг ударит в начале своего хода. Уйдите с них."),
		TEXT("Лес вдвое замедляет, а дерево закрывает обзор лучникам."),
		TEXT("Щёлкните по другому герою, чтобы передать ход ему."),
		TEXT("Погибший слизень делится на двух маленьких."),
		TEXT("Куры зовут подмогу. Не тяните с ними."),
		TEXT("Корни держат врага на месте весь его следующий ход."),
		TEXT("Esc в бою — пауза, настройки и выход."),
		TEXT("Наряд и цвет героев меняются в «Отряде» главного меню."),
	};

	static const FPlace& FindPlace(FName Level)
	{
		FString Name = Level.ToString();
		int32 Slash;
		if (Name.FindLastChar(TEXT('/'), Slash))
		{
			Name.RightChopInline(Slash + 1);
		}
		for (const FPlace& Place : Places)
		{
			if (Name == Place.Level)
			{
				return Place;
			}
		}
		return Places[0];
	}

	/** Vertical ramp: transparent at the top, opaque at the bottom. */
	static UTexture2D* VerticalRamp()
	{
		static UTexture2D* Ramp = nullptr;
		if (!Ramp)
		{
			constexpr int32 Height = 128;
			Ramp = UTexture2D::CreateTransient(1, Height, PF_B8G8R8A8);
			Ramp->Filter = TF_Bilinear;
			Ramp->AddressY = TA_Clamp;
			FColor* Pixels = static_cast<FColor*>(Ramp->GetPlatformData()->Mips[0].BulkData.Lock(LOCK_READ_WRITE));
			for (int32 Y = 0; Y < Height; ++Y)
			{
				const float T = static_cast<float>(Y) / (Height - 1);
				Pixels[Y] = FColor(255, 255, 255, static_cast<uint8>(255.f * FMath::Pow(T, 1.4f)));
			}
			Ramp->GetPlatformData()->Mips[0].BulkData.Unlock();
			Ramp->UpdateResource();
			Ramp->AddToRoot();
		}
		return Ramp;
	}

	void OpenLevel(const UObject* WorldContext, FName Level)
	{
		if (UKeepersLoadingSubsystem* Loading = UKeepersLoadingSubsystem::Get(WorldContext))
		{
			Loading->OpenLevel(Level);
		}
		else if (WorldContext)
		{
			UGameplayStatics::OpenLevel(WorldContext, Level);
		}
	}
}

/** Brushes and fade state shared by the widget's lambdas (also read by the movie player's thread). */
struct UKeepersLoadingSubsystem::FView
{
	FSlateBrush Picture;
	FSlateBrush Bottom;
	FSlateBrush Left;
	FSlateBrush Solid;
	std::atomic<float> Alpha{ 1.f };
	double Start = 0.0;
};

UKeepersLoadingSubsystem* UKeepersLoadingSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	return GameInstance ? GameInstance->GetSubsystem<UKeepersLoadingSubsystem>() : nullptr;
}

void UKeepersLoadingSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	FCoreUObjectDelegates::PreLoadMap.AddUObject(this, &UKeepersLoadingSubsystem::HandlePreLoadMap);
	FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &UKeepersLoadingSubsystem::HandlePostLoadMap);
}

void UKeepersLoadingSubsystem::Deinitialize()
{
	FCoreUObjectDelegates::PreLoadMap.RemoveAll(this);
	FCoreUObjectDelegates::PostLoadMapWithWorld.RemoveAll(this);
	if (TickHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
		TickHandle.Reset();
	}
	Hide();
	Super::Deinitialize();
}

TSharedRef<SWidget> UKeepersLoadingSubsystem::BuildWidget(FName Level)
{
	KeepersLoading::FPlace Place = KeepersLoading::FindPlace(Level);
	// A generated field is named after its biome.
	static FString GeneratedTitle;
	static FString GeneratedSubtitle;
	EKeepersBiome Biome;
	if (Level.ToString().Contains(TEXT("L_Generated")))
	{
		const bool bKnown = UKeepersMapGenerator::GetPendingBiome(Biome);
		GeneratedTitle = bKnown ? UKeepersMapGenerator::BiomeName(Biome).ToString() : FString(TEXT("Неизведанная земля"));
		GeneratedSubtitle = bKnown ? UKeepersMapGenerator::BiomeBlurb(Biome).ToString() : FString(TEXT("Никто ещё не видел этих мест."));
		Place = { TEXT("L_Generated"), TEXT("неизведанная земля"), *GeneratedTitle, *GeneratedSubtitle };
	}
	// A level of the level kit: its row names it.
	static FString KitLevel;
	const FKeepersLevelRow* KitRow = KeepersLevelKit::FindRow(Level);
	if (KitRow)
	{
		KitLevel = FPackageName::GetShortName(KitRow->Level.ToSoftObjectPath().GetLongPackageName());
		GeneratedTitle = KitRow->Title.IsEmpty() ? KitLevel : KitRow->Title.ToString();
		GeneratedSubtitle = KitRow->Subtitle.ToString();
		Place = { *KitLevel, KitRow->Kind == EKeepersLevelKind::Elite ? TEXT("опасный бой") : TEXT("бой"), *GeneratedTitle, *GeneratedSubtitle };
	}
	const TCHAR* Tip = KeepersLoading::Tips[TipIndex % UE_ARRAY_COUNT(KeepersLoading::Tips)];

	// Picture of the place: T_Load_<level>, or the common one.
	Background = KitRow && !KitRow->LoadingImage.IsNull() ? KitRow->LoadingImage.LoadSynchronous()
		: LoadObject<UTexture2D>(nullptr, *FString::Printf(TEXT("/Game/Keepers/Loading/T_Load_%s.T_Load_%s"), Place.Level, Place.Level), nullptr, LOAD_NoWarn | LOAD_Quiet);
	if (!Background)
	{
		Background = LoadObject<UTexture2D>(nullptr, TEXT("/Game/Keepers/Loading/T_Load_Default.T_Load_Default"));
	}

	TSharedPtr<FView> NewView = MakeShared<FView>();
	NewView->Start = FPlatformTime::Seconds();
	if (Background)
	{
		NewView->Picture.SetResourceObject(Background);
		NewView->Picture.ImageSize = FVector2D(Background->GetSizeX(), Background->GetSizeY());
		NewView->Picture.DrawAs = ESlateBrushDrawType::Image;
	}
	else
	{
		NewView->Picture = KeepersStyle::Panel(FLinearColor(0.03f, 0.05f, 0.08f), 0.f, FLinearColor::Transparent);
	}
	NewView->Bottom.SetResourceObject(KeepersLoading::VerticalRamp());
	NewView->Bottom.TintColor = FSlateColor(FLinearColor(0.f, 0.f, 0.01f, 0.95f));
	NewView->Bottom.DrawAs = ESlateBrushDrawType::Image;
	NewView->Left = KeepersStyle::Fade(FLinearColor(0.f, 0.f, 0.01f, 0.7f));
	NewView->Solid = *FCoreStyle::Get().GetBrush("WhiteBrush");
	View = NewView;

	const FLinearColor Gold = KeepersStyle::Gold;
	TSharedPtr<FView> V = NewView;
	auto Faded = [V](const FLinearColor& Color) { return FSlateColor(FLinearColor(Color.R, Color.G, Color.B, Color.A * V->Alpha.load())); };

	return SNew(SBorder)
		.BorderImage(&NewView->Solid)
		.BorderBackgroundColor_Lambda([Faded]() { return Faded(FLinearColor::Black); })
		.ColorAndOpacity_Lambda([V]() { return FLinearColor(1.f, 1.f, 1.f, V->Alpha.load()); })
		.Padding(0.f)
		[
			SNew(SOverlay)
			// The picture, slowly coming closer.
			+ SOverlay::Slot()
			[
				SNew(SScaleBox)
				.Stretch(EStretch::ScaleToFill)
				[
					SNew(SImage)
					.Image(&NewView->Picture)
					.RenderTransformPivot(FVector2D(0.5f, 0.5f))
					.RenderTransform_Lambda([V]()
					{
						const float T = static_cast<float>(FPlatformTime::Seconds() - V->Start);
						return FSlateRenderTransform(1.f + 0.06f * FMath::Min(T / 10.f, 1.f));
					})
				]
			]
			// Dark at the bottom and on the left, where the words are.
			+ SOverlay::Slot()
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().FillHeight(0.45f)[SNullWidget::NullWidget]
				+ SVerticalBox::Slot().FillHeight(0.55f)[SNew(SImage).Image(&NewView->Bottom)]
			]
			+ SOverlay::Slot()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(0.6f)[SNew(SImage).Image(&NewView->Left)]
				+ SHorizontalBox::Slot().FillWidth(0.4f)[SNullWidget::NullWidget]
			]
			// Cinema bars with a thin gold edge.
			+ SOverlay::Slot()
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()[SNew(SBox).HeightOverride(52.f)[SNew(SImage).Image(&NewView->Solid).ColorAndOpacity(FLinearColor::Black)]]
				+ SVerticalBox::Slot().AutoHeight()[SNew(SBox).HeightOverride(2.f)[SNew(SImage).Image(&NewView->Solid).ColorAndOpacity(FLinearColor(Gold.R, Gold.G, Gold.B, 0.5f))]]
				+ SVerticalBox::Slot().FillHeight(1.f)[SNullWidget::NullWidget]
				+ SVerticalBox::Slot().AutoHeight()[SNew(SBox).HeightOverride(2.f)[SNew(SImage).Image(&NewView->Solid).ColorAndOpacity(FLinearColor(Gold.R, Gold.G, Gold.B, 0.5f))]]
				+ SVerticalBox::Slot().AutoHeight()[SNew(SBox).HeightOverride(52.f)[SNew(SImage).Image(&NewView->Solid).ColorAndOpacity(FLinearColor::Black)]]
			]
			// The place and a tip, bottom left.
			+ SOverlay::Slot()
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Bottom)
			.Padding(FMargin(96.f, 0.f, 0.f, 96.f))
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(STextBlock).Text(FText::FromString(Place.Kicker)).Font(KeepersStyle::Heading(20)).ColorAndOpacity(Gold)
				]
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(STextBlock).Text(FText::FromString(Place.Title)).Font(KeepersStyle::Heading(76)).ColorAndOpacity(KeepersStyle::Text)
					.ShadowOffset(FVector2D(0.f, 3.f)).ShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.6f))
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 2.f, 0.f, 12.f))
				[
					SNew(SBox).WidthOverride(420.f).HeightOverride(3.f)[SNew(SImage).Image(&NewView->Solid).ColorAndOpacity(Gold)]
				]
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(SBox).WidthOverride(860.f)
					[
						SNew(STextBlock).Text(FText::FromString(Place.Subtitle)).Font(KeepersStyle::Body(19)).ColorAndOpacity(FLinearColor(0.88f, 0.89f, 0.9f)).AutoWrapText(true)
					]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 26.f, 0.f, 0.f))
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
					[
						SNew(SBorder)
						.BorderImage(&NewView->Solid)
						.BorderBackgroundColor(FLinearColor(Gold.R, Gold.G, Gold.B, 0.18f))
						.Padding(FMargin(10.f, 3.f))
						[
							SNew(STextBlock).Text(FText::FromString(TEXT("Совет"))).Font(KeepersStyle::Heading(16)).ColorAndOpacity(Gold)
						]
					]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(14.f, 0.f, 0.f, 0.f))
					[
						SNew(STextBlock).Text(FText::FromString(Tip)).Font(KeepersStyle::Body(15)).ColorAndOpacity(FLinearColor(0.8f, 0.82f, 0.85f))
					]
				]
			]
			// Spinner, bottom right.
			+ SOverlay::Slot()
			.HAlign(HAlign_Right)
			.VAlign(VAlign_Bottom)
			.Padding(FMargin(0.f, 0.f, 90.f, 100.f))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[
					SNew(SCircularThrobber).Radius(14.f).ColorAndOpacity(Gold)
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(14.f, 0.f, 0.f, 0.f))
				[
					SNew(STextBlock).Text(FText::FromString(TEXT("Загрузка"))).Font(KeepersStyle::Heading(20)).ColorAndOpacity(KeepersStyle::Text)
				]
			]
		];
}

void UKeepersLoadingSubsystem::Show(FName Level)
{
	Hide();
	UGameViewportClient* Viewport = GetGameInstance() ? GetGameInstance()->GetGameViewportClient() : nullptr;
	if (!Viewport)
	{
		return;
	}
	Widget = BuildWidget(Level);
	Viewport->AddViewportWidgetContent(Widget.ToSharedRef(), 10000);
}

void UKeepersLoadingSubsystem::Hide()
{
	if (Widget.IsValid())
	{
		if (UGameViewportClient* Viewport = GetGameInstance() ? GetGameInstance()->GetGameViewportClient() : nullptr)
		{
			Viewport->RemoveViewportWidgetContent(Widget.ToSharedRef());
		}
		Widget.Reset();
	}
}

void UKeepersLoadingSubsystem::OpenLevel(FName Level)
{
	if (bTravelling)
	{
		return;
	}
	bTravelling = true;
	PendingLevel = Level;
	TipIndex = FMath::Rand();
	Show(Level);
	// Let the screen be drawn once before the level change blocks the game thread.
	TWeakObjectPtr<UKeepersLoadingSubsystem> WeakThis(this);
	FTimerHandle Handle;
	GetGameInstance()->GetTimerManager().SetTimer(Handle, FTimerDelegate::CreateLambda([WeakThis, Level]()
	{
		if (WeakThis.IsValid())
		{
			UGameplayStatics::OpenLevel(WeakThis->GetGameInstance()->GetWorld(), Level);
		}
	}), 0.15f, false);
}

void UKeepersLoadingSubsystem::Preview(FName Level, float Seconds)
{
	TipIndex = FMath::Rand();
	Show(Level);
	HoldTime = Seconds;
	if (!TickHandle.IsValid())
	{
		TickHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &UKeepersLoadingSubsystem::TickFade));
	}
}

void UKeepersLoadingSubsystem::PreviewLoadingScreen(const UObject* WorldContextObject, FName Level, float Seconds)
{
	if (UKeepersLoadingSubsystem* Loading = Get(WorldContextObject))
	{
		Loading->Preview(Level, Seconds);
	}
}

void UKeepersLoadingSubsystem::HandlePreLoadMap(const FString& MapName)
{
	// Packaged game: the movie player keeps the screen alive (and the spinner turning) during the load.
	if (bTravelling && !GIsEditor && IsMoviePlayerEnabled())
	{
		FLoadingScreenAttributes Attributes;
		Attributes.bAutoCompleteWhenLoadingCompletes = true;
		Attributes.MinimumLoadingScreenDisplayTime = 1.2f;
		Attributes.WidgetLoadingScreen = BuildWidget(PendingLevel);
		GetMoviePlayer()->SetupLoadingScreen(Attributes);
	}
}

void UKeepersLoadingSubsystem::HandlePostLoadMap(UWorld* World)
{
	if (!bTravelling)
	{
		return;
	}
	bTravelling = false;
	// The level change removed the screen; put it back and fade it out over the new level.
	Show(PendingLevel);
	HoldTime = 0.6f;
	if (!TickHandle.IsValid())
	{
		TickHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &UKeepersLoadingSubsystem::TickFade));
	}
}

bool UKeepersLoadingSubsystem::TickFade(float DeltaTime)
{
	if (!View.IsValid() || !Widget.IsValid())
	{
		TickHandle.Reset();
		return false;
	}
	if (HoldTime > 0.f)
	{
		HoldTime -= DeltaTime;
		return true;
	}
	const float Alpha = View->Alpha.load() - DeltaTime / 0.7f;
	View->Alpha = FMath::Max(Alpha, 0.f);
	if (Alpha <= 0.f)
	{
		Hide();
		TickHandle.Reset();
		return false;
	}
	return true;
}
