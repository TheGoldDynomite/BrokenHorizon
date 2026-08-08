#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BHPatrolPoint.generated.h"

class UArrowComponent;
class USceneComponent;

UCLASS(Blueprintable)
class BROKENHORIZON_API ABHPatrolPoint : public AActor
{
    GENERATED_BODY()

public:
    ABHPatrolPoint();

protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Patrol Point")
    TObjectPtr<USceneComponent> PatrolPointRoot;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Patrol Point")
    TObjectPtr<UArrowComponent> DirectionArrow;
};
