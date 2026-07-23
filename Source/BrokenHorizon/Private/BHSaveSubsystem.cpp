#include "BHSaveSubsystem.h"

#include "BHCharacter.h"
#include "BHDoor.h"
#include "BHKeycard.h"
#include "BHMissionData.h"
#include "BHSaveGame.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "UObject/UObjectGlobals.h"

const FString UBHSaveSubsystem::SaveSlotName(
    TEXT("BrokenHorizon_Checkpoint")
);

void UBHSaveSubsystem::Initialize(
    FSubsystemCollectionBase& Collection
)
{
    Super::Initialize(Collection);

    PostLoadMapHandle =
        FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(
            this,
            &UBHSaveSubsystem::HandlePostLoadMap
        );
}

void UBHSaveSubsystem::Deinitialize()
{
    FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(
        PostLoadMapHandle
    );

    PendingSaveData = nullptr;
    Super::Deinitialize();
}

bool UBHSaveSubsystem::SaveProgress()
{
    UWorld* World = GetGameInstance()
        ? GetGameInstance()->GetWorld()
        : nullptr;
    ABHCharacter* Character = FindPlayerCharacter(World);

    if (!IsValid(World) || !IsValid(Character))
    {
        UE_LOG(
            LogTemp,
            Error,
            TEXT("Save failed because the player world is unavailable.")
        );
        return false;
    }

    if (!ValidatePersistenceIDs(World))
    {
        UE_LOG(
            LogTemp,
            Error,
            TEXT(
                "Save failed because persistence IDs are missing "
                "or duplicated."
            )
        );
        return false;
    }

    if (!IsValid(Character->GetMissionData()))
    {
        UE_LOG(
            LogTemp,
            Error,
            TEXT("Save failed because no active mission is assigned.")
        );
        return false;
    }

    UBHSaveGame* SaveData = Cast<UBHSaveGame>(
        UGameplayStatics::CreateSaveGameObject(
            UBHSaveGame::StaticClass()
        )
    );

    if (!IsValid(SaveData))
    {
        return false;
    }

    SaveData->SchemaVersion = BHSave::CurrentSchemaVersion;
    SaveData->SavedLevelName = FName(
        *UGameplayStatics::GetCurrentLevelName(World, true)
    );
    SaveData->MissionData = Character->GetMissionData();
    SaveData->CurrentObjectiveID =
        Character->GetCurrentObjectiveID();
    SaveData->CompletedObjectiveIDs =
        Character->GetCompletedObjectiveIDs();
    SaveData->OwnedKeycardIDs =
        Character->GetOwnedKeycardIDs();
    SaveData->CollectedKeycardPersistenceIDs =
        Character->GetCollectedKeycardPersistenceIDs();
    SaveData->PlayerTransform = Character->GetActorTransform();

    for (TActorIterator<ABHDoor> DoorIt(World);
        DoorIt;
        ++DoorIt)
    {
        ABHDoor* Door = *DoorIt;

        if (IsValid(Door) && Door->IsUnlocked())
        {
            SaveData->UnlockedDoorPersistenceIDs.Add(
                Door->GetPersistenceID()
            );
        }
    }

    SaveData->OwnedKeycardIDs.Sort(FNameLexicalLess());
    SaveData->CollectedKeycardPersistenceIDs.Sort(
        FNameLexicalLess()
    );
    SaveData->UnlockedDoorPersistenceIDs.Sort(
        FNameLexicalLess()
    );

    const bool bSaved = UGameplayStatics::SaveGameToSlot(
        SaveData,
        SaveSlotName,
        SaveUserIndex
    );

    if (bSaved)
    {
        UE_LOG(
            LogTemp,
            Log,
            TEXT("Checkpoint save succeeded in slot %s."),
            *SaveSlotName
        );
    }
    else
    {
        UE_LOG(
            LogTemp,
            Error,
            TEXT("Checkpoint save failed in slot %s."),
            *SaveSlotName
        );
    }

    return bSaved;
}

bool UBHSaveSubsystem::LoadProgress()
{
    if (!HasSaveGame())
    {
        UE_LOG(
            LogTemp,
            Warning,
            TEXT("No checkpoint save exists in slot %s."),
            *SaveSlotName
        );
        return false;
    }

    UBHSaveGame* SaveData = Cast<UBHSaveGame>(
        UGameplayStatics::LoadGameFromSlot(
            SaveSlotName,
            SaveUserIndex
        )
    );

    if (!IsValid(SaveData))
    {
        UE_LOG(LogTemp, Error, TEXT("Checkpoint save is invalid."));
        return false;
    }

    if (SaveData->SchemaVersion != BHSave::CurrentSchemaVersion)
    {
        UE_LOG(
            LogTemp,
            Error,
            TEXT(
                "Unsupported save schema version %d; expected %d."
            ),
            SaveData->SchemaVersion,
            BHSave::CurrentSchemaVersion
        );
        return false;
    }

    UWorld* World = GetGameInstance()
        ? GetGameInstance()->GetWorld()
        : nullptr;

    if (!IsValid(World))
    {
        return false;
    }

    const FName LevelToLoad =
        SaveData->SavedLevelName.IsNone()
        ? FName(*UGameplayStatics::GetCurrentLevelName(World, true))
        : SaveData->SavedLevelName;

    PendingSaveData = SaveData;
    UGameplayStatics::OpenLevel(World, LevelToLoad);
    return true;
}

bool UBHSaveSubsystem::HasSaveGame() const
{
    return UGameplayStatics::DoesSaveGameExist(
        SaveSlotName,
        SaveUserIndex
    );
}

