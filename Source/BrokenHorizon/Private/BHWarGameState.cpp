#include "BHWarGameState.h"
#include "BHDefenseAMultiplayerTest.h"

#include "BHCharacter.h"
#include "BHBattlefieldConditions.h"
#include "BHLoadoutWeight.h"
#include "BHAmbientWarDirector.h"
#include "BHCombatStatusWidget.h"
#include "BHAmmoSupply.h"
#include "BHArmoredThreat.h"
#include "BHEnemySoldier.h"
#include "BHDoor.h"
#include "BHExtractionZone.h"
#include "BHKeycard.h"
#include "BHMissionItemContainer.h"
#include "BHFieldTransport.h"
#include "BHFragGrenade.h"
#include "BHHealthComponent.h"
#include "BHInteractable.h"
#include "BHInteractionPromptWidget.h"
#include "BHInjuryComponent.h"
#include "BHMissionData.h"
#include "BHOpenWorldOperationDirector.h"
#include "BHObjectiveNotificationWidget.h"
#include "BHPauseMenuWidget.h"
#include "BHSaveSubsystem.h"
#include "BHSectorResupplyStation.h"
#include "BHWorldKitModule.h"
#include "BHSalvagePickup.h"
#include "BHSectorAnchor.h"
#include "BHWarSubsystem.h"
#include "BHWarMapWidget.h"
#include "BHUserSettingsSubsystem.h"
#include "BHWeaponComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/Channel.h"
#include "Engine/NetConnection.h"
#include "Engine/NetDriver.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "Containers/Ticker.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "HighResScreenshot.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "ProfilingDebugging/CsvProfiler.h"
#include "Net/UnrealNetwork.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerState.h"
#include "NavigationSystem.h"
#include "TimerManager.h"
#include "UObject/UObjectIterator.h"

#if !UE_BUILD_SHIPPING
CSV_DEFINE_CATEGORY(BrokenHorizon, true);
namespace
{
bool bBHTestServerTravelIssued = false;
bool bBHTestServerTravelCompleted = false;
bool bBHTestTransportTravelPersistence = false;
bool bBHTestTransportCasualtyPersistence = false;
bool bBHTestOperationCompletionAfterTravel = false;
bool bBHTestPrepareCrashRecovery = false;
bool bBHTestRestoreCrashRecovery = false;
bool bBHTestRenderedTraversalIssued = false;
bool bBHTestRenderedUIReviewIssued = false;
bool bBHTestSquadPingScreenshotIssued = false;
bool bBHTestWatercraftDeliveryIssued = false;
bool bBHTestCivilianAidDeliveryIssued = false;
bool bBHTestInventoryTransferIssued = false;
bool bBHTestMissionCacheTransferIssued = false;
bool bBHTestArmoredThreatTargetingIssued = false;
bool bBHTestWeaponRoleRuntimeIssued = false;
bool bBHTestWorldKitModuleIssued = false;
bool bBHTestOperationFailureIssued = false;
int32 BHTestMissionCacheTransferPhase = 0;
int32 BHTestArmoredThreatTargetingPhase = 0;
int32 BHTestArmoredThreatTargetingAttempts = 0;
FVector BHTestArmoredThreatTargetingFirstFarLocation = FVector::ZeroVector;
FVector BHTestArmoredThreatTargetingSecondNearLocation = FVector::ZeroVector;
FVector BHTestArmoredThreatTargetingFirstNearLocation = FVector::ZeroVector;
FVector BHTestArmoredThreatTargetingSecondOutOfRangeLocation = FVector::ZeroVector;
int32 BHTestDefenseAGarrisonPhase = 0;
int32 BHTestDefenseAGarrisonWaitAttempts = 0;
FName BHTestDefenseAGarrisonCasualtyID = NAME_None;
bool bBHTestDefenseAWave2Spawned = false;
FName BHTestTransportPersistenceID = NAME_None;
}
#endif

ABHWarGameState::ABHWarGameState()
{
    bReplicates = true;
    SetNetUpdateFrequency(2.0f);
    SetMinNetUpdateFrequency(1.0f);
}

void ABHWarGameState::GetLifetimeReplicatedProps(
    TArray<FLifetimeProperty>& OutLifetimeProps
) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME_CONDITION_NOTIFY(
        ABHWarGameState,
        WarStateSnapshot,
        COND_None,
        REPNOTIFY_Always
    );
    DOREPLIFETIME_CONDITION_NOTIFY(
        ABHWarGameState,
        ActiveOperationSnapshot,
        COND_None,
        REPNOTIFY_Always
    );
    DOREPLIFETIME(ABHWarGameState, SquadPingSnapshot);
}

bool ABHWarGameState::HasAuthoritativeWarState() const
{
    return WarStateSnapshot.bInitialized;
}

int32 ABHWarGameState::GetWarStateRevision() const
{
    return WarStateSnapshot.Revision;
}

int32 ABHWarGameState::GetReplicatedWarTurn() const
{
    return WarStateSnapshot.TurnNumber;
}

FBHActiveOperationSnapshot
ABHWarGameState::GetActiveOperationSnapshot() const
{
    return ActiveOperationSnapshot;
}

FBHActiveOperationSnapshotChangedSignature&
ABHWarGameState::OnActiveOperationSnapshotChanged()
{
    return ActiveOperationSnapshotChanged;
}

void ABHWarGameState::PublishActiveOperationSnapshot(
    const FBHActiveOperationSnapshot& NewSnapshot
)
{
    if (!HasAuthority())
    {
        return;
    }

    UBHWarSubsystem* WarSubsystem = ResolveWarSubsystem();

    if (!IsValid(WarSubsystem))
    {
        return;
    }

    BoundWarSubsystem = WarSubsystem;
    ActiveOperationSnapshot = NewSnapshot;
    ActiveOperationSnapshot.Revision =
        WarSubsystem->AllocateReplicatedSnapshotRevision();
    ForceNetUpdate();
    BroadcastActiveOperationSnapshotChanged();

    UE_LOG(
        LogTemp,
        VeryVerbose,
        TEXT(
            "BH_ACTIVE_OPERATION_SNAPSHOT_PUBLISHED "
            "revision=%d operation_id=%s sector=%s phase=%d"
        ),
        ActiveOperationSnapshot.Revision,
        *ActiveOperationSnapshot.OperationID.ToString(),
        *ActiveOperationSnapshot.SectorID.ToString(),
        static_cast<int32>(ActiveOperationSnapshot.Phase)
    );
}

FBHSquadPingSnapshot ABHWarGameState::GetSquadPingSnapshot() const
{
    return SquadPingSnapshot;
}

void ABHWarGameState::PublishSquadPing(
    const FVector& Location,
    FName ContextLabel,
    FName IssuerLabel,
    float LifetimeSeconds,
    AActor* TrackedActor
)
{
    if (!HasAuthority() || ContextLabel.IsNone())
    {
        return;
    }

    SquadPingSnapshot.Revision = FMath::Max(
        1,
        SquadPingSnapshot.Revision + 1
    );
    SquadPingSnapshot.Location = Location;
    SquadPingSnapshot.TrackedActor =
        IsValid(TrackedActor) && TrackedActor->GetIsReplicated()
            ? TrackedActor
            : nullptr;
    SquadPingSnapshot.ContextLabel = ContextLabel;
    SquadPingSnapshot.IssuerLabel = IssuerLabel.IsNone()
        ? FName(TEXT("SQUAD"))
        : IssuerLabel;
    SquadPingSnapshot.ExpiryServerWorldTimeSeconds =
        GetServerWorldTimeSeconds() + FMath::Max(1.0f, LifetimeSeconds);
    ForceNetUpdate();

    UE_LOG(
        LogTemp,
        Display,
        TEXT(
            "BH_SQUAD_PING_PUBLISHED revision=%d issuer=%s "
            "context=%s location=%s tracked=%s expiry=%.2f"
        ),
        SquadPingSnapshot.Revision,
        *SquadPingSnapshot.IssuerLabel.ToString(),
        *SquadPingSnapshot.ContextLabel.ToString(),
        *FVector(SquadPingSnapshot.Location).ToCompactString(),
        IsValid(SquadPingSnapshot.TrackedActor)
            ? *SquadPingSnapshot.TrackedActor->GetName()
            : TEXT("none"),
        SquadPingSnapshot.ExpiryServerWorldTimeSeconds
    );
}

void ABHWarGameState::ClearActiveOperationSnapshot()
{
    if (!HasAuthority())
    {
        return;
    }

    UBHWarSubsystem* WarSubsystem = ResolveWarSubsystem();

    if (!IsValid(WarSubsystem))
    {
        return;
    }

    BoundWarSubsystem = WarSubsystem;
    ActiveOperationSnapshot = FBHActiveOperationSnapshot();
    ActiveOperationSnapshot.Revision =
        WarSubsystem->AllocateReplicatedSnapshotRevision();
    ForceNetUpdate();
    BroadcastActiveOperationSnapshotChanged();
}

