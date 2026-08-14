#include "BHOperationSiteMarker.h"

#include "Components/ArrowComponent.h"
#include "Components/SceneComponent.h"
#include "Components/TextRenderComponent.h"

#include "Engine/Engine.h"

ABHOperationSiteMarker::ABHOperationSiteMarker()
{
    PrimaryActorTick.bCanEverTick = false;
    SetActorEnableCollision(false);
#if WITH_EDITOR
    SetIsSpatiallyLoaded(false);
#endif

    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
    SetRootComponent(SceneRoot);

    ApproachDirection = CreateDefaultSubobject<UArrowComponent>(TEXT("ApproachDirection"));
    ApproachDirection->SetupAttachment(SceneRoot);
    ApproachDirection->SetArrowColor(FLinearColor(0.15f, 0.85f, 0.75f));
    ApproachDirection->SetArrowSize(2.5f);

    SiteLabel = CreateDefaultSubobject<UTextRenderComponent>(TEXT("SiteLabel"));
    SiteLabel->SetupAttachment(SceneRoot);
    SiteLabel->SetRelativeLocation(FVector(0.0f, 0.0f, 260.0f));
    SiteLabel->SetRelativeRotation(FRotator(0.0f, 90.0f, 0.0f));
    SiteLabel->SetHorizontalAlignment(EHTA_Center);
    SiteLabel->SetWorldSize(36.0f);
    SiteLabel->SetTextRenderColor(FColor(92, 220, 200, 255));
    SiteLabel->SetText(NSLOCTEXT("BrokenHorizon", "UnconfiguredOperationSite", "OPERATION SITE"));
    SiteLabel->SetHiddenInGame(false);
}

void ABHOperationSiteMarker::BeginPlay()
{
    Super::BeginPlay();
    UE_LOG(LogTemp, Display,
        TEXT("BH_OPERATION_SITE_MARKER id=%s family=%s variation=%s purpose=%s"),
        *PersistenceID.ToString(), *OperationFamily.ToString(),
        *OperationVariation.ToString(), *SitePurpose.ToString());
}

void ABHOperationSiteMarker::ConfigureOperationSite(
    FName NewPersistenceID,
    FName NewFamily,
    FName NewVariation,
    FName NewSitePurpose,
    const FText& NewApproachLabel
)
{
    PersistenceID = NewPersistenceID;
    OperationFamily = NewFamily;
    OperationVariation = NewVariation;
    SitePurpose = NewSitePurpose;
    ApproachLabel = NewApproachLabel;
    if (IsValid(SiteLabel))
    {
        SiteLabel->SetText(FText::Format(
            NSLOCTEXT("BrokenHorizon", "OperationSiteLabel", "{0}\n{1}"),
            FText::FromName(OperationFamily), ApproachLabel));
    }
}

FName ABHOperationSiteMarker::GetOperationSitePersistenceID() const { return PersistenceID; }
FName ABHOperationSiteMarker::GetOperationFamily() const { return OperationFamily; }
FName ABHOperationSiteMarker::GetOperationVariation() const { return OperationVariation; }
