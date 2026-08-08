#include "BHUserSettingsSubsystem.h"

#include "BHGameShellSettings.h"
#include "BHPlaytestTelemetrySubsystem.h"
#include "BHUIStyle.h"
#include "BHUserSettingsSaveGame.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/GameUserSettings.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundClass.h"
#include "Sound/SoundMix.h"

namespace
{
FBHInputBindingDefinition MakeBinding(
    const TCHAR* BindingID,
    const TCHAR* DisplayName,
    const FKey& KeyboardKey,
    const FKey& GamepadKey
)
{
    FBHInputBindingDefinition Definition;
    Definition.BindingID = FName(BindingID);
    Definition.DisplayName = FText::FromString(DisplayName);
    Definition.DefaultKeyboardKey = KeyboardKey;
    Definition.DefaultGamepadKey = GamepadKey;
    return Definition;
}

FString GetCompactInputKeyLabel(const FKey& Key)
{
    const TPair<FKey, const TCHAR*> CompactLabels[] = {
        {EKeys::Gamepad_FaceButton_Top, TEXT("FACE TOP")},
        {EKeys::Gamepad_FaceButton_Bottom, TEXT("FACE BOTTOM")},
        {EKeys::Gamepad_FaceButton_Left, TEXT("FACE LEFT")},
        {EKeys::Gamepad_FaceButton_Right, TEXT("FACE RIGHT")},
        {EKeys::Gamepad_LeftShoulder, TEXT("L SHOULDER")},
        {EKeys::Gamepad_RightShoulder, TEXT("R SHOULDER")},
        {EKeys::Gamepad_LeftTrigger, TEXT("L TRIGGER")},
        {EKeys::Gamepad_RightTrigger, TEXT("R TRIGGER")},
        {EKeys::Gamepad_LeftThumbstick, TEXT("L STICK")},
        {EKeys::Gamepad_RightThumbstick, TEXT("R STICK")},
        {EKeys::Gamepad_DPad_Up, TEXT("DPAD UP")},
        {EKeys::Gamepad_DPad_Down, TEXT("DPAD DOWN")},
        {EKeys::Gamepad_DPad_Left, TEXT("DPAD LEFT")},
        {EKeys::Gamepad_DPad_Right, TEXT("DPAD RIGHT")},
        {EKeys::Gamepad_Special_Left, TEXT("VIEW")},
        {EKeys::Gamepad_Special_Right, TEXT("MENU")},
        {EKeys::Gamepad_Left2D, TEXT("L STICK")},
        {EKeys::Gamepad_Right2D, TEXT("R STICK")}
    };
    for (const TPair<FKey, const TCHAR*>& Label : CompactLabels)
    {
        if (Label.Key == Key)
        {
            return FString(Label.Value);
        }
    }
    return Key.IsValid()
        ? Key.GetDisplayName(false).ToString().ToUpper()
        : FString();
}
}

const FString UBHUserSettingsSubsystem::SettingsSlotName(
    TEXT("BrokenHorizon_UserSettings")
);

void UBHUserSettingsSubsystem::Initialize(
    FSubsystemCollectionBase& Collection
)
{
    Super::Initialize(Collection);
    LoadSettings();
    ApplyVideoSettings();
    ApplyVisualComfortCVars();
}

bool UBHUserSettingsSubsystem::ApplyAccessibilitySettings(
    float NewHUDScale,
    EBHColorVisionMode NewColorVisionMode,
    bool bNewHighContrastHUD,
    bool bNewReducedMotion
)
{
    if (!IsValid(SettingsData))
    {
        LoadSettings();
    }

    if (!IsValid(SettingsData))
    {
        return false;
    }

    SettingsData->HUDScale = FMath::Clamp(NewHUDScale, 0.75f, 1.5f);
    SettingsData->ColorVisionMode = static_cast<EBHColorVisionMode>(
        FMath::Clamp(static_cast<int32>(NewColorVisionMode), 0, 3)
    );
    SettingsData->bHighContrastHUD = bNewHighContrastHUD;
    SettingsData->bReducedMotion = bNewReducedMotion;

    if (UWorld* World = GetWorld())
    {
        BHUIStyle::RefreshAll(*World);
    }

    return SaveSettings();
}

bool UBHUserSettingsSubsystem::ApplyLookSettings(
    float NewHorizontalSensitivity,
    float NewVerticalSensitivity,
    float NewADSSensitivityMultiplier,
    bool bNewInvertVerticalLook
)
{
    if (!IsValid(SettingsData))
    {
        LoadSettings();
    }

    if (!IsValid(SettingsData))
    {
        return false;
    }

    SettingsData->HorizontalLookSensitivity = FMath::Clamp(
        NewHorizontalSensitivity,
        0.1f,
        5.0f
    );
    SettingsData->VerticalLookSensitivity = FMath::Clamp(
        NewVerticalSensitivity,
        0.1f,
        5.0f
    );
    SettingsData->ADSSensitivityMultiplier = FMath::Clamp(
        NewADSSensitivityMultiplier,
        0.1f,
        1.5f
    );
    SettingsData->bInvertVerticalLook = bNewInvertVerticalLook;
    return SaveSettings();
}

