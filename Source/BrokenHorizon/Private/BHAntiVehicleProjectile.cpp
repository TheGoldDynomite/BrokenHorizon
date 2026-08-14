#include "BHAntiVehicleProjectile.h"

#include "BHArmoredThreat.h"
#include "BHCharacter.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "EngineUtils.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

#if !UE_BUILD_SHIPPING
namespace
{
static FAutoConsoleCommandWithWorldAndArgs GFireAntiVehicleTestCommand(
    TEXT("BHTestFireAntiVehicleProjectile"),
    TEXT("Fires the real anti-vehicle projectile at the first armored threat."),
    FConsoleCommandWithWorldAndArgsDelegate::CreateLambda(
        [](const TArray<FString>&, UWorld* World)
        {
            if (!IsValid(World))
            {
                return;
            }
            APlayerController* PC = World->GetFirstPlayerController();
            APawn* Pawn = IsValid(PC) ? PC->GetPawn() : nullptr;
            ABHArmoredThreat* Threat = nullptr;
            for (TActorIterator<ABHArmoredThreat> It(World); It; ++It)
            {
                if (IsValid(*It))
                {
                    Threat = *It;
                    break;
                }
            }
            if (!IsValid(Pawn) || !IsValid(Threat))
            {
                UE_LOG(LogTemp, Warning,
                    TEXT("BH_TEST_ANTI_VEHICLE_FIRE no_pawn_or_target"));
                return;
            }
            const FVector Origin = Pawn->GetActorLocation() +
                FVector(0.0f, 0.0f, 50.0f);
            const FVector Direction =
                (Threat->GetActorLocation() - Origin).GetSafeNormal();
            FActorSpawnParameters SpawnParameters;
            SpawnParameters.Owner = Pawn;
            SpawnParameters.Instigator = Pawn;
            SpawnParameters.SpawnCollisionHandlingOverride =
                ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
            ABHAntiVehicleProjectile* Projectile =
                World->SpawnActor<ABHAntiVehicleProjectile>(
                    ABHAntiVehicleProjectile::StaticClass(),
                    Origin,
                    Direction.Rotation(),
                    SpawnParameters);
            if (IsValid(Projectile))
            {
                Projectile->Launch(Direction, 2400.0f);
                UE_LOG(LogTemp, Display,
                    TEXT("BH_TEST_ANTI_VEHICLE_FIRE target=%s result=launched"),
                    *Threat->GetPersistenceID().ToString());
            }
        }
    )
);
}
#endif

ABHAntiVehicleProjectile::ABHAntiVehicleProjectile()
{
    bReplicates = true;
    SetReplicateMovement(true);

    Collision = CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
    SetRootComponent(Collision);
    Collision->InitSphereRadius(8.0f);
    Collision->SetCollisionProfileName(TEXT("Projectile"));

    ProjectileMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ProjectileMesh"));
    ProjectileMesh->SetupAttachment(Collision);
    ProjectileMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    ProjectileMesh->SetRelativeScale3D(FVector(0.18f, 0.05f, 0.05f));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> MeshAsset(
        TEXT("/Engine/BasicShapes/Cube.Cube"));
    if (MeshAsset.Succeeded())
    {
        ProjectileMesh->SetStaticMesh(MeshAsset.Object);
    }

    Movement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("Movement"));
    Movement->InitialSpeed = 2400.0f;
    Movement->MaxSpeed = 2400.0f;
    Movement->ProjectileGravityScale = 0.0f;
    Movement->bRotationFollowsVelocity = true;

    InitialLifeSpan = 8.0f;
}

void ABHAntiVehicleProjectile::BeginPlay()
{
    Super::BeginPlay();
    if (IsValid(Collision))
    {
        Collision->OnComponentHit.AddDynamic(this, &ABHAntiVehicleProjectile::HandleProjectileHit);
    }
}

void ABHAntiVehicleProjectile::Launch(const FVector& Direction, float Speed)
{
    if (IsValid(Movement))
    {
        Movement->Velocity = Direction.GetSafeNormal() * FMath::Max(1.0f, Speed);
    }
}

void ABHAntiVehicleProjectile::HandleProjectileHit(
    UPrimitiveComponent* HitComponent,
    AActor* OtherActor,
    UPrimitiveComponent* OtherComp,
    FVector NormalImpulse,
    const FHitResult& Hit)
{
    if (HasAuthority())
    {
        if (ABHArmoredThreat* Threat = Cast<ABHArmoredThreat>(OtherActor))
        {
            const float AppliedDamage = Threat->ApplyAntiVehicleDamage(
                AntiVehicleDamage, Hit.ImpactPoint, this);
            if (ABHCharacter* Shooter = Cast<ABHCharacter>(GetOwner()))
            {
                Shooter->ShowStatusNotification(FText::Format(
                    NSLOCTEXT(
                        "BrokenHorizon",
                        "AntiVehicleImpact",
                        "ANTI-VEHICLE // IMPACT {0} // ARMOR {1}%"
                    ),
                    FText::AsNumber(FMath::RoundToInt(AppliedDamage)),
                    FText::AsNumber(FMath::RoundToInt(
                        Threat->GetArmorIntegrityFraction() * 100.0f))
                ));
            }
            UE_LOG(LogTemp, Display,
                TEXT(
                    "BH_ANTI_VEHICLE_PROJECTILE_HIT target=%s damage=%.1f "
                    "armor=%.3f disabled=%d"
                ),
                *Threat->GetPersistenceID().ToString(),
                AppliedDamage,
                Threat->GetArmorIntegrityFraction(),
                Threat->IsMobilityDisabled() ? 1 : 0);
        }
        Destroy();
    }
}
