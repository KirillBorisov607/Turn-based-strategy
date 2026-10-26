#include "KeepersStory.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "KeepersCampaignUI.h"
#include "KeepersHeroes.h"
#include "KeepersJuice.h"
#include "KeepersStyle.h"

namespace KeepersStory
{
	static FKeepersLine L(const TCHAR* Speaker, const TCHAR* Text)
	{
		return { FName(Speaker), Text };
	}

	TArray<FKeepersLine> RegionIntro(int32 Region)
	{
		switch (Region)
		{
		case 0:
			return {
				L(TEXT("Narrator"), TEXT("Из святилища пропали шесть Камней земель. Без них земли «поплыли»: болота солонеют, лес шепчет гадости, а куры стали подозрительно организованными.")),
				L(TEXT("Hero1"), TEXT("Камни сами не уходят. Кто-то их унёс — и оставил на пороге куриные следы.")),
				L(TEXT("Hero2"), TEXT("Куры? Серьёзно? Они же… куры.")),
				L(TEXT("Hero3"), TEXT("Вот именно. Никто их не подозревает. Первый камень где-то в Зелёных долах — у слизней.")),
			};
		case 1:
			return {
				L(TEXT("Narrator"), TEXT("Первый камень снова в святилище. Но куриные следы ведут дальше — в Шепчущий лес.")),
				L(TEXT("Hero2"), TEXT("Деревья тут разговаривают. И, кажется, обсуждают мою причёску.")),
				L(TEXT("Hero3"), TEXT("Это не деревья, а шаманы кур. Они подгоняют лес, чтобы он запирал тропы.")),
				L(TEXT("Hero1"), TEXT("Значит, ищем того, кто шепчет громче всех. Говорят, это Старый Пень.")),
			};
		case 2:
			return {
				L(TEXT("Narrator"), TEXT("Второй камень у Хранителей. Тропа уходит в Солёные болота: туман, топь и запах подгоревшей каши.")),
				L(TEXT("Witch"), TEXT("Хранители? Наконец-то. Болото солонеет с тех пор, как пропали камни. Я знаю, кто их собирает.")),
				L(TEXT("Hero1"), TEXT("Кто?")),
				L(TEXT("Witch"), TEXT("Сначала Болотная Мать. Она стережёт камень и не любит гостей. И не стойте в трясине — затянет.")),
			};
		case 3:
			return {
				L(TEXT("Witch"), TEXT("Камни собирает Генерал Клюв. По приказу того, кого куры зовут Великим Петухом.")),
				L(TEXT("Hero2"), TEXT("А зачем Петуху камни?")),
				L(TEXT("Witch"), TEXT("Куры боятся перемен. Хотят «заморозить» земли, чтобы всё было как вчера. Навсегда.")),
				L(TEXT("Narrator"), TEXT("Путь к Генералу лежит через Ледяной перевал. Лёд скользкий, а всадники кур быстрые.")),
			};
		case 4:
			return {
				L(TEXT("Narrator"), TEXT("Йети-вождь отдал камень. С перевала виден Город кур: стены, баррикады, тараны.")),
				L(TEXT("Hero1"), TEXT("Никогда не думал, что буду штурмовать курятник.")),
				L(TEXT("Beak"), TEXT("Ко-ко-кто посмел? Камни принадлежат Великому Петуху! Тараны — вперёд!")),
				L(TEXT("Hero3"), TEXT("У них катапульты. Валуны остаются на поле скалами — используем их как укрытие.")),
			};
		default:
			return {
				L(TEXT("Narrator"), TEXT("Генерал Клюв бежал. Пять камней вернулись; шестой — в Сердце земель, где земля плывёт под ногами.")),
				L(TEXT("Rooster"), TEXT("Хранители… Когда-то я был одним из вас. Камни показали мне, как легко всё рушится. Я лишь хочу, чтобы ничего не менялось.")),
				L(TEXT("Hero2"), TEXT("Он… был Хранителем?")),
				L(TEXT("Hero1"), TEXT("Был. Теперь он Великий Петух. И последний камень без боя не отдаст.")),
			};
		}
	}

	TArray<FKeepersLine> Finale()
	{
		return {
			L(TEXT("Rooster"), TEXT("Ко… кончено. Камни ваши. Решайте, что с ними будет. Я однажды уже решил — и посмотрите на меня.")),
			L(TEXT("Narrator"), TEXT("Шесть Камней земель снова вместе. Они тёплые и тихо гудят. Хранителям предстоит выбор.")),
		};
	}

