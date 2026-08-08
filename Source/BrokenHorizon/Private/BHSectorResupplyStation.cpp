#include "BHSectorResupplyStation.h"

#include "BHCharacter.h"
#include "BHFieldTransport.h"
#include "BHInjuryComponent.h"
#include "BHSaveSubsystem.h"
#include "BHWarSubsystem.h"
#include "BHWeaponComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/GameInstance.h"
#include "EngineUtils.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "UObject/ConstructorHelpers.h"

ABHSectorResupplyStation::ABHSectorResupplyStation()
{
    PrimaryActorTick.bCanEverTick = false;

#if WITH_EDITOR
    SetIsSpatiallyLoaded(false);
#endif

    SceneRoot = CreateDefaultSubobject<USceneComponent>(
        TEXT("SceneRoot")
    );
    SetRootComponent(SceneRoot);

    StationMesh = CreateDefaultSubobject<UStaticMeshComponent>(
        TEXT("StationMesh")
    );
    StationMesh->SetupAttachment(SceneRoot);
    StationMesh->SetRelativeScale3D(
        FVector(1.4f, 1.0f, 0.55f)
    );
    StationMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    StationMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
    StationMesh->SetCollisionResponseToChannel(
        ECC_Visibility,
        ECR_Block
    );

    static ConstructorHelpers::FObjectFinder<UStaticMesh> StationMeshAsset(
        TEXT("/Engine/BasicShapes/Cube.Cube")
    );

    if (StationMeshAsset.Succeeded())
    {
        StationMesh->SetStaticMesh(StationMeshAsset.Object);
    }
}

