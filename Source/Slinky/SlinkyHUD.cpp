#include "SlinkyHUD.h"
#include "SlinkyActor.h"
#include "SlinkyGameInstance.h"
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
	// YuseiMagic-Regular_Font is a Runtime (Slate/CompositeFont) font, so Canvas::DrawText's
	// XScale/YScale multiplier isn't re-rendering a vector outline at each call - it's stretching an
	// already-rasterized glyph bitmap cached at the font's LegacyFontSize. That size shipped as the
	// engine default of 9pt, so every Scale value in this file (tuned up to ~5x for the combo number,
	// and further overshooting past that on a spring "pop" - see ASlinkyActor::GetComboPopScale) was
	// blowing a 9pt glyph up many times past its source resolution, reading as blurry/smeared right
	// when the number popped biggest. 96pt wasn't enough headroom for that overshoot; the asset's
	// LegacyFontSize was bumped to 512pt in-editor instead, comfortably covering even the biggest
	// combo-scaled + overshot number, so every Scale here now needs the same correction (old/new) to
	// land on the same on-screen size it was actually tuned against.
	constexpr float LegacyFontSizeCorrection = 9.0f / 512.0f;
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
	// Only ComboFont needs the LegacyFontSize correction - GEngine's built-in fonts are unaffected,
	// so this is 1 whenever ComboFont failed to load and DisplayFont fell back to one of them.
	const float FontCorrection = ComboFont ? LegacyFontSizeCorrection : 1.0f;

	// Bottom-left, stacked: step count directly above the current depth, both at the same large
	// size - a matched pair of constant, easy-to-glance-at readouts rather than a small top-left
	// debug line.
	if (const ASlinkyGameMode* GameMode = Cast<ASlinkyGameMode>(GetWorld()->GetAuthGameMode()))
	{
		constexpr float Margin = 44.0f;
		const float DepthScale = 5.5f * FontCorrection;
		const FString DepthString = FString::Printf(TEXT("%.0fm"), GameMode->GetCurrentDepthMeters());
		const FString StepsString = FString::Printf(TEXT("%d steps"), Slinky->GetStepCount());

		float DepthTextWidth = 0.0f, DepthTextHeight = 0.0f;
		Canvas->TextSize(DisplayFont, DepthString, DepthTextWidth, DepthTextHeight, DepthScale, DepthScale);
		float StepsTextWidth = 0.0f, StepsTextHeight = 0.0f;
		Canvas->TextSize(DisplayFont, StepsString, StepsTextWidth, StepsTextHeight, DepthScale, DepthScale);

		const float DepthY = static_cast<float>(Canvas->SizeY) - Margin - DepthTextHeight;
		const float StepsY = DepthY - StepsTextHeight;

		Canvas->SetDrawColor(FColor(232, 235, 238));
		Canvas->DrawText(DisplayFont, FText::FromString(StepsString), Margin, StepsY, DepthScale, DepthScale);
		Canvas->DrawText(DisplayFont, FText::FromString(DepthString), Margin, DepthY, DepthScale, DepthScale);

		// Top-right, same size as the depth/steps readout: which ruleset is active, and (directly
		// below the challenge name) its countdown - only shown outside FreePlay, where every
		// customization entry point is locked (see ASlinkyGameMode::IsCustomizationLocked), so the
		// player always has an on-screen sign this run is being played under fixed rules and a
		// clock.
		if (GameMode->GetCurrentGameMode() != ESlinkyGameMode::FreePlay)
		{
			const FString ModeLabel = GameMode->GetCurrentGameMode() == ESlinkyGameMode::DailyChallenge
				? FString::Printf(TEXT("デイリー：%s"), *GameMode->GetActiveChallengeConfig().PresetName)
				: FString(TEXT("ランクに挑戦"));
			const int32 RemainingSeconds = FMath::CeilToInt(GameMode->GetChallengeTimeRemaining());
			const FString TimerString = FString::Printf(TEXT("残り %02d:%02d"), RemainingSeconds / 60, RemainingSeconds % 60);

			float ModeTextWidth = 0.0f, ModeTextHeight = 0.0f;
			Canvas->TextSize(DisplayFont, ModeLabel, ModeTextWidth, ModeTextHeight, DepthScale, DepthScale);
			float TimerTextWidth = 0.0f, TimerTextHeight = 0.0f;
			Canvas->TextSize(DisplayFont, TimerString, TimerTextWidth, TimerTextHeight, DepthScale, DepthScale);

			const float ModeX = static_cast<float>(Canvas->SizeX) - Margin - ModeTextWidth;
			const float ModeY = Margin;
			const float TimerX = static_cast<float>(Canvas->SizeX) - Margin - TimerTextWidth;
			const float TimerY = ModeY + ModeTextHeight;

			Canvas->DrawText(DisplayFont, FText::FromString(ModeLabel), ModeX, ModeY, DepthScale, DepthScale);
			Canvas->DrawText(DisplayFont, FText::FromString(TimerString), TimerX, TimerY, DepthScale, DepthScale);
		}
	}

	const USlinkyGameInstance* GameInstance = GetWorld() ? Cast<USlinkyGameInstance>(GetWorld()->GetGameInstance()) : nullptr;
	if (!GameInstance || GameInstance->IsComboDisplayEnabled())
	{
		DrawComboBanner(Slinky);
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

		// The only combo UI: a huge, screen-center "hit pop" of just the number on every landed
		// step, gone in well under a second (see ASlinkyActor::GetComboPopupAlpha01()) instead of
		// sitting on screen and blocking the view between landings. No permanent corner readout -
		// that read as leftover/duplicate UI sitting on screen the whole time the combo lasted.
		const float PopupAlpha = Slinky->GetComboPopupAlpha01();
		if (PopupAlpha > 0.0f)
		{
			const float PopupGrowth = 1.0f + FMath::Min(ComboCount * 0.035f, 1.6f);
			// PopScale already overshoots past 1 on every landing (see ASlinkyActor::SpringToward) -
			// not clamped down here, so that overshoot reads as a real "thump" on the number.
			const float NumberScale = FMath::Max(Slinky->GetComboPopScale(), 0.1f) * 5.2f * PopupGrowth;

			// Stay fully opaque for most of the pop's life and shrink sharply away only right at the
			// very end, instead of fading the alpha down the whole time - a translucent glyph reads
			// as "smeared" rather than a clean, crisp pop that's simply shrinking out of view.
			constexpr float ShrinkStartAlpha = 0.4f;
			const float ShrinkFactor = (PopupAlpha < ShrinkStartAlpha)
				? FMath::Square(PopupAlpha / ShrinkStartAlpha)
				: 1.0f;

			const FString NumberText = FString::Printf(TEXT("x%d"), ComboCount);
			DrawWobblyText(NumberText, CenterX, ScreenH * 0.3f, NumberScale * ShrinkFactor, ComboColorLinear,
				3.0f, 0.9f, false);
		}
	}

	const FString& Milestone = Slinky->GetMilestoneText();
	const float MilestoneAlpha = Slinky->GetMilestoneAlpha01();
	if (!Milestone.IsEmpty() && MilestoneAlpha > 0.0f)
	{
		const FLinearColor ComboColorLinear = Slinky->GetComboColor();
		const float Shake = Slinky->GetMilestoneShake();

		// Huge and briefly dominant on its own merits (size, overshoot, rainbow) - no dark backing
		// band or radiating lines needed to make it read as an event.
		const float MilestoneScale = (4.2f + 2.6f * MilestoneAlpha) * (1.0f + 0.35f * Shake);
		const float BannerCenterY = ScreenH * 0.42f;

		// Same "stay fully opaque, then shrink sharply away" logic as the combo number pop above -
		// fading the alpha instead would read as smeared rather than a clean, crisp banner that's
		// simply shrinking away.
		constexpr float ShrinkStartAlpha = 0.4f;
		const float ShrinkFactor = (MilestoneAlpha < ShrinkStartAlpha)
			? FMath::Square(MilestoneAlpha / ShrinkStartAlpha)
			: 1.0f;

		// Rainbow cycling only for the milestone banner - the everyday combo counter stays one solid
		// color so the rare, extra-loud rainbow treatment keeps reading as special.
		DrawWobblyText(Milestone, CenterX, BannerCenterY, MilestoneScale * ShrinkFactor, ComboColorLinear,
			10.0f + Shake * 14.0f, 1.7f, true);
	}
}

