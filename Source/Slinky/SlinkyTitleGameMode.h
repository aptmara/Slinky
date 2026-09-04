#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "SlinkyTitleGameMode.generated.h"

// The title level's GameMode: no pawn, no HUD, just ASlinkyTitlePlayerController putting up
// USlinkyTitleScreen. Set as this level's World Settings > GameMode Override so the main game
// level can keep using ASlinkyGameMode as the project's global default.
UCLASS()
class ASlinkyTitleGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ASlinkyTitleGameMode();
};
