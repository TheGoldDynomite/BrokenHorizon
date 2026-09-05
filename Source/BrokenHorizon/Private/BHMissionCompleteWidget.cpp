#include "BHMissionCompleteWidget.h"

#include "BHUIStyle.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/ScrollBox.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/TextBlock.h"
#include "Input/Events.h"
#include "Input/Reply.h"
#include "InputCoreTypes.h"
#include "GameFramework/PlayerController.h"

void UBHMissionCompleteWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    if (!IsValid(WidgetTree)) { return; }
    if (!IsValid(MissionCompleteText))
    {
        MissionCompleteText = WidgetTree->ConstructWidget<UTextBlock>(
            UTextBlock::StaticClass(), TEXT("MissionCompleteText"));
    }
    // Preserve the actual bound text object, but discard oversized authored slot constraints.
    MissionCompleteText->RemoveFromParent();
    UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("NativeDebriefRoot"));
    DebriefBackdrop = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("NativeDebriefBackdrop"));
    UVerticalBox* Layout = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("NativeDebriefLayout"));
    DebriefScrollBox = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("NativeDebriefScroll"));
    ContinuePrompt = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("NativeDebriefContinue"));
    WidgetTree->RootWidget = Root;
    Root->AddChildToCanvas(DebriefBackdrop);
    DebriefBackdrop->SetContent(Layout);
    UVerticalBoxSlot* BodySlot = Layout->AddChildToVerticalBox(DebriefScrollBox);
    BodySlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    BodySlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 20.0f));
    DebriefScrollBox->AddChild(MissionCompleteText);
    Layout->AddChildToVerticalBox(ContinuePrompt)->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
    ContinuePrompt->SetText(NSLOCTEXT("BrokenHorizon", "DebriefContinueFooter", "[ENTER / M] OPEN STRATEGIC COMMAND"));
    BHUIStyle::Apply(*this, EBHUIStyleContext::Overlay);
    ApplyDebriefLayout(UWidgetLayoutLibrary::GetViewportSize(this) /
        FMath::Max(0.01f, UWidgetLayoutLibrary::GetViewportScale(this)));
}

void UBHMissionCompleteWidget::ApplyDebriefLayout(const FVector2D& ViewSize)
{
    if (!IsValid(MissionCompleteText) || !IsValid(DebriefBackdrop) ||
        !IsValid(DebriefScrollBox) || !IsValid(ContinuePrompt)) { return; }
    LastLayoutSize = ViewSize;
    if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(DebriefBackdrop->Slot))
    {
        CanvasSlot->SetAnchors(FAnchors(0.06f, 0.07f, 0.94f, 0.93f));
        CanvasSlot->SetOffsets(FMargin(0.0f));
        CanvasSlot->SetAutoSize(false);
    }
    DebriefBackdrop->SetBrushColor(FLinearColor(0.015f, 0.02f, 0.025f, 0.97f));
    DebriefBackdrop->SetPadding(FMargin(24.0f));
    DebriefBackdrop->SetHorizontalAlignment(HAlign_Fill);
    DebriefBackdrop->SetVerticalAlignment(VAlign_Fill);
    DebriefScrollBox->SetClipping(EWidgetClipping::ClipToBounds);
    MissionCompleteText->SetMinDesiredWidth(0.0f);
    MissionCompleteText->SetAutoWrapText(true);
    MissionCompleteText->SetWrapTextAt(FMath::Max(200.0f, ViewSize.X * 0.88f - 80.0f));
    MissionCompleteText->SetJustification(ETextJustify::Left);
    MissionCompleteText->SetLineHeightPercentage(1.0f);
    FSlateFontInfo BodyFont = MissionCompleteText->GetFont();
    BodyFont.Size = 24;
    MissionCompleteText->SetFont(BodyFont);
    MissionCompleteText->SetColorAndOpacity(FSlateColor(FLinearColor(0.92f, 0.95f, 0.88f, 1.0f)));
    FSlateFontInfo FooterFont = ContinuePrompt->GetFont();
    FooterFont.Size = 20;
    ContinuePrompt->SetFont(FooterFont);
    ContinuePrompt->SetJustification(ETextJustify::Center);
    ContinuePrompt->SetAutoWrapText(true);
}

void UBHMissionCompleteWidget::NativeConstruct()
{
    Super::NativeConstruct();
    SetIsFocusable(true);
    bContinueInputArmed = false;
    bContinueRequestSent = false;
}

void UBHMissionCompleteWidget::NativeTick(
    const FGeometry& MyGeometry,
    float InDeltaTime
)
{
    Super::NativeTick(MyGeometry, InDeltaTime);
    if (!MyGeometry.GetLocalSize().Equals(LastLayoutSize, 1.0f))
    {
        ApplyDebriefLayout(MyGeometry.GetLocalSize());
    }

    if (bContinueRequestSent)
    {
        return;
    }

    APlayerController* PlayerController = GetOwningPlayer();

    if (!IsValid(PlayerController))
    {
        return;
    }

    const FKey GamepadAccept =
        EKeys::Virtual_Gamepad_Accept.GetVirtualKey();
    const bool bContinueDown =
        PlayerController->IsInputKeyDown(EKeys::Enter) ||
        PlayerController->IsInputKeyDown(EKeys::SpaceBar) ||
        PlayerController->IsInputKeyDown(EKeys::M) ||
        PlayerController->IsInputKeyDown(GamepadAccept);

    if (!bContinueDown)
    {
        bContinueInputArmed = true;
        return;
    }

    if (bContinueInputArmed &&
        (PlayerController->WasInputKeyJustPressed(EKeys::Enter) ||
         PlayerController->WasInputKeyJustPressed(EKeys::SpaceBar) ||
         PlayerController->WasInputKeyJustPressed(EKeys::M) ||
         PlayerController->WasInputKeyJustPressed(GamepadAccept)))
    {
        RequestContinue();
    }
}

FReply UBHMissionCompleteWidget::NativeOnKeyDown(
    const FGeometry& InGeometry,
    const FKeyEvent& InKeyEvent
)
{
    const FKey Key = InKeyEvent.GetKey();

    if (Key == EKeys::Enter ||
        Key == EKeys::Virtual_Gamepad_Accept.GetVirtualKey() ||
        Key == EKeys::SpaceBar ||
        Key == EKeys::M)
    {
        RequestContinue();
        return FReply::Handled();
    }

    return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void UBHMissionCompleteWidget::RequestContinue()
{
    if (bContinueRequestSent)
    {
        return;
    }

    bContinueRequestSent = true;
    OnContinueRequested.Broadcast();
}

void UBHMissionCompleteWidget::SetMissionCompleteText(const FText& NewText)
{
    BHUIStyle::Apply(*this, EBHUIStyleContext::Overlay);
    if (IsValid(MissionCompleteText)) { MissionCompleteText->SetText(NewText); }
    ApplyDebriefLayout(UWidgetLayoutLibrary::GetViewportSize(this) /
        FMath::Max(0.01f, UWidgetLayoutLibrary::GetViewportScale(this)));
    if (IsValid(DebriefScrollBox)) { DebriefScrollBox->ScrollToStart(); }
}

void UBHMissionCompleteWidget::ResetContinueRequest()
{
    bContinueRequestSent = false;
}
