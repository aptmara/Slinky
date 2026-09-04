#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "SlinkyTitlePlayerController.generated.h"

class USlinkyTitleScreen;

// Mirrors ASlinkyPlayerController's BeginPlay pattern (create the widget, set an appropriate
// input mode) for the title level: UI-only input with the cursor shown, no gameplay pawn/camera
// concerns to juggle.
UCLASS()
class ASlinkyTitlePlayerController : public APlayerController
{
	GENERATED_BODY()

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY()
	TObjectPtr<USlinkyTitleScreen> TitleScreen;
};
