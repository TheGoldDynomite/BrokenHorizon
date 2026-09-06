#include "BHInjuryComponent.h"

#include "BHHealthComponent.h"
#include "BHCharacter.h"
#include "BHWarSubsystem.h"
#include "Components/CapsuleComponent.h"
#include "Engine/GameInstance.h"
#include "GameFramework/Character.h"
#include "GameFramework/Pawn.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Net/UnrealNetwork.h"

UBHInjuryComponent::UBHInjuryComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    SetIsReplicatedByDefault(true);
}

void UBHInjuryComponent::GetLifetimeReplicatedProps(
    TArray<FLifetimeProperty>& OutLifetimeProps
) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

#define BH_REPLICATE_OWNER_MEDICAL(PropertyName) \
    DOREPLIFETIME_CONDITION_NOTIFY( \
        UBHInjuryComponent, \
        PropertyName, \
        COND_OwnerOnly, \
        REPNOTIFY_Always \
    )

    BH_REPLICATE_OWNER_MEDICAL(CurrentBleedRate);
    BH_REPLICATE_OWNER_MEDICAL(CurrentHelmetDurability);
    BH_REPLICATE_OWNER_MEDICAL(CurrentBodyArmorDurability);
    BH_REPLICATE_OWNER_MEDICAL(MedkitTreatmentElapsed);
    BH_REPLICATE_OWNER_MEDICAL(FieldDressingCount);
    BH_REPLICATE_OWNER_MEDICAL(MedkitCount);
    BH_REPLICATE_OWNER_MEDICAL(bBleeding);
    BH_REPLICATE_OWNER_MEDICAL(bArmInjured);
    BH_REPLICATE_OWNER_MEDICAL(bLegInjured);
    BH_REPLICATE_OWNER_MEDICAL(bMedkitTreatmentActive);

#undef BH_REPLICATE_OWNER_MEDICAL
}

void UBHInjuryComponent::BeginPlay()
{
    Super::BeginPlay();

    HealthComponent = GetOwner()
        ? GetOwner()->FindComponentByClass<UBHHealthComponent>()
        : nullptr;

    ResetInjuries();
}

void UBHInjuryComponent::TickComponent(
    float DeltaTime,
    ELevelTick TickType,
    FActorComponentTickFunction* ThisTickFunction
)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (!IsValid(HealthComponent))
    {
        return;
    }

    if (HealthComponent->IsDead())
    {
        CancelMedkitTreatment();
        return;
    }

    if (const ABHCharacter* CharacterOwner =
            Cast<ABHCharacter>(GetOwner());
        IsValid(CharacterOwner) &&
        CharacterOwner->IsPlayerIncapacitated())
    {
        CancelMedkitTreatment();
        return;
    }

    const float SafeDeltaTime = FMath::Max(0.0f, DeltaTime);

    if (bBleeding && CurrentBleedRate > 0.0f)
    {
        const float SafeInterval = FMath::Max(
            0.05f,
            BleedTickInterval
        );
        BleedTickAccumulator += SafeDeltaTime;

        while (BleedTickAccumulator >= SafeInterval &&
            !HealthComponent->IsDead())
        {
            BleedTickAccumulator -= SafeInterval;
            HealthComponent->ApplyOngoingDamage(
                CurrentBleedRate * SafeInterval,
                LastDamageCauser.Get()
            );
        }
    }

    if (bMedkitTreatmentActive && !HealthComponent->IsDead())
    {
        MedkitTreatmentElapsed += SafeDeltaTime;

        if (MedkitTreatmentElapsed >=
            GetEffectiveMedkitTreatmentDuration())
        {
            CompleteMedkitTreatment();
        }
        else
        {
            BroadcastMedicalState();
        }
    }
}

