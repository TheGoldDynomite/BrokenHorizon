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

bool UBHObjectiveComponent::RestoreMissionState(
    UBHMissionData* MissionData,
    FName SavedCurrentObjectiveID,
    const TArray<FName>& SavedCompletedObjectiveIDs
)
{
    ActiveMissionData = MissionData;
    CurrentObjectiveID = NAME_None;
    CompletedObjectiveIDs.Reset();
    CurrentObjectiveIndex = INDEX_NONE;

    if (!IsValid(ActiveMissionData))
    {
        UE_LOG(
            LogTemp,
            Error,
            TEXT("Cannot restore objective state without mission data.")
        );
        return false;
    }

    TSet<FName> RestoredCompletedIDs;

    for (const FName CompletedID : SavedCompletedObjectiveIDs)
    {
        if (!FindObjectiveDefinition(CompletedID))
        {
            UE_LOG(
                LogTemp,
                Warning,
                TEXT(
                    "Skipped saved objective %s because it is not "
                    "present in mission %s."
                ),
                *CompletedID.ToString(),
                *ActiveMissionData->GetName()
            );
            continue;
        }

        if (RestoredCompletedIDs.Contains(CompletedID))
        {
            UE_LOG(
                LogTemp,
                Warning,
                TEXT("Skipped duplicate saved objective %s."),
                *CompletedID.ToString()
            );
            continue;
        }

        RestoredCompletedIDs.Add(CompletedID);
        CompletedObjectiveIDs.Add(CompletedID);
    }

    if (SavedCurrentObjectiveID.IsNone())
    {
        return true;
    }

    const int32 SavedObjectiveIndex =
        FindObjectiveIndex(SavedCurrentObjectiveID);

    if (SavedObjectiveIndex == INDEX_NONE)
    {
        UE_LOG(
            LogTemp,
            Error,
            TEXT(
                "Cannot restore current objective %s because it is "
                "not present in mission %s."
            ),
            *SavedCurrentObjectiveID.ToString(),
            *ActiveMissionData->GetName()
        );
        return false;
    }

    CurrentObjectiveIndex = SavedObjectiveIndex;
    CurrentObjectiveID = SavedCurrentObjectiveID;
    return true;
}

UBHMissionData* UBHObjectiveComponent::GetActiveMissionData() const
{
    return ActiveMissionData;
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

int32 UBHObjectiveComponent::FindObjectiveIndex(
    FName ObjectiveID
) const
{
    if (!IsValid(ActiveMissionData) || ObjectiveID.IsNone())
    {
        return INDEX_NONE;
    }

    return ActiveMissionData->Objectives.IndexOfByPredicate(
        [ObjectiveID](const FBHObjectiveDefinition& Definition)
        {
            return Definition.ObjectiveID == ObjectiveID;
        }
    );
}
