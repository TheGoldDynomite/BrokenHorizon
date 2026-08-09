#include "BHSupplyConvoyTarget.h"

#include "BHCharacter.h"
#include "BHEnemySoldier.h"
#include "BHFieldTransport.h"
#include "BHHealthComponent.h"
#include "BHMissionData.h"
#include "BHPlayerResolver.h"
#include "BHSaveSubsystem.h"
#include "BHWarSubsystem.h"
#include "BHWorldRoute.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/CollisionProfile.h"
#include "EngineUtils.h"
#include "Engine/GameInstance.h"
#include "Engine/StaticMesh.h"
#include "Kismet/GameplayStatics.h"
#include "NavigationSystem.h"
#include "UObject/ConstructorHelpers.h"

ABHSupplyConvoyTarget::ABHSupplyConvoyTarget()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickInterval = 0.1f;
    SetCanBeDamaged(true);

    SceneRoot = CreateDefaultSubobject<USceneComponent>(
        TEXT("SceneRoot")
    );
    SetRootComponent(SceneRoot);

    ChassisMesh = CreateDefaultSubobject<UStaticMeshComponent>(
        TEXT("ChassisMesh")
    );
    ChassisMesh->SetupAttachment(SceneRoot);
    ChassisMesh->SetRelativeLocation(
        FVector(0.0f, 0.0f, 65.0f)
    );
    ChassisMesh->SetRelativeScale3D(
        FVector(4.5f, 1.9f, 0.65f)
    );

    CabMesh = CreateDefaultSubobject<UStaticMeshComponent>(
        TEXT("CabMesh")
    );
    CabMesh->SetupAttachment(SceneRoot);
    CabMesh->SetRelativeLocation(
        FVector(245.0f, 0.0f, 125.0f)
    );
    CabMesh->SetRelativeScale3D(
        FVector(1.25f, 1.8f, 1.25f)
    );

    CargoMesh = CreateDefaultSubobject<UStaticMeshComponent>(
        TEXT("CargoMesh")
    );
    CargoMesh->SetupAttachment(SceneRoot);
    CargoMesh->SetRelativeLocation(
        FVector(-135.0f, 0.0f, 145.0f)
    );
    CargoMesh->SetRelativeScale3D(
        FVector(2.5f, 1.75f, 1.4f)
    );

    static ConstructorHelpers::FObjectFinder<UStaticMesh>
        CubeMeshFinder(
            TEXT("/Engine/BasicShapes/Cube.Cube")
        );

    if (CubeMeshFinder.Succeeded())
    {
        ChassisMesh->SetStaticMesh(CubeMeshFinder.Object);
        CabMesh->SetStaticMesh(CubeMeshFinder.Object);
        CargoMesh->SetStaticMesh(CubeMeshFinder.Object);
    }

    for (UStaticMeshComponent* MeshComponent :
        { ChassisMesh.Get(), CabMesh.Get(), CargoMesh.Get() })
    {
        if (!IsValid(MeshComponent))
        {
            continue;
        }

        MeshComponent->SetCollisionProfileName(
            UCollisionProfile::BlockAllDynamic_ProfileName
        );
        MeshComponent->SetGenerateOverlapEvents(false);
    }

    ConvoyLabel = CreateDefaultSubobject<UTextRenderComponent>(
        TEXT("ConvoyLabel")
    );
    ConvoyLabel->SetupAttachment(SceneRoot);
    ConvoyLabel->SetRelativeLocation(
        FVector(0.0f, 0.0f, 340.0f)
    );
    ConvoyLabel->SetRelativeRotation(
        FRotator(0.0f, 90.0f, 0.0f)
    );
    ConvoyLabel->SetHorizontalAlignment(EHTA_Center);
    ConvoyLabel->SetWorldSize(34.0f);
    ConvoyLabel->SetTextRenderColor(
        FColor(255, 74, 44, 255)
    );
    ConvoyLabel->SetText(
        NSLOCTEXT(
            "BrokenHorizon",
            "UnassignedSupplyConvoyLabel",
            "ENEMY SUPPLY CONVOY"
        )
    );

    HealthComponent =
        CreateDefaultSubobject<UBHHealthComponent>(
            TEXT("HealthComponent")
        );
}

float ABHSupplyConvoyTarget::CalculateRecoverableSupply(
    float Payload,
    float RecoveryFraction
)
{
    return FMath::FloorToFloat(
        FMath::Max(0.0f, Payload) *
        FMath::Clamp(RecoveryFraction, 0.0f, 1.0f)
    );
}

float ABHSupplyConvoyTarget::CalculateRouteSpeedMultiplier(
    float HealthFraction,
    float CriticalHealthFraction,
    float MinimumSpeedMultiplier
)
{
    const float ClampedHealthFraction = FMath::Clamp(
        HealthFraction,
        0.0f,
        1.0f
    );
    const float ClampedCriticalFraction = FMath::Clamp(
        CriticalHealthFraction,
        KINDA_SMALL_NUMBER,
        1.0f
    );
    const float ClampedMinimumMultiplier = FMath::Clamp(
        MinimumSpeedMultiplier,
        0.0f,
        1.0f
    );

    if (ClampedHealthFraction >= ClampedCriticalFraction)
    {
        return 1.0f;
    }

    const float DamageAlpha = FMath::Clamp(
        1.0f -
            ClampedHealthFraction / ClampedCriticalFraction,
        0.0f,
        1.0f
    );
    return FMath::Lerp(
        1.0f,
        ClampedMinimumMultiplier,
        DamageAlpha
    );
}

