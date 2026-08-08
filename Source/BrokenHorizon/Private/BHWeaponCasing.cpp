#include "BHWeaponCasing.h"

#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"

ABHWeaponCasing::ABHWeaponCasing()
{
    PrimaryActorTick.bCanEverTick = false;
    SetReplicates(false);

    CollisionRoot = CreateDefaultSubobject<USphereComponent>(
        TEXT("CollisionRoot")
    );
    SetRootComponent(CollisionRoot);
    CollisionRoot->SetSphereRadius(1.25f);
    CollisionRoot->SetCollisionEnabled(
        ECollisionEnabled::QueryAndPhysics
    );
    CollisionRoot->SetCollisionObjectType(ECC_PhysicsBody);
    CollisionRoot->SetCollisionResponseToAllChannels(ECR_Ignore);
    CollisionRoot->SetCollisionResponseToChannel(
        ECC_WorldStatic,
        ECR_Block
    );
    CollisionRoot->SetCollisionResponseToChannel(
        ECC_WorldDynamic,
        ECR_Block
    );
    CollisionRoot->SetSimulatePhysics(true);
    CollisionRoot->SetEnableGravity(true);
    CollisionRoot->SetLinearDamping(0.15f);
    CollisionRoot->SetAngularDamping(0.08f);

    CasingMeshComponent =
        CreateDefaultSubobject<UStaticMeshComponent>(
            TEXT("CasingMesh")
        );
    CasingMeshComponent->SetupAttachment(CollisionRoot);
    CasingMeshComponent->SetCollisionEnabled(
        ECollisionEnabled::NoCollision
    );
    CasingMeshComponent->SetCastShadow(false);
}

void ABHWeaponCasing::InitializeCasing(
    UStaticMesh* InCasingMesh,
    const FVector& LinearVelocity,
    const FVector& AngularVelocityRadians,
    float LifeSpanSeconds
)
{
    if (IsValid(CasingMeshComponent))
    {
        CasingMeshComponent->SetStaticMesh(InCasingMesh);
    }

    if (IsValid(CollisionRoot))
    {
        CollisionRoot->SetMassOverrideInKg(
            NAME_None,
            0.008f,
            true
        );
        CollisionRoot->SetPhysicsLinearVelocity(LinearVelocity);
        CollisionRoot->SetPhysicsAngularVelocityInRadians(
            AngularVelocityRadians
        );
    }

    SetLifeSpan(FMath::Max(0.1f, LifeSpanSeconds));
}
