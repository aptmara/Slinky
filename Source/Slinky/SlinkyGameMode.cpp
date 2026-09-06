// Copyright Epic Games, Inc. All Rights Reserved.

#include "SlinkyGameMode.h"
#include "SlinkyActor.h"
#include "SlinkyGameInstance.h"
#include "SlinkyHUD.h"
#include "SlinkyPlayerController.h"
#include "SlinkyStaircase.h"
#include "Camera/CameraActor.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/LightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/SkyLight.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

ASlinkyGameMode::ASlinkyGameMode()
{
	DefaultPawnClass = nullptr;
	PlayerControllerClass = ASlinkyPlayerController::StaticClass();
	HUDClass = ASlinkyHUD::StaticClass();
	PrimaryActorTick.bCanEverTick = true;

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> CosmicMaterialFinder(
		TEXT("/Game/Slinky/M_DepthBackdrop.M_DepthBackdrop"));
	if (CosmicMaterialFinder.Succeeded())
	{
		CosmicBackdropBaseMaterial = CosmicMaterialFinder.Object;
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> RoomMaterialFinder(
		TEXT("/Game/Slinky/M_RoomBackdrop.M_RoomBackdrop"));
	if (RoomMaterialFinder.Succeeded())
	{
		RoomBackdropBaseMaterial = RoomMaterialFinder.Object;
	}
}

void ASlinkyGameMode::StartPlay()
{
	Super::StartPlay();

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	Staircase = World->SpawnActor<ASlinkyStaircase>(FVector::ZeroVector, FRotator::ZeroRotator);
	Slinky = World->SpawnActor<ASlinkyActor>(FVector(60.0, 0.0, 0.0), FRotator::ZeroRotator);

	// Which ruleset (自由/デイリー/ランク) the title screen selected before OpenLevel-ing here -
	// consumed once, same reasoning as ConsumePendingContinue below (a later in-level restart must
	// not re-roll it). DailyChallenge/Ranked immediately overwrite every field ApplyChallengeConfig
	// touches; "つづきから" only ever applies to FreePlay (GoToGame() itself forces bContinue off
	// for any other Mode) - continuing a saved FreePlay run into a locked-ruleset mode wouldn't mean
	// anything anyway.
	if (USlinkyGameInstance* GameInstance = GetGameInstance<USlinkyGameInstance>())
	{
		CurrentGameMode = GameInstance->ConsumePendingGameMode();
		switch (CurrentGameMode)
		{
		case ESlinkyGameMode::DailyChallenge:
			ActiveChallengeConfig = GameInstance->GetTodaysChallengeConfig();
			break;
		case ESlinkyGameMode::Ranked:
			ActiveChallengeConfig = FSlinkyChallengeConfig::Defaults();
			break;
		default:
			break;
		}
		if (CurrentGameMode != ESlinkyGameMode::FreePlay)
		{
			ApplyChallengeConfig(ActiveChallengeConfig);
			ChallengeTimeRemaining = ChallengeTimeLimitSeconds;
			LastTimeWarningSecond = -1;
		}

		if (GameInstance->ConsumePendingContinue() && CurrentGameMode == ESlinkyGameMode::FreePlay && Slinky)
		{
			Slinky->ApplyContinueRecord(GameInstance->GetSavedBestCombo());
			Slinky->TeleportToDepth(GameInstance->GetContinueDepthMeters());
			if (Staircase)
			{
				Staircase->SnapStepsToSlinky();
			}
		}
	}

	// Fixed indoor lighting: a soft daylight sun through the entrance window, and a gentle fog just
	// dense enough to give the endless recycled stairs some visible falloff into the distance. This
	// stays constant regardless of depth - only the view through the windows (CosmicBackdrop)
	// changes as the coil falls.
	if (ADirectionalLight* Sun = World->SpawnActor<ADirectionalLight>(FVector::ZeroVector, FRotator(-42.0, -28.0, 0.0)))
	{
		Sun->GetLightComponent()->SetLightColor(FLinearColor(1.00f, 0.96f, 0.88f));
		Sun->GetLightComponent()->SetIntensity(4.5f);
	}

	if (ASkyLight* Sky = World->SpawnActor<ASkyLight>())
	{
		Sky->GetLightComponent()->SetIntensity(0.9f);
		Sky->GetLightComponent()->SetLightColor(FLinearColor(0.95f, 0.94f, 0.90f));
		Sky->GetLightComponent()->SetMobility(EComponentMobility::Movable);
		Sky->GetLightComponent()->RecaptureSky();
	}

	if (AExponentialHeightFog* Fog = World->SpawnActor<AExponentialHeightFog>())
	{
		FogComponent = Fog->GetComponent();
		FogComponent->SetFogHeightFalloff(0.1f);
		FogComponent->SetFogDensity(0.010f);
		FogComponent->SetFogInscatteringColor(FLinearColor(0.90f, 0.88f, 0.82f));
	}

	// Far layer first, so the near layer's window holes have something behind them from frame one.
	CosmicBackdrop = World->SpawnActor<AStaticMeshActor>(FVector(0.0, CosmicDistanceY, 0.0), FRotator::ZeroRotator);
	if (CosmicBackdrop)
	{
		UStaticMeshComponent* Mesh = CosmicBackdrop->GetStaticMeshComponent();
		Mesh->SetMobility(EComponentMobility::Movable);
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Mesh->SetCastShadow(false);
		Mesh->SetRelativeScale3D(FVector(BackdropWidth / 100.0f, 1.0f, BackdropHeight / 100.0f));
		if (UStaticMesh* CubeMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")))
		{
			Mesh->SetStaticMesh(CubeMesh);
		}
		if (CosmicBackdropBaseMaterial)
		{
			CosmicBackdropMaterial = UMaterialInstanceDynamic::Create(CosmicBackdropBaseMaterial, this);
			Mesh->SetMaterial(0, CosmicBackdropMaterial);
		}
	}

	// Near layer: the wood-and-wall room. Its material (M_RoomBackdrop) is Masked and cuts an
	// evenly spaced row of window openings into the wall band, so CosmicBackdrop genuinely shows
	// through them rather than being painted over.
	Backdrop = World->SpawnActor<AStaticMeshActor>(FVector(0.0, RoomDistanceY, 0.0), FRotator::ZeroRotator);
	if (Backdrop)
	{
		UStaticMeshComponent* BackdropMesh = Backdrop->GetStaticMeshComponent();
		BackdropMesh->SetMobility(EComponentMobility::Movable);
		BackdropMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		BackdropMesh->SetCastShadow(false);
		BackdropMesh->SetRelativeScale3D(FVector(BackdropWidth / 100.0f, 1.0f, BackdropHeight / 100.0f));

		if (UStaticMesh* CubeMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")))
		{
			BackdropMesh->SetStaticMesh(CubeMesh);
		}
		if (RoomBackdropBaseMaterial)
		{
			BackdropMaterial = UMaterialInstanceDynamic::Create(RoomBackdropBaseMaterial, this);
			BackdropMesh->SetMaterial(0, BackdropMaterial);
		}
	}

	SpawnEntranceRoom();
}

void ASlinkyGameMode::ApplyChallengeConfig(const FSlinkyChallengeConfig& Config)
{
	if (Staircase)
	{
		Staircase->SetStepDepth(Config.StepDepth);
		Staircase->SetStepRise(Config.StepRise);
		Staircase->SetRiserThickness(Config.RiserThickness);
	}

	if (Slinky)
	{
		Slinky->CoilTurns = FMath::RoundToInt(Config.CoilTurns);
		Slinky->CoilRadius = Config.CoilRadius;
		Slinky->CompactLength = Config.CompactLength;
		Slinky->WireRadius = Config.WireRadius;
		Slinky->AxialStiffnessScale = Config.AxialStiffnessScale;
		Slinky->BendStiffnessScale = Config.BendStiffnessScale;
		Slinky->DampingScale = Config.DampingScale;
		Slinky->Restitution = Config.Restitution;
		Slinky->Friction = Config.Friction;
		// Same clamp USlinkyControlPanel::AdjustValue applies for MaxNodeSpacing - never let the
		// joint's stretch limit sit below the coil's own compact rest spacing.
		Slinky->MaximumNodeSpacing = FMath::Max(Config.MaximumNodeSpacing, Slinky->GetNodeRestSpacing() + 1.0f);

		Slinky->RebuildHelixSegments();
		Slinky->RefreshCompactLength();
		Slinky->RefreshNodeScale();
		Slinky->RefreshCoilTuning();

		// CoilRadius change: the stairs' own width tracks it (see ASlinkyStaircase::GetCoilRadius),
		// so a wider/narrower coil needs the treads and risers re-slid to match - same follow-up
		// USlinkyControlPanel::ApplyValue's CoilRadius case does.
		if (Staircase)
		{
			Staircase->RefreshLayout();
		}
	}
}

void ASlinkyGameMode::FinishChallengeRun()
{
	USlinkyGameInstance* GameInstance = GetGameInstance<USlinkyGameInstance>();
	if (!GameInstance)
	{
		return;
	}

	GameInstance->PlaySfx(ESlinkySfx::TimeUp);
	GameInstance->RecordChallengeResult(CurrentGameMode,
		Slinky ? Slinky->GetStepCount() : 0,
		CurrentDepthMeters,
		Slinky ? Slinky->GetBestCombo() : 0);
	GameInstance->GoToTitle();
}

AStaticMeshActor* ASlinkyGameMode::SpawnBlock(const FVector& Center, const FVector& Size, const TCHAR* MaterialPath)
{
	UWorld* World = GetWorld();
	AStaticMeshActor* Block = World ? World->SpawnActor<AStaticMeshActor>(Center, FRotator::ZeroRotator) : nullptr;
	if (!Block)
	{
		return nullptr;
	}

	UStaticMeshComponent* Mesh = Block->GetStaticMeshComponent();
	// AStaticMeshActor's component starts life Static (that's the actor's whole purpose), so
	// SetStaticMesh below would still warn ("...but Mobility is Static") even with the final
	// SetMobility(Static) moved after it - the component was already static before this function
	// ever touched it. Forcing Movable first, then back to Static once fully configured, is what
	// actually avoids the warning.
	Mesh->SetMobility(EComponentMobility::Movable);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetRelativeScale3D(Size / 100.0f);

	if (UStaticMesh* CubeMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")))
	{
		Mesh->SetStaticMesh(CubeMesh);
	}
	if (UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr, MaterialPath))
	{
		Mesh->SetMaterial(0, Material);
	}

	// Set only after the mesh/material are assigned: setting Static mobility first and then
	// changing the mesh logs "Calling SetStaticMesh ... but Mobility is Static" and leaves the
	// component's render state out of sync with what it's actually showing.
	Mesh->SetMobility(EComponentMobility::Static);
	return Block;
}

void ASlinkyGameMode::SpawnEntranceRoom()
{
	// The plaster entry-room walls that used to sit here (Y=350ish, right around the 0m mark) are
	// removed - unwanted "special terrain" this close to the start. The window itself is kept
	// below, just commented out, in case it's wanted back later.

	// const TCHAR* Wood = TEXT("/Game/Slinky/M_Wood.M_Wood");
	// const float FrameY = 320.0f; // In front of the (now-removed) wall face, so the casing reads as applied trim.
	// const float WinMinX = 0.0f, WinMaxX = 300.0f;
	// const float WinMinZ = 250.0f, WinMaxZ = 550.0f;
	// constexpr float WallThickness = 20.0f;
	//
	// // Wood casing around the opening, overlapping its edge by CasingOverlap so it reads as applied
	// // trim rather than a gap-filling patch, plus one vertical and one horizontal muntin splitting
	// // the opening into four panes.
	// constexpr float CasingOverlap = 30.0f;
	// constexpr float CasingDepth = 36.0f;
	// constexpr float MuntinDepth = 18.0f;
	// const float WinCenterX = (WinMinX + WinMaxX) * 0.5f;
	// const float WinCenterZ = (WinMinZ + WinMaxZ) * 0.5f;
	//
	// SpawnBlock(FVector(WinCenterX, FrameY, WinMaxZ), FVector(WinMaxX - WinMinX + CasingOverlap * 2.0f, WallThickness, CasingDepth), Wood);
	// SpawnBlock(FVector(WinCenterX, FrameY, WinMinZ), FVector(WinMaxX - WinMinX + CasingOverlap * 2.0f, WallThickness, CasingDepth), Wood);
	// SpawnBlock(FVector(WinMinX, FrameY, WinCenterZ), FVector(CasingDepth, WallThickness, WinMaxZ - WinMinZ + CasingOverlap * 2.0f), Wood);
	// SpawnBlock(FVector(WinMaxX, FrameY, WinCenterZ), FVector(CasingDepth, WallThickness, WinMaxZ - WinMinZ + CasingOverlap * 2.0f), Wood);
	// SpawnBlock(FVector(WinCenterX, FrameY, WinCenterZ), FVector(WinMaxX - WinMinX, WallThickness, MuntinDepth), Wood);
	// SpawnBlock(FVector(WinCenterX, FrameY, WinCenterZ), FVector(MuntinDepth, WallThickness, WinMaxZ - WinMinZ), Wood);
}

bool ASlinkyGameMode::ShouldColumnHaveWindowFrame(int32 WindowCol)
{
	// A deterministic integer hash rather than FMath::Rand - the same column must always decide
	// the same way regardless of when/from which direction it's first seen, or the frame would pop
	// in and out as the camera scrolls back and forth across it. Roughly one window in four.
	//
	// A single multiply-then-shift (the previous version here) is NOT enough: multiplying by an
	// odd constant leaves the result's low bits a near-linear function of the input's low bits
	// (2654435761 % 4 == 1, so (WindowCol * 2654435761) % 4 == WindowCol % 4 exactly), and reading
	// those low bits gave a striking real bug - always true for negative columns, always false for
	// positive ones. Thomas Wang's 32-bit mix below actually spreads entropy into the low bits
	// before they're read.
	uint32 Hash = static_cast<uint32>(WindowCol);
	Hash = (Hash ^ 61u) ^ (Hash >> 16);
	Hash = Hash + (Hash << 3);
	Hash = Hash ^ (Hash >> 4);
	Hash = Hash * 0x27d4eb2du;
	Hash = Hash ^ (Hash >> 15);
	return (Hash % 4) == 0;
}

void ASlinkyGameMode::SpawnWindowFrame(int32 WindowCol, TArray<AStaticMeshActor*>& OutBlocks)
{
	if (!Staircase || Staircase->StepDepth <= 0.0f)
	{
		return;
	}

	const TCHAR* Wood = TEXT("/Game/Slinky/M_Wood.M_Wood");
	const float StepDepthValue = Staircase->StepDepth;
	const float StepRiseValue = Staircase->StepRise;
	const FVector2D Anchor = Staircase->GetShaderBoundaryOrigin();

	// Mirrors M_RoomBackdrop's Custom "RoomSplit" HLSL node's own window-placement math exactly
	// (stepsPerWindow/windowWidth/windowHeight/windowHeightOffset/BoundaryMargin are that node's
	// literals, copied here rather than exposed as parameters, since only this function needs
	// them) - so this 3D trim lands precisely on top of the see-through hole the shader already
	// cuts for this column, rather than this function making its own separate decision about where
	// windows are.
	constexpr float StepsPerWindow = 6.0f;
	constexpr float WindowWidth = 220.0f;
	constexpr float WindowHeight = 220.0f;
	constexpr float WindowHeightOffset = 460.0f;
	constexpr float BoundaryMargin = -20.0f;

	const float WindowSpacingX = StepDepthValue * StepsPerWindow;
	const float WindowCenterXLocal = (static_cast<float>(WindowCol) + 0.5f) * WindowSpacingX;
	const float WindowCenterX = WindowCenterXLocal + Anchor.X;
	const float ColStepIndex = FMath::FloorToFloat(WindowCenterXLocal / StepDepthValue);
	const float ColBoundaryZ = Anchor.Y - ColStepIndex * StepRiseValue + BoundaryMargin;
	const float WindowCenterZ = ColBoundaryZ + WindowHeightOffset;

	// In front of the backdrop card's near face (RoomDistanceY minus half its 100uu thickness) by
	// the same margin ASlinkyGameMode::SpawnEntranceRoom used to give its one-off entrance window's
	// casing, so this trim reads as applied to the wall rather than floating in front of it.
	const float FrameY = RoomDistanceY - 50.0f - 20.0f;
	constexpr float ProtrusionY = 20.0f;
	// BarWidth matches the shader's own frameWidth exactly, so a framed window's 3D trim lines up
	// with the flat painted wood-frame mask still visible on every other (unframed) window.
	constexpr float BarWidth = 26.0f;
	constexpr float BarOverlap = 20.0f;
	constexpr float MuntinWidth = 14.0f;

	const float WinMinX = WindowCenterX - WindowWidth * 0.5f;
	const float WinMaxX = WindowCenterX + WindowWidth * 0.5f;
	const float WinMinZ = WindowCenterZ - WindowHeight * 0.5f;
	const float WinMaxZ = WindowCenterZ + WindowHeight * 0.5f;

	const auto AddBlock = [this, Wood, &OutBlocks](const FVector& Center, const FVector& Size)
	{
		if (AStaticMeshActor* Block = SpawnBlock(Center, Size, Wood))
		{
			OutBlocks.Add(Block);
		}
	};

	AddBlock(FVector(WindowCenterX, FrameY, WinMaxZ), FVector(WindowWidth + BarOverlap * 2.0f, ProtrusionY, BarWidth));
	AddBlock(FVector(WindowCenterX, FrameY, WinMinZ), FVector(WindowWidth + BarOverlap * 2.0f, ProtrusionY, BarWidth));
	AddBlock(FVector(WinMinX, FrameY, WindowCenterZ), FVector(BarWidth, ProtrusionY, WindowHeight + BarOverlap * 2.0f));
	AddBlock(FVector(WinMaxX, FrameY, WindowCenterZ), FVector(BarWidth, ProtrusionY, WindowHeight + BarOverlap * 2.0f));
	AddBlock(FVector(WindowCenterX, FrameY, WindowCenterZ), FVector(WindowWidth, ProtrusionY, MuntinWidth));
	AddBlock(FVector(WindowCenterX, FrameY, WindowCenterZ), FVector(MuntinWidth, ProtrusionY, WindowHeight));
}

void ASlinkyGameMode::UpdateWindowFrames(const FVector& CameraLocation)
{
	if (!Staircase || Staircase->StepDepth <= 0.0f)
	{
		return;
	}

	constexpr float StepsPerWindow = 6.0f;
	const float WindowSpacingX = Staircase->StepDepth * StepsPerWindow;
	const float AnchorX = Staircase->GetShaderBoundaryOrigin().X;

	// Which window columns are currently within the backdrop card's width, with one extra column
	// of margin on each side so a frame is already in place before it scrolls into view.
	const float HalfRange = BackdropWidth * 0.5f + WindowSpacingX;
	const int32 FirstCol = FMath::FloorToInt((CameraLocation.X - HalfRange - AnchorX) / WindowSpacingX);
	const int32 LastCol = FMath::FloorToInt((CameraLocation.X + HalfRange - AnchorX) / WindowSpacingX);

	for (auto It = WindowFrameBlocksByColumn.CreateIterator(); It; ++It)
	{
		if (It->Key < FirstCol || It->Key > LastCol)
		{
			for (AStaticMeshActor* Block : It->Value)
			{
				if (Block)
				{
					Block->Destroy();
				}
			}
			It.RemoveCurrent();
		}
	}

	for (int32 Col = FirstCol; Col <= LastCol; ++Col)
	{
		if (WindowFrameBlocksByColumn.Contains(Col))
		{
			continue;
		}
		// Recorded either way (even an empty array for "decided against") so this column's coin
		// flip is never re-rolled while it stays in range.
		TArray<AStaticMeshActor*>& Blocks = WindowFrameBlocksByColumn.FindOrAdd(Col);
		if (ShouldColumnHaveWindowFrame(Col))
		{
			SpawnWindowFrame(Col, Blocks);
		}
	}
}

void ASlinkyGameMode::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (!Slinky || !Backdrop || !CosmicBackdrop)
	{
		return;
	}

	CurrentDepthMeters = FMath::Max(0.0f, -Slinky->GetCenterLocation().Z / 100.0f);
	UpdateCosmicView(CurrentDepthMeters);

	if (CurrentGameMode != ESlinkyGameMode::FreePlay && !bChallengeFinished)
	{
		ChallengeTimeRemaining = FMath::Max(ChallengeTimeRemaining - DeltaTime, 0.0f);

		// One beep per whole second remaining, only inside the last 10 seconds - see
		// LastTimeWarningSecond's comment for why a simple "did the integer change" check is enough.
		constexpr int32 WarningThresholdSeconds = 10;
		const int32 WholeSecondsRemaining = FMath::CeilToInt(ChallengeTimeRemaining);
		if (WholeSecondsRemaining <= WarningThresholdSeconds && WholeSecondsRemaining > 0
			&& WholeSecondsRemaining != LastTimeWarningSecond)
		{
			LastTimeWarningSecond = WholeSecondsRemaining;
			if (USlinkyGameInstance* GameInstance = GetGameInstance<USlinkyGameInstance>())
			{
				GameInstance->PlaySfx(ESlinkySfx::TimeWarning);
			}
		}

		if (ChallengeTimeRemaining <= 0.0f)
		{
			bChallengeFinished = true;
			FinishChallengeRun();
		}
	}

	// Recentered every tick on the camera itself (not the coil, which the camera trails via its
	// dead zone) so neither card ever falls out of alignment with what's actually on screen.
	// Orthographic projection means their different Y distances don't change apparent size, only
	// which one the other's window holes reveal.
	const FVector CameraLocation = Slinky->GetFollowCameraActor()
		? Slinky->GetFollowCameraActor()->GetActorLocation()
		: Slinky->GetCenterLocation();
	Backdrop->SetActorLocation(FVector(CameraLocation.X, RoomDistanceY, CameraLocation.Z));
	CosmicBackdrop->SetActorLocation(FVector(CameraLocation.X, CosmicDistanceY, CameraLocation.Z));

	if (BackdropMaterial && Staircase && Staircase->StepDepth > 0.0f)
	{
		// The stairs' own live-tunable proportions (arrows in ASlinkyPlayerController) decide where
		// the wood/wall boundary steps down, so the backdrop's staircase-shaped fill (and its
		// window row, which rides along the wall band) always matches the real treads and risers.
		BackdropMaterial->SetScalarParameterValue(TEXT("StepDepth"), Staircase->StepDepth);
		BackdropMaterial->SetScalarParameterValue(TEXT("StepRise"), Staircase->StepRise);

		// The staircase's own step positions are anchored on whichever step is under the slinky
		// (see ASlinkyStaircase::CaptureAnchor), not on world X=0, so the shader's step-boundary
		// formula needs that same anchor or it silently drifts out of sync with the real treads and
		// risers every time StepDepth/StepRise/RiserThickness changes (or the slinky simply walks
		// far enough from the origin).
		const FVector2D BoundaryOrigin = Staircase->GetShaderBoundaryOrigin();
		BackdropMaterial->SetScalarParameterValue(TEXT("AnchorX"), BoundaryOrigin.X);
		BackdropMaterial->SetScalarParameterValue(TEXT("AnchorZ"), BoundaryOrigin.Y);
	}

	UpdateWindowFrames(CameraLocation);
}

const TArray<ASlinkyGameMode::FDepthZone>& ASlinkyGameMode::GetDepthZones()
{
	// The ramp reads as one continuous trip rather than a literal geology cross-section: it opens
	// in open sky, sinks through familiar earth and lava, then keeps going past where the ground
	// would end - polar night, aurora, deep sea, storm, desert, forest, snow - and out through the
	// stratosphere into space, a nebula, a black hole, and finally the void. Depth spacing widens
	// zone by zone (10m near the surface, 44m by the end). This is what shows through the room's
	// windows, not the room's own (fixed) lighting.
	static const TArray<FDepthZone> Zones = {
		// depth  name              sky top                     sky bottom                  accent color               amt   mode  scale  thresh sharp  scrollX scrollZ
		{ 0.0f,   TEXT("Blue Sky"),      FLinearColor(0.35f,0.62f,0.92f), FLinearColor(0.78f,0.88f,0.96f), FLinearColor(0.98f,0.99f,1.00f), 0.55f, 0.0f, 0.004f, 0.45f, 2.5f,  3.0f,  0.0f },
		{ 10.0f,  TEXT("Sunset"),        FLinearColor(0.30f,0.18f,0.42f), FLinearColor(0.95f,0.45f,0.25f), FLinearColor(1.00f,0.55f,0.35f), 0.45f, 0.0f, 0.005f, 0.50f, 3.0f,  2.0f,  0.0f },
		{ 20.0f,  TEXT("Topsoil"),       FLinearColor(0.42f,0.30f,0.22f), FLinearColor(0.24f,0.16f,0.11f), FLinearColor(0.15f,0.10f,0.07f), 0.35f, 1.0f, 0.050f, 0.50f, 2.0f,  0.0f,  0.0f },
		{ 32.0f,  TEXT("Bedrock"),       FLinearColor(0.24f,0.25f,0.28f), FLinearColor(0.12f,0.13f,0.15f), FLinearColor(0.08f,0.08f,0.09f), 0.40f, 1.0f, 0.030f, 0.50f, 3.0f,  0.0f,  0.0f },
		{ 46.0f,  TEXT("Aquifer"),       FLinearColor(0.10f,0.24f,0.27f), FLinearColor(0.03f,0.10f,0.12f), FLinearColor(0.75f,0.95f,0.95f), 0.60f, 1.0f, 0.020f, 0.72f, 6.0f,  0.0f,  18.0f },
		{ 62.0f,  TEXT("Crystal Cavern"),FLinearColor(0.14f,0.09f,0.20f), FLinearColor(0.05f,0.03f,0.09f), FLinearColor(0.85f,0.65f,1.00f), 0.70f, 2.0f, 0.040f, 0.80f, 8.0f,  0.0f,  1.0f },
		{ 80.0f,  TEXT("Lava Zone"),     FLinearColor(0.45f,0.12f,0.05f), FLinearColor(0.14f,0.02f,0.01f), FLinearColor(1.00f,0.50f,0.15f), 0.65f, 1.0f, 0.020f, 0.70f, 5.0f,  0.0f,  12.0f },
		{ 100.0f, TEXT("Molten Core"),   FLinearColor(0.65f,0.20f,0.04f), FLinearColor(0.20f,0.04f,0.01f), FLinearColor(1.00f,0.85f,0.50f), 0.75f, 1.0f, 0.030f, 0.55f, 4.0f,  5.0f,  30.0f },
		{ 122.0f, TEXT("Polar Night"),   FLinearColor(0.06f,0.09f,0.20f), FLinearColor(0.02f,0.03f,0.08f), FLinearColor(0.85f,0.90f,1.00f), 0.50f, 2.0f, 0.015f, 0.85f, 10.0f, 0.0f,  0.0f },
		{ 146.0f, TEXT("Aurora"),        FLinearColor(0.08f,0.30f,0.24f), FLinearColor(0.03f,0.08f,0.14f), FLinearColor(1.00f,1.00f,1.00f), 0.65f, 3.0f, 0.006f, 0.50f, 1.5f,  25.0f, 0.0f },
		{ 172.0f, TEXT("Deep Sea"),      FLinearColor(0.03f,0.08f,0.20f), FLinearColor(0.01f,0.02f,0.06f), FLinearColor(0.35f,0.85f,0.95f), 0.50f, 1.0f, 0.018f, 0.78f, 6.0f,  2.0f,  6.0f },
		{ 200.0f, TEXT("Storm Clouds"),  FLinearColor(0.22f,0.22f,0.28f), FLinearColor(0.10f,0.10f,0.14f), FLinearColor(0.65f,0.65f,0.72f), 0.50f, 0.0f, 0.006f, 0.40f, 2.0f,  20.0f, 0.0f },
		{ 230.0f, TEXT("Desert"),        FLinearColor(0.55f,0.72f,0.85f), FLinearColor(0.85f,0.65f,0.42f), FLinearColor(0.95f,0.80f,0.55f), 0.35f, 0.0f, 0.012f, 0.50f, 2.0f,  4.0f,  0.0f },
		{ 262.0f, TEXT("Forest Canopy"), FLinearColor(0.20f,0.42f,0.22f), FLinearColor(0.08f,0.18f,0.09f), FLinearColor(0.45f,0.75f,0.30f), 0.45f, 1.0f, 0.035f, 0.50f, 2.5f,  3.0f,  1.0f },
		{ 296.0f, TEXT("Snowfield"),     FLinearColor(0.65f,0.78f,0.92f), FLinearColor(0.90f,0.93f,0.98f), FLinearColor(1.00f,1.00f,1.00f), 0.55f, 1.0f, 0.025f, 0.75f, 7.0f,  0.0f,  -14.0f },
		{ 332.0f, TEXT("Stratosphere"),  FLinearColor(0.02f,0.03f,0.10f), FLinearColor(0.25f,0.35f,0.62f), FLinearColor(0.85f,0.88f,0.98f), 0.30f, 0.0f, 0.008f, 0.60f, 3.0f,  6.0f,  0.0f },
		{ 370.0f, TEXT("Space"),         FLinearColor(0.00f,0.00f,0.01f), FLinearColor(0.02f,0.02f,0.05f), FLinearColor(1.00f,1.00f,1.00f), 0.80f, 2.0f, 0.020f, 0.86f, 12.0f, 0.0f,  0.0f },
		{ 410.0f, TEXT("Nebula"),        FLinearColor(0.20f,0.05f,0.30f), FLinearColor(0.05f,0.15f,0.35f), FLinearColor(0.75f,0.30f,0.80f), 0.60f, 0.0f, 0.004f, 0.42f, 1.2f,  1.5f,  1.0f },
		{ 452.0f, TEXT("Black Hole"),    FLinearColor(0.01f,0.01f,0.01f), FLinearColor(0.35f,0.15f,0.03f), FLinearColor(1.00f,1.00f,1.00f), 0.85f, 4.0f, 0.030f, 0.60f, 5.0f,  40.0f, 10.0f },
		{ 496.0f, TEXT("The Void"),      FLinearColor(0.00f,0.00f,0.00f), FLinearColor(0.00f,0.00f,0.00f), FLinearColor(0.12f,0.12f,0.13f), 0.20f, 2.0f, 0.010f, 0.95f, 15.0f, 0.0f,  0.0f },
	};
	return Zones;
}

void ASlinkyGameMode::UpdateCosmicView(float DepthMeters)
{
	if (!CosmicBackdropMaterial)
	{
		return;
	}

	const TArray<FDepthZone>& Zones = GetDepthZones();

	int32 Index = 0;
	while (Index + 1 < Zones.Num() && DepthMeters >= Zones[Index + 1].DepthMeters)
	{
		++Index;
	}
	const FDepthZone& Zone = Zones[Index];
	const FDepthZone& Next = Zones[FMath::Min(Index + 1, Zones.Num() - 1)];
	const float Span = Next.DepthMeters - Zone.DepthMeters;
	const float Alpha = Span > 0.0f ? FMath::Clamp((DepthMeters - Zone.DepthMeters) / Span, 0.0f, 1.0f) : 0.0f;

	CosmicBackdropMaterial->SetVectorParameterValue(TEXT("TopColor"), FMath::Lerp(Zone.SkyTopColor, Next.SkyTopColor, Alpha));
	CosmicBackdropMaterial->SetVectorParameterValue(TEXT("BottomColor"), FMath::Lerp(Zone.SkyBottomColor, Next.SkyBottomColor, Alpha));
	CosmicBackdropMaterial->SetVectorParameterValue(TEXT("AccentColor"), FMath::Lerp(Zone.AccentColor, Next.AccentColor, Alpha));
	CosmicBackdropMaterial->SetScalarParameterValue(TEXT("AccentAmount"), FMath::Lerp(Zone.AccentAmount, Next.AccentAmount, Alpha));
	// Snaps to whichever mode is nearer rather than blending fractionally - averaging two unrelated
	// techniques (e.g. Cloud and Voronoi Blobs) mid-transition has no sensible meaning.
	CosmicBackdropMaterial->SetScalarParameterValue(TEXT("PatternMode"), Alpha < 0.5f ? Zone.PatternMode : Next.PatternMode);
	CosmicBackdropMaterial->SetScalarParameterValue(TEXT("NoiseScale"), FMath::Lerp(Zone.NoiseScale, Next.NoiseScale, Alpha));
	CosmicBackdropMaterial->SetScalarParameterValue(TEXT("NoiseThreshold"), FMath::Lerp(Zone.NoiseThreshold, Next.NoiseThreshold, Alpha));
	CosmicBackdropMaterial->SetScalarParameterValue(TEXT("NoiseSharpness"), FMath::Lerp(Zone.NoiseSharpness, Next.NoiseSharpness, Alpha));
	CosmicBackdropMaterial->SetScalarParameterValue(TEXT("ScrollSpeedX"), FMath::Lerp(Zone.ScrollSpeedX, Next.ScrollSpeedX, Alpha));
	CosmicBackdropMaterial->SetScalarParameterValue(TEXT("ScrollSpeedZ"), FMath::Lerp(Zone.ScrollSpeedZ, Next.ScrollSpeedZ, Alpha));
}
