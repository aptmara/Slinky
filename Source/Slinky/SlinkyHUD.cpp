#include "SlinkyHUD.h"
#include "SlinkyActor.h"
#include "SlinkyGameMode.h"
#include "SlinkyStaircase.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	// GetSmallFont()'s old flat-1x calls now go through ComboFont at this scale instead - YuseiMagic
	// bakes noticeably larger glyphs than the engine's tiny debug font, so every former "small font"
	// line needs shrinking down to roughly its old on-screen size.
	constexpr float SmallFontScale = 0.5f;
}

ASlinkyHUD::ASlinkyHUD()
{
	static ConstructorHelpers::FObjectFinder<UFont> ComboFontFinder(
		TEXT("/Game/YuseiMagic-Regular_Font.YuseiMagic-Regular_Font"));
	if (ComboFontFinder.Succeeded())
	{
		ComboFont = ComboFontFinder.Object;
	}
}

void ASlinkyHUD::DrawHUD()
{
	Super::DrawHUD();

	ASlinkyActor* Slinky = nullptr;
	for (TActorIterator<ASlinkyActor> It(GetWorld()); It; ++It)
	{
		Slinky = *It;
		break;
	}
	if (!Slinky || !Canvas || !GEngine)
	{
		return;
	}

	// Every line on this HUD now goes through the same rounded, hand-lettered display font (falling
	// back to the engine defaults only if the asset is ever missing) so the whole readout - not just
	// the combo banner - reads as one consistent, poppy style rather than debug-font labels sitting
	// next to a comic-book combo counter.
	UFont* const DisplayFont = ComboFont ? ComboFont.Get() : GEngine->GetLargeFont();
	UFont* const SmallDisplayFont = ComboFont ? ComboFont.Get() : GEngine->GetSmallFont();
	const float SmallScale = ComboFont ? SmallFontScale : 1.0f;

	// Bounces above 1x on every landed step and springs back down (see ASlinkyActor::RegisterCombo/
	// SpringToward) instead of drawing at a flat scale, so each step reads as a little "pop".
	const float StepScale = Slinky->GetStepPopScale();
	const FText CountText = FText::FromString(FString::Printf(TEXT("%d steps"), Slinky->GetStepCount()));
	Canvas->SetDrawColor(FColor(232, 235, 238));
	Canvas->DrawText(DisplayFont, CountText, 44.0f, 38.0f, StepScale, StepScale);

	DrawComboBanner(Slinky);

	Canvas->SetDrawColor(FColor(182, 188, 194));
	Canvas->DrawText(SmallDisplayFont, FText::FromString(
		TEXT("drag the slinky  /  R to restart  /  arrows: depth,rise  PgUp/PgDn: riser  Tab+[ ]: coil")),
		46.0f, 88.0f, SmallScale, SmallScale);

	ASlinkyStaircase* Staircase = nullptr;
	for (TActorIterator<ASlinkyStaircase> It(GetWorld()); It; ++It)
	{
		Staircase = *It;
		break;
	}
	if (Staircase)
	{
		Canvas->DrawText(SmallDisplayFont, FText::FromString(FString::Printf(
			TEXT("depth %.0f  rise %.0f  riser %.0f"),
			Staircase->StepDepth, Staircase->StepRise, Staircase->RiserThickness)), 46.0f, 106.0f,
			SmallScale, SmallScale);
	}

	Canvas->SetDrawColor(FColor(150, 210, 190));
	Canvas->DrawText(SmallDisplayFont, FText::FromString(
		FString::Printf(TEXT("coil: %s"), *Slinky->GetTuningParamDisplay())), 46.0f, 124.0f,
		SmallScale, SmallScale);

	if (const ASlinkyGameMode* SlinkyGameMode = GetWorld()->GetAuthGameMode<ASlinkyGameMode>())
	{
		Canvas->SetDrawColor(FColor(255, 176, 122));
		Canvas->DrawText(SmallDisplayFont, FText::FromString(FString::Printf(
			TEXT("depth %.1fm"), SlinkyGameMode->GetCurrentDepthMeters())), 46.0f, 142.0f,
			SmallScale, SmallScale);
	}

	// Debug readout: proves on screen whether the view the renderer actually uses is tracking the
	// coil. GetPlayerViewPoint is the exact value the local player renders from, so if these two
	// lines move together the camera is following and any missing geometry is a render issue; if
	// the view line stays frozen while the coil line changes, the camera itself is not updating.
	if (APlayerController* PC = GetOwningPlayerController())
	{
		FVector ViewLocation;
		FRotator ViewRotation;
		PC->GetPlayerViewPoint(ViewLocation, ViewRotation);

		const FVector CoilLocation = Slinky->GetCenterLocation();
		Canvas->SetDrawColor(FColor(255, 214, 120));
		Canvas->DrawText(SmallDisplayFont, FText::FromString(FString::Printf(
			TEXT("view  X %.0f  Z %.0f"), ViewLocation.X, ViewLocation.Z)), 46.0f, 166.0f,
			SmallScale, SmallScale);
		Canvas->DrawText(SmallDisplayFont, FText::FromString(FString::Printf(
			TEXT("coil  X %.0f  Z %.0f"), CoilLocation.X, CoilLocation.Z)), 46.0f, 190.0f,
			SmallScale, SmallScale);
	}
}

