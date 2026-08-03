#pragma once

#include "CoreMinimal.h"
#include "BHCoverPoint.h"
#include "BHInteractable.h"
#include "BHWarTypes.h"
#include "BHFieldFortification.generated.h"

class ABHCharacter;
class UStaticMeshComponent;
class UTextRenderComponent;

UCLASS(BlueprintType, Blueprintable)
class BROKENHORIZON_API ABHFieldFortification
    : public ABHCoverPoint,
      public IBHInteractable
{
    GENERATED_BODY()

public:
    ABHFieldFortification();

    virtual void GetLifetimeReplicatedProps(
        TArray<FLifetimeProperty>& OutLifetimeProps
    ) const override;

    virtual float TakeDamage(
        float DamageAmount,
        const FDamageEvent& DamageEvent,
        AController* EventInstigator,
        AActor* DamageCauser
    ) override;

    virtual void Interact_Implementation(
        AActor* InteractingActor
    ) override;

    virtual FText GetInteractionText_Implementation() const override;

    UFUNCTION(BlueprintCallable, Category = "Field Fortification")
    void ConfigureFortification(
        FName NewPersistenceID,
        FName NewSectorID
    );

    UFUNCTION(BlueprintPure, Category = "Field Fortification")
    FName GetPersistenceID() const;

    UFUNCTION(BlueprintPure, Category = "Field Fortification")
    FName GetSectorID() const;

    UFUNCTION(BlueprintPure, Category = "Field Fortification")
    bool IsConstructed() const;

    UFUNCTION(BlueprintPure, Category = "Field Fortification")
    float GetHealthFraction() const;

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Field Fortification|Persistence")
    void RestorePersistentState(
        const FTransform& SavedTransform,
        bool bSavedConstructed,
        float SavedHealthFraction
    );

    UFUNCTION(BlueprintPure, Category = "Field Fortification|Placement")
    bool IsPlacementValid() const;

    static bool CanConstructForSector(
        const FBHWarSectorState& Sector,
        float RequiredSupply
    );

    static float CalculateRepairSupplyCost(
        float HealthFraction,
        float FullRepairSupplyCost
    );

protected:
    virtual void BeginPlay() override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Field Fortification|Components")
    TObjectPtr<UStaticMeshComponent> BarricadeMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Field Fortification|Components")
    TObjectPtr<UTextRenderComponent> StatusLabel;

    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Field Fortification|Persistence")
    FName PersistenceID = NAME_None;

    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Field Fortification|War")
    FName SectorID = NAME_None;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Field Fortification|Economy", meta = (ClampMin = "0.0"))
    float ConstructionSupplyCost = 12.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Field Fortification|Economy", meta = (ClampMin = "0.0"))
    float FullRepairSupplyCost = 6.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Field Fortification|Durability", meta = (ClampMin = "1.0"))
    float MaximumHealth = 600.0f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Field Fortification|Placement",
        meta = (ClampMin = "100.0")
    )
    float MinimumPeerSpacingCentimeters = 800.0f;

private:
    UFUNCTION()
    void OnRep_FortificationState();

    void RefreshPresentation();
    void NotifyPlayer(ABHCharacter* Character, const FText& Message) const;

    UPROPERTY(ReplicatedUsing = OnRep_FortificationState)
    bool bConstructed = false;

    UPROPERTY(ReplicatedUsing = OnRep_FortificationState)
    float CurrentHealth = 0.0f;
};
