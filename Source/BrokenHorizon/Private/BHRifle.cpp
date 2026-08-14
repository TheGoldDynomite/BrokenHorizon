#include "BHRifle.h"

#include "BHCharacter.h"
#include "BHBattlefieldConditions.h"
#include "BHEnemySoldier.h"
#include "BHHealthComponent.h"
#include "BHImpactEffect.h"
#include "BHUserSettingsSubsystem.h"
#include "BHWeaponCasing.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequenceBase.h"
#include "Camera/CameraShakeBase.h"
#include "Components/PointLightComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/DamageEvents.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/DamageType.h"
#include "HAL/PlatformMisc.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Perception/AISense_Hearing.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "Sound/SoundBase.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
#if !UE_BUILD_SHIPPING
bool bBHBallisticsRuntimeProbeCompleted = false;
#endif
}

ABHRifle::ABHRifle()
{
    PrimaryActorTick.bCanEverTick = false;

    RifleRoot = CreateDefaultSubobject<USceneComponent>(
        TEXT("RifleRoot")
    );
    SetRootComponent(RifleRoot);

    RifleMesh = CreateDefaultSubobject<UStaticMeshComponent>(
        TEXT("RifleMesh")
    );
    RifleMesh->SetupAttachment(RifleRoot);
    RifleMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    RifleMesh->SetCastShadow(false);

    RifleSkeletalMesh = CreateDefaultSubobject<USkeletalMeshComponent>(
        TEXT("RifleSkeletalMesh")
    );
    RifleSkeletalMesh->SetupAttachment(RifleRoot);
    RifleSkeletalMesh->SetCollisionEnabled(
        ECollisionEnabled::NoCollision
    );
    RifleSkeletalMesh->SetOnlyOwnerSee(true);
    RifleSkeletalMesh->SetCastShadow(false);

    MuzzlePoint = CreateDefaultSubobject<USceneComponent>(
        TEXT("MuzzlePoint")
    );
    MuzzlePoint->SetupAttachment(RifleSkeletalMesh);
    MuzzlePoint->SetRelativeLocation(FVector(100.0f, 0.0f, 0.0f));

    EjectionPoint = CreateDefaultSubobject<USceneComponent>(
        TEXT("EjectionPoint")
    );
    EjectionPoint->SetupAttachment(RifleSkeletalMesh);
    EjectionPoint->SetRelativeLocation(
        FVector(35.0f, 15.0f, -10.0f)
    );

    MuzzleFlashLight = CreateDefaultSubobject<UPointLightComponent>(
        TEXT("MuzzleFlashLight")
    );
    MuzzleFlashLight->SetupAttachment(MuzzlePoint);
    MuzzleFlashLight->SetRelativeLocation(FVector::ZeroVector);
    MuzzleFlashLight->SetCastShadows(false);
    MuzzleFlashLight->SetVisibility(false);
    MuzzleFlashLight->SetHiddenInGame(true);

    CasingClass = ABHWeaponCasing::StaticClass();
    ImpactActorClass = ABHImpactEffect::StaticClass();

    SetActorEnableCollision(false);

    const ConstructorHelpers::FObjectFinder<USoundBase> FireSoundAsset(
        TEXT("/Game/BrokenHorizon/Audio/SW_FirstLight_WeaponFire.SW_FirstLight_WeaponFire")
    );
    if (FireSoundAsset.Succeeded())
    {
        FireSound = FireSoundAsset.Object;
    }
    const ConstructorHelpers::FObjectFinder<USoundBase> IndoorTailAsset(
        TEXT("/Game/BrokenHorizon/Audio/SW_FirstLight_IndoorTail.SW_FirstLight_IndoorTail")
    );
    if (IndoorTailAsset.Succeeded())
    {
        IndoorFireTailSound = IndoorTailAsset.Object;
    }
    const ConstructorHelpers::FObjectFinder<USoundBase> OutdoorTailAsset(
        TEXT("/Game/BrokenHorizon/Audio/SW_FirstLight_OutdoorTail.SW_FirstLight_OutdoorTail")
    );
    if (OutdoorTailAsset.Succeeded())
    {
        OutdoorFireTailSound = OutdoorTailAsset.Object;
    }
    const ConstructorHelpers::FObjectFinder<USoundBase> DryFireAsset(
        TEXT("/Game/BrokenHorizon/Audio/SW_FirstLight_WeaponDry.SW_FirstLight_WeaponDry")
    );
    if (DryFireAsset.Succeeded())
    {
        DryFireSound = DryFireAsset.Object;
    }
    const ConstructorHelpers::FObjectFinder<USoundBase> ReloadAsset(
        TEXT("/Game/BrokenHorizon/Audio/SW_FirstLight_Reload.SW_FirstLight_Reload")
    );
    if (ReloadAsset.Succeeded())
    {
        ReloadSound = ReloadAsset.Object;
    }
}

