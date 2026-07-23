#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "BHObjectiveComponent.generated.h"

class UBHMissionData;
struct FBHObjectiveDefinition;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
    FOnObjectiveCompleted,
    FName,
    CompletedObjectiveID,
    FText,
    CompletedObjectiveText
);

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class BROKENHORIZON_API UBHObjectiveComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UBHObjectiveComponent();

    UFUNCTION(BlueprintCallable, Category = "Objectives")
    void StartMission(UBHMissionData* NewMissionData);

    UFUNCTION(BlueprintCallable, Category = "Objectives")
    bool CompleteObjectiveByID(FName ObjectiveID);

    UFUNCTION(BlueprintPure, Category = "Objectives")
    FName GetCurrentObjectiveID() const;

    UFUNCTION(BlueprintPure, Category = "Objectives")
    TArray<FName> GetCompletedObjectiveIDs() const;

    UFUNCTION(BlueprintPure, Category = "Objectives")
    FText ResolveObjectiveDisplayText(FName ObjectiveID) const;

    UFUNCTION(BlueprintPure, Category = "Objectives")
    FText GetCurrentObjectiveText() const;

    UFUNCTION(BlueprintPure, Category = "Objectives")
    TArray<FText> GetCompletedObjectiveTexts() const;

    UPROPERTY(BlueprintAssignable, Category = "Objectives")
    FOnObjectiveCompleted OnObjectiveCompleted;

protected:
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Objectives")
    TObjectPtr<UBHMissionData> ActiveMissionData;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Objectives")
    FName CurrentObjectiveID = NAME_None;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Objectives")
    TArray<FName> CompletedObjectiveIDs;

private:
    const FBHObjectiveDefinition* FindObjectiveDefinition(
        FName ObjectiveID
    ) const;

    void SetCurrentObjectiveFromIndex(int32 ObjectiveIndex);

    int32 CurrentObjectiveIndex = INDEX_NONE;
};
