#include "BHFieldFortification.h"

#include "BHCharacter.h"
#include "BHSaveSubsystem.h"
#include "BHWarSubsystem.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "EngineUtils.h"
#include "Engine/GameInstance.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

ABHFieldFortification::ABHFieldFortification()
{
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
}

void ABHFieldFortification::BeginPlay()
{
    Super::BeginPlay();

    if (HasAuthority() && !SectorID.IsNone())
    {
        UWorld* World = GetWorld();
        UBHWarSubsystem* WarSubsystem =
            IsValid(World) && IsValid(World->GetGameInstance())
                ? World->GetGameInstance()->GetSubsystem<UBHWarSubsystem>()
                : nullptr;
        if (IsValid(WarSubsystem))
        {
            WarSubsystem->SynchronizeFortificationStateWithWorld();
        }
    }

    RefreshPresentation();
}

void ABHFieldFortification::GetLifetimeReplicatedProps(
    TArray<FLifetimeProperty>& OutLifetimeProps
) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ABHFieldFortification, bConstructed);
    DOREPLIFETIME(ABHFieldFortification, CurrentHealth);
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

    CurrentHealth = FMath::Max(0.0f, CurrentHealth - DamageAmount);
    if (CurrentHealth <= KINDA_SMALL_NUMBER)
    {
        bConstructed = false;
        UWorld* World = GetWorld();
        UBHWarSubsystem* WarSubsystem =
            IsValid(World) && IsValid(World->GetGameInstance())
                ? World->GetGameInstance()->GetSubsystem<UBHWarSubsystem>()
                : nullptr;
        if (IsValid(WarSubsystem))
        {
            WarSubsystem->SynchronizeFortificationStateWithWorld();
        }
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

    const float SupplyCost = bConstructed
        ? CalculateRepairSupplyCost(GetHealthFraction(), FullRepairSupplyCost)
        : ConstructionSupplyCost;
    if (!bConstructed)
    {
        if (!WarSubsystem->CanPlaceAdditionalFortification(SectorID))
        {
            NotifyPlayer(
                Character,
                FText::Format(
                    NSLOCTEXT(
                        "BrokenHorizon",
                        "FortificationLimitReached",
                        "BUILD LIMIT REACHED\n\nFortifications: {0}/{1}"
                    ),
                    FText::AsNumber(
                        WarSubsystem->GetSectorConstructedFortificationCount(
                            SectorID
                        )
                    ),
                    FText::AsNumber(
                        WarSubsystem->GetSectorFortificationCapacity(
                            SectorID
                        )
                    )
                )
            );
            return;
        }

        if (!IsPlacementValid())
        {
            NotifyPlayer(
                Character,
                NSLOCTEXT(
                    "BrokenHorizon",
                    "FortificationPlacementBlocked",
                    "PLACEMENT BLOCKED\n\nToo close to existing fortification."
                )
            );
            return;
        }
    }
    if (bConstructed && SupplyCost <= KINDA_SMALL_NUMBER)
    {
        NotifyPlayer(
            Character,
            NSLOCTEXT(
                "BrokenHorizon",
                "FortificationFullHealth",
                "FORTIFICATION READY\n\nBarricade integrity is already at 100%."
            )
        );
        return;
    }

    if (!WarSubsystem->ConsumeSectorSupply(SectorID, SupplyCost))
    {
        NotifyPlayer(
            Character,
            FText::Format(
                NSLOCTEXT(
                    "BrokenHorizon",
                    "FortificationInsufficientSupply",
                    "ENGINEERING SUPPLY REQUIRED\n\n{0}% sector supply required."
                ),
                FText::AsNumber(FMath::CeilToInt(SupplyCost))
            )
        );
        return;
    }

    const bool bWasConstructed = bConstructed;
    bConstructed = true;
    CurrentHealth = MaximumHealth;
    RefreshPresentation();
    ForceNetUpdate();
    if (IsValid(WarSubsystem))
    {
        WarSubsystem->SynchronizeFortificationStateWithWorld();
    }
    const bool bSaved = IsValid(SaveSubsystem) && SaveSubsystem->SaveProgress();
    const FBHWarSectorState UpdatedSector =
        WarSubsystem->GetSectorState(SectorID);
    const FText SaveMessage = bSaved
        ? NSLOCTEXT("BrokenHorizon", "FortificationSaved", "CHECKPOINT SAVED")
        : NSLOCTEXT("BrokenHorizon", "FortificationSaveFailed", "CHECKPOINT SAVE FAILED");
    if (bWasConstructed)
    {
        NotifyPlayer(
            Character,
            FText::Format(
                NSLOCTEXT(
                    "BrokenHorizon",
                    "FortificationRepaired",
                    "FORTIFICATION REPAIRED\n\nIntegrity 100% // sector supply {0}% // {1}"
                ),
                FText::AsNumber(FMath::RoundToInt(UpdatedSector.Supply)),
                SaveMessage
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
                    "FortificationConstructed",
                    "FORTIFICATION CONSTRUCTED\n\nAI cover active // sector supply {0}% // defense {1} // {2}"
                ),
                FText::AsNumber(FMath::RoundToInt(UpdatedSector.Supply)),
                FText::AsPercent(
                    WarSubsystem
                        ->GetSectorFortificationDefenseMultiplier(SectorID)
                ),
                SaveMessage
            )
        );
    }
    UE_LOG(
        LogTemp,
        Display,
        TEXT("BH_FORTIFICATION_%s id=%s sector=%s cost=%.1f health=%.1f"),
        bWasConstructed ? TEXT("REPAIRED") : TEXT("BUILT"),
        *PersistenceID.ToString(),
        *SectorID.ToString(),
        SupplyCost,
        CurrentHealth
    );
}

