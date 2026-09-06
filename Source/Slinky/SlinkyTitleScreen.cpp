#include "SlinkyTitleScreen.h"
#include "SlinkyChallengeTypes.h"
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
#include "UObject/ConstructorHelpers.h"

// Names here are prefixed (Title*) because a packaged (non-editor) build's Unity build merges
// several widget .cpp files - each with their own similarly-named anonymous-namespace helpers -
// into one translation unit, where an unqualified "Shade"/"ButtonRadius" in more than one of them
// is a redefinition error rather than the per-file-scoped name it is in a normal (non-unity) build.
namespace
{
	constexpr float TitleButtonRadius = 16.0f;
	constexpr float TitleButtonShadowOffset = 6.0f;
	constexpr float TitleButtonWidth = 320.0f;
	constexpr float TitleButtonHeight = 72.0f;

	FLinearColor TitleShade(const FLinearColor& Color)
	{
		return FLinearColor(Color.R * 0.55f, Color.G * 0.48f, Color.B * 0.58f, 1.0f);
	}
}

USlinkyTitleScreen::USlinkyTitleScreen(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	static ConstructorHelpers::FObjectFinder<UTexture2D> LogoFinder(TEXT("/Game/Slinky/UI/T_SlinkyLogo.T_SlinkyLogo"));
	if (LogoFinder.Succeeded())
	{
		LogoTexture = LogoFinder.Object;
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
	const FLinearColor ShadowColor = bEnabled ? TitleShade(Accent) : FLinearColor(0.62f, 0.62f, 0.62f);
	const FLinearColor TextColor = bEnabled ? FLinearColor(0.98f, 0.98f, 0.98f) : FLinearColor(0.55f, 0.55f, 0.55f);

	UOverlay* Card = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());

	UBorder* ShadowLayer = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	ShadowLayer->SetBrush(FSlateRoundedBoxBrush(ShadowColor, TitleButtonRadius));
	if (UOverlaySlot* S = Card->AddChildToOverlay(ShadowLayer))
	{
		S->SetHorizontalAlignment(HAlign_Fill);
		S->SetVerticalAlignment(VAlign_Fill);
		S->SetPadding(FMargin(TitleButtonShadowOffset, TitleButtonShadowOffset, 0.0f, 0.0f));
	}

	UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
	FButtonStyle ButtonStyle = Button->WidgetStyle;
	ButtonStyle.Normal = FSlateRoundedBoxBrush(FaceColor, TitleButtonRadius);
	ButtonStyle.Hovered = FSlateRoundedBoxBrush(FaceColor * 1.08f, TitleButtonRadius);
	ButtonStyle.Pressed = FSlateRoundedBoxBrush(FaceColor * 0.85f, TitleButtonRadius);
	ButtonStyle.Disabled = FSlateRoundedBoxBrush(FaceColor, TitleButtonRadius);
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
		S->SetPadding(FMargin(0.0f, 0.0f, TitleButtonShadowOffset, TitleButtonShadowOffset));
	}

	USizeBox* Sized = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	Sized->SetWidthOverride(TitleButtonWidth);
	Sized->SetHeightOverride(TitleButtonHeight);
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

