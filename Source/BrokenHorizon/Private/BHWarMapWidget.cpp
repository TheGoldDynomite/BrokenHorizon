#include "BHWarMapWidget.h"

#include "BHCharacter.h"
#include "BHCharacter.h"
#include "BHFieldTransport.h"
#include "BHFieldFortification.h"
#include "BHHealthComponent.h"
#include "BHInjuryComponent.h"
#include "BHOpenWorldOperationDirector.h"
#include "BHPlayerResolver.h"
#include "BHSectorAnchor.h"
#include "BHUserSettingsSubsystem.h"
#include "BHWarOperationRules.h"
#include "BHWarSubsystem.h"
#include "BHWeaponComponent.h"
#include "EngineUtils.h"
#include "Input/Events.h"
#include "Input/Reply.h"
#include "InputCoreTypes.h"
#include "Rendering/DrawElements.h"
#include "Framework/Application/SlateApplication.h"
#include "Fonts/FontMeasure.h"
#include "Styling/CoreStyle.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "GameFramework/PlayerController.h"

namespace
{
bool IsDeploymentRenderedReview()
{
    FString ReviewMode;
    return FParse::Value(
               FCommandLine::Get(),
               TEXT("BHTestRenderedUIReview="),
               ReviewMode) &&
        ReviewMode.Equals(
            TEXT("WAR_MAP_DEPLOYMENT"),
            ESearchCase::IgnoreCase);
}

const FSlateBrush* GetWhiteBrush()
{
    return FCoreStyle::Get().GetBrush(TEXT("WhiteBrush"));
}

FLinearColor GetFactionColor(EBHWarFaction Faction)
{
    switch (Faction)
    {
        case EBHWarFaction::Friendly:
            return FLinearColor(0.08f, 0.55f, 0.28f, 1.0f);

        case EBHWarFaction::Enemy:
            return FLinearColor(0.72f, 0.08f, 0.06f, 1.0f);

        default:
            return FLinearColor(0.42f, 0.45f, 0.48f, 1.0f);
    }
}

FString GetFactionLabel(EBHWarFaction Faction)
{
    switch (Faction)
    {
        case EBHWarFaction::Friendly:
            return NSLOCTEXT("BrokenHorizon", "WarMapFactionFriendly", "FRIENDLY CONTROL").ToString();

        case EBHWarFaction::Enemy:
            return NSLOCTEXT("BrokenHorizon", "WarMapFactionEnemy", "ENEMY CONTROL").ToString();

        default:
            return NSLOCTEXT("BrokenHorizon", "WarMapFactionNeutral", "CONTESTED / NEUTRAL").ToString();
    }
}

FString GetSiteTypeLabel(EBHWarSiteType SiteType)
{
    switch (SiteType)
    {
        case EBHWarSiteType::Headquarters:
            return NSLOCTEXT("BrokenHorizon", "WarMapSiteHeadquarters", "HEADQUARTERS").ToString();

        case EBHWarSiteType::Village:
            return NSLOCTEXT("BrokenHorizon", "WarMapSiteVillage", "VILLAGE").ToString();

        case EBHWarSiteType::Town:
            return NSLOCTEXT("BrokenHorizon", "WarMapSiteTown", "TOWN").ToString();

        case EBHWarSiteType::Bridge:
            return NSLOCTEXT("BrokenHorizon", "WarMapSiteBridge", "BRIDGE").ToString();

        case EBHWarSiteType::LogisticsDepot:
            return NSLOCTEXT("BrokenHorizon", "WarMapSiteLogisticsDepot", "LOGISTICS DEPOT").ToString();

        default:
            return NSLOCTEXT("BrokenHorizon", "WarMapSiteCheckpoint", "CHECKPOINT").ToString();
    }
}

FString GetFieldLogisticsLabel(
    const FBHWarSectorState& Sector
)
{
    if (Sector.Owner != EBHWarFaction::Friendly)
    {
        return NSLOCTEXT("BrokenHorizon", "WarMapLogisticsLocked", "LOCKED").ToString();
    }

    return Sector.Supply >= 5.0f
        ? NSLOCTEXT("BrokenHorizon", "WarMapLogisticsOnline", "ONLINE").ToString()
        : NSLOCTEXT("BrokenHorizon", "WarMapLogisticsCheckpointOnly", "CHECKPOINT ONLY").ToString();
}

FLinearColor GetFieldLogisticsColor(
    const FBHWarSectorState& Sector
)
{
    if (Sector.Owner != EBHWarFaction::Friendly)
    {
        return FLinearColor(0.56f, 0.64f, 0.62f, 1.0f);
    }

    return Sector.Supply >= 5.0f
        ? FLinearColor(0.32f, 0.84f, 0.51f, 1.0f)
        : FLinearColor(0.92f, 0.66f, 0.18f, 1.0f);
}

FString GetStrategicSupplyStatus(
    const FBHWarSectorState& Sector,
    const UBHWarSubsystem* WarSubsystem
)
{
    if (Sector.Owner != EBHWarFaction::Neutral &&
        IsValid(WarSubsystem) &&
        !WarSubsystem->IsSectorConnectedToFactionLogistics(
            Sector.SectorID
        ))
    {
        return NSLOCTEXT("BrokenHorizon", "WarMapSupplyCutOff", "CUT OFF").ToString();
    }

    if (Sector.Supply <= KINDA_SMALL_NUMBER)
    {
        return NSLOCTEXT("BrokenHorizon", "WarMapSupplyStarved", "STARVED").ToString();
    }

    if (Sector.Supply < 25.0f)
    {
        return NSLOCTEXT("BrokenHorizon", "WarMapSupplyCritical", "CRITICAL").ToString();
    }

    if (Sector.Supply < 75.0f)
    {
        return NSLOCTEXT("BrokenHorizon", "WarMapSupplyStable", "STABLE").ToString();
    }

    return NSLOCTEXT("BrokenHorizon", "WarMapSupplyStockpiled", "STOCKPILED").ToString();
}

FLinearColor GetStrategicSupplyColor(
    const FBHWarSectorState& Sector,
    const UBHWarSubsystem* WarSubsystem
)
{
    if (Sector.Owner != EBHWarFaction::Neutral &&
        IsValid(WarSubsystem) &&
        !WarSubsystem->IsSectorConnectedToFactionLogistics(
            Sector.SectorID
        ))
    {
        return FLinearColor(0.96f, 0.22f, 0.16f, 1.0f);
    }

    if (Sector.Supply <= KINDA_SMALL_NUMBER)
    {
        return FLinearColor(0.96f, 0.22f, 0.16f, 1.0f);
    }

    if (Sector.Supply < 25.0f)
    {
        return FLinearColor(0.95f, 0.46f, 0.12f, 1.0f);
    }

    if (Sector.Supply < 75.0f)
    {
        return FLinearColor(0.92f, 0.66f, 0.18f, 1.0f);
    }

    return FLinearColor(0.32f, 0.84f, 0.51f, 1.0f);
}

FLinearColor GetStrategicConnectionColor(
    const FBHWarSectorState& StartSector,
    const FBHWarSectorState& EndSector,
    const UBHWarSubsystem* WarSubsystem
)
{
    if (StartSector.Owner == EBHWarFaction::Neutral &&
        EndSector.Owner == EBHWarFaction::Neutral)
    {
        return FLinearColor(0.30f, 0.35f, 0.34f, 0.8f);
    }

    if (StartSector.Owner != EndSector.Owner)
    {
        return FLinearColor(0.92f, 0.66f, 0.18f, 0.9f);
    }

    const bool bConnectedToLogistics =
        StartSector.Owner != EBHWarFaction::Neutral &&
        IsValid(WarSubsystem) &&
        WarSubsystem->IsSectorConnectedToFactionLogistics(
            StartSector.SectorID
        ) &&
        WarSubsystem->IsSectorConnectedToFactionLogistics(
            EndSector.SectorID
        );

    if (!bConnectedToLogistics)
    {
        return FLinearColor(0.82f, 0.28f, 0.12f, 0.82f);
    }

    return StartSector.Owner == EBHWarFaction::Friendly
        ? FLinearColor(0.20f, 0.78f, 0.42f, 0.9f)
        : FLinearColor(0.88f, 0.18f, 0.14f, 0.9f);
}

FString GetConvoyRouteThreatLabel(
    int32 RecentInterdictions
)
{
    if (RecentInterdictions >= 2)
    {
        return NSLOCTEXT("BrokenHorizon", "WarMapRouteThreatHigh", "HIGH").ToString();
    }

    return RecentInterdictions == 1
        ? NSLOCTEXT("BrokenHorizon", "WarMapRouteThreatElevated", "ELEVATED").ToString()
        : NSLOCTEXT("BrokenHorizon", "WarMapRouteThreatClear", "CLEAR").ToString();
}

FLinearColor GetConvoyRouteThreatColor(
    int32 RecentInterdictions
)
{
    if (RecentInterdictions >= 2)
    {
        return FLinearColor(0.96f, 0.22f, 0.16f, 1.0f);
    }

    return RecentInterdictions == 1
        ? FLinearColor(0.92f, 0.66f, 0.18f, 1.0f)
        : FLinearColor(0.56f, 0.64f, 0.62f, 1.0f);
}

void DrawPanel(
    FSlateWindowElementList& OutDrawElements,
    int32 Layer,
    const FGeometry& Geometry,
    const FVector2D& Position,
    const FVector2D& Size,
    const FLinearColor& Color
)
{
    FSlateDrawElement::MakeBox(
        OutDrawElements,
        Layer,
        Geometry.ToPaintGeometry(
            Size,
            FSlateLayoutTransform(Position)
        ),
        GetWhiteBrush(),
        ESlateDrawEffect::None,
        Color
    );
}

void DrawLabel(
    FSlateWindowElementList& OutDrawElements,
    int32 Layer,
    const FGeometry& Geometry,
    const FVector2D& Position,
    const FString& Label,
    const FSlateFontInfo& Font,
    const FLinearColor& Color,
    float MaxWidth = 0.0f
)
{
    FString FittedLabel = Label;
    if (MaxWidth > 0.0f && FSlateApplication::IsInitialized())
    {
        const TSharedRef<FSlateFontMeasure> FontMeasure =
            FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
        if (FontMeasure->Measure(FittedLabel, Font).X > MaxWidth)
        {
            const FString Ellipsis(TEXT("..."));
            while (!FittedLabel.IsEmpty() &&
                FontMeasure->Measure(FittedLabel + Ellipsis, Font).X > MaxWidth)
            {
                FittedLabel.LeftChopInline(1, EAllowShrinking::No);
            }
            FittedLabel.TrimEndInline();
            FittedLabel += Ellipsis;
        }
    }
    FSlateDrawElement::MakeText(
        OutDrawElements,
        Layer,
        Geometry.ToPaintGeometry(
            FVector2D(1.0f, 1.0f),
            FSlateLayoutTransform(Position)
        ),
        FittedLabel,
        Font,
        ESlateDrawEffect::None,
        Color
    );
}

FString JoinSectorIDs(const TArray<FName>& SectorIDs)
{
    FString Result;

    for (int32 Index = 0; Index < SectorIDs.Num(); ++Index)
    {
        if (Index > 0)
        {
            Result += TEXT(", ");
        }

        Result += SectorIDs[Index].ToString();
    }

    return Result.IsEmpty() ? TEXT("None") : Result;
}

FString BuildDeploymentForcePreviewText(
    const UBHWarSubsystem& WarSubsystem,
    FName TargetSectorID,
    EBHWarPriorityType OperationType
)
{
    if (OperationType == EBHWarPriorityType::Recon)
    {
        return FText::Format(
            NSLOCTEXT(
                "BrokenHorizon",
                "WarMapReconForcePreview",
                "RECON // INTEL {0}% > 100% // MOVE AND OBSERVE // AVOID MAJOR CONTACT"
            ),
            FMath::RoundToInt(WarSubsystem.GetSectorIntelConfidence(TargetSectorID))
        ).ToString();
    }

    if (OperationType == EBHWarPriorityType::Resupply)
    {
        const FBHWarSectorState Target =
            WarSubsystem.GetSectorState(TargetSectorID);
        const FName SourceSectorID =
            WarSubsystem.GetOperationSupplySource(
                TargetSectorID,
                OperationType
            );
        const FBHWarSectorState Source =
            WarSubsystem.GetSectorState(SourceSectorID);
        return FText::Format(
            NSLOCTEXT(
                "BrokenHorizon",
                "WarMapLogisticsForcePreview",
                "LOGISTICS // LOAD {0} AT {1} // TARGET {2}% // CAPACITY {3}"
            ),
            FMath::RoundToInt(WarSubsystem.GetOperationSupplyCost(TargetSectorID, OperationType)),
            Source.DisplayName.IsEmpty()
                ? NSLOCTEXT("BrokenHorizon", "WarMapNoStagingSector", "NO STAGING SECTOR")
                : FText::FromString(Source.DisplayName.ToString().ToUpper()),
            FMath::RoundToInt(Target.Supply),
            FMath::RoundToInt(WarSubsystem.GetFieldLogisticsDeliveryCapacity(TargetSectorID))
        ).ToString();
    }

    if (OperationType == EBHWarPriorityType::EscortRescue)
    {
        const FName ConvoyID =
            WarSubsystem.GetEscortOperationTargetID(
                TargetSectorID
            );
        const FBHWarSupplyConvoyState Convoy =
            WarSubsystem.GetSupplyConvoyState(ConvoyID);
        return FText::Format(
            NSLOCTEXT(
                "BrokenHorizon",
                "WarMapEscortForcePreview",
                "ESCORT // {0} // PAYLOAD {1} // ETA {2} TURN // ROUTE {3} > {4}"
            ),
            Convoy.CargoType == EBHWarConvoyCargoType::CivilianAid
                ? NSLOCTEXT("BrokenHorizon", "WarMapCargoCivilianAid", "CIVILIAN AID")
                : NSLOCTEXT("BrokenHorizon", "WarMapCargoMilitarySupply", "MILITARY SUPPLY"),
            FMath::RoundToInt(Convoy.SupplyPayload),
            FMath::Max(0, Convoy.TurnsRemaining),
            FText::FromName(Convoy.SourceSectorID),
            FText::FromName(Convoy.DestinationSectorID)
        ).ToString();
    }

    if (OperationType == EBHWarPriorityType::Rescue)
    {
        return FText::Format(
            NSLOCTEXT(
                "BrokenHorizon",
                "WarMapMedevacForcePreview",
                "MEDEVAC // ASSIGNED CASUALTY // TREATMENT {0} // FOOT OR VEHICLE EXTRACTION"
            ),
            FText::FromString(
                WarSubsystem.GetSectorState(TargetSectorID).DisplayName.ToString().ToUpper()
            )
        ).ToString();
    }

    const float IntelConfidence =
        WarSubsystem.GetSectorIntelConfidence(
            TargetSectorID
        );

    if (IntelConfidence < 45.0f)
    {
        return FText::Format(
            NSLOCTEXT(
                "BrokenHorizon",
                "WarMapUnknownIntelForcePreview",
                "INTEL {0}% // HOSTILES UNKNOWN // RECON REQUIRED // {1}"
            ),
            FMath::RoundToInt(IntelConfidence),
            WarSubsystem.GetSectorEnemyAdaptationSummary(TargetSectorID)
        ).ToString();
    }

    const FBHWarOperationForcePackage Preview =
        BHWarOperationRules::BuildForcePackage(
            &WarSubsystem,
            TargetSectorID,
            OperationType,
            WarSubsystem.GetOperationSupplySource(
                TargetSectorID,
                OperationType
            )
        );
    const bool bConfirmedIntel = IntelConfidence >= 80.0f;
    const auto EstimateCount =
        [bConfirmedIntel](int32 ExactCount)
        {
            return bConfirmedIntel
                ? ExactCount
                : FMath::Max(
                    0,
                    FMath::RoundToInt(
                        ExactCount / 2.0f
                    ) * 2
                );
        };
    const FText IntelLabel = bConfirmedIntel
        ? NSLOCTEXT("BrokenHorizon", "WarMapIntelConfirmed", "CONFIRMED")
        : NSLOCTEXT("BrokenHorizon", "WarMapIntelEstimated", "ESTIMATED");
    const FString AdaptationSuffix =
        Preview.EnemyPatternPreparationLevel > 0
            ? FText::Format(
                NSLOCTEXT("BrokenHorizon", "WarMapCounterSuffix", " // COUNTER +{0}"),
                Preview.EnemyPatternPreparationLevel
            ).ToString()
            : FString();

    if (OperationType ==
        EBHWarPriorityType::Defend)
    {
        return FText::Format(
            NSLOCTEXT(
                "BrokenHorizon",
                "WarMapDefenseForcePreview",
                "{0} // HOSTILES {1}x{2} // SUPPORT {3} // GARRISON {4}{5}"
            ),
            IntelLabel,
            EstimateCount(Preview.DefenseWaveCount),
            EstimateCount(Preview.DefenseEnemiesPerWave),
            Preview.FriendlySupportCount,
            Preview.FriendlyGarrisonCount,
            FText::FromString(AdaptationSuffix)
        ).ToString();
    }

    return FText::Format(
        NSLOCTEXT(
            "BrokenHorizon",
            "WarMapAttackForcePreview",
            "{0} // HOSTILES {1} + {2}x{3} // SUPPORT {4} // GARRISON {5}{6}"
        ),
        IntelLabel,
        EstimateCount(Preview.AttackEnemyCount),
        EstimateCount(Preview.AttackReinforcementWaveCount),
        EstimateCount(Preview.AttackReinforcementsPerWave),
        Preview.FriendlySupportCount,
        EstimateCount(Preview.EnemyGarrisonCount),
        FText::FromString(AdaptationSuffix)
    ).ToString();
}

FString BuildDeploymentReadinessText(
    const UBHWarSubsystem& WarSubsystem,
    FName TargetSectorID,
    EBHWarPriorityType OperationType
)
{
    if (OperationType == EBHWarPriorityType::Recon)
    {
        const FName SourceSectorID =
            WarSubsystem.GetOperationSupplySource(
                TargetSectorID,
                OperationType
            );
        return FText::Format(
            NSLOCTEXT(
                "BrokenHorizon",
                "WarMapReconReadiness",
                "{0} // INTEL {1}% // FIELD REPORT KIT {2} SUPPLY"
            ),
            WarSubsystem.CanFundOperation(TargetSectorID, OperationType)
                ? NSLOCTEXT("BrokenHorizon", "WarMapReady", "READY")
                : SourceSectorID.IsNone()
                    ? NSLOCTEXT("BrokenHorizon", "WarMapBlockedNoRoute", "BLOCKED // NO ROUTE")
                    : NSLOCTEXT("BrokenHorizon", "WarMapBlockedSupply", "BLOCKED // SUPPLY SHORTFALL"),
            FMath::RoundToInt(WarSubsystem.GetSectorIntelConfidence(TargetSectorID)),
            FMath::RoundToInt(WarSubsystem.GetOperationSupplyCost(TargetSectorID, OperationType))
        ).ToString();
    }

    if (OperationType == EBHWarPriorityType::Resupply)
    {
        const FName SourceSectorID =
            WarSubsystem.GetOperationSupplySource(
                TargetSectorID,
                OperationType
            );
        const FBHWarSectorState Source =
            WarSubsystem.GetSectorState(SourceSectorID);
        return FText::Format(
            NSLOCTEXT("BrokenHorizon", "WarMapResupplyReadiness", "{0} // CARGO {1} // SOURCE SUP {2}%"),
            WarSubsystem.CanFundOperation(TargetSectorID, OperationType)
                ? NSLOCTEXT("BrokenHorizon", "WarMapReady", "READY")
                : SourceSectorID.IsNone()
                    ? NSLOCTEXT("BrokenHorizon", "WarMapBlockedNoRoute", "BLOCKED // NO ROUTE")
                    : NSLOCTEXT("BrokenHorizon", "WarMapBlockedCargo", "BLOCKED // CARGO SHORTFALL"),
            FMath::RoundToInt(WarSubsystem.GetOperationSupplyCost(TargetSectorID, OperationType)),
            FMath::RoundToInt(Source.Supply)
        ).ToString();
    }

    if (OperationType == EBHWarPriorityType::EscortRescue)
    {
        const FName ConvoyID =
            WarSubsystem.GetEscortOperationTargetID(
                TargetSectorID
            );
        return FText::Format(
            NSLOCTEXT("BrokenHorizon", "WarMapEscortReadiness", "{0} // CONVOY {1} // PROTECTION DETAIL READY"),
            WarSubsystem.CanFundOperation(TargetSectorID, OperationType)
                ? NSLOCTEXT("BrokenHorizon", "WarMapReady", "READY")
                : NSLOCTEXT("BrokenHorizon", "WarMapBlockedDeploymentSupply", "BLOCKED // DEPLOYMENT SUPPLY"),
            ConvoyID.IsNone()
                ? NSLOCTEXT("BrokenHorizon", "WarMapUnavailable", "UNAVAILABLE")
                : FText::FromName(ConvoyID)
        ).ToString();
    }

    if (OperationType == EBHWarPriorityType::Rescue)
    {
        return FText::Format(
            NSLOCTEXT("BrokenHorizon", "WarMapRescueReadiness", "{0} // CASUALTY ASSIGNED // MEDICAL ROUTE READY"),
            WarSubsystem.CanFundOperation(TargetSectorID, OperationType)
                ? NSLOCTEXT("BrokenHorizon", "WarMapReady", "READY")
                : NSLOCTEXT("BrokenHorizon", "WarMapBlockedDeploymentSupply", "BLOCKED // DEPLOYMENT SUPPLY")
        ).ToString();
    }

    const FBHWarOperationForcePackage Preview =
        BHWarOperationRules::BuildForcePackage(
            &WarSubsystem,
            TargetSectorID,
            OperationType,
            WarSubsystem.GetOperationSupplySource(
                TargetSectorID,
                OperationType
            )
        );
    const FString Readiness =
        WarSubsystem.CanFundOperation(
            TargetSectorID,
            OperationType
        )
            ? NSLOCTEXT("BrokenHorizon", "WarMapReady", "READY").ToString()
            : Preview.SupplySourceSectorID.IsNone()
                ? NSLOCTEXT("BrokenHorizon", "WarMapBlockedNoRoute", "BLOCKED // NO ROUTE").ToString()
                : NSLOCTEXT("BrokenHorizon", "WarMapBlockedSupply", "BLOCKED // SUPPLY SHORTFALL").ToString();

    if (OperationType == EBHWarPriorityType::Attack)
    {
        const TCHAR* ManpowerWarning =
            Preview.bOccupationGarrisonShortfall
                ? TEXT(" // OCCUPATION SHORTFALL")
                : Preview.RemainingStagingGarrisonCount < 2
                    ? TEXT(" // REAR EXPOSED")
                    : TEXT("");

        return FText::Format(
            NSLOCTEXT(
                "BrokenHorizon",
                "WarMapAttackReadiness",
                "{0} // FORCE {1} // SUP {2}% // OCC {3}/{4} // REAR GAR {5}{6}"
            ),
            FText::FromString(Readiness),
            FMath::RoundToInt(Preview.StagingFriendlyStrength),
            FMath::RoundToInt(Preview.StagingSupply),
            Preview.OccupationGarrisonCount,
            Preview.DesiredOccupationGarrisonCount,
            Preview.RemainingStagingGarrisonCount,
            FText::FromString(ManpowerWarning)
        ).ToString();
    }

    return FText::Format(
        NSLOCTEXT("BrokenHorizon", "WarMapGeneralReadiness", "{0} // FORCE {1} // SUP {2}% // GARRISON {3}"),
        FText::FromString(Readiness),
        FMath::RoundToInt(Preview.StagingFriendlyStrength),
        FMath::RoundToInt(Preview.StagingSupply),
        Preview.FriendlyGarrisonCount
    ).ToString();
}

bool HasDeploymentManpowerRisk(
    const UBHWarSubsystem& WarSubsystem,
    FName TargetSectorID,
    EBHWarPriorityType OperationType
)
{
    if (OperationType != EBHWarPriorityType::Attack)
    {
        return false;
    }

    const FBHWarOperationForcePackage Preview =
        BHWarOperationRules::BuildForcePackage(
            &WarSubsystem,
            TargetSectorID,
            OperationType,
            WarSubsystem.GetOperationSupplySource(
                TargetSectorID,
                OperationType
            )
        );

    return Preview.bOccupationGarrisonShortfall ||
        Preview.RemainingStagingGarrisonCount < 2;
}

struct FBHDeploymentTravelPreview
{
    FString Text;
    FLinearColor Color = FLinearColor(
        0.56f,
        0.64f,
        0.62f,
        1.0f
    );
};

FBHDeploymentTravelPreview BuildDeploymentTravelPreview(
    const UObject* WorldContextObject,
    FName TargetSectorID,
    EBHWarPriorityType OperationType
)
{
    FBHDeploymentTravelPreview Preview;
    FNumberFormattingOptions OneDecimalNumberOptions;
    OneDecimalNumberOptions.MinimumFractionalDigits = 1;
    OneDecimalNumberOptions.MaximumFractionalDigits = 1;
    const UWorld* World = IsValid(WorldContextObject)
        ? WorldContextObject->GetWorld()
        : nullptr;
    const ABHCharacter* Player =
        BHPlayerResolver::Find(WorldContextObject);

    if (!IsValid(World) || !IsValid(Player))
    {
        Preview.Text = NSLOCTEXT(
            "BrokenHorizon", "WarMapFieldPlanPlayerUnavailable",
            "FIELD PLAN // PLAYER POSITION UNAVAILABLE").ToString();
        return Preview;
    }

    const ABHSectorAnchor* TargetAnchor = nullptr;

    for (TActorIterator<ABHSectorAnchor> It(World); It; ++It)
    {
        if (IsValid(*It) && It->MatchesSector(TargetSectorID))
        {
            TargetAnchor = *It;
            break;
        }
    }

    if (!IsValid(TargetAnchor))
    {
        Preview.Text = NSLOCTEXT(
            "BrokenHorizon", "WarMapFieldPlanTargetUnavailable",
            "FIELD PLAN // TARGET LOCATION NOT STREAMED").ToString();
        return Preview;
    }

    const FVector PlayerLocation = Player->GetActorLocation();
    const float DistanceCentimeters = FVector::Dist2D(
        PlayerLocation,
        TargetAnchor->GetOperationCenter()
    );
    const float DistanceKilometers =
        DistanceCentimeters / 100000.0f;
    const ABHOpenWorldOperationDirector* DirectorDefaults =
        GetDefault<ABHOpenWorldOperationDirector>();
    const float ApproachSeconds =
        IsValid(DirectorDefaults)
            ? DirectorDefaults
                ->CalculateApproachWindowSecondsForOperation(
                    OperationType,
                    DistanceCentimeters
                )
            : 0.0f;
    const float ApproachMinutes = ApproachSeconds / 60.0f;
    const ABHFieldTransport* NearestOperationalTransport = nullptr;
    const ABHFieldTransport* NearestDisabledTransport = nullptr;
    float NearestOperationalDistance =
        TNumericLimits<float>::Max();
    float NearestDisabledDistance =
        TNumericLimits<float>::Max();

    for (TActorIterator<ABHFieldTransport> It(World); It; ++It)
    {
        const ABHFieldTransport* Transport = *It;

        if (!IsValid(Transport))
        {
            continue;
        }

        const float TransportDistance = FVector::Dist2D(
            PlayerLocation,
            Transport->GetActorLocation()
        );

        if (Transport->GetOccupant() == Player)
        {
            NearestOperationalTransport = Transport;
            NearestOperationalDistance = 0.0f;
            break;
        }

        if (!Transport->IsImmobilized() &&
            TransportDistance < NearestOperationalDistance)
        {
            NearestOperationalTransport = Transport;
            NearestOperationalDistance = TransportDistance;
        }
        else if (Transport->IsImmobilized() &&
                 TransportDistance < NearestDisabledDistance)
        {
            NearestDisabledTransport = Transport;
            NearestDisabledDistance = TransportDistance;
        }
    }

    const ABHFieldTransport* NearestTransport =
        IsValid(NearestOperationalTransport)
            ? NearestOperationalTransport
            : NearestDisabledTransport;
    const float NearestTransportDistance =
        IsValid(NearestOperationalTransport)
            ? NearestOperationalDistance
            : NearestDisabledDistance;

    if (!IsValid(NearestTransport))
    {
        Preview.Text = FText::Format(
            NSLOCTEXT("BrokenHorizon", "WarMapFieldPlanNoTransport",
                "FIELD PLAN // {0} KM // WINDOW {1} MIN // NO TRANSPORT AVAILABLE"),
            FText::AsNumber(DistanceKilometers, &OneDecimalNumberOptions),
            FText::AsNumber(ApproachMinutes, &OneDecimalNumberOptions)).ToString();
        Preview.Color =
            FLinearColor(0.94f, 0.30f, 0.18f, 1.0f);
        return Preview;
    }

    const float VehicleTravelMinutes =
        NearestTransport->GetEstimatedTravelMinutes(
            DistanceCentimeters
        );
    const float VehicleRangeKilometers =
        NearestTransport->GetEstimatedRangeKilometers();
    const bool bVehicleNearby =
        NearestTransportDistance <= 15000.0f;
    const bool bEnoughRange =
        VehicleRangeKilometers >= DistanceKilometers;
    const bool bEnoughTime =
        (VehicleTravelMinutes * 60.0f) + 30.0f <=
        ApproachSeconds;
    const bool bVehicleReady =
        bVehicleNearby &&
        bEnoughRange &&
        bEnoughTime &&
        !NearestTransport->IsImmobilized();
    const FText ReadinessLabel =
        !bVehicleNearby
            ? NSLOCTEXT("BrokenHorizon", "WarMapVehicleNotNearby", "VEHICLE NOT NEARBY")
            : NearestTransport->IsImmobilized()
                ? NSLOCTEXT("BrokenHorizon", "WarMapVehicleImmobilized", "VEHICLE IMMOBILIZED")
                : !bEnoughRange
                    ? NSLOCTEXT("BrokenHorizon", "WarMapVehicleFuelShortfall", "FUEL SHORTFALL")
                    : !bEnoughTime
                        ? NSLOCTEXT("BrokenHorizon", "WarMapVehicleLateRisk", "LATE ARRIVAL RISK")
                        : NSLOCTEXT("BrokenHorizon", "WarMapVehicleReady", "TRANSPORT READY");

    Preview.Text = FText::Format(
        NSLOCTEXT("BrokenHorizon", "WarMapFieldPlanTransport",
            "FIELD PLAN // {0} KM // WINDOW {1} MIN // ETA {2} MIN // RANGE {3} KM // {4}"),
        FText::AsNumber(DistanceKilometers, &OneDecimalNumberOptions),
        FText::AsNumber(ApproachMinutes, &OneDecimalNumberOptions),
        FText::AsNumber(VehicleTravelMinutes, &OneDecimalNumberOptions),
        FText::AsNumber(FMath::RoundToInt(VehicleRangeKilometers)),
        ReadinessLabel).ToString();
    Preview.Color = bVehicleReady
        ? FLinearColor(0.32f, 0.84f, 0.51f, 1.0f)
        : FLinearColor(0.96f, 0.66f, 0.18f, 1.0f);
    return Preview;
}

struct FBHDeploymentLoadoutPreview
{
    FString Text;
    FLinearColor Color = FLinearColor(
        0.56f,
        0.64f,
        0.62f,
        1.0f
    );
    bool bMaterialRisk = false;
    FString RiskSummary;
    FString RecoveryGuidance;
};

FBHDeploymentLoadoutPreview BuildDeploymentLoadoutPreview(
    const UObject* WorldContextObject,
    EBHWarPriorityType OperationType
)
{
    FBHDeploymentLoadoutPreview Preview;
    const ABHCharacter* Player =
        BHPlayerResolver::Find(WorldContextObject);

    if (!IsValid(Player))
    {
        Preview.Text = NSLOCTEXT(
            "BrokenHorizon", "WarMapLoadoutPlayerUnavailable",
            "LOADOUT // PLAYER STATUS UNAVAILABLE").ToString();
        return Preview;
    }

    const UBHWeaponComponent* Weapon =
        Player->GetWeaponComponent();
    const UBHInjuryComponent* Injury =
        Player->GetInjuryComponent();
    const UBHHealthComponent* Health =
        Player->GetHealthComponent();
    const int32 MagazineAmmo = IsValid(Weapon)
        ? Weapon->GetMagazineAmmo()
        : 0;
    const int32 ReserveAmmo = IsValid(Weapon)
        ? Weapon->GetReserveAmmo()
        : 0;
    const int32 TotalAmmo = MagazineAmmo + ReserveAmmo;
    const int32 RecommendedAmmo =
        OperationType == EBHWarPriorityType::Defend
            ? 90
            : OperationType == EBHWarPriorityType::Raid
                ? 45
                : OperationType == EBHWarPriorityType::Recon
                    ? 30
                : 60;
    const int32 DressingCount = IsValid(Injury)
        ? Injury->GetFieldDressingCount()
        : 0;
    const int32 MedkitCount = IsValid(Injury)
        ? Injury->GetMedkitCount()
        : 0;
    const float ArmorPercentage = IsValid(Injury)
        ? Injury->GetBodyArmorDurabilityPercentage()
        : 0.0f;
    const bool bBleeding =
        IsValid(Injury) && Injury->IsBleeding();
    const float HealthPercentage = IsValid(Health)
        ? Health->GetHealthPercentage()
        : 0.0f;
    const int32 GrenadeCount = Player->GetFragGrenadeCount();
    const bool bAmmoRisk = TotalAmmo < RecommendedAmmo;
    const bool bMedicalRisk =
        DressingCount <= 0 || MedkitCount <= 0;
    const bool bConditionRisk =
        bBleeding || HealthPercentage < 0.60f;
    const bool bArmorRisk = ArmorPercentage <= 0.35f;
    const bool bGrenadeGap = GrenadeCount <= 0;
    TArray<FString> RiskLabels;

    if (bConditionRisk)
    {
        RiskLabels.Add(
            (bBleeding
                ? NSLOCTEXT("BrokenHorizon", "WarMapRiskBleeding", "BLEEDING")
                : NSLOCTEXT("BrokenHorizon", "WarMapRiskWounded", "WOUNDED")).ToString()
        );
    }

    if (bAmmoRisk)
    {
        RiskLabels.Add(NSLOCTEXT("BrokenHorizon", "WarMapRiskLowAmmo", "LOW AMMO").ToString());
    }

    if (bMedicalRisk)
    {
        RiskLabels.Add(NSLOCTEXT("BrokenHorizon", "WarMapRiskMedicalGap", "MEDICAL GAP").ToString());
    }

    if (bArmorRisk)
    {
        RiskLabels.Add(NSLOCTEXT("BrokenHorizon", "WarMapRiskArmorLow", "ARMOR LOW").ToString());
    }

    if (bGrenadeGap)
    {
        RiskLabels.Add(NSLOCTEXT("BrokenHorizon", "WarMapRiskNoFrags", "NO FRAGS").ToString());
    }

    Preview.bMaterialRisk =
        bAmmoRisk ||
        bMedicalRisk ||
        bConditionRisk ||
        bArmorRisk;
    const bool bLogisticsRecoveryNeeded =
        bAmmoRisk ||
        bMedicalRisk ||
        bArmorRisk ||
        bGrenadeGap;
    Preview.RecoveryGuidance = bLogisticsRecoveryNeeded
        ? NSLOCTEXT("BrokenHorizon", "WarMapRecoveryResupply", "FOLLOW GREEN RESUPPLY MARKER").ToString()
        : bConditionRisk
            ? NSLOCTEXT("BrokenHorizon", "WarMapRecoveryTreatWounds", "TREAT WOUNDS BEFORE DEPLOYING").ToString()
            : FString();
    Preview.RiskSummary = FString::Join(
        RiskLabels,
        TEXT(" + ")
    );
    const FString StatusLabel = RiskLabels.IsEmpty()
        ? NSLOCTEXT("BrokenHorizon", "WarMapLoadoutReady", "READY").ToString()
        : Preview.RiskSummary;
    Preview.Text = FText::Format(
        NSLOCTEXT("BrokenHorizon", "WarMapLoadoutSummary",
            "LOADOUT // AMMO {0}/{1} // DRESS {2} // MED {3} // ARMOR {4}% // FRAGS {5} // {6}"),
        FText::AsNumber(TotalAmmo), FText::AsNumber(RecommendedAmmo),
        FText::AsNumber(DressingCount), FText::AsNumber(MedkitCount),
        FText::AsNumber(FMath::RoundToInt(ArmorPercentage * 100.0f)),
        FText::AsNumber(GrenadeCount), FText::FromString(StatusLabel)).ToString();
    Preview.Color = Preview.bMaterialRisk
        ? FLinearColor(0.94f, 0.30f, 0.18f, 1.0f)
        : bGrenadeGap
            ? FLinearColor(0.96f, 0.66f, 0.18f, 1.0f)
            : FLinearColor(0.32f, 0.84f, 0.51f, 1.0f);
    return Preview;
}

}

