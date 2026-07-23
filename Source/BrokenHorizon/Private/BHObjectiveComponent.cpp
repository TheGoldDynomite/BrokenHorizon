#include "BHObjectiveComponent.h"


UBHObjectiveComponent::UBHObjectiveComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}


void UBHObjectiveComponent::SetObjective(FText NewObjective)
{
    if (!CurrentObjective.IsEmpty())
    {
        CompletedObjectives.Add(CurrentObjective);

        OnObjectiveCompleted.Broadcast(
            CurrentObjective
        );
    }

    CurrentObjective = NewObjective;
}


void UBHObjectiveComponent::CompleteObjective()
{
    CompleteCurrentObjective();
}


void UBHObjectiveComponent::CompleteCurrentObjective()
{
    if (!CurrentObjective.IsEmpty())
    {
        FText FinishedObjective = CurrentObjective;

        CompletedObjectives.Add(
            FinishedObjective
        );

        CurrentObjective = FText::GetEmpty();

        OnObjectiveCompleted.Broadcast(
            FinishedObjective
        );
    }
}


FText UBHObjectiveComponent::GetCurrentObjective() const
{
    return CurrentObjective;
}


TArray<FText> UBHObjectiveComponent::GetCompletedObjectives() const
{
    return CompletedObjectives;
}