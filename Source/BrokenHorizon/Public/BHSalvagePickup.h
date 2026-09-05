#pragma once

#include "CoreMinimal.h"
#include "BHInteractable.h"
#include "GameFramework/Actor.h"
#include "BHSalvagePickup.generated.h"

class UStaticMeshComponent;
class UTextRenderComponent;

UENUM(BlueprintType)
enum class EBHSalvagePickupType : uint8
{
    Ammunition,
    FragGrenades,
    SmokeGrenades,
    EngineeringCharges
    ,AntiVehicleRounds
};

UCLASS(BlueprintType, Blueprintable)
class BROKENHORIZON_API ABHSalvagePickup
    : public AActor,
      public IBHInteractable
{
    GENERATED_BODY()

public:
    ABHSalvagePickup();

    static FText BuildInteractionText(
        EBHSalvagePickupType Type,
        int32 InQuantity
    );

    virtual void Interact_Implementation(AActor* InteractingActor) override;
    virtual FText GetInteractionText_Implementation() const override;

    UFUNCTION(BlueprintCallable, Category = "Loot|Salvage")
    void ConfigureSalvage(
        FName NewPersistenceID,
        EBHSalvagePickupType NewType,
        int32 NewQuantity
    );

    UFUNCTION(BlueprintPure, Category = "Loot|Salvage")
    FName GetPersistenceID() const;

    UFUNCTION(BlueprintPure, Category = "Loot|Salvage")
    EBHSalvagePickupType GetSalvageType() const;

    UFUNCTION(BlueprintPure, Category = "Loot|Salvage")
    int32 GetQuantity() const;

protected:
    virtual void BeginPlay() override;
    virtual void GetLifetimeReplicatedProps(
        TArray<FLifetimeProperty>& OutLifetimeProps
    ) const override;

    UFUNCTION()
    void OnRep_SalvageState();

    void RefreshSalvageLabel();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Loot|Salvage")
    TObjectPtr<UStaticMeshComponent> PickupMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Loot|Salvage")
    TObjectPtr<UTextRenderComponent> PickupLabel;

    UPROPERTY(Replicated, EditInstanceOnly, BlueprintReadOnly, Category = "Loot|Salvage")
    FName PersistenceID = NAME_None;

    UPROPERTY(ReplicatedUsing = OnRep_SalvageState, EditInstanceOnly, BlueprintReadOnly, Category = "Loot|Salvage")
    EBHSalvagePickupType SalvageType = EBHSalvagePickupType::Ammunition;

    UPROPERTY(ReplicatedUsing = OnRep_SalvageState, EditInstanceOnly, BlueprintReadOnly, Category = "Loot|Salvage", meta = (ClampMin = "1"))
    int32 Quantity = 30;
};
