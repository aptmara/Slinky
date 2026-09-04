#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SlinkyActor.generated.h"

class ACameraActor;
class ASlinkyStaircase;
class UInstancedStaticMeshComponent;
class UPhysicalMaterial;
class UPhysicsConstraintComponent;
class UPrimitiveComponent;
class UStaticMeshComponent;

UCLASS()
class ASlinkyActor : public AActor
{
	GENERATED_BODY()

public:
	ASlinkyActor();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	bool BeginDrag(const FVector& HitPoint);
	void UpdateDragTarget(const FVector& Target);
	void EndDrag();
	void ResetSlinky();

	bool IsDragging() const { return GrabbedBody != nullptr; }
	int32 GetStepCount() const { return SuccessfulContacts; }
	FVector GetCenterLocation() const;
	ACameraActor* GetFollowCameraActor() const { return FollowCameraActor; }

	// Combo/step-presentation readout for ASlinkyHUD - see RegisterCombo()/TriggerStepEffects() for
	// how these get driven. Kept as plain getters (rather than the HUD reaching into private state)
	// so all the combo rules live in one place.
	int32 GetComboCount() const { return ComboCount; }
	int32 GetBestCombo() const { return BestCombo; }
	// 1 the instant a step lands, ticking down to 0 as the combo window closes - drives the HUD's
	// combo gauge bar.
	float GetComboWindowRemaining01() const;
	// Spring-driven "pop" scale for the step counter / combo text, each landed step kicks it above 1
	// and it bounces back down - see the SpringTowardOne() helper in the .cpp.
	float GetStepPopScale() const { return StepPopScale; }
	float GetComboPopScale() const { return ComboPopScale; }
	// Hue cycles with combo count so longer streaks read as visibly more colorful/poppy.
	FLinearColor GetComboColor() const;
	// Empty once the milestone banner (e.g. "GREAT COMBO!!") has fully faded - see GetMilestoneAlpha01().
	const FString& GetMilestoneText() const { return MilestoneText; }
	float GetMilestoneAlpha01() const;
	// A bigger, screen-shakier kick than the regular per-step pop - only fired when RegisterCombo()
	// crosses a tier (every 10 combo), so the milestone banner lands with extra punch. See
	// GetMilestoneAlpha01() for the banner's own fade.
	float GetMilestoneShake() const { return MilestoneShake; }
	// One combo-count-wide (0-9=tier 0, 10-19=tier 1, ...) escalating label - "COMBO", "NICE COMBO",
	// "GREAT COMBO", etc. - so the on-screen text itself keeps changing every 10 combo instead of
	// just the trailing number. See ComboLabelForTier() in the .cpp for the full ladder.
	FString GetComboLabel() const;
	// 0 outside a flash, ramping to 1 the instant any step lands and decaying back to 0 - drives a
	// full-screen color flash so even an off-milestone step still reads as an "impact".
	float GetComboFlashAlpha() const { return ComboFlashAlpha; }

	// Live in-game tuning (see ASlinkyPlayerController's Tab/[ ] keys, and USlinkyControlPanel).
	// Editable in the level editor too. PhysicsNodeCount/SegmentsPerTurn/TetherSegmentCount stay
	// compile-time constants since changing them means recreating the physics rig itself (nodes +
	// constraints), not just re-applying numbers to components already spawned. CoilTurns and
	// CoilRadius only affect the purely-visual helix instances and node disc scale respectively, so
	// both are safe to change live - see RebuildHelixSegments()/RefreshNodeScale().
	UPROPERTY(EditAnywhere, Category = "Coil Tuning")
	float CoilRadius = 45.0f;

	UPROPERTY(EditAnywhere, Category = "Coil Tuning")
	int32 CoilTurns = 36;

	UPROPERTY(EditAnywhere, Category = "Coil Tuning")
	float CompactLength = 180.0f;

	UPROPERTY(EditAnywhere, Category = "Coil Tuning")
	float MaximumNodeSpacing = 60.0f;

	UPROPERTY(EditAnywhere, Category = "Coil Tuning")
	float WireRadius = 1.25f;

	UPROPERTY(EditAnywhere, Category = "Coil Tuning")
	float AxialStiffnessScale = 1.0f;

	UPROPERTY(EditAnywhere, Category = "Coil Tuning")
	float BendStiffnessScale = 1.0f;

	UPROPERTY(EditAnywhere, Category = "Coil Tuning")
	float DampingScale = 1.0f;

	UPROPERTY(EditAnywhere, Category = "Coil Tuning")
	float Restitution = 0.08f;

	UPROPERTY(EditAnywhere, Category = "Coil Tuning")
	float Friction = 0.48f;

	enum class ETuningParam : uint8
	{
		CompactLength,
		MaximumNodeSpacing,
		WireRadius,
		AxialStiffnessScale,
		BendStiffnessScale,
		DampingScale,
		Restitution,
		Friction,
		CoilTurns,
		CoilRadius,
		Count
	};

	void CycleTuningParam();
	void AdjustTuningParam(float Direction);
	FString GetTuningParamDisplay() const;

