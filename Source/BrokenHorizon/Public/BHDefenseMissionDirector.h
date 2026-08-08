#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BHDefenseMissionDirector.generated.h"

class ABHCharacter;
class ABHEnemySoldier;

UCLASS()
class BROKENHORIZON_API ABHDefenseMissionDirector : public AActor
{
    GENERATED_BODY()

public:
    ABHDefenseMissionDirector();

    virtual void Tick(float DeltaSeconds) override;

    void InitializeDefenseMission(ABHCharacter* InPlayerCharacter);

    UFUNCTION(BlueprintPure, Category = "Defense Mission")
    int32 GetCurrentWave() const;

    UFUNCTION(BlueprintPure, Category = "Defense Mission")
    int32 GetTotalWaves() const;

protected:
    virtual void EndPlay(
        const EEndPlayReason::Type EndPlayReason
    ) override;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Defense Mission",
        meta = (ClampMin = "1")
    )
    int32 TotalWaves = 3;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Defense Mission",
        meta = (ClampMin = "1")
    )
    int32 ReinforcementsPerWave = 2;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Defense Mission",
        meta = (ClampMin = "0.0", Units = "s")
    )
    float InterWaveDelay = 6.0f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Defense Mission",
        meta = (ClampMin = "0.0", Units = "cm")
    )
    float SpawnOffsetRadius = 180.0f;

private:
    void CaptureExistingDefenders();
    void BeginDefense();
    void ScheduleNextWave();
    void SpawnNextWave();
    void CompleteDefense();
    void NotifyWave(const FText& Message) const;
    bool HasLivingTrackedEnemies() const;
    FTransform BuildSpawnTransform(int32 EnemyIndex) const;

    UPROPERTY()
    TObjectPtr<ABHCharacter> PlayerCharacter;

    UPROPERTY()
    TSubclassOf<ABHEnemySoldier> EnemyClass;

    UPROPERTY()
    TArray<TObjectPtr<ABHEnemySoldier>> TrackedEnemies;

    TArray<FTransform> SpawnTransforms;
    int32 CurrentWave = 0;
    float NextWaveTime = 0.0f;
    bool bDefenseActive = false;
    bool bWaitingForWave = false;
    bool bDefenseComplete = false;
};
