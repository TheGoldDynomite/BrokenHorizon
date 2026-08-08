#include "BHMainMenuWidget.h"

#include "BHSaveSubsystem.h"
#include "BHSessionSubsystem.h"
#include "BHSettingsWidget.h"
#include "BHUIStyle.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "Engine/GameInstance.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/KismetSystemLibrary.h"

namespace
{
void EnforceMainMenuTextSafeFrame(UWidgetTree& WidgetTree)
{
    WidgetTree.ForEachWidget([](UWidget* Widget)
    {
        UTextBlock* Text = Cast<UTextBlock>(Widget);
        UCanvasPanelSlot* CanvasSlot = IsValid(Text)
            ? Cast<UCanvasPanelSlot>(Text->Slot)
            : nullptr;
        if (!IsValid(CanvasSlot))
        {
            return;
        }

        const FAnchors Anchors = CanvasSlot->GetAnchors();
        if (!Anchors.Minimum.IsNearlyZero() ||
            !Anchors.Maximum.IsNearlyZero())
        {
            return;
        }

        FVector2D Position = CanvasSlot->GetPosition();
        Position.X = FMath::Max(Position.X, 36.0f);
        Position.Y = FMath::Max(Position.Y, 36.0f);
        CanvasSlot->SetPosition(Position);
    });
}

void EnsureMainMenuButtonLabel(
    UWidgetTree& WidgetTree,
    UButton* Button,
    const FText& LabelText,
    const FName LabelName
)
{
    if (!IsValid(Button))
    {
        return;
    }

    UTextBlock* Label = Cast<UTextBlock>(Button->GetContent());

    if (!IsValid(Label))
    {
        Label = WidgetTree.ConstructWidget<UTextBlock>(
            UTextBlock::StaticClass(),
            LabelName
        );
        Button->SetContent(Label);
    }

    Label->SetText(LabelText);
    Label->SetJustification(ETextJustify::Center);
}
}

FText UBHMainMenuWidget::BuildSessionStatusText(
    EBHSessionState State,
    const FText& Message
)
{
    const FText StateHeading = [State]()
    {
        switch (State)
        {
            case EBHSessionState::Hosting:
                return NSLOCTEXT(
                    "BrokenHorizon", "SessionHeadingHosting",
                    "MULTIPLAYER // HOSTING"
                );
            case EBHSessionState::Searching:
                return NSLOCTEXT(
                    "BrokenHorizon", "SessionHeadingSearching",
                    "MULTIPLAYER // SEARCHING"
                );
            case EBHSessionState::Joining:
                return NSLOCTEXT(
                    "BrokenHorizon", "SessionHeadingJoining",
                    "MULTIPLAYER // JOINING"
                );
            case EBHSessionState::Traveling:
                return NSLOCTEXT(
                    "BrokenHorizon", "SessionHeadingConnecting",
                    "MULTIPLAYER // CONNECTING"
                );
            case EBHSessionState::InSession:
                return NSLOCTEXT(
                    "BrokenHorizon", "SessionHeadingConnected",
                    "MULTIPLAYER // CONNECTED"
                );
            case EBHSessionState::Leaving:
                return NSLOCTEXT(
                    "BrokenHorizon", "SessionHeadingLeaving",
                    "MULTIPLAYER // LEAVING"
                );
            case EBHSessionState::Error:
                return NSLOCTEXT(
                    "BrokenHorizon", "SessionHeadingError",
                    "MULTIPLAYER // ACTION FAILED"
                );
            case EBHSessionState::Idle:
            default:
                return NSLOCTEXT(
                    "BrokenHorizon", "SessionHeadingReady",
                    "MULTIPLAYER // READY"
                );
        }
    }();

    return Message.IsEmpty()
        ? StateHeading
        : FText::Format(
            NSLOCTEXT(
                "BrokenHorizon", "SessionStatusFormat",
                "{0}\n{1}"
            ),
            StateHeading,
            Message
        );
}

