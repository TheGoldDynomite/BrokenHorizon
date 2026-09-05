#include "BHFieldFortification.h"

#include "BHCharacter.h"
#include "BHSaveSubsystem.h"
#include "BHWarSubsystem.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

ABHFieldFortification::ABHFieldFortification()
{
    PrimaryActorTick.bCanEverTick = true;
    bReplicates = true;
    SetReplicateMovement(true);
    SetCanBeDamaged(true);

#if WITH_EDITOR
    SetIsSpatiallyLoaded(false);
#endif

    BarricadeMesh = CreateDefaultSubobject<UStaticMeshComponent>(
        TEXT("BarricadeMesh")
    );
    BarricadeMesh->SetupAttachment(GetRootComponent());
    BarricadeMesh->SetRelativeLocation(FVector(0.0f, 0.0f, 65.0f));
    BarricadeMesh->SetRelativeScale3D(FVector(0.45f, 2.4f, 0.65f));
    BarricadeMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    BarricadeMesh->SetCollisionResponseToAllChannels(ECR_Block);

    static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(
        TEXT("/Engine/BasicShapes/Cube.Cube")
    );
    if (CubeMesh.Succeeded())
    {
        BarricadeMesh->SetStaticMesh(CubeMesh.Object);
    }

    StatusLabel = CreateDefaultSubobject<UTextRenderComponent>(
        TEXT("StatusLabel")
    );
    StatusLabel->SetupAttachment(GetRootComponent());
    StatusLabel->SetRelativeLocation(FVector(0.0f, 0.0f, 190.0f));
    StatusLabel->SetHorizontalAlignment(EHTA_Center);
    StatusLabel->SetWorldSize(24.0f);
    StatusLabel->SetTextRenderColor(FColor(245, 174, 48));

    ActivePlanProfile = BuildPlanProfile(SelectedPlan);
}

void ABHFieldFortification::BeginPlay()
{
    Super::BeginPlay();
    ApplyPlanProfile(ActivePlanProfile);
    RefreshPresentation();
}

void ABHFieldFortification::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    ReconcileWorkProgress(DeltaSeconds);
}

void ABHFieldFortification::GetLifetimeReplicatedProps(
    TArray<FLifetimeProperty>& OutLifetimeProps
) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ABHFieldFortification, bConstructed);
    DOREPLIFETIME(ABHFieldFortification, CurrentHealth);
    DOREPLIFETIME(ABHFieldFortification, SelectedPlan);
    DOREPLIFETIME(ABHFieldFortification, WorkProgress);
    DOREPLIFETIME(ABHFieldFortification, ActiveWorkerCount);
    DOREPLIFETIME(ABHFieldFortification, bDismantleWork);
    DOREPLIFETIME(ABHFieldFortification, SupplyCacheChargesRemaining);
    DOREPLIFETIME(ABHFieldFortification, ObservationProgress);
    DOREPLIFETIME(ABHFieldFortification, LastObservationTurn);
    DOREPLIFETIME(ABHFieldFortification, RallyDeploymentsRemaining);
}

float ABHFieldFortification::TakeDamage(
    float DamageAmount,
    const FDamageEvent& DamageEvent,
    AController* EventInstigator,
    AActor* DamageCauser
)
{
    const float AppliedDamage = Super::TakeDamage(
        DamageAmount,
        DamageEvent,
        EventInstigator,
        DamageCauser
    );
    if (!HasAuthority() || !bConstructed || DamageAmount <= 0.0f)
    {
        return AppliedDamage;
    }

    const float HealthBeforeDamage = CurrentHealth;
    CurrentHealth = FMath::Max(0.0f, HealthBeforeDamage - DamageAmount);
    if (CurrentHealth <= KINDA_SMALL_NUMBER)
    {
        const FBHFortificationPlanProfile Profile = GetActivePlanProfile();
        bConstructed = false;
        CurrentHealth = 0.0f;
        const float PreDestroyHealthFraction = FMath::Clamp(
            HealthBeforeDamage / FMath::Max(0.01f, Profile.MaximumHealth),
            0.0f,
            1.0f
        );
        const float SalvageFraction = FMath::Clamp(
            CalculateDismantleRecovery(
                Profile.ConstructionSupplyCost,
                PreDestroyHealthFraction,
                DismantleRecoveryMultiplier
            ) / FMath::Max(0.01f, Profile.ConstructionSupplyCost),
            0.0f,
            1.0f
        );
        WorkProgress = FMath::Max(SalvageFraction, 0.0f);
        bDismantleWork = true;
        UE_LOG(
            LogTemp,
            Display,
            TEXT("BH_FORTIFICATION_DESTROYED id=%s sector=%s"),
            *PersistenceID.ToString(),
            *SectorID.ToString()
        );
    }
    RefreshPresentation();
    ForceNetUpdate();
    return DamageAmount;
}

