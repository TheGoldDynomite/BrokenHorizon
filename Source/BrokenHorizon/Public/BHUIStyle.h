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
        0.68f,
        0.62f,
        0.36f,
        1.0f
    );
    inline const FLinearColor White(
        0.82f,
        0.84f,
        0.80f,
        1.0f
    );
    inline const FLinearColor Muted(
        0.46f,
        0.52f,
        0.49f,
        1.0f
    );
    inline const FLinearColor Friendly(
        0.34f,
        0.60f,
        0.48f,
        1.0f
    );
    inline const FLinearColor Warning(
        0.78f,
        0.58f,
        0.20f,
        1.0f
    );
    inline const FLinearColor Danger(
        0.72f,
        0.18f,
        0.13f,
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
