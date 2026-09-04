#include "SlinkyPlayerController.h"
#include "SlinkyActor.h"
#include "SlinkyControlPanel.h"
#include "SlinkyPauseMenu.h"
#include "SlinkyStaircase.h"
#include "Camera/CameraActor.h"
#include "EngineUtils.h"
#include "InputCoreTypes.h"

ASlinkyPlayerController::ASlinkyPlayerController()
{
	bAutoManageActiveCameraTarget = false;
	bShowMouseCursor = true;
	bEnableClickEvents = true;
	bEnableMouseOverEvents = true;
}

void ASlinkyPlayerController::BeginPlay()
{
	Super::BeginPlay();
	AcquireSlinky();
	AcquireStaircase();

	FInputModeGameAndUI InputMode;
	InputMode.SetHideCursorDuringCapture(false);
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(InputMode);

	ControlPanel = CreateWidget<USlinkyControlPanel>(this, USlinkyControlPanel::StaticClass());
	if (ControlPanel)
	{
		ControlPanel->AddToViewport();
	}

	PauseMenu = CreateWidget<USlinkyPauseMenu>(this, USlinkyPauseMenu::StaticClass());
	if (PauseMenu)
	{
		// Above ControlPanel (default Z-order 0) so the paused overlay isn't hidden behind it.
		PauseMenu->AddToViewport(10);
	}
}

void ASlinkyPlayerController::RefreshControlPanelFromLiveValues()
{
	if (ControlPanel && Staircase && Slinky)
	{
		ControlPanel->SetTargets(Staircase, Slinky);
	}
}

void ASlinkyPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	InputComponent->BindKey(EKeys::LeftMouseButton, IE_Pressed, this, &ASlinkyPlayerController::BeginDrag);
	InputComponent->BindKey(EKeys::LeftMouseButton, IE_Released, this, &ASlinkyPlayerController::EndDrag);
	InputComponent->BindKey(EKeys::R, IE_Pressed, this, &ASlinkyPlayerController::RestartSlinky);

	// Live stair tuning: arrow keys resize the tread depth/rise, Page Up/Down resizes the riser
	// thickness. RefreshLayout() re-slides the already-built steps immediately, no restart needed.
	InputComponent->BindKey(EKeys::Right, IE_Pressed, this, &ASlinkyPlayerController::IncreaseStepDepth);
	InputComponent->BindKey(EKeys::Left, IE_Pressed, this, &ASlinkyPlayerController::DecreaseStepDepth);
	InputComponent->BindKey(EKeys::Up, IE_Pressed, this, &ASlinkyPlayerController::IncreaseStepRise);
	InputComponent->BindKey(EKeys::Down, IE_Pressed, this, &ASlinkyPlayerController::DecreaseStepRise);
	InputComponent->BindKey(EKeys::PageUp, IE_Pressed, this, &ASlinkyPlayerController::IncreaseRiserThickness);
	InputComponent->BindKey(EKeys::PageDown, IE_Pressed, this, &ASlinkyPlayerController::DecreaseRiserThickness);

	// Live coil tuning: Tab selects which parameter [ and ] adjust (see ASlinkyActor::ETuningParam).
	InputComponent->BindKey(EKeys::Tab, IE_Pressed, this, &ASlinkyPlayerController::CycleCoilTuningParam);
	InputComponent->BindKey(EKeys::LeftBracket, IE_Pressed, this, &ASlinkyPlayerController::DecreaseCoilTuningParam);
	InputComponent->BindKey(EKeys::RightBracket, IE_Pressed, this, &ASlinkyPlayerController::IncreaseCoilTuningParam);

	InputComponent->BindKey(EKeys::Escape, IE_Pressed, this, &ASlinkyPlayerController::TogglePauseMenu);
}

void ASlinkyPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);
	AcquireSlinky();
	AcquireStaircase();
	if (Slinky && GetViewTarget() != Slinky->GetFollowCameraActor())
	{
		SetViewTarget(Slinky->GetFollowCameraActor());
	}
	if (ControlPanel && !bControlPanelTargetsSet && Slinky && Staircase)
	{
		ControlPanel->SetTargets(Staircase, Slinky);
		bControlPanelTargetsSet = true;
	}
	if (!Slinky || !Slinky->IsDragging())
	{
		return;
	}

	// The mouse-up event can be missed if the cursor leaves the viewport or the window loses
	// focus while dragging, which would otherwise leave the slinky grabbed forever and drag the
	// camera focus off toward wherever that stale grab point ends up. Treat the actual button
	// state as the source of truth instead of relying solely on the Released event.
	if (!IsInputKeyDown(EKeys::LeftMouseButton))
	{
		EndDrag();
		return;
	}

	FVector Target;
	if (ProjectCursorToPlayPlane(Target))
	{
		Slinky->UpdateDragTarget(Target);
	}
}

void ASlinkyPlayerController::AcquireSlinky()
{
	if (Slinky)
	{
		return;
	}

	for (TActorIterator<ASlinkyActor> It(GetWorld()); It; ++It)
	{
		Slinky = *It;
		SetViewTarget(Slinky->GetFollowCameraActor());
		return;
	}
}

void ASlinkyPlayerController::AcquireStaircase()
{
	if (Staircase)
	{
		return;
	}

	for (TActorIterator<ASlinkyStaircase> It(GetWorld()); It; ++It)
	{
		Staircase = *It;
		return;
	}
}

void ASlinkyPlayerController::IncreaseStepDepth()
{
	if (Staircase)
	{
		Staircase->SetStepDepth(FMath::Max(Staircase->StepDepth + 2.0f, 20.0f));
	}
}

void ASlinkyPlayerController::DecreaseStepDepth()
{
	if (Staircase)
	{
		Staircase->SetStepDepth(FMath::Max(Staircase->StepDepth - 2.0f, 20.0f));
	}
}

void ASlinkyPlayerController::IncreaseStepRise()
{
	if (Staircase)
	{
		Staircase->SetStepRise(FMath::Max(Staircase->StepRise + 2.0f, 10.0f));
	}
}

void ASlinkyPlayerController::DecreaseStepRise()
{
	if (Staircase)
	{
		Staircase->SetStepRise(FMath::Max(Staircase->StepRise - 2.0f, 10.0f));
	}
}

void ASlinkyPlayerController::IncreaseRiserThickness()
{
	if (Staircase)
	{
		Staircase->SetRiserThickness(FMath::Max(Staircase->RiserThickness + 1.0f, 1.0f));
	}
}

void ASlinkyPlayerController::DecreaseRiserThickness()
{
	if (Staircase)
	{
		Staircase->SetRiserThickness(FMath::Max(Staircase->RiserThickness - 1.0f, 1.0f));
	}
}

void ASlinkyPlayerController::CycleCoilTuningParam()
{
	if (Slinky)
	{
		Slinky->CycleTuningParam();
	}
}

void ASlinkyPlayerController::IncreaseCoilTuningParam()
{
	if (Slinky)
	{
		Slinky->AdjustTuningParam(1.0f);
	}
}

void ASlinkyPlayerController::DecreaseCoilTuningParam()
{
	if (Slinky)
	{
		Slinky->AdjustTuningParam(-1.0f);
	}
}

void ASlinkyPlayerController::TogglePauseMenu()
{
	if (PauseMenu)
	{
		PauseMenu->TogglePause();
	}
}

void ASlinkyPlayerController::BeginDrag()
{
	if (!Slinky)
	{
		return;
	}

	FVector HitPoint;
	if (ProjectCursorToPlayPlane(HitPoint))
	{
		Slinky->BeginDrag(HitPoint);
	}
}

void ASlinkyPlayerController::EndDrag()
{
	if (Slinky)
	{
		Slinky->EndDrag();
	}
}

void ASlinkyPlayerController::RestartSlinky()
{
	if (Slinky)
	{
		Slinky->ResetSlinky();
	}
}

bool ASlinkyPlayerController::ProjectCursorToPlayPlane(FVector& WorldPosition) const
{
	FVector RayOrigin;
	FVector RayDirection;
	if (!DeprojectMousePositionToWorld(RayOrigin, RayDirection) || FMath::IsNearlyZero(RayDirection.Y))
	{
		return false;
	}

	const double Distance = -RayOrigin.Y / RayDirection.Y;
	if (Distance <= 0.0)
	{
		return false;
	}

	WorldPosition = RayOrigin + RayDirection * Distance;
	WorldPosition.Y = 0.0;
	return true;
}