void ABHSectorResupplyStation::Interact_Implementation(
    AActor* InteractingActor
)
{
    ABHCharacter* Character = Cast<ABHCharacter>(
        InteractingActor
    );
    UWorld* World = GetWorld();
    UGameInstance* GameInstance = IsValid(World)
        ? World->GetGameInstance()
        : nullptr;
    UBHWarSubsystem* WarSubsystem = IsValid(GameInstance)
        ? GameInstance->GetSubsystem<UBHWarSubsystem>()
        : nullptr;
    UBHSaveSubsystem* SaveSubsystem = IsValid(GameInstance)
        ? GameInstance->GetSubsystem<UBHSaveSubsystem>()
        : nullptr;

    if (!IsValid(Character) ||
        !IsValid(World) ||
        !IsValid(WarSubsystem) ||
        SectorID.IsNone())
    {
        return;
    }

    const FBHWarSectorState Sector =
        WarSubsystem->GetSectorState(SectorID);

    if (Sector.SectorID.IsNone() ||
        Sector.Owner != EBHWarFaction::Friendly)
    {
        ShowUnavailableMessage(
            Character,
            NSLOCTEXT(
                "BrokenHorizon",
                "SectorResupplyNotFriendly",
                "RESUPPLY UNAVAILABLE\n\n"
                "This sector is not under friendly control."
            )
        );
        return;
    }

    UBHWeaponComponent* WeaponComponent =
        Character->GetWeaponComponent();
    UBHInjuryComponent* InjuryComponent =
        Character->GetInjuryComponent();
    ABHFieldTransport* ServiceTransport = nullptr;
    ABHFieldTransport* RecoveryTransport = nullptr;
    float NearestTransportDistanceSquared =
        FMath::Square(FMath::Max(100.0f, VehicleServiceRadius));
    float NearestRecoveryDistanceSquared =
        TNumericLimits<float>::Max();

    for (TActorIterator<ABHFieldTransport> It(World); It; ++It)
    {
        ABHFieldTransport* Candidate = *It;

        if (!IsValid(Candidate))
        {
            continue;
        }

        const float DistanceSquared = FVector::DistSquared(
            Character->GetActorLocation(),
            Candidate->GetActorLocation()
        );

        if (DistanceSquared <= NearestTransportDistanceSquared)
        {
            ServiceTransport = Candidate;
            NearestTransportDistanceSquared = DistanceSquared;
        }
        else if (Candidate->IsImmobilized() &&
                 !IsValid(Candidate->GetOccupant()) &&
                 DistanceSquared < NearestRecoveryDistanceSquared)
        {
            RecoveryTransport = Candidate;
            NearestRecoveryDistanceSquared = DistanceSquared;
        }
    }
    const int32 AmmoNeeded = IsValid(WeaponComponent)
        ? FMath::Max(
            0,
            WeaponComponent->GetMaxReserveAmmo() -
                WeaponComponent->GetReserveAmmo()
        )
        : 0;
    const int32 AmmoRequest = FMath::Min(
        FMath::Max(0, ReserveAmmoAmount),
        AmmoNeeded
    );
    const int32 MedkitsNeeded = IsValid(InjuryComponent)
        ? FMath::Max(
            0,
            TargetMedkitCount -
                InjuryComponent->GetMedkitCount()
        )
        : 0;
    const int32 DressingsNeeded = IsValid(InjuryComponent)
        ? FMath::Max(
            0,
            TargetFieldDressingCount -
                InjuryComponent->GetFieldDressingCount()
        )
        : 0;
    const int32 FragGrenadesNeeded = FMath::Max(
        0,
        TargetFragGrenadeCount -
            Character->GetFragGrenadeCount()
    );
    const int32 SmokeGrenadesNeeded = FMath::Max(
        0,
        TargetSmokeGrenadeCount -
            Character->GetSmokeGrenadeCount()
    );
    const int32 EngineeringChargesNeeded = FMath::Max(
        0,
        TargetEngineeringChargeCount -
            Character->GetEngineeringChargeCount()
    );
    const bool bArmorNeedsRepair =
        IsValid(InjuryComponent) &&
        (InjuryComponent->GetHelmetDurabilityPercentage() < 0.999f ||
         InjuryComponent->GetBodyArmorDurabilityPercentage() < 0.999f);
    const bool bVehicleRecoveryNeeded =
        IsValid(RecoveryTransport);
    TArray<ABHCharacter*> ServiceCharacters = {Character};
    for (TActorIterator<ABHCharacter> It(World); It; ++It)
    {
        ABHCharacter* Candidate = *It;

        if (IsValid(Candidate) &&
            Candidate != Character &&
            Candidate->IsRuntimeWarOperation() &&
            Candidate->GetAssignedWarPriorityType() ==
                EBHWarPriorityType::Rescue &&
            Candidate->GetAssignedWarSectorID() == SectorID)
        {
            ServiceCharacters.Add(Candidate);
        }
    }

    int32 FireteamMembersNeedingService = 0;
    for (const ABHCharacter* ServiceCharacter : ServiceCharacters)
    {
        FireteamMembersNeedingService +=
            ServiceCharacter->CountFieldSquadMembersNeedingService(
                GetActorLocation(),
                FireteamServiceRadius
            );
    }
    const bool bNeedsResupply =
        AmmoRequest > 0 ||
        MedkitsNeeded > 0 ||
        DressingsNeeded > 0 ||
        FragGrenadesNeeded > 0 ||
        SmokeGrenadesNeeded > 0 ||
        EngineeringChargesNeeded > 0 ||
        bArmorNeedsRepair ||
        FireteamMembersNeedingService > 0 ||
        bVehicleRecoveryNeeded ||
        (IsValid(ServiceTransport) &&
         ServiceTransport->NeedsService());

    if (!bNeedsResupply)
    {
        const bool bCheckpointSaved =
            IsValid(SaveSubsystem) &&
            SaveSubsystem->SaveProgress();
        ShowUnavailableMessage(
            Character,
            bCheckpointSaved
                ? NSLOCTEXT(
                    "BrokenHorizon",
                    "SectorCheckpointUpdatedLoadoutFull",
                    "CHECKPOINT UPDATED\n\n"
                    "Loadout full. Respawn position secured."
                )
                : NSLOCTEXT(
                    "BrokenHorizon",
                    "SectorCheckpointFailedLoadoutFull",
                    "LOADOUT FULL\n\n"
                    "Checkpoint update failed."
                )
        );
        return;
    }

    const float CurrentTime = World->GetTimeSeconds();

    if (CurrentTime < NextAvailableUseTime)
    {
        const bool bCheckpointSaved =
            IsValid(SaveSubsystem) &&
            SaveSubsystem->SaveProgress();
        ShowUnavailableMessage(
            Character,
            FText::Format(
                bCheckpointSaved
                    ? NSLOCTEXT(
                        "BrokenHorizon",
                        "SectorResupplyRechargingCheckpoint",
                        "RESUPPLY RECHARGING\n\n"
                        "Available in {0} seconds. "
                        "Checkpoint updated."
                    )
                    : NSLOCTEXT(
                        "BrokenHorizon",
                        "SectorResupplyRechargingNoCheckpoint",
                        "RESUPPLY RECHARGING\n\n"
                        "Available in {0} seconds. "
                        "Checkpoint update failed."
                    ),
                FText::AsNumber(FMath::CeilToInt(
                    NextAvailableUseTime - CurrentTime
                ))
            )
        );
        return;
    }

    const float MedicalPressureMultiplier = FMath::Clamp(
        WarSubsystem->GetCampaignDifficulty()
            .MedicalPressureMultiplier,
        0.5f,
        2.0f
    );
    const float CasualtyRecoveryMultiplier =
        WarSubsystem->HasCampaignCapability(
            EBHCampaignCapability::CasualtyRecoveryNetwork)
        ? 0.85f
        : 1.0f;
    const float TransportSupportMultiplier =
        WarSubsystem->HasCampaignCapability(
            EBHCampaignCapability::TransportSupportNetwork)
        ? 0.75f
        : 1.0f;
    const float TotalStrategicSupplyCost =
        StrategicSupplyCost +
        CalculateFireteamServiceSupplyCost(
            FireteamMembersNeedingService,
            FireteamServiceSupplyCostPerMember *
                MedicalPressureMultiplier *
                CasualtyRecoveryMultiplier
        ) +
        (bVehicleRecoveryNeeded
            ? VehicleRecoverySupplyCost *
                TransportSupportMultiplier
            : 0.0f);

    if (!WarSubsystem->ConsumeSectorSupply(
            SectorID,
            TotalStrategicSupplyCost))
    {
        const int32 CurrentReserveAmmo = IsValid(WeaponComponent)
            ? WeaponComponent->GetReserveAmmo()
            : 0;
        const int32 CurrentMedkits = IsValid(InjuryComponent)
            ? InjuryComponent->GetMedkitCount()
            : 0;
        const int32 CurrentDressings = IsValid(InjuryComponent)
            ? InjuryComponent->GetFieldDressingCount()
            : 0;

        if (ShouldIssueEmergencyFallbackKit(
                CurrentReserveAmmo,
                CurrentMedkits,
                CurrentDressings))
        {
            const int32 EmergencyAmmoRequest =
                IsValid(WeaponComponent)
                    ? CalculateEmergencyFallbackAmmoRequest(
                        CurrentReserveAmmo,
                        WeaponComponent->GetMaxReserveAmmo(),
                        EmergencyFallbackReserveAmmo
                    )
                    : 0;
            const int32 EmergencyAmmoAdded =
                IsValid(WeaponComponent)
                    ? WeaponComponent->AddReserveAmmo(
                        EmergencyAmmoRequest
                    )
                    : 0;
            const int32 EmergencyMedkitsAdded =
                IsValid(InjuryComponent)
                    ? FMath::Max(
                        0,
                        EmergencyFallbackMedkits - CurrentMedkits
                    )
                    : 0;
            const int32 EmergencyDressingsAdded =
                IsValid(InjuryComponent)
                    ? FMath::Max(
                        0,
                        EmergencyFallbackDressings - CurrentDressings
                    )
                    : 0;

            if (IsValid(InjuryComponent))
            {
                InjuryComponent->AddMedicalSupplies(
                    EmergencyMedkitsAdded,
                    EmergencyDressingsAdded
                );
            }

            NextAvailableUseTime =
                CurrentTime +
                FMath::Max(0.1f, ResupplyCooldownSeconds);
            const bool bCheckpointSaved =
                IsValid(SaveSubsystem) &&
                SaveSubsystem->SaveProgress();
            Character->ShowStatusNotification(
                FText::Format(
                    NSLOCTEXT(
                        "BrokenHorizon",
                        "SectorEmergencyFallbackIssued",
                        "EMERGENCY FALLBACK KIT ISSUED\n\n"
                        "+{0} ROUNDS // +{1} MEDKIT // "
                        "+{2} DRESSING\n"
                        "NO ARMOR, GRENADES, FIRETEAM, OR VEHICLE "
                        "SERVICE // {3}"
                    ),
                    FText::AsNumber(EmergencyAmmoAdded),
                    FText::AsNumber(EmergencyMedkitsAdded),
                    FText::AsNumber(EmergencyDressingsAdded),
                    bCheckpointSaved
                        ? NSLOCTEXT(
                            "BrokenHorizon",
                            "SectorEmergencyFallbackCheckpointSaved",
                            "CHECKPOINT SAVED"
                        )
                        : NSLOCTEXT(
                            "BrokenHorizon",
                            "SectorEmergencyFallbackCheckpointFailed",
                            "CHECKPOINT SAVE FAILED"
                        )
                )
            );
            UE_LOG(
                LogTemp,
                Display,
                TEXT(
                    "BH_EMERGENCY_FALLBACK_KIT sector=%s "
                    "ammo=%d medkits=%d dressings=%d"
                ),
                *SectorID.ToString(),
                EmergencyAmmoAdded,
                EmergencyMedkitsAdded,
                EmergencyDressingsAdded
            );
            return;
        }

        const bool bCheckpointSaved =
            IsValid(SaveSubsystem) &&
            SaveSubsystem->SaveProgress();
        ShowUnavailableMessage(
            Character,
            FText::Format(
                bCheckpointSaved
                    ? NSLOCTEXT(
                        "BrokenHorizon",
                        "SectorResupplyInsufficientCheckpoint",
                        "RESUPPLY UNAVAILABLE\n\n"
                        "Sector supply is below the required {0}%.\n"
                        "Checkpoint updated."
                    )
                    : NSLOCTEXT(
                        "BrokenHorizon",
                        "SectorResupplyInsufficientNoCheckpoint",
                        "RESUPPLY UNAVAILABLE\n\n"
                        "Sector supply is below the required {0}%.\n"
                        "Checkpoint update failed."
                    ),
                FText::AsNumber(
                    FMath::CeilToInt(TotalStrategicSupplyCost)
                )
            )
        );
        return;
    }

    const int32 AmmoAdded = IsValid(WeaponComponent)
        ? WeaponComponent->AddReserveAmmo(AmmoRequest)
        : 0;

    if (IsValid(InjuryComponent))
    {
        InjuryComponent->AddMedicalSupplies(
            MedkitsNeeded,
            DressingsNeeded
        );

        if (bArmorNeedsRepair)
        {
            InjuryComponent->RepairArmor(
                BIG_NUMBER,
                BIG_NUMBER
            );
        }
    }

    const int32 FragGrenadesAdded =
        Character->AddFragGrenades(FragGrenadesNeeded);
    const int32 SmokeGrenadesAdded =
        Character->AddSmokeGrenades(SmokeGrenadesNeeded);
    const int32 EngineeringChargesAdded =
        Character->AddEngineeringCharges(EngineeringChargesNeeded);
    int32 IncapacitatedOperativesBeforeService = 0;
    int32 EvacuationRequiredBeforeService = 0;
    int32 FireteamMembersServiced = 0;
    for (ABHCharacter* ServiceCharacter : ServiceCharacters)
    {
        const int32 IncapacitatedBefore =
            ServiceCharacter->GetIncapacitatedFieldSquadCount();
        IncapacitatedOperativesBeforeService +=
            IncapacitatedBefore;
        EvacuationRequiredBeforeService +=
            ServiceCharacter
                ->GetFieldSquadMembersRequiringEvacuationCount() +
            IncapacitatedBefore;
        FireteamMembersServiced +=
            ServiceCharacter->ServiceFieldSquadMembers(
                GetActorLocation(),
                FireteamServiceRadius
            );
    }
    int32 IncapacitatedOperativesAfterService = 0;
    int32 EvacuationRequiredAfterService = 0;
    for (const ABHCharacter* ServiceCharacter : ServiceCharacters)
    {
        IncapacitatedOperativesAfterService +=
            ServiceCharacter->GetIncapacitatedFieldSquadCount();
        EvacuationRequiredAfterService +=
            ServiceCharacter
                ->GetFieldSquadMembersRequiringEvacuationCount();
    }
    const int32 CasualtiesStabilized = FMath::Max(
        0,
        IncapacitatedOperativesBeforeService -
            IncapacitatedOperativesAfterService
    );
    const int32 CasualtiesEvacuated = FMath::Max(
        0,
        EvacuationRequiredBeforeService -
            EvacuationRequiredAfterService -
            IncapacitatedOperativesAfterService
    );
    bool bVehicleRecovered = false;

    if (IsValid(RecoveryTransport))
    {
        const FVector RecoveryLocation =
            GetActorLocation() +
            (GetActorForwardVector() * 700.0f) +
            FVector(0.0f, 0.0f, 100.0f);
        const FTransform RecoveryTransform(
            GetActorRotation(),
            RecoveryLocation,
            FVector::OneVector
        );
        bVehicleRecovered =
            RecoveryTransport->RecoverAndService(
                RecoveryTransform
            );
    }

    const bool bVehicleServiced =
        bVehicleRecovered ||
        (IsValid(ServiceTransport) &&
         ServiceTransport->ServiceVehicle());

    NextAvailableUseTime =
        CurrentTime + FMath::Max(0.1f, ResupplyCooldownSeconds);

    const FBHWarSectorState UpdatedSector =
        WarSubsystem->GetSectorState(SectorID);
    const bool bCheckpointSaved =
        IsValid(SaveSubsystem) &&
        SaveSubsystem->SaveProgress();
    Character->ShowStatusNotification(
        FText::Format(
            NSLOCTEXT(
                "BrokenHorizon",
                "SectorResupplyComplete",
                "SECTOR RESUPPLY COMPLETE\n\n"
                "+{0} ROUNDS // +{1} MEDKITS // +{2} DRESSINGS\n"
                "+{3} FRAGS // +{10} ENGINEERING CHARGES // +{11} SMOKES\n"
                "FIRETEAM {4} SERVICED // "
                "CASUALTIES {8} STABILIZED / {9} EVACUATED\n"
                "ARMOR SERVICED // SECTOR SUPPLY {5}%\n"
                "VEHICLE {6} // {7}"
            ),
            FText::AsNumber(AmmoAdded),
            FText::AsNumber(MedkitsNeeded),
            FText::AsNumber(DressingsNeeded),
            FText::AsNumber(FragGrenadesAdded),
            FText::AsNumber(FireteamMembersServiced),
            FText::AsNumber(FMath::RoundToInt(
                UpdatedSector.Supply
            )),
            bVehicleRecovered
                ? NSLOCTEXT(
                    "BrokenHorizon",
                    "SectorVehicleRecovered",
                    "RECOVERED / REFUELED / REPAIRED"
                )
                : bVehicleServiced
                    ? NSLOCTEXT(
                        "BrokenHorizon",
                        "SectorVehicleServiced",
                        "REFUELED / REPAIRED"
                    )
                    : NSLOCTEXT(
                        "BrokenHorizon",
                        "SectorVehicleNotServiced",
                        "NO SERVICE REQUIRED"
                    ),
            bCheckpointSaved
                ? NSLOCTEXT(
                    "BrokenHorizon",
                    "SectorResupplyCheckpointSaved",
                    "CHECKPOINT SAVED"
                )
                : NSLOCTEXT(
                    "BrokenHorizon",
                    "SectorResupplyCheckpointFailed",
                    "CHECKPOINT SAVE FAILED"
                ),
            FText::AsNumber(CasualtiesStabilized),
            FText::AsNumber(CasualtiesEvacuated),
            FText::AsNumber(EngineeringChargesAdded),
            FText::AsNumber(SmokeGrenadesAdded)
        )
    );

    if (CasualtiesEvacuated > 0)
    {
        UE_LOG(
            LogTemp,
            Display,
            TEXT(
                "BH_FIELD_OPERATIVE_EVACUATED sector=%s count=%d"
            ),
            *SectorID.ToString(),
            CasualtiesEvacuated
        );
    }
}

