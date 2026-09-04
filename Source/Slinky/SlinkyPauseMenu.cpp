#include "SlinkyPauseMenu.h"
#include "SlinkyActor.h"
#include "SlinkyGameInstance.h"
#include "SlinkyGameMode.h"
#include "SlinkyPlayerController.h"
#include "SlinkyPopAnimator.h"
#include "SlinkyStaircase.h"
#include "Blueprint/WidgetTree.h"
#include "Brushes/SlateColorBrush.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/CheckBox.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/Slider.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

namespace
{
	constexpr float ButtonRadius = 14.0f;
	constexpr float ButtonShadowOffset = 5.0f;
	constexpr float CardWidth = 420.0f;

	FLinearColor Shade(const FLinearColor& Color)
	{
		return FLinearColor(Color.R * 0.55f, Color.G * 0.48f, Color.B * 0.58f, 1.0f);
	}
}

void USlinkyPauseMenu::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
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

UPopAnimator* USlinkyPauseMenu::AddButtonPop(UButton* Button)
{
	UPopAnimator* Anim = NewObject<UPopAnimator>(this);
	Anim->Bind(Button);
	Anim->BindButtonEvents(Button);
	Animators.Add(Anim);
	return Anim;
}

UButton* USlinkyPauseMenu::AddMenuButton(UPanelWidget* Parent, const FString& Label, FLinearColor Accent)
{
	UOverlay* Card = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());

	UBorder* ShadowLayer = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	ShadowLayer->SetBrush(FSlateRoundedBoxBrush(Shade(Accent), ButtonRadius));
	if (UOverlaySlot* S = Card->AddChildToOverlay(ShadowLayer))
	{
		S->SetHorizontalAlignment(HAlign_Fill);
		S->SetVerticalAlignment(VAlign_Fill);
		S->SetPadding(FMargin(ButtonShadowOffset, ButtonShadowOffset, 0.0f, 0.0f));
	}

	UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
	FButtonStyle ButtonStyle = Button->WidgetStyle;
	ButtonStyle.Normal = FSlateRoundedBoxBrush(Accent, ButtonRadius);
	ButtonStyle.Hovered = FSlateRoundedBoxBrush(Accent * 1.08f, ButtonRadius);
	ButtonStyle.Pressed = FSlateRoundedBoxBrush(Accent * 0.85f, ButtonRadius);
	Button->SetStyle(ButtonStyle);

	UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	Text->SetText(FText::FromString(Label));
	{
		FSlateFontInfo Font = Text->GetFont();
		Font.Size = 20;
		Text->SetFont(Font);
	}
	Text->SetColorAndOpacity(FSlateColor(FLinearColor(0.98f, 0.98f, 0.98f)));
	Text->SetJustification(ETextJustify::Center);
	Button->SetContent(Text);

	if (UOverlaySlot* S = Card->AddChildToOverlay(Button))
	{
		S->SetHorizontalAlignment(HAlign_Fill);
		S->SetVerticalAlignment(VAlign_Fill);
		S->SetPadding(FMargin(0.0f, 0.0f, ButtonShadowOffset, ButtonShadowOffset));
	}

	USizeBox* Sized = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	Sized->SetWidthOverride(CardWidth - 48.0f);
	Sized->SetHeightOverride(56.0f);
	Sized->SetContent(Card);

	if (UVerticalBoxSlot* CardSlot = Cast<UVerticalBoxSlot>(Parent->AddChild(Sized)))
	{
		CardSlot->SetHorizontalAlignment(HAlign_Center);
		CardSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 14.0f));
	}

	AddButtonPop(Button);
	return Button;
}

