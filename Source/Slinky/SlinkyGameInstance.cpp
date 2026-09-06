#include "SlinkyGameInstance.h"
#include "SlinkyDailyChallenge.h"
#include "SlinkyLeaderboardSaveGame.h"
#include "SlinkySaveGame.h"
#include "SlinkySettingsSaveGame.h"
#include "SlinkySynthAudio.h"
#include "Components/AudioComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundClass.h"
#include "Sound/SoundMix.h"
#include "Sound/SoundWave.h"
#include "Sound/SoundWaveProcedural.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	const TCHAR* TitleLevelPath = TEXT("/Game/Slinky/Lvl_Title");
	const TCHAR* GameLevelPath = TEXT("/Game/Slinky/Lvl_Slinky");
}

USlinkyGameInstance::USlinkyGameInstance()
{
	static ConstructorHelpers::FObjectFinder<USoundBase> MusicFinder(TEXT("/Game/Audio/Cruising.Cruising"));
	if (MusicFinder.Succeeded())
	{
		BackgroundMusicSound = MusicFinder.Object;
	}
}

void USlinkyGameInstance::Init()
{
	Super::Init();

	if (UGameplayStatics::DoesSaveGameExist(USlinkySaveGame::SlotName, USlinkySaveGame::UserIndex))
	{
		LoadedSave = Cast<USlinkySaveGame>(
			UGameplayStatics::LoadGameFromSlot(USlinkySaveGame::SlotName, USlinkySaveGame::UserIndex));
	}

	Settings = UGameplayStatics::DoesSaveGameExist(USlinkySettingsSaveGame::SlotName, USlinkySettingsSaveGame::UserIndex)
		? Cast<USlinkySettingsSaveGame>(UGameplayStatics::LoadGameFromSlot(
			USlinkySettingsSaveGame::SlotName, USlinkySettingsSaveGame::UserIndex))
		: nullptr;
	if (!Settings)
	{
		Settings = Cast<USlinkySettingsSaveGame>(UGameplayStatics::CreateSaveGameObject(USlinkySettingsSaveGame::StaticClass()));
	}

	Leaderboard = UGameplayStatics::DoesSaveGameExist(USlinkyLeaderboardSaveGame::SlotName, USlinkyLeaderboardSaveGame::UserIndex)
		? Cast<USlinkyLeaderboardSaveGame>(UGameplayStatics::LoadGameFromSlot(
			USlinkyLeaderboardSaveGame::SlotName, USlinkyLeaderboardSaveGame::UserIndex))
		: nullptr;
	if (!Leaderboard)
	{
		Leaderboard = Cast<USlinkyLeaderboardSaveGame>(UGameplayStatics::CreateSaveGameObject(USlinkyLeaderboardSaveGame::StaticClass()));
	}

	ApplyMasterVolume();
}

void USlinkyGameInstance::OnStart()
{
	Super::OnStart();
	PlayBackgroundMusic();
}

int32 USlinkyGameInstance::GetSavedBestCombo() const
{
	return LoadedSave ? LoadedSave->BestCombo : 0;
}

float USlinkyGameInstance::GetSavedBestDepthMeters() const
{
	return LoadedSave ? LoadedSave->BestDepthMeters : 0.0f;
}

float USlinkyGameInstance::GetContinueDepthMeters() const
{
	return LoadedSave ? LoadedSave->ContinueDepthMeters : 0.0f;
}

void USlinkyGameInstance::SaveProgress(int32 InBestCombo, float InCurrentDepthMeters)
{
	USlinkySaveGame* SaveObject = LoadedSave;
	if (!SaveObject)
	{
		SaveObject = Cast<USlinkySaveGame>(UGameplayStatics::CreateSaveGameObject(USlinkySaveGame::StaticClass()));
	}
	if (!SaveObject)
	{
		return;
	}

	SaveObject->BestCombo = FMath::Max(SaveObject->BestCombo, InBestCombo);
	SaveObject->BestDepthMeters = FMath::Max(SaveObject->BestDepthMeters, InCurrentDepthMeters);
	SaveObject->ContinueDepthMeters = InCurrentDepthMeters;
	UGameplayStatics::SaveGameToSlot(SaveObject, USlinkySaveGame::SlotName, USlinkySaveGame::UserIndex);
	LoadedSave = SaveObject;
}