FLinearColor UBHMainMenuWidget::GetSessionStatusColor(
    EBHSessionState State
)
{
    switch (State)
    {
        case EBHSessionState::Error:
            return FLinearColor(1.0f, 0.28f, 0.20f, 1.0f);
        case EBHSessionState::Hosting:
        case EBHSessionState::Searching:
        case EBHSessionState::Joining:
        case EBHSessionState::Traveling:
        case EBHSessionState::Leaving:
            return FLinearColor(1.0f, 0.78f, 0.20f, 1.0f);
        case EBHSessionState::InSession:
            return FLinearColor(0.28f, 0.92f, 0.52f, 1.0f);
        case EBHSessionState::Idle:
        default:
            return FLinearColor(0.78f, 0.86f, 0.92f, 1.0f);
    }
}

void UBHMainMenuWidget::NativeConstruct()
{
    Super::NativeConstruct();

    EnsureMultiplayerMenuControls();

    if (IsValid(SessionStatusText))
    {
        SessionStatusText->SetAutoWrapText(true);
        SessionStatusText->SetWrapTextAt(520.0f);
    }

    if (IsValid(WidgetTree))
    {
        EnforceMainMenuTextSafeFrame(*WidgetTree);
        EnsureMainMenuButtonLabel(
            *WidgetTree,
            NewGameButton,
            NSLOCTEXT(
                "BrokenHorizon",
                "MenuHostNewCampaign",
                "HOST NEW CAMPAIGN"
            ),
            TEXT("NewGameButtonLabel")
        );
        EnsureMainMenuButtonLabel(
            *WidgetTree,
            ContinueButton,
            NSLOCTEXT(
                "BrokenHorizon",
                "MenuHostContinue",
                "HOST CONTINUE"
            ),
            TEXT("ContinueButtonLabel")
        );
        EnsureMainMenuButtonLabel(
            *WidgetTree,
            JoinCampaignButton,
            NSLOCTEXT(
                "BrokenHorizon",
                "MenuJoinCampaign",
                "JOIN CAMPAIGN"
            ),
            TEXT("JoinCampaignButtonLabel")
        );
        EnsureMainMenuButtonLabel(
            *WidgetTree,
            SettingsButton,
            NSLOCTEXT("BrokenHorizon", "MenuSettings", "SETTINGS"),
            TEXT("SettingsButtonLabel")
        );
        EnsureMainMenuButtonLabel(
            *WidgetTree,
            QuitButton,
            NSLOCTEXT("BrokenHorizon", "MenuQuit", "QUIT TO DESKTOP"),
            TEXT("QuitButtonLabel")
        );
    }

    BHUIStyle::Apply(*this, EBHUIStyleContext::Menu);

    if (IsValid(NewGameButton))
    {
        NewGameButton->OnClicked.AddUniqueDynamic(
            this,
            &UBHMainMenuWidget::HandleNewGameClicked
        );
    }

    if (IsValid(ContinueButton))
    {
        ContinueButton->OnClicked.AddUniqueDynamic(
            this,
            &UBHMainMenuWidget::HandleContinueClicked
        );
    }

    if (IsValid(JoinCampaignButton))
    {
        JoinCampaignButton->OnClicked.AddUniqueDynamic(
            this,
            &UBHMainMenuWidget::HandleJoinCampaignClicked
        );
    }

    if (IsValid(SettingsButton))
    {
        SettingsButton->OnClicked.AddUniqueDynamic(
            this,
            &UBHMainMenuWidget::HandleSettingsClicked
        );
    }

    if (IsValid(QuitButton))
    {
        QuitButton->OnClicked.AddUniqueDynamic(
            this,
            &UBHMainMenuWidget::HandleQuitClicked
        );
    }

    if (UBHSessionSubsystem* SessionSubsystem =
            GetSessionSubsystem())
    {
        SessionSubsystem->OnSessionStateChanged.AddUniqueDynamic(
            this,
            &UBHMainMenuWidget::HandleSessionStateChanged
        );
        HandleSessionStateChanged(
            SessionSubsystem->GetSessionState(),
            NSLOCTEXT(
                "BrokenHorizon",
                "SessionReadyHelp",
                "Host a campaign or join a LAN campaign."
            )
        );
    }

    RefreshContinueState();
}

