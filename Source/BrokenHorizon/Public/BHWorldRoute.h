#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BHWorldRoute.generated.h"

class USceneComponent;
class USplineComponent;

UCLASS(BlueprintType, Blueprintable)
class BROKENHORIZON_API ABHWorldRoute : public AActor
{
    GENERATED_BODY()

public:
    ABHWorldRoute();

    UFUNCTION(BlueprintCallable, Category = "World|Route")
    void ConfigureRoute(
        FName NewRouteID,
        const FText& NewDisplayName,
        const TArray<FVector>& WorldPoints
    );

    UFUNCTION(BlueprintPure, Category = "World|Route")
    FName GetRouteID() const;

    UFUNCTION(BlueprintPure, Category = "World|Route")
    FText GetRouteDisplayName() const;

    UFUNCTION(BlueprintPure, Category = "World|Route")
    float GetRouteLength() const;

    UFUNCTION(BlueprintPure, Category = "World|Route")
    float GetDistanceAlongRouteClosestToWorldLocation(
        const FVector& WorldLocation
    ) const;

    UFUNCTION(BlueprintPure, Category = "World|Route")
    FVector GetWorldLocationAtDistance(
        float Distance
    ) const;

    UFUNCTION(BlueprintPure, Category = "World|Route")
    FVector GetWorldDirectionAtDistance(
        float Distance
    ) const;

protected:
    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "World|Route"
    )
    TObjectPtr<USceneComponent> SceneRoot;

    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "World|Route"
    )
    TObjectPtr<USplineComponent> RouteSpline;

    UPROPERTY(
        EditInstanceOnly,
        BlueprintReadOnly,
        Category = "World|Route"
    )
    FName RouteID = NAME_None;

    UPROPERTY(
        EditInstanceOnly,
        BlueprintReadOnly,
        Category = "World|Route"
    )
    FText RouteDisplayName;
};