void ABHFieldFortification::Interact_Implementation(
    AActor* InteractingActor
)
{
    ABHCharacter* Character = Cast<ABHCharacter>(InteractingActor);
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

    if (!HasAuthority() || !IsValid(Character) ||
        !IsValid(WarSubsystem) || SectorID.IsNone())
    {
        return;
    }

    const FBHWarSectorState Sector = WarSubsystem->GetSectorState(SectorID);
    if (Sector.SectorID.IsNone() || Sector.Owner != EBHWarFaction::Friendly)
    {
        NotifyPlayer(
            Character,
            NSLOCTEXT(
                "BrokenHorizon",
                "FortificationSectorLocked",
                "ENGINEERING LOCKED\n\nSecure friendly control of this sector first."
            )
        );
        return;
    }
    if (Sector.EnemyResponsePressure >= HeavyCombatPlacementPressure)
    {
        NotifyPlayer(
            Character,
            NSLOCTEXT(
                "BrokenHorizon",
                "FortificationUnderFire",
                "UNDER FIRE\n\nConstruction and repair are too dangerous while this sector is under heavy pressure."
            )
        );
        return;
    }

    int32 ConstructedFortifications = 0;
    int32 UnfinishedFortifications = 0;
    float SectorFortificationDefense = 0.0f;
    WarSubsystem->GetSectorFortificationSummary(
        SectorID,
        ConstructedFortifications,
        UnfinishedFortifications,
        SectorFortificationDefense
    );
    if (!bConstructed &&
        WorkProgress <= KINDA_SMALL_NUMBER &&
        (ConstructedFortifications + UnfinishedFortifications) >= MaxFortificationsPerSector)
    {
        const int32 ExistingFortifications =
            ConstructedFortifications + UnfinishedFortifications;
        NotifyPlayer(
            Character,
            FText::Format(
                NSLOCTEXT(
                    "BrokenHorizon",
                    "FortificationSiteFull",
                    "FIELD DEPLOYMENT LIMIT\n\n{0} fortification already occupies this sector."
                ),
                FText::AsNumber(ExistingFortifications)
            )
        );
        return;
    }

    const FBHFortificationPlanProfile PlanProfile = GetActivePlanProfile();
    if (bConstructed && FMath::IsNearlyEqual(GetHealthFraction(), 1.0f))
    {
        if (SupplyCacheChargesRemaining < MaxSupplyCacheCharges &&
            SelectedPlan == EBHFortificationPlan::FieldSupplyCache)
        {
        const int32 MissingCharges =
            MaxSupplyCacheCharges - SupplyCacheChargesRemaining;
            const int32 AddedCharges = FMath::Clamp(
                MissingCharges,
                0,
                1
            );
            const float RefillCost = AddedCharges * SupplyCacheRefillCostPerCharge;
            if (!WarSubsystem->ConsumeSectorSupply(SectorID, RefillCost))
            {
                NotifyPlayer(
                    Character,
                    FText::Format(
                        NSLOCTEXT(
                            "BrokenHorizon",
                            "FortificationInsufficientSupply",
                            "ENGINEERING SUPPLY REQUIRED\n\n{0}% sector supply required."
                        ),
                        FText::AsNumber(FMath::CeilToInt(RefillCost))
                    )
                );
                return;
            }
                if (AddedCharges > 0)
                {
                    SupplyCacheChargesRemaining =
                        FMath::Min(
                            MaxSupplyCacheCharges,
                            SupplyCacheChargesRemaining + AddedCharges
                        );
                    const bool bSavedWithSupply =
                        IsValid(SaveSubsystem) &&
                        SaveSubsystem->SaveProgressForCharacter(Character);
                    NotifyPlayer(
                        Character,
                    FText::Format(
                        NSLOCTEXT(
                            "BrokenHorizon",
                            "FortificationCacheRefill",
                            "FORTIFICATION REFILLED\n\n"
                            "Supply cache charged // remaining {0}"
                        ),
                        FText::AsNumber(SupplyCacheChargesRemaining)
                    )
                );
                if (!bSavedWithSupply)
                {
                    NotifyPlayer(
                        Character,
                        NSLOCTEXT(
                            "BrokenHorizon",
                            "FortificationSaveFailed",
                            "CHECKPOINT SAVE FAILED"
                        )
                    );
                }
                RefreshPresentation();
                ForceNetUpdate();
                return;
            }
        }

        NotifyPlayer(
            Character,
            NSLOCTEXT(
                "BrokenHorizon",
                "FortificationFullHealth",
                "FORTIFICATION READY\n\nBarricade integrity is at 100%."
            )
        );
        return;
    }

    const float CurrentFraction = bConstructed
        ? GetHealthFraction()
        : WorkProgress;
    const float WorkRate = FMath::Min(
        1.0f,
        CalculateAssistedWorkRate(
            FMath::Max(1, ActiveWorkerCount),
            CurrentFraction,
            1.0f / FMath::Max(0.25f, PlanProfile.ConstructionWorkDuration)
        ) * ConstructionUpdateIntervalSeconds
    );
    if (bConstructed)
    {
        const float RepairableFraction = FMath::Max(0.0f, 1.0f - CurrentFraction);
        const float RepairDelta = FMath::Min(WorkRate, RepairableFraction);
        const float CurrentRepairCost = CalculateRepairSupplyCost(
            CurrentFraction,
            FullRepairSupplyCost
        );
        const float NewHealthFraction = CurrentFraction + RepairDelta;
        const float NewRepairCost = CalculateRepairSupplyCost(
            NewHealthFraction,
            FullRepairSupplyCost
        );
        const float RepairCost = FMath::Max(0.0f, CurrentRepairCost - NewRepairCost);

        if (!FMath::IsNearlyZero(RepairCost) &&
            !WarSubsystem->ConsumeSectorSupply(SectorID, RepairCost))
        {
            NotifyPlayer(
                Character,
                FText::Format(
                    NSLOCTEXT(
                        "BrokenHorizon",
                        "FortificationInsufficientSupply",
                        "ENGINEERING SUPPLY REQUIRED\n\n{0}% sector supply required."
                    ),
                    FText::AsNumber(FMath::CeilToInt(RepairCost))
                )
            );
            return;
        }
        bDismantleWork = false;
        CurrentHealth = FMath::Clamp(
            GetCurrentPlanHealth() * NewHealthFraction,
            1.0f,
            GetCurrentPlanHealth()
        );
        WorkProgress = GetHealthFraction();

        RefreshPresentation();
        ForceNetUpdate();

        const bool bFullyRepaired = GetHealthFraction() >= 1.0f;
        const bool bSaved = IsValid(SaveSubsystem) &&
            SaveSubsystem->SaveProgressForCharacter(Character);
        if (bFullyRepaired)
        {
            NotifyPlayer(
                Character,
                FText::Format(
                    NSLOCTEXT(
                        "BrokenHorizon",
                        "FortificationRepaired",
                        "FORTIFICATION REPAIRED\n\nIntegrity 100% // sector supply {0}% // {1}"
                    ),
                    FText::AsNumber(FMath::RoundToInt(Sector.Supply)),
                    bSaved
                        ? NSLOCTEXT(
                            "BrokenHorizon",
                            "FortificationSaved",
                            "CHECKPOINT SAVED")
                        : NSLOCTEXT(
                            "BrokenHorizon",
                            "FortificationSaveFailed",
                            "CHECKPOINT SAVE FAILED")
                )
            );
        }
        else
        {
            NotifyPlayer(
                Character,
                FText::Format(
                    NSLOCTEXT(
                        "BrokenHorizon",
                        "FortificationContinueRepair",
                        "FORTIFICATION REPAIRING\n\nIntegrity {0}% // sector supply {1}% // {2}"
                    ),
                    FText::AsNumber(
                        FMath::RoundToInt(GetHealthFraction() * 100.0f)
                    ),
                    FText::AsNumber(FMath::RoundToInt(Sector.Supply)),
                    bSaved
                        ? NSLOCTEXT(
                            "BrokenHorizon",
                            "FortificationSaved",
                            "CHECKPOINT SAVED")
                        : NSLOCTEXT(
                            "BrokenHorizon",
                            "FortificationSaveFailed",
                            "CHECKPOINT SAVE FAILED")
                )
            );
        }
        UE_LOG(
            LogTemp,
            Display,
            TEXT("BH_FORTIFICATION_%s id=%s sector=%s cost=%.1f health=%.1f"),
            bFullyRepaired ? TEXT("REPAIRED") : TEXT("REPAIRING"),
            *PersistenceID.ToString(),
            *SectorID.ToString(),
            RepairCost,
            CurrentHealth
        );
        return;
    }

    const float ConstructionRemaining = FMath::Max(0.0f, 1.0f - CurrentFraction);
    const float ConstructionDelta = FMath::Min(WorkRate, ConstructionRemaining);
    const float ConstructionCost =
        PlanProfile.ConstructionSupplyCost * ConstructionDelta * (bDismantleWork ? 1.25f : 1.0f);
    if (!WarSubsystem->ConsumeSectorSupply(SectorID, ConstructionCost))
    {
        if (!bConstructed && WorkProgress < 1.0f)
        {
            NotifyPlayer(
                Character,
                FText::Format(
                    NSLOCTEXT(
                        "BrokenHorizon",
                        "FortificationInsufficientSupply",
                        "ENGINEERING SUPPLY REQUIRED\n\n{0}% sector supply required."
                    ),
                    FText::AsNumber(FMath::CeilToInt(ConstructionCost))
                )
            );
            return;
        }
    }

    SetConstructionWorkers(FMath::Max(1, ActiveWorkerCount));
    bConstructed = false;
    bDismantleWork = false;
    WorkProgress = FMath::Clamp(WorkProgress + ConstructionDelta, 0.0f, 1.0f);
    CurrentHealth = GetCurrentPlanHealth() * WorkProgress;

    const bool bFinished = WorkProgress >= 1.0f - KINDA_SMALL_NUMBER;
    if (bFinished)
    {
        bConstructed = true;
        WorkProgress = 1.0f;
        CurrentHealth = GetCurrentPlanHealth();
        SetConstructionWorkers(0);
        if (SelectedPlan == EBHFortificationPlan::FieldSupplyCache)
        {
            SupplyCacheChargesRemaining = MaxSupplyCacheCharges;
        }
        else if (SelectedPlan == EBHFortificationPlan::FieldRallyPoint)
        {
            RallyDeploymentsRemaining = MaxRallyDeployments;
        }
    }

    RefreshPresentation();
    ForceNetUpdate();
    const bool bSaved = IsValid(SaveSubsystem) &&
        SaveSubsystem->SaveProgressForCharacter(Character);
    NotifyPlayer(
        Character,
        FText::Format(
            bFinished
                ? NSLOCTEXT(
                    "BrokenHorizon",
                    "FortificationConstructed",
                    "FORTIFICATION CONSTRUCTED\n\nAI cover active // sector supply {0}% // {1}"
                )
                : NSLOCTEXT(
                    "BrokenHorizon",
                    "FortificationBuildInProgress",
                    "FORTIFICATION BUILDING\n\n{0}% complete // sector supply {1}% // {2}"
                ),
            FText::AsNumber(FMath::RoundToInt(WorkProgress * 100.0f)),
            FText::AsNumber(FMath::RoundToInt(Sector.Supply)),
            bSaved
                ? NSLOCTEXT("BrokenHorizon", "FortificationSaved", "CHECKPOINT SAVED")
                : NSLOCTEXT("BrokenHorizon", "FortificationSaveFailed", "CHECKPOINT SAVE FAILED")
        )
    );
    UE_LOG(
        LogTemp,
        Display,
        TEXT("BH_FORTIFICATION_%s id=%s sector=%s cost=%.1f health=%.1f"),
        bFinished ? TEXT("BUILT") : TEXT("BUILDING"),
        *PersistenceID.ToString(),
        *SectorID.ToString(),
        bFinished ? PlanProfile.ConstructionSupplyCost : ConstructionCost,
        CurrentHealth
    );
}