void UBHMainMenuWidget::NativeDestruct()
{
    if (UBHSessionSubsystem* SessionSubsystem =
            GetSessionSubsystem())
    {
        SessionSubsystem->OnSessionStateChanged.RemoveDynamic(
            this,
            &UBHMainMenuWidget::HandleSessionStateChanged
        );
    }

    Super::NativeDestruct();
}

void UBHMainMenuWidget::FocusControl(
    UWidget* Control,
    const TCHAR* ControlName
)
{
    APlayerController* PlayerController = GetOwningPlayer();
    if (!IsValid(Control) || !Control->GetIsEnabled() ||
        !Control->IsVisible() || !IsValid(PlayerController))
    {
        return;
    }

    Control->SetUserFocus(PlayerController);
    Control->SetKeyboardFocus();
    UE_LOG(
        LogTemp,
        Display,
        TEXT("BH_UI_FOCUS menu=main control=%s"),
        ControlName
    );
}

void UBHMainMenuWidget::FocusInitialControl()
{
    const TPair<UButton*, const TCHAR*> Candidates[] = {
        {NewGameButton, TEXT("new_campaign")},
        {ContinueButton, TEXT("continue")},
        {JoinCampaignButton, TEXT("join")},
        {SettingsButton, TEXT("settings")},
        {QuitButton, TEXT("quit")}
    };
    for (const TPair<UButton*, const TCHAR*>& Candidate : Candidates)
    {
        if (IsValid(Candidate.Key) && Candidate.Key->GetIsEnabled() &&
            Candidate.Key->IsVisible())
        {
            FocusControl(Candidate.Key, Candidate.Value);
            return;
        }
    }
}

void UBHMainMenuWidget::RefreshContinueState()
{
    UGameInstance* GameInstance = GetGameInstance();
    const UBHSaveSubsystem* SaveSubsystem = GameInstance
        ? GameInstance->GetSubsystem<UBHSaveSubsystem>()
        : nullptr;
    const UBHSessionSubsystem* SessionSubsystem = GameInstance
        ? GameInstance->GetSubsystem<UBHSessionSubsystem>()
        : nullptr;

    if (IsValid(ContinueButton))
    {
        ContinueButton->SetIsEnabled(
            IsValid(SaveSubsystem) &&
            SaveSubsystem->HasValidSaveGame() &&
            (!IsValid(SessionSubsystem) ||
                !SessionSubsystem->IsSessionActionPending())
        );
    }
}

void UBHMainMenuWidget::HandleNewGameClicked()
{
    UGameInstance* GameInstance = GetGameInstance();
    UBHSaveSubsystem* SaveSubsystem = GameInstance
        ? GameInstance->GetSubsystem<UBHSaveSubsystem>()
        : nullptr;

    if (IsValid(SaveSubsystem) &&
        !SaveSubsystem->DeleteSaveGame())
    {
        OnMenuActionFailed(
            NSLOCTEXT(
                "BrokenHorizon",
                "NewGameDeleteFailed",
                "The existing checkpoint could not be cleared."
            )
        );
        return;
    }

    UBHSessionSubsystem* SessionSubsystem =
        GetSessionSubsystem();

    if (!IsValid(SessionSubsystem) ||
        !SessionSubsystem->HostCampaign(false))
    {
        ReportSessionActionStartFailure(
            SessionSubsystem,
            NSLOCTEXT(
                "BrokenHorizon",
                "HostNewCampaignFailed",
                "The multiplayer campaign could not be hosted."
            )
        );
    }
}

void UBHMainMenuWidget::HandleContinueClicked()
{
    UGameInstance* GameInstance = GetGameInstance();
    UBHSaveSubsystem* SaveSubsystem = GameInstance
        ? GameInstance->GetSubsystem<UBHSaveSubsystem>()
        : nullptr;

    UBHSessionSubsystem* SessionSubsystem =
        GetSessionSubsystem();

    if (!IsValid(SaveSubsystem) ||
        !SaveSubsystem->HasValidSaveGame() ||
        !IsValid(SessionSubsystem) ||
        !SessionSubsystem->HostCampaign(true))
    {
        RefreshContinueState();
        ReportSessionActionStartFailure(
            SessionSubsystem,
            NSLOCTEXT(
                "BrokenHorizon",
                "ContinueFailed",
                "No valid shared campaign could be hosted."
            )
        );
    }
}

