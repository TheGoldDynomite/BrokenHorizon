#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BHRifle.generated.h"

class AController;
class ABHImpactEffect;
class ABHWeaponCasing;
class UAnimMontage;
class UAnimSequenceBase;
class UCameraShakeBase;
class UMaterialInterface;
class UNiagaraSystem;
class UPointLightComponent;
class USceneComponent;
class USkeletalMeshComponent;
class USoundBase;
class UStaticMeshComponent;
class UStaticMesh;

USTRUCT(BlueprintType)
struct BROKENHORIZON_API FBHBallisticSurfaceResponse
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Ballistics")
    float MaximumPenetrationDepth = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Ballistics")
    float PenetrationDamageRetention = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Ballistics")
    bool bCanRicochet = false;

    UPROPERTY(BlueprintReadOnly, Category = "Ballistics")
    float MaximumRicochetIncidence = 0.0f;
};

USTRUCT(BlueprintType)
struct BROKENHORIZON_API FBHRifleConfig
{
    GENERATED_BODY()

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ammo", meta = (ClampMin = "1"))
    int32 MagazineSize = 30;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ammo", meta = (ClampMin = "0"))
    int32 StartingReserveAmmo = 90;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ammo", meta = (ClampMin = "0.0", Units = "s"))
    float ReloadDuration = 1.6f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Fire", meta = (ClampMin = "1.0"))
    float RoundsPerMinute = 600.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Fire")
    bool bAutomatic = true;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Damage", meta = (ClampMin = "0.0"))
    float Damage = 25.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Damage", meta = (ClampMin = "0.0", Units = "cm"))
    float Range = 50000.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ballistics", meta = (ClampMin = "1000.0", Units = "cm/s"))
    float MuzzleVelocity = 80000.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ballistics", meta = (ClampMin = "0.0", ClampMax = "2.0"))
    float GravityScale = 1.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ballistics", meta = (ClampMin = "0.0", Units = "cm"))
    float DamageFalloffStartDistance = 10000.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ballistics", meta = (ClampMin = "0.0", Units = "cm"))
    float DamageFalloffEndDistance = 40000.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ballistics", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float MinimumDamageRetention = 0.55f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Accuracy", meta = (ClampMin = "0.0", ClampMax = "45.0", Units = "Degrees"))
    float HipSpreadDegrees = 0.9f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Accuracy", meta = (ClampMin = "0.0", ClampMax = "45.0", Units = "Degrees"))
    float ADSSpreadDegrees = 0.08f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Accuracy", meta = (ClampMin = "0.0", ClampMax = "45.0", Units = "Degrees"))
    float SpreadPerShotDegrees = 0.11f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Accuracy", meta = (ClampMin = "0.0", ClampMax = "45.0", Units = "Degrees"))
    float MaxSpreadBloomDegrees = 1.1f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Accuracy", meta = (ClampMin = "0.0", Units = "s"))
    float SpreadRecoveryDelay = 0.18f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Accuracy", meta = (ClampMin = "0.0"))
    float SpreadRecoverySpeed = 2.4f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Recoil", meta = (ClampMin = "0.0", Units = "Degrees"))
    float RecoilPitch = 0.55f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Recoil", meta = (ClampMin = "0.0", Units = "Degrees"))
    float RecoilYaw = 0.12f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Recoil", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float ADSRecoilMultiplier = 0.72f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Recoil", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float RecoilPitchVariation = 0.1f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Recoil", meta = (ClampMin = "0.0", Units = "s"))
    float RecoilRecoveryDelay = 0.18f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Recoil", meta = (ClampMin = "0.0"))
    float RecoilRecoverySpeed = 5.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ballistics", meta = (ClampMin = "0.0", Units = "cm"))
    float MuzzleObstructionCheckDistance = 150.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Aim", meta = (ClampMin = "5.0", ClampMax = "170.0", Units = "Degrees"))
    float ADSFieldOfView = 65.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Aim", meta = (ClampMin = "0.1"))
    float FOVInterpSpeed = 12.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI Hearing", meta = (ClampMin = "0.0"))
    float NoiseLoudness = 1.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI Hearing", meta = (ClampMin = "0.0", Units = "cm"))
    float NoiseRange = 3500.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI Hearing", meta = (ClampMin = "0.0"))
    float IndoorNoiseLoudnessMultiplier = 1.15f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI Hearing", meta = (ClampMin = "0.0"))
    float IndoorNoiseRangeMultiplier = 1.10f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Suppression", meta = (ClampMin = "0.0", Units = "cm"))
    float SuppressionRadius = 250.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Suppression", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float SuppressionAmount = 0.35f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Suppression", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float SuppressionMinimumIntensity = 0.20f;

};