float ABHSupplyConvoyTarget::CalculateDamageAdjustedRecoverableSupply(
    float Payload,
    float RecoveryFraction,
    float IntegrityFraction,
    float MinimumCargoIntegrityAtWreck
)
{
    const float BaseRecoverableSupply = CalculateRecoverableSupply(
        Payload,
        RecoveryFraction
    );
    const float ClampedIntegrityFraction = FMath::Clamp(
        IntegrityFraction,
        0.0f,
        1.0f
    );
    const float ClampedMinimumCargoIntegrity = FMath::Clamp(
        MinimumCargoIntegrityAtWreck,
        0.0f,
        1.0f
    );
    const float CargoConditionMultiplier = FMath::Lerp(
        ClampedMinimumCargoIntegrity,
        1.0f,
        ClampedIntegrityFraction
    );

    return FMath::FloorToFloat(
        BaseRecoverableSupply * CargoConditionMultiplier
    );
}

FBHRouteOperationProfile
ABHSupplyConvoyTarget::BuildRouteOperationProfile(
    const FBHWarSupplyConvoyState& ConvoyState
)
{
    return BHRouteOperations::BuildProfile(ConvoyState);
}

void ABHSupplyConvoyTarget::BeginPlay()
{
    Super::BeginPlay();

    if (IsValid(HealthComponent))
    {
        HealthComponent->OnDeath.AddDynamic(
            this,
            &ABHSupplyConvoyTarget::HandleConvoyDestroyed
        );
    }

    UpdateConvoyLabel();
}

void ABHSupplyConvoyTarget::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    if (bResolved || ConvoyID.IsNone())
    {
        return;
    }

    MoveAlongRoute(DeltaSeconds);

    if (bResolved)
    {
        return;
    }

    StrategicStateRefreshRemaining -=
        FMath::Max(0.0f, DeltaSeconds);

    if (StrategicStateRefreshRemaining > 0.0f)
    {
        return;
    }

    StrategicStateRefreshRemaining = 0.5f;

    const UGameInstance* GameInstance = GetGameInstance();
    const UBHWarSubsystem* WarSubsystem = GameInstance
        ? GameInstance->GetSubsystem<UBHWarSubsystem>()
        : nullptr;

    if (IsValid(WarSubsystem) &&
        !WarSubsystem->HasSupplyConvoy(ConvoyID))
    {
        bResolved = true;

        NotifyPlayer(
            ConvoyOwner == EBHWarFaction::Friendly
                ? NSLOCTEXT(
                    "BrokenHorizon",
                    "FriendlySupplyConvoyDelivered",
                    "FRIENDLY CONVOY DELIVERED\n\n"
                    "The protected shipment reached its "
                    "strategic destination."
                )
                : NSLOCTEXT(
                    "BrokenHorizon",
                    "SupplyConvoyContactEnded",
                    "CONVOY CONTACT ENDED\n\n"
                    "The strategic shipment is no longer "
                    "in transit."
                )
        );

        UE_LOG(
            LogTemp,
            Display,
            TEXT(
                "BH_CONVOY_TARGET_EXPIRED id=%s"
            ),
            *ConvoyID.ToString()
        );

        Destroy();
    }
    else if (IsValid(WarSubsystem))
    {
        const FBHWarSupplyConvoyState StrategicConvoy =
            WarSubsystem->GetSupplyConvoyState(ConvoyID);
        RouteOperationProfile =
            StrategicConvoy.RouteOperationProfile;
        OperationDeadlineRemaining =
            StrategicConvoy.OperationDeadlineSecondsRemaining;

        if (!bOperationDeadlineResolved &&
            RouteOperationProfile.Variation ==
                EBHRouteOperationVariation::TimeCritical &&
            OperationDeadlineRemaining <= KINDA_SMALL_NUMBER)
        {
            HandleOperationDeadlineExpired();
        }
    }
}

void ABHSupplyConvoyTarget::ConfigureConvoy(
    const FBHWarSupplyConvoyState& ConvoyState
)
{
    ConvoyID = ConvoyState.ConvoyID;
    SourceSectorID = ConvoyState.SourceSectorID;
    DestinationSectorID =
        ConvoyState.DestinationSectorID;
    ConvoyOwner = ConvoyState.Owner;
    CargoType = ConvoyState.CargoType;
    SupplyPayload = FMath::Max(
        0.0f,
        ConvoyState.SupplyPayload
    );
    RouteOperationProfile = ConvoyState.bRouteOperationInitialized
        ? ConvoyState.RouteOperationProfile
        : BuildRouteOperationProfile(ConvoyState);
    OperationDeadlineRemaining = ConvoyState.bRouteOperationInitialized
        ? ConvoyState.OperationDeadlineSecondsRemaining
        : RouteOperationProfile.CompletionDeadlineSeconds;

    if (IsValid(HealthComponent) &&
        RouteOperationProfile.InitialIntegrity < 1.0f)
    {
        HealthComponent->RestorePersistentHealthState(
            HealthComponent->GetMaxHealth() *
                FMath::Clamp(
                    RouteOperationProfile.InitialIntegrity,
                    0.01f,
                    1.0f
                )
        );
    }

    if (ConvoyOwner == EBHWarFaction::Friendly)
    {
        Tags.AddUnique(TEXT("BH_Friendly"));
        Tags.Remove(TEXT("BH_Hostile"));
    }
    else if (ConvoyOwner == EBHWarFaction::Enemy)
    {
        Tags.AddUnique(TEXT("BH_Hostile"));
        Tags.Remove(TEXT("BH_Friendly"));
    }

    UpdateConvoyLabel();
}