const TArray<FBHInputBindingDefinition>&
UBHUserSettingsSubsystem::GetDefaultInputBindingDefinitions()
{
    static const TArray<FBHInputBindingDefinition> Definitions = {
        MakeBinding(TEXT("MoveForward"), TEXT("Move Forward"), EKeys::W, FKey()),
        MakeBinding(TEXT("MoveBackward"), TEXT("Move Backward"), EKeys::S, FKey()),
        MakeBinding(TEXT("MoveLeft"), TEXT("Move Left"), EKeys::A, FKey()),
        MakeBinding(TEXT("MoveRight"), TEXT("Move Right"), EKeys::D, FKey()),
        MakeBinding(TEXT("MoveStick"), TEXT("Movement Stick"), FKey(), EKeys::Gamepad_Left2D),
        MakeBinding(TEXT("LookHorizontal"), TEXT("Look Horizontal"), EKeys::MouseX, FKey()),
        MakeBinding(TEXT("LookVertical"), TEXT("Look Vertical"), EKeys::MouseY, FKey()),
        MakeBinding(TEXT("LookStick"), TEXT("Look Stick"), FKey(), EKeys::Gamepad_Right2D),
        MakeBinding(TEXT("Jump"), TEXT("Jump"), EKeys::SpaceBar, EKeys::Gamepad_FaceButton_Bottom),
        MakeBinding(TEXT("Sprint"), TEXT("Sprint"), EKeys::LeftShift, EKeys::Gamepad_LeftThumbstick),
        MakeBinding(TEXT("Crouch"), TEXT("Crouch"), EKeys::LeftControl, EKeys::Gamepad_FaceButton_Right),
        MakeBinding(TEXT("Interact"), TEXT("Interact"), EKeys::F, EKeys::Gamepad_FaceButton_Top),
        MakeBinding(TEXT("Fire"), TEXT("Fire"), EKeys::LeftMouseButton, EKeys::Gamepad_RightTrigger),
        MakeBinding(TEXT("Aim"), TEXT("Aim"), EKeys::RightMouseButton, EKeys::Gamepad_LeftTrigger),
        MakeBinding(TEXT("Reload"), TEXT("Reload"), EKeys::R, EKeys::Gamepad_FaceButton_Left),
        MakeBinding(TEXT("Pause"), TEXT("Pause"), EKeys::Escape, EKeys::Gamepad_Special_Right),
        MakeBinding(TEXT("WarMap"), TEXT("War Map"), EKeys::M, EKeys::Gamepad_Special_Left),
        MakeBinding(TEXT("Grenade"), TEXT("Throw Grenade"), EKeys::G, EKeys::Gamepad_RightShoulder),
        MakeBinding(TEXT("SmokeGrenade"), TEXT("Throw Smoke Grenade"), EKeys::K, FKey()),
        MakeBinding(TEXT("Flashlight"), TEXT("Tactical Flashlight"), EKeys::L, FKey()),
        MakeBinding(TEXT("WeaponBash"), TEXT("Weapon Bash"), EKeys::T, FKey()),
        MakeBinding(TEXT("FieldObservation"), TEXT("Field Observation"), EKeys::O, FKey()),
        MakeBinding(TEXT("ControlledBreathing"), TEXT("Controlled Breathing"), EKeys::LeftAlt, FKey()),
        MakeBinding(TEXT("Engineering"), TEXT("Engineering Charge / Detonate"), EKeys::V, FKey()),
        MakeBinding(TEXT("SquadOrder"), TEXT("Squad Follow / Hold"), EKeys::C, EKeys::Gamepad_DPad_Left),
        MakeBinding(TEXT("ContextAction"), TEXT("Squad Context Action"), EKeys::X, EKeys::Gamepad_DPad_Right),
        MakeBinding(TEXT("SquadPing"), TEXT("Squad Ping"), EKeys::MiddleMouseButton, EKeys::Gamepad_DPad_Up),
        MakeBinding(TEXT("FireMode"), TEXT("Fire Mode"), EKeys::B, EKeys::Gamepad_DPad_Down),
        MakeBinding(TEXT("FieldDressing"), TEXT("Field Dressing"), EKeys::H, EKeys::Gamepad_LeftShoulder),
        MakeBinding(TEXT("Medkit"), TEXT("Medkit"), EKeys::J, EKeys::Gamepad_RightThumbstick),
        MakeBinding(TEXT("LeanLeft"), TEXT("Lean Left"), EKeys::Q, FKey()),
        MakeBinding(TEXT("LeanRight"), TEXT("Lean Right"), EKeys::E, FKey()),
        MakeBinding(TEXT("Prone"), TEXT("Prone"), EKeys::Z, FKey())
    };
    return Definitions;
}

TArray<FBHInputBindingDefinition>
UBHUserSettingsSubsystem::GetInputBindingDefinitions() const
{
    return GetDefaultInputBindingDefinitions();
}