	TArray<FKeepersLine> Ending(EEnding Ending)
	{
		switch (Ending)
		{
		case EEnding::Keep:
			return {
				L(TEXT("Narrator"), TEXT("Хранители оставляют камни себе. Земли слушаются каждого их слова.")),
				L(TEXT("Hero1"), TEXT("Ведь это правильно? Мы же лучше знаем, как надо…")),
				L(TEXT("Narrator"), TEXT("Где-то вдали кукарекает Великий Петух. Кажется, он смеётся. Конец.")),
			};
		case EEnding::Pact:
			return {
				L(TEXT("Narrator"), TEXT("Хранители зовут кур за общий стол. Камни делят: у каждого края свой страж.")),
				L(TEXT("Rooster"), TEXT("Ко-ко… ладно. Перемены не так страшны, если меняться вместе.")),
				L(TEXT("Narrator"), TEXT("С тех пор над курятниками развеваются флаги Хранителей, а Курица-беженка стала советником. Конец.")),
			};
		default:
			return {
				L(TEXT("Narrator"), TEXT("Хранители возвращают камни в святилище. Земли успокаиваются: болота сладеют, лес умолкает, лёд тает к весне.")),
				L(TEXT("Rooster"), TEXT("Я… снова почти человек. Только по утрам кукарекаю. Спасибо, Хранители.")),
				L(TEXT("Narrator"), TEXT("Куры вернулись на насест. Говорят, теперь они просто куры. Говорят. Конец.")),
			};
		}
	}

	FText EndingName(EEnding Ending)
	{
		return FText::FromString(Ending == EEnding::Keep ? TEXT("Оставить камни себе") : Ending == EEnding::Pact ? TEXT("Договор с курами") : TEXT("Вернуть камни духам"));
	}

	/** Name and picture of a speaker. */
	static void Speaker(const UObject* Context, FName Id, FText& OutName, UTexture2D*& OutPicture)
	{
		OutPicture = nullptr;
		const FString Key = Id.ToString();
		if (Key.StartsWith(TEXT("Hero")))
		{
			const UKeepersPartySubsystem* Party = UKeepersPartySubsystem::Get(Context);
			const int32 Slot = FCString::Atoi(*Key.RightChop(4)) - 1;
			const TArray<FName> Squad = Party ? Party->GetParty() : TArray<FName>();
			const FName Hero = Squad.IsValidIndex(Slot) ? Squad[Slot] : FName(TEXT("Warrior"));
			const FKeepersHeroDef* Def = KeepersHeroes::Find(Hero);
			OutName = Def ? Def->Name : FText::FromName(Hero);
			OutPicture = KeepersCards::Portrait(Hero);
			return;
		}
		static const TMap<FString, TPair<const TCHAR*, const TCHAR*>> Cast =
		{
			{ TEXT("Witch"), { TEXT("Ведьма болот"), TEXT("Witch") } },
			{ TEXT("Beak"), { TEXT("Генерал Клюв"), TEXT("Rooster") } },
			{ TEXT("Rooster"), { TEXT("Великий Петух"), TEXT("Rooster") } },
		};
		if (const TPair<const TCHAR*, const TCHAR*>* Found = Cast.Find(Key))
		{
			OutName = FText::FromString(Found->Key);
			OutPicture = KeepersCards::Card(Found->Value);
			return;
		}
		OutName = FText::GetEmpty();
	}
}

void UKeepersDialogWidget::Setup(TArray<FKeepersLine> InLines, TFunction<void()> InOnFinished)
{
	Lines = MoveTemp(InLines);
	OnFinished = MoveTemp(InOnFinished);
	Current = 0;
	ShowLine();
}