	// Re-applies linear drive stiffness and physical-material tuning from the current
	// MaximumNodeSpacing/AxialStiffnessScale/DampingScale/Restitution/Friction values. Safe to call
	// anytime - it only touches drive/limit and material numbers, never recreates joints or nodes -
	// so an external editor (USlinkyControlPanel's sliders) can set those fields directly and call
	// this instead of going through AdjustTuningParam's one-at-a-time stepping.
	// CompactLength needs RefreshCompactLength() instead: each joint's rest length is baked in from
	// the nodes' world positions when ConfigureConstraints() first ran, so recomputing
	// GetNodeRestSpacing() here would only nudge the stretch limit, never the coil's resting length.
	// BendStiffnessScale needs no such call: Tick already re-reads it every frame.
	void RefreshCoilTuning();

	// Re-lays the physics nodes out along the coil's own (vertical) axis at the new
	// CompactLength-derived rest spacing, centered on the coil's current position, then rebuilds
	// every joint's rest frame via ConfigureConstraints(). This is the only way to make CompactLength
	// changes actually visible - the joints' rest length is otherwise baked in once at BeginPlay and
	// never revisited. Call after changing CompactLength.
	void RefreshCompactLength();

	// Clears and re-adds HelixSegments' instances to match the current CoilTurns * SegmentsPerTurn.
	// Call after changing CoilTurns (BeginPlay already calls this once to build the initial coil).
	void RebuildHelixSegments();

	// Suspends physics simulation on every node - called by ASlinkyStaircase::RefreshLayout() to
	// bracket a tread/riser rebuild that might move solid geometry the coil is resting on (every
	// step but the exact one under the coil shifts when StepDepth/StepRise/RiserThickness changes -
	// see ASlinkyStaircase::MakeTreadTransform). Simulating through that teleport can wedge a node so
	// deep into the new geometry the solver never finds a way to push it back out, and it stays
	// "buried" from then on. Resumes on a *later* Tick() rather than immediately (see
	// bResumePhysicsNextTick), so at least one physics step passes with the geometry already settled
	// and this body not yet reacting to it - coming back to a normal, gently-resolvable overlap
	// instead of one caught mid-teleport. No-op if the coil hasn't been activated yet (still resting
	// kinematically pre-grab) - nothing to protect, and this must never be what turns simulation on
	// for the first time.
	void PausePhysicsForLayoutChange();

	// Re-applies the current CoilRadius and WireRadius to every physics node's disc scale (radius
	// and thickness respectively). UpdateHelix() already re-reads both fresh every tick for the
	// coil's purely-visual mesh, but the physics nodes' own collision shape only updates when this is
	// called - call it after changing either value, or the collision won't match what's drawn.
	void RefreshNodeScale();

	float GetNodeRestSpacing() const { return CompactLength / (PhysicsNodeCount - 1); }

private:
	void ApplyLinearTuning();
	void ApplyMaterialTuning();
	void ApplySlinkyForces();
	void ApplyDragForce();
	void UpdateVisuals();
	void UpdateHelix();
	void UpdateTether();
	void UpdateCamera(float DeltaTime);
	void ConfigureNode(UStaticMeshComponent* Node);
	FVector ComputeNodeScale() const;
	ASlinkyStaircase* FindStaircase() const;
	void ConfigureConstraints();
	void UpdateConstraintFlexibility();
	FVector SampleCenterline(float Alpha) const;
	FVector SampleCenterlineTangent(float Alpha) const;
	FVector GetCameraFocusLocation() const;
	FVector GetGrabWorldPosition() const;
	void RegisterGroundContact(UPrimitiveComponent* Body, const FHitResult& Hit);

	// Bumps ComboCount/BestCombo and (re)starts the combo window, and stages a milestone banner
	// (e.g. "GREAT COMBO!!") when ComboCount crosses a tier (every ComboTierSize). Called once per
	// landed step, before TriggerStepEffects() so the burst color already reflects the new combo
	// count. Returns true if this step crossed a tier, so TriggerStepEffects() can scale the burst up.
	bool RegisterCombo();
	// Spawns one shrinking impact ring plus a burst of gravity-affected sparkle cubes at
	// ImpactLocation, and kicks the camera-punch and pop-scale springs - bigger, when bTierUp is true,
	// to match the milestone banner RegisterCombo() just staged. Purely cosmetic - has no effect on
	// SuccessfulContacts/ComboCount, which RegisterGroundContact/RegisterCombo already updated by the
	// time this runs.
	void TriggerStepEffects(const FVector& ImpactLocation, bool bTierUp);
	// Advances every active ring/sparkle's age and position and re-pushes their instance transforms,
	// growing/shrinking InstancedStaticMeshComponent instance counts to match - called once per Tick.
	void UpdateStepEffects(float DeltaTime);
	// Vivid, high-saturation color for the given combo count - cycles hue so a long streak keeps
	// visibly changing rather than settling on one color.
	FLinearColor ComputeComboColor(int32 ForCombo) const;
	// The escalating label ladder behind GetComboLabel() - factored out so RegisterCombo() can look
	// up the label for a specific tier (to detect when it's about to change) without duplicating the
	// ladder itself.
	static FString ComboLabelForTier(int32 Tier);

