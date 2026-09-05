#include "BHSaveSubsystem.h"

#include "BHCharacter.h"
#include "BHAmbientWarDirector.h"
#include "BHDoor.h"
#include "BHFieldFortification.h"
#include "BHFieldTransport.h"
#include "BHGameShellSettings.h"
#include "BHHealthComponent.h"
#include "BHInjuryComponent.h"
#include "BHKeycard.h"
#include "BHMissionItemContainer.h"
#include "BHMissionData.h"
#include "BHPlayerResolver.h"
#include "BHSaveGame.h"
#include "BHEnemySoldier.h"
#include "BHSupplyBase.h"
#include "BHSupplyConvoyTarget.h"
#include "BHWarSubsystem.h"
#include "BHWeaponComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Crc.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Misc/Parse.h"
#include "Subsystems/SubsystemCollection.h"
#include "Templates/UnrealTemplate.h"
#include "TimerManager.h"
#include "UObject/UObjectGlobals.h"

namespace
{
constexpr uint32 BHSaveEnvelopeMagic = 0x45534842;
constexpr uint32 BHLegacySaveMagic = 0x53415647;
constexpr uint32 BHSaveEnvelopeVersion = 1;
constexpr int32 BHMaximumCheckpointBytes = 128 * 1024 * 1024;

struct FBHSaveEnvelopeHeader
{
    uint32 Magic = BHSaveEnvelopeMagic;
    uint32 Version = BHSaveEnvelopeVersion;
    uint32 PayloadSize = 0;
    uint32 PayloadCrc = 0;
};

static_assert(sizeof(FBHSaveEnvelopeHeader) == 16);

UBHSaveGame* LoadProtectedCampaignSave(
    const FString& SlotName,
    int32 UserIndex
)
{
    TArray<uint8> StoredBytes;
    if (!UGameplayStatics::LoadDataFromSlot(
            StoredBytes,
            SlotName,
            UserIndex
        ) ||
        StoredBytes.Num() < static_cast<int32>(sizeof(uint32)))
    {
        return nullptr;
    }

    uint32 StoredMagic = 0;
    FMemory::Memcpy(
        &StoredMagic,
        StoredBytes.GetData(),
        sizeof(StoredMagic)
    );

    TArray<uint8> Payload;
    if (StoredMagic == BHSaveEnvelopeMagic)
    {
        if (StoredBytes.Num() <
            static_cast<int32>(sizeof(FBHSaveEnvelopeHeader)))
        {
            return nullptr;
        }

        FBHSaveEnvelopeHeader Header;
        FMemory::Memcpy(
            &Header,
            StoredBytes.GetData(),
            sizeof(Header)
        );
        const int64 ExpectedStoredSize =
            sizeof(Header) + static_cast<int64>(Header.PayloadSize);
        if (Header.Version != BHSaveEnvelopeVersion ||
            Header.PayloadSize == 0 ||
            Header.PayloadSize > BHMaximumCheckpointBytes ||
            ExpectedStoredSize != StoredBytes.Num())
        {
            UE_LOG(
                LogTemp,
                Warning,
                TEXT(
                    "BH_CHECKPOINT_INTEGRITY_REJECTED slot=%s "
                    "reason=envelope"
                ),
                *SlotName
            );
            return nullptr;
        }

        const uint8* PayloadData =
            StoredBytes.GetData() + sizeof(Header);
        const uint32 ActualCrc = FCrc::MemCrc32(
            PayloadData,
            static_cast<int32>(Header.PayloadSize)
        );
        if (ActualCrc != Header.PayloadCrc)
        {
            UE_LOG(
                LogTemp,
                Warning,
                TEXT(
                    "BH_CHECKPOINT_INTEGRITY_REJECTED slot=%s "
                    "reason=crc"
                ),
                *SlotName
            );
            return nullptr;
        }

        Payload.Append(PayloadData, Header.PayloadSize);
    }
    else if (StoredMagic == BHLegacySaveMagic)
    {
        Payload = MoveTemp(StoredBytes);
    }
    else
    {
        UE_LOG(
            LogTemp,
            Warning,
            TEXT(
                "BH_CHECKPOINT_INTEGRITY_REJECTED slot=%s "
                "reason=magic"
            ),
            *SlotName
        );
        return nullptr;
    }

    return Cast<UBHSaveGame>(
        UGameplayStatics::LoadGameFromMemory(Payload)
    );
}

bool SaveProtectedCampaignSave(
    UBHSaveGame* SaveData,
    const FString& SlotName,
    int32 UserIndex
)
{
    if (!IsValid(SaveData))
    {
        return false;
    }

#if !UE_BUILD_SHIPPING
    int32 TestLegacySchemaVersion = 0;
    if (FParse::Value(
            FCommandLine::Get(),
            TEXT("BHTestLegacySaveSchema="),
            TestLegacySchemaVersion
        ) &&
        TestLegacySchemaVersion >=
            BHSave::MinimumCompatibleSchemaVersion &&
        TestLegacySchemaVersion < BHSave::CurrentSchemaVersion)
    {
        const int32 OriginalSchemaVersion = SaveData->SchemaVersion;
        SaveData->SchemaVersion = TestLegacySchemaVersion;
        const bool bLegacySaved = UGameplayStatics::SaveGameToSlot(
            SaveData,
            SlotName,
            UserIndex
        );
        SaveData->SchemaVersion = OriginalSchemaVersion;
        UE_LOG(
            LogTemp,
            Display,
            TEXT(
                "BH_TEST_LEGACY_CHECKPOINT_WRITTEN slot=%s "
                "schema=%d result=%s"
            ),
            *SlotName,
            TestLegacySchemaVersion,
            bLegacySaved ? TEXT("success") : TEXT("failure")
        );
        return bLegacySaved;
    }
#endif

    TArray<uint8> Payload;
    if (!UGameplayStatics::SaveGameToMemory(SaveData, Payload) ||
        Payload.IsEmpty() ||
        Payload.Num() > BHMaximumCheckpointBytes)
    {
        return false;
    }

    FBHSaveEnvelopeHeader Header;
    Header.PayloadSize = Payload.Num();
    Header.PayloadCrc = FCrc::MemCrc32(
        Payload.GetData(),
        Payload.Num()
    );

    TArray<uint8> StoredBytes;
    StoredBytes.Reserve(sizeof(Header) + Payload.Num());
    StoredBytes.Append(
        reinterpret_cast<const uint8*>(&Header),
        sizeof(Header)
    );
    StoredBytes.Append(Payload);
    return UGameplayStatics::SaveDataToSlot(
        StoredBytes,
        SlotName,
        UserIndex
    );
}

bool IsSaveForConfiguredGameplayMap(const UBHSaveGame* SaveData)
{
    if (!IsValid(SaveData) || SaveData->SavedLevelName.IsNone())
    {
        return false;
    }

    const UBHGameShellSettings* ShellSettings =
        GetDefault<UBHGameShellSettings>();

    if (!IsValid(ShellSettings) ||
        ShellSettings->GameplayMap.IsNull())
    {
        return false;
    }

    TArray<TSoftObjectPtr<UWorld>> GameplayMaps =
        ShellSettings->AdditionalGameplayMaps;
    GameplayMaps.Insert(ShellSettings->GameplayMap, 0);

    for (const TSoftObjectPtr<UWorld>& GameplayMap : GameplayMaps)
    {
        const FString GameplayPackageName =
            GameplayMap.ToSoftObjectPath().GetLongPackageName();
        if (!GameplayPackageName.IsEmpty() &&
            SaveData->SavedLevelName ==
                FName(*FPackageName::GetShortName(
                    GameplayPackageName)))
        {
            return true;
        }
    }
    return false;
}

bool IsCompatibleCampaignSave(const UBHSaveGame* SaveData)
{
    return IsValid(SaveData) &&
        SaveData->SchemaVersion >=
            BHSave::MinimumCompatibleSchemaVersion &&
        SaveData->SchemaVersion <=
            BHSave::CurrentSchemaVersion &&
        IsSaveForConfiguredGameplayMap(SaveData) &&
        !SaveData->MissionData.IsNull();
}

void CapturePlayerResourceState(
    UBHSaveGame* SaveData,
    const ABHCharacter* Character
)
{
    if (!IsValid(SaveData) || !IsValid(Character))
    {
        return;
    }

    if (const UBHHealthComponent* HealthComponent =
        Character->GetHealthComponent())
    {
        SaveData->SavedHealth =
            HealthComponent->GetCurrentHealth();
    }

    if (const UBHInjuryComponent* InjuryComponent =
        Character->GetInjuryComponent())
    {
        SaveData->bHasSavedInjuryState = true;
        SaveData->bSavedBleeding =
            InjuryComponent->IsBleeding();
        SaveData->SavedBleedRate =
            InjuryComponent->GetBleedRate();
        SaveData->bSavedArmInjured =
            InjuryComponent->IsArmInjured();
        SaveData->bSavedLegInjured =
            InjuryComponent->IsLegInjured();
        SaveData->SavedMedkitCount =
            InjuryComponent->GetMedkitCount();
        SaveData->SavedFieldDressingCount =
            InjuryComponent->GetFieldDressingCount();
        SaveData->SavedHelmetDurability =
            InjuryComponent->GetHelmetDurability();
        SaveData->SavedBodyArmorDurability =
            InjuryComponent->GetBodyArmorDurability();
    }

    if (const UBHWeaponComponent* WeaponComponent =
        Character->GetWeaponComponent())
    {
        SaveData->SavedMagazineAmmo =
            WeaponComponent->GetMagazineAmmo();
        SaveData->SavedReserveAmmo =
            WeaponComponent->GetReserveAmmo();
        SaveData->SavedWeaponHeatNormalized =
            WeaponComponent->GetWeaponHeatNormalized();
        SaveData->bSavedWeaponOverheated =
            WeaponComponent->IsWeaponOverheated();
        SaveData->bSavedWeaponHeatStateValid = true;
        SaveData->SavedFireMode =
            WeaponComponent->GetFireMode();
        SaveData->bSavedFireModeStateValid = true;
        SaveData->SavedWeaponRole =
            WeaponComponent->GetWeaponRole();
    }

    SaveData->SavedFragGrenadeCount =
        Character->GetFragGrenadeCount();
    SaveData->SavedSmokeGrenadeCount =
        Character->GetSmokeGrenadeCount();
    SaveData->SavedTacticalFlashlightBattery =
        Character->GetTacticalFlashlightBattery();
    SaveData->bSavedTacticalFlashlightOn =
        Character->IsTacticalFlashlightOn();
    SaveData->SavedEngineeringChargeCount =
        Character->GetEngineeringChargeCount();
    SaveData->SavedAntiVehicleRoundCount =
        Character->GetAntiVehicleRoundCount();
}

void CaptureWarState(
    UBHSaveGame* SaveData,
    UGameInstance* GameInstance
)
{
    if (!IsValid(SaveData) || !IsValid(GameInstance))
    {
        return;
    }

    if (const UBHWarSubsystem* WarSubsystem =
        GameInstance->GetSubsystem<UBHWarSubsystem>())
    {
        SaveData->CampaignDifficulty =
            WarSubsystem->GetCampaignDifficulty();
        SaveData->CampaignProgression =
            WarSubsystem->GetCampaignProgression();
        SaveData->WarSectorStates =
            WarSubsystem->GetSectorStates();
        SaveData->WarSupplyConvoys =
            WarSubsystem->GetSupplyConvoys();
        SaveData->WarGarrisonTransfers =
            WarSubsystem->GetGarrisonTransfers();
        SaveData->WarFriendlyManpowerReserve =
            WarSubsystem->GetFactionManpowerReserve(
                EBHWarFaction::Friendly
            );
        SaveData->WarEnemyManpowerReserve =
            WarSubsystem->GetFactionManpowerReserve(
                EBHWarFaction::Enemy
            );
        SaveData->WarFriendlyRecruitmentProgress =
            WarSubsystem->GetFactionRecruitmentProgress(
                EBHWarFaction::Friendly
            );
        SaveData->WarEnemyRecruitmentProgress =
            WarSubsystem->GetFactionRecruitmentProgress(
                EBHWarFaction::Enemy
            );
        SaveData->WarEventHistory =
            WarSubsystem->GetRecentWarEvents();
        SaveData->WarTurnNumber =
            WarSubsystem->GetTurnNumber();
        SaveData->WarSimulationAccumulator =
            WarSubsystem->GetSimulationAccumulator();
        SaveData->WarCommittedOperationID =
            WarSubsystem->GetCommittedOperationID();
        SaveData->WarCommittedOperationTargetID =
            WarSubsystem->GetCommittedOperationTargetID();
    }
}
}

