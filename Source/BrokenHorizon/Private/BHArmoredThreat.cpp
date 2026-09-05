#include "BHArmoredThreat.h"

#include "BHCharacter.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "GameFramework/DamageType.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "HAL/IConsoleManager.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

#if !UE_BUILD_SHIPPING
namespace
{
static FAutoConsoleCommandWithWorldAndArgs GSpawnArmoredThreatCommand(
    TEXT("BHTestSpawnArmoredThreat"),
    TEXT("Spawns a deterministic armored threat in front of the local player."),
    FConsoleCommandWithWorldAndArgsDelegate::CreateLambda(
        [](const TArray<FString>& Args, UWorld* World)
        {
            APlayerController* PC = IsValid(World)
                ? World->GetFirstPlayerController()
                : nullptr;
            APawn* Pawn = IsValid(PC) ? PC->GetPawn() : nullptr;
            if (!IsValid(World) || !IsValid(Pawn))
            {
                return;
            }
            const FVector Location = Pawn->GetActorLocation() +
                Pawn->GetActorForwardVector() * 700.0f + FVector(0.0f, 0.0f, 90.0f);
            ABHArmoredThreat* Threat = World->SpawnActor<ABHArmoredThreat>(
                ABHArmoredThreat::StaticClass(), Location, FRotator::ZeroRotator);
            if (IsValid(Threat))
            {
                UE_LOG(LogTemp, Display,
                    TEXT("BH_TEST_ARMORED_SPAWNED id=%s"),
                    *Threat->GetPersistenceID().ToString());
            }
        }
    )
);

static FAutoConsoleCommandWithWorldAndArgs GDamageArmoredThreatCommand(
    TEXT("BHTestDamageArmoredThreat"),
    TEXT("Applies a frontal and rear anti-vehicle test hit to the first armored threat."),
    FConsoleCommandWithWorldAndArgsDelegate::CreateLambda(
        [](const TArray<FString>& Args, UWorld* World)
        {
            if (!IsValid(World))
            {
                return;
            }
            for (TActorIterator<ABHArmoredThreat> It(World); It; ++It)
            {
                ABHArmoredThreat* Threat = *It;
                if (!IsValid(Threat))
                {
                    continue;
                }
                const FVector Front = Threat->GetActorLocation() +
                    Threat->GetActorForwardVector() * 100.0f;
                const FVector Rear = Threat->GetActorLocation() -
                    Threat->GetActorForwardVector() * 100.0f;
                Threat->ApplyAntiVehicleDamage(100.0f, Front, nullptr);
                Threat->ApplyAntiVehicleDamage(100.0f, Rear, nullptr);
                int32 RepeatedRearHits = 0;
                while (!Threat->IsMobilityDisabled() && RepeatedRearHits < 10)
                {
                    Threat->ApplyAntiVehicleDamage(100.0f, Rear, nullptr);
                    ++RepeatedRearHits;
                }
                UE_LOG(LogTemp, Display,
                    TEXT("BH_TEST_ARMORED_DAMAGE_COMPLETE id=%s armor=%.3f disabled=%d rear_hits=%d"),
                    *Threat->GetPersistenceID().ToString(),
                    Threat->GetArmorIntegrityFraction(),
                    Threat->IsMobilityDisabled() ? 1 : 0,
                    RepeatedRearHits);
                return;
            }
            UE_LOG(LogTemp, Warning, TEXT("BH_TEST_ARMORED_DAMAGE no_target"));
        }
    )
);
}
#endif