TSharedRef<SWidget> UKeepersDialogWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		SetIsFocusable(true);
		UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
		WidgetTree->RootWidget = Canvas;
		// A soft veil so that the scene behind steps back.
		UBorder* Veil = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Veil->SetBrushColor(FLinearColor(0.f, 0.f, 0.f, 0.35f));
		UCanvasPanelSlot* VeilSlot = Canvas->AddChildToCanvas(Veil);
		VeilSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
		VeilSlot->SetOffsets(FMargin(0.f));

		UBorder* Box = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Box->SetBrush(KeepersStyle::Panel(KeepersStyle::Ink, 16.f));
		Box->SetPadding(FMargin(22.f, 18.f));
		UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		PortraitBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		PortraitBox->SetWidthOverride(150.f);
		PortraitBox->SetHeightOverride(150.f);
		Portrait = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
		PortraitBox->AddChild(Portrait);
		Row->AddChildToHorizontalBox(PortraitBox)->SetPadding(FMargin(0.f, 0.f, 22.f, 0.f));
		UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		NameText = KeepersStyle::MakeText(WidgetTree, FText::GetEmpty(), KeepersStyle::Heading(30), KeepersStyle::Gold);
		Column->AddChildToVerticalBox(NameText)->SetPadding(FMargin(0.f, 0.f, 0.f, 8.f));
		USizeBox* TextSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		TextSize->SetWidthOverride(860.f);
		TextSize->SetMinDesiredHeight(96.f);
		LineText = KeepersStyle::MakeText(WidgetTree, FText::GetEmpty(), KeepersStyle::Body(21), KeepersStyle::Text, true);
		TextSize->AddChild(LineText);
		Column->AddChildToVerticalBox(TextSize);
		HintText = KeepersStyle::MakeText(WidgetTree, FText::GetEmpty(), KeepersStyle::Body(13), KeepersStyle::TextDim);
		Column->AddChildToVerticalBox(HintText)->SetHorizontalAlignment(HAlign_Right);
		Row->AddChildToHorizontalBox(Column)->SetVerticalAlignment(VAlign_Center);
		Box->SetContent(Row);
		UCanvasPanelSlot* BoxSlot = Canvas->AddChildToCanvas(Box);
		BoxSlot->SetAnchors(FAnchors(0.5f, 1.f));
		BoxSlot->SetAlignment(FVector2D(0.5f, 1.f));
		BoxSlot->SetPosition(FVector2D(0.f, -60.f));
		BoxSlot->SetAutoSize(true);
		ShowLine();
	}
	return Super::RebuildWidget();
}

void UKeepersDialogWidget::ShowLine()
{
	if (!LineText || !Lines.IsValidIndex(Current))
	{
		return;
	}
	FText Name;
	UTexture2D* Picture = nullptr;
	KeepersStory::Speaker(this, Lines[Current].Speaker, Name, Picture);
	NameText->SetText(Name);
	NameText->SetVisibility(Name.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	Portrait->SetBrushFromTexture(Picture);
	PortraitBox->SetVisibility(Picture ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	// The narrator speaks in italics-like dim gold; characters in white.
	LineText->SetColorAndOpacity(FSlateColor(Name.IsEmpty() ? FLinearColor(0.95f, 0.85f, 0.6f) : KeepersStyle::Text));
	Shown = 0.f;
	LineText->SetText(FText::GetEmpty());
	HintText->SetText(FText::FromString(Current + 1 < Lines.Num() ? TEXT("щёлкните, чтобы продолжить ▸") : TEXT("щёлкните, чтобы закрыть ▸")));
}

void UKeepersDialogWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (!Lines.IsValidIndex(Current) || !LineText)
	{
		return;
	}
	const FString& Text = Lines[Current].Text;
	if (Shown < Text.Len())
	{
		Shown = FMath::Min<float>(Shown + InDeltaTime * 55.f, Text.Len());
		LineText->SetText(FText::FromString(Text.Left(FMath::FloorToInt(Shown))));
	}
}

void UKeepersDialogWidget::Advance()
{
	if (!Lines.IsValidIndex(Current))
	{
		return;
	}
	// The first click finishes the line, the next one goes on.
	if (Shown < Lines[Current].Text.Len())
	{
		Shown = Lines[Current].Text.Len();
		LineText->SetText(FText::FromString(Lines[Current].Text));
		return;
	}
	UKeepersJuiceSubsystem::Sound(this, TEXT("Click"), 0.5f);
	if (++Current < Lines.Num())
	{
		ShowLine();
		return;
	}
	RemoveFromParent();
	if (OnFinished)
	{
		TFunction<void()> Done = MoveTemp(OnFinished);
		Done();
	}
}

FReply UKeepersDialogWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	Advance();
	return FReply::Handled();
}

FReply UKeepersDialogWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::SpaceBar || InKeyEvent.GetKey() == EKeys::Enter || InKeyEvent.GetKey() == EKeys::Escape)
	{
		Advance();
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}