ABHCharacter* UBHSaveSubsystem::FindPlayerCharacter(
    UWorld* World
) const
{
    return IsValid(World)
        ? Cast<ABHCharacter>(
            UGameplayStatics::GetPlayerCharacter(World, 0)
        )
        : nullptr;
}

bool UBHSaveSubsystem::ValidatePersistenceIDs(
    UWorld* World
) const
{
    if (!IsValid(World))
    {
        return false;
    }

    bool bValid = true;
    TMap<FName, const AActor*> KeycardIDs;
    TMap<FName, const AActor*> DoorIDs;

    for (TActorIterator<ABHKeycard> KeycardIt(World);
        KeycardIt;
        ++KeycardIt)
    {
        const ABHKeycard* Keycard = *KeycardIt;
        const FName PersistenceID = Keycard->GetPersistenceID();

        if (PersistenceID.IsNone())
        {
            UE_LOG(
                LogTemp,
                Error,
                TEXT("Keycard %s has no persistence ID."),
                *Keycard->GetPathName()
            );
            bValid = false;
            continue;
        }

        if (const AActor* const* Existing =
            KeycardIDs.Find(PersistenceID))
        {
            UE_LOG(
                LogTemp,
                Error,
                TEXT(
                    "Duplicate keycard persistence ID %s on %s "
                    "and %s."
                ),
                *PersistenceID.ToString(),
                *(*Existing)->GetPathName(),
                *Keycard->GetPathName()
            );
            bValid = false;
            continue;
        }

        KeycardIDs.Add(PersistenceID, Keycard);
    }

    for (TActorIterator<ABHDoor> DoorIt(World);
        DoorIt;
        ++DoorIt)
    {
        const ABHDoor* Door = *DoorIt;
        const FName PersistenceID = Door->GetPersistenceID();

        if (PersistenceID.IsNone())
        {
            UE_LOG(
                LogTemp,
                Error,
                TEXT("Door %s has no persistence ID."),
                *Door->GetPathName()
            );
            bValid = false;
            continue;
        }

        if (const AActor* const* Existing =
            DoorIDs.Find(PersistenceID))
        {
            UE_LOG(
                LogTemp,
                Error,
                TEXT(
                    "Duplicate door persistence ID %s on %s "
                    "and %s."
                ),
                *PersistenceID.ToString(),
                *(*Existing)->GetPathName(),
                *Door->GetPathName()
            );
            bValid = false;
            continue;
        }

        DoorIDs.Add(PersistenceID, Door);
    }

    return bValid;
}

bool UBHSaveSubsystem::ApplySaveData(
    UBHSaveGame* SaveData,
    UWorld* World
)
{
    ABHCharacter* Character = FindPlayerCharacter(World);

    if (!IsValid(SaveData) ||
        !IsValid(Character) ||
        !ValidatePersistenceIDs(World))
    {
        return false;
    }

    UBHMissionData* MissionData =
        SaveData->MissionData.LoadSynchronous();

    if (!IsValid(MissionData))
    {
        UE_LOG(
            LogTemp,
            Error,
            TEXT("Saved mission asset could not be loaded.")
        );
        return false;
    }

    const bool bRestoredCharacter =
        Character->RestorePersistentState(
            MissionData,
            SaveData->CurrentObjectiveID,
            SaveData->CompletedObjectiveIDs,
            SaveData->OwnedKeycardIDs,
            SaveData->CollectedKeycardPersistenceIDs
        );

    if (!bRestoredCharacter)
    {
        return false;
    }

    Character->SetActorTransform(
        SaveData->PlayerTransform,
        false,
        nullptr,
        ETeleportType::TeleportPhysics
    );

    const TSet<FName> CollectedKeycards(
        SaveData->CollectedKeycardPersistenceIDs
    );

    for (TActorIterator<ABHKeycard> KeycardIt(World);
        KeycardIt;
        ++KeycardIt)
    {
        ABHKeycard* Keycard = *KeycardIt;

        if (CollectedKeycards.Contains(
            Keycard->GetPersistenceID()))
        {
            Keycard->Destroy();
        }
    }

    const TSet<FName> UnlockedDoors(
        SaveData->UnlockedDoorPersistenceIDs
    );

    for (TActorIterator<ABHDoor> DoorIt(World);
        DoorIt;
        ++DoorIt)
    {
        ABHDoor* Door = *DoorIt;
        Door->RestoreUnlockedState(
            UnlockedDoors.Contains(Door->GetPersistenceID())
        );
    }

    UE_LOG(
        LogTemp,
        Log,
        TEXT("Checkpoint load succeeded from slot %s."),
        *SaveSlotName
    );
    return true;
}

void UBHSaveSubsystem::HandlePostLoadMap(UWorld* LoadedWorld)
{
    if (!IsValid(PendingSaveData) ||
        !IsValid(LoadedWorld) ||
        !LoadedWorld->IsGameWorld())
    {
        return;
    }

    FTimerDelegate ApplyDelegate;
    ApplyDelegate.BindUObject(
        this,
        &UBHSaveSubsystem::ApplyPendingSave,
        LoadedWorld
    );
    LoadedWorld->GetTimerManager().SetTimerForNextTick(
        ApplyDelegate
    );
}

void UBHSaveSubsystem::ApplyPendingSave(UWorld* LoadedWorld)
{
    UBHSaveGame* SaveData = PendingSaveData;
    PendingSaveData = nullptr;

    if (!ApplySaveData(SaveData, LoadedWorld))
    {
        UE_LOG(
            LogTemp,
            Error,
            TEXT("Checkpoint state could not be applied.")
        );
    }
}
