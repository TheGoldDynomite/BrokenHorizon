#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "BHObjectiveComponent.generated.h"


DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
    FOnObjectiveCompleted,
    FText,
    CompletedObjective
);


UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class BROKENHORIZON_API UBHObjectiveComponent : public UActorComponent
{
    GENERATED_BODY()

public:

    UBHObjectiveComponent();


    UFUNCTION(BlueprintCallable, Category = "Objectives")
    void SetObjective(FText NewObjective);


    UFUNCTION(BlueprintCallable, Category = "Objectives")
    void CompleteObjective();


    UFUNCTION(BlueprintCallable, Category = "Objectives")
    void CompleteCurrentObjective();


    UFUNCTION(BlueprintPure, Category = "Objectives")
    FText GetCurrentObjective() const;


    UFUNCTION(BlueprintPure, Category = "Objectives")
    TArray<FText> GetCompletedObjectives() const;


    UPROPERTY(BlueprintAssignable, Category = "Objectives")
    FOnObjectiveCompleted OnObjectiveCompleted;


protected:

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objectives")
    FText CurrentObjective;


    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objectives")
    TArray<FText> CompletedObjectives;

};