UCLASS()
class BROKENHORIZON_API ABHRifle : public AActor
{
    GENERATED_BODY()

public:
    ABHRifle();

    const FBHRifleConfig& GetConfig() const;

    void ApplyWeaponConfig(const FBHRifleConfig& NewConfig);

    bool PerformHitscan(
        const FVector& CameraOrigin,
        const FVector& CameraDirection,
        float SpreadDegrees,
        AController* InstigatorController,
        FHitResult* OutHitResult = nullptr,
        bool* bOutHadBlockingHit = nullptr
    );

    void PlayReplicatedFirePresentation(
        const FHitResult& HitResult,
        bool bHadBlockingHit
    );

    void PlayPredictedFirePresentation();

    void PlayReplicatedImpactPresentation(
        const FHitResult& HitResult,
        bool bHadBlockingHit
    );

    void SetFirstPersonPresentation(bool bFirstPerson);

    static bool ShouldUseIndoorFireTail(
        int32 BlockedProbeCount,
        int32 TotalProbeCount
    );

    static float CalculateBulletDropCentimeters(
        float DistanceCentimeters,
        float MuzzleVelocityCentimetersPerSecond,
        float GravityScale
    );

    static float CalculateDamageRetention(
        float DistanceCentimeters,
        float FalloffStartCentimeters,
        float FalloffEndCentimeters,
        float MinimumRetention
    );

    static FBHBallisticSurfaceResponse GetSurfaceResponse(
        uint8 SurfaceType
    );

    static bool CanPenetrateSurface(
        const FBHBallisticSurfaceResponse& Response,
        float ThicknessCentimeters
    );

    static bool ShouldRicochet(
        const FBHBallisticSurfaceResponse& Response,
        float IncidenceDot
    );

    void PlayDryFirePresentation();
    void PlayReloadPresentation(float DurationOverride = -1.0f);
    void SetAimPresentation(bool bNewIsAiming);

