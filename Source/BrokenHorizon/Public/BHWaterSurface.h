#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BHWaterSurface.generated.h"

class UBoxComponent;
class UStaticMeshComponent;
class UTextRenderComponent;

UCLASS(BlueprintType, Blueprintable)
class BROKENHORIZON_API ABHWaterSurface : public AActor
{
    GENERATED_BODY()

public:
    ABHWaterSurface();
    virtual void OnConstruction(const FTransform& Transform) override;
    virtual void PostRegisterAllComponents() override;

    UFUNCTION(BlueprintPure, Category = "Water")
    bool ContainsWorldLocation(FVector WorldLocation) const;

    UFUNCTION(BlueprintPure, Category = "Water")
    float GetSurfaceHeight() const;

    UFUNCTION(BlueprintPure, Category = "Water")
    float GetInfantrySpeedMultiplier() const;

    UFUNCTION(BlueprintPure, Category = "Water")
    FVector GetSurfaceExtents() const;

protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Water")
    TObjectPtr<UBoxComponent> WaterVolume;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Water")
    TObjectPtr<UStaticMeshComponent> WaterMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Water")
    TObjectPtr<UTextRenderComponent> WaterLabel;

    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Water")
    FName WaterID = TEXT("FirstLightWaterRoute01");

    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Water", meta = (ClampMin = "100.0"))
    FVector SurfaceExtents = FVector(5000.0f, 1600.0f, 120.0f);

    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Water", meta = (Units = "cm"))
    float SurfaceHeight = 0.0f;

    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Water", meta = (ClampMin = "0.25", ClampMax = "1.0"))
    float InfantrySpeedMultiplier = 0.60f;

private:
    void SynchronizeGeometry();
};
