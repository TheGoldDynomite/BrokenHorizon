#include "BHAmbientWarDirector.h"
#include "BHBattlefieldConditions.h"

#include "BHCharacter.h"
#include "BHEnemyAIController.h"
#include "BHEnemySoldier.h"
#include "BHMissionData.h"
#include "BHOpenWorldOperationDirector.h"
#include "BHPatrolPoint.h"
#include "BHPlayerResolver.h"
#include "BHSectorAnchor.h"
#include "BHSupplyConvoyTarget.h"
#include "BHWarSubsystem.h"
#include "BHWarOperationRules.h"
#include "BHWorldRoute.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Components/AudioComponent.h"
#include "Components/SceneComponent.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "NavigationSystem.h"
#include "Net/UnrealNetwork.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundBase.h"
#include "UObject/ConstructorHelpers.h"
#include "UObject/UObjectGlobals.h"

namespace
{
constexpr float LowSupplyThreshold = 25.0f;
constexpr float HighSupplyThreshold = 75.0f;
constexpr float MinimumPatrolStrength = 1.0f;
constexpr float PatrolStrengthPerMember = 25.0f;

int32 AdjustControlledPatrolCount(
    int32 BaseCount,
    int32 MaximumCount,
    float Supply
)
{
    if (BaseCount <= 0 || MaximumCount <= 0)
    {
        return 0;
    }

    int32 AdjustedCount = BaseCount;

    if (Supply < LowSupplyThreshold)
    {
        --AdjustedCount;
    }
    else if (Supply >= HighSupplyThreshold)
    {
        ++AdjustedCount;
    }

    return FMath::Clamp(AdjustedCount, 1, MaximumCount);
}

int32 LimitPatrolCountByStrength(
    int32 DesiredCount,
    int32 MaximumCount,
    float SourceStrength
)
{
    if (DesiredCount <= 0 ||
        MaximumCount <= 0 ||
        SourceStrength < MinimumPatrolStrength)
    {
        return 0;
    }

    const int32 StrengthCapacity = FMath::Clamp(
        FMath::CeilToInt(
            SourceStrength / PatrolStrengthPerMember
        ),
        1,
        MaximumCount
    );

    return FMath::Min(DesiredCount, StrengthCapacity);
}

int32 LimitPatrolCountBySupply(
    int32 DesiredCount,
    int32 MaximumCount,
    float SourceSupply,
    float SupplyCostPerMember
)
{
    if (DesiredCount <= 0 || MaximumCount <= 0)
    {
        return 0;
    }

    if (SupplyCostPerMember <= KINDA_SMALL_NUMBER)
    {
        return FMath::Min(DesiredCount, MaximumCount);
    }

    const int32 SupplyCapacity = FMath::Clamp(
        FMath::FloorToInt(
            FMath::Max(0.0f, SourceSupply) /
                SupplyCostPerMember
        ),
        0,
        MaximumCount
    );

    return FMath::Min(DesiredCount, SupplyCapacity);
}

constexpr TCHAR FirstLightWindSoundPath[] = TEXT(
    "/Game/BrokenHorizon/Audio/SW_FirstLight_Wind.SW_FirstLight_Wind"
);
constexpr TCHAR FirstLightRainSoundPath[] = TEXT(
    "/Game/BrokenHorizon/Audio/SW_FirstLight_Rain.SW_FirstLight_Rain"
);
constexpr TCHAR FirstLightWindRainSoundPath[] = TEXT(
    "/Game/BrokenHorizon/Audio/SW_FirstLight_WindRain.SW_FirstLight_WindRain"
);
constexpr TCHAR FirstLightDistantWarSoundPath[] = TEXT(
    "/Game/BrokenHorizon/Audio/SW_FirstLight_DistantWar.SW_FirstLight_DistantWar"
);
constexpr TCHAR FirstLightDistantArtillerySoundPath[] = TEXT(
    "/Game/BrokenHorizon/Audio/SW_FirstLight_DistantArtillery.SW_FirstLight_DistantArtillery"
);
constexpr TCHAR FirstLightDistantAircraftSoundPath[] = TEXT(
    "/Game/BrokenHorizon/Audio/SW_FirstLight_DistantAircraft.SW_FirstLight_DistantAircraft"
);
constexpr TCHAR FirstLightDistantSmallArmsSoundPath[] = TEXT(
    "/Game/BrokenHorizon/Audio/SW_FirstLight_DistantSmallArms.SW_FirstLight_DistantSmallArms"
);

USoundBase* ResolveConfiguredAmbientSound(
    TObjectPtr<USoundBase>& ConfiguredSound,
    const TCHAR* PrimaryPath,
    const TCHAR* LegacyPath
)
{
    if (!IsValid(ConfiguredSound))
    {
        ConfiguredSound = LoadObject<USoundBase>(nullptr, PrimaryPath);
    }
    if (!IsValid(ConfiguredSound) && LegacyPath != nullptr)
    {
        ConfiguredSound = LoadObject<USoundBase>(nullptr, LegacyPath);
    }
    return ConfiguredSound.Get();
}
}

ABHAmbientWarDirector::ABHAmbientWarDirector()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickInterval = 2.0f;
    SetReplicates(true);
    bAlwaysRelevant = true;
    SetActorEnableCollision(false);

    AudioRoot = CreateDefaultSubobject<USceneComponent>(TEXT("AudioRoot"));
    SetRootComponent(AudioRoot);
    WindAudioComponent = CreateDefaultSubobject<UAudioComponent>(TEXT("WindAudio"));
    WindAudioComponent->SetupAttachment(AudioRoot);
    WindAudioComponent->bAutoActivate = false;
    RainAudioComponent = CreateDefaultSubobject<UAudioComponent>(TEXT("RainAudio"));
    RainAudioComponent->SetupAttachment(AudioRoot);
    RainAudioComponent->bAutoActivate = false;
    WarBedAudioComponent = CreateDefaultSubobject<UAudioComponent>(TEXT("WarBedAudio"));
    WarBedAudioComponent->SetupAttachment(AudioRoot);
    WarBedAudioComponent->bAutoActivate = false;

    DistantEventAttenuation =
        CreateDefaultSubobject<USoundAttenuation>(
            TEXT("DistantEventAttenuation")
        );
    FSoundAttenuationSettings& EventAttenuation =
        DistantEventAttenuation->Attenuation;
    EventAttenuation.DistanceAlgorithm =
        EAttenuationDistanceModel::NaturalSound;
    EventAttenuation.FalloffMode = ENaturalSoundFalloffMode::Continues;
    EventAttenuation.AttenuationShape = EAttenuationShape::Sphere;
    EventAttenuation.AttenuationShapeExtents =
        FVector(2500.0f, 0.0f, 0.0f);
    EventAttenuation.FalloffDistance = 10000.0f;
    EventAttenuation.bAttenuate = true;
    EventAttenuation.bSpatialize = true;
    EventAttenuation.bAttenuateWithLPF = true;
    EventAttenuation.bApplyNormalizationToStereoSounds = true;
    EventAttenuation.bEnableOcclusion = true;
    EventAttenuation.OcclusionLowPassFilterFrequency = 1200.0f;
    EventAttenuation.OcclusionVolumeAttenuation = 0.35f;
    EventAttenuation.OcclusionInterpolationTime = 0.15f;
    EventAttenuation.LPFRadiusMin = 2500.0f;
    EventAttenuation.LPFRadiusMax = 12500.0f;
    EventAttenuation.LPFFrequencyAtMin = 18000.0f;
    EventAttenuation.LPFFrequencyAtMax = 1800.0f;
    EventAttenuation.StereoSpread = 400.0f;

    SupplyConvoyTargetClass =
        ABHSupplyConvoyTarget::StaticClass();

    const ConstructorHelpers::FObjectFinder<USoundBase> WindSound(
        FirstLightWindSoundPath
    );
    if (WindSound.Succeeded())
    {
        WindLoopSound = WindSound.Object;
    }

    const ConstructorHelpers::FObjectFinder<USoundBase> RainSound(
        FirstLightRainSoundPath
    );
    if (RainSound.Succeeded())
    {
        RainLoopSound = RainSound.Object;
    }

    const ConstructorHelpers::FObjectFinder<USoundBase> DistantWarSound(
        FirstLightDistantWarSoundPath
    );
    if (DistantWarSound.Succeeded())
    {
        DistantWarLoopSound = DistantWarSound.Object;
    }

    const ConstructorHelpers::FObjectFinder<USoundBase> DistantArtilleryAsset(
        FirstLightDistantArtillerySoundPath
    );
    if (DistantArtilleryAsset.Succeeded())
    {
        DistantArtillerySound = DistantArtilleryAsset.Object;
    }

    const ConstructorHelpers::FObjectFinder<USoundBase> DistantAircraftAsset(
        FirstLightDistantAircraftSoundPath
    );
    if (DistantAircraftAsset.Succeeded())
    {
        DistantAircraftSound = DistantAircraftAsset.Object;
    }

    const ConstructorHelpers::FObjectFinder<USoundBase> DistantSmallArmsAsset(
        FirstLightDistantSmallArmsSoundPath
    );
    if (DistantSmallArmsAsset.Succeeded())
    {
        DistantSmallArmsSound = DistantSmallArmsAsset.Object;
    }
}

void ABHAmbientWarDirector::GetLifetimeReplicatedProps(
    TArray<FLifetimeProperty>& OutLifetimeProps
) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ABHAmbientWarDirector, AmbientAudioState);
    DOREPLIFETIME(ABHAmbientWarDirector, ReplicatedWindIntensity);
    DOREPLIFETIME(ABHAmbientWarDirector, ReplicatedRainIntensity);
}

EBHAmbientAudioState ABHAmbientWarDirector::ResolveAmbientAudioState(
    bool bFrontline,
    bool bActiveOperation,
    int32 ActiveHostileCount,
    float EnemyResponsePressure
)
{
    if (bActiveOperation || ActiveHostileCount > 0)
    {
        return EBHAmbientAudioState::Combat;
    }
    if (bFrontline)
    {
        return EBHAmbientAudioState::Frontline;
    }
    if (EnemyResponsePressure >= 25.0f)
    {
        return EBHAmbientAudioState::Tense;
    }
    return EBHAmbientAudioState::Quiet;
}

float ABHAmbientWarDirector::CalculateWarBedVolume(
    EBHAmbientAudioState AudioState
)
{
    switch (AudioState)
    {
        case EBHAmbientAudioState::Tense: return 0.28f;
        case EBHAmbientAudioState::Frontline: return 0.58f;
        case EBHAmbientAudioState::Combat: return 0.82f;
        default: return 0.08f;
    }
}

void ABHAmbientWarDirector::SetWeatherMix(
    float WindIntensity,
    float RainIntensity
)
{
    if (!HasAuthority())
    {
        return;
    }
    ReplicatedWindIntensity = FMath::Clamp(WindIntensity, 0.0f, 1.0f);
    ReplicatedRainIntensity = FMath::Clamp(RainIntensity, 0.0f, 1.0f);
    ApplyAmbientAudioMix();
    ForceNetUpdate();
}

void ABHAmbientWarDirector::BeginPlay()
{
    Super::BeginPlay();

    if (GetNetMode() != NM_DedicatedServer)
    {
        const bool bWindReady = ResolveConfiguredAmbientSound(
            WindLoopSound,
            FirstLightWindSoundPath,
            FirstLightWindRainSoundPath
        ) != nullptr;
        const bool bRainReady = ResolveConfiguredAmbientSound(
            RainLoopSound,
            FirstLightRainSoundPath,
            FirstLightWindRainSoundPath
        ) != nullptr;
        const bool bWarReady = ResolveConfiguredAmbientSound(
            DistantWarLoopSound,
            FirstLightDistantWarSoundPath,
            nullptr
        ) != nullptr;
        UE_LOG(
            LogTemp,
            Display,
            TEXT("BH_AMBIENT_AUDIO_READY wind=%s rain=%s war=%s looping=1"),
            bWindReady ? TEXT("1") : TEXT("0"),
            bRainReady ? TEXT("1") : TEXT("0"),
            bWarReady ? TEXT("1") : TEXT("0")
        );

        const bool bArtilleryReady = ResolveConfiguredAmbientSound(
            DistantArtillerySound,
            FirstLightDistantArtillerySoundPath,
            nullptr
        ) != nullptr;
        const bool bAircraftReady = ResolveConfiguredAmbientSound(
            DistantAircraftSound,
            FirstLightDistantAircraftSoundPath,
            nullptr
        ) != nullptr;
        const bool bSmallArmsReady = ResolveConfiguredAmbientSound(
            DistantSmallArmsSound,
            FirstLightDistantSmallArmsSoundPath,
            nullptr
        ) != nullptr;
        UE_LOG(
            LogTemp,
            Display,
            TEXT(
                "BH_AMBIENT_EVENT_AUDIO_READY artillery=%s "
                "aircraft=%s small_arms=%s"
            ),
            bArtilleryReady ? TEXT("1") : TEXT("0"),
            bAircraftReady ? TEXT("1") : TEXT("0"),
            bSmallArmsReady ? TEXT("1") : TEXT("0")
        );
        UE_LOG(
            LogTemp,
            Display,
            TEXT(
                "BH_AMBIENT_EVENT_ATTENUATION_READY model=natural_sound "
                "radius_cm=2500 falloff_cm=10000 lpf=1 occlusion=1"
            )
        );
    }

    if (!HasAuthority())
    {
        SetActorTickEnabled(false);
        ApplyAmbientAudioMix();
        return;
    }

    ResolveEnemyClass();

    const FBHBattlefieldConditionProfile Conditions =
        UBHBattlefieldConditions::GetCurrentProfile(this);
    const float RainIntensity =
        Conditions.Weather == EBHBattlefieldWeather::Rain ? 0.65f :
        Conditions.Weather == EBHBattlefieldWeather::Storm ? 1.0f : 0.0f;
    const float WindIntensity =
        Conditions.Weather == EBHBattlefieldWeather::Storm ? 1.0f :
        Conditions.Weather == EBHBattlefieldWeather::Rain ? 0.55f :
        Conditions.Weather == EBHBattlefieldWeather::Fog ? 0.12f : 0.25f;
    SetWeatherMix(WindIntensity, RainIntensity);

    if (const UWorld* World = GetWorld())
    {
        NextSpawnTime =
            World->GetTimeSeconds() + InitialSpawnDelay;
        NextDistantWarEventTime =
            World->GetTimeSeconds() + MinimumDistantEventInterval;
    }

    ApplyAmbientAudioMix();

    UE_LOG(
        LogTemp,
        Display,
        TEXT("BH_AMBIENT_WAR_DIRECTOR_STARTED")
    );
}

void ABHAmbientWarDirector::EndPlay(
    const EEndPlayReason::Type EndPlayReason
)
{
    if (HasAuthority())
    {
        for (ABHEnemySoldier* Enemy : TrackedEnemies)
        {
            if (IsValid(Enemy))
            {
                Enemy->Destroy();
            }
        }

        if (IsValid(ActiveSupplyConvoyTarget))
        {
            ActiveSupplyConvoyTarget->Destroy();
        }
        CleanupSupplyConvoyEscorts();
        DestroyPatrolPoints();
    }

    TrackedEnemies.Reset();
    RoutedCombatants.Reset();
    ActiveSupplyConvoyTarget = nullptr;
    PresentedSupplyConvoyIDs.Reset();
    CleanupSupplyConvoyEscorts();
    ActivePatrolSectorID = NAME_None;
    ActiveFriendlyForceSectorID = NAME_None;
    ActiveEnemyForceSectorID = NAME_None;
    ActiveFriendlySourceHops = INDEX_NONE;
    ActiveEnemySourceHops = INDEX_NONE;
    InitialFriendlyCount = 0;
    InitialEnemyCount = 0;
    PatrolResolvedTime = -1.0f;
    LastPlayerSectorID = NAME_None;
    SectorContactReadyTimes.Reset();
    LastReconSampleLocation = FVector::ZeroVector;
    ReconMovementAccumulated = 0.0f;
    ReconObservationAccumulated = 0.0f;
    NextReconReportTime = 0.0f;
    Super::EndPlay(EndPlayReason);
}