FKey UBHUserSettingsSubsystem::GetInputBinding(
    FName BindingID,
    bool bGamepad
) const
{
    const TMap<FName, FKey>* Overrides = IsValid(SettingsData)
        ? (bGamepad
            ? &SettingsData->GamepadBindings
            : &SettingsData->KeyboardBindings)
        : nullptr;
    if (Overrides)
    {
        if (const FKey* Key = Overrides->Find(BindingID))
        {
            return *Key;
        }
    }

    for (const FBHInputBindingDefinition& Definition :
        GetDefaultInputBindingDefinitions())
    {
        if (Definition.BindingID == BindingID)
        {
            return bGamepad
                ? Definition.DefaultGamepadKey
                : Definition.DefaultKeyboardKey;
        }
    }
    return FKey();
}

FString UBHUserSettingsSubsystem::BuildInputBindingPrompt(
    const FKey& KeyboardKey,
    const FKey& GamepadKey
)
{
    const FString KeyboardLabel = GetCompactInputKeyLabel(KeyboardKey);
    const FString GamepadLabel = GetCompactInputKeyLabel(GamepadKey);

    if (KeyboardLabel.IsEmpty())
    {
        return GamepadLabel;
    }
    if (GamepadLabel.IsEmpty() || GamepadLabel == KeyboardLabel)
    {
        return KeyboardLabel;
    }
    return FString::Printf(
        TEXT("%s / %s"),
        *KeyboardLabel,
        *GamepadLabel
    );
}

FString UBHUserSettingsSubsystem::BuildInputBindingPromptForMode(
    const FKey& KeyboardKey,
    const FKey& GamepadKey,
    EBHInputPromptMode Mode,
    bool bLastInputWasGamepad
)
{
    const FString KeyboardLabel = GetCompactInputKeyLabel(KeyboardKey);
    const FString GamepadLabel = GetCompactInputKeyLabel(GamepadKey);
    const EBHInputPromptMode EffectiveMode = Mode == EBHInputPromptMode::Auto
        ? (bLastInputWasGamepad
            ? EBHInputPromptMode::Gamepad
            : EBHInputPromptMode::KeyboardMouse)
        : Mode;

    if (EffectiveMode == EBHInputPromptMode::KeyboardMouse)
    {
        return !KeyboardLabel.IsEmpty() ? KeyboardLabel : GamepadLabel;
    }
    if (EffectiveMode == EBHInputPromptMode::Gamepad)
    {
        return !GamepadLabel.IsEmpty() ? GamepadLabel : KeyboardLabel;
    }
    return BuildInputBindingPrompt(KeyboardKey, GamepadKey);
}

FString UBHUserSettingsSubsystem::GetInputBindingPrompt(
    FName BindingID
) const
{
    return BuildInputBindingPromptForMode(
        GetInputBinding(BindingID, false),
        GetInputBinding(BindingID, true),
        GetInputPromptMode(),
        bLastInputWasGamepad
    );
}

bool UBHUserSettingsSubsystem::ApplyInputPromptMode(
    EBHInputPromptMode NewMode
)
{
    if (!IsValid(SettingsData))
    {
        LoadSettings();
    }
    if (!IsValid(SettingsData))
    {
        return false;
    }

    SettingsData->InputPromptMode = static_cast<EBHInputPromptMode>(
        FMath::Clamp(static_cast<int32>(NewMode), 0, 3)
    );
    const bool bSaved = SaveSettings();
    if (bSaved)
    {
        OnInputBindingsChanged.Broadcast();
    }
    return bSaved;
}

EBHInputPromptMode UBHUserSettingsSubsystem::GetInputPromptMode() const
{
#if !UE_BUILD_SHIPPING
    if (InputPromptModeReviewOverride.IsSet())
    {
        return InputPromptModeReviewOverride.GetValue();
    }
#endif
    return IsValid(SettingsData)
        ? SettingsData->InputPromptMode
        : EBHInputPromptMode::Auto;
}

#if !UE_BUILD_SHIPPING
void UBHUserSettingsSubsystem::SetInputPromptModeForRenderedReview(
    EBHInputPromptMode Mode,
    bool bLastInputGamepad
)
{
    InputPromptModeReviewOverride = Mode;
    bLastInputWasGamepad = bLastInputGamepad;
    OnInputBindingsChanged.Broadcast();
}
#endif

bool UBHUserSettingsSubsystem::IsUsingGamepadPrompts() const
{
    const EBHInputPromptMode Mode = GetInputPromptMode();
    return Mode == EBHInputPromptMode::Gamepad ||
        (Mode == EBHInputPromptMode::Auto && bLastInputWasGamepad);
}

void UBHUserSettingsSubsystem::NotifyInputDeviceUsed(bool bGamepad)
{
    if (bLastInputWasGamepad == bGamepad ||
        GetInputPromptMode() != EBHInputPromptMode::Auto)
    {
        return;
    }

    bLastInputWasGamepad = bGamepad;
    UE_LOG(
        LogTemp,
        Display,
        TEXT("BH_INPUT_PROMPT_DEVICE device=%s mode=auto"),
        bGamepad ? TEXT("gamepad") : TEXT("keyboard_mouse")
    );
    OnInputBindingsChanged.Broadcast();
}

