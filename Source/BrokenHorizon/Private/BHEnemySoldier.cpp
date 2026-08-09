#include "BHEnemySoldier.h"
#include "BHBattlefieldConditions.h"
#include "BHRifle.h"

#include "BHCharacter.h"
#include "BHAmmoSupply.h"
#include "BHEnemyAIController.h"
#include "BHFragGrenade.h"
#include "BHHealthComponent.h"
#include "NavigationInvokerComponent.h"
#include "BHInjuryComponent.h"
#include "BHImpactEffect.h"
#include "BHPatrolPoint.h"
#include "BHPlayerResolver.h"
#include "BHSaveSubsystem.h"
#include "BHSupplyConvoyTarget.h"
#include "BHWarOperationRules.h"
#include "BHWarSubsystem.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimSequenceBase.h"
#include "Animation/AnimMontage.h"
#include "Components/CapsuleComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/DamageEvents.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/DamageType.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Net/UnrealNetwork.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "Sound/SoundBase.h"
#include "TimerManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
constexpr ECollisionChannel EnemyWeaponTraceChannel =
    ECC_GameTraceChannel2;
}

FBHCombatantArchetypeProfile
ABHEnemySoldier::BuildCombatantArchetypeProfile(
    EBHCombatantArchetype Archetype
)
{
    FBHCombatantArchetypeProfile Profile;
    Profile.Archetype = Archetype;

    switch (Archetype)
    {
    case EBHCombatantArchetype::Scout:
        Profile.DisplayName = NSLOCTEXT(
            "BrokenHorizon",
            "CombatantArchetypeScout",
            "SCOUT"
        );
        Profile.MaximumHealth = 80.0f;
        Profile.MovementSpeed = 380.0f;
        Profile.DesiredEngagementDistance = 1900.0f;
        Profile.MaximumEngagementDistance = 3500.0f;
        Profile.CombatRepositionInterval = 1.35f;
        Profile.CombatRepositionRadius = 750.0f;
        Profile.MinimumBurstShots = 1;
        Profile.MaximumBurstShots = 2;
        Profile.MinimumBurstRecovery = 1.7f;
        Profile.MaximumBurstRecovery = 2.4f;
        Profile.CoverSearchRadius = 3000.0f;
        Profile.CoverHoldDuration = 4.0f;
        Profile.SuppressionCoverThreshold = 0.25f;
        Profile.RetreatHealthThreshold = 0.45f;
        Profile.RetreatSuppressionThreshold = 0.65f;
        Profile.ShotDamage = 15.0f;
        Profile.FireInterval = 0.95f;
        Profile.MagazineCapacity = 15;
        Profile.StartingReserveAmmo = 45;
        Profile.ReloadDuration = 2.0f;
        Profile.MaximumFragGrenades = 0;
        Profile.GrenadeUseChance = 0.0f;
        Profile.SightRadius = 3300.0f;
        Profile.LoseSightRadius = 3900.0f;
        Profile.HearingRange = 4500.0f;
        Profile.bHasBodyArmor = false;
        break;

    case EBHCombatantArchetype::Gunner:
        Profile.DisplayName = NSLOCTEXT(
            "BrokenHorizon",
            "CombatantArchetypeGunner",
            "GUNNER"
        );
        Profile.MaximumHealth = 130.0f;
        Profile.MovementSpeed = 250.0f;
        Profile.DesiredEngagementDistance = 1050.0f;
        Profile.MaximumEngagementDistance = 2800.0f;
        Profile.CombatRepositionInterval = 3.1f;
        Profile.CombatRepositionRadius = 300.0f;
        Profile.MinimumBurstShots = 5;
        Profile.MaximumBurstShots = 8;
        Profile.MinimumBurstRecovery = 0.9f;
        Profile.MaximumBurstRecovery = 1.4f;
        Profile.CoverSearchRadius = 1800.0f;
        Profile.CoverHoldDuration = 8.0f;
        Profile.SuppressionCoverThreshold = 0.55f;
        Profile.RetreatHealthThreshold = 0.20f;
        Profile.RetreatSuppressionThreshold = 0.90f;
        Profile.ShotDamage = 8.0f;
        Profile.FireInterval = 0.34f;
        Profile.MagazineCapacity = 60;
        Profile.StartingReserveAmmo = 120;
        Profile.ReloadDuration = 3.4f;
        Profile.MaximumFragGrenades = 0;
        Profile.GrenadeUseChance = 0.0f;
        Profile.SightRadius = 2600.0f;
        Profile.LoseSightRadius = 3100.0f;
        Profile.HearingRange = 3800.0f;
        Profile.bHasBodyArmor = true;
        break;

    case EBHCombatantArchetype::Rifleman:
    default:
        Profile.Archetype = EBHCombatantArchetype::Rifleman;
        Profile.DisplayName = NSLOCTEXT(
            "BrokenHorizon",
            "CombatantArchetypeRifleman",
            "RIFLEMAN"
        );
        break;
    }

    return Profile;
}

EBHCombatantArchetype ABHEnemySoldier::ChooseFormationArchetype(
    int32 FormationIndex,
    int32 FormationSize
)
{
    const int32 SafeSize = FMath::Max(1, FormationSize);
    const int32 SafeIndex = FMath::Clamp(
        FormationIndex,
        0,
        SafeSize - 1
    );
    if (SafeSize >= 3 && SafeIndex == SafeSize - 1)
    {
        return EBHCombatantArchetype::Gunner;
    }
    if (SafeSize >= 2 && SafeIndex == 1)
    {
        return EBHCombatantArchetype::Scout;
    }
    return EBHCombatantArchetype::Rifleman;
}

namespace
{
    int32 ApplyCasualtyMoraleShock(
        ABHEnemySoldier* Casualty,
        AActor* DamageCauser
    )
    {
        if (!IsValid(Casualty) ||
            !IsValid(Casualty->GetWorld()))
        {
            return 0;
        }

        const float MoraleRadius =
            Casualty->GetAllyCasualtyMoraleRadius();
        const float BaseSuppression =
            Casualty->GetAllyCasualtySuppression();

        if (MoraleRadius <= 0.0f ||
            BaseSuppression <= 0.0f)
        {
            return 0;
        }

        int32 AffectedAllies = 0;
        float TotalSuppressionApplied = 0.0f;

        for (TActorIterator<ABHEnemySoldier> It(
                Casualty->GetWorld());
            It;
            ++It)
        {
            ABHEnemySoldier* Ally = *It;

            if (!IsValid(Ally) ||
                Ally == Casualty ||
                Ally->IsDead() ||
                Ally->IsIncapacitated() ||
                Ally->GetCombatFaction() !=
                    Casualty->GetCombatFaction() ||
                FVector::DistSquared2D(
                    Ally->GetActorLocation(),
                    Casualty->GetActorLocation()
                ) > FMath::Square(MoraleRadius))
            {
                continue;
            }

            const float Distance = FVector::Dist2D(
                Ally->GetActorLocation(),
                Casualty->GetActorLocation()
            );
            const float DistanceAlpha = 1.0f -
                FMath::Clamp(
                    Distance / MoraleRadius,
                    0.0f,
                    1.0f
                );
            const float AppliedSuppression = BaseSuppression *
                FMath::Lerp(
                    0.35f,
                    1.0f,
                    DistanceAlpha
                );

            Ally->ApplySuppression(
                AppliedSuppression,
                DamageCauser
            );
            TotalSuppressionApplied += AppliedSuppression;
            ++AffectedAllies;
        }

        if (AffectedAllies > 0)
        {
            UE_LOG(
                LogTemp,
                Display,
                TEXT(
                    "BH_AI_MORALE_SHOCK distance_falloff "
                    "casualty=%s faction=%d state=%s affected=%d "
                    "total=%.2f radius=%.0f"
                ),
                *Casualty->GetName(),
                static_cast<int32>(
                    Casualty->GetCombatFaction()
                ),
                Casualty->IsIncapacitated()
                    ? TEXT("incapacitated")
                    : TEXT("lethal"),
                AffectedAllies,
                TotalSuppressionApplied,
                MoraleRadius
            );
        }

        return AffectedAllies;
    }
}

ABHEnemySoldier::ABHEnemySoldier()
{
    PrimaryActorTick.bCanEverTick = false;
    SetReplicates(true);
    SetReplicateMovement(true);

    HealthComponent = CreateDefaultSubobject<UBHHealthComponent>(
        TEXT("HealthComponent")
    );

    NavigationInvoker = CreateDefaultSubobject<UNavigationInvokerComponent>(
        TEXT("NavigationInvoker")
    );
    NavigationInvoker->SetGenerationRadii(
        8000.0f,
        12000.0f
    );
    ArchetypeLabel = CreateDefaultSubobject<UTextRenderComponent>(
        TEXT("ArchetypeLabel")
    );
    ArchetypeLabel->SetupAttachment(GetRootComponent());
    ArchetypeLabel->SetRelativeLocation(FVector(0.0f, 0.0f, 125.0f));
    ArchetypeLabel->SetHorizontalAlignment(EHTA_Center);
    ArchetypeLabel->SetWorldSize(18.0f);
    ArchetypeLabel->SetHiddenInGame(true);

    MuzzlePoint = CreateDefaultSubobject<USceneComponent>(
        TEXT("MuzzlePoint")
    );
    MuzzlePoint->SetupAttachment(GetRootComponent());
    MuzzlePoint->SetRelativeLocation(FVector(50.0f, 0.0f, 50.0f));

    ImpactActorClass = ABHImpactEffect::StaticClass();
    FragGrenadeClass = ABHFragGrenade::StaticClass();
    BattlefieldAmmoSupplyClass = ABHAmmoSupply::StaticClass();

    AIControllerClass = ABHEnemyAIController::StaticClass();
    AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

    bUseControllerRotationYaw = true;
    GetCharacterMovement()->bOrientRotationToMovement = false;
    GetCharacterMovement()->MaxWalkSpeed = 300.0f;

    const ConstructorHelpers::FObjectFinder<USoundBase> FireSoundAsset(
        TEXT("/Game/BrokenHorizon/Audio/SW_FirstLight_WeaponFire.SW_FirstLight_WeaponFire")
    );
    if (FireSoundAsset.Succeeded())
    {
        FireSound = FireSoundAsset.Object;
    }
    const ConstructorHelpers::FObjectFinder<USoundBase> IndoorTailAsset(
        TEXT("/Game/BrokenHorizon/Audio/SW_FirstLight_IndoorTail.SW_FirstLight_IndoorTail")
    );
    if (IndoorTailAsset.Succeeded())
    {
        IndoorFireTailSound = IndoorTailAsset.Object;
    }
    const ConstructorHelpers::FObjectFinder<USoundBase> OutdoorTailAsset(
        TEXT("/Game/BrokenHorizon/Audio/SW_FirstLight_OutdoorTail.SW_FirstLight_OutdoorTail")
    );
    if (OutdoorTailAsset.Succeeded())
    {
        OutdoorFireTailSound = OutdoorTailAsset.Object;
    }
}

void ABHEnemySoldier::GetLifetimeReplicatedProps(
    TArray<FLifetimeProperty>& OutLifetimeProps
) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(ABHEnemySoldier, CombatFaction);
    DOREPLIFETIME(ABHEnemySoldier, CombatantArchetype);
    DOREPLIFETIME(ABHEnemySoldier, bIncapacitated);
    DOREPLIFETIME(
        ABHEnemySoldier,
        bRequiresMedicalEvacuation
    );
    DOREPLIFETIME(ABHEnemySoldier, FieldOperativeID);
    DOREPLIFETIME(
        ABHEnemySoldier,
        IncapacitationSecondsRemaining
    );
    DOREPLIFETIME(ABHEnemySoldier, bSurrendered);
    DOREPLIFETIME(
        ABHEnemySoldier,
        SurrenderEscapeSecondsRemaining
    );
    DOREPLIFETIME(ABHEnemySoldier, bSurrenderSecured);
}

