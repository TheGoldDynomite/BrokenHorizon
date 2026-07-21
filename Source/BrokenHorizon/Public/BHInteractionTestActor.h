#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BHInteractable.h"
#include "BHInteractionTestActor.generated.h"

class UStaticMeshComponent;

UCLASS()
class BROKENHORIZON_API ABHInteractionTestActor
    : public AActor,
    public IBHInteractable
{
    GENERATED_BODY()

public:
    ABHInteractionTestActor();

protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UStaticMeshComponent> Mesh;

public:
    virtual void Interact_Implementation(
        AActor* InteractingActor
    ) override;
};