FBHRouteOperationProfile
ABHSupplyConvoyTarget::GetRouteOperationProfile() const
{
    return RouteOperationProfile;
}

float ABHSupplyConvoyTarget::GetOperationDeadlineRemaining() const
{
    return FMath::Max(0.0f, OperationDeadlineRemaining);
}

void ABHSupplyConvoyTarget::SetTravelDestination(
    const FVector& NewTravelDestination
)
{
    TravelRoute = nullptr;
    TravelDestination = NewTravelDestination;
    bHasTravelDestination = true;
}

void ABHSupplyConvoyTarget::SetTravelRoute(
    ABHWorldRoute* NewTravelRoute,
    const FVector& NewTravelDestination
)
{
    if (!IsValid(NewTravelRoute) ||
        NewTravelRoute->GetRouteLength() <= KINDA_SMALL_NUMBER)
    {
        SetTravelDestination(NewTravelDestination);
        return;
    }

    TravelRoute = NewTravelRoute;
    TravelDestination = NewTravelDestination;
    bHasTravelDestination = true;
    CurrentRouteDistance =
        TravelRoute->GetDistanceAlongRouteClosestToWorldLocation(
            GetActorLocation()
        );
    DestinationRouteDistance =
        TravelRoute->GetDistanceAlongRouteClosestToWorldLocation(
            NewTravelDestination
        );
    RouteTravelDirection =
        DestinationRouteDistance >= CurrentRouteDistance
            ? 1.0f
            : -1.0f;

    UE_LOG(
        LogTemp,
        Display,
        TEXT(
            "BH_CONVOY_ROUTE_ASSIGNED id=%s route=%s "
            "start_distance=%.0f destination_distance=%.0f "
            "direction=%.0f"
        ),
        *ConvoyID.ToString(),
        *TravelRoute->GetRouteID().ToString(),
        CurrentRouteDistance,
        DestinationRouteDistance,
        RouteTravelDirection
    );
}

void ABHSupplyConvoyTarget::SetRouteChoices(
    const TArray<ABHWorldRoute*>& NewRouteChoices,
    ABHWorldRoute* InitiallySelectedRoute,
    const FVector& NewTravelDestination
)
{
    RouteChoices.Reset();

    for (ABHWorldRoute* Candidate : NewRouteChoices)
    {
        if (IsValid(Candidate) &&
            Candidate->GetRouteLength() > KINDA_SMALL_NUMBER)
        {
            RouteChoices.AddUnique(Candidate);
        }
    }

    SetTravelRoute(InitiallySelectedRoute, NewTravelDestination);
}

FName ABHSupplyConvoyTarget::GetConvoyID() const
{
    return ConvoyID;
}

float ABHSupplyConvoyTarget::GetSupplyPayload() const
{
    return SupplyPayload;
}

float ABHSupplyConvoyTarget::GetHealthPercentage() const
{
    return IsValid(HealthComponent)
        ? HealthComponent->GetHealthPercentage()
        : 0.0f;
}

float ABHSupplyConvoyTarget::GetRouteSpeedMultiplier() const
{
    const float HealthFraction = GetHealthPercentage();

    return HealthFraction <= KINDA_SMALL_NUMBER
        ? 0.0f
        : CalculateRouteSpeedMultiplier(
            HealthFraction,
            CriticalHealthFraction,
            MinimumDamagedRouteSpeedMultiplier
        );
}

EBHWarFaction ABHSupplyConvoyTarget::GetConvoyOwner() const
{
    return ConvoyOwner;
}

EBHWarConvoyCargoType
ABHSupplyConvoyTarget::GetCargoType() const
{
    return CargoType;
}

bool ABHSupplyConvoyTarget::IsResolved() const
{
    return bResolved;
}

float ABHSupplyConvoyTarget::GetRecoverableSupply() const
{
    return FMath::Max(0.0f, RecoverableSupply);
}

bool ABHSupplyConvoyTarget::HasRecoverableSalvage() const
{
    return bResolved &&
        RecoverableSupply > KINDA_SMALL_NUMBER;
}

float ABHSupplyConvoyTarget::
GetSalvageLifetimeRemaining() const
{
    return HasRecoverableSalvage()
        ? FMath::Max(0.0f, GetLifeSpan())
        : 0.0f;
}

