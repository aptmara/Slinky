#include "SlinkyControlPanel.h"
#include "SlinkyActor.h"
#include "SlinkyPopAnimator.h"
#include "SlinkyStaircase.h"
#include "Blueprint/WidgetTree.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"

// Names here are prefixed (Panel*) because a packaged (non-editor) build's Unity build merges
// several widget .cpp files - each with their own similarly-named anonymous-namespace helpers -
// into one translation unit, where an unqualified "Shade"/"ButtonRadius" in more than one of them
// is a redefinition error rather than the per-file-scoped name it is in a normal (non-unity) build.
namespace
{
	constexpr float PanelCardRadius = 12.0f;
	constexpr float PanelButtonRadius = 9.0f;
	// Both columns are forced to this same width (via a SizeBox each), so the two header buttons
	// line up as a matched pair even collapsed, when there are no rows to determine a natural
	// width - and wide enough for the widest label ("軸方向の硬さ" / "摩擦係数") without wrapping.
	constexpr float ColumnWidth = 250.0f;

	// A darker, slightly desaturated shade of Color for the drop-shadow layer under a card/button.
	FLinearColor PanelShade(const FLinearColor& Color)
	{
		return FLinearColor(Color.R * 0.55f, Color.G * 0.48f, Color.B * 0.58f, 1.0f);
	}
}

void USlinkyControlPanel::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	for (UPopAnimator* Anim : Animators)
	{
		if (Anim)
		{
			Anim->Tick(InDeltaTime);
		}
	}
}

UPopAnimator* USlinkyControlPanel::AddButtonPop(UButton* Button)
{
	UPopAnimator* Anim = NewObject<UPopAnimator>(this);
	Anim->Bind(Button);
	Anim->BindButtonEvents(Button);
	Animators.Add(Anim);
	return Anim;
}