void ABHWarGameState::BeginPlay()
{
    Super::BeginPlay();

    BoundWarSubsystem = ResolveWarSubsystem();

    if (HasAuthority())
    {
        if (IsValid(BoundWarSubsystem))
        {
            BoundWarSubsystem->OnWarStateChanged.RemoveDynamic(
                this,
                &ABHWarGameState::HandleWarStateChanged
            );
            BoundWarSubsystem->OnWarStateChanged.AddDynamic(
                this,
                &ABHWarGameState::HandleWarStateChanged
            );
#if !UE_BUILD_SHIPPING
            if (FParse::Param(
                    FCommandLine::Get(),
                    TEXT("BHTestWorldKitModuleRuntime")))
            {
                FTimerHandle WorldKitModuleTimer;
                GetWorldTimerManager().SetTimer(
                    WorldKitModuleTimer,
                    FTimerDelegate::CreateWeakLambda(
                        this,
                        [this]()
                        {
                            if (bBHTestWorldKitModuleIssued)
                            {
                                return;
                            }

                            ABHCharacter* Player = nullptr;
                            for (TActorIterator<ABHCharacter> It(GetWorld());
                                 It;
                                 ++It)
                            {
                                if (It->HasAuthority() &&
                                    It->IsPlayerControlled())
                                {
                                    Player = *It;
                                    break;
                                }
                            }
                            if (!IsValid(Player))
                            {
                                return;
                            }

                            EBHWorldKitModuleType ModuleType =
                                EBHWorldKitModuleType::Shelter;
                            FString RequestedType;
                            FParse::Value(
                                FCommandLine::Get(),
                                TEXT("BHTestWorldKitModuleType="),
                                RequestedType
                            );
                            if (RequestedType.Equals(
                                    TEXT("Checkpoint"),
                                    ESearchCase::IgnoreCase))
                            {
                                ModuleType =
                                    EBHWorldKitModuleType::Checkpoint;
                            }
                            else if (RequestedType.Equals(
                                         TEXT("Depot"),
                                         ESearchCase::IgnoreCase))
                            {
                                ModuleType = EBHWorldKitModuleType::Depot;
                            }
                            else if (RequestedType.Equals(
                                         TEXT("Base"),
                                         ESearchCase::IgnoreCase))
                            {
                                ModuleType =
                                    EBHWorldKitModuleType::ResistanceBase;
                            }

                            const FVector SpawnLocation =
                                Player->GetActorLocation() +
                                Player->GetActorForwardVector() * 900.0f;
                            ABHWorldKitModule* Module =
                                GetWorld()->SpawnActor<ABHWorldKitModule>(
                                    ABHWorldKitModule::StaticClass(),
                                    SpawnLocation,
                                    Player->GetActorRotation()
                                );
                            if (IsValid(Module))
                            {
                                Module->SetPersistenceIDForTesting(
                                    FName(TEXT("BHTestWorldKitRuntime01"))
                                );
                                Module->SetModuleTypeForTesting(ModuleType);
                                bBHTestWorldKitModuleIssued = true;
                            }
                            UE_LOG(
                                LogTemp,
                                Display,
                                TEXT(
                                    "BH_TEST_WORLD_KIT_RUNTIME result=%s "
                                    "id=%s type=%d"
                                ),
                                IsValid(Module) ? TEXT("success") : TEXT("failure"),
                                IsValid(Module)
                                    ? *Module->GetPersistenceID().ToString()
                                    : TEXT("None"),
                                IsValid(Module)
                                    ? static_cast<int32>(ModuleType)
                                    : -1
                            );
                        }
                    ),
                    0.5f,
                    true
                );
            }

            if (FParse::Param(
                    FCommandLine::Get(),
                    TEXT("BHTestWatercraftDeliveryWhenOccupied")))
            {
                FTimerHandle WatercraftDeliveryTimer;
                GetWorldTimerManager().SetTimer(
                    WatercraftDeliveryTimer,
                    FTimerDelegate::CreateWeakLambda(
                        this,
                        [this]()
                        {
                            if (bBHTestWatercraftDeliveryIssued)
                            {
                                return;
                            }

                            ABHCharacter* Player = nullptr;
                            for (TActorIterator<ABHCharacter> It(GetWorld());
                                 It;
                                 ++It)
                            {
                                if (It->HasAuthority() &&
                                    It->IsPlayerControlled())
                                {
                                    Player = *It;
                                    break;
                                }
                            }

                            ABHFieldTransport* Transport = nullptr;
                            for (TActorIterator<ABHFieldTransport> It(
                                     GetWorld());
                                 It;
                                 ++It)
                            {
                                if (IsValid(*It) &&
                                    It->GetPersistenceID() ==
                                        FName(TEXT("FirstLightWatercraft01")))
                                {
                                    Transport = *It;
                                    break;
                                }
                            }

                            ABHSectorResupplyStation* Station = nullptr;
                            for (TActorIterator<ABHSectorResupplyStation> It(
                                     GetWorld());
                                 It;
                                 ++It)
                            {
                                if (IsValid(*It) &&
                                    It->GetSectorID() ==
                                        FName(TEXT("EasternDepot")))
                                {
                                    Station = *It;
                                    break;
                                }
                            }

                            if (!IsValid(Player) || !IsValid(Transport))
                            {
                                return;
                            }

                            if (!IsValid(Station))
                            {
                                Station = GetWorld()->SpawnActor<
                                    ABHSectorResupplyStation>(
                                    ABHSectorResupplyStation::StaticClass(),
                                    Transport->GetActorLocation(),
                                    FRotator::ZeroRotator
                                );
                                if (IsValid(Station))
                                {
                                    Station->ConfigureStation(
                                        FName(TEXT("EasternDepot"))
                                    );
                                }
                            }

                            if (!IsValid(Station))
                            {
                                return;
                            }

                            BoundWarSubsystem->SetSectorOwnerForTesting(
                                FName(TEXT("EasternDepot")),
                                EBHWarFaction::Friendly
                            );
                            BoundWarSubsystem->SetSectorSupplyForTesting(
                                FName(TEXT("EasternDepot")),
                                80.0f
                            );

                            IBHInteractable::Execute_Interact(
                                Transport,
                                Player
                            );
                            Transport->SetActorLocation(
                                Station->GetActorLocation() +
                                FVector(0.0f, 0.0f, 40.0f)
                            );
                            Transport->ExecuteLogisticsTransferForTesting();
                            bBHTestWatercraftDeliveryIssued = true;
                            UE_LOG(
                                LogTemp,
                                Display,
                                TEXT(
                                    "BH_TEST_WATERCRAFT_DELIVERY_OCCUPIED "
                                    "id=%s remaining=%.1f occupant=%d"
                                ),
                                *Transport->GetPersistenceID().ToString(),
                                Transport->GetCargoSupply(),
                                IsValid(Transport->GetOccupant()) ? 1 : 0
                            );
                        }
                    ),
                    0.5f,
                    true
                );
            }

            if (FParse::Param(
                    FCommandLine::Get(),
                    TEXT("BHTestCivilianAidDeliveryWhenOccupied")))
            {
                FTimerHandle CivilianAidDeliveryTimer;
                GetWorldTimerManager().SetTimer(
                    CivilianAidDeliveryTimer,
                    FTimerDelegate::CreateWeakLambda(
                        this,
                        [this]()
                        {
                            if (bBHTestCivilianAidDeliveryIssued)
                            {
                                return;
                            }

                            ABHCharacter* Player = nullptr;
                            for (TActorIterator<ABHCharacter> It(GetWorld());
                                 It;
                                 ++It)
                            {
                                if (It->HasAuthority() &&
                                    It->IsPlayerControlled())
                                {
                                    Player = *It;
                                    break;
                                }
                            }

                            ABHFieldTransport* Transport = nullptr;
                            for (TActorIterator<ABHFieldTransport> It(
                                     GetWorld());
                                 It;
                                 ++It)
                            {
                                if (IsValid(*It) &&
                                    It->GetPersistenceID() ==
                                        FName(TEXT("FirstLightWatercraft01")))
                                {
                                    Transport = *It;
                                    break;
                                }
                            }

                            if (!IsValid(Player) ||
                                !IsValid(Transport) ||
                                !IsValid(BoundWarSubsystem))
                            {
                                return;
                            }

                            const FName SourceSectorID =
                                FName(TEXT("WesternFOB"));
                            BoundWarSubsystem->ResetCampaign();
                            BoundWarSubsystem->SetSectorOwnerForTesting(
                                SourceSectorID,
                                EBHWarFaction::Friendly
                            );
                            BoundWarSubsystem->SetSectorSupplyForTesting(
                                SourceSectorID,
                                80.0f
                            );

                            const FVector FixtureSourceLocation =
                                Transport->GetActorLocation() +
                                FVector(5000.0f, 0.0f, 0.0f);

                            ABHSectorResupplyStation* SourceStation = nullptr;
                            for (TActorIterator<ABHSectorResupplyStation> It(
                                     GetWorld());
                                 It;
                                 ++It)
                            {
                                if (IsValid(*It) &&
                                    It->GetSectorID() == SourceSectorID)
                                {
                                    SourceStation = *It;
                                    break;
                                }
                            }

                            if (!IsValid(SourceStation))
                            {
                                SourceStation = GetWorld()->SpawnActor<
                                    ABHSectorResupplyStation>(
                                    ABHSectorResupplyStation::StaticClass(),
                                    FixtureSourceLocation,
                                    FRotator::ZeroRotator
                                );
                                if (IsValid(SourceStation))
                                {
                                    SourceStation->ConfigureStation(
                                        SourceSectorID
                                    );
                                }
                            }

                            if (!IsValid(SourceStation))
                            {
                                return;
                            }

                            SourceStation->SetActorLocation(
                                FixtureSourceLocation,
                                false
                            );

                            Transport->ConfigureAuthoredCargo(
                                0.0f,
                                NAME_None,
                                NAME_None
                            );
                            IBHInteractable::Execute_Interact(
                                Transport,
                                Player
                            );
                            if (Transport->GetOccupant() != Player)
                            {
                                bBHTestCivilianAidDeliveryIssued = true;
                                UE_LOG(
                                    LogTemp,
                                    Error,
                                    TEXT(
                                        "BH_TEST_CIVILIAN_AID_DELIVERY_"
                                        "OCCUPIED result=failure stage=board"
                                    )
                                );
                                return;
                            }

                            Transport->SetActorLocation(
                                SourceStation->GetActorLocation() +
                                FVector(0.0f, 0.0f, 40.0f)
                            );
                            Transport->ExecuteCivilianAidTransferForTesting();

                            const FName DestinationSectorID =
                                Transport->GetCargoDestinationSectorID();
                            const bool bLoaded =
                                Transport->GetCargoType() ==
                                    EBHWarConvoyCargoType::CivilianAid &&
                                Transport->GetCargoSupply() >
                                    KINDA_SMALL_NUMBER &&
                                !DestinationSectorID.IsNone();
                            if (!bLoaded)
                            {
                                bBHTestCivilianAidDeliveryIssued = true;
                                UE_LOG(
                                    LogTemp,
                                    Error,
                                    TEXT(
                                        "BH_TEST_CIVILIAN_AID_DELIVERY_"
                                        "OCCUPIED result=failure stage=load "
                                        "cargo=%.1f destination=%s"
                                    ),
                                    Transport->GetCargoSupply(),
                                    *DestinationSectorID.ToString()
                                );
                                return;
                            }

                            const FBHWarSectorState DestinationBefore =
                                BoundWarSubsystem->GetSectorState(
                                    DestinationSectorID
                                );
                            ABHSectorResupplyStation* DestinationStation =
                                nullptr;
                            for (TActorIterator<ABHSectorResupplyStation> It(
                                     GetWorld());
                                 It;
                                 ++It)
                            {
                                if (IsValid(*It) &&
                                    It->GetSectorID() == DestinationSectorID)
                                {
                                    DestinationStation = *It;
                                    break;
                                }
                            }

                            if (!IsValid(DestinationStation))
                            {
                                DestinationStation = GetWorld()->SpawnActor<
                                    ABHSectorResupplyStation>(
                                    ABHSectorResupplyStation::StaticClass(),
                                    FixtureSourceLocation +
                                        FVector(1200.0f, 0.0f, 0.0f),
                                    FRotator::ZeroRotator
                                );
                                if (IsValid(DestinationStation))
                                {
                                    DestinationStation->ConfigureStation(
                                        DestinationSectorID
                                    );
                                }
                            }

                            if (!IsValid(DestinationStation))
                            {
                                bBHTestCivilianAidDeliveryIssued = true;
                                UE_LOG(
                                    LogTemp,
                                    Error,
                                    TEXT(
                                        "BH_TEST_CIVILIAN_AID_DELIVERY_"
                                        "OCCUPIED result=failure "
                                        "stage=destination_station"
                                    )
                                );
                                return;
                            }

                            DestinationStation->SetActorLocation(
                                FixtureSourceLocation +
                                FVector(1200.0f, 0.0f, 0.0f),
                                false
                            );

                            Transport->SetActorLocation(
                                DestinationStation->GetActorLocation() +
                                FVector(0.0f, 0.0f, 40.0f)
                            );
                            Transport->ExecuteCivilianAidTransferForTesting();

                            const FBHWarSectorState DestinationAfter =
                                BoundWarSubsystem->GetSectorState(
                                    DestinationSectorID
                                );
                            const bool bDelivered =
                                Transport->GetCargoSupply() <=
                                    KINDA_SMALL_NUMBER &&
                                Transport->GetCargoType() ==
                                    EBHWarConvoyCargoType::MilitarySupply &&
                                DestinationAfter.CivilianSupport >
                                    DestinationBefore.CivilianSupport;
                            bBHTestCivilianAidDeliveryIssued = true;
                            UE_LOG(
                                LogTemp,
                                Display,
                                TEXT(
                                    "BH_TEST_CIVILIAN_AID_DELIVERY_OCCUPIED "
                                    "result=%s id=%s source=%s destination=%s "
                                    "remaining=%.1f occupant=%d "
                                    "support=%.1f->%.1f"
                                ),
                                bDelivered ? TEXT("success") : TEXT("failure"),
                                *Transport->GetPersistenceID().ToString(),
                                *SourceSectorID.ToString(),
                                *DestinationSectorID.ToString(),
                                Transport->GetCargoSupply(),
                                IsValid(Transport->GetOccupant()) ? 1 : 0,
                                DestinationBefore.CivilianSupport,
                                DestinationAfter.CivilianSupport
                            );
                        }
                    ),
                    0.5f,
                    true
                );
            }

            if (FParse::Param(
                    FCommandLine::Get(),
                    TEXT("BHTestInventoryTransferRuntime")))
            {
                GetWorldTimerManager().SetTimer(
                    InventoryTransferRuntimeTestTimer,
                    this,
                    &ABHWarGameState::RunInventoryTransferRuntimeTest,
                    0.5f,
                    true
                );
            }

            if (FParse::Param(
                    FCommandLine::Get(),
                    TEXT("BHTestMissionCacheTransferRuntime")))
            {
                GetWorldTimerManager().SetTimer(
                    MissionCacheTransferRuntimeTestTimer,
                    this,
                    &ABHWarGameState::RunMissionCacheTransferRuntimeTest,
                    0.5f,
                    true
                );
            }

            if (FParse::Param(
                    FCommandLine::Get(),
                    TEXT("BHTestArmoredThreatTargetingRuntime")))
            {
                GetWorldTimerManager().SetTimer(
                    ArmoredThreatTargetSelectionRuntimeTestTimer,
                    this,
                    &ABHWarGameState::RunArmoredThreatTargetSelectionRuntimeTest,
                    0.5f,
                    true
                );
            }

            if (FParse::Param(
                    FCommandLine::Get(),
                    TEXT("BHTestWeaponRoleRuntime")))
            {
                GetWorldTimerManager().SetTimer(
                    WeaponRoleRuntimeTestTimer,
                    this,
                    &ABHWarGameState::RunWeaponRoleRuntimeTest,
                    0.5f,
                    true
                );
            }

            if (FParse::Param(
                    FCommandLine::Get(),
                    TEXT("BHTestNavigationGrenade")))
            {
                FTimerHandle NavigationGrenadeSpawnTimer;
                GetWorldTimerManager().SetTimer(
                    NavigationGrenadeSpawnTimer,
                    FTimerDelegate::CreateWeakLambda(
                        this,
                        [this]()
                        {
                            ABHCharacter* Player = nullptr;
                            for (TActorIterator<ABHCharacter> It(GetWorld());
                                 It;
                                 ++It)
                            {
                                if (It->HasAuthority() &&
                                    It->IsPlayerControlled())
                                {
                                    Player = *It;
                                    break;
                                }
                            }

                            ABHFragGrenade* Grenade = nullptr;
                            if (IsValid(Player))
                            {
                                const FVector SpawnLocation =
                                    Player->GetActorLocation() +
                                    Player->GetActorForwardVector() * 120.0f +
                                    FVector(0.0f, 0.0f, 70.0f);
                                FActorSpawnParameters SpawnParameters;
                                SpawnParameters.Owner = Player;
                                SpawnParameters.Instigator = Player;
                                SpawnParameters.SpawnCollisionHandlingOverride =
                                    ESpawnActorCollisionHandlingMethod::
                                        AlwaysSpawn;
                                Grenade = GetWorld()->SpawnActor<
                                    ABHFragGrenade>(
                                    ABHFragGrenade::StaticClass(),
                                    SpawnLocation,
                                    Player->GetActorRotation(),
                                    SpawnParameters
                                );
                            }

                            if (IsValid(Grenade))
                            {
                                Grenade->Throw(
                                    Player->GetActorForwardVector() * 1200.0f +
                                    FVector(0.0f, 0.0f, 240.0f)
                                );
                            }

                            UE_LOG(
                                LogTemp,
                                Display,
                                TEXT(
                                    "BH_TEST_NAVIGATION_GRENADE result=%s"
                                ),
                                IsValid(Grenade)
                                    ? TEXT("success")
                                    : TEXT("failure")
                            );
                        }
                    ),
                    2.0f,
                    false
                );

                FTimerHandle NavigationGrenadeExitTimer;
                GetWorldTimerManager().SetTimer(
                    NavigationGrenadeExitTimer,
                    FTimerDelegate::CreateWeakLambda(
                        this,
                        []()
                        {
                            UE_LOG(
                                LogTemp,
                                Display,
                                TEXT(
                                    "BH_TEST_NAVIGATION_GRENADE_COMPLETE"
                                )
                            );
                            FPlatformMisc::RequestExit(false);
                        }
                    ),
                    FParse::Param(
                        FCommandLine::Get(),
                        TEXT("BHTestFirstLightPlayableRoute"))
                        ? 30.0f
                        : 8.0f,
                    false
                );
            }

            if (FParse::Param(
                    FCommandLine::Get(),
                    TEXT("BHTestFirstLightPlayableRoute")))
            {
                float FirstLightPlayableRouteDelaySeconds =
                    FParse::Param(
                        FCommandLine::Get(),
                        TEXT("BHTestNavigationGrenade"))
                    ? 9.0f
                    : 2.0f;
                FParse::Value(
                    FCommandLine::Get(),
                    TEXT("BHTestFirstLightPlayableRouteAfterSeconds="),
                    FirstLightPlayableRouteDelaySeconds
                );
                GetWorldTimerManager().SetTimer(
                    FirstLightPlayableRouteTestTimer,
                    this,
                    &ABHWarGameState::RunFirstLightPlayableRouteTest,
                    0.5f,
                    true,
                    FMath::Max(
                        1.0f,
                        FirstLightPlayableRouteDelaySeconds
                    )
                );
            }

            if (FParse::Param(
                    FCommandLine::Get(),
                    TEXT("BHTestDefenseAGarrisonPersistence")))
            {
                GetWorldTimerManager().SetTimer(
                    DefenseAGarrisonPersistenceTestTimer,
                    this,
                    &ABHWarGameState::RunDefenseAGarrisonPersistenceTest,
                    0.5f,
                    true,
                    2.0f
                );
            }

            if (FParse::Param(
                    FCommandLine::Get(),
                    TEXT("BHTestOperationFailure")))
            {
                GetWorldTimerManager().SetTimer(
                    OperationFailureTestTimer,
                    this,
                    &ABHWarGameState::RunOperationFailureTest,
                    0.5f,
                    true,
                    1.0f
                );
            }

            if (bBHTestServerTravelIssued &&
                !bBHTestServerTravelCompleted)
            {
                bBHTestServerTravelCompleted = true;
                UE_LOG(
                    LogTemp,
                    Display,
                    TEXT(
                        "BH_TEST_SERVER_TRAVEL_COMPLETE map=%s "
                        "operation_id=%s"
                    ),
                    *GetWorld()->GetOutermost()->GetName(),
                    *BoundWarSubsystem
                        ->GetCommittedOperationID()
                        .ToString()
                );

                if (bBHTestTransportTravelPersistence)
                {
                    bool bHasDestinationTransport = false;
                    for (TActorIterator<ABHFieldTransport> It(GetWorld());
                         It;
                         ++It)
                    {
                        if (It->GetPersistenceID() ==
                            BHTestTransportPersistenceID)
                        {
                            bHasDestinationTransport = true;
                            break;
                        }
                    }

                    if (!bHasDestinationTransport)
                    {
                        ABHFieldTransport* DestinationTransport =
                            GetWorld()->SpawnActor<ABHFieldTransport>(
                            ABHFieldTransport::StaticClass(),
                            FTransform::Identity
                        );
                        if (IsValid(DestinationTransport))
                        {
                            DestinationTransport->SetPersistenceIDForTesting(
                                BHTestTransportPersistenceID
                            );
                        }
                    }

                    FTimerHandle VerifyTransportTimer;
                    GetWorldTimerManager().SetTimer(
                        VerifyTransportTimer,
                        FTimerDelegate::CreateWeakLambda(
                            this,
                            [this]()
                            {
                                UWorld* World = GetWorld();
                                ABHFieldTransport* RestoredTransport =
                                    nullptr;
                                for (TActorIterator<ABHFieldTransport> It(
                                        World);
                                     It;
                                     ++It)
                                {
                                    if (It->GetPersistenceID() ==
                                        BHTestTransportPersistenceID)
                                    {
                                        RestoredTransport = *It;
                                        break;
                                    }
                                }

                                ABHCharacter* Driver = IsValid(
                                    RestoredTransport)
                                    ? RestoredTransport->GetOccupant()
                                    : nullptr;
                                const int32 RestoredIncapacitatedCount =
                                    IsValid(Driver)
                                        ? Driver
                                            ->GetIncapacitatedFieldSquadCount()
                                        : 0;
                                const bool bCasualtyRestored =
                                    !bBHTestTransportCasualtyPersistence ||
                                    RestoredIncapacitatedCount == 1;
                                const bool bRestored =
                                    IsValid(RestoredTransport) &&
                                    IsValid(Driver) &&
                                    FMath::IsNearlyEqual(
                                        RestoredTransport
                                            ->GetCargoSupply(),
                                        9.0f,
                                        0.1f) &&
                                    Driver->IsFieldSquadEmbarked() &&
                                    Driver->GetLivingFieldSquadCount() == 2 &&
                                    bCasualtyRestored;

                                if (bBHTestTransportCasualtyPersistence)
                                {
                                    UE_LOG(
                                        LogTemp,
                                        Display,
                                        TEXT(
                                            "BH_TEST_TRANSPORT_CASUALTY_"
                                            "RESTORED result=%s "
                                            "living=%d incapacitated=%d "
                                            "rescue_target=%s"
                                        ),
                                        bCasualtyRestored
                                            ? TEXT("success")
                                            : TEXT("failure"),
                                        IsValid(Driver)
                                            ? Driver
                                                ->GetLivingFieldSquadCount()
                                            : -1,
                                        RestoredIncapacitatedCount,
                                        IsValid(Driver)
                                            ? *Driver
                                                ->GetFieldSquadRescueTargetID()
                                                .ToString()
                                            : TEXT("None")
                                    );
                                }

                                if (bRestored &&
                                    bBHTestOperationCompletionAfterTravel)
                                {
                                    ABHOpenWorldOperationDirector* Director =
                                        nullptr;
                                    for (TActorIterator<
                                             ABHOpenWorldOperationDirector>
                                             It(World);
                                         It;
                                         ++It)
                                    {
                                        if (It->IsOperationInProgress())
                                        {
                                            Director = *It;
                                            break;
                                        }
                                    }

                                    if (IsValid(Director))
                                    {
                                        const EBHWarPriorityType
                                            CompletedOperationType =
                                                BoundWarSubsystem
                                                    ->GetCommittedOperationType();
                                        Director
                                            ->CompleteOperationForTesting();
                                        const bool bCompletionRestored =
                                            Driver->IsMissionComplete() &&
                                            !BoundWarSubsystem
                                                ->HasCommittedOperation();
                                        UE_LOG(
                                            LogTemp,
                                            Display,
                                            TEXT(
                                                "BH_TEST_OPERATION_COMPLETED_"
                                                "AFTER_TRAVEL result=%s "
                                                "mission_complete=%d "
                                                "operation_active=%d type=%d"
                                            ),
                                            bCompletionRestored
                                                ? TEXT("success")
                                                : TEXT("failure"),
                                            Driver->IsMissionComplete()
                                                ? 1
                                                : 0,
                                            BoundWarSubsystem
                                                    ->HasCommittedOperation()
                                                ? 1
                                                : 0,
                                            static_cast<int32>(
                                                CompletedOperationType)
                                        );
                                    }
                                }

                                UE_LOG(
                                    LogTemp,
                                    Display,
                                    TEXT(
                                        "BH_TEST_TRANSPORT_TRAVEL_RESTORED "
                                        "result=%s id=%s cargo=%.1f fuel=%.2f "
                                        "hull=%.2f driver=%s passengers=%d"
                                    ),
                                    bRestored
                                        ? TEXT("success")
                                        : TEXT("failure"),
                                    *BHTestTransportPersistenceID.ToString(),
                                    IsValid(RestoredTransport)
                                        ? RestoredTransport->GetCargoSupply()
                                        : -1.0f,
                                    IsValid(RestoredTransport)
                                        ? RestoredTransport
                                            ->GetFuelPercentage()
                                        : -1.0f,
                                    IsValid(RestoredTransport)
                                        ? RestoredTransport
                                            ->GetHullPercentage()
                                        : -1.0f,
                                    *GetNameSafe(Driver),
                                    IsValid(Driver)
                                        ? Driver
                                            ->GetLivingFieldSquadCount()
                                        : -1
                                );
                            }
                        ),
                        1.0f,
                        false
                    );
                }
            }

            if (FParse::Param(
                    FCommandLine::Get(),
                    TEXT("BHTestCommitPriorityOperation")) &&
                !BoundWarSubsystem->HasCommittedOperation())
            {
                if (FParse::Param(
                        FCommandLine::Get(),
                        TEXT("BHTestWatercraftDeliveryWhenOccupied")))
                {
                    BoundWarSubsystem->SetSectorOwnerForTesting(
                        FName(TEXT("EasternDepot")),
                        EBHWarFaction::Friendly
                    );
                }
                FString RequestedOperationType;
                FParse::Value(
                    FCommandLine::Get(),
                    TEXT("BHTestOperationType="),
                    RequestedOperationType
                );
                EBHWarPriorityType OperationType =
                    BoundWarSubsystem->GetPriorityType();
                if (RequestedOperationType.Equals(
                        TEXT("Attack"),
                        ESearchCase::IgnoreCase))
                {
                    OperationType = EBHWarPriorityType::Attack;
                    if (FParse::Param(
                            FCommandLine::Get(),
                            TEXT("BHTestForceAttackTarget")))
                    {
                        // NorthPass is the canonical First Light checkpoint;
                        // keep Dovren as its friendly supply source for the
                        // isolated active-operation fixture.
                        BoundWarSubsystem->SetSectorOwnerForTesting(
                            FName(TEXT("NorthPass")),
                            EBHWarFaction::Enemy
                        );
                        BoundWarSubsystem->SetSectorOwnerForTesting(
                            FName(TEXT("DovrenVillage")),
                            EBHWarFaction::Friendly
                        );
                        BoundWarSubsystem->SetSectorSupplyForTesting(
                            FName(TEXT("DovrenVillage")),
                            80.0f
                        );
                    }
                }
                else if (RequestedOperationType.Equals(
                             TEXT("Defend"),
                             ESearchCase::IgnoreCase))
                {
                    OperationType = EBHWarPriorityType::Defend;
                }
                else if (RequestedOperationType.Equals(
                             TEXT("Raid"),
                             ESearchCase::IgnoreCase))
                {
                    OperationType = EBHWarPriorityType::Raid;
                }
                else if (RequestedOperationType.Equals(
                             TEXT("Resupply"),
                             ESearchCase::IgnoreCase))
                {
                    OperationType = EBHWarPriorityType::Resupply;
                }
                else if (RequestedOperationType.Equals(
                             TEXT("Recon"),
                             ESearchCase::IgnoreCase))
                {
                    OperationType = EBHWarPriorityType::Recon;
                }

                FName OperationSectorID =
                    BoundWarSubsystem->GetPrioritySectorID();
                if (!RequestedOperationType.IsEmpty())
                {
                    OperationSectorID = NAME_None;
                    for (const FBHWarSectorState& Sector :
                         BoundWarSubsystem->GetSectorStates())
                    {
                        if (BoundWarSubsystem->IsViableOperation(
                                Sector.SectorID,
                                OperationType))
                        {
                            OperationSectorID = Sector.SectorID;
                            break;
                        }
                    }
                    if (OperationSectorID.IsNone() &&
                        FParse::Param(
                            FCommandLine::Get(),
                            TEXT("BHTestForceAttackTarget")) &&
                        OperationType == EBHWarPriorityType::Attack)
                    {
                        OperationSectorID = FName(TEXT("NorthPass"));
                    }
                }
                const bool bCommitted =
                    BoundWarSubsystem->SetCommittedOperation(
                        OperationSectorID,
                        OperationType
                    );
                UE_LOG(
                    LogTemp,
                    Display,
                    TEXT(
                        "BH_TEST_OPERATION_COMMIT result=%s id=%s "
                        "sector=%s type=%d"
                    ),
                    bCommitted ? TEXT("success") : TEXT("failure"),
                    *BoundWarSubsystem
                        ->GetCommittedOperationID()
                        .ToString(),
                    *OperationSectorID.ToString(),
                    static_cast<int32>(OperationType)
                );

                if (bCommitted &&
                    FParse::Param(
                        FCommandLine::Get(),
                        TEXT("BHTestBeginCommittedOperation")))
                {
                    // BeginOperationInWorld owns the real deployment commit;
                    // release the fixture commit first so the character path
                    // can perform its normal viability and supply checks.
                    BoundWarSubsystem->ClearCommittedOperation();
                    ABHCharacter* TestCharacter = nullptr;
                    for (TActorIterator<ABHCharacter> It(GetWorld()); It; ++It)
                    {
                        if (It->HasAuthority() && It->IsPlayerControlled())
                        {
                            TestCharacter = *It;
                            break;
                        }
                    }
                    if (IsValid(TestCharacter))
                    {
                        TestCharacter->PrepareDeploymentModeForTest();
                    }
                    const bool bStarted =
                        IsValid(TestCharacter) &&
                        TestCharacter->BeginOperationInWorld(
                            OperationSectorID,
                            OperationType
                        );
                    UE_LOG(
                        LogTemp,
                        Display,
                        TEXT(
                            "BH_TEST_BEGIN_COMMITTED_OPERATION result=%s "
                            "sector=%s type=%d"
                        ),
                        bStarted ? TEXT("success") : TEXT("failure"),
                        *OperationSectorID.ToString(),
                        static_cast<int32>(OperationType)
                    );
                    if (!bStarted)
                    {
                        FTimerHandle BeginOperationRetryTimer;
                        GetWorldTimerManager().SetTimer(
                            BeginOperationRetryTimer,
                            FTimerDelegate::CreateWeakLambda(
                                this,
                                [this, OperationSectorID, OperationType]()
                                {
                                    ABHCharacter* RetryCharacter = nullptr;
                                    for (TActorIterator<ABHCharacter> It(
                                             GetWorld());
                                         It;
                                         ++It)
                                    {
                                        if (It->HasAuthority() &&
                                            It->IsPlayerControlled())
                                        {
                                            RetryCharacter = *It;
                                            break;
                                        }
                                    }
                                    const bool bRetryStarted =
                                        IsValid(RetryCharacter) &&
                                        (RetryCharacter->SetActorLocation(
                                             FVector(6100.0f, 0.0f, 100.0f),
                                             false,
                                             nullptr,
                                             ETeleportType::TeleportPhysics
                                         ),
                                         RetryCharacter->PrepareDeploymentModeForTest(),
                                         RetryCharacter->BeginOperationInWorld(
                                             OperationSectorID,
                                             OperationType
                                         ));
                                    UE_LOG(
                                        LogTemp,
                                        Display,
                                        TEXT(
                                            "BH_TEST_BEGIN_COMMITTED_OPERATION_RETRY "
                                            "result=%s sector=%s type=%d"
                                        ),
                                        bRetryStarted
                                            ? TEXT("success")
                                            : TEXT("failure"),
                                        *OperationSectorID.ToString(),
                                        static_cast<int32>(OperationType)
                                    );
                                }
                            ),
                            6.0f,
                            false
                        );
                    }
                }
            }

            float TestTravelDelaySeconds = 0.0f;
            if (!bBHTestServerTravelIssued &&
                FParse::Value(
                    FCommandLine::Get(),
                    TEXT("BHTestServerTravelAfterSeconds="),
                    TestTravelDelaySeconds) &&
                TestTravelDelaySeconds > 0.0f)
            {
                const FString CurrentMap =
                    GetWorld()->GetOutermost()->GetName();
                const FString TravelMap =
                    FParse::Param(
                        FCommandLine::Get(),
                        TEXT("BHTestTransportTravelPersistence"))
                        ? TEXT("/Game/BrokenHorizon/Maps/L_BrokenHorizon_World")
                        : CurrentMap;
                FTimerHandle TestTravelTimer;
                GetWorldTimerManager().SetTimer(
                    TestTravelTimer,
                    FTimerDelegate::CreateWeakLambda(
                        this,
                        [this, TravelMap]()
                        {
                            UWorld* World = GetWorld();
                            if (!HasAuthority() || !IsValid(World))
                            {
                                return;
                            }

                            bBHTestServerTravelIssued = true;
                            UE_LOG(
                                LogTemp,
                                Display,
                                TEXT(
                                    "BH_TEST_SERVER_TRAVEL_BEGIN map=%s"
                                ),
                                *TravelMap
                            );
                            bBHTestTransportTravelPersistence =
                                FParse::Param(
                                    FCommandLine::Get(),
                                    TEXT(
                                        "BHTestTransportTravelPersistence"
                                    ));
                            bBHTestTransportCasualtyPersistence =
                                FParse::Param(
                                    FCommandLine::Get(),
                                    TEXT(
                                        "BHTestTransportCasualtyPersistence"
                                    )
                                );
                            bBHTestOperationCompletionAfterTravel =
                                FParse::Param(
                                    FCommandLine::Get(),
                                    TEXT(
                                        "BHTestOperationCompletionAfterTravel"
                                    ));
                            bBHTestPrepareCrashRecovery = FParse::Param(
                                FCommandLine::Get(),
                                TEXT("BHTestPrepareCrashRecovery")
                            );
                            bBHTestRestoreCrashRecovery = FParse::Param(
                                FCommandLine::Get(),
                                TEXT("BHTestRestoreCrashRecovery")
                            );

                            if (bBHTestTransportTravelPersistence)
                            {
                                if (bBHTestRestoreCrashRecovery)
                                {
                                    BHTestTransportPersistenceID =
                                        TEXT("WesternFOBFieldTransport01");
                                    UBHSaveSubsystem* SaveSubsystem =
                                        GetGameInstance()
                                            ? GetGameInstance()->GetSubsystem<
                                                UBHSaveSubsystem>()
                                            : nullptr;
                                    const bool bRecoveryTravelStarted =
                                        IsValid(SaveSubsystem) &&
                                        SaveSubsystem->LoadProgress();
                                    UE_LOG(
                                        LogTemp,
                                        Display,
                                        TEXT(
                                            "BH_TEST_CRASH_RECOVERY_LOAD "
                                            "result=%s"
                                        ),
                                        bRecoveryTravelStarted
                                            ? TEXT("success")
                                            : TEXT("failure")
                                    );
                                    return;
                                }

                                ABHCharacter* Character = nullptr;
                                ABHFieldTransport* Transport = nullptr;
                                for (TActorIterator<ABHCharacter> It(World);
                                     It;
                                     ++It)
                                {
                                    if (It->HasAuthority())
                                    {
                                        Character = *It;
                                        break;
                                    }
                                }
                                for (TActorIterator<ABHFieldTransport> It(
                                        World);
                                     It;
                                     ++It)
                                {
                                    if (!It->GetPersistenceID().IsNone())
                                    {
                                        Transport = *It;
                                        break;
                                    }
                                }

                                if (!IsValid(Transport) &&
                                    IsValid(Character))
                                {
                                    FTransform SpawnTransform(
                                        Character->GetActorRotation(),
                                        Character->GetActorLocation() +
                                            FVector(500.0f, 0.0f, 100.0f)
                                    );
                                    Transport = World->SpawnActor<
                                        ABHFieldTransport>(
                                        ABHFieldTransport::StaticClass(),
                                        SpawnTransform
                                    );
                                    if (IsValid(Transport))
                                    {
                                        Transport->SetPersistenceIDForTesting(
                                            TEXT("BHTestFieldTransport01")
                                        );
                                    }
                                }

                                UBHSaveSubsystem* SaveSubsystem =
                                    GetGameInstance()
                                        ? GetGameInstance()->GetSubsystem<
                                            UBHSaveSubsystem>()
                                        : nullptr;
                                FString RequestedOperationType;
                                FParse::Value(
                                    FCommandLine::Get(),
                                    TEXT("BHTestOperationType="),
                                    RequestedOperationType
                                );
                                EBHWarPriorityType TestOperationType =
                                    BoundWarSubsystem->GetPriorityType();
                                if (RequestedOperationType.Equals(
                                        TEXT("Attack"),
                                        ESearchCase::IgnoreCase))
                                {
                                    TestOperationType =
                                        EBHWarPriorityType::Attack;
                                }
                                else if (RequestedOperationType.Equals(
                                             TEXT("Defend"),
                                             ESearchCase::IgnoreCase))
                                {
                                    TestOperationType =
                                        EBHWarPriorityType::Defend;
                                }
                                else if (RequestedOperationType.Equals(
                                             TEXT("Raid"),
                                             ESearchCase::IgnoreCase))
                                {
                                    TestOperationType =
                                        EBHWarPriorityType::Raid;
                                }
                                else if (RequestedOperationType.Equals(
                                             TEXT("Resupply"),
                                             ESearchCase::IgnoreCase))
                                {
                                    TestOperationType =
                                        EBHWarPriorityType::Resupply;
                                }
                                else if (RequestedOperationType.Equals(
                                             TEXT("Recon"),
                                             ESearchCase::IgnoreCase))
                                {
                                    TestOperationType =
                                        EBHWarPriorityType::Recon;
                                }
                                FName TestOperationSectorID =
                                    BoundWarSubsystem->GetPrioritySectorID();
                                if (!RequestedOperationType.IsEmpty())
                                {
                                    TestOperationSectorID = NAME_None;
                                    for (const FBHWarSectorState& Sector :
                                         BoundWarSubsystem
                                             ->GetSectorStates())
                                    {
                                        if (BoundWarSubsystem
                                                ->IsViableOperation(
                                                    Sector.SectorID,
                                                    TestOperationType))
                                        {
                                            TestOperationSectorID =
                                                Sector.SectorID;
                                            break;
                                        }
                                    }
                                }
                                const bool bOperationPrepared =
                                    IsValid(Character) &&
                                    (Character->IsRuntimeWarOperation() ||
                                     Character->BeginOperationInWorld(
                                         TestOperationSectorID,
                                         TestOperationType));
                                const bool bSquadPrepared =
                                    bOperationPrepared &&
                                    Character->RestoreFieldSquadState(
                                        2,
                                        false) &&
                                    Character->GetLivingFieldSquadCount() ==
                                        2;
                                const bool bCasualtyPrepared =
                                    !bBHTestTransportCasualtyPersistence ||
                                    (
                                        bSquadPrepared &&
                                        Character
                                            ->PrepareFieldSquadCasualtyForTransportTest()
                                    );
                                const bool bPrepared =
                                    IsValid(Character) &&
                                    IsValid(Transport) &&
                                    IsValid(SaveSubsystem) &&
                                    bOperationPrepared &&
                                    bSquadPrepared &&
                                    bCasualtyPrepared;

                                if (FParse::Param(
                                        FCommandLine::Get(),
                                        TEXT(
                                            "BHTestCommitPriorityOperation")))
                                {
                                    UE_LOG(
                                        LogTemp,
                                        Display,
                                        TEXT(
                                            "BH_TEST_OPERATION_COMMIT "
                                            "result=%s id=%s sector=%s "
                                            "type=%d"
                                        ),
                                        bOperationPrepared &&
                                                BoundWarSubsystem
                                                    ->HasCommittedOperation()
                                            ? TEXT("success")
                                            : TEXT("failure"),
                                        *BoundWarSubsystem
                                            ->GetCommittedOperationID()
                                            .ToString(),
                                        *TestOperationSectorID.ToString(),
                                        static_cast<int32>(
                                            TestOperationType)
                                    );
                                }
                                UE_LOG(
                                    LogTemp,
                                    Display,
                                    TEXT(
                                        "BH_TEST_TRANSPORT_PREP_DIAGNOSTIC "
                                        "character=%d transport=%d save=%d "
                                        "operation=%d squad=%d"
                                    ),
                                    IsValid(Character) ? 1 : 0,
                                    IsValid(Transport) ? 1 : 0,
                                    IsValid(SaveSubsystem) ? 1 : 0,
                                    bOperationPrepared ? 1 : 0,
                                    bSquadPrepared ? 1 : 0
                                );

                                if (bPrepared)
                                {
                                    BHTestTransportPersistenceID =
                                        Transport->GetPersistenceID();
                                    const FTransform TestTransportTransform(
                                        Character->GetActorRotation(),
                                        Character->GetActorLocation() +
                                            FVector(200.0f, 0.0f, 50.0f)
                                    );
                                    Transport->RestorePersistentState(
                                        TestTransportTransform,
                                        Character,
                                        true,
                                        0.63f,
                                        0.71f,
                                        9.0f,
                                        TEXT("WesternFOB"),
                                        TEXT("DovrenVillage"),
                                        EBHWarConvoyCargoType::MilitarySupply,
                                        true,
                                        false
                                    );
                                }

                                const bool bCheckpointSaved =
                                    bPrepared &&
                                    SaveSubsystem->SaveProgress();
                                const bool bTravelStarted =
                                    bCheckpointSaved &&
                                    (bBHTestPrepareCrashRecovery ||
                                     SaveSubsystem->LoadProgress());
                                UE_LOG(
                                    LogTemp,
                                    Display,
                                    TEXT(
                                        "BH_TEST_TRANSPORT_TRAVEL_PREPARED "
                                        "result=%s id=%s cargo=9.0 "
                                        "passengers=2"
                                    ),
                                    bTravelStarted
                                        ? TEXT("success")
                                        : TEXT("failure"),
                                    *BHTestTransportPersistenceID.ToString()
                                );
                                if (bBHTestPrepareCrashRecovery)
                                {
                                    UE_LOG(
                                        LogTemp,
                                        Display,
                                        TEXT(
                                            "BH_TEST_CRASH_RECOVERY_PREPARED "
                                            "result=%s operation_id=%s"
                                        ),
                                        bCheckpointSaved
                                            ? TEXT("success")
                                            : TEXT("failure"),
                                        *BoundWarSubsystem
                                            ->GetCommittedOperationID()
                                            .ToString()
                                    );
                                }
                                return;
                            }

                            World->ServerTravel(TravelMap, true);
                        }
                    ),
                    FMath::Max(1.0f, TestTravelDelaySeconds),
                    false
                );
                UE_LOG(
                    LogTemp,
                    Display,
                    TEXT(
                        "BH_TEST_SERVER_TRAVEL_SCHEDULED delay=%.1f "
                        "map=%s"
                    ),
                    TestTravelDelaySeconds,
                    *TravelMap
                );
            }

            float FirstLightCompletionDelaySeconds = 0.0f;
            if (FParse::Value(
                    FCommandLine::Get(),
                    TEXT("BHTestFirstLightCompletionAfterSeconds="),
                    FirstLightCompletionDelaySeconds) &&
                FirstLightCompletionDelaySeconds > 0.0f)
            {
                FTimerHandle FirstLightCompletionTimer;
                GetWorldTimerManager().SetTimer(
                    FirstLightCompletionTimer,
                    FTimerDelegate::CreateWeakLambda(
                        this,
                        [this]()
                        {
                            TArray<ABHCharacter*> Players;
                            for (TActorIterator<ABHCharacter> It(GetWorld());
                                 It;
                                 ++It)
                            {
                                if (It->HasAuthority() &&
                                    It->IsPlayerControlled())
                                {
                                    Players.Add(*It);
                                }
                            }

                            bool bRouteCompleted = Players.Num() >= 2;
                            if (bRouteCompleted)
                            {
                                const TArray<FName> ObjectiveRoute =
                                {
                                    BHObjectiveIds::FindRedKeycard,
                                    BHObjectiveIds::UnlockSecurityDoor,
                                    BHObjectiveIds::EliminateGuard,
                                    BHObjectiveIds::ReachExtraction
                                };
                                for (const FName ObjectiveID :
                                     ObjectiveRoute)
                                {
                                    if (!Players[0]
                                             ->CompleteSharedObjective(
                                                 ObjectiveID))
                                    {
                                        bRouteCompleted = false;
                                        break;
                                    }
                                }
                            }

                            int32 CompletedPlayerCount = 0;
                            for (const ABHCharacter* Player : Players)
                            {
                                if (IsValid(Player) &&
                                    Player->IsMissionComplete() &&
                                    Player->GetCompletedObjectiveIDs().Num() ==
                                        4)
                                {
                                    ++CompletedPlayerCount;
                                }
                            }
                            bRouteCompleted = bRouteCompleted &&
                                CompletedPlayerCount == Players.Num();
                            UE_LOG(
                                LogTemp,
                                Display,
                                TEXT(
                                    "BH_TEST_FIRST_LIGHT_COMPLETION "
                                    "result=%s players=%d completed=%d"
                                ),
                                bRouteCompleted
                                    ? TEXT("success")
                                    : TEXT("failure"),
                                Players.Num(),
                                CompletedPlayerCount
                            );
                        }
                    ),
                    FMath::Max(
                        1.0f,
                        FirstLightCompletionDelaySeconds
                    ),
                    false
                );
            }

            if (FParse::Param(
                    FCommandLine::Get(),
                    TEXT("BHTestFieldSquadContextOwnership")))
            {
                GetWorldTimerManager().SetTimer(
                    FieldSquadContextOwnershipTestTimer,
                    this,
                    &ABHWarGameState::
                        RunFieldSquadContextOwnershipTest,
                    0.5f,
                    true,
                    4.0f
                );
            }

            if (FParse::Param(
                    FCommandLine::Get(),
                    TEXT("BHTestMedicalRecoveryReplication")))
            {
                GetWorldTimerManager().SetTimer(
                    MedicalRecoveryReplicationTestTimer,
                    this,
                    &ABHWarGameState::
                        RunMedicalRecoveryReplicationTest,
                    1.0f,
                    true,
                    4.0f
                );
            }

            if (FParse::Param(
                    FCommandLine::Get(),
                    TEXT("BHTestSquadPingReplication")))
            {
                FTimerHandle SquadPingReplicationTimer;
                GetWorldTimerManager().SetTimer(
                    SquadPingReplicationTimer,
                    FTimerDelegate::CreateWeakLambda(
                        this,
                        [this]()
                        {
                            const bool bRenderedScreenshotFixture =
                                FParse::Param(
                                    FCommandLine::Get(),
                                    TEXT(
                                        "BHTestSquadPingRenderedScreenshot"
                                    )
                                );
                            ABHEnemySoldier* TrackedHostile = nullptr;
                            for (TActorIterator<ABHEnemySoldier> It(
                                    GetWorld());
                                 It;
                                 ++It)
                            {
                                if (IsValid(*It) &&
                                    !It->IsDead() &&
                                    It->GetCombatFaction() ==
                                        EBHCombatFaction::Hostile &&
                                    It->GetIsReplicated())
                                {
                                    TrackedHostile = *It;
                                    break;
                                }
                            }
                            const FVector FixtureLocation =
                                IsValid(TrackedHostile)
                                    ? TrackedHostile->GetActorLocation()
                                    : FVector(1234.0f, -567.0f, 89.0f);
                            PublishSquadPing(
                                FixtureLocation,
                                FName(TEXT("HOSTILE")),
                                FName(TEXT("HOST_FIXTURE")),
                                bRenderedScreenshotFixture ? 90.0f : 30.0f,
                                TrackedHostile
                            );
                            UE_LOG(
                                LogTemp,
                                Display,
                                TEXT(
                                    "BH_TEST_SQUAD_PING_CONFIGURED "
                                    "revision=%d context=%s issuer=%s "
                                    "tracked=%s"
                                ),
                                SquadPingSnapshot.Revision,
                                *SquadPingSnapshot.ContextLabel.ToString(),
                                *SquadPingSnapshot.IssuerLabel.ToString(),
                                IsValid(SquadPingSnapshot.TrackedActor)
                                    ? *SquadPingSnapshot.TrackedActor->GetName()
                                    : TEXT("none")
                            );
                        }
                    ),
                    7.0f,
                    false
                );
            }

            if (FParse::Param(
                    FCommandLine::Get(),
                    TEXT("BHTestCustomDifficultyReplication")))
            {
                FBHCampaignDifficultyProfile CustomDifficulty =
                    BHDifficulty::BuildPreset(
                        EBHCampaignDifficultyPreset::Operator
                    );
                CustomDifficulty.IncomingDamageMultiplier = 0.85f;
                CustomDifficulty.EnemyPerceptionMultiplier = 0.95f;
                CustomDifficulty.EnemyCoordinationMultiplier = 1.10f;
                CustomDifficulty.MedicalPressureMultiplier = 1.20f;
                CustomDifficulty.StrategicPressureMultiplier = 1.30f;
                CustomDifficulty.CheckpointIntervalMultiplier = 0.75f;
                const bool bConfigured =
                    IsValid(BoundWarSubsystem) &&
                    BoundWarSubsystem->SetCustomCampaignDifficulty(
                        CustomDifficulty
                    );
                UE_LOG(
                    LogTemp,
                    Display,
                    TEXT(
                        "BH_TEST_CUSTOM_DIFFICULTY_CONFIGURED "
                        "result=%s damage=0.85 perception=0.95 "
                        "coordination=1.10 medical=1.20 "
                        "strategic=1.30 checkpoint=0.75"
                    ),
                    bConfigured ? TEXT("success") : TEXT("failure")
                );
            }

            const bool bRequestedRenderedWorldTraversal =
                FParse::Param(
                    FCommandLine::Get(),
                    TEXT("BHTestRenderedWorldTraversal"));
            if ((bRequestedRenderedWorldTraversal ||
                 FParse::Param(
                    FCommandLine::Get(),
                    TEXT("BHTestRenderedTraversal"))) &&
                !bBHTestRenderedTraversalIssued)
            {
                bBHTestRenderedTraversalIssued = true;
                bRenderedWorldTraversalTest =
                    bRequestedRenderedWorldTraversal;
                GetWorldTimerManager().SetTimer(
                    RenderedTraversalTestTimer,
                    this,
                    &ABHWarGameState::RunRenderedTraversalTest,
                    0.1f,
                    true,
                    0.5f
                );
            }

            FString UIReviewMode;
            if (FParse::Value(
                    FCommandLine::Get(),
                    TEXT("BHTestRenderedUIReview="),
                    UIReviewMode) &&
                !bBHTestRenderedUIReviewIssued)
            {
                bBHTestRenderedUIReviewIssued = true;
                GetWorldTimerManager().SetTimer(
                    RenderedUIReviewTestTimer,
                    this,
                    &ABHWarGameState::RunRenderedUIReviewTest,
                    2.0f,
                    false
                );
            }

            if (FParse::Param(
                    FCommandLine::Get(),
                    TEXT("BHTestNetworkBudgetTelemetry")))
            {
                FParse::Value(
                    FCommandLine::Get(),
                    TEXT("BHTestNetworkBudgetConnections="),
                    NetworkBudgetTelemetryRequiredConnections
                );
                NetworkBudgetTelemetryRequiredConnections = FMath::Clamp(
                    NetworkBudgetTelemetryRequiredConnections,
                    2,
                    16
                );
                bNetworkCombatDensityRequested = FParse::Param(
                    FCommandLine::Get(),
                    TEXT("BHTestNetworkCombatDensity")
                );
                FParse::Value(
                    FCommandLine::Get(),
                    TEXT("BHTestNetworkHostiles="),
                    NetworkCombatDensityHostileCount
                );
                FParse::Value(
                    FCommandLine::Get(),
                    TEXT("BHTestNetworkFriendlies="),
                    NetworkCombatDensityFriendlyCount
                );
                NetworkCombatDensityHostileCount = FMath::Clamp(
                    NetworkCombatDensityHostileCount,
                    0,
                    32
                );
                NetworkCombatDensityFriendlyCount = FMath::Clamp(
                    NetworkCombatDensityFriendlyCount,
                    0,
                    16
                );
                if (UNetDriver* NetDriver = GetWorld()->GetNetDriver())
                {
                    NetDriver->bCollectNetStats = true;
                }
                GetWorldTimerManager().SetTimer(
                    NetworkBudgetTelemetryTestTimer,
                    this,
                    &ABHWarGameState::RunNetworkBudgetTelemetryTest,
                    1.0f,
                    true,
                    1.0f
                );
            }
#endif
            PublishAuthoritativeSnapshot();
#if !UE_BUILD_SHIPPING
            if (FParse::Param(
                    FCommandLine::Get(),
                    TEXT("BHTestBattlefieldConditionsRuntime")))
            {
                const FBHBattlefieldConditionProfile Current =
                    UBHBattlefieldConditions::GetCurrentProfile(this);
                const FBHBattlefieldConditionProfile Storm =
                    UBHBattlefieldConditions::BuildProfileForTurn(5);
                const bool bSuccess =
                    !Current.ConditionLabel.IsNone() &&
                    Storm.Weather == EBHBattlefieldWeather::Storm &&
                    Storm.SightRangeMultiplier < 1.0f &&
                    Storm.VehicleTractionMultiplier < 1.0f &&
                    Storm.MortarDispersionMultiplier > 1.0f;
                UE_LOG(
                    LogTemp,
                    Display,
                    TEXT(
                        "BH_BATTLEFIELD_CONDITIONS_RUNTIME result=%s "
                        "turn=%d current=%s storm_sight=%.2f "
                        "storm_traction=%.2f storm_mortar=%.2f"
                    ),
                    bSuccess ? TEXT("success") : TEXT("failure"),
                    WarStateSnapshot.TurnNumber,
                    *Current.ConditionLabel.ToString(),
                    Storm.SightRangeMultiplier,
                    Storm.VehicleTractionMultiplier,
                    Storm.MortarDispersionMultiplier
                );
                FPlatformMisc::RequestExit(!bSuccess);
            }
            if (FParse::Param(
                    FCommandLine::Get(),
                    TEXT("BHTestCarryLoadRuntime")))
            {
                const FBHCarryLoadProfile Fighting =
                    UBHLoadoutWeight::BuildCarryLoadProfile(
                        EBHWeaponRole::Assault,
                        15,
                        30,
                        0,
                        0,
                        0,
                        0,
                        0);
                const FBHCarryLoadProfile Heavy =
                    UBHLoadoutWeight::BuildCarryLoadProfile(
                        EBHWeaponRole::Support,
                        60,
                        240,
                        2,
                        0,
                        2,
                        2,
                        3);
                const bool bSuccess =
                    Heavy.TotalKilograms > Fighting.TotalKilograms &&
                    Heavy.StaminaDrainMultiplier >
                        Fighting.StaminaDrainMultiplier &&
                    Heavy.MovementSpeedMultiplier <
                        Fighting.MovementSpeedMultiplier;
                UE_LOG(
                    LogTemp,
                    Display,
                    TEXT(
                        "BH_CARRY_LOAD_RUNTIME result=%s "
                        "fighting_kg=%.2f heavy_kg=%.2f "
                        "heavy_speed=%.2f heavy_drain=%.2f"
                    ),
                    bSuccess ? TEXT("success") : TEXT("failure"),
                    Fighting.TotalKilograms,
                    Heavy.TotalKilograms,
                    Heavy.MovementSpeedMultiplier,
                    Heavy.StaminaDrainMultiplier
                );
                FPlatformMisc::RequestExit(!bSuccess);
            }
            if (FParse::Param(
                    FCommandLine::Get(),
                    TEXT("BHTestCoopSupplyRuntime")))
            {
                FTimerHandle CoopSupplyRuntimeTimer;
                GetWorldTimerManager().SetTimer(
                    CoopSupplyRuntimeTimer,
                    FTimerDelegate::CreateWeakLambda(this, [this]()
                    {
                        ABHCharacter* Donor = nullptr;
                        for (TActorIterator<ABHCharacter> It(GetWorld()); It; ++It)
                        {
                            if (It->HasAuthority() && It->IsPlayerControlled())
                            {
                                Donor = *It;
                                break;
                            }
                        }
                        ABHCharacter* Receiver = nullptr;
                        APlayerController* TestController = nullptr;
                        if (IsValid(Donor))
                        {
                            FActorSpawnParameters SpawnParameters;
                            SpawnParameters.SpawnCollisionHandlingOverride =
                                ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
                            Receiver = GetWorld()->SpawnActor<ABHCharacter>(
                                Donor->GetClass(),
                                Donor->GetActorLocation() + FVector(100.0f, 0.0f, 0.0f),
                                Donor->GetActorRotation(),
                                SpawnParameters);
                            TestController = GetWorld()->SpawnActor<APlayerController>(
                                APlayerController::StaticClass(),
                                FTransform::Identity,
                                SpawnParameters);
                        }
                        if (IsValid(TestController) && IsValid(Receiver))
                        {
                            TestController->Possess(Receiver);
                        }

                        UBHWeaponComponent* DonorWeapon = IsValid(Donor)
                            ? Donor->GetWeaponComponent() : nullptr;
                        UBHWeaponComponent* ReceiverWeapon = IsValid(Receiver)
                            ? Receiver->GetWeaponComponent() : nullptr;
                        if (IsValid(DonorWeapon) && IsValid(ReceiverWeapon))
                        {
                            DonorWeapon->EquipWeaponRole(EBHWeaponRole::Assault, true);
                            ReceiverWeapon->EquipWeaponRole(EBHWeaponRole::Assault, true);
                            DonorWeapon->RestoreAmmoState(30, 120);
                            ReceiverWeapon->RestoreAmmoState(30, 20);
                            Donor->RestoreFragGrenadeCount(2);
                            Receiver->RestoreFragGrenadeCount(0);
                            Donor->RestoreEngineeringChargeCount(2);
                            Receiver->RestoreEngineeringChargeCount(0);
                        }
                        const bool bShared = IsValid(Donor) && IsValid(Receiver) &&
                            Donor->TryShareFieldSuppliesWith(Receiver);
                        const bool bSuccess = bShared &&
                            IsValid(DonorWeapon) && IsValid(ReceiverWeapon) &&
                            DonorWeapon->GetReserveAmmo() == 90 &&
                            ReceiverWeapon->GetReserveAmmo() == 50 &&
                            Donor->GetFragGrenadeCount() == 1 &&
                            Receiver->GetFragGrenadeCount() == 1 &&
                            Donor->GetEngineeringChargeCount() == 1 &&
                            Receiver->GetEngineeringChargeCount() == 1;
                        UE_LOG(LogTemp, Display, TEXT(
                            "BH_COOP_SUPPLY_RUNTIME result=%s donor_ammo=%d "
                            "receiver_ammo=%d donor_frag=%d receiver_frag=%d "
                            "donor_charge=%d receiver_charge=%d"),
                            bSuccess ? TEXT("success") : TEXT("failure"),
                            IsValid(DonorWeapon) ? DonorWeapon->GetReserveAmmo() : -1,
                            IsValid(ReceiverWeapon) ? ReceiverWeapon->GetReserveAmmo() : -1,
                            IsValid(Donor) ? Donor->GetFragGrenadeCount() : -1,
                            IsValid(Receiver) ? Receiver->GetFragGrenadeCount() : -1,
                            IsValid(Donor) ? Donor->GetEngineeringChargeCount() : -1,
                            IsValid(Receiver) ? Receiver->GetEngineeringChargeCount() : -1);
                        FPlatformMisc::RequestExit(!bSuccess);
                    }),
                    1.5f,
                    false
                );
            }
            if (FParse::Param(
                    FCommandLine::Get(),
                    TEXT("BHTestMagazineReloadRuntime")))
            {
                FTimerHandle MagazineReloadRuntimeTimer;
                GetWorldTimerManager().SetTimer(
                    MagazineReloadRuntimeTimer,
                    FTimerDelegate::CreateWeakLambda(this, [this]()
                    {
                        ABHCharacter* Player = nullptr;
                        for (TActorIterator<ABHCharacter> It(GetWorld()); It; ++It)
                        {
                            if (It->HasAuthority() && It->IsPlayerControlled())
                            {
                                Player = *It;
                                break;
                            }
                        }
                        UBHWeaponComponent* Weapon = IsValid(Player)
                            ? Player->GetWeaponComponent() : nullptr;
                        bool bTacticalPreserved = false;
                        bool bEmergencyDiscarded = false;
                        float EmergencyDuration = 0.0f;
                        if (IsValid(Weapon))
                        {
                            Weapon->EquipWeaponRole(EBHWeaponRole::Assault, true);
                            Weapon->RestoreAmmoState(10, 90);
                            bTacticalPreserved = Weapon->StartReload() &&
                                Weapon->GetReloadType() == EBHReloadType::Tactical &&
                                Weapon->GetMagazineAmmo() == 10 &&
                                Weapon->GetReserveAmmo() == 90;
                            bEmergencyDiscarded =
                                Weapon->StartEmergencyReload() &&
                                Weapon->GetReloadType() == EBHReloadType::Emergency &&
                                Weapon->GetMagazineAmmo() == 0 &&
                                Weapon->GetReserveAmmo() == 90;
                            const ABHRifle* Rifle = Weapon->GetEquippedRifle();
                            EmergencyDuration = IsValid(Rifle)
                                ? UBHWeaponComponent::CalculateReloadDuration(
                                    Rifle->GetConfig().ReloadDuration,
                                    EBHReloadType::Emergency)
                                : 0.0f;
                        }

                        FTimerHandle MagazineReloadCompletionTimer;
                        GetWorldTimerManager().SetTimer(
                            MagazineReloadCompletionTimer,
                            FTimerDelegate::CreateWeakLambda(
                                this,
                                [Weapon, bTacticalPreserved,
                                 bEmergencyDiscarded]()
                                {
                                    const bool bSuccess =
                                        bTacticalPreserved &&
                                        bEmergencyDiscarded &&
                                        IsValid(Weapon) &&
                                        Weapon->GetMagazineAmmo() == 30 &&
                                        Weapon->GetReserveAmmo() == 60 &&
                                        !Weapon->IsReloading();
                                    UE_LOG(LogTemp, Display, TEXT(
                                        "BH_MAGAZINE_RELOAD_RUNTIME result=%s "
                                        "tactical_preserved=%d emergency_discarded=%d "
                                        "magazine=%d reserve=%d"),
                                        bSuccess ? TEXT("success") : TEXT("failure"),
                                        bTacticalPreserved ? 1 : 0,
                                        bEmergencyDiscarded ? 1 : 0,
                                        IsValid(Weapon)
                                            ? Weapon->GetMagazineAmmo() : -1,
                                        IsValid(Weapon)
                                            ? Weapon->GetReserveAmmo() : -1);
                                    FPlatformMisc::RequestExit(!bSuccess);
                                }),
                            FMath::Max(0.2f, EmergencyDuration + 0.25f),
                            false
                        );
                    }),
                    1.5f,
                    false
                );
            }
            if (FParse::Param(
                    FCommandLine::Get(),
                    TEXT("BHTestWeaponHeatRuntime")))
            {
                FTimerHandle WeaponHeatRuntimeTimer;
                GetWorldTimerManager().SetTimer(
                    WeaponHeatRuntimeTimer,
                    FTimerDelegate::CreateWeakLambda(this, [this]()
                    {
                        ABHCharacter* Player = nullptr;
                        for (TActorIterator<ABHCharacter> It(GetWorld()); It; ++It)
                        {
                            if (It->HasAuthority() && It->IsPlayerControlled())
                            {
                                Player = *It;
                                break;
                            }
                        }
                        UBHWeaponComponent* Weapon = IsValid(Player)
                            ? Player->GetWeaponComponent() : nullptr;
                        if (IsValid(Weapon))
                        {
                            Weapon->EquipWeaponRole(EBHWeaponRole::Support, true);
                            Weapon->RestoreAmmoState(60, 240);
                            Weapon->StartFiring();
                        }

                        FTimerHandle WeaponOverheatCheckTimer;
                        GetWorldTimerManager().SetTimer(
                            WeaponOverheatCheckTimer,
                            FTimerDelegate::CreateWeakLambda(this, [this, Weapon]()
                            {
                                const bool bReachedLockout = IsValid(Weapon) &&
                                    Weapon->IsWeaponOverheated() &&
                                    Weapon->GetWeaponHeatNormalized() >= 0.95f &&
                                    Weapon->GetMagazineAmmo() > 0;
                                const int32 RemainingAtLockout = IsValid(Weapon)
                                    ? Weapon->GetMagazineAmmo() : -1;
                                const float PeakHeat = IsValid(Weapon)
                                    ? Weapon->GetWeaponHeatNormalized() : -1.0f;
                                FTimerHandle WeaponCooldownCheckTimer;
                                GetWorldTimerManager().SetTimer(
                                    WeaponCooldownCheckTimer,
                                    FTimerDelegate::CreateWeakLambda(
                                        this,
                                        [Weapon, bReachedLockout,
                                         RemainingAtLockout, PeakHeat]()
                                        {
                                            const bool bCooled = IsValid(Weapon) &&
                                                !Weapon->IsWeaponOverheated() &&
                                                Weapon->GetWeaponHeatNormalized() <= 0.55f;
                                            const bool bSuccess =
                                                bReachedLockout && bCooled;
                                            UE_LOG(LogTemp, Display, TEXT(
                                                "BH_WEAPON_HEAT_RUNTIME result=%s "
                                                "peak=%.2f remaining=%d cooled=%.2f "
                                                "lockout_cleared=%d"),
                                                bSuccess
                                                    ? TEXT("success")
                                                    : TEXT("failure"),
                                                PeakHeat,
                                                RemainingAtLockout,
                                                IsValid(Weapon)
                                                    ? Weapon->GetWeaponHeatNormalized()
                                                    : -1.0f,
                                                bCooled ? 1 : 0);
                                            FPlatformMisc::RequestExit(!bSuccess);
                                        }),
                                    4.0f,
                                    false
                                );
                            }),
                            5.2f,
                            false
                        );
                    }),
                    1.5f,
                    false
                );
            }
#endif
        }

        UE_LOG(
            LogTemp,
            Display,
            TEXT(
                "BH_WAR_GAME_STATE_READY revision=%d sectors=%d"
            ),
            WarStateSnapshot.Revision,
            WarStateSnapshot.SectorStates.Num()
        );
        return;
    }

#if !UE_BUILD_SHIPPING
    if (FParse::Param(
            FCommandLine::Get(),
            TEXT("BHTestNetworkCombatDensity")))
    {
        FParse::Value(
            FCommandLine::Get(),
            TEXT("BHTestNetworkHostiles="),
            NetworkCombatDensityHostileCount
        );
        FParse::Value(
            FCommandLine::Get(),
            TEXT("BHTestNetworkFriendlies="),
            NetworkCombatDensityFriendlyCount
        );
        GetWorldTimerManager().SetTimer(
            NetworkCombatDensityClientVerificationTimer,
            this,
            &ABHWarGameState::
                VerifyNetworkCombatDensityReplicationTest,
            1.0f,
            true,
            4.0f
        );
    }
#endif

    ApplyReplicatedSnapshot();
}

