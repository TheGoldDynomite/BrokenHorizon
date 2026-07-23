#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BHInteractable.h"
#include "BHCheckpoint.generated.h"

class UStaticMeshComponent;

UCLASS()
class BROKENHORIZON_API ABHCheckpoint
    : public AActor,
    public IBHInteractable
{
    GENERATED_BODY()

public:
    ABHCheckpoint();

    virtual void Interact_Implementation(
        AActor* InteractingActor
    ) override;

    virtual FText GetInteractionText_Implementation() const override;

protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Checkpoint")
    TObjectPtr<UStaticMeshComponent> CheckpointMesh;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Checkpoint")
    FText InteractionText;
};