ABHArmoredThreat::ABHArmoredThreat()
{
    bReplicates = true;
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickInterval = 0.25f;
    SetCanBeDamaged(true);

    ThreatMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ThreatMesh"));
    SetRootComponent(ThreatMesh);
    ThreatMesh->SetCollisionProfileName(TEXT("BlockAllDynamic"));

    static ConstructorHelpers::FObjectFinder<UStaticMesh> MeshAsset(
        TEXT("/Engine/BasicShapes/Cube.Cube"));
    if (MeshAsset.Succeeded())
    {
        ThreatMesh->SetStaticMesh(MeshAsset.Object);
        ThreatMesh->SetRelativeLocation(FVector(0.0f, 0.0f, 35.0f));
        ThreatMesh->SetRelativeScale3D(FVector(2.2f, 1.2f, 0.65f));

        TurretMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TurretMesh"));
        TurretMesh->SetupAttachment(ThreatMesh);
        TurretMesh->SetStaticMesh(MeshAsset.Object);
        TurretMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        TurretMesh->SetRelativeLocation(FVector(0.0f, 0.0f, 0.8f));
        TurretMesh->SetRelativeScale3D(FVector(0.85f, 0.65f, 0.38f));

        BarrelMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BarrelMesh"));
        BarrelMesh->SetupAttachment(TurretMesh);
        BarrelMesh->SetStaticMesh(MeshAsset.Object);
        BarrelMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        BarrelMesh->SetRelativeLocation(FVector(1.05f, 0.0f, 0.0f));
        BarrelMesh->SetRelativeScale3D(FVector(1.35f, 0.12f, 0.12f));

        static ConstructorHelpers::FObjectFinder<UMaterialInterface> MilitaryMaterial(
            TEXT("/Game/BrokenHorizon/Environment/WorldKit/Materials/M_BH_Military.M_BH_Military"));
        if (MilitaryMaterial.Succeeded())
        {
            ThreatMesh->SetMaterial(0, MilitaryMaterial.Object);
            TurretMesh->SetMaterial(0, MilitaryMaterial.Object);
            BarrelMesh->SetMaterial(0, MilitaryMaterial.Object);
        }
    }
}

void ABHArmoredThreat::BeginPlay()
{
    Super::BeginPlay();
    ArmorIntegrity = FMath::Clamp(ArmorIntegrity, 0.0f, MaximumArmorIntegrity);
    UE_LOG(
        LogTemp,
        Display,
        TEXT(
            "BH_ARMORED_THREAT_PRESENTATION id=%s hull=%d turret=%d barrel=%d"
        ),
        *PersistenceID.ToString(),
        IsValid(ThreatMesh) ? 1 : 0,
        IsValid(TurretMesh) ? 1 : 0,
        IsValid(BarrelMesh) ? 1 : 0
    );
    if (!HasAuthority())
    {
        UE_LOG(
            LogTemp,
            Display,
            TEXT(
                "BH_ARMORED_THREAT_STATE id=%s reason=replicated_begin_play "
                "armor=%.3f mobility=%.3f disabled=%d"
            ),
            *PersistenceID.ToString(),
            GetArmorIntegrityFraction(),
            GetMobilityFraction(),
            IsMobilityDisabled() ? 1 : 0
        );
    }
}

void ABHArmoredThreat::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (ABHCharacter* PresentedTarget =
            Cast<ABHCharacter>(LastPresentedTarget.Get()))
    {
        PresentedTarget->NotifyArmoredThreatContact(
            this,
            GetActorLocation(),
            false
        );
    }
    LastPresentedTarget.Reset();
    Super::EndPlay(EndPlayReason);
}

void ABHArmoredThreat::GetLifetimeReplicatedProps(
    TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ABHArmoredThreat, PersistenceID);
    DOREPLIFETIME(ABHArmoredThreat, MaximumArmorIntegrity);
    DOREPLIFETIME(ABHArmoredThreat, ArmorIntegrity);
    DOREPLIFETIME(ABHArmoredThreat, bHasPlayerContact);
    DOREPLIFETIME(ABHArmoredThreat, CurrentTarget);
}

