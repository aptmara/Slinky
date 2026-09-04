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
	// Huge combo-count "hit marker" (only while ComboCount >= 2) and the tier-up milestone banner
	// (e.g. "GREAT COMBO!!", only while its fade timer is running) - both centered on screen, both
	// pulled from ASlinkyActor's combo/pop-scale state rather than tracking any of it here.
	void DrawComboBanner(const ASlinkyActor* Slinky);

	// Draws Text centered on CenterX with each glyph riding its own little sine bounce (a comic-book
	// "wobbly marquee" look) - used for both the combo number and the big tier-up banner rather than
	// a single flat DrawText call. WobblePhase offsets the per-letter sine so two simultaneous wobbly
	// texts don't bounce in lockstep. Each glyph is drawn exactly once - no repeated outline/shadow
	// passes - so this stays legible even at the very small DrawScale a large LegacyFontSize can force
	// glyph measurement/rendering into.
	// Returns (total width, glyph height) drawn, so a caller can lay out the next line below it.
	FVector2D DrawWobblyText(const FString& Text, float CenterX, float Y, float Scale, const FLinearColor& Color,
		float WobbleAmount, float WobblePhase, bool bRainbow) const;

	// Rounded, hand-lettered display font (Content/YuseiMagic-Regular_Font) used for every combo/
	// milestone glyph drawn via DrawWobblyText() - reads far more "poppy/comical" than the engine's
	// default large font. Loaded once here (ConstructorHelpers only works in a constructor) rather
	// than per-draw-call. Falls back to GEngine->GetLargeFont() in DrawWobblyText() if this is ever
	// null (e.g. the asset got moved/deleted).
	UPROPERTY()
	TObjectPtr<UFont> ComboFont;
};