void UBHMainMenuWidget::HandleJoinCampaignClicked()
{
    UBHSessionSubsystem* SessionSubsystem =
        GetSessionSubsystem();

    if (!IsValid(SessionSubsystem) ||
        !SessionSubsystem->FindAndJoinCampaign())
    {
        ReportSessionActionStartFailure(
            SessionSubsystem,
            NSLOCTEXT(
                "BrokenHorizon",
                "JoinCampaignFailed",
                "No shared campaign could be joined."
            )
        );
    }
}

void UBHMainMenuWidget::HandleSettingsClicked()
{
    if (IsValid(SettingsWidget))
    {
        return;
    }

    if (!SettingsWidgetClass)
    {
        OnMenuActionFailed(
            NSLOCTEXT(
                "BrokenHorizon",
                "SettingsWidgetMissing",
                "No settings widget is configured."
            )
        );
        return;
    }

    SettingsWidget = CreateWidget<UBHSettingsWidget>(
        GetOwningPlayer(),
        SettingsWidgetClass
    );

    if (IsValid(SettingsWidget))
    {
        SettingsWidget->OnSettingsClosed.AddDynamic(
            this,
            &UBHMainMenuWidget::HandleSettingsClosed
        );
        SetVisibility(ESlateVisibility::Collapsed);
        SettingsWidget->AddToViewport(20);
        SettingsWidget->FocusInitialControl();
    }
}

void UBHMainMenuWidget::HandleQuitClicked()
{
    UKismetSystemLibrary::QuitGame(
        this,
        GetOwningPlayer(),
        EQuitPreference::Quit,
        false
    );
}

void UBHMainMenuWidget::HandleSettingsClosed()
{
    SettingsWidget = nullptr;
    SetVisibility(ESlateVisibility::Visible);
    RefreshContinueState();
    FocusControl(SettingsButton, TEXT("settings"));
}

void UBHMainMenuWidget::HandleSessionStateChanged(
    EBHSessionState State,
    FText Message
)
{
    const bool bActionPending =
        State == EBHSessionState::Hosting ||
        State == EBHSessionState::Searching ||
        State == EBHSessionState::Joining ||
        State == EBHSessionState::Traveling ||
        State == EBHSessionState::Leaving;

    if (IsValid(NewGameButton))
    {
        NewGameButton->SetIsEnabled(!bActionPending);
    }

    if (IsValid(JoinCampaignButton))
    {
        JoinCampaignButton->SetIsEnabled(!bActionPending);
    }

    if (IsValid(SettingsButton))
    {
        SettingsButton->SetIsEnabled(!bActionPending);
    }

    if (IsValid(SessionStatusText))
    {
        SessionStatusText->SetText(
            BuildSessionStatusText(State, Message)
        );
        SessionStatusText->SetColorAndOpacity(
            GetSessionStatusColor(State)
        );
    }

    RefreshContinueState();

    if (State == EBHSessionState::Error)
    {
        OnMenuActionFailed(Message);
    }
}

#if !UE_BUILD_SHIPPING
void UBHMainMenuWidget::SetSessionStateForRenderedReview(
    EBHSessionState State,
    const FText& Message
)
{
    HandleSessionStateChanged(State, Message);
}
#endif