void ABHAmbientWarDirector::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    if (!HasAuthority())
    {
        SetActorTickEnabled(false);
        return;
    }

    UWorld* World = GetWorld();
    ABHCharacter* PlayerCharacter = ResolvePlayerCharacter();

    if (!IsValid(World) || !IsValid(PlayerCharacter))
    {
        return;
    }

    UpdatePlayerSectorAwareness(PlayerCharacter);
    UpdateAmbientAudioState(PlayerCharacter);
    UpdateDistantWarEvents(PlayerCharacter);
    UpdateFieldRecon(PlayerCharacter, DeltaSeconds);
    UpdateSupplyConvoyOpportunity(PlayerCharacter);
    UpdateSupplyConvoyEscorts();
    RemoveInvalidEnemies();
    CleanupDistantEnemies(PlayerCharacter);
    UpdatePatrolLifecycle();

    if (!TrackedEnemies.IsEmpty() || HasAssignedOperation())
    {
        return;
    }

    const float CurrentTime = World->GetTimeSeconds();

    if (CurrentTime < NextSpawnTime)
    {
        return;
    }

    FBHWarSectorState SectorState;
    ABHSectorAnchor* SectorAnchor = nullptr;

    if (!ResolveLocalWarState(
            PlayerCharacter->GetActorLocation(),
            SectorState,
            SectorAnchor))
    {
        NextSpawnTime = CurrentTime + SpawnRetryDelay;
        return;
    }

    const int32 DesiredEnemyCount =
        GetDesiredEnemyCount(SectorState);
    const int32 DesiredFriendlyCount =
        GetDesiredFriendlyCount(SectorState);
    const float RemainingContactCooldown =
        GetRemainingSectorContactCooldown(
            SectorState.SectorID,
            CurrentTime
        );

    if (RemainingContactCooldown > 0.0f)
    {
        NextSpawnTime =
            CurrentTime +
            FMath::Min(
                RemainingContactCooldown,
                FMath::Max(0.1f, SpawnRetryDelay)
            );
        return;
    }

    if (DesiredEnemyCount <= 0 &&
        DesiredFriendlyCount <= 0)
    {
        NextSpawnTime = CurrentTime + PatrolRespawnDelay;
        return;
    }

    if (!SpawnPatrol(
            PlayerCharacter,
            SectorState,
            DesiredEnemyCount,
            DesiredFriendlyCount))
    {
        NextSpawnTime = CurrentTime + SpawnRetryDelay;
        return;
    }

    NextSpawnTime = CurrentTime + PatrolRespawnDelay;
}

void ABHAmbientWarDirector::UpdateAmbientAudioState(
    ABHCharacter* PlayerCharacter
)
{
    if (!HasAuthority() || !IsValid(PlayerCharacter))
    {
        return;
    }

    FBHWarSectorState SectorState;
    ABHSectorAnchor* SectorAnchor = nullptr;
    const bool bHasSector = ResolveLocalWarState(
        PlayerCharacter->GetActorLocation(),
        SectorState,
        SectorAnchor
    );
    const EBHAmbientAudioState NewState = ResolveAmbientAudioState(
        bHasSector && IsFrontlineSector(SectorState),
        HasAssignedOperation(),
        TrackedEnemies.Num() + ConvoyEscorts.Num(),
        bHasSector ? SectorState.EnemyResponsePressure : 0.0f
    );
    if (NewState == AmbientAudioState)
    {
        return;
    }

    AmbientAudioState = NewState;
    ApplyAmbientAudioMix();
    ForceNetUpdate();
}

void ABHAmbientWarDirector::UpdateDistantWarEvents(
    ABHCharacter* PlayerCharacter
)
{
    UWorld* World = GetWorld();
    if (!HasAuthority() || !IsValid(World) || !IsValid(PlayerCharacter) ||
        (AmbientAudioState != EBHAmbientAudioState::Frontline &&
            AmbientAudioState != EBHAmbientAudioState::Combat) ||
        World->GetTimeSeconds() < NextDistantWarEventTime)
    {
        return;
    }

    const uint8 EventType = static_cast<uint8>(FMath::RandRange(0, 2));
    const float Angle = FMath::FRandRange(0.0f, UE_TWO_PI);
    const float Distance = FMath::FRandRange(6000.0f, 12000.0f);
    const FVector EventLocation = PlayerCharacter->GetActorLocation() +
        FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.15f) * Distance;
    MulticastPlayDistantWarEvent(
        EventType,
        EventLocation,
        AmbientAudioState == EBHAmbientAudioState::Combat ? 0.75f : 0.55f
    );
    NextDistantWarEventTime = World->GetTimeSeconds() + FMath::FRandRange(
        FMath::Max(1.0f, MinimumDistantEventInterval),
        FMath::Max(MinimumDistantEventInterval, MaximumDistantEventInterval)
    );
}

void ABHAmbientWarDirector::OnRep_AmbientAudioMix()
{
    ApplyAmbientAudioMix();
}

void ABHAmbientWarDirector::ApplyLoopAudio(
    UAudioComponent* AudioComponent,
    USoundBase* Sound,
    float TargetVolume
)
{
    if (!IsValid(AudioComponent))
    {
        return;
    }
    if (GetNetMode() == NM_DedicatedServer || !IsValid(Sound) || TargetVolume <= 0.0f)
    {
        if (AudioComponent->IsPlaying())
        {
            AudioComponent->FadeOut(0.75f, 0.0f);
        }
        return;
    }
    if (AudioComponent->GetSound() != Sound)
    {
        AudioComponent->SetSound(Sound);
    }
    if (!AudioComponent->IsPlaying())
    {
        AudioComponent->FadeIn(0.75f, TargetVolume);
    }
    else
    {
        AudioComponent->AdjustVolume(0.75f, TargetVolume);
    }
}

void ABHAmbientWarDirector::ApplyAmbientAudioMix()
{
    ApplyLoopAudio(
        WindAudioComponent,
        WindLoopSound,
        FMath::Clamp(ReplicatedWindIntensity, 0.0f, 1.0f)
    );
    ApplyLoopAudio(
        RainAudioComponent,
        RainLoopSound,
        FMath::Clamp(ReplicatedRainIntensity, 0.0f, 1.0f)
    );
    ApplyLoopAudio(
        WarBedAudioComponent,
        DistantWarLoopSound,
        CalculateWarBedVolume(AmbientAudioState)
    );
}

USoundBase* ABHAmbientWarDirector::ResolveDistantEventSound(
    uint8 EventType
) const
{
    switch (EventType)
    {
        case 0: return DistantArtillerySound.Get();
        case 1: return DistantAircraftSound.Get();
        default: return DistantSmallArmsSound.Get();
    }
}

void ABHAmbientWarDirector::MulticastPlayDistantWarEvent_Implementation(
    uint8 EventType,
    FVector WorldLocation,
    float VolumeMultiplier
)
{
    if (GetNetMode() == NM_DedicatedServer)
    {
        return;
    }
    if (USoundBase* EventSound = ResolveDistantEventSound(EventType))
    {
        UGameplayStatics::PlaySoundAtLocation(
            this,
            EventSound,
            WorldLocation,
            FMath::Clamp(VolumeMultiplier, 0.0f, 1.0f),
            1.0f,
            0.0f,
            DistantEventAttenuation
        );
    }
}

int32 ABHAmbientWarDirector::GetSurvivingConvoySecurityCount(
    const ABHSupplyConvoyTarget* ConvoyTarget
) const
{
    if (!IsValid(ConvoyTarget) ||
        ActiveSupplyConvoyTarget != ConvoyTarget)
    {
        return 0;
    }

    int32 SurvivingCount = 0;

    for (const ABHEnemySoldier* Escort : ConvoyEscorts)
    {
        if (IsValid(Escort) && !Escort->IsDead())
        {
            ++SurvivingCount;
        }
    }

    return SurvivingCount;
}

void ABHAmbientWarDirector::
ResetSupplyConvoyEncounterForLoad()
{
    if (!HasAuthority())
    {
        return;
    }

    ActiveSupplyConvoyTarget = nullptr;
    ActiveConvoySourceSectorID = NAME_None;
    ActiveConvoyDefenderSourceSectorID = NAME_None;
    ConvoyEscortWithdrawalTime = -1.0f;
    CleanupSupplyConvoyEscorts();
}

bool ABHAmbientWarDirector::
RestoreSupplyConvoySalvageSecurity(
    ABHSupplyConvoyTarget* ConvoyTarget,
    int32 SurvivingSecurityCount
)
{
    if (!HasAuthority())
    {
        return false;
    }

    const int32 SafeSecurityCount = FMath::Clamp(
        SurvivingSecurityCount,
        0,
        4
    );

    if (!IsValid(ConvoyTarget) ||
        !ConvoyTarget->HasRecoverableSalvage() ||
        SafeSecurityCount <= 0 ||
        IsValid(ActiveSupplyConvoyTarget))
    {
        return false;
    }

    ActiveSupplyConvoyTarget = ConvoyTarget;
    ActiveConvoySourceSectorID =
        ConvoyTarget->GetSourceSectorID();
    ActiveConvoyDefenderSourceSectorID = NAME_None;
    PresentedSupplyConvoyIDs.Add(
        ConvoyTarget->GetConvoyID()
    );
    SpawnSupplyConvoyEscorts(
        ConvoyTarget,
        SafeSecurityCount
    );

    const bool bRestored = !ConvoyEscorts.IsEmpty();

    UE_LOG(
        LogTemp,
        Display,
        TEXT(
            "BH_CONVOY_SALVAGE_SECURITY_RESTORED id=%s "
            "requested=%d spawned=%d"
        ),
        *ConvoyTarget->GetConvoyID().ToString(),
        SafeSecurityCount,
        ConvoyEscorts.Num()
    );

    if (!bRestored)
    {
        ActiveSupplyConvoyTarget = nullptr;
        ActiveConvoySourceSectorID = NAME_None;
    }

    return bRestored;
}

FName ABHAmbientWarDirector::GetPlayerSectorID() const
{
    return LastPlayerSectorID;
}

bool ABHAmbientWarDirector::GetFieldReconStatus(
    float& OutIntelConfidence,
    float& OutMovementProgress,
    float& OutMovementRequired,
    float& OutObservationProgress,
    float& OutObservationRequired,
    float& OutReportCooldownRemaining
) const
{
    OutIntelConfidence = 0.0f;
    OutMovementRequired = FMath::Max(
        100.0f,
        FieldReconMovementRequired
    );
    OutObservationRequired = FMath::Max(
        1.0f,
        FieldReconObservationDuration
    );
    OutMovementProgress = FMath::Clamp(
        ReconMovementAccumulated,
        0.0f,
        OutMovementRequired
    );
    OutObservationProgress = FMath::Clamp(
        ReconObservationAccumulated,
        0.0f,
        OutObservationRequired
    );

    const UWorld* World = GetWorld();
    OutReportCooldownRemaining = IsValid(World)
        ? FMath::Max(
            0.0f,
            NextReconReportTime - World->GetTimeSeconds()
        )
        : 0.0f;

    const UGameInstance* GameInstance = GetGameInstance();
    const UBHWarSubsystem* WarSubsystem =
        IsValid(GameInstance)
            ? GameInstance->GetSubsystem<UBHWarSubsystem>()
            : nullptr;

    if (!IsValid(WarSubsystem) ||
        LastPlayerSectorID.IsNone())
    {
        return false;
    }

    const FBHWarSectorState SectorState =
        WarSubsystem->GetSectorState(LastPlayerSectorID);
    OutIntelConfidence = FMath::Clamp(
        SectorState.IntelConfidence,
        0.0f,
        100.0f
    );
    const bool bCommittedReconHere =
        WarSubsystem->HasCommittedOperation() &&
        WarSubsystem->GetCommittedOperationType() ==
            EBHWarPriorityType::Recon &&
        WarSubsystem->GetCommittedOperationSectorID() ==
            LastPlayerSectorID;

    return !SectorState.SectorID.IsNone() &&
        SectorState.Owner != EBHWarFaction::Friendly &&
        SectorState.IntelConfidence < 100.0f &&
        (!HasAssignedOperation() || bCommittedReconHere);
}

ABHCharacter*
ABHAmbientWarDirector::ResolvePlayerCharacter() const
{
    return BHPlayerResolver::Find(this);
}

ABHSectorAnchor*
ABHAmbientWarDirector::FindNearestSectorAnchor(
    const FVector& PlayerLocation
) const
{
    ABHSectorAnchor* BestAnchor = nullptr;
    float BestDistanceSquared = BIG_NUMBER;

    for (TActorIterator<ABHSectorAnchor> It(GetWorld()); It; ++It)
    {
        ABHSectorAnchor* Candidate = *It;

        if (!IsValid(Candidate))
        {
            continue;
        }

        const float DistanceSquared = FVector::DistSquared2D(
            PlayerLocation,
            Candidate->GetOperationCenter()
        );

        if (DistanceSquared < BestDistanceSquared)
        {
            BestDistanceSquared = DistanceSquared;
            BestAnchor = Candidate;
        }
    }

    return BestAnchor;
}

ABHSectorAnchor*
ABHAmbientWarDirector::FindSectorAnchorByID(
    FName SectorID
) const
{
    if (SectorID.IsNone())
    {
        return nullptr;
    }

    for (TActorIterator<ABHSectorAnchor> It(GetWorld()); It; ++It)
    {
        ABHSectorAnchor* Candidate = *It;

        if (IsValid(Candidate) &&
            Candidate->GetSectorID() == SectorID)
        {
            return Candidate;
        }
    }

    return nullptr;
}

bool ABHAmbientWarDirector::ResolveLocalWarState(
    const FVector& PlayerLocation,
    FBHWarSectorState& OutSectorState,
    ABHSectorAnchor*& OutSectorAnchor
) const
{
    OutSectorAnchor = FindNearestSectorAnchor(PlayerLocation);

    const UGameInstance* GameInstance = GetGameInstance();
    const UBHWarSubsystem* WarSubsystem = GameInstance
        ? GameInstance->GetSubsystem<UBHWarSubsystem>()
        : nullptr;

    if (!IsValid(OutSectorAnchor) ||
        !IsValid(WarSubsystem))
    {
        return false;
    }

    OutSectorState = WarSubsystem->GetSectorState(
        OutSectorAnchor->GetSectorID()
    );
    return !OutSectorState.SectorID.IsNone();
}

void ABHAmbientWarDirector::UpdatePlayerSectorAwareness(
    ABHCharacter* PlayerCharacter
)
{
    if (!IsValid(PlayerCharacter))
    {
        return;
    }

    FBHWarSectorState SectorState;
    ABHSectorAnchor* SectorAnchor = nullptr;

    if (!ResolveLocalWarState(
            PlayerCharacter->GetActorLocation(),
            SectorState,
            SectorAnchor) ||
        SectorState.SectorID == LastPlayerSectorID)
    {
        return;
    }

    LastPlayerSectorID = SectorState.SectorID;
    LastReconSampleLocation =
        PlayerCharacter->GetActorLocation();
    ReconMovementAccumulated = 0.0f;
    ReconObservationAccumulated = 0.0f;

    const FText ControlText =
        SectorState.Owner == EBHWarFaction::Friendly
            ? NSLOCTEXT(
                "BrokenHorizon",
                "FriendlySectorControl",
                "FRIENDLY CONTROL"
            )
            : (SectorState.Owner == EBHWarFaction::Enemy
                ? NSLOCTEXT(
                    "BrokenHorizon",
                    "EnemySectorControl",
                    "ENEMY CONTROL"
                )
                : NSLOCTEXT(
                    "BrokenHorizon",
                    "ContestedSectorControl",
                    "CONTESTED TERRITORY"
                ));
    const FText SectorDisplayName =
        IsValid(SectorAnchor)
            ? SectorAnchor->GetSectorDisplayName()
            : SectorState.DisplayName;
    const UGameInstance* GameInstance = GetGameInstance();
    UBHWarSubsystem* WarSubsystem = GameInstance
        ? GameInstance->GetSubsystem<UBHWarSubsystem>()
        : nullptr;
    if (IsValid(WarSubsystem))
    {
        WarSubsystem->ReportSectorRecon(
            SectorState.SectorID,
            SectorState.Owner == EBHWarFaction::Friendly
                ? 0.0f
                : SectorEntryIntelGain
        );
        SectorState = WarSubsystem->GetSectorState(
            SectorState.SectorID
        );
    }
    const bool bLogisticsHub =
        IsValid(WarSubsystem) &&
        WarSubsystem->IsLogisticsHubSector(
            SectorState.SectorID
        );
    const bool bFrontline = IsFrontlineSector(SectorState);
    const FText StrategicRoleText =
        bLogisticsHub
            ? NSLOCTEXT(
                "BrokenHorizon",
                "LogisticsHubSectorRole",
                "LOGISTICS HUB"
            )
            : bFrontline
                ? NSLOCTEXT(
                    "BrokenHorizon",
                    "FrontlineSectorRole",
                    "FRONTLINE"
                )
                : NSLOCTEXT(
                    "BrokenHorizon",
                    "RearAreaSectorRole",
                    "REAR AREA"
                );
    const float SupplyFlow = IsValid(WarSubsystem)
        ? WarSubsystem->GetSectorSupplyChangePerTurn(
            SectorState.SectorID
        )
        : 0.0f;

    PlayerCharacter->ShowStatusNotification(
        FText::Format(
            NSLOCTEXT(
                "BrokenHorizon",
                "SectorEntryNotification",
                "SECTOR // {0}\n{1} // {2}\n\n"
                "FRIENDLY {3} // SUPPLY {4}\n"
                "{5}\n"
                "GARRISON F {6} // CAP {7}\n"
                "SUPPLY FLOW {8} / TURN{9}"
            ),
            SectorDisplayName,
            ControlText,
            StrategicRoleText,
            FText::AsNumber(FMath::RoundToInt(
                SectorState.FriendlyStrength
            )),
            FText::AsNumber(FMath::RoundToInt(
                SectorState.Supply
            )),
            IsValid(WarSubsystem)
                ? WarSubsystem->GetSectorEnemyIntelSummary(
                    SectorState.SectorID
                )
                : NSLOCTEXT(
                    "BrokenHorizon",
                    "SectorIntelUnavailable",
                    "INTEL UNAVAILABLE"
                ),
            FText::AsNumber(SectorState.FriendlyGarrison),
            FText::AsNumber(SectorState.GarrisonCapacity),
            FText::FromString(
                FString::Printf(
                    TEXT("%+.1f"),
                    SupplyFlow
                )
            ),
            SectorState.Owner != EBHWarFaction::Friendly &&
                SectorState.IntelConfidence < 80.0f
                ? NSLOCTEXT(
                    "BrokenHorizon",
                    "SectorReconRequired",
                    "\nRECON // MOVE AND OBSERVE TO CONFIRM HOSTILES"
                )
                : FText::GetEmpty()
        )
    );

    UE_LOG(
        LogTemp,
        Display,
        TEXT(
            "BH_PLAYER_ENTERED_SECTOR sector=%s owner=%d "
            "friendly=%.1f enemy=%.1f supply=%.1f "
            "garrison_f=%d garrison_e=%d capacity=%d "
            "intel=%.0f flow=%+.1f role=%s"
        ),
        *SectorState.SectorID.ToString(),
        static_cast<int32>(SectorState.Owner),
        SectorState.FriendlyStrength,
        SectorState.EnemyStrength,
        SectorState.Supply,
        SectorState.FriendlyGarrison,
        SectorState.EnemyGarrison,
        SectorState.GarrisonCapacity,
        SectorState.IntelConfidence,
        SupplyFlow,
        *StrategicRoleText.ToString()
    );
}