UCheckBox* USlinkyPauseMenu::AddToggleRow(UPanelWidget* Parent, const FString& Label, FLinearColor Accent, bool bInitiallyChecked)
{
	UOverlay* Card = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());

	UBorder* ShadowLayer = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	ShadowLayer->SetBrush(FSlateRoundedBoxBrush(Shade(Accent), ButtonRadius * 0.7f));
	if (UOverlaySlot* S = Card->AddChildToOverlay(ShadowLayer))
	{
		S->SetHorizontalAlignment(HAlign_Fill);
		S->SetVerticalAlignment(VAlign_Fill);
		S->SetPadding(FMargin(4.0f, 4.0f, 0.0f, 0.0f));
	}

	UBorder* Foreground = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Foreground->SetBrush(FSlateRoundedBoxBrush(Accent, ButtonRadius * 0.7f));
	Foreground->SetPadding(FMargin(16.0f, 10.0f));
	if (UOverlaySlot* S = Card->AddChildToOverlay(Foreground))
	{
		S->SetHorizontalAlignment(HAlign_Fill);
		S->SetVerticalAlignment(VAlign_Fill);
		S->SetPadding(FMargin(0.0f, 0.0f, 4.0f, 4.0f));
	}

	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	Foreground->SetContent(Row);

	UTextBlock* LabelText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	LabelText->SetText(FText::FromString(Label));
	{
		FSlateFontInfo Font = LabelText->GetFont();
		Font.Size = 16;
		LabelText->SetFont(Font);
	}
	LabelText->SetColorAndOpacity(FSlateColor(FLinearColor(0.30f, 0.22f, 0.28f)));
	if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(LabelText))
	{
		S->SetVerticalAlignment(VAlign_Center);
		S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}

	UCheckBox* CheckBox = WidgetTree->ConstructWidget<UCheckBox>(UCheckBox::StaticClass());
	CheckBox->SetIsChecked(bInitiallyChecked);
	if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(CheckBox))
	{
		S->SetVerticalAlignment(VAlign_Center);
		S->SetHorizontalAlignment(HAlign_Right);
	}

	USizeBox* Sized = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	Sized->SetWidthOverride(CardWidth - 48.0f);
	Sized->SetContent(Card);

	if (UVerticalBoxSlot* CardSlot = Cast<UVerticalBoxSlot>(Parent->AddChild(Sized)))
	{
		CardSlot->SetHorizontalAlignment(HAlign_Center);
		CardSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 12.0f));
	}

	return CheckBox;
}

