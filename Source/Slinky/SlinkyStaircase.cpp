#include "SlinkyStaircase.h"
#include "SlinkyActor.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

ASlinkyStaircase::ASlinkyStaircase()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.25f;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	Treads = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Treads"));
	Treads->SetupAttachment(SceneRoot);
	Treads->SetCollisionProfileName(TEXT("BlockAll"));
	Treads->SetMobility(EComponentMobility::Movable);

	Risers = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Risers"));
	Risers->SetupAttachment(SceneRoot);
	Risers->SetCollisionProfileName(TEXT("BlockAll"));
	Risers->SetMobility(EComponentMobility::Movable);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (Cube.Succeeded())
	{
		Treads->SetStaticMesh(Cube.Object);
		Risers->SetStaticMesh(Cube.Object);
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> StepMaterial(
		TEXT("/Game/Slinky/M_Wood.M_Wood"));
	if (StepMaterial.Succeeded())
	{
		Treads->SetMaterial(0, StepMaterial.Object);
		Risers->SetMaterial(0, StepMaterial.Object);
	}

	// Matches what MakeTreadTransform used to compute for StepIndex 0 before it was anchor-relative,
	// so the very first layout (before any live parameter edit) is unchanged.
	AnchorTreadLocation = FVector(0.5f * StepDepth, 0.0f, -6.0f);
	AnchorEdgeX = AnchorTreadLocation.X - StepDepth * 0.5f;
}

void ASlinkyStaircase::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	BuildSteps();
}

void ASlinkyStaircase::RefreshLayout()
{
	// Every tread/riser but the exact one under the coil moves when StepDepth/StepRise/
	// RiserThickness changes (see MakeTreadTransform) - simulating straight through that teleport
	// can wedge the coil into the new geometry with no way for the solver to push it back out. See
	// ASlinkyActor::PausePhysicsForLayoutChange().
	if (ASlinkyActor* Slinky = FindSlinky())
	{
		Slinky->PausePhysicsForLayoutChange();
	}
	BuildSteps();
}

void ASlinkyStaircase::SetStepDepth(float NewValue)
{
	CaptureAnchor();
	StepDepth = NewValue;
	RefreshLayout();
}

void ASlinkyStaircase::SetStepRise(float NewValue)
{
	CaptureAnchor();
	StepRise = NewValue;
	RefreshLayout();
}

void ASlinkyStaircase::SetRiserThickness(float NewValue)
{
	CaptureAnchor();
	RiserThickness = NewValue;
	RefreshLayout();
}

void ASlinkyStaircase::CaptureAnchor()
{
	const ASlinkyActor* Slinky = FindSlinky();
	if (!Slinky)
	{
		return;
	}

	// Read with the *old* StepDepth/StepRise and the *previous* anchor - both still describe the
	// real, currently-visible layout, since the caller hasn't changed anything yet.
	const int32 StepIndex = GetStepIndexNear(Slinky->GetCenterLocation().X);
	AnchorTreadLocation = MakeTreadTransform(StepIndex).GetLocation();
	// StepDepth here is still the *old* value - CaptureAnchor() always runs before the setter that
	// called it assigns the new one - so this is the anchor tread's real current left edge.
	AnchorEdgeX = AnchorTreadLocation.X - StepDepth * 0.5f;
	AnchorStepIndex = StepIndex;
}

FVector ASlinkyStaircase::GetTreadTopLocationNear(float WorldX) const
{
	FVector Location = MakeTreadTransform(GetStepIndexNear(WorldX)).GetLocation();
	Location.Z += 6.0f; // Half the tread's fixed 12uu thickness (scale.Z is always 0.12).
	return Location;
}

void ASlinkyStaircase::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	RecycleSteps();
}

