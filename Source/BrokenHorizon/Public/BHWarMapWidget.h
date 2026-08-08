#pragma once

#include "CoreMinimal.h"
#include "BHWarTypes.h"
#include "Blueprint/UserWidget.h"
#include "BHWarMapWidget.generated.h"

class UBHWarSubsystem;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(
    FBHOnWarMapCloseRequested
);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
    FBHOnWarMapDeployRequested,
    FName,
    SectorID,
    EBHWarPriorityType,
    OperationType
);

DECLARE_DYNAMIC_MULTICAST_DELEGATE(
    FBHOnWarMapWithdrawRequested
);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
    FBHOnWarMapMilitiaRequested,
    FName,
    SectorID
);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
    FBHOnWarMapGarrisonRedeployRequested,
    FName,
    DestinationSectorID
);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
    FBHOnWarMapCivilianAidRequested,
    FName,
    TargetSectorID,
    EBHWarPriorityType,
    OperationType
);

UCLASS()
class BROKENHORIZON_API UBHWarMapWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "Persistent War")
    void InitializeWarMap(UBHWarSubsystem* InWarSubsystem);

    UFUNCTION(BlueprintCallable, Category = "Persistent War")
    void RefreshWarMap();

    UFUNCTION(BlueprintCallable, Category = "Persistent War")
    void SetDeploymentMode(bool bEnabled);

    static FString BuildStrategicControlRejection(
        bool bHasStrategicAuthority,
        bool bOperationCommitted
    );

    static FBHCampaignDifficultyProfile AdjustCustomDifficultyAxis(
        const FBHCampaignDifficultyProfile& CurrentProfile,
        int32 AxisIndex,
        float Delta
    );

    static FString GetCustomDifficultyAxisLabel(int32 AxisIndex);
    static float GetCustomDifficultyAxisValue(
        const FBHCampaignDifficultyProfile& Profile,
        int32 AxisIndex
    );

    UPROPERTY(BlueprintAssignable, Category = "Persistent War")
    FBHOnWarMapCloseRequested OnCloseRequested;

    UPROPERTY(BlueprintAssignable, Category = "Persistent War")
    FBHOnWarMapDeployRequested OnDeployRequested;

    UPROPERTY(BlueprintAssignable, Category = "Persistent War")
    FBHOnWarMapWithdrawRequested OnWithdrawRequested;

    UPROPERTY(BlueprintAssignable, Category = "Persistent War")
    FBHOnWarMapMilitiaRequested OnMilitiaRequested;

    UPROPERTY(BlueprintAssignable, Category = "Persistent War")
    FBHOnWarMapGarrisonRedeployRequested
        OnGarrisonRedeployRequested;

    UPROPERTY(BlueprintAssignable, Category = "Persistent War")
    FBHOnWarMapCivilianAidRequested OnCivilianAidRequested;

protected:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;

    virtual void NativeTick(
        const FGeometry& MyGeometry,
        float InDeltaTime
    ) override;

    virtual FReply NativeOnKeyDown(
        const FGeometry& InGeometry,
        const FKeyEvent& InKeyEvent
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

private:
    UFUNCTION()
    void HandleWarStateChanged(
        int32 NewTurnNumber,
        FName NewPrioritySectorID,
        EBHWarPriorityType NewPriorityType
    );

    void BindWarSubsystem();
    void UnbindWarSubsystem();
    bool IsDeploymentRiskConfirmationActive() const;
    bool IsWithdrawalConfirmationActive() const;
    bool IsStrategicActionFeedbackActive() const;
    void RefreshOperationChoices();
    void SelectOperationOffset(int32 Offset);
    bool HasSelectedOperation() const;
    bool TryDeploySelectedOperation();
    void SetStrategicActionFeedback(
        const FString& Feedback,
        float DurationSeconds = 4.0f
    );
    void SelectNextCustomDifficultyAxis();
    bool TryAdjustSelectedCustomDifficultyAxis(float Delta);

    UPROPERTY(Transient)
    TObjectPtr<UBHWarSubsystem> WarSubsystem;

    TArray<FBHWarSectorState> SectorStates;
    TArray<FBHWarEventRecord> RecentWarEvents;
    FText PriorityText;
    int32 TurnNumber = 0;
    float FriendlyControlPercentage = 0.0f;
    bool bDeploymentMode = false;
    bool bDeploymentInputArmed = false;
    TArray<FName> OperationSectorChoices;
    TArray<EBHWarPriorityType> OperationTypeChoices;
    int32 SelectedOperationIndex = INDEX_NONE;
    float DeploymentRiskConfirmationExpiresAt = -1.0f;
    float WithdrawalConfirmationExpiresAt = -1.0f;
    float StrategicActionFeedbackExpiresAt = -1.0f;
    FString StrategicActionFeedback;
    int32 SelectedCustomDifficultyAxis = 0;
};
