#pragma once

#include "CoreMinimal.h"
#include "BHMissionData.h"
#include "Components/ActorComponent.h"
#include "BHObjectiveComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
    FOnObjectiveCompleted,
    FName,
    CompletedObjectiveID,
    FText,
    CompletedObjectiveText
);

DECLARE_DYNAMIC_MULTICAST_DELEGATE(
    FBHOnMissionCompleted
);

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class BROKENHORIZON_API UBHObjectiveComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UBHObjectiveComponent();

    virtual void GetLifetimeReplicatedProps(
        TArray<FLifetimeProperty>& OutLifetimeProps
    ) const override;

    UFUNCTION(BlueprintCallable, Category = "Objectives")
    void StartMission(UBHMissionData* NewMissionData);

    void StartRuntimeMission(
        const TArray<FBHObjectiveDefinition>& NewObjectives
    );

    void ClearMissionState();

    UFUNCTION(BlueprintCallable, Category = "Objectives")
    bool CompleteObjectiveByID(FName ObjectiveID);

    UFUNCTION(BlueprintCallable, Category = "Objectives")
    bool FailMission();

    bool RestoreMissionState(
        UBHMissionData* MissionData,
        FName SavedCurrentObjectiveID,
        const TArray<FName>& SavedCompletedObjectiveIDs,
        bool bSavedMissionComplete,
        bool bSavedMissionFailed
    );

    bool RestoreRuntimeMissionState(
        const TArray<FBHObjectiveDefinition>& SavedObjectives,
        FName SavedCurrentObjectiveID,
        const TArray<FName>& SavedCompletedObjectiveIDs,
        bool bSavedMissionComplete,
        bool bSavedMissionFailed
    );

    UBHMissionData* GetActiveMissionData() const;

    const TArray<FBHObjectiveDefinition>&
        GetRuntimeObjectiveDefinitions() const;

    bool IsRuntimeMission() const;

    UFUNCTION(BlueprintPure, Category = "Objectives")
    FName GetCurrentObjectiveID() const;

    UFUNCTION(BlueprintPure, Category = "Objectives")
    TArray<FName> GetCompletedObjectiveIDs() const;

    UFUNCTION(BlueprintPure, Category = "Objectives")
    bool IsObjectiveCompleted(FName ObjectiveID) const;

    UFUNCTION(BlueprintPure, Category = "Objectives")
    bool IsMissionComplete() const;

    UFUNCTION(BlueprintPure, Category = "Objectives")
    bool IsMissionFailed() const;

    UFUNCTION(BlueprintPure, Category = "Objectives")
    FText ResolveObjectiveDisplayText(FName ObjectiveID) const;

    UFUNCTION(BlueprintPure, Category = "Objectives")
    FText GetCurrentObjectiveText() const;

    UFUNCTION(BlueprintPure, Category = "Objectives")
    TArray<FText> GetCompletedObjectiveTexts() const;

    UFUNCTION(BlueprintCallable, Category = "Objectives|Presentation")
    void SetObjectiveDisplayOverride(
        FName ObjectiveID,
        const FText& DisplayText
    );

    UFUNCTION(BlueprintCallable, Category = "Objectives|Presentation")
    void ClearObjectiveDisplayOverrides();

    UPROPERTY(BlueprintAssignable, Category = "Objectives")
    FOnObjectiveCompleted OnObjectiveCompleted;

    UPROPERTY(BlueprintAssignable, Category = "Objectives")
    FBHOnMissionCompleted OnMissionCompleted;

protected:
    UFUNCTION()
    void OnRep_MissionState();

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Objectives")
    TObjectPtr<UBHMissionData> ActiveMissionData;

    UPROPERTY(
        ReplicatedUsing = OnRep_MissionState,
        VisibleInstanceOnly,
        BlueprintReadOnly,
        Category = "Objectives"
    )
    FName CurrentObjectiveID = NAME_None;

    UPROPERTY(
        ReplicatedUsing = OnRep_MissionState,
        VisibleInstanceOnly,
        BlueprintReadOnly,
        Category = "Objectives"
    )
    TArray<FName> CompletedObjectiveIDs;

    UPROPERTY(
        ReplicatedUsing = OnRep_MissionState,
        VisibleInstanceOnly,
        BlueprintReadOnly,
        Category = "Objectives"
    )
    bool bMissionComplete = false;

    UPROPERTY(
        ReplicatedUsing = OnRep_MissionState,
        VisibleInstanceOnly,
        BlueprintReadOnly,
        Category = "Objectives"
    )
    bool bMissionFailed = false;

    UPROPERTY(
        VisibleInstanceOnly,
        BlueprintReadOnly,
        Category = "Objectives|Presentation"
    )
    TMap<FName, FText> ObjectiveDisplayOverrides;

    UPROPERTY(VisibleInstanceOnly, Category = "Objectives")
    TArray<FBHObjectiveDefinition> RuntimeObjectives;

private:
    const FBHObjectiveDefinition* FindObjectiveDefinition(
        FName ObjectiveID
    ) const;

    void SetCurrentObjectiveFromIndex(int32 ObjectiveIndex);

    int32 FindObjectiveIndex(FName ObjectiveID) const;

    bool AreAllMissionObjectivesCompleted() const;

    bool SetCurrentObjectiveToFirstIncomplete();

    const TArray<FBHObjectiveDefinition>&
        GetObjectiveDefinitions() const;

    int32 CurrentObjectiveIndex = INDEX_NONE;
};
