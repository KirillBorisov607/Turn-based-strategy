#include "KeepersStyle.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Engine/Font.h"
#include "Engine/Texture2D.h"
#include "Sound/SoundBase.h"
#include "Styling/CoreStyle.h"

namespace KeepersStyle
{
	FSlateFontInfo Heading(int32 Size)
	{
		static TWeakObjectPtr<UFont> CachedFont;
		if (!CachedFont.IsValid())
		{
			CachedFont = LoadObject<UFont>(nullptr, TEXT("/Game/Art/fonts/DearType_-_Lifehack_Sans_Bold_Font.DearType_-_Lifehack_Sans_Bold_Font"));
		}
		if (UFont* Font = CachedFont.Get())
		{
			const FCompositeFont* Composite = Font->GetCompositeFont();
			const FName Typeface = Composite && Composite->DefaultTypeface.Fonts.Num() > 0 ? Composite->DefaultTypeface.Fonts[0].Name : NAME_None;
			return FSlateFontInfo(Font, Size, Typeface);
		}
		return FCoreStyle::GetDefaultFontStyle("Bold", Size);
	}

	FSlateFontInfo Body(int32 Size, bool bBold)
	{
		return FCoreStyle::GetDefaultFontStyle(bBold ? "Bold" : "Regular", Size);
	}

	FSlateBrush Fade(const FLinearColor& Color, bool bOpaqueOnLeft)
	{
		// One small white ramp texture, tinted per brush.
		static UTexture2D* Ramp = nullptr;
		if (!Ramp)
		{
			constexpr int32 Width = 128;
			Ramp = UTexture2D::CreateTransient(Width, 1, PF_B8G8R8A8);
			Ramp->Filter = TF_Bilinear;
			Ramp->AddressX = TA_Clamp;
			FColor* Pixels = static_cast<FColor*>(Ramp->GetPlatformData()->Mips[0].BulkData.Lock(LOCK_READ_WRITE));
			for (int32 X = 0; X < Width; ++X)
			{
				const float T = static_cast<float>(X) / (Width - 1);
				Pixels[X] = FColor(255, 255, 255, static_cast<uint8>(255.f * FMath::Pow(1.f - T, 1.6f)));
			}
			Ramp->GetPlatformData()->Mips[0].BulkData.Unlock();
			Ramp->UpdateResource();
			Ramp->AddToRoot();
		}
		FSlateBrush Brush;
		Brush.SetResourceObject(Ramp);
		Brush.TintColor = FSlateColor(Color);
		Brush.DrawAs = ESlateBrushDrawType::Image;
		Brush.ImageSize = FVector2D(128.f, 1.f);
		if (!bOpaqueOnLeft)
		{
			Brush.Mirroring = ESlateBrushMirrorType::Horizontal;
		}
		return Brush;
	}

	FSlateBrush Panel(const FLinearColor& Fill, float Radius, const FLinearColor& OutlineColor, float OutlineWidth)
	{
		FSlateBrush Brush;
		Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
		Brush.TintColor = FSlateColor(Fill);
		Brush.OutlineSettings = FSlateBrushOutlineSettings(FVector4(Radius, Radius, Radius, Radius), FSlateColor(OutlineColor), OutlineColor.A > 0.f ? OutlineWidth : 0.f);
		return Brush;
	}

	static FSlateSound Sound(const TCHAR* Path)
	{
		FSlateSound Result;
		if (USoundBase* Asset = LoadObject<USoundBase>(nullptr, Path))
		{
			Result.SetResourceObject(Asset);
		}
		return Result;
	}

	FSlateSound HoverSound()
	{
		return Sound(TEXT("/Game/UltimateMenu/Sounds/Sound_MenuHover_Cue.Sound_MenuHover_Cue"));
	}

	FSlateSound ClickSound()
	{
		return Sound(TEXT("/Game/UltimateMenu/Sounds/Sound_MenuClick_Cue.Sound_MenuClick_Cue"));
	}

	FButtonStyle ButtonStyle(EButton Kind)
	{
		FLinearColor Fill = InkLight;
		FLinearColor Edge = GoldDim;
		switch (Kind)
		{
		case EButton::Primary: Fill = FLinearColor(0.62f, 0.40f, 0.10f, 0.95f); Edge = Gold; break;
		case EButton::Secondary: break;
		case EButton::Danger: Fill = FLinearColor(0.32f, 0.06f, 0.05f, 0.92f); Edge = FLinearColor(0.9f, 0.3f, 0.2f, 0.6f); break;
		}
		FButtonStyle Style;
		Style.Normal = Panel(Fill, 8.f, Edge);
		FLinearColor Lit = Fill * 1.45f;
		Lit.A = Fill.A;
		Style.Hovered = Panel(Lit, 8.f, Gold, 2.f);
		FLinearColor Dark = Fill * 0.75f;
		Dark.A = Fill.A;
		Style.Pressed = Panel(Dark, 8.f, Gold, 2.f);
		FLinearColor Muted = Fill * 0.5f;
		Muted.A = 0.7f;
		Style.Disabled = Panel(Muted, 8.f, FLinearColor(0.4f, 0.4f, 0.4f, 0.3f));
		Style.NormalPadding = FMargin(18.f, 9.f);
		Style.PressedPadding = FMargin(18.f, 10.f, 18.f, 8.f);
		Style.HoveredSlateSound = HoverSound();
		Style.PressedSlateSound = ClickSound();
		return Style;
	}

	UTextBlock* MakeText(UWidgetTree* Tree, const FText& Content, const FSlateFontInfo& Font, const FLinearColor& Color, bool bWrap)
	{
		UTextBlock* Block = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		Block->SetFont(Font);
		Block->SetColorAndOpacity(FSlateColor(Color));
		Block->SetText(Content);
		Block->SetAutoWrapText(bWrap);
		return Block;
	}

	UButton* MakeButton(UWidgetTree* Tree, const FText& Label, EButton Kind, int32 FontSize)
	{
		UButton* Button = Tree->ConstructWidget<UButton>(UButton::StaticClass());
		Button->SetStyle(ButtonStyle(Kind));
		UTextBlock* Caption = MakeText(Tree, Label, Heading(FontSize), Kind == EButton::Primary ? FLinearColor(1.f, 0.97f, 0.9f) : Text);
		Caption->SetJustification(ETextJustify::Center);
		Button->AddChild(Caption);
		return Button;
	}
}
