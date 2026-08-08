#include "BHSettingsWidget.h"

#include "BHUserSettingsSubsystem.h"
#include "BHUIStyle.h"
#include "Components/Button.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/ComboBoxString.h"
#include "Components/CheckBox.h"
#include "Components/Slider.h"
#include "Components/HorizontalBox.h"
#include "Components/InputKeySelector.h"
#include "Components/PanelWidget.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Blueprint/WidgetTree.h"
#include "Engine/GameInstance.h"
#include "GameFramework/PlayerController.h"

namespace
{
UTextBlock* AddSettingsLabel(
    UWidgetTree& Tree,
    UVerticalBox& Container,
    const FName Name,
    const FText& Text
)
{
    UTextBlock* Label = Tree.ConstructWidget<UTextBlock>(
        UTextBlock::StaticClass(),
        Name
    );
    if (IsValid(Label))
    {
        Label->SetText(Text);
        Container.AddChildToVerticalBox(Label);
    }
    return Label;
}

UButton* AddSettingsButton(
    UWidgetTree& Tree,
    UPanelWidget& Container,
    const FName ButtonName,
    const FName LabelName,
    const FText& LabelText
)
{
    UButton* Button = Tree.ConstructWidget<UButton>(
        UButton::StaticClass(),
        ButtonName
    );
    UTextBlock* Label = Tree.ConstructWidget<UTextBlock>(
        UTextBlock::StaticClass(),
        LabelName
    );
    if (IsValid(Button) && IsValid(Label))
    {
        Label->SetText(LabelText);
        Label->SetJustification(ETextJustify::Center);
        Button->AddChild(Label);
        Container.AddChild(Button);
    }
    return Button;
}

UCheckBox* AddSettingsCheckBox(
    UWidgetTree& Tree,
    UVerticalBox& Container,
    const FName CheckBoxName,
    const FName LabelName,
    const FText& LabelText
)
{
    AddSettingsLabel(Tree, Container, LabelName, LabelText);
    UCheckBox* CheckBox = Tree.ConstructWidget<UCheckBox>(
        UCheckBox::StaticClass(),
        CheckBoxName
    );
    if (IsValid(CheckBox))
    {
        Container.AddChildToVerticalBox(CheckBox);
    }
    return CheckBox;
}

void AddResponsiveSettingsControl(
    UWidgetTree& Tree,
    UVerticalBox& Container,
    UWidget* Control,
    const FName LabelName,
    const FText& LabelText
)
{
    if (!IsValid(Control))
    {
        return;
    }

    AddSettingsLabel(Tree, Container, LabelName, LabelText);
    Control->RemoveFromParent();
    Container.AddChildToVerticalBox(Control);
}

UButton* EnsureResponsiveSettingsAction(
    UWidgetTree& Tree,
    TObjectPtr<UButton>& Button,
    const FName ButtonName,
    const FName LabelName,
    const FText& LabelText
)
{
    if (!IsValid(Button))
    {
        Button = Tree.ConstructWidget<UButton>(
            UButton::StaticClass(),
            ButtonName
        );
    }
    if (!IsValid(Button))
    {
        return nullptr;
    }

    UTextBlock* Label = Cast<UTextBlock>(Button->GetContent());
    if (!IsValid(Label))
    {
        Label = Tree.ConstructWidget<UTextBlock>(
            UTextBlock::StaticClass(),
            LabelName
        );
        Button->SetContent(Label);
    }
    Label->SetText(LabelText);
    Label->SetJustification(ETextJustify::Center);
    return Button;
}
}

void UBHSettingsWidget::NativeConstruct()
{
    Super::NativeConstruct();
    EnsureLookSettingsControls();
    EnsureInputRemappingControls();
    if (!IsValid(InputPromptModeComboBox) && IsValid(WidgetTree))
    {
        InputPromptModeComboBox =
            WidgetTree->ConstructWidget<UComboBoxString>(
                UComboBoxString::StaticClass(),
                TEXT("InputPromptModeComboBox")
            );
    }
    EnsureResponsiveSettingsLayout();
    BHUIStyle::Apply(*this, EBHUIStyleContext::Menu);

    PopulateOptions();
    LoadSavedValues();

    if (IsValid(ApplyButton))
    {
        ApplyButton->OnClicked.AddUniqueDynamic(
            this,
            &UBHSettingsWidget::HandleApplyClicked
        );
    }

    if (IsValid(CancelButton))
    {
        CancelButton->OnClicked.AddUniqueDynamic(
            this,
            &UBHSettingsWidget::HandleCancelClicked
        );
    }

    if (IsValid(DefaultsButton))
    {
        DefaultsButton->OnClicked.AddUniqueDynamic(
            this,
            &UBHSettingsWidget::HandleDefaultsClicked
        );
    }

    if (IsValid(BackButton))
    {
        BackButton->OnClicked.AddUniqueDynamic(
            this,
            &UBHSettingsWidget::HandleBackClicked
        );
    }
    if (IsValid(OpenRemappingButton))
    {
        OpenRemappingButton->OnClicked.AddUniqueDynamic(
            this,
            &UBHSettingsWidget::HandleOpenRemappingClicked
        );
    }
    if (IsValid(CloseRemappingButton))
    {
        CloseRemappingButton->OnClicked.AddUniqueDynamic(
            this,
            &UBHSettingsWidget::HandleCloseRemappingClicked
        );
    }
    if (IsValid(ResetRemappingButton))
    {
        ResetRemappingButton->OnClicked.AddUniqueDynamic(
            this,
            &UBHSettingsWidget::HandleResetRemappingClicked
        );
    }
}

FReply UBHSettingsWidget::NativeOnKeyDown(
    const FGeometry& InGeometry,
    const FKeyEvent& InKeyEvent
)
{
    const FKey Key = InKeyEvent.GetKey();
    if (Key == EKeys::Escape ||
        Key == EKeys::Virtual_Gamepad_Back.GetVirtualKey())
    {
        if (IsValid(InputRemappingOverlay) &&
            InputRemappingOverlay->GetVisibility() ==
                ESlateVisibility::Visible)
        {
            HandleCloseRemappingClicked();
            UE_LOG(
                LogTemp,
                Display,
                TEXT("BH_UI_BACK menu=remapping action=close")
            );
        }
        else
        {
            UE_LOG(
                LogTemp,
                Display,
                TEXT("BH_UI_BACK menu=settings action=close")
            );
            CloseWithoutApplying();
        }
        return FReply::Handled();
    }

    return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void UBHSettingsWidget::FocusControl(
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
        TEXT("BH_UI_FOCUS menu=settings control=%s"),
        ControlName
    );
}

void UBHSettingsWidget::FocusInitialControl()
{
    if (IsValid(MouseSensitivitySlider))
    {
        FocusControl(MouseSensitivitySlider, TEXT("mouse_sensitivity"));
        return;
    }
    FocusControl(OpenRemappingButton, TEXT("input_bindings"));
}

void UBHSettingsWidget::FocusInitialRemappingControl()
{
    if (IsValid(ToggleAimCheckBox))
    {
        FocusControl(ToggleAimCheckBox, TEXT("toggle_aim"));
        return;
    }

    for (const FInputSelectorEntry& Entry : InputSelectorEntries)
    {
        if (UInputKeySelector* Selector = Entry.Selector.Get())
        {
            FocusControl(Selector, TEXT("first_binding"));
            return;
        }
    }
    FocusControl(CloseRemappingButton, TEXT("close_bindings"));
}

#if !UE_BUILD_SHIPPING
bool UBHSettingsWidget::OpenInputRemappingForRenderedReview()
{
    if (!IsValid(InputRemappingOverlay))
    {
        return false;
    }

    HandleOpenRemappingClicked();
    return InputRemappingOverlay->GetVisibility() ==
        ESlateVisibility::Visible;
}
#endif