void USlinkyPauseMenu::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Root"));
	WidgetTree->RootWidget = Root;

	// Always-visible corner icon - the one way into the menu besides Escape (see
	// ASlinkyPlayerController::TogglePauseMenu).
	{
		UOverlay* IconCard = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
		UBorder* IconShadow = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		IconShadow->SetBrush(FSlateRoundedBoxBrush(FLinearColor(0.45f, 0.40f, 0.50f), 12.0f));
		if (UOverlaySlot* S = IconCard->AddChildToOverlay(IconShadow))
		{
			S->SetHorizontalAlignment(HAlign_Fill);
			S->SetVerticalAlignment(VAlign_Fill);
			S->SetPadding(FMargin(4.0f, 4.0f, 0.0f, 0.0f));
		}

		UButton* IconButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
		FButtonStyle IconStyle = IconButton->WidgetStyle;
		IconStyle.Normal = FSlateRoundedBoxBrush(FLinearColor(0.97f, 0.95f, 0.91f), 12.0f);
		IconStyle.Hovered = FSlateRoundedBoxBrush(FLinearColor(1.0f, 0.98f, 0.94f), 12.0f);
		IconStyle.Pressed = FSlateRoundedBoxBrush(FLinearColor(0.85f, 0.83f, 0.80f), 12.0f);
		IconButton->SetStyle(IconStyle);
		IconButton->OnClicked.AddDynamic(this, &USlinkyPauseMenu::OnPauseIconClicked);

		UTextBlock* IconGlyph = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		IconGlyph->SetText(FText::FromString(TEXT("II")));
		{
			FSlateFontInfo Font = IconGlyph->GetFont();
			Font.Size = 18;
			IconGlyph->SetFont(Font);
		}
		IconGlyph->SetColorAndOpacity(FSlateColor(FLinearColor(0.30f, 0.22f, 0.28f)));
		IconGlyph->SetJustification(ETextJustify::Center);
		IconButton->SetContent(IconGlyph);

		if (UOverlaySlot* S = IconCard->AddChildToOverlay(IconButton))
		{
			S->SetHorizontalAlignment(HAlign_Fill);
			S->SetVerticalAlignment(VAlign_Fill);
			S->SetPadding(FMargin(0.0f, 0.0f, 4.0f, 4.0f));
		}

		USizeBox* IconSized = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		IconSized->SetWidthOverride(48.0f);
		IconSized->SetHeightOverride(48.0f);
		IconSized->SetContent(IconCard);

		if (UCanvasPanelSlot* S = Root->AddChildToCanvas(IconSized))
		{
			S->SetAnchors(FAnchors(0.0f, 0.0f));
			S->SetPosition(FVector2D(24.0f, 24.0f));
			S->SetAutoSize(true);
		}

		AddButtonPop(IconButton);
	}

	// Dim backdrop + centered card - both start Collapsed, see SetMenuOpen().
	UBorder* Dim = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Dim->SetBrush(FSlateColorBrush(FLinearColor(0.0f, 0.0f, 0.0f, 0.55f)));
	Dim->SetVisibility(ESlateVisibility::Collapsed);
	if (UCanvasPanelSlot* S = Root->AddChildToCanvas(Dim))
	{
		S->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
		S->SetOffsets(FMargin(0.0f));
	}
	DimBackground = Dim;

	UOverlay* MenuCardOverlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
	MenuCardOverlay->SetVisibility(ESlateVisibility::Collapsed);
	if (UCanvasPanelSlot* S = Root->AddChildToCanvas(MenuCardOverlay))
	{
		S->SetAnchors(FAnchors(0.5f, 0.5f));
		S->SetAlignment(FVector2D(0.5f, 0.5f));
		S->SetAutoSize(true);
	}
	MenuCard = MenuCardOverlay;

	const FLinearColor CardFill(0.97f, 0.95f, 0.91f);
	UBorder* CardShadow = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	CardShadow->SetBrush(FSlateRoundedBoxBrush(Shade(CardFill), 20.0f));
	if (UOverlaySlot* S = MenuCardOverlay->AddChildToOverlay(CardShadow))
	{
		S->SetHorizontalAlignment(HAlign_Fill);
		S->SetVerticalAlignment(VAlign_Fill);
		S->SetPadding(FMargin(8.0f, 8.0f, 0.0f, 0.0f));
	}

	UBorder* CardFace = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	CardFace->SetBrush(FSlateRoundedBoxBrush(CardFill, 20.0f));
	CardFace->SetPadding(FMargin(32.0f, 28.0f));
	if (UOverlaySlot* S = MenuCardOverlay->AddChildToOverlay(CardFace))
	{
		S->SetHorizontalAlignment(HAlign_Fill);
		S->SetVerticalAlignment(VAlign_Fill);
		S->SetPadding(FMargin(0.0f, 0.0f, 8.0f, 8.0f));
	}

	UVerticalBox* Content = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	CardFace->SetContent(Content);

	UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	Title->SetText(FText::FromString(TEXT("ポーズ")));
	{
		FSlateFontInfo Font = Title->GetFont();
		Font.Size = 28;
		Title->SetFont(Font);
	}
	Title->SetColorAndOpacity(FSlateColor(FLinearColor(0.30f, 0.22f, 0.28f)));
	Title->SetJustification(ETextJustify::Center);
	if (UVerticalBoxSlot* S = Content->AddChildToVerticalBox(Title))
	{
		S->SetHorizontalAlignment(HAlign_Center);
		S->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 20.0f));
	}

	USlinkyGameInstance* GameInstance = GetWorld() ? Cast<USlinkyGameInstance>(GetWorld()->GetGameInstance()) : nullptr;

	// Volume slider.
	{
		UOverlay* Card = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
		UBorder* Shadow = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Shadow->SetBrush(FSlateRoundedBoxBrush(Shade(FLinearColor(0.80f, 0.90f, 0.98f)), ButtonRadius * 0.7f));
		if (UOverlaySlot* S = Card->AddChildToOverlay(Shadow))
		{
			S->SetHorizontalAlignment(HAlign_Fill);
			S->SetVerticalAlignment(VAlign_Fill);
			S->SetPadding(FMargin(4.0f, 4.0f, 0.0f, 0.0f));
		}
		UBorder* Foreground = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Foreground->SetBrush(FSlateRoundedBoxBrush(FLinearColor(0.80f, 0.90f, 0.98f), ButtonRadius * 0.7f));
		Foreground->SetPadding(FMargin(16.0f, 10.0f));
		if (UOverlaySlot* S = Card->AddChildToOverlay(Foreground))
		{
			S->SetHorizontalAlignment(HAlign_Fill);
			S->SetVerticalAlignment(VAlign_Fill);
			S->SetPadding(FMargin(0.0f, 0.0f, 4.0f, 4.0f));
		}

		UVerticalBox* VolumeBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		Foreground->SetContent(VolumeBox);

		UTextBlock* VolumeLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		VolumeLabel->SetText(FText::FromString(TEXT("音量")));
		{
			FSlateFontInfo Font = VolumeLabel->GetFont();
			Font.Size = 16;
			VolumeLabel->SetFont(Font);
		}
		VolumeLabel->SetColorAndOpacity(FSlateColor(FLinearColor(0.30f, 0.22f, 0.28f)));
		if (UVerticalBoxSlot* S = VolumeBox->AddChildToVerticalBox(VolumeLabel))
		{
			S->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 6.0f));
		}

		USlider* VolumeSlider = WidgetTree->ConstructWidget<USlider>(USlider::StaticClass());
		VolumeSlider->SetMinValue(0.0f);
		VolumeSlider->SetMaxValue(1.0f);
		VolumeSlider->SetValue(GameInstance ? GameInstance->GetMasterVolume() : 1.0f);
		VolumeSlider->OnValueChanged.AddDynamic(this, &USlinkyPauseMenu::OnVolumeChanged);
		VolumeBox->AddChildToVerticalBox(VolumeSlider);

		USizeBox* Sized = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		Sized->SetWidthOverride(CardWidth - 48.0f);
		Sized->SetContent(Card);
		if (UVerticalBoxSlot* CardSlot = Cast<UVerticalBoxSlot>(Content->AddChild(Sized)))
		{
			CardSlot->SetHorizontalAlignment(HAlign_Center);
			CardSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 12.0f));
		}
	}

	UCheckBox* EffectsCheck = AddToggleRow(Content, TEXT("エフェクト"), FLinearColor(0.87f, 0.83f, 0.97f),
		GameInstance ? GameInstance->AreEffectsEnabled() : true);
	EffectsCheck->OnCheckStateChanged.AddDynamic(this, &USlinkyPauseMenu::OnEffectsToggled);

	UCheckBox* ComboDisplayCheck = AddToggleRow(Content, TEXT("コンボ表示"), FLinearColor(0.78f, 0.92f, 0.85f),
		GameInstance ? GameInstance->IsComboDisplayEnabled() : true);
	ComboDisplayCheck->OnCheckStateChanged.AddDynamic(this, &USlinkyPauseMenu::OnComboDisplayToggled);

	{
		USizeBox* Spacer = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		Spacer->SetHeightOverride(8.0f);
		Content->AddChildToVerticalBox(Spacer);
	}

	AddMenuButton(Content, TEXT("つづける"), FLinearColor(0.35f, 0.62f, 0.42f))
		->OnClicked.AddDynamic(this, &USlinkyPauseMenu::OnResumeClicked);
	AddMenuButton(Content, TEXT("リスポーン"), FLinearColor(0.25f, 0.35f, 0.62f))
		->OnClicked.AddDynamic(this, &USlinkyPauseMenu::OnRespawnClicked);
	AddMenuButton(Content, TEXT("デフォルトに戻す"), FLinearColor(0.60f, 0.55f, 0.35f))
		->OnClicked.AddDynamic(this, &USlinkyPauseMenu::OnResetDefaultsClicked);
	AddMenuButton(Content, TEXT("セーブしてやめる"), FLinearColor(0.70f, 0.30f, 0.28f))
		->OnClicked.AddDynamic(this, &USlinkyPauseMenu::OnSaveAndQuitClicked);
}

