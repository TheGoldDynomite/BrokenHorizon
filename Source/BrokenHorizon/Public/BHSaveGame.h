#pragma once

#include "CoreMinimal.h"
#include "BHMissionData.h"
#include "BHWeaponComponent.h"
#include "BHFieldFortification.h"
#include "BHWarTypes.h"
#include "GameFramework/SaveGame.h"
#include "BHSaveGame.generated.h"

class UBHMissionData;

namespace BHSave
{
    inline constexpr int32 MinimumCompatibleSchemaVersion = 1;
    inline constexpr int32 CurrentSchemaVersion = 61;
}

USTRUCT(BlueprintType)
struct FBHFieldFortificationSaveState
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame)
    FName PersistenceID = NAME_None;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame)
    FName SectorID = NAME_None;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame)
    FTransform Transform = FTransform::Identity;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame)
    bool bConstructed = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame)
    float HealthFraction = 0.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame)
    float WorkProgress = 0.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame)
    bool bDismantleWork = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame)
    EBHFortificationPlan Plan = EBHFortificationPlan::HastyBarricade;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame)
    int32 ActiveWorkerCount = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame)
    int32 SupplyCacheCharges = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame)
    int32 LastObservationTurn = INDEX_NONE;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame)
    int32 RallyDeploymentsRemaining = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame)
    float ObservationProgress = 0.0f;
};

USTRUCT(BlueprintType)
struct FBHConvoySalvageSaveState
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame)
    FName ConvoyID = NAME_None;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame)
    FName SourceSectorID = NAME_None;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame)
    FName DestinationSectorID = NAME_None;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame)
    FTransform Transform = FTransform::Identity;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame)
    float OriginalSupplyPayload = 0.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame)
    float RecoverableSupply = 0.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame)
    float LifetimeRemaining = 0.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame)
    int32 SurvivingSecurityCount = 0;
};

USTRUCT(BlueprintType)
struct FBHFieldTransportSaveState
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame)
    FName PersistenceID = NAME_None;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame)
    FTransform Transform = FTransform::Identity;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame)
    bool bPlayerWasDriving = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame)
    float FuelFraction = 1.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame)
    float HullFraction = 1.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame)
    float CargoSupply = 0.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame)
    FName CargoSourceSectorID = NAME_None;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame)
    FName CargoDestinationSectorID = NAME_None;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame)
    EBHWarConvoyCargoType CargoType =
        EBHWarConvoyCargoType::MilitarySupply;
};

USTRUCT(BlueprintType)
struct FBHSurrenderedEnemySaveState
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	FName FieldOperativeID = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	FName SectorID = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	FTransform Transform = FTransform::Identity;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	bool bSurrendered = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	bool bCustodySecured = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	float SurrenderEscapeSecondsRemaining = 0.0f;
};

USTRUCT(BlueprintType)
struct FBHMissionItemContainerSaveState
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame)
    FName PersistenceID = NAME_None;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame)
    FName MissionItemID = NAME_None;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame)
    FName StoredMissionItemID = NAME_None;
};

USTRUCT(BlueprintType)
struct FBHPersistentEnemyCombatSaveState
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    FName FieldOperativeID = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    FName SectorID = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    FTransform Transform = FTransform::Identity;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    float Health = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    int32 MagazineAmmo = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    int32 ReserveAmmo = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    int32 FragGrenades = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    float CombatReadiness = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    bool bSurrendered = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    bool bCustodySecured = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    float SurrenderEscapeSecondsRemaining = 0.0f;
};

USTRUCT(BlueprintType)
struct FBHPersistentEnemyDeathSaveState
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    FName FieldOperativeID = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    FName SectorID = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    FTransform Transform = FTransform::Identity;
};UCLASS()
class BROKENHORIZON_API UBHSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "World|Surrender")
	TArray<FBHSurrenderedEnemySaveState> SurrenderedEnemyStates;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "World|Combat")
	TArray<FBHPersistentEnemyCombatSaveState> EnemyCombatStates;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "World|Combat")
	TArray<FBHPersistentEnemyDeathSaveState> DefeatedEnemyStates;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Player|Weapon")
	float SavedWeaponHeatNormalized = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Player|Weapon")
	bool bSavedWeaponOverheated = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Player|Weapon")
	bool bSavedWeaponHeatStateValid = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Player|Weapon")
	EBHFireMode SavedFireMode = EBHFireMode::SemiAutomatic;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Player|Weapon")
	bool bSavedFireModeStateValid = false;

public:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Save")
    int32 SchemaVersion = BHSave::CurrentSchemaVersion;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Difficulty")
    FBHCampaignDifficultyProfile CampaignDifficulty;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Progression")
    FBHCampaignProgressionState CampaignProgression;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Save")
    FName SavedLevelName = NAME_None;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Mission")
    TSoftObjectPtr<UBHMissionData> MissionData;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Mission")
    FName CurrentObjectiveID = NAME_None;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Mission")
    TArray<FName> CompletedObjectiveIDs;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Mission")
    bool bMissionComplete = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Mission")
    bool bMissionFailed = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Mission")
    bool bFreshOperation = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Mission")
    bool bRuntimeWarOperation = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Mission")
    bool bCampaignEpilogueAcknowledged = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Mission")
    bool bOperationDebriefAcknowledged = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Mission")
    TArray<FBHObjectiveDefinition> RuntimeObjectives;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Mission")
    FName AssignedWarSectorID = NAME_None;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Mission")
    FName AssignedWarSupplySourceSectorID = NAME_None;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Mission")
    EBHWarPriorityType AssignedWarPriorityType =
        EBHWarPriorityType::None;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Mission")
    FBHOpenWorldOperationState OpenWorldOperationState;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Persistent War|Resistance Force")
    FBHResistanceForceState ResistanceForce;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Persistent War|Field Squad")
    int32 LivingFieldSquadCount = 0;

    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        SaveGame,
        Category = "Persistent War|Field Squad"
    )
    TArray<FBHFieldSquadMemberState> FieldSquadMemberStates;

    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        SaveGame,
        Category = "Persistent War|Field Squad"
    )
    bool bFieldSquadHolding = false;

    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        SaveGame,
        Category = "Persistent War|Field Squad"
    )
    bool bFieldSquadHasCommandLocation = false;

    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        SaveGame,
        Category = "Persistent War|Field Squad"
    )
    FVector FieldSquadCommandLocation = FVector::ZeroVector;

    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        SaveGame,
        Category = "Persistent War|Field Squad"
    )
    float FieldSquadCommandYaw = 0.0f;

    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        SaveGame,
        Category = "Persistent War|Field Squad"
    )
    bool bFieldSquadEmbarked = false;

    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        SaveGame,
        Category = "Persistent War|Field Squad"
    )
    FName FieldSquadTransportPersistenceID = NAME_None;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Inventory")
    TArray<FName> OwnedKeycardIDs;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "World")
    TArray<FName> CollectedKeycardPersistenceIDs;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "World")
    TArray<FName> UnlockedDoorPersistenceIDs;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "World")
    TArray<FName> ConsumedWorldItemIDs;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "World|Inventory")
    TArray<FBHMissionItemContainerSaveState> MissionItemContainerStates;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "World")
    TArray<FBHFieldTransportSaveState> FieldTransportStates;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "World")
    TArray<FBHFieldFortificationSaveState> FieldFortificationStates;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "World")
    TArray<FBHConvoySalvageSaveState> ConvoySalvageStates;

    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        SaveGame,
        Category = "Persistent War"
    )
    TArray<FBHWarSectorState> WarSectorStates;

    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        SaveGame,
        Category = "Persistent War"
    )
    TArray<FBHWarSupplyConvoyState> WarSupplyConvoys;

    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        SaveGame,
        Category = "Persistent War"
    )
    TArray<FBHWarGarrisonTransferState> WarGarrisonTransfers;

    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        SaveGame,
        Category = "Persistent War"
    )
    int32 WarFriendlyManpowerReserve = 0;

    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        SaveGame,
        Category = "Persistent War"
    )
    int32 WarEnemyManpowerReserve = 0;

    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        SaveGame,
        Category = "Persistent War"
    )
    float WarFriendlyRecruitmentProgress = 0.0f;

    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        SaveGame,
        Category = "Persistent War"
    )
    float WarEnemyRecruitmentProgress = 0.0f;

    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        SaveGame,
        Category = "Persistent War"
    )
    TArray<FBHWarEventRecord> WarEventHistory;

    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        SaveGame,
        Category = "Persistent War"
    )
    int32 WarTurnNumber = 0;

    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        SaveGame,
        Category = "Persistent War"
    )
    float WarSimulationAccumulator = 0.0f;

    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        SaveGame,
        Category = "Persistent War|Operation"
    )
    FName WarCommittedOperationID = NAME_None;

    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        SaveGame,
        Category = "Persistent War|Operation"
    )
    FName WarCommittedOperationTargetID = NAME_None;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Player")
    FTransform PlayerTransform = FTransform::Identity;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Player")
    float SavedHealth = -1.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Player")
    bool bHasSavedInjuryState = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Player")
    bool bSavedBleeding = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Player")
    float SavedBleedRate = 0.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Player")
    bool bSavedArmInjured = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Player")
    bool bSavedLegInjured = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Player")
    int32 SavedMedkitCount = -1;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Player")
    int32 SavedFieldDressingCount = -1;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Player")
    float SavedHelmetDurability = -1.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Player")
    float SavedBodyArmorDurability = -1.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Player")
    int32 SavedMagazineAmmo = -1;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Player|Weapon")
    EBHWeaponRole SavedWeaponRole = EBHWeaponRole::Assault;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Player")
    int32 SavedReserveAmmo = -1;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Player")
    int32 SavedFragGrenadeCount = -1;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Player")
    int32 SavedSmokeGrenadeCount = -1;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Player")
    float SavedTacticalFlashlightBattery = -1.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Player")
    bool bSavedTacticalFlashlightOn = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Player")
    int32 SavedEngineeringChargeCount = -1;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Player|Anti Vehicle")
    int32 SavedAntiVehicleRoundCount = -1;
};