bool USlinkyGameInstance::ConsumePendingContinue()
{
	const bool bResult = bPendingContinue;
	bPendingContinue = false;
	return bResult;
}

void USlinkyGameInstance::GoToTitle()
{
	UGameplayStatics::OpenLevel(this, FName(TitleLevelPath));
}

void USlinkyGameInstance::GoToGame(bool bContinue, ESlinkyGameMode Mode)
{
	PendingGameMode = Mode;
	bPendingContinue = bContinue && (Mode == ESlinkyGameMode::FreePlay);
	UGameplayStatics::OpenLevel(this, FName(GameLevelPath));
}

ESlinkyGameMode USlinkyGameInstance::ConsumePendingGameMode()
{
	const ESlinkyGameMode Result = PendingGameMode;
	PendingGameMode = ESlinkyGameMode::FreePlay;
	return Result;
}

FString USlinkyGameInstance::GetTodayChallengeId() const
{
	return FSlinkyDailyChallenge::GetTodayChallengeId();
}

FSlinkyChallengeConfig USlinkyGameInstance::GetTodaysChallengeConfig() const
{
	return FSlinkyDailyChallenge::GenerateForToday();
}

FTimespan USlinkyGameInstance::GetTimeUntilNextChallenge() const
{
	return FSlinkyDailyChallenge::GetTimeUntilNextChallengeUtc();
}

void USlinkyGameInstance::RecordChallengeResult(ESlinkyGameMode Mode, int32 StepCount, float DepthMeters, int32 BestCombo)
{
	if (Mode == ESlinkyGameMode::FreePlay || !Leaderboard)
	{
		return;
	}

	const FDateTime Now = FDateTime::UtcNow();
	if (Mode == ESlinkyGameMode::Ranked)
	{
		FSlinkyLocalRecord Record;
		Record.Timestamp = Now;
		Record.StepCount = StepCount;
		Record.DepthMeters = DepthMeters;
		Record.BestCombo = BestCombo;
		Leaderboard->RankedRecords.Add(Record);
		Leaderboard->RankedRecords.Sort([](const FSlinkyLocalRecord& A, const FSlinkyLocalRecord& B)
		{
			return A.StepCount > B.StepCount;
		});

		constexpr int32 MaxRankedRecords = 50;
		if (Leaderboard->RankedRecords.Num() > MaxRankedRecords)
		{
			Leaderboard->RankedRecords.SetNum(MaxRankedRecords);
		}
	}
	else if (Mode == ESlinkyGameMode::DailyChallenge)
	{
		const FString ChallengeId = FSlinkyDailyChallenge::GetTodayChallengeId();
		FSlinkyLocalRecord& Best = Leaderboard->DailyBestByChallengeId.FindOrAdd(ChallengeId);
		if (StepCount > Best.StepCount)
		{
			Best.Timestamp = Now;
			Best.StepCount = StepCount;
			Best.DepthMeters = DepthMeters;
			Best.BestCombo = BestCombo;
			Best.ChallengeId = ChallengeId;
		}
	}

	SaveLeaderboard();
}

TArray<FSlinkyLocalRecord> USlinkyGameInstance::GetRankedLeaderboard(int32 MaxEntries) const
{
	if (!Leaderboard)
	{
		return {};
	}
	TArray<FSlinkyLocalRecord> Result = Leaderboard->RankedRecords;
	if (Result.Num() > MaxEntries)
	{
		Result.SetNum(MaxEntries);
	}
	return Result;
}