void UBHWarMapWidget::InitializeWarMap(
    UBHWarSubsystem* InWarSubsystem
)
{
    if (WarSubsystem != InWarSubsystem)
    {
        UnbindWarSubsystem();
        WarSubsystem = InWarSubsystem;
        BindWarSubsystem();
    }

    RefreshWarMap();
}

void UBHWarMapWidget::NativeConstruct()
{
    Super::NativeConstruct();
    SetIsFocusable(true);
    BindWarSubsystem();
    RefreshWarMap();
}

void UBHWarMapWidget::NativeDestruct()
{
    UnbindWarSubsystem();
    Super::NativeDestruct();
}

void UBHWarMapWidget::NativeTick(
    const FGeometry& MyGeometry,
    float InDeltaTime
)
{
    Super::NativeTick(MyGeometry, InDeltaTime);

    if (WithdrawalConfirmationExpiresAt >= 0.0f &&
        !IsWithdrawalConfirmationActive())
    {
        WithdrawalConfirmationExpiresAt = -1.0f;
        Invalidate(EInvalidateWidgetReason::Paint);
    }

    if (StrategicActionFeedbackExpiresAt >= 0.0f &&
        !IsStrategicActionFeedbackActive())
    {
        StrategicActionFeedbackExpiresAt = -1.0f;
        StrategicActionFeedback.Reset();
        Invalidate(EInvalidateWidgetReason::Paint);
    }

    if (!bDeploymentMode)
    {
        return;
    }

    APlayerController* PlayerController = GetOwningPlayer();

    if (!IsValid(PlayerController))
    {
        return;
    }

    const bool bEnterDown =
        PlayerController->IsInputKeyDown(EKeys::Enter) ||
        PlayerController->IsInputKeyDown(
            EKeys::Virtual_Gamepad_Accept.GetVirtualKey()
        );

    if (!bEnterDown)
    {
        bDeploymentInputArmed = true;
        return;
    }

    if (bDeploymentInputArmed &&
        (PlayerController->WasInputKeyJustPressed(EKeys::Enter) ||
         PlayerController->WasInputKeyJustPressed(
             EKeys::Virtual_Gamepad_Accept.GetVirtualKey()
         )))
    {
        bDeploymentInputArmed = false;
        TryDeploySelectedOperation();
    }
}