EBHPlayerHitZone UBHInjuryComponent::ResolveHitZone(
    const FHitResult& HitResult
) const
{
    const FString BoneName =
        HitResult.BoneName.ToString().ToLower();

    if (!BoneName.IsEmpty())
    {
        if (BoneName.Contains(TEXT("head")) ||
            BoneName.Contains(TEXT("neck")))
        {
            return EBHPlayerHitZone::Head;
        }

        if (BoneName.Contains(TEXT("clavicle")) ||
            BoneName.Contains(TEXT("shoulder")) ||
            BoneName.Contains(TEXT("upperarm")) ||
            BoneName.Contains(TEXT("lowerarm")) ||
            BoneName.Contains(TEXT("forearm")) ||
            BoneName.Contains(TEXT("hand")))
        {
            return EBHPlayerHitZone::Arm;
        }

        if (BoneName.Contains(TEXT("thigh")) ||
            BoneName.Contains(TEXT("calf")) ||
            BoneName.Contains(TEXT("shin")) ||
            BoneName.Contains(TEXT("leg")) ||
            BoneName.Contains(TEXT("foot")))
        {
            return EBHPlayerHitZone::Leg;
        }
    }

    const ACharacter* CharacterOwner =
        Cast<ACharacter>(GetOwner());
    const UCapsuleComponent* CollisionCapsule =
        IsValid(CharacterOwner)
            ? CharacterOwner->GetCapsuleComponent()
            : nullptr;

    if (!IsValid(CharacterOwner) || !IsValid(CollisionCapsule))
    {
        return EBHPlayerHitZone::Torso;
    }

    const FVector LocalImpact =
        CharacterOwner->GetActorTransform().InverseTransformPosition(
            HitResult.ImpactPoint
        );
    const float HalfHeight = FMath::Max(
        1.0f,
        CollisionCapsule->GetScaledCapsuleHalfHeight()
    );
    const float CapsuleRadius = FMath::Max(
        1.0f,
        CollisionCapsule->GetScaledCapsuleRadius()
    );
    const float NormalizedHeight = LocalImpact.Z / HalfHeight;

    if (NormalizedHeight >= 0.58f)
    {
        return EBHPlayerHitZone::Head;
    }

    if (NormalizedHeight <= -0.20f)
    {
        return EBHPlayerHitZone::Leg;
    }

    if (NormalizedHeight >= -0.05f &&
        FMath::Abs(LocalImpact.Y) >= (CapsuleRadius * 0.72f))
    {
        return EBHPlayerHitZone::Arm;
    }

    return EBHPlayerHitZone::Torso;
}

float UBHInjuryComponent::CalculateDamageForHit(
    const FHitResult& HitResult,
    float RawDamage,
    EBHPlayerHitZone& OutHitZone
)
{
    OutHitZone = ResolveHitZone(HitResult);
    float DamageMultiplier = 1.0f;
    bool bArmorWasUsed = false;

    switch (OutHitZone)
    {
    case EBHPlayerHitZone::Head:
        DamageMultiplier = HeadDamageMultiplier;
        break;

    case EBHPlayerHitZone::Arm:
        DamageMultiplier = ArmDamageMultiplier;
        break;

    case EBHPlayerHitZone::Leg:
        DamageMultiplier = LegDamageMultiplier;
        break;

    case EBHPlayerHitZone::Torso:
    default:
        DamageMultiplier = TorsoDamageMultiplier;
        break;
    }

    float DamageAfterProtection =
        FMath::Max(0.0f, RawDamage) *
        FMath::Max(0.0f, DamageMultiplier);

    if (OutHitZone == EBHPlayerHitZone::Head &&
        bHasHelmet &&
        CurrentHelmetDurability > 0.0f)
    {
        DamageAfterProtection = ApplyArmorProtection(
            DamageAfterProtection,
            HelmetDamageScale,
            CurrentHelmetDurability
        );
        bArmorWasUsed = true;
    }
    else if (OutHitZone == EBHPlayerHitZone::Torso &&
        bHasBodyArmor &&
        CurrentBodyArmorDurability > 0.0f)
    {
        DamageAfterProtection = ApplyArmorProtection(
            DamageAfterProtection,
            BodyArmorDamageScale,
            CurrentBodyArmorDurability
        );
        bArmorWasUsed = true;
    }

    if (bArmorWasUsed)
    {
        BroadcastMedicalState();
    }

    return DamageAfterProtection;
}