bool USlinkyGameInstance::GetDailyBest(const FString& ChallengeId, FSlinkyLocalRecord& OutRecord) const
{
	if (!Leaderboard)
	{
		return false;
	}
	if (const FSlinkyLocalRecord* Found = Leaderboard->DailyBestByChallengeId.Find(ChallengeId))
	{
		OutRecord = *Found;
		return true;
	}
	return false;
}

float USlinkyGameInstance::GetMasterVolume() const
{
	return Settings ? Settings->MasterVolume : 1.0f;
}

void USlinkyGameInstance::SetMasterVolume(float NewVolume)
{
	if (!Settings)
	{
		return;
	}
	Settings->MasterVolume = FMath::Clamp(NewVolume, 0.0f, 1.0f);
	ApplyMasterVolume();
	SaveSettings();
}

bool USlinkyGameInstance::AreEffectsEnabled() const
{
	return Settings ? Settings->bEffectsEnabled : true;
}

void USlinkyGameInstance::SetEffectsEnabled(bool bEnabled)
{
	if (!Settings)
	{
		return;
	}
	Settings->bEffectsEnabled = bEnabled;
	SaveSettings();
}

bool USlinkyGameInstance::IsComboDisplayEnabled() const
{
	return Settings ? Settings->bComboDisplayEnabled : true;
}

void USlinkyGameInstance::SetComboDisplayEnabled(bool bEnabled)
{
	if (!Settings)
	{
		return;
	}
	Settings->bComboDisplayEnabled = bEnabled;
	SaveSettings();
}

void USlinkyGameInstance::ApplyMasterVolume()
{
	if (!Settings)
	{
		return;
	}
	if (!VolumeSoundMix)
	{
		VolumeSoundMix = NewObject<USoundMix>(this);
	}
	// The engine's own root SoundClass every other SoundClass ultimately derives from - overriding
	// its volume here scales all audio without needing a project-specific SoundClass/Mix asset. A
	// harmless no-op today (the game has no sounds yet), but means the slider already does the
	// right thing the moment one gets added.
	if (USoundClass* MasterSoundClass = LoadObject<USoundClass>(nullptr, TEXT("/Engine/EngineSounds/Master.Master")))
	{
		UGameplayStatics::SetSoundMixClassOverride(this, VolumeSoundMix, MasterSoundClass, Settings->MasterVolume, 1.0f, 0.0f, true);
		UGameplayStatics::PushSoundMixModifier(this, VolumeSoundMix);
	}
}

void USlinkyGameInstance::SaveSettings()
{
	if (Settings)
	{
		UGameplayStatics::SaveGameToSlot(Settings, USlinkySettingsSaveGame::SlotName, USlinkySettingsSaveGame::UserIndex);
	}
}

void USlinkyGameInstance::SaveLeaderboard()
{
	if (Leaderboard)
	{
		UGameplayStatics::SaveGameToSlot(Leaderboard, USlinkyLeaderboardSaveGame::SlotName, USlinkyLeaderboardSaveGame::UserIndex);
	}
}

void USlinkyGameInstance::PlayBackgroundMusic()
{
	if (!BackgroundMusicSound || MusicComponent)
	{
		return;
	}

	// Force looping in code rather than relying on the imported asset's own Looping flag - this is
	// meant to underscore the whole session, however long that runs, not play once.
	if (USoundWave* Wave = Cast<USoundWave>(BackgroundMusicSound))
	{
		Wave->bLooping = true;
	}

	// bPersistAcrossLevelTransition=true is what lets this survive GoToTitle()/GoToGame()'s
	// OpenLevel calls instead of being torn down with the level it started on - this is the one
	// and only music cue for the whole process lifetime, started once from Init().
	MusicComponent = UGameplayStatics::SpawnSound2D(this, BackgroundMusicSound,
		/*VolumeMultiplier=*/1.0f, /*PitchMultiplier=*/1.0f, /*StartTime=*/0.0f,
		/*ConcurrencySettings=*/nullptr, /*bPersistAcrossLevelTransition=*/true, /*bAutoDestroy=*/false);
}