#if !UE_BUILD_SHIPPING
void ABHWarGameState::RunNetworkBudgetTelemetryTest()
{
    UWorld* World = GetWorld();
    UNetDriver* NetDriver = IsValid(World)
        ? World->GetNetDriver()
        : nullptr;
    if (!IsValid(NetDriver))
    {
        return;
    }

    if (bNetworkCombatDensityRequested &&
        !bNetworkCombatDensityConfigured)
    {
        ConfigureNetworkCombatDensityTest();
        return;
    }
    if (NetDriver->ClientConnections.Num() <
        NetworkBudgetTelemetryRequiredConnections)
    {
        return;
    }
    if (bNetworkCombatDensityRequested &&
        NetworkCombatDensityWarmupSamples < 3)
    {
        ++NetworkCombatDensityWarmupSamples;
        return;
    }

    int32 ValidConnections = 0;
    int32 PeakConnectionInBytesPerSecond = 0;
    int32 PeakConnectionOutBytesPerSecond = 0;
    int32 PeakConnectionChannels = 0;
    int32 AggregateConnectionInBytesPerSecond = 0;
    int32 AggregateConnectionOutBytesPerSecond = 0;
    int32 AggregateConnectionInPacketsPerSecond = 0;
    int32 AggregateConnectionOutPacketsPerSecond = 0;
    int32 AggregateConnectionPacketLoss = 0;

    for (const TObjectPtr<UNetConnection>& ConnectionPtr :
         NetDriver->ClientConnections)
    {
        UNetConnection* Connection = ConnectionPtr.Get();
        if (!IsValid(Connection))
        {
            continue;
        }

        ++ValidConnections;
        AggregateConnectionInBytesPerSecond +=
            FMath::Max(0, Connection->InBytesPerSecond);
        AggregateConnectionOutBytesPerSecond +=
            FMath::Max(0, Connection->OutBytesPerSecond);
        AggregateConnectionInPacketsPerSecond +=
            FMath::Max(0, Connection->InPacketsPerSecond);
        AggregateConnectionOutPacketsPerSecond +=
            FMath::Max(0, Connection->OutPacketsPerSecond);
        AggregateConnectionPacketLoss +=
            FMath::Max(0, Connection->InPacketsLost) +
            FMath::Max(0, Connection->OutPacketsLost);
        PeakConnectionInBytesPerSecond = FMath::Max(
            PeakConnectionInBytesPerSecond,
            Connection->InBytesPerSecond
        );
        PeakConnectionOutBytesPerSecond = FMath::Max(
            PeakConnectionOutBytesPerSecond,
            Connection->OutBytesPerSecond
        );

        int32 OpenChannelCount = 0;
        for (const TObjectPtr<UChannel>& Channel : Connection->Channels)
        {
            OpenChannelCount += IsValid(Channel) ? 1 : 0;
        }
        PeakConnectionChannels = FMath::Max(
            PeakConnectionChannels,
            OpenChannelCount
        );
    }

    if (ValidConnections < NetworkBudgetTelemetryRequiredConnections)
    {
        return;
    }

    ++NetworkBudgetTelemetrySampleCount;
    UE_LOG(
        LogTemp,
        Display,
        TEXT(
            "BH_NET_BUDGET_SAMPLE sample=%d connections=%d "
            "driver_in_bps=%u driver_out_bps=%u "
            "aggregate_in_bps=%d aggregate_out_bps=%d "
            "peak_connection_in_bps=%d peak_connection_out_bps=%d "
            "aggregate_in_pps=%d aggregate_out_pps=%d "
            "peak_channels=%d packet_loss=%d total_rpcs=%u"
        ),
        NetworkBudgetTelemetrySampleCount,
        ValidConnections,
        NetDriver->InBytesPerSecond,
        NetDriver->OutBytesPerSecond,
        AggregateConnectionInBytesPerSecond,
        AggregateConnectionOutBytesPerSecond,
        PeakConnectionInBytesPerSecond,
        PeakConnectionOutBytesPerSecond,
        AggregateConnectionInPacketsPerSecond,
        AggregateConnectionOutPacketsPerSecond,
        PeakConnectionChannels,
        AggregateConnectionPacketLoss,
        NetDriver->TotalRPCsCalled
    );

    if (NetworkBudgetTelemetrySampleCount == 10)
    {
        UE_LOG(
            LogTemp,
            Display,
            TEXT("BH_NET_BUDGET_WINDOW_READY samples=10 connections=%d"),
            NetworkBudgetTelemetryRequiredConnections
        );
    }
}

