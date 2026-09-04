#include "SlinkyActor.h"
#include "SlinkyStaircase.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Chaos/ChaosEngineInterface.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "PhysicsEngine/PhysicsConstraintComponent.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	FTransform MakeCylinderSegmentTransform(const FVector& Start, const FVector& End, float Radius)
	{
		const FVector Delta = End - Start;
		const float Length = FMath::Max(Delta.Size(), 0.1f);
		const FQuat Rotation = FQuat::FindBetweenNormals(FVector::UpVector, Delta / Length);
		return FTransform(Rotation, (Start + End) * 0.5f,
			FVector(Radius / 50.0f, Radius / 50.0f, Length / 100.0f));
	}

	FVector SafePerpendicular(const FVector& Axis, const FVector& Preferred)
	{
		FVector Result = Preferred - Axis * FVector::DotProduct(Preferred, Axis);
		if (!Result.Normalize())
		{
			Result = FVector::CrossProduct(Axis, FVector::YAxisVector);
			if (!Result.Normalize())
			{
				Result = FVector::XAxisVector;
			}
		}
		return Result;
	}

	FVector CatmullRom(const FVector& P0, const FVector& P1, const FVector& P2, const FVector& P3, float T)
	{
		const float T2 = T * T;
		const float T3 = T2 * T;
		return 0.5f * ((2.0f * P1) + (-P0 + P2) * T +
			(2.0f * P0 - 5.0f * P1 + 4.0f * P2 - P3) * T2 +
			(-P0 + 3.0f * P1 - 3.0f * P2 + P3) * T3);
	}

	// Simple damped spring: Value relaxes toward RestValue every call, Velocity carries the "kick"
	// applied by a caller (e.g. RegisterCombo() bumping *PopVelocity) across frames. Used for every
	// bounce/punch effect below instead of a fixed-duration tween, so overlapping kicks (rapid steps)
	// blend into one continuous bounce rather than restarting a whole new tween.
	void SpringToward(float& Value, float& Velocity, float RestValue, float Stiffness, float Damping,
		float DeltaTime)
	{
		const float Acceleration = -Stiffness * (Value - RestValue) - Damping * Velocity;
		Velocity += Acceleration * DeltaTime;
		Value += Velocity * DeltaTime;
	}
}

ASlinkyActor::ASlinkyActor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PrePhysics;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	for (int32 Index = 0; Index < PhysicsNodeCount; ++Index)
	{
		const FName NodeName(*FString::Printf(TEXT("CoilPhysics_%02d"), Index));
		UStaticMeshComponent* Node = CreateDefaultSubobject<UStaticMeshComponent>(NodeName);
		Node->SetupAttachment(SceneRoot);
		PhysicsNodes.Add(Node);
	}
	for (int32 Index = 0; Index + 1 < PhysicsNodeCount; ++Index)
	{
		const FName ConstraintName(*FString::Printf(TEXT("CoilConstraint_%02d"), Index));
		UPhysicsConstraintComponent* Constraint = CreateDefaultSubobject<UPhysicsConstraintComponent>(ConstraintName);
		Constraint->SetupAttachment(SceneRoot);
		NodeConstraints.Add(Constraint);
	}

	HelixSegments = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Helix"));
	HelixSegments->SetupAttachment(SceneRoot);
	HelixSegments->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	HelixSegments->SetCastShadow(true);

	TetherSegments = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("DragTether"));
	TetherSegments->SetupAttachment(SceneRoot);
	TetherSegments->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	TetherSegments->SetCastShadow(false);

	StepRings = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("StepRings"));
	StepRings->SetupAttachment(SceneRoot);
	StepRings->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	StepRings->SetCastShadow(false);

	StepSparkles = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("StepSparkles"));
	StepSparkles->SetupAttachment(SceneRoot);
	StepSparkles->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	StepSparkles->SetCastShadow(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (Cylinder.Succeeded())
	{
		for (UStaticMeshComponent* Node : PhysicsNodes)
		{
			Node->SetStaticMesh(Cylinder.Object);
		}
		HelixSegments->SetStaticMesh(Cylinder.Object);
		TetherSegments->SetStaticMesh(Cylinder.Object);
		StepRings->SetStaticMesh(Cylinder.Object);
	}

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (Cube.Succeeded())
	{
		StepSparkles->SetStaticMesh(Cube.Object);
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> BaseMaterial(
		TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (BaseMaterial.Succeeded())
	{
		UMaterialInstanceDynamic* SlinkyMaterial = UMaterialInstanceDynamic::Create(BaseMaterial.Object, this);
		SlinkyMaterial->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.72f, 0.76f, 0.82f, 1.0f));
		HelixSegments->SetMaterial(0, SlinkyMaterial);

		UMaterialInstanceDynamic* TetherMaterial = UMaterialInstanceDynamic::Create(BaseMaterial.Object, this);
		TetherMaterial->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.08f, 0.025f, 0.02f, 1.0f));
		TetherSegments->SetMaterial(0, TetherMaterial);

		// Recolored per-step to the current combo's hue in TriggerStepEffects() - the flat starting
		// color here only matters before the first step ever lands.
		RingMaterial = UMaterialInstanceDynamic::Create(BaseMaterial.Object, this);
		RingMaterial->SetVectorParameterValue(TEXT("Color"), FLinearColor(1.0f, 0.85f, 0.25f, 1.0f));
		StepRings->SetMaterial(0, RingMaterial);

		SparkleMaterial = UMaterialInstanceDynamic::Create(BaseMaterial.Object, this);
		SparkleMaterial->SetVectorParameterValue(TEXT("Color"), FLinearColor(1.0f, 0.85f, 0.25f, 1.0f));
		StepSparkles->SetMaterial(0, SparkleMaterial);
	}

	SlinkyPhysicalMaterial = CreateDefaultSubobject<UPhysicalMaterial>(TEXT("SlinkyPhysicalMaterial"));
	SlinkyPhysicalMaterial->StaticFriction = 0.58f;
	SlinkyPhysicalMaterial->bOverrideFrictionCombineMode = true;
	SlinkyPhysicalMaterial->FrictionCombineMode = EFrictionCombineMode::Min;
	// Min combine meant the stairs' default (0) restitution always won, zeroing out any bounce
	// regardless of this value. Max combine lets the coil's own bounciness govern the landing.
	SlinkyPhysicalMaterial->bOverrideRestitutionCombineMode = true;
	SlinkyPhysicalMaterial->RestitutionCombineMode = EFrictionCombineMode::Max;

	for (UStaticMeshComponent* Node : PhysicsNodes)
	{
		Node->OnComponentHit.AddDynamic(this, &ASlinkyActor::OnNodeHit);
	}
}