void UBHMainMenuWidget::EnsureMultiplayerMenuControls()
{
    if (!IsValid(WidgetTree) ||
        IsValid(JoinCampaignButton) ||
        !IsValid(ContinueButton))
    {
        return;
    }

    UPanelWidget* ParentPanel =
        Cast<UPanelWidget>(ContinueButton->GetParent());

    if (!IsValid(ParentPanel))
    {
        return;
    }

    JoinCampaignButton = WidgetTree->ConstructWidget<UButton>(
        UButton::StaticClass(),
        TEXT("JoinCampaignButton")
    );

    if (!IsValid(JoinCampaignButton))
    {
        return;
    }

    const int32 ContinueIndex =
        ParentPanel->GetChildIndex(ContinueButton);
    ParentPanel->InsertChildAt(
        ContinueIndex == INDEX_NONE
            ? ParentPanel->GetChildrenCount()
            : ContinueIndex + 1,
        JoinCampaignButton
    );

    UCanvasPanel* CanvasParent = Cast<UCanvasPanel>(ParentPanel);
    UCanvasPanelSlot* ContinueCanvasSlot =
        Cast<UCanvasPanelSlot>(ContinueButton->Slot);
    if (IsValid(CanvasParent) && IsValid(ContinueCanvasSlot))
    {
        if (UCanvasPanelSlot* JoinCanvasSlot =
                Cast<UCanvasPanelSlot>(JoinCampaignButton->Slot))
        {
            const FVector2D ContinueSize =
                ContinueCanvasSlot->GetSize();
            JoinCanvasSlot->SetAnchors(
                ContinueCanvasSlot->GetAnchors()
            );
            JoinCanvasSlot->SetAlignment(
                ContinueCanvasSlot->GetAlignment()
            );
            JoinCanvasSlot->SetPosition(
                ContinueCanvasSlot->GetPosition() +
                FVector2D(0.0f, ContinueSize.Y + 28.0f)
            );
            JoinCanvasSlot->SetSize(ContinueSize);
            JoinCanvasSlot->SetZOrder(
                ContinueCanvasSlot->GetZOrder()
            );
        }
    }

    if (!IsValid(SessionStatusText))
    {
        UBorder* SessionStatusPanel =
            WidgetTree->ConstructWidget<UBorder>(
                UBorder::StaticClass(),
                TEXT("SessionStatusPanel")
            );
        SessionStatusText = WidgetTree->ConstructWidget<UTextBlock>(
            UTextBlock::StaticClass(),
            TEXT("SessionStatusText")
        );
        SessionStatusText->SetText(
            NSLOCTEXT(
                "BrokenHorizon",
                "SessionReady",
                "MULTIPLAYER // READY"
            )
        );
        SessionStatusText->SetJustification(ETextJustify::Center);
        SessionStatusText->SetAutoWrapText(true);
        SessionStatusText->SetWrapTextAt(520.0f);
        SessionStatusPanel->SetPadding(FMargin(28.0f, 22.0f));
        SessionStatusPanel->SetBrushColor(
            FLinearColor(0.01f, 0.025f, 0.045f, 0.88f)
        );
        SessionStatusPanel->SetContent(SessionStatusText);
        ParentPanel->InsertChildAt(
            ParentPanel->GetChildIndex(JoinCampaignButton) + 1,
            SessionStatusPanel
        );

        if (IsValid(CanvasParent))
        {
            if (UCanvasPanelSlot* StatusCanvasSlot =
                    Cast<UCanvasPanelSlot>(SessionStatusPanel->Slot))
            {
                StatusCanvasSlot->SetAnchors(
                    FAnchors(0.5f, 0.5f)
                );
                StatusCanvasSlot->SetAlignment(FVector2D(0.5f, 0.5f));
                StatusCanvasSlot->SetPosition(FVector2D(275.0f, 0.0f));
                StatusCanvasSlot->SetSize(FVector2D(600.0f, 240.0f));
                StatusCanvasSlot->SetZOrder(10);
            }
        }
    }
}

void UBHMainMenuWidget::ReportSessionActionStartFailure(
    UBHSessionSubsystem* SessionSubsystem,
    const FText& FallbackMessage
)
{
    // FailSessionAction broadcasts Error synchronously. Do not deliver the
    // same Blueprint-facing failure a second time from the click handler.
    if (!IsValid(SessionSubsystem) ||
        SessionSubsystem->GetSessionState() != EBHSessionState::Error)
    {
        OnMenuActionFailed(FallbackMessage);
    }
}

UBHSessionSubsystem* UBHMainMenuWidget::GetSessionSubsystem() const
{
    UGameInstance* GameInstance = GetGameInstance();
    return IsValid(GameInstance)
        ? GameInstance->GetSubsystem<UBHSessionSubsystem>()
        : nullptr;
}
