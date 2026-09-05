#pragma once

#include "CoreMinimal.h"
#include "BHInteractable.h"
#include "BHWarTypes.h"
#include "GameFramework/Actor.h"
#include "BHSupplyConvoyTarget.generated.h"

class UBHHealthComponent;
class ABHWorldRoute;
class USceneComponent;
class UStaticMeshComponent;
class UTextRenderComponent;

UCLASS(BlueprintType, Blueprintable)
class BROKENHORIZON_API ABHSupplyConvoyTarget
    : public AActor,
      public IBHInteractable
{
    GENERATED_BODY()

public:
    ABHSupplyConvoyTarget();

    virtual void Tick(float DeltaSeconds) override;

    static float CalculateRecoverableSupply(
        float Payload,
        float RecoveryFraction
    );

    static float CalculateRouteSpeedMultiplier(
        float HealthFraction,
        float CriticalHealthFraction,
        float MinimumSpeedMultiplier
    );

    static float CalculateDamageAdjustedRecoverableSupply(
        float Payload,
        float RecoveryFraction,
        float IntegrityFraction,
        float MinimumCargoIntegrityAtWreck
    );

    static FBHRouteOperationProfile BuildRouteOperationProfile(
        const FBHWarSupplyConvoyState& ConvoyState
    );

    static FText BuildRouteChoiceInteractionText(
        const FText& CurrentRouteName,
        const FText& NextRouteName
    );

    UFUNCTION(BlueprintCallable, Category = "Convoy")
    void ConfigureConvoy(
        const FBHWarSupplyConvoyState& ConvoyState
    );

    UFUNCTION(BlueprintCallable, Category = "Convoy")
    void SetTravelDestination(
        const FVector& NewTravelDestination
    );

    UFUNCTION(BlueprintCallable, Category = "Convoy")
    void SetTravelRoute(
        ABHWorldRoute* NewTravelRoute,
        const FVector& NewTravelDestination
    );

    void SetRouteChoices(
        const TArray<ABHWorldRoute*>& NewRouteChoices,
        ABHWorldRoute* InitiallySelectedRoute,
        const FVector& NewTravelDestination
    );

    UFUNCTION(BlueprintPure, Category = "Convoy|Operation")
    FBHRouteOperationProfile GetRouteOperationProfile() const;

    UFUNCTION(BlueprintPure, Category = "Convoy|Operation")
    float GetOperationDeadlineRemaining() const;

    UFUNCTION(BlueprintPure, Category = "Convoy")
    FName GetConvoyID() const;

    UFUNCTION(BlueprintPure, Category = "Convoy")
    float GetSupplyPayload() const;

    UFUNCTION(BlueprintPure, Category = "Convoy")
    float GetHealthPercentage() const;

    UFUNCTION(BlueprintPure, Category = "Convoy|Operation")
    float GetRouteSpeedMultiplier() const;

    UFUNCTION(BlueprintPure, Category = "Convoy")
    EBHWarFaction GetConvoyOwner() const;

    UFUNCTION(BlueprintPure, Category = "Convoy")
    EBHWarConvoyCargoType GetCargoType() const;

    UFUNCTION(BlueprintPure, Category = "Convoy")
    bool IsResolved() const;

    UFUNCTION(BlueprintPure, Category = "Convoy|Salvage")
    float GetRecoverableSupply() const;

    UFUNCTION(BlueprintPure, Category = "Convoy|Salvage")
    bool HasRecoverableSalvage() const;

    UFUNCTION(BlueprintPure, Category = "Convoy|Salvage")
    float GetSalvageLifetimeRemaining() const;

    UFUNCTION(BlueprintCallable, Category = "Convoy|Salvage")
    void RestoreSalvageWreck(
        FName SavedConvoyID,
        FName SavedSourceSectorID,
        FName SavedDestinationSectorID,
        float SavedOriginalSupplyPayload,
        float SavedRecoverableSupply,
        float SavedLifetimeRemaining
    );

    UFUNCTION(BlueprintPure, Category = "Convoy")
    FName GetSourceSectorID() const;

    UFUNCTION(BlueprintPure, Category = "Convoy")
    FName GetDestinationSectorID() const;

    virtual void Interact_Implementation(
        AActor* InteractingActor
    ) override;

    virtual FText GetInteractionText_Implementation()
        const override;

protected:
    virtual void BeginPlay() override;

    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "Convoy|Components"
    )
    TObjectPtr<USceneComponent> SceneRoot;

    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "Convoy|Components"
    )
    TObjectPtr<UStaticMeshComponent> ChassisMesh;

    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "Convoy|Components"
    )
    TObjectPtr<UStaticMeshComponent> CabMesh;

    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "Convoy|Components"
    )
    TObjectPtr<UStaticMeshComponent> CargoMesh;

    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "Convoy|Components"
    )
    TObjectPtr<UTextRenderComponent> ConvoyLabel;

    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "Convoy|Components"
    )
    TObjectPtr<UBHHealthComponent> HealthComponent;

    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "Convoy"
    )
    FName ConvoyID = NAME_None;

    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "Convoy"
    )
    FName SourceSectorID = NAME_None;

    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "Convoy"
    )
    FName DestinationSectorID = NAME_None;

    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "Convoy"
    )
    EBHWarFaction ConvoyOwner = EBHWarFaction::Neutral;

    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "Convoy"
    )
    EBHWarConvoyCargoType CargoType =
        EBHWarConvoyCargoType::MilitarySupply;

    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "Convoy"
    )
    float SupplyPayload = 0.0f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Convoy|Movement",
        meta = (ClampMin = "0.0", Units = "cm/s")
    )
    float MovementSpeed = 450.0f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Convoy|Movement",
        meta = (
            ClampMin = "0.05",
            ClampMax = "0.95"
        )
    )
    float CriticalHealthFraction = 0.40f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Convoy|Movement",
        meta = (
            ClampMin = "0.10",
            ClampMax = "1.0"
        )
    )
    float MinimumDamagedRouteSpeedMultiplier = 0.45f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Convoy|Salvage",
        meta = (
            ClampMin = "0.0",
            ClampMax = "1.0"
        )
    )
    float MinimumCargoIntegrityAtWreck = 0.50f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Convoy|Movement",
        meta = (ClampMin = "100.0", Units = "cm")
    )
        float ArrivalRadius = 300.0f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Convoy|Salvage",
        meta = (
            ClampMin = "0.0",
            ClampMax = "1.0"
        )
    )
    float EnemyCargoRecoveryFraction = 0.60f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Convoy|Salvage",
        meta = (ClampMin = "100.0", Units = "cm")
    )
    float SalvageTransportRadius = 2000.0f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Convoy|Salvage",
        meta = (ClampMin = "1.0", Units = "s")
    )
    float SalvageLifetime = 120.0f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Convoy|Salvage",
        meta = (ClampMin = "0.0", Units = "cm")
    )
    float SalvageSecurityRadius = 3000.0f;