void ASlinkyActor::BeginPlay()
{
	Super::BeginPlay();
	for (UStaticMeshComponent* Node : PhysicsNodes)
	{
		ConfigureNode(Node);
	}

	FollowCameraActor = GetWorld()->SpawnActor<ACameraActor>();
	UCameraComponent* FollowCameraComponent = FollowCameraActor->GetCameraComponent();
	FollowCameraComponent->ProjectionMode = ECameraProjectionMode::Orthographic;
	FollowCameraComponent->OrthoWidth = 900.0f;
	FollowCameraComponent->bConstrainAspectRatio = false;
	FollowCameraComponent->AspectRatio = 16.0f / 9.0f;

	RebuildHelixSegments();
	for (int32 Index = 0; Index < TetherSegmentCount; ++Index)
	{
		TetherSegments->AddInstance(FTransform(FVector(0.001f)));
	}

	ApplyMaterialTuning();
	ResetSlinky();
	ConfigureConstraints();
	for (UStaticMeshComponent* Node : PhysicsNodes)
	{
		Node->SetSimulatePhysics(false);
	}
	SmoothedCameraLocation = GetCenterLocation() + FVector(0.0f, -1000.0f, 0.0f);
	UpdateVisuals();
	UpdateCamera(0.0f);
}

void ASlinkyActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (bResumePhysicsNextTick)
	{
		// Only now, on a later frame than PausePhysicsForLayoutChange() paused on - see that
		// function's comment for why resuming immediately (same frame) wouldn't actually protect
		// anything.
		bResumePhysicsNextTick = false;
		for (UStaticMeshComponent* Node : PhysicsNodes)
		{
			Node->SetSimulatePhysics(true);
		}
	}
	ApplySlinkyForces();
	UpdateConstraintFlexibility();
	ApplyDragForce();
	UpdateVisuals();
	UpdateStepEffects(DeltaTime);

	if (ComboTimeRemaining > 0.0f)
	{
		ComboTimeRemaining = FMath::Max(ComboTimeRemaining - DeltaTime, 0.0f);
		if (ComboTimeRemaining == 0.0f)
		{
			// Combo window closed without a follow-up step - the streak ends here.
			ComboCount = 0;
		}
	}
	MilestoneTimer = FMath::Max(MilestoneTimer - DeltaTime, 0.0f);
	MilestoneShake = FMath::Max(MilestoneShake - DeltaTime * 2.2f, 0.0f);
	ComboFlashAlpha = FMath::Max(ComboFlashAlpha - DeltaTime * 2.6f, 0.0f);

	SpringToward(StepPopScale, StepPopVelocity, 1.0f, 300.0f, 16.0f, DeltaTime);
	SpringToward(ComboPopScale, ComboPopVelocity, 1.0f, 300.0f, 16.0f, DeltaTime);
	SpringToward(CameraPunch, CameraPunchVelocity, 0.0f, 220.0f, 14.0f, DeltaTime);

	UpdateCamera(DeltaTime);
}

bool ASlinkyActor::BeginDrag(const FVector& HitPoint)
{
	// Grabbing depends only on proximity in the play plane, not on hitting any collision shape -
	// the physics discs are far too thin to click reliably in a side-on view, and a dedicated pickup
	// collider proved unreliable to keep in sync with the engine's collision profile system. Picking
	// whichever end is closer to the cursor, unconditionally, is simpler and always works.
	UStaticMeshComponent* FirstNode = PhysicsNodes[0];
	UStaticMeshComponent* LastNode = PhysicsNodes.Last();
	const float DistToFirst = FVector::DistSquared(FirstNode->GetComponentLocation(), HitPoint);
	const float DistToLast = FVector::DistSquared(LastNode->GetComponentLocation(), HitPoint);

	GrabbedBody = (DistToFirst <= DistToLast) ? FirstNode : LastNode;
	LocalGrabPoint = GrabbedBody->GetComponentTransform().InverseTransformPosition(HitPoint);
	LocalGrabPoint.Y = 0.0f;
	LocalGrabPoint.Z = 0.0f;
	DragTarget = HitPoint;
	for (UStaticMeshComponent* Node : PhysicsNodes)
	{
		Node->SetSimulatePhysics(true);
		Node->WakeAllRigidBodies();
	}
	bPhysicsActivated = true;
	return true;
}

void ASlinkyActor::UpdateDragTarget(const FVector& Target)
{
	DragTarget = Target;
	DragTarget.Y = 0.0f;
}

void ASlinkyActor::EndDrag()
{
	GrabbedBody = nullptr;
}

void ASlinkyActor::ResetSlinky()
{
	EndDrag();
	SuccessfulContacts = 0;
	LastFirstEndStepIndex = MIN_int32;
	LastLastEndStepIndex = MIN_int32;
	bPhysicsActivated = false;

	// BestCombo deliberately survives a reset - it reads as a session high score, so restarting the
	// coil (R) shouldn't erase it.
	ComboCount = 0;
	ComboTimeRemaining = 0.0f;
	MilestoneTimer = 0.0f;
	MilestoneShake = 0.0f;
	ComboFlashAlpha = 0.0f;
	CameraPunch = 0.0f;
	CameraPunchVelocity = 0.0f;
	ActiveRings.Reset();
	ActiveSparkles.Reset();
	StepRings->ClearInstances();
	StepSparkles->ClearInstances();

	// Reform wherever the coil currently is on the stairs (the tread directly below its current
	// center) rather than always snapping back to the actor's original placement - the same "anchor
	// on the step below the slinky" idea ASlinkyStaircase's setters use for live parameter changes,
	// so neither action can strand the coil far from where the player actually was. Falls back to
	// the actor's own location if the staircase hasn't spawned yet (only true for BeginPlay's very
	// first ResetSlinky() call, before ASlinkyGameMode has necessarily created it).
	const ASlinkyStaircase* Staircase = FindStaircase();
	const FVector Base = Staircase
		? Staircase->GetTreadTopLocationNear(GetCenterLocation().X)
		: GetActorLocation();
	for (int32 Index = 0; Index < PhysicsNodes.Num(); ++Index)
	{
		UStaticMeshComponent* Node = PhysicsNodes[Index];
		Node->SetSimulatePhysics(false);
		const FVector Location = Base + FVector(0.0f, 0.0f, 1.5f + Index * GetNodeRestSpacing());
		Node->SetWorldLocationAndRotation(Location, FRotator::ZeroRotator, false, nullptr,
			ETeleportType::TeleportPhysics);
		Node->SetPhysicsLinearVelocity(FVector::ZeroVector);
		Node->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
	}
}

