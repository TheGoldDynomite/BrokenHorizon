#pragma once

#include "CoreMinimal.h"

class ABHCharacter;
class APawn;
class UObject;

namespace BHPlayerResolver
{
    BROKENHORIZON_API APawn* FindCombatPawn(
        const UObject* WorldContextObject,
        int32 PlayerIndex = 0
    );

    BROKENHORIZON_API ABHCharacter* Find(
        const UObject* WorldContextObject,
        int32 PlayerIndex = 0
    );
}