void UBHInjuryComponent::RegisterBallisticHit(
    EBHPlayerHitZone HitZone,
    float DamageApplied,
    AActor* DamageCauser
)
{
    if (DamageApplied <= 0.0f ||
        !IsValid(HealthComponent) ||
        HealthComponent->IsDead())
    {
        return;
    }

    LastDamageCauser = DamageCauser;
    CancelMedkitTreatment();

    if (HitZone == EBHPlayerHitZone::Arm)
    {
        bArmInjured = true;
    }
    else if (HitZone == EBHPlayerHitZone::Leg)
    {
        bLegInjured = true;
    }

    if (DamageApplied >= MinimumBleedingHitDamage)
    {
        CurrentBleedRate = FMath::Max(
            CurrentBleedRate,
            GetBleedRateForZone(HitZone)
        );
        bBleeding = CurrentBleedRate > 0.0f;
    }

    BroadcastInjuryState();
}

bool UBHInjuryComponent::UseFieldDressing()
{
    if (!bBleeding ||
        FieldDressingCount <= 0 ||
        bMedkitTreatmentActive)
    {
        return false;
    }

    --FieldDressingCount;
    bBleeding = false;
    CurrentBleedRate = 0.0f;
    BleedTickAccumulator = 0.0f;
    BroadcastInjuryState();
    return true;
}

bool UBHInjuryComponent::ConsumeFieldDressingForSquadAid()
{
    if (FieldDressingCount <= 0 ||
        bMedkitTreatmentActive)
    {
        return false;
    }

    --FieldDressingCount;
    BroadcastInjuryState();
    BroadcastMedicalState();
    return true;
}

bool UBHInjuryComponent::StartMedkitTreatment()
{
    if (bMedkitTreatmentActive ||
        bBleeding ||
        MedkitCount <= 0 ||
        !IsValid(HealthComponent) ||
        HealthComponent->IsDead() ||
        HealthComponent->IsFullHealth())
    {
        return false;
    }

    bMedkitTreatmentActive = true;
    MedkitTreatmentElapsed = 0.0f;
    BroadcastMedicalState();
    return true;
}

void UBHInjuryComponent::CancelMedkitTreatment()
{
    if (!bMedkitTreatmentActive)
    {
        return;
    }

    bMedkitTreatmentActive = false;
    MedkitTreatmentElapsed = 0.0f;
    BroadcastMedicalState();
}

void UBHInjuryComponent::AddMedicalSupplies(
    int32 MedkitsToAdd,
    int32 FieldDressingsToAdd
)
{
    MedkitCount = FMath::Max(
        0,
        MedkitCount + MedkitsToAdd
    );
    FieldDressingCount = FMath::Max(
        0,
        FieldDressingCount + FieldDressingsToAdd
    );
    BroadcastInjuryState();
    BroadcastMedicalState();
}

bool UBHInjuryComponent::RepairArmor(
    float HelmetDurabilityToRestore,
    float BodyArmorDurabilityToRestore
)
{
    const float PreviousHelmetDurability =
        CurrentHelmetDurability;
    const float PreviousBodyArmorDurability =
        CurrentBodyArmorDurability;

    if (bHasHelmet && HelmetDurabilityToRestore > 0.0f)
    {
        CurrentHelmetDurability = FMath::Clamp(
            CurrentHelmetDurability + HelmetDurabilityToRestore,
            0.0f,
            FMath::Max(0.0f, MaximumHelmetDurability)
        );
    }

    if (bHasBodyArmor && BodyArmorDurabilityToRestore > 0.0f)
    {
        CurrentBodyArmorDurability = FMath::Clamp(
            CurrentBodyArmorDurability +
                BodyArmorDurabilityToRestore,
            0.0f,
            FMath::Max(0.0f, MaximumBodyArmorDurability)
        );
    }

    const bool bArmorRepaired =
        !FMath::IsNearlyEqual(
            PreviousHelmetDurability,
            CurrentHelmetDurability
        ) ||
        !FMath::IsNearlyEqual(
            PreviousBodyArmorDurability,
            CurrentBodyArmorDurability
        );

    if (bArmorRepaired)
    {
        BroadcastMedicalState();
    }

    return bArmorRepaired;
}