FText ABHFieldFortification::GetInteractionText_Implementation() const
{
    const bool bRubbleSite = bDismantleWork && !bConstructed;
    int32 ConstructedFortifications = 0;
    int32 UnfinishedFortifications = 0;
    float SectorFortificationDefense = 0.0f;
    UWorld* World = GetWorld();
    UGameInstance* GameInstance = IsValid(World)
        ? World->GetGameInstance()
        : nullptr;
    UBHWarSubsystem* WarSubsystem = IsValid(GameInstance)
        ? GameInstance->GetSubsystem<UBHWarSubsystem>()
        : nullptr;
    if (IsValid(WarSubsystem) && !SectorID.IsNone())
    {
        WarSubsystem->GetSectorFortificationSummary(
            SectorID,
            ConstructedFortifications,
            UnfinishedFortifications,
            SectorFortificationDefense
        );
    }
    const int32 ExistingFortifications =
        ConstructedFortifications + UnfinishedFortifications;

    if (!bConstructed && WorkProgress <= KINDA_SMALL_NUMBER)
    {
        const FBHFortificationPlanProfile PlanProfile =
            GetActivePlanProfile();
        const float PreviewConstructionCost = PlanProfile.ConstructionSupplyCost *
            FMath::Min(
                ConstructionUpdateIntervalSeconds / FMath::Max(0.25f, PlanProfile.ConstructionWorkDuration),
                1.0f
            );
        const float PreviewCostModifier = bRubbleSite ? 1.25f : 1.0f;
        if (bRubbleSite)
        {
            return FText::Format(
                NSLOCTEXT(
                    "BrokenHorizon",
                    "BuildRubblePrompt",
                    "[F] Rebuild Fortification // {0}% Supply // SITE {1}/{2}"
                ),
                FText::AsNumber(
                    FMath::CeilToInt(PreviewConstructionCost * PreviewCostModifier)
                ),
                FText::AsNumber(ExistingFortifications),
                FText::AsNumber(MaxFortificationsPerSector)
            );
        }
        return FText::Format(
            NSLOCTEXT(
                "BrokenHorizon",
                "BuildFortificationPrompt",
                "[F] Build Field Fortification // {0}% Supply // SITE {1}/{2}"
            ),
            FText::AsNumber(
                FMath::CeilToInt(PreviewConstructionCost * PreviewCostModifier)
            ),
            FText::AsNumber(ExistingFortifications),
            FText::AsNumber(MaxFortificationsPerSector)
        );
    }
    if (!bConstructed)
    {
        return FText::Format(
            NSLOCTEXT(
                "BrokenHorizon",
                "FortificationBuildProgressPrompt",
                "[F] Continue Engineering // {0}% // SITE {1}/{2}"
            ),
            FText::AsNumber(FMath::RoundToInt(GetConstructionProgress() * 100.0f)),
            FText::AsNumber(ExistingFortifications),
            FText::AsNumber(MaxFortificationsPerSector)
        );
    }
    if (GetHealthFraction() >= 1.0f)
    {
        if (SelectedPlan == EBHFortificationPlan::FieldSupplyCache &&
            SupplyCacheChargesRemaining < MaxSupplyCacheCharges)
        {
            return FText::Format(
                NSLOCTEXT(
                    "BrokenHorizon",
                    "FortificationCacheRefillPrompt",
                    "[F] Refill Supply Cache // +{0} // {1}% Supply"
                ),
                FText::AsNumber(
                    1
                ),
                FText::AsNumber(FMath::CeilToInt(
                    SupplyCacheRefillCostPerCharge
                ))
            );
        }
        const float SupplyCost = CalculateRepairSupplyCost(
            GetHealthFraction(),
            FullRepairSupplyCost
        );
        return SupplyCost > KINDA_SMALL_NUMBER
            ? FText::Format(
                NSLOCTEXT(
                    "BrokenHorizon",
                    "FortificationMaintainPrompt",
                    "[F] Repair Fortification // {0}% Supply"
                ),
                FText::AsNumber(FMath::CeilToInt(SupplyCost))
            )
            : NSLOCTEXT(
                "BrokenHorizon",
                "FortificationReadyPrompt",
                "Field Fortification // Ready"
            );
    }

    return FText::Format(
        NSLOCTEXT(
            "BrokenHorizon",
            "FortificationMaintainPrompt",
            "[F] Repair Fortification // {0}% Supply"
        ),
        FText::AsNumber(
            FMath::CeilToInt(
                CalculateRepairSupplyCost(
                    GetHealthFraction(),
                    FullRepairSupplyCost
                )
            )
        )
    );
}