void ABHAmbientWarDirector::UpdateFieldRecon(
    ABHCharacter* PlayerCharacter,
    float DeltaSeconds
)
{
    UWorld* World = GetWorld();
    UGameInstance* GameInstance = GetGameInstance();
    UBHWarSubsystem* WarSubsystem = IsValid(GameInstance)
        ? GameInstance->GetSubsystem<UBHWarSubsystem>()
        : nullptr;

    const bool bCommittedReconHere =
        IsValid(WarSubsystem) &&
        WarSubsystem->HasCommittedOperation() &&
        WarSubsystem->GetCommittedOperationType() ==
            EBHWarPriorityType::Recon &&
        WarSubsystem->GetCommittedOperationSectorID() ==
            LastPlayerSectorID;

    if (!IsValid(World) ||
        !IsValid(PlayerCharacter) ||
        !IsValid(WarSubsystem) ||
        LastPlayerSectorID.IsNone() ||
        (HasAssignedOperation() && !bCommittedReconHere))
    {
        return;
    }

    const FBHWarSectorState SectorState =
        WarSubsystem->GetSectorState(LastPlayerSectorID);

    if (SectorState.SectorID.IsNone() ||
        SectorState.Owner == EBHWarFaction::Friendly ||
        SectorState.IntelConfidence >= 100.0f)
    {
        LastReconSampleLocation =
            PlayerCharacter->GetActorLocation();
        ReconMovementAccumulated = 0.0f;
        ReconObservationAccumulated = 0.0f;
        return;
    }

    const FVector PlayerLocation =
        PlayerCharacter->GetActorLocation();

    if (LastReconSampleLocation.IsNearlyZero())
    {
        LastReconSampleLocation = PlayerLocation;
        return;
    }

    const float SampleDistance = FMath::Min(
        FVector::Dist2D(PlayerLocation, LastReconSampleLocation),
        1000.0f
    );
    LastReconSampleLocation = PlayerLocation;
    ReconMovementAccumulated += SampleDistance;
    ReconObservationAccumulated += FMath::Max(0.0f, DeltaSeconds);

    if (ReconMovementAccumulated <
            FMath::Max(100.0f, FieldReconMovementRequired) ||
        ReconObservationAccumulated <
            FMath::Max(1.0f, FieldReconObservationDuration) ||
        World->GetTimeSeconds() < NextReconReportTime)
    {
        return;
    }

    ReconMovementAccumulated = 0.0f;
    ReconObservationAccumulated = 0.0f;
    NextReconReportTime =
        World->GetTimeSeconds() +
        FMath::Max(0.0f, FieldReconReportCooldown);

    if (!WarSubsystem->ReportSectorRecon(
            LastPlayerSectorID,
            FieldReconIntelGain))
    {
        return;
    }

    const FBHWarSectorState UpdatedSector =
        WarSubsystem->GetSectorState(LastPlayerSectorID);
    PlayerCharacter->ShowStatusNotification(
        FText::Format(
            NSLOCTEXT(
                "BrokenHorizon",
                "FieldReconReportFiled",
                "FIELD RECON FILED // +{0} INTEL\n\n"
                "{1}\n"
                "CONFIDENCE {2}%"
            ),
            FText::AsNumber(FMath::RoundToInt(
                FieldReconIntelGain
            )),
            WarSubsystem->GetSectorEnemyIntelSummary(
                LastPlayerSectorID
            ),
            FText::AsNumber(FMath::RoundToInt(
                UpdatedSector.IntelConfidence
            ))
        )
    );

    UE_LOG(
        LogTemp,
        Display,
        TEXT(
            "BH_FIELD_RECON_REPORTED sector=%s "
            "gain=%.1f confidence=%.1f"
        ),
        *LastPlayerSectorID.ToString(),
        FieldReconIntelGain,
        UpdatedSector.IntelConfidence
    );

    if (BHWarOperationRules::IsReconReportComplete(
            WarSubsystem->HasCommittedOperation(),
            WarSubsystem->GetCommittedOperationType(),
            WarSubsystem->GetCommittedOperationSectorID(),
            LastPlayerSectorID,
            UpdatedSector.IntelConfidence
        ))
    {
        const bool bCompleted =
            PlayerCharacter->CompleteSharedObjective(
                BHObjectiveIds::ObserveSector
            );
        UE_LOG(
            LogTemp,
            Display,
            TEXT(
                "BH_RECON_OPERATION_REPORT_CONFIRMED sector=%s "
                "objective_completed=%d"
            ),
            *LastPlayerSectorID.ToString(),
            bCompleted ? 1 : 0
        );
    }
}

void ABHAmbientWarDirector::UpdateSupplyConvoyOpportunity(
    ABHCharacter* PlayerCharacter
)
{
    if (!IsValid(PlayerCharacter) ||
        LastPlayerSectorID.IsNone() ||
        HasAssignedOperation())
    {
        return;
    }

    if (IsValid(ActiveSupplyConvoyTarget) ||
        !ConvoyEscorts.IsEmpty() ||
        !ConvoyDefenders.IsEmpty())
    {
        return;
    }

    ActiveSupplyConvoyTarget = nullptr;

    UWorld* World = GetWorld();
    UGameInstance* GameInstance = GetGameInstance();
    UBHWarSubsystem* WarSubsystem = GameInstance
        ? GameInstance->GetSubsystem<UBHWarSubsystem>()
        : nullptr;

    if (!IsValid(World) ||
        !IsValid(WarSubsystem) ||
        !SupplyConvoyTargetClass)
    {
        return;
    }

    FBHWarSupplyConvoyState LocalConvoy;

    for (const FBHWarSupplyConvoyState& Convoy :
        WarSubsystem->GetSupplyConvoys())
    {
        const bool bTouchesPlayerSector =
            Convoy.SourceSectorID == LastPlayerSectorID ||
            Convoy.DestinationSectorID ==
                LastPlayerSectorID;

        if (Convoy.Owner != EBHWarFaction::Neutral &&
            bTouchesPlayerSector &&
            !PresentedSupplyConvoyIDs.Contains(Convoy.ConvoyID))
        {
            LocalConvoy = Convoy;
            break;
        }
    }

    if (LocalConvoy.ConvoyID.IsNone())
    {
        return;
    }

    const ABHSectorAnchor* SourceAnchor =
        FindSectorAnchorByID(
            LocalConvoy.SourceSectorID
        );
    const ABHSectorAnchor* DestinationAnchor =
        FindSectorAnchorByID(
            LocalConvoy.DestinationSectorID
        );

    if (!IsValid(SourceAnchor) ||
        !IsValid(DestinationAnchor))
    {
        return;
    }

    const FVector SourceLocation =
        SourceAnchor->GetOperationCenter();
    const FVector DestinationLocation =
        DestinationAnchor->GetOperationCenter();
    const FVector Segment = DestinationLocation - SourceLocation;
    const float SegmentLength2D = Segment.Size2D();

    if (SegmentLength2D <= KINDA_SMALL_NUMBER)
    {
        return;
    }

    const FVector TravelDirection =
        FVector(Segment.X, Segment.Y, 0.0f).GetSafeNormal();
    const FVector PlayerLocation =
        PlayerCharacter->GetActorLocation();
    const float PlayerDistanceAlongSegment = FMath::Clamp(
        FVector::DotProduct(
            PlayerLocation - SourceLocation,
            TravelDirection
        ),
        0.0f,
        SegmentLength2D
    );
    const float SectorTravelDirection =
        LastPlayerSectorID ==
            LocalConvoy.SourceSectorID
            ? 1.0f
            : -1.0f;
    const float TargetDistanceAlongSegment = FMath::Clamp(
        PlayerDistanceAlongSegment +
            (
                SectorTravelDirection *
                FMath::Max(
                    2500.0f,
                    ConvoyOpportunitySpawnDistance
                )
            ),
        500.0f,
        FMath::Max(500.0f, SegmentLength2D - 500.0f)
    );
    FVector SpawnLocation =
        SourceLocation +
        (TravelDirection * TargetDistanceAlongSegment);
    SpawnLocation.Z = FMath::Lerp(
        SourceLocation.Z,
        DestinationLocation.Z,
        TargetDistanceAlongSegment / SegmentLength2D
    );
    FRotator SpawnRotation = TravelDirection.Rotation();
    const TArray<ABHWorldRoute*> CompatibleRoutes =
        FindCompatibleWorldRoutes(SourceLocation, DestinationLocation);
    ABHWorldRoute* ConvoyRoute = CompatibleRoutes.IsEmpty()
        ? nullptr
        : CompatibleRoutes[0];
    if (!LocalConvoy.SelectedWorldRouteID.IsNone())
    {
        if (ABHWorldRoute* const* SavedRoute =
            CompatibleRoutes.FindByPredicate(
                [&LocalConvoy](const ABHWorldRoute* Candidate)
                {
                    return IsValid(Candidate) &&
                        Candidate->GetRouteID() ==
                            LocalConvoy.SelectedWorldRouteID;
                }))
        {
            ConvoyRoute = *SavedRoute;
        }
    }

    if (IsValid(ConvoyRoute))
    {
        const float SourceRouteDistance =
            ConvoyRoute->
                GetDistanceAlongRouteClosestToWorldLocation(
                    SourceLocation
                );
        const float DestinationRouteDistance =
            ConvoyRoute->
                GetDistanceAlongRouteClosestToWorldLocation(
                    DestinationLocation
                );
        const FVector SourceRouteLocation =
            ConvoyRoute->GetWorldLocationAtDistance(
                SourceRouteDistance
            );
        const FVector DestinationRouteLocation =
            ConvoyRoute->GetWorldLocationAtDistance(
                DestinationRouteDistance
            );
        const float ConnectionTolerance = FMath::Max(
            0.0f,
            ConvoyRouteConnectionTolerance
        );
        const bool bSourceConnected =
            FVector::Dist2D(
                SourceLocation,
                SourceRouteLocation
            ) <= ConnectionTolerance;
        const bool bDestinationConnected =
            FVector::Dist2D(
                DestinationLocation,
                DestinationRouteLocation
            ) <= ConnectionTolerance;
        const float RouteSegmentLength = FMath::Abs(
            DestinationRouteDistance - SourceRouteDistance
        );

        if (bSourceConnected &&
            bDestinationConnected &&
            RouteSegmentLength > 1000.0f)
        {
            const float PlayerRouteDistance =
                ConvoyRoute->
                    GetDistanceAlongRouteClosestToWorldLocation(
                        PlayerLocation
                    );
            const float RouteTravelDirection =
                DestinationRouteDistance >= SourceRouteDistance
                    ? 1.0f
                    : -1.0f;
            const float RouteSpawnDirection =
                RouteTravelDirection * SectorTravelDirection;
            const float MinimumRouteDistance =
                FMath::Min(
                    SourceRouteDistance,
                    DestinationRouteDistance
                ) + 500.0f;
            const float MaximumRouteDistance =
                FMath::Max(
                    SourceRouteDistance,
                    DestinationRouteDistance
                ) - 500.0f;
            const float TargetRouteDistance = FMath::Clamp(
                PlayerRouteDistance +
                    (
                        RouteSpawnDirection *
                        FMath::Max(
                            2500.0f,
                            ConvoyOpportunitySpawnDistance
                        )
                    ),
                MinimumRouteDistance,
                MaximumRouteDistance
            );
            SpawnLocation =
                ConvoyRoute->GetWorldLocationAtDistance(
                    TargetRouteDistance
                );
            const FVector RouteDirection =
                ConvoyRoute->GetWorldDirectionAtDistance(
                    TargetRouteDistance
                ).GetSafeNormal2D() * RouteTravelDirection;

            if (!RouteDirection.IsNearlyZero())
            {
                SpawnRotation = RouteDirection.Rotation();
            }
        }
        else
        {
            ConvoyRoute = nullptr;
        }
    }

    if (UNavigationSystemV1* NavigationSystem =
        UNavigationSystemV1::GetCurrent(World))
    {
        FNavLocation ProjectedLocation;

        if (NavigationSystem->ProjectPointToNavigation(
                SpawnLocation,
                ProjectedLocation,
                FVector(1500.0f, 1500.0f, 5000.0f)))
        {
            SpawnLocation = ProjectedLocation.Location;
        }
    }

    const FTransform SpawnTransform(
        SpawnRotation,
        SpawnLocation
    );
    ABHSupplyConvoyTarget* ConvoyTarget =
        World->SpawnActorDeferred<ABHSupplyConvoyTarget>(
            SupplyConvoyTargetClass,
            SpawnTransform,
            this,
            nullptr,
            ESpawnActorCollisionHandlingMethod::
                AdjustIfPossibleButAlwaysSpawn
        );

    if (!IsValid(ConvoyTarget))
    {
        return;
    }

    ConvoyTarget->ConfigureConvoy(LocalConvoy);

    if (IsValid(ConvoyRoute))
    {
        ConvoyTarget->SetRouteChoices(
            CompatibleRoutes,
            ConvoyRoute,
            DestinationLocation
        );
    }
    else
    {
        ConvoyTarget->SetTravelDestination(
            DestinationLocation
        );
    }

    UGameplayStatics::FinishSpawningActor(
        ConvoyTarget,
        SpawnTransform
    );
    ActiveSupplyConvoyTarget = ConvoyTarget;
    PresentedSupplyConvoyIDs.Add(
        LocalConvoy.ConvoyID
    );

    FBHWarSectorState ConvoyCombatSource;
    FName ConvoyCombatSourceSectorID = NAME_None;
    int32 ConvoyCombatSourceHops = INDEX_NONE;

    if (LocalConvoy.Owner == EBHWarFaction::Enemy)
    {
        ConvoyCombatSourceSectorID =
            LocalConvoy.SourceSectorID;
        ConvoyCombatSource = WarSubsystem->GetSectorState(
            ConvoyCombatSourceSectorID
        );
        ConvoyCombatSourceHops = 0;
    }
    else
    {
        const FBHWarSectorState ContactSector =
            WarSubsystem->GetSectorState(LastPlayerSectorID);
        const bool bResolvedEnemySource =
            ResolveForceSourceSector(
                ContactSector,
                EBHWarFaction::Enemy,
                ConvoyCombatSource,
                &ConvoyCombatSourceHops
            );
        ConvoyCombatSourceSectorID = bResolvedEnemySource
            ? ConvoyCombatSource.SectorID
            : NAME_None;
    }

    const int32 RecentRouteInterdictions =
        WarSubsystem->GetRecentConvoyInterdictionCount(
            LocalConvoy.DestinationSectorID,
            ConvoyRouteSecurityTurnWindow
        );
    const int32 HostileRouteSecurityRisk =
        LocalConvoy.Owner == EBHWarFaction::Enemy
            ? RecentRouteInterdictions
            : 0;
    const FBHRouteOperationProfile RouteProfile =
        ConvoyTarget->GetRouteOperationProfile();
    const int32 RequestedCombatantCount =
        CalculateConvoyCombatantCount(
            ConvoyCombatSource,
            ConvoyCombatSourceHops,
            HostileRouteSecurityRisk,
            EBHWarFaction::Enemy
        ) + RouteProfile.AdditionalAmbushers;
    SpawnSupplyConvoyEscorts(
        ConvoyTarget,
        RequestedCombatantCount
    );
    ActiveConvoySourceSectorID =
        ConvoyCombatSourceSectorID;
    const float RequestedCombatSupply =
        CalculatePatrolSupplyCost(
            ConvoyEscorts.Num(),
            ConvoyCombatSourceHops
        );
    const float CommittedCombatSupply =
        !ActiveConvoySourceSectorID.IsNone()
            ? WarSubsystem->CommitAmbientPatrolSupply(
                ActiveConvoySourceSectorID,
                EBHWarFaction::Enemy,
                RequestedCombatSupply
            )
            : 0.0f;

    UE_LOG(
        LogTemp,
        Display,
        TEXT(
            "BH_CONVOY_FORCE_PACKAGE id=%s owner=%d "
            "source=%s source_hops=%d requested=%d "
            "spawned=%d supply_committed=%.2f "
            "source_strength=%.1f source_supply=%.1f "
            "route_risk=%d security_bonus=%d response=%.1f"
        ),
        *LocalConvoy.ConvoyID.ToString(),
        static_cast<int32>(LocalConvoy.Owner),
        *ActiveConvoySourceSectorID.ToString(),
        ConvoyCombatSourceHops,
        RequestedCombatantCount,
        ConvoyEscorts.Num(),
        CommittedCombatSupply,
        ConvoyCombatSource.EnemyStrength,
        ConvoyCombatSource.Supply,
        HostileRouteSecurityRisk,
        FMath::Min(
            FMath::Max(0, HostileRouteSecurityRisk),
            FMath::Max(0, MaximumConvoyRouteSecurityBonus)
        ),
        ConvoyCombatSource.EnemyResponsePressure
    );

    const FBHWarSectorState SourceSector =
        WarSubsystem->GetSectorState(
            LocalConvoy.SourceSectorID
        );

    if (LocalConvoy.Owner == EBHWarFaction::Friendly)
    {
        const int32 RequestedDefenderCount =
            CalculateConvoyCombatantCount(
                SourceSector,
                0,
                RecentRouteInterdictions,
                EBHWarFaction::Friendly
            );
        SpawnFriendlySupplyConvoyDefenders(
            ConvoyTarget,
            RequestedDefenderCount
        );
        ActiveConvoyDefenderSourceSectorID =
            SourceSector.SectorID;
        const float RequestedDefenderSupply =
            CalculatePatrolSupplyCost(
                ConvoyDefenders.Num(),
                0
            );
        const float CommittedDefenderSupply =
            !ActiveConvoyDefenderSourceSectorID.IsNone()
                ? WarSubsystem->CommitAmbientPatrolSupply(
                    ActiveConvoyDefenderSourceSectorID,
                    EBHWarFaction::Friendly,
                    RequestedDefenderSupply
                )
                : 0.0f;

        UE_LOG(
            LogTemp,
            Display,
            TEXT(
                "BH_CONVOY_DEFENDER_PACKAGE id=%s source=%s "
                "requested=%d spawned=%d supply_committed=%.2f "
                "source_strength=%.1f source_supply=%.1f "
                "route_risk=%d security_bonus=%d"
            ),
            *LocalConvoy.ConvoyID.ToString(),
            *ActiveConvoyDefenderSourceSectorID.ToString(),
            RequestedDefenderCount,
            ConvoyDefenders.Num(),
            CommittedDefenderSupply,
            SourceSector.FriendlyStrength,
            SourceSector.Supply,
            RecentRouteInterdictions,
            FMath::Min(
                FMath::Max(0, RecentRouteInterdictions),
                FMath::Max(
                    0,
                    MaximumConvoyRouteSecurityBonus
                )
            )
        );
    }

    const FBHWarSectorState DestinationSector =
        WarSubsystem->GetSectorState(
            LocalConvoy.DestinationSectorID
        );
    const FText SourceName = SourceSector.DisplayName.IsEmpty()
        ? FText::FromName(LocalConvoy.SourceSectorID)
        : SourceSector.DisplayName;
    const FText DestinationName =
        DestinationSector.DisplayName.IsEmpty()
            ? FText::FromName(
                LocalConvoy.DestinationSectorID
            )
            : DestinationSector.DisplayName;

    const bool bCivilianAid =
        LocalConvoy.CargoType ==
        EBHWarConvoyCargoType::CivilianAid;
    const FText ContactFormat =
        bCivilianAid
            ? NSLOCTEXT(
                "BrokenHorizon",
                "CivilianAidConvoyUnderAttack",
                "CIVILIAN AID CONVOY UNDER ATTACK\n\n"
                "{0} -> {1}\n"
                "{2} supply committed to relief cargo.\n"
                "{3} hostile raiders.\n"
                "{4} friendly guards on station.\n"
                "Defend the marked convoy until it clears "
                "the sector or the local network will receive "
                "nothing."
            )
            : LocalConvoy.Owner == EBHWarFaction::Friendly
            ? NSLOCTEXT(
                "BrokenHorizon",
                "FriendlySupplyConvoyUnderAttack",
                "FRIENDLY SUPPLY CONVOY UNDER ATTACK\n\n"
                "{0} -> {1}\n"
                "{2} supply in transit.\n"
                "{3} hostile raiders.\n"
                "{4} friendly guards on station.\n"
                "Defend the marked cargo until it clears "
                "the sector."
            )
            : NSLOCTEXT(
                "BrokenHorizon",
                "EnemySupplyConvoyDetected",
                "ENEMY SUPPLY CONVOY DETECTED\n\n"
                "{0} -> {1}\n"
                "{2} supply in transit.\n"
                "{3} armed escorts.\n"
                "Destroy the marked cargo before it clears "
                "the sector."
            );
    PlayerCharacter->ShowStatusNotification(
        FText::Format(
            ContactFormat,
            SourceName,
            DestinationName,
            FText::AsNumber(
                FMath::RoundToInt(LocalConvoy.SupplyPayload)
            ),
            FText::AsNumber(ConvoyEscorts.Num()),
            FText::AsNumber(ConvoyDefenders.Num())
        )
    );

    UE_LOG(
        LogTemp,
        Display,
        TEXT(
            "BH_CONVOY_OPPORTUNITY_SPAWNED id=%s "
            "owner=%d sector=%s location=%s destination=%s "
            "distance=%.0f route=%s mode=%s"
        ),
        *LocalConvoy.ConvoyID.ToString(),
        static_cast<int32>(LocalConvoy.Owner),
        *LastPlayerSectorID.ToString(),
        *SpawnLocation.ToCompactString(),
        *DestinationLocation.ToCompactString(),
        FVector::Dist2D(PlayerLocation, SpawnLocation),
        IsValid(ConvoyRoute)
            ? *ConvoyRoute->GetRouteID().ToString()
            : TEXT("None"),
        IsValid(ConvoyRoute)
            ? TEXT("spline")
            : TEXT("direct")
    );
}

