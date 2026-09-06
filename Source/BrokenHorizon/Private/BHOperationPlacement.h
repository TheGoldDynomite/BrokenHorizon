#pragma once

#include "CoreMinimal.h"

class ACharacter;

namespace BHOperationPlacement
{
// Queries only: no actor mutation, and output values remain unchanged on failure.
bool TryFindGroundedInsertion(
    ACharacter& Character,
    const FVector& Center,
    float PreferredDistance,
    FVector& OutLocation,
    FRotator& OutRotation
);
}