FText UBHUserSettingsSubsystem::ResolveLegacyInputPrompts(
    const FText& SourceText
) const
{
    FString Resolved = SourceText.ToString();
    const TPair<const TCHAR*, const TCHAR*> LegacyBindings[] = {
        {TEXT("F"), TEXT("Interact")},
        {TEXT("C"), TEXT("SquadOrder")},
        {TEXT("X"), TEXT("ContextAction")},
        {TEXT("H"), TEXT("FieldDressing")},
        {TEXT("J"), TEXT("Medkit")},
        {TEXT("G"), TEXT("Grenade")},
        {TEXT("V"), TEXT("Engineering")},
        {TEXT("M"), TEXT("WarMap")}
    };

    for (const TPair<const TCHAR*, const TCHAR*>& Binding : LegacyBindings)
    {
        const FString Prompt = GetInputBindingPrompt(FName(Binding.Value));
        if (Prompt.IsEmpty())
        {
            continue;
        }
        Resolved.ReplaceInline(
            *FString::Printf(TEXT("[%s]"), Binding.Key),
            *FString::Printf(TEXT("[%s]"), *Prompt),
            ESearchCase::CaseSensitive
        );
        Resolved.ReplaceInline(
            *FString::Printf(TEXT("PRESS %s"), Binding.Key),
            *FString::Printf(TEXT("PRESS [%s]"), *Prompt),
            ESearchCase::IgnoreCase
        );
    }
    return FText::FromString(Resolved);
}

bool UBHUserSettingsSubsystem::SetInputBinding(
    FName BindingID,
    FKey NewKey,
    bool bGamepad
)
{
    if (!IsValid(SettingsData))
    {
        LoadSettings();
    }

    const FBHInputBindingDefinition* Definition =
        GetDefaultInputBindingDefinitions().FindByPredicate(
            [BindingID](const FBHInputBindingDefinition& Candidate)
            {
                return Candidate.BindingID == BindingID;
            }
        );
    const FKey DeviceDefault = Definition
        ? (bGamepad
            ? Definition->DefaultGamepadKey
            : Definition->DefaultKeyboardKey)
        : FKey();
    if (!IsValid(SettingsData) || !Definition || !DeviceDefault.IsValid() ||
        !NewKey.IsValid() ||
        NewKey.IsGamepadKey() != bGamepad)
    {
        return false;
    }

    TMap<FName, FKey>& Overrides = bGamepad
        ? SettingsData->GamepadBindings
        : SettingsData->KeyboardBindings;
    const FKey PreviousKey = GetInputBinding(BindingID, bGamepad);

    for (const FBHInputBindingDefinition& Candidate :
        GetDefaultInputBindingDefinitions())
    {
        if (Candidate.BindingID != BindingID &&
            GetInputBinding(Candidate.BindingID, bGamepad) == NewKey)
        {
            if (PreviousKey.IsValid())
            {
                Overrides.Add(Candidate.BindingID, PreviousKey);
            }
            else
            {
                Overrides.Remove(Candidate.BindingID);
            }
            break;
        }
    }

    Overrides.Add(BindingID, NewKey);
    const bool bSaved = SaveSettings();
    if (bSaved)
    {
        OnInputBindingsChanged.Broadcast();
    }
    return bSaved;
}

bool UBHUserSettingsSubsystem::ResetInputBindings()
{
    if (!IsValid(SettingsData))
    {
        LoadSettings();
    }
    if (!IsValid(SettingsData))
    {
        return false;
    }
    SettingsData->KeyboardBindings.Reset();
    SettingsData->GamepadBindings.Reset();
    const bool bSaved = SaveSettings();
    if (bSaved)
    {
        OnInputBindingsChanged.Broadcast();
    }
    return bSaved;
}

bool UBHUserSettingsSubsystem::ApplyInputModeSettings(
    bool bToggleAim,
    bool bToggleSprint,
    bool bToggleCrouch,
    bool bToggleProne,
    bool bToggleLean,
    bool bHoldInteraction
)
{
    if (!IsValid(SettingsData))
    {
        LoadSettings();
    }
    if (!IsValid(SettingsData))
    {
        return false;
    }
    SettingsData->bToggleAim = bToggleAim;
    SettingsData->bToggleSprint = bToggleSprint;
    SettingsData->bToggleCrouch = bToggleCrouch;
    SettingsData->bToggleProne = bToggleProne;
    SettingsData->bToggleLean = bToggleLean;
    SettingsData->bHoldInteraction = bHoldInteraction;
    return SaveSettings();
}

bool UBHUserSettingsSubsystem::IsToggleAimEnabled() const
{
    return IsValid(SettingsData) && SettingsData->bToggleAim;
}

bool UBHUserSettingsSubsystem::IsToggleSprintEnabled() const
{
    return IsValid(SettingsData) && SettingsData->bToggleSprint;
}

bool UBHUserSettingsSubsystem::IsToggleCrouchEnabled() const
{
    return IsValid(SettingsData) && SettingsData->bToggleCrouch;
}

bool UBHUserSettingsSubsystem::IsToggleProneEnabled() const
{
    return !IsValid(SettingsData) || SettingsData->bToggleProne;
}

bool UBHUserSettingsSubsystem::IsToggleLeanEnabled() const
{
    return IsValid(SettingsData) && SettingsData->bToggleLean;
}

bool UBHUserSettingsSubsystem::IsHoldInteractionEnabled() const
{
    return IsValid(SettingsData) && SettingsData->bHoldInteraction;
}

