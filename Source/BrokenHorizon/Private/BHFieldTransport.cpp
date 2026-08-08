#include "BHFieldTransport.h"

#include "BHCharacter.h"
#include "BHBattlefieldConditions.h"
#include "BHMissionData.h"
#include "BHSaveSubsystem.h"
#include "BHSectorResupplyStation.h"
#include "BHWarSubsystem.h"
#include "Camera/CameraComponent.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/GameInstance.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "InputCoreTypes.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

ABHFieldTransport::ABHFieldTransport()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bTickEvenWhenPaused = true;
    SetReplicates(true);
    SetReplicateMovement(true);
    SetCanBeDamaged(true);
    SetNetUpdateFrequency(10.0f);
    SetMinNetUpdateFrequency(2.0f);

    VehicleCollision = CreateDefaultSubobject<UBoxComponent>(
        TEXT("VehicleCollision")
    );
    SetRootComponent(VehicleCollision);
    VehicleCollision->SetBoxExtent(FVector(220.0f, 105.0f, 60.0f));
    VehicleCollision->SetCollisionProfileName(
        UCollisionProfile::BlockAllDynamic_ProfileName
    );

    static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(
        TEXT("/Engine/BasicShapes/Cube.Cube")
    );
    static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderFinder(
        TEXT("/Engine/BasicShapes/Cylinder.Cylinder")
    );

    ChassisMesh = CreateDefaultSubobject<UStaticMeshComponent>(
        TEXT("ChassisMesh")
    );
    ChassisMesh->SetupAttachment(VehicleCollision);
    ChassisMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    ChassisMesh->SetRelativeScale3D(FVector(4.4f, 2.1f, 0.7f));

    CabMesh = CreateDefaultSubobject<UStaticMeshComponent>(
        TEXT("CabMesh")
    );
    CabMesh->SetupAttachment(VehicleCollision);
    CabMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    CabMesh->SetRelativeLocation(FVector(-55.0f, 0.0f, 72.0f));
    CabMesh->SetRelativeScale3D(FVector(1.8f, 2.0f, 0.9f));

    HoodMesh = CreateDefaultSubobject<UStaticMeshComponent>(
        TEXT("HoodMesh")
    );
    HoodMesh->SetupAttachment(VehicleCollision);
    HoodMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    HoodMesh->SetRelativeLocation(FVector(110.0f, 0.0f, 48.0f));
    HoodMesh->SetRelativeScale3D(FVector(1.8f, 1.9f, 0.35f));

    if (CubeFinder.Succeeded())
    {
        ChassisMesh->SetStaticMesh(CubeFinder.Object);
        CabMesh->SetStaticMesh(CubeFinder.Object);
        HoodMesh->SetStaticMesh(CubeFinder.Object);
    }

    const FVector WheelLocations[] = {
        FVector(140.0f, -112.0f, -42.0f),
        FVector(140.0f, 112.0f, -42.0f),
        FVector(-140.0f, -112.0f, -42.0f),
        FVector(-140.0f, 112.0f, -42.0f)
    };

    for (int32 WheelIndex = 0; WheelIndex < 4; ++WheelIndex)
    {
        UStaticMeshComponent* Wheel =
            CreateDefaultSubobject<UStaticMeshComponent>(
                *FString::Printf(
                    TEXT("Wheel%d"),
                    WheelIndex + 1
                )
            );
        Wheel->SetupAttachment(VehicleCollision);
        Wheel->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Wheel->SetRelativeLocation(WheelLocations[WheelIndex]);
        Wheel->SetRelativeRotation(FRotator(90.0f, 0.0f, 0.0f));
        Wheel->SetRelativeScale3D(FVector(0.55f, 0.55f, 0.32f));

        if (CylinderFinder.Succeeded())
        {
            Wheel->SetStaticMesh(CylinderFinder.Object);
        }

        WheelMeshes.Add(Wheel);
    }

    CameraBoom = CreateDefaultSubobject<USpringArmComponent>(
        TEXT("CameraBoom")
    );
    CameraBoom->SetupAttachment(VehicleCollision);
    CameraBoom->SetRelativeLocation(FVector(0.0f, 0.0f, 135.0f));
    CameraBoom->TargetArmLength = 620.0f;
    CameraBoom->bUsePawnControlRotation = true;
    CameraBoom->bEnableCameraLag = true;
    CameraBoom->CameraLagSpeed = 8.0f;

    VehicleCamera = CreateDefaultSubobject<UCameraComponent>(
        TEXT("VehicleCamera")
    );
    VehicleCamera->SetupAttachment(
        CameraBoom,
        USpringArmComponent::SocketName
    );
    VehicleCamera->bUsePawnControlRotation = false;

    TransportLabel = CreateDefaultSubobject<UTextRenderComponent>(
        TEXT("TransportLabel")
    );
    TransportLabel->SetupAttachment(VehicleCollision);
    TransportLabel->SetRelativeLocation(
        FVector(0.0f, 0.0f, 190.0f)
    );
    TransportLabel->SetRelativeRotation(
        FRotator(0.0f, 90.0f, 0.0f)
    );
    TransportLabel->SetText(
        FText::FromString(TEXT("FIELD TRANSPORT"))
    );
    TransportLabel->SetTextRenderColor(
        FColor(245, 174, 48, 255)
    );
    TransportLabel->SetWorldSize(34.0f);
    TransportLabel->SetHorizontalAlignment(
        EHorizTextAligment::EHTA_Center
    );
    TransportLabel->SetCullDistance(18000.0f);

    bUseControllerRotationPitch = false;
    bUseControllerRotationYaw = false;
    bUseControllerRotationRoll = false;
}

float ABHFieldTransport::CalculateHullMobilityMultiplier(
    float HullFraction,
    float CriticalHullFraction,
    float MinimumMobilityMultiplier
)
{
    const float ClampedHullFraction = FMath::Clamp(
        HullFraction,
        0.0f,
        1.0f
    );
    const float ClampedCriticalFraction = FMath::Clamp(
        CriticalHullFraction,
        KINDA_SMALL_NUMBER,
        1.0f
    );
    const float ClampedMinimumMultiplier = FMath::Clamp(
        MinimumMobilityMultiplier,
        0.0f,
        1.0f
    );

    if (ClampedHullFraction >= ClampedCriticalFraction)
    {
        return 1.0f;
    }

    const float DamageAlpha = FMath::Clamp(
        1.0f -
            ClampedHullFraction / ClampedCriticalFraction,
        0.0f,
        1.0f
    );
    return FMath::Lerp(
        1.0f,
        ClampedMinimumMultiplier,
        DamageAlpha
    );
}

float ABHFieldTransport::CalculateHullFuelBurnMultiplier(
    float HullFraction,
    float CriticalHullFraction,
    float CriticalFuelBurnMultiplier
)
{
    const float ClampedHullFraction = FMath::Clamp(
        HullFraction,
        0.0f,
        1.0f
    );
    const float ClampedCriticalFraction = FMath::Clamp(
        CriticalHullFraction,
        KINDA_SMALL_NUMBER,
        1.0f
    );
    const float ClampedCriticalMultiplier = FMath::Max(
        1.0f,
        CriticalFuelBurnMultiplier
    );

    if (ClampedHullFraction >= ClampedCriticalFraction)
    {
        return 1.0f;
    }

    const float DamageAlpha = FMath::Clamp(
        1.0f -
            ClampedHullFraction / ClampedCriticalFraction,
        0.0f,
        1.0f
    );
    return FMath::Lerp(
        1.0f,
        ClampedCriticalMultiplier,
        DamageAlpha
    );
}

