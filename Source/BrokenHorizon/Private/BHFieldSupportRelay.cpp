#include "BHFieldSupportRelay.h"

#include "BHCharacter.h"
#include "BHSaveSubsystem.h"
#include "BHWarGameState.h"
#include "BHWarSubsystem.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "HAL/PlatformMisc.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
#if !UE_BUILD_SHIPPING
bool bBHTacticalSupportRuntimeProbeScheduled = false;
#endif
}

ABHFieldSupportRelay::ABHFieldSupportRelay()
{
    PrimaryActorTick.bCanEverTick = false;
    bReplicates = true;

#if WITH_EDITOR
    SetIsSpatiallyLoaded(false);
#endif

    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
    SetRootComponent(SceneRoot);

    RelayMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RelayMesh"));
    RelayMesh->SetupAttachment(SceneRoot);
    RelayMesh->SetRelativeLocation(FVector(0.0f, 0.0f, 85.0f));
    RelayMesh->SetRelativeScale3D(FVector(0.7f, 0.7f, 1.7f));
    RelayMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    RelayMesh->SetCollisionResponseToAllChannels(ECR_Block);

    static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(
        TEXT("/Engine/BasicShapes/Cylinder.Cylinder")
    );
    if (CylinderMesh.Succeeded())
    {
        RelayMesh->SetStaticMesh(CylinderMesh.Object);
    }

    RelayLabel = CreateDefaultSubobject<UTextRenderComponent>(TEXT("RelayLabel"));
    RelayLabel->SetupAttachment(SceneRoot);
    RelayLabel->SetRelativeLocation(FVector(0.0f, 0.0f, 210.0f));
    RelayLabel->SetHorizontalAlignment(EHTA_Center);
    RelayLabel->SetWorldSize(24.0f);
    RefreshPresentation();
}

void ABHFieldSupportRelay::BeginPlay()
{
    Super::BeginPlay();

#if !UE_BUILD_SHIPPING
    if (HasAuthority() &&
        !bBHTacticalSupportRuntimeProbeScheduled &&
        SectorID == FName(TEXT("WesternFOB")) &&
        SupportType == EBHTacticalSupportType::MortarBarrage &&
        FParse::Param(FCommandLine::Get(), TEXT("BHTestTacticalSupportRuntime")))
    {
        bBHTacticalSupportRuntimeProbeScheduled = true;
        FTimerHandle ProbeTimer;
        GetWorldTimerManager().SetTimer(
            ProbeTimer,
            this,
            &ABHFieldSupportRelay::RunTacticalSupportRuntimeProbe,
            1.5f,
            false
        );
    }
#endif
}

#if !UE_BUILD_SHIPPING
void ABHFieldSupportRelay::RunTacticalSupportRuntimeProbe()
{
    UWorld* World = GetWorld();
    ABHCharacter* Character = Cast<ABHCharacter>(
        UGameplayStatics::GetPlayerCharacter(this, 0)
    );
    if (!IsValid(World) || !IsValid(Character))
    {
        UE_LOG(LogTemp, Error, TEXT("BH_TACTICAL_SUPPORT_RUNTIME result=failure reason=no_player"));
        FPlatformMisc::RequestExit(false);
        return;
    }

    auto SpawnSupport = [this, World, Character](
        EBHTacticalSupportType Type,
        const FVector& Location
    )
    {
        const FTransform Transform(FRotator::ZeroRotator, Location);
        ABHTacticalSupportZone* Zone =
            World->SpawnActorDeferred<ABHTacticalSupportZone>(
                ABHTacticalSupportZone::StaticClass(),
                Transform,
                Character,
                Character,
                ESpawnActorCollisionHandlingMethod::AlwaysSpawn
            );
        if (IsValid(Zone))
        {
            Zone->InitializeSupport(Type, Character);
            UGameplayStatics::FinishSpawningActor(Zone, Transform);
        }
        return Zone;
    };

    const bool bVisualProbe = FParse::Param(
        FCommandLine::Get(),
        TEXT("BHVisualTacticalSupportRuntime")
    );
    const FVector Origin = Character->GetActorLocation() +
        (Character->GetActorForwardVector() * (bVisualProbe ? 700.0f : 1800.0f));
    ABHTacticalSupportZone* Smoke = SpawnSupport(
        EBHTacticalSupportType::SmokeScreen,
        Origin
    );
    ABHTacticalSupportZone* Mortar = SpawnSupport(
        EBHTacticalSupportType::MortarBarrage,
        Origin + FVector(1800.0f, 0.0f, 0.0f)
    );
    const bool bSmokeObscures = IsValid(Smoke) &&
        ABHTacticalSupportZone::IsLineObscuredBySmoke(
            World,
            Origin - FVector(1500.0f, 0.0f, 0.0f),
            Origin + FVector(1500.0f, 0.0f, 0.0f)
        );

    FTimerHandle ResultTimer;
    FTimerDelegate ResultDelegate = FTimerDelegate::CreateWeakLambda(
        this,
        [Mortar, bSmokeObscures]()
        {
            const int32 Shells = IsValid(Mortar)
                ? Mortar->GetFiredMortarShells()
                : 0;
            const bool bSuccess = bSmokeObscures && Shells ==
                ABHTacticalSupportZone::GetMortarShellCount();
            if (bSuccess)
            {
                UE_LOG(
                    LogTemp,
                    Display,
                    TEXT("BH_TACTICAL_SUPPORT_RUNTIME result=success smoke_obscures=1 shells=%d"),
                    Shells
                );
            }
            else
            {
                UE_LOG(
                    LogTemp,
                    Error,
                    TEXT("BH_TACTICAL_SUPPORT_RUNTIME result=failure smoke_obscures=%d shells=%d"),
                    bSmokeObscures ? 1 : 0,
                    Shells
                );
            }
            FPlatformMisc::RequestExit(false);
        }
    );
    GetWorldTimerManager().SetTimer(
        ResultTimer,
        ResultDelegate,
        bVisualProbe ? 20.0f : 5.5f,
        false
    );
}
#endif

