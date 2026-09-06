#pragma once

#include "CoreMinimal.h"
#include "SlinkyChallengeTypes.h"

// Deterministic generator for "today's slinky": every player, on every machine, who asks for the
// same UTC calendar date gets back byte-for-byte the same FSlinkyChallengeConfig - the seed comes
// only from the date string itself (see GenerateForChallengeId), never wall-clock time, machine
// state, or FMath::Rand. Entirely local: nothing here talks to a server, which is also why the seed
// isn't a secret HMAC the way a real backend's would be - anyone willing to read this file can
// predict tomorrow's config, but there's no server-authoritative leaderboard here for that to
// undermine (see USlinkyGameInstance::RecordChallengeResult - results are only ever saved locally).
class FSlinkyDailyChallenge
{
public:
	// "YYYY-MM-DD" in UTC - the one identifier a day's challenge, its generated config, and its
	// local-leaderboard entry (see USlinkyLeaderboardSaveGame::DailyBestByChallengeId) all share.
	static FString GetChallengeIdForDate(const FDateTime& UtcDate);
	static FString GetTodayChallengeId() { return GetChallengeIdForDate(FDateTime::UtcNow()); }

	static FSlinkyChallengeConfig GenerateForChallengeId(const FString& ChallengeId);
	static FSlinkyChallengeConfig GenerateForToday() { return GenerateForChallengeId(GetTodayChallengeId()); }

	// How long until the challenge id changes - for a "次のチャレンジまで hh:mm:ss" countdown. Always
	// measured against UTC midnight, independent of the player's local timezone/clock.
	static FTimespan GetTimeUntilNextChallengeUtc();
};