void USlinkyControlPanel::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	Values.SetNum((int32)EParam::Count);
	MinValues.SetNum((int32)EParam::Count);
	MaxValues.SetNum((int32)EParam::Count);
	Steps.SetNum((int32)EParam::Count);
	ValueTexts.SetNum((int32)EParam::Count);
	Suffixes.SetNum((int32)EParam::Count);
	IsInteger.SetNum((int32)EParam::Count);

	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Root"));
	WidgetTree->RootWidget = Root;

	UVerticalBox* Container = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Container"));
	if (UCanvasPanelSlot* ContainerSlot = Root->AddChildToCanvas(Container))
	{
		// Anchored to the bottom-right corner and growing upward as rows are added, instead of
		// hanging down from the top like a dropdown menu.
		ContainerSlot->SetAnchors(FAnchors(1.0f, 1.0f));
		ContainerSlot->SetAlignment(FVector2D(1.0f, 1.0f));
		ContainerSlot->SetPosition(FVector2D(-24.0f, -24.0f));
		ContainerSlot->SetAutoSize(true);
	}

	// A soft, chalky pastel palette, cycled row by row - saturated enough to tell rows apart,
	// gentle enough to read as a toy's control panel rather than a warning light.
	const FLinearColor Palette[] = {
		FLinearColor(0.98f, 0.80f, 0.87f), // pastel pink
		FLinearColor(0.78f, 0.92f, 0.85f), // pastel mint
		FLinearColor(0.87f, 0.83f, 0.97f), // pastel lavender
		FLinearColor(0.99f, 0.86f, 0.72f), // pastel peach
		FLinearColor(0.80f, 0.90f, 0.98f), // pastel sky
		FLinearColor(0.99f, 0.95f, 0.75f), // pastel yellow
	};
	int32 ColorIndex = 0;
	const auto NextColor = [&Palette, &ColorIndex]()
	{
		return Palette[(ColorIndex++) % UE_ARRAY_COUNT(Palette)];
	};

	// Stairs and slinky side by side rather than one long stacked list - this is also what keeps
	// the panel short enough to fit a normal screen height even bottom-anchored, since the taller
	// (slinky, 8 rows) column no longer has the shorter (stairs, 3 rows) one piled on top of it.
	UHorizontalBox* Columns = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	Container->AddChildToVerticalBox(Columns);

	UVerticalBox* StairColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	{
		// A fixed-width SizeBox around the column, not just HAlign_Fill inside it: Fill alone only
		// matches a header to its *own* rows' natural width, which still leaves the two columns
		// (different row counts and label lengths) different widths from each other - most visible
		// collapsed, when there are no rows at all to size against.
		USizeBox* Sized = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		Sized->SetWidthOverride(ColumnWidth);
		Sized->SetContent(StairColumn);
		if (UHorizontalBoxSlot* S = Columns->AddChildToHorizontalBox(Sized))
		{
			// Bottom-aligned, not top: both header buttons must stay level with each other
			// regardless of which section (if either) is currently expanded, since expanding only
			// grows a column upward from its header, never down from a shared top.
			S->SetVerticalAlignment(VAlign_Bottom);
			S->SetPadding(FMargin(0.0f, 0.0f, 12.0f, 0.0f));
		}
	}

	UVerticalBox* SlinkyColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	{
		USizeBox* Sized = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		Sized->SetWidthOverride(ColumnWidth);
		Sized->SetContent(SlinkyColumn);
		if (UHorizontalBoxSlot* S = Columns->AddChildToHorizontalBox(Sized))
		{
			S->SetVerticalAlignment(VAlign_Bottom);
		}
	}

	// Rows box built and populated *before* the header button so it lands above the button in the
	// column (VerticalBox lays out children in add order); collapsed by default so at rest the
	// column is just its bottom header bar.
	StairRowsBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	if (UVerticalBoxSlot* S = StairColumn->AddChildToVerticalBox(StairRowsBox))
	{
		S->SetHorizontalAlignment(HAlign_Fill);
	}
	AddRow(StairRowsBox, EParam::StepDepth,      TEXT("段の奥行き"), 20.0f, 300.0f, 10.0f,  100.0f,  TEXT("cm"), NextColor());
	AddRow(StairRowsBox, EParam::StepRise,       TEXT("段の高さ"),   10.0f, 200.0f, 5.0f,   83.333f, TEXT("cm"), NextColor());
	AddRow(StairRowsBox, EParam::RiserThickness, TEXT("蹴込み厚"),   1.0f,  40.0f,  1.0f,   10.0f,   TEXT("cm"), NextColor());
	// Restitution/Friction live on the slinky's physical material, but they're really about how the
	// coil grips and bounces off the *stair surface* - grouping them here instead of piling every
	// physics number into the slinky column also keeps that column short enough to stay on screen.
	AddRow(StairRowsBox, EParam::Restitution,    TEXT("反発係数"),   0.0f,  1.0f,   0.02f,  0.08f,   TEXT(""),   NextColor());
	AddRow(StairRowsBox, EParam::Friction,       TEXT("摩擦係数"),   0.0f,  2.0f,   0.05f,  0.48f,   TEXT(""),   NextColor());
	StairRowsBox->SetVisibility(ESlateVisibility::Collapsed);
	StairRowsAnim = NewObject<UPopAnimator>(this);
	StairRowsAnim->Bind(StairRowsBox, 0.0f);
	Animators.Add(StairRowsAnim);
	if (UButton* StairHeaderButton = AddSectionHeader(StairColumn, TEXT("階段"), FLinearColor(0.80f, 0.66f, 0.52f), StairHeaderText))
	{
		StairHeaderButton->OnClicked.AddDynamic(this, &USlinkyControlPanel::OnStairHeaderClicked);
	}

	SlinkyRowsBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	if (UVerticalBoxSlot* S = SlinkyColumn->AddChildToVerticalBox(SlinkyRowsBox))
	{
		S->SetHorizontalAlignment(HAlign_Fill);
	}
	AddRow(SlinkyRowsBox, EParam::CoilTurns,      TEXT("巻き数"),       6.0f,  72.0f,  2.0f,   36.0f,   TEXT("回"), NextColor(), true);
	AddRow(SlinkyRowsBox, EParam::CoilRadius,     TEXT("コイル半径"),   15.0f, 100.0f, 2.5f,   45.0f,   TEXT("cm"), NextColor());
	AddRow(SlinkyRowsBox, EParam::CompactLength,  TEXT("コイル全長"),   20.0f, 400.0f, 10.0f,  180.0f,  TEXT("cm"), NextColor());
	AddRow(SlinkyRowsBox, EParam::MaxNodeSpacing, TEXT("最大伸び"),     10.0f, 300.0f, 5.0f,   60.0f,   TEXT("cm"), NextColor());
	AddRow(SlinkyRowsBox, EParam::WireRadius,     TEXT("線の太さ"),     0.25f, 5.0f,   0.25f,  1.25f,   TEXT(""),   NextColor());
	AddRow(SlinkyRowsBox, EParam::AxialStiffness, TEXT("軸方向の硬さ"), 0.1f,  3.0f,   0.1f,   1.0f,    TEXT("x"),  NextColor());
	AddRow(SlinkyRowsBox, EParam::BendStiffness,  TEXT("曲げの硬さ"),   0.1f,  3.0f,   0.1f,   1.0f,    TEXT("x"),  NextColor());
	AddRow(SlinkyRowsBox, EParam::Damping,        TEXT("減衰"),         0.1f,  3.0f,   0.1f,   1.0f,    TEXT("x"),  NextColor());
	AddMaterialStyleButton(SlinkyRowsBox);
	SlinkyRowsBox->SetVisibility(ESlateVisibility::Collapsed);
	SlinkyRowsAnim = NewObject<UPopAnimator>(this);
	SlinkyRowsAnim->Bind(SlinkyRowsBox, 0.0f);
	Animators.Add(SlinkyRowsAnim);
	if (UButton* SlinkyHeaderButton = AddSectionHeader(SlinkyColumn, TEXT("スリンキー"), FLinearColor(0.72f, 0.78f, 0.90f), SlinkyHeaderText))
	{
		SlinkyHeaderButton->OnClicked.AddDynamic(this, &USlinkyControlPanel::OnSlinkyHeaderClicked);
	}
}

