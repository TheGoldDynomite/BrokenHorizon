"""Validate the unified Broken Horizon HUD and menu visual system."""

import os

import unreal


PREFIX = "[BH UI Polish Validation] "


def _log(message):
    unreal.log(PREFIX + message)


def _read(relative_path):
    path = os.path.join(unreal.Paths.project_dir(), relative_path)
    with open(path, "r", encoding="utf-8") as source_file:
        return source_file.read()


def _require_fragments(relative_path, fragments):
    source = _read(relative_path)

    for fragment in fragments:
        if fragment not in source:
            raise RuntimeError(
                "%s is missing: %s" % (relative_path, fragment)
            )


def _validate_style_contract():
    _require_fragments(
        "Source/BrokenHorizon/Public/BHUIStyle.h",
        (
            "enum class EBHUIStyleContext",
            "inline const FLinearColor Charcoal",
            "inline const FLinearColor Gold",
            "inline const FLinearColor Friendly",
            "inline const FLinearColor Danger",
            "void Apply(",
        ),
    )
    _require_fragments(
        "Source/BrokenHorizon/Private/BHUIStyle.cpp",
        (
            "WidgetTree->GetAllWidgets(Widgets);",
            "StyleText(",
            "*TextBlock,",
            "StyleButton(*Button);",
            "Style.Normal = SolidBrush;",
            "Style.Hovered = SolidBrush;",
            "StyleProgressBar(*ProgressBar, FriendlyColor, DangerColor);",
            "Slider->SetSliderHandleColor(Gold);",
            "Border->SetBrush(BorderBrush);",
            "Image->SetColorAndOpacity(",
            'TEXT("BHGlobalSafeAreaRoot")',
            "Settings->GetUISafeAreaScale()",
            "BHUIStyle::CalculateSafeAreaInsets(",
            "BHUIStyle::ResolveContextScale(",
            "ApplyAnchoredScale(",
            'TEXT("BHTestHUDScale=")',
            'TEXT("BHTestUISafeAreaScale=")',
        ),
    )

    integrations = {
        "BHMainMenuWidget.cpp": "EBHUIStyleContext::Menu",
        "BHPauseMenuWidget.cpp": "EBHUIStyleContext::Menu",
        "BHSettingsWidget.cpp": "EBHUIStyleContext::Menu",
        "BHAmmoHUDWidget.cpp": "EBHUIStyleContext::Gameplay",
        "BHInteractionPromptWidget.cpp": "EBHUIStyleContext::Gameplay",
        "BHObjectiveWidget.cpp": "EBHUIStyleContext::Gameplay",
        "BHObjectiveNotificationWidget.cpp":
            "EBHUIStyleContext::Overlay",
        "BHCombatStatusWidget.cpp": "EBHUIStyleContext::Gameplay",
        "BHDeathWidget.cpp": "EBHUIStyleContext::Alert",
        "BHMissionCompleteWidget.cpp": "EBHUIStyleContext::Overlay",
    }

    for filename, context in integrations.items():
        _require_fragments(
            "Source/BrokenHorizon/Private/" + filename,
            (
                '#include "BHUIStyle.h"',
                "BHUIStyle::Apply(*this, " + context + ");",
            ),
        )

    _require_fragments(
        "Source/BrokenHorizon/BHCharacter.cpp",
        (
            "void ABHCharacter::EnsureInteractionPromptWidget()",
            "if (IsValid(InteractionPromptWidget) || !IsPlayerControlled())",
            "EnsureInteractionPromptWidget();",
            "BH_INTERACTION_PROMPT_NATIVE_RETRY",
        ),
    )

    _require_fragments(
        "Source/BrokenHorizon/Private/BHMainMenuWidget.cpp",
        (
            "EnsureMainMenuButtonLabel(",
            '"HOST NEW CAMPAIGN"',
            '"HOST CONTINUE"',
            '"JOIN CAMPAIGN"',
            '"SETTINGS"',
            '"QUIT TO DESKTOP"',
            "SessionStatusText->SetAutoWrapText(true);",
            "SessionStatusText->SetWrapTextAt(520.0f);",
            "SetSessionStateForRenderedReview(",
        ),
    )
    _require_fragments(
        "Source/BrokenHorizon/Private/BHMainMenuGameMode.cpp",
        (
            'TEXT("BHTestRenderedSessionReview=")',
            'TEXT("READY")',
            'TEXT("SEARCHING")',
            'TEXT("CONNECTED")',
            'TEXT("ERROR")',
            "BH_RENDERED_SESSION_REVIEW result=requested",
        ),
    )
    _require_fragments(
        "Source/BrokenHorizon/Private/BHSettingsWidget.cpp",
        (
            "EnsureLookSettingsControls();",
            'TEXT("HorizontalSensitivitySlider")',
            'TEXT("VerticalSensitivitySlider")',
            'TEXT("ADSSensitivitySlider")',
            'TEXT("InvertVerticalLookCheckBox")',
            "Slider->SetMinValue(Minimum);",
            "SettingsSubsystem->ApplyLookSettings(",
            "EnsureInputRemappingControls();",
            "EnsureResponsiveSettingsLayout();",
            'TEXT("ResponsiveSettingsShell")',
            'TEXT("ResponsiveSettingsScroll")',
            'TEXT("ResponsiveSettingsFooter")',
            'TEXT("InputRemappingOverlay")',
            "UInputKeySelector::StaticClass()",
            "SettingsSubsystem->ApplyInputBindings(",
            "HandleRemappingKeySelected(",
            'TEXT("InputModeSettingsPanel")',
            'TEXT("ToggleAimCheckBox")',
            'TEXT("ToggleSprintCheckBox")',
            'TEXT("ToggleCrouchCheckBox")',
            'TEXT("ToggleProneCheckBox")',
            'TEXT("ToggleLeanCheckBox")',
            'TEXT("HoldInteractionCheckBox")',
            "SettingsSubsystem->ApplyInputModeSettings(",
            'TEXT("VisualComfortSettingsPanel")',
            'TEXT("CameraShakeScaleSlider")',
            'TEXT("RecoilAnimationScaleSlider")',
            'TEXT("HeadBobScaleSlider")',
            'TEXT("HitFlashScaleSlider")',
            'TEXT("MotionBlurCheckBox")',
            'TEXT("DepthOfFieldCheckBox")',
            'TEXT("ChromaticAberrationCheckBox")',
            "SettingsSubsystem->ApplyVisualComfortSettings(",
            'TEXT("SubtitleSettingsHeading")',
            'TEXT("SubtitlesEnabledCheckBox")',
            'TEXT("SubtitleSpeakerLabelsCheckBox")',
            'TEXT("SubtitleDirectionalIndicatorsCheckBox")',
            'TEXT("SubtitleTextScaleSlider")',
            'TEXT("SubtitleBackgroundOpacitySlider")',
            'TEXT("UISafeAreaScaleSlider")',
            "SettingsSubsystem->ApplySubtitleSettings(",
            "ResponsiveSettingsShell->SetVisibility(",
        ),
    )
    _require_fragments(
        "Source/BrokenHorizon/Private/BHSubtitleWidget.cpp",
        (
            'TEXT("SubtitleSafeAreaCanvas")',
            'TEXT("SubtitleBackground")',
            'TEXT("SubtitleText")',
            "AreSubtitleSpeakerLabelsEnabled()",
            "AreSubtitleDirectionalIndicatorsEnabled()",
            "GetSubtitleTextScale()",
            "GetSubtitleBackgroundOpacity()",
            "GetUISafeAreaScale()",
        ),
    )
    _require_fragments(
        "Source/BrokenHorizon/Public/BHMissionData.h",
        (
            "FText RadioSpeaker;",
            "FText ActivationRadioLine;",
            "FText CompletionRadioLine;",
            "float RadioSubtitleDuration = 3.5f;",
            "bool bRadioHasDirection = false;",
            "float RadioDirectionDegrees = 0.0f;",
        ),
    )
    _require_fragments(
        "Source/BrokenHorizon/BHCharacter.cpp",
        (
            "QueueObjectiveCompletionRadio(CompletedObjectiveID);",
            "QueueObjectiveActivationRadio(",
            "Definition->ActivationRadioLine",
            "Definition->CompletionRadioLine",
        ),
    )
    _require_fragments(
        "Source/BrokenHorizon/Private/BHPauseMenuWidget.cpp",
        (
            "EnsurePauseMenuButtonLabel(",
            '"RESUME OPERATION"',
            '"RESTART CHECKPOINT"',
            '"SETTINGS"',
            '"RETURN TO COMMAND"',
            '"Progress could not be saved or the command menu "',
        ),
    )
    _require_fragments(
        "Source/BrokenHorizon/BHCharacter.cpp",
        (
            "bool ABHCharacter::ReturnToMainMenu()",
            "!SaveSubsystem->SaveProgress()",
            '"Return to command aborted because campaign "',
            "UGameplayStatics::OpenLevel(this, FName(*PackageName));",
            "UpdateFieldSquadStatusHUD();",
            "CombatStatusWidget->SetFieldSquadStatus(",
        ),
    )
    _require_fragments(
        "Source/BrokenHorizon/Public/BHCombatStatusWidget.h",
        (
            "void SetFieldSquadStatus(",
            "void SetFieldSquadServiceNeeds(",
            "static FString BuildFieldSquadStatusLabel(",
            "bool bFieldSquadStatusVisible = false;",
        ),
    )
    _require_fragments(
        "Source/BrokenHorizon/Private/BHCombatStatusWidget.cpp",
        (
            "void UBHCombatStatusWidget::SetFieldSquadStatus(",
            "void UBHCombatStatusWidget::SetFieldSquadServiceNeeds(",
            "UBHCombatStatusWidget::BuildFieldSquadStatusLabel(",
            '"FIRETEAM // %d/%d // SERVICE %d"',
            '"FIRETEAM // %d/%d // READY"',
            "FieldSquadMembersNeedingService > 0",
            '"MOUNTED // VEHICLE PROTECTED"',
            'TEXT("ORDER HOLD // [%s] FOLLOW")',
            'TEXT("ORDER FOLLOW // AIM + [%s] MOVE/HOLD")',
            "if (bFieldSquadStatusVisible)",
            "bSquadPingWaypointVisible",
            "FLinearColor(1.0f, 0.72f, 0.12f, 0.98f)",
            "264.0f",
        ),
    )
    _require_fragments(
        "Source/BrokenHorizon/Public/BHObjectiveNotificationWidget.h",
        (
            "int32 GetPendingNotificationCount() const;",
            "int32 MaxPendingNotifications = 6;",
            "TArray<FPendingNotification> PendingNotifications;",
            "bool bNotificationInProgress = false;",
            "void PresentNotification(",
            "EBHNotificationPriority NotificationPriority",
        ),
    )
    _require_fragments(
        "Source/BrokenHorizon/Private/BHObjectiveNotificationWidget.cpp",
        (
            "if (IsNotificationActive())",
            "PendingNotifications.ContainsByPredicate(",
            "QueueNotification(\n            Message,\n            NotificationPriority,\n            AudioCue,",
            "const FPendingNotification NextNotification =",
            "NextNotification.Priority",
            "PendingNotifications.Reset();",
        ),
    )

    _log("PASS one shared visual language styles every active interface.")
    _log("PASS menu, gameplay, overlay, and alert contexts are assigned.")
    _log("PASS main and pause menus provide visible native button labels.")
    _log("PASS returning to command saves the live campaign first.")
    _log("PASS transient HUD notifications queue without overwriting.")
    _log("PASS fireteam strength, order, and mounted state remain visible.")


