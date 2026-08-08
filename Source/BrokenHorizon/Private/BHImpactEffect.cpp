#include "BHImpactEffect.h"

#include "Components/PointLightComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "UObject/ConstructorHelpers.h"

ABHImpactEffect::ABHImpactEffect()
{
    PrimaryActorTick.bCanEverTick = true;
    SetReplicates(false);
    SetActorEnableCollision(false);

    ImpactRoot = CreateDefaultSubobject<USceneComponent>(
        TEXT("ImpactRoot")
    );
    SetRootComponent(ImpactRoot);

    BulletMark = CreateDefaultSubobject<UStaticMeshComponent>(
        TEXT("BulletMark")
    );
    BulletMark->SetupAttachment(ImpactRoot);
    BulletMark->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    BulletMark->SetCastShadow(false);
    BulletMark->SetReceivesDecals(false);

    ImpactFlash = CreateDefaultSubobject<UPointLightComponent>(
        TEXT("ImpactFlash")
    );
    ImpactFlash->SetupAttachment(ImpactRoot);
    ImpactFlash->SetCastShadows(false);
    ImpactFlash->SetLightColor(FLinearColor(1.0f, 0.18f, 0.015f));

    static ConstructorHelpers::FObjectFinder<UStaticMesh> PlaneFinder(
        TEXT("/Engine/BasicShapes/Plane.Plane")
    );
    if (PlaneFinder.Succeeded())
    {
        BulletMark->SetStaticMesh(PlaneFinder.Object);
    }

    static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereFinder(
        TEXT("/Engine/BasicShapes/Sphere.Sphere")
    );
    if (SphereFinder.Succeeded())
    {
        SparkMeshAsset = SphereFinder.Object;
    }

    static ConstructorHelpers::FObjectFinder<UMaterialInterface>
        MaterialFinder(
            TEXT(
                "/Engine/BasicShapes/BasicShapeMaterial."
                "BasicShapeMaterial"
            )
        );
    if (MaterialFinder.Succeeded())
    {
        BaseShapeMaterial = MaterialFinder.Object;
    }
}

FBHImpactPresentationProfile ABHImpactEffect::GetSurfacePresentationProfile(
    uint8 SurfaceType,
    const FBHImpactPresentationProfile& BaseProfile
)
{
    FBHImpactPresentationProfile Profile = BaseProfile;

    switch (SurfaceType)
    {
        case SurfaceType1: // Concrete/masonry: stone fragments, low flash.
            Profile.SparkCount = FMath::RoundToInt(
                FMath::Max(0, BaseProfile.SparkCount) * 0.65f
            );
            Profile.SparkSpeed = BaseProfile.SparkSpeed * 0.85f;
            Profile.SparkColor = FLinearColor(
                0.55f,
                0.48f,
                0.38f,
                1.0f
            );
            Profile.FlashIntensity = BaseProfile.FlashIntensity * 0.75f;
            Profile.FlashRadius = BaseProfile.FlashRadius * 0.90f;
            Profile.BulletMarkScale = BaseProfile.BulletMarkScale * 1.10f;
            break;
        case SurfaceType4: // Metal: hot, fast sparks and a brighter flash.
            Profile.SparkCount = FMath::RoundToInt(
                FMath::Max(0, BaseProfile.SparkCount) * 1.40f
            );
            Profile.SparkSpeed = BaseProfile.SparkSpeed * 1.50f;
            Profile.SparkColor = FLinearColor(
                1.0f,
                0.62f,
                0.12f,
                1.0f
            );
            Profile.FlashIntensity = BaseProfile.FlashIntensity * 1.35f;
            Profile.FlashRadius = BaseProfile.FlashRadius * 1.15f;
            Profile.BulletMarkScale = BaseProfile.BulletMarkScale * 0.90f;
            break;
        case SurfaceType6: // Wood: fragments are not generic sparks.
            Profile.SparkCount = 0;
            Profile.FlashIntensity = 0.0f;
            Profile.FlashRadius = 0.0f;
            Profile.BulletMarkScale = BaseProfile.BulletMarkScale * 1.10f;
            break;
        case SurfaceType7: // Glass: light, cool fragments and little flash.
            Profile.SparkCount = FMath::RoundToInt(
                FMath::Max(0, BaseProfile.SparkCount) * 1.10f
            );
            Profile.SparkSpeed = BaseProfile.SparkSpeed * 1.25f;
            Profile.SparkColor = FLinearColor(
                0.55f,
                0.85f,
                1.0f,
                1.0f
            );
            Profile.FlashIntensity = BaseProfile.FlashIntensity * 0.35f;
            Profile.FlashRadius = BaseProfile.FlashRadius * 0.75f;
            Profile.BulletMarkScale = BaseProfile.BulletMarkScale * 0.65f;
            break;
        case SurfaceType8: // Flesh: do not place a generic hard-surface mark.
            Profile.SparkCount = 0;
            Profile.FlashIntensity = 0.0f;
            Profile.FlashRadius = 0.0f;
            Profile.bShowBulletMark = false;
            break;
        default: // Dirt, grass, water, and untagged soft cover.
            Profile.SparkCount = 0;
            Profile.FlashIntensity = 0.0f;
            Profile.FlashRadius = 0.0f;
            break;
    }

    Profile.SparkCount = FMath::Clamp(Profile.SparkCount, 0, 12);
    Profile.SparkSpeed = FMath::Max(0.0f, Profile.SparkSpeed);
    Profile.SparkScale = FMath::Max(0.001f, Profile.SparkScale);
    Profile.SparkGravityScale = FMath::Max(
        0.0f,
        Profile.SparkGravityScale
    );
    Profile.FlashIntensity = FMath::Max(0.0f, Profile.FlashIntensity);
    Profile.FlashRadius = FMath::Max(0.0f, Profile.FlashRadius);
    Profile.BulletMarkScale = FMath::Max(0.005f, Profile.BulletMarkScale);
    return Profile;
}

