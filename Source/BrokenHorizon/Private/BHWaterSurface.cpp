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
    WaterVolume->SetBoxExtent(SurfaceExtents);

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
    // The gameplay volume remains tall enough to detect wading, but the
    // player-facing water should be a thin surface plane rather than a solid
    // cube that occludes the route from inside the volume.
    WaterMesh->SetRelativeLocation(
        FVector(0.0f, 0.0f, SurfaceHeight)
    );
    WaterMesh->SetRelativeScale3D(
        FVector(
            SurfaceExtents.X / 100.0f,
            SurfaceExtents.Y / 100.0f,
            0.02f
        )
    );

    WaterLabel = CreateDefaultSubobject<UTextRenderComponent>(TEXT("WaterLabel"));
    WaterLabel->SetupAttachment(WaterVolume);
    WaterLabel->SetText(NSLOCTEXT("BrokenHorizon", "WaterLabel", "WATER ROUTE"));
    WaterLabel->SetHorizontalAlignment(EHorizTextAligment::EHTA_Center);
    WaterLabel->SetWorldSize(48.0f);
    WaterLabel->SetRelativeLocation(FVector(0.0f, 0.0f, 80.0f));
}

bool ABHWaterSurface::ContainsWorldLocation(FVector WorldLocation) const
{
    return IsValid(WaterVolume) &&
        WaterVolume->Bounds.GetBox().IsInside(WorldLocation);
}

float ABHWaterSurface::GetSurfaceHeight() const
{
    return SurfaceHeight;
}

float ABHWaterSurface::GetInfantrySpeedMultiplier() const
{
    return FMath::Clamp(InfantrySpeedMultiplier, 0.25f, 1.0f);
}

FVector ABHWaterSurface::GetSurfaceExtents() const
{
    return SurfaceExtents;
}