void ABHFieldFortification::ConfigureFortification(
    FName NewPersistenceID,
    FName NewSectorID
)
{
    PersistenceID = NewPersistenceID;
    SectorID = NewSectorID;
    RefreshPresentation();
}

FName ABHFieldFortification::GetPersistenceID() const
{
    return PersistenceID;
}

FName ABHFieldFortification::GetSectorID() const
{
    return SectorID;
}

bool ABHFieldFortification::IsConstructed() const
{
    return bConstructed;
}

float ABHFieldFortification::GetHealthFraction() const
{
    return GetCurrentPlanHealth() > KINDA_SMALL_NUMBER
        ? FMath::Clamp(CurrentHealth / GetCurrentPlanHealth(), 0.0f, 1.0f)
        : 0.0f;
}

float ABHFieldFortification::GetConstructionProgress() const
{
    return FMath::Clamp(WorkProgress, 0.0f, 1.0f);
}

void ABHFieldFortification::SetConstructionWorkers(int32 WorkerCount)
{
    if (!HasAuthority())
    {
        return;
    }
    ActiveWorkerCount = FMath::Max(0, WorkerCount);
    ForceNetUpdate();
}

float ABHFieldFortification::GetStrategicDefenseValue() const
{
    return GetCurrentPlanProfileDefense();
}