void UBHSettingsWidget::ApplyCurrentSettings()
{
    UGameInstance* GameInstance = GetGameInstance();
    UBHUserSettingsSubsystem* SettingsSubsystem = GameInstance
        ? GameInstance->GetSubsystem<UBHUserSettingsSubsystem>()
        : nullptr;

    if (!IsValid(SettingsSubsystem))
    {
        OnSettingsApplied(false);
        return;
    }

    const float MouseSensitivity =
        IsValid(MouseSensitivitySlider)
        ? static_cast<float>(MouseSensitivitySlider->GetValue())
        : SettingsSubsystem->GetMouseSensitivity();
    const float MasterVolume =
        IsValid(MasterVolumeSlider)
        ? static_cast<float>(MasterVolumeSlider->GetValue())
        : SettingsSubsystem->GetMasterVolume();
    const int32 WindowModeIndex =
        IsValid(WindowModeComboBox)
        ? WindowModeComboBox->GetSelectedIndex()
        : SettingsSubsystem->GetWindowModeIndex();
    const int32 GraphicsQuality =
        IsValid(GraphicsQualityComboBox)
        ? GraphicsQualityComboBox->GetSelectedIndex()
        : SettingsSubsystem->GetGraphicsQuality();
    const EBHInputPromptMode InputPromptMode =
        IsValid(InputPromptModeComboBox)
        ? static_cast<EBHInputPromptMode>(FMath::Clamp(
            InputPromptModeComboBox->GetSelectedIndex(),
            0,
            3
        ))
        : SettingsSubsystem->GetInputPromptMode();

    const float HUDScale = IsValid(HUDScaleSlider)
        ? static_cast<float>(HUDScaleSlider->GetValue())
        : SettingsSubsystem->GetHUDScale();
    const EBHColorVisionMode ColorVisionMode = IsValid(ColorVisionModeComboBox)
        ? static_cast<EBHColorVisionMode>(FMath::Max(0, ColorVisionModeComboBox->GetSelectedIndex()))
        : SettingsSubsystem->GetColorVisionMode();
    const bool bHighContrast = IsValid(HighContrastHUDCheckBox)
        ? HighContrastHUDCheckBox->IsChecked()
        : SettingsSubsystem->IsHighContrastHUDEnabled();
    const bool bReducedMotion = IsValid(ReducedMotionCheckBox)
        ? ReducedMotionCheckBox->IsChecked()
        : SettingsSubsystem->IsReducedMotionEnabled();
    const float HorizontalSensitivity =
        IsValid(HorizontalSensitivitySlider)
        ? static_cast<float>(HorizontalSensitivitySlider->GetValue())
        : (IsValid(MouseSensitivitySlider)
            ? MouseSensitivity
            : SettingsSubsystem->GetHorizontalLookSensitivity());
    const float VerticalSensitivity =
        IsValid(VerticalSensitivitySlider)
        ? static_cast<float>(VerticalSensitivitySlider->GetValue())
        : (IsValid(MouseSensitivitySlider)
            ? MouseSensitivity
            : SettingsSubsystem->GetVerticalLookSensitivity());
    const float ADSSensitivity = IsValid(ADSSensitivitySlider)
        ? static_cast<float>(ADSSensitivitySlider->GetValue())
        : SettingsSubsystem->GetADSSensitivityMultiplier();
    const bool bInvertVertical = IsValid(InvertVerticalLookCheckBox)
        ? InvertVerticalLookCheckBox->IsChecked()
        : SettingsSubsystem->IsVerticalLookInverted();

    const bool bApplied = SettingsSubsystem->ApplySettings(
        MouseSensitivity,
        MasterVolume,
        WindowModeIndex,
        GraphicsQuality
    );
    const bool bAccessibilityApplied = SettingsSubsystem->ApplyAccessibilitySettings(
        HUDScale,
        ColorVisionMode,
        bHighContrast,
        bReducedMotion
    );
    const bool bLookApplied = SettingsSubsystem->ApplyLookSettings(
        HorizontalSensitivity,
        VerticalSensitivity,
        ADSSensitivity,
        bInvertVertical
    );
    TMap<FName, FKey> KeyboardBindings;
    TMap<FName, FKey> GamepadBindings;
    for (const FInputSelectorEntry& Entry : InputSelectorEntries)
    {
        if (const UInputKeySelector* Selector = Entry.Selector.Get())
        {
            (Entry.bGamepad ? GamepadBindings : KeyboardBindings).Add(
                Entry.BindingID,
                Selector->GetSelectedKey().Key
            );
        }
    }
    const bool bBindingsApplied = SettingsSubsystem->ApplyInputBindings(
        KeyboardBindings,
        GamepadBindings
    );
    const bool bPromptModeApplied =
        SettingsSubsystem->ApplyInputPromptMode(InputPromptMode);
    const bool bInputModesApplied =
        SettingsSubsystem->ApplyInputModeSettings(
            IsValid(ToggleAimCheckBox) && ToggleAimCheckBox->IsChecked(),
            IsValid(ToggleSprintCheckBox) && ToggleSprintCheckBox->IsChecked(),
            IsValid(ToggleCrouchCheckBox) && ToggleCrouchCheckBox->IsChecked(),
            !IsValid(ToggleProneCheckBox) || ToggleProneCheckBox->IsChecked(),
            IsValid(ToggleLeanCheckBox) && ToggleLeanCheckBox->IsChecked(),
            IsValid(HoldInteractionCheckBox) && HoldInteractionCheckBox->IsChecked()
        );
    const bool bVisualComfortApplied =
        SettingsSubsystem->ApplyVisualComfortSettings(
            IsValid(CameraShakeScaleSlider) ? CameraShakeScaleSlider->GetValue() : SettingsSubsystem->GetCameraShakeScale(),
            IsValid(RecoilAnimationScaleSlider) ? RecoilAnimationScaleSlider->GetValue() : SettingsSubsystem->GetRecoilAnimationScale(),
            IsValid(HeadBobScaleSlider) ? HeadBobScaleSlider->GetValue() : SettingsSubsystem->GetHeadBobScale(),
            IsValid(HitFlashScaleSlider) ? HitFlashScaleSlider->GetValue() : SettingsSubsystem->GetHitFlashScale(),
            !IsValid(MotionBlurCheckBox) || MotionBlurCheckBox->IsChecked(),
            !IsValid(DepthOfFieldCheckBox) || DepthOfFieldCheckBox->IsChecked(),
            !IsValid(ChromaticAberrationCheckBox) || ChromaticAberrationCheckBox->IsChecked()
        );
    const bool bSubtitleSettingsApplied = SettingsSubsystem->ApplySubtitleSettings(
        !IsValid(SubtitlesEnabledCheckBox) || SubtitlesEnabledCheckBox->IsChecked(),
        !IsValid(SubtitleSpeakerLabelsCheckBox) || SubtitleSpeakerLabelsCheckBox->IsChecked(),
        !IsValid(SubtitleDirectionalIndicatorsCheckBox) || SubtitleDirectionalIndicatorsCheckBox->IsChecked(),
        IsValid(SubtitleTextScaleSlider) ? SubtitleTextScaleSlider->GetValue() : SettingsSubsystem->GetSubtitleTextScale(),
        IsValid(SubtitleBackgroundOpacitySlider) ? SubtitleBackgroundOpacitySlider->GetValue() : SettingsSubsystem->GetSubtitleBackgroundOpacity(),
        IsValid(UISafeAreaScaleSlider) ? UISafeAreaScaleSlider->GetValue() : SettingsSubsystem->GetUISafeAreaScale()
    );
    OnSettingsApplied(
        bApplied && bAccessibilityApplied && bLookApplied &&
        bBindingsApplied && bPromptModeApplied && bInputModesApplied &&
        bVisualComfortApplied &&
        bSubtitleSettingsApplied
    );
}

