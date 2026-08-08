#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "BHMissionData.generated.h"

namespace BHObjectiveIds
{
    BROKENHORIZON_API extern const FName FindRedKeycard;
    BROKENHORIZON_API extern const FName UnlockSecurityDoor;
    BROKENHORIZON_API extern const FName ExploreBeyondSecurityDoor;
    BROKENHORIZON_API extern const FName EliminateGuard;
    BROKENHORIZON_API extern const FName ReachExtraction;
    BROKENHORIZON_API extern const FName DeliverResupply;
    BROKENHORIZON_API extern const FName ProtectConvoy;
    BROKENHORIZON_API extern const FName EvacuateCasualty;
    BROKENHORIZON_API extern const FName ObserveSector;
}

USTRUCT(BlueprintType)
struct BROKENHORIZON_API FBHObjectiveDefinition
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objective")
    FName ObjectiveID = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objective")
    FText DisplayText;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objective|Radio")
    FText RadioSpeaker;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objective|Radio")
    FText ActivationRadioLine;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objective|Radio")
    FText CompletionRadioLine;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objective|Radio", meta = (ClampMin = "0.1"))
    float RadioSubtitleDuration = 3.5f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objective|Radio")
    bool bRadioHasDirection = false;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objective|Radio", meta = (ClampMin = "-180.0", ClampMax = "180.0"))
    float RadioDirectionDegrees = 0.0f;
};

UCLASS(BlueprintType)
class BROKENHORIZON_API UBHMissionData : public UDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mission")
    TArray<FBHObjectiveDefinition> Objectives;
};
