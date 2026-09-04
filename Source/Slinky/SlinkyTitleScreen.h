#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SlinkyTitleScreen.generated.h"

class UButton;
class UPopAnimator;
class UTextBlock;

// The title screen's whole widget hierarchy, built entirely in C++ via WidgetTree (no widget-
// blueprint asset), matching USlinkyControlPanel's approach: the SLINKY logo up top, then a
// "はじめから" (new game) button and a "つづきから" (continue) button below it - Continue is
// grayed out and unclickable whenever no save file exists yet.
UCLASS()
class USlinkyTitleScreen : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	// One rounded, drop-shadowed pastel button (the same "3D card" look as the control panel's
	// rows), added to Parent. bEnabled false renders it desaturated and unclickable, used for
	// Continue when there is nothing to continue from.
	UButton* AddMenuButton(UPanelWidget* Parent, const FString& Label, FLinearColor Accent, bool bEnabled);

	UPopAnimator* AddButtonPop(UButton* Button);

	UFUNCTION() void OnNewGameClicked();
	UFUNCTION() void OnContinueClicked();
	UFUNCTION() void OnQuitClicked();

	UPROPERTY()
	TArray<TObjectPtr<UPopAnimator>> Animators;
};
