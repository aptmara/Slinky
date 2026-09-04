#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "SlinkyHUD.generated.h"

class ASlinkyActor;
class UFont;

UCLASS()
class ASlinkyHUD : public AHUD
{
	GENERATED_BODY()

public:
	ASlinkyHUD();
	virtual void DrawHUD() override;

private:
	// Combo label + count + gauge bar (only while ComboCount >= 2) and the tier-up milestone banner
	// (e.g. "GREAT COMBO!!", only while its fade timer is running) - both centered along the top of
	// the screen, both pulled from ASlinkyActor's combo/pop-scale state rather than tracking any of
	// it here.
	void DrawComboBanner(const ASlinkyActor* Slinky);

	// A cheap, cheerful full-screen color wash that fades out - see ASlinkyActor::GetComboFlashAlpha().
	// Drawn first so every other HUD element layers on top of it.
	void DrawComboFlash(const ASlinkyActor* Slinky);

	// Draws Text centered on CenterX with each glyph riding its own little sine bounce (a comic-book
	// "wobbly marquee" look) and a black outline for legibility over busy backgrounds - used for both
	// the everyday combo counter and the big tier-up banner rather than a single flat DrawText call.
	// WobblePhase offsets the per-letter sine so two simultaneous wobbly texts don't bounce in lockstep.
	// Returns (total width, glyph height) drawn, so a caller can lay out the next line below it.
	FVector2D DrawWobblyText(const FString& Text, float CenterX, float Y, float Scale, const FLinearColor& Color,
		float WobbleAmount, float WobblePhase, bool bRainbow) const;

	// Radiating "impact" lines behind a milestone banner - a handful of short strokes fanning out
	// from Center, rotating slowly so the banner never looks static while it's up.
	void DrawStarburst(const FVector2D& Center, float InnerRadius, float OuterRadius, const FLinearColor& Color,
		int32 RayCount, float RotationOffset) const;

	// Rounded, hand-lettered display font (Content/YuseiMagic-Regular_Font) used for every combo/
	// milestone glyph drawn via DrawWobblyText() - reads far more "poppy/comical" than the engine's
	// default large font. Loaded once here (ConstructorHelpers only works in a constructor) rather
	// than per-draw-call. Falls back to GEngine->GetLargeFont() in DrawWobblyText() if this is ever
	// null (e.g. the asset got moved/deleted).
	UPROPERTY()
	TObjectPtr<UFont> ComboFont;
};
