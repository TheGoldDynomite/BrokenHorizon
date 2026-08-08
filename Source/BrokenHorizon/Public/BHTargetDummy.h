#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BHTargetDummy.generated.h"

class UBHHealthComponent;
class UStaticMeshComponent;

UCLASS()
class BROKENHORIZON_API ABHTargetDummy : public AActor
{
    GENERATED_BODY()

public:
    ABHTargetDummy();

    UFUNCTION(BlueprintPure, Category = "Health")
    UBHHealthComponent* GetHealthComponent() const;

protected:
    virtual void BeginPlay() override;

    UFUNCTION()
    void HandleDeath(AActor* DamageCauser);

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Target")
    TObjectPtr<UStaticMeshComponent> TargetMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Target")
    TObjectPtr<UBHHealthComponent> HealthComponent;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Target")
    bool bHideOnDeath = true;
};
