#include "SlinkyPopAnimator.h"
#include "SlinkyGameInstance.h"
#include "Components/Button.h"
#include "Components/Widget.h"
#include "Engine/World.h"

void UPopAnimator::Bind(UWidget* InWidget, float InIdleScale, float InHoverScale, float InPressScale)
{
	Widget = InWidget;
	IdleScale = InIdleScale;
	HoverScale = InHoverScale;
	PressScale = InPressScale;
	Current = InIdleScale;
	Target = InIdleScale;
}

void UPopAnimator::BindButtonEvents(UButton* InButton)
{
	if (!InButton)
	{
		return;
	}
	InButton->OnHovered.AddDynamic(this, &UPopAnimator::HandleHovered);
	InButton->OnUnhovered.AddDynamic(this, &UPopAnimator::HandleUnhovered);
	InButton->OnPressed.AddDynamic(this, &UPopAnimator::HandlePressed);
	InButton->OnReleased.AddDynamic(this, &UPopAnimator::HandleReleased);
}

void UPopAnimator::SetTarget(float NewTarget, bool bInCollapseWhenSettled)
{
	Target = NewTarget;
	bCollapseWhenSettled = bInCollapseWhenSettled;
}

void UPopAnimator::Tick(float DeltaTime)
{
	UWidget* W = Widget.Get();
	if (!W)
	{
		return;
	}

	// A slightly underdamped spring - about half of critical damping (2*sqrt(Stiffness)) - so it
	// overshoots the target a little and settles back rather than easing in flatly: the actual
	// "pop" in "poppy".
	constexpr float Stiffness = 500.0f;
	constexpr float Damping = 22.0f;
	// This semi-implicit Euler integration only stays stable for DeltaTime below roughly
	// 2/sqrt(Stiffness) (~0.09s here). A single hitch (shader compile, GC, alt-tab) produces one
	// large DeltaTime frame that blows Velocity/Current up past that threshold, and the spring
	// diverges from there - the button balloons to a huge scale and never recovers. Clamping the
	// DeltaTime used for integration (running several fixed sub-steps for a big frame instead of one
	// giant one) keeps every step inside the stable range without changing normal-frame feel.
	constexpr float MaxSubStep = 1.0f / 60.0f;
	int32 SubSteps = FMath::Max(FMath::CeilToInt(DeltaTime / MaxSubStep), 1);
	const float SubDeltaTime = DeltaTime / SubSteps;
	for (int32 Step = 0; Step < SubSteps; ++Step)
	{
		const float Accel = (Target - Current) * Stiffness - Velocity * Damping;
		Velocity += Accel * SubDeltaTime;
		Current += Velocity * SubDeltaTime;
	}

	W->SetRenderScale(FVector2D(Current, Current));
	W->SetRenderOpacity(FMath::Clamp(Current, 0.0f, 1.0f));

	if (bCollapseWhenSettled && Target <= 0.0f && Current < 0.02f && FMath::Abs(Velocity) < 0.05f)
	{
		W->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UPopAnimator::HandleHovered()
{
	bHovered = true;
	SetTarget(HoverScale, false);
}

void UPopAnimator::HandleUnhovered()
{
	bHovered = false;
	SetTarget(IdleScale, false);
}

void UPopAnimator::HandlePressed()
{
	SetTarget(PressScale, false);

	// One injection point covers every button on every screen (title, control panel, pause menu) -
	// BindButtonEvents() above is what every one of them goes through, and a rows-box pop (which
	// only ever gets Bind(), never BindButtonEvents()) never reaches HandlePressed at all.
	if (UWorld* World = GetWorld())
	{
		if (USlinkyGameInstance* GameInstance = Cast<USlinkyGameInstance>(World->GetGameInstance()))
		{
			GameInstance->PlaySfx(ESlinkySfx::ButtonClick);
		}
	}
}

void UPopAnimator::HandleReleased()
{
	SetTarget(bHovered ? HoverScale : IdleScale, false);
}