FVector ASlinkyActor::GetCenterLocation() const
{
	if (PhysicsNodes.IsEmpty())
	{
		return GetActorLocation();
	}

	FVector Center = FVector::ZeroVector;
	for (const UStaticMeshComponent* Node : PhysicsNodes)
	{
		Center += Node->GetComponentLocation();
	}
	return Center / PhysicsNodes.Num();
}

void ASlinkyActor::ApplySlinkyForces()
{
	if (!bPhysicsActivated)
	{
		return;
	}

	for (int32 Index = 0; Index < PhysicsNodes.Num(); ++Index)
	{
		UStaticMeshComponent* Node = PhysicsNodes[Index];
		const FVector Location = Node->GetComponentLocation();
		FVector Velocity = Node->GetPhysicsLinearVelocity();
		if (Velocity.SizeSquared() > FMath::Square(1400.0f))
		{
			Velocity = Velocity.GetClampedToMaxSize(1400.0f);
			Node->SetPhysicsLinearVelocity(Velocity);
		}
		Node->AddForce(FVector(0.0f, -Location.Y * 520.0f - Velocity.Y * 115.0f, 0.0f));

		FVector AngularVelocity = Node->GetPhysicsAngularVelocityInRadians();
		if (AngularVelocity.SizeSquared() > FMath::Square(14.0f))
		{
			AngularVelocity = AngularVelocity.GetClampedToMaxSize(14.0f);
			Node->SetPhysicsAngularVelocityInRadians(AngularVelocity);
		}
	}
}

void ASlinkyActor::ApplyDragForce()
{
	if (!GrabbedBody)
	{
		return;
	}

	const FVector GrabPosition = GetGrabWorldPosition();
	const FVector Displacement = DragTarget - GrabPosition;
	const FVector PointVelocity = GrabbedBody->GetPhysicsLinearVelocityAtPoint(GrabPosition);
	FVector Force = Displacement * 340.0f - PointVelocity * 30.0f;
	Force = Force.GetClampedToMaxSize(26000.0f);
	GrabbedBody->AddForceAtLocation(Force * 0.68f, GrabPosition);

	const int32 GrabbedIndex = (GrabbedBody == PhysicsNodes[0]) ? 0 : PhysicsNodes.Num() - 1;
	const int32 Direction = (GrabbedIndex == 0) ? 1 : -1;
	if (PhysicsNodes.IsValidIndex(GrabbedIndex + Direction))
	{
		PhysicsNodes[GrabbedIndex + Direction]->AddForce(Force * 0.22f);
	}
	if (PhysicsNodes.IsValidIndex(GrabbedIndex + Direction * 2))
	{
		PhysicsNodes[GrabbedIndex + Direction * 2]->AddForce(Force * 0.10f);
	}
}

void ASlinkyActor::UpdateVisuals()
{
	UpdateHelix();
	UpdateTether();
}

void ASlinkyActor::UpdateHelix()
{
	float CenterlineLength = 0.0f;
	for (int32 Index = 0; Index + 1 < PhysicsNodes.Num(); ++Index)
	{
		CenterlineLength += FVector::Distance(PhysicsNodes[Index]->GetComponentLocation(),
			PhysicsNodes[Index + 1]->GetComponentLocation());
	}
	const float RadiusScale = FMath::Clamp(1.06f - CenterlineLength / 1400.0f, 0.86f, 1.02f);

	// Read fresh every call (unlike the instance count, which only needs to change - via
	// RebuildHelixSegments() - when CoilTurns itself changes), so live-adjusting CoilTurns or
	// CoilRadius from the UI takes effect the very next tick.
	const int32 SegmentCount = FMath::Max(CoilTurns, 1) * SegmentsPerTurn;

	FVector PreviousPoint;
	for (int32 Segment = 0; Segment <= SegmentCount; ++Segment)
	{
		const float Alpha = static_cast<float>(Segment) / SegmentCount;
		const FVector Center = SampleCenterline(Alpha);
		const FVector Tangent = SampleCenterlineTangent(Alpha);
		const FVector BasisA = SafePerpendicular(Tangent, FVector::YAxisVector);
		const FVector BasisB = FVector::CrossProduct(Tangent, BasisA).GetSafeNormal();
		const float Angle = Alpha * CoilTurns * UE_TWO_PI;
		const FVector Point = Center + (BasisA * FMath::Cos(Angle) + BasisB * FMath::Sin(Angle)) *
			CoilRadius * RadiusScale;

		if (Segment > 0)
		{
			HelixSegments->UpdateInstanceTransform(Segment - 1,
				MakeCylinderSegmentTransform(PreviousPoint, Point, WireRadius), true, false, true);
		}
		PreviousPoint = Point;
	}
	// The instances are written in world space and travel thousands of units away from the
	// component origin as the coil walks down the stairs. The cached component bounds drive frustum
	// culling, so they have to be recomputed and pushed to the render thread before the render
	// state is recreated - otherwise the whole component gets culled and nothing draws at all.
	HelixSegments->UpdateBounds();
	HelixSegments->MarkRenderTransformDirty();
	HelixSegments->MarkRenderStateDirty();
}

void ASlinkyActor::UpdateTether()
{
	if (!GrabbedBody)
	{
		TetherSegments->SetVisibility(false);
		return;
	}

	TetherSegments->SetVisibility(true);
	const FVector Start = GetGrabWorldPosition();
	const float Distance = FVector::Distance(Start, DragTarget);
	const float Sag = FMath::Clamp(Distance * 0.10f, 3.0f, 34.0f);
	FVector PreviousPoint = Start;
	for (int32 Segment = 1; Segment <= TetherSegmentCount; ++Segment)
	{
		const float Alpha = static_cast<float>(Segment) / TetherSegmentCount;
		FVector Point = FMath::Lerp(Start, DragTarget, Alpha);
		Point.Z -= 4.0f * Alpha * (1.0f - Alpha) * Sag;
		TetherSegments->UpdateInstanceTransform(Segment - 1,
			MakeCylinderSegmentTransform(PreviousPoint, Point, 1.35f), true, false, true);
		PreviousPoint = Point;
	}
	TetherSegments->UpdateBounds();
	TetherSegments->MarkRenderTransformDirty();
	TetherSegments->MarkRenderStateDirty();
}