void ABHEnemySoldier::BeginPlay()
{
    Super::BeginPlay();

    if (UGameInstance* GameInstance = GetGameInstance())
    {
        if (UBHSaveSubsystem* SaveSubsystem =
            GameInstance->GetSubsystem<UBHSaveSubsystem>())
        {
            SaveSubsystem->ApplyPendingSurrenderState(this);
        }
    }

    CaptureArchetypeBaseline();
    ApplyCombatantArchetype(false);
    UpdateCombatFactionTags();
    GetCharacterMovement()->MaxWalkSpeed =
        GetNormalMovementSpeed();
    RefillAmmunition();
    RefillFragGrenades();
    bReloading = false;
    ReloadEndTime = -BIG_NUMBER;

#if !UE_BUILD_SHIPPING
    if (HasAuthority() &&
        FParse::Param(
            FCommandLine::Get(),
            TEXT("BHTestCombatantArchetypes")))
    {
        SetCombatantArchetype(EBHCombatantArchetype::Scout);
        const bool bScoutPassed =
            GetCombatantArchetype() == EBHCombatantArchetype::Scout &&
            FMath::IsNearlyEqual(GetNormalMovementSpeed(), 380.0f) &&
            GetMagazineCapacity() == 15 &&
            IsValid(HealthComponent) &&
            FMath::IsNearlyEqual(HealthComponent->GetMaxHealth(), 80.0f);
        SetCombatantArchetype(EBHCombatantArchetype::Gunner);
        const bool bGunnerPassed =
            GetCombatantArchetype() == EBHCombatantArchetype::Gunner &&
            FMath::IsNearlyEqual(GetNormalMovementSpeed(), 250.0f) &&
            GetMagazineCapacity() == 60 &&
            IsValid(HealthComponent) &&
            FMath::IsNearlyEqual(HealthComponent->GetMaxHealth(), 130.0f) &&
            bHasBodyArmor;
        const bool bPassed = bScoutPassed && bGunnerPassed;
        if (bPassed)
        {
            UE_LOG(
                LogTemp,
                Display,
                TEXT(
                    "BH_COMBATANT_ARCHETYPE_RUNTIME result=success "
                    "scout=1 gunner=1"
                )
            );
        }
        else
        {
            UE_LOG(
                LogTemp,
                Error,
                TEXT(
                    "BH_COMBATANT_ARCHETYPE_RUNTIME result=failure "
                    "scout=%d gunner=%d"
                ),
                bScoutPassed ? 1 : 0,
                bGunnerPassed ? 1 : 0
            );
        }
        SetCombatantArchetype(EBHCombatantArchetype::Rifleman);
    }
#endif

    USkeletalMeshComponent* CharacterMesh = GetMesh();
    UCapsuleComponent* CollisionCapsule = GetCapsuleComponent();

    if (IsValid(CharacterMesh))
    {
        InitialMeshRelativeTransform =
            CharacterMesh->GetRelativeTransform();
    }

    if (IsValid(CharacterMesh) &&
        IsValid(CharacterMesh->GetPhysicsAsset()) &&
        IsValid(CollisionCapsule))
    {
        CollisionCapsule->SetCollisionResponseToChannel(
            ECC_Visibility,
            ECR_Ignore
        );
        CharacterMesh->SetCollisionEnabled(
            ECollisionEnabled::QueryOnly
        );
        CharacterMesh->SetCollisionResponseToChannel(
            ECC_Visibility,
            ECR_Block
        );
    }

    if (IsValid(HealthComponent))
    {
        HealthComponent->OnDamaged.AddDynamic(
            this,
            &ABHEnemySoldier::HandleDamaged
        );
        HealthComponent->OnDeath.AddDynamic(
            this,
            &ABHEnemySoldier::HandleDeath
        );
    }
}

void ABHEnemySoldier::SetCombatantArchetype(
    EBHCombatantArchetype NewArchetype
)
{
    if (HasActorBegunPlay() && !HasAuthority())
    {
        return;
    }
    if (static_cast<uint8>(NewArchetype) >
        static_cast<uint8>(EBHCombatantArchetype::Gunner))
    {
        NewArchetype = EBHCombatantArchetype::Rifleman;
    }
    CaptureArchetypeBaseline();
    CombatantArchetype = NewArchetype;
    ApplyCombatantArchetype(true);
    ForceNetUpdate();
}

EBHCombatantArchetype
ABHEnemySoldier::GetCombatantArchetype() const
{
    return CombatantArchetype;
}

FText ABHEnemySoldier::GetCombatantArchetypeDisplayName() const
{
    return BuildCombatantArchetypeProfile(
        CombatantArchetype
    ).DisplayName;
}

void ABHEnemySoldier::CaptureArchetypeBaseline()
{
    if (bHasArchetypeBaseline)
    {
        return;
    }
    ArchetypeBaseline = BuildCombatantArchetypeProfile(
        EBHCombatantArchetype::Rifleman
    );
    ArchetypeBaseline.MaximumHealth = IsValid(HealthComponent)
        ? HealthComponent->GetMaxHealth()
        : 100.0f;
    ArchetypeBaseline.MovementSpeed = NormalMovementSpeed;
    ArchetypeBaseline.DesiredEngagementDistance = DesiredEngagementDistance;
    ArchetypeBaseline.MaximumEngagementDistance = MaximumEngagementDistance;
    ArchetypeBaseline.CombatRepositionInterval = CombatRepositionInterval;
    ArchetypeBaseline.CombatRepositionRadius = CombatRepositionRadius;
    ArchetypeBaseline.MinimumBurstShots = MinimumBurstShots;
    ArchetypeBaseline.MaximumBurstShots = MaximumBurstShots;
    ArchetypeBaseline.MinimumBurstRecovery = MinimumBurstRecovery;
    ArchetypeBaseline.MaximumBurstRecovery = MaximumBurstRecovery;
    ArchetypeBaseline.CoverSearchRadius = CoverSearchRadius;
    ArchetypeBaseline.CoverHoldDuration = CoverHoldDuration;
    ArchetypeBaseline.SuppressionCoverThreshold = SuppressionCoverThreshold;
    ArchetypeBaseline.RetreatHealthThreshold = RetreatHealthThreshold;
    ArchetypeBaseline.RetreatSuppressionThreshold = RetreatSuppressionThreshold;
    ArchetypeBaseline.RetreatReadinessThreshold = RetreatReadinessThreshold;
    ArchetypeBaseline.ShotDamage = ShotDamage;
    ArchetypeBaseline.FireInterval = FireInterval;
    ArchetypeBaseline.MagazineCapacity = MagazineCapacity;
    ArchetypeBaseline.StartingReserveAmmo = StartingReserveAmmo;
    ArchetypeBaseline.ReloadDuration = ReloadDuration;
    ArchetypeBaseline.MaximumFragGrenades = MaximumFragGrenades;
    ArchetypeBaseline.GrenadeUseChance = GrenadeUseChance;
    ArchetypeBaseline.SightRadius = SightRadius;
    ArchetypeBaseline.LoseSightRadius = LoseSightRadius;
    ArchetypeBaseline.HearingRange = HearingRange;
    ArchetypeBaseline.bHasBodyArmor = bHasBodyArmor;
    bHasArchetypeBaseline = true;
}

void ABHEnemySoldier::ApplyCombatantArchetype(
    bool bResetResources
)
{
    FBHCombatantArchetypeProfile Profile =
        CombatantArchetype == EBHCombatantArchetype::Rifleman &&
            bHasArchetypeBaseline
        ? ArchetypeBaseline
        : BuildCombatantArchetypeProfile(CombatantArchetype);
    NormalMovementSpeed = Profile.MovementSpeed;
    DesiredEngagementDistance = Profile.DesiredEngagementDistance;
    MaximumEngagementDistance = Profile.MaximumEngagementDistance;
    CombatRepositionInterval = Profile.CombatRepositionInterval;
    CombatRepositionRadius = Profile.CombatRepositionRadius;
    MinimumBurstShots = Profile.MinimumBurstShots;
    MaximumBurstShots = Profile.MaximumBurstShots;
    MinimumBurstRecovery = Profile.MinimumBurstRecovery;
    MaximumBurstRecovery = Profile.MaximumBurstRecovery;
    CoverSearchRadius = Profile.CoverSearchRadius;
    CoverHoldDuration = Profile.CoverHoldDuration;
    SuppressionCoverThreshold = Profile.SuppressionCoverThreshold;
    RetreatHealthThreshold = Profile.RetreatHealthThreshold;
    RetreatSuppressionThreshold = Profile.RetreatSuppressionThreshold;
    RetreatReadinessThreshold = Profile.RetreatReadinessThreshold;
    ShotDamage = Profile.ShotDamage;
    FireInterval = Profile.FireInterval;
    MagazineCapacity = Profile.MagazineCapacity;
    StartingReserveAmmo = Profile.StartingReserveAmmo;
    ReloadDuration = Profile.ReloadDuration;
    MaximumFragGrenades = Profile.MaximumFragGrenades;
    GrenadeUseChance = Profile.GrenadeUseChance;
    SightRadius = Profile.SightRadius;
    LoseSightRadius = FMath::Max(
        SightRadius,
        Profile.LoseSightRadius
    );
    HearingRange = Profile.HearingRange;
    bHasBodyArmor = Profile.bHasBodyArmor;

    if (IsValid(HealthComponent))
    {
        HealthComponent->ConfigureMaximumHealth(
            Profile.MaximumHealth,
            bResetResources
        );
    }
    if (IsValid(GetCharacterMovement()))
    {
        GetCharacterMovement()->MaxWalkSpeed = GetNormalMovementSpeed();
    }
    if (bResetResources)
    {
        RefillAmmunition();
        RefillFragGrenades();
    }
    RefreshArchetypePresentation();
}

void ABHEnemySoldier::RefreshArchetypePresentation()
{
    if (!IsValid(ArchetypeLabel))
    {
        return;
    }
    const bool bSpecialist =
        CombatantArchetype != EBHCombatantArchetype::Rifleman;
    ArchetypeLabel->SetHiddenInGame(!bSpecialist);
    ArchetypeLabel->SetVisibility(bSpecialist);
    ArchetypeLabel->SetText(GetCombatantArchetypeDisplayName());
    ArchetypeLabel->SetTextRenderColor(
        CombatFaction == EBHCombatFaction::Friendly
            ? FColor(75, 175, 235)
            : CombatantArchetype == EBHCombatantArchetype::Gunner
                ? FColor(235, 75, 55)
                : FColor(245, 174, 48)
    );
}

UBHHealthComponent* ABHEnemySoldier::GetHealthComponent() const
{
    return HealthComponent;
}

bool ABHEnemySoldier::IsDead() const
{
    return bDeathHandled ||
        (IsValid(HealthComponent) && HealthComponent->IsDead());
}

bool ABHEnemySoldier::ShouldNotifyControllerOfDamage(
    float RemainingHealth,
    bool bAlreadyDead
)
{
    return !bAlreadyDead && RemainingHealth > 0.0f;
}

