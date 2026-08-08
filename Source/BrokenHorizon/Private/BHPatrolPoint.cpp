#include "BHPatrolPoint.h"

#include "Components/ArrowComponent.h"
#include "Components/SceneComponent.h"

ABHPatrolPoint::ABHPatrolPoint()
{
    PrimaryActorTick.bCanEverTick = false;

    PatrolPointRoot = CreateDefaultSubobject<USceneComponent>(
        TEXT("PatrolPointRoot")
    );
    SetRootComponent(PatrolPointRoot);

    DirectionArrow = CreateDefaultSubobject<UArrowComponent>(
        TEXT("DirectionArrow")
    );
    DirectionArrow->SetupAttachment(PatrolPointRoot);
    DirectionArrow->ArrowColor = FColor::Cyan;
    DirectionArrow->SetHiddenInGame(true);

    SetActorEnableCollision(false);
}
