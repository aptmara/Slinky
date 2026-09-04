#include "SlinkyTitleScreen.h"
#include "SlinkyGameInstance.h"
#include "SlinkyPopAnimator.h"
#include "Blueprint/WidgetTree.h"
#include "Brushes/SlateColorBrush.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "Kismet/KismetSystemLibrary.h"

namespace
{
	constexpr float ButtonRadius = 16.0f;
	constexpr float ButtonShadowOffset = 6.0f;
	constexpr float ButtonWidth = 320.0f;
	constexpr float ButtonHeight = 72.0f;

	FLinearColor Shade(const FLinearColor& Color)
	{
		return FLinearColor(Color.R * 0.55f, Color.G * 0.48f, Color.B * 0.58f, 1.0f);
	}
}

void USlinkyTitleScreen::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
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

UPopAnimator* USlinkyTitleScreen::AddButtonPop(UButton* Button)
{
	UPopAnimator* Anim = NewObject<UPopAnimator>(this);
	Anim->Bind(Button);
	Anim->BindButtonEvents(Button);
	Animators.Add(Anim);
	return Anim;
}

UButton* USlinkyTitleScreen::AddMenuButton(UPanelWidget* Parent, const FString& Label, FLinearColor Accent, bool bEnabled)
{
	// Disabled (no save yet) renders as a flat gray card instead of the accent color, and skips
	// the pop animator so it doesn't visually invite a click it won't act on.
	const FLinearColor FaceColor = bEnabled ? Accent : FLinearColor(0.82f, 0.82f, 0.82f);
	const FLinearColor ShadowColor = bEnabled ? Shade(Accent) : FLinearColor(0.62f, 0.62f, 0.62f);
	const FLinearColor TextColor = bEnabled ? FLinearColor(0.98f, 0.98f, 0.98f) : FLinearColor(0.55f, 0.55f, 0.55f);

	UOverlay* Card = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());

	UBorder* ShadowLayer = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	ShadowLayer->SetBrush(FSlateRoundedBoxBrush(ShadowColor, ButtonRadius));
	if (UOverlaySlot* S = Card->AddChildToOverlay(ShadowLayer))
	{
		S->SetHorizontalAlignment(HAlign_Fill);
		S->SetVerticalAlignment(VAlign_Fill);
		S->SetPadding(FMargin(ButtonShadowOffset, ButtonShadowOffset, 0.0f, 0.0f));
	}

	UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
	FButtonStyle ButtonStyle = Button->WidgetStyle;
	ButtonStyle.Normal = FSlateRoundedBoxBrush(FaceColor, ButtonRadius);
	ButtonStyle.Hovered = FSlateRoundedBoxBrush(FaceColor * 1.08f, ButtonRadius);
	ButtonStyle.Pressed = FSlateRoundedBoxBrush(FaceColor * 0.85f, ButtonRadius);
	ButtonStyle.Disabled = FSlateRoundedBoxBrush(FaceColor, ButtonRadius);
	Button->SetStyle(ButtonStyle);
	Button->SetIsEnabled(bEnabled);

	UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	Text->SetText(FText::FromString(Label));
	{
		FSlateFontInfo Font = Text->GetFont();
		Font.Size = 26;
		Text->SetFont(Font);
	}
	Text->SetColorAndOpacity(FSlateColor(TextColor));
	Text->SetJustification(ETextJustify::Center);
	Button->SetContent(Text);

	if (UOverlaySlot* S = Card->AddChildToOverlay(Button))
	{
		S->SetHorizontalAlignment(HAlign_Fill);
		S->SetVerticalAlignment(VAlign_Fill);
		S->SetPadding(FMargin(0.0f, 0.0f, ButtonShadowOffset, ButtonShadowOffset));
	}

	USizeBox* Sized = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	Sized->SetWidthOverride(ButtonWidth);
	Sized->SetHeightOverride(ButtonHeight);
	Sized->SetContent(Card);

	if (UVerticalBoxSlot* CardSlot = Cast<UVerticalBoxSlot>(Parent->AddChild(Sized)))
	{
		CardSlot->SetHorizontalAlignment(HAlign_Center);
		CardSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 18.0f));
	}

	if (bEnabled)
	{
		AddButtonPop(Button);
	}

	return Button;
}