bool ABHEnemySoldier::ShouldSurrender(
    float SuppressionLevel,
    float CombatReadiness,
    bool bOutOfAmmunition,
    int32 NearbyAllies
)
{
    const float SafeSuppression = FMath::Clamp(
        SuppressionLevel,
        0.0f,
        1.0f
    );
    const float SafeReadiness = FMath::Clamp(
        CombatReadiness,
        0.0f,
        1.0f
    );
    const int32 SafeNearbyAllies = FMath::Max(0, NearbyAllies);
    const bool bIsolated = SafeNearbyAllies <= 1;
    const bool bOverwhelmed = SafeSuppression >= 0.85f;
    const bool bWoundedMorale = SafeReadiness <= 0.50f;
    return bIsolated && bOverwhelmed &&
        (bOutOfAmmunition || bWoundedMorale);
}

float ABHEnemySoldier::CalculateSurrenderEscapeRemaining(
    float CurrentRemainingSeconds,
    float DeltaSeconds,
    bool bFriendlyPlayerNearby,
    float CustodyGraceSeconds
)
{
    if (bFriendlyPlayerNearby)
    {
        return FMath::Max(0.0f, CustodyGraceSeconds);
    }

    return FMath::Max(
        0.0f,
        CurrentRemainingSeconds - FMath::Max(0.0f, DeltaSeconds)
    );
}

bool ABHEnemySoldier::IsSurrendered() const
{
	return bSurrendered;
}

FName ABHEnemySoldier::GetSurrenderSectorID() const
{
	return ResolveSurrenderSectorID();
}

void ABHEnemySoldier::RestoreSurrenderPersistence(bool bNewSurrendered, bool bNewCustodySecured, float NewEscapeSecondsRemaining)
{
	if (!HasAuthority())
	{
		return;
	}

	if (bDeathHandled)
	{
		return;
	}

	const bool bWasSurrendered = bSurrendered;
	const bool bWasCustodySecured = bSurrenderSecured;
	bSurrendered = bNewSurrendered;
	bSurrenderSecured = bNewSurrendered && bNewCustodySecured;
	SurrenderEscapeSecondsRemaining = bSurrendered && !bSurrenderSecured
		? FMath::Max(0.0f, NewEscapeSecondsRemaining)
		: 0.0f;

	// A detained enemy has already contributed its detention outcome before the save.
	// An unsecured surrender remains eligible to report an escape or death after restore.
	bSurrenderConductReported = false;
	bSurrenderDetentionReported = bSurrenderSecured;

	if (bSurrendered && !bWasSurrendered)
	{
		OnEnemySurrendered(true);
	}

	if (bSurrenderSecured && !bWasCustodySecured)
	{
		OnEnemyCustodySecured(true);
	}

	ForceNetUpdate();
}

bool ABHEnemySoldier::IsSurrenderSecured() const
{
    return bSurrenderSecured;
}

void ABHEnemySoldier::SetSurrendered(bool bNewSurrendered)
{
    if (!HasAuthority())
    {
        return;
    }

    if (bSurrendered == bNewSurrendered)
    {
        return;
    }

    const bool bWasSecured = bSurrenderSecured;
    bSurrendered = bNewSurrendered;
    bSurrenderSecured = false;
    if (bSurrendered)
    {
        bSurrenderConductReported = false;
        bSurrenderDetentionReported = false;
    }
    SurrenderEscapeSecondsRemaining = bSurrendered
        ? GetSurrenderCustodyGraceSeconds()
        : 0.0f;
    if (bWasSecured)
    {
        OnEnemyCustodySecured(false);
    }
    if (bSurrendered)
    {
        PlayBark(EBHEnemyBarkType::Surrender);
    }
    OnEnemySurrendered(bSurrendered);
    ForceNetUpdate();
}

bool ABHEnemySoldier::SecureSurrender()
{
    if (!HasAuthority() || IsDead() || !bSurrendered ||
        bSurrenderSecured)
    {
        return false;
    }

    bSurrenderSecured = true;
    SurrenderEscapeSecondsRemaining = 0.0f;
    ReportSurrenderDetained();
    OnEnemyCustodySecured(true);
    ForceNetUpdate();
    return true;
}

FName ABHEnemySoldier::ResolveSurrenderSectorID() const
{
    UGameInstance* GameInstance = GetGameInstance();
    UBHWarSubsystem* WarSubsystem = IsValid(GameInstance)
        ? GameInstance->GetSubsystem<UBHWarSubsystem>()
        : nullptr;
    if (!IsValid(WarSubsystem))
    {
        return NAME_None;
    }
    FName SectorID = WarSubsystem->GetCommittedOperationSectorID();
    return SectorID.IsNone()
        ? WarSubsystem->GetPrioritySectorID()
        : SectorID;
}

void ABHEnemySoldier::ReportSurrenderDetained()
{
    if (!HasAuthority() || bSurrenderDetentionReported ||
        CombatFaction != EBHCombatFaction::Hostile)
    {
        return;
    }

    UGameInstance* GameInstance = GetGameInstance();
    UBHWarSubsystem* WarSubsystem = IsValid(GameInstance)
        ? GameInstance->GetSubsystem<UBHWarSubsystem>()
        : nullptr;
    const FName SectorID = ResolveSurrenderSectorID();
    if (!IsValid(WarSubsystem) || SectorID.IsNone())
    {
        return;
    }

    bSurrenderDetentionReported = WarSubsystem->ReportEnemyDetained(
        SectorID
    );
}

void ABHEnemySoldier::ReportSurrenderConductOutcome(bool bKilled)
{
    if (!HasAuthority() || bSurrenderConductReported ||
        CombatFaction != EBHCombatFaction::Hostile)
    {
        return;
    }

    UGameInstance* GameInstance = GetGameInstance();
    UBHWarSubsystem* WarSubsystem = IsValid(GameInstance)
        ? GameInstance->GetSubsystem<UBHWarSubsystem>()
        : nullptr;
    if (!IsValid(WarSubsystem))
    {
        return;
    }

    const FName SectorID = ResolveSurrenderSectorID();
    if (SectorID.IsNone())
    {
        return;
    }

    const bool bReported = bKilled
        ? WarSubsystem->ReportSurrenderedEnemyKilled(SectorID)
        : WarSubsystem->ReportSurrenderedEnemyEscaped(SectorID);
    bSurrenderConductReported = bReported;
}

void ABHEnemySoldier::UpdateSurrenderEscapeState(
    float DeltaSeconds,
    bool bFriendlyPlayerNearby
)
{
    if (!HasAuthority() || !bSurrendered || bSurrenderSecured)
    {
        return;
    }

    SurrenderEscapeSecondsRemaining =
        CalculateSurrenderEscapeRemaining(
            SurrenderEscapeSecondsRemaining,
            DeltaSeconds,
            bFriendlyPlayerNearby,
            GetSurrenderCustodyGraceSeconds()
        );
    if (SurrenderEscapeSecondsRemaining <= KINDA_SMALL_NUMBER)
    {
        ReportSurrenderConductOutcome(false);
        SetSurrendered(false);
        return;
    }
    ForceNetUpdate();
}

bool ABHEnemySoldier::IsIncapacitated() const
{
    return bIncapacitated;
}

bool ABHEnemySoldier::RequiresMedicalEvacuation() const
{
    return bRequiresMedicalEvacuation && !IsDead();
}

FName ABHEnemySoldier::GetFieldOperativeID() const
{
    return FieldOperativeID;
}

void ABHEnemySoldier::SetFieldOperativeID(
    FName NewFieldOperativeID
)
{
    if (!HasAuthority())
    {
        return;
    }

    FieldOperativeID = NewFieldOperativeID.IsNone()
        ? FName(*FString::Printf(
            TEXT("FieldOperative_%s"),
            *FGuid::NewGuid().ToString(EGuidFormats::Digits)
        ))
        : NewFieldOperativeID;
    ForceNetUpdate();
}

void ABHEnemySoldier::RestoreMedicalEvacuationState(
    bool bSavedRequiresMedicalEvacuation
)
{
    bRequiresMedicalEvacuation =
        CombatFaction == EBHCombatFaction::Friendly &&
        bSavedRequiresMedicalEvacuation &&
        !IsDead();
    ForceNetUpdate();
}

float ABHEnemySoldier::GetIncapacitationSecondsRemaining() const
{
    return FMath::Max(
        0.0f,
        IncapacitationSecondsRemaining
    );
}

bool ABHEnemySoldier::StabilizeIncapacitatedSoldier()
{
    if (!HasAuthority() ||
        !bIncapacitated ||
        CombatFaction != EBHCombatFaction::Friendly ||
        !IsValid(HealthComponent))
    {
        return false;
    }

    RestoreFromDeathPresentation();
    HealthComponent->RestorePersistentHealthState(
        FMath::Clamp(
            StabilizedHealth,
            1.0f,
            HealthComponent->GetMaxHealth()
        )
    );
    bIncapacitated = false;
    bDeathHandled = false;
    IncapacitationSecondsRemaining = 0.0f;
    GetWorldTimerManager().ClearTimer(
        IncapacitationTimerHandle
    );
    SetLifeSpan(0.0f);
    SetCanBeDamaged(true);
    CombatReadiness = FMath::Min(
        GetCombatReadiness(),
        FMath::Clamp(
            PostCasualtyCombatReadiness,
            0.0f,
            1.0f
        )
    );
    bRequiresMedicalEvacuation = true;

    if (UCapsuleComponent* CollisionCapsule =
            GetCapsuleComponent())
    {
        CollisionCapsule->SetCollisionEnabled(
            ECollisionEnabled::QueryAndPhysics
        );
        CollisionCapsule->SetCollisionResponseToChannel(
            ECC_Visibility,
            ECR_Ignore
        );
    }

    GetCharacterMovement()->SetMovementMode(MOVE_Walking);
    GetCharacterMovement()->MaxWalkSpeed =
        GetNormalMovementSpeed();

    if (AController* ExistingController = GetController())
    {
        ExistingController->UnPossess();
        ExistingController->Destroy();
    }

    SpawnDefaultController();
    MulticastRestoreFromIncapacitation();
    ForceNetUpdate();

    UE_LOG(
        LogTemp,
        Display,
        TEXT(
            "BH_FIELD_OPERATIVE_STABILIZED soldier=%s health=%.1f"
        ),
        *GetName(),
        HealthComponent->GetCurrentHealth()
    );
    return true;
}

void ABHEnemySoldier::RestoreIncapacitatedState()
{
    RestoreIncapacitatedStateWithRemainingTime(
        IncapacitationDuration
    );
}

void ABHEnemySoldier::
    RestoreIncapacitatedStateWithRemainingTime(
        float SavedRemainingSeconds
    )
{
    if (CombatFaction != EBHCombatFaction::Friendly ||
        !IsValid(HealthComponent))
    {
        return;
    }

    HealthComponent->RestorePersistentHealthState(1.0f);
    EnterFriendlyIncapacitation(nullptr, false);
    IncapacitationSecondsRemaining = FMath::Clamp(
        SavedRemainingSeconds,
        1.0f,
        FMath::Max(1.0f, IncapacitationDuration)
    );
    StartIncapacitationTimer();
}

void ABHEnemySoldier::SetCombatFaction(
    EBHCombatFaction NewCombatFaction
)
{
    CombatFaction = NewCombatFaction;
    UpdateCombatFactionTags();
    ForceNetUpdate();
}