void UBHSettingsWidget::LoadDefaults()
{
    if (IsValid(MouseSensitivitySlider))
    {
        MouseSensitivitySlider->SetValue(
            UBHUserSettingsSubsystem::DefaultMouseSensitivity
        );
    }

    if (IsValid(HorizontalSensitivitySlider))
    {
        HorizontalSensitivitySlider->SetValue(
            UBHUserSettingsSubsystem::DefaultMouseSensitivity
        );
    }
    if (IsValid(VerticalSensitivitySlider))
    {
        VerticalSensitivitySlider->SetValue(
            UBHUserSettingsSubsystem::DefaultMouseSensitivity
        );
    }
    if (IsValid(ADSSensitivitySlider))
    {
        ADSSensitivitySlider->SetValue(
            UBHUserSettingsSubsystem::DefaultADSSensitivityMultiplier
        );
    }
    if (IsValid(InvertVerticalLookCheckBox))
    {
        InvertVerticalLookCheckBox->SetIsChecked(false);
    }
    if (IsValid(ToggleAimCheckBox)) ToggleAimCheckBox->SetIsChecked(false);
    if (IsValid(ToggleSprintCheckBox)) ToggleSprintCheckBox->SetIsChecked(false);
    if (IsValid(ToggleCrouchCheckBox)) ToggleCrouchCheckBox->SetIsChecked(false);
    if (IsValid(ToggleProneCheckBox)) ToggleProneCheckBox->SetIsChecked(true);
    if (IsValid(ToggleLeanCheckBox)) ToggleLeanCheckBox->SetIsChecked(false);
    if (IsValid(HoldInteractionCheckBox)) HoldInteractionCheckBox->SetIsChecked(false);
    if (IsValid(CameraShakeScaleSlider)) CameraShakeScaleSlider->SetValue(1.0f);
    if (IsValid(RecoilAnimationScaleSlider)) RecoilAnimationScaleSlider->SetValue(1.0f);
    if (IsValid(HeadBobScaleSlider)) HeadBobScaleSlider->SetValue(1.0f);
    if (IsValid(HitFlashScaleSlider)) HitFlashScaleSlider->SetValue(1.0f);
    if (IsValid(MotionBlurCheckBox)) MotionBlurCheckBox->SetIsChecked(true);
    if (IsValid(DepthOfFieldCheckBox)) DepthOfFieldCheckBox->SetIsChecked(true);
    if (IsValid(ChromaticAberrationCheckBox)) ChromaticAberrationCheckBox->SetIsChecked(true);
    if (IsValid(SubtitlesEnabledCheckBox)) SubtitlesEnabledCheckBox->SetIsChecked(true);
    if (IsValid(SubtitleSpeakerLabelsCheckBox)) SubtitleSpeakerLabelsCheckBox->SetIsChecked(true);
    if (IsValid(SubtitleDirectionalIndicatorsCheckBox)) SubtitleDirectionalIndicatorsCheckBox->SetIsChecked(true);
    if (IsValid(SubtitleTextScaleSlider)) SubtitleTextScaleSlider->SetValue(1.0f);
    if (IsValid(SubtitleBackgroundOpacitySlider)) SubtitleBackgroundOpacitySlider->SetValue(0.75f);
    if (IsValid(UISafeAreaScaleSlider)) UISafeAreaScaleSlider->SetValue(0.95f);
    if (IsValid(InputPromptModeComboBox))
        InputPromptModeComboBox->SetSelectedIndex(0);

    if (IsValid(MasterVolumeSlider))
    {
        MasterVolumeSlider->SetValue(
            UBHUserSettingsSubsystem::DefaultMasterVolume
        );
    }

    if (IsValid(WindowModeComboBox))
    {
        WindowModeComboBox->SetSelectedIndex(
            UBHUserSettingsSubsystem::DefaultWindowModeIndex
        );
    }

    if (IsValid(GraphicsQualityComboBox))
    {
        GraphicsQualityComboBox->SetSelectedIndex(
            UBHUserSettingsSubsystem::DefaultGraphicsQuality
        );
    }

    if (IsValid(HUDScaleSlider))
    {
        HUDScaleSlider->SetValue(UBHUserSettingsSubsystem::DefaultHUDScale);
    }
    if (IsValid(ColorVisionModeComboBox))
    {
        ColorVisionModeComboBox->SetSelectedIndex(0);
    }
    if (IsValid(HighContrastHUDCheckBox))
    {
        HighContrastHUDCheckBox->SetIsChecked(false);
    }
    if (IsValid(ReducedMotionCheckBox))
    {
        ReducedMotionCheckBox->SetIsChecked(false);
    }
    LoadInputBindingValues(true);
}

void UBHSettingsWidget::CloseWithoutApplying()
{
    LoadSavedValues();
    OnSettingsClosed.Broadcast();
    RemoveFromParent();
}

void UBHSettingsWidget::HandleApplyClicked()
{
    ApplyCurrentSettings();
}

void UBHSettingsWidget::HandleCancelClicked()
{
    CloseWithoutApplying();
}

void UBHSettingsWidget::HandleDefaultsClicked()
{
    LoadDefaults();
}

void UBHSettingsWidget::HandleBackClicked()
{
    CloseWithoutApplying();
}

void UBHSettingsWidget::HandleOpenRemappingClicked()
{
    if (IsValid(InputRemappingOverlay))
    {
        if (IsValid(ResponsiveSettingsShell))
        {
            ResponsiveSettingsShell->SetVisibility(
                ESlateVisibility::Collapsed
            );
        }
        InputRemappingOverlay->SetVisibility(ESlateVisibility::Visible);
        FocusInitialRemappingControl();
    }
}

void UBHSettingsWidget::HandleCloseRemappingClicked()
{
    if (IsValid(InputRemappingOverlay))
    {
        InputRemappingOverlay->SetVisibility(ESlateVisibility::Collapsed);
        if (IsValid(ResponsiveSettingsShell))
        {
            ResponsiveSettingsShell->SetVisibility(
                ESlateVisibility::Visible
            );
        }
        FocusControl(OpenRemappingButton, TEXT("input_bindings"));
    }
}

void UBHSettingsWidget::HandleResetRemappingClicked()
{
    LoadInputBindingValues(true);
}

void UBHSettingsWidget::HandleRemappingKeySelected(FInputChord SelectedKey)
{
    for (FInputSelectorEntry& Entry : InputSelectorEntries)
    {
        UInputKeySelector* Selector = Entry.Selector.Get();
        if (!IsValid(Selector) || Selector->GetSelectedKey() == Entry.LastChord)
        {
            continue;
        }

        if (!SelectedKey.Key.IsValid() ||
            SelectedKey.Key.IsGamepadKey() != Entry.bGamepad)
        {
            Selector->SetSelectedKey(Entry.LastChord);
            return;
        }

        const FInputChord PreviousChord = Entry.LastChord;
        Entry.LastChord = SelectedKey;
        for (FInputSelectorEntry& Conflict : InputSelectorEntries)
        {
            UInputKeySelector* ConflictSelector = Conflict.Selector.Get();
            if (&Conflict != &Entry &&
                Conflict.bGamepad == Entry.bGamepad &&
                IsValid(ConflictSelector) &&
                ConflictSelector->GetSelectedKey().Key == SelectedKey.Key)
            {
                Conflict.LastChord = PreviousChord;
                ConflictSelector->SetSelectedKey(PreviousChord);
                break;
            }
        }
        return;
    }
}