const FString UBHSaveSubsystem::SaveSlotName(
    TEXT("BrokenHorizon_Checkpoint")
);

const FString UBHSaveSubsystem::BackupSaveSlotName(
    TEXT("BrokenHorizon_Checkpoint_Backup")
);

FString UBHSaveSubsystem::GetActiveSaveSlotName()
{
#if !UE_BUILD_SHIPPING
    FString TestSuffix;
    if (FParse::Value(
            FCommandLine::Get(),
            TEXT("BHTestSaveSlotSuffix="),
            TestSuffix))
    {
        TestSuffix = FPaths::MakeValidFileName(TestSuffix);
        TestSuffix.ReplaceInline(TEXT("-"), TEXT("_"));
        if (!TestSuffix.IsEmpty())
        {
            return SaveSlotName + TEXT("_") + TestSuffix;
        }
    }
#endif
    return SaveSlotName;
}

FString UBHSaveSubsystem::GetActiveBackupSaveSlotName()
{
#if !UE_BUILD_SHIPPING
    FString TestSuffix;
    if (FParse::Value(
            FCommandLine::Get(),
            TEXT("BHTestSaveSlotSuffix="),
            TestSuffix))
    {
        TestSuffix = FPaths::MakeValidFileName(TestSuffix);
        TestSuffix.ReplaceInline(TEXT("-"), TEXT("_"));
        if (!TestSuffix.IsEmpty())
        {
            return BackupSaveSlotName + TEXT("_") + TestSuffix;
        }
    }
#endif
    return BackupSaveSlotName;
}

void UBHSaveSubsystem::Initialize(
    FSubsystemCollectionBase& Collection
)
{
    Super::Initialize(Collection);
    Collection.InitializeDependency<UBHWarSubsystem>();

    RuntimeConsumedWorldItemIDs.Reset();

    const UWorld* InitializationWorld = GetGameInstance()
        ? GetGameInstance()->GetWorld()
        : nullptr;
    const bool bKnownNetworkClient =
        IsClientCampaignWorld(InitializationWorld);

    bool bUsedBackup = false;
    const UBHSaveGame* ExistingSave = bKnownNetworkClient
        ? nullptr
        : LoadBestSaveGame(&bUsedBackup);

    // Game-instance subsystems can initialize before a game world has a
    // reliable net mode. Preserve the existing standalone/editor bootstrap
    // when authority is not knowable yet. A joining client must therefore
    // still receive and apply the server's replicated campaign snapshot once
    // its connection is established.
    if (bKnownNetworkClient)
    {
        UE_LOG(
            LogTemp,
            VeryVerbose,
            TEXT("BH_CAMPAIGN_BOOTSTRAP_SKIPPED_CLIENT")
        );
    }

    if (IsValid(ExistingSave))
    {
        RuntimeConsumedWorldItemIDs.Append(
            ExistingSave->ConsumedWorldItemIDs
        );

        if (!ExistingSave->WarSectorStates.IsEmpty())
        {
            if (UBHWarSubsystem* WarSubsystem =
                GetGameInstance()
                    ? GetGameInstance()->GetSubsystem<
                        UBHWarSubsystem>()
                    : nullptr)
            {
                const bool bWarRestored =
                    WarSubsystem->RestoreWarState(
                    ExistingSave->WarSectorStates,
                    ExistingSave->WarSupplyConvoys,
                    ExistingSave->WarEventHistory,
                    ExistingSave->WarTurnNumber,
                    ExistingSave->WarSimulationAccumulator,
                    ExistingSave->SchemaVersion
                );

                if (bWarRestored)
                {
                    WarSubsystem->RestoreGarrisonTransfers(
                        ExistingSave->WarGarrisonTransfers
                    );

                    if (ExistingSave->SchemaVersion >= 21)
                    {
                        WarSubsystem->RestoreManpowerState(
                            ExistingSave
                                ->WarFriendlyManpowerReserve,
                            ExistingSave
                                ->WarEnemyManpowerReserve,
                            ExistingSave
                                ->WarFriendlyRecruitmentProgress,
                            ExistingSave
                                ->WarEnemyRecruitmentProgress
                        );
                    }

                    if (ExistingSave->bRuntimeWarOperation)
                    {
                        WarSubsystem->RestoreCommittedOperation(
                            ExistingSave->AssignedWarSectorID,
                            ExistingSave->AssignedWarPriorityType,
                            ExistingSave->WarCommittedOperationID,
                            ExistingSave->SchemaVersion >= 39
                                ? ExistingSave
                                    ->WarCommittedOperationTargetID
                                : NAME_None
                        );
                    }
                }
            }
        }

        if (bUsedBackup)
        {
            UE_LOG(
                LogTemp,
                Warning,
                TEXT(
                    "BH_CHECKPOINT_RECOVERY initialized campaign "
                    "from backup slot %s."
                ),
                *GetActiveBackupSaveSlotName()
            );
        }
    }

    if (UBHWarSubsystem* WarSubsystem =
        GetGameInstance()
            ? GetGameInstance()->GetSubsystem<UBHWarSubsystem>()
            : nullptr)
    {
        WarSubsystem->OnWarStateChanged.AddDynamic(
            this,
            &UBHSaveSubsystem::HandleWarStateChanged
        );
    }

    PostLoadMapHandle =
        FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(
            this,
            &UBHSaveSubsystem::HandlePostLoadMap
        );
}

void UBHSaveSubsystem::Deinitialize()
{
    if (UBHWarSubsystem* WarSubsystem =
        GetGameInstance()
            ? GetGameInstance()->GetSubsystem<UBHWarSubsystem>()
            : nullptr)
    {
        WarSubsystem->OnWarStateChanged.RemoveDynamic(
            this,
            &UBHSaveSubsystem::HandleWarStateChanged
        );
    }

    if (UWorld* World = GetGameInstance()
            ? GetGameInstance()->GetWorld()
            : nullptr)
    {
        World->GetTimerManager().ClearTimer(
            WarAutosaveTimerHandle
        );
        World->GetTimerManager().ClearTimer(
            FieldAutosaveTimerHandle
        );
    }

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

    if (IsClientCampaignWorld(World))
    {
        UE_LOG(
            LogTemp,
            Warning,
            TEXT("BH_CAMPAIGN_SAVE_REJECTED_CLIENT")
        );
        return false;
    }

    ABHCharacter* Character = FindPlayerCharacter(World);
    return SaveProgressForCharacter(Character);
}