void ABHRifle::BeginPlay()
{
    Super::BeginPlay();
#if !UE_BUILD_SHIPPING
    if (GetOwner() && GetOwner()->HasAuthority() &&
        FParse::Param(FCommandLine::Get(), TEXT("BHTestWeaponAudioRuntime")))
    {
        const FString FireSoundPath = IsValid(FireSound)
            ? FireSound->GetPathName()
            : FString();
        UE_LOG(
            LogTemp,
            Display,
            TEXT(
                "BH_WEAPON_AUDIO_RUNTIME fire=%s project_owned=%d "
                "dry=%d reload=%d"
            ),
            *FireSoundPath,
            FireSoundPath.StartsWith(TEXT("/Game/BrokenHorizon/Audio/"))
                ? 1
                : 0,
            IsValid(DryFireSound) ? 1 : 0,
            IsValid(ReloadSound) ? 1 : 0
        );
    }

    if (!bBHBallisticsRuntimeProbeCompleted &&
        GetOwner() && GetOwner()->HasAuthority() &&
        FParse::Param(FCommandLine::Get(), TEXT("BHTestBallisticsRuntime")))
    {
        bBHBallisticsRuntimeProbeCompleted = true;
        const float Drop = CalculateBulletDropCentimeters(
            50000.0f, RifleConfig.MuzzleVelocity, RifleConfig.GravityScale
        );
        const float Retention = CalculateDamageRetention(
            40000.0f,
            RifleConfig.DamageFalloffStartDistance,
            RifleConfig.DamageFalloffEndDistance,
            RifleConfig.MinimumDamageRetention
        );
        const bool bWoodPenetrates = CanPenetrateSurface(
            GetSurfaceResponse(SurfaceType6), 12.0f
        );
        const bool bMetalRicochets = ShouldRicochet(
            GetSurfaceResponse(SurfaceType4), 0.15f
        );
        const bool bSuccess = Drop > 0.0f && Retention < 1.0f &&
            bWoodPenetrates && bMetalRicochets;
        UE_LOG(
            LogTemp,
            Display,
            TEXT("BH_BALLISTICS_RUNTIME result=%s drop_cm=%.1f retention=%.2f wood_penetrates=%d metal_ricochets=%d"),
            bSuccess ? TEXT("success") : TEXT("failure"),
            Drop,
            Retention,
            bWoodPenetrates ? 1 : 0,
            bMetalRicochets ? 1 : 0
        );
        FPlatformMisc::RequestExit(false);
    }
#endif
}

const FBHRifleConfig& ABHRifle::GetConfig() const
{
    return RifleConfig;
}

void ABHRifle::ApplyWeaponConfig(
    const FBHRifleConfig& NewConfig
)
{
    RifleConfig = NewConfig;
}

float ABHRifle::CalculateBulletDropCentimeters(
    float DistanceCentimeters,
    float MuzzleVelocityCentimetersPerSecond,
    float GravityScale
)
{
    if (DistanceCentimeters <= 0.0f ||
        MuzzleVelocityCentimetersPerSecond <= 0.0f ||
        GravityScale <= 0.0f)
    {
        return 0.0f;
    }
    const float FlightTime = DistanceCentimeters /
        MuzzleVelocityCentimetersPerSecond;
    return 0.5f * 980.0f * GravityScale *
        FlightTime * FlightTime;
}

float ABHRifle::CalculateDamageRetention(
    float DistanceCentimeters,
    float FalloffStartCentimeters,
    float FalloffEndCentimeters,
    float MinimumRetention
)
{
    const float SafeMinimum = FMath::Clamp(MinimumRetention, 0.0f, 1.0f);
    const float SafeStart = FMath::Max(0.0f, FalloffStartCentimeters);
    const float SafeEnd = FMath::Max(SafeStart, FalloffEndCentimeters);
    if (DistanceCentimeters <= SafeStart)
    {
        return 1.0f;
    }
    if (SafeEnd <= SafeStart || DistanceCentimeters >= SafeEnd)
    {
        return SafeMinimum;
    }
    return FMath::Lerp(
        1.0f,
        SafeMinimum,
        (DistanceCentimeters - SafeStart) / (SafeEnd - SafeStart)
    );
}

FBHBallisticSurfaceResponse ABHRifle::GetSurfaceResponse(uint8 SurfaceType)
{
    FBHBallisticSurfaceResponse Response;
    switch (SurfaceType)
    {
        case SurfaceType1: // Concrete/masonry.
            Response.bCanRicochet = true;
            Response.MaximumRicochetIncidence = 0.22f;
            break;
        case SurfaceType4: // Structural metal.
            Response.MaximumPenetrationDepth = 1.5f;
            Response.PenetrationDamageRetention = 0.30f;
            Response.bCanRicochet = true;
            Response.MaximumRicochetIncidence = 0.30f;
            break;
        case SurfaceType6: // Timber.
            Response.MaximumPenetrationDepth = 28.0f;
            Response.PenetrationDamageRetention = 0.62f;
            break;
        case SurfaceType7: // Glass/light panels.
            Response.MaximumPenetrationDepth = 8.0f;
            Response.PenetrationDamageRetention = 0.82f;
            break;
        case SurfaceType8: // Flesh.
            Response.MaximumPenetrationDepth = 35.0f;
            Response.PenetrationDamageRetention = 0.58f;
            break;
        default: // Untagged construction is conservative drywall/light cover.
            Response.MaximumPenetrationDepth = 12.0f;
            Response.PenetrationDamageRetention = 0.48f;
            break;
    }
    return Response;
}

bool ABHRifle::CanPenetrateSurface(
    const FBHBallisticSurfaceResponse& Response,
    float ThicknessCentimeters
)
{
    return ThicknessCentimeters > 0.0f &&
        Response.MaximumPenetrationDepth > 0.0f &&
        ThicknessCentimeters <= Response.MaximumPenetrationDepth &&
        Response.PenetrationDamageRetention > 0.0f;
}