UBorder* USlinkyControlPanel::AddShadowCard(UPanelWidget* Parent, FLinearColor FillColor, float ShadowOffset, float Radius)
{
	UOverlay* Card = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());

	UBorder* ShadowLayer = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	ShadowLayer->SetBrush(FSlateRoundedBoxBrush(PanelShade(FillColor), Radius));
	if (UOverlaySlot* ShadowSlot = Card->AddChildToOverlay(ShadowLayer))
	{
		ShadowSlot->SetHorizontalAlignment(HAlign_Fill);
		ShadowSlot->SetVerticalAlignment(VAlign_Fill);
		ShadowSlot->SetPadding(FMargin(ShadowOffset, ShadowOffset, 0.0f, 0.0f));
	}

	UBorder* Foreground = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Foreground->SetBrush(FSlateRoundedBoxBrush(FillColor, Radius));
	if (UOverlaySlot* FgSlot = Card->AddChildToOverlay(Foreground))
	{
		FgSlot->SetHorizontalAlignment(HAlign_Fill);
		FgSlot->SetVerticalAlignment(VAlign_Fill);
		FgSlot->SetPadding(FMargin(0.0f, 0.0f, ShadowOffset, ShadowOffset));
	}

	if (UVerticalBoxSlot* CardSlot = Cast<UVerticalBoxSlot>(Parent->AddChild(Card)))
	{
		// Every card in a column - every row, and (via AddSectionHeader, which builds its card the
		// same way) the header button too - fills the column's width rather than just its own
		// content's natural width, so the header ends up exactly as wide as the widest row instead
		// of the two drifting to different sizes.
		CardSlot->SetHorizontalAlignment(HAlign_Fill);
		CardSlot->SetPadding(FMargin(0.0f, 0.0f, ShadowOffset, ShadowOffset + 10.0f));
	}

	return Foreground;
}

UButton* USlinkyControlPanel::AddSectionHeader(UPanelWidget* Parent, const FString& Text, FLinearColor Accent, TObjectPtr<UTextBlock>& OutHeaderText)
{
	const float Radius = PanelCardRadius * 0.7f;

	UOverlay* Card = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
	UBorder* Shadow = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Shadow->SetBrush(FSlateRoundedBoxBrush(PanelShade(Accent), Radius));
	if (UOverlaySlot* S = Card->AddChildToOverlay(Shadow))
	{
		S->SetHorizontalAlignment(HAlign_Fill);
		S->SetVerticalAlignment(VAlign_Fill);
		S->SetPadding(FMargin(4.0f, 4.0f, 0.0f, 0.0f));
	}

	UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
	FButtonStyle ButtonStyle = Button->WidgetStyle;
	ButtonStyle.Normal = FSlateRoundedBoxBrush(Accent, Radius);
	ButtonStyle.Hovered = FSlateRoundedBoxBrush(Accent * 1.1f, Radius);
	ButtonStyle.Pressed = FSlateRoundedBoxBrush(Accent * 0.85f, Radius);
	Button->SetStyle(ButtonStyle);

	UTextBlock* HeaderText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	// Plain arrows (U+2191/U+2193), not the geometric-shapes triangles (U+25B8/U+25BE) this used to
	// use - those rendered as mojibake/missing-glyph boxes in a packaged build even though they
	// looked fine in the editor, since the packaged font asset's glyph coverage didn't include
	// them. Up = collapsed (RowsBox starts Collapsed) - ToggleSection flips it to down when expanded.
	HeaderText->SetText(FText::FromString(FString::Printf(TEXT("\x2191 %s \x2191"), *Text)));
	FSlateFontInfo Font = HeaderText->GetFont();
	Font.Size = 15;
	HeaderText->SetFont(Font);
	HeaderText->SetColorAndOpacity(FSlateColor(FLinearColor(0.30f, 0.22f, 0.28f)));
	HeaderText->SetJustification(ETextJustify::Center);
	Button->SetContent(HeaderText);
	if (UOverlaySlot* S = Card->AddChildToOverlay(Button))
	{
		S->SetHorizontalAlignment(HAlign_Fill);
		S->SetVerticalAlignment(VAlign_Fill);
		S->SetPadding(FMargin(0.0f, 0.0f, 4.0f, 4.0f));
	}

	if (UVerticalBoxSlot* CardSlot = Cast<UVerticalBoxSlot>(Parent->AddChild(Card)))
	{
		CardSlot->SetHorizontalAlignment(HAlign_Fill);
		CardSlot->SetPadding(FMargin(0.0f, 0.0f, 4.0f, 0.0f));
	}

	AddButtonPop(Button);
	OutHeaderText = HeaderText;
	return Button;
}

