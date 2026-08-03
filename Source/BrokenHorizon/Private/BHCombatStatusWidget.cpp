#include "BHCombatStatusWidget.h"

#include "BHUIStyle.h"
#include "BHUserSettingsSubsystem.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "GameFramework/PlayerController.h"
#include "Engine/GameInstance.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"

namespace
{
constexpr float SquadPingMarkerY = 310.0f;

int32 DrawScreenTint(
    const FGeometry& Geometry,
    FSlateWindowElementList& DrawElements,
    int32 LayerId,
    const FLinearColor& Color
)
{
    FSlateDrawElement::MakeBox(
        DrawElements,
        LayerId,
        Geometry.ToPaintGeometry(),
        FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")),
        ESlateDrawEffect::None,
        Color
    );

    return LayerId;
}

int32 DrawPolyline(
    const FGeometry& Geometry,
    FSlateWindowElementList& DrawElements,
    int32 LayerId,
    const TArray<FVector2D>& Points,
    const FLinearColor& Color,
    float Thickness
)
{
    TArray<FVector2f> SlatePoints;
    SlatePoints.Reserve(Points.Num());

    for (const FVector2D& Point : Points)
    {
        SlatePoints.Add(
            FVector2f(
                static_cast<float>(Point.X),
                static_cast<float>(Point.Y)
            )
        );
    }

    FSlateDrawElement::MakeLines(
        DrawElements,
        LayerId,
        Geometry.ToPaintGeometry(),
        SlatePoints,
        ESlateDrawEffect::None,
        Color,
        true,
        Thickness
    );

    return LayerId;
}

int32 DrawDirectionChevron(
    const FGeometry& Geometry,
    FSlateWindowElementList& DrawElements,
    int32 LayerId,
    float AngleRadians,
    const FLinearColor& Color
)
{
    const FVector2D WidgetSize = Geometry.GetLocalSize();

    if (WidgetSize.X <= 1.0 || WidgetSize.Y <= 1.0)
    {
        return LayerId;
    }

    const FVector2D Center = WidgetSize * 0.5;
    const FVector2D Direction(
        FMath::Sin(AngleRadians),
        -FMath::Cos(AngleRadians)
    );
    const FVector2D Tangent(-Direction.Y, Direction.X);
    const FVector2D Radius(
        WidgetSize.X * 0.34,
        WidgetSize.Y * 0.30
    );
    const FVector2D Tip =
        Center +
        FVector2D(
            Direction.X * Radius.X,
            Direction.Y * Radius.Y
        );
    const FVector2D ChevronBase = Tip - (Direction * 28.0);
    const TArray<FVector2D> Points = {
        ChevronBase + (Tangent * 18.0),
        Tip,
        ChevronBase - (Tangent * 18.0)
    };

    return DrawPolyline(
        Geometry,
        DrawElements,
        LayerId,
        Points,
        Color,
        6.0f
    );
}

int32 DrawOperationWaypoint(
    const FGeometry& Geometry,
    FSlateWindowElementList& DrawElements,
    int32 LayerId,
    float DirectionAngleRadians,
    const FString& Label
)
{
    const FVector2D WidgetSize = Geometry.GetLocalSize();

    if (WidgetSize.X <= 1.0 || WidgetSize.Y <= 1.0)
    {
        return LayerId;
    }

    const float NormalizedBearing = FMath::Clamp(
        DirectionAngleRadians / PI,
        -1.0f,
        1.0f
    );
    const float MarkerX =
        (WidgetSize.X * 0.5f) +
        (NormalizedBearing * WidgetSize.X * 0.42f);
    const float MarkerY = 28.0f;
    const TArray<FVector2D> MarkerPoints = {
        FVector2D(MarkerX - 10.0f, MarkerY + 12.0f),
        FVector2D(MarkerX, MarkerY),
        FVector2D(MarkerX + 10.0f, MarkerY + 12.0f)
    };

    DrawPolyline(
        Geometry,
        DrawElements,
        LayerId,
        MarkerPoints,
        FLinearColor(1.0f, 0.66f, 0.12f, 0.98f),
        4.0f
    );

    int32 LineCount = 1;

    for (int32 CharacterIndex = 0;
        CharacterIndex < Label.Len();
        ++CharacterIndex)
    {
        if (Label[CharacterIndex] == TEXT('\n'))
        {
            ++LineCount;
        }
    }
    const float LabelWidth =
        LineCount > 1 ? 680.0f : 460.0f;
    const float LabelHeight =
        26.0f * static_cast<float>(LineCount);
    const FVector2D TextPosition(
        FMath::Clamp(
            MarkerX - (LabelWidth * 0.5f),
            18.0f,
            FMath::Max(18.0f, WidgetSize.X - LabelWidth - 18.0f)
        ),
        MarkerY + 17.0f
    );

    FSlateDrawElement::MakeText(
        DrawElements,
        LayerId + 1,
        Geometry.ToPaintGeometry(
            FVector2D(LabelWidth, LabelHeight),
            FSlateLayoutTransform(TextPosition)
        ),
        Label,
        FCoreStyle::GetDefaultFontStyle(
            TEXT("Bold"),
            LineCount > 1 ? 15 : 17
        ),
        ESlateDrawEffect::None,
        FLinearColor(1.0f, 0.72f, 0.20f, 0.98f)
    );

    return LayerId + 1;
}

int32 DrawCasualtyWaypoint(
    const FGeometry& Geometry,
    FSlateWindowElementList& DrawElements,
    int32 LayerId,
    float DirectionAngleRadians,
    const FString& Label
)
{
    const FVector2D WidgetSize = Geometry.GetLocalSize();

    if (WidgetSize.X <= 1.0 || WidgetSize.Y <= 1.0)
    {
        return LayerId;
    }

    const float NormalizedBearing = FMath::Clamp(
        DirectionAngleRadians / PI,
        -1.0f,
        1.0f
    );
    const float MarkerX =
        (WidgetSize.X * 0.5f) +
        (NormalizedBearing * WidgetSize.X * 0.42f);
    const float MarkerY = 264.0f;
    const FLinearColor MarkerColor(
        1.0f,
        0.20f,
        0.16f,
        0.98f
    );
    const TArray<FVector2D> MarkerPoints = {
        FVector2D(MarkerX - 10.0f, MarkerY),
        FVector2D(MarkerX + 10.0f, MarkerY),
        FVector2D(MarkerX, MarkerY),
        FVector2D(MarkerX, MarkerY - 10.0f),
        FVector2D(MarkerX, MarkerY + 10.0f)
    };

    DrawPolyline(
        Geometry,
        DrawElements,
        LayerId,
        MarkerPoints,
        MarkerColor,
        4.0f
    );

    const float LabelWidth = 500.0f;
    const FVector2D TextPosition(
        FMath::Clamp(
            MarkerX - (LabelWidth * 0.5f),
            18.0f,
            FMath::Max(
                18.0f,
                WidgetSize.X - LabelWidth - 18.0f
            )
        ),
        MarkerY + 13.0f
    );

    FSlateDrawElement::MakeText(
        DrawElements,
        LayerId + 1,
        Geometry.ToPaintGeometry(
            FVector2D(LabelWidth, 26.0f),
            FSlateLayoutTransform(TextPosition)
        ),
        Label,
        FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 15),
        ESlateDrawEffect::None,
        MarkerColor
    );

    return LayerId + 1;
}

int32 DrawSquadCommandWaypoint(
    const FGeometry& Geometry,
    FSlateWindowElementList& DrawElements,
    int32 LayerId,
    float DirectionAngleRadians,
    const FString& Label,
    const FLinearColor& MarkerColor,
    float MarkerY
)
{
    const FVector2D WidgetSize = Geometry.GetLocalSize();

    if (WidgetSize.X <= 1.0 || WidgetSize.Y <= 1.0)
    {
        return LayerId;
    }

    const float NormalizedBearing = FMath::Clamp(
        DirectionAngleRadians / PI,
        -1.0f,
        1.0f
    );
    const float MarkerX =
        (WidgetSize.X * 0.5f) +
        (NormalizedBearing * WidgetSize.X * 0.42f);
    const TArray<FVector2D> MarkerPoints = {
        FVector2D(MarkerX - 10.0f, MarkerY + 10.0f),
        FVector2D(MarkerX, MarkerY - 9.0f),
        FVector2D(MarkerX + 10.0f, MarkerY + 10.0f),
        FVector2D(MarkerX, MarkerY + 4.0f),
        FVector2D(MarkerX - 10.0f, MarkerY + 10.0f)
    };

    DrawPolyline(
        Geometry,
        DrawElements,
        LayerId,
        MarkerPoints,
        MarkerColor,
        4.0f
    );

    const float LabelWidth = 460.0f;
    const FVector2D TextPosition(
        FMath::Clamp(
            MarkerX - (LabelWidth * 0.5f),
            18.0f,
            FMath::Max(
                18.0f,
                WidgetSize.X - LabelWidth - 18.0f
            )
        ),
        MarkerY + 13.0f
    );

    FSlateDrawElement::MakeText(
        DrawElements,
        LayerId + 1,
        Geometry.ToPaintGeometry(
            FVector2D(LabelWidth, 26.0f),
            FSlateLayoutTransform(TextPosition)
        ),
        Label,
        FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 15),
        ESlateDrawEffect::None,
        MarkerColor
    );

    return LayerId + 1;
}

int32 DrawResupplyWaypoint(
    const FGeometry& Geometry,
    FSlateWindowElementList& DrawElements,
    int32 LayerId,
    float DirectionAngleRadians,
    const FString& Label
)
{
    const FVector2D WidgetSize = Geometry.GetLocalSize();

    if (WidgetSize.X <= 1.0 || WidgetSize.Y <= 1.0)
    {
        return LayerId;
    }

    const float NormalizedBearing = FMath::Clamp(
        DirectionAngleRadians / PI,
        -1.0f,
        1.0f
    );
    const float MarkerX =
        (WidgetSize.X * 0.5f) +
        (NormalizedBearing * WidgetSize.X * 0.42f);
    const float MarkerY = 116.0f;
    const FLinearColor MarkerColor(
        0.20f,
        0.92f,
        0.62f,
        0.98f
    );
    const TArray<FVector2D> MarkerPoints = {
        FVector2D(MarkerX - 9.0f, MarkerY),
        FVector2D(MarkerX, MarkerY + 9.0f),
        FVector2D(MarkerX + 9.0f, MarkerY),
        FVector2D(MarkerX, MarkerY - 9.0f),
        FVector2D(MarkerX - 9.0f, MarkerY)
    };

    DrawPolyline(
        Geometry,
        DrawElements,
        LayerId,
        MarkerPoints,
        MarkerColor,
        3.0f
    );

    const float LabelWidth = 420.0f;
    const FVector2D TextPosition(
        FMath::Clamp(
            MarkerX - (LabelWidth * 0.5f),
            18.0f,
            FMath::Max(18.0f, WidgetSize.X - LabelWidth - 18.0f)
        ),
        MarkerY + 13.0f
    );

    FSlateDrawElement::MakeText(
        DrawElements,
        LayerId + 1,
        Geometry.ToPaintGeometry(
            FVector2D(LabelWidth, 26.0f),
            FSlateLayoutTransform(TextPosition)
        ),
        Label,
        FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 15),
        ESlateDrawEffect::None,
        MarkerColor
    );

    return LayerId + 1;
}

