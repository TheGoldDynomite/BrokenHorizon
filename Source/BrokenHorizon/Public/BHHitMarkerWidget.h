#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BHHitMarkerWidget.generated.h"

UCLASS()
class BROKENHORIZON_API UBHHitMarkerWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "Combat Feedback")
    void ShowHitMarker(bool bLethalHit, bool bHeadshot);

    UFUNCTION(BlueprintCallable, Category = "Combat Feedback")
    void ShowDetailedHitMarker(
        bool bLethalHit,
        bool bHeadshot,
        bool bArmorHit
    );

protected:
    virtual void NativeConstruct() override;

    virtual void NativeTick(
        const FGeometry& MyGeometry,
        float InDeltaTime
    ) override;

    virtual int32 NativePaint(
        const FPaintArgs& Args,
        const FGeometry& AllottedGeometry,
        const FSlateRect& MyCullingRect,
        FSlateWindowElementList& OutDrawElements,
        int32 LayerId,
        const FWidgetStyle& InWidgetStyle,
        bool bParentEnabled
    ) const override;

    UPROPERTY(
        EditDefaultsOnly,
        Category = "Combat Feedback",
        meta = (ClampMin = "0.01", Units = "s")
    )
    float HitMarkerDuration = 0.12f;

    UPROPERTY(
        EditDefaultsOnly,
        Category = "Combat Feedback",
        meta = (ClampMin = "0.01", Units = "s")
    )
    float LethalMarkerDuration = 0.2f;

    UPROPERTY(
        EditDefaultsOnly,
        Category = "Combat Feedback",
        meta = (ClampMin = "0.01", Units = "s")
    )
    float HeadshotMarkerDuration = 0.18f;

    UPROPERTY(EditDefaultsOnly, Category = "Combat Feedback")
    FLinearColor HitColor = FLinearColor::White;

    UPROPERTY(EditDefaultsOnly, Category = "Combat Feedback")
    FLinearColor LethalColor =
        FLinearColor(1.0f, 0.08f, 0.025f, 1.0f);

    UPROPERTY(EditDefaultsOnly, Category = "Combat Feedback")
    FLinearColor HeadshotColor =
        FLinearColor(1.0f, 0.72f, 0.05f, 1.0f);

    UPROPERTY(EditDefaultsOnly, Category = "Combat Feedback")
    FLinearColor ArmorHitColor =
        FLinearColor(0.1f, 0.75f, 1.0f, 1.0f);

    UPROPERTY(
        EditDefaultsOnly,
        Category = "Combat Feedback",
        meta = (ClampMin = "0.5")
    )
    float LineThickness = 2.0f;

    UPROPERTY(
        EditDefaultsOnly,
        Category = "Combat Feedback",
        meta = (ClampMin = "0.0")
    )
    float InnerGap = 6.0f;

    UPROPERTY(
        EditDefaultsOnly,
        Category = "Combat Feedback",
        meta = (ClampMin = "1.0")
    )
    float LineLength = 7.0f;

    UPROPERTY(
        EditDefaultsOnly,
        Category = "Combat Feedback",
        meta = (ClampMin = "1.0")
    )
    float HeadshotDiamondRadius = 4.0f;

    UPROPERTY(
        EditDefaultsOnly,
        Category = "Combat Feedback",
        meta = (ClampMin = "1.0")
    )
    float ArmorIndicatorRadius = 5.5f;

private:
    float RemainingDisplayTime = 0.0f;
    FLinearColor ActiveColor = FLinearColor::White;
    bool bActiveHeadshot = false;
    bool bActiveArmorHit = false;
};
