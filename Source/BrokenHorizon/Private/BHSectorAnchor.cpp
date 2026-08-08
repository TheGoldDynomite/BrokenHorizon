#include "BHSectorAnchor.h"

#include "BHWarSubsystem.h"
#include "Components/ArrowComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SphereComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/GameInstance.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"

ABHSectorAnchor::ABHSectorAnchor()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickInterval = 0.1f;
    SetActorEnableCollision(true);

#if WITH_EDITOR
    SetIsSpatiallyLoaded(false);
#endif

    SceneRoot = CreateDefaultSubobject<USceneComponent>(
        TEXT("SceneRoot")
    );
    SetRootComponent(SceneRoot);

    OperationDirection =
        CreateDefaultSubobject<UArrowComponent>(
            TEXT("OperationDirection")
        );
    OperationDirection->SetupAttachment(SceneRoot);
    OperationDirection->SetArrowColor(
        FLinearColor(0.95f, 0.65f, 0.12f)
    );
    OperationDirection->SetArrowSize(3.0f);
    OperationDirection->SetHiddenInGame(true);

    SquadContextTarget =
        CreateDefaultSubobject<USphereComponent>(
            TEXT("SquadContextTarget")
        );
    SquadContextTarget->SetupAttachment(OperationDirection);
    SquadContextTarget->SetSphereRadius(220.0f);
    SquadContextTarget->SetCollisionEnabled(
        ECollisionEnabled::QueryOnly
    );
    SquadContextTarget->SetCollisionResponseToAllChannels(ECR_Ignore);
    SquadContextTarget->SetCollisionResponseToChannel(
        ECC_Visibility,
        ECR_Block
    );
    SquadContextTarget->SetHiddenInGame(true);

    SectorStatusLabel =
        CreateDefaultSubobject<UTextRenderComponent>(
            TEXT("SectorStatusLabel")
        );
    SectorStatusLabel->SetupAttachment(SceneRoot);
    SectorStatusLabel->SetRelativeLocation(
        FVector(0.0f, 0.0f, 420.0f)
    );
    SectorStatusLabel->SetRelativeRotation(
        FRotator(0.0f, 90.0f, 0.0f)
    );
    SectorStatusLabel->SetHorizontalAlignment(EHTA_Center);
    SectorStatusLabel->SetWorldSize(48.0f);
    SectorStatusLabel->SetTextRenderColor(
        FColor(255, 186, 44, 255)
    );
    SectorStatusLabel->SetText(
        NSLOCTEXT(
            "BrokenHorizon",
            "UninitializedSectorStatus",
            "SECTOR STATUS UNAVAILABLE"
        )
    );
    SectorStatusLabel->SetHiddenInGame(false);
}

void ABHSectorAnchor::BeginPlay()
{
    Super::BeginPlay();

    if (IsValid(SectorStatusLabel))
    {
        SectorStatusLabel->SetCullDistance(
            FMath::Max(1000.0f, SectorStatusCullDistance)
        );
    }

    if (UGameInstance* GameInstance = GetGameInstance())
    {
        if (UBHWarSubsystem* WarSubsystem =
            GameInstance->GetSubsystem<UBHWarSubsystem>())
        {
            WarSubsystem->OnWarStateChanged.AddDynamic(
                this,
                &ABHSectorAnchor::HandleWarStateChanged
            );
        }
    }

    RefreshWarStatus();
    FaceStatusTowardPlayer();
}

void ABHSectorAnchor::EndPlay(
    const EEndPlayReason::Type EndPlayReason
)
{
    if (UGameInstance* GameInstance = GetGameInstance())
    {
        if (UBHWarSubsystem* WarSubsystem =
            GameInstance->GetSubsystem<UBHWarSubsystem>())
        {
            WarSubsystem->OnWarStateChanged.RemoveDynamic(
                this,
                &ABHSectorAnchor::HandleWarStateChanged
            );
        }
    }

    Super::EndPlay(EndPlayReason);
}

