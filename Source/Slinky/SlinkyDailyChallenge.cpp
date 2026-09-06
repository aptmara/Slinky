#include "SlinkyDailyChallenge.h"
#include "Misc/Crc.h"

namespace
{
	struct FDailyPreset
	{
		const TCHAR* DisplayName;
		FSlinkyChallengeConfig Base;
	};

	// Each preset gives every one of the 13 parameters a flavorful starting point (not just the
	// ones its name suggests), so a purely-uniform-random day never happens - GenerateForChallengeId
	// only ever jitters one preset's own numbers by +-DailyVarianceFraction, never mixes two presets
	// or rolls a parameter independently of the rest. Rolling all 13 independently from their full
	// control-panel ranges could produce e.g. maximum length + minimum stiffness + a huge stair rise
	// on the same day, which is unplayable rather than just unusual.
	const TArray<FDailyPreset>& GetPresets()
	{
		static const TArray<FDailyPreset> Presets = {
			{ TEXT("よく跳ねる日"), [] { FSlinkyChallengeConfig C; C.Restitution = 0.55f; C.Friction = 0.22f; C.DampingScale = 0.7f; return C; }() },
			{ TEXT("ふにゃふにゃの日"), [] { FSlinkyChallengeConfig C; C.AxialStiffnessScale = 0.35f; C.BendStiffnessScale = 0.3f; C.DampingScale = 0.6f; return C; }() },
			{ TEXT("太くて短い日"), [] { FSlinkyChallengeConfig C; C.CoilRadius = 75.0f; C.WireRadius = 3.0f; C.CompactLength = 80.0f; C.CoilTurns = 20.0f; return C; }() },
			{ TEXT("びよーんの日"), [] { FSlinkyChallengeConfig C; C.CompactLength = 320.0f; C.MaximumNodeSpacing = 220.0f; C.CoilTurns = 55.0f; return C; }() },
			{ TEXT("階段が険しい日"), [] { FSlinkyChallengeConfig C; C.StepRise = 150.0f; C.StepDepth = 70.0f; C.Friction = 1.1f; return C; }() },
			{ TEXT("細かい階段の日"), [] { FSlinkyChallengeConfig C; C.StepDepth = 35.0f; C.StepRise = 30.0f; C.RiserThickness = 4.0f; return C; }() },
		};
		return Presets;
	}

	// +-15%: enough that a preset's flavor is never bit-for-bit the same two days running, not so
	// much that jitter alone can push a value into a wildly different regime than its preset
	// intended - the per-field Clamp below is what actually stops it from ever leaving the control
	// panel's own min/max range entirely.
	constexpr float DailyVarianceFraction = 0.15f;
}

FString FSlinkyDailyChallenge::GetChallengeIdForDate(const FDateTime& UtcDate)
{
	return FString::Printf(TEXT("%04d-%02d-%02d"), UtcDate.GetYear(), UtcDate.GetMonth(), UtcDate.GetDay());
}

FSlinkyChallengeConfig FSlinkyDailyChallenge::GenerateForChallengeId(const FString& ChallengeId)
{
	const TArray<FDailyPreset>& Presets = GetPresets();
	// Deterministic across every player and machine: seeded purely from the challenge id string
	// itself, not FMath::Rand or any wall-clock/machine state - anyone who computes this for the
	// same ChallengeId gets the exact same stream, and so the exact same preset choice and
	// per-parameter jitter.
	FRandomStream Stream(static_cast<int32>(FCrc::StrCrc32(*ChallengeId)));

	const FDailyPreset& Preset = Presets[Stream.RandRange(0, Presets.Num() - 1)];
	FSlinkyChallengeConfig Config = Preset.Base;
	Config.ChallengeId = ChallengeId;
	Config.PresetName = Preset.DisplayName;

	// Ranges match USlinkyControlPanel::NativeOnInitialized's AddRow() calls - keep both in sync if
	// either ever changes, so a generated day's numbers always land somewhere a returning FreePlay
	// player could also have dialed in by hand.
	const auto Jitter = [&Stream](float Value, float Min, float Max)
	{
		const float Frac = Stream.FRandRange(-DailyVarianceFraction, DailyVarianceFraction);
		return FMath::Clamp(Value * (1.0f + Frac), Min, Max);
	};

	Config.CoilTurns           = Jitter(Config.CoilTurns, 6.0f, 72.0f);
	Config.CoilRadius          = Jitter(Config.CoilRadius, 15.0f, 100.0f);
	Config.CompactLength       = Jitter(Config.CompactLength, 20.0f, 400.0f);
	Config.MaximumNodeSpacing  = Jitter(Config.MaximumNodeSpacing, 10.0f, 300.0f);
	Config.WireRadius          = Jitter(Config.WireRadius, 0.25f, 5.0f);
	Config.AxialStiffnessScale = Jitter(Config.AxialStiffnessScale, 0.1f, 3.0f);
	Config.BendStiffnessScale  = Jitter(Config.BendStiffnessScale, 0.1f, 3.0f);
	Config.DampingScale        = Jitter(Config.DampingScale, 0.1f, 3.0f);
	Config.Restitution         = Jitter(Config.Restitution, 0.0f, 1.0f);
	Config.Friction            = Jitter(Config.Friction, 0.0f, 2.0f);
	Config.StepDepth           = Jitter(Config.StepDepth, 20.0f, 300.0f);
	Config.StepRise            = Jitter(Config.StepRise, 10.0f, 200.0f);
	Config.RiserThickness      = Jitter(Config.RiserThickness, 1.0f, 40.0f);

	return Config;
}

FTimespan FSlinkyDailyChallenge::GetTimeUntilNextChallengeUtc()
{
	const FDateTime Now = FDateTime::UtcNow();
	const FDateTime NextMidnight(Now.GetYear(), Now.GetMonth(), Now.GetDay());
	return (NextMidnight + FTimespan::FromDays(1)) - Now;
}
