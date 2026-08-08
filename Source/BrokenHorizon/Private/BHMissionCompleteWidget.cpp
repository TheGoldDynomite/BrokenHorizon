#include "BHMissionCompleteWidget.h"

#include "BHUIStyle.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/TextBlock.h"
#include "Input/Events.h"
#include "Input/Reply.h"
#include "InputCoreTypes.h"
#include "GameFramework/PlayerController.h"

void UBHMissionCompleteWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();

    if (IsValid(MissionCompleteText) || !IsValid(WidgetTree))
    {
        return;
    }

    UBorder* DebriefBackdrop =
        WidgetTree->ConstructWidget<UBorder>(
            UBorder::StaticClass(),
            TEXT("NativeDebriefBackdrop")
        );
    MissionCompleteText =
        WidgetTree->ConstructWidget<UTextBlock>(
            UTextBlock::StaticClass(),
            TEXT("MissionCompleteText")
        );

    if (!IsValid(DebriefBackdrop) ||
        !IsValid(MissionCompleteText))
    {
        return;
    }

    WidgetTree->RootWidget = DebriefBackdrop;
    DebriefBackdrop->SetContent(MissionCompleteText);
    DebriefBackdrop->SetBrushColor(
        FLinearColor(0.015f, 0.02f, 0.025f, 0.96f)
    );
    DebriefBackdrop->SetPadding(FMargin(72.0f));
    DebriefBackdrop->SetHorizontalAlignment(HAlign_Center);
    DebriefBackdrop->SetVerticalAlignment(VAlign_Center);

    FSlateFontInfo DebriefFont =
        MissionCompleteText->GetFont();
    DebriefFont.Size = 24;
    MissionCompleteText->SetFont(DebriefFont);
    MissionCompleteText->SetColorAndOpacity(
        FSlateColor(FLinearColor(0.92f, 0.95f, 0.88f, 1.0f))
    );
    MissionCompleteText->SetJustification(
        ETextJustify::Center
    );
    MissionCompleteText->SetAutoWrapText(true);
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

void UBHMissionCompleteWidget::SetMissionCompleteText(
    const FText& NewText
)
{
    BHUIStyle::Apply(*this, EBHUIStyleContext::Overlay);

    if (IsValid(MissionCompleteText))
    {
        MissionCompleteText->SetText(
            FText::Format(
                NSLOCTEXT(
                    "BrokenHorizon",
                    "MissionCompleteContinuePrompt",
                    "{0}\n\n[ENTER / M] OPEN STRATEGIC COMMAND"
                ),
                NewText
            )
        );
    }
}

void UBHMissionCompleteWidget::ResetContinueRequest()
{
    bContinueRequestSent = false;
}
