#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BHSubtitleWidget.generated.h"

class UBorder;
class UTextBlock;

UCLASS(Blueprintable)
class BROKENHORIZON_API UBHSubtitleWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "Broken Horizon|Accessibility|Subtitles")
    void ShowSubtitle(
        const FText& Speaker,
        const FText& Line,
        float DurationSeconds = 3.0f,
        float DirectionAngleDegrees = 0.0f,
        bool bHasDirection = false
    );

    UFUNCTION(BlueprintCallable, Category = "Broken Horizon|Accessibility|Subtitles")
    void ClearSubtitle();

    UFUNCTION(BlueprintPure, Category = "Broken Horizon|Accessibility|Subtitles")
    static FText BuildDirectionalIndicator(float DirectionAngleDegrees);

protected:
    virtual void NativeConstruct() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
    struct FPendingSubtitle
    {
        FText Speaker;
        FText Line;
        float Duration = 3.0f;
        float DirectionDegrees = 0.0f;
        bool bHasDirection = false;
    };

    void EnsureNativeLayout();
    void RefreshPresentation();
    void PresentSubtitle(const FPendingSubtitle& Subtitle);

    UPROPERTY(Transient)
    TObjectPtr<UBorder> SubtitleBackground;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> SubtitleText;

    FText ActiveSpeaker;
    FText ActiveLine;
    float RemainingDuration = 0.0f;
    float ActiveDirectionDegrees = 0.0f;
    bool bActiveHasDirection = false;
    TArray<FPendingSubtitle> PendingSubtitles;
};