void ASlinkyActor::UpdateCamera(float DeltaTime)
{
	const FVector Center = GetCameraFocusLocation();
	constexpr float HorizontalDeadZone = 55.0f;
	constexpr float VerticalDeadZone = 35.0f;
	FVector DesiredLocation = SmoothedCameraLocation;
	DesiredLocation.Y = -1000.0f;

	if (Center.X < SmoothedCameraLocation.X - HorizontalDeadZone)
	{
		DesiredLocation.X = Center.X + HorizontalDeadZone;
	}
	else if (Center.X > SmoothedCameraLocation.X + HorizontalDeadZone)
	{
		DesiredLocation.X = Center.X - HorizontalDeadZone;
	}

	if (Center.Z < SmoothedCameraLocation.Z - VerticalDeadZone)
	{
		DesiredLocation.Z = Center.Z + VerticalDeadZone;
	}
	else if (Center.Z > SmoothedCameraLocation.Z + VerticalDeadZone)
	{
		DesiredLocation.Z = Center.Z - VerticalDeadZone;
	}

	SmoothedCameraLocation = FMath::VInterpTo(SmoothedCameraLocation, DesiredLocation, DeltaTime, 3.2f);
	FollowCameraActor->SetActorLocationAndRotation(SmoothedCameraLocation, FRotator(0.0f, 90.0f, 0.0f));

	// CameraPunch is a damped spring kicked negative by each landed step (see TriggerStepEffects()) -
	// a quick zoom-in "impact" that springs back out past 900 before settling, instead of an instant
	// snap. Purely a presentation flourish; never changes what's actually in frame beyond that punch.
	if (UCameraComponent* FollowCameraComponent = FollowCameraActor->GetCameraComponent())
	{
		FollowCameraComponent->SetOrthoWidth(900.0f + CameraPunch);
	}
}

FVector ASlinkyActor::GetCameraFocusLocation() const
{
	if (PhysicsNodes.IsEmpty())
	{
		return GetActorLocation();
	}

	FVector Center = FVector::ZeroVector;
	for (const UStaticMeshComponent* Node : PhysicsNodes)
	{
		Center += Node->GetComponentLocation();
	}
	return Center / PhysicsNodes.Num();
}

void ASlinkyActor::ConfigureNode(UStaticMeshComponent* Node)
{
	Node->SetRelativeScale3D(ComputeNodeScale());
	Node->SetVisibility(false);
	Node->SetCollisionProfileName(TEXT("PhysicsActor"));
	// Non-adjacent nodes are left free to collide with each other (only the constraint between
	// directly-linked pairs disables collision) so a compressed coil is held up by real contact
	// between its own coils, the same way a physical slinky supports itself, instead of relying
	// purely on the angular springs to fake that stiffness.
	Node->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Block);
	Node->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	Node->SetSimulatePhysics(true);
	Node->SetMassOverrideInKg(NAME_None, 0.085f, true);
	Node->SetLinearDamping(0.35f * DampingScale);
	Node->SetAngularDamping(0.5f * DampingScale);
	Node->SetPhysMaterialOverride(SlinkyPhysicalMaterial);
	Node->SetNotifyRigidBodyCollision(true);
	Node->BodyInstance.bUseCCD = true;
	Node->BodyInstance.PositionSolverIterationCount = 60;
	Node->BodyInstance.VelocitySolverIterationCount = 24;
	Node->BodyInstance.SetMaxDepenetrationVelocity(450.0f);
	Node->BodyInstance.SetDOFLock(EDOFMode::XZPlane);
}

void ASlinkyActor::ConfigureConstraints()
{
	for (int32 Index = 0; Index < NodeConstraints.Num(); ++Index)
	{
		UPhysicsConstraintComponent* Constraint = NodeConstraints[Index];
		UStaticMeshComponent* First = PhysicsNodes[Index];
		UStaticMeshComponent* Second = PhysicsNodes[Index + 1];
		Constraint->SetWorldLocationAndRotation(
			(First->GetComponentLocation() + Second->GetComponentLocation()) * 0.5f,
			FRotator(90.0f, 0.0f, 0.0f));
		Constraint->SetConstrainedComponents(First, NAME_None, Second, NAME_None);
		Constraint->SetDisableCollision(true);

		Constraint->SetLinearYLimit(ELinearConstraintMotion::LCM_Locked, 0.0f);
		Constraint->SetLinearZLimit(ELinearConstraintMotion::LCM_Locked, 0.0f);
		Constraint->SetLinearXLimit(ELinearConstraintMotion::LCM_Limited, MaximumNodeSpacing - GetNodeRestSpacing());
		Constraint->SetLinearPositionDrive(true, false, false);
		Constraint->SetLinearVelocityDrive(true, false, false);
		Constraint->SetLinearPositionTarget(FVector::ZeroVector);
		Constraint->SetLinearVelocityTarget(FVector::ZeroVector);
		Constraint->SetLinearDriveAccelerationMode(false);
		Constraint->SetAngularTwistLimit(EAngularConstraintMotion::ACM_Limited, 24.0f);
		Constraint->SetAngularDriveMode(EAngularDriveMode::SLERP);
		Constraint->SetOrientationDriveSLERP(true);
		Constraint->SetAngularOrientationTarget(FRotator::ZeroRotator);
		Constraint->SetAngularVelocityTarget(FVector::ZeroVector);
	}
	ApplyLinearTuning();
}