void ASlinkyHUD::DrawComboBanner(const ASlinkyActor* Slinky)
{
	const float ScreenW = static_cast<float>(Canvas->SizeX);
	const float ScreenH = static_cast<float>(Canvas->SizeY);
	const float CenterX = ScreenW * 0.5f;

	const bool bComboActive = Slinky->GetComboCount() >= 2;
	if (bComboActive)
	{
		const FLinearColor ComboColorLinear = Slinky->GetComboColor();
		const int32 ComboCount = Slinky->GetComboCount();

		// Grows with the streak itself (capped) instead of a single flat size, so a long combo keeps
		// visibly getting bolder rather than sitting as a small, easy-to-miss readout. No background
		// dressing at all here - every bit of "excitement" has to come from the letters themselves.
		const float GrowthScale = 1.0f + FMath::Min(ComboCount * 0.035f, 1.6f);
		// PopScale already overshoots past 1 on every landing (see ASlinkyActor::SpringToward) - not
		// clamped down here, so that overshoot reads as a real "thump" on the number instead of being
		// smoothed away.
		const float PopScale = Slinky->GetComboPopScale();
		// The label ("COMBO"/"NICE COMBO"/...) stays a supporting element; the number is the star of
		// the show - a huge "hit counter" look instead of one evenly-sized line of text.
		const float LabelScale = 1.6f * GrowthScale;
		const float NumberScale = FMath::Max(PopScale, 0.1f) * 5.5f * GrowthScale;

		const float LabelY = ScreenH * 0.24f;
		const FVector2D LabelSize = DrawWobblyText(Slinky->GetComboLabel(), CenterX, LabelY, LabelScale,
			ComboColorLinear, 3.0f, 0.0f, false);

		// The number itself: bold white fill with a thick outline in the combo's own color, so it
		// reads as a distinct "hit marker" popping out in front of the label rather than more of the
		// same text at a bigger size.
		const float NumberY = LabelY + LabelSize.Y * 0.5f + 26.0f;
		const FString NumberText = FString::Printf(TEXT("x%d"), ComboCount);
		const FVector2D NumberSize = DrawWobblyText(NumberText, CenterX, NumberY, NumberScale,
			FLinearColor(1.0f, 1.0f, 1.0f, 1.0f), 4.0f, 0.9f, false, ComboColorLinear);

		// Empties out left-to-right as ComboTimeRemaining runs down, giving a visible countdown to
		// when the streak will drop instead of it just vanishing without warning. The only non-text
		// element left, kept small and purely functional rather than decorative.
		const float GaugeWidth = FMath::Min(220.0f * GrowthScale, ScreenW * 0.5f);
		constexpr float GaugeHeight = 8.0f;
		const float GaugeX = CenterX - GaugeWidth * 0.5f;
		const float GaugeY = NumberY + NumberSize.Y * 0.5f + 20.0f;
		DrawRect(FLinearColor(0.05f, 0.05f, 0.07f, 0.6f), GaugeX, GaugeY, GaugeWidth, GaugeHeight);
		const float Fill = Slinky->GetComboWindowRemaining01();
		if (Fill > 0.0f)
		{
			DrawRect(ComboColorLinear, GaugeX, GaugeY, GaugeWidth * Fill, GaugeHeight);
		}
	}

	const FString& Milestone = Slinky->GetMilestoneText();
	const float MilestoneAlpha = Slinky->GetMilestoneAlpha01();
	if (!Milestone.IsEmpty() && MilestoneAlpha > 0.0f)
	{
		const FLinearColor ComboColorLinear = Slinky->GetComboColor();
		const float Shake = Slinky->GetMilestoneShake();

		// Huge and briefly dominant on its own merits (size, overshoot, rainbow, shadow) - no dark
		// backing band or radiating lines needed to make it read as an event.
		const float MilestoneScale = (4.2f + 2.6f * MilestoneAlpha) * (1.0f + 0.35f * Shake);
		const float BannerCenterY = ScreenH * 0.42f;

		FLinearColor TextColor = ComboColorLinear;
		TextColor.A = FMath::Clamp(MilestoneAlpha, 0.0f, 1.0f);

		// Rainbow cycling only for the milestone banner - the everyday combo counter stays one solid
		// color so the rare, extra-loud rainbow treatment keeps reading as special.
		DrawWobblyText(Milestone, CenterX, BannerCenterY, MilestoneScale, TextColor,
			10.0f + Shake * 14.0f, 1.7f, true, FLinearColor(0.02f, 0.02f, 0.03f, TextColor.A));
	}
}