void ABHSupplyConvoyTarget::RestoreSalvageWreck(
    FName SavedConvoyID,
    FName SavedSourceSectorID,
    FName SavedDestinationSectorID,
    float SavedOriginalSupplyPayload,
    float SavedRecoverableSupply,
    float SavedLifetimeRemaining
)
{
    ConvoyID = SavedConvoyID;
    SourceSectorID = SavedSourceSectorID;
    DestinationSectorID = SavedDestinationSectorID;
    ConvoyOwner = EBHWarFaction::Enemy;
    CargoType = EBHWarConvoyCargoType::MilitarySupply;
    SupplyPayload = FMath::Max(
        0.0f,
        SavedOriginalSupplyPayload
    );
    RecoverableSupply = FMath::Clamp(
        SavedRecoverableSupply,
        0.0f,
        SupplyPayload
    );
    bResolved = true;
    bHasTravelDestination = false;
    SetCanBeDamaged(false);
    DisableConvoyCollision();

    if (RecoverableSupply > KINDA_SMALL_NUMBER)
    {
        EnableWreckInteraction();
        UpdateConvoyLabel();
        SetLifeSpan(
            FMath::Clamp(
                SavedLifetimeRemaining,
                1.0f,
                FMath::Max(1.0f, SalvageLifetime)
            )
        );
    }
    else
    {
        Destroy();
    }

    UE_LOG(
        LogTemp,
        Display,
        TEXT(
            "BH_CONVOY_SALVAGE_RESTORED id=%s source=%s "
            "destination=%s recoverable=%.1f"
        ),
        *ConvoyID.ToString(),
        *SourceSectorID.ToString(),
        *DestinationSectorID.ToString(),
        RecoverableSupply
    );
}

FName ABHSupplyConvoyTarget::GetSourceSectorID() const
{
    return SourceSectorID;
}

FName ABHSupplyConvoyTarget::GetDestinationSectorID() const
{
    return DestinationSectorID;
}

void ABHSupplyConvoyTarget::Interact_Implementation(
    AActor* InteractingActor
)
{
    ABHCharacter* Character =
        Cast<ABHCharacter>(InteractingActor);

    if (!bResolved && IsValid(Character) &&
        SelectNextRoute(Character))
    {
        return;
    }

    if (!bResolved ||
        RecoverableSupply <= KINDA_SMALL_NUMBER ||
        !IsValid(Character))
    {
        return;
    }

    if (HasActiveHostileSecurity())
    {
        Character->ShowStatusNotification(
            NSLOCTEXT(
                "BrokenHorizon",
                "ConvoySalvageSecurityActive",
                "RECOVERY AREA CONTESTED\n\n"
                "Clear or drive off nearby enemy security "
                "before recovering the cargo."
            )
        );
        return;
    }

    ABHFieldTransport* NearestTransport = nullptr;
    float NearestDistanceSquared = FMath::Square(
        FMath::Max(100.0f, SalvageTransportRadius)
    );

    for (TActorIterator<ABHFieldTransport> It(GetWorld());
        It;
        ++It)
    {
        ABHFieldTransport* Candidate = *It;

        if (!IsValid(Candidate))
        {
            continue;
        }

        const float DistanceSquared = FVector::DistSquared(
            GetActorLocation(),
            Candidate->GetActorLocation()
        );

        if (DistanceSquared <= NearestDistanceSquared)
        {
            NearestTransport = Candidate;
            NearestDistanceSquared = DistanceSquared;
        }
    }

    if (!IsValid(NearestTransport))
    {
        Character->ShowStatusNotification(
            NSLOCTEXT(
                "BrokenHorizon",
                "ConvoySalvageNeedsTransport",
                "RECOVERY VEHICLE REQUIRED\n\n"
                "Bring the field transport within 20 meters "
                "to recover this supply."
            )
        );
        return;
    }

    const float LoadedSupply =
        NearestTransport->LoadRecoveredMilitarySupply(
            RecoverableSupply,
            SourceSectorID
        );

    if (LoadedSupply <= KINDA_SMALL_NUMBER)
    {
        Character->ShowStatusNotification(
            NSLOCTEXT(
                "BrokenHorizon",
                "ConvoySalvageCargoBlocked",
                "RECOVERY BLOCKED\n\n"
                "The nearby field transport is full or "
                "carrying civilian aid."
            )
        );
        return;
    }

    RecoverableSupply = FMath::Max(
        0.0f,
        RecoverableSupply - LoadedSupply
    );
    Character->ShowStatusNotification(
        FText::Format(
            NSLOCTEXT(
                "BrokenHorizon",
                "ConvoySalvageRecovered",
                "SUPPLY RECOVERED\n\n"
                "{0} captured supply loaded into the field "
                "transport. Deliver it to friendly territory."
            ),
            FText::AsNumber(FMath::RoundToInt(LoadedSupply))
        )
    );

    if (UBHSaveSubsystem* SaveSubsystem = GetGameInstance()
        ? GetGameInstance()->GetSubsystem<UBHSaveSubsystem>()
        : nullptr)
    {
        SaveSubsystem->SaveProgress();
    }

    UE_LOG(
        LogTemp,
        Display,
        TEXT(
            "BH_CONVOY_SALVAGE_RECOVERED id=%s amount=%.1f "
            "remaining=%.1f transport=%s"
        ),
        *ConvoyID.ToString(),
        LoadedSupply,
        RecoverableSupply,
        *GetNameSafe(NearestTransport)
    );

    if (RecoverableSupply <= KINDA_SMALL_NUMBER)
    {
        Destroy();
    }
    else
    {
        UpdateConvoyLabel();
    }
}