bool UBHSaveSubsystem::SaveProgressForCharacter(
    ABHCharacter* Character
)
{
    UWorld* World = GetGameInstance()
        ? GetGameInstance()->GetWorld()
        : nullptr;

    if (!IsValid(World) ||
        !IsValid(Character) ||
        Character->GetWorld() != World ||
        !Character->HasAuthority())
    {
        UE_LOG(
            LogTemp,
            Warning,
            TEXT(
                "BH_CAMPAIGN_SAVE_REJECTED_AUTHORITY "
                "world=%s character=%s"
            ),
            IsValid(World) ? *World->GetName() : TEXT("None"),
            IsValid(Character) ? *Character->GetName() : TEXT("None")
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
    SaveData->bMissionComplete =
        Character->IsMissionComplete();
    SaveData->bMissionFailed =
        Character->IsMissionFailed();
    SaveData->bRuntimeWarOperation =
        Character->IsRuntimeWarOperation();
    SaveData->bCampaignEpilogueAcknowledged =
        Character->IsCampaignEpilogueAcknowledged();
    SaveData->bOperationDebriefAcknowledged =
        Character->IsOperationDebriefAcknowledged();
    SaveData->RuntimeObjectives =
        Character->GetRuntimeObjectiveDefinitions();
    SaveData->AssignedWarSectorID =
        Character->GetAssignedWarSectorID();
    SaveData->AssignedWarSupplySourceSectorID =
        Character->GetAssignedWarSupplySourceSectorID();
    SaveData->AssignedWarPriorityType =
        Character->GetAssignedWarPriorityType();
    SaveData->OpenWorldOperationState =
        Character->GetOpenWorldOperationState();
    SaveData->LivingFieldSquadCount =
        Character->GetLivingFieldSquadCount();
    SaveData->FieldSquadMemberStates =
        Character->GetFieldSquadMemberStates();

    SaveData->ResistanceForce.Operators.Reset();
    SaveData->ResistanceForce.AvailableOperatorCount = 0;
    float TotalOperatorReadiness = 0.0f;
    for (const FBHFieldSquadMemberState& MemberState :
         SaveData->FieldSquadMemberStates)
    {
        FBHResistanceOperatorState& OperatorState =
            SaveData->ResistanceForce.Operators.AddDefaulted_GetRef();
        OperatorState.OperatorID = MemberState.MemberID;
        OperatorState.bInjured =
            MemberState.bIncapacitated ||
            MemberState.bRequiresMedicalEvacuation ||
            (MemberState.Health >= 0.0f && MemberState.Health < 100.0f);
        OperatorState.bAvailable = !OperatorState.bInjured;
        if (OperatorState.bAvailable)
        {
            ++SaveData->ResistanceForce.AvailableOperatorCount;
        }
        TotalOperatorReadiness +=
            OperatorState.bAvailable ?
                FMath::Clamp(MemberState.CombatReadiness, 0.0f, 1.0f) :
                0.0f;
    }

    SaveData->ResistanceForce.Readiness =
        SaveData->ResistanceForce.Operators.IsEmpty()
            ? 0.0f
            : TotalOperatorReadiness /
                static_cast<float>(SaveData->ResistanceForce.Operators.Num());
    SaveData->ResistanceForce.LastOperationID =
        SaveData->AssignedWarSectorID;
    SaveData->ResistanceForce.bLastOperationSucceeded =
        SaveData->bMissionComplete && !SaveData->bMissionFailed;
    SaveData->ResistanceForce.Deployments.Reset();
    if (!SaveData->AssignedWarSectorID.IsNone())
    {
        FBHResistanceDeploymentState& Deployment =
            SaveData->ResistanceForce.Deployments.AddDefaulted_GetRef();
        Deployment.DeploymentID = FName(
            *FString::Printf(
                TEXT("Deployment_%s"),
                *SaveData->AssignedWarSectorID.ToString()
            )
        );
        Deployment.OperationID = SaveData->AssignedWarSectorID;
        for (const FBHResistanceOperatorState& OperatorState :
             SaveData->ResistanceForce.Operators)
        {
            if (OperatorState.bAvailable)
            {
                Deployment.OperatorIDs.Add(OperatorState.OperatorID);
            }
        }
    }
    SaveData->bFieldSquadHolding =
        Character->IsFieldSquadHolding();
    SaveData->bFieldSquadHasCommandLocation =
        Character->HasFieldSquadCommandLocation();
    SaveData->FieldSquadCommandLocation =
        Character->GetFieldSquadCommandLocation();
    SaveData->FieldSquadCommandYaw =
        Character->GetFieldSquadCommandYaw();
    SaveData->bFieldSquadEmbarked =
        Character->IsFieldSquadEmbarked();
    SaveData->FieldSquadTransportPersistenceID =
        Character->GetFieldSquadTransportPersistenceID();
    SaveData->OwnedKeycardIDs =
        Character->GetOwnedKeycardIDs();
    SaveData->CollectedKeycardPersistenceIDs =
        Character->GetCollectedKeycardPersistenceIDs();
    SaveData->PlayerTransform = Character->GetActorTransform();
    CapturePlayerResourceState(SaveData, Character);
    CaptureWarState(SaveData, GetGameInstance());
    SaveData->ConsumedWorldItemIDs =
        RuntimeConsumedWorldItemIDs.Array();

    SaveData->MissionItemContainerStates.Reset();
    for (TActorIterator<ABHMissionItemContainer> ContainerIt(World);
        ContainerIt;
        ++ContainerIt)
    {
        const ABHMissionItemContainer* Container = *ContainerIt;
        if (!IsValid(Container) ||
            Container->GetPersistenceID().IsNone())
        {
            continue;
        }

        FBHMissionItemContainerSaveState& ContainerState =
            SaveData->MissionItemContainerStates.AddDefaulted_GetRef();
        ContainerState.PersistenceID = Container->GetPersistenceID();
        ContainerState.MissionItemID = Container->GetMissionItemID();
        ContainerState.StoredMissionItemID =
            Container->GetStoredMissionItemID();
    }

    SaveData->DefeatedEnemyStates.Reset();
    for (const TPair<FName, FBHPendingDefeatedEnemyState>& Pair :
        RuntimeDefeatedEnemyStates)
    {
        FBHPersistentEnemyDeathSaveState DeathState;
        DeathState.FieldOperativeID = Pair.Key;
        DeathState.SectorID = Pair.Value.SectorID;
        DeathState.Transform = Pair.Value.Transform;
        SaveData->DefeatedEnemyStates.Add(DeathState);
    }    for (TActorIterator<ABHEnemySoldier> EnemyIt(World);
        EnemyIt;
        ++EnemyIt)
    {
        ABHEnemySoldier* Enemy = *EnemyIt;

        if (!IsValid(Enemy) ||
            Enemy->GetCombatFaction() != EBHCombatFaction::Hostile ||
            Enemy->GetFieldOperativeID().IsNone())
        {
            continue;
        }

        FBHPersistentEnemyCombatSaveState CombatState;
        CombatState.FieldOperativeID = Enemy->GetFieldOperativeID();
        CombatState.SectorID = Enemy->GetSurrenderSectorID();
        CombatState.Transform = Enemy->GetActorTransform();
        const UBHHealthComponent* HealthComponent =
            Enemy->GetHealthComponent();
        CombatState.Health = IsValid(HealthComponent)
            ? HealthComponent->GetCurrentHealth()
            : 0.0f;
        CombatState.MagazineAmmo = Enemy->GetCurrentMagazineAmmo();
        CombatState.ReserveAmmo = Enemy->GetCurrentReserveAmmo();
        CombatState.FragGrenades = Enemy->GetCurrentFragGrenades();
        CombatState.CombatReadiness = Enemy->GetCombatReadiness();
        CombatState.bSurrendered = Enemy->IsSurrendered();
        CombatState.bCustodySecured = Enemy->IsSurrenderSecured();
        CombatState.SurrenderEscapeSecondsRemaining =
            Enemy->GetSurrenderEscapeSecondsRemaining();
        SaveData->EnemyCombatStates.Add(CombatState);
    }

    SaveData->EnemyCombatStates.Sort(
        [](const FBHPersistentEnemyCombatSaveState& A,
            const FBHPersistentEnemyCombatSaveState& B)
        {
            return A.FieldOperativeID.ToString() <
                B.FieldOperativeID.ToString();
        }
    );

    for (TActorIterator<ABHFieldTransport> TransportIt(World);
        TransportIt;
        ++TransportIt)
    {
        const ABHFieldTransport* Transport = *TransportIt;

        if (!IsValid(Transport))
        {
            continue;
        }

        FBHFieldTransportSaveState TransportState;
        TransportState.PersistenceID =
            Transport->GetPersistenceID();
        TransportState.Transform =
            Transport->GetActorTransform();
        TransportState.bPlayerWasDriving =
            Transport->GetOccupant() == Character;
        TransportState.FuelFraction =
            Transport->GetFuelPercentage();
        TransportState.HullFraction =
            Transport->GetHullPercentage();
        TransportState.CargoSupply =
            Transport->GetCargoSupply();
        TransportState.CargoSourceSectorID =
            Transport->GetCargoSourceSectorID();
        TransportState.CargoDestinationSectorID =
            Transport->GetCargoDestinationSectorID();
        TransportState.CargoType =
            Transport->GetCargoType();
        SaveData->FieldTransportStates.Add(TransportState);
    }

    SaveData->ResistanceForce.Vehicles.Reset();
    SaveData->ResistanceForce.OperationalVehicleCount = 0;
    for (const FBHFieldTransportSaveState& TransportState :
         SaveData->FieldTransportStates)
    {
        if (TransportState.PersistenceID.IsNone())
        {
            continue;
        }

        FBHResistanceVehicleState& VehicleState =
            SaveData->ResistanceForce.Vehicles.AddDefaulted_GetRef();
        VehicleState.VehicleID = TransportState.PersistenceID;
        VehicleState.VehicleType = TEXT("FieldTransport");
        VehicleState.SectorID = !TransportState.CargoDestinationSectorID.IsNone()
            ? TransportState.CargoDestinationSectorID
            : TransportState.CargoSourceSectorID;
        VehicleState.Condition = FMath::Clamp(
            FMath::Min(
                TransportState.FuelFraction,
                TransportState.HullFraction
            ),
            0.0f,
            1.0f
        );
        VehicleState.bReady =
            TransportState.FuelFraction > KINDA_SMALL_NUMBER &&
            TransportState.HullFraction > KINDA_SMALL_NUMBER;
        if (VehicleState.bReady)
        {
            ++SaveData->ResistanceForce.OperationalVehicleCount;
        }
    }

    for (TActorIterator<ABHFieldFortification> FortificationIt(World);
        FortificationIt;
        ++FortificationIt)
    {
        const ABHFieldFortification* Fortification = *FortificationIt;
        if (!IsValid(Fortification) ||
            Fortification->GetPersistenceID().IsNone())
        {
            continue;
        }

        FBHFieldFortificationSaveState FortificationState;
        FortificationState.PersistenceID =
            Fortification->GetPersistenceID();
        FortificationState.SectorID = Fortification->GetSectorID();
        FortificationState.Transform = Fortification->GetActorTransform();
        FortificationState.bConstructed = Fortification->IsConstructed();
        FortificationState.HealthFraction =
            Fortification->GetHealthFraction();
        FortificationState.Plan = Fortification->GetSelectedPlan();
        FortificationState.WorkProgress = Fortification->GetConstructionProgress();
        FortificationState.bDismantleWork =
            Fortification->IsDismantling();
        FortificationState.ActiveWorkerCount =
            Fortification->GetActiveWorkerCount();
        FortificationState.SupplyCacheCharges =
            Fortification->GetSupplyCacheChargesRemaining();
        FortificationState.LastObservationTurn =
            Fortification->GetLastObservationTurn();
        FortificationState.RallyDeploymentsRemaining =
            Fortification->GetRallyDeploymentsRemaining();
        FortificationState.ObservationProgress =
            Fortification->GetObservationProgress();
        SaveData->FieldFortificationStates.Add(FortificationState);
    }

    SaveData->ResistanceForce.Facilities.Reset();
    for (const FBHFieldFortificationSaveState& FortificationState :
         SaveData->FieldFortificationStates)
    {
        if (FortificationState.PersistenceID.IsNone())
        {
            continue;
        }

        FBHResistanceFacilityState& FacilityState =
            SaveData->ResistanceForce.Facilities.AddDefaulted_GetRef();
        FacilityState.FacilityID = FortificationState.PersistenceID;
        FacilityState.FacilityType = TEXT("Fortification");
        FacilityState.SectorID = FortificationState.SectorID;
        FacilityState.SupplyCacheCharges =
            FMath::Max(0, FortificationState.SupplyCacheCharges);
        FacilityState.bOperational =
            FortificationState.bConstructed &&
            FortificationState.HealthFraction > KINDA_SMALL_NUMBER &&
            !FortificationState.bDismantleWork;
    }

    SaveData->ResistanceForce.AmmunitionSupply = 0.0f;
    for (const FBHFieldSquadMemberState& MemberState :
         SaveData->FieldSquadMemberStates)
    {
        SaveData->ResistanceForce.AmmunitionSupply +=
            FMath::Max(0, MemberState.MagazineAmmo) +
            FMath::Max(0, MemberState.ReserveAmmo);
    }

    SaveData->ResistanceForce.FuelSupply = 0.0f;
    for (const FBHFieldTransportSaveState& TransportState :
         SaveData->FieldTransportStates)
    {
        SaveData->ResistanceForce.FuelSupply +=
            FMath::Clamp(TransportState.FuelFraction, 0.0f, 1.0f);
    }

    SaveData->ResistanceForce.MedicalSupply =
        FMath::Max(0, SaveData->SavedMedkitCount) +
        FMath::Max(0, SaveData->SavedFieldDressingCount);

    const float OperatorReadiness = SaveData->ResistanceForce.Operators.IsEmpty()
        ? 0.0f
        : FMath::Clamp(
            SaveData->ResistanceForce.Readiness,
            0.0f,
            1.0f
        );
    const float VehicleReadiness = SaveData->ResistanceForce.Vehicles.IsEmpty()
        ? 0.0f
        : static_cast<float>(SaveData->ResistanceForce.OperationalVehicleCount) /
            static_cast<float>(SaveData->ResistanceForce.Vehicles.Num());
    int32 OperationalFacilityCount = 0;
    for (const FBHResistanceFacilityState& FacilityState :
         SaveData->ResistanceForce.Facilities)
    {
        if (FacilityState.bOperational)
        {
            ++OperationalFacilityCount;
        }
    }
    const float FacilityReadiness = SaveData->ResistanceForce.Facilities.IsEmpty()
        ? 0.0f
        : static_cast<float>(OperationalFacilityCount) /
            static_cast<float>(SaveData->ResistanceForce.Facilities.Num());
    SaveData->ResistanceForce.Readiness =
        OperatorReadiness *
        (SaveData->ResistanceForce.Vehicles.IsEmpty() ? 1.0f : VehicleReadiness) *
        (SaveData->ResistanceForce.Facilities.IsEmpty() ? 1.0f : FacilityReadiness);

    for (TActorIterator<ABHSupplyConvoyTarget> ConvoyIt(World);
        ConvoyIt;
        ++ConvoyIt)
    {
        const ABHSupplyConvoyTarget* Convoy = *ConvoyIt;

        if (!IsValid(Convoy) ||
            !Convoy->HasRecoverableSalvage())
        {
            continue;
        }

        FBHConvoySalvageSaveState SalvageState;
        SalvageState.ConvoyID = Convoy->GetConvoyID();
        SalvageState.SourceSectorID =
            Convoy->GetSourceSectorID();
        SalvageState.DestinationSectorID =
            Convoy->GetDestinationSectorID();
        SalvageState.Transform = Convoy->GetActorTransform();
        SalvageState.OriginalSupplyPayload =
            Convoy->GetSupplyPayload();
        SalvageState.RecoverableSupply =
            Convoy->GetRecoverableSupply();
        SalvageState.LifetimeRemaining =
            Convoy->GetSalvageLifetimeRemaining();

        for (TActorIterator<ABHAmbientWarDirector> DirectorIt(
                World);
            DirectorIt;
            ++DirectorIt)
        {
            const ABHAmbientWarDirector* Director = *DirectorIt;

            if (IsValid(Director))
            {
                SalvageState.SurvivingSecurityCount =
                    Director->GetSurvivingConvoySecurityCount(
                        Convoy
                    );
                break;
            }
        }

        SaveData->ConvoySalvageStates.Add(SalvageState);
    }

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
    SaveData->ConsumedWorldItemIDs.Sort(FNameLexicalLess());
    SaveData->FieldTransportStates.Sort(
        [](const FBHFieldTransportSaveState& Left,
           const FBHFieldTransportSaveState& Right)
        {
            return FNameLexicalLess()(
                Left.PersistenceID,
                Right.PersistenceID
            );
        }
    );
    SaveData->FieldFortificationStates.Sort(
        [](const FBHFieldFortificationSaveState& Left,
           const FBHFieldFortificationSaveState& Right)
        {
            return FNameLexicalLess()(
                Left.PersistenceID,
                Right.PersistenceID
            );
        }
    );
    SaveData->ConvoySalvageStates.Sort(
        [](const FBHConvoySalvageSaveState& Left,
           const FBHConvoySalvageSaveState& Right)
        {
            return FNameLexicalLess()(
                Left.ConvoyID,
                Right.ConvoyID
            );
        }
    );
    SaveData->SurrenderedEnemyStates.Sort(
        [](const FBHSurrenderedEnemySaveState& Left,
           const FBHSurrenderedEnemySaveState& Right)
        {
            return FNameLexicalLess()(
                Left.FieldOperativeID,
                Right.FieldOperativeID
            );
        }
    );

    SaveData->DefeatedEnemyStates.Sort(
        [](const FBHPersistentEnemyDeathSaveState& Left,
            const FBHPersistentEnemyDeathSaveState& Right)
        {
            return FNameLexicalLess()(
                Left.FieldOperativeID,
                Right.FieldOperativeID
            );
        }
    );    const bool bSaved = SavePrimaryWithBackup(SaveData);

    if (bSaved)
    {
        PendingSurrenderEnemyStates.Reset();
        PendingDefeatedEnemyStates.Reset();
        PendingSurrenderLevelName = NAME_None;
        ClearPendingWarAutosave(World);

        UE_LOG(
            LogTemp,
            Log,
            TEXT("Checkpoint save succeeded in slot %s."),
            *GetActiveSaveSlotName()
        );
    }
    else
    {
        UE_LOG(
            LogTemp,
            Error,
            TEXT("Checkpoint save failed in slot %s."),
            *GetActiveSaveSlotName()
        );
    }

    return bSaved;
}

bool UBHSaveSubsystem::SavePlayerResources()
{
    UWorld* World = GetGameInstance()
        ? GetGameInstance()->GetWorld()
        : nullptr;

    if (IsClientCampaignWorld(World))
    {
        UE_LOG(
            LogTemp,
            Warning,
            TEXT("BH_PLAYER_RESOURCE_SAVE_REJECTED_CLIENT")
        );
        return false;
    }

    if (!HasValidSaveGame())
    {
        return SaveProgress();
    }

    ABHCharacter* Character = FindPlayerCharacter(World);
    UBHSaveGame* SaveData = LoadBestSaveGame();

    if (!IsValid(World) ||
        !IsValid(Character) ||
        !IsValid(SaveData) ||
        SaveData->SchemaVersion <
            BHSave::MinimumCompatibleSchemaVersion ||
        SaveData->SchemaVersion > BHSave::CurrentSchemaVersion)
    {
        UE_LOG(
            LogTemp,
            Warning,
            TEXT("Player resource save failed validation.")
        );
        return false;
    }

    SaveData->SchemaVersion = BHSave::CurrentSchemaVersion;
    CapturePlayerResourceState(SaveData, Character);
    CaptureWarState(SaveData, GetGameInstance());

    SaveData->DefeatedEnemyStates.Sort(
        [](const FBHPersistentEnemyDeathSaveState& Left,
            const FBHPersistentEnemyDeathSaveState& Right)
        {
            return FNameLexicalLess()(
                Left.FieldOperativeID,
                Right.FieldOperativeID
            );
        }
    );    const bool bSaved = SavePrimaryWithBackup(SaveData);

    if (bSaved)
    {
        ClearPendingWarAutosave(World);
        UE_LOG(
            LogTemp,
            VeryVerbose,
            TEXT("Player resources saved without moving checkpoint.")
        );
    }

    return bSaved;
}

bool UBHSaveSubsystem::LoadProgress()
{
    UWorld* World = GetGameInstance()
        ? GetGameInstance()->GetWorld()
        : nullptr;

    if (IsClientCampaignWorld(World))
    {
        UE_LOG(
            LogTemp,
            Warning,
            TEXT("BH_CAMPAIGN_LOAD_REJECTED_CLIENT")
        );
        return false;
    }

#if !UE_BUILD_SHIPPING
    if (FParse::Param(
            FCommandLine::Get(),
            TEXT("BHTestRestoreCrashRecovery")))
    {
        bCrashRecoveryLoadStarted = true;
    }
#endif

    if (!HasSaveGame())
    {
        UE_LOG(
            LogTemp,
            Warning,
            TEXT("No checkpoint save exists in slot %s."),
            *GetActiveSaveSlotName()
        );
        return false;
    }

    bool bUsedBackup = false;
    UBHSaveGame* SaveData =
        LoadBestSaveGame(&bUsedBackup);

    if (!IsValid(SaveData))
    {
        UE_LOG(
            LogTemp,
            Error,
            TEXT("No compatible primary or backup checkpoint exists.")
        );
        return false;
    }

    if (!IsValid(World))
    {
        return false;
    }

    const FName LevelToLoad =
        SaveData->SavedLevelName.IsNone()
        ? FName(*UGameplayStatics::GetCurrentLevelName(World, true))
        : SaveData->SavedLevelName;

    PendingSaveData = SaveData;
    PendingLoadedSchemaVersion = SaveData->SchemaVersion;
    bPendingLoadedFromBackup = bUsedBackup;

    if (World->GetNetMode() == NM_ListenServer ||
        World->GetNetMode() == NM_DedicatedServer)
    {
        ABHCharacter* TravelingCharacter =
            FindPlayerCharacter(World);

        if (IsValid(TravelingCharacter))
        {
            for (TActorIterator<ABHFieldTransport> TransportIt(World);
                 TransportIt;
                 ++TransportIt)
            {
                ABHFieldTransport* Transport = *TransportIt;
                if (IsValid(Transport) &&
                    Transport->GetOccupant() == TravelingCharacter)
                {
                    Transport->PrepareOccupantForServerTravel(
                        TravelingCharacter
                    );
                    break;
                }
            }
        }
    }

    if (bUsedBackup)
    {
        UE_LOG(
            LogTemp,
            Warning,
            TEXT(
                "BH_CHECKPOINT_RECOVERY loading backup slot %s."
            ),
            *GetActiveBackupSaveSlotName()
        );
    }

    if (World->GetNetMode() == NM_ListenServer ||
        World->GetNetMode() == NM_DedicatedServer)
    {
        FString TravelURL = LevelToLoad.ToString();
        const UBHGameShellSettings* ShellSettings =
            GetDefault<UBHGameShellSettings>();

        if (IsValid(ShellSettings) &&
            !ShellSettings->GameplayMap.IsNull())
        {
            const FString GameplayPackageName =
                ShellSettings->GameplayMap
                    .ToSoftObjectPath()
                    .GetLongPackageName();

            if (!GameplayPackageName.IsEmpty() &&
                FPackageName::GetShortName(GameplayPackageName) ==
                    LevelToLoad.ToString())
            {
                TravelURL = GameplayPackageName;
            }
        }

        if (!World->ServerTravel(TravelURL, true))
        {
            PendingSaveData = nullptr;
            PendingLoadedSchemaVersion = 0;
            bPendingLoadedFromBackup = false;
            UE_LOG(
                LogTemp,
                Error,
                TEXT(
                    "BH_CAMPAIGN_SERVER_TRAVEL_FAILED level=%s"
                ),
                *TravelURL
            );
            return false;
        }

        UE_LOG(
            LogTemp,
            Display,
            TEXT("BH_CAMPAIGN_SERVER_TRAVEL level=%s"),
            *TravelURL
        );
        return true;
    }

    UGameplayStatics::OpenLevel(World, LevelToLoad);
    return true;
}

bool UBHSaveSubsystem::ReloadCheckpointAfterPlayerDeath(
    FName CasualtySectorID
)
{
    const UWorld* World = GetGameInstance()
        ? GetGameInstance()->GetWorld()
        : nullptr;

    if (IsClientCampaignWorld(World))
    {
        UE_LOG(
            LogTemp,
            Warning,
            TEXT("BH_CHECKPOINT_RELOAD_REJECTED_CLIENT")
        );
        return false;
    }

    PendingPlayerDeathAttritionSectorID = CasualtySectorID;

    if (LoadProgress())
    {
        return true;
    }

    PendingPlayerDeathAttritionSectorID = NAME_None;
    return false;
}

bool UBHSaveSubsystem::DeployNextOperation()
{
    UBHWarSubsystem* WarSubsystem = GetGameInstance()
        ? GetGameInstance()->GetSubsystem<UBHWarSubsystem>()
        : nullptr;

    return IsValid(WarSubsystem) &&
        DeployOperation(
            WarSubsystem->GetPrioritySectorID(),
            WarSubsystem->GetPriorityType()
        );
}

bool UBHSaveSubsystem::DeployOperation(
    FName SectorID,
    EBHWarPriorityType OperationType
)
{
    ABHCharacter* Character = FindPlayerCharacter(
        GetGameInstance()
            ? GetGameInstance()->GetWorld()
            : nullptr
    );

    return DeployOperationForCharacter(
        Character,
        SectorID,
        OperationType
    );
}

bool UBHSaveSubsystem::DeployOperationForCharacter(
    ABHCharacter* RequestingCharacter,
    FName SectorID,
    EBHWarPriorityType OperationType
)
{
    UWorld* World = GetGameInstance()
        ? GetGameInstance()->GetWorld()
        : nullptr;

    if (IsClientCampaignWorld(World) ||
        !IsValid(RequestingCharacter) ||
        RequestingCharacter->GetWorld() != World ||
        !RequestingCharacter->HasAuthority())
    {
        UE_LOG(
            LogTemp,
            Warning,
            TEXT(
                "BH_DEPLOYMENT_REJECTED_AUTHORITY sector=%s "
                "type=%d"
            ),
            *SectorID.ToString(),
            static_cast<int32>(OperationType)
        );
        return false;
    }

    if (!SaveProgressForCharacter(RequestingCharacter))
    {
        RequestingCharacter->ShowStatusNotification(
            NSLOCTEXT(
                "BrokenHorizon",
                "DeploymentPreCheckpointFailed",
                "DEPLOYMENT BLOCKED\n\n"
                "The current campaign could not be checkpointed. "
                "No operation resources were committed."
            )
        );
        UE_LOG(
            LogTemp,
            Error,
            TEXT(
                "BH_DEPLOYMENT_CHECKPOINT_BLOCKED sector=%s "
                "type=%d"
            ),
            *SectorID.ToString(),
            static_cast<int32>(OperationType)
        );
        return false;
    }

    if (!RequestingCharacter->BeginOperationInWorld(
            SectorID,
            OperationType))
    {
        UE_LOG(
            LogTemp,
            Error,
            TEXT("The next open-world operation could not be activated.")
        );
        return false;
    }

    const bool bDeploymentCheckpointSaved =
        SaveProgressForCharacter(RequestingCharacter);

    if (!bDeploymentCheckpointSaved)
    {
        RequestingCharacter->ShowStatusNotification(
            NSLOCTEXT(
                "BrokenHorizon",
                "DeploymentPostCheckpointRetry",
                "OPERATION ACTIVE\n\n"
                "The deployment checkpoint is retrying. "
                "The previous campaign checkpoint remains safe."
            )
        );
        UE_LOG(
            LogTemp,
            Warning,
            TEXT(
                "BH_DEPLOYMENT_CHECKPOINT_RETRY_PENDING "
                "sector=%s type=%d"
            ),
            *SectorID.ToString(),
            static_cast<int32>(OperationType)
        );
        return true;
    }

    UE_LOG(
        LogTemp,
        Display,
        TEXT(
            "BH_DEPLOYMENT_CHECKPOINT_COMMITTED sector=%s "
            "type=%d"
        ),
        *SectorID.ToString(),
        static_cast<int32>(OperationType)
    );
    return true;
}

bool UBHSaveSubsystem::HasSaveGame() const
{
    const FString ActiveSaveSlot = GetActiveSaveSlotName();
    const FString ActiveBackupSlot = GetActiveBackupSaveSlotName();
    return UGameplayStatics::DoesSaveGameExist(
            ActiveSaveSlot,
            SaveUserIndex
        ) ||
        UGameplayStatics::DoesSaveGameExist(
            ActiveBackupSlot,
            SaveUserIndex
        );
}

bool UBHSaveSubsystem::HasValidSaveGame() const
{
    return IsValid(LoadBestSaveGame());
}

bool UBHSaveSubsystem::DeleteSaveGame()
{
    UWorld* World = GetGameInstance()
        ? GetGameInstance()->GetWorld()
        : nullptr;

    if (IsClientCampaignWorld(World))
    {
        UE_LOG(
            LogTemp,
            Warning,
            TEXT("BH_CAMPAIGN_RESET_REJECTED_CLIENT")
        );
        return false;
    }

    bool bDeleteSucceeded = true;

    const FString ActiveSaveSlot = GetActiveSaveSlotName();
    const FString ActiveBackupSlot = GetActiveBackupSaveSlotName();
    for (const FString* SlotName :
        {&ActiveSaveSlot, &ActiveBackupSlot})
    {
        if (UGameplayStatics::DoesSaveGameExist(
                *SlotName,
                SaveUserIndex
            ) &&
            !UGameplayStatics::DeleteGameInSlot(
                *SlotName,
                SaveUserIndex
            ))
        {
            UE_LOG(
                LogTemp,
                Error,
                TEXT("Failed to delete save slot %s."),
                **SlotName
            );
            bDeleteSucceeded = false;
        }
    }

    if (!bDeleteSucceeded)
    {
        return false;
    }

    ClearPendingWarAutosave(World);

    PendingSaveData = nullptr;
    PendingLoadedSchemaVersion = 0;
    bPendingLoadedFromBackup = false;
    PendingPlayerDeathAttritionSectorID = NAME_None;
    RuntimeConsumedWorldItemIDs.Reset();
    PendingSurrenderEnemyStates.Reset();
    PendingSurrenderLevelName = NAME_None;

    RuntimeDefeatedEnemyStates.Reset();
    PendingDefeatedEnemyStates.Reset();
    if (UBHWarSubsystem* WarSubsystem =
        GetGameInstance()
            ? GetGameInstance()->GetSubsystem<UBHWarSubsystem>()
            : nullptr)
    {
        WarSubsystem->ResetCampaign();
    }

    UE_LOG(
        LogTemp,
        Log,
        TEXT(
            "BH_CAMPAIGN_RESET slots=%s,%s "
            "pending_load=cleared pending_autosave=cleared"
        ),
        *ActiveSaveSlot,
        *ActiveBackupSlot
    );
    return true;
}

void UBHSaveSubsystem::RecordDefeatedEnemy(ABHEnemySoldier* Enemy)
{
    if (!IsValid(Enemy) ||
        !Enemy->HasAuthority() ||
        Enemy->GetCombatFaction() != EBHCombatFaction::Hostile ||
        Enemy->GetFieldOperativeID().IsNone())
    {
        return;
    }

    FBHPendingDefeatedEnemyState DeathState;
    DeathState.SectorID = Enemy->GetSurrenderSectorID();
    DeathState.Transform = Enemy->GetActorTransform();
    RuntimeDefeatedEnemyStates.Add(
        Enemy->GetFieldOperativeID(),
        DeathState
    );

    UE_LOG(
        LogTemp,
        Display,
        TEXT("BH_ENEMY_DEFEAT_RECORDED id=%s sector=%s"),
        *Enemy->GetFieldOperativeID().ToString(),
        *DeathState.SectorID.ToString()
    );
}bool UBHSaveSubsystem::RecordConsumedWorldItem(
    FName PersistenceID
)
{
    UWorld* World = GetGameInstance()
        ? GetGameInstance()->GetWorld()
        : nullptr;

    if (IsClientCampaignWorld(World))
    {
        UE_LOG(
            LogTemp,
            Warning,
            TEXT(
                "BH_CONSUMED_ITEM_SAVE_REJECTED_CLIENT item=%s"
            ),
            *PersistenceID.ToString()
        );
        return false;
    }

    if (PersistenceID.IsNone())
    {
        return false;
    }

    if (RuntimeConsumedWorldItemIDs.Contains(PersistenceID))
    {
        UE_LOG(
            LogTemp,
            Display,
            TEXT("BH_CONSUMED_ITEM_ALREADY_RECORDED item=%s"),
            *PersistenceID.ToString()
        );
        return true;
    }

    RuntimeConsumedWorldItemIDs.Add(PersistenceID);

    if (!HasValidSaveGame())
    {
        if (SaveProgress())
        {
            UE_LOG(
                LogTemp,
                Display,
                TEXT(
                    "BH_INITIAL_ITEM_CHECKPOINT item=%s"
                ),
                *PersistenceID.ToString()
            );
            return true;
        }

        RuntimeConsumedWorldItemIDs.Remove(PersistenceID);
        return false;
    }

    UBHSaveGame* SaveData = LoadBestSaveGame();

    if (!IsValid(SaveData))
    {
        RuntimeConsumedWorldItemIDs.Remove(PersistenceID);
        return false;
    }

    SaveData->SchemaVersion = BHSave::CurrentSchemaVersion;
    SaveData->ConsumedWorldItemIDs.AddUnique(PersistenceID);
    SaveData->ConsumedWorldItemIDs.Sort(FNameLexicalLess());
    CapturePlayerResourceState(
        SaveData,
        FindPlayerCharacter(World)
    );
    CaptureWarState(SaveData, GetGameInstance());

    if (!SavePrimaryWithBackup(SaveData))
    {
        RuntimeConsumedWorldItemIDs.Remove(PersistenceID);
        return false;
    }

    ClearPendingWarAutosave(World);
    UE_LOG(
        LogTemp,
        Display,
        TEXT("BH_CONSUMED_ITEM_SAVED item=%s schema=%d"),
        *PersistenceID.ToString(),
        SaveData->SchemaVersion
    );
    return true;
}

bool UBHSaveSubsystem::IsWorldItemConsumed(
    FName PersistenceID
) const
{
    const bool bConsumed = !PersistenceID.IsNone() &&
        RuntimeConsumedWorldItemIDs.Contains(PersistenceID);
    if (bConsumed)
    {
        UE_LOG(
            LogTemp,
            Verbose,
            TEXT("BH_CONSUMED_ITEM_RESTORED item=%s"),
            *PersistenceID.ToString()
        );
    }
    return bConsumed;
}

ABHCharacter* UBHSaveSubsystem::FindPlayerCharacter(
    UWorld* World
) const
{
    if (ABHCharacter* LocalCharacter = BHPlayerResolver::Find(World))
    {
        return LocalCharacter;
    }

    if (!IsValid(World) || World->GetNetMode() == NM_Client)
    {
        return nullptr;
    }

    for (FConstPlayerControllerIterator ControllerIt =
             World->GetPlayerControllerIterator();
         ControllerIt;
         ++ControllerIt)
    {
        const APlayerController* PlayerController = ControllerIt->Get();
        APawn* PlayerPawn = IsValid(PlayerController)
            ? PlayerController->GetPawn()
            : nullptr;

        if (ABHCharacter* Character = Cast<ABHCharacter>(PlayerPawn))
        {
            return Character;
        }

        if (const ABHFieldTransport* Transport =
                Cast<ABHFieldTransport>(PlayerPawn))
        {
            if (ABHCharacter* Occupant = Transport->GetOccupant())
            {
                return Occupant;
            }
        }
    }

    return nullptr;
}

bool UBHSaveSubsystem::IsClientCampaignWorld(
    const UWorld* World
) const
{
    return IsValid(World) &&
        World->GetNetMode() == NM_Client;
}

UBHSaveGame* UBHSaveSubsystem::LoadBestSaveGame(
    bool* bOutUsedBackup
) const
{
    if (bOutUsedBackup)
    {
        *bOutUsedBackup = false;
    }

    const FString ActiveSaveSlot = GetActiveSaveSlotName();
    const FString ActiveBackupSlot = GetActiveBackupSaveSlotName();
    const FString* SlotNames[] =
    {
        &ActiveSaveSlot,
        &ActiveBackupSlot
    };

    for (int32 SlotIndex = 0;
        SlotIndex < UE_ARRAY_COUNT(SlotNames);
        ++SlotIndex)
    {
        const FString& SlotName = *SlotNames[SlotIndex];

        if (!UGameplayStatics::DoesSaveGameExist(
                SlotName,
                SaveUserIndex
            ))
        {
            continue;
        }

        UBHSaveGame* SaveData = LoadProtectedCampaignSave(
            SlotName,
            SaveUserIndex
        );

        if (!IsCompatibleCampaignSave(SaveData))
        {
            UE_LOG(
                LogTemp,
                Warning,
                TEXT(
                    "Checkpoint slot %s is missing, corrupt, "
                    "or incompatible."
                ),
                *SlotName
            );
            continue;
        }

        if (bOutUsedBackup)
        {
            *bOutUsedBackup = SlotIndex > 0;
        }
        return SaveData;
    }

    return nullptr;
}

bool UBHSaveSubsystem::SavePrimaryWithBackup(
    UBHSaveGame* SaveData
) const
{
    if (!IsCompatibleCampaignSave(SaveData))
    {
        return false;
    }

    const FString ActiveSaveSlot = GetActiveSaveSlotName();
    const FString ActiveBackupSlot = GetActiveBackupSaveSlotName();

    if (UGameplayStatics::DoesSaveGameExist(
        ActiveSaveSlot,
        SaveUserIndex
    ))
    {
        UBHSaveGame* ExistingPrimary = LoadProtectedCampaignSave(
            ActiveSaveSlot,
            SaveUserIndex
        );

        if (IsCompatibleCampaignSave(ExistingPrimary) &&
            !SaveProtectedCampaignSave(
                ExistingPrimary,
                ActiveBackupSlot,
                SaveUserIndex
            ))
        {
            UE_LOG(
                LogTemp,
                Error,
                TEXT(
                    "Checkpoint backup failed; primary slot "
                    "was left unchanged."
                )
            );
            return false;
        }

        if (IsCompatibleCampaignSave(ExistingPrimary))
        {
            UE_LOG(
                LogTemp,
                VeryVerbose,
                TEXT(
                    "BH_CHECKPOINT_BACKUP slot=%s source=%s"
                ),
                *ActiveBackupSlot,
                *ActiveSaveSlot
            );
        }
    }

    return SaveProtectedCampaignSave(
        SaveData,
        ActiveSaveSlot,
        SaveUserIndex
    );
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
    TMap<FName, const AActor*> SupplyIDs;
    TMap<FName, const AActor*> TransportIDs;
    TMap<FName, const AActor*> MissionItemContainerIDs;

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

    for (TActorIterator<ABHSupplyBase> SupplyIt(World);
        SupplyIt;
        ++SupplyIt)
    {
        const ABHSupplyBase* Supply = *SupplyIt;
        const FName PersistenceID = Supply->GetPersistenceID();

        if (Supply->IsRuntimeSupply())
        {
            continue;
        }

        if (PersistenceID.IsNone())
        {
            UE_LOG(
                LogTemp,
                Warning,
                TEXT("Supply %s has no persistence ID."),
                *Supply->GetPathName()
            );
            continue;
        }

        if (const AActor* const* Existing =
            SupplyIDs.Find(PersistenceID))
        {
            UE_LOG(
                LogTemp,
                Error,
                TEXT(
                    "Duplicate supply persistence ID %s on %s "
                    "and %s."
                ),
                *PersistenceID.ToString(),
                *(*Existing)->GetPathName(),
                *Supply->GetPathName()
            );
            bValid = false;
            continue;
        }

        SupplyIDs.Add(PersistenceID, Supply);
    }

    for (TActorIterator<ABHFieldTransport> TransportIt(World);
        TransportIt;
        ++TransportIt)
    {
        const ABHFieldTransport* Transport = *TransportIt;
        const FName PersistenceID =
            Transport->GetPersistenceID();

        if (PersistenceID.IsNone())
        {
            UE_LOG(
                LogTemp,
                Error,
                TEXT("Field transport %s has no persistence ID."),
                *Transport->GetPathName()
            );
            bValid = false;
            continue;
        }

        if (const AActor* const* Existing =
            TransportIDs.Find(PersistenceID))
        {
            UE_LOG(
                LogTemp,
                Error,
                TEXT(
                    "Duplicate field transport persistence ID %s "
                    "on %s and %s."
                ),
                *PersistenceID.ToString(),
                *(*Existing)->GetPathName(),
                *Transport->GetPathName()
            );
            bValid = false;
            continue;
        }

        TransportIDs.Add(PersistenceID, Transport);
    }

    for (TActorIterator<ABHMissionItemContainer> ContainerIt(World);
        ContainerIt;
        ++ContainerIt)
    {
        const ABHMissionItemContainer* Container = *ContainerIt;
        const FName PersistenceID = Container->GetPersistenceID();

        if (PersistenceID.IsNone())
        {
            UE_LOG(
                LogTemp,
                Error,
                TEXT("Mission item container %s has no persistence ID."),
                *Container->GetPathName()
            );
            bValid = false;
            continue;
        }

        if (Container->GetMissionItemID().IsNone())
        {
            UE_LOG(
                LogTemp,
                Error,
                TEXT("Mission item container %s has no mission item ID."),
                *Container->GetPathName()
            );
            bValid = false;
            continue;
        }

        if (const AActor* const* Existing =
                MissionItemContainerIDs.Find(PersistenceID))
        {
            UE_LOG(
                LogTemp,
                Error,
                TEXT(
                    "Duplicate mission item container persistence ID %s "
                    "on %s and %s."
                ),
                *PersistenceID.ToString(),
                *(*Existing)->GetPathName(),
                *Container->GetPathName()
            );
            bValid = false;
            continue;
        }

        MissionItemContainerIDs.Add(PersistenceID, Container);
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

    TGuardValue<bool> SuppressAutosaveGuard(
        bSuppressWarAutosave,
        true
    );

    const bool bSurrenderLevelMatches =
        SaveData->SavedLevelName.IsNone() ||
        FName(*UGameplayStatics::GetCurrentLevelName(World, true)) ==
            SaveData->SavedLevelName;

    PendingSurrenderEnemyStates.Reset();
    PendingSurrenderLevelName = bSurrenderLevelMatches
        ? SaveData->SavedLevelName
        : NAME_None;

    if (bSurrenderLevelMatches)
    {
        for (const FBHSurrenderedEnemySaveState& SavedSurrenderState :
            SaveData->SurrenderedEnemyStates)
        {
            if (SavedSurrenderState.FieldOperativeID.IsNone() ||
                !SavedSurrenderState.bSurrendered)
            {
                continue;
            }

            FBHPendingSurrenderEnemyState PendingState;
            PendingState.SectorID = SavedSurrenderState.SectorID;
            PendingState.Transform = SavedSurrenderState.Transform;
            PendingState.bSurrendered = SavedSurrenderState.bSurrendered;
            PendingState.bCustodySecured =
                SavedSurrenderState.bCustodySecured;
            PendingState.SurrenderEscapeSecondsRemaining =
                SavedSurrenderState.SurrenderEscapeSecondsRemaining;
            PendingSurrenderEnemyStates.Add(
                SavedSurrenderState.FieldOperativeID,
                PendingState
            );
        }
    }

    if (!SaveData->EnemyCombatStates.IsEmpty())
    {
        for (const FBHPersistentEnemyCombatSaveState& SavedCombatState :
            SaveData->EnemyCombatStates)
        {
            if (SavedCombatState.FieldOperativeID.IsNone())
            {
                continue;
            }

            FBHPendingSurrenderEnemyState PendingState;
            PendingState.SectorID = SavedCombatState.SectorID;
            PendingState.Transform = SavedCombatState.Transform;
            PendingState.bHasCombatState = true;
            PendingState.Health = SavedCombatState.Health;
            PendingState.MagazineAmmo = SavedCombatState.MagazineAmmo;
            PendingState.ReserveAmmo = SavedCombatState.ReserveAmmo;
            PendingState.FragGrenades = SavedCombatState.FragGrenades;
            PendingState.CombatReadiness =
                SavedCombatState.CombatReadiness;
            PendingState.bSurrendered = SavedCombatState.bSurrendered;
            PendingState.bCustodySecured =
                SavedCombatState.bCustodySecured;
            PendingState.SurrenderEscapeSecondsRemaining =
                SavedCombatState.SurrenderEscapeSecondsRemaining;
            PendingSurrenderEnemyStates.Add(
                SavedCombatState.FieldOperativeID,
                PendingState
            );
        }
    }

    if (!SaveData->WarSectorStates.IsEmpty())
    {
    PendingDefeatedEnemyStates.Reset();
    RuntimeDefeatedEnemyStates.Reset();
    for (const FBHPersistentEnemyDeathSaveState& SavedDeathState :
        SaveData->DefeatedEnemyStates)
    {
        if (SavedDeathState.FieldOperativeID.IsNone())
        {
            continue;
        }

        FBHPendingDefeatedEnemyState PendingDeathState;
        PendingDeathState.SectorID = SavedDeathState.SectorID;
        PendingDeathState.Transform = SavedDeathState.Transform;
        RuntimeDefeatedEnemyStates.Add(
            SavedDeathState.FieldOperativeID,
            PendingDeathState
        );
        if (bSurrenderLevelMatches)
        {
            PendingDefeatedEnemyStates.Add(
                SavedDeathState.FieldOperativeID,
                PendingDeathState
            );
        }
    }
        UBHWarSubsystem* WarSubsystem =
            GetGameInstance()
                ? GetGameInstance()->GetSubsystem<UBHWarSubsystem>()
                : nullptr;

        if (!IsValid(WarSubsystem) ||
            !WarSubsystem->RestoreWarState(
                SaveData->WarSectorStates,
                SaveData->WarSupplyConvoys,
                SaveData->WarEventHistory,
                SaveData->WarTurnNumber,
                SaveData->WarSimulationAccumulator,
                SaveData->SchemaVersion
            ))
        {
            UE_LOG(
                LogTemp,
                Error,
                TEXT("Saved persistent-war state could not be restored.")
            );
            return false;
        }

        WarSubsystem->RestoreGarrisonTransfers(
            SaveData->WarGarrisonTransfers
        );
        WarSubsystem->RestoreCampaignDifficulty(
            SaveData->CampaignDifficulty,
            SaveData->SchemaVersion
        );
        WarSubsystem->RestoreCampaignProgression(
            SaveData->CampaignProgression,
            SaveData->SchemaVersion
        );

        if (SaveData->SchemaVersion >= 21)
        {
            WarSubsystem->RestoreManpowerState(
                SaveData->WarFriendlyManpowerReserve,
                SaveData->WarEnemyManpowerReserve,
                SaveData->WarFriendlyRecruitmentProgress,
                SaveData->WarEnemyRecruitmentProgress
            );
        }

        if (SaveData->bRuntimeWarOperation &&
            !WarSubsystem->RestoreCommittedOperation(
                SaveData->AssignedWarSectorID,
                SaveData->AssignedWarPriorityType,
                SaveData->WarCommittedOperationID,
                SaveData->SchemaVersion >= 39
                    ? SaveData->WarCommittedOperationTargetID
                    : NAME_None
            ))
        {
            UE_LOG(
                LogTemp,
                Error,
                TEXT(
                    "BH_CAMPAIGN_OPERATION_RESTORE_FAILED "
                    "sector=%s type=%d"
                ),
                *SaveData->AssignedWarSectorID.ToString(),
                static_cast<int32>(
                    SaveData->AssignedWarPriorityType
                )
            );
            return false;
        }
    }

    if (SaveData->SchemaVersion >= 46)
    {
        for (TActorIterator<ABHFieldFortification> FortificationIt(World);
            FortificationIt;
            ++FortificationIt)
        {
            ABHFieldFortification* Fortification = *FortificationIt;
            const FBHFieldFortificationSaveState* SavedState =
                SaveData->FieldFortificationStates.FindByPredicate(
                    [Fortification](
                        const FBHFieldFortificationSaveState& Candidate
                    )
                    {
                        return Candidate.PersistenceID ==
                            Fortification->GetPersistenceID();
                    }
                );
            if (SavedState)
            {
                Fortification->RestorePersistentState(
                    SavedState->Transform,
                    SavedState->bConstructed,
                    SavedState->HealthFraction
                    ,
                    SavedState->Plan,
                    SavedState->WorkProgress,
                    SavedState->bDismantleWork,
                    SavedState->ActiveWorkerCount,
                    SavedState->SupplyCacheCharges,
                    SavedState->LastObservationTurn,
                    SavedState->RallyDeploymentsRemaining,
                    SavedState->ObservationProgress
                );
            }
        }
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

    const bool bFreshOperation = SaveData->bFreshOperation;

    if (!bFreshOperation)
    {
        Character->SetActorTransform(
            SaveData->PlayerTransform,
            false,
            nullptr,
            ETeleportType::TeleportPhysics
        );
    }

    const bool bRestoredCharacter =
        SaveData->bRuntimeWarOperation
        ? Character->RestoreRuntimeOperationState(
            MissionData,
            SaveData->RuntimeObjectives,
            SaveData->CurrentObjectiveID,
            SaveData->CompletedObjectiveIDs,
            SaveData->bMissionComplete,
            SaveData->bMissionFailed,
            SaveData->AssignedWarSectorID,
            SaveData->AssignedWarSupplySourceSectorID,
            SaveData->AssignedWarPriorityType,
            SaveData->OpenWorldOperationState,
            SaveData->OwnedKeycardIDs,
            SaveData->CollectedKeycardPersistenceIDs
        )
        : Character->RestorePersistentState(
            MissionData,
            SaveData->CurrentObjectiveID,
            SaveData->CompletedObjectiveIDs,
            SaveData->bMissionComplete,
            SaveData->bMissionFailed,
            SaveData->OwnedKeycardIDs,
            SaveData->CollectedKeycardPersistenceIDs
        );

    if (!bRestoredCharacter)
    {
        return false;
    }

    const bool bSavedFieldSquadHolding =
        SaveData->SchemaVersion >= 25 &&
        SaveData->bFieldSquadHolding;
    const bool bSavedFieldSquadHasCommandLocation =
        SaveData->SchemaVersion >= 36 &&
        bSavedFieldSquadHolding &&
        SaveData->bFieldSquadHasCommandLocation;
    const FVector SavedFieldSquadCommandLocation =
        bSavedFieldSquadHasCommandLocation
            ? SaveData->FieldSquadCommandLocation
            : FVector::ZeroVector;
    const float SavedFieldSquadCommandYaw =
        bSavedFieldSquadHasCommandLocation
            ? SaveData->FieldSquadCommandYaw
            : 0.0f;
    const bool bSavedFieldSquadEmbarked =
        SaveData->SchemaVersion >= 37 &&
        !bFreshOperation &&
        SaveData->bFieldSquadEmbarked &&
        !SaveData->FieldSquadTransportPersistenceID.IsNone();
    const bool bRestoredFieldSquad =
        SaveData->SchemaVersion >= 26
            ? Character->RestoreFieldSquadState(
                SaveData->FieldSquadMemberStates,
                bSavedFieldSquadHolding,
                bSavedFieldSquadHasCommandLocation,
                SavedFieldSquadCommandLocation,
                SavedFieldSquadCommandYaw
            )
            : Character->RestoreFieldSquadState(
                SaveData->SchemaVersion >= 25
                    ? SaveData->LivingFieldSquadCount
                    : 0,
                bSavedFieldSquadHolding,
                false,
                FVector::ZeroVector,
                0.0f
            );

    if (!bRestoredFieldSquad)
    {
        UE_LOG(
            LogTemp,
            Error,
            TEXT(
                "Failed to restore persistent field fireteam "
                "(count=%d hold=%s rally=%s schema=%d)."
            ),
            SaveData->SchemaVersion >= 26
                ? SaveData->FieldSquadMemberStates.Num()
                : SaveData->LivingFieldSquadCount,
            bSavedFieldSquadHolding ? TEXT("true") : TEXT("false"),
            bSavedFieldSquadHasCommandLocation
                ? TEXT("true")
                : TEXT("false"),
            SaveData->SchemaVersion
        );
        return false;
    }

    Character->RestoreCampaignEpilogueAcknowledgement(
        SaveData->bCampaignEpilogueAcknowledged
    );
    Character->RestoreOperationDebriefAcknowledgement(
        SaveData->bOperationDebriefAcknowledged
    );

    if (UBHHealthComponent* HealthComponent =
        Character->GetHealthComponent())
    {
        if (!bFreshOperation &&
            SaveData->SavedHealth > 0.0f)
        {
            HealthComponent->RestorePersistentHealthState(
                SaveData->SavedHealth
            );
        }
        else
        {
            HealthComponent->ResetHealth();
        }
    }

    if (!bFreshOperation &&
        SaveData->SavedMedkitCount >= 0 &&
        SaveData->SavedFieldDressingCount >= 0 &&
        SaveData->SavedHelmetDurability >= 0.0f &&
        SaveData->SavedBodyArmorDurability >= 0.0f)
    {
        if (UBHInjuryComponent* InjuryComponent =
            Character->GetInjuryComponent())
        {
            InjuryComponent->RestorePersistentSupplyState(
                SaveData->SavedMedkitCount,
                SaveData->SavedFieldDressingCount,
                SaveData->SavedHelmetDurability,
                SaveData->SavedBodyArmorDurability
            );
        }
    }

    if (!bFreshOperation &&
        SaveData->bHasSavedInjuryState)
    {
        if (UBHInjuryComponent* InjuryComponent =
            Character->GetInjuryComponent())
        {
            InjuryComponent->RestorePersistentInjuryState(
                SaveData->bSavedBleeding,
                SaveData->SavedBleedRate,
                SaveData->bSavedArmInjured,
                SaveData->bSavedLegInjured
            );
        }
    }

    if (!bFreshOperation &&
        SaveData->SavedMagazineAmmo >= 0 &&
        SaveData->SavedReserveAmmo >= 0)
    {
        if (UBHWeaponComponent* WeaponComponent =
            Character->GetWeaponComponent())
        {
            if (SaveData->SchemaVersion >= 47)
            {
                WeaponComponent->EquipWeaponRole(
                    SaveData->SavedWeaponRole,
                    false
                );
            }
            if (SaveData->bSavedFireModeStateValid)
            {
                WeaponComponent->RestoreFireModeState(
                    SaveData->SavedFireMode
                );
            }
            WeaponComponent->RestoreAmmoState(
                SaveData->SavedMagazineAmmo,
                SaveData->SavedReserveAmmo
            );
            if (SaveData->bSavedWeaponHeatStateValid)
            {
                WeaponComponent->RestoreWeaponHeatState(
                    SaveData->SavedWeaponHeatNormalized,
                    SaveData->bSavedWeaponOverheated
                );
            }
        }
    }

    if (!bFreshOperation &&
        SaveData->SavedFragGrenadeCount >= 0)
    {
        Character->RestoreFragGrenadeCount(
            SaveData->SavedFragGrenadeCount
        );
    }

    if (!bFreshOperation &&
        SaveData->SavedSmokeGrenadeCount >= 0)
    {
        Character->RestoreSmokeGrenadeCount(
            SaveData->SavedSmokeGrenadeCount
        );
    }

    if (!bFreshOperation &&
        SaveData->SavedTacticalFlashlightBattery >= 0.0f)
    {
        Character->RestoreTacticalFlashlightState(
            SaveData->SavedTacticalFlashlightBattery,
            SaveData->bSavedTacticalFlashlightOn
        );
    }

    if (!bFreshOperation &&
        SaveData->SavedEngineeringChargeCount >= 0)
    {
        Character->RestoreEngineeringChargeCount(
            SaveData->SavedEngineeringChargeCount
        );
    }

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

    for (TActorIterator<ABHMissionItemContainer> ContainerIt(World);
        ContainerIt;
        ++ContainerIt)
    {
        ABHMissionItemContainer* Container = *ContainerIt;
        const FBHMissionItemContainerSaveState* SavedState =
            SaveData->MissionItemContainerStates.FindByPredicate(
                [Container](
                    const FBHMissionItemContainerSaveState& Candidate
                )
                {
                    return Candidate.PersistenceID ==
                        Container->GetPersistenceID();
                }
            );

        if (SavedState &&
            SavedState->MissionItemID == Container->GetMissionItemID())
        {
            Container->RestoreStoredMissionItem(
                SavedState->StoredMissionItemID
            );
        }
        else
        {
            Container->RestoreStoredMissionItem(NAME_None);
        }
    }

    RuntimeConsumedWorldItemIDs.Reset();
    RuntimeConsumedWorldItemIDs.Append(
        SaveData->ConsumedWorldItemIDs
    );

    ABHAmbientWarDirector* AmbientWarDirector = nullptr;

    for (TActorIterator<ABHAmbientWarDirector> DirectorIt(World);
        DirectorIt;
        ++DirectorIt)
    {
        AmbientWarDirector = *DirectorIt;

        if (IsValid(AmbientWarDirector))
        {
            AmbientWarDirector->
                ResetSupplyConvoyEncounterForLoad();
            break;
        }
    }

    for (TActorIterator<ABHSupplyConvoyTarget> ConvoyIt(World);
        ConvoyIt;
        ++ConvoyIt)
    {
        ABHSupplyConvoyTarget* Convoy = *ConvoyIt;

        if (IsValid(Convoy) &&
            Convoy->HasRecoverableSalvage())
        {
            Convoy->Destroy();
        }
    }

    for (const FBHConvoySalvageSaveState& SalvageState :
        SaveData->ConvoySalvageStates)
    {
        if (SalvageState.ConvoyID.IsNone() ||
            SalvageState.RecoverableSupply <=
                KINDA_SMALL_NUMBER)
        {
            continue;
        }

        ABHSupplyConvoyTarget* RestoredWreck =
            World->SpawnActor<ABHSupplyConvoyTarget>(
                ABHSupplyConvoyTarget::StaticClass(),
                SalvageState.Transform
            );

        if (!IsValid(RestoredWreck))
        {
            UE_LOG(
                LogTemp,
                Warning,
                TEXT(
                    "BH_CONVOY_SALVAGE_RESTORE_FAILED id=%s"
                ),
                *SalvageState.ConvoyID.ToString()
            );
            continue;
        }

        RestoredWreck->RestoreSalvageWreck(
            SalvageState.ConvoyID,
            SalvageState.SourceSectorID,
            SalvageState.DestinationSectorID,
            SalvageState.OriginalSupplyPayload,
            SalvageState.RecoverableSupply,
            SalvageState.LifetimeRemaining
        );

        if (IsValid(AmbientWarDirector) &&
            SalvageState.SurvivingSecurityCount > 0)
        {
            AmbientWarDirector->
                RestoreSupplyConvoySalvageSecurity(
                    RestoredWreck,
                    SalvageState.SurvivingSecurityCount
                );
        }
    }

    for (TActorIterator<ABHSupplyBase> SupplyIt(World);
        SupplyIt;
        ++SupplyIt)
    {
        ABHSupplyBase* Supply = *SupplyIt;
        Supply->RestoreConsumedState(
            RuntimeConsumedWorldItemIDs.Contains(
                Supply->GetPersistenceID()
            )
        );
    }

    bool bDriverRestored = false;
    bool bFieldSquadPassengerStateRestored =
        !bSavedFieldSquadEmbarked;

    for (TActorIterator<ABHFieldTransport> TransportIt(World);
        TransportIt;
        ++TransportIt)
    {
        ABHFieldTransport* Transport = *TransportIt;
        const FBHFieldTransportSaveState* SavedState =
            SaveData->FieldTransportStates.FindByPredicate(
                [Transport](
                    const FBHFieldTransportSaveState& Candidate
                )
                {
                    return Candidate.PersistenceID ==
                        Transport->GetPersistenceID();
                }
            );

        if (!SavedState)
        {
            continue;
        }

        const bool bRestoreDriver =
            !bFreshOperation &&
            !bDriverRestored &&
            SavedState->bPlayerWasDriving;
        const bool bRestoreFieldSquadPassengers =
            ABHFieldTransport::
                ShouldRestoreFieldSquadPassengers(
                    bRestoreDriver,
                    SaveData->SchemaVersion >= 37,
                    bSavedFieldSquadEmbarked,
                    SaveData->
                        FieldSquadTransportPersistenceID,
                    Transport->GetPersistenceID()
                );
        Transport->RestorePersistentState(
            SavedState->Transform,
            Character,
            bRestoreDriver,
            SavedState->FuelFraction,
            SavedState->HullFraction,
            SavedState->CargoSupply,
            SavedState->CargoSourceSectorID,
            SavedState->CargoDestinationSectorID,
            SavedState->CargoType,
            bRestoreFieldSquadPassengers,
            SaveData->SchemaVersion >= 37
        );
        bDriverRestored |= bRestoreDriver;

        if (bRestoreFieldSquadPassengers)
        {
            bFieldSquadPassengerStateRestored =
                Character->IsFieldSquadEmbarked();
        }
    }

    if (bSavedFieldSquadEmbarked &&
        !bFieldSquadPassengerStateRestored)
    {
        UE_LOG(
            LogTemp,
            Warning,
            TEXT(
                "BH_FIELD_SQUAD_EMBARK_RESTORE_SKIPPED "
                "transport=%s members=%d driver=%s"
            ),
            *SaveData->
                FieldSquadTransportPersistenceID.ToString(),
            Character->GetLivingFieldSquadCount(),
            bDriverRestored ? TEXT("true") : TEXT("false")
        );
    }
    else if (bSavedFieldSquadEmbarked)
    {
        UE_LOG(
            LogTemp,
            Display,
            TEXT(
                "BH_FIELD_SQUAD_EMBARK_RESTORED "
                "transport=%s passengers=%d"
            ),
            *SaveData->
                FieldSquadTransportPersistenceID.ToString(),
            Character->GetLivingFieldSquadCount()
        );
    }

    if (!bFreshOperation &&
        SaveData->SavedAntiVehicleRoundCount >= 0)
    {
        Character->RestoreAntiVehicleRoundCount(
            SaveData->SavedAntiVehicleRoundCount
        );
    }

    int32 RestoredEnemyCombatStateCount = 0;
    int32 RestoredSurrenderedEnemyCount = 0;
    if (bSurrenderLevelMatches)
    {
        SaveData->DefeatedEnemyStates.Reset();
    for (const TPair<FName, FBHPendingDefeatedEnemyState>& Pair :
        RuntimeDefeatedEnemyStates)
    {
        FBHPersistentEnemyDeathSaveState DeathState;
        DeathState.FieldOperativeID = Pair.Key;
        DeathState.SectorID = Pair.Value.SectorID;
        DeathState.Transform = Pair.Value.Transform;
        SaveData->DefeatedEnemyStates.Add(DeathState);
    }    for (TActorIterator<ABHEnemySoldier> EnemyIt(World);
            EnemyIt;
            ++EnemyIt)
        {
            ABHEnemySoldier* Enemy = *EnemyIt;

            if (!IsValid(Enemy) ||
                Enemy->GetCombatFaction() != EBHCombatFaction::Hostile ||
                Enemy->GetFieldOperativeID().IsNone())
            {
                continue;
            }

            if (!ApplyPendingSurrenderState(Enemy))
            {
                continue;
            }

            ++RestoredEnemyCombatStateCount;
            if (Enemy->IsSurrendered())
            {
                ++RestoredSurrenderedEnemyCount;
            }
        }

        if (RestoredEnemyCombatStateCount > 0)
        {
            UE_LOG(
                LogTemp,
                Display,
                TEXT(
                    "BH_ENEMY_COMBAT_STATE_RESTORED "
                    "count=%d surrendered=%d"
                ),
                RestoredEnemyCombatStateCount,
                RestoredSurrenderedEnemyCount
            );
        }
    }

    UE_LOG(
        LogTemp,
        Log,
        TEXT("Checkpoint load succeeded from slot %s."),
        *GetActiveSaveSlotName()
    );
    return true;
}

bool UBHSaveSubsystem::ApplyPendingSurrenderState(
    ABHEnemySoldier* Enemy
)
{
    if (!IsValid(Enemy) || !Enemy->HasAuthority())
    {
        return false;
    }

    UWorld* World = Enemy->GetWorld();
    if (!IsValid(World) ||
        (!PendingSurrenderLevelName.IsNone() &&
            FName(*UGameplayStatics::GetCurrentLevelName(World, true)) !=
                PendingSurrenderLevelName))
    {
        return false;
    }

    const FName FieldOperativeID = Enemy->GetFieldOperativeID();
    if (FieldOperativeID.IsNone())
    {
        return false;
    }

    const FBHPendingSurrenderEnemyState* PendingState =
        PendingSurrenderEnemyStates.Find(FieldOperativeID);
    const FBHPendingDefeatedEnemyState* PendingDefeatedState =
        PendingDefeatedEnemyStates.Find(FieldOperativeID);
    const bool bRestoreDefeatedState =
        PendingDefeatedState != nullptr;
    if ((PendingState == nullptr ||
        (!PendingState->bHasCombatState &&
            !PendingState->bSurrendered)) &&
        !bRestoreDefeatedState)
    {
        return false;
    }

    const FName SavedSectorID = PendingState != nullptr
        ? PendingState->SectorID
        : PendingDefeatedState->SectorID;
    const FName CurrentSectorID = Enemy->GetSurrenderSectorID();
    if (!SavedSectorID.IsNone() &&
        !CurrentSectorID.IsNone() &&
        SavedSectorID != CurrentSectorID)
    {
        return false;
    }

    const bool bRestoreCombatState =
        PendingState != nullptr && PendingState->bHasCombatState;
    const bool bRestoreSurrenderState =
        PendingState != nullptr && PendingState->bSurrendered;
    const float SavedHealth = PendingState != nullptr
        ? PendingState->Health
        : 0.0f;
    const int32 SavedMagazineAmmo = PendingState != nullptr
        ? PendingState->MagazineAmmo
        : -1;
    const int32 SavedReserveAmmo = PendingState != nullptr
        ? PendingState->ReserveAmmo
        : -1;
    const int32 SavedFragGrenades = PendingState != nullptr
        ? PendingState->FragGrenades
        : -1;
    const float SavedCombatReadiness = PendingState != nullptr
        ? PendingState->CombatReadiness
        : 1.0f;
    const bool bCustodySecured = PendingState != nullptr
        && PendingState->bCustodySecured;
    const float SurrenderEscapeSecondsRemaining = PendingState != nullptr
        ? PendingState->SurrenderEscapeSecondsRemaining
        : 0.0f;

    const FTransform RestoreTransform = PendingState != nullptr
        ? PendingState->Transform
        : PendingDefeatedState->Transform;

    Enemy->SetActorTransform(RestoreTransform);
    if (bRestoreCombatState)
    {
        Enemy->RestorePersistentCombatState(
            SavedHealth,
            SavedMagazineAmmo,
            SavedReserveAmmo,
            SavedFragGrenades,
            SavedCombatReadiness
        );
    }

    if (bRestoreSurrenderState)
    {
        Enemy->RestoreSurrenderPersistence(
            true,
            bCustodySecured,
            SurrenderEscapeSecondsRemaining
        );
    }

    if (bRestoreDefeatedState)
    {
        Enemy->RestorePersistentDeathState();
    }

    PendingSurrenderEnemyStates.Remove(FieldOperativeID);
    PendingDefeatedEnemyStates.Remove(FieldOperativeID);

    UE_LOG(
        LogTemp,
        Display,
        TEXT(
            "BH_ENEMY_COMBAT_STATE_RESTORED id=%s "
            "combat_state=%s surrendered=%s"
        ),
        *FieldOperativeID.ToString(),
        bRestoreCombatState ? TEXT("true") : TEXT("false"),
        bRestoreSurrenderState ? TEXT("true") : TEXT("false")
    );
    return true;
}

void UBHSaveSubsystem::HandlePostLoadMap(UWorld* LoadedWorld)
{
    if (IsValid(LoadedWorld) && LoadedWorld->IsGameWorld())
    {
        ScheduleFieldAutosave(LoadedWorld);
    }

    if (!IsValid(PendingSaveData) ||
        !IsValid(LoadedWorld) ||
        !LoadedWorld->IsGameWorld())
    {
        return;
    }

    PendingSaveApplyAttempts = 0;

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
    if (!IsValid(PendingSaveData) ||
        !IsValid(LoadedWorld) ||
        !LoadedWorld->IsGameWorld())
    {
        return;
    }

    if (!IsValid(FindPlayerCharacter(LoadedWorld)))
    {
        constexpr int32 MaximumApplyAttempts = 40;
        constexpr float ApplyRetryDelaySeconds = 0.25f;
        ++PendingSaveApplyAttempts;

        if (PendingSaveApplyAttempts > MaximumApplyAttempts)
        {
            UE_LOG(
                LogTemp,
                Error,
                TEXT(
                    "BH_CHECKPOINT_APPLY_TIMEOUT attempts=%d world=%s"
                ),
                PendingSaveApplyAttempts,
                *GetNameSafe(LoadedWorld)
            );
            PendingSaveData = nullptr;
            PendingLoadedSchemaVersion = 0;
            bPendingLoadedFromBackup = false;
            PendingPlayerDeathAttritionSectorID = NAME_None;
            PendingSaveApplyAttempts = 0;
            return;
        }

        if (PendingSaveApplyAttempts == 1)
        {
            UE_LOG(
                LogTemp,
                Display,
                TEXT(
                    "BH_CHECKPOINT_APPLY_WAITING_FOR_PLAYER world=%s"
                ),
                *GetNameSafe(LoadedWorld)
            );
        }

        FTimerDelegate RetryDelegate;
        RetryDelegate.BindUObject(
            this,
            &UBHSaveSubsystem::ApplyPendingSave,
            LoadedWorld
        );
        FTimerHandle RetryTimer;
        LoadedWorld->GetTimerManager().SetTimer(
            RetryTimer,
            RetryDelegate,
            ApplyRetryDelaySeconds,
            false
        );
        return;
    }

    UBHSaveGame* SaveData = PendingSaveData;
    PendingSaveData = nullptr;
    PendingSaveApplyAttempts = 0;
    const int32 LoadedSchemaVersion = PendingLoadedSchemaVersion;
    const bool bLoadedFromBackup = bPendingLoadedFromBackup;
    PendingLoadedSchemaVersion = 0;
    bPendingLoadedFromBackup = false;
    const FName PlayerDeathAttritionSectorID =
        PendingPlayerDeathAttritionSectorID;
    PendingPlayerDeathAttritionSectorID = NAME_None;

    if (!ApplySaveData(SaveData, LoadedWorld))
    {
        UE_LOG(
            LogTemp,
            Error,
            TEXT("Checkpoint state could not be applied.")
        );
        return;
    }

    if (!PlayerDeathAttritionSectorID.IsNone())
    {
        FTimerHandle RapidRedeployTimer;
        LoadedWorld->GetTimerManager().SetTimer(
            RapidRedeployTimer,
            FTimerDelegate::CreateWeakLambda(
                this,
                [this, LoadedWorld]()
                {
                    if (ABHCharacter* RedeployedCharacter =
                            FindPlayerCharacter(LoadedWorld))
                    {
                        RedeployedCharacter->ApplyRapidOperationRedeployment();
                    }
                }
            ),
            1.0f,
            false
        );
    }

    if (PlayerDeathAttritionSectorID.IsNone())
    {
        if (bLoadedFromBackup ||
            LoadedSchemaVersion < BHSave::CurrentSchemaVersion)
        {
            const bool bCheckpointHealed = SaveProgress();
            UE_LOG(
                LogTemp,
                Display,
                TEXT(
                    "BH_CHECKPOINT_HEALED source=%s "
                    "schema_from=%d schema_to=%d result=%s"
                ),
                bLoadedFromBackup
                    ? TEXT("backup")
                    : TEXT("legacy_primary"),
                LoadedSchemaVersion,
                BHSave::CurrentSchemaVersion,
                bCheckpointHealed
                    ? TEXT("success")
                    : TEXT("failure")
            );
        }
        return;
    }

    UGameInstance* GameInstance = GetGameInstance();
    UBHWarSubsystem* WarSubsystem = IsValid(GameInstance)
        ? GameInstance->GetSubsystem<UBHWarSubsystem>()
        : nullptr;
    ABHCharacter* Character = FindPlayerCharacter(LoadedWorld);

    if (!IsValid(WarSubsystem) ||
        !IsValid(Character) ||
        !WarSubsystem->ApplyAmbientBattleResult(
            PlayerDeathAttritionSectorID,
            1,
            0
        ))
    {
        UE_LOG(
            LogTemp,
            Warning,
            TEXT(
                "BH_PLAYER_DEATH_ATTRITION_FAILED sector=%s"
            ),
            *PlayerDeathAttritionSectorID.ToString()
        );
        return;
    }

    const FBHWarSectorState CasualtySector =
        WarSubsystem->GetSectorState(
            PlayerDeathAttritionSectorID
        );
    const FText SectorDisplayName =
        CasualtySector.SectorID.IsNone()
            ? FText::FromName(PlayerDeathAttritionSectorID)
            : CasualtySector.DisplayName;
    const bool bCheckpointSaved = SaveProgress();

    if (bLoadedFromBackup ||
        LoadedSchemaVersion < BHSave::CurrentSchemaVersion)
    {
        UE_LOG(
            LogTemp,
            Display,
            TEXT(
                "BH_CHECKPOINT_HEALED source=%s "
                "schema_from=%d schema_to=%d result=%s"
            ),
            bLoadedFromBackup
                ? TEXT("backup")
                : TEXT("legacy_primary"),
            LoadedSchemaVersion,
            BHSave::CurrentSchemaVersion,
            bCheckpointSaved ? TEXT("success") : TEXT("failure")
        );
    }

    Character->ShowStatusNotification(
        FText::Format(
            NSLOCTEXT(
                "BrokenHorizon",
                "PlayerDeathRedeploymentCost",
                "REDEPLOYMENT COMPLETE\n\n"
                "A replacement was drawn from {0}.\n"
                "Friendly strength and supply were reduced.\n"
                "{1}"
            ),
            SectorDisplayName,
            bCheckpointSaved
                ? NSLOCTEXT(
                    "BrokenHorizon",
                    "PlayerDeathAttritionSaved",
                    "CAMPAIGN UPDATED"
                )
                : NSLOCTEXT(
                    "BrokenHorizon",
                    "PlayerDeathAttritionSaveFailed",
                    "CAMPAIGN SAVE FAILED"
                )
        )
    );

    UE_LOG(
        LogTemp,
        Display,
        TEXT(
            "BH_PLAYER_DEATH_ATTRITION sector=%s "
            "friendly_strength=%.1f supply=%.1f saved=%d"
        ),
        *PlayerDeathAttritionSectorID.ToString(),
        CasualtySector.FriendlyStrength,
        CasualtySector.Supply,
        bCheckpointSaved ? 1 : 0
    );
}

void UBHSaveSubsystem::HandleWarStateChanged(
    int32 TurnNumber,
    FName PrioritySectorID,
    EBHWarPriorityType PriorityType
)
{
    if (bSuppressWarAutosave)
    {
        return;
    }

    UWorld* World = GetGameInstance()
        ? GetGameInstance()->GetWorld()
        : nullptr;

    if (IsClientCampaignWorld(World))
    {
        return;
    }

    ABHCharacter* Character = FindPlayerCharacter(World);

    if (!IsValid(World) ||
        !World->IsGameWorld() ||
        !IsValid(Character) ||
        !IsValid(Character->GetMissionData()))
    {
        return;
    }

    const UBHWarSubsystem* WarSubsystem = GetGameInstance()
        ? GetGameInstance()->GetSubsystem<UBHWarSubsystem>()
        : nullptr;
    const float CheckpointIntervalMultiplier =
        IsValid(WarSubsystem)
        ? FMath::Clamp(
            WarSubsystem->GetCampaignDifficulty()
                .CheckpointIntervalMultiplier,
            0.25f,
            2.0f
        )
        : 1.0f;
    World->GetTimerManager().SetTimer(
        WarAutosaveTimerHandle,
        this,
        &UBHSaveSubsystem::PerformWarAutosave,
        FMath::Max(
            0.1f,
            WarAutosaveDelaySeconds *
                CheckpointIntervalMultiplier
        ),
        false
    );

    UE_LOG(
        LogTemp,
        Verbose,
        TEXT(
            "BH_WAR_AUTOSAVE_SCHEDULED turn=%d "
            "priority=%s type=%d"
        ),
        TurnNumber,
        *PrioritySectorID.ToString(),
        static_cast<int32>(PriorityType)
    );
}

void UBHSaveSubsystem::ClearPendingWarAutosave(
    UWorld* World
)
{
    if (!IsValid(World))
    {
        return;
    }

    FTimerManager& TimerManager =
        World->GetTimerManager();

    if (!TimerManager.IsTimerActive(
            WarAutosaveTimerHandle
        ))
    {
        return;
    }

    TimerManager.ClearTimer(
        WarAutosaveTimerHandle
    );

    UE_LOG(
        LogTemp,
        Verbose,
        TEXT("BH_WAR_AUTOSAVE_COALESCED")
    );
}

void UBHSaveSubsystem::PerformWarAutosave()
{
    const UWorld* World = GetGameInstance()
        ? GetGameInstance()->GetWorld()
        : nullptr;

    if (IsClientCampaignWorld(World))
    {
        return;
    }

    if (ShouldDeferCrashRecoveryAutosave())
    {
        UE_LOG(
            LogTemp,
            Display,
            TEXT("BH_CRASH_RECOVERY_AUTOSAVE_DEFERRED type=war")
        );
        return;
    }

    if (!SaveProgress())
    {
        UE_LOG(
            LogTemp,
            Warning,
            TEXT("BH_WAR_AUTOSAVE_FAILED")
        );
        return;
    }

    const UBHWarSubsystem* WarSubsystem =
        GetGameInstance()
            ? GetGameInstance()->GetSubsystem<UBHWarSubsystem>()
            : nullptr;

    UE_LOG(
        LogTemp,
        Display,
        TEXT("BH_WAR_AUTOSAVE_COMPLETE turn=%d"),
        IsValid(WarSubsystem)
            ? WarSubsystem->GetTurnNumber()
            : 0
    );
}

void UBHSaveSubsystem::ScheduleFieldAutosave(UWorld* World)
{
    if (!IsValid(World) ||
        !World->IsGameWorld() ||
        IsClientCampaignWorld(World))
    {
        return;
    }

    const float Interval =
        FMath::Max(30.0f, FieldAutosaveIntervalSeconds);
    World->GetTimerManager().SetTimer(
        FieldAutosaveTimerHandle,
        this,
        &UBHSaveSubsystem::PerformFieldAutosave,
        Interval,
        true,
        Interval
    );

    UE_LOG(
        LogTemp,
        Display,
        TEXT("BH_FIELD_AUTOSAVE_ARMED interval=%.1f"),
        Interval
    );
}

void UBHSaveSubsystem::PerformFieldAutosave()
{
    if (bSuppressWarAutosave)
    {
        return;
    }

    UWorld* World = GetGameInstance()
        ? GetGameInstance()->GetWorld()
        : nullptr;

    if (IsClientCampaignWorld(World))
    {
        return;
    }

    if (ShouldDeferCrashRecoveryAutosave())
    {
        UE_LOG(
            LogTemp,
            Display,
            TEXT("BH_CRASH_RECOVERY_AUTOSAVE_DEFERRED type=field")
        );
        return;
    }

    ABHCharacter* Character = FindPlayerCharacter(World);

    if (!IsValid(World) ||
        !World->IsGameWorld() ||
        !IsValid(Character) ||
        !IsValid(Character->GetMissionData()) ||
        !HasValidSaveGame() ||
        !Character->CanCreateFieldAutosave())
    {
        UE_LOG(
            LogTemp,
            VeryVerbose,
            TEXT("BH_FIELD_AUTOSAVE_DEFERRED")
        );
        return;
    }

    if (!SaveProgress())
    {
        UE_LOG(
            LogTemp,
            Warning,
            TEXT("BH_FIELD_AUTOSAVE_FAILED")
        );
        return;
    }

    UE_LOG(
        LogTemp,
        Display,
        TEXT(
            "BH_FIELD_AUTOSAVE_COMPLETE location=%s"
        ),
        *Character->GetActorLocation().ToCompactString()
    );
}

bool UBHSaveSubsystem::ShouldDeferCrashRecoveryAutosave() const
{
#if !UE_BUILD_SHIPPING
    return !bCrashRecoveryLoadStarted &&
        FParse::Param(
            FCommandLine::Get(),
            TEXT("BHTestRestoreCrashRecovery")
        );
#else
    return false;
#endif
}