float ABHFieldFortification::GetAICoverQuality() const
{
    if (!bConstructed || CurrentHealth <= KINDA_SMALL_NUMBER)
    {
        return 0.0f;
    }
    const float HealthFactor = FMath::Pow(
        FMath::Clamp(GetHealthFraction(), 0.0f, 1.0f),
        1.5f
    );
    return GetActivePlanProfile().AICoverQuality * HealthFactor;
}

float ABHFieldFortification::GetWeaponBraceQuality() const
{
    if (!bConstructed || CurrentHealth <= KINDA_SMALL_NUMBER)
    {
        return 0.0f;
    }
    const float HealthFactor = FMath::Pow(
        FMath::Clamp(GetHealthFraction(), 0.0f, 1.0f),
        1.35f
    );
    return GetActivePlanProfile().WeaponBraceQuality * HealthFactor;
}

EBHFortificationPlan ABHFieldFortification::GetSelectedPlan() const
{
    return SelectedPlan;
}

void ABHFieldFortification::SetSelectedPlan(EBHFortificationPlan NewPlan)
{
    if (!HasAuthority())
    {
        return;
    }
    if (NewPlan == SelectedPlan)
    {
        return;
    }
    if (WorkProgress > KINDA_SMALL_NUMBER && (ActiveWorkerCount > 0 ||
            (bDismantleWork && !bConstructed) || (bConstructed && WorkProgress < 1.0f)))
    {
        return;
    }
    SelectedPlan = NewPlan;
    ActivePlanProfile = BuildPlanProfile(SelectedPlan);
    ApplyPlanProfile(ActivePlanProfile);
    RefreshPresentation();
    ForceNetUpdate();
}

int32 ABHFieldFortification::GetSupplyCacheChargesRemaining() const
{
    return SupplyCacheChargesRemaining;
}

int32 ABHFieldFortification::GetMaxSupplyCacheCharges() const
{
    return MaxSupplyCacheCharges;
}

int32 ABHFieldFortification::GetRallyDeploymentsRemaining() const
{
    return RallyDeploymentsRemaining;
}

int32 ABHFieldFortification::GetMaxRallyDeployments() const
{
    return MaxRallyDeployments;
}

int32 ABHFieldFortification::GetLastObservationTurn() const
{
    return LastObservationTurn;
}

float ABHFieldFortification::GetObservationProgress() const
{
    return ObservationProgress;
}

int32 ABHFieldFortification::ConsumeRallyDeployments(
    int32 DeploymentCount
)
{
    const int32 SafeDeploymentCount = FMath::Max(1, DeploymentCount);
    if (!HasAuthority() ||
        SelectedPlan != EBHFortificationPlan::FieldRallyPoint ||
        !bConstructed ||
        GetHealthFraction() < 0.5f ||
        RallyDeploymentsRemaining <= 0)
    {
        return 0;
    }

    const int32 Consumed = FMath::Min(
        SafeDeploymentCount,
        RallyDeploymentsRemaining
    );
    RallyDeploymentsRemaining -= Consumed;
    if (Consumed > 0)
    {
        ForceNetUpdate();
    }
    return Consumed;
}

int32 ABHFieldFortification::ConsumeSupplyCacheCharges(
    int32 ChargeCount
)
{
    const int32 SafeChargeCount = FMath::Max(1, ChargeCount);
    if (!HasAuthority() ||
        SelectedPlan != EBHFortificationPlan::FieldSupplyCache ||
        !bConstructed ||
        GetHealthFraction() < 0.25f ||
        SupplyCacheChargesRemaining <= 0)
    {
        return 0;
    }

    const int32 Consumed = FMath::Min(
        SafeChargeCount,
        SupplyCacheChargesRemaining
    );
    SupplyCacheChargesRemaining -= Consumed;
    if (Consumed > 0)
    {
        ForceNetUpdate();
    }
    return Consumed;
}

bool ABHFieldFortification::IsDismantling() const
{
    return bDismantleWork;
}

int32 ABHFieldFortification::GetActiveWorkerCount() const
{
    return ActiveWorkerCount;
}

void ABHFieldFortification::ReportObservationProgress(int32 CurrentTurn)
{
    if (SelectedPlan != EBHFortificationPlan::ObservationPost)
    {
        return;
    }
    ObservationProgress = 1.0f;
    LastObservationTurn = FMath::Max(0, CurrentTurn);
    if (HasAuthority())
    {
        ForceNetUpdate();
    }
}

