#pragma once

#include "CoreMinimal.h"
#include "BHInteractable.h"
#include "GameFramework/Actor.h"
#include "BHMissionItemContainer.generated.h"

class UStaticMeshComponent;
class UTextRenderComponent;

UCLASS(BlueprintType, Blueprintable)
class BROKENHORIZON_API ABHMissionItemContainer
    : public AActor,
      public IBHInteractable
{
    GENERATED_BODY()

public:
    ABHMissionItemContainer();

    static FText BuildInteractionText(
        FName ConfiguredMissionItemID,
        FName StoredMissionItemID
    );

    virtual void Interact_Implementation(AActor* InteractingActor) override;
    virtual FText GetInteractionText_Implementation() const override;

    UFUNCTION(BlueprintPure, Category = "Inventory|Mission Cache")
    FName GetPersistenceID() const;

    UFUNCTION(BlueprintPure, Category = "Inventory|Mission Cache")
    FName GetMissionItemID() const;

    UFUNCTION(BlueprintPure, Category = "Inventory|Mission Cache")
    FName GetStoredMissionItemID() const;

    void RestoreStoredMissionItem(FName NewStoredMissionItemID);

protected:
    virtual void BeginPlay() override;
    virtual void GetLifetimeReplicatedProps(
        TArray<FLifetimeProperty>& OutLifetimeProps
    ) const override;

    UFUNCTION()
    void OnRep_StoredMissionItemID();

    void RefreshContainerLabel();
    bool PersistContainerState(class ABHCharacter* Character) const;
    void ShowTransferFailure(class ABHCharacter* Character, const FText& Reason) const;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory|Mission Cache")
    TObjectPtr<UStaticMeshComponent> ContainerMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory|Mission Cache")
    TObjectPtr<UTextRenderComponent> ContainerLabel;

    UPROPERTY(Replicated, EditInstanceOnly, BlueprintReadOnly, Category = "Persistence")
    FName PersistenceID = NAME_None;

    UPROPERTY(Replicated, EditInstanceOnly, BlueprintReadOnly, Category = "Inventory|Mission Cache")
    FName MissionItemID = TEXT("RedKeycard");

    UPROPERTY(ReplicatedUsing = OnRep_StoredMissionItemID, VisibleAnywhere, BlueprintReadOnly, Category = "Inventory|Mission Cache")
    FName StoredMissionItemID = NAME_None;
};