    UFUNCTION(BlueprintPure, Category = "Rifle|Presentation")
    FTransform GetPresentationMuzzleTransform() const;

protected:
    virtual void BeginPlay() override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rifle")
    TObjectPtr<USceneComponent> RifleRoot;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rifle")
    TObjectPtr<UStaticMeshComponent> RifleMesh;

    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "Rifle|Presentation"
    )
    TObjectPtr<USkeletalMeshComponent> RifleSkeletalMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rifle")
    TObjectPtr<USceneComponent> MuzzlePoint;

    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "Rifle|Presentation|Casing"
    )
    TObjectPtr<USceneComponent> EjectionPoint;

    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "Rifle|Presentation|VFX"
    )
    TObjectPtr<UPointLightComponent> MuzzleFlashLight;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rifle", meta = (ShowOnlyInnerProperties))
    FBHRifleConfig RifleConfig;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Rifle|Presentation"
    )
    FName MuzzleSocketName = TEXT("Muzzle");

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Rifle|Presentation|Animation"
    )
    TObjectPtr<UAnimMontage> FirstPersonFireMontage;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Rifle|Presentation|Animation"
    )
    TObjectPtr<UAnimMontage> FirstPersonReloadMontage;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Rifle|Presentation|Animation"
    )
    TObjectPtr<UAnimSequenceBase> FirstPersonFireAnimation;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Rifle|Presentation|Animation"
    )
    TObjectPtr<UAnimSequenceBase> FirstPersonReloadAnimation;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Rifle|Presentation|Animation"
    )
    TObjectPtr<UAnimSequenceBase> WeaponFireAnimation;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Rifle|Presentation|Animation"
    )
    TObjectPtr<UAnimSequenceBase> WeaponReloadAnimation;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Rifle|Presentation|VFX"
    )
    TObjectPtr<UNiagaraSystem> MuzzleFlashEffect;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Rifle|Presentation|VFX"
    )
    TObjectPtr<UNiagaraSystem> ImpactEffect;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Rifle|Presentation|VFX"
    )
    TObjectPtr<UMaterialInterface> ImpactDecalMaterial;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Rifle|Presentation|VFX"
    )
    TSubclassOf<ABHImpactEffect> ImpactActorClass;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Rifle|Presentation|VFX"
    )
    bool bEnableMuzzleFlashLight = true;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Rifle|Presentation|VFX",
        meta = (ClampMin = "0.0")
    )
    float MuzzleFlashLightIntensity = 4500.0f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Rifle|Presentation|VFX",
        meta = (ClampMin = "0.0", Units = "cm")
    )
    float MuzzleFlashLightRadius = 225.0f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Rifle|Presentation|VFX",
        meta = (ClampMin = "0.0", Units = "s")
    )
    float MuzzleFlashLightDuration = 0.04f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Rifle|Presentation|VFX"
    )
    FLinearColor MuzzleFlashLightColor =
        FLinearColor(1.0f, 0.42f, 0.08f);

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Rifle|Presentation|Casing"
    )
    TSubclassOf<ABHWeaponCasing> CasingClass;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Rifle|Presentation|Casing"
    )
    TObjectPtr<UStaticMesh> CasingMesh;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Rifle|Presentation|Casing"
    )
    FVector CasingEjectionVelocity = FVector(0.0f, 220.0f, 80.0f);

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Rifle|Presentation|Casing"
    )
    FVector CasingAngularVelocityRadians =
        FVector(12.0f, 18.0f, 24.0f);

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Rifle|Presentation|Casing",
        meta = (ClampMin = "0.1", Units = "s")
    )
    float CasingLifeSpan = 4.0f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Rifle|Presentation|VFX",
        meta = (ClampMin = "0.0", Units = "cm")
    )
    FVector ImpactDecalSize = FVector(8.0f);

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Rifle|Presentation|VFX",
        meta = (ClampMin = "0.0", Units = "s")
    )
    float ImpactDecalLifeSpan = 12.0f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Rifle|Presentation|Audio"
    )
    TObjectPtr<USoundBase> FireSound;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rifle|Presentation|Audio")
    TObjectPtr<USoundBase> IndoorFireTailSound;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rifle|Presentation|Audio")
    TObjectPtr<USoundBase> OutdoorFireTailSound;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Rifle|Presentation|Audio"
    )
    TObjectPtr<USoundBase> DryFireSound;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Rifle|Presentation|Audio"
    )
    TObjectPtr<USoundBase> ReloadSound;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Rifle|Presentation|Camera"
    )
    TSubclassOf<UCameraShakeBase> FireCameraShake;

    UFUNCTION(BlueprintImplementableEvent, Category = "Rifle|Presentation")
    void OnFirePresentation(
        const FHitResult& HitResult,
        bool bHadBlockingHit
    );

    UFUNCTION(BlueprintImplementableEvent, Category = "Rifle|Presentation")
    void OnDryFirePresentation();

    UFUNCTION(BlueprintImplementableEvent, Category = "Rifle|Presentation")
    void OnReloadPresentation();

    UFUNCTION(BlueprintImplementableEvent, Category = "Rifle|Presentation")
    void OnAimPresentationChanged(bool bNewIsAiming);

private:
    void PlayFirePresentation(
        const FHitResult& HitResult,
        bool bHadBlockingHit
    );

    void PlayImpactPresentation(
        const FHitResult& HitResult,
        bool bHadBlockingHit
    );

    void HideMuzzleFlashLight();
    void SpawnCasing();
    bool IsMuzzleEnvironmentEnclosed(const FVector& MuzzleLocation) const;

    bool PlayFirstPersonMontage(UAnimMontage* Montage) const;

    void PlayFirstPersonSequence(
        UAnimSequenceBase* ArmsAnimation,
        UAnimSequenceBase* RifleAnimation
    ) const;

    FTimerHandle MuzzleFlashLightTimerHandle;

    mutable bool bHasMuzzleEnvironmentCache = false;
    mutable bool bCachedMuzzleEnvironmentEnclosed = false;
    mutable FVector CachedMuzzleEnvironmentLocation = FVector::ZeroVector;
    mutable FQuat CachedMuzzleEnvironmentRotation = FQuat::Identity;
    mutable float CachedMuzzleEnvironmentSampleTime = -BIG_NUMBER;
};
