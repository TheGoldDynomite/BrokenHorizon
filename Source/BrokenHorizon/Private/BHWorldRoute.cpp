#include "BHWorldRoute.h"

#include "Components/SceneComponent.h"
#include "Components/SplineComponent.h"

ABHWorldRoute::ABHWorldRoute()
{
    PrimaryActorTick.bCanEverTick = false;
    SetActorEnableCollision(false);

#if WITH_EDITOR
    SetIsSpatiallyLoaded(false);
#endif

    SceneRoot = CreateDefaultSubobject<USceneComponent>(
        TEXT("SceneRoot")
    );
    SetRootComponent(SceneRoot);

    RouteSpline = CreateDefaultSubobject<USplineComponent>(
        TEXT("RouteSpline")
    );
    RouteSpline->SetupAttachment(SceneRoot);
    RouteSpline->SetClosedLoop(false);
    RouteSpline->SetDrawDebug(true);

#if WITH_EDITORONLY_DATA
    RouteSpline->EditorUnselectedSplineSegmentColor =
        FLinearColor(0.92f, 0.58f, 0.12f);
    RouteSpline->EditorSelectedSplineSegmentColor =
        FLinearColor(1.0f, 0.82f, 0.25f);
#endif
}

void ABHWorldRoute::ConfigureRoute(
    FName NewRouteID,
    const FText& NewDisplayName,
    const TArray<FVector>& WorldPoints
)
{
    RouteID = NewRouteID;
    RouteDisplayName = NewDisplayName;

    RouteSpline->ClearSplinePoints(false);

    for (const FVector& WorldPoint : WorldPoints)
    {
        RouteSpline->AddSplinePoint(
            WorldPoint,
            ESplineCoordinateSpace::World,
            false
        );
    }

    RouteSpline->UpdateSpline();
}

FName ABHWorldRoute::GetRouteID() const
{
    return RouteID;
}

FText ABHWorldRoute::GetRouteDisplayName() const
{
    return RouteDisplayName;
}

float ABHWorldRoute::GetRouteLength() const
{
    return IsValid(RouteSpline)
        ? RouteSpline->GetSplineLength()
        : 0.0f;
}

float ABHWorldRoute::
GetDistanceAlongRouteClosestToWorldLocation(
    const FVector& WorldLocation
) const
{
    if (!IsValid(RouteSpline))
    {
        return 0.0f;
    }

    const float SplineInputKey =
        RouteSpline->FindInputKeyClosestToWorldLocation(
            WorldLocation
        );

    return FMath::Clamp(
        RouteSpline->GetDistanceAlongSplineAtSplineInputKey(
            SplineInputKey
        ),
        0.0f,
        RouteSpline->GetSplineLength()
    );
}

FVector ABHWorldRoute::GetWorldLocationAtDistance(
    float Distance
) const
{
    if (!IsValid(RouteSpline))
    {
        return GetActorLocation();
    }

    return RouteSpline->GetLocationAtDistanceAlongSpline(
        FMath::Clamp(
            Distance,
            0.0f,
            RouteSpline->GetSplineLength()
        ),
        ESplineCoordinateSpace::World
    );
}

FVector ABHWorldRoute::GetWorldDirectionAtDistance(
    float Distance
) const
{
    if (!IsValid(RouteSpline))
    {
        return GetActorForwardVector();
    }

    return RouteSpline->GetDirectionAtDistanceAlongSpline(
        FMath::Clamp(
            Distance,
            0.0f,
            RouteSpline->GetSplineLength()
        ),
        ESplineCoordinateSpace::World
    );
}
