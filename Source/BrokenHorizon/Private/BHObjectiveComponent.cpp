#include "BHObjectiveComponent.h"

#include "BHCharacter.h"
#include "BHMissionData.h"
#include "BHPlaytestTelemetrySubsystem.h"
#include "Engine/GameInstance.h"
#include "Net/UnrealNetwork.h"

UBHObjectiveComponent::UBHObjectiveComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    SetIsReplicatedByDefault(true);
}

void UBHObjectiveComponent::GetLifetimeReplicatedProps(
    TArray<FLifetimeProperty>& OutLifetimeProps
) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(UBHObjectiveComponent, CurrentObjectiveID);
    DOREPLIFETIME(UBHObjectiveComponent, CompletedObjectiveIDs);
    DOREPLIFETIME(UBHObjectiveComponent, bMissionComplete);
    DOREPLIFETIME(UBHObjectiveComponent, bMissionFailed);
}

void UBHObjectiveComponent::OnRep_MissionState()
{
    if (ABHCharacter* Character = Cast<ABHCharacter>(GetOwner()))
    {
        Character->RefreshReplicatedMissionPresentation();
        UE_LOG(
            LogTemp,
            Display,
            TEXT(
                "BH_MISSION_STATE_REPLICATED owner=%s current=%s "
                "completed=%d complete=%d failed=%d"
            ),
            *Character->GetName(),
            *CurrentObjectiveID.ToString(),
            CompletedObjectiveIDs.Num(),
            bMissionComplete ? 1 : 0,
            bMissionFailed ? 1 : 0
        );
    }
}

void UBHObjectiveComponent::StartMission(
    UBHMissionData* NewMissionData
)
{
    ActiveMissionData = NewMissionData;
    RuntimeObjectives.Reset();
    CompletedObjectiveIDs.Reset();
    CurrentObjectiveID = NAME_None;
    CurrentObjectiveIndex = INDEX_NONE;
    bMissionComplete = false;
    bMissionFailed = false;

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
        : GetObjectiveDefinitions())
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

    if (UGameInstance* GameInstance = GetWorld()
        ? GetWorld()->GetGameInstance()
        : nullptr)
    {
        GameInstance->GetSubsystem<UBHPlaytestTelemetrySubsystem>()->RecordEvent(
            TEXT("mission_started"),
            {{TEXT("objective"), CurrentObjectiveID.ToString()}}
        );
    }
}

void UBHObjectiveComponent::StartRuntimeMission(
    const TArray<FBHObjectiveDefinition>& NewObjectives
)
{
    ActiveMissionData = nullptr;
    RuntimeObjectives = NewObjectives;
    CompletedObjectiveIDs.Reset();
    CurrentObjectiveID = NAME_None;
    CurrentObjectiveIndex = INDEX_NONE;
    bMissionComplete = false;
    bMissionFailed = false;

    if (RuntimeObjectives.IsEmpty())
    {
        UE_LOG(
            LogTemp,
            Warning,
            TEXT("Objective component cannot start an empty runtime mission.")
        );
        return;
    }

    SetCurrentObjectiveFromIndex(0);

    if (UGameInstance* GameInstance = GetWorld()
        ? GetWorld()->GetGameInstance()
        : nullptr)
    {
        GameInstance->GetSubsystem<UBHPlaytestTelemetrySubsystem>()->RecordEvent(
            TEXT("mission_started"),
            {{TEXT("objective"), CurrentObjectiveID.ToString()},
             {TEXT("runtime"), TEXT("true")}}
        );
    }
}

void UBHObjectiveComponent::ClearMissionState()
{
    ActiveMissionData = nullptr;
    RuntimeObjectives.Reset();
    CompletedObjectiveIDs.Reset();
    CurrentObjectiveID = NAME_None;
    CurrentObjectiveIndex = INDEX_NONE;
    bMissionComplete = false;
    bMissionFailed = false;
    ObjectiveDisplayOverrides.Reset();
}