void USlinkyControlPanel::ToggleSection(UVerticalBox* RowsBox, UPopAnimator* RowsAnim, UTextBlock* HeaderText, const FString& Label)
{
	if (!RowsBox || !HeaderText)
	{
		return;
	}
	const bool bWillBeVisible = RowsBox->GetVisibility() == ESlateVisibility::Collapsed;
	if (bWillBeVisible)
	{
		// Visible *now*, at (almost) zero scale/opacity, so the column already occupies its full
		// expanded layout height and the rows just bloom into that space rather than the whole
		// panel resizing gradually underneath a still-growing animation.
		RowsBox->SetVisibility(ESlateVisibility::Visible);
	}
	if (RowsAnim)
	{
		// Collapsing only *targets* zero - UPopAnimator::Tick is what actually sets Collapsed,
		// once the shrink has visibly finished, instead of the rows vanishing instantly.
		RowsAnim->SetTarget(bWillBeVisible ? 1.0f : 0.0f, !bWillBeVisible);
	}
	const TCHAR* Arrow = bWillBeVisible ? TEXT("\x2193") : TEXT("\x2191");
	HeaderText->SetText(FText::FromString(FString::Printf(TEXT("%s %s %s"), Arrow, *Label, Arrow)));
}

UButton* USlinkyControlPanel::AddMaterialStyleButton(UPanelWidget* Parent)
{
	const float Radius = PanelCardRadius * 0.7f;
	const FLinearColor Accent(0.88f, 0.82f, 0.60f);

	UOverlay* Card = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
	UBorder* Shadow = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Shadow->SetBrush(FSlateRoundedBoxBrush(PanelShade(Accent), Radius));
	if (UOverlaySlot* S = Card->AddChildToOverlay(Shadow))
	{
		S->SetHorizontalAlignment(HAlign_Fill);
		S->SetVerticalAlignment(VAlign_Fill);
		S->SetPadding(FMargin(4.0f, 4.0f, 0.0f, 0.0f));
	}

	UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
	FButtonStyle ButtonStyle = Button->WidgetStyle;
	ButtonStyle.Normal = FSlateRoundedBoxBrush(Accent, Radius);
	ButtonStyle.Hovered = FSlateRoundedBoxBrush(Accent * 1.1f, Radius);
	ButtonStyle.Pressed = FSlateRoundedBoxBrush(Accent * 0.85f, Radius);
	Button->SetStyle(ButtonStyle);

	MaterialStyleButtonText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	MaterialStyleButtonText->SetText(FText::FromString(TEXT("見た目: メタル")));
	FSlateFontInfo Font = MaterialStyleButtonText->GetFont();
	Font.Size = 14;
	MaterialStyleButtonText->SetFont(Font);
	MaterialStyleButtonText->SetColorAndOpacity(FSlateColor(FLinearColor(0.30f, 0.24f, 0.12f)));
	MaterialStyleButtonText->SetJustification(ETextJustify::Center);
	Button->SetContent(MaterialStyleButtonText);
	if (UOverlaySlot* S = Card->AddChildToOverlay(Button))
	{
		S->SetHorizontalAlignment(HAlign_Fill);
		S->SetVerticalAlignment(VAlign_Fill);
		S->SetPadding(FMargin(0.0f, 0.0f, 4.0f, 4.0f));
	}

	if (UVerticalBoxSlot* CardSlot = Cast<UVerticalBoxSlot>(Parent->AddChild(Card)))
	{
		CardSlot->SetHorizontalAlignment(HAlign_Fill);
		CardSlot->SetPadding(FMargin(0.0f, 6.0f, 0.0f, 0.0f));
	}

	AddButtonPop(Button);
	Button->OnClicked.AddDynamic(this, &USlinkyControlPanel::OnMaterialStyleClicked);
	return Button;
}

