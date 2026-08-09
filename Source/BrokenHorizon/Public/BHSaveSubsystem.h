#pragma once

#include "BHWarTypes.h"
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "TimerManager.h"
#include "BHSaveSubsystem.generated.h"

class ABHCharacter;
class ABHEnemySoldier;
class UBHSaveGame;
class UWorld;
class FSubsystemCollectionBase;

struct FBHPendingSurrenderEnemyState
{
    FName SectorID = NAME_None;
    FTransform Transform = FTransform::Identity;
    bool bHasCombatState = false;
    float Health = 0.0f;
    int32 MagazineAmmo = 0;
    int32 ReserveAmmo = 0;
    int32 FragGrenades = 0;
    float CombatReadiness = 1.0f;
    bool bSurrendered = true;
    bool bCustodySecured = false;
    float SurrenderEscapeSecondsRemaining = 0.0f;
};

struct FBHPendingDefeatedEnemyState
{
    FName SectorID = NAME_None;
    FTransform Transform = FTransform::Identity;
};
UCLASS()
class BROKENHORIZON_API UBHSaveSubsystem
    : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(
        FSubsystemCollectionBase& Collection
    ) override;

    virtual void Deinitialize() override;

    UFUNCTION(BlueprintCallable, Category = "Save")
    bool SaveProgress();

    bool SavePlayerResources();

    UFUNCTION(BlueprintCallable, Category = "Save")
    bool LoadProgress();

    bool ReloadCheckpointAfterPlayerDeath(
        FName CasualtySectorID
    );

    UFUNCTION(BlueprintCallable, Category = "Save")
    bool DeployNextOperation();

    UFUNCTION(BlueprintCallable, Category = "Save")
    bool DeployOperation(
        FName SectorID,
        EBHWarPriorityType OperationType
    );

    bool DeployOperationForCharacter(
        ABHCharacter* RequestingCharacter,
        FName SectorID,
        EBHWarPriorityType OperationType
    );

    UFUNCTION(BlueprintPure, Category = "Save")
    bool HasSaveGame() const;

    UFUNCTION(BlueprintCallable, Category = "Save")
    bool HasValidSaveGame() const;

    UFUNCTION(BlueprintCallable, Category = "Save")
    bool DeleteSaveGame();

    UFUNCTION(BlueprintCallable, Category = "Save")
    bool RecordConsumedWorldItem(FName PersistenceID);

    UFUNCTION(BlueprintPure, Category = "Save")
    bool IsWorldItemConsumed(FName PersistenceID) const;

    bool ApplyPendingSurrenderState(ABHEnemySoldier* Enemy);
    void RecordDefeatedEnemy(ABHEnemySoldier* Enemy);

private:
    static const FString SaveSlotName;
    static const FString BackupSaveSlotName;
    static FString GetActiveSaveSlotName();
    static FString GetActiveBackupSaveSlotName();
    static constexpr int32 SaveUserIndex = 0;

    bool IsClientCampaignWorld(const UWorld* World) const;

    bool SaveProgressForCharacter(ABHCharacter* Character);

    ABHCharacter* FindPlayerCharacter(UWorld* World) const;

    UBHSaveGame* LoadBestSaveGame(
        bool* bOutUsedBackup = nullptr
    ) const;

    bool SavePrimaryWithBackup(UBHSaveGame* SaveData) const;

    bool ValidatePersistenceIDs(UWorld* World) const;

    bool ApplySaveData(
        UBHSaveGame* SaveData,
        UWorld* World
    );

    void HandlePostLoadMap(UWorld* LoadedWorld);

    void ApplyPendingSave(UWorld* LoadedWorld);

    UFUNCTION()
    void HandleWarStateChanged(
        int32 TurnNumber,
        FName PrioritySectorID,
        EBHWarPriorityType PriorityType
    );

    void PerformWarAutosave();
    void ClearPendingWarAutosave(UWorld* World);
    void ScheduleFieldAutosave(UWorld* World);
    void PerformFieldAutosave();
    bool ShouldDeferCrashRecoveryAutosave() const;

    UPROPERTY(Transient)
    TObjectPtr<UBHSaveGame> PendingSaveData;

    TMap<FName, FBHPendingSurrenderEnemyState>
        PendingSurrenderEnemyStates;

    TMap<FName, FBHPendingDefeatedEnemyState> PendingDefeatedEnemyStates;

    FName PendingSurrenderLevelName = NAME_None;

    int32 PendingSaveApplyAttempts = 0;

    int32 PendingLoadedSchemaVersion = 0;

    bool bPendingLoadedFromBackup = false;

    FName PendingPlayerDeathAttritionSectorID = NAME_None;

    FDelegateHandle PostLoadMapHandle;

    TSet<FName> RuntimeConsumedWorldItemIDs;

    TMap<FName, FBHPendingDefeatedEnemyState> RuntimeDefeatedEnemyStates;

    FTimerHandle WarAutosaveTimerHandle;
    FTimerHandle FieldAutosaveTimerHandle;
    float WarAutosaveDelaySeconds = 5.0f;
    float FieldAutosaveIntervalSeconds = 90.0f;
    bool bSuppressWarAutosave = false;
    bool bCrashRecoveryLoadStarted = false;
};
