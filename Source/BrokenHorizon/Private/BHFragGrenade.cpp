#include "BHFragGrenade.h"

#include "BHCharacter.h"
#include "BHEnemyAIController.h"
#include "BHEnemySoldier.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Engine/OverlapResult.h"
#include "GameFramework/DamageType.h"
#include "Kismet/GameplayStatics.h"
#include "Perception/AISense_Hearing.h"
#include "PhysicsEngine/RadialForceComponent.h"
#include "UObject/ConstructorHelpers.h"

ABHFragGrenade::ABHFragGrenade()
{
    PrimaryActorTick.bCanEverTick = false;
    bReplicates = true;
    SetReplicateMovement(true);

    CollisionRoot = CreateDefaultSubobject<USphereComponent>(
        TEXT("CollisionRoot")
    );
    SetRootComponent(CollisionRoot);
    CollisionRoot->InitSphereRadius(7.5f);
    CollisionRoot->SetCollisionProfileName(
        UCollisionProfile::PhysicsActor_ProfileName
    );
    CollisionRoot->SetCollisionResponseToChannel(
        ECC_Pawn,
        ECR_Ignore
    );
    CollisionRoot->SetCanEverAffectNavigation(false);
    CollisionRoot->SetSimulatePhysics(true);
    CollisionRoot->SetEnableGravity(true);
    CollisionRoot->SetLinearDamping(0.25f);
    CollisionRoot->SetAngularDamping(0.20f);

    GrenadeMesh = CreateDefaultSubobject<UStaticMeshComponent>(
        TEXT("GrenadeMesh")
    );
    GrenadeMesh->SetupAttachment(CollisionRoot);
    GrenadeMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    GrenadeMesh->SetCanEverAffectNavigation(false);
    GrenadeMesh->SetRelativeScale3D(FVector(0.15f));
    GrenadeMesh->SetCastShadow(true);

    static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(
        TEXT("/Engine/BasicShapes/Sphere.Sphere")
    );

    if (SphereMesh.Succeeded())
    {
        GrenadeMesh->SetStaticMesh(SphereMesh.Object);
    }

    BlastImpulse = CreateDefaultSubobject<URadialForceComponent>(
        TEXT("BlastImpulse")
    );
    BlastImpulse->SetupAttachment(CollisionRoot);
    BlastImpulse->Radius = OuterDamageRadius;
    BlastImpulse->ImpulseStrength = 1800.0f;
    BlastImpulse->Falloff = RIF_Linear;
    BlastImpulse->bImpulseVelChange = true;
    BlastImpulse->SetAutoActivate(false);
}

void ABHFragGrenade::BeginPlay()
{
    Super::BeginPlay();

    if (IsValid(CollisionRoot))
    {
        CollisionRoot->IgnoreActorWhenMoving(GetOwner(), true);
        CollisionRoot->IgnoreActorWhenMoving(GetInstigator(), true);
    }

    GetWorldTimerManager().SetTimer(
        FuseTimerHandle,
        this,
        &ABHFragGrenade::Explode,
        FMath::Max(0.1f, FuseDuration),
        false
    );
    GetWorldTimerManager().SetTimer(
        ThreatWarningTimerHandle,
        this,
        &ABHFragGrenade::UpdateThreatWarnings,
        0.15f,
        true
    );
    UpdateThreatWarnings();
    SetLifeSpan(FMath::Max(0.1f, FuseDuration) + 2.0f);
}

void ABHFragGrenade::Throw(
    const FVector& InitialVelocity,
    float CookDuration
)
{
    if (!IsValid(CollisionRoot))
    {
        return;
    }

    CollisionRoot->SetPhysicsLinearVelocity(InitialVelocity);
    CollisionRoot->SetPhysicsAngularVelocityInDegrees(
        FVector(420.0f, 260.0f, 180.0f)
    );

    if (CookDuration > KINDA_SMALL_NUMBER)
    {
        const float SafeFuseDuration = GetFuseDurationAfterCook(
            CookDuration
        );
        GetWorldTimerManager().ClearTimer(FuseTimerHandle);
        GetWorldTimerManager().SetTimer(
            FuseTimerHandle,
            this,
            &ABHFragGrenade::Explode,
            FMath::Max(0.1f, SafeFuseDuration),
            false
        );
        SetLifeSpan(FMath::Max(0.1f, SafeFuseDuration) + 2.0f);
    }
}

float ABHFragGrenade::GetFuseDuration() const
{
    return FuseDuration;
}

