#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BHInteractable.h"
#include "BHSupplyBase.generated.h"

class ABHCharacter;
class USceneComponent;
class UStaticMeshComponent;

UCLASS(Abstract, Blueprintable)
class BROKENHORIZON_API ABHSupplyBase
    : public AActor,
      public IBHInteractable
{
    GENERATED_BODY()

public:
    ABHSupplyBase();

    virtual void GetLifetimeReplicatedProps(
        TArray<FLifetimeProperty>& OutLifetimeProps
    ) const override;

    virtual void Interact_Implementation(
        AActor* InteractingActor
    ) override;

    virtual FText GetInteractionText_Implementation() const override;

    UFUNCTION(BlueprintPure, Category = "Supply")
    FName GetPersistenceID() const;

    UFUNCTION(BlueprintPure, Category = "Supply")
    bool IsConsumed() const;

    UFUNCTION(BlueprintPure, Category = "Supply")
    bool IsRuntimeSupply() const;

    void RestoreConsumedState(bool bShouldBeConsumed);

protected:
    virtual void BeginPlay() override;

    void MarkAsRuntimeSupply();

    virtual bool TryApplyToCharacter(
        ABHCharacter* Character
    ) PURE_VIRTUAL(ABHSupplyBase::TryApplyToCharacter, return false;);

    UFUNCTION(BlueprintImplementableEvent, Category = "Supply")
    void OnSupplyUsed(ABHCharacter* Character);

    UFUNCTION(BlueprintImplementableEvent, Category = "Supply")
    void OnSupplyUnavailable(ABHCharacter* Character);

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Supply")
    TObjectPtr<USceneComponent> SupplyRoot;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Supply")
    TObjectPtr<UStaticMeshComponent> SupplyMesh;

    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Supply")
    FName PersistenceID = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Supply")
    FText InteractionText;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Supply")
    bool bConsumeOnUse = true;

private:
    UFUNCTION()
    void OnRep_Consumed();

    UFUNCTION()
    void OnRep_RuntimeSupply();

    void DisableConsumedSupply();
    void LogRuntimeSupplyReplicationIfReady();

    UPROPERTY(ReplicatedUsing = OnRep_Consumed)
    bool bConsumed = false;

    UPROPERTY(ReplicatedUsing = OnRep_RuntimeSupply)
    bool bRuntimeSupply = false;

    bool bRuntimeSupplyReplicationLogged = false;
    bool bRuntimeSupplyAvailableReplicationLogged = false;
};