void ABHArmoredThreat::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (!HasAuthority() || ArmorIntegrity <= 0.0f)
    {
        return;
    }

    WeaponCooldownRemaining = FMath::Max(0.0f, WeaponCooldownRemaining - DeltaSeconds);

    APawn* PlayerPawn = nullptr;
    float ClosestPlayerDistanceSquared = TNumericLimits<float>::Max();
    const float MaximumContactDistanceSquared = FMath::Square(
        FMath::Max(100.0f, ContactRange)
    );

    for (FConstPlayerControllerIterator Iterator =
             GetWorld()->GetPlayerControllerIterator();
         Iterator;
         ++Iterator)
    {
        APlayerController* PlayerController = Iterator->Get();
        APawn* CandidatePawn = IsValid(PlayerController)
            ? PlayerController->GetPawn()
            : nullptr;
        if (!IsValid(CandidatePawn))
        {
            continue;
        }

        const float CandidateDistanceSquared = FVector::DistSquared(
            GetActorLocation(), CandidatePawn->GetActorLocation()
        );
        if (CandidateDistanceSquared > MaximumContactDistanceSquared)
        {
            continue;
        }

        FHitResult Hit;
        FCollisionQueryParams Params(
            SCENE_QUERY_STAT(ArmoredThreatContact), true, this
        );
        Params.AddIgnoredActor(CandidatePawn);
        const bool bBlocked = GetWorld()->LineTraceSingleByChannel(
            Hit,
            GetActorLocation() + FVector(0.0f, 0.0f, 80.0f),
            CandidatePawn->GetActorLocation() + FVector(0.0f, 0.0f, 60.0f),
            ECC_Visibility,
            Params
        );
        if (bBlocked && Hit.GetActor() != CandidatePawn)
        {
            continue;
        }

        if (CandidateDistanceSquared < ClosestPlayerDistanceSquared)
        {
            PlayerPawn = CandidatePawn;
            ClosestPlayerDistanceSquared = CandidateDistanceSquared;
        }
    }

    const bool bVisible = IsValid(PlayerPawn);

    if (bVisible != bHasPlayerContact || (bVisible && CurrentTarget != PlayerPawn))
    {
        bHasPlayerContact = bVisible;
        CurrentTarget = bVisible ? PlayerPawn : nullptr;
        UpdateLocalContactPresentation();
        BroadcastThreatState(bVisible ? TEXT("player_contact") : TEXT("contact_lost"));
    }

    if (bHasPlayerContact && IsValid(CurrentTarget) && WeaponCooldownRemaining <= 0.0f)
    {
        const FVector MuzzleLocation = GetActorLocation() + FVector(0.0f, 0.0f, 80.0f);
        const FVector TargetLocation = CurrentTarget->GetActorLocation() + FVector(0.0f, 0.0f, 60.0f);
        FHitResult ShotHit;
        FCollisionQueryParams ShotParams(SCENE_QUERY_STAT(ArmoredThreatFire), true, this);
        const bool bBlocked = GetWorld()->LineTraceSingleByChannel(
            ShotHit, MuzzleLocation, TargetLocation, ECC_Visibility, ShotParams);
        if (!bBlocked || ShotHit.GetActor() == CurrentTarget)
        {
            const float AppliedDamage = UGameplayStatics::ApplyDamage(
                CurrentTarget, FMath::Max(0.0f, WeaponDamage),
                GetInstigatorController(), this, UDamageType::StaticClass());
            UE_LOG(LogTemp, Display,
                TEXT("BH_ARMORED_THREAT_FIRE id=%s target=%s damage=%.1f"),
                *PersistenceID.ToString(), *CurrentTarget->GetName(), AppliedDamage);
            WeaponCooldownRemaining = FMath::Max(0.1f, WeaponCooldown);
        }
    }
}

float ABHArmoredThreat::TakeDamage(
    float DamageAmount,
    FDamageEvent const& DamageEvent,
    AController* EventInstigator,
    AActor* DamageCauser)
{
    return ApplyAntiVehicleDamage(DamageAmount, GetActorLocation(), DamageCauser);
}