int32 DrawConvoyWaypoint(
    const FGeometry& Geometry,
    FSlateWindowElementList& DrawElements,
    int32 LayerId,
    float DirectionAngleRadians,
    const FString& Label,
    const FLinearColor& MarkerColor
)
{
    const FVector2D WidgetSize = Geometry.GetLocalSize();

    if (WidgetSize.X <= 1.0 || WidgetSize.Y <= 1.0)
    {
        return LayerId;
    }

    const float NormalizedBearing = FMath::Clamp(
        DirectionAngleRadians / PI,
        -1.0f,
        1.0f
    );
    const float MarkerX =
        (WidgetSize.X * 0.5f) +
        (NormalizedBearing * WidgetSize.X * 0.42f);
    const float MarkerY = 164.0f;
    const TArray<FVector2D> MarkerPoints = {
        FVector2D(MarkerX - 11.0f, MarkerY),
        FVector2D(MarkerX, MarkerY + 11.0f),
        FVector2D(MarkerX + 11.0f, MarkerY),
        FVector2D(MarkerX, MarkerY - 11.0f),
        FVector2D(MarkerX - 11.0f, MarkerY)
    };

    DrawPolyline(
        Geometry,
        DrawElements,
        LayerId,
        MarkerPoints,
        MarkerColor,
        4.0f
    );

    DrawPolyline(
        Geometry,
        DrawElements,
        LayerId + 1,
        {
            FVector2D(MarkerX - 7.0f, MarkerY + 7.0f),
            FVector2D(MarkerX + 7.0f, MarkerY - 7.0f)
        },
        MarkerColor,
        3.0f
    );

    const float LabelWidth = 520.0f;
    const FVector2D TextPosition(
        FMath::Clamp(
            MarkerX - (LabelWidth * 0.5f),
            18.0f,
            FMath::Max(18.0f, WidgetSize.X - LabelWidth - 18.0f)
        ),
        MarkerY + 15.0f
    );

    FSlateDrawElement::MakeText(
        DrawElements,
        LayerId + 2,
        Geometry.ToPaintGeometry(
            FVector2D(LabelWidth, 28.0f),
            FSlateLayoutTransform(TextPosition)
        ),
        Label,
        FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 16),
        ESlateDrawEffect::None,
        MarkerColor
    );

    return LayerId + 2;
}

int32 DrawTransportWaypoint(
    const FGeometry& Geometry,
    FSlateWindowElementList& DrawElements,
    int32 LayerId,
    float DirectionAngleRadians,
    const FString& Label,
    bool bImmobilized
)
{
    const FVector2D WidgetSize = Geometry.GetLocalSize();

    if (WidgetSize.X <= 1.0 || WidgetSize.Y <= 1.0)
    {
        return LayerId;
    }

    const float NormalizedBearing = FMath::Clamp(
        DirectionAngleRadians / PI,
        -1.0f,
        1.0f
    );
    const float MarkerX =
        (WidgetSize.X * 0.5f) +
        (NormalizedBearing * WidgetSize.X * 0.42f);
    const float MarkerY = 214.0f;
    const FLinearColor MarkerColor = bImmobilized
        ? FLinearColor(1.0f, 0.28f, 0.08f, 0.98f)
        : FLinearColor(0.30f, 0.72f, 1.0f, 0.98f);
    const TArray<FVector2D> VehicleOutline = {
        FVector2D(MarkerX - 13.0f, MarkerY - 6.0f),
        FVector2D(MarkerX + 13.0f, MarkerY - 6.0f),
        FVector2D(MarkerX + 16.0f, MarkerY + 7.0f),
        FVector2D(MarkerX - 16.0f, MarkerY + 7.0f),
        FVector2D(MarkerX - 13.0f, MarkerY - 6.0f)
    };

    DrawPolyline(
        Geometry,
        DrawElements,
        LayerId,
        VehicleOutline,
        MarkerColor,
        3.0f
    );

    DrawPolyline(
        Geometry,
        DrawElements,
        LayerId + 1,
        {
            FVector2D(MarkerX - 10.0f, MarkerY + 10.0f),
            FVector2D(MarkerX - 6.0f, MarkerY + 10.0f)
        },
        MarkerColor,
        5.0f
    );
    DrawPolyline(
        Geometry,
        DrawElements,
        LayerId + 1,
        {
            FVector2D(MarkerX + 6.0f, MarkerY + 10.0f),
            FVector2D(MarkerX + 10.0f, MarkerY + 10.0f)
        },
        MarkerColor,
        5.0f
    );

    const float LabelWidth = 680.0f;
    const FVector2D TextPosition(
        FMath::Clamp(
            MarkerX - (LabelWidth * 0.5f),
            18.0f,
            FMath::Max(18.0f, WidgetSize.X - LabelWidth - 18.0f)
        ),
        MarkerY + 16.0f
    );

    FSlateDrawElement::MakeText(
        DrawElements,
        LayerId + 2,
        Geometry.ToPaintGeometry(
            FVector2D(LabelWidth, 28.0f),
            FSlateLayoutTransform(TextPosition)
        ),
        Label,
        FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 15),
        ESlateDrawEffect::None,
        MarkerColor
    );

    return LayerId + 2;
}
}

void UBHCombatStatusWidget::SetHealth(
    float CurrentHealth,
    float MaxHealth
)
{
    BHUIStyle::Apply(*this, EBHUIStyleContext::Gameplay);

    const float SafeMaxHealth = FMath::Max(1.0f, MaxHealth);
    CurrentHealthPercentage = FMath::Clamp(
        CurrentHealth / SafeMaxHealth,
        0.0f,
        1.0f
    );

    if (IsValid(HealthBar))
    {
        HealthBar->SetPercent(CurrentHealthPercentage);
    }

    if (IsValid(HealthText))
    {
        HealthText->SetText(
            FText::Format(
                NSLOCTEXT(
                    "BrokenHorizon",
                    "CombatHealthFormat",
                    "{0} / {1}"
                ),
                FText::AsNumber(FMath::RoundToInt(CurrentHealth)),
                FText::AsNumber(FMath::RoundToInt(SafeMaxHealth))
            )
        );
    }
}

void UBHCombatStatusWidget::SetStamina(
    float CurrentStamina,
    float MaxStamina
)
{
    BHUIStyle::Apply(*this, EBHUIStyleContext::Gameplay);

    const float SafeMaxStamina = FMath::Max(1.0f, MaxStamina);

    if (IsValid(StaminaBar))
    {
        StaminaBar->SetPercent(
            FMath::Clamp(
                CurrentStamina / SafeMaxStamina,
                0.0f,
                1.0f
            )
        );
    }

    if (IsValid(StaminaText))
    {
        StaminaText->SetText(
            FText::Format(
                NSLOCTEXT(
                    "BrokenHorizon",
                    "CombatStaminaFormat",
                    "{0} / {1}"
                ),
                FText::AsNumber(FMath::RoundToInt(CurrentStamina)),
                FText::AsNumber(FMath::RoundToInt(SafeMaxStamina))
            )
        );
    }
}

void UBHCombatStatusWidget::NotifyPlayerDamaged(
    float DamageAmount,
    float HealthPercentage,
    FVector DamageSourceDirection,
    AActor* DamageCauser
)
{
    CurrentHealthPercentage = FMath::Clamp(
        HealthPercentage,
        0.0f,
        1.0f
    );
    DamageFeedbackRemaining = FMath::Max(
        DamageFeedbackRemaining,
        FMath::Max(0.05f, DamageFlashDuration)
    );
    DamageDirectionAngleRadians =
        ResolveRelativeDirectionAngle(DamageSourceDirection);

    InvalidateLayoutAndVolatility();

    OnPlayerDamaged(
        DamageAmount,
        CurrentHealthPercentage,
        DamageSourceDirection,
        DamageCauser
    );
}

void UBHCombatStatusWidget::NotifyNearMiss(
    FVector SourceDirection,
    float Intensity
)
{
    const float ClampedIntensity = FMath::Clamp(
        Intensity,
        0.0f,
        1.0f
    );

    if (ClampedIntensity <= KINDA_SMALL_NUMBER)
    {
        return;
    }

    NearMissIntensity = FMath::Max(
        NearMissIntensity,
        ClampedIntensity
    );
    NearMissFeedbackRemaining = FMath::Max(
        NearMissFeedbackRemaining,
        FMath::Max(0.05f, NearMissFeedbackDuration)
    );
    NearMissDirectionAngleRadians =
        ResolveRelativeDirectionAngle(SourceDirection);

    InvalidateLayoutAndVolatility();
    OnNearMiss(SourceDirection, ClampedIntensity);
}

void UBHCombatStatusWidget::SetSuppression(
    float SuppressionPercentage
)
{
    const float ClampedSuppression = FMath::Clamp(
        SuppressionPercentage,
        0.0f,
        1.0f
    );
    if (FMath::IsNearlyEqual(
            CurrentSuppressionPercentage,
            ClampedSuppression,
            0.001f))
    {
        return;
    }

    CurrentSuppressionPercentage = ClampedSuppression;
    InvalidateLayoutAndVolatility();
}

void UBHCombatStatusWidget::NotifyGrenadeThreat(
    AActor* SourceActor,
    FVector SourceDirection,
    float DistanceCentimeters,
    float TimeUntilDetonation
)
{
    if (!IsValid(SourceActor))
    {
        return;
    }

    FGrenadeThreatState* ExistingThreat =
        GrenadeThreats.FindByPredicate(
            [SourceActor](const FGrenadeThreatState& Threat)
            {
                return Threat.SourceActor.Get() == SourceActor;
            }
        );

    if (!ExistingThreat)
    {
        ExistingThreat = &GrenadeThreats.AddDefaulted_GetRef();
        ExistingThreat->SourceActor = SourceActor;
    }

    ExistingThreat->DirectionAngleRadians =
        ResolveRelativeDirectionAngle(SourceDirection);
    ExistingThreat->DistanceCentimeters =
        FMath::Max(0.0f, DistanceCentimeters);
    ExistingThreat->TimeUntilDetonation =
        FMath::Max(0.0f, TimeUntilDetonation);
    ExistingThreat->RefreshRemaining = 0.35f;

    InvalidateLayoutAndVolatility();
}

void UBHCombatStatusWidget::SetInjuryState(
    bool bBleeding,
    float BleedRate,
    bool bInArmInjured,
    bool bInLegInjured,
    int32 FieldDressings
)
{
    bIsBleeding = bBleeding;
    CurrentBleedRate = FMath::Max(0.0f, BleedRate);
    bArmInjured = bInArmInjured;
    bLegInjured = bInLegInjured;
    FieldDressingCount = FMath::Max(0, FieldDressings);
    InvalidateLayoutAndVolatility();
}