bool UBHWarMapWidget::TryDeploySelectedOperation()
{
    if (!IsValid(WarSubsystem) || !HasSelectedOperation())
    {
        SetStrategicActionFeedback(
            TEXT("DEPLOYMENT BLOCKED // NO VIABLE OPERATION")
        );
        return false;
    }

    const FName TargetSectorID =
        OperationSectorChoices[SelectedOperationIndex];
    const EBHWarPriorityType OperationType =
        OperationTypeChoices[SelectedOperationIndex];

    if (!WarSubsystem->CanFundOperation(
            TargetSectorID,
            OperationType
        ))
    {
        const FName SourceSectorID =
            WarSubsystem->GetOperationSupplySource(
                TargetSectorID,
                OperationType
            );

        if (SourceSectorID.IsNone())
        {
            SetStrategicActionFeedback(
                TEXT(
                    "DEPLOYMENT BLOCKED // "
                    "NO FRIENDLY STAGING ROUTE"
                )
            );
        }
        else
        {
            const FBHWarSectorState Source =
                WarSubsystem->GetSectorState(SourceSectorID);
            SetStrategicActionFeedback(
                FString::Printf(
                    TEXT(
                        "DEPLOYMENT BLOCKED // INSUFFICIENT SUPPLY "
                        "// %s %.0f%% // NEED %.0f"
                    ),
                    *Source.DisplayName.ToString().ToUpper(),
                    Source.Supply,
                    WarSubsystem
                        ->GetOperationSupplyCost(
                            TargetSectorID,
                            OperationType
                        )
                )
            );
        }

        return false;
    }

    const FBHDeploymentLoadoutPreview LoadoutPreview =
        BuildDeploymentLoadoutPreview(this, OperationType);

    if (LoadoutPreview.bMaterialRisk &&
        !IsDeploymentRiskConfirmationActive())
    {
        if (const UWorld* World = GetWorld())
        {
            DeploymentRiskConfirmationExpiresAt =
                World->GetTimeSeconds() + 4.0f;
        }

        SetStrategicActionFeedback(
            FString::Printf(
                TEXT(
                    "READINESS WARNING // %s // %s "
                    "// PRESS ENTER AGAIN TO DEPLOY"
                ),
                *LoadoutPreview.RiskSummary,
                *LoadoutPreview.RecoveryGuidance
            ),
            4.0f
        );
        return false;
    }

    DeploymentRiskConfirmationExpiresAt = -1.0f;
    OnDeployRequested.Broadcast(TargetSectorID, OperationType);
    return true;
}

void UBHWarMapWidget::SetStrategicActionFeedback(
    const FString& Feedback,
    float DurationSeconds
)
{
    StrategicActionFeedback = Feedback;

    if (const UWorld* World = GetWorld())
    {
        StrategicActionFeedbackExpiresAt =
            World->GetTimeSeconds() +
            FMath::Max(0.1f, DurationSeconds);
    }

    Invalidate(EInvalidateWidgetReason::Paint);
}

FString UBHWarMapWidget::BuildStrategicControlRejection(
    bool bHasStrategicAuthority,
    bool bOperationCommitted
)
{
    if (!bHasStrategicAuthority)
    {
        return TEXT("COMMAND LOCKED // HOST AUTHORITY REQUIRED");
    }

    if (bOperationCommitted)
    {
        return TEXT(
            "COMMAND LOCKED // ACTIVE OPERATION IN PROGRESS"
        );
    }

    return FString();
}