void ABHEnemySoldier::UpdateCombatFactionTags()
{
    if (CombatFaction == EBHCombatFaction::Friendly)
    {
        Tags.AddUnique(TEXT("BH_Friendly"));
        Tags.Remove(TEXT("BH_Hostile"));
    }
    else
    {
        Tags.AddUnique(TEXT("BH_Hostile"));
        Tags.Remove(TEXT("BH_Friendly"));
    }
    RefreshArchetypePresentation();
}

void ABHEnemySoldier::OnRep_CombatantArchetype()
{
    CaptureArchetypeBaseline();
    ApplyCombatantArchetype(false);
}

void ABHEnemySoldier::OnRep_CombatFaction()
{
    UpdateCombatFactionTags();
}

void ABHEnemySoldier::OnRep_Surrendered()
{
    OnEnemySurrendered(bSurrendered);
}

void ABHEnemySoldier::OnRep_SurrenderSecured()
{
    OnEnemyCustodySecured(bSurrenderSecured);
}

void ABHEnemySoldier::OnRep_Incapacitated()
{
    if (!bIncapacitated || bDeathHandled)
    {
        return;
    }

    bDeathHandled = true;
    SetCanBeDamaged(false);
    GetCharacterMovement()->DisableMovement();

    if (UCapsuleComponent* CollisionCapsule =
            GetCapsuleComponent())
    {
        CollisionCapsule->SetCollisionEnabled(
            ECollisionEnabled::QueryOnly
        );
        CollisionCapsule->SetCollisionResponseToChannel(
            ECC_Visibility,
            ECR_Block
        );
    }

    if (GetNetMode() != NM_DedicatedServer)
    {
        PlayDeathReaction(nullptr);
    }
}

EBHCombatFaction ABHEnemySoldier::GetCombatFaction() const
{
    return CombatFaction;
}

bool ABHEnemySoldier::IsHostileTo(
    const AActor* OtherActor
) const
{
    if (IsSurrendered() || !IsValid(OtherActor) || OtherActor == this)
    {
        return false;
    }

    if (const ABHSupplyConvoyTarget* ConvoyTarget =
        Cast<ABHSupplyConvoyTarget>(OtherActor))
    {
        if (ConvoyTarget->IsResolved())
        {
            return false;
        }

        return (
            CombatFaction == EBHCombatFaction::Hostile &&
            ConvoyTarget->GetConvoyOwner() ==
                EBHWarFaction::Friendly
        ) || (
            CombatFaction == EBHCombatFaction::Friendly &&
            ConvoyTarget->GetConvoyOwner() ==
                EBHWarFaction::Enemy
        );
    }

    if (const ABHEnemySoldier* OtherSoldier =
        Cast<ABHEnemySoldier>(OtherActor))
    {
        return !OtherSoldier->IsDead() &&
            !OtherSoldier->IsSurrendered() &&
            OtherSoldier->CombatFaction != CombatFaction;
    }

    const APawn* OtherPawn = Cast<APawn>(OtherActor);
    return IsValid(OtherPawn) &&
        OtherPawn->IsPlayerControlled() &&
        CombatFaction == EBHCombatFaction::Hostile;
}

void ABHEnemySoldier::SetObjectiveIdToCompleteOnDeath(
    FName ObjectiveID
)
{
    ObjectiveIdToCompleteOnDeath = ObjectiveID;
}

FName ABHEnemySoldier::GetObjectiveIdToCompleteOnDeath() const
{
    return ObjectiveIdToCompleteOnDeath;
}

EBHHitZone ABHEnemySoldier::ResolveHitZone(
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
            return EBHHitZone::Head;
        }

        if (BoneName.Contains(TEXT("clavicle")) ||
            BoneName.Contains(TEXT("shoulder")) ||
            BoneName.Contains(TEXT("upperarm")) ||
            BoneName.Contains(TEXT("lowerarm")) ||
            BoneName.Contains(TEXT("forearm")) ||
            BoneName.Contains(TEXT("hand")) ||
            BoneName.Contains(TEXT("thumb")) ||
            BoneName.Contains(TEXT("index")) ||
            BoneName.Contains(TEXT("middle")) ||
            BoneName.Contains(TEXT("ring")) ||
            BoneName.Contains(TEXT("pinky")))
        {
            return EBHHitZone::Arm;
        }

        if (BoneName.Contains(TEXT("thigh")) ||
            BoneName.Contains(TEXT("calf")) ||
            BoneName.Contains(TEXT("shin")) ||
            BoneName.Contains(TEXT("leg")) ||
            BoneName.Contains(TEXT("foot")) ||
            BoneName.Contains(TEXT("ball")))
        {
            return EBHHitZone::Leg;
        }

        if (BoneName.Contains(TEXT("spine")) ||
            BoneName.Contains(TEXT("pelvis")) ||
            BoneName.Contains(TEXT("chest")))
        {
            return EBHHitZone::Torso;
        }
    }

    const UCapsuleComponent* CollisionCapsule =
        GetCapsuleComponent();

    if (!IsValid(CollisionCapsule))
    {
        return EBHHitZone::Torso;
    }

    const FVector LocalImpact =
        GetActorTransform().InverseTransformPosition(
            HitResult.ImpactPoint
        );
    const float CapsuleHalfHeight =
        FMath::Max(
            1.0f,
            CollisionCapsule->GetScaledCapsuleHalfHeight()
        );
    const float CapsuleRadius =
        FMath::Max(
            1.0f,
            CollisionCapsule->GetScaledCapsuleRadius()
        );
    const float NormalizedHeight =
        LocalImpact.Z / CapsuleHalfHeight;

    if (NormalizedHeight >= 0.55f)
    {
        return EBHHitZone::Head;
    }

    if (NormalizedHeight <= -0.20f)
    {
        return EBHHitZone::Leg;
    }

    if (NormalizedHeight >= -0.05f &&
        FMath::Abs(LocalImpact.Y) >= (CapsuleRadius * 0.72f))
    {
        return EBHHitZone::Arm;
    }

    return EBHHitZone::Torso;
}

float ABHEnemySoldier::GetHitZoneDamageMultiplier(
    EBHHitZone HitZone
) const
{
    float DamageMultiplier = 1.0f;

    switch (HitZone)
    {
    case EBHHitZone::Head:
        DamageMultiplier = HeadDamageMultiplier;

        if (bHasHelmet)
        {
            DamageMultiplier *= FMath::Clamp(
                HelmetDamageScale,
                0.0f,
                1.0f
            );
        }
        break;

    case EBHHitZone::Arm:
        DamageMultiplier = ArmDamageMultiplier;
        break;

    case EBHHitZone::Leg:
        DamageMultiplier = LegDamageMultiplier;
        break;

    case EBHHitZone::Torso:
    default:
        DamageMultiplier = TorsoDamageMultiplier;

        if (bHasBodyArmor)
        {
            DamageMultiplier *= FMath::Clamp(
                BodyArmorDamageScale,
                0.0f,
                1.0f
            );
        }
        break;
    }

    return FMath::Max(0.0f, DamageMultiplier);
}

bool ABHEnemySoldier::IsArmorMitigatingHitZone(
    EBHHitZone HitZone
) const
{
    return DoesArmorMitigateHitZone(
        HitZone,
        bHasHelmet,
        HelmetDamageScale,
        bHasBodyArmor,
        BodyArmorDamageScale
    );
}

bool ABHEnemySoldier::DoesArmorMitigateHitZone(
    EBHHitZone HitZone,
    bool bHelmetEquipped,
    float InHelmetDamageScale,
    bool bBodyArmorEquipped,
    float InBodyArmorDamageScale
)
{
    if (HitZone == EBHHitZone::Head)
    {
        return bHelmetEquipped &&
            FMath::Clamp(InHelmetDamageScale, 0.0f, 1.0f) <
                (1.0f - KINDA_SMALL_NUMBER);
    }

    if (HitZone == EBHHitZone::Torso)
    {
        return bBodyArmorEquipped &&
            FMath::Clamp(
                InBodyArmorDamageScale,
                0.0f,
                1.0f
            ) < (1.0f - KINDA_SMALL_NUMBER);
    }

    return false;
}

const TArray<TObjectPtr<ABHPatrolPoint>>&
ABHEnemySoldier::GetPatrolPoints() const
{
    return PatrolPoints;
}

void ABHEnemySoldier::SetPatrolPoints(
    const TArray<ABHPatrolPoint*>& NewPatrolPoints
)
{
    PatrolPoints.Reset();

    for (ABHPatrolPoint* PatrolPoint : NewPatrolPoints)
    {
        if (IsValid(PatrolPoint))
        {
            PatrolPoints.Add(PatrolPoint);
        }
    }
}

float ABHEnemySoldier::GetPatrolAcceptanceRadius() const
{
    return PatrolAcceptanceRadius;
}

float ABHEnemySoldier::GetPatrolWaitDuration() const
{
    return PatrolWaitDuration;
}

float ABHEnemySoldier::GetInvestigateDuration() const
{
    return InvestigateDuration;
}

float ABHEnemySoldier::GetSearchDuration() const
{
    return SearchDuration;
}

float ABHEnemySoldier::GetMinimumEngagementDistance() const
{
    return FMath::Min(
        MinimumEngagementDistance,
        DesiredEngagementDistance
    );
}

float ABHEnemySoldier::GetDesiredEngagementDistance() const
{
    return DesiredEngagementDistance;
}

float ABHEnemySoldier::GetMaximumEngagementDistance() const
{
    return FMath::Max(
        DesiredEngagementDistance,
        MaximumEngagementDistance
    );
}

float ABHEnemySoldier::GetCombatRepositionInterval() const
{
    const UGameInstance* GameInstance = GetGameInstance();
    const UBHWarSubsystem* WarSubsystem = GameInstance
        ? GameInstance->GetSubsystem<UBHWarSubsystem>()
        : nullptr;
    const float CoordinationMultiplier =
        CombatFaction == EBHCombatFaction::Hostile &&
            IsValid(WarSubsystem)
        ? FMath::Clamp(
            WarSubsystem->GetCampaignDifficulty()
                .EnemyCoordinationMultiplier,
            0.5f,
            2.0f
        )
        : 1.0f;
    return FMath::Max(
        0.1f,
        CombatRepositionInterval / CoordinationMultiplier
    );
}

float ABHEnemySoldier::GetCombatRepositionRadius() const
{
    return FMath::Max(0.0f, CombatRepositionRadius);
}

int32 ABHEnemySoldier::GetMinimumBurstShots() const
{
    return FMath::Max(1, MinimumBurstShots);
}

int32 ABHEnemySoldier::GetMaximumBurstShots() const
{
    return FMath::Max(
        GetMinimumBurstShots(),
        MaximumBurstShots
    );
}

float ABHEnemySoldier::GetMinimumBurstRecovery() const
{
    return FMath::Max(0.0f, MinimumBurstRecovery);
}

float ABHEnemySoldier::GetMaximumBurstRecovery() const
{
    return FMath::Max(
        GetMinimumBurstRecovery(),
        MaximumBurstRecovery
    );
}

float ABHEnemySoldier::GetCoverSearchRadius() const
{
    return FMath::Max(0.0f, CoverSearchRadius);
}

float ABHEnemySoldier::GetCoverAcceptanceRadius() const
{
    return FMath::Max(1.0f, CoverAcceptanceRadius);
}

float ABHEnemySoldier::GetCoverHoldDuration() const
{
    return FMath::Max(0.1f, CoverHoldDuration);
}

float ABHEnemySoldier::GetCoverHideDuration() const
{
    return FMath::Max(0.0f, CoverHideDuration);
}

