#pragma once

#include "CoreMinimal.h"
#include "BHWarTypes.h"
#include "GameFramework/Actor.h"
#include "BHAmbientWarDirector.generated.h"

class ABHCharacter;
class ABHEnemySoldier;
class ABHPatrolPoint;
class ABHSectorAnchor;
class ABHSupplyConvoyTarget;
class ABHWorldRoute;
class UNavigationSystemV1;
class UAudioComponent;
class USceneComponent;
class USoundAttenuation;
class USoundBase;

UENUM(BlueprintType)
enum class EBHAmbientAudioState : uint8
{
    Quiet,
    Tense,
    Frontline,
    Combat
};

UCLASS()
class BROKENHORIZON_API ABHAmbientWarDirector : public AActor
{
    GENERATED_BODY()

public:
    ABHAmbientWarDirector();

    virtual void Tick(float DeltaSeconds) override;

    virtual void GetLifetimeReplicatedProps(
        TArray<FLifetimeProperty>& OutLifetimeProps
    ) const override;

    static EBHAmbientAudioState ResolveAmbientAudioState(
        bool bFrontline,
        bool bActiveOperation,
        int32 ActiveHostileCount,
        float EnemyResponsePressure
    );

    static float CalculateWarBedVolume(
        EBHAmbientAudioState AudioState
    );

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Ambient War|Audio")
    void SetWeatherMix(float WindIntensity, float RainIntensity);

    UFUNCTION(BlueprintPure, Category = "Ambient War|Awareness")
    FName GetPlayerSectorID() const;

    bool GetFieldReconStatus(
        float& OutIntelConfidence,
        float& OutMovementProgress,
        float& OutMovementRequired,
        float& OutObservationProgress,
        float& OutObservationRequired,
        float& OutReportCooldownRemaining
    ) const;

    int32 GetSurvivingConvoySecurityCount(
        const ABHSupplyConvoyTarget* ConvoyTarget
    ) const;

    void ResetSupplyConvoyEncounterForLoad();

    bool RestoreSupplyConvoySalvageSecurity(
        ABHSupplyConvoyTarget* ConvoyTarget,
        int32 SurvivingSecurityCount
    );

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(
        const EEndPlayReason::Type EndPlayReason
    ) override;

    UPROPERTY(
        EditDefaultsOnly,
        Category = "Ambient War|Population",
        meta = (ClampMin = "1")
    )
    int32 MaxActiveEnemies = 4;

    UPROPERTY(
        EditDefaultsOnly,
        Category = "Ambient War|Population",
        meta = (ClampMin = "1")
    )
    int32 MaxFriendlyPatrolMembers = 3;

    UPROPERTY(
        EditDefaultsOnly,
        Category = "Ambient War|Population",
        meta = (ClampMin = "1")
    )
    int32 PatrolPointCount = 3;

    UPROPERTY(
        EditDefaultsOnly,
        Category = "Ambient War|Timing",
        meta = (ClampMin = "0.0", Units = "s")
    )
    float InitialSpawnDelay = 12.0f;

    UPROPERTY(
        EditDefaultsOnly,
        Category = "Ambient War|Timing",
        meta = (ClampMin = "0.1", Units = "s")
    )
    float SpawnRetryDelay = 4.0f;

    UPROPERTY(
        EditDefaultsOnly,
        Category = "Ambient War|Timing",
        meta = (ClampMin = "0.0", Units = "s")
    )
    float PatrolRespawnDelay = 20.0f;

    UPROPERTY(
        EditDefaultsOnly,
        Category = "Ambient War|Timing",
        meta = (ClampMin = "0.0", Units = "s")
    )
    float SurvivorWithdrawalDelay = 12.0f;

    UPROPERTY(
        EditDefaultsOnly,
        Category = "Ambient War|Timing",
        meta = (ClampMin = "0.0", Units = "s")
    )
    float SectorContactCooldown = 90.0f;

    UPROPERTY(
        EditDefaultsOnly,
        Category = "Ambient War|Reconnaissance",
        meta = (ClampMin = "0.0", ClampMax = "100.0")
    )
    float SectorEntryIntelGain = 8.0f;

    UPROPERTY(
        EditDefaultsOnly,
        Category = "Ambient War|Reconnaissance",
        meta = (ClampMin = "0.0", ClampMax = "100.0")
    )
    float FieldReconIntelGain = 35.0f;

    UPROPERTY(
        EditDefaultsOnly,
        Category = "Ambient War|Reconnaissance",
        meta = (ClampMin = "1.0", Units = "s")
    )
    float FieldReconObservationDuration = 18.0f;

