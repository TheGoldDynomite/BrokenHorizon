#pragma once

#include "CoreMinimal.h"
#include "BHInteractable.h"
#include "GameFramework/Actor.h"
#include "BHFieldArmory.generated.h"

class ABHCharacter;
class UStaticMeshComponent;
class UTextRenderComponent;

UCLASS(BlueprintType, Blueprintable)
class BROKENHORIZON_API ABHFieldArmory
    : public AActor,
      public IBHInteractable
{
    GENERATED_BODY()

public:
    ABHFieldArmory();

    virtual void Interact_Implementation(
        AActor* InteractingActor
    ) override;

    virtual FText GetInteractionText_Implementation() const override;

    UFUNCTION(BlueprintCallable, Category = "Field Armory")
    void ConfigureArmory(FName NewSectorID);

    UFUNCTION(BlueprintPure, Category = "Field Armory")
    FName GetSectorID() const;

protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Field Armory|Components")
    TObjectPtr<USceneComponent> SceneRoot;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Field Armory|Components")
    TObjectPtr<UStaticMeshComponent> ArmoryMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Field Armory|Components")
    TObjectPtr<UTextRenderComponent> ArmoryLabel;

    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Field Armory|War")
    FName SectorID = NAME_None;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Field Armory|Economy", meta = (ClampMin = "0.0"))
    float RoleChangeSupplyCost = 4.0f;
};