void UBHInjuryComponent::RestorePersistentSupplyState(
    int32 SavedMedkits,
    int32 SavedFieldDressings,
    float SavedHelmetDurability,
    float SavedBodyArmorDurability
)
{
    CancelMedkitTreatment();

    MedkitCount = FMath::Max(0, SavedMedkits);
    FieldDressingCount = FMath::Max(0, SavedFieldDressings);
    CurrentHelmetDurability = FMath::Clamp(
        SavedHelmetDurability,
        0.0f,
        FMath::Max(0.0f, MaximumHelmetDurability)
    );
    CurrentBodyArmorDurability = FMath::Clamp(
        SavedBodyArmorDurability,
        0.0f,
        FMath::Max(0.0f, MaximumBodyArmorDurability)
    );

    BroadcastInjuryState();
    BroadcastMedicalState();
}

void UBHInjuryComponent::RestorePersistentInjuryState(
    bool bSavedBleeding,
    float SavedBleedRate,
    bool bSavedArmInjured,
    bool bSavedLegInjured
)
{
    CancelMedkitTreatment();

    CurrentBleedRate = FMath::Max(0.0f, SavedBleedRate);
    bBleeding =
        bSavedBleeding &&
        CurrentBleedRate > KINDA_SMALL_NUMBER;
    bArmInjured = bSavedArmInjured;
    bLegInjured = bSavedLegInjured;
    BleedTickAccumulator = 0.0f;
    LastDamageCauser.Reset();

    BroadcastInjuryState();
    BroadcastMedicalState();
}

void UBHInjuryComponent::ResetInjuries()
{
    bBleeding = false;
    bArmInjured = false;
    bLegInjured = false;
    CurrentBleedRate = 0.0f;
    BleedTickAccumulator = 0.0f;
    CurrentHelmetDurability = FMath::Max(
        0.0f,
        MaximumHelmetDurability
    );
    CurrentBodyArmorDurability = FMath::Max(
        0.0f,
        MaximumBodyArmorDurability
    );
    MedkitTreatmentElapsed = 0.0f;
    FieldDressingCount = FMath::Max(0, StartingFieldDressings);
    MedkitCount = FMath::Max(0, StartingMedkits);
    bMedkitTreatmentActive = false;
    LastDamageCauser.Reset();
    BroadcastInjuryState();
    BroadcastMedicalState();
}

bool UBHInjuryComponent::IsBleeding() const
{
    return bBleeding;
}

float UBHInjuryComponent::GetBleedRate() const
{
    return CurrentBleedRate;
}

bool UBHInjuryComponent::IsArmInjured() const
{
    return bArmInjured;
}

bool UBHInjuryComponent::IsLegInjured() const
{
    return bLegInjured;
}

int32 UBHInjuryComponent::GetFieldDressingCount() const
{
    return FieldDressingCount;
}

int32 UBHInjuryComponent::GetMedkitCount() const
{
    return MedkitCount;
}

float UBHInjuryComponent::GetHelmetDurabilityPercentage() const
{
    return MaximumHelmetDurability > 0.0f
        ? FMath::Clamp(
            CurrentHelmetDurability / MaximumHelmetDurability,
            0.0f,
            1.0f
        )
        : 0.0f;
}