void USlinkyTitleScreen::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Root"));
	WidgetTree->RootWidget = Root;

	// A flat, chalky-cream backdrop - matches the logo's own background - rather than seeing
	// through to whatever (or nothing) is behind an otherwise pawn-less title level.
	UBorder* Background = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Background->SetBrush(FSlateColorBrush(FLinearColor(FColor(0xF7, 0xF7, 0xF2))));
	if (UCanvasPanelSlot* S = Root->AddChildToCanvas(Background))
	{
		S->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
		S->SetOffsets(FMargin(0.0f));
	}

	UVerticalBox* Content = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	if (UCanvasPanelSlot* S = Root->AddChildToCanvas(Content))
	{
		S->SetAnchors(FAnchors(0.5f, 0.5f));
		S->SetAlignment(FVector2D(0.5f, 0.5f));
		S->SetAutoSize(true);
	}

	if (UTexture2D* LogoTexture = LoadObject<UTexture2D>(nullptr, TEXT("/Game/Slinky/UI/T_SlinkyLogo.T_SlinkyLogo")))
	{
		UImage* Logo = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
		Logo->SetBrushFromTexture(LogoTexture, true);

		constexpr float LogoWidth = 380.0f;
		const float AspectRatio = LogoTexture->GetSizeX() > 0
			? static_cast<float>(LogoTexture->GetSizeY()) / static_cast<float>(LogoTexture->GetSizeX())
			: 1.0f;

		USizeBox* LogoBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		LogoBox->SetWidthOverride(LogoWidth);
		LogoBox->SetHeightOverride(LogoWidth * AspectRatio);
		LogoBox->SetContent(Logo);
		if (UVerticalBoxSlot* S = Content->AddChildToVerticalBox(LogoBox))
		{
			S->SetHorizontalAlignment(HAlign_Center);
			S->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 44.0f));
		}
	}

	USlinkyGameInstance* GameInstance = GetWorld() ? Cast<USlinkyGameInstance>(GetWorld()->GetGameInstance()) : nullptr;
	const bool bHasSave = GameInstance && GameInstance->HasSaveGame();

	// Warm orange (from the logo's stair accent) for the primary action, cool blue (the logo's
	// coil color) for Continue - keeps both buttons visually tied to the logo above them.
	UButton* NewGameButton = AddMenuButton(Content, TEXT("はじめから"), FLinearColor(0.92f, 0.46f, 0.24f), true);
	NewGameButton->OnClicked.AddDynamic(this, &USlinkyTitleScreen::OnNewGameClicked);

	UButton* ContinueButton = AddMenuButton(Content, TEXT("つづきから"), FLinearColor(0.16f, 0.28f, 0.58f), bHasSave);
	ContinueButton->OnClicked.AddDynamic(this, &USlinkyTitleScreen::OnContinueClicked);

	if (bHasSave)
	{
		UTextBlock* RecordText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		RecordText->SetText(FText::FromString(FString::Printf(TEXT("ベストコンボ x%d ・ 最深 %.0fm"),
			GameInstance->GetSavedBestCombo(), GameInstance->GetSavedBestDepthMeters())));
		FSlateFontInfo Font = RecordText->GetFont();
		Font.Size = 14;
		RecordText->SetFont(Font);
		RecordText->SetColorAndOpacity(FSlateColor(FLinearColor(0.45f, 0.40f, 0.38f)));
		RecordText->SetJustification(ETextJustify::Center);
		if (UVerticalBoxSlot* S = Content->AddChildToVerticalBox(RecordText))
		{
			S->SetHorizontalAlignment(HAlign_Center);
			S->SetPadding(FMargin(0.0f, 6.0f, 0.0f, 0.0f));
		}
	}

	{
		USizeBox* Spacer = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		Spacer->SetHeightOverride(28.0f);
		Content->AddChildToVerticalBox(Spacer);
	}

	// Muted gray rather than one of the logo's accent colors - reads as the one button here that
	// leaves rather than starts something.
	UButton* QuitButton = AddMenuButton(Content, TEXT("ゲームを終了"), FLinearColor(0.55f, 0.53f, 0.52f), true);
	QuitButton->OnClicked.AddDynamic(this, &USlinkyTitleScreen::OnQuitClicked);
}

void USlinkyTitleScreen::OnNewGameClicked()
{
	if (USlinkyGameInstance* GameInstance = GetWorld() ? Cast<USlinkyGameInstance>(GetWorld()->GetGameInstance()) : nullptr)
	{
		GameInstance->GoToGame(false);
	}
}

void USlinkyTitleScreen::OnContinueClicked()
{
	if (USlinkyGameInstance* GameInstance = GetWorld() ? Cast<USlinkyGameInstance>(GetWorld()->GetGameInstance()) : nullptr)
	{
		if (GameInstance->HasSaveGame())
		{
			GameInstance->GoToGame(true);
		}
	}
}

void USlinkyTitleScreen::OnQuitClicked()
{
	UKismetSystemLibrary::QuitGame(this, GetOwningPlayer(), EQuitPreference::Quit, false);
}