FText ABHSupplyConvoyTarget::
GetInteractionText_Implementation() const
{
    if (!bResolved && IsCommittedFriendlyEscort() &&
        RouteChoices.Num() > 1)
    {
        return NSLOCTEXT(
            "BrokenHorizon",
            "ConvoyRouteChoiceInteraction",
            "Press [F] to choose alternate convoy route"
        );
    }

    if (bResolved &&
        RecoverableSupply > KINDA_SMALL_NUMBER &&
        HasActiveHostileSecurity())
    {
        return NSLOCTEXT(
            "BrokenHorizon",
            "ConvoySalvageSecurityInteraction",
            "Recovery blocked: enemy security nearby"
        );
    }

    return bResolved &&
        RecoverableSupply > KINDA_SMALL_NUMBER
        ? FText::Format(
            NSLOCTEXT(
                "BrokenHorizon",
                "ConvoySalvageInteraction",
                "Press [F] to recover {0} supply"
            ),
            FText::AsNumber(
                FMath::RoundToInt(RecoverableSupply)
            )
        )
        : FText::GetEmpty();
}

bool ABHSupplyConvoyTarget::IsCommittedFriendlyEscort() const
{
    const UGameInstance* GameInstance = GetGameInstance();
    const UBHWarSubsystem* WarSubsystem = GameInstance
        ? GameInstance->GetSubsystem<UBHWarSubsystem>()
        : nullptr;
    return ConvoyOwner == EBHWarFaction::Friendly &&
        IsValid(WarSubsystem) &&
        WarSubsystem->DoesConvoyMatchCommittedEscort(ConvoyID);
}

bool ABHSupplyConvoyTarget::SelectNextRoute(AActor* InteractingActor)
{
    if (!HasAuthority() || !IsCommittedFriendlyEscort() ||
        RouteChoices.Num() <= 1)
    {
        return false;
    }

    const int32 CurrentIndex = RouteChoices.IndexOfByKey(TravelRoute);
    const int32 NextIndex = (CurrentIndex + 1) % RouteChoices.Num();
    ABHWorldRoute* NextRoute = RouteChoices[NextIndex];
    UGameInstance* GameInstance = GetGameInstance();
    UBHWarSubsystem* WarSubsystem = GameInstance
        ? GameInstance->GetSubsystem<UBHWarSubsystem>()
        : nullptr;
    if (!IsValid(WarSubsystem) ||
        !WarSubsystem->SetConvoySelectedWorldRoute(
            ConvoyID,
            NextRoute->GetRouteID()))
    {
        return false;
    }
    SetTravelRoute(NextRoute, TravelDestination);

    if (ABHCharacter* Character = Cast<ABHCharacter>(InteractingActor))
    {
        Character->ShowStatusNotification(FText::Format(
            NSLOCTEXT(
                "BrokenHorizon",
                "ConvoyRouteChanged",
                "CONVOY REROUTED\n\nRoute: {0}\nThe convoy is now committed to the selected corridor."
            ),
            NextRoute->GetRouteDisplayName().IsEmpty()
                ? FText::FromName(NextRoute->GetRouteID())
                : NextRoute->GetRouteDisplayName()
        ));
    }

    return true;
}

void ABHSupplyConvoyTarget::HandleOperationDeadlineExpired()
{
    bOperationDeadlineResolved = true;

    if (!IsCommittedFriendlyEscort())
    {
        return;
    }

    for (TActorIterator<ABHCharacter> It(GetWorld()); It; ++It)
    {
        ABHCharacter* AssignedCharacter = *It;
        if (IsValid(AssignedCharacter) &&
            AssignedCharacter->IsRuntimeWarOperation() &&
            AssignedCharacter->GetAssignedWarPriorityType() ==
                EBHWarPriorityType::EscortRescue)
        {
            AssignedCharacter->FailCurrentWarOperation(
                NSLOCTEXT(
                    "BrokenHorizon",
                    "EscortDeadlineExpiredFailureReason",
                    "The convoy missed its operational window before clearing the route."
                )
            );
            break;
        }
    }
}

bool ABHSupplyConvoyTarget::HasActiveHostileSecurity() const
{
    const UWorld* World = GetWorld();
    const float SecurityRadiusSquared = FMath::Square(
        FMath::Max(0.0f, SalvageSecurityRadius)
    );

    if (!IsValid(World) ||
        SecurityRadiusSquared <= KINDA_SMALL_NUMBER)
    {
        return false;
    }

    for (TActorIterator<ABHEnemySoldier> It(World); It; ++It)
    {
        const ABHEnemySoldier* Soldier = *It;

        if (!IsValid(Soldier) ||
            Soldier->IsDead() ||
            Soldier->IsIncapacitated() ||
            Soldier->GetCombatFaction() !=
                EBHCombatFaction::Hostile)
        {
            continue;
        }

        if (FVector::DistSquared2D(
                GetActorLocation(),
                Soldier->GetActorLocation()
            ) <= SecurityRadiusSquared)
        {
            return true;
        }
    }

    return false;
}

