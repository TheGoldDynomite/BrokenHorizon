#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BHImpactEffect.generated.h"

class UMaterialInstanceDynamic;
class UMaterialInterface;
class UPointLightComponent;
class USceneComponent;
class UStaticMesh;
class UStaticMeshComponent;
struct FHitResult;

USTRUCT(BlueprintType)
struct BROKENHORIZON_API FBHImpactPresentationProfile
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Impact")
    int32 SparkCount = 6;

    UPROPERTY(BlueprintReadOnly, Category = "Impact")
    float SparkSpeed = 150.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Impact")
    float SparkScale = 0.009f;

    UPROPERTY(BlueprintReadOnly, Category = "Impact")
    float SparkGravityScale = 0.35f;

    UPROPERTY(BlueprintReadOnly, Category = "Impact")
    FLinearColor SparkColor =
        FLinearColor(1.0f, 0.24f, 0.025f, 1.0f);

    UPROPERTY(BlueprintReadOnly, Category = "Impact")
    float FlashIntensity = 1400.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Impact")
    float FlashRadius = 75.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Impact")
    float BulletMarkScale = 0.04f;

    UPROPERTY(BlueprintReadOnly, Category = "Impact")
    FLinearColor BulletMarkColor =
        FLinearColor(0.012f, 0.009f, 0.006f, 1.0f);

    UPROPERTY(BlueprintReadOnly, Category = "Impact")
    bool bShowBulletMark = true;
};

UCLASS()
class BROKENHORIZON_API ABHImpactEffect : public AActor
{
    GENERATED_BODY()

public:
    ABHImpactEffect();

    static FBHImpactPresentationProfile GetSurfacePresentationProfile(
        uint8 SurfaceType,
        const FBHImpactPresentationProfile& BaseProfile
    );

    void InitializeImpact(const FHitResult& HitResult);

protected:
    virtual void Tick(float DeltaTime) override;

    UPROPERTY(VisibleAnywhere, Category = "Impact")
    TObjectPtr<USceneComponent> ImpactRoot;

    UPROPERTY(VisibleAnywhere, Category = "Impact")
    TObjectPtr<UStaticMeshComponent> BulletMark;

    UPROPERTY(VisibleAnywhere, Category = "Impact")
    TObjectPtr<UPointLightComponent> ImpactFlash;

    UPROPERTY(EditDefaultsOnly, Category = "Impact|Mark")
    FLinearColor BulletMarkColor =
        FLinearColor(0.012f, 0.009f, 0.006f, 1.0f);

    UPROPERTY(
        EditDefaultsOnly,
        Category = "Impact|Mark",
        meta = (ClampMin = "0.1", Units = "s")
    )
    float BulletMarkLifeSpan = 10.0f;

    UPROPERTY(
        EditDefaultsOnly,
        Category = "Impact|Mark",
        meta = (ClampMin = "0.005")
    )
    float BulletMarkScale = 0.04f;

    UPROPERTY(
        EditDefaultsOnly,
        Category = "Impact|Burst",
        meta = (ClampMin = "0", ClampMax = "12")
    )
    int32 SparkCount = 6;

    UPROPERTY(
        EditDefaultsOnly,
        Category = "Impact|Burst",
        meta = (ClampMin = "0.01", Units = "s")
    )
    float BurstDuration = 0.18f;

    UPROPERTY(EditDefaultsOnly, Category = "Impact|Burst")
    FLinearColor SparkColor =
        FLinearColor(1.0f, 0.24f, 0.025f, 1.0f);

    UPROPERTY(
        EditDefaultsOnly,
        Category = "Impact|Burst",
        meta = (ClampMin = "0.0")
    )
    float SparkSpeed = 150.0f;

    UPROPERTY(
        EditDefaultsOnly,
        Category = "Impact|Burst",
        meta = (ClampMin = "0.001")
    )
    float SparkScale = 0.009f;

    UPROPERTY(
        EditDefaultsOnly,
        Category = "Impact|Burst",
        meta = (ClampMin = "0.0")
    )
    float SparkGravityScale = 0.35f;

    UPROPERTY(
        EditDefaultsOnly,
        Category = "Impact|Flash",
        meta = (ClampMin = "0.0")
    )
    float FlashIntensity = 1400.0f;

    UPROPERTY(
        EditDefaultsOnly,
        Category = "Impact|Flash",
        meta = (ClampMin = "0.0", Units = "cm")
    )
    float FlashRadius = 75.0f;

    UPROPERTY(
        EditDefaultsOnly,
        Category = "Impact|Flash",
        meta = (ClampMin = "0.0", Units = "s")
    )
    float FlashDuration = 0.035f;

private:
    void CreateSparks(
        const FVector& ImpactNormal,
        const FBHImpactPresentationProfile& Profile
    );
    void FinishBurst();

    UPROPERTY()
    TObjectPtr<UStaticMesh> SparkMeshAsset;

    UPROPERTY()
    TObjectPtr<UMaterialInterface> BaseShapeMaterial;

    UPROPERTY(Transient)
    TObjectPtr<UMaterialInstanceDynamic> BulletMarkMaterial;

    UPROPERTY(Transient)
    TObjectPtr<UMaterialInstanceDynamic> SparkMaterial;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UStaticMeshComponent>> SparkComponents;

    TArray<FVector> SparkVelocities;
    float ActiveSparkScale = 0.009f;
    float ImpactAge = 0.0f;
    bool bBurstFinished = false;
};
