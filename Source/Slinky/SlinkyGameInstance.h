#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "SlinkyChallengeTypes.h"
#include "SlinkySynthAudio.h"
#include "SlinkyGameInstance.generated.h"

class UAudioComponent;
class USlinkyLeaderboardSaveGame;
class USlinkySaveGame;
class USlinkySettingsSaveGame;
class USoundBase;
class USoundMix;

// The one place level transitions, save/load, and player preferences are driven from, so the
// title screen, the pause menu, and the game level all go through the same entry points instead
// of each hardcoding level paths, save-slot details, or how volume actually gets applied.
UCLASS()
class USlinkyGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
	USlinkyGameInstance();
	virtual void Init() override;
	// BGM starts here, not Init(): Init() runs before this GameInstance has a World yet, so
	// PlayBackgroundMusic()'s SpawnSound2D call couldn't resolve a WorldContextObject that early
	// and silently did nothing. OnStart() is the engine's own "world now exists" hook, fired once.
	virtual void OnStart() override;

	// Fire-and-forget code-synthesized SFX (see FSlinkySynthAudio/ESlinkySfx) - built once per Sfx
	// on first use and cached, so repeated plays (e.g. every landed step) are just a queue+spawn,
	// not a re-render. PitchMultiplier lets one cached clip cover a small family of sounds (e.g.
	// step pitch rising with combo) without needing a distinct clip per pitch.
	void PlaySfx(ESlinkySfx Sfx, float PitchMultiplier = 1.0f);

	bool HasSaveGame() const { return LoadedSave != nullptr; }
	int32 GetSavedBestCombo() const;
	float GetSavedBestDepthMeters() const;
	// Where "つづきから" resumes - see USlinkySaveGame::ContinueDepthMeters.
	float GetContinueDepthMeters() const;

	// The actual in-game save trigger (when to call this) is added later; this is just the entry
	// point it will call. InCurrentDepthMeters becomes the exact "つづきから" resume point
	// (always overwritten) and also raises BestDepthMeters/BestCombo if it's a new record (merged,
	// so a worse run never erases a previous one).
	void SaveProgress(int32 InBestCombo, float InCurrentDepthMeters);

	// Set by the title screen just before OpenLevel; ASlinkyGameMode consumes it once on the new
	// level's StartPlay to decide whether to seed the run's BestCombo from the save file.
	bool ConsumePendingContinue();

	// Scene management: Title <-> Game. The only two levels the game currently has, but routed
	// through here rather than UGameplayStatics::OpenLevel calls scattered across widgets/actors.
	void GoToTitle();
	// Mode defaults to FreePlay (はじめから/つづきから); a non-FreePlay Mode forces bContinue off -
	// continuing a saved FreePlay run into a locked-ruleset mode wouldn't mean anything.
	void GoToGame(bool bContinue, ESlinkyGameMode Mode = ESlinkyGameMode::FreePlay);

	// Set (via GoToGame above) just before OpenLevel; ASlinkyGameMode consumes it once on the new
	// level's StartPlay, same pattern as ConsumePendingContinue.
	ESlinkyGameMode ConsumePendingGameMode();

	// Today's UTC-dated daily challenge - deterministically generated, so every player sees the
	// same one - see FSlinkyDailyChallenge.
	FString GetTodayChallengeId() const;
	FSlinkyChallengeConfig GetTodaysChallengeConfig() const;
	FTimespan GetTimeUntilNextChallenge() const;

	// Called once a DailyChallenge/Ranked run ends (see USlinkyPauseMenu::OnSaveAndQuitClicked and
	// ASlinkyGameMode::FinishChallengeRun) - merges into the matching local leaderboard, ranked by
	// StepCount (see FSlinkyLocalRecord), never overwriting a better existing entry. A FreePlay
	// Mode is a no-op: those runs aren't ranked against anything.
	void RecordChallengeResult(ESlinkyGameMode Mode, int32 StepCount, float DepthMeters, int32 BestCombo);

	// Local-only leaderboard reads - see USlinkyLeaderboardSaveGame. The Ranked list is sorted
	// descending by depth and capped at MaxEntries; the daily lookup returns false if that day was
	// never played.
	TArray<FSlinkyLocalRecord> GetRankedLeaderboard(int32 MaxEntries = 10) const;
	bool GetDailyBest(const FString& ChallengeId, FSlinkyLocalRecord& OutRecord) const;

	// Preferences (pause menu) - each setter applies immediately and persists to its own save
	// slot, independent of run-record saves so resetting one never touches the other.
	float GetMasterVolume() const;
	void SetMasterVolume(float NewVolume);
	bool AreEffectsEnabled() const;
	void SetEffectsEnabled(bool bEnabled);
	bool IsComboDisplayEnabled() const;
	void SetComboDisplayEnabled(bool bEnabled);

private:
	void ApplyMasterVolume();
	void SaveSettings();
	void SaveLeaderboard();

	// Starts the one BackgroundMusicSound (see the constructor) looping for the rest of the
	// process's lifetime - called once from Init(). Living on the GameInstance rather than being
	// (re)triggered per-level is what lets it play continuously straight through Title<->Game
	// transitions instead of restarting or cutting out.
	void PlayBackgroundMusic();

	// Lazily renders (see FSlinkySynthAudio) and caches the raw samples for one ESlinkySfx - the
	// math only runs the first time a given effect is played.
	const TArray<int16>& GetOrBuildSfxSamples(ESlinkySfx Sfx);

	UPROPERTY()
	TObjectPtr<USlinkySaveGame> LoadedSave;

	// DailyChallenge/Ranked run results - see RecordChallengeResult/GetRankedLeaderboard/
	// GetDailyBest above. Separate save slot from LoadedSave/Settings so wiping one never touches
	// the others.
	UPROPERTY()
	TObjectPtr<USlinkyLeaderboardSaveGame> Leaderboard;

	// Plain struct-like value, not a UPROPERTY save object reference, so it always has sane
	// defaults even before any settings file has ever been written.
	UPROPERTY()
	TObjectPtr<USlinkySettingsSaveGame> Settings;

	// Runtime-constructed (no content asset needed) SoundMix used to scale the engine's default
	// Master SoundClass - see ApplyMasterVolume(). A no-op until something in the game actually
	// plays a sound, but means the slider already does the right thing once one does.
	UPROPERTY()
	TObjectPtr<USoundMix> VolumeSoundMix;

	bool bPendingContinue = false;
	ESlinkyGameMode PendingGameMode = ESlinkyGameMode::FreePlay;

	// Loaded once here via ConstructorHelpers (only valid inside a UObject's own constructor - see
	// ASlinkyHUD::ComboFont for the same pattern), NOT via a runtime LoadObject() call: a plain
	// string-path LoadObject() is invisible to the cooker's static reference analysis, so a
	// packaged build would cook without it and PlayBackgroundMusic() would silently do nothing.
	UPROPERTY()
	TObjectPtr<USoundBase> BackgroundMusicSound;

	UPROPERTY()
	TObjectPtr<UAudioComponent> MusicComponent;

	// Not a UPROPERTY: plain PCM sample data, not a UObject reference, so it needs no reflection or
	// GC tracking - see GetOrBuildSfxSamples/PlaySfx.
	TMap<ESlinkySfx, TArray<int16>> SfxSampleCache;
};