float ABHEnemySoldier::GetCoverPeekDuration() const
{
    return FMath::Max(0.1f, CoverPeekDuration);
}

float ABHEnemySoldier::GetCoverReevaluationInterval() const
{
    return FMath::Max(0.1f, CoverReevaluationInterval);
}

float ABHEnemySoldier::GetSuppressionDecayRate() const
{
    return FMath::Max(0.0f, SuppressionDecayRate);
}

float ABHEnemySoldier::GetSuppressionCoverThreshold() const
{
    return FMath::Clamp(SuppressionCoverThreshold, 0.0f, 1.0f);
}

float ABHEnemySoldier::GetSuppressionSpreadPenalty() const
{
    return FMath::Max(0.0f, SuppressionSpreadPenalty);
}

float ABHEnemySoldier::GetRetreatHealthThreshold() const
{
    return FMath::Clamp(
        RetreatHealthThreshold,
        0.0f,
        1.0f
    );
}

float ABHEnemySoldier::GetRetreatSuppressionThreshold() const
{
    return FMath::Clamp(
        RetreatSuppressionThreshold,
        0.0f,
        1.0f
    );
}

float ABHEnemySoldier::GetRetreatReadinessThreshold() const
{
    return FMath::Clamp(
        RetreatReadinessThreshold,
        0.0f,
        1.0f
    );
}

float ABHEnemySoldier::GetRetreatDistance() const
{
    return FMath::Max(100.0f, RetreatDistance);
}

float ABHEnemySoldier::GetRetreatDuration() const
{
    return FMath::Max(0.1f, RetreatDuration);
}

float ABHEnemySoldier::GetNormalMovementSpeed() const
{
    return FMath::Max(0.0f, NormalMovementSpeed);
}

float ABHEnemySoldier::GetRetreatMovementSpeed() const
{
    return FMath::Max(0.0f, RetreatMovementSpeed);
}

float ABHEnemySoldier::GetAllyCasualtyMoraleRadius() const
{
    return FMath::Max(0.0f, AllyCasualtyMoraleRadius);
}

float ABHEnemySoldier::GetAllyCasualtySuppression() const
{
    return FMath::Clamp(
        AllyCasualtySuppression,
        0.0f,
        1.0f
    );
}

float ABHEnemySoldier::GetSightRadius() const
{
    return SightRadius;
}

float ABHEnemySoldier::GetLoseSightRadius() const
{
    return FMath::Max(SightRadius, LoseSightRadius);
}

float ABHEnemySoldier::GetPeripheralVisionAngle() const
{
    return PeripheralVisionAngle;
}

float ABHEnemySoldier::GetSightMemoryDuration() const
{
    return SightMemoryDuration;
}

float ABHEnemySoldier::GetHearingRange() const
{
    return HearingRange;
}

float ABHEnemySoldier::GetHearingMemoryDuration() const
{
    return HearingMemoryDuration;
}

float ABHEnemySoldier::GetSquadAlertRadius() const
{
    const UGameInstance* GameInstance = GetGameInstance();
    const UBHWarSubsystem* WarSubsystem = GameInstance
        ? GameInstance->GetSubsystem<UBHWarSubsystem>()
        : nullptr;
    const float CoordinationMultiplier =
        CombatFaction == EBHCombatFaction::Hostile &&
            IsValid(WarSubsystem)
        ? FMath::Clamp(
            WarSubsystem->GetCampaignDifficulty()
                .EnemyCoordinationMultiplier,
            0.5f,
            2.0f
        )
        : 1.0f;
    return FMath::Max(
        0.0f,
        SquadAlertRadius * CoordinationMultiplier
    );
}

float ABHEnemySoldier::GetRotationInterpSpeed() const
{
    return RotationInterpSpeed;
}

float ABHEnemySoldier::GetPatrolRetryInterval() const
{
    return PatrolRetryInterval;
}

bool ABHEnemySoldier::IsDebugEnabled() const
{
    return bEnableDebug;
}

bool ABHEnemySoldier::IsReloading() const
{
    if (!bReloading)
    {
        return false;
    }

    const UWorld* World = GetWorld();
    return !IsValid(World) ||
        World->GetTimeSeconds() < ReloadEndTime;
}

int32 ABHEnemySoldier::GetCurrentMagazineAmmo() const
{
    return FMath::Clamp(
        CurrentMagazineAmmo,
        0,
        GetMagazineCapacity()
    );
}

int32 ABHEnemySoldier::GetCurrentReserveAmmo() const
{
    return FMath::Max(0, CurrentReserveAmmo);
}

bool ABHEnemySoldier::HasCombatAmmunition() const
{
    return GetCurrentMagazineAmmo() > 0 ||
        GetCurrentReserveAmmo() > 0;
}

bool ABHEnemySoldier::IsOutOfAmmunition() const
{
    return !HasCombatAmmunition();
}

void ABHEnemySoldier::RefillAmmunition()
{
    CurrentMagazineAmmo = GetMagazineCapacity();
    CurrentReserveAmmo = GetStartingReserveAmmo();
    bReloading = false;
    bOutOfAmmoReported = false;
    ReloadEndTime = -BIG_NUMBER;
}

void ABHEnemySoldier::RestorePersistentCombatState(
    float SavedHealth,
    int32 SavedMagazineAmmo,
    int32 SavedReserveAmmo,
    int32 SavedFragGrenades,
    float SavedCombatReadiness
)
{
    if (IsValid(HealthComponent) && SavedHealth > 0.0f)
    {
        HealthComponent->RestorePersistentHealthState(SavedHealth);
    }

    if (SavedMagazineAmmo >= 0)
    {
        CurrentMagazineAmmo = FMath::Clamp(
            SavedMagazineAmmo,
            0,
            GetMagazineCapacity()
        );
    }

    if (SavedReserveAmmo >= 0)
    {
        CurrentReserveAmmo = FMath::Max(0, SavedReserveAmmo);
    }

    if (SavedFragGrenades >= 0)
    {
        CurrentFragGrenades = FMath::Clamp(
            SavedFragGrenades,
            0,
            GetMaximumFragGrenades()
        );
    }

    CombatReadiness = FMath::Clamp(
        SavedCombatReadiness,
        0.0f,
        1.0f
    );
    bReloading = false;
    bOutOfAmmoReported = !HasCombatAmmunition();
    ReloadEndTime = -BIG_NUMBER;
}

float ABHEnemySoldier::GetCombatReadiness() const
{
    return FMath::Clamp(CombatReadiness, 0.0f, 1.0f);
}

float ABHEnemySoldier::GetSurrenderAllyRadius() const
{
    return FMath::Max(0.0f, SurrenderAllyRadius);
}

float ABHEnemySoldier::GetSurrenderPlayerCaptureRadius() const
{
    return FMath::Max(0.0f, SurrenderPlayerCaptureRadius);
}

float ABHEnemySoldier::GetSurrenderCustodyGraceSeconds() const
{
    return FMath::Max(1.0f, SurrenderCustodyGraceSeconds);
}

float ABHEnemySoldier::GetSurrenderEscapeSecondsRemaining() const
{
    return bSurrenderSecured
        ? 0.0f
        : FMath::Max(0.0f, SurrenderEscapeSecondsRemaining);
}

bool ABHEnemySoldier::NeedsCombatService() const
{
    return !IsDead() &&
        (bRequiresMedicalEvacuation ||
         (IsValid(HealthComponent) &&
          !HealthComponent->IsFullHealth()) ||
         GetCurrentMagazineAmmo() < GetMagazineCapacity() ||
         GetCurrentReserveAmmo() < GetStartingReserveAmmo() ||
         GetCurrentFragGrenades() < GetMaximumFragGrenades() ||
         GetCombatReadiness() < 1.0f - KINDA_SMALL_NUMBER);
}

bool ABHEnemySoldier::ServiceCombatLoadout()
{
    if (!NeedsCombatService())
    {
        return false;
    }

    if (IsValid(HealthComponent))
    {
        HealthComponent->ResetHealth();
    }

    RefillAmmunition();
    RefillFragGrenades();
    CombatReadiness = 1.0f;
    bRequiresMedicalEvacuation = false;
    ForceNetUpdate();
    return true;
}

int32 ABHEnemySoldier::GetMagazineCapacity() const
{
    return FMath::Max(1, MagazineCapacity);
}

int32 ABHEnemySoldier::GetStartingReserveAmmo() const
{
    return FMath::Max(0, StartingReserveAmmo);
}

float ABHEnemySoldier::GetReloadDuration() const
{
    return FMath::Max(0.1f, ReloadDuration);
}

int32 ABHEnemySoldier::GetCurrentFragGrenades() const
{
    return FMath::Clamp(
        CurrentFragGrenades,
        0,
        GetMaximumFragGrenades()
    );
}

void ABHEnemySoldier::RefillFragGrenades()
{
    CurrentFragGrenades = GetMaximumFragGrenades();
}

int32 ABHEnemySoldier::GetMaximumFragGrenades() const
{
    return FMath::Max(0, MaximumFragGrenades);
}

float ABHEnemySoldier::GetMinimumGrenadeRange() const
{
    return FMath::Max(0.0f, MinimumGrenadeRange);
}

float ABHEnemySoldier::GetMaximumGrenadeRange() const
{
    return FMath::Max(
        GetMinimumGrenadeRange(),
        MaximumGrenadeRange
    );
}

float ABHEnemySoldier::GetGrenadeDecisionInterval() const
{
    return FMath::Max(0.1f, GrenadeDecisionInterval);
}

float ABHEnemySoldier::GetGrenadeUseChance() const
{
    return FMath::Clamp(GrenadeUseChance, 0.0f, 1.0f);
}

float ABHEnemySoldier::GetGrenadeFriendlySafetyRadius() const
{
    return FMath::Max(0.0f, GrenadeFriendlySafetyRadius);
}

bool ABHEnemySoldier::ThrowFragGrenadeAt(
    const FVector& TargetLocation
)
{
    UWorld* World = GetWorld();

    if (!IsValid(World) ||
        IsDead() ||
        IsSurrendered() ||
        RequiresMedicalEvacuation() ||
        CurrentFragGrenades <= 0 ||
        !FragGrenadeClass)
    {
        return false;
    }

    const FVector Forward =
        GetActorForwardVector().GetSafeNormal2D();
    const FVector SpawnLocation =
        GetActorLocation() +
        Forward * 55.0f +
        FVector(0.0f, 0.0f, 70.0f);
    const FVector AimLocation =
        TargetLocation + FVector(0.0f, 0.0f, 20.0f);
    const FVector ToTarget = AimLocation - SpawnLocation;
    const FVector HorizontalDelta(
        ToTarget.X,
        ToTarget.Y,
        0.0f
    );
    const float HorizontalDistance = HorizontalDelta.Size();
    const float TravelTime = FMath::Clamp(
        HorizontalDistance / 1100.0f,
        0.8f,
        1.6f
    );
    const float GravityZ = World->GetGravityZ();
    FVector LaunchVelocity =
        HorizontalDelta / TravelTime;
    LaunchVelocity.Z =
        (ToTarget.Z -
            0.5f * GravityZ *
                FMath::Square(TravelTime)) /
        TravelTime;

    FActorSpawnParameters SpawnParameters;
    SpawnParameters.Owner = this;
    SpawnParameters.Instigator = this;
    SpawnParameters.SpawnCollisionHandlingOverride =
        ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    ABHFragGrenade* Grenade =
        World->SpawnActor<ABHFragGrenade>(
            FragGrenadeClass,
            SpawnLocation,
            LaunchVelocity.Rotation(),
            SpawnParameters
        );

    if (!IsValid(Grenade))
    {
        return false;
    }

    Grenade->Throw(LaunchVelocity);
    CurrentFragGrenades = FMath::Max(
        0,
        CurrentFragGrenades - 1
    );
    PlayBark(EBHEnemyBarkType::Grenade);
    OnEnemyGrenadeThrown(TargetLocation);

    UE_LOG(
        LogTemp,
        Display,
        TEXT(
            "BH_AI_GRENADE_THROWN soldier=%s target=%s "
            "velocity=%s remaining=%d"
        ),
        *GetName(),
        *TargetLocation.ToCompactString(),
        *LaunchVelocity.ToCompactString(),
        CurrentFragGrenades
    );
    return true;
}