void ABHSectorAnchor::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    FaceStatusTowardPlayer();
}

void ABHSectorAnchor::ConfigureSector(
    FName NewSectorID,
    const FText& NewDisplayName
)
{
    SectorID = NewSectorID;
    SectorDisplayName = NewDisplayName;

    if (HasActorBegunPlay())
    {
        RefreshWarStatus();
    }
}

bool ABHSectorAnchor::MatchesSector(
    FName CandidateSectorID
) const
{
    return !CandidateSectorID.IsNone() &&
        SectorID == CandidateSectorID;
}

FName ABHSectorAnchor::GetSectorID() const
{
    return SectorID;
}

FText ABHSectorAnchor::GetSectorDisplayName() const
{
    return SectorDisplayName;
}

FVector ABHSectorAnchor::GetOperationCenter() const
{
    return IsValid(OperationDirection)
        ? OperationDirection->GetComponentLocation()
        : GetActorLocation();
}

float ABHSectorAnchor::GetOperationActivationRadius() const
{
    return OperationActivationRadius;
}

FTransform ABHSectorAnchor::BuildEnemySpawnTransform(
    int32 SpawnIndex,
    int32 SpawnCount,
    int32 WaveIndex
) const
{
    const int32 SafeSpawnCount = FMath::Max(1, SpawnCount);
    const float BaseYaw = IsValid(OperationDirection)
        ? OperationDirection->GetComponentRotation().Yaw
        : GetActorRotation().Yaw;
    const float Angle =
        BaseYaw +
        ((360.0f / SafeSpawnCount) * SpawnIndex) +
        (WaveIndex * 47.0f);
    const FVector Direction(
        FMath::Cos(FMath::DegreesToRadians(Angle)),
        FMath::Sin(FMath::DegreesToRadians(Angle)),
        0.0f
    );
    const FVector Center = GetOperationCenter();
    const FVector SpawnLocation =
        Center + (Direction * EnemySpawnRadius);

    return FTransform(
        (Center - SpawnLocation).Rotation(),
        SpawnLocation
    );
}

void ABHSectorAnchor::HandleWarStateChanged(
    int32 NewTurnNumber,
    FName PrioritySectorID,
    EBHWarPriorityType PriorityType
)
{
    RefreshWarStatus();
}

