#include "SlinkyTitlePlayerController.h"
#include "SlinkyTitleScreen.h"

void ASlinkyTitlePlayerController::BeginPlay()
{
	Super::BeginPlay();

	bShowMouseCursor = true;
	FInputModeUIOnly InputMode;
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(InputMode);

	TitleScreen = CreateWidget<USlinkyTitleScreen>(this, USlinkyTitleScreen::StaticClass());
	if (TitleScreen)
	{
		TitleScreen->AddToViewport();
	}
}