void UBHSettingsWidget::PopulateOptions()
{
    if (IsValid(WindowModeComboBox) &&
        WindowModeComboBox->GetOptionCount() == 0)
    {
        WindowModeComboBox->AddOption(NSLOCTEXT("BrokenHorizon", "WindowModeWindowed", "Windowed").ToString());
        WindowModeComboBox->AddOption(NSLOCTEXT("BrokenHorizon", "WindowModeBorderless", "Windowed Fullscreen").ToString());
        WindowModeComboBox->AddOption(NSLOCTEXT("BrokenHorizon", "WindowModeFullscreen", "Fullscreen").ToString());
    }

    if (IsValid(GraphicsQualityComboBox) &&
        GraphicsQualityComboBox->GetOptionCount() == 0)
    {
        GraphicsQualityComboBox->AddOption(NSLOCTEXT("BrokenHorizon", "GraphicsQualityLow", "Low").ToString());
        GraphicsQualityComboBox->AddOption(NSLOCTEXT("BrokenHorizon", "GraphicsQualityMedium", "Medium").ToString());
        GraphicsQualityComboBox->AddOption(NSLOCTEXT("BrokenHorizon", "GraphicsQualityHigh", "High").ToString());
        GraphicsQualityComboBox->AddOption(NSLOCTEXT("BrokenHorizon", "GraphicsQualityEpic", "Epic").ToString());
    }

    if (IsValid(ColorVisionModeComboBox) && ColorVisionModeComboBox->GetOptionCount() == 0)
    {
        ColorVisionModeComboBox->AddOption(NSLOCTEXT("BrokenHorizon", "ColorVisionStandard", "Standard").ToString());
        ColorVisionModeComboBox->AddOption(NSLOCTEXT("BrokenHorizon", "ColorVisionDeuteranopia", "Deuteranopia").ToString());
        ColorVisionModeComboBox->AddOption(NSLOCTEXT("BrokenHorizon", "ColorVisionProtanopia", "Protanopia").ToString());
        ColorVisionModeComboBox->AddOption(NSLOCTEXT("BrokenHorizon", "ColorVisionTritanopia", "Tritanopia").ToString());
    }

    if (IsValid(InputPromptModeComboBox) &&
        InputPromptModeComboBox->GetOptionCount() == 0)
    {
        InputPromptModeComboBox->AddOption(NSLOCTEXT("BrokenHorizon", "InputPromptAuto", "Auto (Last Input)").ToString());
        InputPromptModeComboBox->AddOption(NSLOCTEXT("BrokenHorizon", "InputPromptKeyboardMouse", "Keyboard + Mouse").ToString());
        InputPromptModeComboBox->AddOption(NSLOCTEXT("BrokenHorizon", "InputPromptController", "Controller").ToString());
        InputPromptModeComboBox->AddOption(NSLOCTEXT("BrokenHorizon", "InputPromptBoth", "Show Both").ToString());
    }
}

void UBHSettingsWidget::LoadSavedValues()
{
    UGameInstance* GameInstance = GetGameInstance();
    UBHUserSettingsSubsystem* SettingsSubsystem = GameInstance
        ? GameInstance->GetSubsystem<UBHUserSettingsSubsystem>()
        : nullptr;

    if (!IsValid(SettingsSubsystem))
    {
        return;
    }

    if (IsValid(MouseSensitivitySlider))
    {
        MouseSensitivitySlider->SetValue(
            SettingsSubsystem->GetMouseSensitivity()
        );
    }

    if (IsValid(HorizontalSensitivitySlider))
    {
        HorizontalSensitivitySlider->SetValue(
            SettingsSubsystem->GetHorizontalLookSensitivity()
        );
    }
    if (IsValid(VerticalSensitivitySlider))
    {
        VerticalSensitivitySlider->SetValue(
            SettingsSubsystem->GetVerticalLookSensitivity()
        );
    }
    if (IsValid(ADSSensitivitySlider))
    {
        ADSSensitivitySlider->SetValue(
            SettingsSubsystem->GetADSSensitivityMultiplier()
        );
    }
    if (IsValid(InvertVerticalLookCheckBox))
    {
        InvertVerticalLookCheckBox->SetIsChecked(
            SettingsSubsystem->IsVerticalLookInverted()
        );
    }
    if (IsValid(ToggleAimCheckBox))
        ToggleAimCheckBox->SetIsChecked(SettingsSubsystem->IsToggleAimEnabled());
    if (IsValid(ToggleSprintCheckBox))
        ToggleSprintCheckBox->SetIsChecked(SettingsSubsystem->IsToggleSprintEnabled());
    if (IsValid(ToggleCrouchCheckBox))
        ToggleCrouchCheckBox->SetIsChecked(SettingsSubsystem->IsToggleCrouchEnabled());
    if (IsValid(ToggleProneCheckBox))
        ToggleProneCheckBox->SetIsChecked(SettingsSubsystem->IsToggleProneEnabled());
    if (IsValid(ToggleLeanCheckBox))
        ToggleLeanCheckBox->SetIsChecked(SettingsSubsystem->IsToggleLeanEnabled());
    if (IsValid(HoldInteractionCheckBox))
        HoldInteractionCheckBox->SetIsChecked(SettingsSubsystem->IsHoldInteractionEnabled());
    if (IsValid(CameraShakeScaleSlider))
        CameraShakeScaleSlider->SetValue(SettingsSubsystem->GetCameraShakeScale());
    if (IsValid(RecoilAnimationScaleSlider))
        RecoilAnimationScaleSlider->SetValue(SettingsSubsystem->GetRecoilAnimationScale());
    if (IsValid(HeadBobScaleSlider))
        HeadBobScaleSlider->SetValue(SettingsSubsystem->GetHeadBobScale());
    if (IsValid(HitFlashScaleSlider))
        HitFlashScaleSlider->SetValue(SettingsSubsystem->GetHitFlashScale());
    if (IsValid(MotionBlurCheckBox))
        MotionBlurCheckBox->SetIsChecked(SettingsSubsystem->IsMotionBlurEnabled());
    if (IsValid(DepthOfFieldCheckBox))
        DepthOfFieldCheckBox->SetIsChecked(SettingsSubsystem->IsDepthOfFieldEnabled());
    if (IsValid(ChromaticAberrationCheckBox))
        ChromaticAberrationCheckBox->SetIsChecked(SettingsSubsystem->IsChromaticAberrationEnabled());
    if (IsValid(SubtitlesEnabledCheckBox))
        SubtitlesEnabledCheckBox->SetIsChecked(SettingsSubsystem->AreSubtitlesEnabled());
    if (IsValid(SubtitleSpeakerLabelsCheckBox))
        SubtitleSpeakerLabelsCheckBox->SetIsChecked(SettingsSubsystem->AreSubtitleSpeakerLabelsEnabled());
    if (IsValid(SubtitleDirectionalIndicatorsCheckBox))
        SubtitleDirectionalIndicatorsCheckBox->SetIsChecked(SettingsSubsystem->AreSubtitleDirectionalIndicatorsEnabled());
    if (IsValid(SubtitleTextScaleSlider))
        SubtitleTextScaleSlider->SetValue(SettingsSubsystem->GetSubtitleTextScale());
    if (IsValid(SubtitleBackgroundOpacitySlider))
        SubtitleBackgroundOpacitySlider->SetValue(SettingsSubsystem->GetSubtitleBackgroundOpacity());
    if (IsValid(UISafeAreaScaleSlider))
        UISafeAreaScaleSlider->SetValue(SettingsSubsystem->GetUISafeAreaScale());

    if (IsValid(MasterVolumeSlider))
    {
        MasterVolumeSlider->SetValue(
            SettingsSubsystem->GetMasterVolume()
        );
    }

    if (IsValid(WindowModeComboBox))
    {
        WindowModeComboBox->SetSelectedIndex(
            SettingsSubsystem->GetWindowModeIndex()
        );
    }

    if (IsValid(GraphicsQualityComboBox))
    {
        GraphicsQualityComboBox->SetSelectedIndex(
            SettingsSubsystem->GetGraphicsQuality()
        );
    }

    if (IsValid(InputPromptModeComboBox))
    {
        InputPromptModeComboBox->SetSelectedIndex(
            static_cast<int32>(SettingsSubsystem->GetInputPromptMode())
        );
    }

    if (IsValid(HUDScaleSlider))
    {
        HUDScaleSlider->SetValue(SettingsSubsystem->GetHUDScale());
    }
    if (IsValid(ColorVisionModeComboBox))
    {
        ColorVisionModeComboBox->SetSelectedIndex(static_cast<int32>(SettingsSubsystem->GetColorVisionMode()));
    }
    if (IsValid(HighContrastHUDCheckBox))
    {
        HighContrastHUDCheckBox->SetIsChecked(SettingsSubsystem->IsHighContrastHUDEnabled());
    }
    if (IsValid(ReducedMotionCheckBox))
    {
        ReducedMotionCheckBox->SetIsChecked(SettingsSubsystem->IsReducedMotionEnabled());
    }
    LoadInputBindingValues(false);

}