bool UBHUserSettingsSubsystem::ApplyVisualComfortSettings(
    float CameraShakeScale,
    float RecoilAnimationScale,
    float HeadBobScale,
    float HitFlashScale,
    bool bMotionBlurEnabled,
    bool bDepthOfFieldEnabled,
    bool bChromaticAberrationEnabled
)
{
    if (!IsValid(SettingsData))
    {
        LoadSettings();
    }
    if (!IsValid(SettingsData))
    {
        return false;
    }

    SettingsData->CameraShakeScale = FMath::Clamp(CameraShakeScale, 0.0f, 1.0f);
    SettingsData->RecoilAnimationScale = FMath::Clamp(RecoilAnimationScale, 0.0f, 1.0f);
    SettingsData->HeadBobScale = FMath::Clamp(HeadBobScale, 0.0f, 1.0f);
    SettingsData->HitFlashScale = FMath::Clamp(HitFlashScale, 0.0f, 1.0f);
    SettingsData->bMotionBlurEnabled = bMotionBlurEnabled;
    SettingsData->bDepthOfFieldEnabled = bDepthOfFieldEnabled;
    SettingsData->bChromaticAberrationEnabled = bChromaticAberrationEnabled;
    ApplyVisualComfortCVars();
    return SaveSettings();
}

float UBHUserSettingsSubsystem::GetCameraShakeScale() const
{
    return IsValid(SettingsData) ? SettingsData->CameraShakeScale : 1.0f;
}

float UBHUserSettingsSubsystem::GetRecoilAnimationScale() const
{
    return IsValid(SettingsData) ? SettingsData->RecoilAnimationScale : 1.0f;
}

float UBHUserSettingsSubsystem::GetHeadBobScale() const
{
    return IsValid(SettingsData) ? SettingsData->HeadBobScale : 1.0f;
}

float UBHUserSettingsSubsystem::GetHitFlashScale() const
{
    return IsValid(SettingsData) ? SettingsData->HitFlashScale : 1.0f;
}

bool UBHUserSettingsSubsystem::IsMotionBlurEnabled() const
{
    return !IsValid(SettingsData) || SettingsData->bMotionBlurEnabled;
}

bool UBHUserSettingsSubsystem::IsDepthOfFieldEnabled() const
{
    return !IsValid(SettingsData) || SettingsData->bDepthOfFieldEnabled;
}

bool UBHUserSettingsSubsystem::IsChromaticAberrationEnabled() const
{
    return !IsValid(SettingsData) || SettingsData->bChromaticAberrationEnabled;
}

bool UBHUserSettingsSubsystem::ApplySubtitleSettings(
    bool bSubtitlesEnabled,
    bool bSpeakerLabels,
    bool bDirectionalIndicators,
    float TextScale,
    float BackgroundOpacity,
    float SafeAreaScale
)
{
    if (!IsValid(SettingsData)) LoadSettings();
    if (!IsValid(SettingsData)) return false;
    SettingsData->bSubtitlesEnabled = bSubtitlesEnabled;
    SettingsData->bSubtitleSpeakerLabels = bSpeakerLabels;
    SettingsData->bSubtitleDirectionalIndicators = bDirectionalIndicators;
    SettingsData->SubtitleTextScale = FMath::Clamp(TextScale, 0.75f, 2.0f);
    SettingsData->SubtitleBackgroundOpacity = FMath::Clamp(BackgroundOpacity, 0.0f, 1.0f);
    SettingsData->UISafeAreaScale = FMath::Clamp(SafeAreaScale, 0.8f, 1.0f);
    if (UWorld* World = GetWorld()) BHUIStyle::RefreshAll(*World);
    return SaveSettings();
}

bool UBHUserSettingsSubsystem::AreSubtitlesEnabled() const
{
    return !IsValid(SettingsData) || SettingsData->bSubtitlesEnabled;
}

bool UBHUserSettingsSubsystem::AreSubtitleSpeakerLabelsEnabled() const
{
    return !IsValid(SettingsData) || SettingsData->bSubtitleSpeakerLabels;
}

bool UBHUserSettingsSubsystem::AreSubtitleDirectionalIndicatorsEnabled() const
{
    return !IsValid(SettingsData) || SettingsData->bSubtitleDirectionalIndicators;
}

float UBHUserSettingsSubsystem::GetSubtitleTextScale() const
{
    return IsValid(SettingsData) ? SettingsData->SubtitleTextScale : 1.0f;
}

float UBHUserSettingsSubsystem::GetSubtitleBackgroundOpacity() const
{
    return IsValid(SettingsData) ? SettingsData->SubtitleBackgroundOpacity : 0.75f;
}

float UBHUserSettingsSubsystem::GetUISafeAreaScale() const
{
    return IsValid(SettingsData) ? SettingsData->UISafeAreaScale : 0.95f;
}