private:
    UFUNCTION()
    void HandleConvoyDestroyed(AActor* DamageCauser);

    void UpdateConvoyLabel();
    void DisableConvoyCollision();
    void EnableWreckInteraction();
    void MoveAlongRoute(float DeltaSeconds);
    void ResolveLocalRouteExit();
    void NotifyPlayer(const FText& Message) const;
    bool HasActiveHostileSecurity() const;
    bool ResolveCommittedEscort(bool bConvoySurvived);
    bool IsCommittedFriendlyEscort() const;
    bool SelectNextRoute(AActor* InteractingActor);
    void HandleOperationDeadlineExpired();

    bool bResolved = false;
    bool bHasTravelDestination = false;
    FVector TravelDestination = FVector::ZeroVector;

    UPROPERTY(Transient)
    TObjectPtr<ABHWorldRoute> TravelRoute;

    UPROPERTY(Transient)
    TArray<TObjectPtr<ABHWorldRoute>> RouteChoices;

    FBHRouteOperationProfile RouteOperationProfile;
    float OperationDeadlineRemaining = 0.0f;
    bool bOperationDeadlineResolved = false;

    float CurrentRouteDistance = 0.0f;
    float DestinationRouteDistance = 0.0f;
    float RouteTravelDirection = 1.0f;
    float StrategicStateRefreshRemaining = 0.0f;
    float RecoverableSupply = 0.0f;
};
