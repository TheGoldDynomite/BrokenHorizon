#include "BHWorldBuilderLibrary.h"

#include "EngineUtils.h"
#include "Engine/World.h"
#include "FileHelpers.h"
#include "Landscape.h"
#include "LandscapeEdit.h"
#include "LandscapeInfo.h"
#include "LandscapeProxy.h"
#include "LandscapeSubsystem.h"
#include "Materials/MaterialInterface.h"
#include "WorldPartition/LoaderAdapter/LoaderAdapterShape.h"

namespace
{
constexpr int32 ComponentCount = 25;
constexpr int32 SectionsPerComponent = 2;
constexpr int32 QuadsPerSection = 63;
constexpr int32 QuadsPerComponent =
    SectionsPerComponent * QuadsPerSection;
constexpr int32 LandscapeQuads =
    ComponentCount * QuadsPerComponent;
constexpr int32 LandscapeVertices = LandscapeQuads + 1;
constexpr float XYScaleCentimeters = 800.0f;
constexpr float ZScale = 300.0f;
constexpr float RegionSizeMeters =
    LandscapeQuads * XYScaleCentimeters / 100.0f;
constexpr float HalfRegionMeters = RegionSizeMeters * 0.5f;
constexpr uint32 StreamingGridComponents = 4;

const FName RegionLandscapeTag(
    TEXT("BH_InitialRegionLandscape")
);

float SmoothStep01(float Value)
{
    const float Clamped = FMath::Clamp(Value, 0.0f, 1.0f);
    return Clamped * Clamped * (3.0f - (2.0f * Clamped));
}

float Bell(float Distance, float Radius)
{
    if (Radius <= KINDA_SMALL_NUMBER)
    {
        return 0.0f;
    }

    const float Normalized = Distance / Radius;
    return FMath::Exp(-Normalized * Normalized);
}

float BaseTerrainHeightMeters(float X, float Y)
{
    const float BroadHills =
        (FMath::Sin((X * 0.36f) + 0.7f) * 72.0f) +
        (FMath::Cos((Y * 0.29f) - 0.4f) * 55.0f) +
        (FMath::Sin((X + Y) * 0.17f) * 38.0f);
    const float FineRelief =
        (FMath::Sin((X * 1.13f) + (Y * 0.31f)) * 18.0f) +
        (FMath::Cos((Y * 0.91f) - (X * 0.27f)) * 14.0f);

    const float WestHighlands =
        SmoothStep01((-X - 2.0f) / 9.0f) *
        (190.0f + (FMath::Sin(Y * 0.42f) * 75.0f));
    const float NorthRidge =
        SmoothStep01((Y - 3.0f) / 8.0f) *
        (150.0f + (FMath::Cos(X * 0.38f) * 55.0f));
    const float EasternPlateau =
        SmoothStep01((X - 3.5f) / 7.5f) * 110.0f;

    const float RiverCenterY =
        (FMath::Sin(X * 0.33f) * 1.35f) -
        (X * 0.11f) -
        0.25f;
    const float RiverDistance = FMath::Abs(Y - RiverCenterY);
    const float RiverValley =
        -165.0f * Bell(RiverDistance, 1.15f);

    const float WesternBasin =
        -95.0f * Bell(
            FVector2D::Distance(
                FVector2D(X, Y),
                FVector2D(-9.5f, -4.5f)
            ),
            2.4f
        );
    const float CrossroadsPlain =
        -80.0f * Bell(
            FVector2D::Distance(
                FVector2D(X, Y),
                FVector2D(0.0f, -0.5f)
            ),
            3.0f
        );

    return 115.0f +
        BroadHills +
        FineRelief +
        WestHighlands +
        NorthRidge +
        EasternPlateau +
        RiverValley +
        WesternBasin +
        CrossroadsPlain;
}

float FlattenSite(
    float Height,
    float X,
    float Y,
    const FVector2D& Site,
    float Radius,
    float TargetHeight
)
{
    const float Distance =
        FVector2D::Distance(FVector2D(X, Y), Site);
    const float Blend =
        1.0f - SmoothStep01(
            FMath::Clamp(
                (Distance - (Radius * 0.35f)) /
                    (Radius * 0.65f),
                0.0f,
                1.0f
            )
        );

    return FMath::Lerp(Height, TargetHeight, Blend);
}

float GenerateTerrainHeightMeters(float X, float Y)
{
    float Height = BaseTerrainHeightMeters(X, Y);

    Height = FlattenSite(
        Height,
        X,
        Y,
        FVector2D(-9.5f, -4.5f),
        0.9f,
        92.0f
    );
    Height = FlattenSite(
        Height,
        X,
        Y,
        FVector2D(0.0f, -0.5f),
        1.15f,
        64.0f
    );
    Height = FlattenSite(
        Height,
        X,
        Y,
        FVector2D(9.0f, 4.5f),
        0.95f,
        205.0f
    );

    return FMath::Clamp(Height, -110.0f, 690.0f);
}

uint16 EncodeLandscapeHeight(float HeightMeters)
{
    const float Encoded =
        32768.0f +
        ((HeightMeters * 100.0f * 128.0f) / ZScale);

    return static_cast<uint16>(
        FMath::Clamp(
            FMath::RoundToInt(Encoded),
            0,
            65535
        )
    );
}
}

