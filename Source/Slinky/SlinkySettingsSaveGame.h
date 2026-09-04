#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "SlinkySettingsSaveGame.generated.h"

// Player preferences, persisted separately from USlinkySaveGame's run records so resetting one
// never touches the other. Written/read only through USlinkyGameInstance.
UCLASS()
class USlinkySettingsSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	static const FString SlotName;
	static constexpr int32 UserIndex = 0;

	UPROPERTY()
	float MasterVolume = 1.0f;

	UPROPERTY()
	bool bEffectsEnabled = true;

	UPROPERTY()
	bool bComboDisplayEnabled = true;
};