void ABHSupplyConvoyTarget::HandleConvoyDestroyed(
    AActor* DamageCauser
)
{
    if (bResolved)
    {
        return;
    }

    bResolved = true;

    UGameInstance* GameInstance = GetGameInstance();
    UBHWarSubsystem* WarSubsystem = GameInstance
        ? GameInstance->GetSubsystem<UBHWarSubsystem>()
        : nullptr;
    const bool bInterdicted =
        IsValid(WarSubsystem) &&
        WarSubsystem->InterdictSupplyConvoy(ConvoyID);

    if (!bInterdicted)
    {
        Destroy();
        return;
    }

    ResolveCommittedEscort(false);

    const FBHWarSectorState SourceSector =
        WarSubsystem->GetSectorState(SourceSectorID);
    const FBHWarSectorState DestinationSector =
        WarSubsystem->GetSectorState(DestinationSectorID);
    const FText SourceName = SourceSector.DisplayName.IsEmpty()
        ? FText::FromName(SourceSectorID)
        : SourceSector.DisplayName;
    const FText DestinationName =
        DestinationSector.DisplayName.IsEmpty()
            ? FText::FromName(DestinationSectorID)
            : DestinationSector.DisplayName;

    if (ABHCharacter* PlayerCharacter =
        BHPlayerResolver::Find(this))
    {
        const bool bCivilianAid =
            CargoType == EBHWarConvoyCargoType::CivilianAid;
        const FText OutcomeFormat =
            bCivilianAid
                ? NSLOCTEXT(
                    "BrokenHorizon",
                    "CivilianAidConvoyLost",
                    "AID CONVOY LOST\n\n"
                    "{0} -> {1}\n"
                    "The relief shipment was destroyed."
                )
                : ConvoyOwner == EBHWarFaction::Friendly
                ? NSLOCTEXT(
                    "BrokenHorizon",
                    "FriendlySupplyConvoyLost",
                    "FRIENDLY CONVOY LOST\n\n"
                    "{0} -> {1}\n"
                    "{2} friendly supply destroyed."
                )
                : NSLOCTEXT(
                    "BrokenHorizon",
                    "SupplyConvoyInterdicted",
                    "CONVOY INTERDICTED\n\n"
                    "{0} -> {1}\n"
                    "{2} enemy supply destroyed."
                );
        PlayerCharacter->ShowStatusNotification(
            FText::Format(
                OutcomeFormat,
                SourceName,
                DestinationName,
                FText::AsNumber(
                    FMath::RoundToInt(SupplyPayload)
                )
            )
        );
    }

    DisableConvoyCollision();
    RecoverableSupply =
        ConvoyOwner == EBHWarFaction::Enemy &&
        CargoType == EBHWarConvoyCargoType::MilitarySupply
        ? CalculateDamageAdjustedRecoverableSupply(
            SupplyPayload,
            EnemyCargoRecoveryFraction,
            GetHealthPercentage(),
            MinimumCargoIntegrityAtWreck
        )
        : 0.0f;

    if (IsValid(ConvoyLabel))
    {
        ConvoyLabel->SetText(
            RecoverableSupply > KINDA_SMALL_NUMBER
                ? FText::Format(
                    NSLOCTEXT(
                        "BrokenHorizon",
                        "SupplyConvoySalvageLabel",
                        "RECOVERABLE SUPPLY\n{0} AVAILABLE"
                    ),
                    FText::AsNumber(
                        FMath::RoundToInt(
                            RecoverableSupply
                        )
                    )
                )
                : CargoType ==
                    EBHWarConvoyCargoType::CivilianAid
                ? NSLOCTEXT(
                    "BrokenHorizon",
                    "CivilianAidConvoyDestroyedLabel",
                    "AID CONVOY DESTROYED"
                )
                : NSLOCTEXT(
                    "BrokenHorizon",
                    "SupplyConvoyDestroyedLabel",
                    "SUPPLY CONVOY DESTROYED"
                )
        );
        ConvoyLabel->SetTextRenderColor(
            FColor(255, 186, 44, 255)
        );
    }

    UE_LOG(
        LogTemp,
        Display,
        TEXT(
            "BH_CONVOY_TARGET_DESTROYED id=%s owner=%d "
            "payload=%.1f causer=%s"
        ),
        *ConvoyID.ToString(),
        static_cast<int32>(ConvoyOwner),
        SupplyPayload,
        *GetNameSafe(DamageCauser)
    );

    if (RecoverableSupply > KINDA_SMALL_NUMBER)
    {
        EnableWreckInteraction();
        SetLifeSpan(FMath::Max(1.0f, SalvageLifetime));
    }
    else
    {
        SetLifeSpan(3.0f);
    }

    if (UBHSaveSubsystem* SaveSubsystem = GameInstance
        ? GameInstance->GetSubsystem<UBHSaveSubsystem>()
        : nullptr)
    {
        if (!SaveSubsystem->SaveProgress())
        {
            UE_LOG(
                LogTemp,
                Warning,
                TEXT(
                    "BH_CONVOY_INTERDICTION_CHECKPOINT_FAILED "
                    "id=%s"
                ),
                *ConvoyID.ToString()
            );
        }
    }
}