float ABHSectorResupplyStation::CalculateFireteamServiceSupplyCost(
    int32 MembersNeedingService,
    float SupplyCostPerMember
)
{
    return FMath::Max(0, MembersNeedingService) *
        FMath::Max(0.0f, SupplyCostPerMember);
}

bool ABHSectorResupplyStation::ShouldIssueEmergencyFallbackKit(
    int32 ReserveAmmo,
    int32 MedkitCount,
    int32 FieldDressingCount
)
{
    return ReserveAmmo <= 0 ||
        (MedkitCount <= 0 && FieldDressingCount <= 0);
}

int32 ABHSectorResupplyStation::
CalculateEmergencyFallbackAmmoRequest(
    int32 ReserveAmmo,
    int32 MaximumReserveAmmo,
    int32 FallbackAmmoAmount
)
{
    return FMath::Min(
        FMath::Max(0, FallbackAmmoAmount),
        FMath::Max(0, MaximumReserveAmmo - ReserveAmmo)
    );
}

float ABHSectorResupplyStation::GetResupplySupplyCost(
    int32 FireteamMembersNeedingService
) const
{
    return FMath::Max(0.0f, StrategicSupplyCost) +
        CalculateFireteamServiceSupplyCost(
            FireteamMembersNeedingService,
            FireteamServiceSupplyCostPerMember
        );
}