void ASlinkyActor::ApplyLinearTuning()
{
	// Only touches the numeric drive/limit values, never SetConstrainedComponents or the joint's
	// world transform, so this is safe to call repeatedly at runtime (live tuning) without
	// recreating the physics joint out from under a coil that may already be moving.
	for (int32 Index = 0; Index < NodeConstraints.Num(); ++Index)
	{
		UPhysicsConstraintComponent* Constraint = NodeConstraints[Index];
		Constraint->SetLinearXLimit(ELinearConstraintMotion::LCM_Limited, MaximumNodeSpacing - GetNodeRestSpacing());

		// Every node above this link adds to the weight it has to hold up, so joints near the
		// bottom (low Index) carry much more load than ones near the top. Stiffen the spring
		// toward the bottom so the coil doesn't compress into a squashed disc under its own
		// weight while the upper, lightly-loaded joints stay soft to drag and stretch.
		// More nodes means more series joints sharing the same total weight, and each joint's own
		// give adds up along the chain - so the per-joint axial stiffness needs to be well above a
		// single-joint estimate or the whole coil sags under its own weight even though each joint
		// looks reasonably stiff in isolation.
		const float LoadFactor = static_cast<float>(NodeConstraints.Num() - Index) / NodeConstraints.Num();
		const float LinearStiffness = FMath::Lerp(600.0f, 2000.0f, LoadFactor) * AxialStiffnessScale;
		const float LinearDamping = FMath::Lerp(20.0f, 42.0f, LoadFactor) * DampingScale;
		Constraint->SetLinearDriveParams(LinearStiffness, LinearDamping, 14000.0f);
	}

	for (UStaticMeshComponent* Node : PhysicsNodes)
	{
		Node->SetLinearDamping(0.35f * DampingScale);
		Node->SetAngularDamping(0.5f * DampingScale);
	}
}

void ASlinkyActor::RefreshCoilTuning()
{
	ApplyLinearTuning();
	ApplyMaterialTuning();
}

void ASlinkyActor::PausePhysicsForLayoutChange()
{
	if (!bPhysicsActivated)
	{
		return;
	}

	// A node resting right at the edge of its tread - the most exposed spot there is - is exactly
	// where a resized tread's new, smaller (or differently placed) footprint is most likely to no
	// longer reach, or a repositioned neighboring tread/riser is most likely to now overlap. Staying
	// simulating through that leaves the node either hanging over nothing (falls, fine) or starting
	// the very next physics step already deep inside a box (not fine - the solver can't always find
	// a way to push a body back out of a full overlap, so it can stay wedged there indefinitely).
	// Staircase->GetTreadTopLocationNear() already reflects the *new* StepDepth/StepRise/
	// RiserThickness (RefreshLayout() sets those before calling this), so lifting anything sitting
	// at or below the new surface up to just above it - before BuildSteps() actually moves the
	// meshes - guarantees every node starts clear of the new geometry rather than inside it.
	if (const ASlinkyStaircase* Staircase = FindStaircase())
	{
		for (UStaticMeshComponent* Node : PhysicsNodes)
		{
			FVector Location = Node->GetComponentLocation();
			const float ClearZ = Staircase->GetTreadTopLocationNear(Location.X).Z + 20.0f;
			if (Location.Z < ClearZ)
			{
				Location.Z = ClearZ;
				Node->SetWorldLocationAndRotation(Location, Node->GetComponentRotation(), false, nullptr,
					ETeleportType::TeleportPhysics);
			}
		}
	}

	for (UStaticMeshComponent* Node : PhysicsNodes)
	{
		Node->SetSimulatePhysics(false);
	}
	bResumePhysicsNextTick = true;
}

void ASlinkyActor::RebuildHelixSegments()
{
	// ClearInstances/AddInstance are safe at runtime - unlike PhysicsNodes/NodeConstraints, the
	// helix is a plain visual InstancedStaticMeshComponent with no per-instance physics state to
	// preserve, so there's no equivalent risk to changing its count on the fly.
	const int32 DesiredCount = FMath::Max(CoilTurns, 1) * SegmentsPerTurn;
	HelixSegments->ClearInstances();
	for (int32 Index = 0; Index < DesiredCount; ++Index)
	{
		HelixSegments->AddInstance(FTransform::Identity);
	}
}

void ASlinkyActor::RefreshNodeScale()
{
	const FVector Scale = ComputeNodeScale();
	for (UStaticMeshComponent* Node : PhysicsNodes)
	{
		Node->SetRelativeScale3D(Scale);
	}
}

ASlinkyStaircase* ASlinkyActor::FindStaircase() const
{
	if (!CachedStaircase.IsValid())
	{
		for (TActorIterator<ASlinkyStaircase> It(GetWorld()); It; ++It)
		{
			CachedStaircase = *It;
			break;
		}
	}
	return CachedStaircase.Get();
}

FVector ASlinkyActor::ComputeNodeScale() const
{
	// XY (disc radius) tracks CoilRadius the same way it always has; Z (disc thickness, i.e. the
	// wire's own cross-section) now tracks WireRadius relative to its default of 1.25, instead of
	// being a fixed 0.014 regardless of WireRadius - previously the wire's visual thickness
	// (HelixSegments, see UpdateHelix) changed but the physics nodes' actual collision thickness
	// never did, so the collision silently stopped matching what was drawn.
	return FVector(CoilRadius / 50.0f, CoilRadius / 50.0f, (WireRadius / 1.25f) * 0.014f);
}

void ASlinkyActor::RefreshCompactLength()
{
	if (PhysicsNodes.IsEmpty())
	{
		return;
	}

	// ConfigureConstraints() (via SetConstrainedComponents) bakes each joint's rest length in from
	// the nodes' world positions at the moment it's called - it's never revisited afterward. So
	// simply changing CompactLength and re-deriving GetNodeRestSpacing() elsewhere (e.g.
	// RefreshCoilTuning) only nudges the stretch *limit*, never the coil's actual resting length.
	// Re-laying the nodes out here and reconfiguring the joints is the only way to make the change
	// visible.
	const bool bWasSimulating = PhysicsNodes[0]->IsSimulatingPhysics();
	const FVector Center = GetCenterLocation();
	const float Spacing = GetNodeRestSpacing();
	const float TotalLength = Spacing * (PhysicsNodes.Num() - 1);

	for (UStaticMeshComponent* Node : PhysicsNodes)
	{
		Node->SetSimulatePhysics(false);
	}
	for (int32 Index = 0; Index < PhysicsNodes.Num(); ++Index)
	{
		UStaticMeshComponent* Node = PhysicsNodes[Index];
		const FVector Location = Center + FVector(0.0f, 0.0f, Index * Spacing - TotalLength * 0.5f);
		Node->SetWorldLocationAndRotation(Location, FRotator::ZeroRotator, false, nullptr,
			ETeleportType::TeleportPhysics);
		Node->SetPhysicsLinearVelocity(FVector::ZeroVector);
		Node->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
	}
	ConfigureConstraints();
	if (bWasSimulating)
	{
		for (UStaticMeshComponent* Node : PhysicsNodes)
		{
			Node->SetSimulatePhysics(true);
		}
	}
}