void ABHFieldFortification::RestorePersistentState(
    const FTransform& SavedTransform,
    bool bSavedConstructed,
    float SavedHealthFraction,
    EBHFortificationPlan SavedPlan,
    float SavedWorkProgress,
    bool bSavedDismantleWork,
    int32 SavedActiveWorkerCount,
    int32 SavedSupplyCacheCharges,
    int32 SavedLastObservationTurn,
    int32 SavedRallyDeploymentsRemaining,
    float SavedObservationProgress
)
{
    if (!HasAuthority())
    {
        return;
    }
    SetActorTransform(SavedTransform, false, nullptr, ETeleportType::TeleportPhysics);
    SelectedPlan = SavedPlan;
    ActivePlanProfile = BuildPlanProfile(SelectedPlan);
    ApplyPlanProfile(ActivePlanProfile);
    bConstructed = bSavedConstructed;
    WorkProgress = FMath::Clamp(SavedWorkProgress, 0.0f, 1.0f);
    bDismantleWork = bSavedDismantleWork;
    bDismantleWork =
        bDismantleWork && WorkProgress <= KINDA_SMALL_NUMBER &&
        !bConstructed;
    ActiveWorkerCount = FMath::Max(0, SavedActiveWorkerCount);
    SupplyCacheChargesRemaining = FMath::Max(0, SavedSupplyCacheCharges);
    ObservationProgress = FMath::Clamp(
        SavedObservationProgress,
        0.0f,
        1.0f
    );
    LastObservationTurn = SavedLastObservationTurn;
    RallyDeploymentsRemaining = FMath::Max(0, SavedRallyDeploymentsRemaining);
    CurrentHealth = bConstructed
        ? FMath::Clamp(SavedHealthFraction, 0.01f, 1.0f) * GetCurrentPlanHealth()
        : 0.0f;
    RefreshPresentation();
    ForceNetUpdate();
}

bool ABHFieldFortification::CanConstructForSector(
    const FBHWarSectorState& Sector,
    float RequiredSupply
)
{
    return !Sector.SectorID.IsNone() &&
        Sector.Owner == EBHWarFaction::Friendly &&
        Sector.Supply + KINDA_SMALL_NUMBER >= FMath::Max(0.0f, RequiredSupply);
}

float ABHFieldFortification::CalculateRepairSupplyCost(
    float HealthFraction,
    float InFullRepairSupplyCost
)
{
    return FMath::Max(0.0f, InFullRepairSupplyCost) *
        (1.0f - FMath::Clamp(HealthFraction, 0.0f, 1.0f));
}

float ABHFieldFortification::CalculateWorkProgress(
    float WorkHours,
    float WorkDuration
)
{
    return WorkDuration > KINDA_SMALL_NUMBER
        ? FMath::Clamp(WorkHours / WorkDuration, 0.0f, 1.0f)
        : 1.0f;
}

float ABHFieldFortification::CalculateAssistedWorkRate(
    int32 ActiveWorkers,
    float WorkProgress,
    float BaseWorkRate
)
{
    const float Workers = FMath::Max(
        0.0f,
        static_cast<float>(ActiveWorkers)
    );
    const float Progress = FMath::Clamp(WorkProgress, 0.0f, 1.0f);
    const float WorkerDensity = FMath::Clamp(Workers / 8.0f, 0.0f, 1.0f);
    const float WorkerExponent = 1.18836f;
    const float DiminishedWorkerEffect = FMath::Pow(
        WorkerDensity,
        WorkerExponent
    );
    const float AssistedProgress = FMath::Clamp(
        Progress + ((1.0f - Progress) * DiminishedWorkerEffect),
        Progress,
        1.0f
    );
    return BaseWorkRate * AssistedProgress;
}

float ABHFieldFortification::CalculateDismantleRecovery(
    float ConstructionCost,
    float HealthFraction,
    float RecoveryMultiplier
)
{
    const float ClampedHealth = FMath::Clamp(HealthFraction, 0.0f, 1.0f);
    return FMath::Max(0.0f, ConstructionCost) *
        ClampedHealth *
        FMath::Max(0.0f, RecoveryMultiplier);
}

bool ABHFieldFortification::ShouldWorkerRemainAssigned(
    bool bPlayerActive,
    bool bPlayerIncapacitated,
    bool bPlayerBusy,
    float DistanceToFortification,
    float WorkRadius
)
{
    return bPlayerActive && !bPlayerIncapacitated && !bPlayerBusy &&
        DistanceToFortification <= WorkRadius;
}

