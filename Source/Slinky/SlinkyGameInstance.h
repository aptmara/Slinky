#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "SlinkyGameInstance.generated.h"

class USlinkySaveGame;
class USlinkySettingsSaveGame;
class USoundMix;

// The one place level transitions, save/load, and player preferences are driven from, so the
// title screen, the pause menu, and the game level all go through the same entry points instead
// of each hardcoding level paths, save-slot details, or how volume actually gets applied.
UCLASS()
class USlinkyGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
	virtual void Init() override;

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
	void GoToGame(bool bContinue);

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

	UPROPERTY()
	TObjectPtr<USlinkySaveGame> LoadedSave;

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
};