FReply UBHWarMapWidget::NativeOnKeyDown(
    const FGeometry& InGeometry,
    const FKeyEvent& InKeyEvent
)
{
    const FKey Key = InKeyEvent.GetKey();

    if (bDeploymentMode &&
        (Key == EKeys::Left ||
         Key == EKeys::A ||
         Key == EKeys::Gamepad_DPad_Left))
    {
        SelectOperationOffset(-1);
        return FReply::Handled();
    }

    if (bDeploymentMode &&
        (Key == EKeys::Right ||
         Key == EKeys::D ||
         Key == EKeys::Gamepad_DPad_Right))
    {
        SelectOperationOffset(1);
        return FReply::Handled();
    }

    if (bDeploymentMode &&
        (Key == EKeys::Enter ||
         Key == EKeys::Virtual_Gamepad_Accept.GetVirtualKey() ||
         Key == EKeys::SpaceBar))
    {
        bDeploymentInputArmed = false;
        TryDeploySelectedOperation();
        return FReply::Handled();
    }

    if (bDeploymentMode &&
        Key == EKeys::R &&
        IsValid(WarSubsystem) &&
        HasSelectedOperation())
    {
        const FName TargetSectorID =
            OperationSectorChoices[SelectedOperationIndex];
        const EBHWarPriorityType OperationType =
            OperationTypeChoices[SelectedOperationIndex];
        const FName SectorID =
            BHWarOperationRules::GetMobilizationSectorID(
                WarSubsystem,
                TargetSectorID,
                OperationType
            );

        if (SectorID.IsNone())
        {
            StrategicActionFeedback =
                TEXT(
                    "MOBILIZATION BLOCKED // "
                    "NO FRIENDLY STAGING ROUTE"
                );

            if (const UWorld* World = GetWorld())
            {
                StrategicActionFeedbackExpiresAt =
                    World->GetTimeSeconds() + 4.0f;
            }

            Invalidate(EInvalidateWidgetReason::Paint);
            return FReply::Handled();
        }

        OnMilitiaRequested.Broadcast(SectorID);
        SetStrategicActionFeedback(
            TEXT("MOBILIZATION REQUEST SENT // AWAITING COMMAND"),
            2.0f
        );
        return FReply::Handled();
    }

    if (bDeploymentMode &&
        Key == EKeys::T &&
        IsValid(WarSubsystem) &&
        HasSelectedOperation())
    {
        const FName TargetSectorID =
            OperationSectorChoices[SelectedOperationIndex];
        const EBHWarPriorityType OperationType =
            OperationTypeChoices[SelectedOperationIndex];
        const FName DestinationSectorID =
            BHWarOperationRules::GetMobilizationSectorID(
                WarSubsystem,
                TargetSectorID,
                OperationType
            );
        if (DestinationSectorID.IsNone())
        {
            SetStrategicActionFeedback(
                TEXT(
                    "REDEPLOYMENT BLOCKED // "
                    "NO VALID FRIENDLY DESTINATION"
                )
            );
            return FReply::Handled();
        }

        OnGarrisonRedeployRequested.Broadcast(
            DestinationSectorID
        );
        SetStrategicActionFeedback(
            TEXT("REDEPLOYMENT REQUEST SENT // AWAITING COMMAND"),
            2.0f
        );
        return FReply::Handled();
    }

    if (bDeploymentMode &&
        Key == EKeys::H &&
        IsValid(WarSubsystem) &&
        HasSelectedOperation())
    {
        const FName TargetSectorID =
            OperationSectorChoices[SelectedOperationIndex];
        const EBHWarPriorityType OperationType =
            OperationTypeChoices[SelectedOperationIndex];
        OnCivilianAidRequested.Broadcast(
            TargetSectorID,
            OperationType
        );
        SetStrategicActionFeedback(
            TEXT("AID REQUEST SENT // AWAITING COMMAND"),
            2.0f
        );
        return FReply::Handled();
    }

    if (!bDeploymentMode &&
        Key == EKeys::F6 &&
        IsValid(WarSubsystem))
    {
        const FString Rejection = BuildStrategicControlRejection(
            WarSubsystem->CanIssueStrategicCommands(),
            WarSubsystem->HasCommittedOperation()
        );
        if (!Rejection.IsEmpty())
        {
            SetStrategicActionFeedback(Rejection);
            return FReply::Handled();
        }

        const EBHCampaignDifficultyPreset CurrentPreset =
            WarSubsystem->GetCampaignDifficulty().Preset;
        const EBHCampaignDifficultyPreset NextPreset =
            CurrentPreset == EBHCampaignDifficultyPreset::Recruit
                ? EBHCampaignDifficultyPreset::Operator
                : CurrentPreset == EBHCampaignDifficultyPreset::Operator
                    ? EBHCampaignDifficultyPreset::Veteran
                    : CurrentPreset ==
                            EBHCampaignDifficultyPreset::Veteran
                        ? EBHCampaignDifficultyPreset::Custom
                        : EBHCampaignDifficultyPreset::Recruit;
        const bool bDifficultyChanged =
            NextPreset == EBHCampaignDifficultyPreset::Custom
                ? WarSubsystem->SetCustomCampaignDifficulty(
                    WarSubsystem->GetCampaignDifficulty()
                )
                : WarSubsystem->SetCampaignDifficultyPreset(
                    NextPreset
                );
        if (bDifficultyChanged)
        {
            const TCHAR* PresetName =
                NextPreset == EBHCampaignDifficultyPreset::Recruit
                    ? TEXT("RECRUIT")
                    : NextPreset == EBHCampaignDifficultyPreset::Veteran
                        ? TEXT("VETERAN")
                        : NextPreset ==
                                EBHCampaignDifficultyPreset::Custom
                            ? TEXT("CUSTOM")
                            : TEXT("OPERATOR");
            SetStrategicActionFeedback(
                FString::Printf(
                    TEXT("CAMPAIGN DIFFICULTY SET // %s"),
                    PresetName
                )
            );
        }
        else
        {
            SetStrategicActionFeedback(
                TEXT("COMMAND UPDATE FAILED // DIFFICULTY UNCHANGED")
            );
        }
        return FReply::Handled();
    }

    if (!bDeploymentMode &&
        (Key == EKeys::F8 ||
         Key == EKeys::Gamepad_LeftShoulder) &&
        IsValid(WarSubsystem))
    {
        const FString Rejection = BuildStrategicControlRejection(
            WarSubsystem->CanIssueStrategicCommands(),
            WarSubsystem->HasCommittedOperation()
        );
        if (!Rejection.IsEmpty())
        {
            SetStrategicActionFeedback(Rejection);
            return FReply::Handled();
        }
        SelectNextCustomDifficultyAxis();
        return FReply::Handled();
    }

    if (!bDeploymentMode &&
        (Key == EKeys::Hyphen ||
         Key == EKeys::Gamepad_DPad_Left) &&
        IsValid(WarSubsystem))
    {
        TryAdjustSelectedCustomDifficultyAxis(-0.05f);
        return FReply::Handled();
    }

    if (!bDeploymentMode &&
        (Key == EKeys::Equals ||
         Key == EKeys::Gamepad_DPad_Right) &&
        IsValid(WarSubsystem))
    {
        TryAdjustSelectedCustomDifficultyAxis(0.05f);
        return FReply::Handled();
    }

    if (!bDeploymentMode &&
        Key == EKeys::F7 &&
        IsValid(WarSubsystem))
    {
        const FString Rejection = BuildStrategicControlRejection(
            WarSubsystem->CanIssueStrategicCommands(),
            WarSubsystem->HasCommittedOperation()
        );
        if (!Rejection.IsEmpty())
        {
            SetStrategicActionFeedback(Rejection);
            return FReply::Handled();
        }

        const EBHOperationTacticalOption Current =
            WarSubsystem->GetActiveTacticalOption();
        TArray<EBHOperationTacticalOption> AvailableOptions;
        AvailableOptions.Add(EBHOperationTacticalOption::None);
        if (WarSubsystem->IsTacticalOptionUnlocked(
                EBHOperationTacticalOption::ReconPlanning))
        {
            AvailableOptions.Add(
                EBHOperationTacticalOption::ReconPlanning);
        }
        if (WarSubsystem->IsTacticalOptionUnlocked(
                EBHOperationTacticalOption::ReinforcementPriority))
        {
            AvailableOptions.Add(
                EBHOperationTacticalOption::ReinforcementPriority);
        }
        if (WarSubsystem->IsTacticalOptionUnlocked(
                EBHOperationTacticalOption::MedicalPreparation))
        {
            AvailableOptions.Add(
                EBHOperationTacticalOption::MedicalPreparation);
        }

        const int32 CurrentIndex = AvailableOptions.IndexOfByKey(Current);
        const EBHOperationTacticalOption Next = AvailableOptions[
            (FMath::Max(0, CurrentIndex) + 1) % AvailableOptions.Num()
        ];
        if (WarSubsystem->SetActiveTacticalOption(Next))
        {
            const TCHAR* OptionName =
                Next == EBHOperationTacticalOption::ReconPlanning
                    ? TEXT("RECON PLANNING")
                    : Next ==
                            EBHOperationTacticalOption::ReinforcementPriority
                        ? TEXT("REINFORCEMENT PRIORITY")
                        : Next ==
                                EBHOperationTacticalOption::MedicalPreparation
                            ? TEXT("MEDICAL PREPARATION")
                            : TEXT("STANDARD PLANNING");
            const TCHAR* OptionEffect =
                Next == EBHOperationTacticalOption::ReconPlanning
                    ? TEXT("+6 SUPPLY // CONFIRMED INTEL")
                    : Next ==
                            EBHOperationTacticalOption::ReinforcementPriority
                        ? TEXT("+12 SUPPLY // +1 SUPPORT OPERATIVE")
                        : Next ==
                                EBHOperationTacticalOption::MedicalPreparation
                            ? TEXT("+8 SUPPLY // +1 MEDKIT // +2 DRESSINGS")
                            : TEXT("BASELINE LOADOUT");
            SetStrategicActionFeedback(FString::Printf(
                TEXT("TACTICAL OPTION SET // %s // %s"),
                OptionName,
                OptionEffect
            ));
        }
        else
        {
            SetStrategicActionFeedback(
                TEXT("COMMAND UPDATE FAILED // TACTICAL PLAN UNCHANGED")
            );
        }
        return FReply::Handled();
    }

    if (!bDeploymentMode &&
        Key == EKeys::BackSpace &&
        IsValid(WarSubsystem) &&
        WarSubsystem->HasCommittedOperation())
    {
        if (IsWithdrawalConfirmationActive())
        {
            WithdrawalConfirmationExpiresAt = -1.0f;
            OnWithdrawRequested.Broadcast();
        }
        else if (const UWorld* World = GetWorld())
        {
            WithdrawalConfirmationExpiresAt =
                World->GetTimeSeconds() + 3.0f;
            Invalidate(EInvalidateWidgetReason::Paint);
        }

        return FReply::Handled();
    }

    if (!bDeploymentMode &&
        (Key == EKeys::M || Key == EKeys::Escape))
    {
        OnCloseRequested.Broadcast();
        return FReply::Handled();
    }

    return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

int32 UBHWarMapWidget::NativePaint(
    const FPaintArgs& Args,
    const FGeometry& AllottedGeometry,
    const FSlateRect& MyCullingRect,
    FSlateWindowElementList& OutDrawElements,
    int32 LayerId,
    const FWidgetStyle& InWidgetStyle,
    bool bParentEnabled
) const
{
    const int32 BaseLayer = Super::NativePaint(
        Args,
        AllottedGeometry,
        MyCullingRect,
        OutDrawElements,
        LayerId,
        InWidgetStyle,
        bParentEnabled
    );
    const FVector2D ViewSize =
        AllottedGeometry.GetLocalSize();

    if (ViewSize.X < 400.0f || ViewSize.Y < 300.0f)
    {
        return BaseLayer;
    }

    const bool bNarrowLayout =
        ViewSize.X < 1500.0f || ViewSize.Y < 900.0f;
    const FSlateFontInfo TitleFont =
        FCoreStyle::GetDefaultFontStyle(
            TEXT("Bold"),
            bNarrowLayout ? 25 : 30
        );
    const FSlateFontInfo HeaderFont =
        FCoreStyle::GetDefaultFontStyle(
            TEXT("Bold"),
            bNarrowLayout ? 17 : 20
        );
    const FSlateFontInfo BodyFont =
        FCoreStyle::GetDefaultFontStyle(
            TEXT("Regular"),
            bNarrowLayout ? 14 : 16
        );
    const FSlateFontInfo SmallFont =
        FCoreStyle::GetDefaultFontStyle(
            TEXT("Regular"),
            bNarrowLayout ? 11 : 13
        );
    const FSlateFontInfo IntelFont =
        FCoreStyle::GetDefaultFontStyle(
            TEXT("Regular"),
            bNarrowLayout ? 10 : 11
        );
    const FLinearColor White(0.92f, 0.95f, 0.94f, 1.0f);
    const FLinearColor Muted(0.56f, 0.64f, 0.62f, 1.0f);
    const bool bHasSelectedOperation = HasSelectedOperation();
    const FName SelectedSectorID = bHasSelectedOperation
        ? OperationSectorChoices[SelectedOperationIndex]
        : NAME_None;
    const EBHWarPriorityType SelectedOperationType =
        bHasSelectedOperation
            ? OperationTypeChoices[SelectedOperationIndex]
            : EBHWarPriorityType::None;
    const FLinearColor Gold(0.92f, 0.66f, 0.18f, 1.0f);
    const EBHWarCampaignOutcome CampaignOutcome =
        IsValid(WarSubsystem)
            ? WarSubsystem->GetCampaignOutcome()
            : EBHWarCampaignOutcome::Ongoing;
    const int32 FriendlyManpower =
        IsValid(WarSubsystem)
            ? WarSubsystem->GetFactionManpowerReserve(
                EBHWarFaction::Friendly
            )
            : 0;
    const float FriendlyRecruitment =
        IsValid(WarSubsystem)
            ? WarSubsystem->GetFactionRecruitmentPerTurn(
                EBHWarFaction::Friendly
            )
            : 0.0f;
    const int32 StrategicPulseSeconds =
        IsValid(WarSubsystem)
            ? FMath::CeilToInt(
                WarSubsystem->GetSecondsUntilNextWarTurn()
            )
            : 0;
    const FString StrategicPulseText = FString::Printf(
        TEXT("%02d:%02d"),
        StrategicPulseSeconds / 60,
        StrategicPulseSeconds % 60
    );
    const FBHCampaignDifficultyProfile Difficulty =
        IsValid(WarSubsystem)
            ? WarSubsystem->GetCampaignDifficulty()
            : BHDifficulty::BuildPreset(
                EBHCampaignDifficultyPreset::Operator
            );
    const FString DifficultyName =
        Difficulty.Preset == EBHCampaignDifficultyPreset::Recruit
            ? NSLOCTEXT("BrokenHorizon", "WarMapDifficultyRecruit", "RECRUIT").ToString()
            : Difficulty.Preset == EBHCampaignDifficultyPreset::Veteran
                ? NSLOCTEXT("BrokenHorizon", "WarMapDifficultyVeteran", "VETERAN").ToString()
                : Difficulty.Preset == EBHCampaignDifficultyPreset::Custom
                    ? NSLOCTEXT("BrokenHorizon", "WarMapDifficultyCustom", "CUSTOM").ToString()
                    : NSLOCTEXT("BrokenHorizon", "WarMapDifficultyOperator", "OPERATOR").ToString();
    const FBHCampaignProgressionState Progression =
        IsValid(WarSubsystem)
            ? WarSubsystem->GetCampaignProgression()
            : FBHCampaignProgressionState();
    const FString TacticalOptionName =
        Progression.ActiveTacticalOption ==
                EBHOperationTacticalOption::ReconPlanning
            ? NSLOCTEXT("BrokenHorizon", "WarMapPlanRecon", "RECON").ToString()
            : Progression.ActiveTacticalOption ==
                    EBHOperationTacticalOption::ReinforcementPriority
                ? NSLOCTEXT("BrokenHorizon", "WarMapPlanReinforce", "REINFORCE").ToString()
                : Progression.ActiveTacticalOption ==
                        EBHOperationTacticalOption::MedicalPreparation
                    ? NSLOCTEXT("BrokenHorizon", "WarMapPlanMedical", "MEDICAL").ToString()
                    : NSLOCTEXT("BrokenHorizon", "WarMapPlanStandard", "STANDARD").ToString();
    const bool bShowRecentEventPanel =
        ViewSize.X >= 1500.0f && !RecentWarEvents.IsEmpty();
    FString CampaignHeader =
        CampaignOutcome == EBHWarCampaignOutcome::Ongoing
            ? bShowRecentEventPanel
                ? FText::Format(
                    NSLOCTEXT(
                        "BrokenHorizon",
                        "WarMapCampaignHeaderWide",
                        "TURN {0} // CONTROL {1}% // RESERVE {2} // UPDATE {3} // {4} // MERIT {5}"
                    ),
                    TurnNumber,
                    FMath::RoundToInt(FriendlyControlPercentage),
                    FriendlyManpower,
                    FText::FromString(StrategicPulseText),
                    FText::FromString(DifficultyName),
                    Progression.CampaignMerit
                ).ToString()
                : FText::Format(
                    NSLOCTEXT(
                        "BrokenHorizon",
                        "WarMapCampaignHeaderCompact",
                        "WAR TURN {0} // CONTROL {1}% // RESERVE {2} // RECRUIT {3}/T // NEXT UPDATE {4} // {5} TEMPO // {6} // MERIT {7} // SUPPORT {8}/3 // PLAN {9}"
                    ),
                    TurnNumber,
                    FMath::RoundToInt(FriendlyControlPercentage),
                    FriendlyManpower,
                    FText::AsNumber(FriendlyRecruitment),
                    FText::FromString(StrategicPulseText),
                    IsValid(WarSubsystem) && WarSubsystem->HasCommittedOperation()
                        ? NSLOCTEXT("BrokenHorizon", "WarMapTempoEngaged", "ENGAGED")
                        : NSLOCTEXT("BrokenHorizon", "WarMapTempoField", "FIELD"),
                    FText::FromString(DifficultyName),
                    Progression.CampaignMerit,
                    Progression.UnlockedCapabilities.Num(),
                    FText::FromString(TacticalOptionName)
                ).ToString()
            : FText::Format(
                NSLOCTEXT("BrokenHorizon", "WarMapCampaignOutcomeHeader", "{0}     WAR TURN {1}"),
                WarSubsystem->GetCampaignOutcomeText(),
                TurnNumber
            ).ToString();
    if (CampaignOutcome == EBHWarCampaignOutcome::Ongoing &&
        Difficulty.Preset == EBHCampaignDifficultyPreset::Custom)
    {
        CampaignHeader += FString::Printf(
            TEXT(" // %s %.2fx"),
            *GetCustomDifficultyAxisLabel(
                SelectedCustomDifficultyAxis
            ),
            GetCustomDifficultyAxisValue(
                Difficulty,
                SelectedCustomDifficultyAxis
            )
        );
    }
    const FLinearColor CampaignHeaderColor =
        CampaignOutcome ==
            EBHWarCampaignOutcome::FriendlyVictory
            ? FLinearColor(0.28f, 0.92f, 0.62f, 1.0f)
            : CampaignOutcome ==
                EBHWarCampaignOutcome::EnemyVictory
                ? FLinearColor(1.0f, 0.25f, 0.16f, 1.0f)
                : Muted;

    DrawPanel(
        OutDrawElements,
        BaseLayer + 1,
        AllottedGeometry,
        FVector2D::ZeroVector,
        ViewSize,
        FLinearColor(0.008f, 0.012f, 0.014f, 0.97f)
    );
    DrawPanel(
        OutDrawElements,
        BaseLayer + 2,
        AllottedGeometry,
        FVector2D(0.0f, 0.0f),
        FVector2D(ViewSize.X, 6.0f),
        Gold
    );

    DrawLabel(
        OutDrawElements,
        BaseLayer + 3,
        AllottedGeometry,
        FVector2D(48.0f, 34.0f),
        NSLOCTEXT("BrokenHorizon", "WarMapStrategicCommandTitle", "BROKEN HORIZON // STRATEGIC COMMAND").ToString(),
        TitleFont,
        White
    );
    DrawLabel(
        OutDrawElements,
        BaseLayer + 3,
        AllottedGeometry,
        FVector2D(50.0f, 82.0f),
        CampaignHeader,
        BodyFont,
        CampaignHeaderColor
    );

    int32 FriendlySectorCount = 0;
    int32 ConnectedFriendlySectorCount = 0;
    int32 CutOffFriendlySectorCount = 0;
    int32 FriendlyLogisticsHubCount = 0;
    float FriendlySupplyStockpile = 0.0f;
    int32 FriendlyConvoyCount = 0;
    int32 EnemyConvoyCount = 0;
    int32 ThreatenedRouteCount = 0;
    const TArray<FBHWarSupplyConvoyState> ActiveConvoys =
        IsValid(WarSubsystem)
            ? WarSubsystem->GetSupplyConvoys()
            : TArray<FBHWarSupplyConvoyState>();

    if (IsValid(WarSubsystem))
    {
        for (const FBHWarSupplyConvoyState& Convoy :
            ActiveConvoys)
        {
            if (Convoy.Owner == EBHWarFaction::Friendly)
            {
                ++FriendlyConvoyCount;
            }
            else if (Convoy.Owner == EBHWarFaction::Enemy)
            {
                ++EnemyConvoyCount;
            }
        }

        for (const FBHWarSectorState& Sector : SectorStates)
        {
            if (
                WarSubsystem->GetRecentConvoyInterdictionCount(
                    Sector.SectorID,
                    3
                ) > 0
            )
            {
                ++ThreatenedRouteCount;
            }

            if (Sector.Owner != EBHWarFaction::Friendly)
            {
                continue;
            }

            ++FriendlySectorCount;
            FriendlySupplyStockpile += Sector.Supply;

            if (WarSubsystem->IsLogisticsHubSector(
                    Sector.SectorID
                ))
            {
                ++FriendlyLogisticsHubCount;
            }

            if (WarSubsystem
                    ->IsSectorConnectedToFactionLogistics(
                        Sector.SectorID
                    ))
            {
                ++ConnectedFriendlySectorCount;
            }
            else
            {
                ++CutOffFriendlySectorCount;
            }
        }
    }

    DrawLabel(
        OutDrawElements,
        BaseLayer + 3,
        AllottedGeometry,
        FVector2D(50.0f, 104.0f),
        FText::Format(
            NSLOCTEXT(
                "BrokenHorizon",
                "WarMapLogisticsSummary",
                "LOGISTICS {0}/{1} // CUT OFF {2} // HUBS {3} // SUPPLY {4} // CONVOYS F{5} E{6} // THREATS {7}"
            ),
            ConnectedFriendlySectorCount,
            FriendlySectorCount,
            CutOffFriendlySectorCount,
            FriendlyLogisticsHubCount,
            FMath::RoundToInt(FriendlySupplyStockpile),
            FriendlyConvoyCount,
            EnemyConvoyCount,
            ThreatenedRouteCount
        ).ToString(),
        SmallFont,
        CutOffFriendlySectorCount > 0
            ? FLinearColor(0.96f, 0.35f, 0.18f, 1.0f)
            : ThreatenedRouteCount > 0
                ? Gold
                : FLinearColor(0.32f, 0.84f, 0.51f, 1.0f)
    );

    if (bShowRecentEventPanel)
    {
        constexpr float EventPanelWidth = 550.0f;
        const FVector2D EventPanelPosition(
            ViewSize.X - EventPanelWidth - 48.0f,
            20.0f
        );
        DrawPanel(
            OutDrawElements,
            BaseLayer + 2,
            AllottedGeometry,
            EventPanelPosition,
            FVector2D(EventPanelWidth, 90.0f),
            FLinearColor(0.035f, 0.045f, 0.045f, 0.96f)
        );
        DrawPanel(
            OutDrawElements,
            BaseLayer + 3,
            AllottedGeometry,
            EventPanelPosition,
            FVector2D(5.0f, 90.0f),
            Gold
        );
        DrawLabel(
            OutDrawElements,
            BaseLayer + 4,
            AllottedGeometry,
            EventPanelPosition + FVector2D(18.0f, 8.0f),
            NSLOCTEXT("BrokenHorizon", "WarMapRecentCampaignLog", "CAMPAIGN LOG // RECENT").ToString(),
            SmallFont,
            Gold
        );

        const int32 VisibleEventCount = FMath::Min(
            3,
            RecentWarEvents.Num()
        );

        for (int32 DisplayIndex = 0;
            DisplayIndex < VisibleEventCount;
            ++DisplayIndex)
        {
            const FBHWarEventRecord& Event =
                RecentWarEvents[
                    RecentWarEvents.Num() - 1 - DisplayIndex
                ];
            const FString EventLine = FString::Printf(
                TEXT("T%03d // %s"),
                Event.TurnNumber,
                *Event.Summary.Left(76)
            );
            DrawLabel(
                OutDrawElements,
                BaseLayer + 4,
                AllottedGeometry,
                EventPanelPosition +
                    FVector2D(
                        18.0f,
                        31.0f + (DisplayIndex * 18.0f)
                    ),
                EventLine,
                IntelFont,
                DisplayIndex == 0 ? White : Muted
            );
        }
    }

    const FString PriorityReason =
        IsValid(WarSubsystem)
            ? WarSubsystem
                ->GetPriorityReasonText()
                .ToString()
                .ToUpper()
            : NSLOCTEXT("BrokenHorizon", "WarMapNoActiveFront", "NO ACTIVE FRONT").ToString();
    const FString PriorityLabel = bNarrowLayout
        ? FText::Format(
            NSLOCTEXT("BrokenHorizon", "WarMapPriorityCompact", "PRIORITY // {0}"),
            FText::FromString(PriorityText.ToString().ToUpper())
        ).ToString()
        : FText::Format(
            NSLOCTEXT("BrokenHorizon", "WarMapPriorityFull", "CURRENT PRIORITY // {0} // {1}"),
            FText::FromString(PriorityText.ToString().ToUpper()),
            FText::FromString(PriorityReason)
        ).ToString();
    DrawPanel(
        OutDrawElements,
        BaseLayer + 2,
        AllottedGeometry,
        FVector2D(48.0f, 122.0f),
        FVector2D(ViewSize.X - 96.0f, 58.0f),
        FLinearColor(0.10f, 0.12f, 0.12f, 0.96f)
    );
    DrawPanel(
        OutDrawElements,
        BaseLayer + 3,
        AllottedGeometry,
        FVector2D(48.0f, 122.0f),
        FVector2D(6.0f, 58.0f),
        Gold
    );
    DrawLabel(
        OutDrawElements,
        BaseLayer + 4,
        AllottedGeometry,
        FVector2D(70.0f, 138.0f),
        PriorityLabel,
        HeaderFont,
        Gold
    );
    DrawLabel(
        OutDrawElements,
        BaseLayer + 3,
        AllottedGeometry,
        FVector2D(70.0f, 176.0f),
        NSLOCTEXT(
            "BrokenHorizon",
            "WarMapRouteLegend",
            "ROUTES // GREEN FRIENDLY // RED HOSTILE // AMBER FRONT // ORANGE CUT OFF // BRIGHT TRANSIT"
        ).ToString(),
        SmallFont,
        Muted
    );

    if (SectorStates.IsEmpty())
    {
        DrawLabel(
            OutDrawElements,
            BaseLayer + 4,
            AllottedGeometry,
            FVector2D(50.0f, 225.0f),
            NSLOCTEXT("BrokenHorizon", "WarMapNoSectorData", "NO SECTOR DATA AVAILABLE").ToString(),
            HeaderFont,
            FLinearColor::Red
        );
        return BaseLayer + 4;
    }

    const float OuterMargin = 48.0f;
    const float CardGap = 20.0f;
    const float RowGap = 18.0f;
    const float CardTop = 210.0f;
    const float FooterHeight = bDeploymentMode
        ? 178.0f
        : 78.0f;
    const int32 ColumnCount = FMath::Clamp(
        SectorStates.Num(),
        1,
        3
    );
    const int32 RowCount = FMath::DivideAndRoundUp(
        SectorStates.Num(),
        ColumnCount
    );
    const float CardHeight = FMath::Max(
        180.0f,
        (
            ViewSize.Y -
            CardTop -
            FooterHeight -
            (RowGap * (RowCount - 1))
        ) / RowCount
    );
    const float CardWidth = (
        ViewSize.X -
        (OuterMargin * 2.0f) -
        (CardGap * (ColumnCount - 1))
    ) / ColumnCount;
    const bool bCompactCards =
        SectorStates.Num() > 3 || CardHeight < 300.0f;
    TMap<FName, FVector2D> SectorCenters;

    for (int32 Index = 0; Index < SectorStates.Num(); ++Index)
    {
        const int32 ColumnIndex = Index % ColumnCount;
        const int32 RowIndex = Index / ColumnCount;
        const float CardX =
            OuterMargin +
            (ColumnIndex * (CardWidth + CardGap));
        const float CardY =
            CardTop +
            (RowIndex * (CardHeight + RowGap));
        SectorCenters.Add(
            SectorStates[Index].SectorID,
            FVector2D(
                CardX + (CardWidth * 0.5f),
                CardY + (CardHeight * 0.5f)
            )
        );
    }

    for (const FBHWarSectorState& Sector : SectorStates)
    {
        const FVector2D* Start =
            SectorCenters.Find(Sector.SectorID);

        if (!Start)
        {
            continue;
        }

        for (const FName ConnectedID :
            Sector.ConnectedSectorIDs)
        {
            const FVector2D* End =
                SectorCenters.Find(ConnectedID);

            if (!End ||
                Sector.SectorID.ToString() >
                    ConnectedID.ToString())
            {
                continue;
            }

            TArray<FVector2f> ConnectionPoints;
            ConnectionPoints.Add(FVector2f(*Start));
            ConnectionPoints.Add(FVector2f(*End));
            const FBHWarSectorState* ConnectedSector =
                SectorStates.FindByPredicate(
                    [ConnectedID](
                        const FBHWarSectorState& Candidate
                    )
                    {
                        return Candidate.SectorID == ConnectedID;
                    }
                );
            const FLinearColor ConnectionColor =
                ConnectedSector
                    ? GetStrategicConnectionColor(
                        Sector,
                        *ConnectedSector,
                        WarSubsystem
                    )
                    : FLinearColor(
                        0.30f,
                        0.35f,
                        0.34f,
                        0.8f
                    );
            FSlateDrawElement::MakeLines(
                OutDrawElements,
                BaseLayer + 2,
                AllottedGeometry.ToPaintGeometry(),
                ConnectionPoints,
                ESlateDrawEffect::None,
                ConnectionColor,
                true,
                4.0f
            );

            float TransitCargo = 0.0f;
            EBHWarFaction TransitFaction =
                EBHWarFaction::Neutral;

            for (const FBHWarSupplyConvoyState& Convoy :
                ActiveConvoys)
            {
                const bool bMatchesConnection =
                    (Convoy.SourceSectorID == Sector.SectorID &&
                     Convoy.DestinationSectorID == ConnectedID) ||
                    (Convoy.SourceSectorID == ConnectedID &&
                     Convoy.DestinationSectorID == Sector.SectorID);

                if (bMatchesConnection)
                {
                    TransitCargo += Convoy.SupplyPayload;
                    TransitFaction = Convoy.Owner;
                }
            }

            if (TransitCargo > KINDA_SMALL_NUMBER)
            {
                const FLinearColor TransitColor =
                    TransitFaction == EBHWarFaction::Friendly
                        ? FLinearColor(
                            0.30f,
                            0.86f,
                            0.96f,
                            0.95f
                        )
                        : FLinearColor(
                            1.0f,
                            0.34f,
                            0.22f,
                            0.95f
                        );
                FSlateDrawElement::MakeLines(
                    OutDrawElements,
                    BaseLayer + 3,
                    AllottedGeometry.ToPaintGeometry(),
                    ConnectionPoints,
                    ESlateDrawEffect::None,
                    TransitColor,
                    true,
                    6.0f
                );
            }
        }
    }

    if (bDeploymentMode && IsValid(WarSubsystem))
    {
        const TArray<FName> SupplyRoute =
            WarSubsystem->GetOperationSupplyRoute(
                SelectedSectorID,
                SelectedOperationType
            );

        for (int32 RouteIndex = 1;
            RouteIndex < SupplyRoute.Num();
            ++RouteIndex)
        {
            const FVector2D* RouteStart =
                SectorCenters.Find(SupplyRoute[RouteIndex - 1]);
            const FVector2D* RouteEnd =
                SectorCenters.Find(SupplyRoute[RouteIndex]);

            if (!RouteStart || !RouteEnd)
            {
                continue;
            }

            TArray<FVector2f> RoutePoints;
            RoutePoints.Add(FVector2f(*RouteStart));
            RoutePoints.Add(FVector2f(*RouteEnd));
            FSlateDrawElement::MakeLines(
                OutDrawElements,
                BaseLayer + 3,
                AllottedGeometry.ToPaintGeometry(),
                RoutePoints,
                ESlateDrawEffect::None,
                FLinearColor(0.01f, 0.02f, 0.02f, 0.95f),
                true,
                11.0f
            );
            FSlateDrawElement::MakeLines(
                OutDrawElements,
                BaseLayer + 3,
                AllottedGeometry.ToPaintGeometry(),
                RoutePoints,
                ESlateDrawEffect::None,
                Gold,
                true,
                5.0f
            );
        }
    }

    for (int32 Index = 0; Index < SectorStates.Num(); ++Index)
    {
        const FBHWarSectorState& Sector = SectorStates[Index];
        const int32 ColumnIndex = Index % ColumnCount;
        const int32 RowIndex = Index / ColumnCount;
        const float CardX =
            OuterMargin +
            (ColumnIndex * (CardWidth + CardGap));
        const float CardY =
            CardTop +
            (RowIndex * (CardHeight + RowGap));
        const FVector2D CardPosition(CardX, CardY);
        const FLinearColor FactionColor =
            GetFactionColor(Sector.Owner);
        const bool bPriorityTarget =
            IsValid(WarSubsystem) &&
            Sector.SectorID ==
                WarSubsystem->GetPrioritySectorID();
        const bool bSelectedTarget =
            bDeploymentMode &&
            Sector.SectorID == SelectedSectorID;
        const bool bSupplySource =
            bDeploymentMode &&
            IsValid(WarSubsystem) &&
            Sector.SectorID ==
                WarSubsystem
                    ->GetOperationSupplySource(
                        SelectedSectorID,
                        SelectedOperationType
                    );
        const bool bLogisticsHub =
            IsValid(WarSubsystem) &&
            WarSubsystem->IsLogisticsHubSector(
                Sector.SectorID
            );
        const bool bCommittedOperation =
            IsValid(WarSubsystem) &&
            WarSubsystem->HasCommittedOperation() &&
            Sector.SectorID ==
                WarSubsystem
                    ->GetCommittedOperationSectorID();
        const float IsolationAttrition =
            IsValid(WarSubsystem)
                ? WarSubsystem
                    ->GetSectorIsolationAttritionPerTurn(
                        Sector.SectorID
                    )
                : 0.0f;
        const float IncomingConvoySupply =
            IsValid(WarSubsystem)
                ? WarSubsystem->GetIncomingConvoySupply(
                    Sector.SectorID
                )
                : 0.0f;
        const float OutgoingConvoySupply =
            IsValid(WarSubsystem)
                ? WarSubsystem->GetOutgoingConvoySupply(
                    Sector.SectorID
                )
                : 0.0f;
        const int32 RecentRouteInterdictions =
            IsValid(WarSubsystem)
                ? WarSubsystem
                    ->GetRecentConvoyInterdictionCount(
                        Sector.SectorID,
                        3
                    )
                : 0;
        const FString RouteThreatLabel =
            GetConvoyRouteThreatLabel(
                RecentRouteInterdictions
            );
        const FLinearColor RouteThreatColor =
            GetConvoyRouteThreatColor(
                RecentRouteInterdictions
            );
        FString ContactAndRouteLine =
            Sector.LastBattleTurn == INDEX_NONE
                ? NSLOCTEXT("BrokenHorizon", "WarMapLastContactNone", "LAST CONTACT // NONE").ToString()
                : FText::Format(
                    NSLOCTEXT("BrokenHorizon", "WarMapLastContactTurn", "LAST CONTACT // TURN {0}"),
                    Sector.LastBattleTurn
                ).ToString();
        ContactAndRouteLine += FText::Format(
            NSLOCTEXT("BrokenHorizon", "WarMapRouteSuffix", " // ROUTE {0}"),
            FText::FromString(RouteThreatLabel)
        ).ToString();

        if (RecentRouteInterdictions > 0)
        {
            ContactAndRouteLine += FString::Printf(
                TEXT(" (%d LOSS%s)"),
                RecentRouteInterdictions,
                RecentRouteInterdictions == 1
                    ? TEXT("")
                    : TEXT("ES")
            );
        }

        const bool bHasConvoyTransit =
            IncomingConvoySupply > KINDA_SMALL_NUMBER ||
            OutgoingConvoySupply > KINDA_SMALL_NUMBER;
        const FLinearColor ConvoyTransitColor =
            Sector.Owner == EBHWarFaction::Friendly
                ? FLinearColor(
                    0.30f,
                    0.86f,
                    0.96f,
                    1.0f
                )
                : Sector.Owner == EBHWarFaction::Enemy
                    ? FLinearColor(
                        1.0f,
                        0.34f,
                        0.22f,
                        1.0f
                    )
                    : Muted;
        FString StrategicRole;
        FLinearColor StrategicRoleColor = Muted;

        if (bSupplySource && bCommittedOperation)
        {
            StrategicRole = NSLOCTEXT("BrokenHorizon", "WarMapRoleSupplyActive", "SUPPLY SOURCE // ACTIVE OPERATION").ToString();
            StrategicRoleColor = Gold;
        }
        else if (bCommittedOperation)
        {
            StrategicRole = NSLOCTEXT("BrokenHorizon", "WarMapRoleActiveOperation", "ACTIVE OPERATION").ToString();
            StrategicRoleColor = Gold;
        }
        else if (bSupplySource && bPriorityTarget)
        {
            StrategicRole = NSLOCTEXT("BrokenHorizon", "WarMapRoleSupplyPriority", "SUPPLY SOURCE // PRIORITY TARGET").ToString();
            StrategicRoleColor = Gold;
        }
        else if (bSupplySource)
        {
            StrategicRole = bLogisticsHub
                ? NSLOCTEXT("BrokenHorizon", "WarMapRoleHubSupply", "LOGISTICS HUB // SUPPLY SOURCE").ToString()
                : NSLOCTEXT("BrokenHorizon", "WarMapRoleSupplySource", "SUPPLY SOURCE").ToString();
            StrategicRoleColor =
                FLinearColor(0.32f, 0.84f, 0.51f, 1.0f);
        }
        else if (bSelectedTarget && bPriorityTarget)
        {
            StrategicRole = NSLOCTEXT("BrokenHorizon", "WarMapRoleSelectedPriority", "SELECTED // COMMAND PRIORITY").ToString();
            StrategicRoleColor = Gold;
        }
        else if (bSelectedTarget)
        {
            StrategicRole = NSLOCTEXT("BrokenHorizon", "WarMapRoleSelectedOperation", "SELECTED OPERATION").ToString();
            StrategicRoleColor =
                FLinearColor(0.30f, 0.86f, 0.96f, 1.0f);
        }
        else if (bPriorityTarget)
        {
            StrategicRole = NSLOCTEXT("BrokenHorizon", "WarMapRolePriorityTarget", "PRIORITY TARGET").ToString();
            StrategicRoleColor = Gold;
        }
        else if (bLogisticsHub)
        {
            StrategicRole = NSLOCTEXT("BrokenHorizon", "WarMapRoleLogisticsHub", "LOGISTICS HUB").ToString();
            StrategicRoleColor =
                FLinearColor(0.42f, 0.72f, 0.92f, 1.0f);
        }

        DrawPanel(
            OutDrawElements,
            BaseLayer + 3,
            AllottedGeometry,
            CardPosition - FVector2D(
                bSelectedTarget ? 6.0f : 3.0f,
                bSelectedTarget ? 6.0f : 3.0f
            ),
            FVector2D(
                CardWidth + (bSelectedTarget ? 12.0f : 6.0f),
                CardHeight + (bSelectedTarget ? 12.0f : 6.0f)
            ),
            bSelectedTarget
                ? Gold
                : FactionColor.CopyWithNewOpacity(0.85f)
        );
        DrawPanel(
            OutDrawElements,
            BaseLayer + 4,
            AllottedGeometry,
            CardPosition,
            FVector2D(CardWidth, CardHeight),
            FLinearColor(0.035f, 0.045f, 0.045f, 0.98f)
        );
        DrawPanel(
            OutDrawElements,
            BaseLayer + 5,
            AllottedGeometry,
            CardPosition,
            FVector2D(CardWidth, 10.0f),
            FactionColor
        );

        const float TextX = CardX + 22.0f;
        float TextY = CardY + 24.0f;

        DrawLabel(
            OutDrawElements,
            BaseLayer + 6,
            AllottedGeometry,
            FVector2D(TextX, TextY),
            Sector.DisplayName.ToString().ToUpper(),
            HeaderFont,
            White
        );
        TextY += bCompactCards ? 30.0f : 38.0f;
        DrawLabel(
            OutDrawElements,
            BaseLayer + 6,
            AllottedGeometry,
            FVector2D(TextX, TextY),
            FText::Format(
                NSLOCTEXT("BrokenHorizon", "WarMapFactionSite", "{0} // {1}"),
                FText::FromString(GetFactionLabel(Sector.Owner)),
                FText::FromString(GetSiteTypeLabel(Sector.SiteType))
            ).ToString(),
            SmallFont,
            FactionColor,
            CardWidth - 44.0f
        );

        if (bCompactCards)
        {
            TextY += 28.0f;
            int32 ConstructedFortifications = 0;
            int32 UnfinishedFortifications = 0;
            float FortificationDefense = 0.0f;
            int32 DamagedFortifications = 0;
            int32 RubbleFortifications = 0;
            int32 SectorCacheCharges = 0;
            int32 SectorCacheCapacity = 0;
            int32 SectorRallyDeployments = 0;
            int32 SectorRallyCapacity = 0;
            if (IsValid(WarSubsystem))
            {
                WarSubsystem->GetSectorFortificationSummary(
                    Sector.SectorID,
                    ConstructedFortifications,
                    UnfinishedFortifications,
                    FortificationDefense
                );
                UWorld* MapWorld = GetWorld();
                if (IsValid(MapWorld))
                {
                    for (TActorIterator<ABHFieldFortification> It(MapWorld); IsValid(*It); ++It)
                    {
                        const ABHFieldFortification* Fortification = *It;
                        if (!IsValid(Fortification) ||
                            Fortification->GetSectorID() != Sector.SectorID)
                        {
                            continue;
                        }
                        if (Fortification->GetSelectedPlan() == EBHFortificationPlan::FieldSupplyCache)
                        {
                            SectorCacheCharges += Fortification->GetSupplyCacheChargesRemaining();
                            SectorCacheCapacity += Fortification->GetMaxSupplyCacheCharges();
                        }
                        else if (Fortification->GetSelectedPlan() == EBHFortificationPlan::FieldRallyPoint)
                        {
                            SectorRallyDeployments += Fortification->GetRallyDeploymentsRemaining();
                            SectorRallyCapacity += Fortification->GetMaxRallyDeployments();
                        }
                        if (Fortification->IsDismantling())
                        {
                            ++RubbleFortifications;
                        }
                        else if (Fortification->GetHealthFraction() < 1.0f &&
                            Fortification->GetHealthFraction() > 0.0f)
                        {
                            ++DamagedFortifications;
                        }
                    }
                }
            }
            DrawLabel(
                OutDrawElements,
                BaseLayer + 6,
                AllottedGeometry,
                FVector2D(TextX, TextY),
                FText::Format(
                    NSLOCTEXT(
                        "BrokenHorizon",
                        "WarMapCompactForce",
                        "FORCE {0} // GAR {1}/{2} // LOCALS {3}% // FORT {4}/{5} // +{6} // DAM {7} // RBL {8} // CACHE {9}/{10} // RALLY {11}/{12}"
                    ),
                    FMath::RoundToInt(Sector.FriendlyStrength),
                    Sector.FriendlyGarrison,
                    Sector.GarrisonCapacity,
                    FMath::RoundToInt(Sector.CivilianSupport),
                    FMath::Max(ConstructedFortifications, 0),
                    FMath::Max(UnfinishedFortifications, 0),
                    FMath::RoundToInt(
                        FortificationDefense
                    ),
                    FMath::Max(DamagedFortifications, 0),
                    FMath::Max(RubbleFortifications, 0),
                    SectorCacheCharges,
                    FMath::Max(SectorCacheCapacity, 1),
                    FMath::Max(SectorRallyDeployments, 0),
                    FMath::Max(SectorRallyCapacity, 1)
                ).ToString(),
                BodyFont,
                FLinearColor(0.32f, 0.84f, 0.51f, 1.0f),
                CardWidth - 44.0f
            );
            TextY += 25.0f;
            DrawLabel(
                OutDrawElements,
                BaseLayer + 6,
                AllottedGeometry,
                FVector2D(TextX, TextY),
                IsValid(WarSubsystem)
                    ? WarSubsystem->GetSectorEnemyIntelSummary(
                        Sector.SectorID
                    ).ToString().Left(48)
                    : TEXT("INTEL UNAVAILABLE"),
                IntelFont,
                FLinearColor(0.95f, 0.32f, 0.25f, 1.0f),
                CardWidth - 44.0f
            );
            TextY += 23.0f;
            FString SupplyLine = FText::Format(
                NSLOCTEXT(
                    "BrokenHorizon",
                    "WarMapCompactSupply",
                    "SUPPLY {0}% // {1}/T // {2}"
                ),
                FMath::RoundToInt(Sector.Supply),
                FText::AsNumber(
                    IsValid(WarSubsystem)
                        ? WarSubsystem->GetSectorSupplyChangePerTurn(Sector.SectorID)
                        : 0.0f
                ),
                FText::FromString(GetStrategicSupplyStatus(Sector, WarSubsystem))
            ).ToString();

            if (IsolationAttrition > KINDA_SMALL_NUMBER)
            {
                SupplyLine += FString::Printf(
                    TEXT(" // LOSS -%.1f"),
                    IsolationAttrition
                );
            }

            DrawLabel(
                OutDrawElements,
                BaseLayer + 6,
                AllottedGeometry,
                FVector2D(TextX, TextY),
                SupplyLine,
                SmallFont,
                GetStrategicSupplyColor(
                    Sector,
                    WarSubsystem
                ),
                CardWidth - 44.0f
            );
            TextY += 24.0f;

            DrawLabel(
                OutDrawElements,
                BaseLayer + 6,
                AllottedGeometry,
                FVector2D(TextX, TextY),
                FText::Format(
                    NSLOCTEXT(
                        "BrokenHorizon",
                        "WarMapCompactBaseResponseRoute",
                        "BASE {0} // {1} // ROUTE {2}"
                    ),
                    FText::FromString(GetFieldLogisticsLabel(Sector)),
                    IsValid(WarSubsystem)
                        ? WarSubsystem->GetSectorEnemyResponseSummary(Sector.SectorID)
                        : NSLOCTEXT("BrokenHorizon", "WarMapResponseUnknown", "RESPONSE UNKNOWN"),
                    FText::FromString(RouteThreatLabel)
                ).ToString(),
                SmallFont,
                RecentRouteInterdictions > 0
                    ? RouteThreatColor
                    : GetFieldLogisticsColor(Sector),
                CardWidth - 44.0f
            );
            TextY += 24.0f;

            if (!StrategicRole.IsEmpty())
            {
                DrawLabel(
                    OutDrawElements,
                    BaseLayer + 6,
                    AllottedGeometry,
                    FVector2D(TextX, TextY),
                    StrategicRole,
                    SmallFont,
                    StrategicRoleColor,
                    CardWidth - 44.0f
                );
            }

            continue;
        }

        TextY += 52.0f;
        FString FriendlyForceLine = FText::Format(
            NSLOCTEXT(
                "BrokenHorizon",
                "WarMapFriendlyForce",
                "FRIENDLY FORCE {0} // GARRISON {1} // LOCAL SUPPORT {2}%"
            ),
            FMath::RoundToInt(Sector.FriendlyStrength),
            Sector.FriendlyGarrison,
            FMath::RoundToInt(Sector.CivilianSupport)
        ).ToString();
        int32 ConstructedFortifications = 0;
        int32 UnfinishedFortifications = 0;
        float FortificationDefense = 0.0f;
        int32 DamagedFortifications = 0;
        int32 RubbleFortifications = 0;
        int32 SectorCacheCharges = 0;
        int32 SectorCacheCapacity = 0;
        int32 SectorRallyDeployments = 0;
        int32 SectorRallyCapacity = 0;
        if (IsValid(WarSubsystem))
        {
            WarSubsystem->GetSectorFortificationSummary(
                Sector.SectorID,
                ConstructedFortifications,
                UnfinishedFortifications,
                FortificationDefense
            );
            UWorld* MapWorld = GetWorld();
            if (IsValid(MapWorld))
            {
                for (TActorIterator<ABHFieldFortification> It(MapWorld); IsValid(*It); ++It)
                {
                    const ABHFieldFortification* Fortification = *It;
                    if (!IsValid(Fortification) ||
                        Fortification->GetSectorID() != Sector.SectorID)
                    {
                        continue;
                    }
                    if (Fortification->GetSelectedPlan() == EBHFortificationPlan::FieldSupplyCache)
                    {
                        SectorCacheCharges += Fortification->GetSupplyCacheChargesRemaining();
                        SectorCacheCapacity += Fortification->GetMaxSupplyCacheCharges();
                    }
                    else if (Fortification->GetSelectedPlan() == EBHFortificationPlan::FieldRallyPoint)
                    {
                        SectorRallyDeployments += Fortification->GetRallyDeploymentsRemaining();
                        SectorRallyCapacity += Fortification->GetMaxRallyDeployments();
                    }
                    if (Fortification->IsDismantling())
                    {
                        ++RubbleFortifications;
                    }
                    else if (Fortification->GetHealthFraction() < 1.0f &&
                        Fortification->GetHealthFraction() > 0.0f)
                    {
                        ++DamagedFortifications;
                    }
                }
            }
        }
        if (ConstructedFortifications > 0 || UnfinishedFortifications > 0 ||
            SectorCacheCharges > 0 || SectorRallyDeployments > 0 ||
            RubbleFortifications > 0)
        {
            FriendlyForceLine += FString::Printf(
                TEXT(" // FORT %d/%d"),
                ConstructedFortifications,
                UnfinishedFortifications
            );
            if (FortificationDefense > KINDA_SMALL_NUMBER)
            {
                FriendlyForceLine += FString::Printf(
                    TEXT(" // FORTIFIED +%.0f"),
                    FortificationDefense
                );
            }
            FriendlyForceLine += FString::Printf(
                TEXT(" // DAM %d // RBL %d // CACHE %d/%d // RALLY %d/%d"),
                FMath::Max(DamagedFortifications, 0),
                FMath::Max(RubbleFortifications, 0),
                SectorCacheCharges,
                FMath::Max(SectorCacheCapacity, 1),
                SectorRallyDeployments,
                FMath::Max(SectorRallyCapacity, 1)
            );
        }
        const int32 IncomingGarrison =
            IsValid(WarSubsystem)
                ? WarSubsystem
                    ->GetIncomingGarrisonTransferCount(
                        Sector.SectorID
                    )
                : 0;

        if (IncomingGarrison > 0)
        {
            FriendlyForceLine += FString::Printf(
                TEXT(" // INBOUND +%d ETA %dT"),
                IncomingGarrison,
                WarSubsystem
                    ->GetIncomingGarrisonTransferTurns(
                        Sector.SectorID
                    )
            );
        }

        DrawLabel(
            OutDrawElements,
            BaseLayer + 6,
            AllottedGeometry,
            FVector2D(TextX, TextY),
            FriendlyForceLine,
            BodyFont,
            FLinearColor(0.32f, 0.84f, 0.51f, 1.0f)
        );
        TextY += 30.0f;
        DrawLabel(
            OutDrawElements,
            BaseLayer + 6,
            AllottedGeometry,
            FVector2D(TextX, TextY),
            IsValid(WarSubsystem)
                ? WarSubsystem->GetSectorEnemyIntelSummary(
                    Sector.SectorID
                ).ToString()
                : TEXT("INTEL UNAVAILABLE"),
            BodyFont,
            FLinearColor(0.95f, 0.32f, 0.25f, 1.0f)
        );
        TextY += 46.0f;
        FString SupplyLine = FText::Format(
            NSLOCTEXT(
                "BrokenHorizon",
                "WarMapSupplyFlow",
                "SUPPLY {0}% // FLOW {1} / TURN // {2}"
            ),
            FMath::RoundToInt(Sector.Supply),
            FText::AsNumber(
                IsValid(WarSubsystem)
                    ? WarSubsystem->GetSectorSupplyChangePerTurn(Sector.SectorID)
                    : 0.0f
            ),
            FText::FromString(GetStrategicSupplyStatus(Sector, WarSubsystem))
        ).ToString();

        if (IsolationAttrition > KINDA_SMALL_NUMBER)
        {
            SupplyLine += FString::Printf(
                TEXT(" // ATTRITION -%.1f / TURN"),
                IsolationAttrition
            );
        }

        DrawLabel(
            OutDrawElements,
            BaseLayer + 6,
            AllottedGeometry,
            FVector2D(TextX, TextY),
            SupplyLine,
            BodyFont,
            GetStrategicSupplyColor(
                Sector,
                WarSubsystem
            )
        );
        TextY += 30.0f;

        if (bHasConvoyTransit)
        {
            DrawLabel(
                OutDrawElements,
                BaseLayer + 6,
                AllottedGeometry,
                FVector2D(TextX, TextY),
                FString::Printf(
                    TEXT(
                        "CONVOY TRANSIT     IN +%.0f // OUT -%.0f"
                    ),
                    IncomingConvoySupply,
                    OutgoingConvoySupply
                ),
                SmallFont,
                ConvoyTransitColor
            );
            TextY += 30.0f;
        }

        DrawLabel(
            OutDrawElements,
            BaseLayer + 6,
            AllottedGeometry,
            FVector2D(TextX, TextY),
                FText::Format(
                    NSLOCTEXT("BrokenHorizon", "WarMapFieldResupply", "FIELD RESUPPLY     {0}"),
                    FText::FromString(GetFieldLogisticsLabel(Sector))
                ).ToString(),
            SmallFont,
            GetFieldLogisticsColor(Sector)
        );
        TextY += 30.0f;
        DrawLabel(
            OutDrawElements,
            BaseLayer + 6,
            AllottedGeometry,
            FVector2D(TextX, TextY),
                FText::Format(
                    NSLOCTEXT("BrokenHorizon", "WarMapReinforcements", "REINFORCEMENTS     {0} / TURN"),
                    FText::AsNumber(
                        IsValid(WarSubsystem)
                            ? WarSubsystem->GetSectorReinforcementPerTurn(Sector.SectorID)
                            : 0.0f
                    )
                ).ToString(),
            SmallFont,
            Muted
        );
        TextY += 48.0f;
        DrawLabel(
            OutDrawElements,
            BaseLayer + 6,
            AllottedGeometry,
            FVector2D(TextX, TextY),
                FText::Format(
                    NSLOCTEXT("BrokenHorizon", "WarMapLinkedSectors", "LINKED // {0}"),
                    FText::FromString(JoinSectorIDs(Sector.ConnectedSectorIDs))
                ).ToString(),
            SmallFont,
            Muted
        );
        TextY += 30.0f;
        DrawLabel(
            OutDrawElements,
            BaseLayer + 6,
            AllottedGeometry,
            FVector2D(TextX, TextY),
            ContactAndRouteLine,
            SmallFont,
            RouteThreatColor
        );

        if (!StrategicRole.IsEmpty())
        {
            TextY += 30.0f;
            DrawLabel(
                OutDrawElements,
                BaseLayer + 6,
                AllottedGeometry,
                FVector2D(TextX, TextY),
                StrategicRole,
                SmallFont,
                StrategicRoleColor
            );
        }
    }

    FString FooterText;
    if (!IsValid(WarSubsystem))
    {
        FooterText = NSLOCTEXT("BrokenHorizon", "WarMapFooterOffline",
            "[M] CLOSE STRATEGIC MAP     [ESC] RETURN TO FIELD").ToString();
    }
    else if (!WarSubsystem->HasCommittedOperation())
    {
        FooterText = WarSubsystem->CanIssueStrategicCommands()
            ? WarSubsystem->GetCampaignDifficulty().Preset ==
                    EBHCampaignDifficultyPreset::Custom
                ? NSLOCTEXT("BrokenHorizon", "WarMapFooterCustom",
                    "[F6] PRESET  [F8/LB] AXIS  "
                    "[-/= OR DPAD] ADJUST  [F7] PLAN  [M] CLOSE").ToString()
                : NSLOCTEXT("BrokenHorizon", "WarMapFooterPlanning",
                    "[F6] DIFFICULTY     [F7] TACTICAL OPTION     "
                    "[M] CLOSE MAP     [ESC] RETURN TO FIELD").ToString()
            : NSLOCTEXT("BrokenHorizon", "WarMapFooterHostControls",
                "[F6/F7/F8] HOST CONTROLS     "
                "[M] CLOSE MAP     [ESC] RETURN TO FIELD").ToString();
    }
    else
    {
        FooterText = NSLOCTEXT("BrokenHorizon", "WarMapFooterOperationLocked",
            "[F6/F7] LOCKED DURING OPERATION     "
            "[M] CLOSE STRATEGIC MAP     [ESC] RETURN TO FIELD").ToString();
    }
    FString DeploymentPreviewText;
    FString DeploymentReadinessText;
    FString DeploymentTravelText;
    FString DeploymentLoadoutText;
    FLinearColor DeploymentReadinessColor = Muted;
    FLinearColor DeploymentTravelColor = Muted;
    FLinearColor DeploymentLoadoutColor = Muted;

    if (bDeploymentMode)
    {
        if (!IsValid(WarSubsystem))
        {
            FooterText = NSLOCTEXT("BrokenHorizon", "WarMapDeploymentCommandOffline",
                "DEPLOYMENT BLOCKED // COMMAND LINK OFFLINE").ToString();
        }
        else if (!bHasSelectedOperation)
        {
            FooterText = NSLOCTEXT("BrokenHorizon", "WarMapDeploymentNoFront",
                "DEPLOYMENT BLOCKED // NO VIABLE FRONT").ToString();
        }
        else
        {
            DeploymentPreviewText =
                BuildDeploymentForcePreviewText(
                    *WarSubsystem,
                    SelectedSectorID,
                    SelectedOperationType
                );
            DeploymentReadinessText =
                BuildDeploymentReadinessText(
                    *WarSubsystem,
                    SelectedSectorID,
                    SelectedOperationType
                );
            const FBHDeploymentTravelPreview TravelPreview =
                BuildDeploymentTravelPreview(
                    this,
                    SelectedSectorID,
                    SelectedOperationType
                );
            DeploymentTravelText = TravelPreview.Text;
            DeploymentTravelColor = TravelPreview.Color;
            const FBHDeploymentLoadoutPreview LoadoutPreview =
                BuildDeploymentLoadoutPreview(
                    this,
                    SelectedOperationType
                );
            DeploymentLoadoutText = LoadoutPreview.Text;
            DeploymentLoadoutColor = LoadoutPreview.Color;
            const bool bCanFundDeployment =
                WarSubsystem->CanFundOperation(
                    SelectedSectorID,
                    SelectedOperationType
                );
            const bool bManpowerRisk =
                HasDeploymentManpowerRisk(
                    *WarSubsystem,
                    SelectedSectorID,
                    SelectedOperationType
                );
            DeploymentReadinessColor =
                !bCanFundDeployment
                    ? FLinearColor(
                        0.94f,
                        0.30f,
                        0.18f,
                        1.0f
                    )
                    : bManpowerRisk
                    ? FLinearColor(
                        0.96f,
                        0.66f,
                        0.18f,
                        1.0f
                    )
                    : FLinearColor(
                        0.32f,
                        0.84f,
                        0.51f,
                        1.0f
                    );
            const float OperationCost =
                WarSubsystem->GetOperationSupplyCost(
                    SelectedSectorID,
                    SelectedOperationType
                );
            const FName SourceID =
                WarSubsystem->GetOperationSupplySource(
                    SelectedSectorID,
                    SelectedOperationType
                );

            if (WarSubsystem->CanFundOperation(
                    SelectedSectorID,
                    SelectedOperationType
                ))
            {
                const FBHWarSectorState Source =
                    WarSubsystem->GetSectorState(SourceID);
                const TArray<FName> SupplyRoute =
                    WarSubsystem
                        ->GetOperationSupplyRoute(
                            SelectedSectorID,
                            SelectedOperationType
                        );
                FooterText = FString::Printf(
                    TEXT(
                        "[A/D] SELECT OPERATION // "
                        "[ENTER] DEPLOY %s AT %s "
                        "// %s %.0f SUPPLY "
                        "// STAGING %s // ROUTE %d HOPS"
                    ),
                    SelectedOperationType ==
                            EBHWarPriorityType::Defend
                        ? TEXT("DEFENSE")
                        : SelectedOperationType ==
                                EBHWarPriorityType::Raid
                            ? TEXT("RAID")
                            : SelectedOperationType ==
                                    EBHWarPriorityType::Resupply
                                ? TEXT("RESUPPLY")
                            : SelectedOperationType ==
                                    EBHWarPriorityType::EscortRescue
                                ? TEXT("ESCORT")
                            : SelectedOperationType ==
                                    EBHWarPriorityType::Rescue
                                ? TEXT("RESCUE")
                            : SelectedOperationType ==
                                    EBHWarPriorityType::Recon
                                ? TEXT("RECON")
                            : TEXT("ASSAULT"),
                    *WarSubsystem
                        ->GetSectorState(SelectedSectorID)
                        .DisplayName.ToString().ToUpper(),
                    SelectedOperationType ==
                            EBHWarPriorityType::Resupply
                        ? TEXT("CARGO")
                        : TEXT("COST"),
                    OperationCost,
                    *Source.DisplayName.ToString(),
                    FMath::Max(0, SupplyRoute.Num() - 1)
                );

                const FName MobilizationSectorID =
                    BHWarOperationRules::GetMobilizationSectorID(
                        WarSubsystem,
                        SelectedSectorID,
                        SelectedOperationType
                    );

                if (!MobilizationSectorID.IsNone())
                {
                    const int32 MilitiaCount =
                        WarSubsystem
                            ->GetSectorMilitiaMobilizationCount(
                                MobilizationSectorID
                            );
                    const float MilitiaCost =
                        WarSubsystem
                            ->GetSectorMilitiaMobilizationSupplyCost(
                                MobilizationSectorID
                            );

                    if (WarSubsystem
                        ->CanMobilizeSectorMilitia(
                            MobilizationSectorID
                        ))
                    {
                        FooterText += FString::Printf(
                            TEXT(
                                " // [R] RALLY %s +%d MILITIA (%.0f SUPPLY)"
                            ),
                            SelectedOperationType ==
                                EBHWarPriorityType::Defend
                                    ? TEXT("LOCAL")
                                    : TEXT("STAGING"),
                            MilitiaCount,
                            MilitiaCost
                        );
                    }
                    else
                    {
                        FooterText +=
                            TEXT(" // MILITIA UNAVAILABLE");
                    }

                    const FName RedeploymentSourceID =
                        WarSubsystem
                            ->GetSectorGarrisonRedeploymentSource(
                                MobilizationSectorID
                            );

                    if (!RedeploymentSourceID.IsNone() &&
                        WarSubsystem->CanRedeploySectorGarrison(
                            MobilizationSectorID
                        ))
                    {
                        const int32 RedeploymentCount =
                            WarSubsystem
                                ->GetSectorGarrisonRedeploymentCount(
                                    MobilizationSectorID
                                );
                        const float RedeploymentCost =
                            WarSubsystem
                                ->GetSectorGarrisonRedeploymentSupplyCost(
                                    MobilizationSectorID
                                );
                        const int32 RedeploymentTurns =
                            WarSubsystem
                                ->GetSectorGarrisonRedeploymentTurns(
                                    MobilizationSectorID
                                );
                        const FString RedeploymentSourceName =
                            WarSubsystem
                                ->GetSectorState(
                                    RedeploymentSourceID
                                )
                                .DisplayName.ToString()
                                .ToUpper()
                                .Left(18);
                        FooterText += FString::Printf(
                            TEXT(
                                " // [T] MOVE %d FROM %s "
                                "(%.0f SUP // %dT)"
                            ),
                            RedeploymentCount,
                            *RedeploymentSourceName,
                            RedeploymentCost,
                            RedeploymentTurns
                        );
                    }
                }

                if (WarSubsystem->CanDeliverCivilianAid(
                        SelectedSectorID,
                        SelectedOperationType))
                {
                    FooterText += TEXT(" // [H] AID NETWORK");
                }
            }
            else if (SourceID.IsNone())
            {
                FooterText = NSLOCTEXT(
                    "BrokenHorizon", "WarMapDeploymentNoStagingRoute",
                    "DEPLOYMENT BLOCKED // NO FRIENDLY STAGING ROUTE").ToString();
            }
            else
            {
                const FBHWarSectorState Source =
                    WarSubsystem->GetSectorState(SourceID);
                FooterText = FString::Printf(
                    TEXT(
                        "DEPLOYMENT BLOCKED // %s SUPPLY %.0f / %.0f"
                    ),
                    *Source.DisplayName.ToString(),
                    Source.Supply,
                    OperationCost
                );
            }
        }
    }
    else if (IsValid(WarSubsystem) &&
        WarSubsystem->HasCommittedOperation())
    {
        FooterText = IsWithdrawalConfirmationActive()
            ? NSLOCTEXT("BrokenHorizon", "WarMapConfirmWithdrawal",
                "[BACKSPACE] CONFIRM WITHDRAWAL // "
                "OPERATION WILL BE LOST").ToString()
            : NSLOCTEXT("BrokenHorizon", "WarMapWithdrawOperation",
                "[BACKSPACE] WITHDRAW OPERATION     "
                "[M / ESC] RETURN TO FIELD").ToString();
    }

    if (IsStrategicActionFeedbackActive())
    {
        FooterText = StrategicActionFeedback;
    }

    const UGameInstance* GameInstance = GetGameInstance();
    const UBHUserSettingsSubsystem* UserSettings = GameInstance
        ? GameInstance->GetSubsystem<UBHUserSettingsSubsystem>()
        : nullptr;
    const FString WarMapPrompt = IsValid(UserSettings)
        ? UserSettings->GetInputBindingPrompt(FName(TEXT("WarMap")))
        : FString();
    if (!WarMapPrompt.IsEmpty())
    {
        FooterText.ReplaceInline(
            TEXT("[M / ESC]"),
            *FString::Printf(TEXT("[%s / ESC]"), *WarMapPrompt),
            ESearchCase::CaseSensitive
        );
        FooterText.ReplaceInline(
            TEXT("[M]"),
            *FString::Printf(TEXT("[%s]"), *WarMapPrompt),
            ESearchCase::CaseSensitive
        );
    }

    if (!DeploymentPreviewText.IsEmpty())
    {
        DrawLabel(
            OutDrawElements,
            BaseLayer + 6,
            AllottedGeometry,
            FVector2D(50.0f, ViewSize.Y - 100.0f),
            DeploymentPreviewText,
            SmallFont,
            Gold
        );
    }

    if (!DeploymentTravelText.IsEmpty())
    {
        DrawLabel(
            OutDrawElements,
            BaseLayer + 6,
            AllottedGeometry,
            FVector2D(50.0f, ViewSize.Y - 124.0f),
            DeploymentTravelText,
            SmallFont,
            DeploymentTravelColor
        );
    }

    if (!DeploymentLoadoutText.IsEmpty())
    {
        DrawLabel(
            OutDrawElements,
            BaseLayer + 6,
            AllottedGeometry,
            FVector2D(50.0f, ViewSize.Y - 148.0f),
            DeploymentLoadoutText,
            SmallFont,
            DeploymentLoadoutColor
        );
    }

    if (!DeploymentReadinessText.IsEmpty())
    {
        DrawLabel(
            OutDrawElements,
            BaseLayer + 6,
            AllottedGeometry,
            FVector2D(50.0f, ViewSize.Y - 76.0f),
            DeploymentReadinessText,
            SmallFont,
            DeploymentReadinessColor
        );
    }

    DrawLabel(
        OutDrawElements,
        BaseLayer + 6,
        AllottedGeometry,
        FVector2D(50.0f, ViewSize.Y - 48.0f),
        FooterText,
        SmallFont,
        Muted
    );

    return BaseLayer + 6;
}

FBHCampaignDifficultyProfile
UBHWarMapWidget::AdjustCustomDifficultyAxis(
    const FBHCampaignDifficultyProfile& CurrentProfile,
    int32 AxisIndex,
    float Delta
)
{
    FBHCampaignDifficultyProfile Result = CurrentProfile;
    switch (FMath::Clamp(AxisIndex, 0, 5))
    {
    case 0:
        Result.IncomingDamageMultiplier += Delta;
        break;
    case 1:
        Result.EnemyPerceptionMultiplier += Delta;
        break;
    case 2:
        Result.EnemyCoordinationMultiplier += Delta;
        break;
    case 3:
        Result.MedicalPressureMultiplier += Delta;
        break;
    case 4:
        Result.StrategicPressureMultiplier += Delta;
        break;
    default:
        Result.CheckpointIntervalMultiplier += Delta;
        break;
    }
    Result = BHDifficulty::Sanitize(Result);
    Result.Preset = EBHCampaignDifficultyPreset::Custom;
    return Result;
}

FString UBHWarMapWidget::GetCustomDifficultyAxisLabel(
    int32 AxisIndex
)
{
    switch (FMath::Clamp(AxisIndex, 0, 5))
    {
    case 0:
        return TEXT("DAMAGE");
    case 1:
        return TEXT("PERCEPTION");
    case 2:
        return TEXT("COORDINATION");
    case 3:
        return TEXT("MEDICAL");
    case 4:
        return TEXT("STRATEGIC");
    default:
        return TEXT("CHECKPOINT");
    }
}

float UBHWarMapWidget::GetCustomDifficultyAxisValue(
    const FBHCampaignDifficultyProfile& Profile,
    int32 AxisIndex
)
{
    switch (FMath::Clamp(AxisIndex, 0, 5))
    {
    case 0:
        return Profile.IncomingDamageMultiplier;
    case 1:
        return Profile.EnemyPerceptionMultiplier;
    case 2:
        return Profile.EnemyCoordinationMultiplier;
    case 3:
        return Profile.MedicalPressureMultiplier;
    case 4:
        return Profile.StrategicPressureMultiplier;
    default:
        return Profile.CheckpointIntervalMultiplier;
    }
}

void UBHWarMapWidget::SelectNextCustomDifficultyAxis()
{
    if (!IsValid(WarSubsystem) ||
        WarSubsystem->GetCampaignDifficulty().Preset !=
            EBHCampaignDifficultyPreset::Custom)
    {
        SetStrategicActionFeedback(
            TEXT("CUSTOM AXES LOCKED // SELECT CUSTOM WITH F6")
        );
        return;
    }

    SelectedCustomDifficultyAxis =
        (SelectedCustomDifficultyAxis + 1) % 6;
    SetStrategicActionFeedback(
        FString::Printf(
            TEXT("CUSTOM AXIS // %s %.2fx"),
            *GetCustomDifficultyAxisLabel(
                SelectedCustomDifficultyAxis
            ),
            GetCustomDifficultyAxisValue(
                WarSubsystem->GetCampaignDifficulty(),
                SelectedCustomDifficultyAxis
            )
        )
    );
}

bool UBHWarMapWidget::TryAdjustSelectedCustomDifficultyAxis(
    float Delta
)
{
    const FString Rejection = IsValid(WarSubsystem)
        ? BuildStrategicControlRejection(
            WarSubsystem->CanIssueStrategicCommands(),
            WarSubsystem->HasCommittedOperation()
        )
        : TEXT("COMMAND LINK OFFLINE");
    if (!Rejection.IsEmpty())
    {
        SetStrategicActionFeedback(Rejection);
        return false;
    }
    if (WarSubsystem->GetCampaignDifficulty().Preset !=
        EBHCampaignDifficultyPreset::Custom)
    {
        SetStrategicActionFeedback(
            TEXT("CUSTOM AXES LOCKED // SELECT CUSTOM WITH F6")
        );
        return false;
    }

    const FBHCampaignDifficultyProfile Updated =
        AdjustCustomDifficultyAxis(
            WarSubsystem->GetCampaignDifficulty(),
            SelectedCustomDifficultyAxis,
            Delta
        );
    const bool bChanged =
        WarSubsystem->SetCustomCampaignDifficulty(Updated);
    SetStrategicActionFeedback(
        bChanged
            ? FString::Printf(
                TEXT("CUSTOM DIFFICULTY // %s %.2fx"),
                *GetCustomDifficultyAxisLabel(
                    SelectedCustomDifficultyAxis
                ),
                GetCustomDifficultyAxisValue(
                    Updated,
                    SelectedCustomDifficultyAxis
                )
            )
            : TEXT("COMMAND UPDATE FAILED // DIFFICULTY UNCHANGED")
    );
    return bChanged;
}

void UBHWarMapWidget::SetDeploymentMode(bool bEnabled)
{
    const bool bRenderedReview = IsDeploymentRenderedReview();
    bDeploymentMode =
        bEnabled &&
        IsValid(WarSubsystem) &&
        (!WarSubsystem->IsCampaignResolved() || bRenderedReview);
    bDeploymentInputArmed = false;
    DeploymentRiskConfirmationExpiresAt = -1.0f;
    WithdrawalConfirmationExpiresAt = -1.0f;
    RefreshOperationChoices();
    Invalidate(EInvalidateWidgetReason::Paint);
}

void UBHWarMapWidget::RefreshOperationChoices()
{
    const FName PreviousSectorID = HasSelectedOperation()
        ? OperationSectorChoices[SelectedOperationIndex]
        : NAME_None;
    const EBHWarPriorityType PreviousOperationType =
        HasSelectedOperation()
            ? OperationTypeChoices[SelectedOperationIndex]
            : EBHWarPriorityType::None;

    OperationSectorChoices.Reset();
    OperationTypeChoices.Reset();
    SelectedOperationIndex = INDEX_NONE;

    const bool bRenderedReview = IsDeploymentRenderedReview();
    if (!bDeploymentMode ||
        !IsValid(WarSubsystem) ||
        (WarSubsystem->IsCampaignResolved() && !bRenderedReview))
    {
        return;
    }

    const auto AddChoice =
        [this, bRenderedReview](FName SectorID, EBHWarPriorityType OperationType)
        {
            if (OperationType == EBHWarPriorityType::Rescue)
            {
                const ABHCharacter* Character =
                    Cast<ABHCharacter>(GetOwningPlayerPawn());

                if (!IsValid(Character) ||
                    !Character->HasFieldSquadRescueTarget())
                {
                    return;
                }
            }

            if (!bRenderedReview &&
                !WarSubsystem->IsViableOperation(
                    SectorID,
                    OperationType
                ))
            {
                return;
            }

            for (int32 Index = 0;
                Index < OperationSectorChoices.Num();
                ++Index)
            {
                if (OperationSectorChoices[Index] == SectorID &&
                    OperationTypeChoices[Index] == OperationType)
                {
                    return;
                }
            }

            OperationSectorChoices.Add(SectorID);
            OperationTypeChoices.Add(OperationType);
        };

    AddChoice(
        WarSubsystem->GetPrioritySectorID(),
        WarSubsystem->GetPriorityType()
    );

    for (const FBHWarSectorState& Sector : SectorStates)
    {
        if (Sector.Owner == EBHWarFaction::Friendly)
        {
            AddChoice(
                Sector.SectorID,
                EBHWarPriorityType::Defend
            );
            AddChoice(
                Sector.SectorID,
                EBHWarPriorityType::Resupply
            );
            AddChoice(
                Sector.SectorID,
                EBHWarPriorityType::EscortRescue
            );
            AddChoice(
                Sector.SectorID,
                EBHWarPriorityType::Rescue
            );
        }
        else
        {
            AddChoice(
                Sector.SectorID,
                EBHWarPriorityType::Recon
            );
            AddChoice(
                Sector.SectorID,
                EBHWarPriorityType::Attack
            );

            if (Sector.Owner == EBHWarFaction::Enemy)
            {
                AddChoice(
                    Sector.SectorID,
                    EBHWarPriorityType::Raid
                );
            }
        }
    }

    if (OperationSectorChoices.IsEmpty())
    {
        return;
    }

    SelectedOperationIndex = 0;

    for (int32 Index = 0;
        Index < OperationSectorChoices.Num();
        ++Index)
    {
        if (OperationSectorChoices[Index] == PreviousSectorID &&
            OperationTypeChoices[Index] == PreviousOperationType)
        {
            SelectedOperationIndex = Index;
            break;
        }
    }
}

void UBHWarMapWidget::SelectOperationOffset(int32 Offset)
{
    if (OperationSectorChoices.IsEmpty() || Offset == 0)
    {
        return;
    }

    SelectedOperationIndex =
        (SelectedOperationIndex + Offset +
         OperationSectorChoices.Num()) %
        OperationSectorChoices.Num();
    DeploymentRiskConfirmationExpiresAt = -1.0f;
    Invalidate(EInvalidateWidgetReason::Paint);
}

bool UBHWarMapWidget::HasSelectedOperation() const
{
    return OperationSectorChoices.IsValidIndex(
            SelectedOperationIndex
        ) &&
        OperationTypeChoices.IsValidIndex(
            SelectedOperationIndex
        );
}

bool UBHWarMapWidget::IsWithdrawalConfirmationActive() const
{
    const UWorld* World = GetWorld();

    return IsValid(World) &&
        WithdrawalConfirmationExpiresAt >=
            World->GetTimeSeconds();
}

bool UBHWarMapWidget::IsDeploymentRiskConfirmationActive() const
{
    const UWorld* World = GetWorld();

    return IsValid(World) &&
        DeploymentRiskConfirmationExpiresAt >=
            World->GetTimeSeconds();
}

bool UBHWarMapWidget::IsStrategicActionFeedbackActive() const
{
    const UWorld* World = GetWorld();

    return IsValid(World) &&
        !StrategicActionFeedback.IsEmpty() &&
        StrategicActionFeedbackExpiresAt >=
            World->GetTimeSeconds();
}

void UBHWarMapWidget::RefreshWarMap()
{
    if (!IsValid(WarSubsystem))
    {
        SectorStates.Reset();
        RecentWarEvents.Reset();
        PriorityText = FText::FromString(
            TEXT("War command link unavailable")
        );
        TurnNumber = 0;
        FriendlyControlPercentage = 0.0f;
    }
    else
    {
        SectorStates = WarSubsystem->GetSectorStates();
        RecentWarEvents = WarSubsystem->GetRecentWarEvents();
        PriorityText = WarSubsystem->GetPriorityText();
        TurnNumber = WarSubsystem->GetTurnNumber();
        FriendlyControlPercentage =
            WarSubsystem->GetFriendlyControlPercentage();
    }

    RefreshOperationChoices();
    Invalidate(EInvalidateWidgetReason::Paint);
}

void UBHWarMapWidget::HandleWarStateChanged(
    int32 NewTurnNumber,
    FName NewPrioritySectorID,
    EBHWarPriorityType NewPriorityType
)
{
    (void)NewTurnNumber;
    (void)NewPrioritySectorID;
    (void)NewPriorityType;
    RefreshWarMap();
}

void UBHWarMapWidget::BindWarSubsystem()
{
    if (!IsValid(WarSubsystem))
    {
        return;
    }

    WarSubsystem->OnWarStateChanged.RemoveDynamic(
        this,
        &UBHWarMapWidget::HandleWarStateChanged
    );
    WarSubsystem->OnWarStateChanged.AddDynamic(
        this,
        &UBHWarMapWidget::HandleWarStateChanged
    );
}

void UBHWarMapWidget::UnbindWarSubsystem()
{
    if (IsValid(WarSubsystem))
    {
        WarSubsystem->OnWarStateChanged.RemoveDynamic(
            this,
            &UBHWarMapWidget::HandleWarStateChanged
        );
    }
}