void ABHSectorAnchor::RefreshWarStatus()
{
    UGameInstance* GameInstance = GetGameInstance();
    UBHWarSubsystem* WarSubsystem = GameInstance
        ? GameInstance->GetSubsystem<UBHWarSubsystem>()
        : nullptr;

    if (!IsValid(SectorStatusLabel) ||
        !IsValid(WarSubsystem) ||
        SectorID.IsNone())
    {
        if (IsValid(SectorStatusLabel))
        {
            SectorStatusLabel->SetVisibility(false);
        }

        return;
    }

    const FBHWarSectorState Sector =
        WarSubsystem->GetSectorState(SectorID);

    if (Sector.SectorID.IsNone())
    {
        SectorStatusLabel->SetVisibility(false);
        return;
    }

    const bool bPriority =
        WarSubsystem->GetPrioritySectorID() == SectorID;
    const int32 RoundedSupply =
        FMath::RoundToInt(Sector.Supply);
    const int32 RoundedFriendlyStrength =
        FMath::RoundToInt(Sector.FriendlyStrength);
    const int32 RoundedEnemyStrength =
        FMath::RoundToInt(Sector.EnemyStrength);
    const int32 RoundedIntelConfidence =
        FMath::RoundToInt(Sector.IntelConfidence);
    const TCHAR* OwnerLabel =
        Sector.Owner == EBHWarFaction::Friendly
            ? TEXT("FRIENDLY")
            : Sector.Owner == EBHWarFaction::Enemy
                ? TEXT("ENEMY")
                : TEXT("CONTESTED");
    const FText DisplayName = Sector.DisplayName.IsEmpty()
        ? (
            SectorDisplayName.IsEmpty()
                ? FText::FromName(SectorID)
                : SectorDisplayName
        )
        : Sector.DisplayName;
    const FString EnemyIntelSummary =
        WarSubsystem->GetSectorEnemyIntelSummary(
            Sector.SectorID
        ).ToString();
    const FString StatusText = FString::Printf(
        TEXT(
            "%s\n%s CONTROL%s\n"
            "SUPPLY %d // FRIENDLY FORCE %d\n"
            "F-GARRISON %d / %d\n%s"
        ),
        *DisplayName.ToString().ToUpper(),
        OwnerLabel,
        bPriority ? TEXT(" // PRIORITY") : TEXT(""),
        RoundedSupply,
        RoundedFriendlyStrength,
        Sector.FriendlyGarrison,
        Sector.GarrisonCapacity,
        *EnemyIntelSummary
    );

    SectorStatusLabel->SetText(FText::FromString(StatusText));
    SectorStatusLabel->SetTextRenderColor(
        Sector.Owner == EBHWarFaction::Friendly
            ? FColor(54, 210, 235, 255)
            : Sector.Owner == EBHWarFaction::Enemy
                ? FColor(255, 74, 44, 255)
                : FColor(255, 186, 44, 255)
    );
    SectorStatusLabel->SetVisibility(true);

    const bool bStatusChanged =
        !bHasCachedWarStatus ||
        CachedOwner != Sector.Owner ||
        CachedSupply != RoundedSupply ||
        CachedFriendlyStrength != RoundedFriendlyStrength ||
        CachedEnemyStrength != RoundedEnemyStrength ||
        CachedFriendlyGarrison != Sector.FriendlyGarrison ||
        CachedEnemyGarrison != Sector.EnemyGarrison ||
        CachedIntelConfidence != RoundedIntelConfidence ||
        bCachedPriority != bPriority;

    if (bStatusChanged)
    {
        UE_LOG(
            LogTemp,
            Display,
            TEXT(
                "BH_SECTOR_WORLD_STATUS sector=%s owner=%d "
                "supply=%d friendly=%d enemy=%d "
                "garrison_f=%d garrison_e=%d capacity=%d "
                "intel=%d priority=%d"
            ),
            *SectorID.ToString(),
            static_cast<int32>(Sector.Owner),
            RoundedSupply,
            RoundedFriendlyStrength,
            RoundedEnemyStrength,
            Sector.FriendlyGarrison,
            Sector.EnemyGarrison,
            Sector.GarrisonCapacity,
            RoundedIntelConfidence,
            bPriority ? 1 : 0
        );
    }

    bHasCachedWarStatus = true;
    CachedOwner = Sector.Owner;
    CachedSupply = RoundedSupply;
    CachedFriendlyStrength = RoundedFriendlyStrength;
    CachedEnemyStrength = RoundedEnemyStrength;
    CachedFriendlyGarrison = Sector.FriendlyGarrison;
    CachedEnemyGarrison = Sector.EnemyGarrison;
    CachedIntelConfidence = RoundedIntelConfidence;
    bCachedPriority = bPriority;
}

void ABHSectorAnchor::FaceStatusTowardPlayer()
{
    if (!IsValid(SectorStatusLabel))
    {
        return;
    }

    const APawn* PlayerPawn =
        UGameplayStatics::GetPlayerPawn(this, 0);

    if (!IsValid(PlayerPawn))
    {
        return;
    }

    const FVector ToPlayer =
        PlayerPawn->GetActorLocation() -
        SectorStatusLabel->GetComponentLocation();

    if (ToPlayer.IsNearlyZero())
    {
        return;
    }

    SectorStatusLabel->SetWorldRotation(
        FRotator(0.0f, ToPlayer.Rotation().Yaw, 0.0f)
    );
}
