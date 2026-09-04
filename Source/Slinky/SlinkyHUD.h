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
	// Combo label + huge combo-count "hit marker" + gauge bar (only while ComboCount >= 2) and the
	// tier-up milestone banner (e.g. "GREAT COMBO!!", only while its fade timer is running) - both
	// centered on screen, both pulled from ASlinkyActor's combo/pop-scale state rather than tracking
	// any of it here. Deliberately text-only: no background flashes/starbursts/panels - the letters
	// themselves (size, punchy pop, outline, shadow) carry all the excitement so they never compete
	// with, or get buried under, screen-wide dressing.
	void DrawComboBanner(const ASlinkyActor* Slinky);

	// Draws Text centered on CenterX with a drop shadow, a thick colored outline, and each glyph
	// riding its own little sine bounce (a comic-book "wobbly marquee" look) - used for both the
	// everyday combo counter and the big tier-up banner rather than a single flat DrawText call.
	// WobblePhase offsets the per-letter sine so two simultaneous wobbly texts don't bounce in
	// lockstep. OutlineColor defaults to near-black but the combo number readout passes the combo's
	// own color with a white fill for extra "hit marker" contrast.
	// Returns (total width, glyph height) drawn, so a caller can lay out the next line below it.
	FVector2D DrawWobblyText(const FString& Text, float CenterX, float Y, float Scale, const FLinearColor& Color,
		float WobbleAmount, float WobblePhase, bool bRainbow,
		const FLinearColor& OutlineColor = FLinearColor(0.07f, 0.055f, 0.086f, 1.0f)) const;

	// Rounded, hand-lettered display font (Content/YuseiMagic-Regular_Font) used for every combo/
	// milestone glyph drawn via DrawWobblyText() - reads far more "poppy/comical" than the engine's
	// default large font. Loaded once here (ConstructorHelpers only works in a constructor) rather
	// than per-draw-call. Falls back to GEngine->GetLargeFont() in DrawWobblyText() if this is ever
	// null (e.g. the asset got moved/deleted).
	UPROPERTY()
	TObjectPtr<UFont> ComboFont;
};
