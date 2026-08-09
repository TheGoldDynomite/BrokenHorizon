#include "BHCoverPoint.h"

#include "BHEnemySoldier.h"
#include "BHHealthComponent.h"
#include "Components/SceneComponent.h"

ABHCoverPoint::ABHCoverPoint()
{
    PrimaryActorTick.bCanEverTick = false;

    CoverRoot = CreateDefaultSubobject<USceneComponent>(
        TEXT("CoverRoot")
    );
    SetRootComponent(CoverRoot);
}

bool ABHCoverPoint::ShouldReleaseClaimForCasualty(
    bool bClaimantIsDead,
    bool bClaimantIsIncapacitated,
    bool bClaimantRequiresMedicalEvacuation
)
{
    return bClaimantIsDead ||
        bClaimantIsIncapacitated ||
        bClaimantRequiresMedicalEvacuation;
}

bool ABHCoverPoint::IsAvailableFor(
    const AActor* RequestingActor
) const
{
    if (!bCoverEnabled || !IsValid(RequestingActor))
    {
        return false;
    }

    const AActor* Claimant = ClaimedBy.Get();
    if (!IsValid(Claimant) || Claimant == RequestingActor)
    {
        return true;
    }

    const UBHHealthComponent* ClaimantHealth =
        Claimant->FindComponentByClass<UBHHealthComponent>();
    if (IsValid(ClaimantHealth) &&
        ShouldReleaseClaimForCasualty(
            ClaimantHealth->IsDead(),
            false,
            false
        ))
    {
        return true;
    }

    const ABHEnemySoldier* EnemyClaimant =
        Cast<ABHEnemySoldier>(Claimant);
    return IsValid(EnemyClaimant) &&
        ShouldReleaseClaimForCasualty(
            false,
            EnemyClaimant->IsIncapacitated(),
            EnemyClaimant->RequiresMedicalEvacuation()
        );
}

bool ABHCoverPoint::TryClaim(AActor* RequestingActor)
{
    if (!IsValid(RequestingActor) ||
        !IsAvailableFor(RequestingActor))
    {
        return false;
    }

    ClaimedBy = RequestingActor;
    return true;
}

void ABHCoverPoint::Release(AActor* RequestingActor)
{
    if (!IsValid(RequestingActor) ||
        ClaimedBy.Get() == RequestingActor)
    {
        ClaimedBy.Reset();
    }
}

FVector ABHCoverPoint::GetAnchorLocation() const
{
    return GetActorLocation();
}

FVector ABHCoverPoint::GetPeekLocation(bool bRightSide) const
{
    const float Side = bRightSide ? 1.0f : -1.0f;
    return GetActorLocation() +
        (GetActorForwardVector() * PeekForwardOffset) +
        (GetActorRightVector() * PeekLateralOffset * Side);
}
