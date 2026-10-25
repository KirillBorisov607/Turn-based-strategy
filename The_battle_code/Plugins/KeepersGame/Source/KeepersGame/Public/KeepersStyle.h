#pragma once

#include "CoreMinimal.h"
#include "Fonts/SlateFontInfo.h"
#include "Styling/SlateBrush.h"
#include "Styling/SlateTypes.h"

class UButton;
class UTextBlock;
class UWidgetTree;

/**
 * One look for every Keepers screen (main menu, battle HUD, tutorial, campaign), in the spirit of the
 * Ultimate Menu System reference: dark glassy panels with a thin gold edge, upper-case headings in the
 * project's DearType Sans font, Roboto for reading text, and the pack's hover/click sounds.
 */
namespace KeepersStyle
{
	// Palette.
	inline const FLinearColor Ink(0.010f, 0.013f, 0.020f, 0.90f);
	inline const FLinearColor InkLight(0.035f, 0.045f, 0.065f, 0.92f);
	inline const FLinearColor Gold(1.f, 0.74f, 0.30f);
	inline const FLinearColor GoldDim(1.f, 0.74f, 0.30f, 0.35f);
	inline const FLinearColor Text(0.95f, 0.95f, 0.93f);
	inline const FLinearColor TextDim(0.62f, 0.65f, 0.70f);
	inline const FLinearColor Ally(0.16f, 0.42f, 0.85f);
	inline const FLinearColor Enemy(0.78f, 0.16f, 0.12f);
	inline const FLinearColor Good(0.30f, 0.62f, 0.36f);

	enum class EButton : uint8
	{
		Primary,   // gold: the main action of a screen
		Secondary, // dark with a gold edge
		Danger     // dark red
	};

	/** Upper-case display font (DearType Lifehack Sans Bold), falls back to Roboto Bold. */
	FSlateFontInfo Heading(int32 Size);

	/** Reading font (Roboto). */
	FSlateFontInfo Body(int32 Size, bool bBold = false);

	/** Horizontal fade from Color (left) to transparent (right), or the other way round. */
	FSlateBrush Fade(const FLinearColor& Color, bool bOpaqueOnLeft = true);

	/** Rounded panel; a visible edge when OutlineColor has alpha. */
	FSlateBrush Panel(const FLinearColor& Fill = Ink, float Radius = 10.f, const FLinearColor& OutlineColor = GoldDim, float OutlineWidth = 1.5f);

	FButtonStyle ButtonStyle(EButton Kind);

	/** The pack's UI sounds. */
	FSlateSound HoverSound();
	FSlateSound ClickSound();

	UTextBlock* MakeText(UWidgetTree* Tree, const FText& Content, const FSlateFontInfo& Font, const FLinearColor& Color = KeepersStyle::Text, bool bWrap = false);

	/** A styled button with a centred caption. */
	UButton* MakeButton(UWidgetTree* Tree, const FText& Label, EButton Kind, int32 FontSize = 18);
}