bool UBHUserSettingsSubsystem::ApplyInputBindings(
    const TMap<FName, FKey>& KeyboardBindings,
    const TMap<FName, FKey>& GamepadBindings
)
{
    if (!IsValid(SettingsData))
    {
        LoadSettings();
    }
    if (!IsValid(SettingsData))
    {
        return false;
    }

    TMap<FName, FKey> ValidatedKeyboard;
    TMap<FName, FKey> ValidatedGamepad;
    TSet<FKey> UsedKeyboardKeys;
    TSet<FKey> UsedGamepadKeys;
    for (const FBHInputBindingDefinition& Definition :
        GetDefaultInputBindingDefinitions())
    {
        const TPair<const TMap<FName, FKey>*, bool> Devices[] = {
            {&KeyboardBindings, false},
            {&GamepadBindings, true}
        };
        for (const TPair<const TMap<FName, FKey>*, bool>& Device : Devices)
        {
            const FKey DefaultKey = Device.Value
                ? Definition.DefaultGamepadKey
                : Definition.DefaultKeyboardKey;
            if (!DefaultKey.IsValid())
            {
                continue;
            }
            const FKey* RequestedKey = Device.Key->Find(
                Definition.BindingID
            );
            const FKey Key = RequestedKey ? *RequestedKey : DefaultKey;
            TSet<FKey>& UsedKeys = Device.Value
                ? UsedGamepadKeys
                : UsedKeyboardKeys;
            if (!Key.IsValid() || Key.IsGamepadKey() != Device.Value ||
                UsedKeys.Contains(Key))
            {
                return false;
            }
            UsedKeys.Add(Key);
            if (Key != DefaultKey)
            {
                (Device.Value ? ValidatedGamepad : ValidatedKeyboard).Add(
                    Definition.BindingID,
                    Key
                );
            }
        }
    }

    SettingsData->KeyboardBindings = MoveTemp(ValidatedKeyboard);
    SettingsData->GamepadBindings = MoveTemp(ValidatedGamepad);
    const bool bSaved = SaveSettings();
    if (bSaved)
    {
        OnInputBindingsChanged.Broadcast();
    }
    return bSaved;
}

bool UBHUserSettingsSubsystem::ApplySettings(
    float NewMouseSensitivity,
    float NewMasterVolume,
    int32 NewWindowModeIndex,
    int32 NewGraphicsQuality
)
{
    if (!IsValid(SettingsData))
    {
        LoadSettings();
    }

    if (!IsValid(SettingsData))
    {
        return false;
    }

    SettingsData->MouseSensitivity = FMath::Clamp(
        NewMouseSensitivity,
        0.1f,
        5.0f
    );
    SettingsData->MasterVolume = FMath::Clamp(
        NewMasterVolume,
        0.0f,
        1.0f
    );
    SettingsData->WindowModeIndex = FMath::Clamp(
        NewWindowModeIndex,
        0,
        2
    );
    SettingsData->GraphicsQuality = FMath::Clamp(
        NewGraphicsQuality,
        0,
        3
    );

    ApplyVideoSettings();
    ApplyAudioSettings();
    return SaveSettings();
}

void UBHUserSettingsSubsystem::ApplyPersistedSettings()
{
    ApplyVideoSettings();
    ApplyAudioSettings();
    ApplyVisualComfortCVars();
}

float UBHUserSettingsSubsystem::GetMouseSensitivity() const
{
    return IsValid(SettingsData)
        ? SettingsData->MouseSensitivity
        : DefaultMouseSensitivity;
}

float UBHUserSettingsSubsystem::GetMasterVolume() const
{
    return IsValid(SettingsData)
        ? SettingsData->MasterVolume
        : DefaultMasterVolume;
}

int32 UBHUserSettingsSubsystem::GetWindowModeIndex() const
{
    return IsValid(SettingsData)
        ? SettingsData->WindowModeIndex
        : DefaultWindowModeIndex;
}

int32 UBHUserSettingsSubsystem::GetGraphicsQuality() const
{
    return IsValid(SettingsData)
        ? SettingsData->GraphicsQuality
        : DefaultGraphicsQuality;
}

float UBHUserSettingsSubsystem::GetHorizontalLookSensitivity() const
{
    return IsValid(SettingsData)
        ? SettingsData->HorizontalLookSensitivity
        : DefaultMouseSensitivity;
}

float UBHUserSettingsSubsystem::GetVerticalLookSensitivity() const
{
    return IsValid(SettingsData)
        ? SettingsData->VerticalLookSensitivity
        : DefaultMouseSensitivity;
}

float UBHUserSettingsSubsystem::GetADSSensitivityMultiplier() const
{
    return IsValid(SettingsData)
        ? SettingsData->ADSSensitivityMultiplier
        : DefaultADSSensitivityMultiplier;
}

bool UBHUserSettingsSubsystem::IsVerticalLookInverted() const
{
    return IsValid(SettingsData) && SettingsData->bInvertVerticalLook;
}

FVector2D UBHUserSettingsSubsystem::CalculateLookInput(
    const FVector2D& RawInput,
    float HorizontalSensitivity,
    float VerticalSensitivity,
    float ADSSensitivityMultiplier,
    bool bAiming,
    bool bInvertVerticalLook
)
{
    const float AimScale = bAiming
        ? FMath::Clamp(ADSSensitivityMultiplier, 0.1f, 1.5f)
        : 1.0f;
    return FVector2D(
        RawInput.X *
            FMath::Clamp(HorizontalSensitivity, 0.1f, 5.0f) *
            AimScale,
        RawInput.Y *
            FMath::Clamp(VerticalSensitivity, 0.1f, 5.0f) *
            AimScale *
            (bInvertVerticalLook ? -1.0f : 1.0f)
    );
}