void ABHImpactEffect::InitializeImpact(
    const FHitResult& HitResult
)
{
    const FVector ImpactNormal =
        HitResult.ImpactNormal.GetSafeNormal(
            KINDA_SMALL_NUMBER,
            FVector::UpVector
        );
    const FVector MarkLocation =
        HitResult.ImpactPoint + (ImpactNormal * 0.15f);
    const FRotator MarkRotation =
        FRotationMatrix::MakeFromZ(ImpactNormal).Rotator();
    const uint8 SurfaceType = static_cast<uint8>(
        UPhysicalMaterial::DetermineSurfaceType(
            HitResult.PhysMaterial.Get()
        )
    );
    FBHImpactPresentationProfile BaseProfile;
    BaseProfile.SparkCount = SparkCount;
    BaseProfile.SparkSpeed = SparkSpeed;
    BaseProfile.SparkScale = SparkScale;
    BaseProfile.SparkGravityScale = SparkGravityScale;
    BaseProfile.SparkColor = SparkColor;
    BaseProfile.FlashIntensity = FlashIntensity;
    BaseProfile.FlashRadius = FlashRadius;
    BaseProfile.BulletMarkScale = BulletMarkScale;
    BaseProfile.BulletMarkColor = BulletMarkColor;
    const FBHImpactPresentationProfile Profile =
        GetSurfacePresentationProfile(SurfaceType, BaseProfile);
    ActiveSparkScale = Profile.SparkScale;

    SetActorLocationAndRotation(MarkLocation, MarkRotation);

    if (IsValid(HitResult.GetComponent()) &&
        HitResult.GetComponent()->Mobility == EComponentMobility::Movable)
    {
        AttachToComponent(
            HitResult.GetComponent(),
            FAttachmentTransformRules::KeepWorldTransform
        );
    }

    if (IsValid(BulletMark))
    {
        BulletMark->SetRelativeScale3D(
            FVector(Profile.BulletMarkScale)
        );
        BulletMark->SetVisibility(Profile.bShowBulletMark);

        if (IsValid(BaseShapeMaterial))
        {
            BulletMarkMaterial =
                UMaterialInstanceDynamic::Create(
                    BaseShapeMaterial,
                    this
                );

            if (IsValid(BulletMarkMaterial))
            {
                BulletMarkMaterial->SetVectorParameterValue(
                    TEXT("Color"),
                    Profile.BulletMarkColor
                );
                BulletMark->SetMaterial(0, BulletMarkMaterial);
            }
        }
    }

    if (IsValid(ImpactFlash))
    {
        ImpactFlash->SetIntensity(Profile.FlashIntensity);
        ImpactFlash->SetAttenuationRadius(
            Profile.FlashRadius
        );
        ImpactFlash->SetLightColor(Profile.SparkColor);
        ImpactFlash->SetVisibility(
            Profile.FlashIntensity > 0.0f && FlashDuration > 0.0f
        );
    }

    CreateSparks(ImpactNormal, Profile);
    SetLifeSpan(FMath::Max(0.1f, BulletMarkLifeSpan));
}