bool ABHRifle::ShouldRicochet(
    const FBHBallisticSurfaceResponse& Response,
    float IncidenceDot
)
{
    return Response.bCanRicochet &&
        FMath::Clamp(IncidenceDot, 0.0f, 1.0f) <=
            FMath::Clamp(Response.MaximumRicochetIncidence, 0.0f, 1.0f);
}

FTransform ABHRifle::GetPresentationMuzzleTransform() const
{
    if (IsValid(RifleSkeletalMesh) &&
        !MuzzleSocketName.IsNone() &&
        RifleSkeletalMesh->DoesSocketExist(MuzzleSocketName))
    {
        return RifleSkeletalMesh->GetSocketTransform(
            MuzzleSocketName
        );
    }

    if (IsValid(RifleMesh) &&
        !MuzzleSocketName.IsNone() &&
        RifleMesh->DoesSocketExist(MuzzleSocketName))
    {
        return RifleMesh->GetSocketTransform(MuzzleSocketName);
    }

    if (IsValid(MuzzlePoint))
    {
        return MuzzlePoint->GetComponentTransform();
    }

    return GetActorTransform();
}

bool ABHRifle::PerformHitscan(
    const FVector& CameraOrigin,
    const FVector& CameraDirection,
    float SpreadDegrees,
    AController* InstigatorController,
    FHitResult* OutHitResult,
    bool* bOutHadBlockingHit
)
{
    if (OutHitResult)
    {
        *OutHitResult = FHitResult();
    }

    if (bOutHadBlockingHit)
    {
        *bOutHadBlockingHit = false;
    }

    UWorld* World = GetWorld();
    AActor* WeaponOwner = GetOwner();

    if (!IsValid(World) ||
        !IsValid(WeaponOwner) ||
        !WeaponOwner->HasAuthority() ||
        RifleConfig.Range <= 0.0f)
    {
        return false;
    }

    const FVector SafeCameraDirection =
        CameraDirection.GetSafeNormal();

    if (SafeCameraDirection.IsNearlyZero())
    {
        return false;
    }

    const float SpreadRadians = FMath::DegreesToRadians(
        FMath::Max(0.0f, SpreadDegrees)
    );
    const FVector ShotDirection = FMath::VRandCone(
        SafeCameraDirection,
        SpreadRadians
    );
    const float BulletDrop = CalculateBulletDropCentimeters(
        RifleConfig.Range,
        RifleConfig.MuzzleVelocity,
        RifleConfig.GravityScale
    );
    const FVector CameraTraceEnd = CameraOrigin +
        (ShotDirection * RifleConfig.Range) -
        (FVector::UpVector * BulletDrop);

    FCollisionQueryParams CameraQuery(
        SCENE_QUERY_STAT(BHRifleCameraTrace),
        true,
        WeaponOwner
    );
    CameraQuery.AddIgnoredActor(this);
    CameraQuery.AddIgnoredActor(WeaponOwner);
    CameraQuery.bReturnPhysicalMaterial = true;

    FHitResult CameraHit;
    bool bCameraHit = false;
    FVector BallisticSegmentStart = CameraOrigin;
    constexpr int32 BallisticTraceSegments = 16;
    for (int32 Segment = 1;
         Segment <= BallisticTraceSegments && !bCameraHit;
         ++Segment)
    {
        const float SegmentDistance = RifleConfig.Range *
            (static_cast<float>(Segment) / BallisticTraceSegments);
        const FVector BallisticSegmentEnd = CameraOrigin +
            ShotDirection * SegmentDistance -
            FVector::UpVector * CalculateBulletDropCentimeters(
                SegmentDistance,
                RifleConfig.MuzzleVelocity,
                RifleConfig.GravityScale
            );
        bCameraHit = World->LineTraceSingleByChannel(
            CameraHit,
            BallisticSegmentStart,
            BallisticSegmentEnd,
            ECC_Visibility,
            CameraQuery
        );
        BallisticSegmentStart = BallisticSegmentEnd;
    }
    const FVector MuzzleOrigin =
        GetPresentationMuzzleTransform().GetLocation();
    const bool bMuzzleEnclosed = IsMuzzleEnvironmentEnclosed(
        MuzzleOrigin
    );

    if (RifleConfig.NoiseLoudness > 0.0f)
    {
        const float AcousticLoudnessMultiplier = bMuzzleEnclosed
            ? FMath::Max(
                0.0f,
                RifleConfig.IndoorNoiseLoudnessMultiplier
            )
            : 1.0f;
        const float AcousticRangeMultiplier = bMuzzleEnclosed
            ? FMath::Max(
                0.0f,
                RifleConfig.IndoorNoiseRangeMultiplier
            )
            : 1.0f;
        const float BattlefieldNoiseMultiplier = FMath::Max(
            0.0f,
            UBHBattlefieldConditions::GetCurrentProfile(this).
                GunfireNoiseMultiplier
        );
        UAISense_Hearing::ReportNoiseEvent(
            World,
            MuzzleOrigin,
            RifleConfig.NoiseLoudness *
                AcousticLoudnessMultiplier *
                BattlefieldNoiseMultiplier,
            WeaponOwner,
            RifleConfig.NoiseRange *
                AcousticRangeMultiplier *
                BattlefieldNoiseMultiplier,
            TEXT("Gunfire")
        );
    }

    FHitResult MuzzleObstructionHit;
    bool bMuzzleObstructed = false;
    const float ObstructionDistance = FMath::Max(
        0.0f,
        RifleConfig.MuzzleObstructionCheckDistance
    );

    if (ObstructionDistance > 0.0f)
    {
        FCollisionQueryParams MuzzleQuery(
            SCENE_QUERY_STAT(BHRifleMuzzleObstructionTrace),
            true,
            WeaponOwner
        );
        MuzzleQuery.AddIgnoredActor(this);
        MuzzleQuery.AddIgnoredActor(WeaponOwner);
        MuzzleQuery.bReturnPhysicalMaterial = true;

        bMuzzleObstructed = World->LineTraceSingleByChannel(
            MuzzleObstructionHit,
            MuzzleOrigin,
            MuzzleOrigin + (ShotDirection * ObstructionDistance),
            ECC_Visibility,
            MuzzleQuery
        );
    }

    const bool bWeaponHit = bMuzzleObstructed || bCameraHit;
    const FHitResult& WeaponHit = bMuzzleObstructed
        ? MuzzleObstructionHit
        : CameraHit;

    if (OutHitResult && bWeaponHit)
    {
        *OutHitResult = WeaponHit;
    }

    if (bOutHadBlockingHit)
    {
        *bOutHadBlockingHit = bWeaponHit;
    }

    if (GetNetMode() != NM_DedicatedServer)
    {
        PlayFirePresentation(WeaponHit, bWeaponHit);
    }

    if (RifleConfig.SuppressionRadius > 0.0f &&
        RifleConfig.SuppressionAmount > 0.0f)
    {
        const FVector SuppressionTraceEnd = bWeaponHit
            ? WeaponHit.ImpactPoint
            : CameraTraceEnd;

        for (TActorIterator<ABHEnemySoldier> It(World); It; ++It)
        {
            ABHEnemySoldier* Enemy = *It;

            if (!IsValid(Enemy) ||
                Enemy->IsDead() ||
                !Enemy->IsHostileTo(WeaponOwner) ||
                Enemy == WeaponHit.GetActor())
            {
                continue;
            }

            const FVector EnemyTargetLocation =
                Enemy->GetActorLocation() +
                    FVector(0.0f, 0.0f, 60.0f);
            const FVector ClosestPoint =
                FMath::ClosestPointOnSegment(
                    EnemyTargetLocation,
                    MuzzleOrigin,
                    SuppressionTraceEnd
                );
            const float DistanceToShot = FVector::Distance(
                EnemyTargetLocation,
                ClosestPoint
            );

            if (DistanceToShot <= RifleConfig.SuppressionRadius)
            {
                const float Proximity = 1.0f - FMath::Clamp(
                    DistanceToShot / RifleConfig.SuppressionRadius,
                    0.0f,
                    1.0f
                );
                const float SuppressionIntensity = FMath::Lerp(
                    FMath::Clamp(
                        RifleConfig.SuppressionMinimumIntensity,
                        0.0f,
                        1.0f
                    ),
                    1.0f,
                    Proximity
                );
                Enemy->ApplySuppression(
                    FMath::Clamp(
                        RifleConfig.SuppressionAmount,
                        0.0f,
                        1.0f
                    ) * SuppressionIntensity,
                    WeaponOwner
                );
            }
        }
    }

    if (!bWeaponHit || !IsValid(WeaponHit.GetActor()))
    {
        return false;
    }

    float DamageToApply = RifleConfig.Damage * CalculateDamageRetention(
        FVector::Distance(CameraOrigin, WeaponHit.ImpactPoint),
        RifleConfig.DamageFalloffStartDistance,
        RifleConfig.DamageFalloffEndDistance,
        RifleConfig.MinimumDamageRetention
    );
    bool bHeadshot = false;
    bool bArmorHit = false;

    if (const ABHEnemySoldier* Enemy =
        Cast<ABHEnemySoldier>(WeaponHit.GetActor()))
    {
        const EBHHitZone HitZone =
            Enemy->ResolveHitZone(WeaponHit);
        DamageToApply *=
            Enemy->GetHitZoneDamageMultiplier(HitZone);
        bHeadshot = HitZone == EBHHitZone::Head;
        bArmorHit = Enemy->IsArmorMitigatingHitZone(HitZone);
    }

    const float DamageApplied = UGameplayStatics::ApplyPointDamage(
        WeaponHit.GetActor(),
        DamageToApply,
        ShotDirection,
        WeaponHit,
        InstigatorController,
        this,
        UDamageType::StaticClass()
    );

    if (DamageApplied > 0.0f)
    {
        const UBHHealthComponent* HitHealth =
            WeaponHit.GetActor()
                ->FindComponentByClass<UBHHealthComponent>();
        const bool bLethalHit =
            IsValid(HitHealth) && HitHealth->IsDead();

        if (ABHCharacter* CharacterOwner =
            Cast<ABHCharacter>(WeaponOwner))
        {
            CharacterOwner->ShowDetailedHitConfirmation(
                bLethalHit,
                bHeadshot,
                bArmorHit
            );
        }
    }

    const uint8 SurfaceType = static_cast<uint8>(
        UPhysicalMaterial::DetermineSurfaceType(
            WeaponHit.PhysMaterial.Get()
        )
    );
    const FBHBallisticSurfaceResponse SurfaceResponse =
        GetSurfaceResponse(SurfaceType);
    FHitResult SecondaryHit;
    float SecondaryDamageScale = 0.0f;
    FVector SecondaryDirection = ShotDirection;

    if (SurfaceResponse.MaximumPenetrationDepth > 0.0f)
    {
        const FVector ReverseStart = WeaponHit.ImpactPoint +
            ShotDirection *
                (SurfaceResponse.MaximumPenetrationDepth + 2.0f);
        const FVector ReverseEnd = WeaponHit.ImpactPoint +
            ShotDirection * 1.0f;
        FCollisionQueryParams ExitQuery(
            SCENE_QUERY_STAT(BHRiflePenetrationExit),
            true,
            WeaponOwner
        );
        ExitQuery.AddIgnoredActor(this);
        ExitQuery.AddIgnoredActor(WeaponOwner);
        ExitQuery.bReturnPhysicalMaterial = true;
        FHitResult ExitHit;
        const bool bFoundExit = World->LineTraceSingleByChannel(
            ExitHit,
            ReverseStart,
            ReverseEnd,
            ECC_Visibility,
            ExitQuery
        );
        const float Thickness = bFoundExit
            ? FVector::Distance(
                WeaponHit.ImpactPoint,
                ExitHit.ImpactPoint
            )
            : BIG_NUMBER;
        if (bFoundExit &&
            ExitHit.GetComponent() == WeaponHit.GetComponent() &&
            CanPenetrateSurface(SurfaceResponse, Thickness))
        {
            FCollisionQueryParams ContinuationQuery(
                SCENE_QUERY_STAT(BHRiflePenetrationContinuation),
                true,
                WeaponOwner
            );
            ContinuationQuery.AddIgnoredActor(this);
            ContinuationQuery.AddIgnoredActor(WeaponOwner);
            ContinuationQuery.AddIgnoredActor(WeaponHit.GetActor());
            ContinuationQuery.bReturnPhysicalMaterial = true;
            const FVector ContinuationStart =
                ExitHit.ImpactPoint + ShotDirection * 2.0f;
            if (World->LineTraceSingleByChannel(
                    SecondaryHit,
                    ContinuationStart,
                    CameraTraceEnd,
                    ECC_Visibility,
                    ContinuationQuery
                ))
            {
                const float ThicknessFraction = FMath::Clamp(
                    Thickness /
                        SurfaceResponse.MaximumPenetrationDepth,
                    0.0f,
                    1.0f
                );
                SecondaryDamageScale =
                    SurfaceResponse.PenetrationDamageRetention *
                    FMath::Lerp(1.0f, 0.65f, ThicknessFraction);
            }
        }
    }

    if (SecondaryDamageScale <= 0.0f &&
        ShouldRicochet(
            SurfaceResponse,
            FVector::DotProduct(
                -ShotDirection,
                WeaponHit.ImpactNormal.GetSafeNormal()
            )
        ))
    {
        SecondaryDirection = FMath::GetReflectionVector(
            ShotDirection,
            WeaponHit.ImpactNormal.GetSafeNormal()
        ).GetSafeNormal();
        FCollisionQueryParams RicochetQuery(
            SCENE_QUERY_STAT(BHRifleRicochet),
            true,
            WeaponOwner
        );
        RicochetQuery.AddIgnoredActor(this);
        RicochetQuery.AddIgnoredActor(WeaponOwner);
        RicochetQuery.AddIgnoredActor(WeaponHit.GetActor());
        RicochetQuery.bReturnPhysicalMaterial = true;
        if (World->LineTraceSingleByChannel(
                SecondaryHit,
                WeaponHit.ImpactPoint + SecondaryDirection * 2.0f,
                WeaponHit.ImpactPoint + SecondaryDirection * 1500.0f,
                ECC_Visibility,
                RicochetQuery
            ))
        {
            SecondaryDamageScale = 0.28f;
        }
    }

    float SecondaryDamageApplied = 0.0f;
    if (SecondaryDamageScale > 0.0f &&
        IsValid(SecondaryHit.GetActor()))
    {
        float SecondaryDamage = RifleConfig.Damage *
            SecondaryDamageScale * CalculateDamageRetention(
                FVector::Distance(
                    CameraOrigin,
                    SecondaryHit.ImpactPoint
                ),
                RifleConfig.DamageFalloffStartDistance,
                RifleConfig.DamageFalloffEndDistance,
                RifleConfig.MinimumDamageRetention
            );
        if (const ABHEnemySoldier* SecondaryEnemy =
            Cast<ABHEnemySoldier>(SecondaryHit.GetActor()))
        {
            SecondaryDamage *= SecondaryEnemy->GetHitZoneDamageMultiplier(
                SecondaryEnemy->ResolveHitZone(SecondaryHit)
            );
        }
        SecondaryDamageApplied = UGameplayStatics::ApplyPointDamage(
            SecondaryHit.GetActor(),
            SecondaryDamage,
            SecondaryDirection,
            SecondaryHit,
            InstigatorController,
            this,
            UDamageType::StaticClass()
        );
        UE_LOG(
            LogTemp,
            Verbose,
            TEXT("BH_BALLISTIC_CONTINUATION surface=%d scale=%.2f damage=%.1f"),
            SurfaceType,
            SecondaryDamageScale,
            SecondaryDamageApplied
        );
    }

    return DamageApplied > 0.0f || SecondaryDamageApplied > 0.0f;
}