void ABHFieldSupportRelay::GetLifetimeReplicatedProps(
    TArray<FLifetimeProperty>& OutLifetimeProps
) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ABHFieldSupportRelay, SupportType);
    DOREPLIFETIME(ABHFieldSupportRelay, CooldownEndsServerTime);
}

float ABHFieldSupportRelay::GetSupplyCost(EBHTacticalSupportType Type)
{
    return Type == EBHTacticalSupportType::MortarBarrage ? 10.0f : 6.0f;
}

bool ABHFieldSupportRelay::IsPingEligible(
    float ServerTimeSeconds,
    float PingExpiryServerTimeSeconds,
    float DistanceFromRelayCentimeters,
    float MaximumRangeCentimeters
)
{
    return PingExpiryServerTimeSeconds > ServerTimeSeconds &&
        DistanceFromRelayCentimeters <= FMath::Max(0.0f, MaximumRangeCentimeters);
}

void ABHFieldSupportRelay::Interact_Implementation(AActor* InteractingActor)
{
    ABHCharacter* Character = Cast<ABHCharacter>(InteractingActor);
    UWorld* World = GetWorld();
    UGameInstance* GameInstance = GetGameInstance();
    UBHWarSubsystem* War = IsValid(GameInstance)
        ? GameInstance->GetSubsystem<UBHWarSubsystem>()
        : nullptr;
    UBHSaveSubsystem* Save = IsValid(GameInstance)
        ? GameInstance->GetSubsystem<UBHSaveSubsystem>()
        : nullptr;
    ABHWarGameState* WarState = IsValid(World)
        ? World->GetGameState<ABHWarGameState>()
        : nullptr;

    if (!HasAuthority() || !IsValid(Character) || !IsValid(World) ||
        !IsValid(War) || !IsValid(WarState) || SectorID.IsNone())
    {
        return;
    }

    const FBHWarSectorState Sector = War->GetSectorState(SectorID);
    if (Sector.SectorID.IsNone() || Sector.Owner != EBHWarFaction::Friendly)
    {
        Character->ShowStatusNotification(NSLOCTEXT(
            "BrokenHorizon", "SupportRelayLocked",
            "SUPPORT RELAY LOCKED\n\nSecure friendly control of this sector first."
        ));
        return;
    }

    const float ServerTime = WarState->GetServerWorldTimeSeconds();
    if (CooldownEndsServerTime > ServerTime)
    {
        Character->ShowStatusNotification(FText::Format(
            NSLOCTEXT(
                "BrokenHorizon", "SupportRelayCoolingDown",
                "SUPPORT RELAY RECHARGING\n\nAvailable in {0} seconds."
            ),
            FText::AsNumber(FMath::CeilToInt(CooldownEndsServerTime - ServerTime))
        ));
        return;
    }

    const FBHSquadPingSnapshot Ping = WarState->GetSquadPingSnapshot();
    const FVector TargetLocation = IsValid(Ping.TrackedActor.Get())
        ? Ping.TrackedActor->GetActorLocation()
        : FVector(Ping.Location);
    const float TargetDistance = FVector::Dist(GetActorLocation(), TargetLocation);
    const float EffectiveMaximumRange =
        SupportType == EBHTacticalSupportType::MortarBarrage
            ? FMath::Max(100000.0f, MaximumCallInRange)
            : FMath::Min(FMath::Max(50000.0f, MaximumCallInRange), 150000.0f);
    if (!IsPingEligible(
            ServerTime,
            Ping.ExpiryServerWorldTimeSeconds,
            TargetDistance,
            EffectiveMaximumRange))
    {
        Character->ShowStatusNotification(FText::Format(
            NSLOCTEXT(
                "BrokenHorizon", "SupportRelayNeedsPing",
                "NO VALID TARGET\n\nPlace a squad ping within {0} meters, then use this relay."
            ),
            FText::AsNumber(FMath::RoundToInt(EffectiveMaximumRange / 100.0f))
        ));
        return;
    }

    const float SupplyCost = GetSupplyCost(SupportType);
    if (!War->ConsumeSectorSupply(SectorID, SupplyCost))
    {
        Character->ShowStatusNotification(FText::Format(
            NSLOCTEXT(
                "BrokenHorizon", "SupportRelayNeedsSupply",
                "INSUFFICIENT SECTOR SUPPLY\n\n{0}% required for {1}."
            ),
            FText::AsNumber(FMath::CeilToInt(SupplyCost)),
            ABHTacticalSupportZone::GetSupportDisplayName(SupportType)
        ));
        return;
    }

    FHitResult GroundHit;
    const FVector TraceStart = TargetLocation + FVector(0.0f, 0.0f, 3000.0f);
    const FVector TraceEnd = TargetLocation - FVector(0.0f, 0.0f, 3000.0f);
    FCollisionQueryParams Query(SCENE_QUERY_STAT(BHSupportGround), false, Character);
    const FVector GroundLocation = World->LineTraceSingleByChannel(
        GroundHit,
        TraceStart,
        TraceEnd,
        ECC_Visibility,
        Query
    ) ? GroundHit.ImpactPoint : TargetLocation;

    FTransform SpawnTransform(FRotator::ZeroRotator, GroundLocation);
    ABHTacticalSupportZone* Zone = World->SpawnActorDeferred<ABHTacticalSupportZone>(
        ABHTacticalSupportZone::StaticClass(),
        SpawnTransform,
        Character,
        Character,
        ESpawnActorCollisionHandlingMethod::AlwaysSpawn
    );
    if (!IsValid(Zone))
    {
        return;
    }
    Zone->InitializeSupport(SupportType, Character);
    UGameplayStatics::FinishSpawningActor(Zone, SpawnTransform);

    CooldownEndsServerTime = ServerTime + FMath::Max(1.0f, RelayCooldownSeconds);
    ForceNetUpdate();
    const bool bSaved = IsValid(Save) &&
        Save->SaveProgressForCharacter(Character);
    const FBHWarSectorState Updated = War->GetSectorState(SectorID);
    Character->ShowPriorityStatusNotification(
        FText::Format(
            NSLOCTEXT(
                "BrokenHorizon", "SupportRelayCalled",
                "{0} INBOUND\n\nTarget: {1} // Sector supply {2}% // {3}"
            ),
            ABHTacticalSupportZone::GetSupportDisplayName(SupportType),
            FText::FromName(Ping.ContextLabel),
            FText::AsNumber(FMath::RoundToInt(Updated.Supply)),
            bSaved
                ? NSLOCTEXT("BrokenHorizon", "SupportRelaySaved", "CHECKPOINT SAVED")
                : NSLOCTEXT("BrokenHorizon", "SupportRelayUnsaved", "CHECKPOINT SAVE FAILED")
        ),
        EBHNotificationPriority::High
    );

    UE_LOG(
        LogTemp,
        Display,
        TEXT("BH_TACTICAL_SUPPORT_CALLED sector=%s type=%d cost=%.1f target=%s saved=%d"),
        *SectorID.ToString(),
        static_cast<int32>(SupportType),
        SupplyCost,
        *GroundLocation.ToCompactString(),
        bSaved ? 1 : 0
    );
}

