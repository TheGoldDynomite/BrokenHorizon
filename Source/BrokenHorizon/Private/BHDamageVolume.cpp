#include "BHDamageVolume.h"

#include "BHHealthComponent.h"
#include "Components/BoxComponent.h"

ABHDamageVolume::ABHDamageVolume()
{
    PrimaryActorTick.bCanEverTick = false;

    DamageBounds = CreateDefaultSubobject<UBoxComponent>(
        TEXT("DamageBounds")
    );
    SetRootComponent(DamageBounds);

    DamageBounds->SetBoxExtent(FVector(100.0f));
    DamageBounds->SetCollisionEnabled(
        ECollisionEnabled::QueryOnly
    );
    DamageBounds->SetCollisionObjectType(ECC_WorldDynamic);
    DamageBounds->SetCollisionResponseToAllChannels(ECR_Ignore);
    DamageBounds->SetCollisionResponseToChannel(
        ECC_Pawn,
        ECR_Overlap
    );
    DamageBounds->SetGenerateOverlapEvents(true);
    DamageBounds->OnComponentBeginOverlap.AddDynamic(
        this,
        &ABHDamageVolume::HandleBeginOverlap
    );
}

void ABHDamageVolume::HandleBeginOverlap(
    UPrimitiveComponent* OverlappedComponent,
    AActor* OtherActor,
    UPrimitiveComponent* OtherComponent,
    int32 OtherBodyIndex,
    bool bFromSweep,
    const FHitResult& SweepResult
)
{
    if (!IsValid(OtherActor) || OtherActor == this)
    {
        return;
    }

    UBHHealthComponent* HealthComponent =
        OtherActor->FindComponentByClass<UBHHealthComponent>();

    if (IsValid(HealthComponent))
    {
        HealthComponent->ApplyDamage(DamageAmount, this);
    }
}
