#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BHInteractable.h"
#include "BHKeycard.generated.h"

class UStaticMeshComponent;

UCLASS()
class BROKENHORIZON_API ABHKeycard : public AActor, public IBHInteractable
{
    GENERATED_BODY()

public:
    ABHKeycard();

    virtual void Interact_Implementation(AActor* InteractingActor) override;

    virtual FText GetInteractionText_Implementation() const override;

protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Keycard")
    TObjectPtr<UStaticMeshComponent> KeycardMesh;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Keycard")
    FName KeycardID = TEXT("RedKeycard");

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Keycard")
    FText KeycardName = FText::FromString(TEXT("Red Keycard"));
};