bool UBHWorldBuilderLibrary::BuildInitialRegionLandscape(
    UObject* WorldContextObject
)
{
    UWorld* World = WorldContextObject
        ? WorldContextObject->GetWorld()
        : nullptr;

    if (!IsValid(World) || !World->IsEditorWorld())
    {
        UE_LOG(
            LogTemp,
            Error,
            TEXT(
                "Broken Horizon world builder requires "
                "an editor world."
            )
        );
        return false;
    }

    for (TActorIterator<ALandscape> It(World); It; ++It)
    {
        if (It->ActorHasTag(RegionLandscapeTag))
        {
            UE_LOG(
                LogTemp,
                Display,
                TEXT(
                    "Broken Horizon initial-region "
                    "landscape already exists."
                )
            );
            return true;
        }
    }

    TArray<TObjectPtr<ALandscapeProxy>> ExistingLandscapes;
    for (TActorIterator<ALandscapeProxy> It(World); It; ++It)
    {
        ExistingLandscapes.Add(*It);
    }

    for (ALandscapeProxy* ExistingLandscape : ExistingLandscapes)
    {
        if (IsValid(ExistingLandscape))
        {
            World->EditorDestroyActor(
                ExistingLandscape,
                true
            );
        }
    }

    TArray<uint16> HeightData;
    HeightData.SetNumUninitialized(
        LandscapeVertices * LandscapeVertices
    );

    for (int32 Y = 0; Y < LandscapeVertices; ++Y)
    {
        const float WorldY =
            -HalfRegionMeters +
            ((static_cast<float>(Y) / LandscapeQuads) *
                RegionSizeMeters);

        for (int32 X = 0; X < LandscapeVertices; ++X)
        {
            const float WorldX =
                -HalfRegionMeters +
                ((static_cast<float>(X) / LandscapeQuads) *
                    RegionSizeMeters);
            HeightData[(Y * LandscapeVertices) + X] =
                EncodeLandscapeHeight(
                    GenerateTerrainHeightMeters(
                        WorldX / 1000.0f,
                        WorldY / 1000.0f
                    )
                );
        }
    }

    const FVector LandscapeLocation(
        -HalfRegionMeters * 100.0f,
        -HalfRegionMeters * 100.0f,
        0.0f
    );
    ALandscape* Landscape = World->SpawnActor<ALandscape>(
        LandscapeLocation,
        FRotator::ZeroRotator
    );

    if (!IsValid(Landscape))
    {
        UE_LOG(
            LogTemp,
            Error,
            TEXT(
                "Broken Horizon could not spawn the "
                "initial-region landscape."
            )
        );
        return false;
    }

    Landscape->SetActorLabel(TEXT("Landscape_RegionAlpha"));
    Landscape->Tags.AddUnique(RegionLandscapeTag);
    Landscape->SetActorScale3D(
        FVector(
            XYScaleCentimeters,
            XYScaleCentimeters,
            ZScale
        )
    );
    Landscape->SetIsSpatiallyLoaded(false);
    Landscape->StaticLightingLOD = 2;

    TMap<FGuid, TArray<uint16>> HeightDataPerLayers;
    HeightDataPerLayers.Add(FGuid(), MoveTemp(HeightData));

    TMap<FGuid, TArray<FLandscapeImportLayerInfo>>
        MaterialLayerDataPerLayers;
    MaterialLayerDataPerLayers.Add(
        FGuid(),
        TArray<FLandscapeImportLayerInfo>()
    );

    Landscape->Import(
        FGuid::NewGuid(),
        0,
        0,
        LandscapeQuads,
        LandscapeQuads,
        SectionsPerComponent,
        QuadsPerSection,
        HeightDataPerLayers,
        nullptr,
        MaterialLayerDataPerLayers,
        ELandscapeImportAlphamapType::Additive,
        TArrayView<const FLandscapeLayer>()
    );

    ULandscapeInfo* LandscapeInfo =
        Landscape->GetLandscapeInfo();
    if (!IsValid(LandscapeInfo))
    {
        UE_LOG(
            LogTemp,
            Error,
            TEXT(
                "Broken Horizon created a landscape "
                "without valid landscape info."
            )
        );
        return false;
    }

    LandscapeInfo->UpdateLayerInfoMap(Landscape);

    if (ULandscapeSubsystem* LandscapeSubsystem =
        World->GetSubsystem<ULandscapeSubsystem>())
    {
        LandscapeSubsystem->ChangeGridSize(
            LandscapeInfo,
            StreamingGridComponents
        );
    }

    Landscape->PostEditChange();
    Landscape->MarkPackageDirty();

    UE_LOG(
        LogTemp,
        Display,
        TEXT(
            "BH_REGION_BUILD landscape=%dx%d "
            "region=%.1fkm components=%dx%d "
            "streaming_grid=%u"
        ),
        LandscapeVertices,
        LandscapeVertices,
        RegionSizeMeters / 1000.0f,
        ComponentCount,
        ComponentCount,
        StreamingGridComponents
    );

    return true;
}

