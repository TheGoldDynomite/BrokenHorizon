#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BHArmoredThreat.generated.h"

class UStaticMeshComponent;

UCLASS(Blueprintable)
class BROKENHORIZON_API ABHArmoredThreat : public AActor
{
    GENERATED_BODY()

public:
    ABHArmoredThreat();

    virtual float TakeDamage(
        float DamageAmount,
        FDamageEvent const& DamageEvent,
        AController* EventInstigator,
        AActor* DamageCauser
    ) override;

    UFUNCTION(BlueprintCallable, Category = "Armored Threat|Damage")
    float ApplyAntiVehicleDamage(
        float DamageAmount,
        FVector ImpactLocation,
        AActor* DamageCauser
    );

    UFUNCTION(BlueprintPure, Category = "Armored Threat|Damage")
    float GetArmorIntegrityFraction() const;

    UFUNCTION(BlueprintPure, Category = "Armored Threat|Damage")
    float GetMobilityFraction() const;

    UFUNCTION(BlueprintPure, Category = "Armored Threat|Damage")
    bool IsMobilityDisabled() const;

    UFUNCTION(BlueprintPure, Category = "Armored Threat|Persistence")
    FName GetPersistenceID() const;

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void GetLifetimeReplicatedProps(
        TArray<FLifetimeProperty>& OutLifetimeProps
    ) const override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Armored Threat")
    TObjectPtr<UStaticMeshComponent> ThreatMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Armored Threat")
    TObjectPtr<UStaticMeshComponent> TurretMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Armored Threat")
    TObjectPtr<UStaticMeshComponent> BarrelMesh;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Replicated, Category = "Armored Threat|Persistence")
    FName PersistenceID = TEXT("EasternDepotArmoredThreat01");

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Replicated, Category = "Armored Threat|Damage", meta = (ClampMin = "1.0"))
    float MaximumArmorIntegrity = 500.0f;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, ReplicatedUsing = OnRep_ArmorIntegrity, Category = "Armored Threat|Damage")
    float ArmorIntegrity = 500.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Armored Threat|Damage", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float FrontalDamageMultiplier = 0.15f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Armored Threat|Damage", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float RearDamageMultiplier = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Armored Threat|Damage", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float MobilityDisableFraction = 0.25f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Armored Threat|AI", meta = (ClampMin = "100.0"))
    float ContactRange = 4500.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Armored Threat|AI", meta = (ClampMin = "0.1"))
    float WeaponCooldown = 2.5f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Armored Threat|AI", meta = (ClampMin = "0.0"))
    float WeaponDamage = 18.0f;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, ReplicatedUsing = OnRep_ContactState, Category = "Armored Threat|AI")
    bool bHasPlayerContact = false;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Replicated, Category = "Armored Threat|AI")
    TObjectPtr<AActor> CurrentTarget;

    float WeaponCooldownRemaining = 0.0f;

    UFUNCTION()
    void OnRep_ArmorIntegrity();

    UFUNCTION()
    void OnRep_ContactState();

private:
    void BroadcastThreatState(const TCHAR* Reason) const;
};
