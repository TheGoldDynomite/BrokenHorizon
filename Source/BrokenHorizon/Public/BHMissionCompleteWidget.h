#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BHMissionCompleteWidget.generated.h"

class UTextBlock;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(
    FBHOnMissionContinueRequested
);

UCLASS()
class BROKENHORIZON_API UBHMissionCompleteWidget
    : public UUserWidget
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "Mission")
    void SetMissionCompleteText(const FText& NewText);

    UFUNCTION(BlueprintCallable, Category = "Mission")
    void ResetContinueRequest();

    UPROPERTY(BlueprintAssignable, Category = "Mission")
    FBHOnMissionContinueRequested OnContinueRequested;

protected:
    virtual void NativeOnInitialized() override;

    virtual void NativeConstruct() override;

    virtual void NativeTick(
        const FGeometry& MyGeometry,
        float InDeltaTime
    ) override;

    virtual FReply NativeOnKeyDown(
        const FGeometry& InGeometry,
        const FKeyEvent& InKeyEvent
    ) override;

    void RequestContinue();

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> MissionCompleteText;

    bool bContinueInputArmed = false;
    bool bContinueRequestSent = false;
};
