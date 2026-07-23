#include "BHObjectiveComponent.h"

#include "BHMissionData.h"

UBHObjectiveComponent::UBHObjectiveComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UBHObjectiveComponent::StartMission(
    UBHMissionData* NewMissionData
)
{
    ActiveMissionData = NewMissionData;
    CompletedObjectiveIDs.Reset();
    CurrentObjectiveID = NAME_None;
    CurrentObjectiveIndex = INDEX_NONE;

    if (!IsValid(ActiveMissionData))
    {
        UE_LOG(
            LogTemp,
            Warning,
            TEXT("Objective component cannot start a null mission.")
        );
        return;
    }

    TSet<FName> ObjectiveIDs;

    for (const FBHObjectiveDefinition& Definition
        : ActiveMissionData->Objectives)
    {
        if (Definition.ObjectiveID.IsNone())
        {
            UE_LOG(
                LogTemp,
                Error,
                TEXT("Mission %s contains an objective with no ID."),
                *ActiveMissionData->GetName()
            );
            continue;
        }

        if (ObjectiveIDs.Contains(Definition.ObjectiveID))
        {
            UE_LOG(
                LogTemp,
                Error,
                TEXT("Mission %s contains duplicate objective ID %s."),
                *ActiveMissionData->GetName(),
                *Definition.ObjectiveID.ToString()
            );
        }

        ObjectiveIDs.Add(Definition.ObjectiveID);
    }

    SetCurrentObjectiveFromIndex(0);
}

bool UBHObjectiveComponent::CompleteObjectiveByID(
    FName ObjectiveID
)
{
    if (ObjectiveID.IsNone() ||
        CurrentObjectiveID.IsNone() ||
        ObjectiveID != CurrentObjectiveID)
    {
        UE_LOG(
            LogTemp,
            Warning,
            TEXT(
                "Ignored objective completion for %s; "
                "the expected objective is %s."
            ),
            *ObjectiveID.ToString(),
            *CurrentObjectiveID.ToString()
        );
        return false;
    }

    const FName CompletedID = CurrentObjectiveID;
    const FText CompletedText =
        ResolveObjectiveDisplayText(CompletedID);

    CompletedObjectiveIDs.Add(CompletedID);
    SetCurrentObjectiveFromIndex(CurrentObjectiveIndex + 1);

    OnObjectiveCompleted.Broadcast(
        CompletedID,
        CompletedText
    );

    return true;
}

FName UBHObjectiveComponent::GetCurrentObjectiveID() const
{
    return CurrentObjectiveID;
}

TArray<FName> UBHObjectiveComponent::GetCompletedObjectiveIDs() const
{
    return CompletedObjectiveIDs;
}

FText UBHObjectiveComponent::ResolveObjectiveDisplayText(
    FName ObjectiveID
) const
{
    const FBHObjectiveDefinition* Definition =
        FindObjectiveDefinition(ObjectiveID);

    return Definition
        ? Definition->DisplayText
        : FText::GetEmpty();
}

FText UBHObjectiveComponent::GetCurrentObjectiveText() const
{
    return ResolveObjectiveDisplayText(CurrentObjectiveID);
}

TArray<FText> UBHObjectiveComponent::GetCompletedObjectiveTexts() const
{
    TArray<FText> CompletedTexts;
    CompletedTexts.Reserve(CompletedObjectiveIDs.Num());

    for (const FName ObjectiveID : CompletedObjectiveIDs)
    {
        CompletedTexts.Add(
            ResolveObjectiveDisplayText(ObjectiveID)
        );
    }

    return CompletedTexts;
}

const FBHObjectiveDefinition*
UBHObjectiveComponent::FindObjectiveDefinition(
    FName ObjectiveID
) const
{
    if (!IsValid(ActiveMissionData) || ObjectiveID.IsNone())
    {
        return nullptr;
    }

    return ActiveMissionData->Objectives.FindByPredicate(
        [ObjectiveID](const FBHObjectiveDefinition& Definition)
        {
            return Definition.ObjectiveID == ObjectiveID;
        }
    );
}

void UBHObjectiveComponent::SetCurrentObjectiveFromIndex(
    int32 ObjectiveIndex
)
{
    if (!IsValid(ActiveMissionData) ||
        !ActiveMissionData->Objectives.IsValidIndex(ObjectiveIndex))
    {
        CurrentObjectiveIndex = INDEX_NONE;
        CurrentObjectiveID = NAME_None;
        return;
    }

    CurrentObjectiveIndex = ObjectiveIndex;
    CurrentObjectiveID =
        ActiveMissionData->Objectives[ObjectiveIndex].ObjectiveID;
}