void ABHWarGameState::RunRenderedTraversalTest()
{
    UWorld* World = GetWorld();
    if (!IsValid(World) || !HasAuthority())
    {
        return;
    }

    ABHCharacter* Player = nullptr;
    for (TActorIterator<ABHCharacter> It(World); It; ++It)
    {
        if (IsValid(*It) && It->IsPlayerControlled())
        {
            Player = *It;
            break;
        }
    }
    if (!IsValid(Player))
    {
        return;
    }

    Player->SetCanBeDamaged(false);

    if (RenderedTraversalTestDwellTicks > 0)
    {
        if (bRenderedWorldTraversalTest)
        {
            CSV_CUSTOM_STAT(
                BrokenHorizon,
                WorldTraversalMeasure,
                0,
                ECsvCustomStatOp::Set
            );
        }
        --RenderedTraversalTestDwellTicks;
        return;
    }

    if (RenderedTraversalTestSubstep == 0 &&
        (!bRenderedWorldTraversalTest ||
         !bRenderedWorldTraversalTargetSettled))
    {
        AActor* Target = nullptr;
        if (bRenderedWorldTraversalTest)
        {
            TArray<ABHSectorAnchor*> SectorAnchors;
            for (TActorIterator<ABHSectorAnchor> It(World); It; ++It)
            {
                if (IsValid(*It) && !It->GetSectorID().IsNone())
                {
                    SectorAnchors.Add(*It);
                }
            }
            SectorAnchors.Sort(
                [](const ABHSectorAnchor& Left,
                   const ABHSectorAnchor& Right)
                {
                    return Left.GetSectorID().LexicalLess(
                        Right.GetSectorID()
                    );
                }
            );
            if (!SectorAnchors.IsEmpty())
            {
                Target = SectorAnchors[
                    RenderedTraversalTestStep % SectorAnchors.Num()
                ];
                RenderedTraversalRouteLabel = FName(
                    *FString::Printf(
                        TEXT("SECTOR_%s"),
                        *CastChecked<ABHSectorAnchor>(Target)
                            ->GetSectorID().ToString()
                    )
                );
            }
            else
            {
                RenderedTraversalRouteLabel =
                    FName(TEXT("SECTOR_ANCHOR"));
            }
        }
        else
        {
            const int32 RouteIndex = RenderedTraversalTestStep % 4;
            if (RouteIndex == 0)
            {
                for (TActorIterator<ABHKeycard> It(World); It; ++It)
                {
                    Target = *It;
                    break;
                }
                RenderedTraversalRouteLabel = FName(TEXT("KEYCARD"));
            }
            else if (RouteIndex == 1)
            {
                for (TActorIterator<ABHDoor> It(World); It; ++It)
                {
                    Target = *It;
                    break;
                }
                RenderedTraversalRouteLabel =
                    FName(TEXT("SECURITY_DOOR"));
            }
            else if (RouteIndex == 2)
            {
                for (TActorIterator<ABHEnemySoldier> It(World); It; ++It)
                {
                    if (It->GetCombatFaction() ==
                        EBHCombatFaction::Hostile)
                    {
                        Target = *It;
                        break;
                    }
                }
                RenderedTraversalRouteLabel =
                    FName(TEXT("HOSTILE_CONTACT"));
            }
            else
            {
                for (TActorIterator<ABHExtractionZone> It(World); It; ++It)
                {
                    Target = *It;
                    break;
                }
                RenderedTraversalRouteLabel =
                    FName(TEXT("EXTRACTION"));
            }
        }

        if (!IsValid(Target))
        {
            UE_LOG(
                LogTemp,
                Error,
                TEXT(
                    "BH_RENDERED_%sTRAVERSAL_STEP result=failure step=%d "
                    "route=%s reason=missing_target"
                ),
                bRenderedWorldTraversalTest ? TEXT("WORLD_") : TEXT(""),
                RenderedTraversalTestStep,
                *RenderedTraversalRouteLabel.ToString()
            );
            GetWorldTimerManager().ClearTimer(
                RenderedTraversalTestTimer
            );
            return;
        }

        RenderedTraversalFocalTarget = Target->GetActorLocation();
        if (bRenderedWorldTraversalTest)
        {
            // Cross-sector repositioning represents a loading transition, not
            // player locomotion. Settle the destination cell, then profile a
            // local 12 m walk through each authored sector.
            RenderedTraversalSegmentStart =
                RenderedTraversalFocalTarget +
                FVector(600.0f, 0.0f, 300.0f);
            RenderedTraversalSegmentTarget =
                RenderedTraversalFocalTarget +
                FVector(-600.0f, 0.0f, 300.0f);
            Player->SetActorLocation(
                RenderedTraversalSegmentStart,
                false,
                nullptr,
                ETeleportType::TeleportPhysics
            );
            CSV_CUSTOM_STAT(
                BrokenHorizon,
                WorldTraversalMeasure,
                0,
                ECsvCustomStatOp::Set
            );
            bRenderedWorldTraversalTargetSettled = true;
            RenderedTraversalTestDwellTicks = 50;
            return;
        }

        RenderedTraversalSegmentStart = Player->GetActorLocation();
        RenderedTraversalSegmentTarget =
            RenderedTraversalFocalTarget +
            FVector(-600.0f, 0.0f, 300.0f);
    }

    if (bRenderedWorldTraversalTest)
    {
        if (RenderedTraversalTestSubstep == 0)
        {
            FNavLocation ProjectedNavigationLocation;
            UNavigationSystemV1* NavigationSystem =
                FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
            const bool bNavigationAvailable =
                IsValid(NavigationSystem) &&
                NavigationSystem->ProjectPointToNavigation(
                    RenderedTraversalFocalTarget,
                    ProjectedNavigationLocation,
                    FVector(2000.0f, 2000.0f, 5000.0f)
                );
            if (!bNavigationAvailable)
            {
                UE_LOG(
                    LogTemp,
                    Error,
                    TEXT(
                        "BH_RENDERED_WORLD_TRAVERSAL_STEP result=failure "
                        "step=%d route=%s reason=navigation_unavailable"
                    ),
                    RenderedTraversalTestStep,
                    *RenderedTraversalRouteLabel.ToString()
                );
                GetWorldTimerManager().ClearTimer(
                    RenderedTraversalTestTimer
                );
                return;
            }

            UE_LOG(
                LogTemp,
                Display,
                TEXT(
                    "BH_RENDERED_WORLD_NAVIGATION result=success step=%d "
                    "route=%s location=%s"
                ),
                RenderedTraversalTestStep,
                *RenderedTraversalRouteLabel.ToString(),
                *ProjectedNavigationLocation.Location.ToCompactString()
            );
        }

        CSV_CUSTOM_STAT(
            BrokenHorizon,
            WorldTraversalMeasure,
            1,
            ECsvCustomStatOp::Set
        );
    }
    ++RenderedTraversalTestSubstep;
    const float SegmentAlpha = FMath::Clamp(
        static_cast<float>(RenderedTraversalTestSubstep) / 24.0f,
        0.0f,
        1.0f
    );
    const FVector ViewLocation = FMath::Lerp(
        RenderedTraversalSegmentStart,
        RenderedTraversalSegmentTarget,
        SegmentAlpha
    );
    Player->SetActorLocation(
        ViewLocation,
        false,
        nullptr,
        ETeleportType::TeleportPhysics
    );
    const FRotator ViewRotation =
        (RenderedTraversalFocalTarget - ViewLocation).Rotation();
    Player->SetActorRotation(ViewRotation);
    if (AController* Controller = Player->GetController())
    {
        Controller->SetControlRotation(ViewRotation);
    }

    if (RenderedTraversalTestSubstep < 24)
    {
        return;
    }

    UE_LOG(
        LogTemp,
        Display,
        TEXT(
            "BH_RENDERED_%sTRAVERSAL_STEP result=success step=%d "
            "route=%s location=%s"
        ),
        bRenderedWorldTraversalTest ? TEXT("WORLD_") : TEXT(""),
        RenderedTraversalTestStep,
        *RenderedTraversalRouteLabel.ToString(),
        *ViewLocation.ToCompactString()
    );
    ++RenderedTraversalTestStep;
    RenderedTraversalTestSubstep = 0;
    bRenderedWorldTraversalTargetSettled = false;
    if (bRenderedWorldTraversalTest)
    {
        CSV_CUSTOM_STAT(
            BrokenHorizon,
            WorldTraversalMeasure,
            0,
            ECsvCustomStatOp::Set
        );
    }
    if (RenderedTraversalTestStep >= 8)
    {
        GetWorldTimerManager().ClearTimer(RenderedTraversalTestTimer);
        UE_LOG(
            LogTemp,
            Display,
            TEXT(
                "BH_RENDERED_%sTRAVERSAL_COMPLETE result=success "
                "steps=8 loops=2"
            ),
            bRenderedWorldTraversalTest ? TEXT("WORLD_") : TEXT("")
        );
    }
    else
    {
        // Hold briefly at each gameplay landmark so the profile measures
        // streaming recovery as well as traversal pressure.
        RenderedTraversalTestDwellTicks = 10;
    }
}

void ABHWarGameState::RunRenderedUIReviewTest()
{
    UWorld* World = GetWorld();
    if (!IsValid(World) || !HasAuthority())
    {
        return;
    }

    FString ReviewMode;
    if (!FParse::Value(
            FCommandLine::Get(),
            TEXT("BHTestRenderedUIReview="),
            ReviewMode))
    {
        return;
    }
    ReviewMode = ReviewMode.ToUpper();
    const bool bPauseReview = ReviewMode == TEXT("PAUSE");
    const bool bBriefingReview = ReviewMode == TEXT("BRIEFING");
    const bool bSettingsReview = ReviewMode == TEXT("SETTINGS");
    const bool bRemappingReview = ReviewMode == TEXT("REMAPPING");
    const bool bWarMapReview = ReviewMode == TEXT("WAR_MAP");
    const bool bWarMapDeploymentReview =
        ReviewMode == TEXT("WAR_MAP_DEPLOYMENT");
    const bool bCustomDifficultyReview =
        ReviewMode == TEXT("CUSTOM_DIFFICULTY");
    const bool bInteractionPromptReview =
        ReviewMode == TEXT("HUD_INTERACTION_PROMPT");
    const bool bGamepadPromptReview =
        ReviewMode == TEXT("HUD_GAMEPAD_PROMPTS");
    const bool bBothPromptReview =
        ReviewMode == TEXT("HUD_BOTH_PROMPTS");
    if (ReviewMode != TEXT("HUD") &&
        !bInteractionPromptReview &&
        !bGamepadPromptReview &&
        !bBothPromptReview &&
        !bPauseReview &&
        !bBriefingReview &&
        !bSettingsReview &&
        !bRemappingReview &&
        !bWarMapReview &&
        !bWarMapDeploymentReview &&
        !bCustomDifficultyReview)
    {
        UE_LOG(
            LogTemp,
            Error,
            TEXT(
                "BH_RENDERED_UI_REVIEW result=failure "
                "reason=unsupported_mode mode=%s"
            ),
            *ReviewMode
        );
        FPlatformMisc::RequestExit(false);
        return;
    }

    ABHCharacter* Player = nullptr;
    for (TActorIterator<ABHCharacter> It(World); It; ++It)
    {
        if (IsValid(*It) && It->IsPlayerControlled())
        {
            Player = *It;
            break;
        }
    }
    if (!IsValid(Player))
    {
        UE_LOG(
            LogTemp,
            Error,
            TEXT(
                "BH_RENDERED_UI_REVIEW result=failure "
                "reason=missing_player mode=%s"
            ),
            *ReviewMode
        );
        FPlatformMisc::RequestExit(false);
        return;
    }

    Player->SetCanBeDamaged(false);
    if (bInteractionPromptReview)
    {
        Player->SetActorTickEnabled(false);
    }
    if (bGamepadPromptReview || bBothPromptReview)
    {
        if (UGameInstance* GameInstance = GetGameInstance())
        {
            if (UBHUserSettingsSubsystem* UserSettings =
                    GameInstance->GetSubsystem<
                        UBHUserSettingsSubsystem>())
            {
                UserSettings->SetInputPromptModeForRenderedReview(
                    bBothPromptReview
                        ? EBHInputPromptMode::Both
                        : EBHInputPromptMode::Gamepad,
                    bGamepadPromptReview
                );
            }
        }
    }
    if (bPauseReview || bSettingsReview || bRemappingReview)
    {
        Player->TogglePauseMenu();
        if (bSettingsReview || bRemappingReview)
        {
            UBHPauseMenuWidget* PauseMenu = nullptr;
            for (TObjectIterator<UBHPauseMenuWidget> It; It; ++It)
            {
                if (IsValid(*It) &&
                    It->GetWorld() == World &&
                    It->IsInViewport())
                {
                    PauseMenu = *It;
                    break;
                }
            }
            if (!IsValid(PauseMenu) ||
                !PauseMenu->OpenSettingsForRenderedReview(
                    bRemappingReview
                ))
            {
                UE_LOG(
                    LogTemp,
                    Error,
                    TEXT(
                        "BH_RENDERED_UI_REVIEW result=failure "
                        "reason=settings_open_failed mode=%s"
                    ),
                    *ReviewMode
                );
                FPlatformMisc::RequestExit(false);
                return;
            }
        }
    }
    else if (bBriefingReview)
    {
        Player->ShowPriorityStatusNotification(
            NSLOCTEXT(
                "BrokenHorizon",
                "RenderedUIReviewStrategicBriefing",
                "STRATEGIC BRIEFING\n\n"
                "OPERATION BREAKTHROUGH // North Pass\n\n"
                "Enemy forces control North Pass. Infiltrate the "
                "sector, breach the security line, eliminate the "
                "guard force, and reach extraction."
            ),
            EBHNotificationPriority::Critical
        );
    }
    else if (bWarMapReview || bWarMapDeploymentReview || bCustomDifficultyReview)
    {
        if (bCustomDifficultyReview)
        {
            FBHCampaignDifficultyProfile CustomDifficulty =
                BHDifficulty::BuildPreset(
                    EBHCampaignDifficultyPreset::Operator
                );
            CustomDifficulty.IncomingDamageMultiplier = 0.85f;
            CustomDifficulty.EnemyPerceptionMultiplier = 0.95f;
            CustomDifficulty.EnemyCoordinationMultiplier = 1.10f;
            CustomDifficulty.MedicalPressureMultiplier = 1.20f;
            CustomDifficulty.StrategicPressureMultiplier = 1.30f;
            CustomDifficulty.CheckpointIntervalMultiplier = 0.75f;
            if (!IsValid(BoundWarSubsystem) ||
                !BoundWarSubsystem->SetCustomCampaignDifficulty(
                    CustomDifficulty
                ))
            {
                UE_LOG(
                    LogTemp,
                    Error,
                    TEXT(
                        "BH_RENDERED_UI_REVIEW result=failure "
                        "reason=custom_difficulty_failed mode=%s"
                    ),
                    *ReviewMode
                );
                FPlatformMisc::RequestExit(false);
                return;
            }
        }

        Player->ToggleWarMap();
        if (bWarMapDeploymentReview)
        {
            for (TObjectIterator<UBHWarMapWidget> It; It; ++It)
            {
                if (IsValid(*It) &&
                    It->GetWorld() == World &&
                    It->IsInViewport())
                {
                    It->SetDeploymentMode(true);
                    break;
                }
            }
        }
    }
    else
    {
        // The production First Light startup can present a strategic briefing
        // during this fixture's warmup. A HUD review must isolate the base HUD
        // rather than accidentally certifying a notification-heavy frame.
        for (TObjectIterator<UBHObjectiveNotificationWidget> It; It; ++It)
        {
            if (IsValid(*It) && It->GetWorld() == World)
            {
                It->SetVisibility(ESlateVisibility::Collapsed);
            }
        }

        // Exercise the real upper-right strategic lane in the HUD fixture so
        // 1080p evidence catches collisions with health/stamina presentation.
        for (TObjectIterator<UBHCombatStatusWidget> It; It; ++It)
        {
            if (IsValid(*It) && It->GetWorld() == World)
            {
                It->SetStrategicSituation(
                    true,
                    NSLOCTEXT(
                        "BrokenHorizon",
                        "RenderedHUDStrategicSector",
                        "Western Forward Base"
                    ),
                    EBHWarFaction::Friendly,
                    79.0f,
                    1.5f,
                    12
                );
                It->SetEnemyResponsePressure(42.0f);
                It->SetCivilianSupport(68.0f);
            }
        }

        if (bInteractionPromptReview)
        {
            bool bPromptApplied = false;
            for (TObjectIterator<UBHInteractionPromptWidget> It;
                It;
                ++It)
            {
                if (IsValid(*It) &&
                    (*It)->GetWorld() == World &&
                    (*It)->IsInViewport())
                {
                    (*It)->SetInteractionText(NSLOCTEXT(
                        "BrokenHorizon",
                        "RenderedInteractionPrompt",
                        "Press [F] to Pick Up Red Keycard"
                    ));
                    (*It)->SetVisibility(ESlateVisibility::Visible);
                    bPromptApplied = true;
                    break;
                }
            }

            UE_LOG(
                LogTemp,
                Display,
                TEXT(
                    "BH_RENDERED_UI_REVIEW prompt_fixture applied=%d"
                ),
                bPromptApplied ? 1 : 0
            );
        }
    }

    const FString ReportDirectory = FPaths::Combine(
        FPaths::ProjectSavedDir(),
        TEXT("Reports")
    );
    IFileManager::Get().MakeDirectory(
        *ReportDirectory,
        true
    );
    FString OutputPath;
    if (!FParse::Value(
            FCommandLine::Get(),
            TEXT("BHTestRenderedUIScreenshotPath="),
            OutputPath) ||
        OutputPath.IsEmpty())
    {
        OutputPath = FPaths::Combine(
            ReportDirectory,
            FString::Printf(
                TEXT("BHRenderedUI-%s.png"),
                *ReviewMode
            )
        );
    }
    OutputPath = FPaths::ConvertRelativePathToFull(OutputPath);
    IFileManager::Get().MakeDirectory(
        *FPaths::GetPath(OutputPath),
        true
    );
    if (bInteractionPromptReview)
    {
        const FString DelayedScreenshotPath = OutputPath;
        FTSTicker::GetCoreTicker().AddTicker(
            FTickerDelegate::CreateLambda(
                [DelayedScreenshotPath](float)
                {
                    FScreenshotRequest::RequestScreenshot(
                        DelayedScreenshotPath,
                        true,
                        false
                    );
                    return false;
                }
            ),
            0.25f
        );
    }
    else
    {
        FScreenshotRequest::RequestScreenshot(
            OutputPath,
            true,
            false
        );
    }
    UE_LOG(
        LogTemp,
        Display,
        TEXT(
            "BH_RENDERED_UI_REVIEW result=requested "
            "mode=%s path=%s"
        ),
        *ReviewMode,
        *OutputPath
    );

    FTSTicker::GetCoreTicker().AddTicker(
        FTickerDelegate::CreateLambda(
            [](float)
            {
                FPlatformMisc::RequestExit(false);
                return false;
            }
        ),
        1.0f
    );
}