bool UBHWorldBuilderLibrary::ApplyInitialRegionLandscapeMaterial(
    UObject* WorldContextObject,
    UMaterialInterface* Material
)
{
    UWorld* World = WorldContextObject
        ? WorldContextObject->GetWorld()
        : nullptr;

    if (
        !IsValid(World)
        || !World->IsEditorWorld()
        || !IsValid(Material)
    )
    {
        UE_LOG(
            LogTemp,
            Error,
            TEXT(
                "Broken Horizon could not apply the initial-region "
                "Landscape material."
            )
        );
        return false;
    }

    const float HalfRegionCentimeters =
        HalfRegionMeters * 100.0f;
    FLoaderAdapterShape RegionLoader(
        World,
        FBox(
            FVector(
                -HalfRegionCentimeters,
                -HalfRegionCentimeters,
                -HALF_WORLD_MAX
            ),
            FVector(
                HalfRegionCentimeters,
                HalfRegionCentimeters,
                HALF_WORLD_MAX
            )
        ),
        TEXT("Broken Horizon Landscape Material")
    );
    RegionLoader.Load();

    ALandscape* Landscape = nullptr;
    for (TActorIterator<ALandscape> It(World); It; ++It)
    {
        if (It->ActorHasTag(RegionLandscapeTag))
        {
            Landscape = *It;
            break;
        }
    }

    if (!IsValid(Landscape))
    {
        RegionLoader.Unload();
        UE_LOG(
            LogTemp,
            Error,
            TEXT(
                "Broken Horizon could not find the initial-region "
                "Landscape while applying its material."
            )
        );
        return false;
    }

    const FGuid LandscapeGuid = Landscape->GetLandscapeGuid();
    TArray<ALandscapeProxy*> RegionProxies;
    for (TActorIterator<ALandscapeProxy> It(World); It; ++It)
    {
        if (It->GetLandscapeGuid() == LandscapeGuid)
        {
            It->Modify();
            RegionProxies.Add(*It);
        }
    }

    Landscape->Modify();
    Landscape->LandscapeMaterial = Material;
    Landscape->PostEditChange();

    if (ULandscapeInfo* LandscapeInfo =
        Landscape->GetLandscapeInfo())
    {
        LandscapeInfo->UpdateAllComponentMaterialInstances(true);
    }

    for (ALandscapeProxy* Proxy : RegionProxies)
    {
        if (IsValid(Proxy))
        {
            Proxy->MarkPackageDirty();
        }
    }

    Landscape->MarkPackageDirty();
    const bool bSaved =
        UEditorLoadingAndSavingUtils::SaveDirtyPackages(
            true,
            true
        );

    RegionLoader.Unload();

    if (bSaved)
    {
        UE_LOG(
            LogTemp,
            Display,
            TEXT(
                "BH_REGION_MATERIAL material=%s "
                "proxies=%d saved=true"
            ),
            *Material->GetPathName(),
            RegionProxies.Num()
        );
    }
    else
    {
        UE_LOG(
            LogTemp,
            Error,
            TEXT(
                "BH_REGION_MATERIAL material=%s "
                "proxies=%d saved=false"
            ),
            *Material->GetPathName(),
            RegionProxies.Num()
        );
    }

    return bSaved;
}

