#include "SlinkyGameInstance.h"
#include "SlinkySaveGame.h"
#include "SlinkySettingsSaveGame.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundClass.h"
#include "Sound/SoundMix.h"

namespace
{
	const TCHAR* TitleLevelPath = TEXT("/Game/Slinky/Lvl_Title");
	const TCHAR* GameLevelPath = TEXT("/Game/Slinky/Lvl_Slinky");
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
	ApplyMasterVolume();
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

void USlinkyGameInstance::GoToGame(bool bContinue)
{
	bPendingContinue = bContinue;
	UGameplayStatics::OpenLevel(this, FName(GameLevelPath));
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
