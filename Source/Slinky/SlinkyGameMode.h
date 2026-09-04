// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "SlinkyGameMode.generated.h"

class ASlinkyActor;
class ASlinkyStaircase;
class AStaticMeshActor;
class UExponentialHeightFogComponent;
class ULightComponent;
class UMaterialInstanceDynamic;

UCLASS()
class ASlinkyGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ASlinkyGameMode();
	virtual void StartPlay() override;
	virtual void Tick(float DeltaTime) override;

	// For SlinkyHUD's readout.
	float GetCurrentDepthMeters() const { return CurrentDepthMeters; }

private:
	// One stop on the CosmicBackdrop's color ramp - what is visible through the room's windows,
	// independent of the room's own fixed indoor lighting. Depths are in meters below the top step;
	// the ramp is walked in order and linearly interpolated between whichever two entries bracket
	// the coil's current depth. See M_DepthBackdrop's shader graph for what Accent*/Noise*/
	// ScrollSpeed*/PatternMode actually drive - a hand-written HLSL function (Cloud/Blobs/Stars/
	// Aurora/BlackHole) rather than one generic noise formula.
	struct FDepthZone
	{
		float DepthMeters;
		const TCHAR* Name;
		FLinearColor SkyTopColor;
		FLinearColor SkyBottomColor;
		FLinearColor AccentColor;
		float AccentAmount;
		float PatternMode;
		float NoiseScale;
		float NoiseThreshold;
		float NoiseSharpness;
		float ScrollSpeedX;
		float ScrollSpeedZ;
	};

	static const TArray<FDepthZone>& GetDepthZones();
	void UpdateCosmicView(float DepthMeters);

	// A plain block of geometry with one flat-shaded material - the one primitive every entrance
	// piece (wall segment or window frame bar) below is built from.
	void SpawnBlock(const FVector& Center, const FVector& Size, const TCHAR* MaterialPath);

	// Builds the entryway the coil starts inside: a white plaster wall (built as four segments
	// around a rectangular gap, since there's no boolean-subtract at runtime) with a wood-framed,
	// four-pane window in that gap.
	void SpawnEntranceRoom();

	UPROPERTY()
	TObjectPtr<ASlinkyActor> Slinky;

	UPROPERTY()
	TObjectPtr<ASlinkyStaircase> Staircase;

	UPROPERTY()
	TObjectPtr<UExponentialHeightFogComponent> FogComponent;

	// The near layer: wood-and-wall room geometry (M_RoomBackdrop), masked so its evenly spaced
	// windows are genuinely transparent and let CosmicBackdrop show through them.
	UPROPERTY()
	TObjectPtr<AStaticMeshActor> Backdrop;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> BackdropMaterial;

	// The far layer: the cosmic depth-zone sky (M_DepthBackdrop), only ever actually seen through
	// the room's window openings.
	UPROPERTY()
	TObjectPtr<AStaticMeshActor> CosmicBackdrop;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> CosmicBackdropMaterial;

	// How far along the camera's forward axis (world +Y) each card sits, and how big a flat card
	// needs to be to fill the orthographic frame at that distance (orthographic projection means
	// apparent size doesn't change with distance, so both cards share one size). Sized well past
	// OrthoWidth (900) and the 16:9 frame height so the camera's dead-zone lag never exposes an edge.
	static constexpr float RoomDistanceY = 500.0f;
	static constexpr float CosmicDistanceY = 1500.0f;
	static constexpr float BackdropWidth = 1600.0f;
	static constexpr float BackdropHeight = 1200.0f;

	float CurrentDepthMeters = 0.0f;
};