void UBHSettingsWidget::LoadInputBindingValues(bool bUseDefaults)
{
    UGameInstance* GameInstance = GetGameInstance();
    const UBHUserSettingsSubsystem* SettingsSubsystem = GameInstance
        ? GameInstance->GetSubsystem<UBHUserSettingsSubsystem>()
        : nullptr;
    const TArray<FBHInputBindingDefinition>& Definitions =
        UBHUserSettingsSubsystem::GetDefaultInputBindingDefinitions();

    for (FInputSelectorEntry& Entry : InputSelectorEntries)
    {
        UInputKeySelector* Selector = Entry.Selector.Get();
        const FBHInputBindingDefinition* Definition =
            Definitions.FindByPredicate(
                [&Entry](const FBHInputBindingDefinition& Candidate)
                {
                    return Candidate.BindingID == Entry.BindingID;
                }
            );
        if (!IsValid(Selector) || !Definition)
        {
            continue;
        }
        const FKey Key = bUseDefaults || !IsValid(SettingsSubsystem)
            ? (Entry.bGamepad
                ? Definition->DefaultGamepadKey
                : Definition->DefaultKeyboardKey)
            : SettingsSubsystem->GetInputBinding(
                Entry.BindingID,
                Entry.bGamepad
            );
        Entry.LastChord = FInputChord(Key);
        Selector->SetSelectedKey(Entry.LastChord);
    }
}

void UBHSettingsWidget::EnsureLookSettingsControls()
{
    if (IsValid(HorizontalSensitivitySlider) &&
        IsValid(VerticalSensitivitySlider) &&
        IsValid(ADSSensitivitySlider) &&
        IsValid(InvertVerticalLookCheckBox))
    {
        return;
    }

    if (!IsValid(WidgetTree))
    {
        return;
    }

    UCanvasPanel* Canvas = Cast<UCanvasPanel>(WidgetTree->RootWidget);
    if (!IsValid(Canvas))
    {
        TArray<UWidget*> Widgets;
        WidgetTree->GetAllWidgets(Widgets);
        for (UWidget* Widget : Widgets)
        {
            Canvas = Cast<UCanvasPanel>(Widget);
            if (IsValid(Canvas))
            {
                break;
            }
        }
    }

    if (!IsValid(Canvas))
    {
        UE_LOG(
            LogTemp,
            Warning,
            TEXT("BH settings could not create look controls: no canvas panel")
        );
        return;
    }

    UVerticalBox* LookPanel = WidgetTree->ConstructWidget<UVerticalBox>(
        UVerticalBox::StaticClass(),
        TEXT("NativeLookSettingsPanel")
    );
    if (!IsValid(LookPanel))
    {
        return;
    }

    UCanvasPanelSlot* PanelSlot = Canvas->AddChildToCanvas(LookPanel);
    PanelSlot->SetAnchors(FAnchors(1.0f, 0.0f));
    PanelSlot->SetAlignment(FVector2D(1.0f, 0.0f));
    PanelSlot->SetPosition(FVector2D(-48.0f, 112.0f));
    PanelSlot->SetSize(FVector2D(380.0f, 360.0f));

    AddSettingsLabel(
        *WidgetTree,
        *LookPanel,
        TEXT("NativeLookSettingsHeading"),
        NSLOCTEXT("BrokenHorizon", "LookSettingsHeading", "LOOK CONTROLS")
    );

    auto AddSensitivitySlider = [this, LookPanel](
        TObjectPtr<USlider>& Slider,
        const FName SliderName,
        const FName LabelName,
        const FText& LabelText,
        const float Minimum,
        const float Maximum
    )
    {
        AddSettingsLabel(*WidgetTree, *LookPanel, LabelName, LabelText);
        Slider = WidgetTree->ConstructWidget<USlider>(
            USlider::StaticClass(),
            SliderName
        );
        if (IsValid(Slider))
        {
            Slider->SetMinValue(Minimum);
            Slider->SetMaxValue(Maximum);
            Slider->SetStepSize(0.05f);
            LookPanel->AddChildToVerticalBox(Slider);
        }
    };

    if (!IsValid(HorizontalSensitivitySlider))
    {
        AddSensitivitySlider(
            HorizontalSensitivitySlider,
            TEXT("HorizontalSensitivitySlider"),
            TEXT("HorizontalSensitivityLabel"),
            NSLOCTEXT("BrokenHorizon", "HorizontalSensitivity", "HORIZONTAL SENSITIVITY"),
            0.1f,
            5.0f
        );
    }
    if (!IsValid(VerticalSensitivitySlider))
    {
        AddSensitivitySlider(
            VerticalSensitivitySlider,
            TEXT("VerticalSensitivitySlider"),
            TEXT("VerticalSensitivityLabel"),
            NSLOCTEXT("BrokenHorizon", "VerticalSensitivity", "VERTICAL SENSITIVITY"),
            0.1f,
            5.0f
        );
    }
    if (!IsValid(ADSSensitivitySlider))
    {
        AddSensitivitySlider(
            ADSSensitivitySlider,
            TEXT("ADSSensitivitySlider"),
            TEXT("ADSSensitivityLabel"),
            NSLOCTEXT("BrokenHorizon", "ADSSensitivity", "ADS SENSITIVITY"),
            0.1f,
            1.5f
        );
    }
    if (!IsValid(InvertVerticalLookCheckBox))
    {
        AddSettingsLabel(
            *WidgetTree,
            *LookPanel,
            TEXT("InvertVerticalLookLabel"),
            NSLOCTEXT("BrokenHorizon", "InvertVerticalLook", "INVERT VERTICAL LOOK")
        );
        InvertVerticalLookCheckBox = WidgetTree->ConstructWidget<UCheckBox>(
            UCheckBox::StaticClass(),
            TEXT("InvertVerticalLookCheckBox")
        );
        if (IsValid(InvertVerticalLookCheckBox))
        {
            LookPanel->AddChildToVerticalBox(InvertVerticalLookCheckBox);
        }
    }
}