void ABHFieldTransport::GetLifetimeReplicatedProps(
    TArray<FLifetimeProperty>& OutLifetimeProps
) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ABHFieldTransport, Occupant);
    DOREPLIFETIME_CONDITION(
        ABHFieldTransport,
        CurrentSpeed,
        COND_OwnerOnly
    );
    DOREPLIFETIME(ABHFieldTransport, CurrentFuel);
    DOREPLIFETIME(ABHFieldTransport, CurrentHull);
    DOREPLIFETIME(ABHFieldTransport, CurrentCargoSupply);
    DOREPLIFETIME(ABHFieldTransport, CargoSourceSectorID);
    DOREPLIFETIME(
        ABHFieldTransport,
        CargoDestinationSectorID
    );
    DOREPLIFETIME(ABHFieldTransport, CargoType);
}

void ABHFieldTransport::BeginPlay()
{
    Super::BeginPlay();
    CurrentFuel = FMath::Clamp(
        CurrentFuel,
        0.0f,
        FMath::Max(1.0f, MaximumFuel)
    );
    CurrentHull = FMath::Clamp(
        CurrentHull,
        0.0f,
        FMath::Max(1.0f, MaximumHull)
    );
    CurrentCargoSupply = FMath::Clamp(
        CurrentCargoSupply,
        0.0f,
        FMath::Max(1.0f, MaximumCargoSupply)
    );

    if (CurrentCargoSupply <= KINDA_SMALL_NUMBER)
    {
        CargoSourceSectorID = NAME_None;
        CargoDestinationSectorID = NAME_None;
        CargoType = EBHWarConvoyCargoType::MilitarySupply;
    }

    UpdateTransportLabel();
    UpdateGroundPosition(1.0f);
}

void ABHFieldTransport::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (IsLocallyControlled())
    {
        UpdateDriverInput(DeltaTime);
    }

    if (!HasAuthority())
    {
        return;
    }

    if (!IsLocallyControlled())
    {
        DriverInputStaleSeconds += DeltaTime;

        if (DriverInputStaleSeconds > 0.35f)
        {
            ThrottleInput = 0.0f;
            SteeringInput = 0.0f;
            bBrakeInput = false;
            bBoostInput = false;
        }
    }

    UpdateMovement(DeltaTime);
    UpdateGroundPosition(DeltaTime);
}

float ABHFieldTransport::CalculatePassengerDamage(
    float VehicleDamage,
    float CrewFraction,
    float PassengerMultiplier
)
{
    return FMath::Max(0.0f, VehicleDamage) *
        FMath::Clamp(CrewFraction, 0.0f, 1.0f) *
        FMath::Clamp(PassengerMultiplier, 0.0f, 1.0f);
}

bool ABHFieldTransport::ShouldRestoreFieldSquadPassengers(
    bool bRestoreDriver,
    bool bHasExplicitPassengerState,
    bool bSavedFieldSquadEmbarked,
    FName SavedFieldSquadTransportID,
    FName CandidateTransportID
)
{
    if (!bRestoreDriver)
    {
        return false;
    }

    if (!bHasExplicitPassengerState)
    {
        return true;
    }

    return
        bSavedFieldSquadEmbarked &&
        !SavedFieldSquadTransportID.IsNone() &&
        SavedFieldSquadTransportID == CandidateTransportID;
}

float ABHFieldTransport::TakeDamage(
    float DamageAmount,
    FDamageEvent const& DamageEvent,
    AController* EventInstigator,
    AActor* DamageCauser
)
{
    if (!HasAuthority() ||
        DamageAmount <= 0.0f ||
        CurrentHull <= 0.0f)
    {
        return 0.0f;
    }

    CurrentHull = FMath::Max(0.0f, CurrentHull - DamageAmount);
    UpdateTransportLabel();
    ForceNetUpdate();

    if (CurrentHull <= 0.0f)
    {
        CurrentSpeed = 0.0f;
        ThrottleInput = 0.0f;
        bBoostInput = false;

        if (!bHullDisabledWarningIssued)
        {
            bHullDisabledWarningIssued = true;
            NotifyDriver(
                TEXT(
                    "FIELD TRANSPORT DISABLED // "
                    "REPAIR AT A FRIENDLY RESUPPLY POINT"
                )
            );
        }
    }

    const float CrewDamage = DamageAmount * FMath::Clamp(
        CrewDamageFraction,
        0.0f,
        1.0f
    );

    if (IsValid(Occupant) && CrewDamage > 0.0f)
    {
        Occupant->TakeDamage(
            CrewDamage,
            DamageEvent,
            EventInstigator,
            DamageCauser
        );
        Occupant->ApplyFieldSquadTransportDamage(
            CalculatePassengerDamage(
                DamageAmount,
                CrewDamageFraction,
                PassengerDamageMultiplier
            ),
            DamageEvent,
            EventInstigator,
            DamageCauser
        );
    }

    return DamageAmount;
}

FName ABHFieldTransport::GetPersistenceID() const
{
    return PersistenceID;
}

#if !UE_BUILD_SHIPPING
void ABHFieldTransport::SetPersistenceIDForTesting(FName InPersistenceID)
{
    PersistenceID = InPersistenceID;
}
#endif

ABHCharacter* ABHFieldTransport::GetOccupant() const
{
    return Occupant;
}

void ABHFieldTransport::ForceExitOccupantForRespawn(
    ABHCharacter* Character
)
{
    if (HasAuthority() &&
        IsValid(Character) &&
        Occupant == Character)
    {
        ExitVehicle();
    }
}

void ABHFieldTransport::PrepareOccupantForServerTravel(
    ABHCharacter* Character
)
{
    if (!HasAuthority() ||
        !IsValid(Character) ||
        Occupant != Character)
    {
        return;
    }

    APlayerController* PlayerController =
        Cast<APlayerController>(GetController());

    if (!IsValid(PlayerController))
    {
        return;
    }

    Character->DisembarkFieldSquadTransport(this);
    Character->DetachFromActor(
        FDetachmentTransformRules::KeepWorldTransform
    );
    Character->SetActorLocation(
        GetActorLocation() + FVector(0.0f, 0.0f, 150.0f),
        false,
        nullptr,
        ETeleportType::TeleportPhysics
    );
    Character->SetActorRotation(GetActorRotation());
    Character->SetActorEnableCollision(true);
    Character->SetActorHiddenInGame(false);
    PlayerController->Possess(Character);
    SetOwner(nullptr);
    Occupant = nullptr;
    ForceNetUpdate();

    UE_LOG(
        LogTemp,
        Display,
        TEXT(
            "BH_FIELD_TRANSPORT_TRAVEL_HANDOFF transport=%s "
            "driver=%s"
        ),
        *GetName(),
        *GetNameSafe(Character)
    );
}

float ABHFieldTransport::GetFuelPercentage() const
{
    return FMath::Clamp(
        CurrentFuel / FMath::Max(1.0f, MaximumFuel),
        0.0f,
        1.0f
    );
}

float ABHFieldTransport::GetHullPercentage() const
{
    return FMath::Clamp(
        CurrentHull / FMath::Max(1.0f, MaximumHull),
        0.0f,
        1.0f
    );
}

float ABHFieldTransport::GetSpeedKPH() const
{
    return FMath::Abs(CurrentSpeed) * 0.036f;
}

float ABHFieldTransport::GetEstimatedRangeKilometers() const
{
    return CurrentFuel /
        (
            FMath::Max(0.01f, FuelBurnPerKilometer) *
            GetCargoFuelBurnMultiplier()
        );
}

float ABHFieldTransport::GetEstimatedTravelMinutes(
    float DistanceCentimeters
) const
{
    const float DistanceKilometers =
        FMath::Max(0.0f, DistanceCentimeters) / 100000.0f;
    const float ConservativeCruiseSpeedKPH =
        FMath::Max(
            1.0f,
            MaximumForwardSpeed * 0.036f * 0.70f
        ) * GetCargoSpeedMultiplier();

    return (DistanceKilometers / ConservativeCruiseSpeedKPH) *
        60.0f;
}