void USlinkyTitleScreen::AddCaption(UPanelWidget* Parent, const FString& Text)
{
	UTextBlock* Caption = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	Caption->SetText(FText::FromString(Text));
	FSlateFontInfo Font = Caption->GetFont();
	Font.Size = 14;
	Caption->SetFont(Font);
	Caption->SetColorAndOpacity(FSlateColor(FLinearColor(0.45f, 0.40f, 0.38f)));
	Caption->SetJustification(ETextJustify::Center);
	if (UVerticalBoxSlot* S = Cast<UVerticalBoxSlot>(Parent->AddChild(Caption)))
	{
		S->SetHorizontalAlignment(HAlign_Center);
		S->SetPadding(FMargin(0.0f, 6.0f, 0.0f, 0.0f));
	}
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

	if (LogoTexture)
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
		AddCaption(Content, FString::Printf(TEXT("ベストコンボ x%d ・ 最深 %.0fm"),
			GameInstance->GetSavedBestCombo(), GameInstance->GetSavedBestDepthMeters()));
	}

	{
		USizeBox* Spacer = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		Spacer->SetHeightOverride(20.0f);
		Content->AddChildToVerticalBox(Spacer);
	}

	// デイリーチャレンジ: today's UTC-dated, deterministically generated slinky+stairs - every
	// player sees the same one today (see FSlinkyDailyChallenge) and it's ranked purely against
	// itself (GetTodayChallengeId()), never against ランク or 自由 runs.
	const FSlinkyChallengeConfig TodaysChallenge = GameInstance ? GameInstance->GetTodaysChallengeConfig() : FSlinkyChallengeConfig();
	const FString TodayChallengeId = GameInstance ? GameInstance->GetTodayChallengeId() : FString();
	UButton* DailyButton = AddMenuButton(Content, TEXT("今日のチャレンジ"), FLinearColor(0.62f, 0.40f, 0.78f), GameInstance != nullptr);
	DailyButton->OnClicked.AddDynamic(this, &USlinkyTitleScreen::OnDailyChallengeClicked);

	FSlinkyLocalRecord DailyBest;
	const bool bHasDailyBest = GameInstance && GameInstance->GetDailyBest(TodayChallengeId, DailyBest);
	AddCaption(Content, bHasDailyBest
		? FString::Printf(TEXT("本日: %s ・ ベスト %d歩"), *TodaysChallenge.PresetName, DailyBest.StepCount)
		: FString::Printf(TEXT("本日: %s ・ 未挑戦"), *TodaysChallenge.PresetName));

	{
		USizeBox* Spacer = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		Spacer->SetHeightOverride(18.0f);
		Content->AddChildToVerticalBox(Spacer);
	}

	// ランクに挑戦: fixed standard settings (FSlinkyChallengeConfig::Defaults()) so every attempt
	// is graded on the same slinky+stairs, unlike 自由 where anything goes.
	UButton* RankedButton = AddMenuButton(Content, TEXT("ランクに挑戦"), FLinearColor(0.20f, 0.56f, 0.48f), true);
	RankedButton->OnClicked.AddDynamic(this, &USlinkyTitleScreen::OnRankedClicked);

	const TArray<FSlinkyLocalRecord> RankedTop = GameInstance ? GameInstance->GetRankedLeaderboard(1) : TArray<FSlinkyLocalRecord>();
	AddCaption(Content, RankedTop.Num() > 0
		? FString::Printf(TEXT("自己ベスト %d歩"), RankedTop[0].StepCount)
		: FString(TEXT("記録なし")));

	{
		USizeBox* Spacer = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		Spacer->SetHeightOverride(18.0f);
		Content->AddChildToVerticalBox(Spacer);
	}

	UButton* LeaderboardButton = AddMenuButton(Content, TEXT("ローカルランキング"), FLinearColor(0.55f, 0.53f, 0.30f), true);
	LeaderboardButton->OnClicked.AddDynamic(this, &USlinkyTitleScreen::OnLeaderboardClicked);

	{
		USizeBox* Spacer = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		Spacer->SetHeightOverride(10.0f);
		Content->AddChildToVerticalBox(Spacer);
	}

	// Muted gray rather than one of the logo's accent colors - reads as the one button here that
	// leaves rather than starts something.
	UButton* QuitButton = AddMenuButton(Content, TEXT("ゲームを終了"), FLinearColor(0.55f, 0.53f, 0.52f), true);
	QuitButton->OnClicked.AddDynamic(this, &USlinkyTitleScreen::OnQuitClicked);

	BuildLeaderboardOverlay(Root);
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

void USlinkyTitleScreen::OnDailyChallengeClicked()
{
	if (USlinkyGameInstance* GameInstance = GetWorld() ? Cast<USlinkyGameInstance>(GetWorld()->GetGameInstance()) : nullptr)
	{
		GameInstance->GoToGame(false, ESlinkyGameMode::DailyChallenge);
	}
}

void USlinkyTitleScreen::OnRankedClicked()
{
	if (USlinkyGameInstance* GameInstance = GetWorld() ? Cast<USlinkyGameInstance>(GetWorld()->GetGameInstance()) : nullptr)
	{
		GameInstance->GoToGame(false, ESlinkyGameMode::Ranked);
	}
}

void USlinkyTitleScreen::OnLeaderboardClicked()
{
	if (LeaderboardOverlay)
	{
		RefreshLeaderboardRows();
		LeaderboardOverlay->SetVisibility(ESlateVisibility::Visible);
	}
}