void ABHAmbientWarDirector::SpawnSupplyConvoyEscorts(
    ABHSupplyConvoyTarget* ConvoyTarget,
    int32 RequestedCombatantCount
)
{
    CleanupSupplyConvoyEscorts();

    UWorld* World = GetWorld();
    UNavigationSystemV1* NavigationSystem =
        FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
    const int32 SafeEscortCount = FMath::Clamp(
        RequestedCombatantCount,
        0,
        4
    );

    if (!IsValid(World) ||
        !IsValid(NavigationSystem) ||
        !IsValid(ConvoyTarget) ||
        !EnemyClass)
    {
        return;
    }

    const FVector ConvoyLocation = ConvoyTarget->GetActorLocation();
    const FVector ConvoyForward =
        ConvoyTarget->GetActorForwardVector().GetSafeNormal2D();
    const FVector ConvoyRight(
        -ConvoyForward.Y,
        ConvoyForward.X,
        0.0f
    );

    for (int32 PointIndex = 0;
        PointIndex < 3;
        ++PointIndex)
    {
        const float AngleDegrees =
            120.0f * static_cast<float>(PointIndex);
        const FVector PointDirection =
            (ConvoyForward *
                FMath::Cos(FMath::DegreesToRadians(AngleDegrees))) +
            (ConvoyRight *
                FMath::Sin(FMath::DegreesToRadians(AngleDegrees)));
        const FVector Candidate =
            ConvoyLocation +
            (PointDirection *
                FMath::Max(100.0f, ConvoyEscortSpacing));
        FNavLocation PatrolLocation;

        if (!NavigationSystem->ProjectPointToNavigation(
                Candidate,
                PatrolLocation,
                FVector(800.0f, 800.0f, 2000.0f)))
        {
            continue;
        }

        FActorSpawnParameters SpawnParameters;
        SpawnParameters.SpawnCollisionHandlingOverride =
            ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        SpawnParameters.ObjectFlags |= RF_Transient;

        ABHPatrolPoint* PatrolPoint =
            World->SpawnActor<ABHPatrolPoint>(
                ABHPatrolPoint::StaticClass(),
                PatrolLocation.Location,
                FRotator::ZeroRotator,
                SpawnParameters
            );

        if (IsValid(PatrolPoint))
        {
            PatrolPoint->AttachToActor(
                ConvoyTarget,
                FAttachmentTransformRules::KeepWorldTransform
            );
            ConvoyEscortPatrolPoints.Add(PatrolPoint);
        }
    }

    if (ConvoyEscortPatrolPoints.IsEmpty() ||
        SafeEscortCount <= 0)
    {
        return;
    }

    for (int32 EscortIndex = 0;
        EscortIndex < SafeEscortCount;
        ++EscortIndex)
    {
        const float CenteredIndex =
            static_cast<float>(EscortIndex) -
            (static_cast<float>(SafeEscortCount - 1) * 0.5f);
        const FVector PreferredSpawnLocation =
            ConvoyLocation -
            (ConvoyForward * 250.0f) +
            (ConvoyRight *
                CenteredIndex *
                FMath::Max(100.0f, ConvoyEscortSpacing));
        FNavLocation SpawnLocation;

        if (!NavigationSystem->ProjectPointToNavigation(
                PreferredSpawnLocation,
                SpawnLocation,
                FVector(800.0f, 800.0f, 2000.0f)))
        {
            continue;
        }

        const FTransform SpawnTransform(
            ConvoyForward.Rotation(),
            SpawnLocation.Location
        );
        ABHEnemySoldier* Escort =
            World->SpawnActorDeferred<ABHEnemySoldier>(
                EnemyClass,
                SpawnTransform,
                this,
                nullptr,
                ESpawnActorCollisionHandlingMethod::
                    AdjustIfPossibleButAlwaysSpawn
            );

        if (!IsValid(Escort))
        {
            continue;
        }

        Escort->SetFlags(RF_Transient);
        Escort->SetCombatFaction(EBHCombatFaction::Hostile);
        Escort->SetCombatantArchetype(
            ABHEnemySoldier::ChooseFormationArchetype(
                EscortIndex,
                SafeEscortCount
            )
        );
        Escort->SetObjectiveIdToCompleteOnDeath(NAME_None);

        TArray<ABHPatrolPoint*> AssignedPatrolPoints;
        AssignedPatrolPoints.Reserve(
            ConvoyEscortPatrolPoints.Num()
        );

        for (int32 Offset = 0;
            Offset < ConvoyEscortPatrolPoints.Num();
            ++Offset)
        {
            const int32 PatrolIndex =
                (EscortIndex + Offset) %
                ConvoyEscortPatrolPoints.Num();
            AssignedPatrolPoints.Add(
                ConvoyEscortPatrolPoints[PatrolIndex]
            );
        }

        Escort->SetPatrolPoints(AssignedPatrolPoints);
        UGameplayStatics::FinishSpawningActor(
            Escort,
            SpawnTransform
        );
        ConvoyEscorts.Add(Escort);
    }

    UE_LOG(
        LogTemp,
        Display,
        TEXT(
            "BH_CONVOY_ESCORTS_DEPLOYED id=%s "
            "count=%d patrol_points=%d"
        ),
        *ConvoyTarget->GetConvoyID().ToString(),
        ConvoyEscorts.Num(),
        ConvoyEscortPatrolPoints.Num()
    );
}

void ABHAmbientWarDirector::
SpawnFriendlySupplyConvoyDefenders(
    ABHSupplyConvoyTarget* ConvoyTarget,
    int32 RequestedDefenderCount
)
{
    UWorld* World = GetWorld();
    UNavigationSystemV1* NavigationSystem =
        FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
    const int32 SafeDefenderCount = FMath::Clamp(
        RequestedDefenderCount,
        0,
        4
    );

    if (!IsValid(World) ||
        !IsValid(NavigationSystem) ||
        !IsValid(ConvoyTarget) ||
        !EnemyClass ||
        ConvoyEscortPatrolPoints.IsEmpty() ||
        SafeDefenderCount <= 0)
    {
        return;
    }

    const FVector ConvoyLocation =
        ConvoyTarget->GetActorLocation();
    const FVector ConvoyForward =
        ConvoyTarget->GetActorForwardVector().GetSafeNormal2D();
    const FVector ConvoyRight(
        -ConvoyForward.Y,
        ConvoyForward.X,
        0.0f
    );

    for (int32 DefenderIndex = 0;
        DefenderIndex < SafeDefenderCount;
        ++DefenderIndex)
    {
        const float CenteredIndex =
            static_cast<float>(DefenderIndex) -
            (
                static_cast<float>(SafeDefenderCount - 1) *
                0.5f
            );
        const FVector PreferredSpawnLocation =
            ConvoyLocation +
            (ConvoyForward * 300.0f) +
            (
                ConvoyRight *
                CenteredIndex *
                FMath::Max(100.0f, ConvoyEscortSpacing)
            );
        FNavLocation SpawnLocation;

        if (!NavigationSystem->ProjectPointToNavigation(
                PreferredSpawnLocation,
                SpawnLocation,
                FVector(800.0f, 800.0f, 2000.0f)))
        {
            continue;
        }

        const FTransform SpawnTransform(
            (-ConvoyForward).Rotation(),
            SpawnLocation.Location
        );
        ABHEnemySoldier* Defender =
            World->SpawnActorDeferred<ABHEnemySoldier>(
                EnemyClass,
                SpawnTransform,
                this,
                nullptr,
                ESpawnActorCollisionHandlingMethod::
                    AdjustIfPossibleButAlwaysSpawn
            );

        if (!IsValid(Defender))
        {
            continue;
        }

        Defender->SetFlags(RF_Transient);
        Defender->SetCombatFaction(
            EBHCombatFaction::Friendly
        );
        Defender->SetCombatantArchetype(
            ABHEnemySoldier::ChooseFormationArchetype(
                DefenderIndex,
                SafeDefenderCount
            )
        );
        Defender->SetObjectiveIdToCompleteOnDeath(NAME_None);

        TArray<ABHPatrolPoint*> AssignedPatrolPoints;
        AssignedPatrolPoints.Reserve(
            ConvoyEscortPatrolPoints.Num()
        );

        for (int32 Offset = 0;
            Offset < ConvoyEscortPatrolPoints.Num();
            ++Offset)
        {
            const int32 PatrolIndex =
                (
                    ConvoyEscortPatrolPoints.Num() -
                    1 -
                    (
                        (DefenderIndex + Offset) %
                        ConvoyEscortPatrolPoints.Num()
                    )
                );
            AssignedPatrolPoints.Add(
                ConvoyEscortPatrolPoints[PatrolIndex]
            );
        }

        Defender->SetPatrolPoints(AssignedPatrolPoints);
        UGameplayStatics::FinishSpawningActor(
            Defender,
            SpawnTransform
        );
        ConvoyDefenders.Add(Defender);
    }

    UE_LOG(
        LogTemp,
        Display,
        TEXT(
            "BH_CONVOY_DEFENDERS_DEPLOYED id=%s "
            "count=%d patrol_points=%d"
        ),
        *ConvoyTarget->GetConvoyID().ToString(),
        ConvoyDefenders.Num(),
        ConvoyEscortPatrolPoints.Num()
    );
}

