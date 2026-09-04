#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SlinkyActor.h"
#include "SlinkyStaircase.generated.h"

class UInstancedStaticMeshComponent;

UCLASS()
class ASlinkyStaircase : public AActor
{
	GENERATED_BODY()

public:
	ASlinkyStaircase();
	virtual void PostInitializeComponents() override;
	virtual void Tick(float DeltaTime) override;

	// Editable in the level editor. Live in-game tuning (ASlinkyPlayerController's arrow/page keys,
	// USlinkyControlPanel's rows) goes through SetStepDepth/SetStepRise/SetRiserThickness below
	// instead of writing these directly, so the anchor step gets captured first - see those setters.
	UPROPERTY(EditAnywhere, Category = "Stairs")
	float StepDepth = 100.0f;

	UPROPERTY(EditAnywhere, Category = "Stairs")
	float StepRise = 83.333f;

	UPROPERTY(EditAnywhere, Category = "Stairs")
	float RiserThickness = 10.0f;

	UPROPERTY(EditAnywhere, Category = "Stairs")
	int32 StepCount = 112;

	// Re-slides all existing step instances onto the current StepDepth/StepRise/RiserThickness
	// values without needing a restart. Safe to call at runtime. Called internally by the setters
	// below; call directly only when changing something that isn't itself anchored (e.g. after the
	// coil's CoilRadius changes, which only affects tread/riser width, not position).
	void RefreshLayout();

	// Setters for the three live-tunable stair values: each captures whichever step is currently
	// under the slinky as a fixed anchor *before* changing the value, so every other tread/riser is
	// repositioned relative to that anchor rather than to step index 0. Writing StepDepth/StepRise
	// directly and calling RefreshLayout() (the old pattern) rescales the whole flight from a
	// fixed, usually-distant origin - fine near the top of the stairs, but for a slinky already many
	// steps down, even a small depth/rise change yanks the tread out from under it (or through it).
	void SetStepDepth(float NewValue);
	void SetStepRise(float NewValue);
	void SetRiserThickness(float NewValue);

	// Puts StepDepth/StepRise/RiserThickness back to this class's compile-time defaults (read off
	// the CDO) via the anchor-aware setters above. Used by the pause menu's "デフォルトに戻す" -
	// see ASlinkyActor::ResetTuningToDefaults() for the matching coil-side reset.
	void ResetToDefaults();

	// Immediately rebuilds the visible tread/riser window around wherever the slinky actually is,
	// instead of waiting for this actor's own 0.25s tick interval to notice. Normally that lag is
	// unnoticeable (ResetSlinky() only ever moves the coil a few steps), but
	// ASlinkyActor::TeleportToDepth() (continuing from a save) can jump it far down the flight in
	// one frame - safe to call anytime since it's just RecycleSteps()'s own distance check.
	void SnapStepsToSlinky() { RecycleSteps(); }

	// The world location of the top surface of the tread nearest WorldX - used by
	// ASlinkyActor::ResetSlinky() so restarting the coil reforms wherever it currently is on the
	// stairs, the same "anchor on the step below the slinky" idea as the setters above, rather than
	// snapping back to the actor's original placement at the top.
	FVector GetTreadTopLocationNear(float WorldX) const;

	// The (X, Z) pair M_RoomBackdrop's step-boundary shader should treat as the origin of its own
	// "floor(x / StepDepth) * StepRise" formula, pushed to the material every tick by
	// ASlinkyGameMode - the painted wood/wall silhouette used the same formula from world X=0 for
	// every step, so a live StepDepth/StepRise/RiserThickness edit (or just the slinky walking far
	// from the origin) desynced it from the real, now anchor-relative tread/riser instances. X is
	// the anchor tread's left edge and Z matches the original formula's zero-offset convention
	// (tread top-of-boundary, not tread center), so this equals (0, 0) - a no-op for the shader -
	// until the first live parameter edit.
	FVector2D GetShaderBoundaryOrigin() const { return FVector2D(AnchorEdgeX, AnchorTreadLocation.Z + 6.0f); }

private:
	void BuildSteps();
	void RecycleSteps();
	FTransform MakeTreadTransform(int32 StepIndex) const;
	FTransform MakeRiserTransform(int32 StepIndex) const;
	// Which StepIndex's tread WorldX falls under. Anchor-relative to match MakeTreadTransform's own
	// grid (AnchorEdgeX is that tread's left edge, not world X=0) - using the plain
	// floor(WorldX / StepDepth) this used to be would pick the tread that WOULD be nearest WorldX
	// under the *original*, un-anchored grid, which silently diverges from the real one the moment
	// the anchor drifts from its default (AnchorStepIndex 0, AnchorEdgeX 0) after the first live
	// parameter edit. That broke every caller: RecycleSteps() centered its recycling window on the
	// wrong step, CaptureAnchor() re-anchored on the wrong step, and GetTreadTopLocationNear() (so
	// ASlinkyActor::ResetSlinky()) sent a respawn to the wrong tread entirely - forward or back of
	// wherever the coil actually was.
	int32 GetStepIndexNear(float WorldX) const
	{
		return AnchorStepIndex + FMath::FloorToInt((WorldX - AnchorEdgeX) / StepDepth);
	}

	// Records whichever step is currently under the live ASlinkyActor as the new anchor - reading
	// the tread's current world position (via the still-unchanged StepDepth/StepRise and the
	// *previous* anchor) before a setter overwrites any of those. Must run before the value it's
	// guarding actually changes.
	void CaptureAnchor();

	// The stairs' own width tracks the coil's CoilRadius, which is live-adjustable (see
	// USlinkyControlPanel) and so no longer a compile-time constant - this looks up (and caches)
	// the live ASlinkyActor to read its current value instead, falling back to CoilRadius's own
	// default if the coil hasn't spawned yet (true only for the very first BuildSteps() call,
	// which happens in PostInitializeComponents() before ASlinkyGameMode has spawned it).
	float GetCoilRadius() const;
	ASlinkyActor* FindSlinky() const;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UInstancedStaticMeshComponent> Treads;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UInstancedStaticMeshComponent> Risers;

	mutable TWeakObjectPtr<ASlinkyActor> CachedSlinky;

	int32 FirstStepIndex = -20;

	// Which step index MakeTreadTransform/MakeRiserTransform treat as the fixed reference point, and
	// that step's own tread world location at the time it was captured - see CaptureAnchor().
	// Defaulted (in the constructor body) to what step 0 would be under the original, un-anchored
	// formula, so behavior is unchanged until the first live parameter edit.
	//
	// Only the DeltaIndex*StepDepth/StepRise *accumulation* is anchor-relative - both
	// MakeTreadTransform and MakeRiserTransform recompute their own per-step centering terms
	// (+0.5*StepDepth for a tread, -RiserThickness*0.5 and -StepRise*0.5 for a riser) fresh every
	// call from the *current* values rather than baking them in here. Baking those in too (an
	// earlier version of this code did) meant every tread/riser - not just ones far from the anchor
	// - stayed offset by half of whatever StepDepth/StepRise/RiserThickness had just changed by,
	// since that constant term never gets multiplied by DeltaIndex and so never gets corrected by
	// the next CaptureAnchor() either: treads visibly popped forward and risers popped upward on
	// every edit.
	int32 AnchorStepIndex = 0;
	FVector AnchorTreadLocation = FVector::ZeroVector;

	// The anchor tread's left edge in world X, using the StepDepth that was current at capture time
	// (CaptureAnchor() reads it before the setter changes it) - see GetShaderBoundaryOrigin() and
	// MakeTreadTransform/MakeRiserTransform.
	float AnchorEdgeX = 0.0f;
};