FBHFortificationPlanProfile ABHFieldFortification::BuildPlanProfile(
    EBHFortificationPlan Plan
)
{
    FBHFortificationPlanProfile Profile;
    switch (Plan)
    {
    case EBHFortificationPlan::ReinforcedBulwark:
        Profile.ConstructionSupplyCost = 18.0f;
        Profile.ConstructionWorkDuration = 7.8f;
        Profile.MaximumHealth = 840.0f;
        Profile.AICoverQuality = 0.95f;
        Profile.WeaponBraceQuality = 0.7f;
        Profile.StrategicDefenseValue = 4.8f;
        Profile.MeshScale = FVector(0.55f, 2.6f, 0.7f);
        break;
    case EBHFortificationPlan::FiringPosition:
        Profile.ConstructionSupplyCost = 16.0f;
        Profile.ConstructionWorkDuration = 3.8f;
        Profile.MaximumHealth = 420.0f;
        Profile.AICoverQuality = 0.4f;
        Profile.WeaponBraceQuality = 1.0f;
        Profile.StrategicDefenseValue = 1.5f;
        Profile.MeshScale = FVector(0.46f, 2.0f, 0.58f);
        break;
    case EBHFortificationPlan::FieldSupplyCache:
        Profile.ConstructionSupplyCost = 28.0f;
        Profile.ConstructionWorkDuration = 8.0f;
        Profile.MaximumHealth = 380.0f;
        Profile.AICoverQuality = 0.3f;
        Profile.WeaponBraceQuality = 0.0f;
        Profile.StrategicDefenseValue = 1.1f;
        Profile.MeshScale = FVector(0.45f, 2.2f, 0.55f);
        break;
    case EBHFortificationPlan::ObservationPost:
        Profile.ConstructionSupplyCost = 20.0f;
        Profile.ConstructionWorkDuration = 8.5f;
        Profile.MaximumHealth = 360.0f;
        Profile.AICoverQuality = 0.45f;
        Profile.WeaponBraceQuality = 0.45f;
        Profile.StrategicDefenseValue = 0.9f;
        Profile.MeshScale = FVector(0.55f, 2.6f, 0.75f);
        break;
    case EBHFortificationPlan::FieldRallyPoint:
        Profile.ConstructionSupplyCost = 32.0f;
        Profile.ConstructionWorkDuration = 12.0f;
        Profile.MaximumHealth = 600.0f;
        Profile.AICoverQuality = 0.25f;
        Profile.WeaponBraceQuality = 0.2f;
        Profile.StrategicDefenseValue = 2.2f;
        Profile.MeshScale = FVector(0.52f, 2.3f, 0.75f);
        break;
    case EBHFortificationPlan::HastyBarricade:
        Profile.StrategicDefenseValue = 1.6f;
        break;
    default:
        break;
    }
    return Profile;
}

bool ABHFieldFortification::IsRallyDeploymentSafe(
    bool bSectorFriendly,
    EBHFortificationPlan Plan,
    float HealthFraction,
    int32 HostileWavePressure,
    float DistanceToHostileFront,
    float RallyDistanceToFriendly
)
{
    if (!bSectorFriendly || Plan != EBHFortificationPlan::FieldRallyPoint)
    {
        return false;
    }
    if (HealthFraction < 0.5f)
    {
        return false;
    }
    return HostileWavePressure <= 2 && DistanceToHostileFront >= RallyDistanceToFriendly;
}

void ABHFieldFortification::OnRep_FortificationState()
{
    RefreshPresentation();
}

void ABHFieldFortification::OnRep_FortificationWork()
{
    RefreshPresentation();
}

void ABHFieldFortification::RefreshPresentation()
{
    bCoverEnabled = bConstructed && CurrentHealth > KINDA_SMALL_NUMBER;
    if (IsValid(BarricadeMesh))
    {
        BarricadeMesh->SetVisibility(bConstructed, true);
        BarricadeMesh->SetCollisionEnabled(
            bConstructed ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::QueryOnly
        );
        BarricadeMesh->SetCollisionResponseToAllChannels(
            bConstructed ? ECR_Block : ECR_Ignore
        );
        BarricadeMesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
        BarricadeMesh->SetRelativeScale3D(ActivePlanProfile.MeshScale);
    }
    if (IsValid(StatusLabel))
    {
        if (!bConstructed)
        {
            if (bDismantleWork)
            {
                StatusLabel->SetText(
                    FText::Format(
                        NSLOCTEXT(
                            "BrokenHorizon",
                            "FortificationWorldRubbleSite",
                            "FORTIFICATION RUBBLE // {0}% // {1}"
                        ),
                        FText::AsNumber(
                            FMath::RoundToInt(GetConstructionProgress() * 100.0f)
                        ),
                        FText::FromString(
                            UEnum::GetDisplayValueAsText(SelectedPlan).ToString()
                        )
                    )
                );
            }
            else
            {
                StatusLabel->SetText(
                    FText::Format(
                        NSLOCTEXT(
                            "BrokenHorizon",
                            "FortificationWorldBuildSite",
                            "FORTIFICATION BUILD SITE // {0}% // {1}"
                        ),
                        FText::AsNumber(
                            FMath::RoundToInt(GetConstructionProgress() * 100.0f)
                        ),
                        FText::FromString(
                            UEnum::GetDisplayValueAsText(SelectedPlan).ToString()
                        )
                    )
                );
            }
        }
        else
        {
            const FText SupportLine = SelectedPlan == EBHFortificationPlan::FieldSupplyCache
                ? FText::Format(
                    NSLOCTEXT(
                        "BrokenHorizon",
                        "FortificationWorldSupplyCacheLine",
                        "SUPPLY CACHE // {0}/{1}"
                    ),
                    FText::AsNumber(SupplyCacheChargesRemaining),
                    FText::AsNumber(MaxSupplyCacheCharges)
                )
                : SelectedPlan == EBHFortificationPlan::FieldRallyPoint
                    ? FText::Format(
                        NSLOCTEXT(
                            "BrokenHorizon",
                            "FortificationWorldRallyLine",
                            "RALLY DEPLOY // {0}/{1}"
                        ),
                        FText::AsNumber(RallyDeploymentsRemaining),
                        FText::AsNumber(MaxRallyDeployments)
                    )
                    : FText::GetEmpty();

            StatusLabel->SetText(
                FText::Format(
                    NSLOCTEXT(
                        "BrokenHorizon",
                        "FortificationWorldReady",
                        "FIELD FORTIFICATION // {0}% // {1} // {2}"
                    ),
                    FText::AsNumber(FMath::RoundToInt(GetHealthFraction() * 100.0f)),
                    FText::FromString(
                        UEnum::GetDisplayValueAsText(SelectedPlan).ToString()
                    ),
                    SupportLine
                )
            );
        }
        StatusLabel->SetTextRenderColor(
            bConstructed ? FColor(75, 210, 105) : FColor(245, 174, 48)
        );
    }
}

