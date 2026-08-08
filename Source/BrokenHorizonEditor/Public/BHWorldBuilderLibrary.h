#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "BHWorldBuilderLibrary.generated.h"

class UMaterialInterface;

UCLASS()
class BROKENHORIZONEDITOR_API UBHWorldBuilderLibrary
    : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    UFUNCTION(
        BlueprintCallable,
        Category = "Broken Horizon|World Building"
    )
    static bool BuildInitialRegionLandscape(
        UObject* WorldContextObject
    );

    UFUNCTION(
        BlueprintCallable,
        Category = "Broken Horizon|World Building"
    )
    static bool RepairInitialRegionLandscapeHeight(
        UObject* WorldContextObject
    );

    UFUNCTION(
        BlueprintCallable,
        Category = "Broken Horizon|World Building"
    )
    static bool ApplyInitialRegionLandscapeMaterial(
        UObject* WorldContextObject,
        UMaterialInterface* Material
    );

    UFUNCTION(
        BlueprintPure,
        Category = "Broken Horizon|World Building"
    )
    static float GetInitialRegionHeightMeters(
        FVector2D WorldPositionCentimeters
    );
};
