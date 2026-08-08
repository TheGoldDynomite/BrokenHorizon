#include "BHPlayerResolver.h"

#include "BHCharacter.h"
#include "BHFieldTransport.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"

APawn* BHPlayerResolver::FindCombatPawn(
    const UObject* WorldContextObject,
    int32 PlayerIndex
)
{
    if (!IsValid(WorldContextObject) || PlayerIndex < 0)
    {
        return nullptr;
    }

    APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(
        WorldContextObject,
        PlayerIndex
    );

    if (!IsValid(PlayerPawn))
    {
        return nullptr;
    }

    if (PlayerPawn->IsA<ABHCharacter>())
    {
        return PlayerPawn;
    }

    if (const ABHFieldTransport* Transport =
        Cast<ABHFieldTransport>(PlayerPawn))
    {
        return IsValid(Transport->GetOccupant())
            ? PlayerPawn
            : nullptr;
    }

    return nullptr;
}

ABHCharacter* BHPlayerResolver::Find(
    const UObject* WorldContextObject,
    int32 PlayerIndex
)
{
    if (!IsValid(WorldContextObject) || PlayerIndex < 0)
    {
        return nullptr;
    }

    APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(
        WorldContextObject,
        PlayerIndex
    );

    if (!IsValid(PlayerPawn))
    {
        return nullptr;
    }

    if (ABHCharacter* Character =
        Cast<ABHCharacter>(PlayerPawn))
    {
        return Character;
    }

    if (const ABHFieldTransport* Transport =
        Cast<ABHFieldTransport>(PlayerPawn))
    {
        return Transport->GetOccupant();
    }

    return nullptr;
}