FText
ABHSectorResupplyStation::GetInteractionText_Implementation() const
{
    const UWorld* World = GetWorld();
    const UGameInstance* GameInstance = IsValid(World)
        ? World->GetGameInstance()
        : nullptr;
    const UBHWarSubsystem* WarSubsystem = IsValid(GameInstance)
        ? GameInstance->GetSubsystem<UBHWarSubsystem>()
        : nullptr;

    if (!IsValid(World) ||
        !IsValid(WarSubsystem) ||
        SectorID.IsNone())
    {
        return FText::GetEmpty();
    }

    const FBHWarSectorState Sector =
        WarSubsystem->GetSectorState(SectorID);

    if (Sector.Owner != EBHWarFaction::Friendly)
    {
        return NSLOCTEXT(
            "BrokenHorizon",
            "SectorResupplyLockedInteraction",
            "Sector Resupply Locked"
        );
    }

    if (World->GetTimeSeconds() < NextAvailableUseTime)
    {
        return NSLOCTEXT(
            "BrokenHorizon",
            "SectorResupplyCooldownInteraction",
            "Sector Resupply Recharging"
        );
    }

    if (Sector.Supply + KINDA_SMALL_NUMBER <
        StrategicSupplyCost)
    {
        return NSLOCTEXT(
            "BrokenHorizon",
            "SectorEmergencyFallbackInteraction",
            "[F] Emergency Fallback Kit"
        );
    }

    return NSLOCTEXT(
        "BrokenHorizon",
        "SectorResupplyInteraction",
        "[F] Resupply / Vehicle Support // [C] Recruit Fireteam"
    );
}

void ABHSectorResupplyStation::ConfigureStation(
    FName NewSectorID
)
{
    SectorID = NewSectorID;
}

FName ABHSectorResupplyStation::GetSectorID() const
{
    return SectorID;
}

void ABHSectorResupplyStation::ShowUnavailableMessage(
    ABHCharacter* Character,
    const FText& Message
) const
{
    if (IsValid(Character))
    {
        Character->ShowStatusNotification(Message);
    }
}