float UBHUserSettingsSubsystem::GetHUDScale() const
{
    return IsValid(SettingsData) ? SettingsData->HUDScale : DefaultHUDScale;
}

EBHColorVisionMode UBHUserSettingsSubsystem::GetColorVisionMode() const
{
    return IsValid(SettingsData)
        ? SettingsData->ColorVisionMode
        : EBHColorVisionMode::Standard;
}

bool UBHUserSettingsSubsystem::IsHighContrastHUDEnabled() const
{
    return IsValid(SettingsData) && SettingsData->bHighContrastHUD;
}

bool UBHUserSettingsSubsystem::IsReducedMotionEnabled() const
{
    return IsValid(SettingsData) && SettingsData->bReducedMotion;
}

void UBHUserSettingsSubsystem::LoadSettings()
{
    SettingsData = nullptr;

    if (UGameplayStatics::DoesSaveGameExist(
        SettingsSlotName,
        SettingsUserIndex
    ))
    {
        SettingsData = Cast<UBHUserSettingsSaveGame>(
            UGameplayStatics::LoadGameFromSlot(
                SettingsSlotName,
                SettingsUserIndex
            )
        );
    }

    if (!IsValid(SettingsData))
    {
        SettingsData = Cast<UBHUserSettingsSaveGame>(
            UGameplayStatics::CreateSaveGameObject(
                UBHUserSettingsSaveGame::StaticClass()
            )
        );
    }

    if (IsValid(SettingsData) && SettingsData->SchemaVersion < 2)
    {
        SettingsData->HUDScale = DefaultHUDScale;
        SettingsData->ColorVisionMode = EBHColorVisionMode::Standard;
        SettingsData->bHighContrastHUD = false;
        SettingsData->bReducedMotion = false;
        SettingsData->SchemaVersion = 2;
        SaveSettings();
    }

    if (IsValid(SettingsData) && SettingsData->SchemaVersion < 3)
    {
        const float LegacySensitivity = FMath::Clamp(
            SettingsData->MouseSensitivity,
            0.1f,
            5.0f
        );
        SettingsData->HorizontalLookSensitivity = LegacySensitivity;
        SettingsData->VerticalLookSensitivity = LegacySensitivity;
        SettingsData->ADSSensitivityMultiplier =
            DefaultADSSensitivityMultiplier;
        SettingsData->bInvertVerticalLook = false;
        SettingsData->SchemaVersion = 3;
        SaveSettings();
    }

    if (IsValid(SettingsData) && SettingsData->SchemaVersion < 4)
    {
        SettingsData->KeyboardBindings.Reset();
        SettingsData->GamepadBindings.Reset();
        SettingsData->SchemaVersion = 4;
        SaveSettings();
    }

    if (IsValid(SettingsData) && SettingsData->SchemaVersion < 5)
    {
        SettingsData->bToggleAim = false;
        SettingsData->bToggleSprint = false;
        SettingsData->bToggleCrouch = false;
        SettingsData->bToggleProne = true;
        SettingsData->bToggleLean = false;
        SettingsData->bHoldInteraction = false;
        SettingsData->SchemaVersion = 5;
        SaveSettings();
    }

    if (IsValid(SettingsData) && SettingsData->SchemaVersion < 6)
    {
        SettingsData->CameraShakeScale = 1.0f;
        SettingsData->RecoilAnimationScale = 1.0f;
        SettingsData->HeadBobScale = 1.0f;
        SettingsData->HitFlashScale = 1.0f;
        SettingsData->bMotionBlurEnabled = true;
        SettingsData->bDepthOfFieldEnabled = true;
        SettingsData->bChromaticAberrationEnabled = true;
        SettingsData->SchemaVersion = 6;
        SaveSettings();
    }

    if (IsValid(SettingsData) && SettingsData->SchemaVersion < 7)
    {
        SettingsData->bSubtitlesEnabled = true;
        SettingsData->bSubtitleSpeakerLabels = true;
        SettingsData->bSubtitleDirectionalIndicators = true;
        SettingsData->SubtitleTextScale = 1.0f;
        SettingsData->SubtitleBackgroundOpacity = 0.75f;
        SettingsData->UISafeAreaScale = 0.95f;
        SettingsData->SchemaVersion = 7;
        SaveSettings();
    }

    if (IsValid(SettingsData) && SettingsData->SchemaVersion < 8)
    {
        SettingsData->InputPromptMode = EBHInputPromptMode::Auto;
        SettingsData->SchemaVersion = 8;
        SaveSettings();
    }

    if (IsValid(SettingsData))
    {
        SettingsData->SchemaVersion = 8;
        SettingsData->InputPromptMode =
            static_cast<EBHInputPromptMode>(FMath::Clamp(
                static_cast<int32>(SettingsData->InputPromptMode),
                0,
                3
            ));
        SettingsData->MouseSensitivity = FMath::Clamp(SettingsData->MouseSensitivity, 0.1f, 5.0f);
        SettingsData->MasterVolume = FMath::Clamp(SettingsData->MasterVolume, 0.0f, 1.0f);
        SettingsData->WindowModeIndex = FMath::Clamp(SettingsData->WindowModeIndex, 0, 2);
        SettingsData->GraphicsQuality = FMath::Clamp(SettingsData->GraphicsQuality, 0, 3);
        SettingsData->HUDScale = FMath::Clamp(SettingsData->HUDScale, 0.75f, 1.5f);
        SettingsData->HorizontalLookSensitivity = FMath::Clamp(SettingsData->HorizontalLookSensitivity, 0.1f, 5.0f);
        SettingsData->VerticalLookSensitivity = FMath::Clamp(SettingsData->VerticalLookSensitivity, 0.1f, 5.0f);
        SettingsData->ADSSensitivityMultiplier = FMath::Clamp(SettingsData->ADSSensitivityMultiplier, 0.1f, 1.5f);
        SettingsData->CameraShakeScale = FMath::Clamp(SettingsData->CameraShakeScale, 0.0f, 1.0f);
        SettingsData->RecoilAnimationScale = FMath::Clamp(SettingsData->RecoilAnimationScale, 0.0f, 1.0f);
        SettingsData->HeadBobScale = FMath::Clamp(SettingsData->HeadBobScale, 0.0f, 1.0f);
        SettingsData->HitFlashScale = FMath::Clamp(SettingsData->HitFlashScale, 0.0f, 1.0f);
        SettingsData->SubtitleTextScale = FMath::Clamp(SettingsData->SubtitleTextScale, 0.75f, 2.0f);
        SettingsData->SubtitleBackgroundOpacity = FMath::Clamp(SettingsData->SubtitleBackgroundOpacity, 0.0f, 1.0f);
        SettingsData->UISafeAreaScale = FMath::Clamp(SettingsData->UISafeAreaScale, 0.8f, 1.0f);
    }
}

