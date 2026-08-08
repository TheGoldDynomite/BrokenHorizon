#pragma once

#include "CoreMinimal.h"

class UUserWidget;
class UWorld;
enum class EBHColorVisionMode : uint8;

enum class EBHUIStyleContext : uint8
{
    Gameplay,
    Menu,
    Overlay,
    Alert
};

namespace BHUIStyle
{
    inline const FLinearColor Charcoal(
        0.018f,
        0.024f,
        0.026f,
        0.96f
    );
    inline const FLinearColor Gunmetal(
        0.075f,
        0.095f,
        0.098f,
        0.96f
    );
    inline const FLinearColor Gold(
        0.92f,
        0.66f,
        0.18f,
        1.0f
    );
    inline const FLinearColor White(
        0.90f,
        0.94f,
        0.93f,
        1.0f
    );
    inline const FLinearColor Muted(
        0.51f,
        0.59f,
        0.58f,
        1.0f
    );
    inline const FLinearColor Friendly(
        0.16f,
        0.78f,
        0.43f,
        1.0f
    );
    inline const FLinearColor Warning(
        1.0f,
        0.52f,
        0.08f,
        1.0f
    );
    inline const FLinearColor Danger(
        0.92f,
        0.10f,
        0.07f,
        1.0f
    );

    void Apply(
        UUserWidget& Widget,
        EBHUIStyleContext Context
    );

    void RefreshAll(UWorld& World);
    FLinearColor ResolveFriendlyColor(EBHColorVisionMode Mode);
    FLinearColor ResolveDangerColor(EBHColorVisionMode Mode);
    FMargin CalculateSafeAreaInsets(float SafeAreaScale);
    float ResolveContextScale(
        float HUDScale,
        EBHUIStyleContext Context
    );
}