float ABHFieldTransport::GetCargoLoadFraction() const
{
    return FMath::Clamp(
        CurrentCargoSupply /
            FMath::Max(1.0f, MaximumCargoSupply),
        0.0f,
        1.0f
    );
}

float ABHFieldTransport::GetCargoSpeedMultiplier() const
{
    return FMath::Clamp(
        FMath::Lerp(
            1.0f,
            LoadedSpeedMultiplierAtCapacity,
            GetCargoLoadFraction()
        ),
        0.1f,
        1.0f
    );
}

float ABHFieldTransport::GetCargoFuelBurnMultiplier() const
{
    return FMath::Max(
        1.0f,
        FMath::Lerp(
            1.0f,
            LoadedFuelBurnMultiplierAtCapacity,
            GetCargoLoadFraction()
        )
    );
}

bool ABHFieldTransport::NeedsService() const
{
    return GetFuelPercentage() < 0.999f ||
        GetHullPercentage() < 0.999f;
}

bool ABHFieldTransport::IsImmobilized() const
{
    return CurrentFuel <= KINDA_SMALL_NUMBER ||
        CurrentHull <= KINDA_SMALL_NUMBER;
}

float ABHFieldTransport::GetCargoSupply() const
{
    return FMath::Max(0.0f, CurrentCargoSupply);
}

float ABHFieldTransport::GetCargoCapacity() const
{
    return FMath::Max(1.0f, MaximumCargoSupply);
}

FName ABHFieldTransport::GetCargoSourceSectorID() const
{
    return CargoSourceSectorID;
}

FName ABHFieldTransport::GetCargoDestinationSectorID() const
{
    return CargoDestinationSectorID;
}

EBHWarConvoyCargoType ABHFieldTransport::GetCargoType() const
{
    return CargoType;
}

FName ABHFieldTransport::RefreshMilitaryCargoDestination()
{
    if (!HasAuthority())
    {
        return CargoDestinationSectorID;
    }

    if (CurrentCargoSupply <= KINDA_SMALL_NUMBER ||
        CargoType != EBHWarConvoyCargoType::MilitarySupply)
    {
        return CargoDestinationSectorID;
    }

    UGameInstance* GameInstance = GetGameInstance();
    UBHWarSubsystem* WarSubsystem = IsValid(GameInstance)
        ? GameInstance->GetSubsystem<UBHWarSubsystem>()
        : nullptr;

    if (!IsValid(WarSubsystem))
    {
        return CargoDestinationSectorID;
    }

    const bool bPinnedDestinationCanReceive =
        !CargoDestinationSectorID.IsNone() &&
        CargoDestinationSectorID != CargoSourceSectorID &&
        WarSubsystem->GetFieldLogisticsDeliveryCapacity(
            CargoDestinationSectorID
        ) > KINDA_SMALL_NUMBER;

    if (!bPinnedDestinationCanReceive)
    {
        const FName PreviousDestination =
            CargoDestinationSectorID;
        CargoDestinationSectorID =
            WarSubsystem->GetRecommendedFieldLogisticsDestination(
                CargoSourceSectorID
            );

        if (CargoDestinationSectorID != PreviousDestination)
        {
            ForceNetUpdate();
        }
    }

    return CargoDestinationSectorID;
}

FName ABHFieldTransport::RefreshCivilianAidDestination()
{
    if (!HasAuthority())
    {
        return CargoDestinationSectorID;
    }

    if (CurrentCargoSupply <= KINDA_SMALL_NUMBER ||
        CargoType != EBHWarConvoyCargoType::CivilianAid)
    {
        return CargoDestinationSectorID;
    }

    UGameInstance* GameInstance = GetGameInstance();
    UBHWarSubsystem* WarSubsystem = IsValid(GameInstance)
        ? GameInstance->GetSubsystem<UBHWarSubsystem>()
        : nullptr;

    if (!IsValid(WarSubsystem))
    {
        return CargoDestinationSectorID;
    }

    const FName PreviousDestination = CargoDestinationSectorID;
    CargoDestinationSectorID =
        WarSubsystem->
            GetRecommendedInTransitCivilianAidDestination(
                CargoSourceSectorID,
                CargoDestinationSectorID
            );

    if (CargoDestinationSectorID != PreviousDestination)
    {
        ForceNetUpdate();
    }

    return CargoDestinationSectorID;
}

float ABHFieldTransport::LoadRecoveredMilitarySupply(
    float AvailableSupply,
    FName RecoverySourceSectorID
)
{
    if (!HasAuthority())
    {
        return 0.0f;
    }

    const float SafeAvailableSupply =
        FMath::Max(0.0f, AvailableSupply);

    if (SafeAvailableSupply <= KINDA_SMALL_NUMBER ||
        (
            CurrentCargoSupply > KINDA_SMALL_NUMBER &&
            CargoType == EBHWarConvoyCargoType::CivilianAid
        ))
    {
        return 0.0f;
    }

    const float RemainingCapacity = FMath::Max(
        0.0f,
        GetCargoCapacity() - CurrentCargoSupply
    );
    const float LoadedSupply = FMath::Min(
        SafeAvailableSupply,
        RemainingCapacity
    );

    if (LoadedSupply <= KINDA_SMALL_NUMBER)
    {
        return 0.0f;
    }

    const bool bWasEmpty =
        CurrentCargoSupply <= KINDA_SMALL_NUMBER;
    CurrentCargoSupply += LoadedSupply;
    CargoType = EBHWarConvoyCargoType::MilitarySupply;
    CargoDestinationSectorID = NAME_None;

    if (bWasEmpty)
    {
        CargoSourceSectorID = RecoverySourceSectorID;
    }

    RefreshMilitaryCargoDestination();
    UpdateTransportLabel();
    ForceNetUpdate();

    UE_LOG(
        LogTemp,
        Display,
        TEXT(
            "BH_FIELD_TRANSPORT_SALVAGE_LOADED id=%s "
            "source=%s amount=%.1f cargo=%.1f"
        ),
        *PersistenceID.ToString(),
        *RecoverySourceSectorID.ToString(),
        LoadedSupply,
        CurrentCargoSupply
    );
    return LoadedSupply;
}

bool ABHFieldTransport::ServiceVehicle()
{
    if (!HasAuthority())
    {
        return false;
    }

    const bool bChanged = NeedsService();
    CurrentFuel = FMath::Max(1.0f, MaximumFuel);
    CurrentHull = FMath::Max(1.0f, MaximumHull);
    CurrentSpeed = 0.0f;
    bLowFuelWarningIssued = false;
    bCriticalFuelWarningIssued = false;
    bOutOfFuelWarningIssued = false;
    bHullDisabledWarningIssued = false;
    UpdateTransportLabel();
    ForceNetUpdate();
    return bChanged;
}

bool ABHFieldTransport::RecoverAndService(
    const FTransform& RecoveryTransform
)
{
    if (!HasAuthority() || IsValid(Occupant))
    {
        return false;
    }

    SetActorTransform(
        RecoveryTransform,
        false,
        nullptr,
        ETeleportType::TeleportPhysics
    );
    ServiceVehicle();
    UpdateGroundPosition(1.0f);
    ForceNetUpdate();

    UE_LOG(
        LogTemp,
        Display,
        TEXT(
            "BH_FIELD_TRANSPORT_RECOVERED id=%s location=%s"
        ),
        *PersistenceID.ToString(),
        *GetActorLocation().ToCompactString()
    );
    return true;
}