FText ABHFieldSupportRelay::GetInteractionText_Implementation() const
{
    return FText::Format(
        NSLOCTEXT(
            "BrokenHorizon", "SupportRelayPrompt",
            "[F] Call {0} at Squad Ping // {1}% Supply"
        ),
        ABHTacticalSupportZone::GetSupportDisplayName(SupportType),
        FText::AsNumber(FMath::CeilToInt(GetSupplyCost(SupportType)))
    );
}

void ABHFieldSupportRelay::ConfigureRelay(
    FName NewSectorID,
    EBHTacticalSupportType NewSupportType
)
{
    SectorID = NewSectorID;
    SupportType = NewSupportType;
    RefreshPresentation();
}

FName ABHFieldSupportRelay::GetSectorID() const
{
    return SectorID;
}

EBHTacticalSupportType ABHFieldSupportRelay::GetSupportType() const
{
    return SupportType;
}

void ABHFieldSupportRelay::OnRep_SupportType()
{
    RefreshPresentation();
}

void ABHFieldSupportRelay::RefreshPresentation()
{
    const bool bMortar = SupportType == EBHTacticalSupportType::MortarBarrage;
    if (IsValid(RelayLabel))
    {
        RelayLabel->SetText(FText::Format(
            NSLOCTEXT("BrokenHorizon", "SupportRelayLabel", "{0}\nPING TARGET + INTERACT"),
            ABHTacticalSupportZone::GetSupportDisplayName(SupportType)
        ));
        RelayLabel->SetTextRenderColor(
            bMortar ? FColor(255, 110, 55) : FColor(165, 195, 205)
        );
    }
}