void USlinkyControlPanel::OnMaterialStyleClicked()
{
	if (!Slinky || !MaterialStyleButtonText)
	{
		return;
	}

	const bool bToPlastic = Slinky->GetCoilMaterialStyle() == ASlinkyActor::ECoilMaterialStyle::Metal;
	Slinky->SetCoilMaterialStyle(bToPlastic
		? ASlinkyActor::ECoilMaterialStyle::PlasticRainbow
		: ASlinkyActor::ECoilMaterialStyle::Metal);
	MaterialStyleButtonText->SetText(FText::FromString(bToPlastic ? TEXT("見た目: 虹色プラスチック") : TEXT("見た目: メタル")));
}

void USlinkyControlPanel::OnStairHeaderClicked()
{
	ToggleSection(StairRowsBox, StairRowsAnim, StairHeaderText, TEXT("階段"));
}

void USlinkyControlPanel::OnSlinkyHeaderClicked()
{
	ToggleSection(SlinkyRowsBox, SlinkyRowsAnim, SlinkyHeaderText, TEXT("スリンキー"));
}

void USlinkyControlPanel::AddRow(UPanelWidget* Parent, EParam Param, const FString& Label, float Min, float Max,
	float Step, float InitialValue, const TCHAR* Suffix, FLinearColor Accent, bool bInteger)
{
	const int32 Index = (int32)Param;
	MinValues[Index] = Min;
	MaxValues[Index] = Max;
	Steps[Index] = Step;
	Values[Index] = InitialValue;
	Suffixes[Index] = Suffix;
	IsInteger[Index] = bInteger;

	const FLinearColor Ink(0.32f, 0.24f, 0.30f);

	UBorder* Foreground = AddShadowCard(Parent, Accent, 6.0f, PanelCardRadius);
	Foreground->SetPadding(FMargin(16.0f, 10.0f));

	UVerticalBox* RowBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Foreground->SetContent(RowBox);

	// Label + live value readout.
	UHorizontalBox* HeaderRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	if (UVerticalBoxSlot* HeaderSlot = RowBox->AddChildToVerticalBox(HeaderRow))
	{
		HeaderSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 8.0f));
	}

	UTextBlock* LabelText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	LabelText->SetText(FText::FromString(Label));
	{
		FSlateFontInfo Font = LabelText->GetFont();
		Font.Size = 16;
		LabelText->SetFont(Font);
	}
	LabelText->SetColorAndOpacity(FSlateColor(Ink));
	if (UHorizontalBoxSlot* LabelSlot = HeaderRow->AddChildToHorizontalBox(LabelText))
	{
		LabelSlot->SetHorizontalAlignment(HAlign_Left);
		LabelSlot->SetVerticalAlignment(VAlign_Center);
		LabelSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}

	UTextBlock* ValueText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	{
		FSlateFontInfo Font = ValueText->GetFont();
		Font.Size = 17;
		ValueText->SetFont(Font);
	}
	ValueText->SetColorAndOpacity(FSlateColor(Ink));
	if (UHorizontalBoxSlot* ValueSlot = HeaderRow->AddChildToHorizontalBox(ValueText))
	{
		ValueSlot->SetHorizontalAlignment(HAlign_Right);
		ValueSlot->SetVerticalAlignment(VAlign_Center);
	}
	ValueTexts[Index] = ValueText;

	// The "-"/"+" pair, each its own small rounded 3D shadow-button, split evenly across the row.
	UHorizontalBox* ButtonRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	RowBox->AddChildToVerticalBox(ButtonRow);

	const FLinearColor ButtonColor = FLinearColor(1.0f, 1.0f, 1.0f, 1.0f) * 0.4f + Accent * 0.6f;
	UButton* MinusButton = nullptr;
	UButton* PlusButton = nullptr;

	for (int32 Side = 0; Side < 2; ++Side)
	{
		UOverlay* ButtonCard = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
		UBorder* BtnShadow = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		BtnShadow->SetBrush(FSlateRoundedBoxBrush(PanelShade(ButtonColor), PanelButtonRadius));
		if (UOverlaySlot* S = ButtonCard->AddChildToOverlay(BtnShadow))
		{
			S->SetHorizontalAlignment(HAlign_Fill);
			S->SetVerticalAlignment(VAlign_Fill);
			S->SetPadding(FMargin(4.0f, 4.0f, 0.0f, 0.0f));
		}

		UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
		FButtonStyle ButtonStyle = Button->WidgetStyle;
		ButtonStyle.Normal = FSlateRoundedBoxBrush(ButtonColor, PanelButtonRadius);
		ButtonStyle.Hovered = FSlateRoundedBoxBrush(ButtonColor * 1.12f, PanelButtonRadius);
		ButtonStyle.Pressed = FSlateRoundedBoxBrush(ButtonColor * 0.85f, PanelButtonRadius);
		Button->SetStyle(ButtonStyle);

		UTextBlock* Glyph = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		Glyph->SetText(FText::FromString(Side == 0 ? TEXT("\x2212") : TEXT("+")));
		{
			FSlateFontInfo Font = Glyph->GetFont();
			Font.Size = 20;
			Glyph->SetFont(Font);
		}
		Glyph->SetColorAndOpacity(FSlateColor(Ink));
		Glyph->SetJustification(ETextJustify::Center);
		Button->SetContent(Glyph);

		if (UOverlaySlot* S = ButtonCard->AddChildToOverlay(Button))
		{
			S->SetHorizontalAlignment(HAlign_Fill);
			S->SetVerticalAlignment(VAlign_Fill);
			S->SetPadding(FMargin(0.0f, 0.0f, 4.0f, 4.0f));
		}

		if (UHorizontalBoxSlot* ButtonSlot = ButtonRow->AddChildToHorizontalBox(ButtonCard))
		{
			ButtonSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			ButtonSlot->SetPadding(Side == 0 ? FMargin(0.0f, 0.0f, 5.0f, 0.0f) : FMargin(5.0f, 0.0f, 0.0f, 0.0f));
			ButtonSlot->SetVerticalAlignment(VAlign_Fill);
		}

		AddButtonPop(Button);
		if (Side == 0) { MinusButton = Button; } else { PlusButton = Button; }
	}

	switch (Param)
	{
	case EParam::StepDepth:
		MinusButton->OnClicked.AddDynamic(this, &USlinkyControlPanel::OnStepDepthMinus);
		PlusButton->OnClicked.AddDynamic(this, &USlinkyControlPanel::OnStepDepthPlus);
		break;
	case EParam::StepRise:
		MinusButton->OnClicked.AddDynamic(this, &USlinkyControlPanel::OnStepRiseMinus);
		PlusButton->OnClicked.AddDynamic(this, &USlinkyControlPanel::OnStepRisePlus);
		break;
	case EParam::RiserThickness:
		MinusButton->OnClicked.AddDynamic(this, &USlinkyControlPanel::OnRiserThicknessMinus);
		PlusButton->OnClicked.AddDynamic(this, &USlinkyControlPanel::OnRiserThicknessPlus);
		break;
	case EParam::CompactLength:
		MinusButton->OnClicked.AddDynamic(this, &USlinkyControlPanel::OnCompactLengthMinus);
		PlusButton->OnClicked.AddDynamic(this, &USlinkyControlPanel::OnCompactLengthPlus);
		break;
	case EParam::MaxNodeSpacing:
		MinusButton->OnClicked.AddDynamic(this, &USlinkyControlPanel::OnMaxNodeSpacingMinus);
		PlusButton->OnClicked.AddDynamic(this, &USlinkyControlPanel::OnMaxNodeSpacingPlus);
		break;
	case EParam::WireRadius:
		MinusButton->OnClicked.AddDynamic(this, &USlinkyControlPanel::OnWireRadiusMinus);
		PlusButton->OnClicked.AddDynamic(this, &USlinkyControlPanel::OnWireRadiusPlus);
		break;
	case EParam::AxialStiffness:
		MinusButton->OnClicked.AddDynamic(this, &USlinkyControlPanel::OnAxialStiffnessMinus);
		PlusButton->OnClicked.AddDynamic(this, &USlinkyControlPanel::OnAxialStiffnessPlus);
		break;
	case EParam::BendStiffness:
		MinusButton->OnClicked.AddDynamic(this, &USlinkyControlPanel::OnBendStiffnessMinus);
		PlusButton->OnClicked.AddDynamic(this, &USlinkyControlPanel::OnBendStiffnessPlus);
		break;
	case EParam::Damping:
		MinusButton->OnClicked.AddDynamic(this, &USlinkyControlPanel::OnDampingMinus);
		PlusButton->OnClicked.AddDynamic(this, &USlinkyControlPanel::OnDampingPlus);
		break;
	case EParam::Restitution:
		MinusButton->OnClicked.AddDynamic(this, &USlinkyControlPanel::OnRestitutionMinus);
		PlusButton->OnClicked.AddDynamic(this, &USlinkyControlPanel::OnRestitutionPlus);
		break;
	case EParam::Friction:
		MinusButton->OnClicked.AddDynamic(this, &USlinkyControlPanel::OnFrictionMinus);
		PlusButton->OnClicked.AddDynamic(this, &USlinkyControlPanel::OnFrictionPlus);
		break;
	case EParam::CoilTurns:
		MinusButton->OnClicked.AddDynamic(this, &USlinkyControlPanel::OnCoilTurnsMinus);
		PlusButton->OnClicked.AddDynamic(this, &USlinkyControlPanel::OnCoilTurnsPlus);
		break;
	case EParam::CoilRadius:
		MinusButton->OnClicked.AddDynamic(this, &USlinkyControlPanel::OnCoilRadiusMinus);
		PlusButton->OnClicked.AddDynamic(this, &USlinkyControlPanel::OnCoilRadiusPlus);
		break;
	default:
		break;
	}

	RefreshValueText(Param, InitialValue);
}

