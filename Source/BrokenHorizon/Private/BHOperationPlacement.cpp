#include "BHOperationPlacement.h"

#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Pawn.h"

bool BHOperationPlacement::TryFindGroundedInsertion(
    ACharacter& Character,
    const FVector& Center,
    float PreferredDistance,
    FVector& OutLocation,
    FRotator& OutRotation
)
{
    UWorld* World = Character.GetWorld();
    const UCapsuleComponent* Capsule = Character.GetCapsuleComponent();
    const UCharacterMovementComponent* Movement = Character.GetCharacterMovement();
    if (!IsValid(World) || !IsValid(Capsule) || !IsValid(Movement) ||
        Center.ContainsNaN() || !FMath::IsFinite(PreferredDistance) || PreferredDistance <= 0.0f)
    {
        return false;
    }

    FVector RadialDirection = (Character.GetActorLocation() - Center).GetSafeNormal2D();
    if (RadialDirection.IsNearlyZero())
    {
        RadialDirection = -Character.GetActorForwardVector().GetSafeNormal2D();
    }
    if (RadialDirection.IsNearlyZero())
    {
        RadialDirection = -FVector::ForwardVector;
    }

    TArray<float, TInlineAllocator<5>> Distances;
    for (const float Distance : {PreferredDistance, PreferredDistance * 0.5f,
            PreferredDistance * 0.25f, 2000.0f, 1000.0f})
    {
        if (!Distances.ContainsByPredicate([Distance](float Existing)
            { return FMath::IsNearlyEqual(Existing, Distance); }))
        {
            Distances.Add(Distance);
        }
    }

    const FCollisionShape Shape = FCollisionShape::MakeCapsule(
        Capsule->GetScaledCapsuleRadius(), Capsule->GetScaledCapsuleHalfHeight());
    FCollisionQueryParams Query(SCENE_QUERY_STAT(BHOperationInsertion), false, &Character);
    const FCollisionResponseParams Responses(Capsule->GetCollisionResponseToChannels());
    const ECollisionChannel Channel = Capsule->GetCollisionObjectType();
    constexpr float VerticalSearch = 20000.0f;
    constexpr float FloorClearance = 2.0f;
    for (const float Distance : Distances)
    {
        for (const float Angle : {0.0f, 45.0f, -45.0f, 90.0f, -90.0f, 180.0f})
        {
            const FVector Offset = RadialDirection.RotateAngleAxis(Angle, FVector::UpVector) * Distance;
            const FVector Column = Center + Offset;
            FHitResult FloorHit;
            if (!World->SweepSingleByChannel(FloorHit,
                    Column + FVector(0.0f, 0.0f, VerticalSearch),
                    Column - FVector(0.0f, 0.0f, VerticalSearch),
                    FQuat::Identity, Channel, Shape, Query, Responses) ||
                FloorHit.bStartPenetrating || Cast<APawn>(FloorHit.GetActor()) ||
                !Movement->IsWalkable(FloorHit))
            {
                continue;
            }

            const FVector Candidate = FloorHit.Location + FVector(0.0f, 0.0f, FloorClearance);
            if (World->OverlapBlockingTestByChannel(Candidate, FQuat::Identity,
                    Channel, Shape, Query, Responses))
            {
                continue;
            }

            const FVector Facing = (Center - Candidate).GetSafeNormal2D();
            OutLocation = Candidate;
            OutRotation = FRotator(0.0f, Facing.Rotation().Yaw, 0.0f);
            return true;
        }
    }
    return false;
}