bool UBHUserSettingsSubsystem::SaveSettings()
{
    const bool bSaved = IsValid(SettingsData) &&
        UGameplayStatics::SaveGameToSlot(
            SettingsData,
            SettingsSlotName,
            SettingsUserIndex
        );
    if (bSaved)
    {
        if (UGameInstance* GameInstance = GetGameInstance())
        {
            GameInstance->GetSubsystem<UBHPlaytestTelemetrySubsystem>()->RecordEvent(
                TEXT("settings_changed")
            );
        }
    }
    return bSaved;
}

void UBHUserSettingsSubsystem::ApplyVideoSettings() const
{
    if (!IsValid(SettingsData) || !GEngine)
    {
        return;
    }

    UGameUserSettings* GameUserSettings =
        GEngine->GetGameUserSettings();

    if (!IsValid(GameUserSettings))
    {
        return;
    }

    EWindowMode::Type WindowMode =
        EWindowMode::WindowedFullscreen;

    if (SettingsData->WindowModeIndex == 0)
    {
        WindowMode = EWindowMode::Windowed;
    }
    else if (SettingsData->WindowModeIndex == 2)
    {
        WindowMode = EWindowMode::Fullscreen;
    }

    GameUserSettings->SetFullscreenMode(WindowMode);
    GameUserSettings->SetOverallScalabilityLevel(
        SettingsData->GraphicsQuality
    );
    GameUserSettings->ApplySettings(false);
    GameUserSettings->SaveSettings();
}

void UBHUserSettingsSubsystem::ApplyAudioSettings() const
{
    UWorld* World = GetWorld();
    const UBHGameShellSettings* ShellSettings =
        GetDefault<UBHGameShellSettings>();

    if (!IsValid(SettingsData) ||
        !IsValid(World) ||
        !IsValid(ShellSettings))
    {
        return;
    }

    USoundMix* SoundMix =
        ShellSettings->MasterSoundMix.LoadSynchronous();
    USoundClass* SoundClass =
        ShellSettings->MasterSoundClass.LoadSynchronous();

    if (!IsValid(SoundMix) || !IsValid(SoundClass))
    {
        UE_LOG(
            LogTemp,
            Verbose,
            TEXT(
                "Master volume was not applied because the game "
                "shell Sound Mix or Sound Class is unassigned."
            )
        );
        return;
    }

    UGameplayStatics::SetSoundMixClassOverride(
        World,
        SoundMix,
        SoundClass,
        SettingsData->MasterVolume,
        1.0f,
        0.0f,
        true
    );
    UGameplayStatics::PushSoundMixModifier(World, SoundMix);
}

void UBHUserSettingsSubsystem::ApplyVisualComfortCVars() const
{
    if (!IsValid(SettingsData))
    {
        return;
    }

    const TPair<const TCHAR*, int32> Settings[] = {
        {TEXT("r.MotionBlurQuality"), SettingsData->bMotionBlurEnabled ? 4 : 0},
        {TEXT("r.DepthOfFieldQuality"), SettingsData->bDepthOfFieldEnabled ? 2 : 0},
        {TEXT("r.SceneColorFringeQuality"), SettingsData->bChromaticAberrationEnabled ? 1 : 0}
    };
    for (const TPair<const TCHAR*, int32>& Setting : Settings)
    {
        if (IConsoleVariable* Variable = IConsoleManager::Get().FindConsoleVariable(Setting.Key))
        {
            Variable->Set(Setting.Value, ECVF_SetByGameSetting);
        }
    }
}