bool UBHObjectiveComponent::CompleteObjectiveByID(
    FName ObjectiveID
)
{
    if (const AActor* OwnerActor = GetOwner();
        IsValid(OwnerActor) && !OwnerActor->HasAuthority())
    {
        UE_LOG(
            LogTemp,
            Verbose,
            TEXT(
                "Ignored client objective completion for %s on %s."
            ),
            *ObjectiveID.ToString(),
            *GetNameSafe(OwnerActor)
        );
        return false;
    }

    if (bMissionFailed ||
        ObjectiveID.IsNone() ||
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
    bMissionComplete = CurrentObjectiveID.IsNone();
    bMissionFailed = false;

    if (UGameInstance* GameInstance = GetWorld()
        ? GetWorld()->GetGameInstance()
        : nullptr)
    {
        GameInstance->GetSubsystem<UBHPlaytestTelemetrySubsystem>()->RecordEvent(
            TEXT("objective_completed"),
            {{TEXT("objective"), CompletedID.ToString()},
             {TEXT("next"), CurrentObjectiveID.ToString()},
             {TEXT("missionComplete"), bMissionComplete ? TEXT("true") : TEXT("false")}}
        );
    }

    OnObjectiveCompleted.Broadcast(
        CompletedID,
        CompletedText
    );

    if (bMissionComplete)
    {
        OnMissionCompleted.Broadcast();
    }

    return true;
}

bool UBHObjectiveComponent::FailMission()
{
    if (const AActor* OwnerActor = GetOwner();
        IsValid(OwnerActor) && !OwnerActor->HasAuthority())
    {
        UE_LOG(
            LogTemp,
            Verbose,
            TEXT("Ignored client mission failure on %s."),
            *GetNameSafe(OwnerActor)
        );
        return false;
    }

    if (bMissionComplete ||
        bMissionFailed ||
        CurrentObjectiveID.IsNone())
    {
        return false;
    }

    const FName FailedObjectiveID = CurrentObjectiveID;
    CurrentObjectiveID = NAME_None;
    CurrentObjectiveIndex = INDEX_NONE;
    bMissionComplete = false;
    bMissionFailed = true;
    if (UGameInstance* GameInstance = GetWorld()
        ? GetWorld()->GetGameInstance()
        : nullptr)
    {
        GameInstance->GetSubsystem<UBHPlaytestTelemetrySubsystem>()->RecordEvent(
            TEXT("mission_failed"),
            {{TEXT("objective"), FailedObjectiveID.ToString()}}
        );
    }
    return true;
}

bool UBHObjectiveComponent::RestoreMissionState(
    UBHMissionData* MissionData,
    FName SavedCurrentObjectiveID,
    const TArray<FName>& SavedCompletedObjectiveIDs,
    bool bSavedMissionComplete,
    bool bSavedMissionFailed
)
{
    ActiveMissionData = MissionData;
    RuntimeObjectives.Reset();
    CurrentObjectiveID = NAME_None;
    CompletedObjectiveIDs.Reset();
    CurrentObjectiveIndex = INDEX_NONE;
    bMissionComplete = false;
    bMissionFailed = false;

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

    if (bSavedMissionFailed)
    {
        bMissionFailed = true;
        return true;
    }

    if (SavedCurrentObjectiveID.IsNone())
    {
        bMissionComplete =
            bSavedMissionComplete ||
            AreAllMissionObjectivesCompleted();

        if (!bMissionComplete)
        {
            SetCurrentObjectiveToFirstIncomplete();
        }

        return true;
    }

    const int32 SavedObjectiveIndex =
        FindObjectiveIndex(SavedCurrentObjectiveID);

    if (SavedObjectiveIndex == INDEX_NONE)
    {
        UE_LOG(
            LogTemp,
            Warning,
            TEXT(
                "Saved current objective %s is not present in "
                "mission %s; advancing to its first incomplete "
                "objective."
            ),
            *SavedCurrentObjectiveID.ToString(),
            *ActiveMissionData->GetName()
        );

        bMissionComplete =
            bSavedMissionComplete ||
            !SetCurrentObjectiveToFirstIncomplete();
        return true;
    }

    CurrentObjectiveIndex = SavedObjectiveIndex;
    CurrentObjectiveID = SavedCurrentObjectiveID;

    if (bSavedMissionComplete)
    {
        UE_LOG(
            LogTemp,
            Warning,
            TEXT(
                "Ignored saved mission-complete flag because "
                "current objective %s is still active."
            ),
            *SavedCurrentObjectiveID.ToString()
        );
    }

    return true;
}

bool UBHObjectiveComponent::RestoreRuntimeMissionState(
    const TArray<FBHObjectiveDefinition>& SavedObjectives,
    FName SavedCurrentObjectiveID,
    const TArray<FName>& SavedCompletedObjectiveIDs,
    bool bSavedMissionComplete,
    bool bSavedMissionFailed
)
{
    RuntimeObjectives = SavedObjectives;
    ActiveMissionData = nullptr;

    if (RuntimeObjectives.IsEmpty())
    {
        return false;
    }

    CompletedObjectiveIDs.Reset();

    for (const FName CompletedID : SavedCompletedObjectiveIDs)
    {
        if (!CompletedID.IsNone() &&
            FindObjectiveDefinition(CompletedID) &&
            !CompletedObjectiveIDs.Contains(CompletedID))
        {
            CompletedObjectiveIDs.Add(CompletedID);
        }
    }

    CurrentObjectiveID = SavedCurrentObjectiveID;
    CurrentObjectiveIndex =
        FindObjectiveIndex(CurrentObjectiveID);
    bMissionComplete = bSavedMissionComplete;
    bMissionFailed = bSavedMissionFailed;

    if (bMissionFailed)
    {
        CurrentObjectiveID = NAME_None;
        CurrentObjectiveIndex = INDEX_NONE;
        bMissionComplete = false;
        return true;
    }

    if (!bMissionComplete &&
        CurrentObjectiveIndex == INDEX_NONE)
    {
        bMissionComplete =
            !SetCurrentObjectiveToFirstIncomplete();
    }

    return true;
}

UBHMissionData* UBHObjectiveComponent::GetActiveMissionData() const
{
    return ActiveMissionData;
}

const TArray<FBHObjectiveDefinition>&
UBHObjectiveComponent::GetRuntimeObjectiveDefinitions() const
{
    return RuntimeObjectives;
}

bool UBHObjectiveComponent::IsRuntimeMission() const
{
    return !RuntimeObjectives.IsEmpty();
}

FName UBHObjectiveComponent::GetCurrentObjectiveID() const
{
    return CurrentObjectiveID;
}

TArray<FName> UBHObjectiveComponent::GetCompletedObjectiveIDs() const
{
    return CompletedObjectiveIDs;
}

bool UBHObjectiveComponent::IsObjectiveCompleted(
    FName ObjectiveID
) const
{
    return !ObjectiveID.IsNone() &&
        CompletedObjectiveIDs.Contains(ObjectiveID);
}

bool UBHObjectiveComponent::IsMissionComplete() const
{
    return bMissionComplete;
}

bool UBHObjectiveComponent::IsMissionFailed() const
{
    return bMissionFailed;
}

FText UBHObjectiveComponent::ResolveObjectiveDisplayText(
    FName ObjectiveID
) const
{
    if (const FText* OverrideText =
        ObjectiveDisplayOverrides.Find(ObjectiveID);
        OverrideText && !OverrideText->IsEmpty())
    {
        return *OverrideText;
    }

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

void UBHObjectiveComponent::SetObjectiveDisplayOverride(
    FName ObjectiveID,
    const FText& DisplayText
)
{
    if (ObjectiveID.IsNone() || DisplayText.IsEmpty())
    {
        return;
    }

    ObjectiveDisplayOverrides.Add(ObjectiveID, DisplayText);
}

void UBHObjectiveComponent::ClearObjectiveDisplayOverrides()
{
    ObjectiveDisplayOverrides.Reset();
}

const FBHObjectiveDefinition*
UBHObjectiveComponent::FindObjectiveDefinition(
    FName ObjectiveID
) const
{
    if (!IsValid(ActiveMissionData) || ObjectiveID.IsNone())
    {
        if (RuntimeObjectives.IsEmpty() || ObjectiveID.IsNone())
        {
            return nullptr;
        }
    }

    const TArray<FBHObjectiveDefinition>& Definitions =
        GetObjectiveDefinitions();

    if (Definitions.IsEmpty())
    {
        return nullptr;
    }

    return Definitions.FindByPredicate(
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
    const TArray<FBHObjectiveDefinition>& Definitions =
        GetObjectiveDefinitions();

    if (!Definitions.IsValidIndex(ObjectiveIndex))
    {
        CurrentObjectiveIndex = INDEX_NONE;
        CurrentObjectiveID = NAME_None;
        return;
    }

    CurrentObjectiveIndex = ObjectiveIndex;
    CurrentObjectiveID =
        Definitions[ObjectiveIndex].ObjectiveID;
}

int32 UBHObjectiveComponent::FindObjectiveIndex(
    FName ObjectiveID
) const
{
    const TArray<FBHObjectiveDefinition>& Definitions =
        GetObjectiveDefinitions();

    if (Definitions.IsEmpty() || ObjectiveID.IsNone())
    {
        return INDEX_NONE;
    }

    return Definitions.IndexOfByPredicate(
        [ObjectiveID](const FBHObjectiveDefinition& Definition)
        {
            return Definition.ObjectiveID == ObjectiveID;
        }
    );
}

bool UBHObjectiveComponent::AreAllMissionObjectivesCompleted() const
{
    const TArray<FBHObjectiveDefinition>& Definitions =
        GetObjectiveDefinitions();

    if (Definitions.IsEmpty())
    {
        return false;
    }

    for (const FBHObjectiveDefinition& Definition
        : Definitions)
    {
        if (Definition.ObjectiveID.IsNone() ||
            !CompletedObjectiveIDs.Contains(Definition.ObjectiveID))
        {
            return false;
        }
    }

    return true;
}

bool UBHObjectiveComponent::SetCurrentObjectiveToFirstIncomplete()
{
    const TArray<FBHObjectiveDefinition>& Definitions =
        GetObjectiveDefinitions();

    if (Definitions.IsEmpty())
    {
        return false;
    }

    for (int32 Index = 0;
        Index < Definitions.Num();
        ++Index)
    {
        const FName ObjectiveID =
            Definitions[Index].ObjectiveID;

        if (!ObjectiveID.IsNone() &&
            !CompletedObjectiveIDs.Contains(ObjectiveID))
        {
            SetCurrentObjectiveFromIndex(Index);
            return true;
        }
    }

    CurrentObjectiveIndex = INDEX_NONE;
    CurrentObjectiveID = NAME_None;
    return false;
}

const TArray<FBHObjectiveDefinition>&
UBHObjectiveComponent::GetObjectiveDefinitions() const
{
    return RuntimeObjectives.IsEmpty() && IsValid(ActiveMissionData)
        ? ActiveMissionData->Objectives
        : RuntimeObjectives;
}
