#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BHInteractable.h"
#include "BHDoor.generated.h"

class USceneComponent;
class UStaticMeshComponent;
class ABHCharacter;

UCLASS()
class BROKENHORIZON_API ABHDoor
    : public AActor,
    public IBHInteractable
{
    GENERATED_BODY()

public:
    ABHDoor();

    virtual void GetLifetimeReplicatedProps(
        TArray<FLifetimeProperty>& OutLifetimeProps
    ) const override;

    virtual void Interact_Implementation(
        AActor* InteractingActor
    ) override;

    virtual FText GetInteractionText_Implementation() const override;

    virtual void Tick(float DeltaTime) override;

    FName GetPersistenceID() const;

    bool IsUnlocked() const;

    void RestoreUnlockedState(bool bShouldBeUnlocked);

    bool BreachDoor(ABHCharacter* BreachingCharacter);
    void SetBreachChargePlanted(bool bPlanted);
    bool CanBeBreached() const;

protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Door|Components")
    TObjectPtr<USceneComponent> DoorRoot;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Door|Components")
    TObjectPtr<UStaticMeshComponent> DoorMesh;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Door")
    float OpenAngle = 90.0f;

    UPROPERTY(
        Replicated,
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "Door"
    )
    bool bIsOpen = false;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Door")
    float DoorOpenSpeed = 4.0f;

    UPROPERTY(
        Replicated,
        EditAnywhere,
        BlueprintReadOnly,
        Category = "Door"
    )
    bool bLocked = false;

    UPROPERTY(Replicated, VisibleAnywhere, BlueprintReadOnly, Category = "Door")
    bool bBreached = false;

    UPROPERTY(Replicated, VisibleAnywhere, BlueprintReadOnly, Category = "Door")
    bool bBreachChargePlanted = false;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Door")
    FName RequiredKeycard = NAME_None;

    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Persistence")
    FName PersistenceID = NAME_None;

    FRotator ClosedRotation;
    FRotator OpenRotation;
    UPROPERTY(Replicated)
    FRotator TargetOpenRotation;

    virtual void BeginPlay() override;
};
