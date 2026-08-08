#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "InputCoreTypes.h"
#include "BHUserSettingsSaveGame.generated.h"

UENUM(BlueprintType)
enum class EBHColorVisionMode : uint8
{
    Standard,
    Deuteranopia,
    Protanopia,
    Tritanopia
};

UENUM(BlueprintType)
enum class EBHInputPromptMode : uint8
{
    Auto,
    KeyboardMouse,
    Gamepad,
    Both
};

UCLASS()
class BROKENHORIZON_API UBHUserSettingsSaveGame
    : public USaveGame
{
    GENERATED_BODY()

public:
    UPROPERTY(SaveGame)
    int32 SchemaVersion = 8;

    UPROPERTY(SaveGame)
    float MouseSensitivity = 1.0f;

    UPROPERTY(SaveGame)
    float HorizontalLookSensitivity = 1.0f;

    UPROPERTY(SaveGame)
    float VerticalLookSensitivity = 1.0f;

    UPROPERTY(SaveGame)
    float ADSSensitivityMultiplier = 0.75f;

    UPROPERTY(SaveGame)
    bool bInvertVerticalLook = false;

    UPROPERTY(SaveGame)
    TMap<FName, FKey> KeyboardBindings;

    UPROPERTY(SaveGame)
    TMap<FName, FKey> GamepadBindings;

    UPROPERTY(SaveGame)
    EBHInputPromptMode InputPromptMode = EBHInputPromptMode::Auto;

    UPROPERTY(SaveGame)
    bool bToggleAim = false;

    UPROPERTY(SaveGame)
    bool bToggleSprint = false;

    UPROPERTY(SaveGame)
    bool bToggleCrouch = false;

    UPROPERTY(SaveGame)
    bool bToggleProne = true;

    UPROPERTY(SaveGame)
    bool bToggleLean = false;

    UPROPERTY(SaveGame)
    bool bHoldInteraction = false;

    UPROPERTY(SaveGame)
    float CameraShakeScale = 1.0f;

    UPROPERTY(SaveGame)
    float RecoilAnimationScale = 1.0f;

    UPROPERTY(SaveGame)
    float HeadBobScale = 1.0f;

    UPROPERTY(SaveGame)
    float HitFlashScale = 1.0f;

    UPROPERTY(SaveGame)
    bool bMotionBlurEnabled = true;

    UPROPERTY(SaveGame)
    bool bDepthOfFieldEnabled = true;

    UPROPERTY(SaveGame)
    bool bChromaticAberrationEnabled = true;

    UPROPERTY(SaveGame)
    bool bSubtitlesEnabled = true;

    UPROPERTY(SaveGame)
    bool bSubtitleSpeakerLabels = true;

    UPROPERTY(SaveGame)
    bool bSubtitleDirectionalIndicators = true;

    UPROPERTY(SaveGame)
    float SubtitleTextScale = 1.0f;

    UPROPERTY(SaveGame)
    float SubtitleBackgroundOpacity = 0.75f;

    UPROPERTY(SaveGame)
    float UISafeAreaScale = 0.95f;

    UPROPERTY(SaveGame)
    float MasterVolume = 1.0f;

    UPROPERTY(SaveGame)
    int32 WindowModeIndex = 1;

    UPROPERTY(SaveGame)
    int32 GraphicsQuality = 3;

    UPROPERTY(SaveGame)
    float HUDScale = 1.0f;

    UPROPERTY(SaveGame)
    EBHColorVisionMode ColorVisionMode = EBHColorVisionMode::Standard;

    UPROPERTY(SaveGame)
    bool bHighContrastHUD = false;

    UPROPERTY(SaveGame)
    bool bReducedMotion = false;
};
