#pragma once

#include "CoreMinimal.h"
#include "BHInteractable.h"
#include "GameFramework/Actor.h"
#include "BHRaidSabotageTarget.generated.h"

class ABHOpenWorldOperationDirector;
class ABHCharacter;
class ABHEnemySoldier;
class USceneComponent;
class UStaticMeshComponent;
class UTextRenderComponent;

UCLASS()
class BROKENHORIZON_API ABHRaidSabotageTarget
    : public AActor,
      public IBHInteractable
{
    GENERATED_BODY()

public:
    ABHRaidSabotageTarget();

    static FText BuildInteractionText(bool bInSabotaged);

    void ConfigureTarget(
        ABHOpenWorldOperationDirector* InOperationDirector,
        FName InSectorID
    );

    bool IsSabotaged() const;

    bool CanAcceptFieldSquadSabotage() const;

    bool SabotageByFieldOperative(
        ABHEnemySoldier* Operative,
        ABHCharacter* Commander
    );

    bool SabotageByEngineeringCharge(
        ABHCharacter* Commander,
        const AActor* ChargeActor
    );

    virtual void Interact_Implementation(
        AActor* InteractingActor
    ) override;

    virtual FText GetInteractionText_Implementation()
        const override;

private:
    bool CompleteSabotage(
        ABHCharacter* Commander,
        const AActor* SabotageActor
    );

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<USceneComponent> SceneRoot;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UStaticMeshComponent> CacheBaseMesh;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UStaticMeshComponent> CacheCrateMesh;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UTextRenderComponent> TargetLabel;

    UPROPERTY(Transient)
    TObjectPtr<ABHOpenWorldOperationDirector>
        OperationDirector;

    FName SectorID = NAME_None;
    bool bSabotaged = false;
};