FVector2D ASlinkyHUD::DrawWobblyText(const FString& Text, float CenterX, float Y, float Scale,
	const FLinearColor& Color, float WobbleAmount, float WobblePhase, bool bRainbow) const
{
	if (Text.IsEmpty() || !Canvas || !GEngine)
	{
		return FVector2D::ZeroVector;
	}

	UFont* const Font = ComboFont ? ComboFont.Get() : GEngine->GetLargeFont();
	const double Time = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;

	// DrawScale is what actually goes to Canvas->TextSize/DrawText; Scale itself keeps meaning "how
	// big this reads on screen" (MaxWidth's fit-factor math below was tuned against that meaning) and
	// gets corrected into DrawScale here so ComboFont's bumped LegacyFontSize (see
	// LegacyFontSizeCorrection) doesn't also blow up the actual on-screen size. GEngine's built-in
	// fonts need no correction.
	const float FontCorrection = ComboFont ? LegacyFontSizeCorrection : 1.0f;
	float DrawScale = Scale * FontCorrection;
	if (ComboFont)
	{
		// Never ask Canvas to stretch the cached glyph bitmap past its own source resolution (1x) -
		// always a straight blit or a downscale, never an upscale - so even an unexpectedly large
		// Scale value can't reintroduce the blur/smear LegacyFontSizeCorrection exists to prevent.
		DrawScale = FMath::Min(DrawScale, 1.0f);
	}

	// Measured glyph-by-glyph (rather than assuming a monospace font) so letters sit flush against
	// each other despite each being drawn as its own DrawText call.
	TArray<float> GlyphWidths;
	GlyphWidths.Reserve(Text.Len());
	float TotalWidth = 0.0f;
	float TextHeight = 0.0f;
	for (int32 Index = 0; Index < Text.Len(); ++Index)
	{
		float GlyphWidth, GlyphHeight;
		Canvas->TextSize(Font, Text.Mid(Index, 1), GlyphWidth, GlyphHeight, DrawScale, DrawScale);
		GlyphWidths.Add(GlyphWidth);
		TotalWidth += GlyphWidth;
		TextHeight = FMath::Max(TextHeight, GlyphHeight);
	}

	// The aggressive, combo-count-scaled sizes above can otherwise run a long escalated label (e.g.
	// "BONKERS COMBO x87") off both screen edges - shrink uniformly to fit and remeasure once at the
	// corrected scale rather than guessing an exact fit factor up front. Applied equally to both
	// scales so Scale/DrawScale stay in the same ratio.
	const float MaxWidth = static_cast<float>(Canvas->SizeX) * 0.94f;
	if (TotalWidth > MaxWidth && TotalWidth > 0.0f)
	{
		const float FitFactor = MaxWidth / TotalWidth;
		Scale *= FitFactor;
		DrawScale *= FitFactor;
		GlyphWidths.Reset();
		TotalWidth = 0.0f;
		TextHeight = 0.0f;
		for (int32 Index = 0; Index < Text.Len(); ++Index)
		{
			float GlyphWidth, GlyphHeight;
			Canvas->TextSize(Font, Text.Mid(Index, 1), GlyphWidth, GlyphHeight, DrawScale, DrawScale);
			GlyphWidths.Add(GlyphWidth);
			TotalWidth += GlyphWidth;
			TextHeight = FMath::Max(TextHeight, GlyphHeight);
		}
	}

	// One DrawText call per glyph, nothing layered underneath - no outline ring or drop shadow copies.
	// Those extra passes read as noise (and are what broke down into smeared/overlapping glyphs) when
	// LegacyFontSizeCorrection pushes DrawScale down to keep a large-LegacyFontSize font at its
	// intended on-screen size, so a single crisp draw is both simpler and more robust.
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
			FLinearColor GlyphColor = Color;
			if (bRainbow)
			{
				const float Hue = FMath::Fmod(static_cast<float>(Time) * 110.0f + Index * 32.0f, 360.0f);
				GlyphColor = FLinearColor::MakeFromHSV8(static_cast<uint8>(Hue / 360.0f * 255.0f), 210, 255);
				GlyphColor.A = Color.A;
			}
			Canvas->SetDrawColor(GlyphColor.ToFColor(true));
			Canvas->DrawText(Font, Glyph, PenX, GlyphY, DrawScale, DrawScale);
		}

		PenX += GlyphWidths[Index];
	}

	return FVector2D(TotalWidth, TextHeight);
}
