#pragma once

#include "CoreMinimal.h"
#include "BHUserSettingsSaveGame.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "BHUserSettingsSubsystem.generated.h"

class FSubsystemCollectionBase;

USTRUCT(BlueprintType)
struct FBHInputBindingDefinition
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Settings|Controls")
    FName BindingID = NAME_None;

    UPROPERTY(BlueprintReadOnly, Category = "Settings|Controls")
    FText DisplayName;

    UPROPERTY(BlueprintReadOnly, Category = "Settings|Controls")
    FKey DefaultKeyboardKey;

    UPROPERTY(BlueprintReadOnly, Category = "Settings|Controls")
    FKey DefaultGamepadKey;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FBHOnInputBindingsChanged);

UCLASS()
class BROKENHORIZON_API UBHUserSettingsSubsystem
    : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    UPROPERTY(BlueprintAssignable, Category = "Settings|Controls")
    FBHOnInputBindingsChanged OnInputBindingsChanged;

    virtual void Initialize(
        FSubsystemCollectionBase& Collection
    ) override;

    UFUNCTION(BlueprintCallable, Category = "Settings")
    bool ApplySettings(
        float NewMouseSensitivity,
        float NewMasterVolume,
        int32 NewWindowModeIndex,
        int32 NewGraphicsQuality
    );

    UFUNCTION(BlueprintCallable, Category = "Settings|Accessibility")
    bool ApplyAccessibilitySettings(
        float NewHUDScale,
        EBHColorVisionMode NewColorVisionMode,
        bool bNewHighContrastHUD,
        bool bNewReducedMotion
    );

    UFUNCTION(BlueprintCallable, Category = "Settings|Controls")
    bool ApplyLookSettings(
        float NewHorizontalSensitivity,
        float NewVerticalSensitivity,
        float NewADSSensitivityMultiplier,
        bool bNewInvertVerticalLook
    );

    UFUNCTION(BlueprintPure, Category = "Settings|Controls")
    TArray<FBHInputBindingDefinition> GetInputBindingDefinitions() const;

    UFUNCTION(BlueprintPure, Category = "Settings|Controls")
    FKey GetInputBinding(FName BindingID, bool bGamepad) const;

    UFUNCTION(BlueprintPure, Category = "Settings|Controls")
    FString GetInputBindingPrompt(FName BindingID) const;

    UFUNCTION(BlueprintCallable, Category = "Settings|Controls")
    bool ApplyInputPromptMode(EBHInputPromptMode NewMode);

    UFUNCTION(BlueprintPure, Category = "Settings|Controls")
    EBHInputPromptMode GetInputPromptMode() const;

    UFUNCTION(BlueprintPure, Category = "Settings|Controls")
    bool IsUsingGamepadPrompts() const;

    void NotifyInputDeviceUsed(bool bGamepad);

#if !UE_BUILD_SHIPPING
    void SetInputPromptModeForRenderedReview(
        EBHInputPromptMode Mode,
        bool bLastInputGamepad
    );
