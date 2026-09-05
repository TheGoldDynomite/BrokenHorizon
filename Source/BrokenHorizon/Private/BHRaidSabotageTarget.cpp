#include "BHRaidSabotageTarget.h"

#include "BHCharacter.h"
#include "BHEnemySoldier.h"
#include "BHOpenWorldOperationDirector.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

ABHRaidSabotageTarget::ABHRaidSabotageTarget()
{
    PrimaryActorTick.bCanEverTick = false;

    SceneRoot = CreateDefaultSubobject<USceneComponent>(
        TEXT("SceneRoot")
    );
    SetRootComponent(SceneRoot);

    CacheBaseMesh = CreateDefaultSubobject<UStaticMeshComponent>(
        TEXT("CacheBaseMesh")
    );
    CacheBaseMesh->SetupAttachment(SceneRoot);
    CacheBaseMesh->SetRelativeLocation(
        FVector(0.0f, 0.0f, 45.0f)
    );
    CacheBaseMesh->SetRelativeScale3D(
        FVector(1.8f, 1.3f, 0.45f)
    );
    CacheBaseMesh->SetCollisionEnabled(
        ECollisionEnabled::QueryAndPhysics
    );
    CacheBaseMesh->SetCollisionResponseToAllChannels(ECR_Block);

    CacheCrateMesh = CreateDefaultSubobject<UStaticMeshComponent>(
        TEXT("CacheCrateMesh")
    );
    CacheCrateMesh->SetupAttachment(SceneRoot);
    CacheCrateMesh->SetRelativeLocation(
        FVector(0.0f, 0.0f, 135.0f)
    );
    CacheCrateMesh->SetRelativeScale3D(
        FVector(1.15f, 0.9f, 0.55f)
    );
    CacheCrateMesh->SetCollisionEnabled(
        ECollisionEnabled::QueryAndPhysics
    );
    CacheCrateMesh->SetCollisionResponseToAllChannels(ECR_Block);

    static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeAsset(
        TEXT("/Engine/BasicShapes/Cube.Cube")
    );

    if (CubeAsset.Succeeded())
    {
        CacheBaseMesh->SetStaticMesh(CubeAsset.Object);
        CacheCrateMesh->SetStaticMesh(CubeAsset.Object);
    }

    TargetLabel = CreateDefaultSubobject<UTextRenderComponent>(
        TEXT("TargetLabel")
    );
    TargetLabel->SetupAttachment(SceneRoot);
    TargetLabel->SetRelativeLocation(
        FVector(0.0f, 0.0f, 245.0f)
    );
    TargetLabel->SetRelativeRotation(
        FRotator(0.0f, 90.0f, 0.0f)
    );
    TargetLabel->SetHorizontalAlignment(EHTA_Center);
    TargetLabel->SetWorldSize(34.0f);
    TargetLabel->SetTextRenderColor(
        FColor(255, 176, 40)
    );
    TargetLabel->SetText(
        NSLOCTEXT(
            "BrokenHorizon",
            "RaidSabotageTargetWorldLabel",
            "ENEMY LOGISTICS CACHE"
        )
    );
}

void ABHRaidSabotageTarget::ConfigureTarget(
    ABHOpenWorldOperationDirector* InOperationDirector,
    FName InSectorID
)
{
    OperationDirector = InOperationDirector;
    SectorID = InSectorID;
}

bool ABHRaidSabotageTarget::IsSabotaged() const
{
    return bSabotaged;
}

bool ABHRaidSabotageTarget::CanAcceptFieldSquadSabotage() const
{
    return HasAuthority() &&
        !bSabotaged &&
        IsValid(OperationDirector) &&
        OperationDirector->IsOperationActivated();
}

bool ABHRaidSabotageTarget::SabotageByFieldOperative(
    ABHEnemySoldier* Operative,
    ABHCharacter* Commander
)
{
    constexpr float OperativeSabotageRadius = 325.0f;

    if (!CanAcceptFieldSquadSabotage() ||
        !IsValid(Operative) ||
        !IsValid(Commander) ||
        Operative->IsDead() ||
        Operative->IsIncapacitated() ||
        Operative->GetCombatFaction() !=
            EBHCombatFaction::Friendly ||
        FVector::DistSquared2D(
            Operative->GetActorLocation(),
            GetActorLocation()
        ) > FMath::Square(OperativeSabotageRadius))
    {
        return false;
    }

    return CompleteSabotage(Commander, Operative);
}

bool ABHRaidSabotageTarget::SabotageByEngineeringCharge(
    ABHCharacter* Commander,
    const AActor* ChargeActor
)
{
    return CompleteSabotage(Commander, ChargeActor);
}

void ABHRaidSabotageTarget::Interact_Implementation(
    AActor* InteractingActor
)
{
    ABHCharacter* Character =
        Cast<ABHCharacter>(InteractingActor);

    CompleteSabotage(Character, Character);
}

bool ABHRaidSabotageTarget::CompleteSabotage(
    ABHCharacter* Commander,
    const AActor* SabotageActor
)
{
    if (!HasAuthority() ||
        bSabotaged ||
        !IsValid(Commander) ||
        !IsValid(SabotageActor) ||
        !IsValid(OperationDirector))
    {
        return false;
    }

    bSabotaged = true;
    SetActorEnableCollision(false);

    if (IsValid(CacheBaseMesh))
    {
        CacheBaseMesh->SetCollisionEnabled(
            ECollisionEnabled::NoCollision
        );
    }

    if (IsValid(CacheCrateMesh))
    {
        CacheCrateMesh->SetCollisionEnabled(
            ECollisionEnabled::NoCollision
        );
    }

    if (IsValid(TargetLabel))
    {
        TargetLabel->SetTextRenderColor(FColor(80, 255, 120));
        TargetLabel->SetText(
            NSLOCTEXT(
                "BrokenHorizon",
                "RaidSabotageTargetArmedWorldLabel",
                "DEMOLITION CHARGES ARMED"
            )
        );
    }

    Commander->ShowStatusNotification(
        FText::Format(
            NSLOCTEXT(
                "BrokenHorizon",
                "RaidSabotageTargetArmed",
                "LOGISTICS TARGET SABOTAGED // {0}\n\n"
                "Demolition charges armed. Break contact."
            ),
            FText::FromName(SectorID)
        )
    );

    UE_LOG(
        LogTemp,
        Display,
        TEXT(
            "BH_RAID_TARGET_SABOTAGED sector=%s player=%s"
        ),
        *SectorID.ToString(),
        *GetNameSafe(Commander)
    );

    UE_LOG(
        LogTemp,
        Display,
        TEXT("BH_RAID_SABOTAGE_ACTOR commander=%s actor=%s"),
        *GetNameSafe(Commander),
        *GetNameSafe(SabotageActor)
    );

    OperationDirector->HandleRaidTargetSabotaged(this);
    return true;
}

FText ABHRaidSabotageTarget::BuildInteractionText(bool bInSabotaged)
{
    return bInSabotaged
        ? NSLOCTEXT(
            "BrokenHorizon",
            "RaidSabotageTargetAlreadyArmedPrompt",
            "Demolition Charges Armed"
        )
        : NSLOCTEXT(
            "BrokenHorizon",
            "RaidSabotageTargetPrompt",
            "Press [F] to PLANT DEMOLITION CHARGES"
        );
}

FText
ABHRaidSabotageTarget::GetInteractionText_Implementation() const
{
    return BuildInteractionText(bSabotaged);
}