void ABHImpactEffect::CreateSparks(
    const FVector& ImpactNormal,
    const FBHImpactPresentationProfile& Profile
)
{
    if (!IsValid(SparkMeshAsset) || Profile.SparkCount <= 0)
    {
        FinishBurst();
        return;
    }

    if (IsValid(BaseShapeMaterial))
    {
        SparkMaterial = UMaterialInstanceDynamic::Create(
            BaseShapeMaterial,
            this
        );

        if (IsValid(SparkMaterial))
        {
            SparkMaterial->SetVectorParameterValue(
                TEXT("Color"),
                Profile.SparkColor
            );
        }
    }

    const FVector TangentX =
        FVector::CrossProduct(ImpactNormal, FVector::UpVector)
            .GetSafeNormal(
                KINDA_SMALL_NUMBER,
                FVector::ForwardVector
            );
    const FVector TangentY =
        FVector::CrossProduct(ImpactNormal, TangentX)
            .GetSafeNormal(
                KINDA_SMALL_NUMBER,
                FVector::RightVector
            );

    const int32 BoundedSparkCount = FMath::Clamp(
        Profile.SparkCount,
        0,
        12
    );
    SparkComponents.Reserve(BoundedSparkCount);
    SparkVelocities.Reserve(BoundedSparkCount);

    for (int32 Index = 0; Index < BoundedSparkCount; ++Index)
    {
        UStaticMeshComponent* Spark =
            NewObject<UStaticMeshComponent>(this);

        if (!IsValid(Spark))
        {
            continue;
        }

        AddInstanceComponent(Spark);
        Spark->SetupAttachment(ImpactRoot);
        Spark->SetStaticMesh(SparkMeshAsset);
        Spark->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Spark->SetCastShadow(false);
        Spark->SetRelativeLocation(FVector::ZeroVector);
        Spark->SetRelativeScale3D(
            FVector(Profile.SparkScale)
        );

        if (IsValid(SparkMaterial))
        {
            Spark->SetMaterial(0, SparkMaterial);
        }

        Spark->RegisterComponent();
        SparkComponents.Add(Spark);

        const FVector TangentDirection =
            (TangentX * FMath::FRandRange(-1.0f, 1.0f)) +
            (TangentY * FMath::FRandRange(-1.0f, 1.0f));
        const FVector Direction = (
            ImpactNormal * FMath::FRandRange(0.25f, 0.8f) +
            TangentDirection
        ).GetSafeNormal();

        SparkVelocities.Add(
            Direction *
            Profile.SparkSpeed *
            FMath::FRandRange(0.65f, 1.25f)
        );
    }
}

void ABHImpactEffect::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    ImpactAge += DeltaTime;

    if (IsValid(ImpactFlash) && ImpactAge >= FlashDuration)
    {
        ImpactFlash->SetVisibility(false);
    }

    if (bBurstFinished)
    {
        return;
    }

    if (ImpactAge >= FMath::Max(0.01f, BurstDuration))
    {
        FinishBurst();
        return;
    }

    const float BurstAlpha = 1.0f - FMath::Clamp(
        ImpactAge / FMath::Max(0.01f, BurstDuration),
        0.0f,
        1.0f
    );
    const FVector Gravity =
        FVector(0.0f, 0.0f, -980.0f * SparkGravityScale);

    const int32 Count = FMath::Min(
        SparkComponents.Num(),
        SparkVelocities.Num()
    );

    for (int32 Index = 0; Index < Count; ++Index)
    {
        UStaticMeshComponent* Spark = SparkComponents[Index];
        if (!IsValid(Spark))
        {
            continue;
        }

        SparkVelocities[Index] += Gravity * DeltaTime;
        Spark->AddWorldOffset(
            SparkVelocities[Index] * DeltaTime,
            false
        );
        Spark->SetRelativeScale3D(
            FVector(
                ActiveSparkScale *
                FMath::Max(0.05f, BurstAlpha)
            )
        );
    }
}

void ABHImpactEffect::FinishBurst()
{
    bBurstFinished = true;

    for (UStaticMeshComponent* Spark : SparkComponents)
    {
        if (IsValid(Spark))
        {
            Spark->SetVisibility(false);
        }
    }

    if (IsValid(ImpactFlash))
    {
        ImpactFlash->SetVisibility(false);
    }

    SetActorTickEnabled(false);
}