void ABHAmbientWarDirector::UpdateSupplyConvoyEscorts()
{
    int32 EscortCasualties = 0;
    int32 DefenderCasualties = 0;

    for (int32 Index = ConvoyEscorts.Num() - 1;
        Index >= 0;
        --Index)
    {
        ABHEnemySoldier* Escort = ConvoyEscorts[Index];

        if (!IsValid(Escort))
        {
            ConvoyEscorts.RemoveAtSwap(Index);
            continue;
        }

        if (Escort->IsDead())
        {
            ++EscortCasualties;
            ConvoyEscorts.RemoveAtSwap(Index);
        }
    }

    for (int32 Index = ConvoyDefenders.Num() - 1;
        Index >= 0;
        --Index)
    {
        ABHEnemySoldier* Defender = ConvoyDefenders[Index];

        if (!IsValid(Defender))
        {
            ConvoyDefenders.RemoveAtSwap(Index);
            continue;
        }

        if (Defender->IsDead())
        {
            ++DefenderCasualties;
            ConvoyDefenders.RemoveAtSwap(Index);
        }
    }

    ReportSupplyConvoyEscortCasualties(EscortCasualties);
    ReportSupplyConvoyDefenderCasualties(
        DefenderCasualties
    );

    if (IsValid(ActiveSupplyConvoyTarget))
    {
        ConvoyEscortWithdrawalTime = -1.0f;
        return;
    }

    if (ConvoyEscorts.IsEmpty() &&
        ConvoyDefenders.IsEmpty())
    {
        CleanupSupplyConvoyEscorts();
        return;
    }

    UWorld* World = GetWorld();

    if (!IsValid(World))
    {
        return;
    }

    bool bEscortInCombat = false;

    for (ABHEnemySoldier* Escort : ConvoyEscorts)
    {
        const ABHEnemyAIController* Controller =
            IsValid(Escort)
                ? Cast<ABHEnemyAIController>(
                    Escort->GetController()
                )
                : nullptr;

        if (IsValid(Controller) &&
            Controller->GetCurrentState() ==
                EBHEnemyAIState::Combat)
        {
            bEscortInCombat = true;
            break;
        }
    }

    if (!bEscortInCombat)
    {
        for (ABHEnemySoldier* Defender : ConvoyDefenders)
        {
            const ABHEnemyAIController* Controller =
                IsValid(Defender)
                    ? Cast<ABHEnemyAIController>(
                        Defender->GetController()
                    )
                    : nullptr;

            if (IsValid(Controller) &&
                Controller->GetCurrentState() ==
                    EBHEnemyAIState::Combat)
            {
                bEscortInCombat = true;
                break;
            }
        }
    }

    const float CurrentTime = World->GetTimeSeconds();

    if (bEscortInCombat ||
        ConvoyEscortWithdrawalTime < 0.0f)
    {
        ConvoyEscortWithdrawalTime =
            CurrentTime +
            FMath::Max(0.0f, ConvoyEscortWithdrawalDelay);
        return;
    }

    if (CurrentTime >= ConvoyEscortWithdrawalTime)
    {
        UE_LOG(
            LogTemp,
            Display,
            TEXT(
                "BH_CONVOY_ESCORTS_WITHDREW hostile=%d "
                "friendly=%d"
            ),
            ConvoyEscorts.Num(),
            ConvoyDefenders.Num()
        );
        CleanupSupplyConvoyEscorts();
    }
}

void ABHAmbientWarDirector::CleanupSupplyConvoyEscorts()
{
    for (ABHEnemySoldier* Escort : ConvoyEscorts)
    {
        if (IsValid(Escort))
        {
            Escort->Destroy();
        }
    }

    for (ABHEnemySoldier* Defender : ConvoyDefenders)
    {
        if (IsValid(Defender))
        {
            Defender->Destroy();
        }
    }

    for (ABHPatrolPoint* PatrolPoint :
        ConvoyEscortPatrolPoints)
    {
        if (IsValid(PatrolPoint))
        {
            PatrolPoint->Destroy();
        }
    }

    ConvoyEscorts.Reset();
    ConvoyDefenders.Reset();
    ConvoyEscortPatrolPoints.Reset();
    ConvoyEscortWithdrawalTime = -1.0f;
    ActiveConvoySourceSectorID = NAME_None;
    ActiveConvoyDefenderSourceSectorID = NAME_None;
}

void ABHAmbientWarDirector::
ReportSupplyConvoyEscortCasualties(
    int32 EscortCasualties
)
{
    if (EscortCasualties <= 0 ||
        ActiveConvoySourceSectorID.IsNone())
    {
        return;
    }

    UGameInstance* GameInstance = GetGameInstance();
    UBHWarSubsystem* WarSubsystem = GameInstance
        ? GameInstance->GetSubsystem<UBHWarSubsystem>()
        : nullptr;

    if (!IsValid(WarSubsystem))
    {
        return;
    }

    WarSubsystem->ApplyAmbientBattleResult(
        ActiveConvoySourceSectorID,
        0,
        EscortCasualties
    );

    UE_LOG(
        LogTemp,
        Display,
        TEXT(
            "BH_CONVOY_ESCORT_CASUALTIES source=%s "
            "enemy_casualties=%d"
        ),
        *ActiveConvoySourceSectorID.ToString(),
        EscortCasualties
    );
}

void ABHAmbientWarDirector::
ReportSupplyConvoyDefenderCasualties(
    int32 DefenderCasualties
)
{
    if (DefenderCasualties <= 0 ||
        ActiveConvoyDefenderSourceSectorID.IsNone())
    {
        return;
    }

    UGameInstance* GameInstance = GetGameInstance();
    UBHWarSubsystem* WarSubsystem = GameInstance
        ? GameInstance->GetSubsystem<UBHWarSubsystem>()
        : nullptr;

    if (!IsValid(WarSubsystem))
    {
        return;
    }

    WarSubsystem->ApplyAmbientBattleResult(
        ActiveConvoyDefenderSourceSectorID,
        DefenderCasualties,
        0
    );

    UE_LOG(
        LogTemp,
        Display,
        TEXT(
            "BH_CONVOY_DEFENDER_CASUALTIES source=%s "
            "friendly_casualties=%d"
        ),
        *ActiveConvoyDefenderSourceSectorID.ToString(),
        DefenderCasualties
    );
}

bool ABHAmbientWarDirector::IsFrontlineSector(
    const FBHWarSectorState& SectorState
) const
{
    const UGameInstance* GameInstance = GetGameInstance();
    const UBHWarSubsystem* WarSubsystem = GameInstance
        ? GameInstance->GetSubsystem<UBHWarSubsystem>()
        : nullptr;

    if (!IsValid(WarSubsystem))
    {
        return false;
    }

    for (const FName ConnectedSectorID :
        SectorState.ConnectedSectorIDs)
    {
        const FBHWarSectorState ConnectedSector =
            WarSubsystem->GetSectorState(ConnectedSectorID);

        if (!ConnectedSector.SectorID.IsNone() &&
            ConnectedSector.Owner != SectorState.Owner)
        {
            return true;
        }
    }

    return false;
}

bool ABHAmbientWarDirector::ResolveForceSourceSector(
    const FBHWarSectorState& ContactSector,
    EBHWarFaction ForceFaction,
    FBHWarSectorState& OutSourceSector,
    int32* OutHopCount
) const
{
    OutSourceSector = FBHWarSectorState();

    if (OutHopCount)
    {
        *OutHopCount = INDEX_NONE;
    }

    const UGameInstance* GameInstance = GetGameInstance();
    const UBHWarSubsystem* WarSubsystem = GameInstance
        ? GameInstance->GetSubsystem<UBHWarSubsystem>()
        : nullptr;

    if (!IsValid(WarSubsystem) ||
        ContactSector.SectorID.IsNone() ||
        (ForceFaction != EBHWarFaction::Friendly &&
            ForceFaction != EBHWarFaction::Enemy))
    {
        return false;
    }

    TArray<FName> Frontier;
    TSet<FName> VisitedSectorIDs;
    Frontier.Add(ContactSector.SectorID);

    const EBHWarFaction OpposingFaction =
        ForceFaction == EBHWarFaction::Friendly
            ? EBHWarFaction::Enemy
            : EBHWarFaction::Friendly;
    const int32 SafeMaximumHops =
        FMath::Max(0, MaxForceProjectionHops);

    for (int32 HopCount = 0;
        HopCount <= SafeMaximumHops &&
            !Frontier.IsEmpty();
        ++HopCount)
    {
        TArray<FName> NextFrontier;
        FBHWarSectorState BestSource;
        float BestSourceScore = -1.0f;

        for (const FName SectorID : Frontier)
        {
            if (SectorID.IsNone() ||
                VisitedSectorIDs.Contains(SectorID))
            {
                continue;
            }

            VisitedSectorIDs.Add(SectorID);

            const FBHWarSectorState Candidate =
                WarSubsystem->GetSectorState(SectorID);

            if (Candidate.SectorID.IsNone())
            {
                continue;
            }

            const float CandidateStrength =
                ForceFaction == EBHWarFaction::Friendly
                    ? Candidate.FriendlyStrength
                    : Candidate.EnemyStrength;
            const int32 CandidateGarrison =
                ForceFaction == EBHWarFaction::Friendly
                    ? Candidate.FriendlyGarrison
                    : Candidate.EnemyGarrison;

            if (CandidateStrength >= MinimumPatrolStrength &&
                CandidateGarrison > 0)
            {
                const bool bOwnedByForce =
                    Candidate.Owner == ForceFaction;
                const float CandidateScore =
                    CandidateStrength +
                    (bOwnedByForce ? 1000.0f : 0.0f);

                if (CandidateScore > BestSourceScore)
                {
                    BestSource = Candidate;
                    BestSourceScore = CandidateScore;
                }
            }

            const bool bMayProjectThroughSector =
                Candidate.SectorID == ContactSector.SectorID ||
                Candidate.Owner != OpposingFaction;

            if (bMayProjectThroughSector)
            {
                for (const FName ConnectedSectorID :
                    Candidate.ConnectedSectorIDs)
                {
                    if (!ConnectedSectorID.IsNone() &&
                        !VisitedSectorIDs.Contains(
                            ConnectedSectorID))
                    {
                        NextFrontier.AddUnique(
                            ConnectedSectorID
                        );
                    }
                }
            }
        }

        if (!BestSource.SectorID.IsNone())
        {
            OutSourceSector = BestSource;

            if (OutHopCount)
            {
                *OutHopCount = HopCount;
            }

            return true;
        }

        Frontier = MoveTemp(NextFrontier);
    }

    return false;
}

float ABHAmbientWarDirector::CalculatePatrolSupplyCost(
    int32 MemberCount,
    int32 SourceHops
) const
{
    const int32 SafeMemberCount = FMath::Max(0, MemberCount);
    const int32 SafeSourceHops = FMath::Max(0, SourceHops);
    const float PerMemberCost = FMath::Max(
        0.0f,
        PatrolSupplyCostPerMember
    );
    const float ProjectionCost = FMath::Max(
        0.0f,
        PatrolProjectionSupplyCostPerHop
    ) * static_cast<float>(SafeSourceHops);

    return static_cast<float>(SafeMemberCount) *
        (PerMemberCost + ProjectionCost);
}

int32 ABHAmbientWarDirector::CalculateConvoyCombatantCount(
    const FBHWarSectorState& SourceSector,
    int32 SourceHops,
    int32 RecentRouteInterdictions,
    EBHWarFaction CombatantFaction
) const
{
    const int32 MaximumCombatants = 4;
    const int32 RouteSecurityBonus = FMath::Min(
        FMath::Max(0, RecentRouteInterdictions),
        FMath::Max(0, MaximumConvoyRouteSecurityBonus)
    );
    const int32 ResponseSecurityBonus =
        CombatantFaction == EBHWarFaction::Enemy
            ? SourceSector.EnemyResponsePressure >= 75.0f
                ? 2
                : SourceSector.EnemyResponsePressure >= 50.0f
                    ? 1
                    : 0
            : 0;
    const int32 BaseCombatants = FMath::Clamp(
        ConvoyEscortCount +
            RouteSecurityBonus +
            ResponseSecurityBonus,
        0,
        MaximumCombatants
    );

    if (BaseCombatants <= 0 ||
        SourceSector.SectorID.IsNone())
    {
        return 0;
    }

    const int32 SupplyAdjustedCount =
        AdjustControlledPatrolCount(
            BaseCombatants,
            MaximumCombatants,
            SourceSector.Supply
        );
    const int32 StrengthLimitedCount =
        LimitPatrolCountByStrength(
            SupplyAdjustedCount,
            MaximumCombatants,
            CombatantFaction == EBHWarFaction::Friendly
                ? SourceSector.FriendlyStrength
                : SourceSector.EnemyStrength
        );

    const int32 GarrisonLimitedCount = FMath::Min(
        StrengthLimitedCount,
        CombatantFaction == EBHWarFaction::Friendly
            ? SourceSector.FriendlyGarrison
            : SourceSector.EnemyGarrison
    );

    return LimitPatrolCountBySupply(
        GarrisonLimitedCount,
        MaximumCombatants,
        SourceSector.Supply,
        CalculatePatrolSupplyCost(1, SourceHops)
    );
}

int32 ABHAmbientWarDirector::GetDesiredEnemyCount(
    const FBHWarSectorState& SectorState
) const
{
    FBHWarSectorState SourceSector;
    int32 SourceHops = INDEX_NONE;

    if (!ResolveForceSourceSector(
            SectorState,
            EBHWarFaction::Enemy,
            SourceSector,
            &SourceHops))
    {
        return 0;
    }

    int32 DesiredCount = 0;

    if (SectorState.Owner == EBHWarFaction::Enemy)
    {
        const int32 BaseCount = FMath::Clamp(
            FMath::CeilToInt(
                SourceSector.EnemyStrength /
                PatrolStrengthPerMember
            ),
            2,
            MaxActiveEnemies
        );

        DesiredCount = AdjustControlledPatrolCount(
            BaseCount,
            MaxActiveEnemies,
            SourceSector.Supply
        );
    }
    else if (SectorState.Owner == EBHWarFaction::Neutral)
    {
        DesiredCount = SectorState.EnemyStrength >=
                SectorState.FriendlyStrength
            ? FMath::Min(2, MaxActiveEnemies)
            : 0;
    }
    else if (
        SectorState.Owner == EBHWarFaction::Friendly &&
        IsFrontlineSector(SectorState))
    {
        const UGameInstance* GameInstance = GetGameInstance();
        const UBHWarSubsystem* WarSubsystem = GameInstance
            ? GameInstance->GetSubsystem<UBHWarSubsystem>()
            : nullptr;
        const bool bPriorityDefense =
            IsValid(WarSubsystem) &&
            WarSubsystem->GetPrioritySectorID() ==
                SectorState.SectorID &&
            WarSubsystem->GetPriorityType() ==
                EBHWarPriorityType::Defend;

        DesiredCount = AdjustControlledPatrolCount(
            FMath::Min(
                bPriorityDefense ? 3 : 2,
                MaxActiveEnemies
            ),
            MaxActiveEnemies,
            SourceSector.Supply
        );
    }
    else if (SectorState.Owner == EBHWarFaction::Friendly)
    {
        DesiredCount = 1;
    }

    const int32 ResponseAdjustment =
        SectorState.EnemyResponsePressure >= 75.0f
            ? 2
            : SectorState.EnemyResponsePressure >= 50.0f
                ? 1
                : 0;
    const int32 PopulationAdjustment =
        SectorState.CivilianSupport <= 25.0f
            ? 1
            : SectorState.CivilianSupport >= 70.0f
                ? -1
                : 0;
    DesiredCount = FMath::Min(
        FMath::Max(
            0,
            DesiredCount +
                ResponseAdjustment +
                PopulationAdjustment
        ),
        MaxActiveEnemies
    );

    const int32 StrengthLimitedCount =
        LimitPatrolCountByStrength(
            DesiredCount,
            MaxActiveEnemies,
            SourceSector.EnemyStrength
        );

    const int32 GarrisonLimitedCount = FMath::Min(
        StrengthLimitedCount,
        SourceSector.EnemyGarrison
    );

    return LimitPatrolCountBySupply(
        GarrisonLimitedCount,
        MaxActiveEnemies,
        SourceSector.Supply,
        CalculatePatrolSupplyCost(1, SourceHops)
    );
}