float UBHInjuryComponent::GetBodyArmorDurabilityPercentage() const
{
    return MaximumBodyArmorDurability > 0.0f
        ? FMath::Clamp(
            CurrentBodyArmorDurability /
                MaximumBodyArmorDurability,
            0.0f,
            1.0f
        )
        : 0.0f;
}

float UBHInjuryComponent::GetHelmetDurability() const
{
    return CurrentHelmetDurability;
}

float UBHInjuryComponent::GetBodyArmorDurability() const
{
    return CurrentBodyArmorDurability;
}

bool UBHInjuryComponent::IsMedkitTreatmentActive() const
{
    return bMedkitTreatmentActive;
}

float UBHInjuryComponent::GetMedkitTreatmentProgress() const
{
    if (!bMedkitTreatmentActive)
    {
        return 0.0f;
    }

    return FMath::Clamp(
        MedkitTreatmentElapsed /
            GetEffectiveMedkitTreatmentDuration(),
        0.0f,
        1.0f
    );
}

bool UBHInjuryComponent::ConsumeMedkitForSquadAid()
{
    if (MedkitCount <= 0 || bMedkitTreatmentActive)
    {
        return false;
    }
    --MedkitCount;
    BroadcastMedicalState();
    return true;
}

void UBHInjuryComponent::ClearBleedingForSquadAid()
{
    bBleeding = false;
    CurrentBleedRate = 0.0f;
    BleedTickAccumulator = 0.0f;
    CancelMedkitTreatment();
    BroadcastInjuryState();
}

float UBHInjuryComponent::GetEffectiveMedkitTreatmentDuration() const
{
    const AActor* Owner = GetOwner();
    const UGameInstance* GameInstance = Owner
        ? Owner->GetGameInstance()
        : nullptr;
    const UBHWarSubsystem* WarSubsystem = GameInstance
        ? GameInstance->GetSubsystem<UBHWarSubsystem>()
        : nullptr;
    const float MedicalPressure = IsValid(WarSubsystem)
        ? FMath::Clamp(
            WarSubsystem->GetCampaignDifficulty()
                .MedicalPressureMultiplier,
            0.5f,
            2.0f
        )
        : 1.0f;
    const float RecoveryNetworkMultiplier =
        IsValid(WarSubsystem) &&
            WarSubsystem->HasCampaignCapability(
                EBHCampaignCapability::CasualtyRecoveryNetwork)
        ? 0.85f
        : 1.0f;
    return FMath::Max(
        0.1f,
        MedkitTreatmentDuration * MedicalPressure *
            RecoveryNetworkMultiplier
    );
}

float UBHInjuryComponent::GetWeaponSpreadMultiplier() const
{
    return bArmInjured
        ? FMath::Max(1.0f, ArmInjurySpreadMultiplier)
        : 1.0f;
}

float UBHInjuryComponent::GetMovementSpeedMultiplier() const
{
    return bLegInjured
        ? FMath::Clamp(
            LegInjuryMovementMultiplier,
            0.1f,
            1.0f
        )
        : 1.0f;
}

float UBHInjuryComponent::GetWeaponSwayDegrees() const
{
    return bArmInjured
        ? FMath::Max(0.0f, ArmInjuryWeaponSwayDegrees)
        : 0.0f;
}

void UBHInjuryComponent::BroadcastInjuryState()
{
    OnInjuryStateChanged.Broadcast(
        bBleeding,
        CurrentBleedRate,
        bArmInjured,
        bLegInjured,
        FieldDressingCount
    );
}

void UBHInjuryComponent::BroadcastMedicalState()
{
    OnMedicalStateChanged.Broadcast(
        MedkitCount,
        GetHelmetDurabilityPercentage(),
        GetBodyArmorDurabilityPercentage(),
        bMedkitTreatmentActive,
        GetMedkitTreatmentProgress()
    );
}

