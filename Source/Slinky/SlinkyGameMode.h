// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "SlinkyChallengeTypes.h"
#include "SlinkyGameMode.generated.h"

class ASlinkyActor;
class ASlinkyStaircase;
class AStaticMeshActor;
class UExponentialHeightFogComponent;
class ULightComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;

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

	ESlinkyGameMode GetCurrentGameMode() const { return CurrentGameMode; }
	const FSlinkyChallengeConfig& GetActiveChallengeConfig() const { return ActiveChallengeConfig; }

	// FreePlay leaves the control panel and every keyboard tuning shortcut interactive; the other
	// two modes apply a fixed config at StartPlay and lock every one of those entry points, since a
	// leaderboard only means something if everyone on it played the same slinky - see
	// ASlinkyPlayerController::IsCustomizationLocked and USlinkyPauseMenu::OnResetDefaultsClicked.
	bool IsCustomizationLocked() const { return CurrentGameMode != ESlinkyGameMode::FreePlay; }

	// Seconds left before a DailyChallenge/Ranked run auto-finishes (see Tick()/FinishChallengeRun)
	// - 0 for FreePlay, which has no time limit. For SlinkyHUD's countdown readout.
	float GetChallengeTimeRemaining() const { return ChallengeTimeRemaining; }

private:
	// Pushes every field of Config onto the just-spawned Staircase/Slinky, mirroring exactly what
	// USlinkyControlPanel::ApplyValue does per-row - see StartPlay's DailyChallenge/Ranked branch.
	void ApplyChallengeConfig(const FSlinkyChallengeConfig& Config);

	// Records the run's result (StepCount as of right now) to the local leaderboard and returns to
	// the title screen - called from Tick() the instant ChallengeTimeRemaining reaches 0, the
	// DailyChallenge/Ranked equivalent of USlinkyPauseMenu::OnSaveAndQuitClicked's manual path.
	void FinishChallengeRun();
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
	// piece (wall segment or window frame bar) below is built from. Returns the spawned actor so
	// callers that need to track/recycle their blocks (see SpawnWindowFrame) can keep it.
	AStaticMeshActor* SpawnBlock(const FVector& Center, const FVector& Size, const TCHAR* MaterialPath);

	// Builds the entryway the coil starts inside: a white plaster wall (built as four segments
	// around a rectangular gap, since there's no boolean-subtract at runtime) with a wood-framed,
	// four-pane window in that gap.
	void SpawnEntranceRoom();

	// M_RoomBackdrop's shader (see its Custom "RoomSplit" node) cuts a see-through window every
	// StepsPerWindow steps along the wall band, entirely on its own - these are never the reason a
	// window exists. What this adds is 3D wood trim on top of some of those same holes, so a few
	// read as an actual built window instead of a flat painted opening; the hole underneath (and
	// so the CosmicBackdrop glimpsed through it) is identical either way.
	//
	// Recycled the same way ASlinkyStaircase recycles treads: called once a tick with the camera's
	// current world position, it spawns frames for newly-visible window columns and destroys ones
	// that scrolled out of range, keyed by their column index so a given position in the world
	// keeps making the same choice (framed or not) no matter which direction the camera crosses it
	// from.
	void UpdateWindowFrames(const FVector& CameraLocation);
	// Deterministic per-column choice (not FMath::Rand) - see UpdateWindowFrames().
	static bool ShouldColumnHaveWindowFrame(int32 WindowCol);
	// Spawns the wood casing + muntin blocks for one window column, computing that column's
	// world-space center from the *same* StepDepth/StepRise/Anchor/BoundaryMargin formula as
	// M_RoomBackdrop's shader, so the trim lands exactly on that shader's own cutout.
	void SpawnWindowFrame(int32 WindowCol, TArray<AStaticMeshActor*>& OutBlocks);

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

	// Loaded once here via ConstructorHelpers (only valid inside a UObject's own constructor - see
	// ASlinkyHUD::ComboFont for the same pattern), NOT via a runtime LoadObject() call in StartPlay
	// as these used to be: a plain string-path LoadObject() is invisible to the cooker's static
	// reference analysis, so a packaged build cooked without either material at all - the near/far
	// backdrop cards silently had no material, meaning no visible background, no windows cut into
	// the wall, and no wood-textured wall band, even though the actors themselves still spawned.
	UPROPERTY()
	TObjectPtr<UMaterialInterface> CosmicBackdropBaseMaterial;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> RoomBackdropBaseMaterial;

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

	ESlinkyGameMode CurrentGameMode = ESlinkyGameMode::FreePlay;

	// What ApplyChallengeConfig actually pushed for this run - default-constructed for FreePlay.
	// Kept around so a Daily Challenge HUD readout (e.g. "本日のスリンキー: びよーんの日") can show it.
	FSlinkyChallengeConfig ActiveChallengeConfig;

	// DailyChallenge/Ranked runs are timed so every attempt is comparable on more than just skill
	// at an otherwise-unbounded fall - FreePlay has no such limit. Counted down in Tick(); reaching
	// 0 triggers FinishChallengeRun() exactly once (see bChallengeFinished).
	static constexpr float ChallengeTimeLimitSeconds = 90.0f;
	float ChallengeTimeRemaining = 0.0f;
	bool bChallengeFinished = false;

	// Whole seconds remaining last time ESlinkySfx::TimeWarning fired, so the countdown's last few
	// seconds each get exactly one beep (on crossing the boundary) instead of one every Tick() -
	// FMath::CeilToInt(ChallengeTimeRemaining) is monotonically decreasing, so a plain "did the
	// integer change" check is enough without a separate timer. Reset to -1 whenever the timer
	// itself is (re)armed in StartPlay, so a later run's countdown always starts fresh.
	int32 LastTimeWarningSecond = -1;

	// Window-column index -> the (up to 6) wood trim blocks spawned for it. Not a UPROPERTY - UHT
	// doesn't support a TArray-valued TMap - but every actor in it is already kept alive by simply
	// being a live actor in the level (the same reason StartPlay's Sun/Sky/Fog spawns above never
	// bother storing a pointer either); UpdateWindowFrames() is solely responsible for Destroy()ing
	// them once their column scrolls out of range.
	TMap<int32, TArray<AStaticMeshActor*>> WindowFrameBlocksByColumn;
};
