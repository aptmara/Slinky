#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SlinkyControlPanel.generated.h"

class ASlinkyActor;
class ASlinkyStaircase;
class UBorder;
class UButton;
class UOverlay;
class UPanelWidget;
class UPopAnimator;
class UTextBlock;
class UVerticalBox;

// A fully code-built (no widget-blueprint asset) on-screen panel of pastel, rounded, chunky "+"/"-"
// rows for every live-tunable stair and coil parameter, split into a stair section and a slinky
// section, replacing/duplicating the keyboard-only tuning in ASlinkyPlayerController with something
// visible and mouse-driven. Built entirely in C++ via WidgetTree, matching the rest of the project.
UCLASS()
class USlinkyControlPanel : public UUserWidget
{
	GENERATED_BODY()

public:
	// Builds the whole widget hierarchy here rather than in NativeConstruct(): NativeConstruct()
	// only fires after the widget's underlying Slate content has already been taken from
	// WidgetTree (via RebuildWidget), so populating WidgetTree that late leaves the panel
	// constructed but never actually shown. NativeOnInitialized() runs before that first
	// RebuildWidget, which is why every C++-only (no widget-blueprint asset) UMG widget needs to
	// build its tree here instead.
	virtual void NativeOnInitialized() override;

	// Drives every UPopAnimator's spring each frame - the one Tick this whole panel needs, shared
	// by all 24+ buttons and both sections' expand/collapse pop.
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	// Wires the panel to the live actors and pushes their current values into every row. Safe to
	// call once they exist - ASlinkyPlayerController acquires both lazily and calls this as soon
	// as they're found.
	void SetTargets(ASlinkyStaircase* InStaircase, ASlinkyActor* InSlinky);

private:
	// Which row a value belongs to - indexes Values/MinValues/MaxValues/Steps/ValueTexts/Suffixes
	// so the row-building code and the individual button handlers below share one layout instead
	// of eleven near-copies.
	enum class EParam : uint8
	{
		StepDepth, StepRise, RiserThickness,
		CompactLength, MaxNodeSpacing, WireRadius,
		AxialStiffness, BendStiffness, Damping, Restitution, Friction,
		CoilTurns, CoilRadius,
		Count
	};

	// A rounded "3D" card: a solid-color rounded Border on top of a second, darker rounded Border
	// offset down-and-right by ShadowOffset px, so the darker one peeks out as a drop shadow. Used
	// for every row, section header, and +/- button - the one primitive the whole panel's chunky
	// look is built from.
	UBorder* AddShadowCard(UPanelWidget* Parent, FLinearColor FillColor, float ShadowOffset, float Radius);

	// A rounded, clickable label bar for a group of rows (stairs vs. slinky), added to Parent
	// *after* RowsBox so it lands at the bottom of the column - clicking it toggles RowsBox
	// between Collapsed (just the bar, its rows take no layout space) and Visible. Rows start
	// collapsed, so at rest the panel is just the two bottom bars, not a wall of controls.
	UButton* AddSectionHeader(UPanelWidget* Parent, const FString& Text, FLinearColor Accent, TObjectPtr<UTextBlock>& OutHeaderText);
	void ToggleSection(UVerticalBox* RowsBox, UPopAnimator* RowsAnim, UTextBlock* HeaderText, const FString& Label);

	// A small button added at the bottom of the slinky section's rows, cycling
	// ASlinkyActor::ECoilMaterialStyle (Metal <-> PlasticRainbow) on every click and relabeling
	// itself to name whichever style it just switched *to*.
	UButton* AddMaterialStyleButton(UPanelWidget* Parent);

	// Creates a fresh UPopAnimator bound to Button's hover/press events and registers it in
	// Animators so NativeTick keeps advancing it. See UPopAnimator's own comment for why each
	// button needs its own instance rather than one shared handler.
	UPopAnimator* AddButtonPop(UButton* Button);

	UFUNCTION() void OnStairHeaderClicked();
	UFUNCTION() void OnSlinkyHeaderClicked();
	UFUNCTION() void OnMaterialStyleClicked();