void ABHRifle::PlayReplicatedFirePresentation(
    const FHitResult& HitResult,
    bool bHadBlockingHit
)
{
    PlayFirePresentation(HitResult, bHadBlockingHit);
}

void ABHRifle::PlayPredictedFirePresentation()
{
    PlayFirePresentation(FHitResult(), false);
}

void ABHRifle::PlayReplicatedImpactPresentation(
    const FHitResult& HitResult,
    bool bHadBlockingHit
)
{
    PlayImpactPresentation(HitResult, bHadBlockingHit);
}

void ABHRifle::SetFirstPersonPresentation(bool bFirstPerson)
{
    if (IsValid(RifleMesh))
    {
        RifleMesh->SetOnlyOwnerSee(bFirstPerson);
        RifleMesh->SetOwnerNoSee(false);
    }

    if (IsValid(RifleSkeletalMesh))
    {
        RifleSkeletalMesh->SetOnlyOwnerSee(bFirstPerson);
        RifleSkeletalMesh->SetOwnerNoSee(false);
    }
}

bool ABHRifle::ShouldUseIndoorFireTail(
    int32 BlockedProbeCount,
    int32 TotalProbeCount
)
{
    return TotalProbeCount > 0 &&
        FMath::Clamp(BlockedProbeCount, 0, TotalProbeCount) * 2 >=
            TotalProbeCount;
}