FVector2D ASlinkyHUD::DrawWobblyText(const FString& Text, float CenterX, float Y, float Scale,
	const FLinearColor& Color, float WobbleAmount, float WobblePhase, bool bRainbow,
	const FLinearColor& OutlineColor) const
{
	if (Text.IsEmpty() || !Canvas || !GEngine)
	{
		return FVector2D::ZeroVector;
	}

	UFont* const Font = ComboFont ? ComboFont.Get() : GEngine->GetLargeFont();
	const double Time = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;

	// Measured glyph-by-glyph (rather than assuming a monospace font) so letters sit flush against
	// each other despite each being drawn as its own DrawText call.
	TArray<float> GlyphWidths;
	GlyphWidths.Reserve(Text.Len());
	float TotalWidth = 0.0f;
	float TextHeight = 0.0f;
	for (int32 Index = 0; Index < Text.Len(); ++Index)
	{
		float GlyphWidth, GlyphHeight;
		Canvas->TextSize(Font, Text.Mid(Index, 1), GlyphWidth, GlyphHeight, Scale, Scale);
		GlyphWidths.Add(GlyphWidth);
		TotalWidth += GlyphWidth;
		TextHeight = FMath::Max(TextHeight, GlyphHeight);
	}

	// The aggressive, combo-count-scaled sizes above can otherwise run a long escalated label (e.g.
	// "BONKERS COMBO x87") off both screen edges - shrink uniformly to fit and remeasure once at the
	// corrected scale rather than guessing an exact fit factor up front.
	const float MaxWidth = static_cast<float>(Canvas->SizeX) * 0.94f;
	if (TotalWidth > MaxWidth && TotalWidth > 0.0f)
	{
		const float FitFactor = MaxWidth / TotalWidth;
		Scale *= FitFactor;
		GlyphWidths.Reset();
		TotalWidth = 0.0f;
		TextHeight = 0.0f;
		for (int32 Index = 0; Index < Text.Len(); ++Index)
		{
			float GlyphWidth, GlyphHeight;
			Canvas->TextSize(Font, Text.Mid(Index, 1), GlyphWidth, GlyphHeight, Scale, Scale);
			GlyphWidths.Add(GlyphWidth);
			TotalWidth += GlyphWidth;
			TextHeight = FMath::Max(TextHeight, GlyphHeight);
		}
	}

	// Drawing the glyph repeatedly in a ring around itself fakes a solid outline (Canvas text has no
	// native stroke) - a cheap comic-book "inked" look that keeps text readable over the bright, busy
	// stair backdrop. The ring's reach scales with glyph size so a huge combo number still reads with
	// a bold, thick stroke instead of a hairline that gets proportionally thinner as text grows.
	const float OutlineReach = FMath::Clamp(2.0f + Scale * 1.3f, 2.0f, 16.0f);
	static const FVector2D UnitOffsets[] = {
		{-1.0f, -1.0f}, {1.0f, -1.0f}, {-1.0f, 1.0f}, {1.0f, 1.0f},
		{0.0f, -1.0f}, {0.0f, 1.0f}, {-1.0f, 0.0f}, {1.0f, 0.0f},
		{-0.7f, -0.7f}, {0.7f, -0.7f}, {-0.7f, 0.7f}, {0.7f, 0.7f},
	};
	FColor OutlineFColor = OutlineColor.ToFColor(true);
	OutlineFColor.A = static_cast<uint8>(FMath::Clamp(Color.A, 0.0f, 1.0f) * 255.0f);

	// A single hard drop shadow, offset down-right and scaled with the glyph, so the text reads as
	// "popping off the screen" on its own - no background art needed to sell the depth.
	const float ShadowOffset = FMath::Clamp(Scale * 0.9f, 3.0f, 22.0f);
	FColor ShadowFColor(4, 3, 6);
	ShadowFColor.A = static_cast<uint8>(FMath::Clamp(Color.A, 0.0f, 1.0f) * 200.0f);

	float PenX = CenterX - TotalWidth * 0.5f;
	for (int32 Index = 0; Index < Text.Len(); ++Index)
	{
		const FString Glyph = Text.Mid(Index, 1);
		// Each letter bounces slightly out of phase with its neighbors - a "marquee wobble" instead
		// of the whole word moving as one rigid block.
		const float Bounce = FMath::Sin(Time * 7.0f + WobblePhase + Index * 0.85f) * WobbleAmount;
		const float GlyphY = Y + Bounce;

		if (!Glyph.Equals(TEXT(" ")))
		{
			Canvas->SetDrawColor(ShadowFColor);
			Canvas->DrawText(Font, Glyph, PenX + ShadowOffset, GlyphY + ShadowOffset, Scale, Scale);

			Canvas->SetDrawColor(OutlineFColor);
			for (const FVector2D& Unit : UnitOffsets)
			{
				Canvas->DrawText(Font, Glyph, PenX + Unit.X * OutlineReach, GlyphY + Unit.Y * OutlineReach,
					Scale, Scale);
			}

			FLinearColor GlyphColor = Color;
			if (bRainbow)
			{
				const float Hue = FMath::Fmod(static_cast<float>(Time) * 110.0f + Index * 32.0f, 360.0f);
				GlyphColor = FLinearColor::MakeFromHSV8(static_cast<uint8>(Hue / 360.0f * 255.0f), 210, 255);
				GlyphColor.A = Color.A;
			}
			Canvas->SetDrawColor(GlyphColor.ToFColor(true));
			Canvas->DrawText(Font, Glyph, PenX, GlyphY, Scale, Scale);
		}

		PenX += GlyphWidths[Index];
	}

	return FVector2D(TotalWidth, TextHeight);
}