void USlinkyControlPanel::RefreshValueText(EParam Param, float Value)
{
	const int32 Index = (int32)Param;
	if (!ValueTexts.IsValidIndex(Index) || !ValueTexts[Index])
	{
		return;
	}
	// FString::Printf's format string must be a compile-time literal (UE5's format-string checker
	// validates placeholders against argument types at compile time), so this can't pick between
	// "%.0f%s" and "%.2f%s" via a runtime TCHAR* - hence the branch instead of a shared call.
	if (IsInteger.IsValidIndex(Index) && IsInteger[Index])
	{
		ValueTexts[Index]->SetText(FText::FromString(FString::Printf(TEXT("%.0f%s"), Value, Suffixes[Index])));
	}
	else
	{
		ValueTexts[Index]->SetText(FText::FromString(FString::Printf(TEXT("%.2f%s"), Value, Suffixes[Index])));
	}
}

void USlinkyControlPanel::AdjustValue(EParam Param, float Sign)
{
	const int32 Index = (int32)Param;
	if (!Values.IsValidIndex(Index))
	{
		return;
	}
	const float NewValue = FMath::Clamp(Values[Index] + Sign * Steps[Index], MinValues[Index], MaxValues[Index]);
	Values[Index] = NewValue;
	ApplyValue(Param, NewValue);
	RefreshValueText(Param, NewValue);
}

