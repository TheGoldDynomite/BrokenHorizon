#include "BHTargetDummy.h"

#include "BHHealthComponent.h"
#include "Components/StaticMeshComponent.h"

ABHTargetDummy::ABHTargetDummy()
{
    PrimaryActorTick.bCanEverTick = false;

    TargetMesh = CreateDefaultSubobject<UStaticMeshComponent>(
        TEXT("TargetMesh")
    );
    SetRootComponent(TargetMesh);
    TargetMesh->SetCollisionEnabled(
        ECollisionEnabled::QueryAndPhysics
    );
    TargetMesh->SetCollisionResponseToChannel(
        ECC_Visibility,
        ECR_Block
    );

    HealthComponent = CreateDefaultSubobject<UBHHealthComponent>(
        TEXT("HealthComponent")
    );
}

void ABHTargetDummy::BeginPlay()
{
    Super::BeginPlay();

    if (IsValid(HealthComponent))
    {
        HealthComponent->OnDeath.AddDynamic(
            this,
            &ABHTargetDummy::HandleDeath
        );
    }
}

UBHHealthComponent* ABHTargetDummy::GetHealthComponent() const
{
    return HealthComponent;
}

void ABHTargetDummy::HandleDeath(AActor* DamageCauser)
{
    UE_LOG(
        LogTemp,
        Log,
        TEXT("Target dummy %s was destroyed."),
        *GetName()
    );

    if (bHideOnDeath && IsValid(TargetMesh))
    {
        TargetMesh->SetCollisionEnabled(
            ECollisionEnabled::NoCollision
        );
        TargetMesh->SetVisibility(false, true);
    }
}
