#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SlinkyPauseMenu.generated.h"

class ASlinkyActor;
class ASlinkyStaircase;
class UBorder;
class UButton;
class UCheckBox;
class UPanelWidget;
class UPopAnimator;
class USlider;
class UWidget;

// The whole pause overlay, built in C++ via WidgetTree (same approach as USlinkyControlPanel and
// USlinkyTitleScreen - no widget-blueprint asset). Always in the viewport during gameplay: a small
// corner icon is visible at rest, and clicking it (or pressing Escape, see
// ASlinkyPlayerController) expands the full paused card - resume/respawn/reset/save & quit plus
// the volume/effects/combo-display preferences, all routed through USlinkyGameInstance.
UCLASS()
class USlinkyPauseMenu : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	void TogglePause();
	bool IsMenuOpen() const { return bMenuOpen; }

private:
	UButton* AddMenuButton(UPanelWidget* Parent, const FString& Label, FLinearColor Accent);
	UCheckBox* AddToggleRow(UPanelWidget* Parent, const FString& Label, FLinearColor Accent, bool bInitiallyChecked);
	UPopAnimator* AddButtonPop(UButton* Button);
	void SetMenuOpen(bool bOpen);

	ASlinkyActor* FindSlinky() const;
	ASlinkyStaircase* FindStaircase() const;

	UFUNCTION() void OnPauseIconClicked();
	UFUNCTION() void OnResumeClicked();
	UFUNCTION() void OnRespawnClicked();
	UFUNCTION() void OnResetDefaultsClicked();
	UFUNCTION() void OnSaveAndQuitClicked();
	UFUNCTION() void OnVolumeChanged(float NewValue);
	UFUNCTION() void OnEffectsToggled(bool bChecked);
	UFUNCTION() void OnComboDisplayToggled(bool bChecked);

	UPROPERTY()
	TObjectPtr<UBorder> DimBackground;

	UPROPERTY()
	TObjectPtr<UWidget> MenuCard;

	UPROPERTY()
	TArray<TObjectPtr<UPopAnimator>> Animators;

	bool bMenuOpen = false;
};