bool ABHRifle::ShouldRefreshMuzzleEnvironmentProbe(
    bool bHasCachedSample,
    float CurrentTime,
    float CachedSampleTime,
    const FVector& MuzzleLocation,
    const FVector& CachedMuzzleLocation,
    const FQuat& MuzzleRotation,
    const FQuat& CachedMuzzleRotation,
    float OwnerSpeedCentimetersPerSecond
)
{
    if (!bHasCachedSample)
    {
        return true;
    }

    const float SampleAge = CurrentTime - CachedSampleTime;
    if (SampleAge < 0.0f)
    {
        return true;
    }

    const bool bOwnerIsMoving =
        FMath::Max(0.0f, OwnerSpeedCentimetersPerSecond) > 25.0f;
    const float MaximumSampleAge = bOwnerIsMoving ? 0.06f : 0.12f;
    const float MaximumMuzzleTravel = bOwnerIsMoving ? 18.0f : 35.0f;
    const float MaximumRotationTolerance = bOwnerIsMoving ? 0.01f : 0.02f;

    return SampleAge > MaximumSampleAge ||
        FVector::DistSquared(MuzzleLocation, CachedMuzzleLocation) >
            FMath::Square(MaximumMuzzleTravel) ||
        !CachedMuzzleRotation.Equals(
            MuzzleRotation,
            MaximumRotationTolerance
        );
}

