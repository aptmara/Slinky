// Copyright Epic Games, Inc. All Rights Reserved.

#include "SlinkyGameMode.h"
#include "SlinkyActor.h"
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

ASlinkyGameMode::ASlinkyGameMode()
{
	DefaultPawnClass = nullptr;
	PlayerControllerClass = ASlinkyPlayerController::StaticClass();
	HUDClass = ASlinkyHUD::StaticClass();
	PrimaryActorTick.bCanEverTick = true;
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
		if (UMaterialInterface* CosmicBaseMaterial = LoadObject<UMaterialInterface>(
			nullptr, TEXT("/Game/Slinky/M_DepthBackdrop.M_DepthBackdrop")))
		{
			CosmicBackdropMaterial = UMaterialInstanceDynamic::Create(CosmicBaseMaterial, this);
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

		// ConstructorHelpers::FObjectFinder only works inside a UObject constructor; loading here at
		// runtime needs the plain LoadObject path instead.
		if (UStaticMesh* CubeMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")))
		{
			BackdropMesh->SetStaticMesh(CubeMesh);
		}
		if (UMaterialInterface* BackdropBaseMaterial = LoadObject<UMaterialInterface>(
			nullptr, TEXT("/Game/Slinky/M_RoomBackdrop.M_RoomBackdrop")))
		{
			BackdropMaterial = UMaterialInstanceDynamic::Create(BackdropBaseMaterial, this);
			BackdropMesh->SetMaterial(0, BackdropMaterial);
		}
	}

	SpawnEntranceRoom();
}

void ASlinkyGameMode::SpawnBlock(const FVector& Center, const FVector& Size, const TCHAR* MaterialPath)
{
	UWorld* World = GetWorld();
	AStaticMeshActor* Block = World ? World->SpawnActor<AStaticMeshActor>(Center, FRotator::ZeroRotator) : nullptr;
	if (!Block)
	{
		return;
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
}

void ASlinkyGameMode::SpawnEntranceRoom()
{
	const TCHAR* Plaster = TEXT("/Game/Slinky/M_Plaster.M_Plaster");
	const TCHAR* Wood = TEXT("/Game/Slinky/M_Wood.M_Wood");

	// Sits just behind the first several steps (Y=0) and in front of both backdrop cards.
	const float WallY = 350.0f;
	const float FrameY = 320.0f; // In front of the wall face, so the casing reads as applied trim.

	// Wall and window-opening bounds, in world X/Z. The wall is built as the four rectangles that
	// remain once the window rectangle is cut out of it, since there is no runtime CSG subtract.
	const float WallMinX = -450.0f, WallMaxX = 750.0f;
	const float WallMinZ = -350.0f, WallMaxZ = 820.0f;
	const float WinMinX = 0.0f, WinMaxX = 300.0f;
	const float WinMinZ = 250.0f, WinMaxZ = 550.0f;
	constexpr float WallThickness = 20.0f;

	// Each wall segment is given as (MinX, MaxX, MinZ, MaxZ); Center and Size are derived from that
	// one rectangle so there is only one place per segment where its bounds are stated.
	const auto SpawnWallSegment = [this, Plaster, WallY, WallThickness](float MinX, float MaxX, float MinZ, float MaxZ)
	{
		SpawnBlock(FVector((MinX + MaxX) * 0.5f, WallY, (MinZ + MaxZ) * 0.5f),
			FVector(MaxX - MinX, WallThickness, MaxZ - MinZ), Plaster);
	};

	SpawnWallSegment(WallMinX, WallMaxX, WinMaxZ, WallMaxZ); // Top strip, above the window.
	SpawnWallSegment(WallMinX, WallMaxX, WallMinZ, WinMinZ); // Bottom strip, below the window.
	SpawnWallSegment(WallMinX, WinMinX, WinMinZ, WinMaxZ);   // Left strip, beside the window.
	SpawnWallSegment(WinMaxX, WallMaxX, WinMinZ, WinMaxZ);   // Right strip, beside the window.

	// Wood casing around the opening, overlapping its edge by CasingOverlap so it reads as applied
	// trim rather than a gap-filling patch, plus one vertical and one horizontal muntin splitting
	// the opening into four panes.
	constexpr float CasingOverlap = 30.0f;
	constexpr float CasingDepth = 36.0f;
	constexpr float MuntinDepth = 18.0f;
	const float WinCenterX = (WinMinX + WinMaxX) * 0.5f;
	const float WinCenterZ = (WinMinZ + WinMaxZ) * 0.5f;

	SpawnBlock(FVector(WinCenterX, FrameY, WinMaxZ), FVector(WinMaxX - WinMinX + CasingOverlap * 2.0f, WallThickness, CasingDepth), Wood);
	SpawnBlock(FVector(WinCenterX, FrameY, WinMinZ), FVector(WinMaxX - WinMinX + CasingOverlap * 2.0f, WallThickness, CasingDepth), Wood);
	SpawnBlock(FVector(WinMinX, FrameY, WinCenterZ), FVector(CasingDepth, WallThickness, WinMaxZ - WinMinZ + CasingOverlap * 2.0f), Wood);
	SpawnBlock(FVector(WinMaxX, FrameY, WinCenterZ), FVector(CasingDepth, WallThickness, WinMaxZ - WinMinZ + CasingOverlap * 2.0f), Wood);
	SpawnBlock(FVector(WinCenterX, FrameY, WinCenterZ), FVector(WinMaxX - WinMinX, WallThickness, MuntinDepth), Wood);
	SpawnBlock(FVector(WinCenterX, FrameY, WinCenterZ), FVector(MuntinDepth, WallThickness, WinMaxZ - WinMinZ), Wood);
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
