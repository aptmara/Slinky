#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "SlinkySaveGame.generated.h"

// The one persisted record file for the whole game: best combo streak and deepest fall reached,
// across all runs. Written by whatever in-game trigger USlinkyGameInstance::SaveProgress() ends up
// wired to later, and read back by the title screen (to show/enable Continue) and by
// ASlinkyGameMode (to seed a continued run's starting record).
UCLASS()
class USlinkySaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	static const FString SlotName;
	static constexpr int32 UserIndex = 0;

	UPROPERTY()
	int32 BestCombo = 0;

	// Record (high-water mark) depth, shown on the title screen - never decreases.
	UPROPERTY()
	float BestDepthMeters = 0.0f;

	// Exact depth to resume at on "つづきから" - always overwritten to the latest save, unlike
	// BestDepthMeters above, so continuing always picks up exactly where that save left off even
	// after a run that didn't beat the record.
	UPROPERTY()
	float ContinueDepthMeters = 0.0f;
};