bool ABHRifle::IsMuzzleEnvironmentEnclosed(
    const FVector& MuzzleLocation
) const
{
    const UWorld* World = GetWorld();
    if (!IsValid(World))
    {
        return false;
    }

    const FTransform MuzzleTransform =
        GetPresentationMuzzleTransform();
    const float CurrentTime = World->GetTimeSeconds();
    const float OwnerSpeedCentimetersPerSecond = IsValid(GetOwner())
        ? GetOwner()->GetVelocity().Size()
        : 0.0f;
    if (!ShouldRefreshMuzzleEnvironmentProbe(
            bHasMuzzleEnvironmentCache,
            CurrentTime,
            CachedMuzzleEnvironmentSampleTime,
            MuzzleLocation,
            CachedMuzzleEnvironmentLocation,
            MuzzleTransform.GetRotation(),
            CachedMuzzleEnvironmentRotation,
            OwnerSpeedCentimetersPerSecond
        ))
    {
        return bCachedMuzzleEnvironmentEnclosed;
    }

    const FVector ProbeDirections[] = {
        MuzzleTransform.GetUnitAxis(EAxis::Z),
        MuzzleTransform.GetUnitAxis(EAxis::X),
        -MuzzleTransform.GetUnitAxis(EAxis::X),
        MuzzleTransform.GetUnitAxis(EAxis::Y),
        -MuzzleTransform.GetUnitAxis(EAxis::Y)
    };
    int32 BlockedProbeCount = 0;
    FCollisionQueryParams QueryParams(
        SCENE_QUERY_STAT(BHRifleAudioEnvironment),
        false,
        GetOwner()
    );
    QueryParams.AddIgnoredActor(this);
    for (const FVector& ProbeDirection : ProbeDirections)
    {
        FHitResult ProbeHit;
        const float ProbeDistance = ProbeDirection.Z > 0.5f
            ? 1200.0f
            : 800.0f;
        if (World->LineTraceSingleByChannel(
                ProbeHit,
                MuzzleLocation,
                MuzzleLocation + ProbeDirection * ProbeDistance,
                ECC_Visibility,
                QueryParams
            ))
        {
            ++BlockedProbeCount;
        }
    }
    const bool bMuzzleEnclosed = ShouldUseIndoorFireTail(
        BlockedProbeCount,
        UE_ARRAY_COUNT(ProbeDirections)
    );
    bHasMuzzleEnvironmentCache = true;
    bCachedMuzzleEnvironmentEnclosed = bMuzzleEnclosed;
    CachedMuzzleEnvironmentLocation = MuzzleLocation;
    CachedMuzzleEnvironmentRotation = MuzzleTransform.GetRotation();
    CachedMuzzleEnvironmentSampleTime = CurrentTime;
    return bMuzzleEnclosed;
}