FText ABHFieldFortification::GetInteractionText_Implementation() const
{
    const UWorld* World = GetWorld();
    const UGameInstance* GameInstance = IsValid(World)
        ? World->GetGameInstance()
        : nullptr;
    const UBHWarSubsystem* WarSubsystem = IsValid(GameInstance)
        ? GameInstance->GetSubsystem<UBHWarSubsystem>()
        : nullptr;
    const bool bHasSector = !SectorID.IsNone();
    const FBHWarSectorState SectorState = IsValid(WarSubsystem) && bHasSector
        ? WarSubsystem->GetSectorState(SectorID)
        : FBHWarSectorState();
    const bool bFriendlySector =
        IsValid(WarSubsystem) && bHasSector &&
        SectorState.Owner == EBHWarFaction::Friendly;
    const int32 ConstructedCount = IsValid(WarSubsystem) && bHasSector
        ? WarSubsystem->GetSectorConstructedFortificationCount(SectorID)
        : 0;
    const int32 Capacity = IsValid(WarSubsystem) && bHasSector
        ? WarSubsystem->GetSectorFortificationCapacity(SectorID)
        : 0;

    if (!bHasSector)
    {
        return NSLOCTEXT(
            "BrokenHorizon",
            "FortificationUnassigned",
            "Field Barricade // Sector not assigned"
        );
    }

    if (!WarSubsystem)
    {
        return NSLOCTEXT(
            "BrokenHorizon",
            "FortificationSubsystemUnavailable",
            "Field Barricade // Offline"
        );
    }

    if (!bFriendlySector)
    {
        return NSLOCTEXT(
            "BrokenHorizon",
            "FortificationSectorLockedPrompt",
            "Field Barricade // Sector Locked"
        );
    }

    if (!bConstructed && !WarSubsystem->CanPlaceAdditionalFortification(SectorID))
    {
        return FText::Format(
            NSLOCTEXT(
                "BrokenHorizon",
                "BuildFortificationLockedPrompt",
                "[F] Build Field Barricade // LIMIT REACHED // {0}/{1}"
            ),
            FText::AsNumber(ConstructedCount),
            FText::AsNumber(Capacity)
        );
    }

    if (!bConstructed && !IsPlacementValid())
    {
        return NSLOCTEXT(
            "BrokenHorizon",
            "BuildFortificationBlockedPrompt",
            "Field Barricade // Placement Blocked"
        );
    }

    if (!bConstructed && SectorState.Supply + KINDA_SMALL_NUMBER < ConstructionSupplyCost)
    {
        return FText::Format(
            NSLOCTEXT(
                "BrokenHorizon",
                "BuildFortificationSupplyPrompt",
                "[F] Build Field Barricade // {0}% Supply (Need {1}%)"
            ),
            FText::AsNumber(FMath::CeilToInt(SectorState.Supply)),
            FText::AsNumber(FMath::CeilToInt(ConstructionSupplyCost))
        );
    }

    if (!bConstructed)
    {
        return FText::Format(
            NSLOCTEXT(
                "BrokenHorizon",
                "BuildFortificationPrompt",
                "[F] Build Field Barricade // {0}% Supply // {1}/{2}"
            ),
            FText::AsNumber(FMath::CeilToInt(ConstructionSupplyCost)),
            FText::AsNumber(ConstructedCount),
            FText::AsNumber(Capacity)
        );
    }
    const float RepairCost = CalculateRepairSupplyCost(
        GetHealthFraction(),
        FullRepairSupplyCost
    );
    return RepairCost > KINDA_SMALL_NUMBER
        ? FText::Format(
            NSLOCTEXT(
                "BrokenHorizon",
                "RepairFortificationPrompt",
                "[F] Repair Barricade // {0}% Supply"
            ),
            FText::AsNumber(FMath::CeilToInt(RepairCost))
        )
        : NSLOCTEXT(
            "BrokenHorizon",
            "FortificationReadyPrompt",
            "Field Barricade // Ready"
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
    return MaximumHealth > KINDA_SMALL_NUMBER
        ? FMath::Clamp(CurrentHealth / MaximumHealth, 0.0f, 1.0f)
        : 0.0f;
}

void ABHFieldFortification::RestorePersistentState(
    const FTransform& SavedTransform,
    bool bSavedConstructed,
    float SavedHealthFraction
)
{
    if (!HasAuthority())
    {
        return;
    }
    SetActorTransform(SavedTransform, false, nullptr, ETeleportType::TeleportPhysics);
    bConstructed = bSavedConstructed;
    CurrentHealth = bConstructed
        ? FMath::Clamp(SavedHealthFraction, 0.01f, 1.0f) * MaximumHealth
        : 0.0f;
    UWorld* World = GetWorld();
    UBHWarSubsystem* WarSubsystem =
        IsValid(World) && IsValid(World->GetGameInstance())
            ? World->GetGameInstance()->GetSubsystem<UBHWarSubsystem>()
            : nullptr;
    if (IsValid(WarSubsystem))
    {
        WarSubsystem->SynchronizeFortificationStateWithWorld();
    }
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

bool ABHFieldFortification::IsPlacementValid() const
{
    const UWorld* World = GetWorld();
    if (!IsValid(World))
    {
        return false;
    }

    for (TActorIterator<ABHFieldFortification> It(World); It; ++It)
    {
        const ABHFieldFortification* ExistingFortification = *It;
        if (!IsValid(ExistingFortification) ||
            ExistingFortification == this ||
            ExistingFortification->GetSectorID() != SectorID ||
            !ExistingFortification->IsConstructed() ||
            ExistingFortification->GetPersistenceID().IsNone())
        {
            continue;
        }

        if ((ExistingFortification->GetActorLocation() -
            GetActorLocation()).Size() <
            MinimumPeerSpacingCentimeters)
        {
            return false;
        }
    }
    return true;
}

float ABHFieldFortification::CalculateRepairSupplyCost(
    float HealthFraction,
    float InFullRepairSupplyCost
)
{
    return FMath::Max(0.0f, InFullRepairSupplyCost) *
        (1.0f - FMath::Clamp(HealthFraction, 0.0f, 1.0f));
}

void ABHFieldFortification::OnRep_FortificationState()
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
            bConstructed
                ? ECollisionEnabled::QueryAndPhysics
                : ECollisionEnabled::QueryOnly
        );
        BarricadeMesh->SetCollisionResponseToAllChannels(
            bConstructed ? ECR_Block : ECR_Ignore
        );
        BarricadeMesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
    }
    if (IsValid(StatusLabel))
    {
        StatusLabel->SetText(
            bConstructed
                ? FText::Format(
                    NSLOCTEXT(
                        "BrokenHorizon",
                        "FortificationWorldReady",
                        "FIELD BARRICADE // {0}%"
                    ),
                    FText::AsNumber(FMath::RoundToInt(GetHealthFraction() * 100.0f))
                )
                : NSLOCTEXT(
                    "BrokenHorizon",
                    "FortificationWorldBuildSite",
                    "FORTIFICATION BUILD POSITION"
                )
        );
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
