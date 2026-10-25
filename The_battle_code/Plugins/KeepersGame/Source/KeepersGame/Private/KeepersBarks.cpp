#include "KeepersBarks.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/TextBlock.h"
#include "Components/WidgetComponent.h"
#include "Engine/World.h"
#include "KeepersReflection.h"
#include "KeepersSettings.h"

namespace KeepersBarks
{
	/** E_UnitType order: Warrior, Ranger, Priest, Slime, Chicken, Bat. */
	enum EUnitType : uint8 { Warrior, Ranger, Priest, Slime, Chicken, Bat, Count };

	using FLines = TArray<const TCHAR*>;

	static const FLines& LinesFor(uint8 UnitType, EKeepersBarkSituation Situation)
	{
		static const FLines None;
		static const TMap<uint8, TMap<EKeepersBarkSituation, FLines>> Table = {
			{ Slime, {
				{ EKeepersBarkSituation::PlansAttack, { TEXT("Ща как плюну!"), TEXT("Буль. Это угроза."), TEXT("Я липкий и опасный!"), TEXT("Иду обниматься!") } },
				{ EKeepersBarkSituation::Pushed, { TEXT("Я не так хотел течь!"), TEXT("Шлёп!") } },
				{ EKeepersBarkSituation::Bumped, { TEXT("Буль... больно."), TEXT("Сплющило!") } },
				{ EKeepersBarkSituation::Falls, { TEXT("Бульк-бульк-бульк..."), TEXT("Я ещё вернусь лужей!") } },
			} },
			{ Chicken, {
				{ EKeepersBarkSituation::PlansAttack, { TEXT("Ко-ко-конец тебе!"), TEXT("Клюну в коленку!"), TEXT("Я позову своих!") } },
				{ EKeepersBarkSituation::Pushed, { TEXT("Ко-ко-КУДА?!"), TEXT("Я лечу! Я правда лечу!") } },
				{ EKeepersBarkSituation::Bumped, { TEXT("Ко... ой."), TEXT("Перья! Мои перья!") } },
				{ EKeepersBarkSituation::Falls, { TEXT("Куры не летают! Не летают!"), TEXT("Ко-ко-кошма-а-ар!") } },
			} },
			{ Bat, {
				{ EKeepersBarkSituation::PlansAttack, { TEXT("Летать — не жульничество!"), TEXT("Сверху виднее!"), TEXT("Пи-пи-пи! (злобно)") } },
				{ EKeepersBarkSituation::Pushed, { TEXT("Сквозняк!"), TEXT("Кто открыл окно?!") } },
				{ EKeepersBarkSituation::Bumped, { TEXT("Эхолокация подвела..."), TEXT("Пи!") } },
			} },
			{ Warrior, {
				{ EKeepersBarkSituation::RaisesRock, { TEXT("Стоять, камень!"), TEXT("Земля, подержи!"), TEXT("За этой скалой не страшно. Почти.") } },
				{ EKeepersBarkSituation::PlansAttack, { TEXT("Щит наготове!"), TEXT("Только не куры...") } },
				{ EKeepersBarkSituation::Pushed, { TEXT("Эй, я тут стоял!") } },
				{ EKeepersBarkSituation::Bumped, { TEXT("Уф! Доспехи выдержат.") } },
			} },
			{ Ranger, {
				{ EKeepersBarkSituation::PlansAttack, { TEXT("Ветер в мою пользу."), TEXT("Сначала стреляю, потом думаю.") } },
				{ EKeepersBarkSituation::Pushed, { TEXT("Это мой приём вообще-то!") } },
				{ EKeepersBarkSituation::Bumped, { TEXT("Ай! Колчан помял!") } },
			} },
			{ Priest, {
				{ EKeepersBarkSituation::GrowsForest, { TEXT("Расти, малыш!"), TEXT("Простите, что вырос прямо тут."), TEXT("Немного зелени никому не повредит.") } },
				{ EKeepersBarkSituation::PlansAttack, { TEXT("Извините заранее!") } },
				{ EKeepersBarkSituation::Pushed, { TEXT("Ой, простите, это я виноват?") } },
				{ EKeepersBarkSituation::Bumped, { TEXT("Ничего, заживёт!") } },
			} },
		};
		const TMap<EKeepersBarkSituation, FLines>* ByType = Table.Find(UnitType);
		const FLines* Lines = ByType ? ByType->Find(Situation) : nullptr;
		return Lines ? *Lines : None;
	}
}