void ASlinkyActor::ApplyMaterialTuning()
{
	if (SlinkyPhysicalMaterial)
	{
		SlinkyPhysicalMaterial->Restitution = Restitution;
		SlinkyPhysicalMaterial->Friction = Friction;
		// Setting the UPROPERTY fields above only changes this UObject's own data - the physics
		// engine keeps its own copy of the material (SlinkyPhysicalMaterial->MaterialHandle),
		// baked in once the first time GetPhysicsMaterial() is called (from ConfigureNode's
		// SetPhysMaterialOverride during BeginPlay) and otherwise only re-synced when the editor's
		// details panel fires PostEditChangeProperty. Plain gameplay assignment never triggers that,
		// so without this call Restitution/Friction changes from the control panel would update the
		// UI number and this UObject but never actually reach the simulation.
		FChaosEngineInterface::UpdateMaterial(SlinkyPhysicalMaterial->GetPhysicsMaterial(), SlinkyPhysicalMaterial);
	}
}

void ASlinkyActor::UpdateConstraintFlexibility()
{
	// A real slinky's coils resist bending strongly while pressed against their neighbors, but
	// swing much more freely once a stretch has pulled them apart. Softening the angular drive as
	// each link's spacing approaches its stretch limit reproduces that behavior instead of fighting
	// the stretch with a single constant stiffness.
	const float RestSpacing = GetNodeRestSpacing();
	for (int32 Index = 0; Index < NodeConstraints.Num(); ++Index)
	{
		const float Distance = FVector::Distance(PhysicsNodes[Index]->GetComponentLocation(),
			PhysicsNodes[Index + 1]->GetComponentLocation());
		const float StretchAlpha = FMath::Clamp(
			(Distance - RestSpacing) / (MaximumNodeSpacing - RestSpacing), 0.0f, 1.0f);
		const float SmoothStretch = FMath::SmoothStep(0.0f, 1.0f, StretchAlpha);
		const float SwingLimit = FMath::Lerp(38.0f, 72.0f, SmoothStretch);
		const float AngularStrength = FMath::Lerp(450.0f, 28.0f, SmoothStretch) * BendStiffnessScale;
		const float AngularDamping = FMath::Lerp(22.0f, 4.0f, SmoothStretch) * DampingScale;
		const float TorqueLimit = FMath::Lerp(6500.0f, 1500.0f, SmoothStretch);

		UPhysicsConstraintComponent* Constraint = NodeConstraints[Index];
		Constraint->SetAngularSwing1Limit(EAngularConstraintMotion::ACM_Limited, SwingLimit);
		Constraint->SetAngularSwing2Limit(EAngularConstraintMotion::ACM_Limited, SwingLimit);
		Constraint->SetAngularDriveParams(AngularStrength, AngularDamping, TorqueLimit);
	}
}

FVector ASlinkyActor::SampleCenterline(float Alpha) const
{
	const float Position = FMath::Clamp(Alpha, 0.0f, 1.0f) * (PhysicsNodes.Num() - 1);
	const int32 Index = FMath::Min(FMath::FloorToInt(Position), PhysicsNodes.Num() - 2);
	const float LocalAlpha = Position - Index;
	const FVector P0 = PhysicsNodes[FMath::Max(Index - 1, 0)]->GetComponentLocation();
	const FVector P1 = PhysicsNodes[Index]->GetComponentLocation();
	const FVector P2 = PhysicsNodes[Index + 1]->GetComponentLocation();
	const FVector P3 = PhysicsNodes[FMath::Min(Index + 2, PhysicsNodes.Num() - 1)]->GetComponentLocation();
	return CatmullRom(P0, P1, P2, P3, LocalAlpha);
}

FVector ASlinkyActor::SampleCenterlineTangent(float Alpha) const
{
	const float SampleOffset = 0.0025f;
	return (SampleCenterline(FMath::Min(Alpha + SampleOffset, 1.0f)) -
		SampleCenterline(FMath::Max(Alpha - SampleOffset, 0.0f))).GetSafeNormal(UE_SMALL_NUMBER, FVector::UpVector);
}

FVector ASlinkyActor::GetGrabWorldPosition() const
{
	return GrabbedBody ? GrabbedBody->GetComponentTransform().TransformPosition(LocalGrabPoint) : FVector::ZeroVector;
}

void ASlinkyActor::RegisterGroundContact(UPrimitiveComponent* Body, const FHitResult& Hit)
{
	const ASlinkyStaircase* Staircase = Cast<ASlinkyStaircase>(Hit.GetActor());
	if (!Staircase || Hit.ImpactNormal.Z < 0.52f)
	{
		return;
	}

	int32* LastStepIndex = nullptr;
	if (Body == PhysicsNodes[0])
	{
		LastStepIndex = &LastFirstEndStepIndex;
	}
	else if (Body == PhysicsNodes.Last())
	{
		LastStepIndex = &LastLastEndStepIndex;
	}
	else
	{
		return;
	}

	const int32 ContactStepIndex = FMath::FloorToInt(Hit.ImpactPoint.X / Staircase->StepDepth);
	if (ContactStepIndex > *LastStepIndex)
	{
		*LastStepIndex = ContactStepIndex;
		++SuccessfulContacts;
		const bool bTierUp = RegisterCombo();
		TriggerStepEffects(Hit.ImpactPoint, bTierUp);
	}
}

bool ASlinkyActor::RegisterCombo()
{
	const int32 PreviousTier = ComboCount / ComboTierSize;
	++ComboCount;
	ComboTimeRemaining = ComboWindowSeconds;
	BestCombo = FMath::Max(BestCombo, ComboCount);

	// Every landed step gets a quick full-screen color pulse, tier or not - see GetComboFlashAlpha().
	ComboFlashAlpha = 1.0f;

	const int32 NewTier = ComboCount / ComboTierSize;
	const bool bTierUp = NewTier > PreviousTier;
	if (bTierUp)
	{
		// The label itself just escalated (see ComboLabelForTier()) - stage a banner announcing the
		// new tier by name, e.g. "NICE COMBO!!", instead of a generic "level up" message.
		MilestoneText = ComboLabelForTier(NewTier) + TEXT("!!");
		MilestoneTimer = MilestoneDuration;
		MilestoneShake = 1.0f;
	}
	return bTierUp;
}

