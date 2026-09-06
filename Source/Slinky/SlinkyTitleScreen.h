#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SlinkyTitleScreen.generated.h"

class UButton;
class UOverlay;
class UPanelWidget;
class UPopAnimator;
class UTextBlock;
class UTexture2D;
class UVerticalBox;

// The title screen's whole widget hierarchy, built entirely in C++ via WidgetTree (no widget-
// blueprint asset), matching USlinkyControlPanel's approach: the SLINKY logo up top, then the
// mode buttons - "はじめから"/"つづきから" (自由, unchanged), "今日のチャレンジ" (デイリー) and
// "ランクに挑戦" (ランク) - and a "ローカルランキング" button that toggles a centered overlay
// listing local Ranked/Daily results. Continue is grayed out and unclickable whenever no save file
// exists yet.
UCLASS()
class USlinkyTitleScreen : public UUserWidget
{
	GENERATED_BODY()

public:
	USlinkyTitleScreen(const FObjectInitializer& ObjectInitializer);
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	// One rounded, drop-shadowed pastel button (the same "3D card" look as the control panel's
	// rows), added to Parent. bEnabled false renders it desaturated and unclickable, used for
	// Continue when there is nothing to continue from.
	UButton* AddMenuButton(UPanelWidget* Parent, const FString& Label, FLinearColor Accent, bool bEnabled);

	// A small centered caption line added right under a menu button - the save-record line under
	// Continue, and the challenge/ranked status lines under their own buttons.
	void AddCaption(UPanelWidget* Parent, const FString& Text);

	UPopAnimator* AddButtonPop(UButton* Button);

	// Builds (once) the ローカルランキング overlay: a centered card listing the Ranked top scores
	// and today's Daily best, hidden until OnLeaderboardClicked toggles it on. Its rows are rebuilt
	// from scratch each time it opens (see RefreshLeaderboardRows) rather than kept incrementally in
	// sync, since this screen is only ever shown between runs, never while a score is still changing.
	void BuildLeaderboardOverlay(UPanelWidget* RootParent);
	void RefreshLeaderboardRows();

	UFUNCTION() void OnNewGameClicked();
	UFUNCTION() void OnContinueClicked();
	UFUNCTION() void OnDailyChallengeClicked();
	UFUNCTION() void OnRankedClicked();
	UFUNCTION() void OnLeaderboardClicked();
	UFUNCTION() void OnLeaderboardCloseClicked();
	UFUNCTION() void OnQuitClicked();

	UPROPERTY()
	TArray<TObjectPtr<UPopAnimator>> Animators;

	// Loaded once here via ConstructorHelpers (only valid inside a UObject's own constructor -
	// see ASlinkyHUD::ComboFont for the same pattern), NOT via a runtime LoadObject() call in
	// NativeOnInitialized as this used to do: a plain string-path LoadObject() is invisible to the
	// cooker's static reference analysis, so the packaged build silently cooked without this
	// texture at all and the logo never rendered outside the editor.
	UPROPERTY()
	TObjectPtr<UTexture2D> LogoTexture;

	UPROPERTY()
	TObjectPtr<UOverlay> LeaderboardOverlay;

	UPROPERTY()
	TObjectPtr<UVerticalBox> LeaderboardListBox;
};