void ABHWarGameState::ConfigureNetworkCombatDensityTest()
{
    UWorld* World = GetWorld();
    if (!IsValid(World) || !HasAuthority())
    {
        return;
    }

    FVector Center = FVector::ZeroVector;
    bool bFoundPlayer = false;
    for (TActorIterator<ABHCharacter> It(World); It; ++It)
    {
        if (IsValid(*It) && IsValid(It->GetPlayerState()))
        {
            Center = It->GetActorLocation();
            bFoundPlayer = true;
            break;
        }
    }
    if (!bFoundPlayer)
    {
        return;
    }

    const int32 RequestedTotal =
        NetworkCombatDensityHostileCount +
        NetworkCombatDensityFriendlyCount;
    UNavigationSystemV1* NavigationSystem =
        FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
    if (!IsValid(NavigationSystem))
    {
        return;
    }
    FNavLocation NavigableCenter;
    if (!NavigationSystem->ProjectPointToNavigation(
            Center,
            NavigableCenter,
            FVector(500.0f, 500.0f, 2000.0f)))
    {
        // Dynamic navigation is still building. BeginPlay retries this setup;
        // do not create a partial density fixture in the meantime.
        return;
    }

    int32 SpawnedHostiles = 0;
    int32 SpawnedFriendlies = 0;
    TArray<TWeakObjectPtr<ABHEnemySoldier>> SpawnedSoldiers;
    SpawnedSoldiers.Reserve(RequestedTotal);
    for (int32 Index = 0; Index < RequestedTotal; ++Index)
    {
        const bool bHostile = Index < NetworkCombatDensityHostileCount;
        const int32 FactionIndex = bHostile
            ? Index
            : Index - NetworkCombatDensityHostileCount;
        const int32 FactionCount = bHostile
            ? NetworkCombatDensityHostileCount
            : NetworkCombatDensityFriendlyCount;
        const float Angle =
            (2.0f * UE_PI * static_cast<float>(FactionIndex)) /
            static_cast<float>(FMath::Max(1, FactionCount));
        const float Radius = bHostile ? 3200.0f : 1200.0f;
        const FVector DesiredSpawnLocation = Center + FVector(
            FMath::Cos(Angle) * Radius,
            FMath::Sin(Angle) * Radius,
            120.0f
        );
        FNavLocation ProjectedSpawnLocation;
        bool bFoundSpawnLocation =
            NavigationSystem->ProjectPointToNavigation(
                DesiredSpawnLocation,
                ProjectedSpawnLocation,
                FVector(1200.0f, 1200.0f, 2000.0f));
        if (!bFoundSpawnLocation)
        {
            bFoundSpawnLocation =
                NavigationSystem->GetRandomReachablePointInRadius(
                    NavigableCenter.Location,
                    FMath::Max(1200.0f, Radius),
                    ProjectedSpawnLocation);
        }
        if (!bFoundSpawnLocation)
        {
            UE_LOG(
                LogTemp,
                Warning,
                TEXT(
                    "BH_NET_COMBAT_DENSITY_SPAWN_REJECTED "
                    "index=%d desired=%s reason=no_navigation"
                ),
                Index,
                *DesiredSpawnLocation.ToCompactString()
            );
            continue;
        }

        const FVector SpawnLocation = ProjectedSpawnLocation.Location;
        const FTransform SpawnTransform(
            (Center - SpawnLocation).Rotation(),
            SpawnLocation
        );
        ABHEnemySoldier* Soldier =
            World->SpawnActorDeferred<ABHEnemySoldier>(
                ABHEnemySoldier::StaticClass(),
                SpawnTransform,
                this,
                nullptr,
                ESpawnActorCollisionHandlingMethod::
                    AdjustIfPossibleButAlwaysSpawn
            );
        if (!IsValid(Soldier))
        {
            continue;
        }

        Soldier->SetFlags(RF_Transient);
        Soldier->bAlwaysRelevant = true;
        Soldier->SetCanBeDamaged(false);
        Soldier->SetCombatFaction(
            bHostile
                ? EBHCombatFaction::Hostile
                : EBHCombatFaction::Friendly
        );
        Soldier->SetObjectiveIdToCompleteOnDeath(NAME_None);
        UGameplayStatics::FinishSpawningActor(Soldier, SpawnTransform);
        SpawnedSoldiers.Add(Soldier);
        if (bHostile)
        {
            ++SpawnedHostiles;
        }
        else
        {
            ++SpawnedFriendlies;
        }
    }

    bNetworkCombatDensityConfigured =
        SpawnedHostiles == NetworkCombatDensityHostileCount &&
        SpawnedFriendlies == NetworkCombatDensityFriendlyCount;
    if (!bNetworkCombatDensityConfigured)
    {
        for (const TWeakObjectPtr<ABHEnemySoldier>& SpawnedSoldier :
             SpawnedSoldiers)
        {
            if (SpawnedSoldier.IsValid())
            {
                SpawnedSoldier->Destroy();
            }
        }
    }
    UE_LOG(
        LogTemp,
        Display,
        TEXT(
            "BH_NET_COMBAT_DENSITY_CONFIGURED result=%s "
            "hostiles=%d friendlies=%d total=%d"
        ),
        bNetworkCombatDensityConfigured
            ? TEXT("success")
            : TEXT("failure"),
        SpawnedHostiles,
        SpawnedFriendlies,
        SpawnedHostiles + SpawnedFriendlies
    );
}

void ABHWarGameState::VerifyNetworkCombatDensityReplicationTest()
{
    ++NetworkCombatDensityClientVerificationAttempts;
    int32 ReplicatedHostiles = 0;
    int32 ReplicatedFriendlies = 0;
    for (TActorIterator<ABHEnemySoldier> It(GetWorld()); It; ++It)
    {
        if (!IsValid(*It))
        {
            continue;
        }
        if (It->GetCombatFaction() == EBHCombatFaction::Friendly)
        {
            ++ReplicatedFriendlies;
        }
        else
        {
            ++ReplicatedHostiles;
        }
    }

    const bool bReplicated =
        ReplicatedHostiles >= NetworkCombatDensityHostileCount &&
        ReplicatedFriendlies >= NetworkCombatDensityFriendlyCount;
    if (!bReplicated &&
        NetworkCombatDensityClientVerificationAttempts < 30)
    {
        return;
    }

    GetWorldTimerManager().ClearTimer(
        NetworkCombatDensityClientVerificationTimer
    );
    UE_LOG(
        LogTemp,
        Display,
        TEXT(
            "BH_NET_COMBAT_DENSITY_REPLICATED result=%s "
            "hostiles=%d friendlies=%d total=%d attempts=%d"
        ),
        bReplicated ? TEXT("success") : TEXT("failure"),
        ReplicatedHostiles,
        ReplicatedFriendlies,
        ReplicatedHostiles + ReplicatedFriendlies,
        NetworkCombatDensityClientVerificationAttempts
    );
}

void ABHWarGameState::RunInventoryTransferRuntimeTest()
{
    if (bBHTestInventoryTransferIssued || !HasAuthority())
    {
        return;
    }

    ABHCharacter* Source = nullptr;
    ABHCharacter* Recipient = nullptr;
    for (TActorIterator<ABHCharacter> It(GetWorld()); It; ++It)
    {
        ABHCharacter* Candidate = *It;
        if (!IsValid(Candidate) || !Candidate->HasAuthority() ||
            !Candidate->IsPlayerControlled())
        {
            continue;
        }
        if (!IsValid(Source))
        {
            Source = Candidate;
        }
        else if (Candidate != Source)
        {
            Recipient = Candidate;
            break;
        }
    }

    if (!IsValid(Source) || !IsValid(Recipient))
    {
        return;
    }

    Source->SetActorLocation(Recipient->GetActorLocation() + FVector(100.0f, 0.0f, 0.0f));
    const int32 SourceBefore = IsValid(Source->GetWeaponComponent())
        ? Source->GetWeaponComponent()->GetReserveAmmo()
        : 0;
    const int32 RecipientBefore = IsValid(Recipient->GetWeaponComponent())
        ? Recipient->GetWeaponComponent()->GetReserveAmmo()
        : 0;
    const bool bTransferred = Source->TransferInventoryItemTo(
        Recipient,
        EBHSalvagePickupType::Ammunition,
        1
    );
    bBHTestInventoryTransferIssued = true;
    UE_LOG(
        LogTemp,
        Display,
        TEXT(
            "BH_TEST_INVENTORY_TRANSFER_RUNTIME result=%s "
            "source_before=%d source_after=%d recipient_before=%d "
            "recipient_after=%d"
        ),
        bTransferred ? TEXT("success") : TEXT("failure"),
        SourceBefore,
        IsValid(Source->GetWeaponComponent())
            ? Source->GetWeaponComponent()->GetReserveAmmo()
            : 0,
        RecipientBefore,
        IsValid(Recipient->GetWeaponComponent())
            ? Recipient->GetWeaponComponent()->GetReserveAmmo()
            : 0
    );
    GetWorldTimerManager().ClearTimer(InventoryTransferRuntimeTestTimer);
}

void ABHWarGameState::RunMissionCacheTransferRuntimeTest()
{
    if (bBHTestMissionCacheTransferIssued || !HasAuthority())
    {
        return;
    }

    ABHCharacter* StoreOwner = nullptr;
    ABHCharacter* RetrieveOwner = nullptr;
    for (TActorIterator<ABHCharacter> It(GetWorld()); It; ++It)
    {
        ABHCharacter* Candidate = *It;
        if (!IsValid(Candidate) || !Candidate->HasAuthority() ||
            !Candidate->IsPlayerControlled())
        {
            continue;
        }

        if (!IsValid(StoreOwner))
        {
            StoreOwner = Candidate;
        }
        else if (Candidate != StoreOwner)
        {
            RetrieveOwner = Candidate;
            break;
        }
    }

    ABHMissionItemContainer* MissionCache = nullptr;
    for (TActorIterator<ABHMissionItemContainer> It(GetWorld()); It; ++It)
    {
        if (IsValid(*It) &&
            It->GetPersistenceID() ==
                FName(TEXT("FirstLightMissionCache01")))
        {
            MissionCache = *It;
            break;
        }
    }

    if (!IsValid(StoreOwner) || !IsValid(RetrieveOwner) ||
        !IsValid(MissionCache))
    {
        return;
    }

    if (!MissionCache->Implements<UBHInteractable>() ||
        MissionCache->GetMissionItemID().IsNone())
    {
        bBHTestMissionCacheTransferIssued = true;
        UE_LOG(
            LogTemp,
            Error,
            TEXT(
                "BH_TEST_MISSION_CACHE_TRANSFER result=failure "
                "stage=configuration container=%s"
            ),
            *MissionCache->GetPersistenceID().ToString()
        );
        GetWorldTimerManager().ClearTimer(
            MissionCacheTransferRuntimeTestTimer
        );
        return;
    }

    const FName MissionItemID = MissionCache->GetMissionItemID();
    if (BHTestMissionCacheTransferPhase == 0)
    {
        StoreOwner->RemoveKeycard(MissionItemID);
        RetrieveOwner->RemoveKeycard(MissionItemID);
        MissionCache->RestoreStoredMissionItem(NAME_None);
        StoreOwner->AddKeycard(MissionItemID);
        BHTestMissionCacheTransferPhase = 1;
        UE_LOG(
            LogTemp,
            Display,
            TEXT(
                "BH_TEST_MISSION_CACHE_TRANSFER stage=setup "
                "container=%s store_owner=%s retrieve_owner=%s item=%s"
            ),
            *MissionCache->GetPersistenceID().ToString(),
            *StoreOwner->GetName(),
            *RetrieveOwner->GetName(),
            *MissionItemID.ToString()
        );
        return;
    }

    if (BHTestMissionCacheTransferPhase == 1)
    {
        IBHInteractable::Execute_Interact(MissionCache, StoreOwner);
        const bool bStored =
            MissionCache->GetStoredMissionItemID() == MissionItemID &&
            !StoreOwner->HasKeycard(MissionItemID) &&
            !RetrieveOwner->HasKeycard(MissionItemID);
        if (!bStored)
        {
            bBHTestMissionCacheTransferIssued = true;
            UE_LOG(
                LogTemp,
                Error,
                TEXT(
                    "BH_TEST_MISSION_CACHE_TRANSFER result=failure "
                    "stage=store stored=%s store_owner_has=%d "
                    "retrieve_owner_has=%d"
                ),
                *MissionCache->GetStoredMissionItemID().ToString(),
                StoreOwner->HasKeycard(MissionItemID) ? 1 : 0,
                RetrieveOwner->HasKeycard(MissionItemID) ? 1 : 0
            );
            GetWorldTimerManager().ClearTimer(
                MissionCacheTransferRuntimeTestTimer
            );
            return;
        }

        BHTestMissionCacheTransferPhase = 2;
        UE_LOG(
            LogTemp,
            Display,
            TEXT(
                "BH_TEST_MISSION_CACHE_TRANSFER stage=store "
                "result=success owner=%s"
            ),
            *StoreOwner->GetName()
        );
        return;
    }

    if (BHTestMissionCacheTransferPhase == 2)
    {
        IBHInteractable::Execute_Interact(MissionCache, RetrieveOwner);
        const bool bRetrieved =
            MissionCache->GetStoredMissionItemID().IsNone() &&
            !StoreOwner->HasKeycard(MissionItemID) &&
            RetrieveOwner->HasKeycard(MissionItemID);
        if (!bRetrieved)
        {
            bBHTestMissionCacheTransferIssued = true;
            UE_LOG(
                LogTemp,
                Error,
                TEXT(
                    "BH_TEST_MISSION_CACHE_TRANSFER result=failure "
                    "stage=retrieve stored=%s store_owner_has=%d "
                    "retrieve_owner_has=%d"
                ),
                *MissionCache->GetStoredMissionItemID().ToString(),
                StoreOwner->HasKeycard(MissionItemID) ? 1 : 0,
                RetrieveOwner->HasKeycard(MissionItemID) ? 1 : 0
            );
            GetWorldTimerManager().ClearTimer(
                MissionCacheTransferRuntimeTestTimer
            );
            return;
        }

        BHTestMissionCacheTransferPhase = 3;
        UE_LOG(
            LogTemp,
            Display,
            TEXT(
                "BH_TEST_MISSION_CACHE_TRANSFER stage=retrieve "
                "result=success owner=%s"
            ),
            *RetrieveOwner->GetName()
        );
        return;
    }

    const bool bPassed =
        MissionCache->GetStoredMissionItemID().IsNone() &&
        !StoreOwner->HasKeycard(MissionItemID) &&
        RetrieveOwner->HasKeycard(MissionItemID);
    bBHTestMissionCacheTransferIssued = true;
    UE_LOG(
        LogTemp,
        Display,
        TEXT(
            "BH_TEST_MISSION_CACHE_TRANSFER result=%s "
            "container=%s item=%s store_owner=%s retrieve_owner=%s "
            "owner_a_has=%d owner_b_has=%d stored=%s"
        ),
        bPassed ? TEXT("success") : TEXT("failure"),
        *MissionCache->GetPersistenceID().ToString(),
        *MissionItemID.ToString(),
        *StoreOwner->GetName(),
        *RetrieveOwner->GetName(),
        StoreOwner->HasKeycard(MissionItemID) ? 1 : 0,
        RetrieveOwner->HasKeycard(MissionItemID) ? 1 : 0,
        *MissionCache->GetStoredMissionItemID().ToString()
    );
    GetWorldTimerManager().ClearTimer(MissionCacheTransferRuntimeTestTimer);
}

void ABHWarGameState::RunArmoredThreatTargetSelectionRuntimeTest()
{
    if (bBHTestArmoredThreatTargetingIssued || !HasAuthority())
    {
        return;
    }

    TArray<ABHCharacter*> Players;
    for (FConstPlayerControllerIterator Iterator =
             GetWorld()->GetPlayerControllerIterator();
         Iterator;
         ++Iterator)
    {
        APlayerController* PlayerController = Iterator->Get();
        ABHCharacter* Player = IsValid(PlayerController)
            ? Cast<ABHCharacter>(PlayerController->GetPawn())
            : nullptr;
        if (IsValid(Player) && Player->HasAuthority() &&
            Player->IsPlayerControlled())
        {
            Players.Add(Player);
        }
    }

    ABHArmoredThreat* Threat = nullptr;
    for (TActorIterator<ABHArmoredThreat> It(GetWorld()); It; ++It)
    {
        if (IsValid(*It) &&
            It->GetPersistenceID() == FName(TEXT("FirstLightArmoredThreat01")))
        {
            Threat = *It;
            break;
        }
    }

    if (Players.Num() < 2 || !IsValid(Threat))
    {
        ++BHTestArmoredThreatTargetingAttempts;
        if (BHTestArmoredThreatTargetingAttempts < 20)
        {
            return;
        }

        bBHTestArmoredThreatTargetingIssued = true;
        UE_LOG(
            LogTemp,
            Error,
            TEXT(
                "BH_TEST_ARMORED_THREAT_TARGETING result=failure "
                "stage=setup players=%d threat=%d"
            ),
            Players.Num(),
            IsValid(Threat) ? 1 : 0
        );
        GetWorldTimerManager().ClearTimer(
            ArmoredThreatTargetSelectionRuntimeTestTimer
        );
        return;
    }

    if (BHTestArmoredThreatTargetingPhase == 0)
    {
        auto PositionTestPlayer = [](ABHCharacter* Player,
                                     const FVector& Location)
        {
            if (IsValid(Player->GetCharacterMovement()))
            {
                Player->GetCharacterMovement()->StopMovementImmediately();
                Player->GetCharacterMovement()->SetMovementMode(MOVE_None);
            }
            Player->SetActorLocation(
                Location,
                false,
                nullptr,
                ETeleportType::TeleportPhysics
            );
        };
        const float ContactRange = Threat->GetContactRange();
        const float NearDistance = FMath::Clamp(
            ContactRange * 0.20f,
            500.0f,
            900.0f
        );
        const float FarDistance = FMath::Max(
            NearDistance + 300.0f,
            FMath::Min(ContactRange * 0.55f, ContactRange - 250.0f)
        );
        const float OutOfRangeDistance = ContactRange + 600.0f;

        TArray<FVector> Directions;
        Directions.Add(FVector(1.0f, 0.0f, 0.0f));
        Directions.Add(FVector(0.0f, 1.0f, 0.0f));
        Directions.Add(FVector(-1.0f, 0.0f, 0.0f));
        Directions.Add(FVector(0.0f, -1.0f, 0.0f));

        auto IsVisibleFromThreat = [this, Threat, &Players](
                                        const FVector& TargetLocation)
        {
            FHitResult Hit;
            FCollisionQueryParams Params(
                SCENE_QUERY_STAT(ArmoredThreatTargetingFixture),
                true,
                Threat
            );
            for (ABHCharacter* Player : Players)
            {
                Params.AddIgnoredActor(Player);
            }

            const bool bBlocked = GetWorld()->LineTraceSingleByChannel(
                Hit,
                Threat->GetActorLocation() + FVector(0.0f, 0.0f, 80.0f),
                TargetLocation + FVector(0.0f, 0.0f, 60.0f),
                ECC_Visibility,
                Params
            );
            return !bBlocked;
        };

        bool bFoundVisiblePair = false;
        for (int32 FirstDirectionIndex = 0;
             FirstDirectionIndex < Directions.Num() && !bFoundVisiblePair;
             ++FirstDirectionIndex)
        {
            for (int32 SecondDirectionIndex = 0;
                 SecondDirectionIndex < Directions.Num();
                 ++SecondDirectionIndex)
            {
                if (FirstDirectionIndex == SecondDirectionIndex)
                {
                    continue;
                }

                const FVector FirstFarLocation = Threat->GetActorLocation() +
                    Directions[FirstDirectionIndex] * FarDistance;
                const FVector SecondNearLocation = Threat->GetActorLocation() +
                    Directions[SecondDirectionIndex] * NearDistance;
                if (!IsVisibleFromThreat(FirstFarLocation) ||
                    !IsVisibleFromThreat(SecondNearLocation))
                {
                    continue;
                }

                BHTestArmoredThreatTargetingFirstFarLocation =
                    FirstFarLocation;
                BHTestArmoredThreatTargetingSecondNearLocation =
                    SecondNearLocation;
                BHTestArmoredThreatTargetingFirstNearLocation =
                    Threat->GetActorLocation() +
                    Directions[FirstDirectionIndex] * NearDistance;
                BHTestArmoredThreatTargetingSecondOutOfRangeLocation =
                    Threat->GetActorLocation() +
                    Directions[SecondDirectionIndex] * OutOfRangeDistance;
                bFoundVisiblePair = true;
                break;
            }
        }

        if (!bFoundVisiblePair)
        {
            bBHTestArmoredThreatTargetingIssued = true;
            UE_LOG(
                LogTemp,
                Error,
                TEXT(
                    "BH_TEST_ARMORED_THREAT_TARGETING result=failure "
                    "stage=visibility threat=%s"
                ),
                *Threat->GetPersistenceID().ToString()
            );
            GetWorldTimerManager().ClearTimer(
                ArmoredThreatTargetSelectionRuntimeTestTimer
            );
            return;
        }

        PositionTestPlayer(
            Players[0],
            BHTestArmoredThreatTargetingFirstFarLocation
        );
        PositionTestPlayer(
            Players[1],
            BHTestArmoredThreatTargetingSecondNearLocation
        );
        BHTestArmoredThreatTargetingPhase = 1;
        BHTestArmoredThreatTargetingAttempts = 0;
        UE_LOG(
            LogTemp,
            Display,
            TEXT(
                "BH_TEST_ARMORED_THREAT_TARGETING stage=setup "
                "result=success threat=%s first_far=%s second_near=%s"
            ),
            *Threat->GetPersistenceID().ToString(),
            *Players[0]->GetName(),
            *Players[1]->GetName()
        );
        return;
    }

    if (BHTestArmoredThreatTargetingPhase == 1)
    {
        if (Threat->HasPlayerContact() &&
            Threat->GetCurrentTarget() == Players[1])
        {
            UE_LOG(
                LogTemp,
                Display,
                TEXT(
                    "BH_TEST_ARMORED_THREAT_TARGETING stage=initial "
                    "result=success initial_target=%s"
                ),
                *Players[1]->GetName()
            );
            auto RepositionTestPlayer = [](ABHCharacter* Player,
                                           const FVector& Location)
            {
                Player->SetActorLocation(
                    Location,
                    false,
                    nullptr,
                    ETeleportType::TeleportPhysics
                );
            };
            RepositionTestPlayer(
                Players[0],
                BHTestArmoredThreatTargetingFirstNearLocation
            );
            RepositionTestPlayer(
                Players[1],
                BHTestArmoredThreatTargetingSecondOutOfRangeLocation
            );
            BHTestArmoredThreatTargetingPhase = 2;
            BHTestArmoredThreatTargetingAttempts = 0;
            return;
        }

        ++BHTestArmoredThreatTargetingAttempts;
        if (BHTestArmoredThreatTargetingAttempts < 20)
        {
            return;
        }

        bBHTestArmoredThreatTargetingIssued = true;
        UE_LOG(
            LogTemp,
            Error,
            TEXT(
                "BH_TEST_ARMORED_THREAT_TARGETING result=failure "
                "stage=initial contact=%d target=%s"
            ),
            Threat->HasPlayerContact() ? 1 : 0,
            IsValid(Threat->GetCurrentTarget())
                ? *Threat->GetCurrentTarget()->GetName()
                : TEXT("None")
        );
        GetWorldTimerManager().ClearTimer(
            ArmoredThreatTargetSelectionRuntimeTestTimer
        );
        return;
    }

    if (BHTestArmoredThreatTargetingPhase == 2 &&
        Threat->HasPlayerContact() &&
        Threat->GetCurrentTarget() == Players[0])
    {
        bBHTestArmoredThreatTargetingIssued = true;
        UE_LOG(
            LogTemp,
            Display,
            TEXT(
                "BH_TEST_ARMORED_THREAT_TARGETING result=success "
                "threat=%s initial_target=%s switched_target=%s"
            ),
            *Threat->GetPersistenceID().ToString(),
            *Players[1]->GetName(),
            *Players[0]->GetName()
        );
        GetWorldTimerManager().ClearTimer(
            ArmoredThreatTargetSelectionRuntimeTestTimer
        );
        return;
    }

    ++BHTestArmoredThreatTargetingAttempts;
    if (BHTestArmoredThreatTargetingAttempts < 20)
    {
        return;
    }

    bBHTestArmoredThreatTargetingIssued = true;
    UE_LOG(
        LogTemp,
        Error,
        TEXT(
            "BH_TEST_ARMORED_THREAT_TARGETING result=failure "
            "stage=switch contact=%d target=%s"
        ),
        Threat->HasPlayerContact() ? 1 : 0,
        IsValid(Threat->GetCurrentTarget())
            ? *Threat->GetCurrentTarget()->GetName()
            : TEXT("None")
    );
    GetWorldTimerManager().ClearTimer(
        ArmoredThreatTargetSelectionRuntimeTestTimer
    );
}