void ABHRifle::PlayFirePresentation(
    const FHitResult& HitResult,
    bool bHadBlockingHit
)
{
    if (ABHCharacter* CharacterOwner =
        Cast<ABHCharacter>(GetOwner()))
    {
        CharacterOwner->AddFirstPersonFireKick();
    }

    const FTransform MuzzleTransform =
        GetPresentationMuzzleTransform();

    if (bEnableMuzzleFlashLight &&
        IsValid(MuzzleFlashLight) &&
        MuzzleFlashLightDuration > 0.0f)
    {
        MuzzleFlashLight->SetIntensity(
            FMath::Max(0.0f, MuzzleFlashLightIntensity)
        );
        MuzzleFlashLight->SetAttenuationRadius(
            FMath::Max(0.0f, MuzzleFlashLightRadius)
        );
        MuzzleFlashLight->SetLightColor(MuzzleFlashLightColor);
        MuzzleFlashLight->SetHiddenInGame(false);
        MuzzleFlashLight->SetVisibility(true);

        GetWorldTimerManager().ClearTimer(
            MuzzleFlashLightTimerHandle
        );
        GetWorldTimerManager().SetTimer(
            MuzzleFlashLightTimerHandle,
            this,
            &ABHRifle::HideMuzzleFlashLight,
            MuzzleFlashLightDuration,
            false
        );
    }

    SpawnCasing();

    if (IsValid(MuzzleFlashEffect))
    {
        UNiagaraFunctionLibrary::SpawnSystemAtLocation(
            this,
            MuzzleFlashEffect,
            MuzzleTransform.GetLocation(),
            MuzzleTransform.Rotator()
        );
    }

    if (IsValid(FireSound))
    {
        UGameplayStatics::PlaySoundAtLocation(
            this,
            FireSound,
            MuzzleTransform.GetLocation()
        );
    }

    USoundBase* FireTail = IsMuzzleEnvironmentEnclosed(
        MuzzleTransform.GetLocation()
    )
        ? IndoorFireTailSound.Get()
        : OutdoorFireTailSound.Get();
    if (IsValid(FireTail))
    {
        UGameplayStatics::PlaySoundAtLocation(
            this,
            FireTail,
            MuzzleTransform.GetLocation()
        );
    }

    PlayImpactPresentation(HitResult, bHadBlockingHit);

    if (!PlayFirstPersonMontage(FirstPersonFireMontage))
    {
        PlayFirstPersonSequence(
            FirstPersonFireAnimation,
            WeaponFireAnimation
        );
    }
    else
    {
        PlayFirstPersonSequence(nullptr, WeaponFireAnimation);
    }

    if (FireCameraShake)
    {
        const APawn* PawnOwner = Cast<APawn>(GetOwner());
        APlayerController* PlayerController = PawnOwner
            ? Cast<APlayerController>(PawnOwner->GetController())
            : nullptr;

        if (IsValid(PlayerController) &&
            PlayerController->IsLocalController())
        {
            const UGameInstance* GameInstance = PlayerController->GetGameInstance();
            const UBHUserSettingsSubsystem* SettingsSubsystem = GameInstance
                ? GameInstance->GetSubsystem<UBHUserSettingsSubsystem>()
                : nullptr;
            PlayerController->ClientStartCameraShake(
                FireCameraShake,
                SettingsSubsystem
                    ? SettingsSubsystem->GetCameraShakeScale()
                    : 1.0f
            );
        }
    }

    OnFirePresentation(HitResult, bHadBlockingHit);
}

void ABHRifle::PlayImpactPresentation(
    const FHitResult& HitResult,
    bool bHadBlockingHit
)
{
    if (!bHadBlockingHit)
    {
        return;
    }

    if (ImpactActorClass)
    {
        FActorSpawnParameters SpawnParameters;
        SpawnParameters.Owner = this;
        SpawnParameters.Instigator =
            Cast<APawn>(GetOwner());
        SpawnParameters.SpawnCollisionHandlingOverride =
            ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

        ABHImpactEffect* SpawnedImpact =
            GetWorld()->SpawnActor<ABHImpactEffect>(
                ImpactActorClass,
                HitResult.ImpactPoint,
                HitResult.ImpactNormal.Rotation(),
                SpawnParameters
            );

        if (IsValid(SpawnedImpact))
        {
            SpawnedImpact->InitializeImpact(HitResult);
        }
    }

    if (IsValid(ImpactEffect))
    {
        UNiagaraFunctionLibrary::SpawnSystemAtLocation(
            this,
            ImpactEffect,
            HitResult.ImpactPoint,
            HitResult.ImpactNormal.Rotation()
        );
    }

    if (IsValid(ImpactDecalMaterial) &&
        ImpactDecalLifeSpan > 0.0f)
    {
        UGameplayStatics::SpawnDecalAtLocation(
            this,
            ImpactDecalMaterial,
            ImpactDecalSize.ComponentMax(FVector::ZeroVector),
            HitResult.ImpactPoint,
            HitResult.ImpactNormal.Rotation(),
            ImpactDecalLifeSpan
        );
    }
}