int32 ABHAmbientWarDirector::GetDesiredFriendlyCount(
    const FBHWarSectorState& SectorState
) const
{
    FBHWarSectorState SourceSector;
    int32 SourceHops = INDEX_NONE;

    if (!ResolveForceSourceSector(
            SectorState,
            EBHWarFaction::Friendly,
            SourceSector,
            &SourceHops))
    {
        return 0;
    }

    int32 DesiredCount = 0;

    if (SectorState.Owner == EBHWarFaction::Friendly)
    {
        DesiredCount = AdjustControlledPatrolCount(
            FMath::Min(2, MaxFriendlyPatrolMembers),
            MaxFriendlyPatrolMembers,
            SourceSector.Supply
        );
    }
    else if (SectorState.Owner == EBHWarFaction::Neutral)
    {
        DesiredCount = FMath::Min(
            1,
            MaxFriendlyPatrolMembers
        );
    }
    else if (
        SectorState.Owner == EBHWarFaction::Enemy &&
        (IsFrontlineSector(SectorState) ||
            SectorState.FriendlyStrength >= 15.0f))
    {
        DesiredCount = AdjustControlledPatrolCount(
            FMath::CeilToInt(
                SourceSector.FriendlyStrength /
                PatrolStrengthPerMember
            ),
            MaxFriendlyPatrolMembers,
            SourceSector.Supply
        );
    }

    const int32 PopulationAdjustment =
        SectorState.CivilianSupport >= 70.0f
            ? 1
            : SectorState.CivilianSupport <= 25.0f
                ? -1
                : 0;
    DesiredCount = FMath::Clamp(
        DesiredCount + PopulationAdjustment,
        0,
        MaxFriendlyPatrolMembers
    );

    const int32 StrengthLimitedCount =
        LimitPatrolCountByStrength(
            DesiredCount,
            MaxFriendlyPatrolMembers,
            SourceSector.FriendlyStrength
        );

    const int32 GarrisonLimitedCount = FMath::Min(
        StrengthLimitedCount,
        SourceSector.FriendlyGarrison
    );

    return LimitPatrolCountBySupply(
        GarrisonLimitedCount,
        MaxFriendlyPatrolMembers,
        SourceSector.Supply,
        CalculatePatrolSupplyCost(1, SourceHops)
    );
}

bool ABHAmbientWarDirector::HasAssignedOperation() const
{
    for (TActorIterator<ABHOpenWorldOperationDirector> It(
            GetWorld());
        It;
        ++It)
    {
        if (IsValid(*It) && It->IsOperationInProgress())
        {
            return true;
        }
    }

    return false;
}

ABHWorldRoute* ABHAmbientWarDirector::FindNearestWorldRoute(
    const FVector& WorldLocation
) const
{
    ABHWorldRoute* BestRoute = nullptr;
    float BestDistanceSquared = BIG_NUMBER;

    for (TActorIterator<ABHWorldRoute> It(GetWorld()); It; ++It)
    {
        ABHWorldRoute* Candidate = *It;

        if (!IsValid(Candidate) ||
            Candidate->GetRouteLength() <= 0.0f)
        {
            continue;
        }

        const float RouteDistance =
            Candidate->
                GetDistanceAlongRouteClosestToWorldLocation(
                    WorldLocation
                );
        const FVector RouteLocation =
            Candidate->GetWorldLocationAtDistance(RouteDistance);
        const float DistanceSquared = FVector::DistSquared2D(
            WorldLocation,
            RouteLocation
        );

        if (DistanceSquared < BestDistanceSquared)
        {
            BestDistanceSquared = DistanceSquared;
            BestRoute = Candidate;
        }
    }

    return BestRoute;
}

ABHWorldRoute* ABHAmbientWarDirector::FindBestWorldRoute(
    const FVector& SourceLocation,
    const FVector& DestinationLocation
) const
{
    ABHWorldRoute* BestRoute = nullptr;
    float BestConnectionScore = BIG_NUMBER;
    const float ConnectionTolerance = FMath::Max(
        0.0f,
        ConvoyRouteConnectionTolerance
    );

    for (TActorIterator<ABHWorldRoute> It(GetWorld()); It; ++It)
    {
        ABHWorldRoute* Candidate = *It;

        if (!IsValid(Candidate) ||
            Candidate->GetRouteLength() <= 0.0f)
        {
            continue;
        }

        const float SourceRouteDistance =
            Candidate->
                GetDistanceAlongRouteClosestToWorldLocation(
                    SourceLocation
                );
        const float DestinationRouteDistance =
            Candidate->
                GetDistanceAlongRouteClosestToWorldLocation(
                    DestinationLocation
                );
        const FVector SourceRouteLocation =
            Candidate->GetWorldLocationAtDistance(
                SourceRouteDistance
            );
        const FVector DestinationRouteLocation =
            Candidate->GetWorldLocationAtDistance(
                DestinationRouteDistance
            );
        const float SourceConnectionDistance = FVector::Dist2D(
            SourceLocation,
            SourceRouteLocation
        );
        const float DestinationConnectionDistance = FVector::Dist2D(
            DestinationLocation,
            DestinationRouteLocation
        );
        const float RouteSegmentLength = FMath::Abs(
            DestinationRouteDistance - SourceRouteDistance
        );

        if (SourceConnectionDistance > ConnectionTolerance ||
            DestinationConnectionDistance > ConnectionTolerance ||
            RouteSegmentLength <= 1000.0f)
        {
            continue;
        }

        const float ConnectionScore =
            SourceConnectionDistance +
            DestinationConnectionDistance;

        if (ConnectionScore < BestConnectionScore)
        {
            BestConnectionScore = ConnectionScore;
            BestRoute = Candidate;
        }
    }

    return BestRoute;
}

TArray<ABHWorldRoute*>
ABHAmbientWarDirector::FindCompatibleWorldRoutes(
    const FVector& SourceLocation,
    const FVector& DestinationLocation
) const
{
    struct FRouteCandidate
    {
        ABHWorldRoute* Route = nullptr;
        float ConnectionScore = BIG_NUMBER;
    };

    TArray<FRouteCandidate> Candidates;
    const float ConnectionTolerance = FMath::Max(
        0.0f,
        ConvoyRouteConnectionTolerance
    );

    for (TActorIterator<ABHWorldRoute> It(GetWorld()); It; ++It)
    {
        ABHWorldRoute* Candidate = *It;
        if (!IsValid(Candidate) ||
            Candidate->GetRouteLength() <= KINDA_SMALL_NUMBER)
        {
            continue;
        }

        const float SourceDistance =
            Candidate->GetDistanceAlongRouteClosestToWorldLocation(
                SourceLocation
            );
        const float DestinationDistance =
            Candidate->GetDistanceAlongRouteClosestToWorldLocation(
                DestinationLocation
            );
        const float SourceConnection = FVector::Dist2D(
            SourceLocation,
            Candidate->GetWorldLocationAtDistance(SourceDistance)
        );
        const float DestinationConnection = FVector::Dist2D(
            DestinationLocation,
            Candidate->GetWorldLocationAtDistance(DestinationDistance)
        );

        if (SourceConnection <= ConnectionTolerance &&
            DestinationConnection <= ConnectionTolerance &&
            FMath::Abs(DestinationDistance - SourceDistance) > 1000.0f)
        {
            Candidates.Add({
                Candidate,
                SourceConnection + DestinationConnection
            });
        }
    }

    Candidates.Sort([](
        const FRouteCandidate& Left,
        const FRouteCandidate& Right)
    {
        if (!FMath::IsNearlyEqual(
                Left.ConnectionScore,
                Right.ConnectionScore))
        {
            return Left.ConnectionScore < Right.ConnectionScore;
        }
        return Left.Route->GetRouteID().LexicalLess(
            Right.Route->GetRouteID()
        );
    });

    TArray<ABHWorldRoute*> Result;
    for (const FRouteCandidate& Candidate : Candidates)
    {
        Result.Add(Candidate.Route);
    }
    return Result;
}

bool ABHAmbientWarDirector::TryBuildRoutePatrolCenter(
    const ABHCharacter* PlayerCharacter,
    UNavigationSystemV1* NavigationSystem,
    FVector& OutPatrolCenter
) const
{
    if (!IsValid(PlayerCharacter) ||
        !IsValid(NavigationSystem))
    {
        return false;
    }

    const FVector PlayerLocation =
        PlayerCharacter->GetActorLocation();
    const FVector PlayerForward =
        PlayerCharacter->GetActorForwardVector().GetSafeNormal2D();
    ABHWorldRoute* Route =
        FindNearestWorldRoute(PlayerLocation);

    if (!IsValid(Route))
    {
        UE_LOG(
            LogTemp,
            Warning,
            TEXT(
                "BH_AMBIENT_ROUTE_FALLBACK reason=no_route "
                "player_x=%.0f player_y=%.0f"
            ),
            PlayerLocation.X,
            PlayerLocation.Y
        );
        return false;
    }

    const float RouteLength = Route->GetRouteLength();
    const float ClosestRouteDistance =
        Route->GetDistanceAlongRouteClosestToWorldLocation(
            PlayerLocation
        );
    const float MinimumDistance = FMath::Min(
        MinimumSpawnDistance,
        MaximumSpawnDistance
    );
    const float MaximumDistance = FMath::Max(
        MinimumSpawnDistance,
        MaximumSpawnDistance
    );

    for (int32 Attempt = 0; Attempt < 12; ++Attempt)
    {
        const float RouteDirection =
            FMath::RandBool() ? 1.0f : -1.0f;
        const float RouteOffset = FMath::FRandRange(
            MinimumDistance,
            MaximumDistance
        );
        const float CandidateDistance = FMath::Clamp(
            ClosestRouteDistance +
                (RouteDirection * RouteOffset),
            0.0f,
            RouteLength
        );
        const FVector RouteLocation =
            Route->GetWorldLocationAtDistance(
                CandidateDistance
            );
        const FVector RouteForward =
            Route->GetWorldDirectionAtDistance(
                CandidateDistance
            ).GetSafeNormal2D();
        const FVector RouteRight(
            -RouteForward.Y,
            RouteForward.X,
            0.0f
        );
        const float SignedLateralOffset =
            FMath::FRandRange(
                RouteLateralOffset * 0.5f,
                RouteLateralOffset
            ) *
            (FMath::RandBool() ? 1.0f : -1.0f);
        const FVector Candidate =
            RouteLocation +
            (RouteRight * SignedLateralOffset);
        const FVector PlayerToCandidate =
            (Candidate - PlayerLocation).GetSafeNormal2D();

        if (FVector::DistSquared2D(
                PlayerLocation,
                Candidate
            ) <
            FMath::Square(MinimumDistance * 0.75f))
        {
            continue;
        }

        if (Attempt < 8 &&
            FVector::DotProduct(
                PlayerForward,
                PlayerToCandidate
            ) > 0.35f)
        {
            continue;
        }

        FNavLocation ProjectedLocation;

        if (!NavigationSystem->ProjectPointToNavigation(
                Candidate,
                ProjectedLocation,
                FVector(2500.0f, 2500.0f, 15000.0f)))
        {
            continue;
        }

        OutPatrolCenter = ProjectedLocation.Location;

        UE_LOG(
            LogTemp,
            Display,
            TEXT(
                "BH_AMBIENT_ROUTE_CONTACT route=%s "
                "distance=%.0f lateral=%.0f"
            ),
            *Route->GetRouteID().ToString(),
            CandidateDistance,
            SignedLateralOffset
        );
        return true;
    }

    UE_LOG(
        LogTemp,
        Display,
        TEXT(
            "BH_AMBIENT_ROUTE_FALLBACK reason=no_nav fallback=local "
            "route=%s route_length=%.0f closest_distance=%.0f"
        ),
        *Route->GetRouteID().ToString(),
        RouteLength,
        ClosestRouteDistance
    );
    return false;
}

bool ABHAmbientWarDirector::TryBuildPatrolCenter(
    const ABHCharacter* PlayerCharacter,
    FVector& OutPatrolCenter
) const
{
    if (!IsValid(PlayerCharacter))
    {
        return false;
    }

    UNavigationSystemV1* NavigationSystem =
        FNavigationSystem::GetCurrent<UNavigationSystemV1>(
            GetWorld()
        );

    if (!IsValid(NavigationSystem))
    {
        return false;
    }

    const FVector PlayerLocation =
        PlayerCharacter->GetActorLocation();
    const FVector PlayerForward =
        PlayerCharacter->GetActorForwardVector().GetSafeNormal2D();

    if (TryBuildRoutePatrolCenter(
            PlayerCharacter,
            NavigationSystem,
            OutPatrolCenter))
    {
        return true;
    }

    for (int32 Attempt = 0; Attempt < 16; ++Attempt)
    {
        const float Angle = FMath::FRandRange(0.0f, 360.0f);
        const FVector Direction(
            FMath::Cos(FMath::DegreesToRadians(Angle)),
            FMath::Sin(FMath::DegreesToRadians(Angle)),
            0.0f
        );

        if (FVector::DotProduct(PlayerForward, Direction) > 0.15f)
        {
            continue;
        }

        const float Distance = FMath::FRandRange(
            FMath::Min(
                MinimumSpawnDistance,
                MaximumSpawnDistance
            ),
            FMath::Max(
                MinimumSpawnDistance,
                MaximumSpawnDistance
            )
        );
        const FVector Candidate =
            PlayerLocation + (Direction * Distance);
        FNavLocation ProjectedLocation;

        if (NavigationSystem->ProjectPointToNavigation(
                Candidate,
                ProjectedLocation,
                FVector(2500.0f, 2500.0f, 15000.0f)))
        {
            OutPatrolCenter = ProjectedLocation.Location;
            return true;
        }
    }

    return false;
}