FString ASlinkyActor::ComboLabelForTier(int32 Tier)
{
	// Escalating, deliberately over-the-top ladder - one rung per 10 combo (see RegisterCombo()),
	// capping out at the silliest one rather than looping, so a very long streak keeps reading as
	// "still climbing" instead of resetting its own vocabulary.
	static const TCHAR* Ladder[] = {
		TEXT("COMBO"),
		TEXT("NICE COMBO"),
		TEXT("GREAT COMBO"),
		TEXT("SUPER COMBO"),
		TEXT("MEGA COMBO"),
		TEXT("ULTRA COMBO"),
		TEXT("CRAZY COMBO"),
		TEXT("INSANE COMBO"),
		TEXT("BONKERS COMBO"),
		TEXT("GODLIKE COMBO"),
	};
	const int32 ClampedTier = FMath::Clamp(Tier, 0, UE_ARRAY_COUNT(Ladder) - 1);
	return Ladder[ClampedTier];
}

FString ASlinkyActor::GetComboLabel() const
{
	return ComboLabelForTier(ComboCount / ComboTierSize);
}

void ASlinkyActor::TriggerStepEffects(const FVector& ImpactLocation, bool bTierUp)
{
	const FLinearColor ComboColor = ComputeComboColor(ComboCount);
	if (RingMaterial)
	{
		RingMaterial->SetVectorParameterValue(TEXT("Color"), ComboColor);
	}
	if (SparkleMaterial)
	{
		SparkleMaterial->SetVectorParameterValue(TEXT("Color"), ComboColor);
	}

	// A tier-up (every 10th combo) gets a second, larger ring a beat behind the first - reads as a
	// double "thump" instead of a single pop, matching the bigger banner/shake it comes with.
	const int32 RingCount = bTierUp ? 2 : 1;
	for (int32 RingIndex = 0; RingIndex < RingCount; ++RingIndex)
	{
		FStepRingFx& Ring = ActiveRings.AddDefaulted_GetRef();
		Ring.Location = ImpactLocation;
		Ring.Age = -RingIndex * 0.08f;
		Ring.Lifetime = bTierUp ? 0.6f : 0.42f;
	}

	// Bigger bursts as the combo grows so a long streak visibly escalates, with an extra jolt right
	// on a tier-up so the milestone reads as a proper "explosion" - capped so it never gets too heavy
	// to update every frame.
	const int32 SparkleCount = FMath::Clamp(6 + ComboCount / 2 + (bTierUp ? 16 : 0), 6, 34);
	for (int32 Index = 0; Index < SparkleCount; ++Index)
	{
		FStepSparkleFx& Sparkle = ActiveSparkles.AddDefaulted_GetRef();
		const float Angle = FMath::FRandRange(0.0f, UE_TWO_PI);
		const float Speed = FMath::FRandRange(120.0f, bTierUp ? 420.0f : 260.0f);
		Sparkle.Location = ImpactLocation;
		Sparkle.Velocity = FVector(FMath::Cos(Angle) * Speed, 0.0f, FMath::Abs(FMath::Sin(Angle)) * Speed + 90.0f);
		Sparkle.RotationAxis = FMath::VRand();
		Sparkle.SpinDegreesPerSec = FMath::FRandRange(180.0f, 720.0f);
		Sparkle.Age = 0.0f;
		Sparkle.Lifetime = FMath::FRandRange(0.4f, bTierUp ? 0.95f : 0.7f);
		Sparkle.BaseScale = FMath::FRandRange(0.3f, bTierUp ? 0.85f : 0.55f);
	}

	// A quick zoom-in "impact" that grows slightly with combo count, with a much bigger kick on a
	// tier-up, clamped so a long streak never yanks the frame too hard.
	const float PunchStrength = bTierUp
		? 620.0f
		: FMath::Clamp(180.0f + ComboCount * 6.0f, 180.0f, 420.0f);
	CameraPunchVelocity -= PunchStrength;
	StepPopVelocity += bTierUp ? 22.0f : 12.0f;
	ComboPopVelocity += bTierUp ? 36.0f : 16.0f;
}

void ASlinkyActor::UpdateStepEffects(float DeltaTime)
{
	for (int32 Index = ActiveRings.Num() - 1; Index >= 0; --Index)
	{
		ActiveRings[Index].Age += DeltaTime;
		if (ActiveRings[Index].Age >= ActiveRings[Index].Lifetime)
		{
			ActiveRings.RemoveAt(Index);
		}
	}
	while (StepRings->GetInstanceCount() < ActiveRings.Num())
	{
		StepRings->AddInstance(FTransform(FVector(0.001f)));
	}
	while (StepRings->GetInstanceCount() > ActiveRings.Num())
	{
		StepRings->RemoveInstance(StepRings->GetInstanceCount() - 1);
	}
	for (int32 Index = 0; Index < ActiveRings.Num(); ++Index)
	{
		const FStepRingFx& Ring = ActiveRings[Index];
		const float Alpha = FMath::Clamp(Ring.Age / Ring.Lifetime, 0.0f, 1.0f);
		// Ring expands fast then eases off (Pow < 1) while its thickness collapses to nothing, so it
		// reads as an outward shockwave that thins out and vanishes rather than fading in place.
		const float RadiusScale = FMath::Lerp(0.15f, 3.2f, FMath::Pow(Alpha, 0.6f));
		const float ThicknessScale = FMath::Lerp(0.45f, 0.0f, Alpha);
		const FTransform Transform(FRotator(90.0f, 0.0f, 0.0f), Ring.Location + FVector(0.0f, 0.0f, 2.0f),
			FVector(RadiusScale, RadiusScale, ThicknessScale));
		StepRings->UpdateInstanceTransform(Index, Transform, true, false, true);
	}
	if (ActiveRings.Num() > 0)
	{
		StepRings->UpdateBounds();
		StepRings->MarkRenderTransformDirty();
		StepRings->MarkRenderStateDirty();
	}

	for (int32 Index = ActiveSparkles.Num() - 1; Index >= 0; --Index)
	{
		FStepSparkleFx& Sparkle = ActiveSparkles[Index];
		Sparkle.Age += DeltaTime;
		if (Sparkle.Age >= Sparkle.Lifetime)
		{
			ActiveSparkles.RemoveAt(Index);
			continue;
		}
		Sparkle.Velocity.Z -= 420.0f * DeltaTime;
		Sparkle.Location += Sparkle.Velocity * DeltaTime;
	}
	while (StepSparkles->GetInstanceCount() < ActiveSparkles.Num())
	{
		StepSparkles->AddInstance(FTransform(FVector(0.001f)));
	}
	while (StepSparkles->GetInstanceCount() > ActiveSparkles.Num())
	{
		StepSparkles->RemoveInstance(StepSparkles->GetInstanceCount() - 1);
	}
	for (int32 Index = 0; Index < ActiveSparkles.Num(); ++Index)
	{
		const FStepSparkleFx& Sparkle = ActiveSparkles[Index];
		const float Alpha = FMath::Clamp(Sparkle.Age / Sparkle.Lifetime, 0.0f, 1.0f);
		const float Scale = Sparkle.BaseScale * FMath::Lerp(1.0f, 0.0f, FMath::Pow(Alpha, 1.5f)) * 0.12f;
		const FQuat Rotation(Sparkle.RotationAxis, FMath::DegreesToRadians(Sparkle.SpinDegreesPerSec * Sparkle.Age));
		const FTransform Transform(Rotation, Sparkle.Location, FVector(Scale));
		StepSparkles->UpdateInstanceTransform(Index, Transform, true, false, true);
	}
	if (ActiveSparkles.Num() > 0)
	{
		StepSparkles->UpdateBounds();
		StepSparkles->MarkRenderTransformDirty();
		StepSparkles->MarkRenderStateDirty();
	}
}