void USlinkyTitleScreen::OnLeaderboardCloseClicked()
{
	if (LeaderboardOverlay)
	{
		LeaderboardOverlay->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void USlinkyTitleScreen::OnQuitClicked()
{
	UKismetSystemLibrary::QuitGame(this, GetOwningPlayer(), EQuitPreference::Quit, false);
}

void USlinkyTitleScreen::BuildLeaderboardOverlay(UPanelWidget* RootParent)
{
	UCanvasPanel* Root = Cast<UCanvasPanel>(RootParent);
	if (!Root)
	{
		return;
	}

	LeaderboardOverlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
	if (UCanvasPanelSlot* S = Root->AddChildToCanvas(LeaderboardOverlay))
	{
		S->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
		S->SetOffsets(FMargin(0.0f));
	}
	// Hidden until OnLeaderboardClicked shows it - not built collapsed-and-populated up front,
	// since a run finishing while the title screen widget is still alive (it isn't, it's
	// recreated per level, but the same "read fresh, not once" habit applies) would otherwise
	// leave stale numbers on screen.
	LeaderboardOverlay->SetVisibility(ESlateVisibility::Collapsed);

	UBorder* Dim = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Dim->SetBrush(FSlateColorBrush(FLinearColor(0.0f, 0.0f, 0.0f, 0.55f)));
	LeaderboardOverlay->AddChildToOverlay(Dim);

	UBorder* Card = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Card->SetBrush(FSlateRoundedBoxBrush(FLinearColor(0.98f, 0.98f, 0.95f), 16.0f));
	Card->SetPadding(FMargin(28.0f));
	if (UOverlaySlot* S = LeaderboardOverlay->AddChildToOverlay(Card))
	{
		S->SetHorizontalAlignment(HAlign_Center);
		S->SetVerticalAlignment(VAlign_Center);
	}

	UVerticalBox* CardContent = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());

	USizeBox* Sized = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	Sized->SetWidthOverride(420.0f);
	Sized->SetContent(CardContent);
	Card->SetContent(Sized);

	UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	Title->SetText(FText::FromString(TEXT("ローカルランキング")));
	{
		FSlateFontInfo Font = Title->GetFont();
		Font.Size = 22;
		Title->SetFont(Font);
	}
	Title->SetJustification(ETextJustify::Center);
	if (UVerticalBoxSlot* S = CardContent->AddChildToVerticalBox(Title))
	{
		S->SetHorizontalAlignment(HAlign_Center);
		S->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 16.0f));
	}

	LeaderboardListBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	if (UVerticalBoxSlot* S = CardContent->AddChildToVerticalBox(LeaderboardListBox))
	{
		S->SetHorizontalAlignment(HAlign_Fill);
		S->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 20.0f));
	}

	UButton* CloseButton = AddMenuButton(CardContent, TEXT("閉じる"), FLinearColor(0.55f, 0.53f, 0.52f), true);
	CloseButton->OnClicked.AddDynamic(this, &USlinkyTitleScreen::OnLeaderboardCloseClicked);
}

void USlinkyTitleScreen::RefreshLeaderboardRows()
{
	if (!LeaderboardListBox)
	{
		return;
	}
	LeaderboardListBox->ClearChildren();

	USlinkyGameInstance* GameInstance = GetWorld() ? Cast<USlinkyGameInstance>(GetWorld()->GetGameInstance()) : nullptr;
	if (!GameInstance)
	{
		return;
	}

	const auto AddRow = [this](const FString& Text)
	{
		UTextBlock* Row = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		Row->SetText(FText::FromString(Text));
		FSlateFontInfo Font = Row->GetFont();
		Font.Size = 15;
		Row->SetFont(Font);
		Row->SetColorAndOpacity(FSlateColor(FLinearColor(0.30f, 0.28f, 0.26f)));
		if (UVerticalBoxSlot* S = LeaderboardListBox->AddChildToVerticalBox(Row))
		{
			S->SetPadding(FMargin(0.0f, 3.0f));
		}
	};

	AddRow(TEXT("【ランク：自己ベスト上位】"));
	const TArray<FSlinkyLocalRecord> RankedTop = GameInstance->GetRankedLeaderboard(10);
	if (RankedTop.Num() == 0)
	{
		AddRow(TEXT("　まだ記録がありません"));
	}
	else
	{
		for (int32 Index = 0; Index < RankedTop.Num(); ++Index)
		{
			const FSlinkyLocalRecord& Record = RankedTop[Index];
			AddRow(FString::Printf(TEXT("　%2d位  %4d歩  (コンボx%d)"), Index + 1, Record.StepCount, Record.BestCombo));
		}
	}

	AddRow(FString());
	AddRow(TEXT("【本日のチャレンジ】"));
	FSlinkyLocalRecord DailyBest;
	if (GameInstance->GetDailyBest(GameInstance->GetTodayChallengeId(), DailyBest))
	{
		AddRow(FString::Printf(TEXT("　ベスト %d歩  (コンボx%d)"), DailyBest.StepCount, DailyBest.BestCombo));
	}
	else
	{
		AddRow(TEXT("　まだ挑戦していません"));
	}
}