bool ABHEnemySoldier::FireAt(
    AActor* TargetActor,
    float AdditionalSpreadDegrees
)
{
    UWorld* World = GetWorld();
    AController* EnemyController = GetController();

    if (!HasAuthority() ||
        IsDead() ||
        IsSurrendered() ||
        RequiresMedicalEvacuation() ||
        !IsValid(World) ||
        !IsValid(TargetActor) ||
        !IsHostileTo(TargetActor) ||
        !IsValid(EnemyController) ||
        !EnemyController->LineOfSightTo(TargetActor))
    {
        return false;
    }

    const float CurrentTime = World->GetTimeSeconds();

    if (!UpdateReloadState(CurrentTime))
    {
        return false;
    }

    if (CurrentMagazineAmmo <= 0)
    {
        BeginReload(CurrentTime);
        return false;
    }

    if ((CurrentTime - LastFireTime) <
        BHWarOperationRules::
            CalculateFieldOperativeFireInterval(
                FireInterval,
                GetCombatReadiness(),
                MaximumReadinessFireIntervalMultiplier
            ))
    {
        return false;
    }

    const FVector TraceStart =
        GetPresentationMuzzleTransform().GetLocation();

    FVector TargetLocation;
    FRotator TargetRotation;
    TargetActor->GetActorEyesViewPoint(TargetLocation, TargetRotation);

    if (const ABHSupplyConvoyTarget* ConvoyTarget =
        Cast<ABHSupplyConvoyTarget>(TargetActor);
        IsValid(ConvoyTarget))
    {
        FVector BoundsExtent = FVector::ZeroVector;
        ConvoyTarget->GetActorBounds(
            true,
            TargetLocation,
            BoundsExtent
        );
    }
    else if (const ABHCharacter* PlayerTarget =
        Cast<ABHCharacter>(TargetActor);
        IsValid(PlayerTarget))
    {
        TargetLocation = PlayerTarget->GetActorLocation();

        if (const UCapsuleComponent* TargetCapsule =
            PlayerTarget->GetCapsuleComponent();
            IsValid(TargetCapsule))
        {
            TargetLocation += FVector::UpVector *
                TargetCapsule->GetScaledCapsuleHalfHeight() *
                0.15f;
        }
    }

    if (!HasClearLineOfFireTo(
            TraceStart,
            TargetLocation,
            TargetActor
        ))
    {
        return false;
    }

    const FVector AimDirection =
        (TargetLocation - TraceStart).GetSafeNormal();
    const FVector ShotDirection = FMath::VRandCone(
        AimDirection,
        FMath::DegreesToRadians(
            FMath::Max(
                0.0f,
                (ShotSpreadDegrees +
                    AdditionalSpreadDegrees +
                    BHWarOperationRules::
                        CalculateFieldOperativeReadinessSpread(
                            GetCombatReadiness(),
                            MaximumReadinessSpreadPenalty
                        )) *
                    UBHBattlefieldConditions::GetCurrentProfile(this).
                        WeaponSpreadMultiplier
            )
        )
    );
    const FVector TraceEnd =
        TraceStart + (ShotDirection * FMath::Max(0.0f, ShotRange));

    FCollisionQueryParams QueryParams(
        SCENE_QUERY_STAT(BHEnemyShot),
        true,
        this
    );
    QueryParams.AddIgnoredActor(this);

    FHitResult HitResult;
    const bool bHit = World->LineTraceSingleByChannel(
        HitResult,
        TraceStart,
        TraceEnd,
        EnemyWeaponTraceChannel,
        QueryParams
    );

    LastFireTime = CurrentTime;
    CurrentMagazineAmmo = FMath::Max(
        0,
        CurrentMagazineAmmo - 1
    );

    if (CurrentMagazineAmmo <= 0)
    {
        BeginReload(CurrentTime);
    }

    AActor* HitActor = bHit ? HitResult.GetActor() : nullptr;
    const bool bHitIntendedTarget =
        IsHitPartOfTarget(HitActor, TargetActor);
    AActor* DamageTarget = nullptr;

    if (bHitIntendedTarget)
    {
        DamageTarget = TargetActor;
    }
    else if (IsHostileTo(HitActor))
    {
        DamageTarget = HitActor;
    }

    MulticastFirePresentation(
        TargetActor,
        HitResult,
        bHit && !bHitIntendedTarget
    );

    if (!bHitIntendedTarget && NearMissRadius > 0.0f)
    {
        const FVector BulletTravelEnd =
            bHit ? HitResult.ImpactPoint : TraceEnd;
        const FVector ClosestPoint = FMath::ClosestPointOnSegment(
            TargetLocation,
            TraceStart,
            BulletTravelEnd
        );
        const float MissDistance = FVector::Distance(
            TargetLocation,
            ClosestPoint
        );

        if (MissDistance <= NearMissRadius)
        {
            const float Proximity = 1.0f - FMath::Clamp(
                MissDistance / NearMissRadius,
                0.0f,
                1.0f
            );
            const float Intensity = FMath::Lerp(
                FMath::Clamp(
                    NearMissMinimumIntensity,
                    0.0f,
                    1.0f
                ),
                1.0f,
                Proximity
            );

            if (ABHCharacter* PlayerTarget =
                Cast<ABHCharacter>(TargetActor);
                IsValid(PlayerTarget))
            {
                const FVector SourceDirection =
                    (TraceStart - TargetLocation).GetSafeNormal();

                PlayerTarget->NotifyIncomingRound(
                    SourceDirection,
                    Intensity
                );
            }

            if (ABHEnemySoldier* SoldierTarget =
                Cast<ABHEnemySoldier>(TargetActor);
                IsValid(SoldierTarget) &&
                IsHostileTo(SoldierTarget))
            {
                SoldierTarget->ApplySuppression(
                    FMath::Clamp(
                        NearMissSuppressionAmount,
                        0.0f,
                        1.0f
                    ) * Intensity,
                    this
                );
            }
        }
    }

    float DamageApplied = 0.0f;

    if (IsValid(DamageTarget))
    {
        float DamageToApply = FMath::Max(0.0f, ShotDamage);
        EBHPlayerHitZone PlayerHitZone =
            EBHPlayerHitZone::Torso;
        ABHCharacter* HitPlayer =
            Cast<ABHCharacter>(DamageTarget);

        if (IsValid(HitPlayer))
        {
            DamageToApply =
                HitPlayer->CalculateIncomingBallisticDamage(
                    HitResult,
                    DamageToApply,
                    PlayerHitZone
                );
        }

        DamageApplied = UGameplayStatics::ApplyPointDamage(
            DamageTarget,
            DamageToApply,
            ShotDirection,
            HitResult,
            GetController(),
            this,
            UDamageType::StaticClass()
        );

        if (DamageApplied > 0.0f && IsValid(HitPlayer))
        {
            HitPlayer->RegisterIncomingBallisticHit(
                PlayerHitZone,
                DamageApplied,
                this
            );
        }
    }

    if (bEnableDebug)
    {
        UE_LOG(
            LogTemp,
            Log,
            TEXT(
                "%s fired at %s: hit actor=%s, "
                "damage target=%s, damage applied=%.1f"
            ),
            *GetName(),
            *GetNameSafe(TargetActor),
            *GetNameSafe(HitActor),
            *GetNameSafe(DamageTarget),
            DamageApplied
        );
    }

    return true;
}

bool ABHEnemySoldier::HasClearLineOfFireTo(
    const FVector& TraceStart,
    const FVector& TargetLocation,
    AActor* TargetActor
) const
{
    UWorld* World = GetWorld();

    if (!IsValid(World) || !IsValid(TargetActor))
    {
        return false;
    }

    FCollisionQueryParams QueryParams(
        SCENE_QUERY_STAT(BHEnemyLineOfFire),
        true,
        this
    );
    QueryParams.AddIgnoredActor(this);

    FHitResult ObstructionHit;
    const bool bBlocked = World->LineTraceSingleByChannel(
        ObstructionHit,
        TraceStart,
        TargetLocation,
        EnemyWeaponTraceChannel,
        QueryParams
    );

    if (!bBlocked ||
        IsHitPartOfTarget(
            ObstructionHit.GetActor(),
            TargetActor
        ))
    {
        return true;
    }

    if (bEnableDebug)
    {
        UE_LOG(
            LogTemp,
            Log,
            TEXT(
                "BH_AI_FIRE_BLOCKED soldier=%s blocker=%s "
                "target=%s"
            ),
            *GetName(),
            *GetNameSafe(ObstructionHit.GetActor()),
            *GetNameSafe(TargetActor)
        );
    }

    return false;
}

bool ABHEnemySoldier::UpdateReloadState(float CurrentTime)
{
    if (!bReloading)
    {
        return true;
    }

    if (CurrentTime < ReloadEndTime)
    {
        return false;
    }

    const int32 RoundsLoaded = FMath::Min(
        GetMagazineCapacity(),
        GetCurrentReserveAmmo()
    );
    CurrentMagazineAmmo = RoundsLoaded;
    CurrentReserveAmmo = FMath::Max(
        0,
        CurrentReserveAmmo - RoundsLoaded
    );
    bReloading = false;
    ReloadEndTime = -BIG_NUMBER;

    UE_LOG(
        LogTemp,
        Display,
        TEXT(
            "BH_AI_RELOAD_COMPLETE soldier=%s magazine=%d "
            "reserve=%d"
        ),
        *GetName(),
        CurrentMagazineAmmo,
        CurrentReserveAmmo
    );
    return true;
}

void ABHEnemySoldier::BeginReload(float CurrentTime)
{
    if (bReloading || IsDead() || CurrentMagazineAmmo > 0)
    {
        return;
    }

    if (GetCurrentReserveAmmo() <= 0)
    {
        if (!bOutOfAmmoReported)
        {
            bOutOfAmmoReported = true;
            UE_LOG(
                LogTemp,
                Display,
                TEXT(
                    "BH_AI_OUT_OF_AMMO soldier=%s faction=%d"
                ),
                *GetName(),
                static_cast<int32>(CombatFaction)
            );
        }
        return;
    }

    bReloading = true;
    ReloadEndTime = CurrentTime + GetReloadDuration();
    PlayBark(EBHEnemyBarkType::Reload);
    OnEnemyReloadStarted();

    UE_LOG(
        LogTemp,
        Display,
        TEXT(
            "BH_AI_RELOAD_STARTED soldier=%s duration=%.2f "
            "reserve=%d"
        ),
        *GetName(),
        GetReloadDuration(),
        CurrentReserveAmmo
    );
}