void ABHFieldTransport::RestorePersistentState(
    const FTransform& SavedTransform,
    ABHCharacter* PlayerCharacter,
    bool bRestoreDriver,
    float SavedFuelFraction,
    float SavedHullFraction,
    float SavedCargoSupply,
    FName SavedCargoSourceSectorID,
    FName SavedCargoDestinationSectorID,
    EBHWarConvoyCargoType SavedCargoType,
    bool bRestoreFieldSquadPassengers,
    bool bUseSavedFieldSquadPassengerManifest
)
{
    if (!HasAuthority())
    {
        return;
    }

    SetActorTransform(
        SavedTransform,
        false,
        nullptr,
        ETeleportType::TeleportPhysics
    );
    CurrentSpeed = 0.0f;
    ThrottleInput = 0.0f;
    SteeringInput = 0.0f;
    bBrakeInput = false;
    bBoostInput = false;
    CurrentFuel = FMath::Clamp(
        SavedFuelFraction,
        0.0f,
        1.0f
    ) * FMath::Max(1.0f, MaximumFuel);
    CurrentHull = FMath::Clamp(
        SavedHullFraction,
        0.0f,
        1.0f
    ) * FMath::Max(1.0f, MaximumHull);
    CurrentCargoSupply = FMath::Clamp(
        SavedCargoSupply,
        0.0f,
        FMath::Max(1.0f, MaximumCargoSupply)
    );
    CargoSourceSectorID =
        CurrentCargoSupply > KINDA_SMALL_NUMBER
            ? SavedCargoSourceSectorID
            : NAME_None;
    CargoDestinationSectorID =
        CurrentCargoSupply > KINDA_SMALL_NUMBER
            ? SavedCargoDestinationSectorID
            : NAME_None;
    CargoType =
        CurrentCargoSupply > KINDA_SMALL_NUMBER
            ? SavedCargoType
            : EBHWarConvoyCargoType::MilitarySupply;
    RefreshMilitaryCargoDestination();
    RefreshCivilianAidDestination();
    bLowFuelWarningIssued = GetFuelPercentage() <= 0.25f;
    bCriticalFuelWarningIssued = GetFuelPercentage() <= 0.10f;
    bOutOfFuelWarningIssued = CurrentFuel <= KINDA_SMALL_NUMBER;
    bHullDisabledWarningIssued = CurrentHull <= KINDA_SMALL_NUMBER;
    UpdateTransportLabel();
    UpdateGroundPosition(1.0f);
    ForceNetUpdate();

    if (bRestoreDriver &&
        IsValid(PlayerCharacter) &&
        !IsValid(Occupant))
    {
        EnterVehicle(
            PlayerCharacter,
            bRestoreFieldSquadPassengers,
            bUseSavedFieldSquadPassengerManifest
        );
    }
}

void ABHFieldTransport::Interact_Implementation(
    AActor* InteractingActor
)
{
    if (!HasAuthority() || IsValid(Occupant))
    {
        return;
    }

    EnterVehicle(Cast<ABHCharacter>(InteractingActor));
}

FText ABHFieldTransport::GetInteractionText_Implementation() const
{
    return IsValid(Occupant)
        ? FText::FromString(TEXT("Field transport occupied"))
        : FText::FromString(TEXT("Press [F] to drive field transport"));
}

void ABHFieldTransport::EnterVehicle(
    ABHCharacter* Character,
    bool bBoardFieldSquad,
    bool bUseSavedFieldSquadPassengerManifest
)
{
    if (!IsValid(Character))
    {
        return;
    }

    APlayerController* PlayerController =
        Cast<APlayerController>(Character->GetController());

    if (!IsValid(PlayerController))
    {
        return;
    }

    Occupant = Character;
    ForceNetUpdate();
    bExitInputArmed = false;
    DriverInputStaleSeconds = 0.0f;
    Character->SetActorEnableCollision(false);
    Character->SetActorHiddenInGame(true);
    Character->AttachToActor(
        this,
        FAttachmentTransformRules::KeepWorldTransform
    );
    Character->SetActorRelativeLocation(
        FVector(-55.0f, 0.0f, 55.0f)
    );
    const int32 FireteamPassengers =
        bBoardFieldSquad
            ? Character->BoardFieldSquadTransport(
                this,
                bUseSavedFieldSquadPassengerManifest
            )
            : 0;
    SetOwner(PlayerController);
    PlayerController->Possess(this);
    ClientActivateDriverControls();
    PlayerController->SetControlRotation(
        GetActorRotation() + FRotator(-10.0f, 0.0f, 0.0f)
    );
    PlayerController->ClientMessage(
        *FString::Printf(
            TEXT(
                "FIELD TRANSPORT // FUEL %.0f%% // HULL %.0f%% "
                "// CARGO %.0f/%.0f "
                "// WASD DRIVE // SHIFT BOOST // SPACE BRAKE "
                "// FIRETEAM %d ABOARD "
                "// X LOGISTICS // V CIVILIAN AID "
                "// F OR E EXIT "
                "// M COMMAND // ESC PAUSE"
            ),
            GetFuelPercentage() * 100.0f,
            GetHullPercentage() * 100.0f,
            GetCargoSupply(),
            GetCargoCapacity(),
            FireteamPassengers
        )
    );

    UE_LOG(
        LogTemp,
        Display,
        TEXT(
            "BH_FIELD_TRANSPORT_DRIVER_ENTERED transport=%s "
            "driver=%s local=%d authority=%d fuel=%.1f hull=%.1f"
        ),
        *GetName(),
        *Character->GetName(),
        IsLocallyControlled() ? 1 : 0,
        HasAuthority() ? 1 : 0,
        CurrentFuel,
        CurrentHull
    );
}

void ABHFieldTransport::ExitVehicle()
{
    APlayerController* PlayerController =
        Cast<APlayerController>(GetController());
    ABHCharacter* Character = Occupant;

    if (!IsValid(PlayerController) || !IsValid(Character))
    {
        return;
    }

    FVector ExitLocation = FVector::ZeroVector;
    FRotator ExitRotation = FRotator::ZeroRotator;
    Character->SetActorEnableCollision(true);

    if (!FindSafeExitLocation(
            Character,
            ExitLocation,
            ExitRotation))
    {
        Character->SetActorEnableCollision(false);
        NotifyDriver(
            TEXT(
                "EXIT BLOCKED // MOVE THE VEHICLE CLEAR "
                "OF OBSTRUCTIONS"
            )
        );
        return;
    }

    Character->DetachFromActor(
        FDetachmentTransformRules::KeepWorldTransform
    );
    Character->SetActorLocation(
        ExitLocation,
        false,
        nullptr,
        ETeleportType::TeleportPhysics
    );
    Character->SetActorRotation(ExitRotation);
    Character->SetActorHiddenInGame(false);
    Character->DisembarkFieldSquadTransport(this);
    Occupant = nullptr;
    SetOwner(nullptr);
    ForceNetUpdate();
    PlayerController->Possess(Character);
    bExitInputArmed = false;
    ThrottleInput = 0.0f;
    SteeringInput = 0.0f;
    bBrakeInput = false;
    bBoostInput = false;
    DriverInputStaleSeconds = 0.0f;
}