	UFUNCTION()
	void OnNodeHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComponent,
		FVector NormalImpulse, const FHitResult& Hit);

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere)
	TArray<TObjectPtr<UStaticMeshComponent>> PhysicsNodes;

	UPROPERTY(VisibleAnywhere)
	TArray<TObjectPtr<UPhysicsConstraintComponent>> NodeConstraints;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UInstancedStaticMeshComponent> HelixSegments;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UInstancedStaticMeshComponent> TetherSegments;

	// Step-landing presentation: a shrinking ring plus a burst of tumbling sparkle cubes, both purely
	// visual InstancedStaticMeshComponents like HelixSegments/TetherSegments above - see
	// TriggerStepEffects()/UpdateStepEffects().
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UInstancedStaticMeshComponent> StepRings;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UInstancedStaticMeshComponent> StepSparkles;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> RingMaterial;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> SparkleMaterial;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<ACameraActor> FollowCameraActor;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UPhysicalMaterial> SlinkyPhysicalMaterial;

	UPROPERTY()
	TObjectPtr<UPrimitiveComponent> GrabbedBody;

	// Looked up lazily (see FindStaircase()) rather than assumed spawned yet, since ASlinkyGameMode
	// may create the staircase and the slinky in either order.
	mutable TWeakObjectPtr<ASlinkyStaircase> CachedStaircase;

	FVector DragTarget = FVector::ZeroVector;
	FVector LocalGrabPoint = FVector::ZeroVector;
	FVector SmoothedCameraLocation = FVector::ZeroVector;
	bool bPhysicsActivated = false;
	int32 LastFirstEndStepIndex = MIN_int32;
	int32 LastLastEndStepIndex = MIN_int32;
	int32 SuccessfulContacts = 0;
	ETuningParam SelectedTuningParam = ETuningParam::CompactLength;
	bool bResumePhysicsNextTick = false;

	// One shrinking impact ring spawned per landed step - see TriggerStepEffects()/UpdateStepEffects().
	// Array index always matches the ring's instance index in StepRings (both grow/shrink together,
	// oldest-first), so no separate handle/ID bookkeeping is needed.
	struct FStepRingFx
	{
		FVector Location = FVector::ZeroVector;
		float Age = 0.0f;
		float Lifetime = 0.42f;
	};

	// One tumbling sparkle cube from a step-landing burst - see TriggerStepEffects()/
	// UpdateStepEffects(). Same array-index-matches-instance-index convention as FStepRingFx.
	struct FStepSparkleFx
	{
		FVector Location = FVector::ZeroVector;
		FVector Velocity = FVector::ZeroVector;
		FVector RotationAxis = FVector::UpVector;
		float SpinDegreesPerSec = 0.0f;
		float Age = 0.0f;
		float Lifetime = 0.55f;
		float BaseScale = 0.5f;
	};

	TArray<FStepRingFx> ActiveRings;
	TArray<FStepSparkleFx> ActiveSparkles;

	// How long a landed step keeps the combo alive - land the next one before this runs out or
	// ComboCount drops back to 0 (see Tick()'s countdown).
	static constexpr float ComboWindowSeconds = 1.4f;
	static constexpr float MilestoneDuration = 1.3f;
	// Combo count per label rung - ComboCount / ComboTierSize indexes ComboLabelForTier()'s ladder,
	// so every 10 combo the on-screen label itself escalates ("COMBO" -> "NICE COMBO" -> ...).
	static constexpr int32 ComboTierSize = 10;

	int32 ComboCount = 0;
	int32 BestCombo = 0;
	float ComboTimeRemaining = 0.0f;

	// Critically-damped-ish springs (see SpringTowardOne()/SpringTowardZero() in the .cpp): each
	// landed step kicks the *Velocity term, Tick() relaxes Value back toward its rest state every
	// frame, producing the bounce/punch instead of an instant snap.
	float StepPopScale = 1.0f;
	float StepPopVelocity = 0.0f;
	float ComboPopScale = 1.0f;
	float ComboPopVelocity = 0.0f;
	float CameraPunch = 0.0f;
	float CameraPunchVelocity = 0.0f;

	// Banner text ("GREAT COMBO!!" etc.) shown when ComboCount crosses a tier (every 10 combo) - see
	// RegisterCombo() and GetMilestoneAlpha01(). Timer counts down from MilestoneDuration; text is
	// only meaningful while MilestoneTimer > 0.
	FString MilestoneText;
	float MilestoneTimer = 0.0f;
	// Extra shake/pop magnitude for a tier-crossing banner, decaying independently of MilestoneTimer
	// so it can settle out visually before the text itself fades - see SpringToward() calls in Tick().
	float MilestoneShake = 0.0f;

	// Full-screen flash strength - set to 1 on every landed step and decayed in Tick(), giving even
	// an off-tier step a quick, poppy screen-color pulse instead of only the milestone banner reading
	// as an "event".
	float ComboFlashAlpha = 0.0f;

	static constexpr int32 PhysicsNodeCount = 31;
	static constexpr int32 SegmentsPerTurn = 9;
	static constexpr int32 TetherSegmentCount = 14;
};