void ABHWarGameState::RunWeaponRoleRuntimeTest()
{
    if (bBHTestWeaponRoleRuntimeIssued || !HasAuthority())
    {
        return;
    }

    ABHCharacter* Player = nullptr;
    for (TActorIterator<ABHCharacter> It(GetWorld()); It; ++It)
    {
        if (IsValid(*It) && (*It)->HasAuthority() &&
            (*It)->IsPlayerControlled())
        {
            Player = *It;
            break;
        }
    }

    UBHWeaponComponent* Weapon = IsValid(Player)
        ? Player->GetWeaponComponent()
        : nullptr;
    if (!IsValid(Weapon))
    {
        return;
    }

    const EBHWeaponRole OriginalRole = Weapon->GetWeaponRole();
    const int32 OriginalMagazineAmmo = Weapon->GetMagazineAmmo();
    const int32 OriginalReserveAmmo = Weapon->GetReserveAmmo();
    ON_SCOPE_EXIT
    {
        // Leave the player's fixture intact for inventory and subsequent runtime checks.
        const bool bRoleRestored = Weapon->EquipWeaponRole(OriginalRole, true);
        const bool bAmmoRestored = Weapon->RestoreAmmoState(
            OriginalMagazineAmmo,
            OriginalReserveAmmo
        );
        const bool bRestored = bRoleRestored && bAmmoRestored &&
            Weapon->GetWeaponRole() == OriginalRole &&
            Weapon->GetMagazineAmmo() == OriginalMagazineAmmo &&
            Weapon->GetReserveAmmo() == OriginalReserveAmmo;
        if (!bRestored)
        {
            UE_LOG(LogTemp, Error, TEXT("BH_TEST_WEAPON_ROLE_RUNTIME_RESTORE result=failure"));
        }
        else
        {
            UE_LOG(
                LogTemp,
                Display,
                TEXT("BH_TEST_WEAPON_ROLE_RUNTIME_RESTORE result=success role=%d magazine=%d reserve=%d"),
                static_cast<int32>(OriginalRole),
                OriginalMagazineAmmo,
                OriginalReserveAmmo
            );
        }
    };

    const bool bMarksman = Weapon->EquipWeaponRole(EBHWeaponRole::Marksman, true);
    const bool bSupport = Weapon->EquipWeaponRole(EBHWeaponRole::Support, true);
    const bool bCarbine = Weapon->EquipWeaponRole(EBHWeaponRole::Carbine, true);
    const bool bPistol = Weapon->EquipWeaponRole(EBHWeaponRole::Pistol, true);
    const bool bShotgun = Weapon->EquipWeaponRole(EBHWeaponRole::Shotgun, true);
    const FBHRifleConfig* Config = IsValid(Weapon->GetEquippedRifle())
        ? &Weapon->GetEquippedRifle()->GetConfig()
        : nullptr;
    const bool bPassed = bMarksman && bSupport && bCarbine && bPistol && bShotgun &&
        Weapon->GetWeaponRole() == EBHWeaponRole::Shotgun && Config &&
        Config->MagazineSize == 8 && !Config->bAutomatic &&
        Weapon->GetMagazineAmmo() == 8 && Weapon->GetReserveAmmo() == 48;
    bBHTestWeaponRoleRuntimeIssued = true;
    UE_LOG(
        LogTemp,
        Display,
        TEXT(
            "BH_TEST_WEAPON_ROLE_RUNTIME result=%s role=%d magazine=%d "
            "reserve=%d automatic=%d"
        ),
        bPassed ? TEXT("success") : TEXT("failure"),
        static_cast<int32>(Weapon->GetWeaponRole()),
        Weapon->GetMagazineAmmo(),
        Weapon->GetReserveAmmo(),
        Config && Config->bAutomatic ? 1 : 0
    );
    GetWorldTimerManager().ClearTimer(WeaponRoleRuntimeTestTimer);
}

void ABHWarGameState::RunFirstLightPlayableRouteTest()
{
    UWorld* World = GetWorld();
    const auto FailTest = [this](const TCHAR* Reason)
    {
        UE_LOG(
            LogTemp,
            Error,
            TEXT(
                "BH_TEST_FIRST_LIGHT_PLAYABLE_ROUTE result=failure "
                "phase=%d reason=%s"
            ),
            FirstLightPlayableRouteTestPhase,
            Reason
        );
        GetWorldTimerManager().ClearTimer(
            FirstLightPlayableRouteTestTimer
        );
        FPlatformMisc::RequestExit(false);
    };

    if (!HasAuthority() || !IsValid(World))
    {
        FailTest(TEXT("authority_or_world"));
        return;
    }

    if (FirstLightPlayableRouteTestPhase == 0)
    {
        ABHCharacter* Player = nullptr;
        ABHKeycard* Keycard = nullptr;
        ABHMissionItemContainer* MissionCache = nullptr;
        ABHExtractionZone* Extraction = nullptr;
        for (TActorIterator<ABHCharacter> It(World); It; ++It)
        {
            if (It->HasAuthority() &&
                IsValid(It->GetPlayerState()))
            {
                Player = *It;
                break;
            }
        }
        if (!IsValid(Player))
        {
            for (FConstPlayerControllerIterator It =
                     World->GetPlayerControllerIterator(); It; ++It)
            {
                APlayerController* Controller = It->Get();
                ABHCharacter* PossessedPlayer = IsValid(Controller)
                    ? Cast<ABHCharacter>(Controller->GetPawn())
                    : nullptr;
                if (IsValid(PossessedPlayer) &&
                    PossessedPlayer->HasAuthority())
                {
                    Player = PossessedPlayer;
                    break;
                }
            }
        }
        for (TActorIterator<ABHKeycard> It(World); It; ++It)
        {
            if (It->GetPersistenceID() ==
                FName(TEXT("FirstLightRedKeycard")))
            {
                Keycard = *It;
                break;
            }
        }
        for (TActorIterator<ABHMissionItemContainer> It(World); It; ++It)
        {
            if (It->GetPersistenceID() ==
                FName(TEXT("FirstLightMissionCache01")))
            {
                MissionCache = *It;
                break;
            }
        }
        for (TActorIterator<ABHExtractionZone> It(World); It; ++It)
        {
            Extraction = *It;
            break;
        }

        if (!IsValid(Player))
        {
            UE_LOG(
                LogTemp,
                Display,
                TEXT("BH_TEST_FIRST_LIGHT_PLAYABLE_ROUTE waiting_for_player")
            );
            return;
        }

        if (!IsValid(Keycard) || !IsValid(MissionCache) ||
            !MissionCache->Implements<UBHInteractable>() ||
            !IsValid(Extraction) ||
            Player->GetCurrentObjectiveID() !=
                BHObjectiveIds::FindRedKeycard)
        {
            UE_LOG(
                LogTemp,
                Warning,
                TEXT(
                    "BH_TEST_FIRST_LIGHT_PLAYABLE_ROUTE_DIAGNOSTIC "
                    "player=%s keycard=%s mission_cache=%s "
                    "extraction=%s objective=%s"
                ),
                IsValid(Player) ? TEXT("true") : TEXT("false"),
                IsValid(Keycard) ? TEXT("true") : TEXT("false"),
                IsValid(MissionCache) ? TEXT("true") : TEXT("false"),
                IsValid(Extraction) ? TEXT("true") : TEXT("false"),
                IsValid(Player)
                    ? *Player->GetCurrentObjectiveID().ToString()
                    : TEXT("none")
            );
            FailTest(TEXT("route_actors_or_initial_objective"));
            return;
        }

        FirstLightPlayableRouteTestPlayer = Player;
        FirstLightPlayableRouteTestExtraction = Extraction;
        IBHInteractable::Execute_Interact(Keycard, Player);
        if (!Player->HasKeycard(FName(TEXT("RedKeycard"))) ||
            Player->GetCurrentObjectiveID() !=
                BHObjectiveIds::UnlockSecurityDoor)
        {
            FailTest(TEXT("keycard_interaction"));
            return;
        }
        UE_LOG(
            LogTemp,
            Display,
            TEXT(
                "BH_TEST_FIRST_LIGHT_PLAYABLE_ROUTE step=keycard "
                "result=success"
            )
        );

        const FName RedKeycardID = FName(TEXT("RedKeycard"));
        if (MissionCache->GetStoredMissionItemID() != NAME_None)
        {
            FailTest(TEXT("mission_cache_initial_state"));
            return;
        }

        IBHInteractable::Execute_Interact(MissionCache, Player);
        if (MissionCache->GetStoredMissionItemID() != RedKeycardID ||
            Player->HasKeycard(RedKeycardID) ||
            Player->GetCurrentObjectiveID() !=
                BHObjectiveIds::UnlockSecurityDoor)
        {
            FailTest(TEXT("mission_cache_store"));
            return;
        }
        UE_LOG(
            LogTemp,
            Display,
            TEXT(
                "BH_TEST_FIRST_LIGHT_PLAYABLE_ROUTE "
                "step=mission_cache_store result=success "
                "container=%s"
            ),
            *MissionCache->GetPersistenceID().ToString()
        );

        IBHInteractable::Execute_Interact(MissionCache, Player);
        if (MissionCache->GetStoredMissionItemID() != NAME_None ||
            !Player->HasKeycard(RedKeycardID) ||
            Player->GetCurrentObjectiveID() !=
                BHObjectiveIds::UnlockSecurityDoor)
        {
            FailTest(TEXT("mission_cache_retrieve"));
            return;
        }
        UE_LOG(
            LogTemp,
            Display,
            TEXT(
                "BH_TEST_FIRST_LIGHT_PLAYABLE_ROUTE "
                "step=mission_cache_retrieve result=success "
                "container=%s"
            ),
            *MissionCache->GetPersistenceID().ToString()
        );
    }
    else if (FirstLightPlayableRouteTestPhase == 1)
    {
        ABHCharacter* Player = FirstLightPlayableRouteTestPlayer.Get();
        if (!IsValid(Player))
        {
            for (TActorIterator<ABHCharacter> It(World); It; ++It)
            {
                if (It->HasAuthority() && IsValid(It->GetPlayerState()))
                {
                    Player = *It;
                    FirstLightPlayableRouteTestPlayer = Player;
                    break;
                }
            }
        }
        ABHDoor* Door = nullptr;
        for (TActorIterator<ABHDoor> It(World); It; ++It)
        {
            if (It->GetPersistenceID() ==
                FName(TEXT("FirstLightSecurityDoor")))
            {
                Door = *It;
                break;
            }
        }
        if (!IsValid(Player) || !IsValid(Door))
        {
            FailTest(TEXT("security_door"));
            return;
        }
        IBHInteractable::Execute_Interact(Door, Player);
        if (!Door->IsUnlocked() ||
            Player->GetCurrentObjectiveID() !=
                BHObjectiveIds::EliminateGuard)
        {
            FailTest(TEXT("door_interaction"));
            return;
        }
        UE_LOG(
            LogTemp,
            Display,
            TEXT(
                "BH_TEST_FIRST_LIGHT_PLAYABLE_ROUTE step=door "
                "result=success"
            )
        );
    }
    else if (FirstLightPlayableRouteTestPhase == 2)
    {
        ABHCharacter* Player = FirstLightPlayableRouteTestPlayer.Get();
        if (!IsValid(Player))
        {
            for (TActorIterator<ABHCharacter> It(World); It; ++It)
            {
                if (It->HasAuthority() && IsValid(It->GetPlayerState()))
                {
                    Player = *It;
                    FirstLightPlayableRouteTestPlayer = Player;
                    break;
                }
            }
        }
        ABHExtractionZone* Extraction =
            FirstLightPlayableRouteTestExtraction.Get();
        if (!IsValid(Player) || !IsValid(Extraction) ||
            Player->GetCurrentObjectiveID() != BHObjectiveIds::EliminateGuard)
        {
            FailTest(TEXT("pre_guard_extraction_setup"));
            return;
        }

        const FVector PriorLocation = Player->GetActorLocation();
        if (!Player->SetActorLocation(
                Extraction->GetActorLocation(),
                false,
                nullptr,
                ETeleportType::TeleportPhysics
            ))
        {
            FailTest(TEXT("pre_guard_extraction_approach"));
            return;
        }
        Player->UpdateOverlaps();

        const bool bGatePreserved =
            Player->GetCurrentObjectiveID() == BHObjectiveIds::EliminateGuard &&
            !Player->IsObjectiveCompleted(BHObjectiveIds::EliminateGuard) &&
            !Player->IsMissionComplete();
        const bool bRestored = Player->SetActorLocation(
            PriorLocation,
            false,
            nullptr,
            ETeleportType::TeleportPhysics
        );
        Player->UpdateOverlaps();
        if (!bGatePreserved)
        {
            FailTest(TEXT("pre_guard_extraction_gate"));
            return;
        }
        if (!bRestored)
        {
            FailTest(TEXT("pre_guard_extraction_restore"));
            return;
        }
        UE_LOG(
            LogTemp,
            Display,
            TEXT(
                "BH_TEST_FIRST_LIGHT_PLAYABLE_ROUTE "
                "step=pre_guard_extraction_gate result=success "
                "objective=%s mission_complete=%d"
            ),
            *Player->GetCurrentObjectiveID().ToString(),
            Player->IsMissionComplete() ? 1 : 0
        );
    }
    else if (FirstLightPlayableRouteTestPhase == 3)
    {
        ABHCharacter* Player = FirstLightPlayableRouteTestPlayer.Get();
        if (!IsValid(Player))
        {
            for (TActorIterator<ABHCharacter> It(World); It; ++It)
            {
                if (It->HasAuthority() && IsValid(It->GetPlayerState()))
                {
                    Player = *It;
                    FirstLightPlayableRouteTestPlayer = Player;
                    break;
                }
            }
        }
        TArray<ABHEnemySoldier*> ObjectiveGuards;
        int32 ObjectiveGuardCandidates = 0;
        for (TActorIterator<ABHEnemySoldier> It(World); It; ++It)
        {
            const bool bHasRouteObjective =
                It->GetObjectiveIdToCompleteOnDeath() ==
                BHObjectiveIds::EliminateGuard;
            if (bHasRouteObjective)
            {
                ++ObjectiveGuardCandidates;
                UE_LOG(
                    LogTemp,
                    Display,
                    TEXT(
                        "BH_TEST_FIRST_LIGHT_PLAYABLE_ROUTE "
                        "guard_candidate name=%s faction=%d dead=%d "
                        "defense_tag=%d objective=%s health=%.1f"
                    ),
                    *It->GetName(),
                    static_cast<int32>(It->GetCombatFaction()),
                    It->IsDead() ? 1 : 0,
                    It->ActorHasTag(
                        FName(TEXT("BH_Auto_DefenseA_Garrison"))
                    ) ? 1 : 0,
                    *It->GetObjectiveIdToCompleteOnDeath().ToString(),
                    IsValid(It->GetHealthComponent())
                        ? It->GetHealthComponent()->GetCurrentHealth()
                        : -1.0f
                );
            }

            if (!It->IsDead() &&
                It->GetCombatFaction() ==
                    EBHCombatFaction::Hostile &&
                !It->ActorHasTag(
                    FName(TEXT("BH_Auto_DefenseA_Garrison"))
                ) &&
                bHasRouteObjective)
            {
                ObjectiveGuards.Add(*It);
            }
        }
        UE_LOG(
            LogTemp,
            Display,
            TEXT(
                "BH_TEST_FIRST_LIGHT_PLAYABLE_ROUTE "
                "guard_selection candidates=%d selected=%d"
            ),
            ObjectiveGuardCandidates,
            ObjectiveGuards.Num()
        );
        if (!IsValid(Player))
        {
            FailTest(TEXT("route_player"));
            return;
        }
        if (ObjectiveGuardCandidates != 3 ||
            ObjectiveGuards.Num() != 3)
        {
            FailTest(TEXT("objective_guards_contract"));
            return;
        }
        for (ABHEnemySoldier* Guard : ObjectiveGuards)
        {
            UBHHealthComponent* Health = Guard->GetHealthComponent();
            if (!IsValid(Health) ||
                Health->ApplyDamage(
                    Health->GetCurrentHealth() + 1.0f,
                    Player
                ) <= 0.0f)
            {
                FailTest(TEXT("guard_damage"));
                return;
            }
            ++FirstLightPlayableRouteEnemiesDefeated;
        }
        ++FirstLightPlayableRouteCombatPasses;
        if (Player->GetCurrentObjectiveID() ==
                BHObjectiveIds::EliminateGuard)
        {
            if (FirstLightPlayableRouteCombatPasses >= 60)
            {
                FailTest(TEXT("guard_or_defense_wave_timeout"));
            }
            return;
        }
        if (Player->GetCurrentObjectiveID() !=
                BHObjectiveIds::ReachExtraction ||
            !Player->IsObjectiveCompleted(
                BHObjectiveIds::EliminateGuard))
        {
            FailTest(TEXT("guard_group_completion"));
            return;
        }
        UE_LOG(
            LogTemp,
            Display,
            TEXT(
                "BH_TEST_FIRST_LIGHT_PLAYABLE_ROUTE step=guards "
                "result=success count=%d passes=%d"
            ),
            FirstLightPlayableRouteEnemiesDefeated,
            FirstLightPlayableRouteCombatPasses
        );
    }
    else if (FirstLightPlayableRouteTestPhase == 4)
    {
        ABHCharacter* Player = FirstLightPlayableRouteTestPlayer.Get();
        if (!IsValid(Player))
        {
            for (TActorIterator<ABHCharacter> It(World); It; ++It)
            {
                if (It->HasAuthority() && IsValid(It->GetPlayerState()))
                {
                    Player = *It;
                    FirstLightPlayableRouteTestPlayer = Player;
                    break;
                }
            }
        }
        ABHAmmoSupply* RuntimeAmmoDrop = nullptr;
        for (TActorIterator<ABHAmmoSupply> It(World); It; ++It)
        {
            if (It->IsRuntimeSupply() && !It->IsConsumed())
            {
                RuntimeAmmoDrop = *It;
                break;
            }
        }

        UBHWeaponComponent* Weapon = IsValid(Player)
            ? Player->GetWeaponComponent()
            : nullptr;
        if (!IsValid(RuntimeAmmoDrop) || !IsValid(Weapon))
        {
            FailTest(TEXT("runtime_ammo_drop_or_weapon"));
            return;
        }

        const int32 PickupRounds =
            RuntimeAmmoDrop->GetReserveAmmoAmount();
        const int32 MaxReserveAmmo = Weapon->GetMaxReserveAmmo();
        const int32 PreparedReserveAmmo = FMath::Max(
            0,
            MaxReserveAmmo - FMath::Max(1, PickupRounds)
        );
        if (PickupRounds <= 0 || MaxReserveAmmo <= 0 ||
            !Weapon->RestoreAmmoState(
                Weapon->GetMagazineAmmo(),
                PreparedReserveAmmo
            ))
        {
            FailTest(TEXT("runtime_ammo_drop_prepare"));
            return;
        }

        const int32 ReserveAmmoBefore = Weapon->GetReserveAmmo();
        const int32 ExpectedReserveAmmo = FMath::Min(
            MaxReserveAmmo,
            ReserveAmmoBefore + PickupRounds
        );
        IBHInteractable::Execute_Interact(RuntimeAmmoDrop, Player);
        const int32 ReserveAmmoAfter = Weapon->GetReserveAmmo();
        if (!RuntimeAmmoDrop->IsConsumed() ||
            ReserveAmmoAfter != ExpectedReserveAmmo ||
            ReserveAmmoAfter <= ReserveAmmoBefore)
        {
            FailTest(TEXT("runtime_ammo_drop_interaction"));
            return;
        }

        UE_LOG(
            LogTemp,
            Display,
            TEXT(
                "BH_TEST_FIRST_LIGHT_PLAYABLE_ROUTE step=ammo_drop "
                "result=success before=%d after=%d rounds=%d"
            ),
            ReserveAmmoBefore,
            ReserveAmmoAfter,
            PickupRounds
        );
    }
    else if (FirstLightPlayableRouteTestPhase == 5)
    {
        ABHCharacter* Player = FirstLightPlayableRouteTestPlayer.Get();
        if (!IsValid(Player))
        {
            for (TActorIterator<ABHCharacter> It(World); It; ++It)
            {
                if (It->HasAuthority() && IsValid(It->GetPlayerState()))
                {
                    Player = *It;
                    FirstLightPlayableRouteTestPlayer = Player;
                    break;
                }
            }
        }
        ABHExtractionZone* Extraction =
            FirstLightPlayableRouteTestExtraction.Get();
        if (!IsValid(Player) || !IsValid(Extraction) ||
            !Player->SetActorLocation(
                Extraction->GetActorLocation() +
                    FVector(0.0f, 0.0f, 500.0f),
                false,
                nullptr,
                ETeleportType::TeleportPhysics
            ))
        {
            FailTest(TEXT("extraction_approach"));
            return;
        }
    }
    else if (FirstLightPlayableRouteTestPhase == 6)
    {
        ABHCharacter* Player = FirstLightPlayableRouteTestPlayer.Get();
        if (!IsValid(Player))
        {
            for (TActorIterator<ABHCharacter> It(World); It; ++It)
            {
                if (It->HasAuthority() && IsValid(It->GetPlayerState()))
                {
                    Player = *It;
                    FirstLightPlayableRouteTestPlayer = Player;
                    break;
                }
            }
        }
        ABHExtractionZone* Extraction =
            FirstLightPlayableRouteTestExtraction.Get();
        if (!IsValid(Player) || !IsValid(Extraction) ||
            !Player->SetActorLocation(
                Extraction->GetActorLocation(),
                false,
                nullptr,
                ETeleportType::TeleportPhysics
            ))
        {
            FailTest(TEXT("extraction_overlap"));
            return;
        }
        Player->UpdateOverlaps();
    }
    else
    {
        ABHCharacter* Player = FirstLightPlayableRouteTestPlayer.Get();
        if (!IsValid(Player))
        {
            for (TActorIterator<ABHCharacter> It(World); It; ++It)
            {
                if (It->HasAuthority() && IsValid(It->GetPlayerState()))
                {
                    Player = *It;
                    FirstLightPlayableRouteTestPlayer = Player;
                    break;
                }
            }
        }
        TArray<ABHCharacter*> Players;
        int32 CompletedPlayerCount = 0;
        for (TActorIterator<ABHCharacter> It(World); It; ++It)
        {
            if (It->HasAuthority() && IsValid(It->GetPlayerState()))
            {
                Players.Add(*It);
                if (It->IsMissionComplete() &&
                    It->GetCompletedObjectiveIDs().Num() == 4 &&
                    It->IsObjectiveCompleted(
                        BHObjectiveIds::ReachExtraction))
                {
                    ++CompletedPlayerCount;
                }
            }
        }
        const bool bCompleted = IsValid(Player) &&
            Player->IsMissionComplete() &&
            Player->GetCompletedObjectiveIDs().Num() == 4 &&
            Player->IsObjectiveCompleted(
                BHObjectiveIds::ReachExtraction) &&
            Players.Num() > 0 &&
            CompletedPlayerCount == Players.Num();
        if (!bCompleted)
        {
            FailTest(TEXT("mission_completion"));
            return;
        }
        UE_LOG(
            LogTemp,
            Display,
            TEXT(
                "BH_TEST_FIRST_LIGHT_PLAYABLE_ROUTE_COMPLETE "
                "result=success objectives=%d players=%d completed=%d"
            ),
            Player->GetCompletedObjectiveIDs().Num(),
            Players.Num(),
            CompletedPlayerCount
        );
        GetWorldTimerManager().ClearTimer(
            FirstLightPlayableRouteTestTimer
        );
        if (!IsRunningDedicatedServer() &&
            !FParse::Param(
                FCommandLine::Get(),
                TEXT("BHTestNavigationGrenade")))
        {
            FPlatformMisc::RequestExit(false);
        }
        return;
    }

    ++FirstLightPlayableRouteTestPhase;
}

