#include "SlinkyTitleGameMode.h"
#include "SlinkyTitlePlayerController.h"

ASlinkyTitleGameMode::ASlinkyTitleGameMode()
{
	DefaultPawnClass = nullptr;
	HUDClass = nullptr;
	PlayerControllerClass = ASlinkyTitlePlayerController::StaticClass();
}