float ABHFragGrenade::GetFuseDurationAfterCook(float CookDuration) const
{
    const float ClampedFuseDuration = FMath::Max(0.0f, FuseDuration);
    const float ClampedCookDuration = FMath::Clamp(
        CookDuration,
        0.0f,
        ClampedFuseDuration
    );
    return FMath::Max(
        FMath::Max(0.0f, MinimumRemainingFuse),
        ClampedFuseDuration - ClampedCookDuration
    );
}

void ABHFragGrenade::UpdateThreatWarnings()
{
    UWorld* World = GetWorld();

    if (bHasExploded || !IsValid(World))
    {
        return;
    }

    const float TimeUntilDetonation = FMath::Max(
        0.0f,
        GetWorldTimerManager().GetTimerRemaining(
            FuseTimerHandle
        )
    );

    if (TimeUntilDetonation >
        FMath::Max(0.1f, ThreatWarningLeadTime))
    {
        return;
    }

    TArray<FOverlapResult> Overlaps;
    FCollisionObjectQueryParams ObjectQuery;
    ObjectQuery.AddObjectTypesToQuery(ECC_Pawn);
    FCollisionQueryParams QueryParams(
        SCENE_QUERY_STAT(BHFragGrenadeThreat),
        false,
        this
    );
    const float WarningRadius =
        FMath::Max(0.0f, OuterDamageRadius) + 250.0f;

    if (!World->OverlapMultiByObjectType(
            Overlaps,
            GetActorLocation(),
            FQuat::Identity,
            ObjectQuery,
            FCollisionShape::MakeSphere(WarningRadius),
            QueryParams
        ))
    {
        return;
    }

    const ABHEnemySoldier* GrenadeInstigator =
        Cast<ABHEnemySoldier>(GetInstigator());

    for (const FOverlapResult& Overlap : Overlaps)
    {
        if (ABHCharacter* PlayerCharacter =
            Cast<ABHCharacter>(Overlap.GetActor()))
        {
            if (IsValid(GrenadeInstigator) &&
                PlayerCharacter->IsPlayerControlled() &&
                GrenadeInstigator->IsHostileTo(PlayerCharacter))
            {
                PlayerCharacter->NotifyGrenadeThreat(
                    this,
                    GetActorLocation(),
                    TimeUntilDetonation
                );
            }

            continue;
        }

        ABHEnemySoldier* Soldier =
            Cast<ABHEnemySoldier>(Overlap.GetActor());

        if (!IsValid(Soldier) || Soldier->IsDead())
        {
            continue;
        }

        ABHEnemyAIController* Controller =
            Cast<ABHEnemyAIController>(Soldier->GetController());

        if (!IsValid(Controller) ||
            AlertedControllers.Contains(Controller))
        {
            continue;
        }

        if (Controller->NotifyGrenadeThreat(
            GetActorLocation(),
            OuterDamageRadius,
            TimeUntilDetonation
        ))
        {
            AlertedControllers.Add(Controller);
        }
    }
}

void ABHFragGrenade::Explode()
{
    if (bHasExploded || !IsValid(GetWorld()))
    {
        return;
    }

    bHasExploded = true;
    GetWorldTimerManager().ClearTimer(FuseTimerHandle);
    GetWorldTimerManager().ClearTimer(ThreatWarningTimerHandle);

    const FVector BlastOrigin = GetActorLocation();
    TArray<AActor*> IgnoredActors;
    IgnoredActors.Add(this);

    SetActorEnableCollision(false);

    if (IsValid(GrenadeMesh))
    {
        GrenadeMesh->SetVisibility(false, true);
    }

    if (IsValid(BlastImpulse))
    {
        BlastImpulse->Radius = FMath::Max(
            InnerDamageRadius,
            OuterDamageRadius
        );
        BlastImpulse->FireImpulse();
    }

    UGameplayStatics::ApplyRadialDamageWithFalloff(
        this,
        FMath::Max(0.0f, MaximumDamage),
        FMath::Max(0.0f, MinimumDamage),
        BlastOrigin,
        FMath::Max(0.0f, InnerDamageRadius),
        FMath::Max(InnerDamageRadius, OuterDamageRadius),
        FMath::Max(0.01f, DamageFalloff),
        UDamageType::StaticClass(),
        IgnoredActors,
        this,
        GetInstigatorController(),
        ECC_Visibility
    );

    UAISense_Hearing::ReportNoiseEvent(
        this,
        BlastOrigin,
        2.0f,
        GetInstigator(),
        FMath::Max(0.0f, ExplosionNoiseRange),
        FName(TEXT("Explosion"))
    );

    UE_LOG(
        LogTemp,
        Display,
        TEXT(
            "BH_FRAG_EXPLODED location=%s radius=%.0f "
            "maximum_damage=%.0f"
        ),
        *BlastOrigin.ToCompactString(),
        OuterDamageRadius,
        MaximumDamage
    );

    Destroy();
}