void UBHCombatStatusWidget::SetMedicalState(
    int32 Medkits,
    float InHelmetDurabilityPercentage,
    float InBodyArmorDurabilityPercentage,
    bool bTreatmentActive,
    float TreatmentProgress
)
{
    MedkitCount = FMath::Max(0, Medkits);
    HelmetDurabilityPercentage = FMath::Clamp(
        InHelmetDurabilityPercentage,
        0.0f,
        1.0f
    );
    BodyArmorDurabilityPercentage = FMath::Clamp(
        InBodyArmorDurabilityPercentage,
        0.0f,
        1.0f
    );
    bMedicalTreatmentActive = bTreatmentActive;
    MedicalTreatmentProgress = FMath::Clamp(
        TreatmentProgress,
        0.0f,
        1.0f
    );
    InvalidateLayoutAndVolatility();
}

void UBHCombatStatusWidget::SetFragGrenadeCount(
    int32 GrenadeCount
)
{
    FragGrenadeCount = FMath::Max(0, GrenadeCount);
    InvalidateLayoutAndVolatility();
}

void UBHCombatStatusWidget::SetOperationWaypoint(
    bool bVisible,
    bool bOperationActive,
    const FText& SectorDisplayName,
    const FText& OperationStatus,
    const FVector& WorldDirection,
    float DistanceCentimeters,
    bool bTravelEstimateVisible,
    float EstimatedTravelMinutes,
    float EstimatedRangeKilometers,
    bool bFuelShortfall
)
{
    bOperationWaypointVisible =
        bVisible && !SectorDisplayName.IsEmpty();
    bOperationWaypointActive =
        bOperationWaypointVisible && bOperationActive;
    OperationSectorDisplayName = SectorDisplayName;
    OperationStatusText = OperationStatus;
    OperationDistanceCentimeters =
        FMath::Max(0.0f, DistanceCentimeters);
    OperationDirectionAngleRadians =
        ResolveRelativeDirectionAngle(WorldDirection);
    bOperationTravelEstimateVisible =
        bOperationWaypointVisible && bTravelEstimateVisible;
    OperationEstimatedTravelMinutes =
        FMath::Max(0.0f, EstimatedTravelMinutes);
    OperationEstimatedRangeKilometers =
        FMath::Max(0.0f, EstimatedRangeKilometers);
    bOperationFuelShortfall =
        bOperationTravelEstimateVisible && bFuelShortfall;
    InvalidateLayoutAndVolatility();
}

void UBHCombatStatusWidget::SetOperationArrivalDeadlineRisk(
    bool bAtRisk
)
{
    bOperationArrivalDeadlineRisk =
        bOperationTravelEstimateVisible && bAtRisk;
    InvalidateLayoutAndVolatility();
}

void UBHCombatStatusWidget::SetCasualtyWaypoint(
    bool bVisible,
    int32 IncapacitatedOperatives,
    const FVector& WorldDirection,
    float DistanceCentimeters,
    float RecoverySecondsRemaining
)
{
    CasualtyWaypointOperativeCount =
        FMath::Max(0, IncapacitatedOperatives);
    bCasualtyWaypointVisible =
        bVisible && CasualtyWaypointOperativeCount > 0;
    CasualtyWaypointDistanceCentimeters =
        FMath::Max(0.0f, DistanceCentimeters);
    CasualtyWaypointDirectionAngleRadians =
        ResolveRelativeDirectionAngle(WorldDirection);
    CasualtyWaypointRecoverySecondsRemaining =
        FMath::Max(0.0f, RecoverySecondsRemaining);
    InvalidateLayoutAndVolatility();
}

void UBHCombatStatusWidget::SetSquadCommandWaypoint(
    bool bVisible,
    const FVector& WorldDirection,
    float DistanceCentimeters
)
{
    bSquadCommandWaypointVisible = bVisible;
    SquadCommandWaypointDistanceCentimeters =
        FMath::Max(0.0f, DistanceCentimeters);
    SquadCommandWaypointDirectionAngleRadians =
        ResolveRelativeDirectionAngle(WorldDirection);
    InvalidateLayoutAndVolatility();
}

void UBHCombatStatusWidget::SetResupplyWaypoint(
    bool bVisible,
    const FText& SectorDisplayName,
    const FVector& WorldDirection,
    float DistanceCentimeters
)
{
    bResupplyWaypointVisible =
        bVisible && !SectorDisplayName.IsEmpty();
    ResupplySectorDisplayName = SectorDisplayName;
    ResupplyDistanceCentimeters =
        FMath::Max(0.0f, DistanceCentimeters);
    ResupplyDirectionAngleRadians =
        ResolveRelativeDirectionAngle(WorldDirection);
    InvalidateLayoutAndVolatility();
}

void UBHCombatStatusWidget::SetLogisticsWaypoint(
    bool bVisible,
    const FText& SectorDisplayName,
    const FVector& WorldDirection,
    float DistanceCentimeters,
    float CargoSupply
)
{
    bLogisticsWaypointVisible =
        bVisible &&
        !SectorDisplayName.IsEmpty() &&
        CargoSupply > KINDA_SMALL_NUMBER;
    LogisticsSectorDisplayName = SectorDisplayName;
    LogisticsDistanceCentimeters =
        FMath::Max(0.0f, DistanceCentimeters);
    LogisticsDirectionAngleRadians =
        ResolveRelativeDirectionAngle(WorldDirection);
    LogisticsCargoSupply = FMath::Max(0.0f, CargoSupply);
    InvalidateLayoutAndVolatility();
}

void UBHCombatStatusWidget::SetSalvageWaypoint(
    bool bVisible,
    const FVector& WorldDirection,
    float DistanceCentimeters,
    float RecoverableSupply
)
{
    bSalvageWaypointVisible =
        bVisible &&
        RecoverableSupply > KINDA_SMALL_NUMBER;
    SalvageDistanceCentimeters =
        FMath::Max(0.0f, DistanceCentimeters);
    SalvageDirectionAngleRadians =
        ResolveRelativeDirectionAngle(WorldDirection);
    SalvageSupply = FMath::Max(
        0.0f,
        RecoverableSupply
    );
    InvalidateLayoutAndVolatility();
}

void UBHCombatStatusWidget::SetConvoyWaypoint(
    bool bVisible,
    EBHWarFaction ConvoyOwner,
    const FVector& WorldDirection,
    float DistanceCentimeters,
    float SupplyPayload,
    float IntegrityPercentage
)
{
    bConvoyWaypointVisible = bVisible;
    ConvoyWaypointOwner = ConvoyOwner;
    ConvoyDistanceCentimeters =
        FMath::Max(0.0f, DistanceCentimeters);
    ConvoySupplyPayload = FMath::Max(0.0f, SupplyPayload);
    ConvoyIntegrityPercentage = FMath::Clamp(
        IntegrityPercentage,
        0.0f,
        1.0f
    );
    ConvoyDirectionAngleRadians =
        ResolveRelativeDirectionAngle(WorldDirection);
    InvalidateLayoutAndVolatility();
}

void UBHCombatStatusWidget::SetEngineeringChargeState(
    int32 CarriedCharges,
    int32 ActiveCharges
)
{
    EngineeringChargeCount = FMath::Max(0, CarriedCharges);
    ActiveEngineeringChargeCount = FMath::Max(0, ActiveCharges);
    InvalidateLayoutAndVolatility();
}

void UBHCombatStatusWidget::SetCarryLoad(
    float TotalKilograms,
    EBHCarryLoadState LoadState,
    float MovementSpeedMultiplier,
    float StaminaDrainMultiplier
)
{
    const float NewLoad = FMath::Max(0.0f, TotalKilograms);
    const float NewSpeed = FMath::Clamp(MovementSpeedMultiplier, 0.0f, 1.0f);
    const float NewDrain = FMath::Max(1.0f, StaminaDrainMultiplier);
    if (!FMath::IsNearlyEqual(CarryLoadKilograms, NewLoad, 0.01f) ||
        CarryLoadState != LoadState ||
        !FMath::IsNearlyEqual(CarryMovementSpeedMultiplier, NewSpeed, 0.001f) ||
        !FMath::IsNearlyEqual(CarryStaminaDrainMultiplier, NewDrain, 0.001f))
    {
        CarryLoadKilograms = NewLoad;
        CarryLoadState = LoadState;
        CarryMovementSpeedMultiplier = NewSpeed;
        CarryStaminaDrainMultiplier = NewDrain;
        InvalidateLayoutAndVolatility();
    }
}

void UBHCombatStatusWidget::SetSquadPingWaypoint(
    bool bVisible,
    const FVector& WorldDirection,
    float DistanceCentimeters,
    const FString& ContextLabel,
    const FString& IssuerLabel,
    bool bTrackedTarget,
    bool bLineOfSightVisible
)
{
    bSquadPingWaypointVisible =
        bVisible && !ContextLabel.IsEmpty();
    SquadPingWaypointDistanceCentimeters =
        FMath::Max(0.0f, DistanceCentimeters);
    SquadPingWaypointDirectionAngleRadians =
        ResolveRelativeDirectionAngle(WorldDirection);
    SquadPingContextLabel = ContextLabel;
    SquadPingIssuerLabel = IssuerLabel;
    bSquadPingTrackedTarget = bTrackedTarget;
    bSquadPingLineOfSightVisible =
        !bTrackedTarget || bLineOfSightVisible;
    InvalidateLayoutAndVolatility();
}

void UBHCombatStatusWidget::SetConvoyOperationProfile(
    EBHRouteOperationVariation Variation,
    float DeadlineSecondsRemaining
)
{
    ConvoyOperationVariation = Variation;
    ConvoyDeadlineSecondsRemaining = FMath::Max(
        0.0f,
        DeadlineSecondsRemaining
    );
    InvalidateLayoutAndVolatility();
}

void UBHCombatStatusWidget::SetTransportWaypoint(
    bool bVisible,
    const FVector& WorldDirection,
    float DistanceCentimeters,
    float FuelPercentage,
    float HullPercentage,
    bool bImmobilized
)
{
    bTransportWaypointVisible = bVisible;
    TransportWaypointDistanceCentimeters =
        FMath::Max(0.0f, DistanceCentimeters);
    TransportWaypointFuelPercentage = FMath::Clamp(
        FuelPercentage,
        0.0f,
        1.0f
    );
    TransportWaypointHullPercentage = FMath::Clamp(
        HullPercentage,
        0.0f,
        1.0f
    );
    bTransportWaypointImmobilized = bImmobilized;
    TransportWaypointDirectionAngleRadians =
        ResolveRelativeDirectionAngle(WorldDirection);
    InvalidateLayoutAndVolatility();
}

void UBHCombatStatusWidget::SetVehicleReadiness(
    bool bVisible,
    float FuelPercentage,
    float HullPercentage,
    float SpeedKPH,
    bool bImmobilized
)
{
    bVehicleReadinessVisible = bVisible;
    VehicleFuelPercentage = FMath::Clamp(
        FuelPercentage,
        0.0f,
        1.0f
    );
    VehicleHullPercentage = FMath::Clamp(
        HullPercentage,
        0.0f,
        1.0f
    );
    VehicleSpeedKPH = FMath::Max(0.0f, SpeedKPH);
    bVehicleImmobilized = bVisible && bImmobilized;
    InvalidateLayoutAndVolatility();
}

