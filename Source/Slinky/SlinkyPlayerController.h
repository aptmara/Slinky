#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "SlinkyPlayerController.generated.h"

class ASlinkyActor;
class ASlinkyStaircase;
class USlinkyControlPanel;
class USlinkyPauseMenu;

UCLASS()
class ASlinkyPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ASlinkyPlayerController();

	// Called by USlinkyPauseMenu after resetting the staircase/coil to defaults, so the control
	// panel's cached row values (and their on-screen readouts) pick up the new numbers instead of
	// silently going stale until the player next nudges a slider.
	void RefreshControlPanelFromLiveValues();

protected:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;
	virtual void PlayerTick(float DeltaTime) override;

private:
	void AcquireSlinky();
	void AcquireStaircase();
	// DailyChallenge/Ranked lock every customization entry point (keyboard tuning below, and the
	// on-screen ControlPanel - see PlayerTick) since a leaderboard only means something if everyone
	// on it played the same slinky. See ASlinkyGameMode::IsCustomizationLocked.
	bool IsCustomizationLocked() const;
	void BeginDrag();
	void EndDrag();
	void RestartSlinky();
	bool ProjectCursorToPlayPlane(FVector& WorldPosition) const;

	// Live in-game tuning of the staircase, so its shape can be adjusted while watching the slinky
	// react without a rebuild-and-relaunch cycle.
	void IncreaseStepDepth();
	void DecreaseStepDepth();
	void IncreaseStepRise();
	void DecreaseStepRise();
	void IncreaseRiserThickness();
	void DecreaseRiserThickness();

	// Live in-game tuning of the coil itself: Tab cycles which parameter is selected, [ and ]
	// adjust it. See ASlinkyActor::ETuningParam for the full list.
	void CycleCoilTuningParam();
	void IncreaseCoilTuningParam();
	void DecreaseCoilTuningParam();

	void TogglePauseMenu();

	UPROPERTY()
	TObjectPtr<ASlinkyActor> Slinky;

	UPROPERTY()
	TObjectPtr<ASlinkyStaircase> Staircase;

	// The on-screen slider panel duplicating the keyboard tuning above. Created once in BeginPlay;
	// SetTargets() is called on it as soon as AcquireSlinky/AcquireStaircase both succeed.
	UPROPERTY()
	TObjectPtr<USlinkyControlPanel> ControlPanel;

	// The always-in-viewport pause overlay - see USlinkyPauseMenu. Created alongside ControlPanel
	// in BeginPlay and given a higher Z-order so it renders above it when opened.
	UPROPERTY()
	TObjectPtr<USlinkyPauseMenu> PauseMenu;

	bool bControlPanelTargetsSet = false;
};