#endif

    UFUNCTION(BlueprintPure, Category = "Settings|Controls")
    FText ResolveLegacyInputPrompts(const FText& SourceText) const;

    static FString BuildInputBindingPrompt(
        const FKey& KeyboardKey,
        const FKey& GamepadKey
    );

    static FString BuildInputBindingPromptForMode(
        const FKey& KeyboardKey,
        const FKey& GamepadKey,
        EBHInputPromptMode Mode,
        bool bLastInputWasGamepad
    );

    UFUNCTION(BlueprintCallable, Category = "Settings|Controls")
    bool SetInputBinding(FName BindingID, FKey NewKey, bool bGamepad);

    UFUNCTION(BlueprintCallable, Category = "Settings|Controls")
    bool ApplyInputBindings(
        const TMap<FName, FKey>& KeyboardBindings,
        const TMap<FName, FKey>& GamepadBindings
    );

    UFUNCTION(BlueprintCallable, Category = "Settings|Controls")
    bool ResetInputBindings();

    UFUNCTION(BlueprintCallable, Category = "Settings|Controls")
    bool ApplyInputModeSettings(
        bool bToggleAim,
        bool bToggleSprint,
        bool bToggleCrouch,
        bool bToggleProne,
        bool bToggleLean,
        bool bHoldInteraction
    );

    UFUNCTION(BlueprintPure, Category = "Settings|Controls")
    bool IsToggleAimEnabled() const;

    UFUNCTION(BlueprintPure, Category = "Settings|Controls")
    bool IsToggleSprintEnabled() const;

    UFUNCTION(BlueprintPure, Category = "Settings|Controls")
    bool IsToggleCrouchEnabled() const;

    UFUNCTION(BlueprintPure, Category = "Settings|Controls")
    bool IsToggleProneEnabled() const;

    UFUNCTION(BlueprintPure, Category = "Settings|Controls")
    bool IsToggleLeanEnabled() const;

    UFUNCTION(BlueprintPure, Category = "Settings|Controls")
    bool IsHoldInteractionEnabled() const;

    UFUNCTION(BlueprintCallable, Category = "Settings|Accessibility")
    bool ApplyVisualComfortSettings(
        float CameraShakeScale,
        float RecoilAnimationScale,
        float HeadBobScale,
        float HitFlashScale,
        bool bMotionBlurEnabled,
        bool bDepthOfFieldEnabled,
        bool bChromaticAberrationEnabled
    );

    UFUNCTION(BlueprintPure, Category = "Settings|Accessibility")
    float GetCameraShakeScale() const;

    UFUNCTION(BlueprintPure, Category = "Settings|Accessibility")
    float GetRecoilAnimationScale() const;

    UFUNCTION(BlueprintPure, Category = "Settings|Accessibility")
    float GetHeadBobScale() const;

    UFUNCTION(BlueprintPure, Category = "Settings|Accessibility")
    float GetHitFlashScale() const;

    UFUNCTION(BlueprintPure, Category = "Settings|Accessibility")
    bool IsMotionBlurEnabled() const;

    UFUNCTION(BlueprintPure, Category = "Settings|Accessibility")
    bool IsDepthOfFieldEnabled() const;

    UFUNCTION(BlueprintPure, Category = "Settings|Accessibility")
    bool IsChromaticAberrationEnabled() const;

    UFUNCTION(BlueprintCallable, Category = "Settings|Accessibility")
    bool ApplySubtitleSettings(
        bool bSubtitlesEnabled,
        bool bSpeakerLabels,
        bool bDirectionalIndicators,
        float TextScale,
        float BackgroundOpacity,
        float SafeAreaScale
    );

    UFUNCTION(BlueprintPure, Category = "Settings|Accessibility")
    bool AreSubtitlesEnabled() const;

    UFUNCTION(BlueprintPure, Category = "Settings|Accessibility")
    bool AreSubtitleSpeakerLabelsEnabled() const;

    UFUNCTION(BlueprintPure, Category = "Settings|Accessibility")
    bool AreSubtitleDirectionalIndicatorsEnabled() const;

    UFUNCTION(BlueprintPure, Category = "Settings|Accessibility")
    float GetSubtitleTextScale() const;

    UFUNCTION(BlueprintPure, Category = "Settings|Accessibility")
    float GetSubtitleBackgroundOpacity() const;

    UFUNCTION(BlueprintPure, Category = "Settings|Accessibility")
    float GetUISafeAreaScale() const;

    static const TArray<FBHInputBindingDefinition>&
        GetDefaultInputBindingDefinitions();

    UFUNCTION(BlueprintCallable, Category = "Settings")
    void ApplyPersistedSettings();

    UFUNCTION(BlueprintPure, Category = "Settings")
    float GetMouseSensitivity() const;

    UFUNCTION(BlueprintPure, Category = "Settings|Controls")
    float GetHorizontalLookSensitivity() const;

    UFUNCTION(BlueprintPure, Category = "Settings|Controls")
    float GetVerticalLookSensitivity() const;

    UFUNCTION(BlueprintPure, Category = "Settings|Controls")
    float GetADSSensitivityMultiplier() const;

    UFUNCTION(BlueprintPure, Category = "Settings|Controls")
    bool IsVerticalLookInverted() const;

    UFUNCTION(BlueprintPure, Category = "Settings|Controls")
    static FVector2D CalculateLookInput(
        const FVector2D& RawInput,
        float HorizontalSensitivity,
        float VerticalSensitivity,
        float ADSSensitivityMultiplier,
        bool bAiming,
        bool bInvertVerticalLook
    );

    UFUNCTION(BlueprintPure, Category = "Settings")
    float GetMasterVolume() const;

    UFUNCTION(BlueprintPure, Category = "Settings")
    int32 GetWindowModeIndex() const;

    UFUNCTION(BlueprintPure, Category = "Settings")
    int32 GetGraphicsQuality() const;

    UFUNCTION(BlueprintPure, Category = "Settings|Accessibility")
    float GetHUDScale() const;

    UFUNCTION(BlueprintPure, Category = "Settings|Accessibility")
    EBHColorVisionMode GetColorVisionMode() const;

    UFUNCTION(BlueprintPure, Category = "Settings|Accessibility")
    bool IsHighContrastHUDEnabled() const;

    UFUNCTION(BlueprintPure, Category = "Settings|Accessibility")
    bool IsReducedMotionEnabled() const;

    static constexpr float DefaultMouseSensitivity = 1.0f;
    static constexpr float DefaultADSSensitivityMultiplier = 0.75f;
    static constexpr float DefaultMasterVolume = 1.0f;
    static constexpr int32 DefaultWindowModeIndex = 1;
    static constexpr int32 DefaultGraphicsQuality = 3;
    static constexpr float DefaultHUDScale = 1.0f;

private:
    void LoadSettings();
    bool SaveSettings();
    void ApplyVideoSettings() const;
    void ApplyAudioSettings() const;
    void ApplyVisualComfortCVars() const;

    static const FString SettingsSlotName;
    static constexpr int32 SettingsUserIndex = 0;

    UPROPERTY(Transient)
    TObjectPtr<UBHUserSettingsSaveGame> SettingsData;

    bool bLastInputWasGamepad = false;

#if !UE_BUILD_SHIPPING
    TOptional<EBHInputPromptMode> InputPromptModeReviewOverride;
#endif
};