void UBHSettingsWidget::EnsureInputRemappingControls()
{
    if (!IsValid(WidgetTree) || IsValid(InputRemappingOverlay))
    {
        return;
    }

    UCanvasPanel* Canvas = Cast<UCanvasPanel>(WidgetTree->RootWidget);
    if (!IsValid(Canvas))
    {
        TArray<UWidget*> Widgets;
        WidgetTree->GetAllWidgets(Widgets);
        for (UWidget* Widget : Widgets)
        {
            Canvas = Cast<UCanvasPanel>(Widget);
            if (IsValid(Canvas)) break;
        }
    }
    if (!IsValid(Canvas))
    {
        return;
    }

    UVerticalBox* LaunchPanel = WidgetTree->ConstructWidget<UVerticalBox>(
        UVerticalBox::StaticClass(),
        TEXT("NativeRemapLaunchPanel")
    );
    UCanvasPanelSlot* LaunchSlot = Canvas->AddChildToCanvas(LaunchPanel);
    LaunchSlot->SetAnchors(FAnchors(1.0f, 1.0f));
    LaunchSlot->SetAlignment(FVector2D(1.0f, 1.0f));
    LaunchSlot->SetPosition(FVector2D(-48.0f, -48.0f));
    LaunchSlot->SetSize(FVector2D(380.0f, 56.0f));
    OpenRemappingButton = AddSettingsButton(
        *WidgetTree,
        *LaunchPanel,
        TEXT("OpenRemappingButton"),
        TEXT("OpenRemappingButtonLabel"),
        NSLOCTEXT("BrokenHorizon", "OpenRemapping", "REMAP CONTROLS")
    );

    InputRemappingOverlay = WidgetTree->ConstructWidget<UBorder>(
        UBorder::StaticClass(),
        TEXT("InputRemappingOverlay")
    );
    InputRemappingOverlay->SetBrushColor(
        FLinearColor(0.015f, 0.02f, 0.025f, 0.98f)
    );
    InputRemappingOverlay->SetVisibility(ESlateVisibility::Collapsed);
    UCanvasPanelSlot* OverlaySlot = Canvas->AddChildToCanvas(
        InputRemappingOverlay
    );
    OverlaySlot->SetAnchors(FAnchors(0.04f, 0.04f, 0.96f, 0.96f));
    OverlaySlot->SetAlignment(FVector2D(0.5f, 0.5f));
    OverlaySlot->SetOffsets(FMargin(0.0f));
    OverlaySlot->SetZOrder(300);
    InputRemappingOverlay->SetPadding(FMargin(28.0f, 20.0f));

    UVerticalBox* OverlayContents =
        WidgetTree->ConstructWidget<UVerticalBox>(
            UVerticalBox::StaticClass(),
            TEXT("InputRemappingContents")
        );
    InputRemappingOverlay->AddChild(OverlayContents);
    UTextBlock* RemappingInstructions = AddSettingsLabel(
        *WidgetTree,
        *OverlayContents,
        TEXT("InputRemappingHeading"),
        NSLOCTEXT(
            "BrokenHorizon",
            "InputRemappingHeading",
            "KEYBOARD + CONTROLLER REMAPPING"
        )
    );
    AddSettingsLabel(
        *WidgetTree,
        *OverlayContents,
        TEXT("InputRemappingInstructions"),
        NSLOCTEXT(
            "BrokenHorizon",
            "InputRemappingInstructions",
            "Select a field, then press the replacement input. Conflicts are rejected until every action has a unique input."
        )
    );
    if (IsValid(RemappingInstructions))
    {
        RemappingInstructions->SetAutoWrapText(true);
    }

    UScrollBox* Scroll = WidgetTree->ConstructWidget<UScrollBox>(
        UScrollBox::StaticClass(),
        TEXT("InputRemappingScroll")
    );
    if (UVerticalBoxSlot* ScrollSlot =
            OverlayContents->AddChildToVerticalBox(Scroll))
    {
        ScrollSlot->SetSize(
            FSlateChildSize(ESlateSizeRule::Fill)
        );
        ScrollSlot->SetPadding(FMargin(0.0f, 10.0f));
    }

    UVerticalBox* ModePanel = WidgetTree->ConstructWidget<UVerticalBox>(
        UVerticalBox::StaticClass(),
        TEXT("InputModeSettingsPanel")
    );
    Scroll->AddChild(ModePanel);
    AddSettingsLabel(
        *WidgetTree,
        *ModePanel,
        TEXT("InputModeSettingsHeading"),
        NSLOCTEXT("BrokenHorizon", "InputModeSettingsHeading", "TOGGLE + HOLD MODES")
    );
    ToggleAimCheckBox = AddSettingsCheckBox(
        *WidgetTree, *ModePanel, TEXT("ToggleAimCheckBox"),
        TEXT("ToggleAimLabel"), NSLOCTEXT("BrokenHorizon", "ToggleAim", "TOGGLE AIM")
    );
    ToggleSprintCheckBox = AddSettingsCheckBox(
        *WidgetTree, *ModePanel, TEXT("ToggleSprintCheckBox"),
        TEXT("ToggleSprintLabel"), NSLOCTEXT("BrokenHorizon", "ToggleSprint", "TOGGLE SPRINT")
    );
    ToggleCrouchCheckBox = AddSettingsCheckBox(
        *WidgetTree, *ModePanel, TEXT("ToggleCrouchCheckBox"),
        TEXT("ToggleCrouchLabel"), NSLOCTEXT("BrokenHorizon", "ToggleCrouch", "TOGGLE CROUCH")
    );
    ToggleProneCheckBox = AddSettingsCheckBox(
        *WidgetTree, *ModePanel, TEXT("ToggleProneCheckBox"),
        TEXT("ToggleProneLabel"), NSLOCTEXT("BrokenHorizon", "ToggleProne", "TOGGLE PRONE")
    );
    ToggleLeanCheckBox = AddSettingsCheckBox(
        *WidgetTree, *ModePanel, TEXT("ToggleLeanCheckBox"),
        TEXT("ToggleLeanLabel"), NSLOCTEXT("BrokenHorizon", "ToggleLean", "TOGGLE LEAN")
    );
    HoldInteractionCheckBox = AddSettingsCheckBox(
        *WidgetTree, *ModePanel, TEXT("HoldInteractionCheckBox"),
        TEXT("HoldInteractionLabel"), NSLOCTEXT("BrokenHorizon", "HoldInteraction", "HOLD TO INTERACT")
    );

    UVerticalBox* ComfortPanel = WidgetTree->ConstructWidget<UVerticalBox>(
        UVerticalBox::StaticClass(), TEXT("VisualComfortSettingsPanel")
    );
    Scroll->AddChild(ComfortPanel);
    AddSettingsLabel(
        *WidgetTree, *ComfortPanel, TEXT("VisualComfortSettingsHeading"),
        NSLOCTEXT("BrokenHorizon", "VisualComfortSettingsHeading", "CAMERA + VISUAL COMFORT")
    );
    const auto AddComfortSlider = [this, ComfortPanel](
        const TCHAR* LabelName,
        const TCHAR* SliderName,
        const FText& LabelText
    ) -> USlider*
    {
        AddSettingsLabel(*WidgetTree, *ComfortPanel, FName(LabelName), LabelText);
        USlider* Slider = WidgetTree->ConstructWidget<USlider>(
            USlider::StaticClass(), FName(SliderName)
        );
        Slider->SetMinValue(0.0f);
        Slider->SetMaxValue(1.0f);
        Slider->SetStepSize(0.05f);
        ComfortPanel->AddChildToVerticalBox(Slider);
        return Slider;
    };
    CameraShakeScaleSlider = AddComfortSlider(
        TEXT("CameraShakeScaleLabel"), TEXT("CameraShakeScaleSlider"),
        NSLOCTEXT("BrokenHorizon", "CameraShakeScale", "CAMERA SHAKE")
    );
    RecoilAnimationScaleSlider = AddComfortSlider(
        TEXT("RecoilAnimationScaleLabel"), TEXT("RecoilAnimationScaleSlider"),
        NSLOCTEXT("BrokenHorizon", "RecoilAnimationScale", "RECOIL MOTION")
    );
    HeadBobScaleSlider = AddComfortSlider(
        TEXT("HeadBobScaleLabel"), TEXT("HeadBobScaleSlider"),
        NSLOCTEXT("BrokenHorizon", "HeadBobScale", "HEAD BOB")
    );
    HitFlashScaleSlider = AddComfortSlider(
        TEXT("HitFlashScaleLabel"), TEXT("HitFlashScaleSlider"),
        NSLOCTEXT("BrokenHorizon", "HitFlashScale", "DAMAGE FLASH")
    );
    MotionBlurCheckBox = AddSettingsCheckBox(
        *WidgetTree, *ComfortPanel, TEXT("MotionBlurCheckBox"),
        TEXT("MotionBlurLabel"), NSLOCTEXT("BrokenHorizon", "MotionBlur", "MOTION BLUR")
    );
    DepthOfFieldCheckBox = AddSettingsCheckBox(
        *WidgetTree, *ComfortPanel, TEXT("DepthOfFieldCheckBox"),
        TEXT("DepthOfFieldLabel"), NSLOCTEXT("BrokenHorizon", "DepthOfField", "DEPTH OF FIELD")
    );
    ChromaticAberrationCheckBox = AddSettingsCheckBox(
        *WidgetTree, *ComfortPanel, TEXT("ChromaticAberrationCheckBox"),
        TEXT("ChromaticAberrationLabel"), NSLOCTEXT("BrokenHorizon", "ChromaticAberration", "CHROMATIC ABERRATION")
    );

    AddSettingsLabel(
        *WidgetTree, *ComfortPanel, TEXT("SubtitleSettingsHeading"),
        NSLOCTEXT("BrokenHorizon", "SubtitleSettingsHeading", "SUBTITLES + SAFE AREA")
    );
    SubtitlesEnabledCheckBox = AddSettingsCheckBox(
        *WidgetTree, *ComfortPanel, TEXT("SubtitlesEnabledCheckBox"),
        TEXT("SubtitlesEnabledLabel"), NSLOCTEXT("BrokenHorizon", "SubtitlesEnabled", "SUBTITLES")
    );
    SubtitleSpeakerLabelsCheckBox = AddSettingsCheckBox(
        *WidgetTree, *ComfortPanel, TEXT("SubtitleSpeakerLabelsCheckBox"),
        TEXT("SubtitleSpeakerLabelsLabel"), NSLOCTEXT("BrokenHorizon", "SubtitleSpeakerLabels", "SPEAKER LABELS")
    );
    SubtitleDirectionalIndicatorsCheckBox = AddSettingsCheckBox(
        *WidgetTree, *ComfortPanel, TEXT("SubtitleDirectionalIndicatorsCheckBox"),
        TEXT("SubtitleDirectionalIndicatorsLabel"), NSLOCTEXT("BrokenHorizon", "SubtitleDirectionalIndicators", "DIRECTION INDICATORS")
    );
    SubtitleTextScaleSlider = AddComfortSlider(
        TEXT("SubtitleTextScaleLabel"), TEXT("SubtitleTextScaleSlider"),
        NSLOCTEXT("BrokenHorizon", "SubtitleTextScale", "SUBTITLE SIZE")
    );
    SubtitleTextScaleSlider->SetMinValue(0.75f);
    SubtitleTextScaleSlider->SetMaxValue(2.0f);
    SubtitleBackgroundOpacitySlider = AddComfortSlider(
        TEXT("SubtitleBackgroundOpacityLabel"), TEXT("SubtitleBackgroundOpacitySlider"),
        NSLOCTEXT("BrokenHorizon", "SubtitleBackgroundOpacity", "SUBTITLE BACKGROUND")
    );
    UISafeAreaScaleSlider = AddComfortSlider(
        TEXT("UISafeAreaScaleLabel"), TEXT("UISafeAreaScaleSlider"),
        NSLOCTEXT("BrokenHorizon", "UISafeAreaScale", "UI SAFE AREA")
    );
    UISafeAreaScaleSlider->SetMinValue(0.8f);

    UGameInstance* GameInstance = GetGameInstance();
    const UBHUserSettingsSubsystem* SettingsSubsystem = GameInstance
        ? GameInstance->GetSubsystem<UBHUserSettingsSubsystem>()
        : nullptr;
    const TArray<FBHInputBindingDefinition>& Definitions =
        UBHUserSettingsSubsystem::GetDefaultInputBindingDefinitions();
    for (const FBHInputBindingDefinition& Definition : Definitions)
    {
        UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(
            UHorizontalBox::StaticClass(),
            *FString::Printf(TEXT("RemapRow_%s"), *Definition.BindingID.ToString())
        );
        Scroll->AddChild(Row);

        UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>(
            UTextBlock::StaticClass(),
            *FString::Printf(TEXT("RemapLabel_%s"), *Definition.BindingID.ToString())
        );
        Label->SetText(Definition.DisplayName);
        Row->AddChildToHorizontalBox(Label);

        const TPair<FKey, bool> Devices[] = {
            {Definition.DefaultKeyboardKey, false},
            {Definition.DefaultGamepadKey, true}
        };
        for (const TPair<FKey, bool>& Device : Devices)
        {
            if (!Device.Key.IsValid())
            {
                continue;
            }
            UInputKeySelector* Selector =
                WidgetTree->ConstructWidget<UInputKeySelector>(
                    UInputKeySelector::StaticClass(),
                    *FString::Printf(
                        TEXT("%s_%sSelector"),
                        *Definition.BindingID.ToString(),
                        Device.Value ? TEXT("Gamepad") : TEXT("Keyboard")
                    )
                );
            Selector->SetAllowGamepadKeys(Device.Value);
            const FKey CurrentKey = IsValid(SettingsSubsystem)
                ? SettingsSubsystem->GetInputBinding(
                    Definition.BindingID,
                    Device.Value
                )
                : Device.Key;
            Selector->SetSelectedKey(FInputChord(CurrentKey));
            Selector->OnKeySelected.AddUniqueDynamic(
                this,
                &UBHSettingsWidget::HandleRemappingKeySelected
            );
            Row->AddChildToHorizontalBox(Selector);

            FInputSelectorEntry Entry;
            Entry.Selector = Selector;
            Entry.BindingID = Definition.BindingID;
            Entry.bGamepad = Device.Value;
            Entry.LastChord = FInputChord(CurrentKey);
            InputSelectorEntries.Add(Entry);
        }
    }

    UHorizontalBox* Footer = WidgetTree->ConstructWidget<UHorizontalBox>(
        UHorizontalBox::StaticClass(),
        TEXT("InputRemappingFooter")
    );
    OverlayContents->AddChildToVerticalBox(Footer);
    ResetRemappingButton = AddSettingsButton(
        *WidgetTree,
        *Footer,
        TEXT("ResetRemappingButton"),
        TEXT("ResetRemappingButtonLabel"),
        NSLOCTEXT("BrokenHorizon", "ResetRemapping", "RESET BINDINGS")
    );
    CloseRemappingButton = AddSettingsButton(
        *WidgetTree,
        *Footer,
        TEXT("CloseRemappingButton"),
        TEXT("CloseRemappingButtonLabel"),
        NSLOCTEXT("BrokenHorizon", "CloseRemapping", "BACK TO SETTINGS")
    );
}

