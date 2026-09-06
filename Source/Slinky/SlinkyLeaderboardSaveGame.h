#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "SlinkyChallengeTypes.h"
#include "SlinkyLeaderboardSaveGame.generated.h"

// Local-only "leaderboard" storage - see USlinkyGameInstance::RecordChallengeResult. Kept in its
// own save slot, separate from USlinkySaveGame's single all-time record and
// USlinkySettingsSaveGame's preferences, so wiping one never touches the others.
UCLASS()
class USlinkyLeaderboardSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	static const FString SlotName;
	static constexpr int32 UserIndex = 0;

	// Every Ranked run ever recorded (see USlinkyGameInstance::RecordChallengeResult), sorted
	// descending by DepthMeters and capped at a small MaxRankedRecords - a "personal top scores"
	// list, not a full run history.
	UPROPERTY()
	TArray<FSlinkyLocalRecord> RankedRecords;

	// One best-of-the-day entry per daily challenge id ("2026-09-04") ever played, so both "today's
	// best" and a history of past days can be read back without keeping every individual run.
	UPROPERTY()
	TMap<FString, FSlinkyLocalRecord> DailyBestByChallengeId;
};