bool ABHAmbientWarDirector::SpawnPatrol(
    ABHCharacter* PlayerCharacter,
    const FBHWarSectorState& SectorState,
    int32 EnemyCount,
    int32 FriendlyCount
)
{
    UWorld* World = GetWorld();

    if (!IsValid(World) ||
        !IsValid(PlayerCharacter) ||
        !EnemyClass)
    {
        return false;
    }

    FVector PatrolCenter;

    if (!TryBuildPatrolCenter(
            PlayerCharacter,
            PatrolCenter))
    {
        return false;
    }

    UNavigationSystemV1* NavigationSystem =
        FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
    const int32 SafeEnemyCount = FMath::Clamp(
        EnemyCount,
        0,
        MaxActiveEnemies
    );
    const int32 SafeFriendlyCount = FMath::Clamp(
        FriendlyCount,
        0,
        MaxFriendlyPatrolMembers
    );
    FBHWarSectorState EnemySourceSector;
    FBHWarSectorState FriendlySourceSector;
    int32 EnemySourceHops = INDEX_NONE;
    int32 FriendlySourceHops = INDEX_NONE;

    if ((SafeEnemyCount > 0 &&
            !ResolveForceSourceSector(
                SectorState,
                EBHWarFaction::Enemy,
                EnemySourceSector,
                &EnemySourceHops)) ||
        (SafeFriendlyCount > 0 &&
            !ResolveForceSourceSector(
                SectorState,
                EBHWarFaction::Friendly,
                FriendlySourceSector,
                &FriendlySourceHops)))
    {
        return false;
    }

    FVector EncounterAxis = (
        PlayerCharacter->GetActorLocation() - PatrolCenter
    ).GetSafeNormal2D();

    if (EncounterAxis.IsNearlyZero())
    {
        EncounterAxis =
            PlayerCharacter->GetActorForwardVector().GetSafeNormal2D();
    }

    if (EncounterAxis.IsNearlyZero())
    {
        EncounterAxis = FVector::ForwardVector;
    }

    const FVector FormationRight(
        -EncounterAxis.Y,
        EncounterAxis.X,
        0.0f
    );
    const float HalfSeparation =
        FMath::Max(500.0f, OpposingPatrolSeparation) * 0.5f;
    const FVector FriendlyPatrolCenter =
        PatrolCenter + (EncounterAxis * HalfSeparation);
    const FVector HostilePatrolCenter =
        PatrolCenter - (EncounterAxis * HalfSeparation);
    TArray<ABHPatrolPoint*> HostilePatrolPoints;
    TArray<ABHPatrolPoint*> FriendlyPatrolPoints;

    DestroyPatrolPoints();

    if (SafeEnemyCount > 0)
    {
        BuildPatrolPoints(
            HostilePatrolCenter,
            HostilePatrolPoints
        );
    }

    if (SafeFriendlyCount > 0)
    {
        BuildPatrolPoints(
            FriendlyPatrolCenter,
            FriendlyPatrolPoints
        );
    }

    if ((SafeEnemyCount > 0 && HostilePatrolPoints.IsEmpty()) ||
        (SafeFriendlyCount > 0 &&
            FriendlyPatrolPoints.IsEmpty()))
    {
        DestroyPatrolPoints();
        return false;
    }

    int32 SpawnedEnemyCount = 0;
    int32 SpawnedFriendlyCount = 0;
    RoutedCombatants.Reset();

    const auto SpawnCombatant = [
        this,
        World,
        NavigationSystem,
        FormationRight
    ](
        EBHCombatFaction CombatFaction,
        int32 TeamIndex,
        int32 TeamSize,
        const FVector& TeamCenter,
        const FVector& OpposingCenter,
        const TArray<ABHPatrolPoint*>& TeamPatrolPoints
    ) -> ABHEnemySoldier*
    {
        const float CenteredIndex =
            static_cast<float>(TeamIndex) -
            (static_cast<float>(FMath::Max(1, TeamSize) - 1) *
                0.5f);
        const FVector PreferredSpawnLocation =
            TeamCenter +
            (FormationRight * CenteredIndex *
                FMath::Max(0.0f, FormationSpacing));
        FNavLocation SpawnLocation(PreferredSpawnLocation);

        if (IsValid(NavigationSystem))
        {
            NavigationSystem->GetRandomReachablePointInRadius(
                PreferredSpawnLocation,
                100.0f,
                SpawnLocation
            );
        }

        const FVector FacingDirection = (
            OpposingCenter -
            SpawnLocation.Location
        ).GetSafeNormal2D();
        const FTransform SpawnTransform(
            FacingDirection.Rotation(),
            SpawnLocation.Location
        );
        ABHEnemySoldier* Soldier =
            World->SpawnActorDeferred<ABHEnemySoldier>(
                EnemyClass,
                SpawnTransform,
                this,
                nullptr,
                ESpawnActorCollisionHandlingMethod::
                    AdjustIfPossibleButAlwaysSpawn
            );

        if (!IsValid(Soldier))
        {
            return nullptr;
        }

        Soldier->SetFlags(RF_Transient);
        Soldier->SetCombatFaction(CombatFaction);
        Soldier->SetCombatantArchetype(
            ABHEnemySoldier::ChooseFormationArchetype(
                TeamIndex,
                TeamSize
            )
        );
        Soldier->SetObjectiveIdToCompleteOnDeath(NAME_None);

        TArray<ABHPatrolPoint*> AssignedPatrolPoints;
        AssignedPatrolPoints.Reserve(TeamPatrolPoints.Num());

        for (int32 Offset = 0;
            Offset < TeamPatrolPoints.Num();
            ++Offset)
        {
            const int32 PointIndex =
                (TeamIndex + Offset) % TeamPatrolPoints.Num();
            AssignedPatrolPoints.Add(
                TeamPatrolPoints[PointIndex]
            );
        }

        Soldier->SetPatrolPoints(AssignedPatrolPoints);
        UGameplayStatics::FinishSpawningActor(
            Soldier,
            SpawnTransform
        );
        return Soldier;
    };

    for (int32 Index = 0; Index < SafeEnemyCount; ++Index)
    {
        if (ABHEnemySoldier* Soldier =
            SpawnCombatant(
                EBHCombatFaction::Hostile,
                Index,
                SafeEnemyCount,
                HostilePatrolCenter,
                SafeFriendlyCount > 0
                    ? FriendlyPatrolCenter
                    : PlayerCharacter->GetActorLocation(),
                HostilePatrolPoints
            ))
        {
            TrackedEnemies.Add(Soldier);
            ++SpawnedEnemyCount;
        }
    }

    for (int32 Index = 0; Index < SafeFriendlyCount; ++Index)
    {
        if (ABHEnemySoldier* Soldier =
            SpawnCombatant(
                EBHCombatFaction::Friendly,
                Index,
                SafeFriendlyCount,
                FriendlyPatrolCenter,
                SafeEnemyCount > 0
                    ? HostilePatrolCenter
                    : PatrolCenter,
                FriendlyPatrolPoints
            ))
        {
            TrackedEnemies.Add(Soldier);
            ++SpawnedFriendlyCount;
        }
    }

    if ((SpawnedEnemyCount + SpawnedFriendlyCount) <= 0)
    {
        DestroyPatrolPoints();
        return false;
    }

    ActivePatrolSectorID = SectorState.SectorID;
    ActiveFriendlyForceSectorID =
        SpawnedFriendlyCount > 0
            ? FriendlySourceSector.SectorID
            : NAME_None;
    ActiveEnemyForceSectorID =
        SpawnedEnemyCount > 0
            ? EnemySourceSector.SectorID
            : NAME_None;
    ActiveFriendlySourceHops =
        SpawnedFriendlyCount > 0
            ? FriendlySourceHops
            : INDEX_NONE;
    ActiveEnemySourceHops =
        SpawnedEnemyCount > 0
            ? EnemySourceHops
            : INDEX_NONE;
    InitialFriendlyCount = SpawnedFriendlyCount;
    InitialEnemyCount = SpawnedEnemyCount;
    PatrolResolvedTime = -1.0f;

    UGameInstance* GameInstance = GetGameInstance();
    UBHWarSubsystem* WarSubsystem = GameInstance
        ? GameInstance->GetSubsystem<UBHWarSubsystem>()
        : nullptr;
    const float FriendlySupplyRequested =
        CalculatePatrolSupplyCost(
            SpawnedFriendlyCount,
            ActiveFriendlySourceHops
        );
    const float EnemySupplyRequested =
        CalculatePatrolSupplyCost(
            SpawnedEnemyCount,
            ActiveEnemySourceHops
        );
    const float FriendlySupplyCommitted =
        IsValid(WarSubsystem) &&
            !ActiveFriendlyForceSectorID.IsNone()
            ? WarSubsystem->CommitAmbientPatrolSupply(
                ActiveFriendlyForceSectorID,
                EBHWarFaction::Friendly,
                FriendlySupplyRequested
            )
            : 0.0f;
    const float EnemySupplyCommitted =
        IsValid(WarSubsystem) &&
            !ActiveEnemyForceSectorID.IsNone()
            ? WarSubsystem->CommitAmbientPatrolSupply(
                ActiveEnemyForceSectorID,
                EBHWarFaction::Enemy,
                EnemySupplyRequested
            )
            : 0.0f;
    const FBHWarSectorState FriendlySourceState =
        IsValid(WarSubsystem) &&
            !ActiveFriendlyForceSectorID.IsNone()
            ? WarSubsystem->GetSectorState(
                ActiveFriendlyForceSectorID
            )
            : FBHWarSectorState();
    const FBHWarSectorState EnemySourceState =
        IsValid(WarSubsystem) &&
            !ActiveEnemyForceSectorID.IsNone()
            ? WarSubsystem->GetSectorState(
                ActiveEnemyForceSectorID
            )
            : FBHWarSectorState();
    const float CurrentContactSupply =
        IsValid(WarSubsystem)
            ? WarSubsystem->GetSectorState(
                SectorState.SectorID
            ).Supply
            : SectorState.Supply;
    const FText ContactSectorName =
        SectorState.DisplayName.IsEmpty()
            ? FText::FromName(SectorState.SectorID)
            : SectorState.DisplayName;
    const FText FriendlySourceName =
        FriendlySourceState.DisplayName.IsEmpty()
            ? FText::FromName(ActiveFriendlyForceSectorID)
            : FriendlySourceState.DisplayName;
    const FText EnemySourceName =
        EnemySourceState.DisplayName.IsEmpty()
            ? FText::FromName(ActiveEnemyForceSectorID)
            : EnemySourceState.DisplayName;
    FText ContactAlert;

    if (SpawnedFriendlyCount > 0 && SpawnedEnemyCount > 0)
    {
        ContactAlert = FText::Format(
            NSLOCTEXT(
                "BrokenHorizon",
                "AmbientContactAlert",
                "CONTACT // {0}\n"
                "FRIENDLY {1} FROM {2}\n"
                "ENEMY {3} FROM {4}"
            ),
            ContactSectorName,
            FText::AsNumber(SpawnedFriendlyCount),
            FriendlySourceName,
            FText::AsNumber(SpawnedEnemyCount),
            EnemySourceName
        );
    }
    else if (SpawnedEnemyCount > 0)
    {
        ContactAlert = FText::Format(
            NSLOCTEXT(
                "BrokenHorizon",
                "AmbientEnemyPatrolAlert",
                "ENEMY PATROL // {0}\n"
                "{1} HOSTILES FROM {2}"
            ),
            ContactSectorName,
            FText::AsNumber(SpawnedEnemyCount),
            EnemySourceName
        );
    }
    else
    {
        ContactAlert = FText::Format(
            NSLOCTEXT(
                "BrokenHorizon",
                "AmbientFriendlyPatrolAlert",
                "FRIENDLY PATROL // {0}\n"
                "{1} TROOPS FROM {2}"
            ),
            ContactSectorName,
            FText::AsNumber(SpawnedFriendlyCount),
            FriendlySourceName
        );
    }

    PlayerCharacter->ShowStatusNotification(ContactAlert);

    UE_LOG(
        LogTemp,
        Display,
        TEXT(
            "BH_AMBIENT_DEPLOYMENT_SUPPLY contact=%s "
            "friendly_source=%s friendly_requested=%.2f "
            "friendly_committed=%.2f "
            "enemy_source=%s enemy_requested=%.2f "
            "enemy_committed=%.2f"
        ),
        *SectorState.SectorID.ToString(),
        *ActiveFriendlyForceSectorID.ToString(),
        FriendlySupplyRequested,
        FriendlySupplyCommitted,
        *ActiveEnemyForceSectorID.ToString(),
        EnemySupplyRequested,
        EnemySupplyCommitted
    );

    UE_LOG(
        LogTemp,
        Display,
        TEXT(
            "BH_AMBIENT_CONTACT_ALERT sector=%s "
            "friendly_size=%d friendly_source=%s friendly_hops=%d "
            "enemy_size=%d enemy_source=%s enemy_hops=%d"
        ),
        *SectorState.SectorID.ToString(),
        SpawnedFriendlyCount,
        *ActiveFriendlyForceSectorID.ToString(),
        ActiveFriendlySourceHops,
        SpawnedEnemyCount,
        *ActiveEnemyForceSectorID.ToString(),
        ActiveEnemySourceHops
    );

    UE_LOG(
        LogTemp,
        Display,
        TEXT(
            "BH_AMBIENT_WAR_READY sector=%s owner=%d "
            "hostile_size=%d friendly_size=%d supply=%.1f "
            "enemy_source=%s enemy_hops=%d "
            "friendly_source=%s friendly_hops=%d "
            "separation=%.0f formation_spacing=%.0f"
        ),
        *SectorState.SectorID.ToString(),
        static_cast<int32>(SectorState.Owner),
        SpawnedEnemyCount,
        SpawnedFriendlyCount,
        CurrentContactSupply,
        *ActiveEnemyForceSectorID.ToString(),
        ActiveEnemySourceHops,
        *ActiveFriendlyForceSectorID.ToString(),
        ActiveFriendlySourceHops,
        FMath::Max(500.0f, OpposingPatrolSeparation),
        FMath::Max(0.0f, FormationSpacing)
    );
    return true;
}

void ABHAmbientWarDirector::BuildPatrolPoints(
    const FVector& PatrolCenter,
    TArray<ABHPatrolPoint*>& OutPatrolPoints
)
{
    UWorld* World = GetWorld();
    UNavigationSystemV1* NavigationSystem =
        FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);

    if (!IsValid(World) || !IsValid(NavigationSystem))
    {
        return;
    }

    const int32 SafePointCount = FMath::Max(1, PatrolPointCount);

    for (int32 Index = 0; Index < SafePointCount; ++Index)
    {
        const float Angle =
            (360.0f / SafePointCount) * Index +
            FMath::FRandRange(-25.0f, 25.0f);
        const FVector Direction(
            FMath::Cos(FMath::DegreesToRadians(Angle)),
            FMath::Sin(FMath::DegreesToRadians(Angle)),
            0.0f
        );
        const FVector Candidate =
            PatrolCenter + (Direction * PatrolRadius);
        FNavLocation PatrolLocation;

        if (!NavigationSystem->ProjectPointToNavigation(
                Candidate,
                PatrolLocation,
                FVector(1500.0f, 1500.0f, 8000.0f)))
        {
            continue;
        }

        FActorSpawnParameters SpawnParameters;
        SpawnParameters.SpawnCollisionHandlingOverride =
            ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        SpawnParameters.ObjectFlags |= RF_Transient;

        ABHPatrolPoint* PatrolPoint =
            World->SpawnActor<ABHPatrolPoint>(
                ABHPatrolPoint::StaticClass(),
                PatrolLocation.Location,
                FRotator::ZeroRotator,
                SpawnParameters
            );

        if (IsValid(PatrolPoint))
        {
            ActivePatrolPoints.Add(PatrolPoint);
            OutPatrolPoints.Add(PatrolPoint);
        }
    }
}

void ABHAmbientWarDirector::RemoveInvalidEnemies()
{
    int32 FriendlyCasualties = 0;
    int32 EnemyCasualties = 0;

    for (int32 Index = TrackedEnemies.Num() - 1;
        Index >= 0;
        --Index)
    {
        ABHEnemySoldier* Soldier = TrackedEnemies[Index];

        if (!IsValid(Soldier))
        {
            RoutedCombatants.Remove(Soldier);
            TrackedEnemies.RemoveAtSwap(Index);
            continue;
        }

        if (!Soldier->IsDead())
        {
            continue;
        }

        if (RoutedCombatants.Contains(Soldier))
        {
            RoutedCombatants.Remove(Soldier);

            UE_LOG(
                LogTemp,
                Display,
                TEXT(
                    "BH_AMBIENT_ROUT_CASUALTY_IGNORED "
                    "soldier=%s sector=%s"
                ),
                *Soldier->GetName(),
                *ActivePatrolSectorID.ToString()
            );
        }
        else if (Soldier->GetCombatFaction() ==
            EBHCombatFaction::Friendly)
        {
            ++FriendlyCasualties;
        }
        else
        {
            ++EnemyCasualties;
        }

        TrackedEnemies.RemoveAtSwap(Index);
    }

    ReportAmbientCasualties(
        FriendlyCasualties,
        EnemyCasualties
    );

    if (TrackedEnemies.IsEmpty())
    {
        BeginSectorContactCooldown(ActivePatrolSectorID);
        RoutedCombatants.Reset();
        ActivePatrolSectorID = NAME_None;
        ActiveFriendlyForceSectorID = NAME_None;
        ActiveEnemyForceSectorID = NAME_None;
        ActiveFriendlySourceHops = INDEX_NONE;
        ActiveEnemySourceHops = INDEX_NONE;
        InitialFriendlyCount = 0;
        InitialEnemyCount = 0;
        PatrolResolvedTime = -1.0f;
        DestroyPatrolPoints();
    }
}

void ABHAmbientWarDirector::CleanupDistantEnemies(
    const ABHCharacter* PlayerCharacter
)
{
    if (!IsValid(PlayerCharacter))
    {
        return;
    }

    const FVector PlayerLocation =
        PlayerCharacter->GetActorLocation();
    const float CleanupDistanceSquared =
        FMath::Square(CleanupDistance);

    for (int32 Index = TrackedEnemies.Num() - 1;
        Index >= 0;
        --Index)
    {
        ABHEnemySoldier* Enemy = TrackedEnemies[Index];

        if (IsValid(Enemy) &&
            FVector::DistSquared2D(
                PlayerLocation,
                Enemy->GetActorLocation()
            ) > CleanupDistanceSquared)
        {
            RoutedCombatants.Remove(Enemy);
            Enemy->Destroy();
            TrackedEnemies.RemoveAtSwap(Index);
        }
    }

    if (TrackedEnemies.IsEmpty())
    {
        RoutedCombatants.Reset();
        ActivePatrolSectorID = NAME_None;
        ActiveFriendlyForceSectorID = NAME_None;
        ActiveEnemyForceSectorID = NAME_None;
        ActiveFriendlySourceHops = INDEX_NONE;
        ActiveEnemySourceHops = INDEX_NONE;
        InitialFriendlyCount = 0;
        InitialEnemyCount = 0;
        PatrolResolvedTime = -1.0f;
        DestroyPatrolPoints();
    }
}