bool ABHFieldTransport::FindSafeExitLocation(
    ABHCharacter* Character,
    FVector& OutExitLocation,
    FRotator& OutExitRotation
) const
{
    UWorld* World = GetWorld();

    if (!IsValid(World) || !IsValid(Character))
    {
        return false;
    }

    const FVector Origin =
        GetActorLocation() + FVector(0.0f, 0.0f, 80.0f);
    const FVector Forward = GetActorForwardVector();
    const FVector Right = GetActorRightVector();
    const TArray<FVector> ExitOffsets = {
        Right * 285.0f,
        Right * -285.0f,
        Forward * -340.0f,
        Forward * 340.0f,
        (Right * 250.0f) + (Forward * -250.0f),
        (Right * -250.0f) + (Forward * -250.0f)
    };
    const FRotator CandidateRotation(
        0.0f,
        GetActorRotation().Yaw,
        0.0f
    );

    for (const FVector& Offset : ExitOffsets)
    {
        FVector CandidateLocation = Origin + Offset;

        if (World->FindTeleportSpot(
                Character,
                CandidateLocation,
                CandidateRotation))
        {
            OutExitLocation = CandidateLocation;
            OutExitRotation = CandidateRotation;
            return true;
        }
    }

    return false;
}

void ABHFieldTransport::UpdateDriverInput(float DeltaTime)
{
    (void)DeltaTime;
    ThrottleInput = 0.0f;
    SteeringInput = 0.0f;
    bBrakeInput = false;
    bBoostInput = false;

    APlayerController* PlayerController =
        Cast<APlayerController>(GetController());

    if (!IsValid(PlayerController) || !IsValid(Occupant))
    {
        return;
    }

    if (PlayerController->WasInputKeyJustPressed(EKeys::Escape))
    {
        Occupant->TogglePauseMenu();
        return;
    }

    if (PlayerController->WasInputKeyJustPressed(EKeys::M))
    {
        Occupant->ToggleWarMap();
        return;
    }

    if (PlayerController->WasInputKeyJustPressed(EKeys::X))
    {
        if (HasAuthority())
        {
            TryTransferFieldLogistics();
        }
        else
        {
            ServerRequestFieldLogisticsTransfer();
        }
        return;
    }

    if (PlayerController->WasInputKeyJustPressed(EKeys::V))
    {
        if (HasAuthority())
        {
            TryTransferCivilianAid();
        }
        else
        {
            ServerRequestCivilianAidTransfer();
        }
        return;
    }

    if (PlayerController->IsMoveInputIgnored() ||
        PlayerController->IsLookInputIgnored())
    {
        return;
    }

    ThrottleInput +=
        PlayerController->IsInputKeyDown(EKeys::W) ? 1.0f : 0.0f;
    ThrottleInput -=
        PlayerController->IsInputKeyDown(EKeys::S) ? 1.0f : 0.0f;
    SteeringInput +=
        PlayerController->IsInputKeyDown(EKeys::D) ? 1.0f : 0.0f;
    SteeringInput -=
        PlayerController->IsInputKeyDown(EKeys::A) ? 1.0f : 0.0f;
    bBrakeInput =
        PlayerController->IsInputKeyDown(EKeys::SpaceBar);
    bBoostInput =
        PlayerController->IsInputKeyDown(EKeys::LeftShift);

    float MouseX = 0.0f;
    float MouseY = 0.0f;
    PlayerController->GetInputMouseDelta(MouseX, MouseY);
    PlayerController->AddYawInput(MouseX);
    PlayerController->AddPitchInput(MouseY * -0.75f);

    const bool bExitKeyDown =
        PlayerController->IsInputKeyDown(EKeys::F) ||
        PlayerController->IsInputKeyDown(EKeys::E);

    if (!bExitKeyDown)
    {
        bExitInputArmed = true;
    }
    else if (
        bExitInputArmed &&
        (
            PlayerController->WasInputKeyJustPressed(EKeys::F) ||
            PlayerController->WasInputKeyJustPressed(EKeys::E)
        )
    )
    {
        if (FMath::Abs(CurrentSpeed) >
            FMath::Max(0.0f, MaximumSafeExitSpeed))
        {
            bBrakeInput = true;
            PlayerController->ClientMessage(
                TEXT("SLOW DOWN BEFORE EXITING")
            );
            return;
        }

        if (HasAuthority())
        {
            ExitVehicle();
        }
        else
        {
            ServerRequestExitVehicle();
        }
        return;
    }

    if (!HasAuthority())
    {
        ServerSubmitDriverInput(
            ThrottleInput,
            SteeringInput,
            bBrakeInput,
            bBoostInput
        );
    }
}

void ABHFieldTransport::ServerSubmitDriverInput_Implementation(
    float NewThrottleInput,
    float NewSteeringInput,
    bool bNewBrakeInput,
    bool bNewBoostInput
)
{
    if (!IsValid(Occupant) ||
        !IsValid(GetController()))
    {
        return;
    }

    ThrottleInput = FMath::Clamp(
        NewThrottleInput,
        -1.0f,
        1.0f
    );
    SteeringInput = FMath::Clamp(
        NewSteeringInput,
        -1.0f,
        1.0f
    );
    bBrakeInput = bNewBrakeInput;
    bBoostInput = bNewBoostInput;
    DriverInputStaleSeconds = 0.0f;
}

void ABHFieldTransport::ServerRequestExitVehicle_Implementation()
{
    if (IsValid(Occupant) && IsValid(GetController()))
    {
        ExitVehicle();
    }
}

void ABHFieldTransport::
    ServerRequestFieldLogisticsTransfer_Implementation()
{
    if (IsValid(Occupant) && IsValid(GetController()))
    {
        TryTransferFieldLogistics();
    }
}

void ABHFieldTransport::
    ServerRequestCivilianAidTransfer_Implementation()
{
    if (IsValid(Occupant) && IsValid(GetController()))
    {
        TryTransferCivilianAid();
    }
}

void ABHFieldTransport::ClientActivateDriverControls_Implementation()
{
    APlayerController* PlayerController =
        Cast<APlayerController>(GetController());

    if (!IsValid(PlayerController) ||
        !PlayerController->IsLocalController())
    {
        return;
    }

    PlayerController->ResetIgnoreMoveInput();
    PlayerController->ResetIgnoreLookInput();
    PlayerController->SetInputMode(FInputModeGameOnly());
    PlayerController->bShowMouseCursor = false;
    PlayerController->FlushPressedKeys();

    UE_LOG(
        LogTemp,
        Display,
        TEXT(
            "BH_FIELD_TRANSPORT_DRIVER_CONTROLS_ACTIVE "
            "transport=%s controller=%s"
        ),
        *GetName(),
        *PlayerController->GetName()
    );
}