void UBHCombatStatusWidget::SetStrategicSituation(
    bool bVisible,
    const FText& SectorDisplayName,
    EBHWarFaction SectorOwner,
    float SectorSupply,
    float SupplyFlowPerTurn,
    int32 WarTurn,
    int32 ConstructedFortifications,
    int32 FortificationCapacity,
    float FortificationCoverage,
    float FortificationDefenseMultiplier,
    bool bConnectedFortifications
)
{
    bStrategicSituationVisible =
        bVisible && !SectorDisplayName.IsEmpty();
    StrategicSectorDisplayName = SectorDisplayName;
    StrategicSectorOwner = SectorOwner;
    StrategicSectorSupply = FMath::Clamp(
        SectorSupply,
        0.0f,
        100.0f
    );
    StrategicSupplyFlowPerTurn = SupplyFlowPerTurn;
    StrategicWarTurn = FMath::Max(0, WarTurn);
    StrategicFortificationConstructed = FMath::Max(0, ConstructedFortifications);
    StrategicFortificationCapacity = FMath::Max(1, FortificationCapacity);
    StrategicFortificationCoverage = FMath::Max(
        0.0f,
        FortificationCoverage
    );
    StrategicFortificationDefenseMultiplier = FMath::Max(
        1.0f,
        FortificationDefenseMultiplier
    );
    bFortificationsConnected = bConnectedFortifications;
    InvalidateLayoutAndVolatility();
}

void UBHCombatStatusWidget::SetEnemyResponsePressure(
    float ResponsePressure
)
{
    StrategicEnemyResponsePressure = FMath::Clamp(
        ResponsePressure,
        0.0f,
        100.0f
    );
    InvalidateLayoutAndVolatility();
}

void UBHCombatStatusWidget::SetCivilianSupport(
    float CivilianSupport
)
{
    StrategicCivilianSupport = FMath::Clamp(
        CivilianSupport,
        0.0f,
        100.0f
    );
    InvalidateLayoutAndVolatility();
}

void UBHCombatStatusWidget::SetFieldReconStatus(
    bool bActive,
    float IntelConfidence,
    float MovementProgress,
    float MovementRequired,
    float ObservationProgress,
    float ObservationRequired,
    float ReportCooldownRemaining
)
{
    bFieldReconActive = bActive;
    StrategicIntelConfidence = FMath::Clamp(
        IntelConfidence,
        0.0f,
        100.0f
    );
    ReconMovementRequired = FMath::Max(1.0f, MovementRequired);
    ReconMovementProgress = FMath::Clamp(
        MovementProgress,
        0.0f,
        ReconMovementRequired
    );
    ReconObservationRequired = FMath::Max(
        1.0f,
        ObservationRequired
    );
    ReconObservationProgress = FMath::Clamp(
        ObservationProgress,
        0.0f,
        ReconObservationRequired
    );
    ReconReportCooldownRemaining = FMath::Max(
        0.0f,
        ReportCooldownRemaining
    );
    InvalidateLayoutAndVolatility();
}

void UBHCombatStatusWidget::SetFieldSquadStatus(
    bool bVisible,
    int32 LivingOperatives,
    int32 MaximumOperatives,
    bool bHolding,
    bool bEmbarked
)
{
    const int32 SafeMaximum = FMath::Max(1, MaximumOperatives);
    const int32 SafeLiving = FMath::Clamp(
        LivingOperatives,
        0,
        SafeMaximum
    );
    const bool bSafeVisible = bVisible && SafeLiving > 0;
    if (bFieldSquadStatusVisible == bSafeVisible &&
        LivingFieldSquadOperatives == SafeLiving &&
        MaximumFieldSquadOperatives == SafeMaximum &&
        bFieldSquadHolding == bHolding &&
        bFieldSquadEmbarked == bEmbarked)
    {
        return;
    }

    bFieldSquadStatusVisible = bSafeVisible;
    LivingFieldSquadOperatives = SafeLiving;
    MaximumFieldSquadOperatives = SafeMaximum;
    bFieldSquadHolding = bHolding;
    bFieldSquadEmbarked = bEmbarked;
    InvalidateLayoutAndVolatility();
}

void UBHCombatStatusWidget::SetFieldSquadServiceNeeds(
    int32 MembersNeedingService,
    int32 MembersRequiringEvacuation
)
{
    const int32 SafeMembersNeedingService = FMath::Clamp(
        MembersNeedingService,
        0,
        LivingFieldSquadOperatives
    );
    const int32 SafeMembersRequiringEvacuation = FMath::Clamp(
        MembersRequiringEvacuation,
        0,
        SafeMembersNeedingService
    );

    if (FieldSquadMembersNeedingService ==
            SafeMembersNeedingService &&
        FieldSquadMembersRequiringEvacuation ==
            SafeMembersRequiringEvacuation)
    {
        return;
    }

    FieldSquadMembersNeedingService =
        SafeMembersNeedingService;
    FieldSquadMembersRequiringEvacuation =
        SafeMembersRequiringEvacuation;
    InvalidateLayoutAndVolatility();
}

void UBHCombatStatusWidget::SetFieldSquadReadiness(
    float AverageReadiness,
    float LowestReadiness
)
{
    const float SafeAverageReadiness = FMath::Clamp(
        AverageReadiness,
        0.0f,
        1.0f
    );
    const float SafeLowestReadiness = FMath::Min(
        SafeAverageReadiness,
        FMath::Clamp(LowestReadiness, 0.0f, 1.0f)
    );

    if (FMath::IsNearlyEqual(
            FieldSquadAverageReadiness,
            SafeAverageReadiness
        ) &&
        FMath::IsNearlyEqual(
            FieldSquadLowestReadiness,
            SafeLowestReadiness
        ))
    {
        return;
    }

    FieldSquadAverageReadiness = SafeAverageReadiness;
    FieldSquadLowestReadiness = SafeLowestReadiness;
    InvalidateLayoutAndVolatility();
}

void UBHCombatStatusWidget::SetFieldSquadContextStatus(
    const FString& ActionLabel,
    const FString& TargetLabel,
    bool bReachedTarget
)
{
    const FString NewStatusLine =
        BuildFieldSquadContextStatusLine(
            ActionLabel,
            TargetLabel,
            bReachedTarget
        );

    if (FieldSquadContextStatusLine == NewStatusLine)
    {
        return;
    }

    FieldSquadContextStatusLine = NewStatusLine;
    InvalidateLayoutAndVolatility();
}

FString UBHCombatStatusWidget::BuildFieldSquadStatusLabel(
    int32 LivingOperatives,
    int32 MaximumOperatives,
    bool bHolding,
    bool bEmbarked,
    int32 MembersNeedingService,
    int32 MembersRequiringEvacuation,
    float AverageReadiness,
    float LowestReadiness,
    const FString& ContextStatusLine,
    const FString& SquadOrderPrompt
)
{
    const int32 SafeMaximum = FMath::Max(1, MaximumOperatives);
    const int32 SafeLiving = FMath::Clamp(
        LivingOperatives,
        0,
        SafeMaximum
    );
    const int32 SafeMembersNeedingService = FMath::Clamp(
        MembersNeedingService,
        0,
        SafeLiving
    );
    const int32 SafeMembersRequiringEvacuation = FMath::Clamp(
        MembersRequiringEvacuation,
        0,
        SafeMembersNeedingService
    );
    const FString SafeSquadOrderPrompt = SquadOrderPrompt.IsEmpty()
        ? FString(TEXT("C"))
        : SquadOrderPrompt;
    const FString StatusLine = bEmbarked
        ? FString(TEXT("MOUNTED // VEHICLE PROTECTED"))
        : bHolding
            ? FString::Printf(
                TEXT("ORDER HOLD // [%s] FOLLOW"),
                *SafeSquadOrderPrompt
            )
            : FString::Printf(
                TEXT("ORDER FOLLOW // AIM + [%s] MOVE/HOLD"),
                *SafeSquadOrderPrompt
            );

    const FString ConditionLine =
        SafeMembersRequiringEvacuation > 0
        ? FString::Printf(
            TEXT("FIRETEAM // %d/%d // MEDEVAC %d"),
            SafeLiving,
            SafeMaximum,
            SafeMembersRequiringEvacuation
        )
        : SafeMembersNeedingService > 0
        ? FString::Printf(
            TEXT("FIRETEAM // %d/%d // SERVICE %d"),
            SafeLiving,
            SafeMaximum,
            SafeMembersNeedingService
        )
        : FString::Printf(
            TEXT("FIRETEAM // %d/%d // READY"),
            SafeLiving,
            SafeMaximum
        );

    if (AverageReadiness < 0.0f || LowestReadiness < 0.0f)
    {
        return ContextStatusLine.IsEmpty()
            ? FString::Printf(
                TEXT("%s\n%s"),
                *ConditionLine,
                *StatusLine
            )
            : FString::Printf(
                TEXT("%s\n%s\n%s"),
                *ConditionLine,
                *ContextStatusLine,
                *StatusLine
            );
    }

    const int32 AveragePercent = FMath::RoundToInt(
        FMath::Clamp(AverageReadiness, 0.0f, 1.0f) * 100.0f
    );
    const int32 LowestPercent = FMath::Min(
        AveragePercent,
        FMath::RoundToInt(
            FMath::Clamp(LowestReadiness, 0.0f, 1.0f) * 100.0f
        )
    );

    return ContextStatusLine.IsEmpty()
        ? FString::Printf(
            TEXT("%s\nCOHESION %d%% // LOWEST %d%%\n%s"),
            *ConditionLine,
            AveragePercent,
            LowestPercent,
            *StatusLine
        )
        : FString::Printf(
            TEXT("%s\nCOHESION %d%% // LOWEST %d%%\n%s\n%s"),
            *ConditionLine,
            AveragePercent,
            LowestPercent,
            *ContextStatusLine,
            *StatusLine
        );
}

FString UBHCombatStatusWidget::BuildFieldSquadContextStatusLine(
    const FString& ActionLabel,
    const FString& TargetLabel,
    bool bReachedTarget
)
{
    FString SafeAction = ActionLabel.TrimStartAndEnd().ToUpper();
    FString SafeTarget = TargetLabel.TrimStartAndEnd().ToUpper();

    if (SafeAction.IsEmpty() || SafeTarget.IsEmpty())
    {
        return FString();
    }

    constexpr int32 MaximumContextLabelLength = 28;
    SafeAction.LeftInline(
        MaximumContextLabelLength,
        EAllowShrinking::No
    );
    SafeTarget.LeftInline(
        MaximumContextLabelLength,
        EAllowShrinking::No
    );
    return FString::Printf(
        TEXT("CONTEXT %s // %s // %s"),
        *SafeAction,
        bReachedTarget ? TEXT("ACTIVE") : TEXT("MOVING"),
        *SafeTarget
    );
}