bool UBHWorldBuilderLibrary::RepairInitialRegionLandscapeHeight(
    UObject* WorldContextObject
)
{
    UWorld* World = WorldContextObject
        ? WorldContextObject->GetWorld()
        : nullptr;

    if (!IsValid(World) || !World->IsEditorWorld())
    {
        UE_LOG(
            LogTemp,
            Error,
            TEXT(
                "Broken Horizon could not repair the initial-region "
                "Landscape height outside an editor world."
            )
        );
        return false;
    }

    const float HalfRegionCentimeters =
        HalfRegionMeters * 100.0f;
    FLoaderAdapterShape RegionLoader(
        World,
        FBox(
            FVector(
                -HalfRegionCentimeters,
                -HalfRegionCentimeters,
                -HALF_WORLD_MAX
            ),
            FVector(
                HalfRegionCentimeters,
                HalfRegionCentimeters,
                HALF_WORLD_MAX
            )
        ),
        TEXT("Broken Horizon Landscape Height Repair")
    );
    RegionLoader.Load();

    ALandscape* Landscape = nullptr;
    for (TActorIterator<ALandscape> It(World); It; ++It)
    {
        if (It->ActorHasTag(RegionLandscapeTag))
        {
            Landscape = *It;
            break;
        }
    }

    ULandscapeInfo* LandscapeInfo = IsValid(Landscape)
        ? Landscape->GetLandscapeInfo()
        : nullptr;
    FIntRect LandscapeExtent;

    if (!IsValid(Landscape) ||
        !IsValid(LandscapeInfo) ||
        !LandscapeInfo->GetLandscapeExtent(LandscapeExtent))
    {
        RegionLoader.Unload();
        UE_LOG(
            LogTemp,
            Error,
            TEXT(
                "Broken Horizon could not resolve the initial-region "
                "Landscape extent for height repair."
            )
        );
        return false;
    }

    const int32 Width =
        LandscapeExtent.Max.X - LandscapeExtent.Min.X + 1;
    const int32 Height =
        LandscapeExtent.Max.Y - LandscapeExtent.Min.Y + 1;

    if (Width != LandscapeVertices ||
        Height != LandscapeVertices)
    {
        RegionLoader.Unload();
        UE_LOG(
            LogTemp,
            Error,
            TEXT(
                "Broken Horizon height repair expected %dx%d vertices "
                "but found %dx%d."
            ),
            LandscapeVertices,
            LandscapeVertices,
            Width,
            Height
        );
        return false;
    }

    TArray<uint16> HeightData;
    HeightData.SetNumUninitialized(Width * Height);
    uint16 MinimumEncodedHeight = MAX_uint16;
    uint16 MaximumEncodedHeight = 0;

    for (int32 Y = LandscapeExtent.Min.Y;
         Y <= LandscapeExtent.Max.Y;
         ++Y)
    {
        const float WorldYCentimeters =
            Landscape->GetActorLocation().Y +
            (static_cast<float>(Y) * XYScaleCentimeters);

        for (int32 X = LandscapeExtent.Min.X;
             X <= LandscapeExtent.Max.X;
             ++X)
        {
            const float WorldXCentimeters =
                Landscape->GetActorLocation().X +
                (static_cast<float>(X) * XYScaleCentimeters);
            const uint16 EncodedHeight = EncodeLandscapeHeight(
                GenerateTerrainHeightMeters(
                    WorldXCentimeters / 100000.0f,
                    WorldYCentimeters / 100000.0f
                )
            );
            const int32 DataIndex =
                ((Y - LandscapeExtent.Min.Y) * Width) +
                (X - LandscapeExtent.Min.X);
            HeightData[DataIndex] = EncodedHeight;
            MinimumEncodedHeight = FMath::Min(
                MinimumEncodedHeight,
                EncodedHeight
            );
            MaximumEncodedHeight = FMath::Max(
                MaximumEncodedHeight,
                EncodedHeight
            );
        }
    }

    Landscape->Modify();
    FLandscapeEditDataInterface LandscapeEdit(LandscapeInfo);
    LandscapeEdit.SetHeightData(
        LandscapeExtent.Min.X,
        LandscapeExtent.Min.Y,
        LandscapeExtent.Max.X,
        LandscapeExtent.Max.Y,
        HeightData.GetData(),
        Width,
        true,
        nullptr,
        nullptr,
        nullptr,
        false,
        nullptr,
        nullptr,
        true,
        true,
        true
    );
    LandscapeEdit.Flush();

    const FGuid LandscapeGuid = Landscape->GetLandscapeGuid();
    int32 ProxyCount = 0;
    for (TActorIterator<ALandscapeProxy> It(World); It; ++It)
    {
        if (It->GetLandscapeGuid() == LandscapeGuid)
        {
            It->Modify();
            It->PostEditChange();
            It->MarkPackageDirty();
            ++ProxyCount;
        }
    }

    LandscapeInfo->UpdateAllAddCollisions();
    Landscape->PostEditChange();
    Landscape->MarkPackageDirty();

    const bool bSaved =
        UEditorLoadingAndSavingUtils::SaveDirtyPackages(
            true,
            true
        );

    RegionLoader.Unload();

    if (bSaved)
    {
        UE_LOG(
            LogTemp,
            Display,
            TEXT(
                "BH_REGION_HEIGHT vertices=%dx%d proxies=%d "
                "encoded_min=%u encoded_max=%u saved=true"
            ),
            Width,
            Height,
            ProxyCount,
            MinimumEncodedHeight,
            MaximumEncodedHeight
        );
    }
    else
    {
        UE_LOG(
            LogTemp,
            Error,
            TEXT(
                "BH_REGION_HEIGHT vertices=%dx%d proxies=%d "
                "encoded_min=%u encoded_max=%u saved=false"
            ),
            Width,
            Height,
            ProxyCount,
            MinimumEncodedHeight,
            MaximumEncodedHeight
        );
    }

    return bSaved;
}

float UBHWorldBuilderLibrary::GetInitialRegionHeightMeters(
    FVector2D WorldPositionCentimeters
)
{
    return GenerateTerrainHeightMeters(
        WorldPositionCentimeters.X / 100000.0f,
        WorldPositionCentimeters.Y / 100000.0f
    );
}