ABHAmmoSupply* ABHEnemySoldier::DropRemainingAmmunition()
{
    if (bAmmoDropSpawned ||
        !bDropAmmoOnDeath ||
        CombatFaction != EBHCombatFaction::Hostile ||
        !BattlefieldAmmoSupplyClass)
    {
        return nullptr;
    }

    const int32 RemainingRounds =
        GetCurrentMagazineAmmo() + GetCurrentReserveAmmo();
    const int32 DroppedRounds = FMath::Min(
        FMath::Max(1, MaximumDroppedAmmo),
        RemainingRounds
    );
    UWorld* World = GetWorld();

    if (DroppedRounds <= 0 || !IsValid(World))
    {
        return nullptr;
    }

    const FTransform DropTransform(
        FRotator::ZeroRotator,
        GetActorLocation() + FVector(0.0f, 0.0f, 12.0f)
    );
    ABHAmmoSupply* AmmoDrop =
        World->SpawnActorDeferred<ABHAmmoSupply>(
            BattlefieldAmmoSupplyClass,
            DropTransform,
            nullptr,
            nullptr,
            ESpawnActorCollisionHandlingMethod::AlwaysSpawn
        );

    if (!IsValid(AmmoDrop))
    {
        return nullptr;
    }

    AmmoDrop->ConfigureRuntimePickup(DroppedRounds);
    UGameplayStatics::FinishSpawningActor(
        AmmoDrop,
        DropTransform
    );

    UE_LOG(
        LogTemp,
        Display,
        TEXT(
            "BH_AI_AMMO_DROPPED soldier=%s rounds=%d pickup=%s"
        ),
        *GetName(),
        DroppedRounds,
        *AmmoDrop->GetName()
    );
    bAmmoDropSpawned = true;
    return AmmoDrop;
}

void ABHEnemySoldier::ApplySuppression(
    float Amount,
    AActor* SourceActor
)
{
    if (IsDead() || Amount <= 0.0f)
    {
        return;
    }

    if (ABHEnemyAIController* EnemyController =
        Cast<ABHEnemyAIController>(GetController()))
    {
        EnemyController->NotifySuppressed(SourceActor, Amount);
    }
}

void ABHEnemySoldier::HandleDamaged(
    float DamageApplied,
    AActor* DamageCauser
)
{
    if (!IsValid(HealthComponent) || DamageApplied <= 0.0f)
    {
        return;
    }

    const ABHEnemySoldier* AttackingSoldier =
        Cast<ABHEnemySoldier>(DamageCauser);

    if (!bLoggedFactionDamage &&
        IsValid(AttackingSoldier) &&
        AttackingSoldier->GetCombatFaction() != CombatFaction)
    {
        bLoggedFactionDamage = true;
        UE_LOG(
            LogTemp,
            Display,
            TEXT(
                "BH_AI_FACTION_DAMAGE target=%s "
                "target_faction=%d attacker=%s damage=%.1f"
            ),
            *GetName(),
            static_cast<int32>(CombatFaction),
            *AttackingSoldier->GetName(),
            DamageApplied
        );
    }

    MulticastHitPresentation(DamageApplied, DamageCauser);

    if (ShouldNotifyControllerOfDamage(
            HealthComponent->GetCurrentHealth(),
            IsDead()))
    {
        if (ABHEnemyAIController* EnemyController =
            Cast<ABHEnemyAIController>(GetController()))
        {
            EnemyController->NotifyPawnDamaged(DamageCauser);
        }
    }
}

void ABHEnemySoldier::HandleDeath(AActor* DamageCauser)
{
    if (CombatFaction == EBHCombatFaction::Friendly &&
        bAllowFriendlyIncapacitation &&
        !bIncapacitated)
    {
        EnterFriendlyIncapacitation(
            DamageCauser,
            true
        );
        return;
    }

    if (bDeathHandled)
    {
        return;
    }

    if (bSurrendered)
    {
        ReportSurrenderConductOutcome(true);
    }

    bDeathHandled = true;
    PlayBark(EBHEnemyBarkType::Casualty);
    DropRemainingAmmunition();
    SetCanBeDamaged(false);
    GetCharacterMovement()->DisableMovement();
    GetCapsuleComponent()->SetCollisionEnabled(
        ECollisionEnabled::NoCollision
    );

    if (ABHEnemyAIController* EnemyController =
        Cast<ABHEnemyAIController>(GetController()))
    {
        EnemyController->HandlePawnDeath();
    }

    ApplyCasualtyMoraleShock(
        this,
        DamageCauser
    );
    if (!ObjectiveIdToCompleteOnDeath.IsNone())
    {
        bool bLivingObjectivePeerExists = false;

        for (TActorIterator<ABHEnemySoldier> It(GetWorld());
            It;
            ++It)
        {
            const ABHEnemySoldier* OtherEnemy = *It;

            if (IsValid(OtherEnemy) &&
                OtherEnemy != this &&
                !OtherEnemy->IsDead() &&
                OtherEnemy->ObjectiveIdToCompleteOnDeath ==
                    ObjectiveIdToCompleteOnDeath)
            {
                bLivingObjectivePeerExists = true;
                break;
            }
        }

        if (bLivingObjectivePeerExists)
        {
            MulticastDeathPresentation(
                DamageCauser,
                false
            );

            if (CorpseLifeSpan > 0.0f)
            {
                SetLifeSpan(CorpseLifeSpan);
            }
            return;
        }

        ABHCharacter* PlayerCharacter =
            BHPlayerResolver::Find(this);

        if (!IsValid(PlayerCharacter))
        {
            UE_LOG(
                LogTemp,
                Warning,
                TEXT(
                    "Enemy %s died but no BH player character "
                    "was available for objective %s."
                ),
                *GetPathName(),
                *ObjectiveIdToCompleteOnDeath.ToString()
            );
        }
        else
        {
            PlayerCharacter->CompleteSharedObjective(
                ObjectiveIdToCompleteOnDeath
            );
        }
    }

    MulticastDeathPresentation(
        DamageCauser,
        false
    );

    if (CorpseLifeSpan > 0.0f)
    {
        SetLifeSpan(CorpseLifeSpan);
    }
}

void ABHEnemySoldier::EnterFriendlyIncapacitation(
    AActor* DamageCauser,
    bool bPlayPresentation
)
{
    if (bIncapacitated)
    {
        return;
    }

    bIncapacitated = true;
    bDeathHandled = true;
    CombatReadiness = FMath::Min(
        GetCombatReadiness(),
        FMath::Clamp(
            PostCasualtyCombatReadiness,
            0.0f,
            1.0f
        )
    );
    IncapacitationSecondsRemaining =
        FMath::Max(1.0f, IncapacitationDuration);
    SetLifeSpan(0.0f);
    SetCanBeDamaged(false);
    GetCharacterMovement()->DisableMovement();

    if (UCapsuleComponent* CollisionCapsule =
            GetCapsuleComponent())
    {
        CollisionCapsule->SetCollisionEnabled(
            ECollisionEnabled::QueryOnly
        );
        CollisionCapsule->SetCollisionResponseToChannel(
            ECC_Visibility,
            ECR_Block
        );
    }

    if (ABHEnemyAIController* EnemyController =
        Cast<ABHEnemyAIController>(GetController()))
    {
        EnemyController->HandlePawnDeath();
    }
    ApplyCasualtyMoraleShock(
        this,
        DamageCauser
    );


    if (bPlayPresentation)
    {
        MulticastDeathPresentation(
            DamageCauser,
            true
        );
    }

    StartIncapacitationTimer();
    ForceNetUpdate();

    UE_LOG(
        LogTemp,
        Display,
        TEXT(
            "BH_FIELD_OPERATIVE_INCAPACITATED soldier=%s causer=%s"
        ),
        *GetName(),
        *GetNameSafe(DamageCauser)
    );
}

void ABHEnemySoldier::StartIncapacitationTimer()
{
    GetWorldTimerManager().ClearTimer(
        IncapacitationTimerHandle
    );

    if (!bIncapacitated ||
        IncapacitationSecondsRemaining <= 0.0f)
    {
        return;
    }

    GetWorldTimerManager().SetTimer(
        IncapacitationTimerHandle,
        this,
        &ABHEnemySoldier::UpdateIncapacitationTimer,
        1.0f,
        true
    );
}

void ABHEnemySoldier::UpdateIncapacitationTimer()
{
    if (!bIncapacitated)
    {
        GetWorldTimerManager().ClearTimer(
            IncapacitationTimerHandle
        );
        return;
    }

    IncapacitationSecondsRemaining = FMath::Max(
        0.0f,
        IncapacitationSecondsRemaining - 1.0f
    );
    ForceNetUpdate();

    if (IncapacitationSecondsRemaining <= 0.0f)
    {
        ExpireFriendlyIncapacitation();
    }
}

void ABHEnemySoldier::ExpireFriendlyIncapacitation()
{
    if (!bIncapacitated)
    {
        return;
    }

    bIncapacitated = false;
    bDeathHandled = true;
    IncapacitationSecondsRemaining = 0.0f;
    GetWorldTimerManager().ClearTimer(
        IncapacitationTimerHandle
    );
    GetCapsuleComponent()->SetCollisionEnabled(
        ECollisionEnabled::NoCollision
    );

    UE_LOG(
        LogTemp,
        Warning,
        TEXT("BH_FIELD_OPERATIVE_EXPIRED soldier=%s"),
        *GetName()
    );

    OnFriendlyCasualtyExpired.Broadcast(this);
    ForceNetUpdate();

    if (CorpseLifeSpan > 0.0f)
    {
        SetLifeSpan(CorpseLifeSpan);
    }
}

void ABHEnemySoldier::RestoreFromDeathPresentation()
{
    GetWorldTimerManager().ClearTimer(
        DeathRagdollTimerHandle
    );

    USkeletalMeshComponent* CharacterMesh = GetMesh();

    if (!IsValid(CharacterMesh))
    {
        return;
    }

    CharacterMesh->SetSimulatePhysics(false);
    CharacterMesh->SetAllBodiesSimulatePhysics(false);
    CharacterMesh->bBlendPhysics = false;
    CharacterMesh->AttachToComponent(
        GetCapsuleComponent(),
        FAttachmentTransformRules::KeepRelativeTransform
    );
    CharacterMesh->SetRelativeTransform(
        InitialMeshRelativeTransform
    );
    CharacterMesh->SetCollisionEnabled(
        ECollisionEnabled::QueryOnly
    );
    CharacterMesh->SetCollisionResponseToChannel(
        ECC_Visibility,
        ECR_Block
    );
}

void ABHEnemySoldier::PlayHitReaction()
{
    UWorld* World = GetWorld();
    UAnimInstance* AnimInstance = IsValid(GetMesh())
        ? GetMesh()->GetAnimInstance()
        : nullptr;

    if (!IsValid(World) ||
        !IsValid(AnimInstance) ||
        HitReactionAnimations.IsEmpty() ||
        (World->GetTimeSeconds() - LastHitReactionTime) <
            FMath::Max(0.0f, MinimumHitReactionInterval))
    {
        return;
    }

    TArray<UAnimSequenceBase*> ValidReactions;
    ValidReactions.Reserve(HitReactionAnimations.Num());

    for (UAnimSequenceBase* Reaction : HitReactionAnimations)
    {
        if (IsValid(Reaction))
        {
            ValidReactions.Add(Reaction);
        }
    }

    if (ValidReactions.IsEmpty())
    {
        return;
    }

    UAnimSequenceBase* Reaction = ValidReactions[
        FMath::RandRange(0, ValidReactions.Num() - 1)
    ];

    AnimInstance->PlaySlotAnimationAsDynamicMontage(
        Reaction,
        ReactionSlotName,
        0.04f,
        0.12f,
        1.0f,
        1,
        -1.0f,
        0.0f
    );
    LastHitReactionTime = World->GetTimeSeconds();
}