float ABHArmoredThreat::ApplyAntiVehicleDamage(
    float DamageAmount,
    FVector ImpactLocation,
    AActor* DamageCauser)
{
    if (!HasAuthority() || DamageAmount <= 0.0f || ArmorIntegrity <= 0.0f)
    {
        return 0.0f;
    }

    const FVector LocalImpact = GetActorTransform().InverseTransformPosition(ImpactLocation);
    const bool bRearImpact = LocalImpact.X < 0.0f;
    const float AppliedDamage = DamageAmount *
        (bRearImpact ? FMath::Clamp(RearDamageMultiplier, 0.0f, 1.0f)
                     : FMath::Clamp(FrontalDamageMultiplier, 0.0f, 1.0f));
    ArmorIntegrity = FMath::Max(0.0f, ArmorIntegrity - AppliedDamage);
    BroadcastThreatState(bRearImpact ? TEXT("rear_hit") : TEXT("frontal_hit"));
    return AppliedDamage;
}

float ABHArmoredThreat::GetArmorIntegrityFraction() const
{
    return MaximumArmorIntegrity > KINDA_SMALL_NUMBER
        ? FMath::Clamp(ArmorIntegrity / MaximumArmorIntegrity, 0.0f, 1.0f)
        : 0.0f;
}

float ABHArmoredThreat::GetMobilityFraction() const
{
    return FMath::Clamp(GetArmorIntegrityFraction() /
        FMath::Max(KINDA_SMALL_NUMBER, MobilityDisableFraction), 0.0f, 1.0f);
}

bool ABHArmoredThreat::IsMobilityDisabled() const
{
    return GetArmorIntegrityFraction() <= FMath::Clamp(MobilityDisableFraction, 0.0f, 1.0f);
}

bool ABHArmoredThreat::HasPlayerContact() const
{
    return bHasPlayerContact;
}

AActor* ABHArmoredThreat::GetCurrentTarget() const
{
    return CurrentTarget;
}

float ABHArmoredThreat::GetContactRange() const
{
    return FMath::Max(100.0f, ContactRange);
}

FName ABHArmoredThreat::GetPersistenceID() const
{
    return PersistenceID;
}

void ABHArmoredThreat::OnRep_ArmorIntegrity()
{
    BroadcastThreatState(TEXT("replicated_damage"));
}

void ABHArmoredThreat::OnRep_ContactState()
{
    UpdateLocalContactPresentation();
    BroadcastThreatState(bHasPlayerContact ? TEXT("replicated_contact") : TEXT("replicated_contact_lost"));
}

void ABHArmoredThreat::OnRep_CurrentTarget()
{
    UpdateLocalContactPresentation();
    if (bHasPlayerContact)
    {
        BroadcastThreatState(TEXT("replicated_target"));
    }
}

void ABHArmoredThreat::UpdateLocalContactPresentation()
{
    ABHCharacter* NewTarget = bHasPlayerContact
        ? Cast<ABHCharacter>(CurrentTarget)
        : nullptr;
    const bool bTargetChanged =
        LastPresentedTarget.Get() != NewTarget;

    if (bTargetChanged)
    {
        if (ABHCharacter* PreviousTarget =
                Cast<ABHCharacter>(LastPresentedTarget.Get()))
        {
            PreviousTarget->NotifyArmoredThreatContact(
                this,
                GetActorLocation(),
                false
            );
        }
        LastPresentedTarget.Reset();
    }

    if (!IsValid(NewTarget) || !NewTarget->IsLocallyControlled())
    {
        return;
    }

    if (!bTargetChanged)
    {
        return;
    }

    NewTarget->NotifyArmoredThreatContact(
        this,
        GetActorLocation(),
        true
    );
    LastPresentedTarget = NewTarget;
}

void ABHArmoredThreat::BroadcastThreatState(const TCHAR* Reason) const
{
    UE_LOG(LogTemp, Display,
        TEXT("BH_ARMORED_THREAT_STATE id=%s reason=%s target=%s armor=%.3f mobility=%.3f disabled=%d"),
        *PersistenceID.ToString(),
        Reason,
        IsValid(CurrentTarget) ? *CurrentTarget->GetName() : TEXT("None"),
        GetArmorIntegrityFraction(),
        GetMobilityFraction(),
        IsMobilityDisabled() ? 1 : 0);
}