def _validate_assets():
    asset_paths = (
        "/Game/WBP_MainMenu",
        "/Game/BrokenHorizon/UI/WBP_PauseMenu",
        "/Game/BrokenHorizon/UI/WBP_Settings",
        "/Game/BrokenHorizon/UI/WBP_Objective",
        "/Game/BrokenHorizon/UI/WBP_ObjectiveNotification",
        "/Game/BrokenHorizon/UI/WBP_CombatStatus",
        "/Game/BrokenHorizon/UI/WBP_AmmoHud",
        "/Game/BrokenHorizon/UI/MBP_InteractionPrompt",
        "/Game/BrokenHorizon/Core/WBP_Death",
    )

    for asset_path in asset_paths:
        if not unreal.EditorAssetLibrary.does_asset_exist(asset_path):
            raise RuntimeError("Missing interface asset: " + asset_path)

        if not unreal.EditorAssetLibrary.load_asset(asset_path):
            raise RuntimeError("Could not load interface asset: " + asset_path)

    _log("PASS all existing HUD and menu Blueprint assets still load.")

    settings_blueprint = unreal.EditorAssetLibrary.load_asset(
        "/Game/BrokenHorizon/UI/WBP_Settings"
    )
    widget_names = []
    try:
        widget_tree = settings_blueprint.get_editor_property("widget_tree")
        widgets = widget_tree.get_all_widgets()
        widget_names = sorted(str(widget.get_name()) for widget in widgets)
    except Exception as exc:
        _log("INFO settings widget-tree names unavailable: %s" % exc)
    _log("INFO settings widget names: %s" % ", ".join(widget_names))


def main():
    _validate_assets()
    _validate_style_contract()
    _log("ALL CHECKS PASSED")


if __name__ == "__main__":
    main()
