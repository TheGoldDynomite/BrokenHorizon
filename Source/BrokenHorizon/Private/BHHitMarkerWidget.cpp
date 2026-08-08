#include "BHHitMarkerWidget.h"

#include "Rendering/DrawElements.h"

void UBHHitMarkerWidget::NativeConstruct()
{
    Super::NativeConstruct();
    SetVisibility(ESlateVisibility::Hidden);
}

void UBHHitMarkerWidget::ShowHitMarker(
    bool bLethalHit,
    bool bHeadshot
)
{
    ShowDetailedHitMarker(bLethalHit, bHeadshot, false);
}

void UBHHitMarkerWidget::ShowDetailedHitMarker(
    bool bLethalHit,
    bool bHeadshot,
    bool bArmorHit
)
{
    bActiveHeadshot = bHeadshot;
    bActiveArmorHit = bArmorHit && !bLethalHit;
    ActiveColor = bLethalHit
        ? LethalColor
        : (bHeadshot
            ? HeadshotColor
            : (bArmorHit ? ArmorHitColor : HitColor));

    if (bLethalHit)
    {
        RemainingDisplayTime =
            FMath::Max(0.01f, LethalMarkerDuration);
    }
    else if (bHeadshot)
    {
        RemainingDisplayTime =
            FMath::Max(0.01f, HeadshotMarkerDuration);
    }
    else
    {
        RemainingDisplayTime =
            FMath::Max(0.01f, HitMarkerDuration);
    }

    SetVisibility(ESlateVisibility::HitTestInvisible);
    Invalidate(EInvalidateWidgetReason::Paint);
}

void UBHHitMarkerWidget::NativeTick(
    const FGeometry& MyGeometry,
    float InDeltaTime
)
{
    Super::NativeTick(MyGeometry, InDeltaTime);

    if (RemainingDisplayTime <= 0.0f)
    {
        return;
    }

    RemainingDisplayTime -= InDeltaTime;

    if (RemainingDisplayTime <= 0.0f)
    {
        RemainingDisplayTime = 0.0f;
        SetVisibility(ESlateVisibility::Hidden);
    }
}

int32 UBHHitMarkerWidget::NativePaint(
    const FPaintArgs& Args,
    const FGeometry& AllottedGeometry,
    const FSlateRect& MyCullingRect,
    FSlateWindowElementList& OutDrawElements,
    int32 LayerId,
    const FWidgetStyle& InWidgetStyle,
    bool bParentEnabled
) const
{
    const int32 BaseLayer = Super::NativePaint(
        Args,
        AllottedGeometry,
        MyCullingRect,
        OutDrawElements,
        LayerId,
        InWidgetStyle,
        bParentEnabled
    );

    if (RemainingDisplayTime <= 0.0f)
    {
        return BaseLayer;
    }

    const FVector2D LocalSize = AllottedGeometry.GetLocalSize();
    const FVector2f Center(
        static_cast<float>(LocalSize.X * 0.5),
        static_cast<float>(LocalSize.Y * 0.5)
    );
    const float Gap = FMath::Max(0.0f, InnerGap);
    const float Length = FMath::Max(1.0f, LineLength);

    const FVector2f Directions[] =
    {
        FVector2f(-1.0f, -1.0f).GetSafeNormal(),
        FVector2f(1.0f, -1.0f).GetSafeNormal(),
        FVector2f(-1.0f, 1.0f).GetSafeNormal(),
        FVector2f(1.0f, 1.0f).GetSafeNormal()
    };

    for (const FVector2f& Direction : Directions)
    {
        TArray<FVector2f> Points;
        Points.Reserve(2);
        Points.Add(Center + (Direction * Gap));
        Points.Add(Center + (Direction * (Gap + Length)));

        FSlateDrawElement::MakeLines(
            OutDrawElements,
            BaseLayer + 1,
            AllottedGeometry.ToPaintGeometry(),
            Points,
            ESlateDrawEffect::None,
            ActiveColor,
            true,
            FMath::Max(0.5f, LineThickness)
        );
    }

    if (bActiveHeadshot)
    {
        const float DiamondRadius =
            FMath::Max(1.0f, HeadshotDiamondRadius);
        TArray<FVector2f> DiamondPoints;
        DiamondPoints.Reserve(5);
        DiamondPoints.Add(Center + FVector2f(0.0f, -DiamondRadius));
        DiamondPoints.Add(Center + FVector2f(DiamondRadius, 0.0f));
        DiamondPoints.Add(Center + FVector2f(0.0f, DiamondRadius));
        DiamondPoints.Add(Center + FVector2f(-DiamondRadius, 0.0f));
        DiamondPoints.Add(Center + FVector2f(0.0f, -DiamondRadius));

        FSlateDrawElement::MakeLines(
            OutDrawElements,
            BaseLayer + 2,
            AllottedGeometry.ToPaintGeometry(),
            DiamondPoints,
            ESlateDrawEffect::None,
            HeadshotColor,
            true,
            FMath::Max(0.5f, LineThickness)
        );
    }

    if (bActiveArmorHit)
    {
        const float ArmorRadius =
            FMath::Max(1.0f, ArmorIndicatorRadius);
        TArray<FVector2f> ArmorPoints;
        ArmorPoints.Reserve(5);
        ArmorPoints.Add(Center + FVector2f(-ArmorRadius, -ArmorRadius));
        ArmorPoints.Add(Center + FVector2f(ArmorRadius, -ArmorRadius));
        ArmorPoints.Add(Center + FVector2f(ArmorRadius, ArmorRadius));
        ArmorPoints.Add(Center + FVector2f(-ArmorRadius, ArmorRadius));
        ArmorPoints.Add(Center + FVector2f(-ArmorRadius, -ArmorRadius));

        FSlateDrawElement::MakeLines(
            OutDrawElements,
            BaseLayer + 3,
            AllottedGeometry.ToPaintGeometry(),
            ArmorPoints,
            ESlateDrawEffect::None,
            ArmorHitColor,
            true,
            FMath::Max(0.5f, LineThickness)
        );
    }

    return BaseLayer + (bActiveArmorHit
        ? 3
        : (bActiveHeadshot ? 2 : 1));
}