void UBHSettingsWidget::EnsureResponsiveSettingsLayout()
{
    if (!IsValid(WidgetTree) || IsValid(ResponsiveSettingsShell))
    {
        return;
    }

    UCanvasPanel* Canvas = Cast<UCanvasPanel>(WidgetTree->RootWidget);
    if (!IsValid(Canvas))
    {
        return;
    }

    ResponsiveSettingsShell = WidgetTree->ConstructWidget<UBorder>(
        UBorder::StaticClass(),
        TEXT("ResponsiveSettingsShell")
    );
    ResponsiveSettingsShell->SetBrushColor(
        FLinearColor(0.012f, 0.018f, 0.021f, 0.985f)
    );
    ResponsiveSettingsShell->SetPadding(FMargin(72.0f, 20.0f));
    UCanvasPanelSlot* ShellSlot = Canvas->AddChildToCanvas(
        ResponsiveSettingsShell
    );
    ShellSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
    ShellSlot->SetOffsets(FMargin(0.0f));
    ShellSlot->SetZOrder(200);

    UVerticalBox* ShellContents =
        WidgetTree->ConstructWidget<UVerticalBox>(
            UVerticalBox::StaticClass(),
            TEXT("ResponsiveSettingsContents")
        );
    ResponsiveSettingsShell->AddChild(ShellContents);
    AddSettingsLabel(
        *WidgetTree,
        *ShellContents,
        TEXT("ResponsiveSettingsHeading"),
        NSLOCTEXT("BrokenHorizon", "ResponsiveSettingsHeading", "SETTINGS")
    );

    UScrollBox* SettingsScroll = WidgetTree->ConstructWidget<UScrollBox>(
        UScrollBox::StaticClass(),
        TEXT("ResponsiveSettingsScroll")
    );
    if (UVerticalBoxSlot* SettingsScrollSlot =
            ShellContents->AddChildToVerticalBox(SettingsScroll))
    {
        SettingsScrollSlot->SetSize(
            FSlateChildSize(ESlateSizeRule::Fill)
        );
        SettingsScrollSlot->SetPadding(FMargin(0.0f, 8.0f));
    }

    UVerticalBox* SettingsList =
        WidgetTree->ConstructWidget<UVerticalBox>(
            UVerticalBox::StaticClass(),
            TEXT("ResponsiveSettingsList")
        );
    SettingsScroll->AddChild(SettingsList);

    AddSettingsLabel(
        *WidgetTree,
        *SettingsList,
        TEXT("ResponsiveLookHeading"),
        NSLOCTEXT("BrokenHorizon", "ResponsiveLookHeading", "LOOK CONTROLS")
    );
    AddResponsiveSettingsControl(
        *WidgetTree, *SettingsList, MouseSensitivitySlider,
        TEXT("ResponsiveMouseSensitivityLabel"),
        NSLOCTEXT("BrokenHorizon", "ResponsiveMouseSensitivity", "MOUSE SENSITIVITY")
    );
    AddResponsiveSettingsControl(
        *WidgetTree, *SettingsList, HorizontalSensitivitySlider,
        TEXT("ResponsiveHorizontalSensitivityLabel"),
        NSLOCTEXT("BrokenHorizon", "ResponsiveHorizontalSensitivity", "HORIZONTAL SENSITIVITY")
    );
    AddResponsiveSettingsControl(
        *WidgetTree, *SettingsList, VerticalSensitivitySlider,
        TEXT("ResponsiveVerticalSensitivityLabel"),
        NSLOCTEXT("BrokenHorizon", "ResponsiveVerticalSensitivity", "VERTICAL SENSITIVITY")
    );
    AddResponsiveSettingsControl(
        *WidgetTree, *SettingsList, ADSSensitivitySlider,
        TEXT("ResponsiveADSSensitivityLabel"),
        NSLOCTEXT("BrokenHorizon", "ResponsiveADSSensitivity", "ADS SENSITIVITY")
    );
    AddResponsiveSettingsControl(
        *WidgetTree, *SettingsList, InvertVerticalLookCheckBox,
        TEXT("ResponsiveInvertVerticalLookLabel"),
        NSLOCTEXT("BrokenHorizon", "ResponsiveInvertVerticalLook", "INVERT VERTICAL LOOK")
    );

    AddSettingsLabel(
        *WidgetTree,
        *SettingsList,
        TEXT("ResponsiveDisplayHeading"),
        NSLOCTEXT("BrokenHorizon", "ResponsiveDisplayHeading", "AUDIO, DISPLAY + ACCESSIBILITY")
    );
    AddResponsiveSettingsControl(
        *WidgetTree, *SettingsList, MasterVolumeSlider,
        TEXT("ResponsiveMasterVolumeLabel"),
        NSLOCTEXT("BrokenHorizon", "ResponsiveMasterVolume", "MASTER VOLUME")
    );
    AddResponsiveSettingsControl(
        *WidgetTree, *SettingsList, WindowModeComboBox,
        TEXT("ResponsiveWindowModeLabel"),
        NSLOCTEXT("BrokenHorizon", "ResponsiveWindowMode", "WINDOW MODE")
    );
    AddResponsiveSettingsControl(
        *WidgetTree, *SettingsList, GraphicsQualityComboBox,
        TEXT("ResponsiveGraphicsQualityLabel"),
        NSLOCTEXT("BrokenHorizon", "ResponsiveGraphicsQuality", "GRAPHICS QUALITY")
    );
    AddResponsiveSettingsControl(
        *WidgetTree, *SettingsList, InputPromptModeComboBox,
        TEXT("ResponsiveInputPromptModeLabel"),
        NSLOCTEXT(
            "BrokenHorizon",
            "ResponsiveInputPromptMode",
            "INPUT PROMPTS"
        )
    );
    AddResponsiveSettingsControl(
        *WidgetTree, *SettingsList, HUDScaleSlider,
        TEXT("ResponsiveHUDScaleLabel"),
        NSLOCTEXT("BrokenHorizon", "ResponsiveHUDScale", "HUD SCALE")
    );
    AddResponsiveSettingsControl(
        *WidgetTree, *SettingsList, ColorVisionModeComboBox,
        TEXT("ResponsiveColorVisionLabel"),
        NSLOCTEXT("BrokenHorizon", "ResponsiveColorVision", "COLOR VISION MODE")
    );
    AddResponsiveSettingsControl(
        *WidgetTree, *SettingsList, HighContrastHUDCheckBox,
        TEXT("ResponsiveHighContrastLabel"),
        NSLOCTEXT("BrokenHorizon", "ResponsiveHighContrast", "HIGH-CONTRAST HUD")
    );
    AddResponsiveSettingsControl(
        *WidgetTree, *SettingsList, ReducedMotionCheckBox,
        TEXT("ResponsiveReducedMotionLabel"),
        NSLOCTEXT("BrokenHorizon", "ResponsiveReducedMotion", "REDUCED MOTION")
    );

    if (IsValid(OpenRemappingButton))
    {
        OpenRemappingButton->RemoveFromParent();
        SettingsList->AddChildToVerticalBox(OpenRemappingButton);
    }

    UHorizontalBox* Footer = WidgetTree->ConstructWidget<UHorizontalBox>(
        UHorizontalBox::StaticClass(),
        TEXT("ResponsiveSettingsFooter")
    );
    ShellContents->AddChildToVerticalBox(Footer);
    EnsureResponsiveSettingsAction(
        *WidgetTree,
        ApplyButton,
        TEXT("ResponsiveApplyButton"),
        TEXT("ResponsiveApplyButtonLabel"),
        NSLOCTEXT("BrokenHorizon", "ResponsiveApply", "APPLY")
    );
    EnsureResponsiveSettingsAction(
        *WidgetTree,
        DefaultsButton,
        TEXT("ResponsiveDefaultsButton"),
        TEXT("ResponsiveDefaultsButtonLabel"),
        NSLOCTEXT("BrokenHorizon", "ResponsiveDefaults", "RESTORE DEFAULTS")
    );
    EnsureResponsiveSettingsAction(
        *WidgetTree,
        BackButton,
        TEXT("ResponsiveBackButton"),
        TEXT("ResponsiveBackButtonLabel"),
        NSLOCTEXT("BrokenHorizon", "ResponsiveBack", "BACK")
    );
    UButton* FooterButtons[] = {
        ApplyButton,
        DefaultsButton,
        CancelButton,
        BackButton
    };
    for (UButton* Button : FooterButtons)
    {
        if (IsValid(Button))
        {
            Button->RemoveFromParent();
            Footer->AddChildToHorizontalBox(Button);
        }
    }

    const FName SupersededPanelNames[] = {
        TEXT("NativeLookSettingsPanel"),
        TEXT("NativeRemapLaunchPanel")
    };
    for (const FName PanelName : SupersededPanelNames)
    {
        if (UWidget* SupersededPanel = WidgetTree->FindWidget(PanelName))
        {
            SupersededPanel->SetVisibility(ESlateVisibility::Collapsed);
        }
    }
}
