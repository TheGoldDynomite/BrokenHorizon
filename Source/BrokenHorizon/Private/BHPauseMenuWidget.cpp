#include "BHPauseMenuWidget.h"

#include "BHCharacter.h"
#include "BHSettingsWidget.h"
#include "BHUIStyle.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"

namespace
{
void EnsurePauseMenuBackdrop(UWidgetTree& WidgetTree)
{
    UCanvasPanel* RootCanvas =
        Cast<UCanvasPanel>(WidgetTree.RootWidget);
    if (!IsValid(RootCanvas) ||
        WidgetTree.FindWidget(TEXT("PauseMenuBackdrop")))
    {
        return;
    }

    UBorder* Backdrop = WidgetTree.ConstructWidget<UBorder>(
        UBorder::StaticClass(),
        TEXT("PauseMenuBackdrop")
    );
    Backdrop->SetBrushColor(FLinearColor(0.008f, 0.012f, 0.014f, 0.72f));
    Backdrop->SetVisibility(ESlateVisibility::SelfHitTestInvisible);

    if (UCanvasPanelSlot* BackdropSlot =
            RootCanvas->AddChildToCanvas(Backdrop))
    {
        BackdropSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
        BackdropSlot->SetOffsets(FMargin(0.0f));
        BackdropSlot->SetZOrder(-100);
    }
}

void EnsurePauseMenuButtonLabel(
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

void UBHPauseMenuWidget::InitializePauseMenu(
    ABHCharacter* InCharacter
)
{
    OwningCharacter = InCharacter;
}

#if !UE_BUILD_SHIPPING
bool UBHPauseMenuWidget::OpenSettingsForRenderedReview(
    bool bOpenRemapping
)
{
    HandleSettingsClicked();
    if (!IsValid(SettingsWidget))
    {
        return false;
    }

    return !bOpenRemapping ||
        SettingsWidget->OpenInputRemappingForRenderedReview();
}
#endif

void UBHPauseMenuWidget::NativeConstruct()
{
    Super::NativeConstruct();

    if (IsValid(WidgetTree))
    {
        EnsurePauseMenuBackdrop(*WidgetTree);

        EnsurePauseMenuButtonLabel(
            *WidgetTree,
            ResumeButton,
            NSLOCTEXT("BrokenHorizon", "PauseResume", "RESUME OPERATION"),
            TEXT("ResumeButtonLabel")
        );
        EnsurePauseMenuButtonLabel(
            *WidgetTree,
            RestartCheckpointButton,
            NSLOCTEXT(
                "BrokenHorizon",
                "PauseRestart",
                "RESTART CHECKPOINT"
            ),
            TEXT("RestartCheckpointButtonLabel")
        );
        EnsurePauseMenuButtonLabel(
            *WidgetTree,
            SettingsButton,
            NSLOCTEXT("BrokenHorizon", "PauseSettings", "SETTINGS"),
            TEXT("PauseSettingsButtonLabel")
        );
        EnsurePauseMenuButtonLabel(
            *WidgetTree,
            MainMenuButton,
            NSLOCTEXT(
                "BrokenHorizon",
                "PauseMainMenu",
                "RETURN TO COMMAND"
            ),
            TEXT("MainMenuButtonLabel")
        );
    }

    BHUIStyle::Apply(*this, EBHUIStyleContext::Menu);

    if (IsValid(ResumeButton))
    {
        ResumeButton->OnClicked.AddUniqueDynamic(
            this,
            &UBHPauseMenuWidget::HandleResumeClicked
        );
    }

    if (IsValid(RestartCheckpointButton))
    {
        RestartCheckpointButton->OnClicked.AddUniqueDynamic(
            this,
            &UBHPauseMenuWidget::HandleRestartCheckpointClicked
        );
    }

    if (IsValid(SettingsButton))
    {
        SettingsButton->OnClicked.AddUniqueDynamic(
            this,
            &UBHPauseMenuWidget::HandleSettingsClicked
        );
    }

    if (IsValid(MainMenuButton))
    {
        MainMenuButton->OnClicked.AddUniqueDynamic(
            this,
            &UBHPauseMenuWidget::HandleMainMenuClicked
        );
    }
}

void UBHPauseMenuWidget::FocusControl(
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
        TEXT("BH_UI_FOCUS menu=pause control=%s"),
        ControlName
    );
}

void UBHPauseMenuWidget::FocusInitialControl()
{
    FocusControl(ResumeButton, TEXT("resume"));
}

void UBHPauseMenuWidget::HandleResumeClicked()
{
    if (OwningCharacter.IsValid())
    {
        OwningCharacter->ResumeFromPause();
    }
}

void UBHPauseMenuWidget::HandleRestartCheckpointClicked()
{
    if (!OwningCharacter.IsValid() ||
        !OwningCharacter->RestartCheckpoint())
    {
        OnPauseActionFailed(
            NSLOCTEXT(
                "BrokenHorizon",
                "RestartCheckpointFailed",
                "The checkpoint could not be restarted."
            )
        );
    }
}

void UBHPauseMenuWidget::HandleSettingsClicked()
{
    if (IsValid(SettingsWidget) || !SettingsWidgetClass)
    {
        if (!SettingsWidgetClass)
        {
            OnPauseActionFailed(
                NSLOCTEXT(
                    "BrokenHorizon",
                    "PauseSettingsMissing",
                    "No settings widget is configured."
                )
            );
        }
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
            &UBHPauseMenuWidget::HandleSettingsClosed
        );
        SetVisibility(ESlateVisibility::Collapsed);
        SettingsWidget->AddToViewport(310);
        SettingsWidget->FocusInitialControl();
    }
}

void UBHPauseMenuWidget::HandleMainMenuClicked()
{
    if (!OwningCharacter.IsValid() ||
        !OwningCharacter->ReturnToMainMenu())
    {
        OnPauseActionFailed(
            NSLOCTEXT(
                "BrokenHorizon",
                "MainMenuFailed",
                "Progress could not be saved or the command menu "
                "is unavailable."
            )
        );
    }
}

void UBHPauseMenuWidget::HandleSettingsClosed()
{
    SettingsWidget = nullptr;
    SetVisibility(ESlateVisibility::Visible);
    FocusControl(SettingsButton, TEXT("settings"));
}
