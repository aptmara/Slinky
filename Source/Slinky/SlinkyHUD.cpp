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

	// Drawn first so every other element in this function layers on top of the wash instead of
	// being tinted by it.
	DrawComboFlash(Slinky);

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

void ASlinkyHUD::DrawComboFlash(const ASlinkyActor* Slinky)
{
	const float FlashAlpha = Slinky->GetComboFlashAlpha();
	const float MilestoneShake = Slinky->GetMilestoneShake();
	if (FlashAlpha <= 0.0f && MilestoneShake <= 0.0f)
	{
		return;
	}

	const float ScreenW = static_cast<float>(Canvas->SizeX);
	const float ScreenH = static_cast<float>(Canvas->SizeY);

	if (FlashAlpha > 0.0f)
	{
		const FLinearColor FlashColor = Slinky->GetComboColor();
		// Punchier than a subtle tint - meant to read as a real color hit on every landing, not just
		// a faint wash.
		DrawRect(FLinearColor(FlashColor.R, FlashColor.G, FlashColor.B, FlashAlpha * 0.32f),
			0.0f, 0.0f, ScreenW, ScreenH);
	}
	if (MilestoneShake > 0.0f)
	{
		// A near-white punch layered on top specifically for a tier-up, so crossing a tier reads as a
		// distinct, bigger "event" flash rather than just a stronger version of the per-step pulse.
		DrawRect(FLinearColor(1.0f, 1.0f, 1.0f, MilestoneShake * 0.4f), 0.0f, 0.0f, ScreenW, ScreenH);
	}
}

void ASlinkyHUD::DrawComboBanner(const ASlinkyActor* Slinky)
{
	const float CenterX = static_cast<float>(Canvas->SizeX) * 0.5f;
	const float ScreenH = static_cast<float>(Canvas->SizeY);
	const double Time = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;

	if (Slinky->GetComboCount() >= 2)
	{
		const FLinearColor ComboColorLinear = Slinky->GetComboColor();
		// Grows with the streak itself (capped) instead of a single flat size, so a long combo keeps
		// visibly filling more of the screen rather than sitting as a small, easy-to-miss readout.
		const float GrowthScale = 1.0f + FMath::Min(Slinky->GetComboCount() * 0.018f, 0.6f);
		const float ComboScale = FMath::Max(Slinky->GetComboPopScale(), 0.1f) * 1.6f * GrowthScale;
		// The label itself escalates every 10 combo (see ASlinkyActor::GetComboLabel/ComboLabelForTier)
		// - "COMBO x7" today can read "NICE COMBO x14" a few steps later without this ever needing a
		// separate milestone check.
		const FString ComboText = FString::Printf(TEXT("%s x%d"), *Slinky->GetComboLabel(), Slinky->GetComboCount());

		// Dead-center-ish rather than pinned to the top edge - a corner/top readout was too easy to
		// lose track of while actually watching the coil fall down the stairs.
		const float ComboY = ScreenH * 0.32f;
		// A constant light jitter (independent of the per-step pop spring) keeps the counter feeling
		// like it's buzzing with energy even between landings, not just sitting still between pops.
		const float IdleShakeX = FMath::Sin(Time * 14.0f) * 2.0f;
		const float IdleShakeY = FMath::Cos(Time * 11.0f) * 1.5f;

		const FVector2D Size = DrawWobblyText(ComboText, CenterX + IdleShakeX, ComboY + IdleShakeY, ComboScale,
			ComboColorLinear, 4.5f, 0.0f, false);

		// Empties out left-to-right as ComboTimeRemaining runs down, giving a visible countdown to
		// when the streak will drop instead of it just vanishing without warning. Scales up with the
		// text above it so it never looks like an afterthought next to a huge combo label.
		const float GaugeWidth = 280.0f * GrowthScale;
		constexpr float GaugeHeight = 10.0f;
		const float GaugeX = CenterX - GaugeWidth * 0.5f;
		const float GaugeY = ComboY + Size.Y * 0.5f + 16.0f;
		DrawRect(FLinearColor(0.05f, 0.05f, 0.07f, 0.7f), GaugeX, GaugeY, GaugeWidth, GaugeHeight);
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

		// Big enough to dominate the screen for its brief moment - a tier-up is meant to interrupt
		// and demand attention, not politely add a line near the top.
		const float MilestoneScale = (2.6f + 1.6f * MilestoneAlpha) * (1.0f + 0.3f * Shake);
		const float BannerCenterY = ScreenH * 0.42f;

		// Screen-shake jitters the whole banner (text + starburst) together, not just the text glyphs,
		// so a tier-up reads as a genuine impact rather than only the letters wobbling.
		const float ShakeX = FMath::Sin(Time * 53.0f) * Shake * 16.0f;
		const float ShakeY = FMath::Cos(Time * 61.0f) * Shake * 11.0f;

		FLinearColor TextColor = ComboColorLinear;
		TextColor.A = FMath::Clamp(MilestoneAlpha, 0.0f, 1.0f);

		// Starburst now reaches most of the way to the screen edges so the tier-up reads as a real
		// explosion filling the frame, not a modest badge behind the text.
		const float ScreenDiagonalHalf = 0.5f * FMath::Sqrt(
			FMath::Square(static_cast<float>(Canvas->SizeX)) + FMath::Square(ScreenH));
		DrawStarburst(FVector2D(CenterX + ShakeX, BannerCenterY + ShakeY),
			70.0f * MilestoneScale, ScreenDiagonalHalf * 0.72f, TextColor, 16, Time * 2.4f);

		// Rainbow cycling only for the milestone banner - the everyday combo counter stays one solid
		// color so the rare, extra-loud rainbow treatment keeps reading as special.
		DrawWobblyText(Milestone, CenterX + ShakeX, BannerCenterY + ShakeY, MilestoneScale, TextColor,
			9.0f + Shake * 12.0f, 1.7f, true);
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

	// Drawing the glyph eight times at a small offset around itself fakes a solid outline (Canvas
	// text has no native stroke) - a cheap comic-book "inked" look that keeps light text readable
	// over the bright, busy stair backdrop.
	static const FVector2D OutlineOffsets[] = {
		{-1.5f, -1.5f}, {1.5f, -1.5f}, {-1.5f, 1.5f}, {1.5f, 1.5f},
		{0.0f, -2.0f}, {0.0f, 2.0f}, {-2.0f, 0.0f}, {2.0f, 0.0f},
	};
	FColor OutlineColor(18, 14, 22);
	OutlineColor.A = static_cast<uint8>(FMath::Clamp(Color.A, 0.0f, 1.0f) * 255.0f);

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
			Canvas->SetDrawColor(OutlineColor);
			for (const FVector2D& Offset : OutlineOffsets)
			{
				Canvas->DrawText(Font, Glyph, PenX + Offset.X, GlyphY + Offset.Y, Scale, Scale);
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

void ASlinkyHUD::DrawStarburst(const FVector2D& Center, float InnerRadius, float OuterRadius,
	const FLinearColor& Color, int32 RayCount, float RotationOffset) const
{
	if (!Canvas || RayCount <= 0)
	{
		return;
	}

	FLinearColor RayColor = Color;
	RayColor.A *= 0.8f;
	for (int32 Index = 0; Index < RayCount; ++Index)
	{
		const float Angle = (UE_TWO_PI * Index) / RayCount + RotationOffset;
		const FVector2D Direction(FMath::Cos(Angle), FMath::Sin(Angle));
		Canvas->K2_DrawLine(Center + Direction * InnerRadius, Center + Direction * OuterRadius, 4.0f, RayColor);
	}
}
