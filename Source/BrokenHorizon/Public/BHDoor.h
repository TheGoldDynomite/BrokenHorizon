#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BHInteractable.h"
#include "BHDoor.generated.h"

class USceneComponent;
class UStaticMeshComponent;

UCLASS()
class BROKENHORIZON_API ABHDoor
    : public AActor,
    public IBHInteractable
{
    GENERATED_BODY()

public:
    ABHDoor();

    virtual void Interact_Implementation(
        AActor* InteractingActor
    ) override;

    virtual FText GetInteractionText_Implementation() const override;

protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Door|Components")
    TObjectPtr<USceneComponent> DoorRoot;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Door|Components")
    TObjectPtr<UStaticMeshComponent> DoorMesh;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Door")
    float OpenAngle = 90.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Door")
    bool bIsOpen = false;
};