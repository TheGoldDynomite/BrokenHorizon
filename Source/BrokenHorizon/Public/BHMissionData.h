#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "BHMissionData.generated.h"

namespace BHObjectiveIds
{
    BROKENHORIZON_API extern const FName FindRedKeycard;
    BROKENHORIZON_API extern const FName UnlockSecurityDoor;
    BROKENHORIZON_API extern const FName ExploreBeyondSecurityDoor;
}

USTRUCT(BlueprintType)
struct BROKENHORIZON_API FBHObjectiveDefinition
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objective")
    FName ObjectiveID = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objective")
    FText DisplayText;
};

UCLASS(BlueprintType)
class BROKENHORIZON_API UBHMissionData : public UDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mission")
    TArray<FBHObjectiveDefinition> Objectives;
};
