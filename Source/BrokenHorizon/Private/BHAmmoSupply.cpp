#include "BHAmmoSupply.h"

#include "BHCharacter.h"
#include "BHWeaponComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

ABHAmmoSupply::ABHAmmoSupply()
{
    InteractionText = NSLOCTEXT(
        "BrokenHorizon",
        "AmmoSupplyInteraction",
        "Take Ammunition"
    );

    static ConstructorHelpers::FObjectFinder<UStaticMesh>
        DefaultSupplyMesh(
            TEXT("/Engine/BasicShapes/Cube.Cube")
        );

    if (DefaultSupplyMesh.Succeeded() && IsValid(SupplyMesh))
    {
        SupplyMesh->SetStaticMesh(DefaultSupplyMesh.Object);
        SupplyMesh->SetRelativeLocation(
            FVector(0.0f, 0.0f, 18.0f)
        );
        SupplyMesh->SetRelativeScale3D(
            FVector(0.35f, 0.25f, 0.18f)
        );
    }
}

void ABHAmmoSupply::ConfigureRuntimePickup(
    int32 NewReserveAmmoAmount
)
{
    ReserveAmmoAmount = FMath::Max(1, NewReserveAmmoAmount);
    MarkAsRuntimeSupply();
}

int32 ABHAmmoSupply::GetReserveAmmoAmount() const
{
    return FMath::Max(1, ReserveAmmoAmount);
}

bool ABHAmmoSupply::TryApplyToCharacter(
    ABHCharacter* Character
)
{
    UBHWeaponComponent* WeaponComponent = IsValid(Character)
        ? Character->GetWeaponComponent()
        : nullptr;

    if (!IsValid(WeaponComponent))
    {
        return false;
    }

    const int32 AmmoAdded = WeaponComponent->AddReserveAmmo(
        GetReserveAmmoAmount()
    );

    if (AmmoAdded <= 0)
    {
        return false;
    }

    Character->ShowStatusNotification(
        FText::Format(
            NSLOCTEXT(
                "BrokenHorizon",
                "AmmoResupplyNotification",
                "AMMUNITION ACQUIRED\n+{0} ROUNDS"
            ),
            FText::AsNumber(AmmoAdded)
        )
    );
    return true;
}