void ABHFieldFortification::NotifyPlayer(
    ABHCharacter* Character,
    const FText& Message
) const
{
    if (IsValid(Character))
    {
        Character->ShowStatusNotification(Message);
    }
}

void ABHFieldFortification::ReconcileWorkProgress(float DeltaSeconds)
{
    if (!HasAuthority())
    {
        return;
    }
    const bool bRepairInProgress =
        bConstructed && WorkProgress < 1.0f;
    const bool bBuildOrRebuildInProgress = !bConstructed && WorkProgress < 1.0f;
    if (!(bRepairInProgress || bBuildOrRebuildInProgress))
    {
        return;
    }
    if (ActiveWorkerCount <= 0)
    {
        return;
    }
    if (SectorID.IsNone())
    {
        return;
    }

    UWorld* World = GetWorld();
    UGameInstance* GameInstance = IsValid(World)
        ? World->GetGameInstance()
        : nullptr;
    UBHWarSubsystem* WarSubsystem = IsValid(GameInstance)
        ? GameInstance->GetSubsystem<UBHWarSubsystem>()
        : nullptr;
    if (!IsValid(WarSubsystem))
    {
        return;
    }

        const FBHFortificationPlanProfile PlanProfile = GetActivePlanProfile();
        const float AssistedRate = CalculateAssistedWorkRate(
            ActiveWorkerCount,
            WorkProgress,
            1.0f / FMath::Max(0.25f, PlanProfile.ConstructionWorkDuration)
    );
    WorkAccumulator += DeltaSeconds * AssistedRate;

    if (WorkAccumulator >= ConstructionUpdateIntervalSeconds)
    {
        const float Increment = CalculateWorkProgress(
            WorkAccumulator,
            1.0f
        );
        const float ProposedProgress = FMath::Clamp(
            WorkProgress + Increment,
            0.0f,
            1.0f
        );

        const float RequiredWorkSupply =
            bRepairInProgress
            ? FMath::Max(
                0.0f,
                CalculateRepairSupplyCost(GetHealthFraction(), FullRepairSupplyCost) -
                    CalculateRepairSupplyCost(
                        ProposedProgress,
                        FullRepairSupplyCost
                    )
            )
            : (ProposedProgress - WorkProgress) *
                PlanProfile.ConstructionSupplyCost *
                (bDismantleWork ? 1.25f : 1.0f);

        if (RequiredWorkSupply > KINDA_SMALL_NUMBER &&
            !WarSubsystem->ConsumeSectorSupply(SectorID, RequiredWorkSupply))
        {
            ActiveWorkerCount = 0;
            WorkAccumulator = 0.0f;
            return;
        }

        WorkProgress = ProposedProgress;
        CurrentHealth = GetCurrentPlanHealth() * FMath::Clamp(WorkProgress, 0.0f, 1.0f);
        WorkAccumulator = 0.0f;
        if (WorkProgress >= 1.0f)
        {
            bConstructed = true;
            bDismantleWork = false;
            CurrentHealth = PlanProfile.MaximumHealth;
            ActiveWorkerCount = 0;
            if (SelectedPlan == EBHFortificationPlan::FieldSupplyCache)
            {
                SupplyCacheChargesRemaining = MaxSupplyCacheCharges;
            }
            else if (SelectedPlan == EBHFortificationPlan::FieldRallyPoint)
            {
                RallyDeploymentsRemaining = MaxRallyDeployments;
            }
        }
        RefreshPresentation();
        ForceNetUpdate();
    }
}

FBHFortificationPlanProfile ABHFieldFortification::GetActivePlanProfile() const
{
    return ActivePlanProfile;
}

float ABHFieldFortification::GetCurrentPlanProfileDefense() const
{
    const float HealthRatio = FMath::Clamp(GetHealthFraction(), 0.0f, 1.0f);
    return FMath::Max(
        0.0f,
        ActivePlanProfile.StrategicDefenseValue * HealthRatio
    );
}

float ABHFieldFortification::GetCurrentPlanHealth() const
{
    return FMath::Max(1.0f, ActivePlanProfile.MaximumHealth);
}

void ABHFieldFortification::ApplyPlanProfile(
    const FBHFortificationPlanProfile& PlanProfile
)
{
    ConstructionSupplyCost = PlanProfile.ConstructionSupplyCost;
    FullRepairSupplyCost = FMath::Max(
        1.0f,
        PlanProfile.ConstructionSupplyCost * 0.45f
    );
    MaximumHealth = PlanProfile.MaximumHealth;
    if (!bConstructed && CurrentHealth <= 0.0f)
    {
        CurrentHealth = MaximumHealth * FMath::Clamp(WorkProgress, 0.0f, 1.0f);
    }
}