void ABHRifle::HideMuzzleFlashLight()
{
    if (!IsValid(MuzzleFlashLight))
    {
        return;
    }

    MuzzleFlashLight->SetVisibility(false);
    MuzzleFlashLight->SetHiddenInGame(true);
}

void ABHRifle::SpawnCasing()
{
    UWorld* World = GetWorld();
    APawn* PawnOwner = Cast<APawn>(GetOwner());

    if (!IsValid(World) ||
        !IsValid(PawnOwner) ||
        !PawnOwner->IsLocallyControlled() ||
        !IsValid(EjectionPoint) ||
        !IsValid(CasingMesh) ||
        !CasingClass)
    {
        return;
    }

    FActorSpawnParameters SpawnParameters;
    SpawnParameters.Owner = this;
    SpawnParameters.Instigator = PawnOwner;
    SpawnParameters.SpawnCollisionHandlingOverride =
        ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    ABHWeaponCasing* Casing = World->SpawnActor<ABHWeaponCasing>(
        CasingClass,
        EjectionPoint->GetComponentTransform(),
        SpawnParameters
    );

    if (!IsValid(Casing))
    {
        return;
    }

    const FTransform EjectionTransform =
        EjectionPoint->GetComponentTransform();
    const float VelocityVariation = FMath::FRandRange(0.85f, 1.15f);
    const float SpinVariation = FMath::FRandRange(0.8f, 1.2f);

    FVector WorldVelocity = EjectionTransform.TransformVectorNoScale(
        CasingEjectionVelocity * VelocityVariation
    );
    WorldVelocity += PawnOwner->GetVelocity();

    const FVector WorldAngularVelocity =
        EjectionTransform.TransformVectorNoScale(
            CasingAngularVelocityRadians * SpinVariation
        );

    Casing->InitializeCasing(
        CasingMesh,
        WorldVelocity,
        WorldAngularVelocity,
        CasingLifeSpan
    );
}

void ABHRifle::PlayDryFirePresentation()
{
    if (IsValid(DryFireSound))
    {
        UGameplayStatics::PlaySoundAtLocation(
            this,
            DryFireSound,
            GetPresentationMuzzleTransform().GetLocation()
        );
    }

    OnDryFirePresentation();
}

void ABHRifle::PlayReloadPresentation(float DurationOverride)
{
    if (ABHCharacter* CharacterOwner =
        Cast<ABHCharacter>(GetOwner()))
    {
        CharacterOwner->PlayFirstPersonReloadMotion(
            DurationOverride > 0.0f
                ? DurationOverride
                : RifleConfig.ReloadDuration
        );
    }

    if (IsValid(ReloadSound))
    {
        UGameplayStatics::PlaySoundAtLocation(
            this,
            ReloadSound,
            GetActorLocation()
        );
    }

    if (!PlayFirstPersonMontage(FirstPersonReloadMontage))
    {
        PlayFirstPersonSequence(
            FirstPersonReloadAnimation,
            WeaponReloadAnimation
        );
    }
    else
    {
        PlayFirstPersonSequence(nullptr, WeaponReloadAnimation);
    }
    OnReloadPresentation();
}

void ABHRifle::CancelReloadPresentation()
{
    if (ABHCharacter* CharacterOwner = Cast<ABHCharacter>(GetOwner()))
    {
        if (USkeletalMeshComponent* FirstPersonArms = CharacterOwner->GetFirstPersonArmsMesh())
        {
            if (UAnimInstance* AnimInstance = FirstPersonArms->GetAnimInstance())
            {
                AnimInstance->Montage_Stop(0.08f);
            }
        }

        CharacterOwner->CancelFirstPersonActionAnimation();
        CharacterOwner->CancelFirstPersonReloadMotion();
    }

    if (IsValid(RifleSkeletalMesh))
    {
        RifleSkeletalMesh->Stop();
    }
}
void ABHRifle::SetAimPresentation(bool bNewIsAiming)
{
    OnAimPresentationChanged(bNewIsAiming);
}

bool ABHRifle::PlayFirstPersonMontage(
    UAnimMontage* Montage
) const
{
    const ABHCharacter* CharacterOwner =
        Cast<ABHCharacter>(GetOwner());
    USkeletalMeshComponent* ArmsMesh = CharacterOwner
        ? CharacterOwner->GetFirstPersonArmsMesh()
        : nullptr;
    UAnimInstance* AnimInstance = IsValid(ArmsMesh)
        ? ArmsMesh->GetAnimInstance()
        : nullptr;

    if (IsValid(Montage) && IsValid(AnimInstance))
    {
        return AnimInstance->Montage_Play(Montage) > 0.0f;
    }

    return false;
}

void ABHRifle::PlayFirstPersonSequence(
    UAnimSequenceBase* ArmsAnimation,
    UAnimSequenceBase* RifleAnimation
) const
{
    ABHCharacter* CharacterOwner =
        Cast<ABHCharacter>(GetOwner());

    if (IsValid(CharacterOwner) && IsValid(ArmsAnimation))
    {
        CharacterOwner->PlayFirstPersonActionAnimation(
            ArmsAnimation
        );
    }

    if (IsValid(RifleSkeletalMesh) && IsValid(RifleAnimation))
    {
        RifleSkeletalMesh->PlayAnimation(
            RifleAnimation,
            false
        );
    }
}