void ABHEnemySoldier::PlayDeathReaction(AActor* DamageCauser)
{
    LastDamageCauser = DamageCauser;

    UAnimInstance* AnimInstance = IsValid(GetMesh())
        ? GetMesh()->GetAnimInstance()
        : nullptr;

    if (IsValid(DeathAnimation) && IsValid(AnimInstance))
    {
        AnimInstance->PlaySlotAnimationAsDynamicMontage(
            DeathAnimation,
            ReactionSlotName,
            0.03f,
            0.15f,
            1.0f,
            1,
            -1.0f,
            0.0f
        );
    }

    if (!bEnableRagdollOnDeath)
    {
        if (IsValid(GetMesh()))
        {
            GetMesh()->SetCollisionEnabled(
                ECollisionEnabled::NoCollision
            );
        }
        return;
    }

    if (DeathRagdollDelay <= 0.0f)
    {
        EnableDeathRagdoll();
        return;
    }

    GetWorldTimerManager().SetTimer(
        DeathRagdollTimerHandle,
        this,
        &ABHEnemySoldier::EnableDeathRagdoll,
        DeathRagdollDelay,
        false
    );
}

void ABHEnemySoldier::EnableDeathRagdoll()
{
    USkeletalMeshComponent* CharacterMesh = GetMesh();

    if (!IsValid(CharacterMesh) ||
        !IsValid(CharacterMesh->GetPhysicsAsset()))
    {
        if (IsValid(CharacterMesh))
        {
            CharacterMesh->SetCollisionEnabled(
                ECollisionEnabled::NoCollision
            );
        }
        return;
    }

    CharacterMesh->SetCollisionProfileName(TEXT("Ragdoll"));
    CharacterMesh->SetCollisionEnabled(
        ECollisionEnabled::QueryAndPhysics
    );
    CharacterMesh->SetAllBodiesSimulatePhysics(true);
    CharacterMesh->SetSimulatePhysics(true);
    CharacterMesh->WakeAllRigidBodies();
    CharacterMesh->bBlendPhysics = true;

    FVector ImpulseDirection = -GetActorForwardVector();

    if (LastDamageCauser.IsValid())
    {
        ImpulseDirection = (
            GetActorLocation() -
            LastDamageCauser->GetActorLocation()
        ).GetSafeNormal();
    }

    ImpulseDirection.Z = FMath::Max(0.15f, ImpulseDirection.Z);
    ImpulseDirection.Normalize();

    CharacterMesh->AddImpulse(
        ImpulseDirection * FMath::Max(0.0f, DeathImpulse),
        NAME_None,
        true
    );
}

FTransform ABHEnemySoldier::GetPresentationMuzzleTransform() const
{
    if (IsValid(GetMesh()) &&
        !MuzzleSocketName.IsNone() &&
        GetMesh()->DoesSocketExist(MuzzleSocketName))
    {
        return GetMesh()->GetSocketTransform(MuzzleSocketName);
    }

    if (IsValid(MuzzlePoint))
    {
        return MuzzlePoint->GetComponentTransform();
    }

    return GetActorTransform();
}

void ABHEnemySoldier::MulticastFirePresentation_Implementation(
    AActor* TargetActor,
    const FHitResult& HitResult,
    bool bPlayImpact
)
{
    if (GetNetMode() == NM_DedicatedServer)
    {
        return;
    }

    PlayFirePresentation(TargetActor);

    if (bPlayImpact)
    {
        PlayImpactPresentation(HitResult);
    }
}

void ABHEnemySoldier::MulticastHitPresentation_Implementation(
    float DamageApplied,
    AActor* DamageCauser
)
{
    if (GetNetMode() == NM_DedicatedServer)
    {
        return;
    }

    OnEnemyHitCosmetics(DamageApplied, DamageCauser);
    PlayHitReaction();
}

void ABHEnemySoldier::MulticastDeathPresentation_Implementation(
    AActor* DamageCauser,
    bool bFriendlyIncapacitation
)
{
    if (GetNetMode() == NM_DedicatedServer)
    {
        return;
    }

    if (!HasAuthority())
    {
        bDeathHandled = true;
        SetCanBeDamaged(false);
        GetCharacterMovement()->DisableMovement();

        if (UCapsuleComponent* CollisionCapsule =
                GetCapsuleComponent())
        {
            CollisionCapsule->SetCollisionEnabled(
                bFriendlyIncapacitation
                    ? ECollisionEnabled::QueryOnly
                    : ECollisionEnabled::NoCollision
            );
        }
    }

    OnEnemyDeathCosmetics(DamageCauser);
    PlayDeathReaction(DamageCauser);
}

void ABHEnemySoldier::
    MulticastRestoreFromIncapacitation_Implementation()
{
    if (HasAuthority() ||
        GetNetMode() == NM_DedicatedServer)
    {
        return;
    }

    bDeathHandled = false;
    SetCanBeDamaged(true);
    RestoreFromDeathPresentation();

    if (UCapsuleComponent* CollisionCapsule =
            GetCapsuleComponent())
    {
        CollisionCapsule->SetCollisionEnabled(
            ECollisionEnabled::QueryAndPhysics
        );
        CollisionCapsule->SetCollisionResponseToChannel(
            ECC_Visibility,
            ECR_Ignore
        );
    }

    GetCharacterMovement()->SetMovementMode(MOVE_Walking);
    GetCharacterMovement()->MaxWalkSpeed =
        GetNormalMovementSpeed();
}

bool ABHEnemySoldier::CanPlayBark(
    float CurrentTime,
    float LastBarkTime,
    float MinimumInterval
)
{
    return CurrentTime >= 0.0f &&
        CurrentTime - LastBarkTime >= FMath::Max(0.0f, MinimumInterval);
}

bool ABHEnemySoldier::PlayBark(EBHEnemyBarkType BarkType)
{
    UWorld* World = GetWorld();
    if (!HasAuthority() || !IsValid(World) ||
        !IsValid(ResolveBarkSound(BarkType)) ||
        (BarkType != EBHEnemyBarkType::Casualty &&
            !CanPlayBark(
                World->GetTimeSeconds(),
                LastBarkTime,
                MinimumBarkInterval
            )))
    {
        return false;
    }
    LastBarkTime = World->GetTimeSeconds();
    MulticastPlayBark(BarkType);
    return true;
}

USoundBase* ABHEnemySoldier::ResolveBarkSound(
    EBHEnemyBarkType BarkType
) const
{
    switch (BarkType)
    {
        case EBHEnemyBarkType::Alert: return AlertBark;
        case EBHEnemyBarkType::Contact: return ContactBark;
        case EBHEnemyBarkType::Reload: return ReloadBark;
        case EBHEnemyBarkType::Grenade: return GrenadeBark;
        case EBHEnemyBarkType::Casualty: return CasualtyBark;
        case EBHEnemyBarkType::Retreat: return RetreatBark;
        case EBHEnemyBarkType::Search: return SearchBark;
        case EBHEnemyBarkType::Surrender: return SurrenderBark;
        default: return nullptr;
    }
}

void ABHEnemySoldier::MulticastPlayBark_Implementation(
    EBHEnemyBarkType BarkType
)
{
    if (GetNetMode() == NM_DedicatedServer) return;
    if (USoundBase* Bark = ResolveBarkSound(BarkType))
    {
        UGameplayStatics::PlaySoundAtLocation(
            this,
            Bark,
            GetActorLocation()
        );
    }
}

void ABHEnemySoldier::PlayFirePresentation(AActor* TargetActor)
{
    const FTransform MuzzleTransform =
        GetPresentationMuzzleTransform();

    if (IsValid(MuzzleFlashEffect))
    {
        UNiagaraFunctionLibrary::SpawnSystemAtLocation(
            this,
            MuzzleFlashEffect,
            MuzzleTransform.GetLocation(),
            MuzzleTransform.Rotator()
        );
    }

    if (IsValid(FireSound))
    {
        UGameplayStatics::PlaySoundAtLocation(
            this,
            FireSound,
            MuzzleTransform.GetLocation()
        );
    }

    USoundBase* FireTail = IsMuzzleEnvironmentEnclosed(
        MuzzleTransform.GetLocation()
    )
        ? IndoorFireTailSound.Get()
        : OutdoorFireTailSound.Get();
    if (IsValid(FireTail))
    {
        UGameplayStatics::PlaySoundAtLocation(
            this,
            FireTail,
            MuzzleTransform.GetLocation()
        );
    }

    UAnimInstance* AnimInstance = IsValid(GetMesh())
        ? GetMesh()->GetAnimInstance()
        : nullptr;

    if (IsValid(FireMontage) && IsValid(AnimInstance))
    {
        AnimInstance->Montage_Play(FireMontage);
    }

    OnEnemyFireCosmetics(TargetActor);
}

bool ABHEnemySoldier::IsMuzzleEnvironmentEnclosed(
    const FVector& MuzzleLocation
) const
{
    const UWorld* World = GetWorld();
    if (!IsValid(World))
    {
        return false;
    }
    const FVector ProbeDirections[] = {
        FVector::UpVector,
        FVector::ForwardVector,
        -FVector::ForwardVector,
        FVector::RightVector,
        -FVector::RightVector
    };
    int32 BlockedProbeCount = 0;
    FCollisionQueryParams QueryParams(
        SCENE_QUERY_STAT(BHEnemyAudioEnvironment),
        false,
        this
    );
    for (const FVector& ProbeDirection : ProbeDirections)
    {
        FHitResult ProbeHit;
        const float ProbeDistance = ProbeDirection.Z > 0.5f
            ? 1200.0f
            : 800.0f;
        if (World->LineTraceSingleByChannel(
                ProbeHit,
                MuzzleLocation,
                MuzzleLocation + ProbeDirection * ProbeDistance,
                ECC_Visibility,
                QueryParams
            ))
        {
            ++BlockedProbeCount;
        }
    }
    return ABHRifle::ShouldUseIndoorFireTail(
        BlockedProbeCount,
        UE_ARRAY_COUNT(ProbeDirections)
    );
}

void ABHEnemySoldier::PlayImpactPresentation(
    const FHitResult& HitResult
)
{
    if (!ImpactActorClass || !IsValid(GetWorld()))
    {
        return;
    }

    FActorSpawnParameters SpawnParameters;
    SpawnParameters.Owner = this;
    SpawnParameters.Instigator = this;
    SpawnParameters.SpawnCollisionHandlingOverride =
        ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    ABHImpactEffect* SpawnedImpact =
        GetWorld()->SpawnActor<ABHImpactEffect>(
            ImpactActorClass,
            HitResult.ImpactPoint,
            HitResult.ImpactNormal.Rotation(),
            SpawnParameters
        );

    if (IsValid(SpawnedImpact))
    {
        SpawnedImpact->InitializeImpact(HitResult);
    }
}

bool ABHEnemySoldier::IsHitPartOfTarget(
    AActor* HitActor,
    AActor* TargetActor
) const
{
    if (!IsValid(HitActor) || !IsValid(TargetActor))
    {
        return false;
    }

    AActor* Current = HitActor;

    for (int32 Depth = 0; IsValid(Current) && Depth < 4; ++Depth)
    {
        if (Current == TargetActor)
        {
            return true;
        }

        AActor* Next = Current->GetOwner();

        if (!IsValid(Next))
        {
            Next = Current->GetInstigator();
        }

        if (Next == Current)
        {
            break;
        }

        Current = Next;
    }

    return false;
}