void USlinkyControlPanel::SetTargets(ASlinkyStaircase* InStaircase, ASlinkyActor* InSlinky)
{
	Staircase = InStaircase;
	Slinky = InSlinky;
	if (!Staircase || !Slinky)
	{
		return;
	}

	const auto Push = [this](EParam Param, float Value)
	{
		const int32 Index = (int32)Param;
		if (Values.IsValidIndex(Index))
		{
			Values[Index] = Value;
		}
		RefreshValueText(Param, Value);
	};

	Push(EParam::StepDepth, Staircase->StepDepth);
	Push(EParam::StepRise, Staircase->StepRise);
	Push(EParam::RiserThickness, Staircase->RiserThickness);
	Push(EParam::CoilTurns, (float)Slinky->CoilTurns);
	Push(EParam::CoilRadius, Slinky->CoilRadius);
	Push(EParam::CompactLength, Slinky->CompactLength);
	Push(EParam::MaxNodeSpacing, Slinky->MaximumNodeSpacing);
	Push(EParam::WireRadius, Slinky->WireRadius);
	Push(EParam::AxialStiffness, Slinky->AxialStiffnessScale);
	Push(EParam::BendStiffness, Slinky->BendStiffnessScale);
	Push(EParam::Damping, Slinky->DampingScale);
	Push(EParam::Restitution, Slinky->Restitution);
	Push(EParam::Friction, Slinky->Friction);
}