FString UBHCombatStatusWidget::BuildSquadCommandWaypointLabel(
    float DistanceCentimeters,
    const FString& SquadOrderPrompt
)
{
    const float SafeDistanceCentimeters =
        FMath::Max(0.0f, DistanceCentimeters);
    const FString SafeSquadOrderPrompt = SquadOrderPrompt.IsEmpty()
        ? FString(TEXT("C"))
        : SquadOrderPrompt;

    return SafeDistanceCentimeters >= 100000.0f
        ? FString::Printf(
            TEXT("SQUAD HOLD POINT // %.1f KM // [%s] FOLLOW"),
            SafeDistanceCentimeters / 100000.0f,
            *SafeSquadOrderPrompt
        )
        : FString::Printf(
            TEXT("SQUAD HOLD POINT // %.0f M // [%s] FOLLOW"),
            SafeDistanceCentimeters / 100.0f,
            *SafeSquadOrderPrompt
        );
}

FString UBHCombatStatusWidget::BuildSquadPingWaypointLabel(
    float DistanceCentimeters,
    const FString& ContextLabel,
    const FString& IssuerLabel,
    bool bTrackedTarget,
    bool bLineOfSightVisible
)
{
    FString SafeContext = ContextLabel.ToUpper();
    FString SafeIssuer = IssuerLabel.ToUpper();
    SafeContext.LeftInline(24, EAllowShrinking::No);
    SafeIssuer.LeftInline(18, EAllowShrinking::No);
    const float SafeDistance = FMath::Max(0.0f, DistanceCentimeters);
    const FString DistanceText = SafeDistance >= 100000.0f
        ? FString::Printf(TEXT("%.1f KM"), SafeDistance / 100000.0f)
        : FString::Printf(TEXT("%.0f M"), SafeDistance / 100.0f);
    const TCHAR* TrackingState = !bTrackedTarget
        ? TEXT("")
        : bLineOfSightVisible
            ? TEXT(" // TRACKED")
            : TEXT(" // LAST KNOWN");
    return FString::Printf(
        TEXT("PING // %s // %s // %s%s"),
        SafeContext.IsEmpty() ? TEXT("LOCATION") : *SafeContext,
        *DistanceText,
        SafeIssuer.IsEmpty() ? TEXT("SQUAD") : *SafeIssuer,
        TrackingState
    );
}

bool UBHCombatStatusWidget::IsSquadPingTargetVisible(
    bool bBlockingHit,
    const AActor* HitActor,
    const AActor* TrackedActor
)
{
    return IsValid(TrackedActor) &&
        (!bBlockingHit || HitActor == TrackedActor);
}

void UBHCombatStatusWidget::NativeTick(
    const FGeometry& MyGeometry,
    float InDeltaTime
)
{
    Super::NativeTick(MyGeometry, InDeltaTime);

    const float SafeDeltaTime = FMath::Max(0.0f, InDeltaTime);
    DamageFeedbackRemaining = FMath::Max(
        0.0f,
        DamageFeedbackRemaining - SafeDeltaTime
    );
    NearMissFeedbackRemaining = FMath::Max(
        0.0f,
        NearMissFeedbackRemaining - SafeDeltaTime
    );
    const bool bHadGrenadeThreats = !GrenadeThreats.IsEmpty();

    for (int32 ThreatIndex = GrenadeThreats.Num() - 1;
        ThreatIndex >= 0;
        --ThreatIndex)
    {
        FGrenadeThreatState& Threat = GrenadeThreats[ThreatIndex];
        Threat.TimeUntilDetonation = FMath::Max(
            0.0f,
            Threat.TimeUntilDetonation - SafeDeltaTime
        );
        Threat.RefreshRemaining = FMath::Max(
            0.0f,
            Threat.RefreshRemaining - SafeDeltaTime
        );

        if (!Threat.SourceActor.IsValid() ||
            Threat.RefreshRemaining <= 0.0f)
        {
            GrenadeThreats.RemoveAtSwap(ThreatIndex);
        }
    }

    LowHealthPulseTime += SafeDeltaTime;
    InjuryPulseTime += SafeDeltaTime;
    GrenadeWarningPulseTime += SafeDeltaTime;

    if (NearMissFeedbackRemaining <= 0.0f)
    {
        NearMissIntensity = 0.0f;
    }

    const bool bLowHealth =
        CurrentHealthPercentage > 0.0f &&
        CurrentHealthPercentage <= LowHealthThreshold;

    if (IsValid(HealthBar))
    {
        const float Pulse =
            0.5f + (0.5f * FMath::Sin(LowHealthPulseTime * 6.0f));
        HealthBar->SetRenderOpacity(
            bLowHealth
                ? FMath::Lerp(0.55f, 1.0f, Pulse)
                : 1.0f
        );
    }

    if (DamageFeedbackRemaining > 0.0f ||
        NearMissFeedbackRemaining > 0.0f ||
        bHadGrenadeThreats ||
        !GrenadeThreats.IsEmpty() ||
        bLowHealth ||
        bIsBleeding ||
        bMedicalTreatmentActive)
    {
        InvalidateLayoutAndVolatility();
    }
}

