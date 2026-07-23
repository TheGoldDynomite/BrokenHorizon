#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "BHSaveGame.generated.h"

class UBHMissionData;

namespace BHSave
{
    inline constexpr int32 CurrentSchemaVersion = 1;
}

UCLASS()
class BROKENHORIZON_API UBHSaveGame : public USaveGame
{
    GENERATED_BODY()

public:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Save")
    int32 SchemaVersion = BHSave::CurrentSchemaVersion;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Save")
    FName SavedLevelName = NAME_None;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Mission")
    TSoftObjectPtr<UBHMissionData> MissionData;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Mission")
    FName CurrentObjectiveID = NAME_None;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Mission")
    TArray<FName> CompletedObjectiveIDs;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Inventory")
    TArray<FName> OwnedKeycardIDs;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "World")
    TArray<FName> CollectedKeycardPersistenceIDs;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "World")
    TArray<FName> UnlockedDoorPersistenceIDs;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Player")
    FTransform PlayerTransform = FTransform::Identity;
};