void USlinkyGameInstance::PlaySfx(ESlinkySfx Sfx, float PitchMultiplier)
{
	const TArray<int16>& Samples = GetOrBuildSfxSamples(Sfx);
	if (Samples.Num() == 0)
	{
		return;
	}

	USoundWaveProcedural* Wave = FSlinkySynthAudio::BuildPlayableWave(this, Samples);
	// bPersistAcrossLevelTransition=true: a few of these (ESlinkySfx::TimeUp in particular) are
	// fired the instant before a GoToTitle() call, and would otherwise be torn down mid-playback
	// along with the level they started on. bAutoDestroy=true still cleans each one-shot component
	// up for good once its short clip actually finishes.
	UGameplayStatics::SpawnSound2D(this, Wave,
		/*VolumeMultiplier=*/1.0f, PitchMultiplier, /*StartTime=*/0.0f,
		/*ConcurrencySettings=*/nullptr, /*bPersistAcrossLevelTransition=*/true, /*bAutoDestroy=*/true);
}

const TArray<int16>& USlinkyGameInstance::GetOrBuildSfxSamples(ESlinkySfx Sfx)
{
	if (const TArray<int16>* Cached = SfxSampleCache.Find(Sfx))
	{
		return *Cached;
	}

	TArray<int16> Samples;
	switch (Sfx)
	{
	case ESlinkySfx::StepMetal:
		// A short metallic clink - PlaySfx's PitchMultiplier (driven by the landing ComboCount, see
		// ASlinkyActor::RegisterGroundContact) is what actually varies step to step, not this base
		// clip.
		Samples = FSlinkySynthAudio::RenderMetallicRing(900.0f, 0.14f, 0.4f);
		break;
	case ESlinkySfx::StepPlastic:
	{
		// A short, soft "pop" - duller and rounder than the metal clink, no ringing tail.
		FSlinkyToneSpec Spec;
		Spec.StartFrequencyHz = 520.0f;
		Spec.EndFrequencyHz = 340.0f;
		Spec.DurationSeconds = 0.09f;
		Spec.AttackSeconds = 0.004f;
		Spec.DecayCurve = 2.5f;
		Spec.HarmonicAmount = 0.2f;
		Spec.Volume = 0.45f;
		Samples = FSlinkySynthAudio::Render(Spec);
		break;
	}
	case ESlinkySfx::ComboTierUp:
		// A short three-note upward flourish, matching the escalating "NICE COMBO"/"GREAT COMBO"
		// banner ladder (see ASlinkyActor::ComboLabelForTier) with an equally escalating sound.
		Samples = FSlinkySynthAudio::RenderArpeggio({ 523.25f, 659.25f, 783.99f }, 0.09f, 0.5f);
		break;
	case ESlinkySfx::ButtonClick:
	{
		// Mostly noise with a touch of tone underneath - a crisp UI click rather than a musical
		// note. Bound centrally in UPopAnimator::HandlePressed, so this covers every button on
		// every screen (title, control panel, pause menu) from one call site.
		FSlinkyToneSpec Spec;
		Spec.StartFrequencyHz = 1200.0f;
		Spec.EndFrequencyHz = 900.0f;
		Spec.DurationSeconds = 0.045f;
		Spec.AttackSeconds = 0.002f;
		Spec.DecayCurve = 4.0f;
		Spec.NoiseAmount = 0.55f;
		Spec.Volume = 0.35f;
		Samples = FSlinkySynthAudio::Render(Spec);
		break;
	}
	case ESlinkySfx::PauseOpen:
	{
		FSlinkyToneSpec Spec;
		Spec.StartFrequencyHz = 330.0f;
		Spec.EndFrequencyHz = 660.0f;
		Spec.DurationSeconds = 0.14f;
		Spec.AttackSeconds = 0.01f;
		Spec.DecayCurve = 2.0f;
		Spec.Volume = 0.4f;
		Samples = FSlinkySynthAudio::Render(Spec);
		break;
	}
	case ESlinkySfx::PauseClose:
	{
		FSlinkyToneSpec Spec;
		Spec.StartFrequencyHz = 660.0f;
		Spec.EndFrequencyHz = 330.0f;
		Spec.DurationSeconds = 0.14f;
		Spec.AttackSeconds = 0.01f;
		Spec.DecayCurve = 2.0f;
		Spec.Volume = 0.4f;
		Samples = FSlinkySynthAudio::Render(Spec);
		break;
	}
	case ESlinkySfx::Grab:
	{
		FSlinkyToneSpec Spec;
		Spec.StartFrequencyHz = 240.0f;
		Spec.EndFrequencyHz = 200.0f;
		Spec.DurationSeconds = 0.06f;
		Spec.AttackSeconds = 0.003f;
		Spec.DecayCurve = 3.0f;
		Spec.NoiseAmount = 0.2f;
		Spec.Volume = 0.35f;
		Samples = FSlinkySynthAudio::Render(Spec);
		break;
	}
	case ESlinkySfx::Release:
	{
		FSlinkyToneSpec Spec;
		Spec.StartFrequencyHz = 200.0f;
		Spec.EndFrequencyHz = 260.0f;
		Spec.DurationSeconds = 0.06f;
		Spec.AttackSeconds = 0.003f;
		Spec.DecayCurve = 3.0f;
		Spec.NoiseAmount = 0.2f;
		Spec.Volume = 0.35f;
		Samples = FSlinkySynthAudio::Render(Spec);
		break;
	}
	case ESlinkySfx::ResetMetal:
		// A longer metallic ring for restarting the coil (R key, or the pause menu's "リスポーン")
		// - the ringing tail is what should read as "metal", not just a lower pitch.
		Samples = FSlinkySynthAudio::RenderMetallicRing(360.0f, 0.32f, 0.45f);
		break;
	case ESlinkySfx::ResetPlastic:
	{
		// A shorter, duller downward "thunk" swoop - no ring, decays out well before Metal's does.
		FSlinkyToneSpec Spec;
		Spec.StartFrequencyHz = 420.0f;
		Spec.EndFrequencyHz = 140.0f;
		Spec.DurationSeconds = 0.2f;
		Spec.AttackSeconds = 0.01f;
		Spec.DecayCurve = 2.2f;
		Spec.HarmonicAmount = 0.15f;
		Spec.Volume = 0.45f;
		Samples = FSlinkySynthAudio::Render(Spec);
		break;
	}
	case ESlinkySfx::TimeWarning:
	{
		// A single sharp beep - ASlinkyGameMode::Tick() re-triggers this once per remaining second
		// during the countdown's last few seconds, so the repetition itself reads as urgency rather
		// than needing a busier clip.
		FSlinkyToneSpec Spec;
		Spec.StartFrequencyHz = 880.0f;
		Spec.EndFrequencyHz = 880.0f;
		Spec.DurationSeconds = 0.1f;
		Spec.AttackSeconds = 0.005f;
		Spec.DecayCurve = 3.0f;
		Spec.Volume = 0.4f;
		Samples = FSlinkySynthAudio::Render(Spec);
		break;
	}
	case ESlinkySfx::TimeUp:
		// A firmer three-note downward buzzer - the DailyChallenge/Ranked equivalent of a match
		// ending, distinct from ComboTierUp's upward flourish.
		Samples = FSlinkySynthAudio::RenderArpeggio({ 440.0f, 349.23f, 293.66f }, 0.16f, 0.55f);
		break;
	}

	return SfxSampleCache.Add(Sfx, MoveTemp(Samples));
}
