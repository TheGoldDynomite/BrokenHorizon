#include "BHWaterSurface.h"

#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

ABHWaterSurface::ABHWaterSurface()
{
    PrimaryActorTick.bCanEverTick = false;
    SetReplicates(true);

    WaterVolume = CreateDefaultSubobject<UBoxComponent>(TEXT("WaterVolume"));
    SetRootComponent(WaterVolume);
    WaterVolume->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
    WaterVolume->SetGenerateOverlapEvents(true);

    WaterMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WaterMesh"));
    WaterMesh->SetupAttachment(WaterVolume);
    WaterMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(
        TEXT("/Engine/BasicShapes/Cube.Cube"));
    if (CubeFinder.Succeeded())
    {
        WaterMesh->SetStaticMesh(CubeFinder.Object);
    }
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> WaterMaterialFinder(
        TEXT("/Game/BrokenHorizon/Environment/Materials/M_BH_WaterSurface.M_BH_WaterSurface"));
    if (WaterMaterialFinder.Succeeded())
    {
        WaterMesh->SetMaterial(0, WaterMaterialFinder.Object);
    }
    WaterLabel = CreateDefaultSubobject<UTextRenderComponent>(TEXT("WaterLabel"));
    WaterLabel->SetupAttachment(WaterVolume);
    WaterLabel->SetText(NSLOCTEXT("BrokenHorizon", "WaterLabel", "WATER ROUTE"));
    WaterLabel->SetHorizontalAlignment(EHorizTextAligment::EHTA_Center);
    WaterLabel->SetWorldSize(48.0f);
    WaterLabel->SetRelativeLocation(FVector(0.0f, 0.0f, 80.0f));
    SynchronizeGeometry();
}

void ABHWaterSurface::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    SynchronizeGeometry();
}

void ABHWaterSurface::PostRegisterAllComponents()
{
    Super::PostRegisterAllComponents();
    SynchronizeGeometry();
}

void ABHWaterSurface::SynchronizeGeometry()
{
    const FVector Extents = GetSurfaceExtents();
    if (IsValid(WaterVolume))
    {
        WaterVolume->SetBoxExtent(Extents);
    }
    UStaticMesh* Mesh = IsValid(WaterMesh) ? WaterMesh->GetStaticMesh() : nullptr;
    if (!IsValid(Mesh)) { return; }

    const FBox LocalBounds = Mesh->GetBoundingBox();
    if (!LocalBounds.IsValid) { return; }
    const FVector LocalExtents = LocalBounds.GetExtent();
    const FVector LocalCenter = LocalBounds.GetCenter();
    if (LocalExtents.ContainsNaN() || LocalCenter.ContainsNaN() ||
        LocalExtents.X <= 0.0 || LocalExtents.Y <= 0.0) { return; }

    // Actor extents are half sizes. Preserve the thin native surface, and fit
    // XY to the assigned asset's local bounds rather than assuming a 100cm half size.
    const FVector Scale(Extents.X / LocalExtents.X, Extents.Y / LocalExtents.Y, 0.02);
    const FVector CenterOffset = WaterMesh->GetRelativeRotation().Quaternion().RotateVector(LocalCenter * Scale);
    const FVector Location = FVector(0.0, 0.0, GetSurfaceHeight()) - CenterOffset;
    if (Scale.ContainsNaN() || Location.ContainsNaN()) { return; }
    WaterMesh->SetRelativeScale3D(Scale);
    WaterMesh->SetRelativeLocation(Location);
}

bool ABHWaterSurface::ContainsWorldLocation(FVector WorldLocation) const
{
    return IsValid(WaterVolume) &&
        WaterVolume->Bounds.GetBox().IsInside(WorldLocation);
}

float ABHWaterSurface::GetSurfaceHeight() const
{
    return FMath::IsFinite(SurfaceHeight) ? SurfaceHeight : 0.0f;
}

float ABHWaterSurface::GetInfantrySpeedMultiplier() const
{
    return FMath::Clamp(InfantrySpeedMultiplier, 0.25f, 1.0f);
}

FVector ABHWaterSurface::GetSurfaceExtents() const
{
    const auto EffectiveExtent = [](double Value)
    {
        return FMath::IsFinite(Value) ? FMath::Max(100.0, Value) : 100.0;
    };
    return FVector(EffectiveExtent(SurfaceExtents.X), EffectiveExtent(SurfaceExtents.Y), EffectiveExtent(SurfaceExtents.Z));
}
