#include "BHInteractionPromptWidget.h"

#include "BHUIStyle.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Styling/CoreStyle.h"
#include "Widgets/SWidget.h"

TSharedRef<SWidget> UBHInteractionPromptWidget::RebuildWidget()
{
    if (!WidgetTree || !WidgetTree->RootWidget || !InteractionText)
    {
        BuildNativeLayout();
    }

    return Super::RebuildWidget();
}

void UBHInteractionPromptWidget::NativeConstruct()
{
    Super::NativeConstruct();

    const FText PendingText = ActiveText;
    const bool bHadPendingText = bHasActiveText && !PendingText.IsEmpty();

    if (!WidgetTree || !WidgetTree->RootWidget || !InteractionText)
    {
        BuildNativeLayout();
    }

    BHUIStyle::Apply(*this, EBHUIStyleContext::Gameplay);
    if (bHadPendingText)
    {
        ActiveText = PendingText;
        bHasActiveText = true;

        if (InteractionText)
        {
            InteractionText->SetText(PendingText);
            InteractionText->SetVisibility(ESlateVisibility::Visible);
            InteractionText->SetRenderOpacity(1.0f);
        }

        SetVisibility(ESlateVisibility::Visible);
        SetRenderOpacity(1.0f);
        Invalidate(EInvalidateWidgetReason::Paint);
    }
    else
    {
        ActiveText = FText::GetEmpty();
        bHasActiveText = false;

        if (InteractionText)
        {
            InteractionText->SetText(FText::GetEmpty());
        }

        SetVisibility(ESlateVisibility::Collapsed);
    }

    UE_LOG(
        LogTemp,
        Display,
        TEXT(
            "BH_INTERACTION_PROMPT_READY_NATIVE text_widget=%d"
        ),
        InteractionText != nullptr ? 1 : 0
    );
}

void UBHInteractionPromptWidget::BuildNativeLayout()
{
    if (!WidgetTree)
    {
        UE_LOG(
            LogTemp,
            Error,
            TEXT("BH_INTERACTION_PROMPT_LAYOUT_FAILED widget_tree=0")
        );
        return;
    }

    UCanvasPanel* NativeRoot = WidgetTree->ConstructWidget<UCanvasPanel>(
        UCanvasPanel::StaticClass(),
        TEXT("NativeInteractionPromptRoot")
    );
    UBorder* NativeBorder = WidgetTree->ConstructWidget<UBorder>(
        UBorder::StaticClass(),
        TEXT("NativeInteractionPromptBorder")
    );
    UTextBlock* NativeText = WidgetTree->ConstructWidget<UTextBlock>(
        UTextBlock::StaticClass(),
        TEXT("NativeInteractionPromptText")
    );

    if (!NativeRoot || !NativeBorder || !NativeText)
    {
        UE_LOG(
            LogTemp,
            Error,
            TEXT(
                "BH_INTERACTION_PROMPT_LAYOUT_FAILED "
                "root=%d border=%d text=%d"
            ),
            NativeRoot != nullptr ? 1 : 0,
            NativeBorder != nullptr ? 1 : 0,
            NativeText != nullptr ? 1 : 0
        );
        return;
    }

    NativeBorder->SetPadding(FMargin(24.0f, 14.0f));
    NativeBorder->SetHorizontalAlignment(HAlign_Center);
    NativeBorder->SetVerticalAlignment(VAlign_Center);

    FSlateBrush BackgroundBrush =
        *FCoreStyle::Get().GetBrush(TEXT("WhiteBrush"));
    BackgroundBrush.TintColor = FSlateColor(FLinearColor(
        0.018f,
        0.024f,
        0.026f,
        0.94f
    ));
    NativeBorder->SetBrush(BackgroundBrush);

    FSlateFontInfo Font = NativeText->GetFont();
    Font.TypefaceFontName = TEXT("Bold");
    Font.Size = 20;
    NativeText->SetFont(Font);
    NativeText->SetJustification(ETextJustify::Center);
    NativeText->SetAutoWrapText(true);
    NativeText->SetColorAndOpacity(FSlateColor(BHUIStyle::Gold));
    NativeText->SetShadowOffset(FVector2D(2.0f, 2.0f));
    NativeText->SetShadowColorAndOpacity(
        FLinearColor(0.0f, 0.0f, 0.0f, 0.9f)
    );

    NativeBorder->SetContent(NativeText);
    UCanvasPanelSlot* PromptSlot = NativeRoot->AddChildToCanvas(
        NativeBorder
    );
    if (!PromptSlot)
    {
        UE_LOG(
            LogTemp,
            Error,
            TEXT("BH_INTERACTION_PROMPT_LAYOUT_FAILED canvas_slot=0")
        );
        return;
    }

    PromptSlot->SetAnchors(FAnchors(0.5f, 0.42f, 0.5f, 0.42f));
    PromptSlot->SetAlignment(FVector2D(0.5f, 0.5f));
    PromptSlot->SetPosition(FVector2D::ZeroVector);
    PromptSlot->SetSize(FVector2D(760.0f, 96.0f));

    WidgetTree->RootWidget = NativeRoot;
    InteractionText = NativeText;
}

void UBHInteractionPromptWidget::SetInteractionText(
    const FText& NewText
)
{
    ActiveText = NewText;
    bHasActiveText = !NewText.IsEmpty();

    if (!InteractionText)
    {
        BuildNativeLayout();
    }

    if (!InteractionText)
    {
        return;
    }

    if (NewText.IsEmpty())
    {
        ClearInteractionText();
        return;
    }

    InteractionText->SetText(NewText);
    InteractionText->SetVisibility(ESlateVisibility::Visible);
    InteractionText->SetRenderOpacity(1.0f);
    SetVisibility(ESlateVisibility::Visible);
    SetRenderOpacity(1.0f);
    Invalidate(EInvalidateWidgetReason::Paint);

    UE_LOG(
        LogTemp,
        Display,
        TEXT("BH_INTERACTION_PROMPT_TEXT text=%s"),
        *NewText.ToString()
    );
}

void UBHInteractionPromptWidget::ClearInteractionText()
{
    ActiveText = FText::GetEmpty();
    bHasActiveText = false;

    if (InteractionText)
    {
        InteractionText->SetText(FText::GetEmpty());
    }

    SetVisibility(ESlateVisibility::Collapsed);
    Invalidate(EInvalidateWidgetReason::Paint);
}