FLinearColor ASlinkyActor::ComputeComboColor(int32 ForCombo) const
{
	// High, fixed saturation/value keeps every hue equally vivid - the point is a poppy rainbow
	// cycle, not a naturalistic color.
	const float Hue = FMath::Fmod(static_cast<float>(FMath::Max(ForCombo, 1)) * 34.0f, 360.0f);
	return FLinearColor::MakeFromHSV8(static_cast<uint8>(Hue / 360.0f * 255.0f), 200, 255);
}

float ASlinkyActor::GetComboWindowRemaining01() const
{
	return FMath::Clamp(ComboTimeRemaining / ComboWindowSeconds, 0.0f, 1.0f);
}

float ASlinkyActor::GetMilestoneAlpha01() const
{
	return FMath::Clamp(MilestoneTimer / MilestoneDuration, 0.0f, 1.0f);
}

FLinearColor ASlinkyActor::GetComboColor() const
{
	return ComputeComboColor(FMath::Max(ComboCount, 1));
}

void ASlinkyActor::OnNodeHit(UPrimitiveComponent* HitComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComponent, FVector NormalImpulse, const FHitResult& Hit)
{
	RegisterGroundContact(HitComponent, Hit);
}

void ASlinkyActor::CycleTuningParam()
{
	SelectedTuningParam = static_cast<ETuningParam>(
		(static_cast<uint8>(SelectedTuningParam) + 1) % static_cast<uint8>(ETuningParam::Count));
}

void ASlinkyActor::AdjustTuningParam(float Direction)
{
	switch (SelectedTuningParam)
	{
	case ETuningParam::CompactLength:
		CompactLength = FMath::Max(CompactLength + Direction * 5.0f, 20.0f);
		RefreshCompactLength();
		break;
	case ETuningParam::MaximumNodeSpacing:
		MaximumNodeSpacing = FMath::Max(MaximumNodeSpacing + Direction * 5.0f, GetNodeRestSpacing() + 1.0f);
		ApplyLinearTuning();
		break;
	case ETuningParam::WireRadius:
		WireRadius = FMath::Max(WireRadius + Direction * 0.25f, 0.25f);
		RefreshNodeScale();
		break;
	case ETuningParam::AxialStiffnessScale:
		AxialStiffnessScale = FMath::Max(AxialStiffnessScale + Direction * 0.1f, 0.1f);
		ApplyLinearTuning();
		break;
	case ETuningParam::BendStiffnessScale:
		BendStiffnessScale = FMath::Max(BendStiffnessScale + Direction * 0.1f, 0.1f);
		break;
	case ETuningParam::DampingScale:
		DampingScale = FMath::Max(DampingScale + Direction * 0.1f, 0.1f);
		ApplyLinearTuning();
		break;
	case ETuningParam::Restitution:
		Restitution = FMath::Clamp(Restitution + Direction * 0.02f, 0.0f, 1.0f);
		ApplyMaterialTuning();
		break;
	case ETuningParam::Friction:
		Friction = FMath::Max(Friction + Direction * 0.05f, 0.0f);
		ApplyMaterialTuning();
		break;
	case ETuningParam::CoilTurns:
		CoilTurns = FMath::Clamp(CoilTurns + FMath::RoundToInt(Direction) * 2, 6, 72);
		RebuildHelixSegments();
		break;
	case ETuningParam::CoilRadius:
		CoilRadius = FMath::Max(CoilRadius + Direction * 2.5f, 15.0f);
		RefreshNodeScale();
		break;
	default:
		break;
	}
}

FString ASlinkyActor::GetTuningParamDisplay() const
{
	switch (SelectedTuningParam)
	{
	case ETuningParam::CompactLength:
		return FString::Printf(TEXT("compact length %.0f"), CompactLength);
	case ETuningParam::MaximumNodeSpacing:
		return FString::Printf(TEXT("max stretch %.0f"), MaximumNodeSpacing);
	case ETuningParam::WireRadius:
		return FString::Printf(TEXT("wire radius %.2f"), WireRadius);
	case ETuningParam::AxialStiffnessScale:
		return FString::Printf(TEXT("axial stiffness x%.1f"), AxialStiffnessScale);
	case ETuningParam::BendStiffnessScale:
		return FString::Printf(TEXT("bend stiffness x%.1f"), BendStiffnessScale);
	case ETuningParam::DampingScale:
		return FString::Printf(TEXT("damping x%.1f"), DampingScale);
	case ETuningParam::Restitution:
		return FString::Printf(TEXT("restitution %.2f"), Restitution);
	case ETuningParam::Friction:
		return FString::Printf(TEXT("friction %.2f"), Friction);
	case ETuningParam::CoilTurns:
		return FString::Printf(TEXT("coil turns %d"), CoilTurns);
	case ETuningParam::CoilRadius:
		return FString::Printf(TEXT("coil radius %.1f"), CoilRadius);
	default:
		return TEXT("");
	}
}