void ABHSupplyConvoyTarget::UpdateConvoyLabel()
{
    if (!IsValid(ConvoyLabel))
    {
        return;
    }

    if (bResolved &&
        RecoverableSupply > KINDA_SMALL_NUMBER)
    {
        ConvoyLabel->SetText(
            FText::Format(
                NSLOCTEXT(
                    "BrokenHorizon",
                    "SupplyConvoySalvageLabel",
                    "RECOVERABLE SUPPLY\n{0} AVAILABLE"
                ),
                FText::AsNumber(
                    FMath::RoundToInt(RecoverableSupply)
                )
            )
        );
        ConvoyLabel->SetTextRenderColor(
            FColor(255, 186, 44, 255)
        );
        return;
    }

    const bool bFriendly =
        ConvoyOwner == EBHWarFaction::Friendly;
    const bool bCivilianAid =
        CargoType == EBHWarConvoyCargoType::CivilianAid;
    ConvoyLabel->SetText(
        FText::Format(
            bCivilianAid
                ? NSLOCTEXT(
                    "BrokenHorizon",
                    "CivilianAidConvoyTargetLabel",
                    "CIVILIAN AID CONVOY\n{0} SUPPLY"
                )
                : bFriendly
                ? NSLOCTEXT(
                    "BrokenHorizon",
                    "FriendlySupplyConvoyTargetLabel",
                    "FRIENDLY SUPPLY CONVOY\n{0} SUPPLY"
                )
                : NSLOCTEXT(
                    "BrokenHorizon",
                    "SupplyConvoyTargetLabel",
                    "ENEMY SUPPLY CONVOY\n{0} SUPPLY"
                ),
            FText::AsNumber(
                FMath::RoundToInt(SupplyPayload)
            )
        )
    );
    ConvoyLabel->SetTextRenderColor(
        bFriendly
            ? FColor(54, 210, 235, 255)
            : FColor(255, 74, 44, 255)
    );
}

void ABHSupplyConvoyTarget::DisableConvoyCollision()
{
    SetCanBeDamaged(false);

    for (UStaticMeshComponent* MeshComponent :
        { ChassisMesh.Get(), CabMesh.Get(), CargoMesh.Get() })
    {
        if (IsValid(MeshComponent))
        {
            MeshComponent->SetCollisionEnabled(
                ECollisionEnabled::NoCollision
            );
        }
    }
}

void ABHSupplyConvoyTarget::EnableWreckInteraction()
{
    if (!IsValid(ChassisMesh))
    {
        return;
    }

    ChassisMesh->SetCollisionEnabled(
        ECollisionEnabled::QueryOnly
    );
    ChassisMesh->SetCollisionResponseToAllChannels(
        ECollisionResponse::ECR_Ignore
    );
    ChassisMesh->SetCollisionResponseToChannel(
        ECollisionChannel::ECC_Visibility,
        ECollisionResponse::ECR_Block
    );
}

void ABHSupplyConvoyTarget::MoveAlongRoute(float DeltaSeconds)
{
    const float EffectiveMovementSpeed =
        MovementSpeed * GetRouteSpeedMultiplier();

    if (!bHasTravelDestination ||
        EffectiveMovementSpeed <= KINDA_SMALL_NUMBER)
    {
        return;
    }

    if (IsValid(TravelRoute))
    {
        const float DistanceRemaining = FMath::Abs(
            DestinationRouteDistance - CurrentRouteDistance
        );

        if (DistanceRemaining <= ArrivalRadius)
        {
            ResolveLocalRouteExit();
            return;
        }

        const float TravelDistance = FMath::Min(
            EffectiveMovementSpeed * FMath::Max(0.0f, DeltaSeconds),
            DistanceRemaining
        );
        CurrentRouteDistance = FMath::Clamp(
            CurrentRouteDistance +
                (RouteTravelDirection * TravelDistance),
            0.0f,
            TravelRoute->GetRouteLength()
        );

        FVector DesiredLocation =
            TravelRoute->GetWorldLocationAtDistance(
                CurrentRouteDistance
            );

        if (UNavigationSystemV1* NavigationSystem =
            UNavigationSystemV1::GetCurrent(GetWorld()))
        {
            FNavLocation ProjectedLocation;

            if (NavigationSystem->ProjectPointToNavigation(
                    DesiredLocation,
                    ProjectedLocation,
                    FVector(500.0f, 500.0f, 1500.0f)))
            {
                DesiredLocation = ProjectedLocation.Location;
            }
        }

        const FVector RouteDirection =
            TravelRoute->GetWorldDirectionAtDistance(
                CurrentRouteDistance
            ).GetSafeNormal2D() * RouteTravelDirection;

        if (!RouteDirection.IsNearlyZero())
        {
            SetActorRotation(
                FMath::RInterpTo(
                    GetActorRotation(),
                    RouteDirection.Rotation(),
                    DeltaSeconds,
                    4.0f
                )
            );
        }

        SetActorLocation(DesiredLocation);
        return;
    }

    const FVector CurrentLocation = GetActorLocation();
    const FVector ToDestination =
        TravelDestination - CurrentLocation;
    const float DistanceRemaining = ToDestination.Size2D();

    if (DistanceRemaining <= ArrivalRadius)
    {
        ResolveLocalRouteExit();
        return;
    }

    const FVector TravelDirection(
        ToDestination.X / DistanceRemaining,
        ToDestination.Y / DistanceRemaining,
        0.0f
    );
    const float TravelDistance = FMath::Min(
        EffectiveMovementSpeed * FMath::Max(0.0f, DeltaSeconds),
        DistanceRemaining
    );
    FVector DesiredLocation =
        CurrentLocation + (TravelDirection * TravelDistance);

    if (UNavigationSystemV1* NavigationSystem =
        UNavigationSystemV1::GetCurrent(GetWorld()))
    {
        FNavLocation ProjectedLocation;

        if (NavigationSystem->ProjectPointToNavigation(
                DesiredLocation,
                ProjectedLocation,
                FVector(500.0f, 500.0f, 1500.0f)))
        {
            DesiredLocation = ProjectedLocation.Location;
        }
    }

    const FRotator DesiredRotation = TravelDirection.Rotation();
    SetActorRotation(
        FMath::RInterpTo(
            GetActorRotation(),
            DesiredRotation,
            DeltaSeconds,
            4.0f
        )
    );
    SetActorLocation(DesiredLocation);
}