void ABHWarGameState::RunDefenseAGarrisonPersistenceTest()
{
    UWorld* World = GetWorld();
    const bool bMultiplayer = BHDefenseAMultiplayerTest::IsHost();
    const auto FailTest = [this](const TCHAR* Reason)
    {
        UE_LOG(
            LogTemp,
            Error,
            TEXT(
                "BH_TEST_DEFENSE_A_GARRISON result=failure "
                "phase=%d reason=%s"
            ),
            BHTestDefenseAGarrisonPhase,
            Reason
        );
        GetWorldTimerManager().ClearTimer(
            DefenseAGarrisonPersistenceTestTimer
        );
        if (BHDefenseAMultiplayerTest::IsEnabled()) { BHDefenseAMultiplayerTest::Fail(Reason); }
        FPlatformMisc::RequestExit(false);
    };

    if (!HasAuthority() || !IsValid(World))
    {
        FailTest(TEXT("authority_or_world"));
        return;
    }

    ABHCharacter* Player = nullptr;
    TArray<ABHCharacter*> Participants;
    for (TActorIterator<ABHCharacter> It(World); It; ++It)
    {
        if (It->HasAuthority() && It->IsPlayerControlled())
        {
            APlayerController* Controller = Cast<APlayerController>(It->GetController());
            UNetConnection* Connection = IsValid(Controller) ? Controller->GetNetConnection() : nullptr;
            if (bMultiplayer && (!IsValid(Connection) || Connection->GetConnectionState() != USOCK_Open))
            {
                continue;
            }
            Participants.Add(*It);
            if (!IsValid(Player)) { Player = *It; }
        }
    }
    if (bMultiplayer && Participants.Num() != 2)
    {
        if (BHTestDefenseAGarrisonPhase > 0) { FailTest(TEXT("two_participants_lost")); }
        return;
    }

    if (!IsValid(Player))
    {
        return;
    }

    TArray<ABHEnemySoldier*> Garrison;
    for (TActorIterator<ABHEnemySoldier> It(World); It; ++It)
    {
        if (IsValid(*It) &&
            It->ActorHasTag(FName(TEXT("BH_Auto_DefenseA_Garrison"))))
        {
            Garrison.Add(*It);
        }
    }

    if (Garrison.IsEmpty())
    {
        FailTest(TEXT("authored_garrison_missing"));
        return;
    }

    const auto CountLivingActiveAuthoredGarrison =
        [&Garrison]() -> int32
    {
        int32 LivingActiveCount = 0;
        for (ABHEnemySoldier* Enemy : Garrison)
        {
            if (IsValid(Enemy) &&
                !Enemy->IsDead() &&
                Enemy->IsOperationGarrisonActive())
            {
                ++LivingActiveCount;
            }
        }

        return LivingActiveCount;
    };

    const auto GatherRuntimeOperationHostiles =
        [World]() -> TArray<ABHEnemySoldier*>
    {
        TArray<ABHEnemySoldier*> Hostiles;
        for (TActorIterator<ABHEnemySoldier> It(World); It; ++It)
        {
            ABHEnemySoldier* Enemy = *It;
            if (IsValid(Enemy) &&
                !Enemy->IsDead() &&
                Enemy->HasAnyFlags(RF_Transient) &&
                Enemy->GetCombatFaction() ==
                    EBHCombatFaction::Hostile &&
                Enemy->GetObjectiveIdToCompleteOnDeath().IsNone() &&
                !Enemy->ActorHasTag(
                    FName(TEXT("BH_Auto_DefenseA_Garrison"))))
            {
                Hostiles.Add(Enemy);
            }
        }

        return Hostiles;
    };

    const auto KillOperationEnemy =
        [Player](ABHEnemySoldier* Enemy) -> bool
    {
        UBHHealthComponent* Health =
            IsValid(Enemy) ? Enemy->GetHealthComponent() : nullptr;
        return IsValid(Health) &&
            Health->ApplyDamage(
                Health->GetCurrentHealth() + 1.0f,
                Player
            ) > 0.0f;
    };

    const auto KillOperationHostiles =
        [&KillOperationEnemy](const TArray<ABHEnemySoldier*>& Hostiles)
            -> int32
    {
        int32 KilledCount = 0;
        for (ABHEnemySoldier* Enemy : Hostiles)
        {
            if (KillOperationEnemy(Enemy))
            {
                ++KilledCount;
            }
        }

        return KilledCount;
    };

    if (bMultiplayer && BHTestDefenseAGarrisonPhase > 0)
    {
        const FBHActiveOperationSnapshot Snapshot = GetActiveOperationSnapshot();
        BHDefenseAMultiplayerTest::ObserveHost(Snapshot, Garrison.Num(),
            CountLivingActiveAuthoredGarrison(), GatherRuntimeOperationHostiles().Num(), Participants.Num(), false);
        if (BHTestDefenseAGarrisonPhase <= 6 && Snapshot.OperationState.bOperationActivated &&
            !BHDefenseAMultiplayerTest::HasControl(TEXT("secure.ready")))
        {
            // Position the real actors counted by securing; production clocks continue normally.
            const FVector Outside = FVector(Snapshot.OperationCenter) + FVector(1800.0f, 0.0f, 90.0f);
            for (int32 Index = 0; Index < Participants.Num(); ++Index)
            {
                Participants[Index]->SetActorLocation(Outside + FVector(0.0f, Index * 200.0f, 0.0f),
                    false, nullptr, ETeleportType::TeleportPhysics);
                Participants[Index]->UpdateOverlaps();
            }
            for (TActorIterator<ABHEnemySoldier> It(World); It; ++It)
            {
                if (IsValid(*It) && !It->IsDead() && !It->IsIncapacitated() &&
                    It->GetCombatFaction() == EBHCombatFaction::Friendly && Cast<ABHCharacter>(It->GetOwner()))
                {
                    It->SetActorLocation(Outside + FVector(0.0f, -300.0f, 0.0f), false, nullptr, ETeleportType::TeleportPhysics);
                    It->UpdateOverlaps();
                }
            }
        }
    }

    if (BHTestDefenseAGarrisonPhase == 0)
    {
        const TArray<FName> ExpectedGarrisonIDs = {
            FName(TEXT("DefenseA_FL_DefenseA_Garrison_01_West")),
            FName(TEXT("DefenseA_FL_DefenseA_Garrison_02_East")),
            FName(TEXT("DefenseA_FL_DefenseA_Garrison_03_NorthWest")),
            FName(TEXT("DefenseA_FL_DefenseA_Garrison_04_NorthEast")),
            FName(TEXT("DefenseA_FL_DefenseA_Garrison_05_OuterWest")),
            FName(TEXT("DefenseA_FL_DefenseA_Garrison_06_OuterEast")),
        };
        if (Garrison.Num() != ExpectedGarrisonIDs.Num())
        {
            FailTest(TEXT("garrison_identity_count"));
            return;
        }
        for (const FName ExpectedID : ExpectedGarrisonIDs)
        {
            bool bFoundExpectedID = false;
            for (ABHEnemySoldier* Enemy : Garrison)
            {
                if (IsValid(Enemy) &&
                    Enemy->GetFieldOperativeID() == ExpectedID)
                {
                    bFoundExpectedID = true;
                    break;
                }
            }
            if (!bFoundExpectedID)
            {
                FailTest(TEXT("garrison_identity_contract"));
                return;
            }
        }
        for (ABHEnemySoldier* Enemy : Garrison)
        {
            if (!IsValid(Enemy) || Enemy->IsDead() ||
                Enemy->IsOperationGarrisonActive())
            {
                FailTest(TEXT("garrison_not_dormant"));
                return;
            }
        }

        if (bMultiplayer)
        {
            BHDefenseAMultiplayerTest::ObserveHost(GetActiveOperationSnapshot(), Garrison.Num(),
                CountLivingActiveAuthoredGarrison(), 0, Participants.Num(), true);
            // The harness acknowledges both clients' local viewport HUD readiness.
            if (!BHDefenseAMultiplayerTest::HasControl(TEXT("start.ready"))) { return; }
        }

        UBHWarSubsystem* WarSubsystem =
            GetGameInstance()
                ? GetGameInstance()->GetSubsystem<UBHWarSubsystem>()
                : nullptr;
        if (!IsValid(WarSubsystem))
        {
            FailTest(TEXT("war_subsystem_missing"));
            return;
        }

        FName OperationSectorID = NAME_None;
        for (const FBHWarSectorState& Sector :
             WarSubsystem->GetSectorStates())
        {
            if (WarSubsystem->IsViableOperation(
                    Sector.SectorID,
                    EBHWarPriorityType::Defend
                ))
            {
                OperationSectorID = Sector.SectorID;
                break;
            }
        }

        if (OperationSectorID.IsNone())
        {
            FailTest(TEXT("no_viable_defense_sector"));
            return;
        }

        ++BHTestDefenseAGarrisonWaitAttempts;
        Player->PrepareDeploymentModeForTest();
        if (!Player->BeginOperationInWorld(
                OperationSectorID,
                EBHWarPriorityType::Defend
            ))
        {
            if (BHTestDefenseAGarrisonWaitAttempts >= 30)
            {
                FailTest(TEXT("operation_start"));
            }
            return;
        }

        BHTestDefenseAGarrisonPhase = 1;
        BHTestDefenseAGarrisonWaitAttempts = 0;
        UE_LOG(
            LogTemp,
            Display,
            TEXT(
                "BH_TEST_DEFENSE_A_GARRISON step=operation_started "
                "result=success sector=%s"
            ),
            *OperationSectorID.ToString()
        );
        return;
    }

    if (BHTestDefenseAGarrisonPhase == 1)
    {
        const FBHOpenWorldOperationState State =
            Player->GetOpenWorldOperationState();
        if (!Player->IsRuntimeWarOperation() ||
            !State.bHasSnapshot ||
            !State.bOperationActivated ||
            State.OperationVariationIndex != 0)
        {
            ++BHTestDefenseAGarrisonWaitAttempts;
            if (BHTestDefenseAGarrisonWaitAttempts >= 30)
            {
                FailTest(TEXT("operation_not_active"));
            }
            return;
        }

        if (bMultiplayer && !BHDefenseAMultiplayerTest::HasControl(TEXT("clear_wave1.ready"))) { return; }
        ABHEnemySoldier* Casualty = nullptr;
        for (ABHEnemySoldier* Enemy : Garrison)
        {
            if (IsValid(Enemy) && !Enemy->IsDead() &&
                Enemy->IsOperationGarrisonActive())
            {
                Casualty = Enemy;
                break;
            }
        }

        if (!IsValid(Casualty) ||
            !IsValid(Casualty->GetHealthComponent()))
        {
            FailTest(TEXT("active_garrison_casualty_target"));
            return;
        }

        BHTestDefenseAGarrisonCasualtyID =
            Casualty->GetFieldOperativeID();
        if (BHTestDefenseAGarrisonCasualtyID.IsNone() ||
            Casualty->GetHealthComponent()->ApplyDamage(
                Casualty->GetHealthComponent()->GetCurrentHealth() + 1.0f,
                Player
            ) <= 0.0f)
        {
            FailTest(TEXT("casualty_damage"));
            return;
        }

        BHTestDefenseAGarrisonPhase = 2;
        UE_LOG(
            LogTemp,
            Display,
            TEXT(
                "BH_TEST_DEFENSE_A_GARRISON step=casualty "
                "result=success id=%s"
            ),
            *BHTestDefenseAGarrisonCasualtyID.ToString()
        );
        return;
    }

    if (BHTestDefenseAGarrisonPhase == 2)
    {
        const FBHOpenWorldOperationState State =
            Player->GetOpenWorldOperationState();
        if (!State.bHasSnapshot || State.EnemyCasualties < 1)
        {
            ++BHTestDefenseAGarrisonWaitAttempts;
            if (BHTestDefenseAGarrisonWaitAttempts >= 30)
            {
                FailTest(TEXT("casualty_not_recorded"));
            }
            return;
        }

        if (bMultiplayer)
        {
            // Standalone retains checkpoint/reload; this variation proves live replication/presentation.
            BHTestDefenseAGarrisonPhase = 3;
            BHTestDefenseAGarrisonWaitAttempts = 0;
            return;
        }

        UBHSaveSubsystem* SaveSubsystem =
            GetGameInstance()
                ? GetGameInstance()->GetSubsystem<UBHSaveSubsystem>()
                : nullptr;
        if (!IsValid(SaveSubsystem) || !SaveSubsystem->SaveProgress())
        {
            FailTest(TEXT("checkpoint_save"));
            return;
        }

        BHTestDefenseAGarrisonPhase = 3;
        if (!SaveSubsystem->LoadProgress())
        {
            FailTest(TEXT("checkpoint_load"));
            return;
        }

        UE_LOG(
            LogTemp,
            Display,
            TEXT(
                "BH_TEST_DEFENSE_A_GARRISON step=checkpoint_reload "
                "result=success"
            )
        );
        return;
    }

    if (BHTestDefenseAGarrisonPhase == 3)
    {
        const FBHOpenWorldOperationState RestoredState =
            Player->GetOpenWorldOperationState();
        if (!Player->IsRuntimeWarOperation() ||
            !RestoredState.bHasSnapshot ||
            !RestoredState.bOperationActivated)
        {
            ++BHTestDefenseAGarrisonWaitAttempts;
            if (BHTestDefenseAGarrisonWaitAttempts >= 40)
            {
                FailTest(TEXT("operation_restore"));
            }
            return;
        }

        ABHEnemySoldier* RestoredCasualty = nullptr;
        int32 LivingActiveGarrisonCount = 0;
        TArray<ABHEnemySoldier*> LivingAuthoredWave;
        for (ABHEnemySoldier* Enemy : Garrison)
        {
            if (!IsValid(Enemy))
            {
                continue;
            }

            if (Enemy->GetFieldOperativeID() ==
                BHTestDefenseAGarrisonCasualtyID)
            {
                RestoredCasualty = Enemy;
                continue;
            }

            if (!Enemy->IsDead() && Enemy->IsOperationGarrisonActive())
            {
                ++LivingActiveGarrisonCount;
                LivingAuthoredWave.Add(Enemy);
            }
        }

        // Live death retains the authored activation flag; reload restores a dormant casualty.
        // The live variation must still account for exactly one casualty and every remaining actor.
        const bool bInvalidLiveAccounting = bMultiplayer &&
            (RestoredState.EnemyCasualties != 1 ||
             LivingActiveGarrisonCount + RestoredState.EnemyCasualties != Garrison.Num());
        if (!IsValid(RestoredCasualty) ||
            !RestoredCasualty->IsDead() ||
            (!bMultiplayer && RestoredCasualty->IsOperationGarrisonActive()) ||
            LivingActiveGarrisonCount != RestoredState.LivingEnemyCount ||
            RestoredState.EnemyCasualties < 1 ||
            RestoredState.CurrentWave != 1 ||
            RestoredState.OperationVariationIndex != 0 ||
            bInvalidLiveAccounting)
        {
            FailTest(bMultiplayer ? TEXT("garrison_live_casualty_state") : TEXT("garrison_restore_state"));
            return;
        }

        const int32 ClearedCount =
            KillOperationHostiles(LivingAuthoredWave);
        if (ClearedCount != LivingActiveGarrisonCount)
        {
            FailTest(TEXT("wave1_clear_damage"));
            return;
        }

        BHTestDefenseAGarrisonPhase = 4;
        BHTestDefenseAGarrisonWaitAttempts = 0;
        UE_LOG(
            LogTemp,
            Display,
            TEXT(
                "BH_TEST_DEFENSE_A_GARRISON step=wave1_cleared "
                "result=success restored_living=%d casualties=%d"
            ),
            LivingActiveGarrisonCount,
            RestoredState.EnemyCasualties
        );
        return;
    }

    if (BHTestDefenseAGarrisonPhase == 4)
    {
        const FBHOpenWorldOperationState State =
            Player->GetOpenWorldOperationState();
        const FBHActiveOperationSnapshot Snapshot =
            GetActiveOperationSnapshot();
        if (State.bHasSnapshot &&
            State.bOperationActivated &&
            State.bWaitingForWave)
        {
            if (State.CurrentWave != 1 ||
                State.LivingEnemyCount != 0 ||
                State.SecondsUntilNextWave <= 0.0f ||
                Snapshot.Phase !=
                    EBHActiveOperationPhase::AwaitingWave ||
                !Snapshot.OperationState.bHasSnapshot ||
                Snapshot.OperationState.SecondsUntilNextWave <= 0.0f ||
                CountLivingActiveAuthoredGarrison() != 0)
            {
                FailTest(TEXT("awaiting_wave_state"));
                return;
            }

            BHTestDefenseAGarrisonPhase = 5;
            BHTestDefenseAGarrisonWaitAttempts = 0;
            UE_LOG(
                LogTemp,
                Display,
                TEXT(
                    "BH_TEST_DEFENSE_A_GARRISON "
                    "step=awaiting_wave2 result=success countdown=%.1f"
                ),
                State.SecondsUntilNextWave
            );
            return;
        }

        ++BHTestDefenseAGarrisonWaitAttempts;
        if (BHTestDefenseAGarrisonWaitAttempts >= 30)
        {
            FailTest(TEXT("awaiting_wave_timeout"));
        }
        return;
    }

    if (BHTestDefenseAGarrisonPhase == 5)
    {
        const FBHOpenWorldOperationState State =
            Player->GetOpenWorldOperationState();
        if (State.bHasSnapshot &&
            State.bOperationActivated &&
            !State.bWaitingForWave &&
            State.CurrentWave == 2 &&
            State.LivingEnemyCount > 0)
        {
            const TArray<ABHEnemySoldier*> RuntimeHostiles =
                GatherRuntimeOperationHostiles();
            if (CountLivingActiveAuthoredGarrison() != 0 ||
                RuntimeHostiles.Num() != State.LivingEnemyCount)
            {
                FailTest(TEXT("wave2_runtime_spawn"));
                return;
            }

            if (bMultiplayer && !BHDefenseAMultiplayerTest::HasControl(TEXT("clear_wave2.ready"))) { return; }
            const int32 ClearedCount =
                KillOperationHostiles(RuntimeHostiles);
            if (ClearedCount != RuntimeHostiles.Num())
            {
                FailTest(TEXT("wave2_clear_damage"));
                return;
            }

            bBHTestDefenseAWave2Spawned = true;
            BHTestDefenseAGarrisonPhase = 6;
            BHTestDefenseAGarrisonWaitAttempts = 0;
            UE_LOG(
                LogTemp,
                Display,
                TEXT(
                    "BH_TEST_DEFENSE_A_GARRISON "
                    "step=wave2_spawned result=success runtime=%d"
                ),
                RuntimeHostiles.Num()
            );
            return;
        }

        ++BHTestDefenseAGarrisonWaitAttempts;
        if (BHTestDefenseAGarrisonWaitAttempts >= 40)
        {
            FailTest(TEXT("wave2_spawn_timeout"));
        }
        return;
    }

    if (BHTestDefenseAGarrisonPhase == 6)
    {
        if (!bBHTestDefenseAWave2Spawned)
        {
            FailTest(TEXT("wave2_not_observed"));
            return;
        }

        const FBHOpenWorldOperationState State =
            Player->GetOpenWorldOperationState();
        if (!State.bHasSnapshot || !State.bOperationActivated)
        {
            ++BHTestDefenseAGarrisonWaitAttempts;
            if (BHTestDefenseAGarrisonWaitAttempts >= 40)
            {
                FailTest(TEXT("post_wave2_operation_state"));
            }
            return;
        }

        if (CountLivingActiveAuthoredGarrison() != 0)
        {
            FailTest(TEXT("authored_garrison_reactivated"));
            return;
        }

        if (State.bSecuringObjective)
        {
            if (bMultiplayer && !BHDefenseAMultiplayerTest::HasControl(TEXT("secure.ready"))) { return; }
            if (!Player->SetActorLocation(
                    State.OperationCenter + FVector(0.0f, 0.0f, 50.0f),
                    false,
                    nullptr,
                    ETeleportType::TeleportPhysics
                ))
            {
                FailTest(TEXT("hold_position_set"));
                return;
            }
            Player->UpdateOverlaps();
            if (bMultiplayer)
            {
                for (ABHCharacter* Participant : Participants)
                {
                    Participant->SetActorLocation(State.OperationCenter + FVector(0.0f, 0.0f, 50.0f),
                        false, nullptr, ETeleportType::TeleportPhysics);
                    Participant->UpdateOverlaps();
                }
            }
            BHTestDefenseAGarrisonPhase = 7;
            BHTestDefenseAGarrisonWaitAttempts = 0;
            UE_LOG(
                LogTemp,
                Display,
                TEXT(
                    "BH_TEST_DEFENSE_A_GARRISON "
                    "step=final_hold_started result=success wave=%d"
                ),
                State.CurrentWave
            );
            return;
        }

        if (State.bWaitingForWave)
        {
            if (State.CurrentWave >= State.DefenseWaveCount ||
                State.LivingEnemyCount != 0 ||
                State.SecondsUntilNextWave <= 0.0f)
            {
                FailTest(TEXT("later_wave_countdown"));
                return;
            }
            return;
        }

        const TArray<ABHEnemySoldier*> RuntimeHostiles =
            GatherRuntimeOperationHostiles();
        if (State.LivingEnemyCount > 0)
        {
            if (RuntimeHostiles.Num() != State.LivingEnemyCount)
            {
                FailTest(TEXT("later_wave_runtime_spawn"));
                return;
            }

            if (bMultiplayer && !BHDefenseAMultiplayerTest::HasControl(
                    FString::Printf(TEXT("clear_wave%d.ready"), State.CurrentWave))) { return; }
            const int32 ClearedCount =
                KillOperationHostiles(RuntimeHostiles);
            if (ClearedCount != RuntimeHostiles.Num())
            {
                FailTest(TEXT("later_wave_clear_damage"));
                return;
            }

            BHTestDefenseAGarrisonWaitAttempts = 0;
            UE_LOG(
                LogTemp,
                Display,
                TEXT(
                    "BH_TEST_DEFENSE_A_GARRISON "
                    "step=later_wave_cleared result=success "
                    "wave=%d runtime=%d"
                ),
                State.CurrentWave,
                RuntimeHostiles.Num()
            );
            return;
        }

        ++BHTestDefenseAGarrisonWaitAttempts;
        if (BHTestDefenseAGarrisonWaitAttempts >= 60)
        {
            FailTest(TEXT("final_hold_timeout"));
        }
        return;
    }

    if (BHTestDefenseAGarrisonPhase == 7)
    {
        const FBHActiveOperationSnapshot Snapshot =
            GetActiveOperationSnapshot();
        int32 CompletedParticipants = 0;
        for (ABHCharacter* Participant : Participants)
        {
            CompletedParticipants += Participant->IsMissionComplete() ? 1 : 0;
        }
        if (Snapshot.OperationState.bHasSnapshot &&
            Snapshot.Phase == EBHActiveOperationPhase::DebriefSuccess &&
            Player->IsMissionComplete() &&
            (!bMultiplayer || CompletedParticipants == Participants.Num()))
        {
            if (!Snapshot.OperationState.bSecuringObjective ||
                Snapshot.OperationState.CurrentWave <
                    Snapshot.OperationState.DefenseWaveCount ||
                CountLivingActiveAuthoredGarrison() != 0)
            {
                FailTest(TEXT("terminal_success_snapshot"));
                return;
            }

            UE_LOG(
                LogTemp,
                Display,
                TEXT(
                    "BH_TEST_DEFENSE_A_GARRISON result=success "
                    "wave2_spawned=%d terminal_phase=%d wave=%d "
                    "casualties=%d"
                ),
                bBHTestDefenseAWave2Spawned ? 1 : 0,
                static_cast<int32>(Snapshot.Phase),
                Snapshot.OperationState.CurrentWave,
                Snapshot.OperationState.EnemyCasualties
            );
            GetWorldTimerManager().ClearTimer(
                DefenseAGarrisonPersistenceTestTimer
            );
            if (bMultiplayer)
            {
                BHDefenseAMultiplayerTest::CompleteHost(Snapshot, Participants.Num(), CompletedParticipants);
            }
            else
            {
                FPlatformMisc::RequestExit(false);
            }
            return;
        }

        const FBHOpenWorldOperationState State =
            Player->GetOpenWorldOperationState();
        if (State.bHasSnapshot && State.bSecuringObjective)
        {
            Player->SetActorLocation(
                State.OperationCenter + FVector(0.0f, 0.0f, 50.0f),
                false,
                nullptr,
                ETeleportType::TeleportPhysics
            );
            Player->UpdateOverlaps();
        }

        ++BHTestDefenseAGarrisonWaitAttempts;
        if (BHTestDefenseAGarrisonWaitAttempts >= 80)
        {
            FailTest(TEXT("terminal_success_timeout"));
        }
    }
}