void USlinkyPauseMenu::TogglePause()
{
	SetMenuOpen(!bMenuOpen);
}

void USlinkyPauseMenu::SetMenuOpen(bool bOpen)
{
	bMenuOpen = bOpen;
	if (DimBackground)
	{
		DimBackground->SetVisibility(bOpen ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	if (MenuCard)
	{
		MenuCard->SetVisibility(bOpen ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}

	// A drag held into the pause menu would otherwise keep updating (or get stuck) once gameplay
	// resumes, since the mouse-up that would normally end it can land on the menu's UI instead.
	if (bOpen)
	{
		if (ASlinkyActor* Slinky = FindSlinky())
		{
			if (Slinky->IsDragging())
			{
				Slinky->EndDrag();
			}
		}
	}

	UGameplayStatics::SetGamePaused(GetWorld(), bOpen);
}

ASlinkyActor* USlinkyPauseMenu::FindSlinky() const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}
	for (TActorIterator<ASlinkyActor> It(World); It; ++It)
	{
		return *It;
	}
	return nullptr;
}

ASlinkyStaircase* USlinkyPauseMenu::FindStaircase() const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}
	for (TActorIterator<ASlinkyStaircase> It(World); It; ++It)
	{
		return *It;
	}
	return nullptr;
}

void USlinkyPauseMenu::OnPauseIconClicked()
{
	TogglePause();
}