void ABHAmbientWarDirector::UpdatePatrolLifecycle()
{
    UWorld* World = GetWorld();

    if (!IsValid(World) ||
        TrackedEnemies.IsEmpty() ||
        ActivePatrolSectorID.IsNone() ||
        InitialFriendlyCount <= 0 ||
        InitialEnemyCount <= 0)
    {
        return;
    }

    int32 LivingFriendlyCount = 0;
    int32 LivingEnemyCount = 0;
    int32 RetreatingFriendlyCount = 0;
    int32 RetreatingEnemyCount = 0;

    for (const ABHEnemySoldier* Soldier : TrackedEnemies)
    {
        if (!IsValid(Soldier) || Soldier->IsDead())
        {
            continue;
        }

        if (Soldier->GetCombatFaction() ==
            EBHCombatFaction::Friendly)
        {
            ++LivingFriendlyCount;

            const ABHEnemyAIController* AIController =
                Cast<ABHEnemyAIController>(
                    Soldier->GetController()
                );

            if (
                Soldier->IsOutOfAmmunition() ||
                (
                    IsValid(AIController) &&
                    AIController->GetCurrentState() ==
                        EBHEnemyAIState::Retreat
                )
            )
            {
                ++RetreatingFriendlyCount;
            }
        }
        else
        {
            ++LivingEnemyCount;

            const ABHEnemyAIController* AIController =
                Cast<ABHEnemyAIController>(
                    Soldier->GetController()
                );

            if (
                Soldier->IsOutOfAmmunition() ||
                (
                    IsValid(AIController) &&
                    AIController->GetCurrentState() ==
                        EBHEnemyAIState::Retreat
                )
            )
            {
                ++RetreatingEnemyCount;
            }
        }
    }

    if (LivingFriendlyCount <= 0 && LivingEnemyCount <= 0)
    {
        return;
    }

    const bool bFriendlyRouted =
        LivingFriendlyCount > 0 &&
        LivingEnemyCount > 0 &&
        RetreatingFriendlyCount == LivingFriendlyCount &&
        RetreatingEnemyCount < LivingEnemyCount;
    const bool bEnemyRouted =
        LivingEnemyCount > 0 &&
        LivingFriendlyCount > 0 &&
        RetreatingEnemyCount == LivingEnemyCount &&
        RetreatingFriendlyCount < LivingFriendlyCount;
    const bool bFriendlyEliminated =
        LivingFriendlyCount <= 0 && LivingEnemyCount > 0;
    const bool bEnemyEliminated =
        LivingEnemyCount <= 0 && LivingFriendlyCount > 0;

    if (PatrolResolvedTime < 0.0f &&
        !bFriendlyRouted &&
        !bEnemyRouted &&
        !bFriendlyEliminated &&
        !bEnemyEliminated)
    {
        return;
    }

    const float CurrentTime = World->GetTimeSeconds();

    if (PatrolResolvedTime < 0.0f)
    {
        const bool bFriendlyWon =
            bEnemyRouted || bEnemyEliminated;
        const TCHAR* WinningFaction =
            bFriendlyWon ? TEXT("friendly") : TEXT("enemy");
        const TCHAR* Resolution =
            (bFriendlyRouted || bEnemyRouted)
                ? TEXT("rout")
                : TEXT("elimination");
        const int32 RoutedCount =
            bFriendlyRouted
                ? RetreatingFriendlyCount
                : (bEnemyRouted ? RetreatingEnemyCount : 0);
        const EBHCombatFaction RoutedFaction =
            bFriendlyRouted
                ? EBHCombatFaction::Friendly
                : EBHCombatFaction::Hostile;

        if (bFriendlyRouted || bEnemyRouted)
        {
            for (ABHEnemySoldier* Soldier : TrackedEnemies)
            {
                if (IsValid(Soldier) &&
                    Soldier->GetCombatFaction() == RoutedFaction)
                {
                    RoutedCombatants.Add(Soldier);
                }
            }
        }

        ReportAmbientRout(
            bFriendlyRouted ? RoutedCount : 0,
            bEnemyRouted ? RoutedCount : 0
        );

        PatrolResolvedTime = CurrentTime;

        UGameInstance* GameInstance = GetGameInstance();
        UBHWarSubsystem* WarSubsystem = GameInstance
            ? GameInstance->GetSubsystem<UBHWarSubsystem>()
            : nullptr;
        if (IsValid(WarSubsystem))
        {
            WarSubsystem->ReportCivilianSecurityOutcome(
                ActivePatrolSectorID,
                bFriendlyWon
            );
        }
        const FBHWarSectorState SectorState =
            IsValid(WarSubsystem)
                ? WarSubsystem->GetSectorState(
                    ActivePatrolSectorID
                )
                : FBHWarSectorState();
        const FText SectorDisplayName =
            !SectorState.DisplayName.IsEmpty()
                ? SectorState.DisplayName
                : FText::FromName(ActivePatrolSectorID);
        const FText ResultText =
            bFriendlyWon
                ? NSLOCTEXT(
                    "BrokenHorizon",
                    "AmbientFriendlyPatrolVictory",
                    "FRIENDLY PATROL PREVAILS"
                )
                : NSLOCTEXT(
                    "BrokenHorizon",
                    "AmbientFriendlyPatrolDefeat",
                    "FRIENDLY PATROL DEFEATED"
                );
        const FText ResolutionText =
            bEnemyRouted
                ? NSLOCTEXT(
                    "BrokenHorizon",
                    "AmbientEnemyForceRouted",
                    "ENEMY FORCE ROUTED"
                )
                : bFriendlyRouted
                    ? NSLOCTEXT(
                        "BrokenHorizon",
                        "AmbientFriendlyForceRouted",
                        "FRIENDLY FORCE ROUTED"
                    )
                    : bEnemyEliminated
                        ? NSLOCTEXT(
                            "BrokenHorizon",
                            "AmbientEnemyForceEliminated",
                            "ENEMY FORCE ELIMINATED"
                        )
                        : NSLOCTEXT(
                            "BrokenHorizon",
                            "AmbientFriendlyForceEliminated",
                            "FRIENDLY FORCE ELIMINATED"
                        );
        ABHCharacter* PlayerCharacter = ResolvePlayerCharacter();

        if (IsValid(PlayerCharacter))
        {
            PlayerCharacter->ShowStatusNotification(
                FText::Format(
                    NSLOCTEXT(
                        "BrokenHorizon",
                        "AmbientSkirmishContactReport",
                        "CONTACT REPORT // {0}\n{1}\n{2}\n\n"
                        "FRIENDLY {3} // ENEMY {4}\n"
                        "LOCAL SUPPORT {5}%"
                    ),
                    SectorDisplayName,
                    ResultText,
                    ResolutionText,
                    FText::AsNumber(LivingFriendlyCount),
                    FText::AsNumber(LivingEnemyCount),
                    FText::AsNumber(
                        FMath::RoundToInt(
                            SectorState.CivilianSupport
                        )
                    )
                )
            );
        }

        UE_LOG(
            LogTemp,
            Display,
            TEXT(
                "BH_AMBIENT_CONTACT_REPORT sector=%s "
                "friendly_victory=%d resolution=%s "
                "friendly_survivors=%d enemy_survivors=%d"
            ),
            *ActivePatrolSectorID.ToString(),
            bFriendlyWon ? 1 : 0,
            Resolution,
            LivingFriendlyCount,
            LivingEnemyCount
        );

        UE_LOG(
            LogTemp,
            Display,
            TEXT(
                "BH_AMBIENT_SKIRMISH_ENDED sector=%s "
                "winner=%s resolution=%s routed=%d "
                "friendly_survivors=%d enemy_survivors=%d"
            ),
            *ActivePatrolSectorID.ToString(),
            WinningFaction,
            Resolution,
            RoutedCount,
            LivingFriendlyCount,
            LivingEnemyCount
        );
        return;
    }

    if (CurrentTime - PatrolResolvedTime <
        FMath::Max(0.0f, SurvivorWithdrawalDelay))
    {
        return;
    }

    const FName ResolvedSectorID = ActivePatrolSectorID;
    BeginSectorContactCooldown(ResolvedSectorID);

    for (ABHEnemySoldier* Soldier : TrackedEnemies)
    {
        if (IsValid(Soldier))
        {
            Soldier->Destroy();
        }
    }

    TrackedEnemies.Reset();
    RoutedCombatants.Reset();
    ActivePatrolSectorID = NAME_None;
    ActiveFriendlyForceSectorID = NAME_None;
    ActiveEnemyForceSectorID = NAME_None;
    ActiveFriendlySourceHops = INDEX_NONE;
    ActiveEnemySourceHops = INDEX_NONE;
    InitialFriendlyCount = 0;
    InitialEnemyCount = 0;
    PatrolResolvedTime = -1.0f;
    DestroyPatrolPoints();
    NextSpawnTime =
        CurrentTime + FMath::Max(0.0f, PatrolRespawnDelay);

    UE_LOG(
        LogTemp,
        Display,
        TEXT(
            "BH_AMBIENT_SURVIVORS_WITHDREW sector=%s "
            "next_patrol_in=%.1f"
        ),
        *ResolvedSectorID.ToString(),
        FMath::Max(0.0f, PatrolRespawnDelay)
    );
}

void ABHAmbientWarDirector::ReportAmbientCasualties(
    int32 FriendlyCasualties,
    int32 EnemyCasualties
)
{
    if (ActivePatrolSectorID.IsNone() ||
        (FriendlyCasualties + EnemyCasualties) <= 0)
    {
        return;
    }

    UGameInstance* GameInstance = GetGameInstance();
    UBHWarSubsystem* WarSubsystem = GameInstance
        ? GameInstance->GetSubsystem<UBHWarSubsystem>()
        : nullptr;

    if (IsValid(WarSubsystem))
    {
        const FName FriendlySourceSectorID =
            ActiveFriendlyForceSectorID.IsNone()
                ? ActivePatrolSectorID
                : ActiveFriendlyForceSectorID;
        const FName EnemySourceSectorID =
            ActiveEnemyForceSectorID.IsNone()
                ? ActivePatrolSectorID
                : ActiveEnemyForceSectorID;

        if (FriendlySourceSectorID == EnemySourceSectorID)
        {
            WarSubsystem->ApplyAmbientBattleResult(
                FriendlySourceSectorID,
                FriendlyCasualties,
                EnemyCasualties
            );
        }
        else
        {
            if (FriendlyCasualties > 0)
            {
                WarSubsystem->ApplyAmbientBattleResult(
                    FriendlySourceSectorID,
                    FriendlyCasualties,
                    0
                );
            }

            if (EnemyCasualties > 0)
            {
                WarSubsystem->ApplyAmbientBattleResult(
                    EnemySourceSectorID,
                    0,
                    EnemyCasualties
                );
            }
        }

        const float RecoveredMateriel =
            WarSubsystem->RecoverBattlefieldMateriel(
                ActivePatrolSectorID,
                EnemyCasualties,
                FriendlyCasualties,
                false
            );

        if (RecoveredMateriel > KINDA_SMALL_NUMBER)
        {
            const FBHWarSectorState RecoverySector =
                WarSubsystem->GetSectorState(
                    ActivePatrolSectorID
                );

            if (ABHCharacter* PlayerCharacter =
                BHPlayerResolver::Find(this))
            {
                PlayerCharacter->ShowStatusNotification(
                    FText::Format(
                        NSLOCTEXT(
                            "BrokenHorizon",
                            "AmbientBattlefieldRecovery",
                            "BATTLEFIELD SALVAGE SECURED\n\n"
                            "+{0} supply recovered for {1}."
                        ),
                        FText::AsNumber(
                            FMath::RoundToInt(
                                RecoveredMateriel
                            )
                        ),
                        RecoverySector.DisplayName.IsEmpty()
                            ? FText::FromName(
                                ActivePatrolSectorID
                            )
                            : RecoverySector.DisplayName
                    )
                );
            }
        }

        UE_LOG(
            LogTemp,
            Display,
            TEXT(
                "BH_AMBIENT_CASUALTY_SOURCES contact=%s "
                "friendly_source=%s enemy_source=%s "
                "friendly_casualties=%d enemy_casualties=%d "
                "recovered_supply=%.1f"
            ),
            *ActivePatrolSectorID.ToString(),
            *FriendlySourceSectorID.ToString(),
            *EnemySourceSectorID.ToString(),
            FriendlyCasualties,
            EnemyCasualties,
            RecoveredMateriel
        );
    }
}

void ABHAmbientWarDirector::ReportAmbientRout(
    int32 FriendlyRouted,
    int32 EnemyRouted
)
{
    if (ActivePatrolSectorID.IsNone() ||
        (FriendlyRouted + EnemyRouted) <= 0)
    {
        return;
    }

    UGameInstance* GameInstance = GetGameInstance();
    UBHWarSubsystem* WarSubsystem = GameInstance
        ? GameInstance->GetSubsystem<UBHWarSubsystem>()
        : nullptr;

    if (IsValid(WarSubsystem))
    {
        const FName FriendlySourceSectorID =
            ActiveFriendlyForceSectorID.IsNone()
                ? ActivePatrolSectorID
                : ActiveFriendlyForceSectorID;
        const FName EnemySourceSectorID =
            ActiveEnemyForceSectorID.IsNone()
                ? ActivePatrolSectorID
                : ActiveEnemyForceSectorID;

        if (FriendlySourceSectorID == EnemySourceSectorID)
        {
            WarSubsystem->ApplyAmbientRoutResult(
                FriendlySourceSectorID,
                FriendlyRouted,
                EnemyRouted
            );
        }
        else
        {
            if (FriendlyRouted > 0)
            {
                WarSubsystem->ApplyAmbientRoutResult(
                    FriendlySourceSectorID,
                    FriendlyRouted,
                    0
                );
            }

            if (EnemyRouted > 0)
            {
                WarSubsystem->ApplyAmbientRoutResult(
                    EnemySourceSectorID,
                    0,
                    EnemyRouted
                );
            }
        }

        UE_LOG(
            LogTemp,
            Display,
            TEXT(
                "BH_AMBIENT_ROUT_SOURCES contact=%s "
                "friendly_source=%s enemy_source=%s "
                "friendly_routed=%d enemy_routed=%d"
            ),
            *ActivePatrolSectorID.ToString(),
            *FriendlySourceSectorID.ToString(),
            *EnemySourceSectorID.ToString(),
            FriendlyRouted,
            EnemyRouted
        );
    }
}

float ABHAmbientWarDirector::GetRemainingSectorContactCooldown(
    FName SectorID,
    float CurrentTime
) const
{
    const float* ReadyTime =
        SectorContactReadyTimes.Find(SectorID);

    return ReadyTime
        ? FMath::Max(0.0f, *ReadyTime - CurrentTime)
        : 0.0f;
}

void ABHAmbientWarDirector::BeginSectorContactCooldown(
    FName SectorID
)
{
    const UWorld* World = GetWorld();

    if (SectorID.IsNone() || !IsValid(World))
    {
        return;
    }

    const float CooldownDuration =
        FMath::Max(0.0f, SectorContactCooldown);
    const float ReadyTime =
        World->GetTimeSeconds() + CooldownDuration;
    float& ExistingReadyTime =
        SectorContactReadyTimes.FindOrAdd(SectorID);
    ExistingReadyTime = FMath::Max(
        ExistingReadyTime,
        ReadyTime
    );

    UE_LOG(
        LogTemp,
        Display,
        TEXT(
            "BH_AMBIENT_CONTACT_COOLDOWN sector=%s "
            "duration=%.1f"
        ),
        *SectorID.ToString(),
        CooldownDuration
    );
}

void ABHAmbientWarDirector::DestroyPatrolPoints()
{
    for (ABHPatrolPoint* PatrolPoint : ActivePatrolPoints)
    {
        if (IsValid(PatrolPoint))
        {
            PatrolPoint->Destroy();
        }
    }

    ActivePatrolPoints.Reset();
}

void ABHAmbientWarDirector::ResolveEnemyClass()
{
    if (EnemyClass)
    {
        return;
    }

    for (TActorIterator<ABHEnemySoldier> It(GetWorld()); It; ++It)
    {
        if (IsValid(*It))
        {
            EnemyClass = It->GetClass();
            return;
        }
    }

    EnemyClass = ABHEnemySoldier::StaticClass();

    UE_LOG(
        LogTemp,
        Display,
        TEXT(
            "BH_AMBIENT_ENEMY_CLASS_RESOLVED class=%s source=native"
        ),
        *GetNameSafe(EnemyClass.Get())
    );
}
