#include "BHSubtitleWidget.h"

#include "BHUserSettingsSubsystem.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Engine/GameInstance.h"

void UBHSubtitleWidget::NativeConstruct()
{
    Super::NativeConstruct();
    EnsureNativeLayout();
    SetVisibility(ESlateVisibility::Collapsed);
}

void UBHSubtitleWidget::NativeTick(
    const FGeometry& MyGeometry,
    float InDeltaTime
)
{
    Super::NativeTick(MyGeometry, InDeltaTime);
    if (RemainingDuration > 0.0f)
    {
        RemainingDuration = FMath::Max(0.0f, RemainingDuration - InDeltaTime);
        if (RemainingDuration <= 0.0f)
        {
            if (PendingSubtitles.Num() > 0)
            {
                const FPendingSubtitle Next = PendingSubtitles[0];
                PendingSubtitles.RemoveAt(0);
                PresentSubtitle(Next);
            }
            else
            {
                ClearSubtitle();
            }
        }
    }
}

void UBHSubtitleWidget::EnsureNativeLayout()
{
    if (!IsValid(WidgetTree) || IsValid(SubtitleText)) return;

    UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(
        UCanvasPanel::StaticClass(), TEXT("SubtitleSafeAreaCanvas")
    );
    WidgetTree->RootWidget = Canvas;
    SubtitleBackground = WidgetTree->ConstructWidget<UBorder>(
        UBorder::StaticClass(), TEXT("SubtitleBackground")
    );
    SubtitleText = WidgetTree->ConstructWidget<UTextBlock>(
        UTextBlock::StaticClass(), TEXT("SubtitleText")
    );
    SubtitleText->SetAutoWrapText(true);
    SubtitleText->SetJustification(ETextJustify::Center);
    SubtitleBackground->SetPadding(FMargin(20.0f, 10.0f));
    SubtitleBackground->AddChild(SubtitleText);
    UCanvasPanelSlot* CanvasSlot = Canvas->AddChildToCanvas(SubtitleBackground);
    CanvasSlot->SetAnchors(FAnchors(0.5f, 0.82f));
    CanvasSlot->SetAlignment(FVector2D(0.5f, 0.5f));
    CanvasSlot->SetSize(FVector2D(900.0f, 120.0f));
}

void UBHSubtitleWidget::ShowSubtitle(
    const FText& Speaker,
    const FText& Line,
    float DurationSeconds,
    float DirectionAngleDegrees,
    bool bHasDirection
)
{
    EnsureNativeLayout();
    const UGameInstance* GameInstance = GetGameInstance();
    const UBHUserSettingsSubsystem* Settings = GameInstance
        ? GameInstance->GetSubsystem<UBHUserSettingsSubsystem>()
        : nullptr;
    if ((Settings && !Settings->AreSubtitlesEnabled()) || Line.IsEmpty())
    {
        ClearSubtitle();
        return;
    }
    FPendingSubtitle Subtitle;
    Subtitle.Speaker = Speaker;
    Subtitle.Line = Line;
    Subtitle.Duration = DurationSeconds;
    Subtitle.DirectionDegrees = DirectionAngleDegrees;
    Subtitle.bHasDirection = bHasDirection;
    if (RemainingDuration > 0.0f)
    {
        PendingSubtitles.Add(MoveTemp(Subtitle));
        return;
    }
    PresentSubtitle(Subtitle);
}

void UBHSubtitleWidget::PresentSubtitle(const FPendingSubtitle& Subtitle)
{
    ActiveSpeaker = Subtitle.Speaker;
    ActiveLine = Subtitle.Line;
    ActiveDirectionDegrees = Subtitle.DirectionDegrees;
    bActiveHasDirection = Subtitle.bHasDirection;
    RemainingDuration = FMath::Max(0.1f, Subtitle.Duration);
    RefreshPresentation();
    SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UBHSubtitleWidget::ClearSubtitle()
{
    RemainingDuration = 0.0f;
    ActiveSpeaker = FText::GetEmpty();
    ActiveLine = FText::GetEmpty();
    PendingSubtitles.Reset();
    SetVisibility(ESlateVisibility::Collapsed);
}

FText UBHSubtitleWidget::BuildDirectionalIndicator(float DirectionAngleDegrees)
{
    const float Angle = FRotator::NormalizeAxis(DirectionAngleDegrees);
    if (FMath::Abs(Angle) <= 30.0f) return FText::FromString(TEXT("^"));
    if (Angle > 30.0f && Angle < 150.0f) return FText::FromString(TEXT(">"));
    if (Angle < -30.0f && Angle > -150.0f) return FText::FromString(TEXT("<"));
    return FText::FromString(TEXT("v"));
}

void UBHSubtitleWidget::RefreshPresentation()
{
    if (!IsValid(SubtitleText) || !IsValid(SubtitleBackground)) return;
    const UGameInstance* GameInstance = GetGameInstance();
    const UBHUserSettingsSubsystem* Settings = GameInstance
        ? GameInstance->GetSubsystem<UBHUserSettingsSubsystem>()
        : nullptr;
    const bool bShowSpeaker = !Settings || Settings->AreSubtitleSpeakerLabelsEnabled();
    const bool bShowDirection = bActiveHasDirection &&
        (!Settings || Settings->AreSubtitleDirectionalIndicatorsEnabled());
    FString Display;
    if (bShowDirection)
    {
        Display += BuildDirectionalIndicator(ActiveDirectionDegrees).ToString() + TEXT("  ");
    }
    if (bShowSpeaker && !ActiveSpeaker.IsEmpty())
    {
        Display += ActiveSpeaker.ToString().ToUpper() + TEXT(": ");
    }
    Display += ActiveLine.ToString();
    SubtitleText->SetText(FText::FromString(Display));
    FSlateFontInfo Font = SubtitleText->GetFont();
    Font.Size = FMath::RoundToInt(24.0f * (Settings ? Settings->GetSubtitleTextScale() : 1.0f));
    SubtitleText->SetFont(Font);
    SubtitleBackground->SetBrushColor(FLinearColor(
        0.0f, 0.0f, 0.0f,
        Settings ? Settings->GetSubtitleBackgroundOpacity() : 0.75f
    ));
    if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(SubtitleBackground->Slot))
    {
        const float SafeScale = Settings ? Settings->GetUISafeAreaScale() : 0.95f;
        CanvasSlot->SetSize(FVector2D(900.0f * SafeScale, 120.0f));
    }
}
