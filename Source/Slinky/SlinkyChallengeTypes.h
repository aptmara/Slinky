#pragma once

#include "CoreMinimal.h"
#include "SlinkyChallengeTypes.generated.h"

// Which ruleset governs the current run and where its result (if any) gets recorded - see
// USlinkyGameInstance::SetPendingGameMode/ASlinkyGameMode::GetCurrentGameMode. FreePlay is the only
// mode where the on-screen control panel and every keyboard tuning shortcut are left interactive;
// the other two apply a fixed FSlinkyChallengeConfig at StartPlay and lock every customization entry
// point (see ASlinkyGameMode::IsCustomizationLocked) - a leaderboard only means something if
// everyone on it played the same slinky.
enum class ESlinkyGameMode : uint8
{
	FreePlay,
	DailyChallenge,
	Ranked
};

// One full set of the 13 live-tunable stair/coil parameters (see USlinkyControlPanel::EParam for
// the same list) plus which challenge produced it - what ASlinkyGameMode::ApplyChallengeConfig
// pushes onto the live ASlinkyStaircase/ASlinkyActor for DailyChallenge/Ranked runs. Not a
// UPROPERTY-bearing USTRUCT: nothing here is a UObject reference and the config itself is never
// persisted (only the *outcome* of playing one, FSlinkyLocalRecord below, is saved), so plain C++
// is enough.
struct FSlinkyChallengeConfig
{
	FString ChallengeId;
	// Empty outside DailyChallenge - the preset display name jittered to produce this config, e.g.
	// "びよーんの日". See FSlinkyDailyChallenge.
	FString PresetName;

	float CoilTurns = 36.0f;
	float CoilRadius = 45.0f;
	float CompactLength = 180.0f;
	float MaximumNodeSpacing = 60.0f;
	float WireRadius = 1.25f;
	float AxialStiffnessScale = 1.0f;
	float BendStiffnessScale = 1.0f;
	float DampingScale = 1.0f;
	float Restitution = 0.08f;
	float Friction = 0.48f;

	float StepDepth = 100.0f;
	float StepRise = 83.333f;
	float RiserThickness = 10.0f;

	// The Ranked ruleset - the plain compile-time defaults every field above is already initialized
	// to, named explicitly so ASlinkyGameMode's Ranked branch reads as "the standard config" rather
	// than an unexplained default-constructed struct. Keep in sync with ASlinkyActor's/
	// ASlinkyStaircase's own UPROPERTY defaults if either ever changes.
	static FSlinkyChallengeConfig Defaults()
	{
		FSlinkyChallengeConfig Config;
		Config.ChallengeId = TEXT("Ranked");
		return Config;
	}
};

// One completed run's result, kept locally (see USlinkyLeaderboardSaveGame) for DailyChallenge and
// Ranked modes only - FreePlay runs aren't ranked against anything so they never produce one of
// these. Timestamp is UTC, matching FSlinkyDailyChallenge's challenge-id dates.
USTRUCT()
struct FSlinkyLocalRecord
{
	GENERATED_BODY()

	UPROPERTY()
	FDateTime Timestamp = FDateTime(0);

	// The ranking metric every leaderboard comparison/sort actually uses (see
	// USlinkyGameInstance::RecordChallengeResult/GetRankedLeaderboard) - steps landed, not depth.
	UPROPERTY()
	int32 StepCount = 0;

	// Depth reached, in meters - carried along for display only; StepCount above is what a
	// leaderboard is ordered and compared by.
	UPROPERTY()
	float DepthMeters = 0.0f;

	UPROPERTY()
	int32 BestCombo = 0;

	// Which daily challenge this run was played under (e.g. "2026-09-04") - empty for a Ranked run.
	UPROPERTY()
	FString ChallengeId;
};
