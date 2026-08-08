#include "BHSmokeGrenade.h"

#include "BHCharacter.h"
#include "BHTacticalSupportZone.h"
#include "Engine/World.h"

ABHSmokeGrenade::ABHSmokeGrenade()
{
    FuseDuration = 2.4f;
    MaximumDamage = 0.0f;
    MinimumDamage = 0.0f;
    InnerDamageRadius = 90.0f;
    OuterDamageRadius = 120.0f;
    ExplosionNoiseRange = 1200.0f;
}

void ABHSmokeGrenade::Explode()
{
    if (!IsValid(GetWorld()))
    {
        return;
    }

    const FVector Origin = GetActorLocation();
    SetActorEnableCollision(false);

    if (HasAuthority())
    {
        const FActorSpawnParameters SpawnParameters;
        ABHTacticalSupportZone* SmokeZone = GetWorld()->SpawnActor<ABHTacticalSupportZone>(
            ABHTacticalSupportZone::StaticClass(),
            Origin,
            FRotator::ZeroRotator,
            SpawnParameters
        );

        if (IsValid(SmokeZone))
        {
            SmokeZone->InitializeSupport(
                EBHTacticalSupportType::SmokeScreen,
                Cast<ABHCharacter>(GetInstigator())
            );
        }
    }

    UE_LOG(
        LogTemp,
        Display,
        TEXT("BH_SMOKE_GRENADE_EXPLODED location=%s"),
        *Origin.ToCompactString()
    );

    Destroy();
}