void UKeepersBarkWidget::SetText(const FText& Text)
{
	PendingText = Text;
	if (Label)
	{
		Label->SetText(Text);
	}
}

TSharedRef<SWidget> UKeepersBarkWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		UBorder* Box = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Box"));
		Box->SetBrushColor(FLinearColor(0.02f, 0.03f, 0.05f, 0.82f));
		Box->SetPadding(FMargin(12.f, 6.f));
		FSlateBrush Brush = Box->Background;
		Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
		Brush.OutlineSettings = FSlateBrushOutlineSettings(FVector4(10.f, 10.f, 10.f, 10.f));
		Box->SetBrush(Brush);

		Label = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Label"));
		FSlateFontInfo Font = Label->GetFont();
		Font.Size = 16;
		Label->SetFont(Font);
		Label->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		Label->SetText(PendingText);
		Box->SetContent(Label);
		WidgetTree->RootWidget = Box;
	}
	return Super::RebuildWidget();
}

AKeepersBarkActor::AKeepersBarkActor()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Bubble = CreateDefaultSubobject<UWidgetComponent>(TEXT("Bubble"));
	Bubble->SetupAttachment(RootComponent);
	Bubble->SetWidgetSpace(EWidgetSpace::Screen);
	Bubble->SetDrawAtDesiredSize(true);
	Bubble->SetWidgetClass(UKeepersBarkWidget::StaticClass());
	Bubble->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void AKeepersBarkActor::Show(AActor* Speaker, const FText& Text)
{
	SpeakerActor = Speaker;
	Age = 0.f;
	Bubble->InitWidget();
	if (UKeepersBarkWidget* Widget = Cast<UKeepersBarkWidget>(Bubble->GetUserWidgetObject()))
	{
		Widget->SetText(Text);
	}
	if (Speaker)
	{
		SetActorLocation(Speaker->GetActorLocation() + FVector(0.f, 0.f, HeightAboveUnit));
	}
}

void AKeepersBarkActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Age += DeltaSeconds;
	if (const AActor* Speaker = SpeakerActor.Get())
	{
		// Follow the unit and drift up a little.
		SetActorLocation(Speaker->GetActorLocation() + FVector(0.f, 0.f, HeightAboveUnit + Age * 15.f));
	}
	if (UUserWidget* Widget = Bubble->GetUserWidgetObject())
	{
		const float FadeStart = Lifetime - 0.5f;
		Widget->SetRenderOpacity(Age < FadeStart ? 1.f : FMath::Clamp((Lifetime - Age) / 0.5f, 0.f, 1.f));
	}
	if (Age >= Lifetime)
	{
		Destroy();
	}
}

void UKeepersBarkLibrary::Bark(AActor* Unit, const FText& Text)
{
	UWorld* World = Unit ? Unit->GetWorld() : nullptr;
	// The player can turn the heroes' lines off in the game settings.
	if (!World || !World->IsGameWorld() || Text.IsEmpty() || KeepersSettings::Get(KeepersSettings::Barks) < 0.5f)
	{
		return;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	if (AKeepersBarkActor* BarkActor = World->SpawnActor<AKeepersBarkActor>(Unit->GetActorLocation(), FRotator::ZeroRotator, Params))
	{
		BarkActor->Show(Unit, Text);
	}
}

void UKeepersBarkLibrary::BarkAbout(AActor* Unit, EKeepersBarkSituation Situation, float Chance)
{
	if (!Unit || FMath::FRand() > Chance)
	{
		return;
	}
	uint8 UnitType = KeepersBarks::Count;
	FProperty* Property = nullptr;
	void* TypePtr = KeepersReflection::FindValuePtr(Unit->GetClass(), Unit, TEXT("UnitType"), &Property);
	if (const FByteProperty* ByteProperty = CastField<FByteProperty>(Property))
	{
		UnitType = ByteProperty->GetPropertyValue(TypePtr);
	}
	const KeepersBarks::FLines& Lines = KeepersBarks::LinesFor(UnitType, Situation);
	if (Lines.Num() > 0)
	{
		Bark(Unit, FText::FromString(Lines[FMath::RandRange(0, Lines.Num() - 1)]));
	}
}