void ABHFieldTransport::TryTransferFieldLogistics()
{
    if (!IsValid(Occupant))
    {
        return;
    }

    if (CurrentCargoSupply > KINDA_SMALL_NUMBER &&
        CargoType == EBHWarConvoyCargoType::CivilianAid)
    {
        NotifyDriver(
            TEXT(
                "CARGO RESERVED FOR CIVILIAN AID // "
                "PRESS V AT THE ASSIGNED COMMUNITY"
            )
        );
        return;
    }

    if (FMath::Abs(CurrentSpeed) > 50.0f)
    {
        NotifyDriver(
            TEXT("LOGISTICS TRANSFER BLOCKED // STOP THE VEHICLE")
        );
        return;
    }

    UWorld* World = GetWorld();
    ABHSectorResupplyStation* NearestStation = nullptr;
    float NearestDistanceSquared =
        FMath::Square(FMath::Max(100.0f, LogisticsStationRadius));

    if (IsValid(World))
    {
        for (TActorIterator<ABHSectorResupplyStation> It(World);
            It;
            ++It)
        {
            ABHSectorResupplyStation* Candidate = *It;

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
                NearestStation = Candidate;
                NearestDistanceSquared = DistanceSquared;
            }
        }
    }

    if (!IsValid(NearestStation))
    {
        NotifyDriver(
            TEXT(
                "NO LOGISTICS POINT IN RANGE // "
                "PARK BESIDE A FRIENDLY RESUPPLY STATION"
            )
        );
        return;
    }

    UGameInstance* GameInstance = World->GetGameInstance();
    UBHWarSubsystem* WarSubsystem = IsValid(GameInstance)
        ? GameInstance->GetSubsystem<UBHWarSubsystem>()
        : nullptr;
    UBHSaveSubsystem* SaveSubsystem = IsValid(GameInstance)
        ? GameInstance->GetSubsystem<UBHSaveSubsystem>()
        : nullptr;
    const FName StationSectorID = NearestStation->GetSectorID();
    const FBHWarSectorState StationSector =
        IsValid(WarSubsystem)
            ? WarSubsystem->GetSectorState(StationSectorID)
            : FBHWarSectorState();

    if (!IsValid(WarSubsystem) ||
        StationSector.SectorID.IsNone() ||
        StationSector.Owner != EBHWarFaction::Friendly)
    {
        NotifyDriver(
            TEXT(
                "LOGISTICS TRANSFER BLOCKED // "
                "SECTOR IS NOT UNDER FRIENDLY CONTROL"
            )
        );
        return;
    }

    if (CurrentCargoSupply > KINDA_SMALL_NUMBER)
    {
        const FName DeliveredFromSectorID =
            CargoSourceSectorID;
        const float DeliveredSupply =
            WarSubsystem->DeliverFieldLogisticsSupply(
                DeliveredFromSectorID,
                StationSectorID,
                CurrentCargoSupply
            );

        if (DeliveredSupply <= KINDA_SMALL_NUMBER)
        {
            NotifyDriver(
                TEXT(
                    "DELIVERY BLOCKED // "
                    "DESTINATION SUPPLY IS FULL"
                )
            );
            return;
        }

        CurrentCargoSupply = FMath::Max(
            0.0f,
            CurrentCargoSupply - DeliveredSupply
        );

        if (CurrentCargoSupply <= KINDA_SMALL_NUMBER)
        {
            CurrentCargoSupply = 0.0f;
            CargoSourceSectorID = NAME_None;
            CargoDestinationSectorID = NAME_None;
            CargoType = EBHWarConvoyCargoType::MilitarySupply;
        }
        else
        {
            RefreshMilitaryCargoDestination();
        }

        NotifyDriver(FString::Printf(
            TEXT(
                "FIELD LOGISTICS DELIVERED // %.0f SUPPLY TO %s "
                "// REMAINING %.0f"
            ),
            DeliveredSupply,
            *StationSector.DisplayName.ToString().ToUpper(),
            CurrentCargoSupply
        ));

        bool bCompletedResupplyOperation = false;

        if (WarSubsystem->
            DoesFieldLogisticsDeliveryCompleteOperation(
                DeliveredFromSectorID,
                StationSectorID,
                DeliveredSupply
            ))
        {
            for (TActorIterator<ABHCharacter> It(World); It; ++It)
            {
                ABHCharacter* AssignedCharacter = *It;

                if (IsValid(AssignedCharacter) &&
                    AssignedCharacter->IsRuntimeWarOperation() &&
                    AssignedCharacter->GetAssignedWarSectorID() ==
                        StationSectorID &&
                    AssignedCharacter->GetAssignedWarPriorityType() ==
                        EBHWarPriorityType::Resupply &&
                    AssignedCharacter->GetCurrentObjectiveID() ==
                        BHObjectiveIds::DeliverResupply)
                {
                    bCompletedResupplyOperation =
                        AssignedCharacter->CompleteObjective(
                            BHObjectiveIds::DeliverResupply
                        );
                    break;
                }
            }
        }

        if (bCompletedResupplyOperation)
        {
            NotifyDriver(
                TEXT(
                    "OPERATION LIFELINE COMPLETE // "
                    "SUPPLY LINE RESTORED"
                )
            );
        }
    }
    else
    {
        const float LoadedSupply =
            WarSubsystem->WithdrawFieldLogisticsSupply(
                StationSectorID,
                GetCargoCapacity()
            );

        if (LoadedSupply <= KINDA_SMALL_NUMBER)
        {
            NotifyDriver(
                TEXT(
                    "CARGO UNAVAILABLE // "
                    "SECTOR MUST RETAIN ITS 25 SUPPLY RESERVE"
                )
            );
            return;
        }

        CurrentCargoSupply = LoadedSupply;
        CargoSourceSectorID = StationSectorID;
        CargoType = EBHWarConvoyCargoType::MilitarySupply;
        const FName DestinationSectorID =
            RefreshMilitaryCargoDestination();
        const FBHWarSectorState DestinationSector =
            WarSubsystem->GetSectorState(DestinationSectorID);
        NotifyDriver(FString::Printf(
            TEXT(
                "FIELD LOGISTICS LOADED // %.0f SUPPLY FROM %s "
                "// ROUTE %s"
            ),
            LoadedSupply,
            *StationSector.DisplayName.ToString().ToUpper(),
            DestinationSector.DisplayName.IsEmpty()
                ? TEXT("AWAITING CAPACITY")
                : *DestinationSector.DisplayName
                    .ToString()
                    .ToUpper()
        ));
    }

    UpdateTransportLabel();
    ForceNetUpdate();

    if (IsValid(SaveSubsystem) && !SaveSubsystem->SaveProgress())
    {
        NotifyDriver(
            TEXT("LOGISTICS UPDATED // CHECKPOINT SAVE FAILED")
        );
    }
}