	// Builds one pastel row: label, live value readout, and a "-"/"+" button pair, and registers
	// its state at index (int32)Param in the arrays below. bInteger drops the value readout's
	// decimal places (e.g. coil turns is a whole count, not "36.00").
	void AddRow(UPanelWidget* Parent, EParam Param, const FString& Label, float Min, float Max,
		float Step, float InitialValue, const TCHAR* Suffix, FLinearColor Accent, bool bInteger = false);
	void RefreshValueText(EParam Param, float Value);

	// Shared by every button handler: steps Values[Param] by Sign*Steps[Param], clamps it, and
	// dispatches to the one Apply* function that actually pushes it onto the live actor.
	void AdjustValue(EParam Param, float Sign);
	void ApplyValue(EParam Param, float Value);

	// UButton::OnClicked (dynamic multicast, zero parameters) can only bind UFUNCTIONs and can't
	// tell the handler which button fired, hence one pair of named handlers per parameter rather
	// than a generic dispatch table. Each is a one-line call into AdjustValue.
	UFUNCTION() void OnStepDepthMinus();
	UFUNCTION() void OnStepDepthPlus();
	UFUNCTION() void OnStepRiseMinus();
	UFUNCTION() void OnStepRisePlus();
	UFUNCTION() void OnRiserThicknessMinus();
	UFUNCTION() void OnRiserThicknessPlus();
	UFUNCTION() void OnCompactLengthMinus();
	UFUNCTION() void OnCompactLengthPlus();
	UFUNCTION() void OnMaxNodeSpacingMinus();
	UFUNCTION() void OnMaxNodeSpacingPlus();
	UFUNCTION() void OnWireRadiusMinus();
	UFUNCTION() void OnWireRadiusPlus();
	UFUNCTION() void OnAxialStiffnessMinus();
	UFUNCTION() void OnAxialStiffnessPlus();
	UFUNCTION() void OnBendStiffnessMinus();
	UFUNCTION() void OnBendStiffnessPlus();
	UFUNCTION() void OnDampingMinus();
	UFUNCTION() void OnDampingPlus();
	UFUNCTION() void OnRestitutionMinus();
	UFUNCTION() void OnRestitutionPlus();
	UFUNCTION() void OnFrictionMinus();
	UFUNCTION() void OnFrictionPlus();
	UFUNCTION() void OnCoilTurnsMinus();
	UFUNCTION() void OnCoilTurnsPlus();
	UFUNCTION() void OnCoilRadiusMinus();
	UFUNCTION() void OnCoilRadiusPlus();

	UPROPERTY()
	TObjectPtr<ASlinkyStaircase> Staircase;

	UPROPERTY()
	TObjectPtr<ASlinkyActor> Slinky;

	UPROPERTY()
	TArray<TObjectPtr<UTextBlock>> ValueTexts;

	UPROPERTY()
	TObjectPtr<UVerticalBox> StairRowsBox;

	UPROPERTY()
	TObjectPtr<UVerticalBox> SlinkyRowsBox;

	UPROPERTY()
	TObjectPtr<UTextBlock> StairHeaderText;

	UPROPERTY()
	TObjectPtr<UTextBlock> SlinkyHeaderText;

	UPROPERTY()
	TObjectPtr<UTextBlock> MaterialStyleButtonText;

	UPROPERTY()
	TObjectPtr<UPopAnimator> StairRowsAnim;

	UPROPERTY()
	TObjectPtr<UPopAnimator> SlinkyRowsAnim;

	// Every UPopAnimator this panel owns (one per button, plus the two rows-box ones), ticked
	// uniformly from NativeTick.
	UPROPERTY()
	TArray<TObjectPtr<UPopAnimator>> Animators;

	TArray<float> Values;
	TArray<float> MinValues;
	TArray<float> MaxValues;
	TArray<float> Steps;
	TArray<const TCHAR*> Suffixes;
	TArray<bool> IsInteger;
};