void UBHInjuryComponent::OnRep_InjuryMedicalState()
{
    CurrentBleedRate = FMath::Max(0.0f, CurrentBleedRate);
    CurrentHelmetDurability = FMath::Clamp(
        CurrentHelmetDurability,
        0.0f,
        FMath::Max(0.0f, MaximumHelmetDurability)
    );
    CurrentBodyArmorDurability = FMath::Clamp(
        CurrentBodyArmorDurability,
        0.0f,
        FMath::Max(0.0f, MaximumBodyArmorDurability)
    );
    MedkitTreatmentElapsed = FMath::Max(
        0.0f,
        MedkitTreatmentElapsed
    );
    FieldDressingCount = FMath::Max(0, FieldDressingCount);
    MedkitCount = FMath::Max(0, MedkitCount);
    BroadcastInjuryState();
    BroadcastMedicalState();

#if !UE_BUILD_SHIPPING
    const APawn* OwnerPawn = Cast<APawn>(GetOwner());
    if (FParse::Param(
            FCommandLine::Get(),
            TEXT("BHTestMedicalRecoveryReplication")) &&
        IsValid(OwnerPawn) &&
        OwnerPawn->IsLocallyControlled())
    {
        UE_LOG(
            LogTemp,
            Display,
            TEXT(
                "BH_MEDICAL_STATE_REPLICATED character=%s local=1 "
                "medkits=%d dressings=%d helmet=%.2f body=%.2f "
                "bleeding=%d arm=%d leg=%d treatment=%d progress=%.2f"
            ),
            *GetNameSafe(GetOwner()),
            MedkitCount,
            FieldDressingCount,
            GetHelmetDurabilityPercentage(),
            GetBodyArmorDurabilityPercentage(),
            bBleeding ? 1 : 0,
            bArmInjured ? 1 : 0,
            bLegInjured ? 1 : 0,
            bMedkitTreatmentActive ? 1 : 0,
            GetMedkitTreatmentProgress()
        );
    }
#endif
}

void UBHInjuryComponent::CompleteMedkitTreatment()
{
    if (!bMedkitTreatmentActive ||
        MedkitCount <= 0 ||
        !IsValid(HealthComponent) ||
        HealthComponent->IsDead())
    {
        CancelMedkitTreatment();
        return;
    }

    --MedkitCount;
    HealthComponent->Heal(FMath::Max(0.0f, MedkitHealingAmount));

    if (bMedkitTreatsLimbInjuries)
    {
        bArmInjured = false;
        bLegInjured = false;
    }

    bMedkitTreatmentActive = false;
    MedkitTreatmentElapsed = 0.0f;
    BroadcastInjuryState();
    BroadcastMedicalState();
    OnTreatmentCompleted.Broadcast();
}

float UBHInjuryComponent::ApplyArmorProtection(
    float UnarmoredDamage,
    float DamageScale,
    float& CurrentDurability
)
{
    const float SafeDamage = FMath::Max(0.0f, UnarmoredDamage);
    const float DesiredAbsorption =
        SafeDamage *
        (1.0f - FMath::Clamp(DamageScale, 0.0f, 1.0f));
    const float ActualAbsorption = FMath::Min(
        FMath::Max(0.0f, CurrentDurability),
        DesiredAbsorption
    );

    CurrentDurability = FMath::Max(
        0.0f,
        CurrentDurability - ActualAbsorption
    );

    return FMath::Max(0.0f, SafeDamage - ActualAbsorption);
}

float UBHInjuryComponent::GetBleedRateForZone(
    EBHPlayerHitZone HitZone
) const
{
    switch (HitZone)
    {
    case EBHPlayerHitZone::Head:
        return FMath::Max(0.0f, HeadBleedRate);

    case EBHPlayerHitZone::Arm:
        return FMath::Max(0.0f, ArmBleedRate);

    case EBHPlayerHitZone::Leg:
        return FMath::Max(0.0f, LegBleedRate);

    case EBHPlayerHitZone::Torso:
    default:
        return FMath::Max(0.0f, TorsoBleedRate);
    }
}