void ABHSupplyConvoyTarget::ResolveLocalRouteExit()
{
    if (bResolved)
    {
        return;
    }

    bResolved = true;

    ResolveCommittedEscort(true);

    NotifyPlayer(
        FText::Format(
            CargoType == EBHWarConvoyCargoType::CivilianAid
                ? NSLOCTEXT(
                    "BrokenHorizon",
                    "CivilianAidConvoyCleared",
                    "AID CONVOY SECURED\n\n"
                    "{0} supply of relief cargo cleared the "
                    "local area."
                )
                : ConvoyOwner == EBHWarFaction::Friendly
                ? NSLOCTEXT(
                    "BrokenHorizon",
                    "FriendlySupplyConvoyCleared",
                    "FRIENDLY CONVOY SECURED\n\n"
                    "{0} friendly supply cleared the "
                    "local area."
                )
                : NSLOCTEXT(
                    "BrokenHorizon",
                    "SupplyConvoyEscaped",
                    "CONVOY ESCAPED\n\n"
                    "{0} enemy supply left the local area."
                ),
            FText::AsNumber(
                FMath::RoundToInt(SupplyPayload)
            )
        )
    );

    UE_LOG(
        LogTemp,
        Display,
        TEXT(
            "BH_CONVOY_TARGET_CLEARED id=%s "
            "destination=%s route=%s"
        ),
        *ConvoyID.ToString(),
        *DestinationSectorID.ToString(),
        IsValid(TravelRoute)
            ? *TravelRoute->GetRouteID().ToString()
            : TEXT("Direct")
    );

    Destroy();
}

bool ABHSupplyConvoyTarget::ResolveCommittedEscort(
    bool bConvoySurvived
)
{
    UWorld* World = GetWorld();
    UGameInstance* GameInstance = IsValid(World)
        ? World->GetGameInstance()
        : nullptr;
    UBHWarSubsystem* WarSubsystem = IsValid(GameInstance)
        ? GameInstance->GetSubsystem<UBHWarSubsystem>()
        : nullptr;

    if (!IsValid(WarSubsystem) ||
        !WarSubsystem->DoesConvoyMatchCommittedEscort(ConvoyID))
    {
        return false;
    }

    for (TActorIterator<ABHCharacter> It(World); It; ++It)
    {
        ABHCharacter* AssignedCharacter = *It;

        if (!IsValid(AssignedCharacter) ||
            !AssignedCharacter->IsRuntimeWarOperation() ||
            AssignedCharacter->GetAssignedWarSectorID() !=
                DestinationSectorID ||
            AssignedCharacter->GetAssignedWarPriorityType() !=
                EBHWarPriorityType::EscortRescue)
        {
            continue;
        }

        const bool bResolvedOperation = bConvoySurvived
            ? AssignedCharacter->CompleteObjective(
                BHObjectiveIds::ProtectConvoy
            )
            : AssignedCharacter->FailCurrentWarOperation(
                NSLOCTEXT(
                    "BrokenHorizon",
                    "EscortConvoyDestroyedFailureReason",
                    "The protected convoy was destroyed before "
                    "it cleared the route."
                )
            );

        if (bResolvedOperation)
        {
            UE_LOG(
                LogTemp,
                Display,
                TEXT(
                    "BH_ESCORT_OPERATION_RESOLVED convoy=%s "
                    "survived=%d destination=%s"
                ),
                *ConvoyID.ToString(),
                bConvoySurvived ? 1 : 0,
                *DestinationSectorID.ToString()
            );
        }

        return bResolvedOperation;
    }

    return false;
}

void ABHSupplyConvoyTarget::NotifyPlayer(
    const FText& Message
) const
{
    if (Message.IsEmpty())
    {
        return;
    }

    if (ABHCharacter* PlayerCharacter =
        BHPlayerResolver::Find(this))
    {
        PlayerCharacter->ShowStatusNotification(Message);
    }
}