    UPROPERTY(
        EditDefaultsOnly,
        Category = "Ambient War|Reconnaissance",
        meta = (ClampMin = "100.0", Units = "cm")
    )
    float FieldReconMovementRequired = 2500.0f;

    UPROPERTY(
        EditDefaultsOnly,
        Category = "Ambient War|Reconnaissance",
        meta = (ClampMin = "0.0", Units = "s")
    )
    float FieldReconReportCooldown = 45.0f;

    UPROPERTY(
        EditDefaultsOnly,
        Category = "Ambient War|Placement",
        meta = (ClampMin = "1000.0", Units = "cm")
    )
    float MinimumSpawnDistance = 12000.0f;

    UPROPERTY(
        EditDefaultsOnly,
        Category = "Ambient War|Placement",
        meta = (ClampMin = "1000.0", Units = "cm")
    )
    float MaximumSpawnDistance = 18000.0f;

    UPROPERTY(
        EditDefaultsOnly,
        Category = "Ambient War|Placement",
        meta = (ClampMin = "100.0", Units = "cm")
    )
    float PatrolRadius = 2500.0f;

    UPROPERTY(
        EditDefaultsOnly,
        Category = "Ambient War|Placement",
        meta = (ClampMin = "1000.0", Units = "cm")
    )
    float CleanupDistance = 45000.0f;

    UPROPERTY(
        EditDefaultsOnly,
        Category = "Ambient War|Placement",
        meta = (ClampMin = "0.0", Units = "cm")
    )
    float RouteLateralOffset = 450.0f;

    UPROPERTY(
        EditDefaultsOnly,
        Category = "Ambient War|Deployment",
        meta = (ClampMin = "500.0", Units = "cm")
    )
    float OpposingPatrolSeparation = 1800.0f;

    UPROPERTY(
        EditDefaultsOnly,
        Category = "Ambient War|Deployment",
        meta = (ClampMin = "0.0", Units = "cm")
    )
    float FormationSpacing = 200.0f;

    UPROPERTY(
        EditDefaultsOnly,
        Category = "Ambient War|Strategic",
        meta = (ClampMin = "0")
    )
    int32 MaxForceProjectionHops = 3;

    UPROPERTY(
        EditDefaultsOnly,
        Category = "Ambient War|Strategic",
        meta = (ClampMin = "0.0")
    )
    float PatrolSupplyCostPerMember = 0.35f;

    UPROPERTY(
        EditDefaultsOnly,
        Category = "Ambient War|Strategic",
        meta = (ClampMin = "0.0")
    )
    float PatrolProjectionSupplyCostPerHop = 0.15f;

    UPROPERTY(
        EditDefaultsOnly,
        Category = "Ambient War|Logistics"
    )
    TSubclassOf<ABHSupplyConvoyTarget>
        SupplyConvoyTargetClass;

    UPROPERTY(
        EditDefaultsOnly,
        Category = "Ambient War|Logistics",
        meta = (ClampMin = "2500.0", Units = "cm")
    )
    float ConvoyOpportunitySpawnDistance = 6000.0f;

    UPROPERTY(
        EditDefaultsOnly,
        Category = "Ambient War|Logistics",
        meta = (ClampMin = "0.0", Units = "cm")
    )
    float ConvoyRouteConnectionTolerance = 150000.0f;

    UPROPERTY(
        EditDefaultsOnly,
        Category = "Ambient War|Logistics",
        meta = (ClampMin = "0", ClampMax = "4")
    )
    int32 ConvoyEscortCount = 2;

    UPROPERTY(
        EditDefaultsOnly,
        Category = "Ambient War|Logistics",
        meta = (ClampMin = "0", ClampMax = "10")
    )
    int32 ConvoyRouteSecurityTurnWindow = 3;

    UPROPERTY(
        EditDefaultsOnly,
        Category = "Ambient War|Logistics",
        meta = (ClampMin = "0", ClampMax = "2")
    )
    int32 MaximumConvoyRouteSecurityBonus = 2;

    UPROPERTY(
        EditDefaultsOnly,
        Category = "Ambient War|Logistics",
        meta = (ClampMin = "100.0", Units = "cm")
    )
    float ConvoyEscortSpacing = 350.0f;