void USlinkyPauseMenu::OnResumeClicked()
{
	SetMenuOpen(false);
}

void USlinkyPauseMenu::OnRespawnClicked()
{
	if (ASlinkyActor* Slinky = FindSlinky())
	{
		Slinky->ResetSlinky();
	}
	SetMenuOpen(false);
}

void USlinkyPauseMenu::OnResetDefaultsClicked()
{
	if (ASlinkyStaircase* Staircase = FindStaircase())
	{
		Staircase->ResetToDefaults();
	}
	if (ASlinkyActor* Slinky = FindSlinky())
	{
		Slinky->ResetTuningToDefaults();
	}
	// USlinkyControlPanel caches its own copy of every tunable value for its rows' readouts -
	// without this it would keep showing the pre-reset numbers until the player nudged a slider.
	if (ASlinkyPlayerController* PC = Cast<ASlinkyPlayerController>(GetOwningPlayer()))
	{
		PC->RefreshControlPanelFromLiveValues();
	}
	SetMenuOpen(false);
}

void USlinkyPauseMenu::OnSaveAndQuitClicked()
{
	UWorld* World = GetWorld();
	USlinkyGameInstance* GameInstance = World ? Cast<USlinkyGameInstance>(World->GetGameInstance()) : nullptr;
	if (!GameInstance)
	{
		return;
	}

	const ASlinkyActor* Slinky = FindSlinky();
	const ASlinkyGameMode* GameMode = World ? Cast<ASlinkyGameMode>(World->GetAuthGameMode()) : nullptr;
	GameInstance->SaveProgress(
		Slinky ? Slinky->GetBestCombo() : 0,
		GameMode ? GameMode->GetCurrentDepthMeters() : 0.0f);

	UGameplayStatics::SetGamePaused(World, false);
	GameInstance->GoToTitle();
}

void USlinkyPauseMenu::OnVolumeChanged(float NewValue)
{
	if (USlinkyGameInstance* GameInstance = GetWorld() ? Cast<USlinkyGameInstance>(GetWorld()->GetGameInstance()) : nullptr)
	{
		GameInstance->SetMasterVolume(NewValue);
	}
}

void USlinkyPauseMenu::OnEffectsToggled(bool bChecked)
{
	if (USlinkyGameInstance* GameInstance = GetWorld() ? Cast<USlinkyGameInstance>(GetWorld()->GetGameInstance()) : nullptr)
	{
		GameInstance->SetEffectsEnabled(bChecked);
	}
}

void USlinkyPauseMenu::OnComboDisplayToggled(bool bChecked)
{
	if (USlinkyGameInstance* GameInstance = GetWorld() ? Cast<USlinkyGameInstance>(GetWorld()->GetGameInstance()) : nullptr)
	{
		GameInstance->SetComboDisplayEnabled(bChecked);
	}
}