void ABHFieldTransport::TryTransferCivilianAid()
{
    if (!IsValid(Occupant))
    {
        return;
    }

    if (FMath::Abs(CurrentSpeed) > 50.0f)
    {
        NotifyDriver(
            TEXT("AID TRANSFER BLOCKED // STOP THE VEHICLE")
        );
        return;
    }

    UWorld* World = GetWorld();
    ABHSectorResupplyStation* NearestStation = nullptr;
    float NearestDistanceSquared =
        FMath::Square(FMath::Max(100.0f, LogisticsStationRadius));

    if (IsValid(World))
    {
        for (TActorIterator<ABHSectorResupplyStation> It(World);
            It;
            ++It)
        {
            ABHSectorResupplyStation* Candidate = *It;

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
                NearestStation = Candidate;
                NearestDistanceSquared = DistanceSquared;
            }
        }
    }

    if (!IsValid(NearestStation))
    {
        NotifyDriver(
            TEXT(
                "NO COMMUNITY LOGISTICS POINT IN RANGE // "
                "PARK BESIDE A SECTOR RESUPPLY STATION"
            )
        );
        return;
    }

    UGameInstance* GameInstance = World->GetGameInstance();
    UBHWarSubsystem* WarSubsystem = IsValid(GameInstance)
        ? GameInstance->GetSubsystem<UBHWarSubsystem>()
        : nullptr;
    UBHSaveSubsystem* SaveSubsystem = IsValid(GameInstance)
        ? GameInstance->GetSubsystem<UBHSaveSubsystem>()
        : nullptr;
    const FName StationSectorID = NearestStation->GetSectorID();
    const FBHWarSectorState StationSector =
        IsValid(WarSubsystem)
            ? WarSubsystem->GetSectorState(StationSectorID)
            : FBHWarSectorState();

    if (!IsValid(WarSubsystem) ||
        StationSector.SectorID.IsNone())
    {
        NotifyDriver(
            TEXT("AID TRANSFER BLOCKED // UNKNOWN SECTOR")
        );
        return;
    }

    if (CurrentCargoSupply > KINDA_SMALL_NUMBER)
    {
        if (CargoType !=
            EBHWarConvoyCargoType::CivilianAid)
        {
            NotifyDriver(
                TEXT(
                    "MILITARY SUPPLY ALREADY LOADED // "
                    "PRESS X TO DELIVER IT FIRST"
                )
            );
            return;
        }

        const FName PreviousDestinationSectorID =
            CargoDestinationSectorID;
        RefreshCivilianAidDestination();

        if (CargoDestinationSectorID.IsNone())
        {
            NotifyDriver(
                TEXT(
                    "AID ROUTE PAUSED // "
                    "NO COMMUNITY CAN RECEIVE CARGO"
                )
            );
            return;
        }

        if (CargoDestinationSectorID !=
                PreviousDestinationSectorID &&
            StationSectorID != CargoDestinationSectorID)
        {
            const FBHWarSectorState UpdatedDestination =
                WarSubsystem->GetSectorState(
                    CargoDestinationSectorID
                );
            NotifyDriver(FString::Printf(
                TEXT(
                    "AID ROUTE UPDATED // DELIVER TO %s"
                ),
                UpdatedDestination.DisplayName.IsEmpty()
                    ? *CargoDestinationSectorID
                        .ToString()
                        .ToUpper()
                    : *UpdatedDestination.DisplayName
                        .ToString()
                        .ToUpper()
            ));
            UpdateTransportLabel();
            ForceNetUpdate();

            if (IsValid(SaveSubsystem) &&
                !SaveSubsystem->SaveProgress())
            {
                NotifyDriver(
                    TEXT(
                        "AID ROUTE UPDATED // "
                        "CHECKPOINT SAVE FAILED"
                    )
                );
            }
            return;
        }

        if (StationSectorID != CargoDestinationSectorID)
        {
            const FBHWarSectorState Destination =
                WarSubsystem->GetSectorState(
                    CargoDestinationSectorID
                );
            NotifyDriver(FString::Printf(
                TEXT(
                    "AID DESTINATION MISMATCH // DELIVER TO %s"
                ),
                Destination.DisplayName.IsEmpty()
                    ? *CargoDestinationSectorID.ToString().ToUpper()
                    : *Destination.DisplayName.ToString().ToUpper()
            ));
            return;
        }

        if (!WarSubsystem->DeliverFieldCivilianAidSupply(
                CargoSourceSectorID,
                CargoDestinationSectorID,
                CurrentCargoSupply))
        {
            NotifyDriver(
                TEXT(
                    "AID DELIVERY BLOCKED // "
                    "COMMUNITY NETWORK CANNOT RECEIVE CARGO"
                )
            );
            return;
        }

        NotifyDriver(FString::Printf(
            TEXT(
                "CIVILIAN AID DELIVERED // %s // "
                "LOCAL SUPPORT AND INTELLIGENCE IMPROVED"
            ),
            StationSector.DisplayName.IsEmpty()
                ? *StationSectorID.ToString().ToUpper()
                : *StationSector.DisplayName.ToString().ToUpper()
        ));
        CurrentCargoSupply = 0.0f;
        CargoSourceSectorID = NAME_None;
        CargoDestinationSectorID = NAME_None;
        CargoType = EBHWarConvoyCargoType::MilitarySupply;
    }
    else
    {
        if (StationSector.Owner != EBHWarFaction::Friendly)
        {
            NotifyDriver(
                TEXT(
                    "AID LOADING BLOCKED // "
                    "START AT A FRIENDLY CONNECTED SECTOR"
                )
            );
            return;
        }

        const FName DestinationSectorID =
            WarSubsystem->
                GetRecommendedFieldCivilianAidDestination(
                    StationSectorID
                );

        if (DestinationSectorID.IsNone())
        {
            NotifyDriver(
                TEXT(
                    "NO CIVILIAN AID REQUEST // "
                    "SUPPLY OR COMMUNITY NEED IS INSUFFICIENT"
                )
            );
            return;
        }

        const float LoadedAid =
            WarSubsystem->WithdrawFieldCivilianAidSupply(
                StationSectorID,
                DestinationSectorID
            );

        if (LoadedAid <= KINDA_SMALL_NUMBER)
        {
            NotifyDriver(
                TEXT(
                    "AID CARGO UNAVAILABLE // "
                    "FRIENDLY NETWORK REQUIRES 10 SUPPLY"
                )
            );
            return;
        }

        CurrentCargoSupply = LoadedAid;
        CargoSourceSectorID = StationSectorID;
        CargoDestinationSectorID = DestinationSectorID;
        CargoType = EBHWarConvoyCargoType::CivilianAid;
        const FBHWarSectorState Destination =
            WarSubsystem->GetSectorState(DestinationSectorID);
        NotifyDriver(FString::Printf(
            TEXT(
                "CIVILIAN AID LOADED // %.0f SUPPLY // "
                "DELIVER TO %s // HUD ROUTE ACTIVE"
            ),
            LoadedAid,
            Destination.DisplayName.IsEmpty()
                ? *DestinationSectorID.ToString().ToUpper()
                : *Destination.DisplayName.ToString().ToUpper()
        ));
    }

    UpdateTransportLabel();
    ForceNetUpdate();

    if (IsValid(SaveSubsystem) && !SaveSubsystem->SaveProgress())
    {
        NotifyDriver(
            TEXT("CIVILIAN AID UPDATED // CHECKPOINT SAVE FAILED")
        );
    }
}