    UPROPERTY(
        EditDefaultsOnly,
        Category = "Ambient War|Logistics",
        meta = (ClampMin = "0.0", Units = "s")
    )
    float ConvoyEscortWithdrawalDelay = 12.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ambient War|Audio")
    TObjectPtr<USoundBase> WindLoopSound;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ambient War|Audio")
    TObjectPtr<USoundBase> RainLoopSound;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ambient War|Audio")
    TObjectPtr<USoundBase> DistantWarLoopSound;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ambient War|Audio")
    TObjectPtr<USoundBase> DistantArtillerySound;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ambient War|Audio")
    TObjectPtr<USoundBase> DistantAircraftSound;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ambient War|Audio")
    TObjectPtr<USoundBase> DistantSmallArmsSound;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ambient War|Audio")
    TObjectPtr<USoundAttenuation> DistantEventAttenuation;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Ambient War|Audio",
        meta = (ClampMin = "1.0", Units = "s")
    )
    float MinimumDistantEventInterval = 18.0f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Ambient War|Audio",
        meta = (ClampMin = "1.0", Units = "s")
    )
    float MaximumDistantEventInterval = 45.0f;

private:
    ABHCharacter* ResolvePlayerCharacter() const;
    ABHSectorAnchor* FindNearestSectorAnchor(
        const FVector& PlayerLocation
    ) const;
    ABHSectorAnchor* FindSectorAnchorByID(
        FName SectorID
    ) const;
    bool ResolveLocalWarState(
        const FVector& PlayerLocation,
        FBHWarSectorState& OutSectorState,
        ABHSectorAnchor*& OutSectorAnchor
    ) const;
    void UpdatePlayerSectorAwareness(
        ABHCharacter* PlayerCharacter
    );
    void UpdateFieldRecon(
        ABHCharacter* PlayerCharacter,
        float DeltaSeconds
    );
    void UpdateSupplyConvoyOpportunity(
        ABHCharacter* PlayerCharacter
    );
    void SpawnSupplyConvoyEscorts(
        ABHSupplyConvoyTarget* ConvoyTarget,
        int32 RequestedCombatantCount
    );
    void SpawnFriendlySupplyConvoyDefenders(
        ABHSupplyConvoyTarget* ConvoyTarget,
        int32 RequestedDefenderCount
    );
    void UpdateSupplyConvoyEscorts();
    void CleanupSupplyConvoyEscorts();
    void ReportSupplyConvoyEscortCasualties(
        int32 EscortCasualties
    );
    void ReportSupplyConvoyDefenderCasualties(
        int32 DefenderCasualties
    );
    bool IsFrontlineSector(
        const FBHWarSectorState& SectorState
    ) const;
    bool ResolveForceSourceSector(
        const FBHWarSectorState& ContactSector,
        EBHWarFaction ForceFaction,
        FBHWarSectorState& OutSourceSector,
        int32* OutHopCount = nullptr
    ) const;
    float CalculatePatrolSupplyCost(
        int32 MemberCount,
        int32 SourceHops
    ) const;
    int32 CalculateConvoyCombatantCount(
        const FBHWarSectorState& SourceSector,
        int32 SourceHops,
        int32 RecentRouteInterdictions,
        EBHWarFaction CombatantFaction
    ) const;
    int32 GetDesiredEnemyCount(
        const FBHWarSectorState& SectorState
    ) const;
    int32 GetDesiredFriendlyCount(
        const FBHWarSectorState& SectorState
    ) const;
    bool HasAssignedOperation() const;
    ABHWorldRoute* FindNearestWorldRoute(
        const FVector& WorldLocation
    ) const;
    ABHWorldRoute* FindBestWorldRoute(
        const FVector& SourceLocation,
        const FVector& DestinationLocation
    ) const;
    TArray<ABHWorldRoute*> FindCompatibleWorldRoutes(
        const FVector& SourceLocation,
        const FVector& DestinationLocation
    ) const;
    bool TryBuildRoutePatrolCenter(
        const ABHCharacter* PlayerCharacter,
        UNavigationSystemV1* NavigationSystem,
        FVector& OutPatrolCenter
    ) const;
    bool TryBuildPatrolCenter(
        const ABHCharacter* PlayerCharacter,
        FVector& OutPatrolCenter
    ) const;
    bool SpawnPatrol(
        ABHCharacter* PlayerCharacter,
        const FBHWarSectorState& SectorState,
        int32 EnemyCount,
        int32 FriendlyCount
    );
    void BuildPatrolPoints(
        const FVector& PatrolCenter,
        TArray<ABHPatrolPoint*>& OutPatrolPoints
    );
    void RemoveInvalidEnemies();
    void CleanupDistantEnemies(
        const ABHCharacter* PlayerCharacter
    );
    void UpdatePatrolLifecycle();
    void ReportAmbientCasualties(
        int32 FriendlyCasualties,
        int32 EnemyCasualties
    );
    void ReportAmbientRout(
        int32 FriendlyRouted,
        int32 EnemyRouted
    );
    float GetRemainingSectorContactCooldown(
        FName SectorID,
        float CurrentTime
    ) const;
    void BeginSectorContactCooldown(FName SectorID);
    void DestroyPatrolPoints();
    void ResolveEnemyClass();
    void UpdateAmbientAudioState(ABHCharacter* PlayerCharacter);
    void UpdateDistantWarEvents(ABHCharacter* PlayerCharacter);
    void ApplyAmbientAudioMix();
    void ApplyLoopAudio(
        UAudioComponent* AudioComponent,
        USoundBase* Sound,
        float TargetVolume
    );
    USoundBase* ResolveDistantEventSound(uint8 EventType) const;

    UFUNCTION()
    void OnRep_AmbientAudioMix();

    UFUNCTION(NetMulticast, Unreliable)
    void MulticastPlayDistantWarEvent(
        uint8 EventType,
        FVector WorldLocation,
        float VolumeMultiplier
    );

    UPROPERTY(VisibleAnywhere, Category = "Ambient War|Audio")
    TObjectPtr<USceneComponent> AudioRoot;

    UPROPERTY(VisibleAnywhere, Category = "Ambient War|Audio")
    TObjectPtr<UAudioComponent> WindAudioComponent;

    UPROPERTY(VisibleAnywhere, Category = "Ambient War|Audio")
    TObjectPtr<UAudioComponent> RainAudioComponent;

    UPROPERTY(VisibleAnywhere, Category = "Ambient War|Audio")
    TObjectPtr<UAudioComponent> WarBedAudioComponent;

    UPROPERTY(ReplicatedUsing = OnRep_AmbientAudioMix)
    EBHAmbientAudioState AmbientAudioState = EBHAmbientAudioState::Quiet;

    UPROPERTY(ReplicatedUsing = OnRep_AmbientAudioMix)
    float ReplicatedWindIntensity = 0.35f;

    UPROPERTY(ReplicatedUsing = OnRep_AmbientAudioMix)
    float ReplicatedRainIntensity = 0.0f;

    UPROPERTY()
    TSubclassOf<ABHEnemySoldier> EnemyClass;

    UPROPERTY()
    TArray<TObjectPtr<ABHEnemySoldier>> TrackedEnemies;

    UPROPERTY()
    TSet<TObjectPtr<ABHEnemySoldier>> RoutedCombatants;

    UPROPERTY()
    TArray<TObjectPtr<ABHPatrolPoint>> ActivePatrolPoints;

    UPROPERTY()
    TObjectPtr<ABHSupplyConvoyTarget>
        ActiveSupplyConvoyTarget;

    TSet<FName> PresentedSupplyConvoyIDs;

    UPROPERTY()
    TArray<TObjectPtr<ABHEnemySoldier>> ConvoyEscorts;

    UPROPERTY()
    TArray<TObjectPtr<ABHEnemySoldier>> ConvoyDefenders;

    UPROPERTY()
    TArray<TObjectPtr<ABHPatrolPoint>>
        ConvoyEscortPatrolPoints;

    FName ActiveConvoySourceSectorID = NAME_None;
    FName ActiveConvoyDefenderSourceSectorID = NAME_None;

    FName ActivePatrolSectorID = NAME_None;
    FName ActiveFriendlyForceSectorID = NAME_None;
    FName ActiveEnemyForceSectorID = NAME_None;
    int32 ActiveFriendlySourceHops = INDEX_NONE;
    int32 ActiveEnemySourceHops = INDEX_NONE;
    int32 InitialFriendlyCount = 0;
    int32 InitialEnemyCount = 0;
    float PatrolResolvedTime = -1.0f;
    float ConvoyEscortWithdrawalTime = -1.0f;
    float NextSpawnTime = 0.0f;
    FName LastPlayerSectorID = NAME_None;
    TMap<FName, float> SectorContactReadyTimes;
    FVector LastReconSampleLocation = FVector::ZeroVector;
    float ReconMovementAccumulated = 0.0f;
    float ReconObservationAccumulated = 0.0f;
    float NextReconReportTime = 0.0f;
    float NextDistantWarEventTime = 0.0f;
};