void ABHWarGameState::RunOperationFailureTest()
{
    if (bBHTestOperationFailureIssued)
    {
        GetWorldTimerManager().ClearTimer(OperationFailureTestTimer);
        return;
    }

    UWorld* World = GetWorld();
    if (!HasAuthority() || !IsValid(World) || !IsValid(BoundWarSubsystem))
    {
        return;
    }

    TArray<ABHCharacter*> Participants;
    FName OperationSectorID = NAME_None;
    EBHWarPriorityType OperationType = EBHWarPriorityType::None;
    for (TActorIterator<ABHCharacter> It(World); It; ++It)
    {
        ABHCharacter* Participant = *It;
        if (!IsValid(Participant) ||
            !Participant->HasAuthority() ||
            !Participant->IsPlayerControlled() ||
            !Participant->IsRuntimeWarOperation() ||
            Participant->GetAssignedWarSectorID().IsNone() ||
            Participant->GetAssignedWarPriorityType() ==
                EBHWarPriorityType::None ||
            Participant->IsMissionComplete() ||
            Participant->IsMissionFailed())
        {
            continue;
        }

        if (Participants.IsEmpty())
        {
            OperationSectorID = Participant->GetAssignedWarSectorID();
            OperationType = Participant->GetAssignedWarPriorityType();
        }
        else if (
            Participant->GetAssignedWarSectorID() != OperationSectorID ||
            Participant->GetAssignedWarPriorityType() != OperationType)
        {
            continue;
        }

        Participants.Add(Participant);
    }

    if (Participants.Num() < 2)
    {
        return;
    }

    ABHOpenWorldOperationDirector* Director = nullptr;
    for (TActorIterator<ABHOpenWorldOperationDirector> It(World); It; ++It)
    {
        if (IsValid(*It) &&
            (*It)->HasAuthority() &&
            (*It)->IsOperationActivated() &&
            (*It)->IsOperationInProgress())
        {
            Director = *It;
            break;
        }
    }

    if (!IsValid(Director))
    {
        return;
    }

    Director->FailOperationForTesting();

    int32 FailedParticipantCount = 0;
    for (ABHCharacter* Participant : Participants)
    {
        if (IsValid(Participant) && Participant->IsMissionFailed())
        {
            ++FailedParticipantCount;
        }
    }

    const bool bFailureVerified =
        FailedParticipantCount == Participants.Num() &&
        !Director->IsOperationInProgress();

    UE_LOG(
        LogTemp,
        Display,
        TEXT(
            "BH_TEST_OPERATION_FAILURE result=%s sector=%s type=%d "
            "participants=%d failed=%d director_in_progress=%d"
        ),
        bFailureVerified ? TEXT("success") : TEXT("failure"),
        *OperationSectorID.ToString(),
        static_cast<int32>(OperationType),
        Participants.Num(),
        FailedParticipantCount,
        Director->IsOperationInProgress() ? 1 : 0
    );

    bBHTestOperationFailureIssued = true;
    GetWorldTimerManager().ClearTimer(OperationFailureTestTimer);

    if (!bFailureVerified)
    {
        FPlatformMisc::RequestExit(false);
    }
}

void ABHWarGameState::RunFieldSquadContextOwnershipTest()
{
    UWorld* World = GetWorld();

    if (!HasAuthority() || !IsValid(World))
    {
        GetWorldTimerManager().ClearTimer(
            FieldSquadContextOwnershipTestTimer
        );
        return;
    }

    TArray<ABHCharacter*> Players;
    for (TActorIterator<ABHCharacter> It(World); It; ++It)
    {
        if (It->HasAuthority() && It->IsPlayerControlled())
        {
            Players.Add(*It);
        }
    }

    Players.Sort(
        [](const ABHCharacter& Left, const ABHCharacter& Right)
        {
            return Left.GetName() < Right.GetName();
        }
    );

    if (Players.Num() < 2)
    {
        if (World->GetTimeSeconds() >= 35.0f)
        {
            UE_LOG(
                LogTemp,
                Error,
                TEXT("BH_TEST_FIELD_SQUAD_CONTEXT_OWNERSHIP result=failure reason=players players=%d"),
                Players.Num()
            );
            GetWorldTimerManager().ClearTimer(
                FieldSquadContextOwnershipTestTimer
            );
        }
        return;
    }

    const bool bFirstConfigured =
        Players[0]->ConfigureFieldSquadContextReplicationTest(
            EBHFieldSquadContextAction::Secure,
            TEXT("CONTEXT_OWNER_A"),
            false
        );
    const bool bSecondConfigured =
        Players[1]->ConfigureFieldSquadContextReplicationTest(
            EBHFieldSquadContextAction::Defend,
            TEXT("CONTEXT_OWNER_B"),
            true
        );
    const bool bSuccess = bFirstConfigured && bSecondConfigured;

    UE_LOG(
        LogTemp,
        Display,
        TEXT("BH_TEST_FIELD_SQUAD_CONTEXT_OWNERSHIP result=%s players=%d first=%s second=%s"),
        bSuccess ? TEXT("success") : TEXT("failure"),
        Players.Num(),
        *GetNameSafe(Players[0]),
        *GetNameSafe(Players[1])
    );
    GetWorldTimerManager().ClearTimer(
        FieldSquadContextOwnershipTestTimer
    );
}

void ABHWarGameState::RunMedicalRecoveryReplicationTest()
{
    UWorld* World = GetWorld();

    if (!HasAuthority() || !IsValid(World))
    {
        GetWorldTimerManager().ClearTimer(
            MedicalRecoveryReplicationTestTimer
        );
        return;
    }

    TArray<ABHCharacter*> Players;
    for (TActorIterator<ABHCharacter> It(World); It; ++It)
    {
        if (It->HasAuthority() && It->IsPlayerControlled())
        {
            Players.Add(*It);
        }
    }
    Players.Sort(
        [](const ABHCharacter& Left, const ABHCharacter& Right)
        {
            return Left.GetName() < Right.GetName();
        }
    );

    if (Players.Num() < 2)
    {
        if (World->GetTimeSeconds() >= 35.0f)
        {
            UE_LOG(
                LogTemp,
                Error,
                TEXT(
                    "BH_TEST_MEDICAL_RECOVERY_REPLICATION "
                    "result=failure reason=players players=%d"
                ),
                Players.Num()
            );
            GetWorldTimerManager().ClearTimer(
                MedicalRecoveryReplicationTestTimer
            );
        }
        return;
    }

    UBHWeaponComponent* FirstWeapon =
        Players[0]->GetWeaponComponent();
    UBHWeaponComponent* SecondWeapon =
        Players[1]->GetWeaponComponent();
    UBHInjuryComponent* FirstInjury =
        Players[0]->GetInjuryComponent();
    UBHInjuryComponent* SecondInjury =
        Players[1]->GetInjuryComponent();

    if (!IsValid(FirstWeapon) ||
        !IsValid(SecondWeapon) ||
        !IsValid(FirstInjury) ||
        !IsValid(SecondInjury))
    {
        UE_LOG(
            LogTemp,
            Error,
            TEXT(
                "BH_TEST_MEDICAL_RECOVERY_REPLICATION "
                "result=failure reason=components"
            )
        );
        GetWorldTimerManager().ClearTimer(
            MedicalRecoveryReplicationTestTimer
        );
        return;
    }

    if (MedicalRecoveryReplicationTestPhase == 0)
    {
        const bool bFirstAmmoConfigured =
            FirstWeapon->RestoreAmmoState(30, 0);
        const bool bSecondAmmoConfigured =
            SecondWeapon->RestoreAmmoState(30, 0);
        FirstInjury->RestorePersistentSupplyState(
            0,
            0,
            9.0f,
            20.0f
        );
        FirstInjury->RestorePersistentInjuryState(
            true,
            0.50f,
            true,
            false
        );
        SecondInjury->RestorePersistentSupplyState(
            0,
            0,
            18.0f,
            40.0f
        );
        SecondInjury->RestorePersistentInjuryState(
            true,
            0.30f,
            false,
            true
        );

        FActorSpawnParameters SpawnParameters;
        SpawnParameters.SpawnCollisionHandlingOverride =
            ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        ABHFieldTransport* TestTransport =
            World->SpawnActor<ABHFieldTransport>(
                Players[0]->GetActorLocation() +
                    FVector(900.0f, 0.0f, 100.0f),
                Players[0]->GetActorRotation(),
                SpawnParameters
            );
        if (IsValid(TestTransport))
        {
            TestTransport->SetPersistenceIDForTesting(
                TEXT("BHTestMedicalRecoveryTransport")
            );
            TestTransport->RestorePersistentState(
                TestTransport->GetActorTransform(),
                nullptr,
                false,
                0.0f,
                0.0f,
                0.0f,
                NAME_None
            );
            MedicalRecoveryReplicationTestTransport =
                TestTransport;
        }

        Players[0]->ForceNetUpdate();
        Players[1]->ForceNetUpdate();
        MedicalRecoveryReplicationTestPhase = 1;
        UE_LOG(
            LogTemp,
            Display,
            TEXT(
                "BH_TEST_MEDICAL_RECOVERY_PREPARED result=%s "
                "players=2 transport_immobilized=%d"
            ),
            bFirstAmmoConfigured && bSecondAmmoConfigured &&
                    IsValid(TestTransport) &&
                    TestTransport->IsImmobilized()
                ? TEXT("success")
                : TEXT("failure"),
            IsValid(TestTransport) &&
                    TestTransport->IsImmobilized()
                ? 1
                : 0
        );
        return;
    }

    const int32 FirstAmmoAdded = FirstWeapon->AddReserveAmmo(30);
    const int32 SecondAmmoAdded = SecondWeapon->AddReserveAmmo(45);
    FirstInjury->AddMedicalSupplies(1, 1);
    SecondInjury->AddMedicalSupplies(2, 1);
    const bool bFirstArmorRepaired =
        FirstInjury->RepairArmor(BIG_NUMBER, BIG_NUMBER);
    const bool bSecondArmorRepaired =
        SecondInjury->RepairArmor(BIG_NUMBER, BIG_NUMBER);
    ABHFieldTransport* TestTransport =
        MedicalRecoveryReplicationTestTransport.Get();
    const bool bVehicleRecovered =
        IsValid(TestTransport) &&
        TestTransport->RecoverAndService(
            FTransform(
                Players[0]->GetActorRotation(),
                Players[0]->GetActorLocation() +
                    FVector(700.0f, 0.0f, 100.0f),
                FVector::OneVector
            )
        );
    Players[0]->ForceNetUpdate();
    Players[1]->ForceNetUpdate();

    const bool bSuccess =
        FirstAmmoAdded == 30 &&
        SecondAmmoAdded == 45 &&
        FirstInjury->GetMedkitCount() == 1 &&
        FirstInjury->GetFieldDressingCount() == 1 &&
        SecondInjury->GetMedkitCount() == 2 &&
        SecondInjury->GetFieldDressingCount() == 1 &&
        bFirstArmorRepaired &&
        bSecondArmorRepaired &&
        bVehicleRecovered &&
        TestTransport->GetFuelPercentage() >= 0.999f &&
        TestTransport->GetHullPercentage() >= 0.999f;
    UE_LOG(
        LogTemp,
        Display,
        TEXT(
            "BH_TEST_MEDICAL_RECOVERY_REPLICATION result=%s "
            "players=2 first_ammo=%d first_medkits=%d "
            "first_dressings=%d second_ammo=%d "
            "second_medkits=%d second_dressings=%d "
            "vehicle_recovered=%d fuel=%.2f hull=%.2f"
        ),
        bSuccess ? TEXT("success") : TEXT("failure"),
        FirstWeapon->GetReserveAmmo(),
        FirstInjury->GetMedkitCount(),
        FirstInjury->GetFieldDressingCount(),
        SecondWeapon->GetReserveAmmo(),
        SecondInjury->GetMedkitCount(),
        SecondInjury->GetFieldDressingCount(),
        bVehicleRecovered ? 1 : 0,
        IsValid(TestTransport)
            ? TestTransport->GetFuelPercentage()
            : 0.0f,
        IsValid(TestTransport)
            ? TestTransport->GetHullPercentage()
            : 0.0f
    );
    GetWorldTimerManager().ClearTimer(
        MedicalRecoveryReplicationTestTimer
    );
}
#endif

void ABHWarGameState::EndPlay(
    const EEndPlayReason::Type EndPlayReason
)
{
    if (IsValid(BoundWarSubsystem))
    {
        BoundWarSubsystem->OnWarStateChanged.RemoveDynamic(
            this,
            &ABHWarGameState::HandleWarStateChanged
        );
    }

    BoundWarSubsystem = nullptr;
    Super::EndPlay(EndPlayReason);
}

void ABHWarGameState::HandleWarStateChanged(
    int32 TurnNumber,
    FName PrioritySectorID,
    EBHWarPriorityType PriorityType
)
{
    (void)TurnNumber;
    (void)PrioritySectorID;
    (void)PriorityType;

    if (HasAuthority())
    {
        PublishAuthoritativeSnapshot();

        const FBHBattlefieldConditionProfile Conditions =
            UBHBattlefieldConditions::BuildProfileForTurn(
                WarStateSnapshot.TurnNumber
            );
        const FString ConditionMessage = FString::Printf(
            TEXT(
                "BATTLEFIELD CONDITIONS // %s\n"
                "Visibility %d%% // Footstep masking %d%% // "
                "Mortar dispersion %d%%"
            ),
            *Conditions.ConditionLabel.ToString(),
            FMath::RoundToInt(Conditions.SightRangeMultiplier * 100.0f),
            FMath::RoundToInt(Conditions.MovementNoiseMultiplier * 100.0f),
            FMath::RoundToInt(Conditions.MortarDispersionMultiplier * 100.0f)
        );
        for (TActorIterator<ABHCharacter> It(GetWorld()); It; ++It)
        {
            if (It->IsPlayerControlled())
            {
                It->ShowPriorityStatusNotification(
                    FText::FromString(ConditionMessage),
                    EBHNotificationPriority::High
                );
            }
        }
        for (TActorIterator<ABHAmbientWarDirector> It(GetWorld()); It; ++It)
        {
            const float RainIntensity =
                Conditions.Weather == EBHBattlefieldWeather::Rain ? 0.65f :
                Conditions.Weather == EBHBattlefieldWeather::Storm ? 1.0f : 0.0f;
            const float WindIntensity =
                Conditions.Weather == EBHBattlefieldWeather::Storm ? 1.0f :
                Conditions.Weather == EBHBattlefieldWeather::Rain ? 0.55f :
                Conditions.Weather == EBHBattlefieldWeather::Fog ? 0.12f : 0.25f;
            It->SetWeatherMix(WindIntensity, RainIntensity);
        }

        UE_LOG(
            LogTemp,
            Display,
            TEXT(
                "BH_BATTLEFIELD_CONDITION turn=%d label=%s "
                "sight=%.2f noise=%.2f spread=%.2f infantry=%.2f "
                "traction=%.2f fuel=%.2f mortar=%.2f"
            ),
            WarStateSnapshot.TurnNumber,
            *Conditions.ConditionLabel.ToString(),
            Conditions.SightRangeMultiplier,
            Conditions.MovementNoiseMultiplier,
            Conditions.WeaponSpreadMultiplier,
            Conditions.InfantrySpeedMultiplier,
            Conditions.VehicleTractionMultiplier,
            Conditions.VehicleFuelBurnMultiplier,
            Conditions.MortarDispersionMultiplier
        );
    }
}

void ABHWarGameState::OnRep_WarStateSnapshot()
{
    ApplyReplicatedSnapshot();
}

void ABHWarGameState::OnRep_ActiveOperationSnapshot()
{
    BroadcastActiveOperationSnapshotChanged();

    UE_LOG(
        LogTemp,
        Display,
        TEXT(
            "BH_ACTIVE_OPERATION_SNAPSHOT_APPLIED "
            "revision=%d phase=%d sector=%s"
        ),
        ActiveOperationSnapshot.Revision,
        static_cast<int32>(ActiveOperationSnapshot.Phase),
        *ActiveOperationSnapshot.SectorID.ToString()
    );
}

void ABHWarGameState::BroadcastActiveOperationSnapshotChanged()
{
    ActiveOperationSnapshotChanged.Broadcast(ActiveOperationSnapshot);
}

void ABHWarGameState::PublishAuthoritativeSnapshot()
{
    if (!HasAuthority())
    {
        return;
    }

    UBHWarSubsystem* WarSubsystem = ResolveWarSubsystem();

    if (!IsValid(WarSubsystem))
    {
        return;
    }

    BoundWarSubsystem = WarSubsystem;
    int32 NextRevision =
        WarSubsystem->AllocateReplicatedSnapshotRevision();
#if !UE_BUILD_SHIPPING
    int32 RevisionFloor = 0;
    if (WarStateSnapshot.Revision == 0 && FParse::Value(
            FCommandLine::Get(), TEXT("BHTestSnapshotRevisionFloor="), RevisionFloor))
    {
        if (RevisionFloor < 1 || RevisionFloor > 65536)
        {
            UE_LOG(LogTemp, Error, TEXT("BH_TEST_SNAPSHOT_REVISION_FLOOR result=failure reason=invalid_floor"));
        }
        else
        {
            // Exercise the real allocator; no fabricated snapshot or campaign state.
            while (NextRevision < RevisionFloor)
            {
                NextRevision = WarSubsystem->AllocateReplicatedSnapshotRevision();
            }
            UE_LOG(LogTemp, Display, TEXT("BH_TEST_SNAPSHOT_REVISION_FLOOR result=success floor=%d revision=%d"),
                RevisionFloor, NextRevision);
        }
    }
#endif
    WarStateSnapshot =
        WarSubsystem->CaptureReplicatedSnapshot(NextRevision);
    ForceNetUpdate();

    UE_LOG(
        LogTemp,
        VeryVerbose,
        TEXT(
            "BH_WAR_SNAPSHOT_PUBLISHED revision=%d turn=%d "
            "sectors=%d operation_id=%s"
        ),
        WarStateSnapshot.Revision,
        WarStateSnapshot.TurnNumber,
        WarStateSnapshot.SectorStates.Num(),
        *WarStateSnapshot.CommittedOperationID.ToString()
    );

}

void ABHWarGameState::OnRep_SquadPingSnapshot()
{
    UE_LOG(
        LogTemp,
        Display,
        TEXT(
            "BH_SQUAD_PING_APPLIED revision=%d context=%s issuer=%s "
            "location=%s tracked=%s expiry=%.2f"
        ),
        SquadPingSnapshot.Revision,
        *SquadPingSnapshot.ContextLabel.ToString(),
        *SquadPingSnapshot.IssuerLabel.ToString(),
        *FVector(SquadPingSnapshot.Location).ToCompactString(),
        IsValid(SquadPingSnapshot.TrackedActor)
            ? *SquadPingSnapshot.TrackedActor->GetName()
            : TEXT("none"),
        SquadPingSnapshot.ExpiryServerWorldTimeSeconds
    );

#if !UE_BUILD_SHIPPING
    FString ScreenshotPath;
    if (!bBHTestSquadPingScreenshotIssued &&
        SquadPingSnapshot.Revision > 0 &&
        FParse::Value(
            FCommandLine::Get(),
            TEXT("BHTestSquadPingScreenshotPath="),
            ScreenshotPath) &&
        !ScreenshotPath.IsEmpty())
    {
        bBHTestSquadPingScreenshotIssued = true;
        IFileManager::Get().MakeDirectory(
            *FPaths::GetPath(ScreenshotPath),
            true
        );
        const int32 Revision = SquadPingSnapshot.Revision;
        const FString Context =
            SquadPingSnapshot.ContextLabel.ToString();
        const FString Issuer =
            SquadPingSnapshot.IssuerLabel.ToString();
        FTSTicker::GetCoreTicker().AddTicker(
            FTickerDelegate::CreateLambda(
                [ScreenshotPath, Revision, Context, Issuer](float)
                {
                    FScreenshotRequest::RequestScreenshot(
                        ScreenshotPath,
                        true,
                        false
                    );
                    UE_LOG(
                        LogTemp,
                        Display,
                        TEXT(
                            "BH_SQUAD_PING_SCREENSHOT result=requested "
                            "revision=%d context=%s issuer=%s path=%s"
                        ),
                        Revision,
                        *Context,
                        *Issuer,
                        *ScreenshotPath
                    );
                    return false;
                }
            ),
            0.1f
        );
    }
#endif
}

void ABHWarGameState::ApplyReplicatedSnapshot()
{
    if (HasAuthority() || !WarStateSnapshot.bInitialized)
    {
        return;
    }

    UBHWarSubsystem* WarSubsystem = ResolveWarSubsystem();

    if (!IsValid(WarSubsystem) ||
        !WarSubsystem->ApplyReplicatedSnapshot(WarStateSnapshot))
    {
        UE_LOG(
            LogTemp,
            Error,
            TEXT(
                "BH_WAR_SNAPSHOT_APPLY_FAILED revision=%d"
            ),
            WarStateSnapshot.Revision
        );
        return;
    }

    BoundWarSubsystem = WarSubsystem;

    UE_LOG(
        LogTemp,
        Display,
        TEXT(
            "BH_WAR_SNAPSHOT_APPLIED revision=%d turn=%d "
            "sectors=%d operation_id=%s"
        ),
        WarStateSnapshot.Revision,
        WarStateSnapshot.TurnNumber,
        WarStateSnapshot.SectorStates.Num(),
        *WarStateSnapshot.CommittedOperationID.ToString()
    );

#if !UE_BUILD_SHIPPING
    if (FParse::Param(
            FCommandLine::Get(),
            TEXT("BHTestCustomDifficultyReplication")))
    {
        const FBHCampaignDifficultyProfile Difficulty =
            WarSubsystem->GetCampaignDifficulty();
        UE_LOG(
            LogTemp,
            Display,
            TEXT(
                "BH_CUSTOM_DIFFICULTY_REPLICATED preset=%d "
                "damage=%.2f perception=%.2f coordination=%.2f "
                "medical=%.2f strategic=%.2f checkpoint=%.2f"
            ),
            static_cast<int32>(Difficulty.Preset),
            Difficulty.IncomingDamageMultiplier,
            Difficulty.EnemyPerceptionMultiplier,
            Difficulty.EnemyCoordinationMultiplier,
            Difficulty.MedicalPressureMultiplier,
            Difficulty.StrategicPressureMultiplier,
            Difficulty.CheckpointIntervalMultiplier
        );
    }
#endif
}

UBHWarSubsystem* ABHWarGameState::ResolveWarSubsystem() const
{
    const UGameInstance* GameInstance = GetGameInstance();

    return IsValid(GameInstance)
        ? GameInstance->GetSubsystem<UBHWarSubsystem>()
        : nullptr;
}
