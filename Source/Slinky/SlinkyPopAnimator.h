#pragma once

#include "CoreMinimal.h"
#include "SlinkyPopAnimator.generated.h"

class UButton;
class UWidget;

// A tiny spring-physics scale/opacity animator bound to one UMG widget, giving it a "poppy",
// slightly-overshooting bounce instead of an instant snap - used for every button's hover/press
// feedback and for each section's expand/collapse pop.
//
// One instance per animated widget (see USlinkyControlPanel::Animators), not one shared instance:
// UButton's OnHovered/OnUnhovered/OnPressed/OnReleased are dynamic multicast delegates, which can
// only bind UFUNCTIONs and can't tell a single shared handler which button actually fired. Binding
// a *fresh instance* of this class per button sidesteps that without needing one named UFUNCTION
// pair per button (there are 24 buttons on the panel).
UCLASS()
class UPopAnimator : public UObject
{
	GENERATED_BODY()

public:
	// Widget is whatever this animator scales/fades - a button (for hover/press feedback) or a
	// rows box (for expand/collapse). Idle/Hover/Press only matter when BindButtonEvents is also
	// called; a non-button target (a rows box) just uses SetTarget directly instead.
	void Bind(UWidget* InWidget, float InIdleScale = 1.0f, float InHoverScale = 1.08f, float InPressScale = 0.88f);
	void BindButtonEvents(UButton* InButton);

	// Springs the widget's scale/opacity toward NewTarget. bInCollapseWhenSettled sets the widget
	// to Collapsed once it actually settles at (or near) a target of 0 - used to let a section's
	// rows shrink away before they stop taking up layout space, instead of vanishing instantly.
	void SetTarget(float NewTarget, bool bInCollapseWhenSettled = false);

	// Advances the spring by DeltaTime and pushes the result onto the bound widget. Called once
	// per frame per animator from USlinkyControlPanel::NativeTick.
	void Tick(float DeltaTime);

	UFUNCTION() void HandleHovered();
	UFUNCTION() void HandleUnhovered();
	UFUNCTION() void HandlePressed();
	UFUNCTION() void HandleReleased();

private:
	TWeakObjectPtr<UWidget> Widget;

	float Current = 1.0f;
	float Velocity = 0.0f;
	float Target = 1.0f;

	float IdleScale = 1.0f;
	float HoverScale = 1.08f;
	float PressScale = 0.88f;

	bool bHovered = false;
	bool bCollapseWhenSettled = false;
};
