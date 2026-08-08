#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "InputCoreTypes.h"
#include "BHSettingsWidget.generated.h"

class UButton;
class UComboBoxString;
class USlider;
class UCheckBox;
class UBorder;
class UInputKeySelector;
class UWidget;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FBHOnSettingsClosed);

UCLASS()
class BROKENHORIZON_API UBHSettingsWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    UPROPERTY(BlueprintAssignable, Category = "Settings")
    FBHOnSettingsClosed OnSettingsClosed;

    UFUNCTION(BlueprintCallable, Category = "Settings")
    void ApplyCurrentSettings();

    UFUNCTION(BlueprintCallable, Category = "Settings")
    void LoadDefaults();

    UFUNCTION(BlueprintCallable, Category = "Settings")
    void CloseWithoutApplying();

    void FocusInitialControl();

#if !UE_BUILD_SHIPPING
    bool OpenInputRemappingForRenderedReview();
#endif

protected:
    virtual void NativeConstruct() override;

    virtual FReply NativeOnKeyDown(
        const FGeometry& InGeometry,
        const FKeyEvent& InKeyEvent
    ) override;

    UFUNCTION()
    void HandleApplyClicked();

    UFUNCTION()
    void HandleCancelClicked();

    UFUNCTION()
    void HandleDefaultsClicked();

    UFUNCTION()
    void HandleBackClicked();

    UFUNCTION()
    void HandleOpenRemappingClicked();

    UFUNCTION()
    void HandleCloseRemappingClicked();

    UFUNCTION()
    void HandleResetRemappingClicked();

    UFUNCTION()
    void HandleRemappingKeySelected(FInputChord SelectedKey);

    UFUNCTION(BlueprintImplementableEvent, Category = "Settings")
    void OnSettingsApplied(bool bSucceeded);

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<USlider> MouseSensitivitySlider;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<USlider> HorizontalSensitivitySlider;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<USlider> VerticalSensitivitySlider;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<USlider> ADSSensitivitySlider;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UCheckBox> InvertVerticalLookCheckBox;

    UPROPERTY(Transient)
    TObjectPtr<UCheckBox> ToggleAimCheckBox;

    UPROPERTY(Transient)
    TObjectPtr<UCheckBox> ToggleSprintCheckBox;

    UPROPERTY(Transient)
    TObjectPtr<UCheckBox> ToggleCrouchCheckBox;

    UPROPERTY(Transient)
    TObjectPtr<UCheckBox> ToggleProneCheckBox;

    UPROPERTY(Transient)
    TObjectPtr<UCheckBox> ToggleLeanCheckBox;

    UPROPERTY(Transient)
    TObjectPtr<UCheckBox> HoldInteractionCheckBox;

    UPROPERTY(Transient)
    TObjectPtr<USlider> CameraShakeScaleSlider;

    UPROPERTY(Transient)
    TObjectPtr<USlider> RecoilAnimationScaleSlider;

    UPROPERTY(Transient)
    TObjectPtr<USlider> HeadBobScaleSlider;

    UPROPERTY(Transient)
    TObjectPtr<USlider> HitFlashScaleSlider;

    UPROPERTY(Transient)
    TObjectPtr<UCheckBox> MotionBlurCheckBox;

    UPROPERTY(Transient)
    TObjectPtr<UCheckBox> DepthOfFieldCheckBox;

    UPROPERTY(Transient)
    TObjectPtr<UCheckBox> ChromaticAberrationCheckBox;

    UPROPERTY(Transient)
    TObjectPtr<UCheckBox> SubtitlesEnabledCheckBox;

    UPROPERTY(Transient)
    TObjectPtr<UCheckBox> SubtitleSpeakerLabelsCheckBox;

    UPROPERTY(Transient)
    TObjectPtr<UCheckBox> SubtitleDirectionalIndicatorsCheckBox;

    UPROPERTY(Transient)
    TObjectPtr<USlider> SubtitleTextScaleSlider;

    UPROPERTY(Transient)
    TObjectPtr<USlider> SubtitleBackgroundOpacitySlider;

    UPROPERTY(Transient)
    TObjectPtr<USlider> UISafeAreaScaleSlider;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<USlider> MasterVolumeSlider;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UComboBoxString> WindowModeComboBox;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UComboBoxString> GraphicsQualityComboBox;

    UPROPERTY(Transient)
    TObjectPtr<UComboBoxString> InputPromptModeComboBox;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<USlider> HUDScaleSlider;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UComboBoxString> ColorVisionModeComboBox;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UCheckBox> HighContrastHUDCheckBox;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UCheckBox> ReducedMotionCheckBox;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> ApplyButton;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> CancelButton;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> DefaultsButton;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> BackButton;

private:
    void FocusControl(UWidget* Control, const TCHAR* ControlName);
    void FocusInitialRemappingControl();

    struct FInputSelectorEntry
    {
        TWeakObjectPtr<UInputKeySelector> Selector;
        FName BindingID = NAME_None;
        bool bGamepad = false;
        FInputChord LastChord;
    };

    void EnsureLookSettingsControls();
    void EnsureInputRemappingControls();
    void EnsureResponsiveSettingsLayout();
    void LoadInputBindingValues(bool bUseDefaults = false);
    void PopulateOptions();
    void LoadSavedValues();

    UPROPERTY(Transient)
    TObjectPtr<UBorder> InputRemappingOverlay;

    UPROPERTY(Transient)
    TObjectPtr<UBorder> ResponsiveSettingsShell;

    UPROPERTY(Transient)
    TObjectPtr<UButton> OpenRemappingButton;

    UPROPERTY(Transient)
    TObjectPtr<UButton> CloseRemappingButton;

    UPROPERTY(Transient)
    TObjectPtr<UButton> ResetRemappingButton;

    TArray<FInputSelectorEntry> InputSelectorEntries;
};