void ABHFieldTransport::UpdateMovement(float DeltaTime)
{
    if (CurrentHull <= KINDA_SMALL_NUMBER ||
        CurrentFuel <= KINDA_SMALL_NUMBER)
    {
        CurrentSpeed = FMath::FInterpTo(
            CurrentSpeed,
            0.0f,
            DeltaTime,
            AccelerationResponse * 3.0f
        );

        if (CurrentFuel <= KINDA_SMALL_NUMBER &&
            !bOutOfFuelWarningIssued)
        {
            bOutOfFuelWarningIssued = true;
            NotifyDriver(
                TEXT(
                    "FIELD TRANSPORT OUT OF FUEL // "
                    "SERVICE AT A FRIENDLY RESUPPLY POINT"
                )
            );
        }

        return;
    }

    const FBHBattlefieldConditionProfile Conditions =
        UBHBattlefieldConditions::GetCurrentProfile(this);
    const float ForwardLimit =
        (bBoostInput ? BoostForwardSpeed : MaximumForwardSpeed) *
        Conditions.VehicleTractionMultiplier *
        GetCargoSpeedMultiplier() *
        GetHullMobilityMultiplier();
    float TargetSpeed = 0.0f;

    if (ThrottleInput > 0.0f)
    {
        TargetSpeed = ForwardLimit * ThrottleInput;
    }
    else if (ThrottleInput < 0.0f)
    {
        TargetSpeed = MaximumReverseSpeed *
            GetCargoSpeedMultiplier() *
            GetHullMobilityMultiplier() *
            ThrottleInput;
    }

    const float Response = bBrakeInput
        ? AccelerationResponse * 4.0f
        : AccelerationResponse;
    CurrentSpeed = FMath::FInterpTo(
        CurrentSpeed,
        bBrakeInput ? 0.0f : TargetSpeed,
        DeltaTime,
        Response
    );

    if (FMath::Abs(CurrentSpeed) < 2.0f)
    {
        CurrentSpeed = 0.0f;
    }

    const float SpeedRatio = FMath::Clamp(
        FMath::Abs(CurrentSpeed) /
            FMath::Max(1.0f, MaximumForwardSpeed),
        0.0f,
        1.0f
    );

    if (!FMath::IsNearlyZero(SteeringInput) &&
        SpeedRatio > 0.01f)
    {
        const float DirectionSign =
            CurrentSpeed >= 0.0f ? 1.0f : -1.0f;
        AddActorWorldRotation(
            FRotator(
                0.0f,
                SteeringInput *
                    SteeringRate *
                    DirectionSign *
                    FMath::Lerp(0.30f, 1.0f, SpeedRatio) *
                    DeltaTime,
                0.0f
            )
        );
    }

    if (FMath::IsNearlyZero(CurrentSpeed))
    {
        return;
    }

    const FVector PreviousLocation = GetActorLocation();
    const FVector DesiredLocation =
        PreviousLocation +
        GetActorForwardVector() * CurrentSpeed * DeltaTime;
    const FVector CollisionExtent =
        VehicleCollision->GetScaledBoxExtent();
    const float TravelDirection =
        CurrentSpeed >= 0.0f ? 1.0f : -1.0f;
    const FVector LeadingEdgeOffset =
        GetActorForwardVector() *
        (CollisionExtent.X + 5.0f) *
        TravelDirection;
    const FVector SweepLift(
        0.0f,
        0.0f,
        FMath::Max(35.0f, CollisionExtent.Z * 0.65f)
    );
    const FVector SweepExtent(
        12.0f,
        FMath::Max(20.0f, CollisionExtent.Y - 12.0f),
        FMath::Max(20.0f, CollisionExtent.Z * 0.55f)
    );
    const FVector SweepStart =
        PreviousLocation + LeadingEdgeOffset + SweepLift;
    const FVector SweepEnd =
        DesiredLocation + LeadingEdgeOffset + SweepLift;
    FCollisionQueryParams MovementQuery(
        SCENE_QUERY_STAT(BHFieldTransportMovement),
        false,
        this
    );

    if (IsValid(Occupant))
    {
        MovementQuery.AddIgnoredActor(Occupant);
    }

    FHitResult Hit;
    const bool bMovementBlocked =
        GetWorld()->SweepSingleByChannel(
            Hit,
            SweepStart,
            SweepEnd,
            GetActorQuat(),
            VehicleCollision->GetCollisionObjectType(),
            FCollisionShape::MakeBox(SweepExtent),
            MovementQuery
        );

    if (!bMovementBlocked)
    {
        SetActorLocation(
            DesiredLocation,
            false,
            nullptr,
            ETeleportType::None
        );
    }

    const float DistanceKilometers =
        FVector::Dist2D(PreviousLocation, GetActorLocation()) /
        100000.0f;
    const float BoostBurnMultiplier = bBoostInput ? 1.35f : 1.0f;
    CurrentFuel = FMath::Max(
        0.0f,
        CurrentFuel -
            DistanceKilometers *
            FMath::Max(0.01f, FuelBurnPerKilometer) *
            BoostBurnMultiplier *
            GetCargoFuelBurnMultiplier() *
            GetHullFuelBurnMultiplier() *
            Conditions.VehicleFuelBurnMultiplier
    );

    const float FuelPercentage = GetFuelPercentage();

    if (FuelPercentage <= 0.10f &&
        !bCriticalFuelWarningIssued)
    {
        bCriticalFuelWarningIssued = true;
        NotifyDriver(TEXT("FIELD TRANSPORT FUEL CRITICAL // 10%"));
    }
    else if (FuelPercentage <= 0.25f &&
        !bLowFuelWarningIssued)
    {
        bLowFuelWarningIssued = true;
        NotifyDriver(TEXT("FIELD TRANSPORT LOW FUEL // 25%"));
    }

    UpdateTransportLabel();

    if (bMovementBlocked)
    {
        UE_LOG(
            LogTemp,
            Verbose,
            TEXT(
                "BH_FIELD_TRANSPORT_MOVEMENT_BLOCKED "
                "transport=%s blocker=%s location=%s normal=%s"
            ),
            *GetName(),
            IsValid(Hit.GetActor())
                ? *Hit.GetActor()->GetName()
                : TEXT("World"),
            *Hit.ImpactPoint.ToCompactString(),
            *Hit.ImpactNormal.ToCompactString()
        );
        CurrentSpeed = 0.0f;
    }
}

float ABHFieldTransport::GetHullMobilityMultiplier() const
{
    return CalculateHullMobilityMultiplier(
        GetHullPercentage(),
        CriticalHullFraction,
        MinimumDamagedSpeedMultiplier
    );
}

float ABHFieldTransport::GetHullFuelBurnMultiplier() const
{
    return CalculateHullFuelBurnMultiplier(
        GetHullPercentage(),
        CriticalHullFraction,
        CriticalHullFuelBurnMultiplier
    );
}

void ABHFieldTransport::UpdateGroundPosition(float DeltaTime)
{
    UWorld* World = GetWorld();

    if (!IsValid(World))
    {
        return;
    }

    const FVector CurrentLocation = GetActorLocation();
    const FVector TraceStart =
        CurrentLocation + FVector(0.0f, 0.0f, 600.0f);
    const FVector TraceEnd =
        CurrentLocation - FVector(0.0f, 0.0f, 3000.0f);
    FCollisionQueryParams QueryParams(
        SCENE_QUERY_STAT(BHFieldTransportGround),
        false,
        this
    );

    if (IsValid(Occupant))
    {
        QueryParams.AddIgnoredActor(Occupant);
    }

    FHitResult GroundHit;

    if (!World->LineTraceSingleByChannel(
            GroundHit,
            TraceStart,
            TraceEnd,
            ECC_Visibility,
            QueryParams
        ))
    {
        return;
    }

    FVector TargetLocation = CurrentLocation;
    TargetLocation.Z = GroundHit.ImpactPoint.Z + GroundClearance;
    const float FollowSpeed =
        DeltaTime >= 0.99f ? 1000.0f : 12.0f;
    SetActorLocation(
        FMath::VInterpTo(
            CurrentLocation,
            TargetLocation,
            DeltaTime,
            FollowSpeed
        )
    );
}

void ABHFieldTransport::UpdateTransportLabel()
{
    if (!IsValid(TransportLabel))
    {
        return;
    }

    const FString CargoText =
        CargoType == EBHWarConvoyCargoType::CivilianAid &&
            GetCargoSupply() > KINDA_SMALL_NUMBER
            ? FString::Printf(
                TEXT("AID %.0f > %s"),
                GetCargoSupply(),
                *CargoDestinationSectorID.ToString().ToUpper()
            )
            : FString::Printf(
                TEXT("CARGO %.0f/%.0f"),
                GetCargoSupply(),
                GetCargoCapacity()
            );
    const FString ReadinessText = FString::Printf(
        TEXT(
            "FIELD TRANSPORT\n"
            "FUEL %.0f%% // HULL %.0f%% // %s"
        ),
        GetFuelPercentage() * 100.0f,
        GetHullPercentage() * 100.0f,
        *CargoText
    );
    TransportLabel->SetText(FText::FromString(ReadinessText));

    const bool bDisabled =
        CurrentHull <= KINDA_SMALL_NUMBER ||
        CurrentFuel <= KINDA_SMALL_NUMBER;
    TransportLabel->SetTextRenderColor(
        bDisabled
            ? FColor(220, 55, 45, 255)
            : GetFuelPercentage() <= 0.25f ||
                GetHullPercentage() <= 0.35f
                ? FColor(255, 190, 35, 255)
                : FColor(245, 174, 48, 255)
    );
}

void ABHFieldTransport::OnRep_TransportState()
{
    CurrentFuel = FMath::Clamp(
        CurrentFuel,
        0.0f,
        FMath::Max(1.0f, MaximumFuel)
    );
    CurrentHull = FMath::Clamp(
        CurrentHull,
        0.0f,
        FMath::Max(1.0f, MaximumHull)
    );
    CurrentCargoSupply = FMath::Clamp(
        CurrentCargoSupply,
        0.0f,
        FMath::Max(1.0f, MaximumCargoSupply)
    );
    UpdateTransportLabel();
}

void ABHFieldTransport::NotifyDriver(
    const FString& Message
) const
{
    if (const APlayerController* PlayerController =
            Cast<APlayerController>(GetController()))
    {
        const_cast<APlayerController*>(PlayerController)
            ->ClientMessage(Message);
    }
}