void USlinkyControlPanel::ApplyValue(EParam Param, float Value)
{
	switch (Param)
	{
	case EParam::CoilTurns:
		if (Slinky) { Slinky->CoilTurns = FMath::RoundToInt(Value); Slinky->RebuildHelixSegments(); }
		break;
	case EParam::CoilRadius:
		if (Slinky)
		{
			Slinky->CoilRadius = Value;
			Slinky->RefreshNodeScale();
			// The stairs' own width tracks CoilRadius (see ASlinkyStaircase::GetCoilRadius), so a
			// wider/narrower coil needs the treads and risers re-slid to match.
			if (Staircase) { Staircase->RefreshLayout(); }
		}
		break;
	case EParam::StepDepth:
		if (Staircase) { Staircase->SetStepDepth(Value); }
		break;
	case EParam::StepRise:
		if (Staircase) { Staircase->SetStepRise(Value); }
		break;
	case EParam::RiserThickness:
		if (Staircase) { Staircase->SetRiserThickness(Value); }
		break;
	case EParam::CompactLength:
		if (Slinky) { Slinky->CompactLength = Value; Slinky->RefreshCompactLength(); }
		break;
	case EParam::MaxNodeSpacing:
		if (Slinky) { Slinky->MaximumNodeSpacing = FMath::Max(Value, Slinky->GetNodeRestSpacing() + 1.0f); Slinky->RefreshCoilTuning(); }
		break;
	case EParam::WireRadius:
		if (Slinky) { Slinky->WireRadius = Value; Slinky->RefreshNodeScale(); }
		break;
	case EParam::AxialStiffness:
		if (Slinky) { Slinky->AxialStiffnessScale = Value; Slinky->RefreshCoilTuning(); }
		break;
	case EParam::BendStiffness:
		if (Slinky) { Slinky->BendStiffnessScale = Value; }
		break;
	case EParam::Damping:
		if (Slinky) { Slinky->DampingScale = Value; Slinky->RefreshCoilTuning(); }
		break;
	case EParam::Restitution:
		if (Slinky) { Slinky->Restitution = Value; Slinky->RefreshCoilTuning(); }
		break;
	case EParam::Friction:
		if (Slinky) { Slinky->Friction = Value; Slinky->RefreshCoilTuning(); }
		break;
	default:
		break;
	}
}

void USlinkyControlPanel::OnStepDepthMinus() { AdjustValue(EParam::StepDepth, -1.0f); }
void USlinkyControlPanel::OnStepDepthPlus() { AdjustValue(EParam::StepDepth, 1.0f); }
void USlinkyControlPanel::OnStepRiseMinus() { AdjustValue(EParam::StepRise, -1.0f); }
void USlinkyControlPanel::OnStepRisePlus() { AdjustValue(EParam::StepRise, 1.0f); }
void USlinkyControlPanel::OnRiserThicknessMinus() { AdjustValue(EParam::RiserThickness, -1.0f); }
void USlinkyControlPanel::OnRiserThicknessPlus() { AdjustValue(EParam::RiserThickness, 1.0f); }
void USlinkyControlPanel::OnCompactLengthMinus() { AdjustValue(EParam::CompactLength, -1.0f); }
void USlinkyControlPanel::OnCompactLengthPlus() { AdjustValue(EParam::CompactLength, 1.0f); }
void USlinkyControlPanel::OnMaxNodeSpacingMinus() { AdjustValue(EParam::MaxNodeSpacing, -1.0f); }
void USlinkyControlPanel::OnMaxNodeSpacingPlus() { AdjustValue(EParam::MaxNodeSpacing, 1.0f); }
void USlinkyControlPanel::OnWireRadiusMinus() { AdjustValue(EParam::WireRadius, -1.0f); }
void USlinkyControlPanel::OnWireRadiusPlus() { AdjustValue(EParam::WireRadius, 1.0f); }
void USlinkyControlPanel::OnAxialStiffnessMinus() { AdjustValue(EParam::AxialStiffness, -1.0f); }
void USlinkyControlPanel::OnAxialStiffnessPlus() { AdjustValue(EParam::AxialStiffness, 1.0f); }
void USlinkyControlPanel::OnBendStiffnessMinus() { AdjustValue(EParam::BendStiffness, -1.0f); }
void USlinkyControlPanel::OnBendStiffnessPlus() { AdjustValue(EParam::BendStiffness, 1.0f); }
void USlinkyControlPanel::OnDampingMinus() { AdjustValue(EParam::Damping, -1.0f); }
void USlinkyControlPanel::OnDampingPlus() { AdjustValue(EParam::Damping, 1.0f); }
void USlinkyControlPanel::OnRestitutionMinus() { AdjustValue(EParam::Restitution, -1.0f); }
void USlinkyControlPanel::OnRestitutionPlus() { AdjustValue(EParam::Restitution, 1.0f); }
void USlinkyControlPanel::OnFrictionMinus() { AdjustValue(EParam::Friction, -1.0f); }
void USlinkyControlPanel::OnFrictionPlus() { AdjustValue(EParam::Friction, 1.0f); }
void USlinkyControlPanel::OnCoilTurnsMinus() { AdjustValue(EParam::CoilTurns, -1.0f); }
void USlinkyControlPanel::OnCoilTurnsPlus() { AdjustValue(EParam::CoilTurns, 1.0f); }
void USlinkyControlPanel::OnCoilRadiusMinus() { AdjustValue(EParam::CoilRadius, -1.0f); }
void USlinkyControlPanel::OnCoilRadiusPlus() { AdjustValue(EParam::CoilRadius, 1.0f); }
