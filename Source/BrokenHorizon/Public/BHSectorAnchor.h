#pragma once

#include "CoreMinimal.h"
#include "BHWarTypes.h"
#include "GameFramework/Actor.h"
#include "BHSectorAnchor.generated.h"

class UArrowComponent;
class USceneComponent;
class USphereComponent;
class UTextRenderComponent;

UCLASS(BlueprintType, Blueprintable)
class BROKENHORIZON_API ABHSectorAnchor : public AActor
{
    GENERATED_BODY()

public:
    ABHSectorAnchor();

    virtual void Tick(float DeltaSeconds) override;

    UFUNCTION(BlueprintCallable, Category = "Persistent War|Sector")
    void ConfigureSector(
        FName NewSectorID,
        const FText& NewDisplayName
    );

    UFUNCTION(BlueprintPure, Category = "Persistent War|Sector")
    bool MatchesSector(FName CandidateSectorID) const;

    UFUNCTION(BlueprintPure, Category = "Persistent War|Sector")
    FName GetSectorID() const;

    UFUNCTION(BlueprintPure, Category = "Persistent War|Sector")
    FText GetSectorDisplayName() const;

    UFUNCTION(BlueprintPure, Category = "Persistent War|Sector")
    FVector GetOperationCenter() const;

    UFUNCTION(BlueprintPure, Category = "Persistent War|Sector")
    float GetOperationActivationRadius() const;

    FTransform BuildEnemySpawnTransform(
        int32 SpawnIndex,
        int32 SpawnCount,
        int32 WaveIndex
    ) const;

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(
        const EEndPlayReason::Type EndPlayReason
    ) override;

    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "Persistent War|Sector"
    )
    TObjectPtr<USceneComponent> SceneRoot;

    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "Persistent War|Sector"
    )
    TObjectPtr<UArrowComponent> OperationDirection;

    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "Persistent War|Operation"
    )
    TObjectPtr<USphereComponent> SquadContextTarget;

    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "Persistent War|Sector"
    )
    TObjectPtr<UTextRenderComponent> SectorStatusLabel;

    UPROPERTY(
        EditInstanceOnly,
        BlueprintReadOnly,
        Category = "Persistent War|Sector"
    )
    FName SectorID = NAME_None;

    UPROPERTY(
        EditInstanceOnly,
        BlueprintReadOnly,
        Category = "Persistent War|Sector"
    )
    FText SectorDisplayName;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadOnly,
        Category = "Persistent War|Operation",
        meta = (ClampMin = "100.0", Units = "cm")
    )
    float EnemySpawnRadius = 2500.0f;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadOnly,
        Category = "Persistent War|Operation",
        meta = (ClampMin = "100.0", Units = "cm")
    )
    float OperationActivationRadius = 50000.0f;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadOnly,
        Category = "Persistent War|Presentation",
        meta = (ClampMin = "1000.0", Units = "cm")
    )
    float SectorStatusCullDistance = 30000.0f;

private:
    UFUNCTION()
    void HandleWarStateChanged(
        int32 NewTurnNumber,
        FName PrioritySectorID,
        EBHWarPriorityType PriorityType
    );

    void RefreshWarStatus();
    void FaceStatusTowardPlayer();

    bool bHasCachedWarStatus = false;
    EBHWarFaction CachedOwner = EBHWarFaction::Neutral;
    int32 CachedSupply = INDEX_NONE;
    int32 CachedFriendlyStrength = INDEX_NONE;
    int32 CachedEnemyStrength = INDEX_NONE;
    int32 CachedFriendlyGarrison = INDEX_NONE;
    int32 CachedEnemyGarrison = INDEX_NONE;
    int32 CachedIntelConfidence = INDEX_NONE;
    bool bCachedPriority = false;
};