int32 UBHCombatStatusWidget::NativePaint(
    const FPaintArgs& Args,
    const FGeometry& AllottedGeometry,
    const FSlateRect& MyCullingRect,
    FSlateWindowElementList& OutDrawElements,
    int32 LayerId,
    const FWidgetStyle& InWidgetStyle,
    bool bParentEnabled
) const
{
    int32 MaxLayer = Super::NativePaint(
        Args,
        AllottedGeometry,
        MyCullingRect,
        OutDrawElements,
        LayerId,
        InWidgetStyle,
        bParentEnabled
    );
    const UGameInstance* GameInstance = GetGameInstance();
    const UBHUserSettingsSubsystem* UserSettings = GameInstance
        ? GameInstance->GetSubsystem<UBHUserSettingsSubsystem>()
        : nullptr;
    const auto GetBindingPrompt = [UserSettings](
        FName BindingID,
        const TCHAR* Fallback
    )
    {
        const FString Prompt = IsValid(UserSettings)
            ? UserSettings->GetInputBindingPrompt(BindingID)
            : FString();
        return Prompt.IsEmpty() ? FString(Fallback) : Prompt;
    };
    const FString SquadOrderPrompt = GetBindingPrompt(
        FName(TEXT("SquadOrder")),
        TEXT("C")
    );

    if (bOperationWaypointVisible)
    {
        FString WaypointLabel;
        const FString TravelLabel =
            bOperationTravelEstimateVisible
                ? FString::Printf(
                    TEXT(" // ETA %d MIN"),
                    FMath::Max(
                        1,
                        FMath::CeilToInt(
                            OperationEstimatedTravelMinutes
                        )
                    )
                )
                : FString();

        if (OperationStatusText.IsEmpty())
        {
            WaypointLabel = FString::Printf(
                TEXT("OPERATION // %s // %.1f KM%s"),
                *OperationSectorDisplayName.ToString(),
                OperationDistanceCentimeters / 100000.0f,
                *TravelLabel
            );
        }
        else if (bOperationFuelShortfall)
        {
            WaypointLabel = FString::Printf(
                TEXT(
                    "OPERATION // %s // %.1f KM%s\n"
                    "FUEL SHORTFALL // RANGE %.1f KM // RESUPPLY%s"
                ),
                *OperationSectorDisplayName.ToString(),
                OperationDistanceCentimeters / 100000.0f,
                *TravelLabel,
                OperationEstimatedRangeKilometers,
                bOperationArrivalDeadlineRisk
                    ? TEXT("\nLATE ARRIVAL RISK // CHANGE VEHICLE OR ROUTE")
                    : TEXT("")
            );
        }
        else if (bOperationArrivalDeadlineRisk)
        {
            WaypointLabel = FString::Printf(
                TEXT(
                    "OPERATION // %s // %.1f KM%s\n"
                    "LATE ARRIVAL RISK // ETA EXCEEDS WINDOW\n%s"
                ),
                *OperationSectorDisplayName.ToString(),
                OperationDistanceCentimeters / 100000.0f,
                *TravelLabel,
                *OperationStatusText.ToString()
            );
        }
        else if (bOperationWaypointActive)
        {
            WaypointLabel = FString::Printf(
                TEXT("ACTIVE OP // %s // %.1f KM%s\n%s"),
                *OperationSectorDisplayName.ToString(),
                OperationDistanceCentimeters / 100000.0f,
                *TravelLabel,
                *OperationStatusText.ToString()
            );
        }
        else
        {
            WaypointLabel = FString::Printf(
                TEXT("OPERATION // %s // %.1f KM%s\n%s"),
                *OperationSectorDisplayName.ToString(),
                OperationDistanceCentimeters / 100000.0f,
                *TravelLabel,
                *OperationStatusText.ToString()
            );
        }

        MaxLayer = DrawOperationWaypoint(
            AllottedGeometry,
            OutDrawElements,
            MaxLayer + 1,
            OperationDirectionAngleRadians,
            WaypointLabel
        );
    }

    if (bCasualtyWaypointVisible)
    {
        const int32 RecoveryTotalSeconds = FMath::CeilToInt(
            CasualtyWaypointRecoverySecondsRemaining
        );
        const int32 RecoveryMinutes =
            RecoveryTotalSeconds / 60;
        const int32 RecoverySeconds =
            RecoveryTotalSeconds % 60;
        const FString WaypointLabel = FString::Printf(
            TEXT(
                "OPERATIVE DOWN x%d // %.1f KM // "
                "%d:%02d\nSTABILIZE WITH FIELD DRESSING"
            ),
            CasualtyWaypointOperativeCount,
            CasualtyWaypointDistanceCentimeters / 100000.0f,
            RecoveryMinutes,
            RecoverySeconds
        );
        MaxLayer = DrawCasualtyWaypoint(
            AllottedGeometry,
            OutDrawElements,
            MaxLayer + 1,
            CasualtyWaypointDirectionAngleRadians,
            WaypointLabel
        );
    }

    if (bSquadCommandWaypointVisible)
    {
        MaxLayer = DrawSquadCommandWaypoint(
            AllottedGeometry,
            OutDrawElements,
            MaxLayer + 1,
            SquadCommandWaypointDirectionAngleRadians,
            BuildSquadCommandWaypointLabel(
                SquadCommandWaypointDistanceCentimeters,
                SquadOrderPrompt
            ),
            FLinearColor(0.32f, 0.78f, 1.0f, 0.98f),
            314.0f
        );
    }

    if (bSquadPingWaypointVisible)
    {
        MaxLayer = DrawSquadCommandWaypoint(
            AllottedGeometry,
            OutDrawElements,
            MaxLayer + 1,
            SquadPingWaypointDirectionAngleRadians,
            BuildSquadPingWaypointLabel(
                SquadPingWaypointDistanceCentimeters,
                SquadPingContextLabel,
                SquadPingIssuerLabel,
                bSquadPingTrackedTarget,
                bSquadPingLineOfSightVisible
            ),
            bSquadPingLineOfSightVisible
                ? FLinearColor(1.0f, 0.72f, 0.12f, 0.98f)
                : FLinearColor(0.82f, 0.58f, 0.18f, 0.72f),
            SquadPingMarkerY
        );
    }

    if (bResupplyWaypointVisible)
    {
        const FString WaypointLabel = FString::Printf(
            TEXT("RESUPPLY // %s // %.1f KM"),
            *ResupplySectorDisplayName.ToString(),
            ResupplyDistanceCentimeters / 100000.0f
        );
        MaxLayer = DrawResupplyWaypoint(
            AllottedGeometry,
            OutDrawElements,
            MaxLayer + 1,
            ResupplyDirectionAngleRadians,
            WaypointLabel
        );
    }

    if (bLogisticsWaypointVisible)
    {
        const FString WaypointLabel = FString::Printf(
            TEXT("LOGISTICS // DELIVER %.0f // %s // %.1f KM"),
            LogisticsCargoSupply,
            *LogisticsSectorDisplayName.ToString(),
            LogisticsDistanceCentimeters / 100000.0f
        );
        MaxLayer = DrawResupplyWaypoint(
            AllottedGeometry,
            OutDrawElements,
            MaxLayer + 1,
            LogisticsDirectionAngleRadians,
            WaypointLabel
        );
    }

    if (bSalvageWaypointVisible)
    {
        const FString DistanceLabel =
            SalvageDistanceCentimeters >= 100000.0f
                ? FString::Printf(
                    TEXT("%.1f KM"),
                    SalvageDistanceCentimeters / 100000.0f
                )
                : FString::Printf(
                    TEXT("%.0f M"),
                    SalvageDistanceCentimeters / 100.0f
                );
        const FString WaypointLabel = FString::Printf(
            TEXT(
                "CONVOY WRECK // RECOVER %.0f SUPPLY // %s"
            ),
            SalvageSupply,
            *DistanceLabel
        );
        MaxLayer = DrawResupplyWaypoint(
            AllottedGeometry,
            OutDrawElements,
            MaxLayer + 1,
            SalvageDirectionAngleRadians,
            WaypointLabel
        );
    }

    if (bConvoyWaypointVisible)
    {
        const bool bFriendlyConvoy =
            ConvoyWaypointOwner == EBHWarFaction::Friendly;
        const FString DistanceLabel =
            ConvoyDistanceCentimeters >= 100000.0f
                ? FString::Printf(
                    TEXT("%.1f KM"),
                    ConvoyDistanceCentimeters / 100000.0f
                )
                : FString::Printf(
                    TEXT("%.0f M"),
                    ConvoyDistanceCentimeters / 100.0f
                );
        const TCHAR* VariationLabel = TEXT("STANDARD ROUTE");
        switch (ConvoyOperationVariation)
        {
        case EBHRouteOperationVariation::Ambush:
            VariationLabel = TEXT("HEAVY AMBUSH");
            break;
        case EBHRouteOperationVariation::DamagedVehicle:
            VariationLabel = TEXT("DAMAGED VEHICLE");
            break;
        case EBHRouteOperationVariation::TimeCritical:
            VariationLabel = TEXT("TIME CRITICAL");
            break;
        default:
            break;
        }
        const FString DeadlineLabel =
            ConvoyOperationVariation ==
                    EBHRouteOperationVariation::TimeCritical
                ? FString::Printf(
                    TEXT(" // %02d:%02d REMAINING"),
                    FMath::FloorToInt(ConvoyDeadlineSecondsRemaining) / 60,
                    FMath::FloorToInt(ConvoyDeadlineSecondsRemaining) % 60
                )
                : FString();
        const FString WaypointLabel = bFriendlyConvoy
            ? FString::Printf(
                TEXT(
                    "FRIENDLY CONVOY // %s%s // %.0f SUPPLY // "
                    "%d%% INTEGRITY // %s // DEFEND"
                ),
                VariationLabel,
                *DeadlineLabel,
                ConvoySupplyPayload,
                FMath::RoundToInt(
                    ConvoyIntegrityPercentage * 100.0f
                ),
                *DistanceLabel
            )
            : FString::Printf(
                TEXT(
                    "HOSTILE CONVOY // %s // %.0f SUPPLY // "
                    "%d%% INTEGRITY // %s // INTERDICT"
                ),
                VariationLabel,
                ConvoySupplyPayload,
                FMath::RoundToInt(
                    ConvoyIntegrityPercentage * 100.0f
                ),
                *DistanceLabel
            );
        const FLinearColor MarkerColor = bFriendlyConvoy
            ? FLinearColor(0.14f, 0.82f, 1.0f, 0.98f)
            : FLinearColor(1.0f, 0.24f, 0.08f, 0.98f);
        MaxLayer = DrawConvoyWaypoint(
            AllottedGeometry,
            OutDrawElements,
            MaxLayer + 1,
            ConvoyDirectionAngleRadians,
            WaypointLabel,
            MarkerColor
        );
    }

    if (bTransportWaypointVisible)
    {
        const FString DistanceLabel =
            TransportWaypointDistanceCentimeters >= 100000.0f
                ? FString::Printf(
                    TEXT("%.1f KM"),
                    TransportWaypointDistanceCentimeters /
                        100000.0f
                )
                : FString::Printf(
                    TEXT("%.0f M"),
                    TransportWaypointDistanceCentimeters / 100.0f
                );
        const FString WaypointLabel =
            bTransportWaypointImmobilized
                ? FString::Printf(
                    TEXT(
                        "FIELD TRANSPORT // IMMOBILIZED // "
                        "FUEL %d%% // HULL %d%% // %s // "
                        "RECOVER AT FRIENDLY RESUPPLY"
                    ),
                    FMath::RoundToInt(
                        TransportWaypointFuelPercentage * 100.0f
                    ),
                    FMath::RoundToInt(
                        TransportWaypointHullPercentage * 100.0f
                    ),
                    *DistanceLabel
                )
                : FString::Printf(
                    TEXT(
                        "FIELD TRANSPORT // FUEL %d%% // "
                        "HULL %d%% // %s"
                    ),
                    FMath::RoundToInt(
                        TransportWaypointFuelPercentage * 100.0f
                    ),
                    FMath::RoundToInt(
                        TransportWaypointHullPercentage * 100.0f
                    ),
                    *DistanceLabel
                );

        MaxLayer = DrawTransportWaypoint(
            AllottedGeometry,
            OutDrawElements,
            MaxLayer + 1,
            TransportWaypointDirectionAngleRadians,
            WaypointLabel,
            bTransportWaypointImmobilized
        );
    }

    if (bFieldSquadStatusVisible)
    {
        const FVector2D WidgetSize =
            AllottedGeometry.GetLocalSize();
        const bool bContextStatusVisible =
            !FieldSquadContextStatusLine.IsEmpty();
        const FVector2D PanelSize(
            400.0f,
            bContextStatusVisible ? 104.0f : 84.0f
        );
        const FVector2D PanelPosition(
            FMath::Max(
                18.0f,
                WidgetSize.X - PanelSize.X - 24.0f
            ),
            bStrategicSituationVisible ? 168.0f : 24.0f
        );
        const FLinearColor FireteamColor =
            FieldSquadMembersNeedingService > 0
                ? FLinearColor(1.0f, 0.44f, 0.12f, 0.98f)
                : FieldSquadLowestReadiness < 0.35f
                ? FLinearColor(1.0f, 0.20f, 0.16f, 0.98f)
                : FieldSquadLowestReadiness < 0.65f
                ? FLinearColor(1.0f, 0.66f, 0.14f, 0.98f)
                : bFieldSquadEmbarked
                ? FLinearColor(0.20f, 0.88f, 0.64f, 0.98f)
                : bFieldSquadHolding
                    ? FLinearColor(1.0f, 0.66f, 0.14f, 0.98f)
                    : FLinearColor(0.18f, 0.80f, 0.92f, 0.98f);
        const FString FireteamLabel =
            BuildFieldSquadStatusLabel(
                LivingFieldSquadOperatives,
                MaximumFieldSquadOperatives,
                bFieldSquadHolding,
                bFieldSquadEmbarked,
                FieldSquadMembersNeedingService,
                FieldSquadMembersRequiringEvacuation,
                FieldSquadAverageReadiness,
                FieldSquadLowestReadiness,
                FieldSquadContextStatusLine,
                SquadOrderPrompt
            );

        FSlateDrawElement::MakeBox(
            OutDrawElements,
            MaxLayer + 1,
            AllottedGeometry.ToPaintGeometry(
                PanelSize,
                FSlateLayoutTransform(PanelPosition)
            ),
            FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")),
            ESlateDrawEffect::None,
            FLinearColor(0.015f, 0.025f, 0.035f, 0.84f)
        );
        DrawPolyline(
            AllottedGeometry,
            OutDrawElements,
            MaxLayer + 2,
            {
                PanelPosition,
                PanelPosition + FVector2D(PanelSize.X, 0.0f),
                PanelPosition + PanelSize,
                PanelPosition + FVector2D(0.0f, PanelSize.Y),
                PanelPosition
            },
            FireteamColor,
            2.0f
        );
        FSlateDrawElement::MakeText(
            OutDrawElements,
            MaxLayer + 3,
            AllottedGeometry.ToPaintGeometry(
                FVector2D(
                    364.0f,
                    bContextStatusVisible ? 82.0f : 62.0f
                ),
                FSlateLayoutTransform(
                    PanelPosition + FVector2D(18.0f, 9.0f)
                )
            ),
            FireteamLabel,
            FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 14),
            ESlateDrawEffect::None,
            FLinearColor(0.94f, 0.97f, 1.0f, 0.98f)
        );
        MaxLayer += 3;
    }

    if (bStrategicSituationVisible)
    {
        const FVector2D WidgetSize =
            AllottedGeometry.GetLocalSize();
        const FVector2D PanelSize(440.0f, 130.0f);
        const FVector2D PanelPosition(
            FMath::Max(
                18.0f,
                WidgetSize.X - PanelSize.X - 24.0f
            ),
            132.0f
        );
        const FLinearColor FactionColor =
            StrategicSectorOwner == EBHWarFaction::Friendly
                ? FLinearColor(
                    0.18f,
                    0.80f,
                    0.92f,
                    0.96f
                )
                : StrategicSectorOwner == EBHWarFaction::Enemy
                    ? FLinearColor(
                        1.0f,
                        0.24f,
                        0.12f,
                        0.96f
                    )
                    : FLinearColor(
                        1.0f,
                        0.68f,
                        0.18f,
                        0.96f
                    );
        const TCHAR* FactionLabel =
            StrategicSectorOwner == EBHWarFaction::Friendly
                ? TEXT("FRIENDLY")
                : StrategicSectorOwner == EBHWarFaction::Enemy
                    ? TEXT("ENEMY")
                    : TEXT("CONTESTED");
        const TCHAR* ResponseLabel =
            StrategicEnemyResponsePressure >= 75.0f
                ? TEXT("CRACKDOWN")
                : StrategicEnemyResponsePressure >= 50.0f
                    ? TEXT("HUNTING")
                    : StrategicEnemyResponsePressure >= 25.0f
                        ? TEXT("WATCHFUL")
                        : TEXT("DORMANT");
        FString ReconLabel;

        if (bFieldReconActive)
        {
            ReconLabel = ReconReportCooldownRemaining > 0.0f
                ? FString::Printf(
                    TEXT(
                        "INTEL %.0f%% // REPORT READY %.0fS"
                    ),
                    StrategicIntelConfidence,
                    ReconReportCooldownRemaining
                )
                : FString::Printf(
                    TEXT(
                        "INTEL %.0f%% // RECON M %.0f/%.0fM "
                        "// OBS %.0f/%.0fS"
                    ),
                    StrategicIntelConfidence,
                    ReconMovementProgress / 100.0f,
                    ReconMovementRequired / 100.0f,
                    ReconObservationProgress,
                    ReconObservationRequired
                );
        }
        else
        {
            ReconLabel = FString::Printf(
                TEXT("INTEL %.0f%%"),
                StrategicIntelConfidence
            );
        }

        const FString SituationLabel = FString::Printf(
            TEXT(
                "LOCAL AO // %s // T%03d\n"
                "%s // SUP %.0f // FLOW %+.1f\n"
                "LOCALS %.0f%% // RESPONSE %s %.0f%%\n"
                "FORTIFICATIONS %d/%d // COVER %d%% // DEF +%d%%\n"
                "FORT ROUTE %s\n"
                "%s"
            ),
            *StrategicSectorDisplayName.ToString(),
            StrategicWarTurn,
            FactionLabel,
            StrategicSectorSupply,
            StrategicSupplyFlowPerTurn,
            StrategicCivilianSupport,
            ResponseLabel,
            StrategicEnemyResponsePressure,
            StrategicFortificationConstructed,
            StrategicFortificationCapacity,
            FMath::Clamp(
                FMath::RoundToInt(
                    FMath::Min(
                        100.0f,
                        (StrategicFortificationCoverage /
                            FMath::Max(
                                1.0f,
                                static_cast<float>(
                                    StrategicFortificationCapacity
                                )
                            )) * 100.0f
                    )
                ),
                0,
                100
            ),
            FMath::Max(
                0,
                FMath::RoundToInt(
                    (StrategicFortificationDefenseMultiplier - 1.0f) * 100.0f
                )
            ),
            bFortificationsConnected ? TEXT("ONLINE") : TEXT("OFFLINE"),
            *ReconLabel
        );

        FSlateDrawElement::MakeBox(
            OutDrawElements,
            MaxLayer + 1,
            AllottedGeometry.ToPaintGeometry(
                PanelSize,
                FSlateLayoutTransform(PanelPosition)
            ),
            FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")),
            ESlateDrawEffect::None,
            FLinearColor(0.015f, 0.025f, 0.035f, 0.80f)
        );

        const TArray<FVector2D> PanelBorder = {
            PanelPosition,
            PanelPosition + FVector2D(PanelSize.X, 0.0f),
            PanelPosition + PanelSize,
            PanelPosition + FVector2D(0.0f, PanelSize.Y),
            PanelPosition
        };
        DrawPolyline(
            AllottedGeometry,
            OutDrawElements,
            MaxLayer + 2,
            PanelBorder,
            FactionColor,
            2.0f
        );

        const FVector2D SupplyBarPosition =
            PanelPosition + FVector2D(18.0f, 111.0f);
        const FVector2D SupplyBarSize(404.0f, 5.0f);
        FSlateDrawElement::MakeBox(
            OutDrawElements,
            MaxLayer + 2,
            AllottedGeometry.ToPaintGeometry(
                SupplyBarSize,
                FSlateLayoutTransform(SupplyBarPosition)
            ),
            FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")),
            ESlateDrawEffect::None,
            FLinearColor(0.12f, 0.15f, 0.18f, 0.94f)
        );
        FSlateDrawElement::MakeBox(
            OutDrawElements,
            MaxLayer + 3,
            AllottedGeometry.ToPaintGeometry(
                FVector2D(
                    SupplyBarSize.X *
                        FMath::Clamp(
                            StrategicSectorSupply / 100.0f,
                            0.0f,
                            1.0f
                        ),
                    SupplyBarSize.Y
                ),
                FSlateLayoutTransform(SupplyBarPosition)
            ),
            FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")),
            ESlateDrawEffect::None,
            FactionColor
        );
        FSlateDrawElement::MakeText(
            OutDrawElements,
            MaxLayer + 4,
            AllottedGeometry.ToPaintGeometry(
                FVector2D(404.0f, 90.0f),
                FSlateLayoutTransform(
                    PanelPosition + FVector2D(18.0f, 12.0f)
                )
            ),
            SituationLabel,
            FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 14),
            ESlateDrawEffect::None,
            FLinearColor(0.92f, 0.95f, 0.98f, 0.98f)
        );
        MaxLayer += 4;
    }

    if (bVehicleReadinessVisible)
    {
        const FVector2D WidgetSize =
            AllottedGeometry.GetLocalSize();
        const FVector2D PanelSize(420.0f, 86.0f);
        const FVector2D PanelPosition(
            FMath::Max(
                18.0f,
                (WidgetSize.X - PanelSize.X) * 0.5f
            ),
            FMath::Max(
                18.0f,
                WidgetSize.Y - PanelSize.Y - 28.0f
            )
        );
        const bool bFuelCritical =
            VehicleFuelPercentage <= 0.10f;
        const bool bHullCritical =
            VehicleHullPercentage <= 0.20f;
        const FLinearColor ReadinessColor =
            bVehicleImmobilized || bFuelCritical || bHullCritical
                ? FLinearColor(1.0f, 0.18f, 0.08f, 0.98f)
                : VehicleFuelPercentage <= 0.25f ||
                    VehicleHullPercentage <= 0.35f
                    ? FLinearColor(
                        1.0f,
                        0.68f,
                        0.12f,
                        0.98f
                    )
                    : FLinearColor(
                        0.20f,
                        0.88f,
                        0.64f,
                        0.98f
                    );
        const FString ReadinessLabel = FString::Printf(
            TEXT(
                "FIELD TRANSPORT // %s\n"
                "SPEED %03d KM/H // FUEL %d%% // HULL %d%%"
            ),
            bVehicleImmobilized
                ? TEXT("IMMOBILIZED")
                : TEXT("MOBILE"),
            FMath::RoundToInt(VehicleSpeedKPH),
            FMath::RoundToInt(VehicleFuelPercentage * 100.0f),
            FMath::RoundToInt(VehicleHullPercentage * 100.0f)
        );

        FSlateDrawElement::MakeBox(
            OutDrawElements,
            MaxLayer + 1,
            AllottedGeometry.ToPaintGeometry(
                PanelSize,
                FSlateLayoutTransform(PanelPosition)
            ),
            FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")),
            ESlateDrawEffect::None,
            FLinearColor(0.015f, 0.025f, 0.035f, 0.88f)
        );
        DrawPolyline(
            AllottedGeometry,
            OutDrawElements,
            MaxLayer + 2,
            {
                PanelPosition,
                PanelPosition + FVector2D(PanelSize.X, 0.0f),
                PanelPosition + PanelSize,
                PanelPosition + FVector2D(0.0f, PanelSize.Y),
                PanelPosition
            },
            ReadinessColor,
            2.0f
        );
        FSlateDrawElement::MakeText(
            OutDrawElements,
            MaxLayer + 3,
            AllottedGeometry.ToPaintGeometry(
                FVector2D(384.0f, 50.0f),
                FSlateLayoutTransform(
                    PanelPosition + FVector2D(18.0f, 10.0f)
                )
            ),
            ReadinessLabel,
            FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 15),
            ESlateDrawEffect::None,
            FLinearColor(0.94f, 0.97f, 1.0f, 0.98f)
        );

        const FVector2D BarSize(184.0f, 6.0f);
        const FVector2D FuelBarPosition =
            PanelPosition + FVector2D(18.0f, 70.0f);
        const FVector2D HullBarPosition =
            PanelPosition + FVector2D(218.0f, 70.0f);

        for (const FVector2D& BarPosition :
            { FuelBarPosition, HullBarPosition })
        {
            FSlateDrawElement::MakeBox(
                OutDrawElements,
                MaxLayer + 2,
                AllottedGeometry.ToPaintGeometry(
                    BarSize,
                    FSlateLayoutTransform(BarPosition)
                ),
                FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")),
                ESlateDrawEffect::None,
                FLinearColor(0.12f, 0.15f, 0.18f, 0.96f)
            );
        }

        FSlateDrawElement::MakeBox(
            OutDrawElements,
            MaxLayer + 3,
            AllottedGeometry.ToPaintGeometry(
                FVector2D(
                    BarSize.X * VehicleFuelPercentage,
                    BarSize.Y
                ),
                FSlateLayoutTransform(FuelBarPosition)
            ),
            FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")),
            ESlateDrawEffect::None,
            ReadinessColor
        );
        FSlateDrawElement::MakeBox(
            OutDrawElements,
            MaxLayer + 3,
            AllottedGeometry.ToPaintGeometry(
                FVector2D(
                    BarSize.X * VehicleHullPercentage,
                    BarSize.Y
                ),
                FSlateLayoutTransform(HullBarPosition)
            ),
            FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")),
            ESlateDrawEffect::None,
            ReadinessColor
        );
        MaxLayer += 3;
    }

    const FGrenadeThreatState* ActiveGrenadeThreat = nullptr;

    for (const FGrenadeThreatState& Threat : GrenadeThreats)
    {
        if (!Threat.SourceActor.IsValid())
        {
            continue;
        }

        if (!ActiveGrenadeThreat ||
            Threat.TimeUntilDetonation <
                ActiveGrenadeThreat->TimeUntilDetonation ||
            (
                FMath::IsNearlyEqual(
                    Threat.TimeUntilDetonation,
                    ActiveGrenadeThreat->TimeUntilDetonation,
                    0.05f
                ) &&
                Threat.DistanceCentimeters <
                    ActiveGrenadeThreat->DistanceCentimeters
            ))
        {
            ActiveGrenadeThreat = &Threat;
        }
    }

    if (ActiveGrenadeThreat)
    {
        const FVector2D WidgetSize =
            AllottedGeometry.GetLocalSize();
        const float Pulse =
            0.5f +
            (0.5f * FMath::Sin(GrenadeWarningPulseTime * 12.0f));
        const FLinearColor WarningColor(
            1.0f,
            FMath::Lerp(0.12f, 0.48f, Pulse),
            0.02f,
            FMath::Lerp(0.78f, 1.0f, Pulse)
        );

        MaxLayer = DrawDirectionChevron(
            AllottedGeometry,
            OutDrawElements,
            MaxLayer + 1,
            ActiveGrenadeThreat->DirectionAngleRadians,
            WarningColor
        );

        if (WidgetSize.X > 1.0f && WidgetSize.Y > 1.0f)
        {
            const FVector2D PanelSize(330.0f, 46.0f);
            const FVector2D PanelPosition(
                (WidgetSize.X - PanelSize.X) * 0.5f,
                WidgetSize.Y * 0.62f
            );
            const FString WarningLabel = FString::Printf(
                TEXT("GRENADE // %.0f M // %.1f S"),
                ActiveGrenadeThreat->DistanceCentimeters / 100.0f,
                ActiveGrenadeThreat->TimeUntilDetonation
            );

            FSlateDrawElement::MakeBox(
                OutDrawElements,
                MaxLayer + 1,
                AllottedGeometry.ToPaintGeometry(
                    PanelSize,
                    FSlateLayoutTransform(PanelPosition)
                ),
                FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")),
                ESlateDrawEffect::None,
                FLinearColor(0.08f, 0.01f, 0.0f, 0.84f)
            );
            DrawPolyline(
                AllottedGeometry,
                OutDrawElements,
                MaxLayer + 2,
                {
                    PanelPosition,
                    PanelPosition +
                        FVector2D(PanelSize.X, 0.0f),
                    PanelPosition + PanelSize,
                    PanelPosition +
                        FVector2D(0.0f, PanelSize.Y),
                    PanelPosition
                },
                WarningColor,
                3.0f
            );
            FSlateDrawElement::MakeText(
                OutDrawElements,
                MaxLayer + 3,
                AllottedGeometry.ToPaintGeometry(
                    FVector2D(294.0f, 28.0f),
                    FSlateLayoutTransform(
                        PanelPosition + FVector2D(18.0f, 10.0f)
                    )
                ),
                WarningLabel,
                FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 18),
                ESlateDrawEffect::None,
                WarningColor
            );
            MaxLayer += 3;
        }
    }

    if (NearMissFeedbackRemaining > 0.0f)
    {
        const float Fade = FMath::Clamp(
            NearMissFeedbackRemaining /
                FMath::Max(0.05f, NearMissFeedbackDuration),
            0.0f,
            1.0f
        );
        const float Opacity =
            Fade *
            NearMissIntensity *
            FMath::Max(0.0f, MaximumNearMissOpacity);

        MaxLayer = DrawScreenTint(
            AllottedGeometry,
            OutDrawElements,
            MaxLayer + 1,
            FLinearColor(0.16f, 0.12f, 0.08f, Opacity)
        );
        MaxLayer = DrawDirectionChevron(
            AllottedGeometry,
            OutDrawElements,
            MaxLayer + 1,
            NearMissDirectionAngleRadians,
            FLinearColor(1.0f, 0.55f, 0.12f, Fade)
        );
    }

    if (DamageFeedbackRemaining > 0.0f)
    {
        const float HitFlashScale = UserSettings
            ? UserSettings->GetHitFlashScale()
            : 1.0f;
        const float Fade = FMath::Clamp(
            DamageFeedbackRemaining /
                FMath::Max(0.05f, DamageFlashDuration),
            0.0f,
            1.0f
        );

        MaxLayer = DrawScreenTint(
            AllottedGeometry,
            OutDrawElements,
            MaxLayer + 1,
            FLinearColor(
                0.75f,
                0.0f,
                0.0f,
                Fade * FMath::Max(0.0f, MaximumDamageFlashOpacity) *
                    FMath::Clamp(HitFlashScale, 0.0f, 1.0f)
            )
        );
        MaxLayer = DrawDirectionChevron(
            AllottedGeometry,
            OutDrawElements,
            MaxLayer + 1,
            DamageDirectionAngleRadians,
            FLinearColor(1.0f, 0.08f, 0.04f, Fade)
        );
    }

    const bool bLowHealth =
        CurrentHealthPercentage > 0.0f &&
        CurrentHealthPercentage <= LowHealthThreshold;

    if (bLowHealth)
    {
        const FVector2D WidgetSize = AllottedGeometry.GetLocalSize();
        const float Severity = 1.0f - FMath::Clamp(
            CurrentHealthPercentage /
                FMath::Max(0.01f, LowHealthThreshold),
            0.0f,
            1.0f
        );
        const float Pulse =
            0.5f + (0.5f * FMath::Sin(LowHealthPulseTime * 6.0f));
        const float Alpha =
            FMath::Lerp(0.18f, 0.52f, Severity) *
            FMath::Lerp(0.55f, 1.0f, Pulse);
        const float Inset = 8.0f;
        const TArray<FVector2D> BorderPoints = {
            FVector2D(Inset, Inset),
            FVector2D(WidgetSize.X - Inset, Inset),
            FVector2D(
                WidgetSize.X - Inset,
                WidgetSize.Y - Inset
            ),
            FVector2D(Inset, WidgetSize.Y - Inset),
            FVector2D(Inset, Inset)
        };

        MaxLayer = DrawPolyline(
            AllottedGeometry,
            OutDrawElements,
            MaxLayer + 1,
            BorderPoints,
            FLinearColor(0.9f, 0.0f, 0.0f, Alpha),
            8.0f
        );
    }

    const FVector2D WidgetSize = AllottedGeometry.GetLocalSize();

    if (WidgetSize.X > 1.0f && WidgetSize.Y > 1.0f)
    {
        const FString FieldDressingPrompt = GetBindingPrompt(
            FName(TEXT("FieldDressing")),
            TEXT("H")
        );
        const FString MedkitPrompt = GetBindingPrompt(
            FName(TEXT("Medkit")),
            TEXT("J")
        );
        const FString GrenadePrompt = GetBindingPrompt(
            FName(TEXT("Grenade")),
            TEXT("G")
        );
        const FString EngineeringPrompt = GetBindingPrompt(
            FName(TEXT("Engineering")),
            TEXT("V")
        );
        FString InjuryStatus = FString::Printf(
            TEXT(
                "ARMOR  H:%d%%  V:%d%%\n"
                "DRESSINGS: %d  [%s]  MEDKITS: %d [%s]\n"
                "FRAGS: %d  [%s]  CHARGES: %d [%s]  ARMED: %d\n"
                "LOAD: %.1f KG  %s  SPEED:%d%%  ENDURANCE:%d%%"
            ),
            FMath::RoundToInt(
                HelmetDurabilityPercentage * 100.0f
            ),
            FMath::RoundToInt(
                BodyArmorDurabilityPercentage * 100.0f
            ),
            FieldDressingCount,
            *FieldDressingPrompt,
            MedkitCount,
            *MedkitPrompt,
            FragGrenadeCount,
            *GrenadePrompt,
            EngineeringChargeCount,
            *EngineeringPrompt,
            ActiveEngineeringChargeCount,
            CarryLoadKilograms,
            CarryLoadState == EBHCarryLoadState::Overloaded
                ? TEXT("OVERLOADED")
                : CarryLoadState == EBHCarryLoadState::Heavy
                    ? TEXT("HEAVY") : TEXT("FIGHTING"),
            FMath::RoundToInt(CarryMovementSpeedMultiplier * 100.0f),
            FMath::RoundToInt(100.0f / CarryStaminaDrainMultiplier)
        );
        FLinearColor InjuryColor(
            0.82f,
            0.86f,
            0.90f,
            0.92f
        );

        if (bLegInjured)
        {
            InjuryStatus =
                FString(TEXT("LEG WOUND - MOVEMENT REDUCED\n")) +
                InjuryStatus;
            InjuryColor = FLinearColor(
                1.0f,
                0.58f,
                0.10f,
                0.96f
            );
        }

        if (bArmInjured)
        {
            InjuryStatus =
                FString(TEXT("ARM WOUND - ACCURACY REDUCED\n")) +
                InjuryStatus;
            InjuryColor = FLinearColor(
                1.0f,
                0.58f,
                0.10f,
                0.96f
            );
        }

        if ((HelmetDurabilityPercentage <= 0.25f ||
            BodyArmorDurabilityPercentage <= 0.25f) &&
            !bArmInjured &&
            !bLegInjured)
        {
            InjuryColor = FLinearColor(
                1.0f,
                0.58f,
                0.10f,
                0.96f
            );
        }

        if (bMedicalTreatmentActive)
        {
            InjuryStatus = FString::Printf(
                TEXT("USING MEDKIT  %d%%\n%s"),
                FMath::RoundToInt(
                    MedicalTreatmentProgress * 100.0f
                ),
                *InjuryStatus
            );
            InjuryColor = FLinearColor(
                0.18f,
                0.85f,
                1.0f,
                0.98f
            );
        }

        if (CurrentSuppressionPercentage > 0.05f)
        {
            InjuryStatus = FString::Printf(
                TEXT("SUPPRESSED - ACCURACY %d%%\n%s"),
                FMath::RoundToInt(
                    CurrentSuppressionPercentage * 100.0f
                ),
                *InjuryStatus
            );
            InjuryColor = FLinearColor(
                1.0f,
                0.58f,
                0.10f,
                0.96f
            );
        }

        if (bIsBleeding)
        {
            InjuryStatus = FString::Printf(
                TEXT("BLEEDING  %.1f HP/S - PRESS [%s]\n%s"),
                CurrentBleedRate,
                *FieldDressingPrompt,
                *InjuryStatus
            );
            const float Pulse =
                0.5f +
                (0.5f * FMath::Sin(InjuryPulseTime * 7.0f));
            InjuryColor = FLinearColor(
                1.0f,
                0.04f,
                0.02f,
                FMath::Lerp(0.62f, 1.0f, Pulse)
            );
        }

        const FVector2D TextPosition(
            28.0f,
            FMath::Max(24.0f, WidgetSize.Y - 172.0f)
        );
        const FVector2D TextSize(
            FMath::Max(280.0f, WidgetSize.X * 0.42f),
            160.0f
        );

        FSlateDrawElement::MakeText(
            OutDrawElements,
            MaxLayer + 1,
            AllottedGeometry.ToPaintGeometry(
                TextSize,
                FSlateLayoutTransform(TextPosition)
            ),
            InjuryStatus,
            FCoreStyle::GetDefaultFontStyle(
                TEXT("Bold"),
                16
            ),
            ESlateDrawEffect::None,
            InjuryColor
        );
        ++MaxLayer;
    }

    return MaxLayer;
}

float UBHCombatStatusWidget::ResolveRelativeDirectionAngle(
    const FVector& SourceDirection
) const
{
    const FVector FlatSourceDirection(
        SourceDirection.X,
        SourceDirection.Y,
        0.0
    );

    if (FlatSourceDirection.IsNearlyZero())
    {
        return 0.0f;
    }

    FRotator ViewRotation = FRotator::ZeroRotator;
    FVector ViewLocation = FVector::ZeroVector;

    if (const APlayerController* PlayerController = GetOwningPlayer())
    {
        PlayerController->GetPlayerViewPoint(
            ViewLocation,
            ViewRotation
        );
    }

    const FRotator YawRotation(
        0.0,
        ViewRotation.Yaw,
        0.0
    );
    const FVector ForwardDirection =
        FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
    const FVector RightDirection =
        FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);
    const FVector NormalizedSource =
        FlatSourceDirection.GetSafeNormal();

    return FMath::Atan2(
        FVector::DotProduct(NormalizedSource, RightDirection),
        FVector::DotProduct(NormalizedSource, ForwardDirection)
    );
}
