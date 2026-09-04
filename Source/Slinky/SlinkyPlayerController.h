#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "SlinkyPlayerController.generated.h"

class ASlinkyActor;
class ASlinkyStaircase;
class USlinkyControlPanel;

UCLASS()
class ASlinkyPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ASlinkyPlayerController();

protected:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;
	virtual void PlayerTick(float DeltaTime) override;

private:
	void AcquireSlinky();
	void AcquireStaircase();
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

	UPROPERTY()
	TObjectPtr<ASlinkyActor> Slinky;

	UPROPERTY()
	TObjectPtr<ASlinkyStaircase> Staircase;

	// The on-screen slider panel duplicating the keyboard tuning above. Created once in BeginPlay;
	// SetTargets() is called on it as soon as AcquireSlinky/AcquireStaircase both succeed.
	UPROPERTY()
	TObjectPtr<USlinkyControlPanel> ControlPanel;

	bool bControlPanelTargetsSet = false;
};