void ASlinkyStaircase::BuildSteps()
{
	if (Treads->GetInstanceCount() != StepCount ||
		Risers->GetInstanceCount() != StepCount)
	{
		Treads->ClearInstances();
		Risers->ClearInstances();
		for (int32 Slot = 0; Slot < StepCount; ++Slot)
		{
			const int32 StepIndex = FirstStepIndex + Slot;
			Treads->AddInstance(MakeTreadTransform(StepIndex));
			Risers->AddInstance(MakeRiserTransform(StepIndex));
		}
		Treads->UpdateBounds();
		Risers->UpdateBounds();
		Treads->MarkRenderTransformDirty();
		Risers->MarkRenderTransformDirty();
		return;
	}

	// Re-slide the existing instances instead of clearing and re-adding them: clearing briefly
	// removes their collision from the physics scene, which can let the slinky free-fall into a
	// tread for a frame and end up stuck inside it once the step is rebuilt underneath it.
	for (int32 Slot = 0; Slot < StepCount; ++Slot)
	{
		const int32 StepIndex = FirstStepIndex + Slot;
		Treads->UpdateInstanceTransform(Slot, MakeTreadTransform(StepIndex), true, false, true);
		Risers->UpdateInstanceTransform(Slot, MakeRiserTransform(StepIndex), true, false, true);
	}
	// Same reason as the coil: the recycled steps slide far away from the actor origin, so the
	// cached bounds must be recomputed and sent to the renderer or the steps get frustum-culled.
	Treads->UpdateBounds();
	Risers->UpdateBounds();
	Treads->MarkRenderTransformDirty();
	Risers->MarkRenderTransformDirty();
	Treads->MarkRenderStateDirty();
	Risers->MarkRenderStateDirty();
}

void ASlinkyStaircase::RecycleSteps()
{
	ASlinkyActor* Slinky = FindSlinky();
	if (!Slinky)
	{
		return;
	}

	const int32 SlinkyStep = GetStepIndexNear(Slinky->GetCenterLocation().X);
	const int32 DesiredFirst = SlinkyStep - 20;
	if (FMath::Abs(DesiredFirst - FirstStepIndex) > 8)
	{
		FirstStepIndex = DesiredFirst;
		BuildSteps();
	}
}

FTransform ASlinkyStaircase::MakeTreadTransform(int32 StepIndex) const
{
	// AnchorEdgeX/AnchorStepIndex/AnchorTreadLocation.Z mark a fixed point in world space (the
	// anchor step's left edge - see CaptureAnchor()) that DeltaIndex*StepDepth/StepRise accumulates
	// away from, so a StepDepth/StepRise change (see SetStepDepth/SetStepRise) rescales the flight
	// around wherever the slinky currently is instead of around a fixed, possibly many-steps-away
	// origin. The "+0.5*StepDepth" centering term is NOT part of that anchor - it's recomputed fresh
	// with the *current* StepDepth every call, or every tread (not just ones far from the anchor)
	// would stay offset by half of whatever StepDepth just changed by.
	const float DeltaIndex = static_cast<float>(StepIndex - AnchorStepIndex);
	const FVector Location(AnchorEdgeX + DeltaIndex * StepDepth + 0.5f * StepDepth, 0.0f,
		AnchorTreadLocation.Z - DeltaIndex * StepRise);
	return FTransform(FRotator::ZeroRotator, Location,
		FVector(StepDepth / 100.0f, GetCoilRadius() * 1.5f / 100.0f, 0.12f));
}

FTransform ASlinkyStaircase::MakeRiserTransform(int32 StepIndex) const
{
	// Same anchor as MakeTreadTransform (a riser sits StepDepth past its tread's left edge, minus
	// half its own thickness). As with the tread's centering term, "-RiserThickness*0.5" and
	// "-StepRise*0.5" always use the *current* values rather than being baked into the anchor, so a
	// RiserThickness or StepRise edit doesn't leave every riser offset by half of whatever changed.
	const float DeltaIndex = static_cast<float>(StepIndex - AnchorStepIndex);
	const FVector Location(AnchorEdgeX + DeltaIndex * StepDepth + StepDepth - RiserThickness * 0.5f, 0.0f,
		AnchorTreadLocation.Z - DeltaIndex * StepRise - StepRise * 0.5f);
	return FTransform(FRotator::ZeroRotator, Location,
		FVector(RiserThickness / 100.0f, GetCoilRadius() * 1.5f / 100.0f, StepRise / 100.0f));
}

float ASlinkyStaircase::GetCoilRadius() const
{
	const ASlinkyActor* Slinky = FindSlinky();
	return Slinky ? Slinky->CoilRadius : 45.0f;
}

ASlinkyActor* ASlinkyStaircase::FindSlinky() const
{
	if (!CachedSlinky.IsValid())
	{
		for (TActorIterator<ASlinkyActor> It(GetWorld()); It; ++It)
		{
			CachedSlinky = *It;
			break;
		}
	}
	return CachedSlinky.Get();
}

