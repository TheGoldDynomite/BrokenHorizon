#include "BHCharacter.h"
#include "Private/BHDefenseAMultiplayerTest.h"

// Strategic map input contract: EKeys::M,
// Strategic deployment contract: SaveSubsystem->DeployOperation(
// Validation contract: ExhaustionMultiplier; EKeys::B
#include "BHAntiVehicleProjectile.h"
#include "BHBattlefieldConditions.h"
#include "BHLoadoutWeight.h"


#include "Animation/AnimSequenceBase.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/WorldPartitionStreamingSourceComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputCoreTypes.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/World.h"
#include "BHInteractable.h"
#include "BHDoor.h"
#include "Blueprint/UserWidget.h"
#include "BHInteractionPromptWidget.h"
#include "BHObjectiveWidget.h"
#include "BHObjectiveComponent.h"
#include "BHObjectiveNotificationWidget.h"
#include "BHMissionData.h"
#include "BHHealthComponent.h"
#include "BHInjuryComponent.h"
#include "BHDeathWidget.h"
#include "BHDefenseMissionDirector.h"
#include "BHAmbientWarDirector.h"
#include "BHOpenWorldOperationDirector.h"
#include "BHRaidSabotageTarget.h"
#include "BHEnemyAIController.h"
#include "BHEnemySoldier.h"
#include "BHWarOperationRules.h"
#include "BHPlayerResolver.h"
#include "BHFieldTransport.h"
#include "BHSectorResupplyStation.h"
#include "BHSectorAnchor.h"
#include "BHSupplyConvoyTarget.h"
#include "BHSaveSubsystem.h"
#include "BHPlaytestTelemetrySubsystem.h"
#include "BHSessionSubsystem.h"
#include "BHWeaponComponent.h"
#include "BHAmmoHUDWidget.h"
#include "BHHitMarkerWidget.h"
#include "BHMissionCompleteWidget.h"
#include "BHPauseMenuWidget.h"
#include "BHWarMapWidget.h"
#include "BHWarGameState.h"
#include "BHWarSubsystem.h"
#include "BHGameShellSettings.h"
#include "BHUserSettingsSubsystem.h"
#include "BHSubtitleWidget.h"
#include "BHCombatStatusWidget.h"
#include "BHInventoryWidget.h"
#include "BHSalvagePickup.h"
#include "BHWaterSurface.h"
#include "BHFragGrenade.h"
#include "BHSmokeGrenade.h"
#include "BHEngineeringCharge.h"
#include "Engine/GameInstance.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "GameFramework/DamageType.h"
#include "HAL/PlatformMisc.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "NavigationInvokerComponent.h"
#include "Net/UnrealNetwork.h"
#include "Perception/AISense_Hearing.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "Sound/SoundBase.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
bool bBHPlayerCasualtyRuntimeProbeScheduled = false;
bool bBHEngineeringRuntimeProbeScheduled = false;

enum class EBHSectorSupplyReadiness : uint8
{
    Starved,
    Critical,
    Stable,
    Stockpiled
};

EBHSectorSupplyReadiness GetSectorSupplyReadiness(
    float Supply
)
{
    if (Supply <= KINDA_SMALL_NUMBER)
    {
        return EBHSectorSupplyReadiness::Starved;
    }

    if (Supply < 25.0f)
    {
        return EBHSectorSupplyReadiness::Critical;
    }

    if (Supply < 75.0f)
    {
        return EBHSectorSupplyReadiness::Stable;
    }

    return EBHSectorSupplyReadiness::Stockpiled;
}

FText GetSectorSupplyReadinessText(
    EBHSectorSupplyReadiness Readiness
)
{
    switch (Readiness)
    {
        case EBHSectorSupplyReadiness::Starved:
            return NSLOCTEXT(
                "BrokenHorizon",
                "SectorSupplyStarved",
                "STARVED"
            );
        case EBHSectorSupplyReadiness::Critical:
            return NSLOCTEXT(
                "BrokenHorizon",
                "SectorSupplyCritical",
                "CRITICAL"
            );
        case EBHSectorSupplyReadiness::Stable:
            return NSLOCTEXT(
                "BrokenHorizon",
                "SectorSupplyStable",
                "STABLE"
            );
        default:
            return NSLOCTEXT(
                "BrokenHorizon",
                "SectorSupplyStockpiled",
                "STOCKPILED"
            );
    }
}

FName ResolveMappingBindingID(
    const UInputAction* Action,
    const FKey& DefaultKey
)
{
    if (!IsValid(Action))
    {
        return NAME_None;
    }

    const FString ActionName = Action->GetName();
    if (ActionName.Contains(TEXT("Move")))
    {
        if (DefaultKey == EKeys::W) return TEXT("MoveForward");
        if (DefaultKey == EKeys::S) return TEXT("MoveBackward");
        if (DefaultKey == EKeys::A) return TEXT("MoveLeft");
        if (DefaultKey == EKeys::D) return TEXT("MoveRight");
    }
    if (ActionName.Contains(TEXT("Look")))
    {
        if (DefaultKey == EKeys::MouseX) return TEXT("LookHorizontal");
        if (DefaultKey == EKeys::MouseY) return TEXT("LookVertical");
    }

    const TPair<const TCHAR*, const TCHAR*> ActionBindings[] = {
        {TEXT("Jump"), TEXT("Jump")},
        {TEXT("Sprint"), TEXT("Sprint")},
        {TEXT("Crouch"), TEXT("Crouch")},
        {TEXT("Interact"), TEXT("Interact")},
        {TEXT("Fire"), TEXT("Fire")},
        {TEXT("Aim"), TEXT("Aim")},
        {TEXT("Reload"), TEXT("Reload")},
        {TEXT("Pause"), TEXT("Pause")},
        {TEXT("Inventory"), TEXT("Inventory")},
        {TEXT("InventoryCycle"), TEXT("InventoryCycle")},
        {TEXT("LeanLeft"), TEXT("LeanLeft")},
        {TEXT("LeanRight"), TEXT("LeanRight")},
        {TEXT("Prone"), TEXT("Prone")},
        {TEXT("Smoke"), TEXT("SmokeGrenade")},
        {TEXT("FieldDressing"), TEXT("FieldDressing")},
        {TEXT("Medkit"), TEXT("Medkit")}
        ,{TEXT("AntiVehicle"), TEXT("AntiVehicle")}
    };
    for (const TPair<const TCHAR*, const TCHAR*>& Entry : ActionBindings)
    {
        if (ActionName.Contains(Entry.Key))
        {
            return FName(Entry.Value);
        }
    }
    return NAME_None;
}
}


ABHCharacter::ABHCharacter()
{

    PrimaryActorTick.bCanEverTick = false;
    SetReplicates(true);
    SetReplicateMovement(true);

    ObjectiveComponent = CreateDefaultSubobject<UBHObjectiveComponent>(
        TEXT("ObjectiveComponent")
    );

    HealthComponent = CreateDefaultSubobject<UBHHealthComponent>(
        TEXT("HealthComponent")
    );

    InjuryComponent = CreateDefaultSubobject<UBHInjuryComponent>(
        TEXT("InjuryComponent")
    );

    WeaponComponent = CreateDefaultSubobject<UBHWeaponComponent>(
        TEXT("WeaponComponent")
    );

    OpenWorldStreamingSource =
        CreateDefaultSubobject<UWorldPartitionStreamingSourceComponent>(
            TEXT("OpenWorldStreamingSource")
        );

    FStreamingSourceShape OpenWorldStreamingShape;
    OpenWorldStreamingShape.bUseGridLoadingRange = false;
    OpenWorldStreamingShape.Radius = 600000.0f;
    OpenWorldStreamingSource->Shapes.Add(
        OpenWorldStreamingShape
    );

    NavigationInvoker =
        CreateDefaultSubobject<UNavigationInvokerComponent>(
            TEXT("NavigationInvoker")
        );
    NavigationInvoker->SetGenerationRadii(
        25000.0f,
        35000.0f
    );

    FirstPersonCamera =
        CreateDefaultSubobject<UCameraComponent>(
            TEXT("FirstPersonCamera"));

    FirstPersonCamera->SetupAttachment(GetCapsuleComponent());

    FirstPersonCamera->SetRelativeLocation(
        FVector(0.0f, 0.0f, 64.0f));

    FirstPersonCamera->bUsePawnControlRotation = true;
    FirstPersonCamera->SetFieldOfView(90.0f);

    TacticalFlashlight = CreateDefaultSubobject<USpotLightComponent>(
        TEXT("TacticalFlashlight")
    );
    TacticalFlashlight->SetupAttachment(FirstPersonCamera);
    TacticalFlashlight->SetRelativeLocation(
        FVector(24.0f, 0.0f, -8.0f)
    );
    TacticalFlashlight->SetIntensity(4500.0f);
    TacticalFlashlight->SetAttenuationRadius(2200.0f);
    TacticalFlashlight->SetInnerConeAngle(12.0f);
    TacticalFlashlight->SetOuterConeAngle(24.0f);
    TacticalFlashlight->SetLightColor(FLinearColor(1.0f, 0.95f, 0.82f));
    TacticalFlashlight->SetCastShadows(true);
    TacticalFlashlight->SetVisibility(false);

    FirstPersonArms = CreateDefaultSubobject<USkeletalMeshComponent>(
        TEXT("FirstPersonArms")
    );
    FirstPersonArms->SetupAttachment(FirstPersonCamera);
    FirstPersonArms->SetCollisionEnabled(
        ECollisionEnabled::NoCollision
    );
    FirstPersonArms->SetOnlyOwnerSee(true);
    FirstPersonArms->SetCastShadow(false);

    bUseControllerRotationPitch = false;
    bUseControllerRotationYaw = true;
    bUseControllerRotationRoll = false;

    GetCharacterMovement()->MaxWalkSpeed = WalkSpeed;

    GetCharacterMovement()->NavAgentProps.bCanCrouch = true;

    PrimaryActorTick.bCanEverTick = true;

    CurrentStamina = MaxStamina;
    FragGrenadeClass = ABHFragGrenade::StaticClass();
    InventoryWidgetClass = UBHInventoryWidget::StaticClass();
    SmokeGrenadeClass = ABHSmokeGrenade::StaticClass();
    EngineeringChargeClass = ABHEngineeringCharge::StaticClass();
    AntiVehicleProjectileClass = ABHAntiVehicleProjectile::StaticClass();
    MissionCompleteMessage = NSLOCTEXT(
        "BrokenHorizon",
        "MissionCompleteMessage",
        "MISSION COMPLETE"
    );
    WarMapWidgetClass = UBHWarMapWidget::StaticClass();

    const ConstructorHelpers::FObjectFinder<USoundBase> DefaultFootstepAsset(
        TEXT("/Game/BrokenHorizon/Audio/SW_FirstLight_FootstepConcrete.SW_FirstLight_FootstepConcrete")
    );
    if (DefaultFootstepAsset.Succeeded())
    {
        DefaultFootstepSound = DefaultFootstepAsset.Object;
    }
    const ConstructorHelpers::FObjectFinder<USoundBase> ConcreteFootstepAsset(
        TEXT("/Game/BrokenHorizon/Audio/SW_FirstLight_FootstepConcrete.SW_FirstLight_FootstepConcrete")
    );
    if (ConcreteFootstepAsset.Succeeded())
    {
        ConcreteFootstepSound = ConcreteFootstepAsset.Object;
    }
    const ConstructorHelpers::FObjectFinder<USoundBase> DirtFootstepAsset(
        TEXT("/Game/BrokenHorizon/Audio/SW_FirstLight_FootstepDirt.SW_FirstLight_FootstepDirt")
    );
    if (DirtFootstepAsset.Succeeded())
    {
        DirtFootstepSound = DirtFootstepAsset.Object;
    }
    const ConstructorHelpers::FObjectFinder<USoundBase> GrassFootstepAsset(
        TEXT("/Game/BrokenHorizon/Audio/SW_FirstLight_FootstepGrass.SW_FirstLight_FootstepGrass")
    );
    if (GrassFootstepAsset.Succeeded())
    {
        GrassFootstepSound = GrassFootstepAsset.Object;
    }
    const ConstructorHelpers::FObjectFinder<USoundBase> MetalFootstepAsset(
        TEXT("/Game/BrokenHorizon/Audio/SW_FirstLight_FootstepMetal.SW_FirstLight_FootstepMetal")
    );
    if (MetalFootstepAsset.Succeeded())
    {
        MetalFootstepSound = MetalFootstepAsset.Object;
    }
    const ConstructorHelpers::FObjectFinder<USoundBase> WaterFootstepAsset(
        TEXT("/Game/BrokenHorizon/Audio/SW_FirstLight_FootstepWater.SW_FirstLight_FootstepWater")
    );
    if (WaterFootstepAsset.Succeeded())
    {
        WaterFootstepSound = WaterFootstepAsset.Object;
    }
    const ConstructorHelpers::FObjectFinder<USoundBase> NearMissAsset(
        TEXT("/Game/BrokenHorizon/Audio/SW_FirstLight_NearMiss.SW_FirstLight_NearMiss")
    );
    if (NearMissAsset.Succeeded())
    {
        NearMissSound = NearMissAsset.Object;
    }
}

APlayerController*
ABHCharacter::ResolveOwningPlayerController() const
{
    if (APlayerController* DirectController =
        Cast<APlayerController>(GetController()))
    {
        return DirectController;
    }

    APlayerController* PlayerController =
        UGameplayStatics::GetPlayerController(this, 0);

    return IsValid(PlayerController) &&
        BHPlayerResolver::Find(this) == this
            ? PlayerController
            : nullptr;
}

void ABHCharacter::GetLifetimeReplicatedProps(
    TArray<FLifetimeProperty>& OutLifetimeProps
) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME_CONDITION(
        ABHCharacter,
        CurrentStamina,
        COND_OwnerOnly
    );
    DOREPLIFETIME_CONDITION(
        ABHCharacter,
        OwnedKeycards,
        COND_OwnerOnly
    );

    DOREPLIFETIME(
        ABHCharacter,
        AssignedWarSectorID
    );
    DOREPLIFETIME(
        ABHCharacter,
        AssignedWarSupplySourceSectorID
    );
    DOREPLIFETIME(
        ABHCharacter,
        AssignedWarPriorityType
    );
    DOREPLIFETIME(
        ABHCharacter,
        bRuntimeWarOperation
    );
    DOREPLIFETIME_CONDITION(
        ABHCharacter,
        FieldSquadMembers,
        COND_OwnerOnly
    );
    DOREPLIFETIME_CONDITION(
        ABHCharacter,
        FieldSquadContextAction,
        COND_OwnerOnly
    );
    DOREPLIFETIME_CONDITION(
        ABHCharacter,
        FieldSquadContextTargetLabel,
        COND_OwnerOnly
    );
    DOREPLIFETIME_CONDITION(
        ABHCharacter,
        bFieldSquadContextActionReachedTarget,
        COND_OwnerOnly
    );
    DOREPLIFETIME(ABHCharacter, bPlayerIncapacitated);
    DOREPLIFETIME(ABHCharacter, bPlayerCasualtyStabilized);
    DOREPLIFETIME(ABHCharacter, PlayerBleedOutDeadline);
    DOREPLIFETIME(ABHCharacter, DraggedCasualty);
    DOREPLIFETIME(ABHCharacter, bHoldingControlledBreath);
    DOREPLIFETIME(ABHCharacter, bWeaponBraced);
    DOREPLIFETIME(ABHCharacter, WeaponBraceSupportQuality);
    DOREPLIFETIME(ABHCharacter, bTacticalFlashlightOn);
    DOREPLIFETIME(ABHCharacter, TacticalFlashlightBattery);
    DOREPLIFETIME_CONDITION(
        ABHCharacter, FragGrenadeCount, COND_OwnerOnly
    );
    DOREPLIFETIME_CONDITION(
        ABHCharacter, SmokeGrenadeCount, COND_OwnerOnly
    );
    DOREPLIFETIME_CONDITION(
        ABHCharacter, EngineeringChargeCount, COND_OwnerOnly
    );
    DOREPLIFETIME_CONDITION(
        ABHCharacter, AntiVehicleRoundCount, COND_OwnerOnly
    );
    DOREPLIFETIME_CONDITION(
        ABHCharacter, ActiveEngineeringChargeCount, COND_OwnerOnly
    );
}

void ABHCharacter::BeginPlay()
{
    Super::BeginPlay();

    TacticalFlashlightBattery = FMath::Clamp(
        TacticalFlashlightBattery,
        0.0f,
        FMath::Max(1.0f, TacticalFlashlightMaxBattery)
    );
    UpdateTacticalFlashlightVisual();

    if (HasAuthority() && IsValid(GetWorld()))
    {
        for (TActorIterator<ABHAmbientWarDirector> It(GetWorld());
            It;
            ++It)
        {
            if (IsValid(*It))
            {
                AmbientWarDirector = *It;
                break;
            }
        }

        if (!IsValid(AmbientWarDirector))
        {
            AmbientWarDirector =
                GetWorld()->SpawnActor<ABHAmbientWarDirector>();
        }
    }

    if (IsValid(OpenWorldStreamingSource) &&
        IsValid(GetWorld()) &&
        GetWorld()->IsPartitionedWorld())
    {
        bWaitingForInitialWorldStreaming = true;
        InitialWorldStreamingElapsed = 0.0f;
        GetCharacterMovement()->DisableMovement();
    }

    if (UCapsuleComponent* CollisionCapsule = GetCapsuleComponent())
    {
        CollisionCapsule->SetCollisionResponseToChannel(
            ECC_GameTraceChannel2,
            ECR_Block
        );
    }

    if (IsValid(FirstPersonArms))
    {
        FirstPersonPresentationBaseLocation =
            FirstPersonArms->GetRelativeLocation();
        FirstPersonPresentationBaseRotation =
            FirstPersonArms->GetRelativeRotation();
        bFirstPersonPresentationBaseCached = true;
    }

    if (IsValid(FirstPersonCamera))
    {
        LeanCameraBaseRelativeLocation =
            FirstPersonCamera->GetRelativeLocation();
        LeanCameraBaseRelativeRotation =
            FirstPersonCamera->GetRelativeRotation();
        bLeanCameraBaseCached = true;
    }

    if (APlayerController* PlayerController =
        ResolveOwningPlayerController();
        IsValid(PlayerController) &&
        PlayerController->GetLocalPlayer() != nullptr)
    {
        if (ShouldPauseWorldForMenu(GetNetMode()))
        {
            UGameplayStatics::SetGamePaused(this, false);
        }
        PlayerController->SetIgnoreMoveInput(false);
        PlayerController->SetIgnoreLookInput(false);
        PlayerController->SetInputMode(FInputModeGameOnly());
        PlayerController->bShowMouseCursor = false;
    }

    if (UGameInstance* GameInstance = GetGameInstance())
    {
        if (UBHUserSettingsSubsystem* SettingsSubsystem =
            GameInstance->GetSubsystem<UBHUserSettingsSubsystem>())
        {
            SettingsSubsystem->ApplyPersistedSettings();
            SettingsSubsystem->OnInputBindingsChanged.AddUniqueDynamic(
                this,
                &ABHCharacter::HandleInputBindingsChanged
            );
        }
    }

    if (IsValid(HealthComponent))
    {
        HealthComponent->OnHealthChanged.AddDynamic(
            this,
            &ABHCharacter::HandleHealthChanged
        );
        HealthComponent->OnDamaged.AddDynamic(
            this,
            &ABHCharacter::HandlePlayerDamaged
        );
        HealthComponent->OnDeath.AddDynamic(
            this,
            &ABHCharacter::HandleDeath
        );
    }

    APlayerController* PlayerController =
        ResolveOwningPlayerController();

    if (IsValid(PlayerController) && PlayerController->GetLocalPlayer() != nullptr)
    {
        const TSubclassOf<UBHSubtitleWidget> WidgetClass = SubtitleWidgetClass
            ? SubtitleWidgetClass
            : TSubclassOf<UBHSubtitleWidget>(UBHSubtitleWidget::StaticClass());
        SubtitleWidget = CreateWidget<UBHSubtitleWidget>(
            PlayerController,
            WidgetClass
        );
        if (IsValid(SubtitleWidget))
        {
            SubtitleWidget->AddToViewport(40);
        }
    }

    if (ObjectiveWidgetClass &&
        IsValid(PlayerController) &&
        PlayerController->GetLocalPlayer() != nullptr)
    {
        ObjectiveWidget = CreateWidget<UBHObjectiveWidget>(
            PlayerController,
            ObjectiveWidgetClass
        );

        if (ObjectiveWidget)
        {
            ObjectiveWidget->SetAnchorsInViewport(FAnchors(0.0f, 0.0f));
            ObjectiveWidget->SetAlignmentInViewport(FVector2D::ZeroVector);
            ObjectiveWidget->SetPositionInViewport(
                FVector2D(36.0f, 36.0f),
                false
            );
            ObjectiveWidget->SetDesiredSizeInViewport(
                FVector2D(460.0f, 190.0f)
            );
            ObjectiveWidget->AddToViewport();
        }
    }

    if (IsValid(PlayerController) &&
        PlayerController->GetLocalPlayer() != nullptr)
    {
        const TSubclassOf<UBHInteractionPromptWidget> WidgetClass =
            TSubclassOf<UBHInteractionPromptWidget>(
                UBHInteractionPromptWidget::StaticClass()
            );
        InteractionPromptWidget = CreateWidget<UBHInteractionPromptWidget>(
            PlayerController,
            WidgetClass
        );

        if (IsValid(InteractionPromptWidget))
        {
            InteractionPromptWidget->SetAlignmentInViewport(
                FVector2D(0.5f, 0.5f)
            );
            InteractionPromptWidget->SetPositionInViewport(
                FVector2D::ZeroVector,
                false
            );
            InteractionPromptWidget->SetDesiredSizeInViewport(
                FVector2D(760.0f, 96.0f)
            );
            InteractionPromptWidget->SetAnchorsInViewport(
                FAnchors(0.5f, 0.42f, 0.5f, 0.42f)
            );
            InteractionPromptWidget->AddToViewport(60);

            InteractionPromptWidget->SetVisibility(
                ESlateVisibility::Collapsed
            );

            UE_LOG(
                LogTemp,
                Display,
                TEXT(
                    "BH_INTERACTION_PROMPT_NATIVE_INSTANCE class=%s "
                    "blueprint_class_ignored=%d"
                ),
                *WidgetClass->GetPathName(),
                InteractionPromptClass != nullptr ? 1 : 0
            );
        }
        else
        {
            UE_LOG(
                LogTemp,
                Error,
                TEXT("BH_INTERACTION_PROMPT_CREATE_FAILED class=%s"),
                *WidgetClass->GetPathName()
            );
        }
    }
    else
    {
        UE_LOG(
            LogTemp,
            Warning,
            TEXT("BH_INTERACTION_PROMPT_DEFERRED controller=%d local_player=%d"),
            IsValid(PlayerController) ? 1 : 0,
            IsValid(PlayerController) &&
                    PlayerController->GetLocalPlayer() != nullptr
                ? 1
                : 0
        );
    }


    if (IsValid(WeaponComponent))
    {
        WeaponComponent->OnAmmoChanged.AddDynamic(
            this,
            &ABHCharacter::HandleAmmoChanged
        );
        WeaponComponent->OnWeaponRoleChanged.AddDynamic(
            this,
            &ABHCharacter::HandleWeaponRoleChanged
        );
        WeaponComponent->OnWeaponHeatChanged.AddDynamic(
            this,
            &ABHCharacter::HandleWeaponHeatChanged
        );
    }

    if (AmmoHUDWidgetClass &&
        IsValid(PlayerController) &&
        PlayerController->GetLocalPlayer() != nullptr)
    {
        AmmoHUDWidget = CreateWidget<UBHAmmoHUDWidget>(
            PlayerController,
            AmmoHUDWidgetClass
        );

        if (IsValid(AmmoHUDWidget))
        {
            AmmoHUDWidget->AddToViewport();
            HandleAmmoChanged(
                WeaponComponent
                    ? WeaponComponent->GetMagazineAmmo()
                    : 0,
                WeaponComponent
                    ? WeaponComponent->GetReserveAmmo()
                    : 0
            );
            HandleWeaponRoleChanged(
                WeaponComponent
                    ? WeaponComponent->GetWeaponRole()
                    : EBHWeaponRole::Assault
            );
            HandleWeaponHeatChanged(
                WeaponComponent
                    ? WeaponComponent->GetWeaponHeatNormalized() : 0.0f,
                WeaponComponent
                    ? WeaponComponent->IsWeaponOverheated() : false
            );
        }
    }

    if (IsValid(PlayerController) &&
        PlayerController->GetLocalPlayer() != nullptr)
    {
        HitMarkerWidget = CreateWidget<UBHHitMarkerWidget>(
            PlayerController,
            UBHHitMarkerWidget::StaticClass()
        );

        if (IsValid(HitMarkerWidget))
        {
            HitMarkerWidget->SetAnchorsInViewport(
                FAnchors(0.5f, 0.5f)
            );
            HitMarkerWidget->SetAlignmentInViewport(
                FVector2D(0.5f, 0.5f)
            );
            HitMarkerWidget->SetDesiredSizeInViewport(
                FVector2D(64.0f, 64.0f)
            );
            HitMarkerWidget->AddToViewport(50);
        }
    }

    if (CombatStatusWidgetClass &&
        IsValid(PlayerController) &&
        PlayerController->GetLocalPlayer() != nullptr)
    {
        CombatStatusWidget =
            CreateWidget<UBHCombatStatusWidget>(
                PlayerController,
                CombatStatusWidgetClass
            );

        if (IsValid(CombatStatusWidget))
        {
            CombatStatusWidget->AddToViewport();
            CombatStatusWidget->SetHealth(
                HealthComponent
                    ? HealthComponent->GetCurrentHealth()
                    : 0.0f,
                HealthComponent
                    ? HealthComponent->GetMaxHealth()
                    : 1.0f
            );
            CombatStatusWidget->SetStamina(
                CurrentStamina,
                MaxStamina
            );
            RefreshFragGrenadeHUD();
            RefreshEngineeringHUD();
        }
    }

    if (IsValid(InjuryComponent))
    {
        InjuryComponent->OnInjuryStateChanged.AddDynamic(
            this,
            &ABHCharacter::HandleInjuryStateChanged
        );
        InjuryComponent->OnMedicalStateChanged.AddDynamic(
            this,
            &ABHCharacter::HandleMedicalStateChanged
        );
        InjuryComponent->OnTreatmentCompleted.AddDynamic(
            this,
            &ABHCharacter::HandleMedkitTreatmentCompleted
        );
        HandleInjuryStateChanged(
            InjuryComponent->IsBleeding(),
            InjuryComponent->GetBleedRate(),
            InjuryComponent->IsArmInjured(),
            InjuryComponent->IsLegInjured(),
            InjuryComponent->GetFieldDressingCount()
        );
        HandleMedicalStateChanged(
            InjuryComponent->GetMedkitCount(),
            InjuryComponent->GetHelmetDurabilityPercentage(),
            InjuryComponent->GetBodyArmorDurabilityPercentage(),
            InjuryComponent->IsMedkitTreatmentActive(),
            InjuryComponent->GetMedkitTreatmentProgress()
        );
    }

    // Objective Notification Event
    if (ObjectiveComponent)
    {
        ObjectiveComponent->OnObjectiveCompleted.AddDynamic(
            this,
            &ABHCharacter::OnObjectiveCompleted
        );
        ObjectiveComponent->OnMissionCompleted.AddDynamic(
            this,
            &ABHCharacter::OnMissionCompleted
        );

        ObjectiveComponent->StartMission(MissionData);
        ConfigureStrategicMissionPresentation();
        RefreshObjectiveWidget();
        QueueObjectiveActivationRadio(
            ObjectiveComponent->GetCurrentObjectiveID()
        );
    }

    if (UGameInstance* GameInstance = GetGameInstance())
    {
        if (UBHWarSubsystem* WarSubsystem =
            GameInstance->GetSubsystem<UBHWarSubsystem>())
        {
            CacheObservedWarState(WarSubsystem);
            WarSubsystem->OnWarStateChanged.AddDynamic(
                this,
                &ABHCharacter::HandleWarStateChanged
            );
        }
    }

    TryBindActiveOperationSnapshotPresentation();

    EnsureRuntimeInputActions();
    RefreshPlayerInputMappings();

#if !UE_BUILD_SHIPPING
    if (HasAuthority() && IsPlayerControlled() &&
        !bBHPlayerCasualtyRuntimeProbeScheduled &&
        FParse::Param(
            FCommandLine::Get(),
            TEXT("BHTestPlayerCasualtyRuntime")
        ))
    {
        bBHPlayerCasualtyRuntimeProbeScheduled = true;
        FTimerHandle ProbeTimer;
        FTimerDelegate ProbeDelegate = FTimerDelegate::CreateWeakLambda(
            this,
            [this]()
            {
                const int32 InitialDressings = IsValid(InjuryComponent)
                    ? InjuryComponent->GetFieldDressingCount() : 0;
                const int32 InitialMedkits = IsValid(InjuryComponent)
                    ? InjuryComponent->GetMedkitCount() : 0;
                if (IsValid(HealthComponent))
                {
                    HealthComponent->ApplyDamage(
                        HealthComponent->GetCurrentHealth() + 10.0f,
                        this
                    );
                }
                const bool bEnteredCasualty = bPlayerIncapacitated &&
                    !bPlayerCasualtyStabilized;
                FActorSpawnParameters SpawnParameters;
                SpawnParameters.SpawnCollisionHandlingOverride =
                    ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
                ABHCharacter* Reviver = GetWorld()->SpawnActor<ABHCharacter>(
                    ABHCharacter::StaticClass(),
                    GetActorLocation() + FVector(100.0f, 0.0f, 0.0f),
                    GetActorRotation(),
                    SpawnParameters
                );
                if (IsValid(Reviver))
                {
                    Reviver->TryTreatPlayerCasualty(this);
                    Reviver->TryTreatPlayerCasualty(this);
                }
                const bool bSuccess = bEnteredCasualty &&
                    !bPlayerIncapacitated &&
                    IsValid(HealthComponent) &&
                    FMath::IsNearlyEqual(
                        HealthComponent->GetCurrentHealth(),
                        PlayerReviveHealth
                    ) &&
                    IsValid(Reviver) &&
                    IsValid(Reviver->InjuryComponent) &&
                    Reviver->InjuryComponent->GetFieldDressingCount() ==
                        InitialDressings - 1 &&
                    Reviver->InjuryComponent->GetMedkitCount() ==
                        InitialMedkits - 1;
                UE_LOG(
                    LogTemp,
                    Display,
                    TEXT("BH_PLAYER_CASUALTY_RUNTIME result=%s entered=%d revived=%d health=%.1f dressing_spent=%d medkit_spent=%d"),
                    bSuccess ? TEXT("success") : TEXT("failure"),
                    bEnteredCasualty ? 1 : 0,
                    bPlayerIncapacitated ? 0 : 1,
                    IsValid(HealthComponent)
                        ? HealthComponent->GetCurrentHealth() : 0.0f,
                    IsValid(Reviver) && IsValid(Reviver->InjuryComponent)
                        ? InitialDressings - Reviver->InjuryComponent
                            ->GetFieldDressingCount() : 0,
                    IsValid(Reviver) && IsValid(Reviver->InjuryComponent)
                        ? InitialMedkits - Reviver->InjuryComponent
                            ->GetMedkitCount() : 0
                );
                if (IsValid(Reviver))
                {
                    Reviver->Destroy();
                }
                FPlatformMisc::RequestExit(false);
            }
        );
        GetWorldTimerManager().SetTimer(
            ProbeTimer,
            ProbeDelegate,
            1.0f,
            false
        );
    }

    if (HasAuthority() && IsPlayerControlled() &&
        !bBHEngineeringRuntimeProbeScheduled &&
        FParse::Param(FCommandLine::Get(), TEXT("BHTestEngineeringRuntime")))
    {
        bBHEngineeringRuntimeProbeScheduled = true;
        FTimerHandle EngineeringProbeTimer;
        FTimerDelegate EngineeringProbeDelegate =
            FTimerDelegate::CreateWeakLambda(this, [this]()
            {
                FActorSpawnParameters SpawnParameters;
                SpawnParameters.SpawnCollisionHandlingOverride =
                    ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
                ABHDoor* TestDoor = GetWorld()->SpawnActor<ABHDoor>(
                    ABHDoor::StaticClass(),
                    GetActorLocation() + GetActorForwardVector() * 100.0f,
                    GetActorRotation(),
                    SpawnParameters
                );
                if (IsValid(TestDoor))
                {
                    TestDoor->RestoreUnlockedState(false);
                }
                const int32 StartingCharges = EngineeringChargeCount;
                const bool bPlaced = IsValid(TestDoor) &&
                    TryPlaceBreachingCharge(TestDoor);
                FTimerHandle DetonationTimer;
                FTimerDelegate DetonationDelegate =
                    FTimerDelegate::CreateWeakLambda(
                        this,
                        [this, TestDoor, StartingCharges, bPlaced]()
                        {
                            DetonateEngineeringCharges();
                            const bool bSuccess = bPlaced &&
                                IsValid(TestDoor) && TestDoor->IsUnlocked() &&
                                EngineeringChargeCount == StartingCharges - 1 &&
                                ActiveEngineeringChargeCount == 0;
                            UE_LOG(LogTemp, Display,
                                TEXT("BH_ENGINEERING_RUNTIME result=%s placed=%d breached=%d carried=%d active=%d"),
                                bSuccess ? TEXT("success") : TEXT("failure"),
                                bPlaced ? 1 : 0,
                                IsValid(TestDoor) && TestDoor->IsUnlocked() ? 1 : 0,
                                EngineeringChargeCount,
                                ActiveEngineeringChargeCount);
                            if (IsValid(TestDoor))
                            {
                                TestDoor->Destroy();
                            }
                            FPlatformMisc::RequestExit(false);
                        }
                    );
                GetWorldTimerManager().SetTimer(
                    DetonationTimer,
                    DetonationDelegate,
                    2.4f,
                    false
                );
            });
        GetWorldTimerManager().SetTimer(
            EngineeringProbeTimer,
            EngineeringProbeDelegate,
            1.0f,
            false
        );
    }
#endif
}

void ABHCharacter::ShowSubtitle(
    const FText& Speaker,
    const FText& Line,
    float DurationSeconds,
    float DirectionAngleDegrees,
    bool bHasDirection
)
{
    if (IsValid(SubtitleWidget))
    {
        SubtitleWidget->ShowSubtitle(
            Speaker,
            Line,
            DurationSeconds,
            DirectionAngleDegrees,
            bHasDirection
        );
    }
}

const FBHObjectiveDefinition* ABHCharacter::FindAuthoredObjectiveDefinition(
    FName ObjectiveID
) const
{
    if (!IsValid(MissionData) || ObjectiveID.IsNone())
    {
        return nullptr;
    }
    return MissionData->Objectives.FindByPredicate(
        [ObjectiveID](const FBHObjectiveDefinition& Definition)
        {
            return Definition.ObjectiveID == ObjectiveID;
        }
    );
}

void ABHCharacter::QueueObjectiveActivationRadio(FName ObjectiveID)
{
    const FBHObjectiveDefinition* Definition =
        FindAuthoredObjectiveDefinition(ObjectiveID);
    if (!Definition || Definition->ActivationRadioLine.IsEmpty()) return;
    ShowSubtitle(
        Definition->RadioSpeaker,
        Definition->ActivationRadioLine,
        Definition->RadioSubtitleDuration,
        Definition->RadioDirectionDegrees,
        Definition->bRadioHasDirection
    );
}

void ABHCharacter::QueueObjectiveCompletionRadio(FName ObjectiveID)
{
    const FBHObjectiveDefinition* Definition =
        FindAuthoredObjectiveDefinition(ObjectiveID);
    if (!Definition || Definition->CompletionRadioLine.IsEmpty()) return;
    ShowSubtitle(
        Definition->RadioSpeaker,
        Definition->CompletionRadioLine,
        Definition->RadioSubtitleDuration,
        Definition->RadioDirectionDegrees,
        Definition->bRadioHasDirection
    );
}

void ABHCharacter::EnsureRuntimeInputActions()
{
    auto EnsureAction = [this](
        TObjectPtr<UInputAction>& Action,
        const FName Name
    )
    {
        if (!IsValid(Action))
        {
            Action = NewObject<UInputAction>(this, Name);
            Action->ValueType = EInputActionValueType::Boolean;
        }
    };

    EnsureAction(WarMapInputAction, TEXT("IA_Runtime_WarMap"));
    // Some legacy gameplay mapping contexts predate the pause action. Always
    // provide a native runtime action so packaged maps cannot lose Escape/Menu
    // after taking permanent mouse capture.
    EnsureAction(PauseAction, TEXT("IA_Runtime_Pause"));
    EnsureAction(InventoryAction, TEXT("IA_Runtime_Inventory"));
    EnsureAction(InventoryCycleAction, TEXT("IA_Runtime_InventoryCycle"));
    EnsureAction(GrenadeInputAction, TEXT("IA_Runtime_Grenade"));
    EnsureAction(SmokeGrenadeInputAction, TEXT("IA_Runtime_SmokeGrenade"));
    EnsureAction(
        TacticalFlashlightInputAction,
        TEXT("IA_Runtime_TacticalFlashlight")
    );
    EnsureAction(
        WeaponBashInputAction,
        TEXT("IA_Runtime_WeaponBash")
    );
    EnsureAction(
        FieldObservationInputAction,
        TEXT("IA_Runtime_FieldObservation")
    );
    EnsureAction(
        ControlledBreathingInputAction,
        TEXT("IA_Runtime_ControlledBreathing")
    );
    EnsureAction(EngineeringInputAction, TEXT("IA_Runtime_Engineering"));
    EnsureAction(AntiVehicleInputAction, TEXT("IA_Runtime_AntiVehicle"));
    EnsureAction(SquadOrderInputAction, TEXT("IA_Runtime_SquadOrder"));
    EnsureAction(ContextInputAction, TEXT("IA_Runtime_ContextAction"));
    EnsureAction(SquadPingInputAction, TEXT("IA_Runtime_SquadPing"));
    EnsureAction(FireModeInputAction, TEXT("IA_Runtime_FireMode"));
    EnsureAction(LeanLeftAction, TEXT("IA_Runtime_LeanLeft"));
    EnsureAction(LeanRightAction, TEXT("IA_Runtime_LeanRight"));
    EnsureAction(ProneAction, TEXT("IA_Runtime_Prone"));
    EnsureAction(FieldDressingAction, TEXT("IA_Runtime_FieldDressing"));
    EnsureAction(MedkitAction, TEXT("IA_Runtime_Medkit"));
}

void ABHCharacter::RefreshPlayerInputMappings()
{
    APlayerController* PlayerController = ResolveOwningPlayerController();
    ULocalPlayer* LocalPlayer = IsValid(PlayerController)
        ? PlayerController->GetLocalPlayer()
        : nullptr;
    UEnhancedInputLocalPlayerSubsystem* InputSubsystem = IsValid(LocalPlayer)
        ? LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>()
        : nullptr;
    if (!IsValid(InputSubsystem) || !IsValid(PlayerMappingContext))
    {
        return;
    }

    InputSubsystem->RemoveMappingContext(PlayerMappingContext);
    if (IsValid(RuntimePlayerMappingContext))
    {
        InputSubsystem->RemoveMappingContext(RuntimePlayerMappingContext);
    }

    const TArray<FEnhancedActionKeyMapping> SourceMappings =
        PlayerMappingContext->GetMappings();

    // Cooked mapping contexts already own loaded modifier and trigger objects.
    // Recursively duplicating them causes packaged builds to attempt to replace
    // those loaded subobjects. Rebuild a transient context and reuse the source
    // references instead.
    RuntimePlayerMappingContext = NewObject<UInputMappingContext>(
        this,
        NAME_None,
        RF_Transient
    );
    if (!IsValid(RuntimePlayerMappingContext))
    {
        return;
    }

    for (const FEnhancedActionKeyMapping& SourceMapping : SourceMappings)
    {
        if (!IsValid(SourceMapping.Action) || !SourceMapping.Key.IsValid())
        {
            continue;
        }

        FEnhancedActionKeyMapping& RuntimeMapping =
            RuntimePlayerMappingContext->MapKey(
                SourceMapping.Action,
                SourceMapping.Key
            );
        RuntimeMapping.Modifiers = SourceMapping.Modifiers;
        RuntimeMapping.Triggers = SourceMapping.Triggers;
    }

    const UBHUserSettingsSubsystem* SettingsSubsystem =
        GetGameInstance()
        ? GetGameInstance()->GetSubsystem<UBHUserSettingsSubsystem>()
        : nullptr;
    for (const FEnhancedActionKeyMapping& SourceMapping : SourceMappings)
    {
        const FName BindingID = ResolveMappingBindingID(
            SourceMapping.Action,
            SourceMapping.Key
        );
        if (BindingID.IsNone() || !IsValid(SettingsSubsystem))
        {
            continue;
        }

        const FKey RemappedKey = SettingsSubsystem->GetInputBinding(
            BindingID,
            SourceMapping.Key.IsGamepadKey()
        );
        if (!RemappedKey.IsValid() || RemappedKey == SourceMapping.Key)
        {
            continue;
        }

        RuntimePlayerMappingContext->UnmapKey(
            SourceMapping.Action,
            SourceMapping.Key
        );
        FEnhancedActionKeyMapping& Remapped =
            RuntimePlayerMappingContext->MapKey(
                SourceMapping.Action,
                RemappedKey
            );
        Remapped.Modifiers = SourceMapping.Modifiers;
        Remapped.Triggers = SourceMapping.Triggers;
    }

    auto AddBinding = [this, SettingsSubsystem](
        UInputAction* Action,
        const FName BindingID,
        const bool bGamepad
    )
    {
        if (!IsValid(Action) || !IsValid(SettingsSubsystem))
        {
            return;
        }
        for (const FEnhancedActionKeyMapping& Existing :
            RuntimePlayerMappingContext->GetMappings())
        {
            if (Existing.Action == Action &&
                Existing.Key.IsGamepadKey() == bGamepad)
            {
                return;
            }
        }
        const FKey Key = SettingsSubsystem->GetInputBinding(
            BindingID,
            bGamepad
        );
        if (Key.IsValid())
        {
            RuntimePlayerMappingContext->MapKey(Action, Key);
        }
    };

    AddBinding(MoveAction, TEXT("MoveStick"), true);
    AddBinding(LookAction, TEXT("LookStick"), true);
    const TPair<UInputAction*, FName> MappedActions[] = {
        {JumpAction, TEXT("Jump")},
        {SprintAction, TEXT("Sprint")},
        {CrouchAction, TEXT("Crouch")},
        {InteractAction, TEXT("Interact")},
        {FireAction, TEXT("Fire")},
        {AimAction, TEXT("Aim")},
        {ReloadAction, TEXT("Reload")},
        {PauseAction, TEXT("Pause")},
        {InventoryAction, TEXT("Inventory")},
        {InventoryCycleAction, TEXT("InventoryCycle")},
        {WarMapInputAction, TEXT("WarMap")},
        {GrenadeInputAction, TEXT("Grenade")},
        {SmokeGrenadeInputAction, TEXT("SmokeGrenade")},
        {TacticalFlashlightInputAction, TEXT("Flashlight")},
        {WeaponBashInputAction, TEXT("WeaponBash")},
        {FieldObservationInputAction, TEXT("FieldObservation")},
        {ControlledBreathingInputAction, TEXT("ControlledBreathing")},
        {EngineeringInputAction, TEXT("Engineering")},
        {AntiVehicleInputAction, TEXT("AntiVehicle")},
        {SquadOrderInputAction, TEXT("SquadOrder")},
        {ContextInputAction, TEXT("ContextAction")},
        {SquadPingInputAction, TEXT("SquadPing")},
        {FireModeInputAction, TEXT("FireMode")},
        {FieldDressingAction, TEXT("FieldDressing")},
        {MedkitAction, TEXT("Medkit")},
        {LeanLeftAction, TEXT("LeanLeft")},
        {LeanRightAction, TEXT("LeanRight")},
        {ProneAction, TEXT("Prone")}
    };
    for (const TPair<UInputAction*, FName>& Entry : MappedActions)
    {
        AddBinding(Entry.Key, Entry.Value, false);
        AddBinding(Entry.Key, Entry.Value, true);
    }

    InputSubsystem->AddMappingContext(RuntimePlayerMappingContext, 0);
    UE_LOG(
        LogTemp,
        Display,
        TEXT("BH_INPUT_BINDINGS_APPLIED mappings=%d"),
        RuntimePlayerMappingContext->GetMappings().Num()
    );
}

void ABHCharacter::HandleInputBindingsChanged()
{
    RefreshPlayerInputMappings();
}

void ABHCharacter::SetupPlayerInputComponent(
    UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);

    EnsureRuntimeInputActions();
    RefreshPlayerInputMappings();

    bool bInventoryKeyboardMappingPresent = false;
    if (IsValid(InventoryAction) && IsValid(RuntimePlayerMappingContext))
    {
        for (const FEnhancedActionKeyMapping& Mapping :
             RuntimePlayerMappingContext->GetMappings())
        {
            if (Mapping.Action == InventoryAction &&
                Mapping.Key.IsValid() &&
                !Mapping.Key.IsGamepadKey())
            {
                bInventoryKeyboardMappingPresent = true;
                break;
            }
        }
    }

    if (!bInventoryKeyboardMappingPresent)
    {
        PlayerInputComponent->BindKey(
            EKeys::I,
            IE_Pressed,
            this,
            &ABHCharacter::ToggleInventoryPanel
        );
        UE_LOG(
            LogTemp,
            Warning,
            TEXT("BH_INVENTORY_KEY_FALLBACK_BOUND key=I")
        );
    }

    UEnhancedInputComponent* EnhancedInputComponent =
        Cast<UEnhancedInputComponent>(PlayerInputComponent);

    if (!EnhancedInputComponent)
    {
        return;
    }

    EnhancedInputComponent->BindAction(
        WarMapInputAction,
        ETriggerEvent::Started,
        this,
        &ABHCharacter::ToggleWarMap
    );
    EnhancedInputComponent->BindAction(
        GrenadeInputAction,
        ETriggerEvent::Started,
        this,
        &ABHCharacter::BeginFragGrenadeCook
    );
    EnhancedInputComponent->BindAction(
        GrenadeInputAction,
        ETriggerEvent::Completed,
        this,
        &ABHCharacter::ReleaseFragGrenade
    );
    EnhancedInputComponent->BindAction(
        GrenadeInputAction,
        ETriggerEvent::Canceled,
        this,
        &ABHCharacter::ReleaseFragGrenade
    );
    EnhancedInputComponent->BindAction(
        SmokeGrenadeInputAction,
        ETriggerEvent::Started,
        this,
        &ABHCharacter::ThrowSmokeGrenade
    );
    EnhancedInputComponent->BindAction(
        TacticalFlashlightInputAction,
        ETriggerEvent::Started,
        this,
        &ABHCharacter::ToggleTacticalFlashlight
    );
    EnhancedInputComponent->BindAction(
        WeaponBashInputAction,
        ETriggerEvent::Started,
        this,
        &ABHCharacter::PerformWeaponBash
    );
    EnhancedInputComponent->BindAction(
        FieldObservationInputAction,
        ETriggerEvent::Started,
        this,
        &ABHCharacter::PerformFieldObservation
    );
    EnhancedInputComponent->BindAction(
        ControlledBreathingInputAction,
        ETriggerEvent::Started,
        this,
        &ABHCharacter::BeginControlledBreathing
    );
    EnhancedInputComponent->BindAction(
        ControlledBreathingInputAction,
        ETriggerEvent::Completed,
        this,
        &ABHCharacter::EndControlledBreathing
    );
    EnhancedInputComponent->BindAction(
        ControlledBreathingInputAction,
        ETriggerEvent::Canceled,
        this,
        &ABHCharacter::EndControlledBreathing
    );
    EnhancedInputComponent->BindAction(
        EngineeringInputAction,
        ETriggerEvent::Started,
        this,
        &ABHCharacter::UseEngineeringTool
    );
    EnhancedInputComponent->BindAction(
        AntiVehicleInputAction,
        ETriggerEvent::Started,
        this,
        &ABHCharacter::LaunchAntiVehicleProjectile
    );
    EnhancedInputComponent->BindAction(
        SquadOrderInputAction,
        ETriggerEvent::Started,
        this,
        &ABHCharacter::ToggleFriendlySquadOrder
    );
    EnhancedInputComponent->BindAction(
        ContextInputAction,
        ETriggerEvent::Started,
        this,
        &ABHCharacter::IssueFieldSquadContextAction
    );
    EnhancedInputComponent->BindAction(
        SquadPingInputAction,
        ETriggerEvent::Started,
        this,
        &ABHCharacter::IssueSquadPing
    );
    EnhancedInputComponent->BindAction(
        FireModeInputAction,
        ETriggerEvent::Started,
        this,
        &ABHCharacter::ToggleFireMode
    );

    if (MoveAction)
    {
        EnhancedInputComponent->BindAction(
            MoveAction,
            ETriggerEvent::Triggered,
            this,
            &ABHCharacter::Move);
    }

    if (LookAction)
    {
        EnhancedInputComponent->BindAction(
            LookAction,
            ETriggerEvent::Triggered,
            this,
            &ABHCharacter::Look);
    }
    
    if (JumpAction)
    {
        UE_LOG(LogTemp, Warning, TEXT("JumpAction is assigned"));

        EnhancedInputComponent->BindAction(
            JumpAction,
            ETriggerEvent::Triggered,
            this,
            &ABHCharacter::StartJump);

        EnhancedInputComponent->BindAction(
            JumpAction,
            ETriggerEvent::Completed,
            this,
            &ABHCharacter::StopJump);
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("JumpAction is NOT assigned"));
    }

    if (SprintAction)
    {
        EnhancedInputComponent->BindAction(
            SprintAction,
            ETriggerEvent::Started,
            this,
            &ABHCharacter::HandleSprintPressed);

        EnhancedInputComponent->BindAction(
            SprintAction,
            ETriggerEvent::Completed,
            this,
            &ABHCharacter::HandleSprintReleased);
        EnhancedInputComponent->BindAction(
            SprintAction,
            ETriggerEvent::Canceled,
            this,
            &ABHCharacter::HandleSprintReleased);
    }

    if (CrouchAction)
    {
        EnhancedInputComponent->BindAction(
            CrouchAction,
            ETriggerEvent::Started,
            this,
            &ABHCharacter::HandleCrouchPressed);

        EnhancedInputComponent->BindAction(
            CrouchAction,
            ETriggerEvent::Completed,
            this,
            &ABHCharacter::HandleCrouchReleased);
        EnhancedInputComponent->BindAction(
            CrouchAction,
            ETriggerEvent::Canceled,
            this,
            &ABHCharacter::HandleCrouchReleased);
    }

    if (LeanLeftAction)
    {
        EnhancedInputComponent->BindAction(
            LeanLeftAction,
            ETriggerEvent::Started,
            this,
            &ABHCharacter::HandleLeanLeftPressed
        );
        EnhancedInputComponent->BindAction(
            LeanLeftAction,
            ETriggerEvent::Completed,
            this,
            &ABHCharacter::HandleLeanLeftReleased
        );
        EnhancedInputComponent->BindAction(
            LeanLeftAction,
            ETriggerEvent::Canceled,
            this,
            &ABHCharacter::HandleLeanLeftReleased
        );
    }
    else
    {
        PlayerInputComponent->BindKey(
            EKeys::Q,
            IE_Pressed,
            this,
            &ABHCharacter::StartLeanLeft
        );
        PlayerInputComponent->BindKey(
            EKeys::Q,
            IE_Released,
            this,
            &ABHCharacter::StopLeanLeft
        );
    }

    if (LeanRightAction)
    {
        EnhancedInputComponent->BindAction(
            LeanRightAction,
            ETriggerEvent::Started,
            this,
            &ABHCharacter::HandleLeanRightPressed
        );
        EnhancedInputComponent->BindAction(
            LeanRightAction,
            ETriggerEvent::Completed,
            this,
            &ABHCharacter::HandleLeanRightReleased
        );
        EnhancedInputComponent->BindAction(
            LeanRightAction,
            ETriggerEvent::Canceled,
            this,
            &ABHCharacter::HandleLeanRightReleased
        );
    }
    else
    {
        PlayerInputComponent->BindKey(
            EKeys::E,
            IE_Pressed,
            this,
            &ABHCharacter::StartLeanRight
        );
        PlayerInputComponent->BindKey(
            EKeys::E,
            IE_Released,
            this,
            &ABHCharacter::StopLeanRight
        );
    }

    if (ProneAction)
    {
        EnhancedInputComponent->BindAction(
            ProneAction,
            ETriggerEvent::Started,
            this,
            &ABHCharacter::HandlePronePressed
        );
        EnhancedInputComponent->BindAction(
            ProneAction,
            ETriggerEvent::Completed,
            this,
            &ABHCharacter::HandleProneReleased
        );
        EnhancedInputComponent->BindAction(
            ProneAction,
            ETriggerEvent::Canceled,
            this,
            &ABHCharacter::HandleProneReleased
        );
    }
    else
    {
        PlayerInputComponent->BindKey(
            EKeys::Z,
            IE_Pressed,
            this,
            &ABHCharacter::ToggleProne
        );
    }
    
     if (InteractAction)
    {
        EnhancedInputComponent->BindAction(
            InteractAction,
            ETriggerEvent::Started,
            this,
            &ABHCharacter::HandleInteractPressed);
        EnhancedInputComponent->BindAction(
            InteractAction,
            ETriggerEvent::Completed,
            this,
            &ABHCharacter::HandleInteractReleased);
        EnhancedInputComponent->BindAction(
            InteractAction,
            ETriggerEvent::Canceled,
            this,
            &ABHCharacter::CancelInteractionInput);
    }

    if (FireAction)
    {
        EnhancedInputComponent->BindAction(
            FireAction,
            ETriggerEvent::Started,
            this,
            &ABHCharacter::StartFire
        );
        EnhancedInputComponent->BindAction(
            FireAction,
            ETriggerEvent::Completed,
            this,
            &ABHCharacter::StopFire
        );
        EnhancedInputComponent->BindAction(
            FireAction,
            ETriggerEvent::Canceled,
            this,
            &ABHCharacter::StopFire
        );
    }

    if (AimAction)
    {
        EnhancedInputComponent->BindAction(
            AimAction,
            ETriggerEvent::Started,
            this,
            &ABHCharacter::HandleAimPressed
        );
        EnhancedInputComponent->BindAction(
            AimAction,
            ETriggerEvent::Completed,
            this,
            &ABHCharacter::HandleAimReleased
        );
        EnhancedInputComponent->BindAction(
            AimAction,
            ETriggerEvent::Canceled,
            this,
            &ABHCharacter::HandleAimReleased
        );
    }

    if (ReloadAction)
    {
        EnhancedInputComponent->BindAction(
            ReloadAction,
            ETriggerEvent::Started,
            this,
            &ABHCharacter::Reload
        );
    }

    if (PauseAction)
    {
        EnhancedInputComponent->BindAction(
            PauseAction,
            ETriggerEvent::Started,
            this,
            &ABHCharacter::TogglePauseMenu
        );
    }

    if (InventoryAction)
    {
        EnhancedInputComponent->BindAction(
            InventoryAction,
            ETriggerEvent::Started,
            this,
            &ABHCharacter::ToggleInventoryPanel
        );
    }

    if (InventoryCycleAction)
    {
        EnhancedInputComponent->BindAction(
            InventoryCycleAction,
            ETriggerEvent::Started,
            this,
            &ABHCharacter::CycleInventoryWeaponRole
        );
    }

    if (FieldDressingAction)
    {
        EnhancedInputComponent->BindAction(
            FieldDressingAction,
            ETriggerEvent::Started,
            this,
            &ABHCharacter::UseFieldDressing
        );
    }
    else
    {
        PlayerInputComponent->BindKey(
            EKeys::H,
            IE_Pressed,
            this,
            &ABHCharacter::UseFieldDressing
        );
    }

    if (MedkitAction)
    {
        EnhancedInputComponent->BindAction(
            MedkitAction,
            ETriggerEvent::Started,
            this,
            &ABHCharacter::UseMedkit
        );
    }
    else
    {
        PlayerInputComponent->BindKey(
            EKeys::J,
            IE_Pressed,
            this,
            &ABHCharacter::UseMedkit
        );
    }
}

void ABHCharacter::Move(const FInputActionValue& Value)
{
    const FVector2D MovementInput =
        Value.Get<FVector2D>();

    if (!Controller || bIsTraversing)
    {
        return;
    }

    if (IsValid(InjuryComponent) &&
        InjuryComponent->IsMedkitTreatmentActive())
    {
        if (UCharacterMovementComponent* MovementComponent =
                GetCharacterMovement())
        {
            MovementComponent->StopMovementImmediately();
        }
        return;
    }

    const FRotator ControlRotation =
        Controller->GetControlRotation();

    const FRotator YawRotation(
        0.0f,
        ControlRotation.Yaw,
        0.0f);

    const FVector ForwardDirection =
        FRotationMatrix(YawRotation)
        .GetUnitAxis(EAxis::X);

    const FVector RightDirection =
        FRotationMatrix(YawRotation)
        .GetUnitAxis(EAxis::Y);

    AddMovementInput(
        ForwardDirection,
        MovementInput.Y);

    AddMovementInput(
        RightDirection,
        MovementInput.X);
}

void ABHCharacter::Look(const FInputActionValue& Value)
{
    const FVector2D LookInput =
        Value.Get<FVector2D>();

    float HorizontalSensitivity = 1.0f;
    float VerticalSensitivity = 1.0f;
    float ADSSensitivityMultiplier = 1.0f;
    bool bInvertVerticalLook = false;

    if (UGameInstance* GameInstance = GetGameInstance())
    {
        if (const UBHUserSettingsSubsystem* SettingsSubsystem =
            GameInstance->GetSubsystem<UBHUserSettingsSubsystem>())
        {
            HorizontalSensitivity = SettingsSubsystem->
                GetHorizontalLookSensitivity();
            VerticalSensitivity = SettingsSubsystem->
                GetVerticalLookSensitivity();
            ADSSensitivityMultiplier = SettingsSubsystem->
                GetADSSensitivityMultiplier();
            bInvertVerticalLook = SettingsSubsystem->
                IsVerticalLookInverted();
        }
    }

    const FVector2D AdjustedLookInput =
        UBHUserSettingsSubsystem::CalculateLookInput(
            LookInput,
            HorizontalSensitivity,
            VerticalSensitivity,
            ADSSensitivityMultiplier,
            IsValid(WeaponComponent) && WeaponComponent->IsAiming(),
            bInvertVerticalLook
        );
    AddControllerYawInput(AdjustedLookInput.X);
    AddControllerPitchInput(AdjustedLookInput.Y);
}

void ABHCharacter::StartJump()
{
    if (bIsTraversing ||
        bIsProne ||
        bIsHandlingDeath ||
        bIsHandlingMissionComplete ||
        (IsValid(InjuryComponent) &&
            InjuryComponent->IsMedkitTreatmentActive()))
    {
        return;
    }

    if (TryStartTraversal())
    {
        return;
    }

    Jump();
}

void ABHCharacter::StopJump()
{
    StopJumping();
}

bool ABHCharacter::IsTraversing() const
{
    return bIsTraversing;
}

bool ABHCharacter::IsMantling() const
{
    return bIsTraversing && bTraversalIsMantle;
}

bool ABHCharacter::CanStartTraversal() const
{
    const UCharacterMovementComponent* MovementComponent =
        GetCharacterMovement();

    return !bIsTraversing &&
        !bIsProne &&
        !bIsCrouched &&
        !bIsHandlingDeath &&
        !bIsHandlingMissionComplete &&
        IsValid(MovementComponent) &&
        MovementComponent->IsMovingOnGround() &&
        (!IsValid(InjuryComponent) ||
            (!InjuryComponent->IsMedkitTreatmentActive() &&
             !InjuryComponent->IsArmInjured() &&
             !InjuryComponent->IsLegInjured()));
}

bool ABHCharacter::TryStartTraversal()
{
    if (!CanStartTraversal())
    {
        return false;
    }

    bool bMantle = false;
    FVector TargetLocation = FVector::ZeroVector;
    FVector ApexLocation = FVector::ZeroVector;

    if (!FindTraversalTarget(
        bMantle,
        TargetLocation,
        ApexLocation
    ))
    {
        return false;
    }

    const float StaminaCost = bMantle
        ? MantleStaminaCost
        : VaultStaminaCost;

    if (!SpendStamina(StaminaCost))
    {
        return false;
    }

    UCharacterMovementComponent* MovementComponent =
        GetCharacterMovement();

    if (!IsValid(MovementComponent))
    {
        return false;
    }

    StopSprint();

    if (IsValid(WeaponComponent))
    {
        WeaponComponent->StopAllActions();
    }

    TraversalStartLocation = GetActorLocation();
    TraversalApexLocation = ApexLocation;
    TraversalTargetLocation = TargetLocation;
    TraversalElapsed = 0.0f;
    TraversalDuration = FMath::Max(
        KINDA_SMALL_NUMBER,
        bMantle ? MantleDuration : VaultDuration
    );
    bTraversalIsMantle = bMantle;
    bIsTraversing = true;

    MovementComponent->StopMovementImmediately();
    MovementComponent->SetMovementMode(MOVE_Flying);
    UpdateLean(0.0f);

    return true;
}

bool ABHCharacter::FindTraversalTarget(
    bool& bOutMantle,
    FVector& OutTargetLocation,
    FVector& OutApexLocation
) const
{
    UWorld* World = GetWorld();
    const UCapsuleComponent* CollisionCapsule =
        GetCapsuleComponent();
    const UCharacterMovementComponent* MovementComponent =
        GetCharacterMovement();

    if (!IsValid(World) ||
        !IsValid(CollisionCapsule) ||
        !IsValid(MovementComponent))
    {
        return false;
    }

    const float CapsuleRadius =
        CollisionCapsule->GetScaledCapsuleRadius();
    const float CapsuleHalfHeight =
        CollisionCapsule->GetScaledCapsuleHalfHeight();
    const FVector ActorLocation = GetActorLocation();
    const float FloorZ = ActorLocation.Z - CapsuleHalfHeight;

    FRotator FacingRotation = GetActorRotation();

    if (IsValid(Controller))
    {
        FacingRotation = Controller->GetControlRotation();
    }

    FacingRotation.Pitch = 0.0f;
    FacingRotation.Roll = 0.0f;

    const FVector ForwardDirection =
        FacingRotation.Vector().GetSafeNormal2D();
    const FVector FaceTraceStart =
        FVector(
            ActorLocation.X,
            ActorLocation.Y,
            FloorZ + TraversalMinimumHeight
        ) +
        (ForwardDirection * (CapsuleRadius + 2.0f));
    const FVector FaceTraceEnd =
        FaceTraceStart +
        (ForwardDirection * TraversalReach);

    FCollisionQueryParams QueryParams(
        SCENE_QUERY_STAT(BHTraversalFaceTrace),
        false,
        this
    );
    QueryParams.AddIgnoredActor(this);

    FHitResult FaceHit;

    if (!World->LineTraceSingleByChannel(
        FaceHit,
        FaceTraceStart,
        FaceTraceEnd,
        ECC_Visibility,
        QueryParams
    ))
    {
        return false;
    }

    const float FacingDot = FVector::DotProduct(
        FaceHit.ImpactNormal,
        -ForwardDirection
    );

    if (FMath::Abs(FaceHit.ImpactNormal.Z) > 0.35f ||
        FacingDot < 0.25f)
    {
        return false;
    }

    const FVector TopProbeLocation =
        FaceHit.ImpactPoint +
        (ForwardDirection * TraversalSurfaceProbeDepth);
    const FVector TopTraceStart(
        TopProbeLocation.X,
        TopProbeLocation.Y,
        FloorZ + MantleMaximumHeight + 20.0f
    );
    const FVector TopTraceEnd(
        TopProbeLocation.X,
        TopProbeLocation.Y,
        FloorZ + TraversalMinimumHeight
    );

    FHitResult TopHit;

    if (!World->LineTraceSingleByChannel(
        TopHit,
        TopTraceStart,
        TopTraceEnd,
        ECC_Visibility,
        QueryParams
    ) ||
        !MovementComponent->IsWalkable(TopHit))
    {
        return false;
    }

    const float ObstacleHeight = TopHit.ImpactPoint.Z - FloorZ;

    if (ObstacleHeight < TraversalMinimumHeight ||
        ObstacleHeight > MantleMaximumHeight)
    {
        return false;
    }

    bOutMantle = ObstacleHeight > VaultMaximumHeight;

    FVector TargetSurfacePoint = TopHit.ImpactPoint;

    if (bOutMantle)
    {
        const FVector MantleProbeLocation =
            TopHit.ImpactPoint +
            (ForwardDirection *
                (CapsuleRadius + MantleForwardOffset));
        const FVector MantleTraceStart(
            MantleProbeLocation.X,
            MantleProbeLocation.Y,
            TopHit.ImpactPoint.Z + CapsuleHalfHeight
        );
        const FVector MantleTraceEnd(
            MantleProbeLocation.X,
            MantleProbeLocation.Y,
            TopHit.ImpactPoint.Z -
                TraversalMaximumDrop
        );

        FHitResult MantleLandingHit;

        if (!World->LineTraceSingleByChannel(
            MantleLandingHit,
            MantleTraceStart,
            MantleTraceEnd,
            ECC_Visibility,
            QueryParams
        ) ||
            !MovementComponent->IsWalkable(MantleLandingHit))
        {
            return false;
        }

        TargetSurfacePoint = MantleLandingHit.ImpactPoint;
    }
    else
    {
        const FVector LandingProbeLocation =
            FaceHit.ImpactPoint +
            (ForwardDirection * VaultForwardDistance);
        const FVector LandingTraceStart(
            LandingProbeLocation.X,
            LandingProbeLocation.Y,
            TopHit.ImpactPoint.Z + CapsuleHalfHeight
        );
        const FVector LandingTraceEnd(
            LandingProbeLocation.X,
            LandingProbeLocation.Y,
            FloorZ - TraversalMaximumDrop
        );

        FHitResult LandingHit;

        if (!World->LineTraceSingleByChannel(
            LandingHit,
            LandingTraceStart,
            LandingTraceEnd,
            ECC_Visibility,
            QueryParams
        ) ||
            !MovementComponent->IsWalkable(LandingHit))
        {
            return false;
        }

        TargetSurfacePoint = LandingHit.ImpactPoint;
    }

    OutTargetLocation =
        TargetSurfacePoint +
        (FVector::UpVector *
            (CapsuleHalfHeight + TraversalLandingClearance));

    const float ApexZ = FMath::Max3(
        ActorLocation.Z,
        OutTargetLocation.Z + TraversalApexClearance,
        TopHit.ImpactPoint.Z +
            CapsuleHalfHeight +
            TraversalApexClearance
    );
    OutApexLocation = FVector(
        ActorLocation.X,
        ActorLocation.Y,
        ApexZ
    );

    return IsTraversalPathClear(
        ActorLocation,
        OutApexLocation,
        OutTargetLocation
    );
}

bool ABHCharacter::IsTraversalPathClear(
    const FVector& StartLocation,
    const FVector& ApexLocation,
    const FVector& TargetLocation
) const
{
    const UWorld* World = GetWorld();
    const UCapsuleComponent* CollisionCapsule =
        GetCapsuleComponent();

    if (!IsValid(World) || !IsValid(CollisionCapsule))
    {
        return false;
    }

    const FCollisionShape CapsuleShape =
        FCollisionShape::MakeCapsule(
            CollisionCapsule->GetScaledCapsuleRadius(),
            CollisionCapsule->GetScaledCapsuleHalfHeight()
        );
    const ECollisionChannel CollisionChannel =
        CollisionCapsule->GetCollisionObjectType();

    FCollisionQueryParams QueryParams(
        SCENE_QUERY_STAT(BHTraversalClearance),
        false,
        this
    );
    QueryParams.AddIgnoredActor(this);

    if (World->OverlapBlockingTestByChannel(
        TargetLocation,
        GetActorQuat(),
        CollisionChannel,
        CapsuleShape,
        QueryParams
    ))
    {
        return false;
    }

    const FVector AboveTarget(
        TargetLocation.X,
        TargetLocation.Y,
        ApexLocation.Z
    );
    FHitResult PathHit;

    if (World->SweepSingleByChannel(
        PathHit,
        StartLocation,
        ApexLocation,
        GetActorQuat(),
        CollisionChannel,
        CapsuleShape,
        QueryParams
    ))
    {
        return false;
    }

    if (World->SweepSingleByChannel(
        PathHit,
        ApexLocation,
        AboveTarget,
        GetActorQuat(),
        CollisionChannel,
        CapsuleShape,
        QueryParams
    ))
    {
        return false;
    }

    return !World->SweepSingleByChannel(
        PathHit,
        AboveTarget,
        TargetLocation,
        GetActorQuat(),
        CollisionChannel,
        CapsuleShape,
        QueryParams
    );
}

bool ABHCharacter::SpendStamina(float Amount)
{
    const float ClampedAmount = FMath::Max(0.0f, Amount) *
        GetCarryLoadProfile().TraversalCostMultiplier;

    if (CurrentStamina + KINDA_SMALL_NUMBER < ClampedAmount)
    {
        return false;
    }

    CurrentStamina = FMath::Clamp(
        CurrentStamina - ClampedAmount,
        0.0f,
        MaxStamina
    );
    TimeSinceSprintStopped = 0.0f;
    OnStaminaChanged.Broadcast(CurrentStamina, MaxStamina);

    if (IsValid(CombatStatusWidget))
    {
        CombatStatusWidget->SetStamina(
            CurrentStamina,
            MaxStamina
        );
    }

    return true;
}

void ABHCharacter::UpdateTraversal(float DeltaTime)
{
    if (!bIsTraversing)
    {
        return;
    }

    TraversalElapsed += FMath::Max(0.0f, DeltaTime);

    const float TraversalAlpha = FMath::Clamp(
        TraversalElapsed /
            FMath::Max(KINDA_SMALL_NUMBER, TraversalDuration),
        0.0f,
        1.0f
    );
    const FVector AboveTarget(
        TraversalTargetLocation.X,
        TraversalTargetLocation.Y,
        TraversalApexLocation.Z
    );

    FVector DesiredLocation = TraversalTargetLocation;

    if (TraversalAlpha < 0.35f)
    {
        const float PhaseAlpha = FMath::SmoothStep(
            0.0f,
            1.0f,
            TraversalAlpha / 0.35f
        );
        DesiredLocation = FMath::Lerp(
            TraversalStartLocation,
            TraversalApexLocation,
            PhaseAlpha
        );
    }
    else if (TraversalAlpha < 0.8f)
    {
        const float PhaseAlpha = FMath::SmoothStep(
            0.0f,
            1.0f,
            (TraversalAlpha - 0.35f) / 0.45f
        );
        DesiredLocation = FMath::Lerp(
            TraversalApexLocation,
            AboveTarget,
            PhaseAlpha
        );
    }
    else
    {
        const float PhaseAlpha = FMath::SmoothStep(
            0.0f,
            1.0f,
            (TraversalAlpha - 0.8f) / 0.2f
        );
        DesiredLocation = FMath::Lerp(
            AboveTarget,
            TraversalTargetLocation,
            PhaseAlpha
        );
    }

    FHitResult MovementHit;
    SetActorLocation(
        DesiredLocation,
        true,
        &MovementHit,
        ETeleportType::None
    );

    if (MovementHit.bBlockingHit &&
        !MovementHit.bStartPenetrating &&
        TraversalAlpha < 1.0f - KINDA_SMALL_NUMBER)
    {
        FinishTraversal(false);
        return;
    }

    if (TraversalAlpha >= 1.0f - KINDA_SMALL_NUMBER)
    {
        FinishTraversal(true);
    }
}

void ABHCharacter::FinishTraversal(bool bCompleted)
{
    if (!bIsTraversing)
    {
        return;
    }

    if (bCompleted)
    {
        SetActorLocation(
            TraversalTargetLocation,
            false,
            nullptr,
            ETeleportType::None
        );
    }

    bIsTraversing = false;
    bTraversalIsMantle = false;
    TraversalElapsed = 0.0f;
    TraversalDuration = 0.0f;

    if (UCharacterMovementComponent* MovementComponent =
        GetCharacterMovement())
    {
        MovementComponent->StopMovementImmediately();
        MovementComponent->SetMovementMode(MOVE_Walking);
    }
}

const UBHUserSettingsSubsystem* ABHCharacter::GetUserSettings() const
{
    return GetGameInstance()
        ? GetGameInstance()->GetSubsystem<UBHUserSettingsSubsystem>()
        : nullptr;
}

bool ABHCharacter::ResolveToggleHoldState(
    bool bCurrentState,
    bool bToggleMode,
    bool bPressed
)
{
    if (bPressed)
    {
        return bToggleMode ? !bCurrentState : true;
    }
    return bToggleMode ? bCurrentState : false;
}

bool ABHCharacter::ShouldCommitHeldInteraction(
    float HeldDuration,
    float RequiredDuration
)
{
    return HeldDuration >= FMath::Max(0.1f, RequiredDuration);
}

void ABHCharacter::HandleSprintPressed()
{
    const UBHUserSettingsSubsystem* Settings = GetUserSettings();
    const bool bToggle = IsValid(Settings) &&
        Settings->IsToggleSprintEnabled();
    if (ResolveToggleHoldState(bIsSprinting, bToggle, true))
    {
        StartSprint();
    }
    else
    {
        StopSprint();
    }
}

void ABHCharacter::HandleSprintReleased()
{
    const UBHUserSettingsSubsystem* Settings = GetUserSettings();
    const bool bToggle = IsValid(Settings) &&
        Settings->IsToggleSprintEnabled();
    if (!ResolveToggleHoldState(bIsSprinting, bToggle, false))
    {
        StopSprint();
    }
}

void ABHCharacter::StartSprint()
{
    if (CurrentStamina <= 0.0f ||
        bIsTraversing ||
        bIsProne ||
        bIsHandlingDeath ||
        bIsHandlingMissionComplete ||
        (IsValid(InjuryComponent) &&
            InjuryComponent->IsMedkitTreatmentActive()) ||
        (IsValid(WeaponComponent) &&
            (WeaponComponent->IsAiming() ||
             WeaponComponent->IsFiring())))
    {
        return;
    }

    bIsSprinting = true;
    TimeSinceSprintStopped = 0.0f;

    ApplyMovementSpeed();
}

void ABHCharacter::StopSprint()
{
    bIsSprinting = false;
    TimeSinceSprintStopped = 0.0f;

    ApplyMovementSpeed();
}


void ABHCharacter::HandleCrouchPressed()
{
    const UBHUserSettingsSubsystem* Settings = GetUserSettings();
    const bool bToggle = IsValid(Settings) &&
        Settings->IsToggleCrouchEnabled();
    if (ResolveToggleHoldState(bIsCrouched, bToggle, true))
    {
        StartCrouch();
    }
    else
    {
        StopCrouch();
    }
}

void ABHCharacter::HandleCrouchReleased()
{
    const UBHUserSettingsSubsystem* Settings = GetUserSettings();
    const bool bToggle = IsValid(Settings) &&
        Settings->IsToggleCrouchEnabled();
    if (!ResolveToggleHoldState(bIsCrouched, bToggle, false))
    {
        StopCrouch();
    }
}

void ABHCharacter::StartCrouch()
{
    if (bIsTraversing || bIsProne)
    {
        return;
    }

    Crouch();
}

void ABHCharacter::StopCrouch()
{
    if (bIsTraversing || bIsProne)
    {
        return;
    }

    UnCrouch();
}

void ABHCharacter::ToggleProne()
{
    if (bIsProne)
    {
        TryExitProne();
    }
    else
    {
        EnterProne();
    }
}

void ABHCharacter::HandlePronePressed()
{
    const UBHUserSettingsSubsystem* Settings = GetUserSettings();
    const bool bToggle = !IsValid(Settings) ||
        Settings->IsToggleProneEnabled();
    if (bToggle)
    {
        ToggleProne();
    }
    else
    {
        EnterProne();
    }
}

void ABHCharacter::HandleProneReleased()
{
    const UBHUserSettingsSubsystem* Settings = GetUserSettings();
    if (IsValid(Settings) && !Settings->IsToggleProneEnabled())
    {
        TryExitProne();
    }
}

bool ABHCharacter::CanEnterProne() const
{
    const UCharacterMovementComponent* MovementComponent =
        GetCharacterMovement();

    return !bIsProne &&
        !bIsTraversing &&
        !bIsHandlingDeath &&
        !bIsHandlingMissionComplete &&
        !bIsSprinting &&
        IsValid(MovementComponent) &&
        !MovementComponent->IsFalling() &&
        (!IsValid(InjuryComponent) ||
            !InjuryComponent->IsMedkitTreatmentActive());
}

bool ABHCharacter::EnterProne()
{
    if (!CanEnterProne())
    {
        return false;
    }

    UCapsuleComponent* CollisionCapsule =
        GetCapsuleComponent();

    if (!IsValid(CollisionCapsule))
    {
        return false;
    }

    StopSprint();

    PreProneCapsuleRadius =
        CollisionCapsule->GetUnscaledCapsuleRadius();
    PreProneCapsuleHalfHeight =
        CollisionCapsule->GetUnscaledCapsuleHalfHeight();

    const float TargetRadius = FMath::Max(
        1.0f,
        ProneCapsuleRadius
    );
    const float TargetHalfHeight = FMath::Max(
        TargetRadius,
        ProneCapsuleHalfHeight
    );
    const float CenterDrop = FMath::Max(
        0.0f,
        PreProneCapsuleHalfHeight - TargetHalfHeight
    );

    CollisionCapsule->SetCapsuleSize(
        TargetRadius,
        TargetHalfHeight,
        true
    );

    if (CenterDrop > KINDA_SMALL_NUMBER)
    {
        AddActorWorldOffset(
            FVector(0.0f, 0.0f, -CenterDrop),
            false
        );
        ProneCameraTransitionCompensationZ += CenterDrop;
    }

    bIsProne = true;
    ApplyMovementSpeed();
    UpdateLean(0.0f);
    return true;
}

bool ABHCharacter::HasProneExitClearance(
    float TargetCapsuleRadius,
    float TargetCapsuleHalfHeight
) const
{
    const UCapsuleComponent* CollisionCapsule =
        GetCapsuleComponent();
    const UWorld* World = GetWorld();

    if (!IsValid(CollisionCapsule) || !IsValid(World))
    {
        return false;
    }

    const float CurrentHalfHeight =
        CollisionCapsule->GetUnscaledCapsuleHalfHeight();
    const float CenterRise = FMath::Max(
        0.0f,
        TargetCapsuleHalfHeight - CurrentHalfHeight
    );
    const FVector TargetLocation =
        GetActorLocation() +
        (FVector::UpVector * CenterRise);

    FCollisionQueryParams QueryParams(
        SCENE_QUERY_STAT(BHProneExitClearance),
        false,
        this
    );
    QueryParams.AddIgnoredActor(this);

    return !World->OverlapBlockingTestByChannel(
        TargetLocation,
        GetActorQuat(),
        CollisionCapsule->GetCollisionObjectType(),
        FCollisionShape::MakeCapsule(
            TargetCapsuleRadius,
            TargetCapsuleHalfHeight
        ),
        QueryParams
    );
}

bool ABHCharacter::TryExitProne()
{
    if (!bIsProne)
    {
        return true;
    }

    UCapsuleComponent* CollisionCapsule =
        GetCapsuleComponent();

    if (!IsValid(CollisionCapsule))
    {
        return false;
    }

    const float TargetRadius = FMath::Max(
        1.0f,
        PreProneCapsuleRadius
    );
    const float TargetHalfHeight = FMath::Max(
        TargetRadius,
        PreProneCapsuleHalfHeight
    );

    if (!HasProneExitClearance(
        TargetRadius,
        TargetHalfHeight
    ))
    {
        return false;
    }

    const float CurrentHalfHeight =
        CollisionCapsule->GetUnscaledCapsuleHalfHeight();
    const float CenterRise = FMath::Max(
        0.0f,
        TargetHalfHeight - CurrentHalfHeight
    );

    if (CenterRise > KINDA_SMALL_NUMBER)
    {
        AddActorWorldOffset(
            FVector(0.0f, 0.0f, CenterRise),
            false
        );
        ProneCameraTransitionCompensationZ -= CenterRise;
    }

    CollisionCapsule->SetCapsuleSize(
        TargetRadius,
        TargetHalfHeight,
        true
    );

    bIsProne = false;
    ApplyMovementSpeed();
    UpdateLean(0.0f);
    return true;
}

void ABHCharacter::UpdateProne(float DeltaTime)
{
    const float TargetAlpha = bIsProne ? 1.0f : 0.0f;
    const float InterpolationSpeed =
        FMath::Max(0.0f, ProneTransitionSpeed);

    CurrentProneAlpha = FMath::FInterpTo(
        CurrentProneAlpha,
        TargetAlpha,
        DeltaTime,
        InterpolationSpeed
    );
    ProneCameraTransitionCompensationZ = FMath::FInterpTo(
        ProneCameraTransitionCompensationZ,
        0.0f,
        DeltaTime,
        InterpolationSpeed
    );

    if (FMath::IsNearlyEqual(
        CurrentProneAlpha,
        TargetAlpha,
        KINDA_SMALL_NUMBER
    ))
    {
        CurrentProneAlpha = TargetAlpha;
    }

    if (FMath::IsNearlyZero(
        ProneCameraTransitionCompensationZ,
        KINDA_SMALL_NUMBER
    ))
    {
        ProneCameraTransitionCompensationZ = 0.0f;
    }
}

void ABHCharacter::ApplyMovementSpeed()
{
    UCharacterMovementComponent* MovementComponent =
        GetCharacterMovement();

    if (!IsValid(MovementComponent))
    {
        return;
    }

    const float InjurySpeedMultiplier =
        IsValid(InjuryComponent)
            ? InjuryComponent->GetMovementSpeedMultiplier()
            : 1.0f;
    const float BaseSpeed = bIsProne
        ? ProneSpeed
        : (bIsSprinting ? SprintSpeed : WalkSpeed);
    float WaterSpeedMultiplier = 1.0f;
    for (TActorIterator<ABHWaterSurface> It(GetWorld()); It; ++It)
    {
        if (IsValid(*It) && (*It)->ContainsWorldLocation(GetActorLocation()))
        {
            WaterSpeedMultiplier = FMath::Min(
                WaterSpeedMultiplier,
                (*It)->GetInfantrySpeedMultiplier()
            );
        }
    }

    MovementComponent->MaxWalkSpeed =
        BaseSpeed *
        InjurySpeedMultiplier *
        UBHBattlefieldConditions::GetCurrentProfile(this).
            InfantrySpeedMultiplier *
        GetCarryLoadProfile().MovementSpeedMultiplier *
        WaterSpeedMultiplier;
}

void ABHCharacter::HandleLeanLeftPressed()
{
    const UBHUserSettingsSubsystem* Settings = GetUserSettings();
    const bool bToggle = IsValid(Settings) &&
        Settings->IsToggleLeanEnabled();
    if (ResolveToggleHoldState(bLeanLeftHeld, bToggle, true))
    {
        StopLeanRight();
        StartLeanLeft();
    }
    else
    {
        StopLeanLeft();
    }
}

void ABHCharacter::HandleLeanLeftReleased()
{
    const UBHUserSettingsSubsystem* Settings = GetUserSettings();
    if (!IsValid(Settings) || !Settings->IsToggleLeanEnabled())
    {
        StopLeanLeft();
    }
}

void ABHCharacter::HandleLeanRightPressed()
{
    const UBHUserSettingsSubsystem* Settings = GetUserSettings();
    const bool bToggle = IsValid(Settings) &&
        Settings->IsToggleLeanEnabled();
    if (ResolveToggleHoldState(bLeanRightHeld, bToggle, true))
    {
        StopLeanLeft();
        StartLeanRight();
    }
    else
    {
        StopLeanRight();
    }
}

void ABHCharacter::HandleLeanRightReleased()
{
    const UBHUserSettingsSubsystem* Settings = GetUserSettings();
    if (!IsValid(Settings) || !Settings->IsToggleLeanEnabled())
    {
        StopLeanRight();
    }
}

void ABHCharacter::StartLeanLeft()
{
    bLeanLeftHeld = true;
}

void ABHCharacter::StopLeanLeft()
{
    bLeanLeftHeld = false;
}

void ABHCharacter::StartLeanRight()
{
    bLeanRightHeld = true;
}

void ABHCharacter::StopLeanRight()
{
    bLeanRightHeld = false;
}

bool ABHCharacter::CanLean() const
{
    const UCharacterMovementComponent* MovementComponent =
        GetCharacterMovement();

    return !bIsHandlingDeath &&
        !bIsHandlingMissionComplete &&
        !bIsTraversing &&
        !bIsSprinting &&
        !bIsProne &&
        (!IsValid(MovementComponent) ||
            !MovementComponent->IsFalling()) &&
        (!IsValid(InjuryComponent) ||
            !InjuryComponent->IsMedkitTreatmentActive());
}

float ABHCharacter::GetRequestedLeanDirection() const
{
    if (!CanLean() || bLeanLeftHeld == bLeanRightHeld)
    {
        return 0.0f;
    }

    return bLeanRightHeld ? 1.0f : -1.0f;
}

float ABHCharacter::ResolveCollisionLimitedLean(
    float LeanDirection
) const
{
    if (!IsValid(FirstPersonCamera) ||
        !IsValid(GetWorld()) ||
        !bLeanCameraBaseCached ||
        FMath::IsNearlyZero(LeanDirection) ||
        MaximumLeanDistance <= KINDA_SMALL_NUMBER)
    {
        return LeanDirection;
    }

    const USceneComponent* CameraParent =
        FirstPersonCamera->GetAttachParent();

    if (!IsValid(CameraParent))
    {
        return 0.0f;
    }

    FVector CameraBaseLocation = LeanCameraBaseRelativeLocation;
    CameraBaseLocation.Z -=
        CurrentProneAlpha * ProneCameraDrop;
    CameraBaseLocation.Z +=
        ProneCameraTransitionCompensationZ;

    const FVector TraceStart =
        CameraParent->GetComponentTransform().TransformPosition(
            CameraBaseLocation
        );
    const FVector TraceEnd =
        TraceStart +
        (GetActorRightVector() *
            LeanDirection *
            MaximumLeanDistance);

    FHitResult HitResult;
    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActor(this);

    const bool bBlocked = GetWorld()->SweepSingleByChannel(
        HitResult,
        TraceStart,
        TraceEnd,
        FQuat::Identity,
        ECC_Visibility,
        FCollisionShape::MakeSphere(
            FMath::Max(0.0f, LeanCollisionRadius)
        ),
        QueryParams
    );

    if (!bBlocked)
    {
        return LeanDirection;
    }

    const float AllowedDistance = FMath::Max(
        0.0f,
        HitResult.Distance -
            FMath::Max(0.0f, LeanCollisionPadding)
    );

    return LeanDirection *
        FMath::Clamp(
            AllowedDistance / MaximumLeanDistance,
            0.0f,
            1.0f
        );
}

void ABHCharacter::UpdateLean(float DeltaTime)
{
    if (!IsValid(FirstPersonCamera) || !bLeanCameraBaseCached)
    {
        return;
    }

    const float TargetLeanAmount =
        ResolveCollisionLimitedLean(
            GetRequestedLeanDirection()
        );
    float NewLeanAmount = FMath::FInterpTo(
        CurrentLeanAmount,
        TargetLeanAmount,
        DeltaTime,
        FMath::Max(0.0f, LeanInterpolationSpeed)
    );

    if (FMath::Sign(NewLeanAmount) ==
            FMath::Sign(TargetLeanAmount) &&
        FMath::Abs(NewLeanAmount) >
            FMath::Abs(TargetLeanAmount))
    {
        NewLeanAmount = TargetLeanAmount;
    }

    CurrentLeanAmount = FMath::Clamp(
        NewLeanAmount,
        -1.0f,
        1.0f
    );

    if (FMath::IsNearlyZero(
        CurrentLeanAmount,
        KINDA_SMALL_NUMBER
    ))
    {
        CurrentLeanAmount = 0.0f;
    }

    FVector CameraLocation = LeanCameraBaseRelativeLocation;
    CameraLocation.Y += CurrentLeanAmount * MaximumLeanDistance;
    CameraLocation.Z -= CurrentProneAlpha * ProneCameraDrop;
    CameraLocation.Z += ProneCameraTransitionCompensationZ;
    CameraLocation.Z += CurrentHeadBobOffsetZ;

    FRotator CameraRotation = LeanCameraBaseRelativeRotation;
    CameraRotation.Roll += CurrentLeanAmount * MaximumLeanRoll;

    FirstPersonCamera->SetRelativeLocationAndRotation(
        CameraLocation,
        CameraRotation
    );
}

void ABHCharacter::StartFire()
{
    if (bIsHandlingDeath ||
        bIsHandlingMissionComplete ||
        bIsTraversing ||
        (IsValid(InjuryComponent) &&
            InjuryComponent->IsMedkitTreatmentActive()) ||
        !IsValid(WeaponComponent))
    {
        return;
    }

    StopSprint();
    WeaponComponent->StartFiring();
}

void ABHCharacter::StopFire()
{
    if (IsValid(WeaponComponent))
    {
        WeaponComponent->StopFiring();
    }
}

void ABHCharacter::HandleAimPressed()
{
    const UBHUserSettingsSubsystem* Settings = GetUserSettings();
    const bool bToggle = IsValid(Settings) &&
        Settings->IsToggleAimEnabled();
    const bool bAiming = IsValid(WeaponComponent) &&
        WeaponComponent->IsAiming();
    if (ResolveToggleHoldState(bAiming, bToggle, true))
    {
        StartAim();
    }
    else
    {
        StopAim();
    }
}

void ABHCharacter::UpdateHeadBob(float DeltaTime)
{
    float ComfortScale = 1.0f;
    if (const UGameInstance* GameInstance = GetGameInstance())
    {
        if (const UBHUserSettingsSubsystem* SettingsSubsystem =
            GameInstance->GetSubsystem<UBHUserSettingsSubsystem>())
        {
            ComfortScale = SettingsSubsystem->GetHeadBobScale();
        }
    }

    const UCharacterMovementComponent* Movement = GetCharacterMovement();
    const bool bShouldBob = IsLocallyControlled() && IsValid(Movement) &&
        Movement->IsMovingOnGround() && !bIsProne && !bIsTraversing &&
        GetVelocity().SizeSquared2D() > FMath::Square(10.0f) &&
        ComfortScale > KINDA_SMALL_NUMBER;
    if (bShouldBob)
    {
        const float SpeedAlpha = FMath::Clamp(
            GetVelocity().Size2D() / FMath::Max(1.0f, SprintSpeed),
            0.25f,
            1.0f
        );
        HeadBobPhase = FMath::Fmod(
            HeadBobPhase + DeltaTime * FMath::Max(0.0f, HeadBobFrequency) *
                (0.75f + SpeedAlpha * 0.5f),
            2.0f * PI
        );
        const float TargetOffset = FMath::Sin(HeadBobPhase) *
            FMath::Max(0.0f, HeadBobAmplitude) * SpeedAlpha *
            FMath::Clamp(ComfortScale, 0.0f, 1.0f);
        CurrentHeadBobOffsetZ = FMath::FInterpTo(
            CurrentHeadBobOffsetZ,
            TargetOffset,
            DeltaTime,
            14.0f
        );
    }
    else
    {
        CurrentHeadBobOffsetZ = FMath::FInterpTo(
            CurrentHeadBobOffsetZ,
            0.0f,
            DeltaTime,
            12.0f
        );
    }
}

void ABHCharacter::HandleAimReleased()
{
    const UBHUserSettingsSubsystem* Settings = GetUserSettings();
    const bool bToggle = IsValid(Settings) &&
        Settings->IsToggleAimEnabled();
    const bool bAiming = IsValid(WeaponComponent) &&
        WeaponComponent->IsAiming();
    if (!ResolveToggleHoldState(bAiming, bToggle, false))
    {
        StopAim();
    }
}

void ABHCharacter::StartAim()
{
    if (bIsHandlingDeath ||
        bIsHandlingMissionComplete ||
        bIsTraversing ||
        (IsValid(InjuryComponent) &&
            InjuryComponent->IsMedkitTreatmentActive()) ||
        !IsValid(WeaponComponent))
    {
        return;
    }

    StopSprint();
    WeaponComponent->StartAiming();
}

void ABHCharacter::StopAim()
{
    if (IsValid(WeaponComponent))
    {
        WeaponComponent->StopAiming();
    }
}

void ABHCharacter::Reload()
{
    if (bIsHandlingDeath ||
        bIsHandlingMissionComplete ||
        bIsTraversing ||
        (IsValid(InjuryComponent) &&
            InjuryComponent->IsMedkitTreatmentActive()) ||
        !IsValid(WeaponComponent))
    {
        return;
    }

    const float CurrentTime = IsValid(GetWorld())
        ? GetWorld()->GetTimeSeconds() : 0.0f;
    const bool bEmergencyDoubleTap =
        CurrentTime - LastReloadInputTime <=
            FMath::Max(0.15f, EmergencyReloadDoubleTapWindow);
    LastReloadInputTime = CurrentTime;

    if (bEmergencyDoubleTap)
    {
        const int32 DiscardedRounds = WeaponComponent->GetMagazineAmmo();
        if (WeaponComponent->StartEmergencyReload())
        {
            ShowStatusNotification(FText::Format(
                NSLOCTEXT(
                    "BrokenHorizon",
                    "EmergencyReloadStatus",
                    "EMERGENCY RELOAD // {0} ROUNDS DROPPED\n\n"
                    "Faster weapon recovery."
                ),
                FText::AsNumber(DiscardedRounds)
            ));
        }
        return;
    }

    const int32 RetainedRounds = WeaponComponent->GetMagazineAmmo();
    const bool bEmptyWeapon = RetainedRounds <= 0;
    if (WeaponComponent->StartReload())
    {
        ShowStatusNotification(
            bEmptyWeapon
                ? NSLOCTEXT(
                    "BrokenHorizon",
                    "EmptyEmergencyReloadStatus",
                    "EMPTY RELOAD // EMERGENCY PROCEDURE\n\n"
                    "No ammunition discarded."
                )
                : FText::Format(
                    NSLOCTEXT(
                        "BrokenHorizon",
                        "TacticalReloadStatus",
                        "TACTICAL RELOAD // {0} ROUNDS RETAINED\n\n"
                        "Double-tap reload to drop the magazine and reload faster."
                    ),
                    FText::AsNumber(RetainedRounds)
                )
        );
    }
}

void ABHCharacter::ToggleFireMode()
{
    if (bIsHandlingDeath ||
        bIsHandlingMissionComplete ||
        bIsTraversing ||
        !IsValid(WeaponComponent) ||
        WeaponComponent->IsReloading())
    {
        return;
    }

    const EBHFireMode NewFireMode =
        WeaponComponent->ToggleFireMode();
    if (IsValid(AmmoHUDWidget))
    {
        AmmoHUDWidget->SetFireMode(
            NewFireMode == EBHFireMode::Automatic
                ? NSLOCTEXT(
                    "BrokenHorizon",
                    "WeaponFireModeAutomaticShort",
                    "AUTO"
                )
                : NSLOCTEXT(
                    "BrokenHorizon",
                    "WeaponFireModeSemiAutomaticShort",
                    "SEMI"
                )
        );
    }

    ShowStatusNotification(
        NewFireMode == EBHFireMode::Automatic
        ? NSLOCTEXT(
            "BrokenHorizon",
            "FireModeAutomatic",
            "FIRE MODE // AUTO"
        )
        : NSLOCTEXT(
            "BrokenHorizon",
            "FireModeSemiAutomatic",
            "FIRE MODE // SEMI"
        )
    );
}

void ABHCharacter::BeginFragGrenadeCook()
{
    if (bIsHandlingDeath ||
        bIsHandlingMissionComplete ||
        bPauseMenuOpen ||
        bWarMapOpen ||
        bIsTraversing ||
        bWaitingForInitialWorldStreaming ||
        bFragGrenadeCooking ||
        FragGrenadeCount <= 0 ||
        !FragGrenadeClass ||
        !IsValid(FirstPersonCamera) ||
        !IsValid(GetWorld()) ||
        (IsValid(InjuryComponent) &&
            InjuryComponent->IsMedkitTreatmentActive()))
    {
        return;
    }

    if (IsValid(WeaponComponent))
    {
        WeaponComponent->StopAllActions();
    }

    StopSprint();
    bFragGrenadeCooking = true;
    FragGrenadeCookStartedTime = GetWorld()->GetTimeSeconds();
}

void ABHCharacter::ReleaseFragGrenade()
{
    if (!bFragGrenadeCooking)
    {
        return;
    }

    const float CookDuration = FMath::Clamp(
        GetWorld()->GetTimeSeconds() - FragGrenadeCookStartedTime,
        0.0f,
        FMath::Max(0.0f, MaxFragGrenadeCookDuration)
    );
    bFragGrenadeCooking = false;
    ThrowFragGrenade(CookDuration);
}

void ABHCharacter::BeginControlledBreathing()
{
    SetControlledBreathingRequested(true);
}

void ABHCharacter::EndControlledBreathing()
{
    SetControlledBreathingRequested(false);
}

void ABHCharacter::ThrowFragGrenade()
{
    ThrowFragGrenade(0.0f);
}

void ABHCharacter::ThrowFragGrenade(float CookDuration)
{
    if (bIsHandlingDeath ||
        bIsHandlingMissionComplete ||
        bPauseMenuOpen ||
        bWarMapOpen ||
        bIsTraversing ||
        bWaitingForInitialWorldStreaming ||
        FragGrenadeCount <= 0 ||
        !FragGrenadeClass ||
        !IsValid(FirstPersonCamera) ||
        !IsValid(GetWorld()) ||
        (IsValid(InjuryComponent) &&
            InjuryComponent->IsMedkitTreatmentActive()))
    {
        return;
    }

    if (IsValid(WeaponComponent))
    {
        WeaponComponent->StopAllActions();
    }

    StopSprint();

    const FVector Forward = FirstPersonCamera->GetForwardVector();
    const FVector Right = FirstPersonCamera->GetRightVector();
    const FVector SpawnLocation =
        FirstPersonCamera->GetComponentLocation() +
        Forward * 70.0f +
        Right * 20.0f -
        FVector::UpVector * 10.0f;

    FActorSpawnParameters SpawnParameters;
    SpawnParameters.Owner = this;
    SpawnParameters.Instigator = this;
    SpawnParameters.SpawnCollisionHandlingOverride =
        ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    ABHFragGrenade* Grenade =
        GetWorld()->SpawnActor<ABHFragGrenade>(
            FragGrenadeClass,
            SpawnLocation,
            FirstPersonCamera->GetComponentRotation(),
            SpawnParameters
        );

    if (!IsValid(Grenade))
    {
        return;
    }

    FragGrenadeCount = FMath::Max(0, FragGrenadeCount - 1);
    Grenade->Throw(
        Forward * FMath::Max(0.0f, FragGrenadeThrowSpeed) +
        FVector::UpVector * 200.0f +
        GetVelocity()
    );
    RefreshFragGrenadeHUD();

    if (UBHSaveSubsystem* SaveSubsystem =
        GetGameInstance()
            ? GetGameInstance()->GetSubsystem<UBHSaveSubsystem>()
            : nullptr)
    {
        SaveSubsystem->SavePlayerResources();
    }

    UE_LOG(
        LogTemp,
        Display,
        TEXT("BH_FRAG_THROWN remaining=%d"),
        FragGrenadeCount
    );
}

void ABHCharacter::ThrowSmokeGrenade()
{
    if (bIsHandlingDeath ||
        bIsHandlingMissionComplete ||
        bPauseMenuOpen ||
        bWarMapOpen ||
        bIsTraversing ||
        bWaitingForInitialWorldStreaming ||
        SmokeGrenadeCount <= 0 ||
        !SmokeGrenadeClass ||
        !IsValid(FirstPersonCamera) ||
        !IsValid(GetWorld()) ||
        (IsValid(InjuryComponent) &&
            InjuryComponent->IsMedkitTreatmentActive()))
    {
        return;
    }

    if (IsValid(WeaponComponent))
    {
        WeaponComponent->StopAllActions();
    }

    StopSprint();

    const FVector Forward = FirstPersonCamera->GetForwardVector();
    const FVector Right = FirstPersonCamera->GetRightVector();
    const FVector SpawnLocation =
        FirstPersonCamera->GetComponentLocation() +
        Forward * 70.0f +
        Right * 20.0f -
        FVector::UpVector * 10.0f;

    FActorSpawnParameters SpawnParameters;
    SpawnParameters.Owner = this;
    SpawnParameters.Instigator = this;
    SpawnParameters.SpawnCollisionHandlingOverride =
        ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    ABHSmokeGrenade* Grenade =
        GetWorld()->SpawnActor<ABHSmokeGrenade>(
            SmokeGrenadeClass,
            SpawnLocation,
            FirstPersonCamera->GetComponentRotation(),
            SpawnParameters
        );

    if (!IsValid(Grenade))
    {
        return;
    }

    SmokeGrenadeCount = FMath::Max(0, SmokeGrenadeCount - 1);
    Grenade->Throw(
        Forward * FMath::Max(0.0f, FragGrenadeThrowSpeed) +
        FVector::UpVector * 200.0f +
        GetVelocity(),
        0.0f
    );
    RefreshSmokeGrenadeHUD();
    UpdateCarryLoadHUD();

    if (UBHSaveSubsystem* SaveSubsystem =
        GetGameInstance()
            ? GetGameInstance()->GetSubsystem<UBHSaveSubsystem>()
            : nullptr)
    {
        SaveSubsystem->SavePlayerResources();
    }

    UE_LOG(
        LogTemp,
        Display,
        TEXT("BH_SMOKE_THROWN remaining=%d"),
        SmokeGrenadeCount
    );
}

bool ABHCharacter::IsTacticalFlashlightOn() const
{
    return bTacticalFlashlightOn &&
        TacticalFlashlightBattery > KINDA_SMALL_NUMBER;
}

float ABHCharacter::GetTacticalFlashlightBattery() const
{
    return FMath::Clamp(
        TacticalFlashlightBattery,
        0.0f,
        FMath::Max(1.0f, TacticalFlashlightMaxBattery)
    );
}

void ABHCharacter::ToggleTacticalFlashlight()
{
    SetTacticalFlashlightOn(!IsTacticalFlashlightOn());
}

void ABHCharacter::SetTacticalFlashlightOn(bool bEnabled)
{
    if (!HasAuthority())
    {
        ServerSetTacticalFlashlightOn(bEnabled);
        return;
    }

    const bool bNewState = bEnabled &&
        TacticalFlashlightBattery > KINDA_SMALL_NUMBER;
    if (bTacticalFlashlightOn == bNewState)
    {
        UpdateTacticalFlashlightVisual();
        return;
    }

    bTacticalFlashlightOn = bNewState;
    UpdateTacticalFlashlightVisual();
    ForceNetUpdate();
}

void ABHCharacter::ServerSetTacticalFlashlightOn_Implementation(
    bool bEnabled
)
{
    SetTacticalFlashlightOn(bEnabled);
}

void ABHCharacter::ServerPerformWeaponBash_Implementation()
{
    PerformWeaponBash();
}

void ABHCharacter::RestoreTacticalFlashlightState(
    float SavedBattery,
    bool bSavedOn
)
{
    if (!HasAuthority())
    {
        return;
    }

    TacticalFlashlightBattery = FMath::Clamp(
        SavedBattery,
        0.0f,
        FMath::Max(1.0f, TacticalFlashlightMaxBattery)
    );
    bTacticalFlashlightOn = bSavedOn &&
        TacticalFlashlightBattery > KINDA_SMALL_NUMBER;
    UpdateTacticalFlashlightVisual();
    ForceNetUpdate();
}

void ABHCharacter::OnRep_TacticalFlashlight()
{
    UpdateTacticalFlashlightVisual();
}

void ABHCharacter::UpdateTacticalFlashlightVisual()
{
    if (IsValid(TacticalFlashlight))
    {
        TacticalFlashlight->SetVisibility(IsTacticalFlashlightOn(), true);
    }
}

void ABHCharacter::PerformWeaponBash()
{
    if (!HasAuthority())
    {
        ServerPerformWeaponBash();
        return;
    }

    UWorld* World = GetWorld();
    APlayerController* PlayerController =
        ResolveOwningPlayerController();
    const float CurrentTime = IsValid(World)
        ? World->GetTimeSeconds()
        : 0.0f;
    if (!CanPerformWeaponBash(
            bIsSprinting,
            bIsTraversing,
            bIsHandlingDeath,
            IsValid(DraggedCasualty),
            CurrentStamina,
            WeaponBashStaminaCost,
            CurrentTime,
            LastWeaponBashAllowedTime
        ) ||
        !IsValid(World) ||
        !IsValid(FirstPersonCamera) ||
        !SpendStamina(WeaponBashStaminaCost))
    {
        return;
    }

    LastWeaponBashAllowedTime = CurrentTime +
        FMath::Max(0.05f, WeaponBashCooldown);
    if (IsValid(WeaponComponent))
    {
        WeaponComponent->StopAllActions();
    }

    const FVector Start = FirstPersonCamera->GetComponentLocation();
    const FVector End = Start +
        FirstPersonCamera->GetForwardVector() *
        FMath::Max(0.0f, WeaponBashRange);
    FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(WeaponBash), false);
    QueryParams.AddIgnoredActor(this);

    FHitResult Hit;
    const bool bHit = World->LineTraceSingleByChannel(
        Hit,
        Start,
        End,
        ECC_Visibility,
        QueryParams
    );
    const bool bDamagedTarget = bHit && IsValid(Hit.GetActor()) &&
        UGameplayStatics::ApplyPointDamage(
            Hit.GetActor(),
            FMath::Max(0.0f, WeaponBashDamage),
            FirstPersonCamera->GetForwardVector(),
            Hit,
            GetController(),
            this,
            UDamageType::StaticClass()
        ) > 0.0f;
    const FText Feedback = bDamagedTarget
        ? NSLOCTEXT(
            "BrokenHorizon",
            "WeaponBashContact",
            "WEAPON BASH // CONTACT"
        )
        : NSLOCTEXT(
            "BrokenHorizon",
            "WeaponBashMiss",
            "WEAPON BASH // MISS"
        );
    if (IsLocallyControlled())
    {
        ShowStatusNotification(Feedback);
    }
    else
    {
        ClientShowStatusNotification(Feedback);
    }
}

void ABHCharacter::PerformFieldObservation()
{
    UWorld* World = GetWorld();
    UBHWeaponComponent* ActiveWeapon = GetWeaponComponent();
    const float CurrentTime = IsValid(World)
        ? World->GetTimeSeconds()
        : 0.0f;
    if (!IsValid(World) ||
        !IsValid(FirstPersonCamera) ||
        !CanPerformFieldObservation(
            bIsSprinting,
            bIsTraversing,
            IsValid(ActiveWeapon) && ActiveWeapon->IsAiming(),
            bIsHandlingDeath,
            false,
            GetVelocity().Size2D(),
            FieldObservationMaximumStableSpeed,
            CurrentTime,
            LastFieldObservationAllowedTime
        ))
    {
        return;
    }

    LastFieldObservationAllowedTime = CurrentTime +
        FMath::Max(0.1f, FieldObservationCooldown);
    const FVector Start = FirstPersonCamera->GetComponentLocation();
    const FVector End = Start +
        FirstPersonCamera->GetForwardVector() *
        FMath::Max(0.0f, FieldObservationRange);
    FCollisionQueryParams QueryParams(
        SCENE_QUERY_STAT(FieldObservation),
        false
    );
    QueryParams.AddIgnoredActor(this);

    FHitResult Hit;
    const bool bHit = World->LineTraceSingleByChannel(
        Hit,
        Start,
        End,
        ECC_Visibility,
        QueryParams
    );
    const bool bEnemyContact = bHit &&
        IsValid(Cast<ABHEnemySoldier>(Hit.GetActor()));
    ShowStatusNotification(
        bEnemyContact
            ? NSLOCTEXT(
                "BrokenHorizon",
                "FieldObservationEnemyContact",
                "OBSERVATION // ENEMY CONTACT"
            )
            : bHit
                ? NSLOCTEXT(
                    "BrokenHorizon",
                    "FieldObservationUnknownContact",
                    "OBSERVATION // UNKNOWN CONTACT"
                )
                : NSLOCTEXT(
                    "BrokenHorizon",
                    "FieldObservationClear",
                    "OBSERVATION // SECTOR CLEAR"
                )
    );
}

void ABHCharacter::UseFieldDressing()
{
    if (bIsHandlingDeath ||
        bIsHandlingMissionComplete ||
        bIsTraversing ||
        !IsValid(InjuryComponent))
    {
        return;
    }

    if (!HasAuthority())
    {
        ServerUseFieldDressing();
        return;
    }

    if (!InjuryComponent->UseFieldDressing())
    {
        return;
    }

    if (IsValid(WeaponComponent))
    {
        WeaponComponent->StopAllActions();
    }

    StopSprint();
    SavePlayerConditionCheckpoint(TEXT("FieldDressing"));
}

void ABHCharacter::UseMedkit()
{
    if (bIsHandlingDeath ||
        bIsHandlingMissionComplete ||
        bIsTraversing ||
        !IsValid(InjuryComponent))
    {
        return;
    }

    if (!HasAuthority())
    {
        ServerUseMedkit();
        return;
    }

    if (!InjuryComponent->StartMedkitTreatment())
    {
        return;
    }

    if (IsValid(WeaponComponent))
    {
        WeaponComponent->StopAllActions();
    }

    StopSprint();

    if (UCharacterMovementComponent* MovementComponent =
            GetCharacterMovement())
    {
        MovementComponent->StopMovementImmediately();
    }
}


void ABHCharacter::UpdateWeaponBraceState()
{
    if (!HasAuthority())
    {
        return;
    }

    bool bNewWeaponBraced = false;
    float NewSupportQuality = 0.0f;
    UWorld* World = GetWorld();
    if (IsValid(World) &&
        IsValid(FirstPersonCamera) &&
        IsValid(WeaponComponent) &&
        WeaponComponent->IsAiming())
    {
        const FVector Start = FirstPersonCamera->GetComponentLocation();
        const FVector Forward = FirstPersonCamera->GetForwardVector();
        const FVector End = Start + Forward *
            FMath::Max(1.0f, WeaponBraceMaximumSupportDistance);
        FCollisionQueryParams QueryParams(
            SCENE_QUERY_STAT(BHWeaponBraceSupport),
            false,
            this
        );
        QueryParams.AddIgnoredActor(this);

        FHitResult Hit;
        if (World->LineTraceSingleByChannel(
                Hit,
                Start,
                End,
                ECC_Visibility,
                QueryParams
            ))
        {
            const float DistanceToSupport = FVector::Distance(
                Start,
                Hit.ImpactPoint
            );
            const float SupportAlignment = FMath::Clamp(
                FVector::DotProduct(Forward, -Hit.ImpactNormal),
                0.0f,
                1.0f
            );
            const bool bSupportUsable = IsValid(Hit.GetActor()) &&
                !Hit.GetActor()->IsA<APawn>();
            bNewWeaponBraced = CanBraceWeapon(
                true,
                bIsSprinting,
                bIsTraversing,
                GetVelocity().Size2D(),
                WeaponBraceMaximumStableSpeed,
                DistanceToSupport,
                WeaponBraceMaximumSupportDistance,
                SupportAlignment,
                WeaponBraceMinimumAlignment,
                bSupportUsable,
                bSupportUsable ? 1.0f : 0.0f
            );
            if (bNewWeaponBraced)
            {
                const float DistanceQuality = 1.0f - FMath::Clamp(
                    DistanceToSupport /
                        FMath::Max(1.0f, WeaponBraceMaximumSupportDistance),
                    0.0f,
                    1.0f
                );
                NewSupportQuality = FMath::Clamp(
                    DistanceQuality * SupportAlignment,
                    0.0f,
                    1.0f
                );
            }
        }
    }

    if (bWeaponBraced != bNewWeaponBraced ||
        FMath::Abs(WeaponBraceSupportQuality - NewSupportQuality) > 0.02f)
    {
        bWeaponBraced = bNewWeaponBraced;
        WeaponBraceSupportQuality = NewSupportQuality;
        if (IsValid(AmmoHUDWidget))
        {
            AmmoHUDWidget->SetWeaponBraced(bWeaponBraced);
        }
        ForceNetUpdate();
    }
}

void ABHCharacter::OnRep_WeaponBrace()
{
    if (IsValid(AmmoHUDWidget))
    {
        AmmoHUDWidget->SetWeaponBraced(bWeaponBraced);
    }
}

void ABHCharacter::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (HasAuthority() && bTacticalFlashlightOn)
    {
        TacticalFlashlightBattery = FMath::Clamp(
            TacticalFlashlightBattery -
                FMath::Max(0.0f, TacticalFlashlightBatteryDrainPerSecond) *
                FMath::Max(0.0f, DeltaTime),
            0.0f,
            FMath::Max(1.0f, TacticalFlashlightMaxBattery)
        );

        if (TacticalFlashlightBattery <= KINDA_SMALL_NUMBER)
        {
            bTacticalFlashlightOn = false;
            UpdateTacticalFlashlightVisual();
            ForceNetUpdate();
        }
    }

    if (HasAuthority())
    {
        AuthoritativePlayerSuppression =
            DecayPlayerSuppression(
                AuthoritativePlayerSuppression,
                PlayerSuppressionDecayPerSecond,
                DeltaTime
            );
    }

    if (IsLocallyControlled())
    {
        UpdateInputPromptDevice();
        LocalPlayerSuppressionPresentation =
            DecayPlayerSuppression(
                LocalPlayerSuppressionPresentation,
                PlayerSuppressionDecayPerSecond,
                DeltaTime
            );
        if (IsValid(CombatStatusWidget))
        {
            CombatStatusWidget->SetSuppression(
                LocalPlayerSuppressionPresentation
            );
        }
        if (IsValid(ObjectiveNotificationWidget))
        {
            ObjectiveNotificationWidget->SetCombatIntensityActive(
                IsLocalCombatIntensityActive()
            );
        }
    }

    UpdateInitialWorldStreaming(DeltaTime);

    UpdateTraversal(DeltaTime);
    UpdateWeaponBraceState();

    const float PreviousStamina = CurrentStamina;

    ControlledBreathRecoveryRemaining = FMath::Max(
        0.0f,
        ControlledBreathRecoveryRemaining -
            FMath::Max(0.0f, DeltaTime)
    );

    if (bHoldingControlledBreath)
    {
        const bool bCanMaintainControlledBreathing =
            IsValid(WeaponComponent) &&
            WeaponComponent->IsAiming() &&
            !bIsSprinting &&
            !bIsTraversing &&
            !bIsHandlingDeath &&
            CurrentStamina > KINDA_SMALL_NUMBER;
        if (!bCanMaintainControlledBreathing)
        {
            bHoldingControlledBreath = false;
            ControlledBreathHeldDuration = 0.0f;
            ControlledBreathRecoveryRemaining = FMath::Max(
                0.0f,
                ControlledBreathRecoveryDuration
            );
            if (IsValid(CombatStatusWidget))
            {
                CombatStatusWidget->SetControlledBreathing(false);
            }
            ForceNetUpdate();
        }
        else
        {
            ControlledBreathHeldDuration += FMath::Max(0.0f, DeltaTime);
            CurrentStamina = FMath::Clamp(
                CurrentStamina -
                    FMath::Max(0.0f, ControlledBreathStaminaDrainPerSecond) *
                    FMath::Max(0.0f, DeltaTime),
                0.0f,
                MaxStamina
            );
            if (CurrentStamina <= KINDA_SMALL_NUMBER)
            {
                bHoldingControlledBreath = false;
                ControlledBreathHeldDuration = 0.0f;
                ControlledBreathRecoveryRemaining = FMath::Max(
                    0.0f,
                    ControlledBreathRecoveryDuration
                );
                if (IsValid(CombatStatusWidget))
                {
                    CombatStatusWidget->SetControlledBreathing(false);
                }
                ForceNetUpdate();
            }
        }
    }

    if (bIsSprinting && GetVelocity().SizeSquared2D() > 0.0f)
    {
        const float EffectiveStaminaDrainRate = 5.0f;
        CurrentStamina -= EffectiveStaminaDrainRate *
            GetCarryLoadProfile().StaminaDrainMultiplier * DeltaTime;
        CurrentStamina = FMath::Clamp(CurrentStamina, 0.0f, MaxStamina);

        if (CurrentStamina <= 0.0f)
        {
            StopSprint();
        }
    }
    else
    {
        TimeSinceSprintStopped += DeltaTime;

        if (TimeSinceSprintStopped >= StaminaRecoveryDelay)
        {
            const float EffectiveStaminaRecoveryRate = 35.0f;
            CurrentStamina += EffectiveStaminaRecoveryRate *
                GetCarryLoadProfile().StaminaRecoveryMultiplier * DeltaTime;
            CurrentStamina = FMath::Clamp(CurrentStamina, 0.0f, MaxStamina);
        }
    }

    if (!FMath::IsNearlyEqual(
        PreviousStamina,
        CurrentStamina,
        KINDA_SMALL_NUMBER
    ))
    {
        OnStaminaChanged.Broadcast(CurrentStamina, MaxStamina);

        if (IsValid(CombatStatusWidget))
        {
            CombatStatusWidget->SetStamina(
                CurrentStamina,
                MaxStamina
            );
        }
    }

    UpdateProne(DeltaTime);
    UpdateCarryLoadHUD();
    UpdateMovementAudio(DeltaTime);
    UpdateHeadBob(DeltaTime);
    UpdateLean(DeltaTime);
    SynchronizeReplicatedOperationPresentation();
    UpdateOperationWaypointHUD();
    UpdateSquadCommandWaypointHUD();
    UpdateSquadPingWaypointHUD();
    UpdateResupplyWaypointHUD(DeltaTime);
    UpdateConvoyWaypointHUD(DeltaTime);
    UpdateTransportWaypointHUD(DeltaTime);
    UpdateLogisticsWaypointHUD(DeltaTime);
    UpdateVehicleReadinessHUD();
    UpdateFieldSquadStatusHUD();
    UpdateStrategicSituationHUD(DeltaTime);

    if (HasAuthority())
    {
        UpdateFieldSquadContextAction();
    }

    if (!bIsHandlingDeath &&
        !bIsHandlingMissionComplete &&
        !bIsTraversing)
    {
        RefreshFirstPersonArmsAnimation();
        UpdateInteractionPrompt();
    }

    UpdateFirstPersonPresentationOffsets(DeltaTime);
}

void ABHCharacter::UpdateInputPromptDevice()
{
    APlayerController* PlayerController = Cast<APlayerController>(
        GetController()
    );
    UGameInstance* GameInstance = GetGameInstance();
    UBHUserSettingsSubsystem* Settings = IsValid(GameInstance)
        ? GameInstance->GetSubsystem<UBHUserSettingsSubsystem>()
        : nullptr;
    if (!IsValid(PlayerController) || !IsValid(Settings) ||
        Settings->GetInputPromptMode() != EBHInputPromptMode::Auto)
    {
        return;
    }

    const FKey GamepadAxes[] = {
        EKeys::Gamepad_LeftX,
        EKeys::Gamepad_LeftY,
        EKeys::Gamepad_RightX,
        EKeys::Gamepad_RightY,
        EKeys::Gamepad_LeftTriggerAxis,
        EKeys::Gamepad_RightTriggerAxis
    };
    bool bGamepadActive = false;
    for (const FKey& Axis : GamepadAxes)
    {
        if (FMath::Abs(PlayerController->GetInputAnalogKeyState(Axis)) > 0.35f)
        {
            bGamepadActive = true;
            break;
        }
    }

    bool bKeyboardMouseActive =
        FMath::Abs(PlayerController->GetInputAnalogKeyState(EKeys::MouseX)) >
            KINDA_SMALL_NUMBER ||
        FMath::Abs(PlayerController->GetInputAnalogKeyState(EKeys::MouseY)) >
            KINDA_SMALL_NUMBER;
    for (const FBHInputBindingDefinition& Definition :
        UBHUserSettingsSubsystem::GetDefaultInputBindingDefinitions())
    {
        const FKey KeyboardKey = Settings->GetInputBinding(
            Definition.BindingID,
            false
        );
        const FKey GamepadKey = Settings->GetInputBinding(
            Definition.BindingID,
            true
        );
        bKeyboardMouseActive = bKeyboardMouseActive ||
            (KeyboardKey.IsValid() &&
                PlayerController->IsInputKeyDown(KeyboardKey));
        bGamepadActive = bGamepadActive ||
            (GamepadKey.IsValid() &&
                PlayerController->IsInputKeyDown(GamepadKey));
    }

    if (bGamepadActive)
    {
        Settings->NotifyInputDeviceUsed(true);
    }
    else if (bKeyboardMouseActive)
    {
        Settings->NotifyInputDeviceUsed(false);
    }
}

float ABHCharacter::CalculateMovementNoiseLoudness(
    float HorizontalSpeed,
    bool bSprinting,
    bool bCrouched,
    bool bProne,
    float SurfaceLoudnessMultiplier,
    float EquipmentLoudnessMultiplier
)
{
    if (HorizontalSpeed < 10.0f)
    {
        return 0.0f;
    }

    const float SpeedAlpha = FMath::Clamp(
        HorizontalSpeed / 700.0f,
        0.0f,
        1.0f
    );
    float PostureMultiplier = 1.0f;
    if (bProne)
    {
        PostureMultiplier = 0.22f;
    }
    else if (bCrouched)
    {
        PostureMultiplier = 0.48f;
    }
    else if (bSprinting)
    {
        PostureMultiplier = 1.35f;
    }

    return FMath::Clamp(
        FMath::Lerp(0.18f, 1.0f, SpeedAlpha) *
            PostureMultiplier *
            FMath::Max(0.0f, SurfaceLoudnessMultiplier) *
            FMath::Max(0.0f, EquipmentLoudnessMultiplier),
        0.0f,
        2.0f
    );
}

void ABHCharacter::UseEngineeringTool()
{
    if (bIsHandlingDeath || bIsHandlingMissionComplete || bIsTraversing)
    {
        return;
    }
    if (!HasAuthority())
    {
        ServerUseEngineeringTool();
        return;
    }
    if (ActiveEngineeringCharges.ContainsByPredicate(
            [](const ABHEngineeringCharge* Charge)
            {
                return IsValid(Charge);
            }))
    {
        DetonateEngineeringCharges();
        return;
    }
    PlaceEngineeringCharge(
        nullptr,
        EBHEngineeringChargeMode::AreaDenial
    );
}

void ABHCharacter::LaunchAntiVehicleProjectile()
{
    if (bIsHandlingDeath || bIsHandlingMissionComplete || bIsTraversing)
    {
        return;
    }
    if (!HasAuthority())
    {
        ServerLaunchAntiVehicleProjectile();
        return;
    }
    if (!AntiVehicleProjectileClass || !IsValid(GetWorld()))
    {
        ShowStatusNotification(NSLOCTEXT(
            "BrokenHorizon", "AntiVehicleUnavailable",
            "ANTI-VEHICLE // LAUNCHER UNAVAILABLE"
        ));
        return;
    }
    if (AntiVehicleRoundCount <= 0)
    {
        ShowStatusNotification(NSLOCTEXT(
            "BrokenHorizon", "AntiVehicleNoRounds",
            "ANTI-VEHICLE // NO ROUNDS AVAILABLE"
        ));
        return;
    }

    const FVector Direction = GetControlRotation().Vector();
    const FVector SpawnLocation = GetActorLocation() +
        Direction * 120.0f + FVector(0.0f, 0.0f, 50.0f);
    FActorSpawnParameters SpawnParameters;
    SpawnParameters.Owner = this;
    SpawnParameters.Instigator = this;
    SpawnParameters.SpawnCollisionHandlingOverride =
        ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    ABHAntiVehicleProjectile* Projectile = GetWorld()->SpawnActor<ABHAntiVehicleProjectile>(
        AntiVehicleProjectileClass, SpawnLocation, Direction.Rotation(), SpawnParameters);
    if (IsValid(Projectile))
    {
        --AntiVehicleRoundCount;
        Projectile->Launch(Direction);
        ShowStatusNotification(NSLOCTEXT(
            "BrokenHorizon", "AntiVehicleLaunched",
            "ANTI-VEHICLE // PROJECTILE LAUNCHED"
        ));
        UE_LOG(LogTemp, Display, TEXT("BH_ANTI_VEHICLE_LAUNCHED player=%s"), *GetName());
    }
}

void ABHCharacter::ServerLaunchAntiVehicleProjectile_Implementation()
{
    LaunchAntiVehicleProjectile();
}

int32 ABHCharacter::GetAntiVehicleRoundCount() const
{
    return FMath::Max(0, AntiVehicleRoundCount);
}

int32 ABHCharacter::AddAntiVehicleRounds(int32 Amount)
{
    const int32 Previous = AntiVehicleRoundCount;
    AntiVehicleRoundCount = FMath::Clamp(
        AntiVehicleRoundCount + FMath::Max(0, Amount),
        0,
        FMath::Max(0, MaxAntiVehicleRounds));
    return AntiVehicleRoundCount - Previous;
}

void ABHCharacter::RestoreAntiVehicleRoundCount(int32 SavedCount)
{
    AntiVehicleRoundCount = FMath::Clamp(
        SavedCount, 0, FMath::Max(0, MaxAntiVehicleRounds));
}

void ABHCharacter::ServerUseEngineeringTool_Implementation()
{
    UseEngineeringTool();
}

void ABHCharacter::ServerUseFieldDressing_Implementation()
{
    UseFieldDressing();
}

void ABHCharacter::ServerUseMedkit_Implementation()
{
    UseMedkit();
}

bool ABHCharacter::TryPlaceBreachingCharge(AActor* TargetActor)
{
    if (!HasAuthority() || !IsValid(TargetActor) ||
        EngineeringChargeCount <= 0 ||
        FVector::DistSquared(GetActorLocation(), TargetActor->GetActorLocation()) >
            FMath::Square(InteractionDistance + 150.0f))
    {
        return false;
    }
    return PlaceEngineeringCharge(
        TargetActor,
        EBHEngineeringChargeMode::Breach
    );
}

bool ABHCharacter::PlaceEngineeringCharge(
    AActor* TargetActor,
    EBHEngineeringChargeMode Mode
)
{
    if (!HasAuthority() || EngineeringChargeCount <= 0 ||
        !EngineeringChargeClass || !IsValid(GetWorld()))
    {
        ShowStatusNotification(NSLOCTEXT(
            "BrokenHorizon", "EngineeringNoCharges",
            "ENGINEERING // NO CHARGES AVAILABLE"
        ));
        return false;
    }

    FVector PlacementLocation;
    FRotator PlacementRotation = FRotator::ZeroRotator;
    if (IsValid(TargetActor))
    {
        const FVector TowardPlayer = (
            GetActorLocation() - TargetActor->GetActorLocation()
        ).GetSafeNormal2D();
        PlacementLocation = TargetActor->GetActorLocation() +
            TowardPlayer * 35.0f + FVector(0.0f, 0.0f, 90.0f);
        PlacementRotation = (-TowardPlayer).Rotation();
    }
    else
    {
        if (!IsValid(FirstPersonCamera))
        {
            return false;
        }
        FHitResult PlacementHit;
        FCollisionQueryParams Query(
            SCENE_QUERY_STAT(BHEngineeringPlacement), true, this
        );
        const FVector Start = FirstPersonCamera->GetComponentLocation();
        const FVector End = Start +
            FirstPersonCamera->GetForwardVector() * 500.0f;
        if (!GetWorld()->LineTraceSingleByChannel(
                PlacementHit, Start, End, ECC_Visibility, Query))
        {
            ShowStatusNotification(NSLOCTEXT(
                "BrokenHorizon", "EngineeringNoSurface",
                "ENGINEERING // AIM AT A SURFACE WITHIN 5 METERS"
            ));
            return false;
        }
        PlacementLocation = PlacementHit.ImpactPoint +
            PlacementHit.ImpactNormal * 5.0f;
        PlacementRotation = PlacementHit.ImpactNormal.Rotation();
        TargetActor = Cast<APawn>(PlacementHit.GetActor())
            ? nullptr
            : PlacementHit.GetActor();
    }

    FActorSpawnParameters SpawnParameters;
    SpawnParameters.Owner = this;
    SpawnParameters.Instigator = this;
    SpawnParameters.SpawnCollisionHandlingOverride =
        ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    ABHEngineeringCharge* Charge =
        GetWorld()->SpawnActor<ABHEngineeringCharge>(
            EngineeringChargeClass,
            PlacementLocation,
            PlacementRotation,
            SpawnParameters
        );
    if (!IsValid(Charge))
    {
        return false;
    }
    Charge->InitializeCharge(this, TargetActor, Mode);
    --EngineeringChargeCount;
    ActiveEngineeringCharges.Add(Charge);
    ActiveEngineeringChargeCount = ActiveEngineeringCharges.Num();
    RefreshEngineeringHUD();
    ForceNetUpdate();
    ShowPriorityStatusNotification(
        Mode == EBHEngineeringChargeMode::Breach
            ? NSLOCTEXT(
                "BrokenHorizon", "BreachChargePlaced",
                "BREACH CHARGE PLACED // ARMING 2 SECONDS\n\n"
                "Use the engineering control to detonate. Danger close."
            )
            : NSLOCTEXT(
                "BrokenHorizon", "AreaChargePlaced",
                "AREA-DENIAL CHARGE PLACED // ARMING 2 SECONDS\n\n"
                "Use the engineering control to command detonate."
            ),
        EBHNotificationPriority::High
    );
    if (!FParse::Param(FCommandLine::Get(), TEXT("BHTestEngineeringRuntime")))
    {
        if (UBHSaveSubsystem* SaveSubsystem = GetGameInstance()
                ? GetGameInstance()->GetSubsystem<UBHSaveSubsystem>() : nullptr)
        {
            SaveSubsystem->SavePlayerResources();
        }
    }
    UE_LOG(LogTemp, Display,
        TEXT("BH_ENGINEERING_CHARGE state=placed mode=%d carried=%d active=%d"),
        static_cast<int32>(Mode), EngineeringChargeCount,
        ActiveEngineeringChargeCount);
    return true;
}

void ABHCharacter::DetonateEngineeringCharges()
{
    if (!HasAuthority())
    {
        return;
    }
    TArray<TObjectPtr<ABHEngineeringCharge>> Charges =
        ActiveEngineeringCharges;
    int32 DetonatedCount = 0;
    for (ABHEngineeringCharge* Charge : Charges)
    {
        if (IsValid(Charge) && Charge->Detonate(this))
        {
            ++DetonatedCount;
        }
    }
    if (DetonatedCount <= 0)
    {
        ShowStatusNotification(NSLOCTEXT(
            "BrokenHorizon", "EngineeringStillArming",
            "ENGINEERING // CHARGE STILL ARMING"
        ));
    }
}

void ABHCharacter::NotifyEngineeringChargeRemoved(
    ABHEngineeringCharge* Charge
)
{
    if (!HasAuthority())
    {
        return;
    }
    ActiveEngineeringCharges.Remove(Charge);
    ActiveEngineeringCharges.RemoveAll(
        [](const ABHEngineeringCharge* Candidate)
        {
            return !IsValid(Candidate);
        }
    );
    ActiveEngineeringChargeCount = ActiveEngineeringCharges.Num();
    RefreshEngineeringHUD();
    ForceNetUpdate();
}

int32 ABHCharacter::GetEngineeringChargeCount() const
{
    return EngineeringChargeCount;
}

int32 ABHCharacter::GetMaxEngineeringCharges() const
{
    return FMath::Max(0, MaxEngineeringCharges);
}

int32 ABHCharacter::AddEngineeringCharges(int32 Amount)
{
    const int32 Previous = EngineeringChargeCount;
    EngineeringChargeCount = FMath::Clamp(
        EngineeringChargeCount + FMath::Max(0, Amount),
        0,
        GetMaxEngineeringCharges()
    );
    RefreshEngineeringHUD();
    ForceNetUpdate();
    return EngineeringChargeCount - Previous;
}

void ABHCharacter::RestoreEngineeringChargeCount(int32 SavedCount)
{
    EngineeringChargeCount = FMath::Clamp(
        SavedCount, 0, GetMaxEngineeringCharges()
    );
    RefreshEngineeringHUD();
}

void ABHCharacter::OnRep_EngineeringInventory()
{
    RefreshEngineeringHUD();
    RefreshOpenInventoryPanel();
}

void ABHCharacter::RefreshEngineeringHUD()
{
    if (IsValid(CombatStatusWidget))
    {
        CombatStatusWidget->SetEngineeringChargeState(
            EngineeringChargeCount,
            ActiveEngineeringChargeCount
        );
    }
}

float ABHCharacter::CalculateFootstepInterval(
    float HorizontalSpeed,
    bool bSprinting,
    bool bCrouched,
    bool bProne
)
{
    if (HorizontalSpeed < 10.0f)
    {
        return BIG_NUMBER;
    }
    if (bProne)
    {
        return 1.05f;
    }
    if (bCrouched)
    {
        return 0.82f;
    }
    if (bSprinting)
    {
        return 0.38f;
    }

    return FMath::Lerp(
        0.78f,
        0.55f,
        FMath::Clamp(HorizontalSpeed / 400.0f, 0.0f, 1.0f)
    );
}

void ABHCharacter::UpdateMovementAudio(float DeltaTime)
{
    if (!HasAuthority() ||
        !IsValid(GetCharacterMovement()) ||
        !GetCharacterMovement()->IsMovingOnGround() ||
        bIsTraversing ||
        bIsHandlingDeath)
    {
        FootstepElapsed = 0.0f;
        return;
    }

    const float HorizontalSpeed = GetVelocity().Size2D();
    const float Interval = CalculateFootstepInterval(
        HorizontalSpeed,
        bIsSprinting,
        bIsCrouched,
        bIsProne
    );
    if (!FMath::IsFinite(Interval) || Interval >= BIG_NUMBER)
    {
        FootstepElapsed = 0.0f;
        return;
    }

    FootstepElapsed += FMath::Max(0.0f, DeltaTime);
    if (FootstepElapsed < Interval)
    {
        return;
    }

    FootstepElapsed = FMath::Fmod(FootstepElapsed, Interval);
    EmitAuthoritativeFootstep();
}

uint8 ABHCharacter::ResolveFootstepSurfaceType() const
{
    if (!IsValid(GetWorld()))
    {
        return static_cast<uint8>(SurfaceType_Default);
    }

    for (TActorIterator<ABHWaterSurface> It(GetWorld()); It; ++It)
    {
        if (IsValid(*It) && (*It)->ContainsWorldLocation(GetActorLocation()))
        {
            return static_cast<uint8>(SurfaceType5);
        }
    }

    FHitResult FloorHit;
    FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(BHFootstepSurface), false, this);
    QueryParams.bReturnPhysicalMaterial = true;
    const float TraceDistance =
        IsValid(GetCapsuleComponent())
            ? GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 35.0f
            : 130.0f;
    const FVector Start = GetActorLocation();
    const FVector End = Start - FVector(0.0f, 0.0f, TraceDistance);
    if (!GetWorld()->LineTraceSingleByChannel(
            FloorHit,
            Start,
            End,
            ECC_Visibility,
            QueryParams
        ))
    {
        return static_cast<uint8>(SurfaceType_Default);
    }

    return static_cast<uint8>(
        UPhysicalMaterial::DetermineSurfaceType(
            FloorHit.PhysMaterial.Get()
        )
    );
}

void ABHCharacter::EmitAuthoritativeFootstep()
{
    if (!HasAuthority() || !IsValid(GetWorld()))
    {
        return;
    }

    const uint8 SurfaceType = ResolveFootstepSurfaceType();
    float SurfaceMultiplier = 1.0f;
    switch (SurfaceType)
    {
        case SurfaceType1: SurfaceMultiplier = 1.10f; break;
        case SurfaceType2: SurfaceMultiplier = 0.82f; break;
        case SurfaceType3: SurfaceMultiplier = 0.68f; break;
        case SurfaceType4: SurfaceMultiplier = 1.35f; break;
        case SurfaceType5: SurfaceMultiplier = 1.20f; break;
        default: break;
    }

    const float HorizontalSpeed = GetVelocity().Size2D();
    const float Loudness = CalculateMovementNoiseLoudness(
        HorizontalSpeed,
        bIsSprinting,
        bIsCrouched,
        bIsProne,
        SurfaceMultiplier,
        EquipmentNoiseMultiplier *
            GetCarryLoadProfile().MovementNoiseMultiplier
    ) * UBHBattlefieldConditions::GetCurrentProfile(this).
        MovementNoiseMultiplier;
    if (Loudness <= KINDA_SMALL_NUMBER)
    {
        return;
    }

    UAISense_Hearing::ReportNoiseEvent(
        this,
        GetActorLocation(),
        Loudness,
        this,
        0.0f,
        TEXT("Footstep")
    );

    MulticastPlayFootstep(
        SurfaceType,
        FMath::Clamp(Loudness, 0.15f, 1.25f),
        FMath::Lerp(
            0.92f,
            1.08f,
            FMath::Clamp(HorizontalSpeed / SprintSpeed, 0.0f, 1.0f)
        )
    );
}

USoundBase* ABHCharacter::ResolveFootstepSound(uint8 SurfaceType) const
{
    USoundBase* SurfaceSound = nullptr;
    switch (SurfaceType)
    {
        case SurfaceType1: SurfaceSound = ConcreteFootstepSound; break;
        case SurfaceType2: SurfaceSound = DirtFootstepSound; break;
        case SurfaceType3: SurfaceSound = GrassFootstepSound; break;
        case SurfaceType4: SurfaceSound = MetalFootstepSound; break;
        case SurfaceType5: SurfaceSound = WaterFootstepSound; break;
        default: break;
    }
    return IsValid(SurfaceSound)
        ? SurfaceSound
        : DefaultFootstepSound.Get();
}

void ABHCharacter::MulticastPlayFootstep_Implementation(
    uint8 SurfaceType,
    float VolumeMultiplier,
    float PitchMultiplier
)
{
    if (GetNetMode() == NM_DedicatedServer)
    {
        return;
    }

    if (USoundBase* FootstepSound = ResolveFootstepSound(SurfaceType))
    {
        UGameplayStatics::PlaySoundAtLocation(
            this,
            FootstepSound,
            GetActorLocation(),
            FMath::Max(0.0f, VolumeMultiplier),
            FMath::Clamp(PitchMultiplier, 0.5f, 2.0f)
        );
    }
}

bool ABHCharacter::ShouldBindActiveOperationSnapshotPresentation() const
{
    const APlayerController* PlayerController =
        ResolveOwningPlayerController();
    return IsPlayerControlled() &&
        IsValid(PlayerController) &&
        PlayerController->GetLocalPlayer() != nullptr;
}

void ABHCharacter::TryBindActiveOperationSnapshotPresentation()
{
    if (!ShouldBindActiveOperationSnapshotPresentation())
    {
        UnbindActiveOperationSnapshotPresentation();
        return;
    }

    UWorld* World = GetWorld();
    ABHWarGameState* WarGameState = IsValid(World)
        ? World->GetGameState<ABHWarGameState>()
        : nullptr;

    if (!IsValid(WarGameState))
    {
        UnbindActiveOperationSnapshotPresentation();
        return;
    }

    if (BoundActiveOperationSnapshotGameState.Get() == WarGameState &&
        ActiveOperationSnapshotChangedHandle.IsValid())
    {
        return;
    }

    UnbindActiveOperationSnapshotPresentation();
    ActiveOperationSnapshotChangedHandle =
        WarGameState->OnActiveOperationSnapshotChanged().AddUObject(
            this,
            &ABHCharacter::HandleActiveOperationSnapshotChanged
        );
    BoundActiveOperationSnapshotGameState = WarGameState;
}

void ABHCharacter::UnbindActiveOperationSnapshotPresentation()
{
    if (ActiveOperationSnapshotChangedHandle.IsValid())
    {
        if (ABHWarGameState* WarGameState =
                BoundActiveOperationSnapshotGameState.Get())
        {
            WarGameState->OnActiveOperationSnapshotChanged().Remove(
                ActiveOperationSnapshotChangedHandle
            );
        }

        ActiveOperationSnapshotChangedHandle.Reset();
    }

    BoundActiveOperationSnapshotGameState = nullptr;
}

void ABHCharacter::HandleActiveOperationSnapshotChanged(
    const FBHActiveOperationSnapshot& Snapshot
)
{
    (void)Snapshot;

    if (!ShouldBindActiveOperationSnapshotPresentation())
    {
        UnbindActiveOperationSnapshotPresentation();
        return;
    }

    SynchronizeReplicatedOperationPresentation();
    UpdateOperationWaypointHUD();
}

void ABHCharacter::SynchronizeReplicatedOperationPresentation()
{
    if (GetWorld() == nullptr)
    {
        return;
    }

    if (!ShouldBindActiveOperationSnapshotPresentation())
    {
        UnbindActiveOperationSnapshotPresentation();
        return;
    }

    ABHWarGameState* WarGameState =
        GetWorld()->GetGameState<ABHWarGameState>();

    if (!IsValid(WarGameState))
    {
        UnbindActiveOperationSnapshotPresentation();
        return;
    }

    TryBindActiveOperationSnapshotPresentation();

    const FBHActiveOperationSnapshot Snapshot =
        WarGameState->GetActiveOperationSnapshot();
    const uint8 SnapshotPhase =
        static_cast<uint8>(Snapshot.Phase);

    if (Snapshot.Revision == LastPresentedOperationRevision &&
        Snapshot.OperationID == LastPresentedOperationID &&
        Snapshot.SectorID == LastPresentedOperationSectorID &&
        SnapshotPhase == LastPresentedOperationPhase)
    {
        return;
    }

    LastPresentedOperationRevision = Snapshot.Revision;
    LastPresentedOperationID = Snapshot.OperationID;
    LastPresentedOperationSectorID = Snapshot.SectorID;
    LastPresentedOperationPhase = SnapshotPhase;
    RefreshMissionPresentationVisibility();

    const bool bActiveOperation =
        Snapshot.Phase == EBHActiveOperationPhase::Approach ||
        Snapshot.Phase == EBHActiveOperationPhase::Combat ||
        Snapshot.Phase ==
            EBHActiveOperationPhase::AwaitingWave ||
        Snapshot.Phase == EBHActiveOperationPhase::Securing ||
        Snapshot.Phase ==
            EBHActiveOperationPhase::RaidExfiltration;

    if (!bActiveOperation)
    {
        if (Snapshot.Phase == EBHActiveOperationPhase::None &&
            bRuntimeWarOperation)
        {
            bRuntimeWarOperation = false;
            AssignedWarSectorID = NAME_None;
            AssignedWarSupplySourceSectorID = NAME_None;
            AssignedWarPriorityType =
                EBHWarPriorityType::None;

            if (IsValid(ObjectiveComponent) &&
                ObjectiveComponent->IsRuntimeMission())
            {
                ObjectiveComponent->ClearMissionState();
            }

            RefreshObjectiveWidget();
        }
        if (Snapshot.Phase == EBHActiveOperationPhase::None)
        {
            LastNotifiedOperationID = NAME_None;
            LastNotifiedOperationSectorID = NAME_None;
            LastNotifiedOperationPhase = MAX_uint8;
        }
        else if (
            Snapshot.Phase ==
                EBHActiveOperationPhase::DebriefSuccess)
        {
            const bool bShouldNotify =
                Snapshot.OperationID != LastNotifiedOperationID ||
                Snapshot.SectorID != LastNotifiedOperationSectorID ||
                SnapshotPhase != LastNotifiedOperationPhase;
            if (bShouldNotify)
            {
                LastNotifiedOperationID = Snapshot.OperationID;
                LastNotifiedOperationSectorID = Snapshot.SectorID;
                LastNotifiedOperationPhase = SnapshotPhase;
                DisplayStatusNotificationLocally(
                    NSLOCTEXT(
                        "BrokenHorizon",
                        "ReplicatedOperationDebriefSuccess",
                        "SHARED OPERATION COMPLETE\n\n"
                        "Campaign command is processing the outcome."
                    ),
                    EBHNotificationPriority::Critical
                );
            }
        }
        else if (
            Snapshot.Phase ==
                EBHActiveOperationPhase::DebriefFailure)
        {
            const bool bShouldNotify =
                Snapshot.OperationID != LastNotifiedOperationID ||
                Snapshot.SectorID != LastNotifiedOperationSectorID ||
                SnapshotPhase != LastNotifiedOperationPhase;
            if (bShouldNotify)
            {
                LastNotifiedOperationID = Snapshot.OperationID;
                LastNotifiedOperationSectorID = Snapshot.SectorID;
                LastNotifiedOperationPhase = SnapshotPhase;
                DisplayStatusNotificationLocally(
                    NSLOCTEXT(
                        "BrokenHorizon",
                        "ReplicatedOperationDebriefFailure",
                        "SHARED OPERATION FAILED\n\n"
                        "Campaign command is assessing the losses."
                    ),
                    EBHNotificationPriority::Critical
                );
            }
        }

        return;
    }

    bRuntimeWarOperation = true;
    AssignedWarSectorID = Snapshot.SectorID;
    AssignedWarSupplySourceSectorID =
        Snapshot.SupplySourceSectorID;
    AssignedWarPriorityType = Snapshot.OperationType;

    if (IsValid(ObjectiveComponent) &&
        (
            !ObjectiveComponent->IsRuntimeMission() ||
            ObjectiveComponent->GetCurrentObjectiveID() !=
                BHObjectiveIds::EliminateGuard
        ))
    {
        FBHObjectiveDefinition OperationObjective;
        OperationObjective.ObjectiveID =
            BHObjectiveIds::EliminateGuard;

        if (UGameInstance* GameInstance = GetGameInstance())
        {
            if (UBHWarSubsystem* WarSubsystem =
                    GameInstance->
                        GetSubsystem<UBHWarSubsystem>())
            {
                OperationObjective.DisplayText =
                    WarSubsystem->GetOperationObjectiveText(
                        Snapshot.SectorID,
                        Snapshot.OperationType,
                        BHObjectiveIds::EliminateGuard
                    );
            }
        }

        ObjectiveComponent->StartRuntimeMission(
            {OperationObjective}
        );
    }

    ConfigureStrategicMissionPresentation();
    RefreshObjectiveWidget();

    if (IsValid(ObjectiveWidget))
    {
        RefreshMissionPresentationVisibility();
    }

    const uint8 NotificationPhase =
        Snapshot.Phase == EBHActiveOperationPhase::Approach
            ? SnapshotPhase
            : static_cast<uint8>(EBHActiveOperationPhase::Combat);
    const bool bShouldNotify =
        Snapshot.OperationID != LastNotifiedOperationID ||
        Snapshot.SectorID != LastNotifiedOperationSectorID ||
        NotificationPhase != LastNotifiedOperationPhase;

    if (bShouldNotify)
    {
        LastNotifiedOperationID = Snapshot.OperationID;
        LastNotifiedOperationSectorID = Snapshot.SectorID;
        LastNotifiedOperationPhase = NotificationPhase;

        DisplayStatusNotificationLocally(
            FText::Format(
                Snapshot.Phase == EBHActiveOperationPhase::Approach
                    ? NSLOCTEXT(
                        "BrokenHorizon",
                        "ReplicatedOperationJoinedApproach",
                        "SHARED OPERATION ASSIGNED\n\n"
                        "Move to {0}. Follow the operation waypoint."
                    )
                    : NSLOCTEXT(
                        "BrokenHorizon",
                        "ReplicatedOperationJoinedActive",
                        "SHARED OPERATION ACTIVE\n\n"
                        "Support friendly forces in {0}."
                    ),
                FText::FromName(Snapshot.SectorID)
            ),
            EBHNotificationPriority::High
        );
    }
}

void ABHCharacter::UpdateOperationWaypointHUD()
{
    RefreshMissionPresentationVisibility();
    if (!IsValid(CombatStatusWidget))
    {
        return;
    }

    ABHEnemySoldier* NearestDownedOperative = nullptr;
    float NearestDownedDistanceSquared =
        TNumericLimits<float>::Max();
    float NearestRecoverySecondsRemaining = 0.0f;
    int32 IncapacitatedOperativeCount = 0;

    for (ABHEnemySoldier* Member : FieldSquadMembers)
    {
        if (!IsValid(Member) ||
            !Member->IsIncapacitated())
        {
            continue;
        }

        ++IncapacitatedOperativeCount;
        const float DistanceSquared =
            FVector::DistSquared2D(
                GetActorLocation(),
                Member->GetActorLocation()
            );

        if (DistanceSquared < NearestDownedDistanceSquared)
        {
            NearestDownedDistanceSquared = DistanceSquared;
            NearestDownedOperative = Member;
            NearestRecoverySecondsRemaining =
                Member->GetIncapacitationSecondsRemaining();
        }
    }

    if (IsValid(NearestDownedOperative))
    {
        const FVector ToCasualty =
            NearestDownedOperative->GetActorLocation() -
            GetActorLocation();
        CombatStatusWidget->SetCasualtyWaypoint(
            true,
            IncapacitatedOperativeCount,
            ToCasualty,
            ToCasualty.Size2D(),
            NearestRecoverySecondsRemaining
        );
    }
    else
    {
        CombatStatusWidget->SetCasualtyWaypoint(
            false,
            0,
            FVector::ZeroVector,
            0.0f,
            0.0f
        );
    }

    const ABHWarGameState* WarGameState =
        GetWorld() != nullptr
            ? GetWorld()->GetGameState<ABHWarGameState>()
            : nullptr;
    const FBHActiveOperationSnapshot OperationSnapshot =
        IsValid(WarGameState)
            ? WarGameState->GetActiveOperationSnapshot()
            : FBHActiveOperationSnapshot();
    const bool bSnapshotHasWaypoint =
        OperationSnapshot.Phase ==
            EBHActiveOperationPhase::Approach ||
        OperationSnapshot.Phase ==
            EBHActiveOperationPhase::Combat ||
        OperationSnapshot.Phase ==
            EBHActiveOperationPhase::AwaitingWave ||
        OperationSnapshot.Phase ==
            EBHActiveOperationPhase::Securing ||
        OperationSnapshot.Phase ==
            EBHActiveOperationPhase::RaidExfiltration;
    const bool bShowWaypoint = HasTacticalOperationWaypoint();

    if (!bShowWaypoint)
    {
        CombatStatusWidget->SetOperationWaypoint(
            false,
            false,
            FText::GetEmpty(),
            FText::GetEmpty(),
            FVector::ZeroVector,
            0.0f,
            false,
            0.0f,
            0.0f,
            false
        );
        CombatStatusWidget->SetOperationArrivalDeadlineRisk(false);
        return;
    }

    const FVector OperationLocation = bSnapshotHasWaypoint
        ? FVector(OperationSnapshot.OperationCenter)
        : OpenWorldOperationDirector->GetOperationCenter();
    const bool bOperationActivated = bSnapshotHasWaypoint
        ? OperationSnapshot.Phase !=
            EBHActiveOperationPhase::Approach
        : OpenWorldOperationDirector->IsOperationActivated();
    const FText SectorDisplayName = bSnapshotHasWaypoint
        ? FText::FromName(OperationSnapshot.SectorID)
        : OpenWorldOperationDirector->GetSectorDisplayName();
    FText OperationStatusText;

    if (bSnapshotHasWaypoint)
    {
        const FBHOpenWorldOperationState& ReplicatedOperationState =
            OperationSnapshot.OperationState;
        const int32 TotalWaveCount =
            OperationSnapshot.OperationType == EBHWarPriorityType::Defend
                ? FMath::Max(
                    1,
                    ReplicatedOperationState.DefenseWaveCount
                )
                : FMath::Max(
                    1,
                    1 +
                        ReplicatedOperationState
                            .AttackReinforcementWaveCount
                );
        const int32 CurrentWave = FMath::Clamp(
            ReplicatedOperationState.CurrentWave,
            1,
            TotalWaveCount
        );
        const FText OperationLabel =
            OperationSnapshot.OperationType == EBHWarPriorityType::Defend
                ? NSLOCTEXT(
                    "BrokenHorizon",
                    "ReplicatedOperationDefendLabel",
                    "DEFEND"
                )
            : OperationSnapshot.OperationType == EBHWarPriorityType::Raid
                ? NSLOCTEXT(
                    "BrokenHorizon",
                    "ReplicatedOperationRaidLabel",
                    "RAID"
                )
            : OperationSnapshot.OperationType == EBHWarPriorityType::Resupply
                ? NSLOCTEXT(
                    "BrokenHorizon",
                    "ReplicatedOperationResupplyLabel",
                    "RESUPPLY"
                )
                : NSLOCTEXT(
                    "BrokenHorizon",
                    "ReplicatedOperationAttackLabel",
                    "ATTACK"
                );

        switch (OperationSnapshot.Phase)
        {
        case EBHActiveOperationPhase::Approach:
            OperationStatusText = NSLOCTEXT(
                "BrokenHorizon",
                "ReplicatedOperationApproach",
                "Travel to the operation area"
            );
            break;
        case EBHActiveOperationPhase::AwaitingWave:
            OperationStatusText = FText::Format(
                NSLOCTEXT(
                    "BrokenHorizon",
                    "ReplicatedOperationAwaitingWaveDetailed",
                    "{0} // WAVE {1}/{2} CLEAR\n"
                    "REINFORCEMENTS {3}s // SUPPORT {4}/{5}"
                ),
                OperationLabel,
                FText::AsNumber(CurrentWave),
                FText::AsNumber(TotalWaveCount),
                FText::AsNumber(
                    FMath::Max(
                        0,
                        FMath::CeilToInt(
                            ReplicatedOperationState
                                .SecondsUntilNextWave
                        )
                    )
                ),
                FText::AsNumber(
                    FMath::Max(
                        0,
                        ReplicatedOperationState.LivingAllyCount
                    )
                ),
                FText::AsNumber(
                    FMath::Max(
                        0,
                        ReplicatedOperationState.FriendlySupportCount
                    )
                )
            );
            break;
        case EBHActiveOperationPhase::Securing:
            OperationStatusText = FText::Format(
                NSLOCTEXT(
                    "BrokenHorizon",
                    "ReplicatedOperationSecuringDetailed",
                    "{0} // SECURE AND HOLD\nSUPPORT {1}/{2}"
                ),
                OperationLabel,
                FText::AsNumber(
                    FMath::Max(
                        0,
                        ReplicatedOperationState.LivingAllyCount
                    )
                ),
                FText::AsNumber(
                    FMath::Max(
                        0,
                        ReplicatedOperationState.FriendlySupportCount
                    )
                )
            );
            break;
        case EBHActiveOperationPhase::RaidExfiltration:
            OperationStatusText = FText::Format(
                NSLOCTEXT(
                    "BrokenHorizon",
                    "ReplicatedOperationRaidExfiltrationDetailed",
                    "{0} // BREAK CONTACT\nSUPPORT {1}/{2}"
                ),
                OperationLabel,
                FText::AsNumber(
                    FMath::Max(
                        0,
                        ReplicatedOperationState.LivingAllyCount
                    )
                ),
                FText::AsNumber(
                    FMath::Max(
                        0,
                        ReplicatedOperationState.FriendlySupportCount
                    )
                )
            );
            break;
        default:
            OperationStatusText = FText::Format(
                NSLOCTEXT(
                    "BrokenHorizon",
                    "ReplicatedOperationCombatDetailed",
                    "{0} // WAVE {1}/{2}\n"
                    "HOSTILES {3} // LOSSES {4} // SUPPORT {5}/{6}"
                ),
                OperationLabel,
                FText::AsNumber(CurrentWave),
                FText::AsNumber(TotalWaveCount),
                FText::AsNumber(
                    FMath::Max(
                        0,
                        ReplicatedOperationState.LivingEnemyCount
                    )
                ),
                FText::AsNumber(
                    FMath::Max(
                        0,
                        ReplicatedOperationState.EnemyCasualties
                    )
                ),
                FText::AsNumber(
                    FMath::Max(
                        0,
                        ReplicatedOperationState.LivingAllyCount
                    )
                ),
                FText::AsNumber(
                    FMath::Max(
                        0,
                        ReplicatedOperationState.FriendlySupportCount
                    )
                )
            );
            break;
        }
    }
    else
    {
        OperationStatusText =
            OpenWorldOperationDirector->GetOperationStatusText();
    }

    const FVector ToOperation =
        OperationLocation - GetActorLocation();
    ABHFieldTransport* TravelTransport =
        Cast<ABHFieldTransport>(GetAttachParentActor());
    const float OperationDistanceCentimeters =
        ToOperation.Size2D();
    const bool bShowTravelEstimate =
        IsValid(TravelTransport) &&
        !bOperationActivated;
    const float EstimatedTravelMinutes =
        bShowTravelEstimate
            ? TravelTransport->GetEstimatedTravelMinutes(
                OperationDistanceCentimeters
            )
            : 0.0f;
    const float EstimatedRangeKilometers =
        bShowTravelEstimate
            ? TravelTransport->GetEstimatedRangeKilometers()
            : 0.0f;
    constexpr float ArrivalFuelReserveKilometers = 5.0f;
    const bool bFuelShortfall =
        bShowTravelEstimate &&
        EstimatedRangeKilometers <
            (
                OperationDistanceCentimeters / 100000.0f +
                ArrivalFuelReserveKilometers
            );
    constexpr float ArrivalPreparationReserveSeconds = 30.0f;
    const float ApproachSecondsRemaining =
        bSnapshotHasWaypoint &&
        IsValid(WarGameState)
            ? FMath::Max(
                0.0f,
                OperationSnapshot.
                    PhaseEndServerWorldTimeSeconds -
                    WarGameState->
                        GetServerWorldTimeSeconds()
            )
            : OpenWorldOperationDirector->
                GetApproachSecondsRemaining();
    const bool bArrivalDeadlineRisk =
        bShowTravelEstimate &&
        ApproachSecondsRemaining > 0.0f &&
        (
            EstimatedTravelMinutes * 60.0f +
            ArrivalPreparationReserveSeconds
        ) > ApproachSecondsRemaining;

    CombatStatusWidget->SetOperationWaypoint(
        true,
        bOperationActivated,
        SectorDisplayName,
        OperationStatusText,
        ToOperation,
        OperationDistanceCentimeters,
        bShowTravelEstimate,
        EstimatedTravelMinutes,
        EstimatedRangeKilometers,
        bFuelShortfall
    );
    CombatStatusWidget->SetOperationArrivalDeadlineRisk(
        bArrivalDeadlineRisk
    );
}

void ABHCharacter::UpdateSquadCommandWaypointHUD()
{
    if (!IsValid(CombatStatusWidget))
    {
        return;
    }

    const bool bHasFieldSquadCommand =
        bFieldSquadHolding &&
        bFieldSquadHasCommandLocation &&
        GetLivingFieldSquadCount() > 0;
    const bool bHasOperationSupportCommand =
        IsValid(OpenWorldOperationDirector) &&
        OpenWorldOperationDirector->
            HasFriendlySupportCommandLocation();

    if (!bHasFieldSquadCommand &&
        !bHasOperationSupportCommand)
    {
        CombatStatusWidget->SetSquadCommandWaypoint(
            false,
            FVector::ZeroVector,
            0.0f
        );
        return;
    }

    FVector CommandLocation = FVector::ZeroVector;

    if (bHasFieldSquadCommand &&
        bHasOperationSupportCommand)
    {
        CommandLocation =
            (
                FieldSquadCommandLocation +
                OpenWorldOperationDirector->
                    GetFriendlySupportCommandLocation()
            ) * 0.5f;
    }
    else if (bHasFieldSquadCommand)
    {
        CommandLocation = FieldSquadCommandLocation;
    }
    else
    {
        CommandLocation =
            OpenWorldOperationDirector->
                GetFriendlySupportCommandLocation();
    }

    const FVector ToCommand =
        CommandLocation - GetActorLocation();
    CombatStatusWidget->SetSquadCommandWaypoint(
        true,
        ToCommand,
        ToCommand.Size2D()
    );
}

void ABHCharacter::UpdateSquadPingWaypointHUD()
{
    EnsureCombatStatusWidget();

    if (!IsValid(CombatStatusWidget))
    {
        return;
    }

    const UWorld* World = GetWorld();
    const ABHWarGameState* WarGameState = IsValid(World)
        ? World->GetGameState<ABHWarGameState>()
        : nullptr;

    if (!IsValid(WarGameState))
    {
        CombatStatusWidget->SetSquadPingWaypoint(
            false,
            FVector::ZeroVector,
            0.0f,
            FString(),
            FString()
        );
        return;
    }

    const FBHSquadPingSnapshot Ping =
        WarGameState->GetSquadPingSnapshot();

    if (!Ping.IsActiveAt(
            WarGameState->GetServerWorldTimeSeconds()))
    {
        CombatStatusWidget->SetSquadPingWaypoint(
            false,
            FVector::ZeroVector,
            0.0f,
            FString(),
            FString()
        );
        return;
    }

    FVector PresentedPingLocation = FVector(Ping.Location);
    bool bTrackedTargetVisible = false;
    const bool bTrackedTarget = IsValid(Ping.TrackedActor);

    if (Ping.Revision != ObservedSquadPingRevision)
    {
        ObservedSquadPingRevision = Ping.Revision;
        LastVisibleSquadPingLocation = PresentedPingLocation;
    }

    if (bTrackedTarget)
    {
        FCollisionQueryParams QueryParams(
            SCENE_QUERY_STAT(BHSquadPingVisibility),
            false,
            this
        );
        const FVector TargetLocation =
            Ping.TrackedActor->GetActorLocation();
        FHitResult VisibilityHit;
        const bool bBlockingHit = World->LineTraceSingleByChannel(
            VisibilityHit,
            GetPawnViewLocation(),
            TargetLocation,
            ECC_Visibility,
            QueryParams
        );
        bTrackedTargetVisible =
            UBHCombatStatusWidget::IsSquadPingTargetVisible(
                bBlockingHit,
                bBlockingHit ? VisibilityHit.GetActor() : nullptr,
                Ping.TrackedActor
            );
        if (bTrackedTargetVisible)
        {
            LastVisibleSquadPingLocation = TargetLocation;
        }
        PresentedPingLocation = LastVisibleSquadPingLocation;

        if (LoggedSquadPingPresentationRevision != Ping.Revision ||
            bLoggedSquadPingTargetVisible != bTrackedTargetVisible)
        {
            LoggedSquadPingPresentationRevision = Ping.Revision;
            bLoggedSquadPingTargetVisible = bTrackedTargetVisible;
            UE_LOG(
                LogTemp,
                Display,
                TEXT(
                    "BH_SQUAD_PING_PRESENTATION revision=%d tracked=1 "
                    "visible=%d mode=%s location=%s"
                ),
                Ping.Revision,
                bTrackedTargetVisible ? 1 : 0,
                bTrackedTargetVisible
                    ? TEXT("TRACKED")
                    : TEXT("LAST_KNOWN"),
                *PresentedPingLocation.ToCompactString()
            );
        }
    }

    const FVector PresentedToPing =
        PresentedPingLocation - GetActorLocation();
    CombatStatusWidget->SetSquadPingWaypoint(
        true,
        PresentedToPing,
        PresentedToPing.Size2D(),
        Ping.ContextLabel.ToString(),
        Ping.IssuerLabel.ToString(),
        bTrackedTarget,
        bTrackedTargetVisible
    );
}

void ABHCharacter::UpdateResupplyWaypointHUD(float DeltaTime)
{
    if (!IsValid(CombatStatusWidget))
    {
        return;
    }

    ResupplyWaypointRefreshRemaining -=
        FMath::Max(0.0f, DeltaTime);

    if (ResupplyWaypointRefreshRemaining > 0.0f)
    {
        return;
    }

    ResupplyWaypointRefreshRemaining = 0.5f;

    const auto ClearWaypoint = [this]()
    {
        CombatStatusWidget->SetResupplyWaypoint(
            false,
            FText::GetEmpty(),
            FVector::ZeroVector,
            0.0f
        );
    };

    if (bRuntimeWarOperation &&
        AssignedWarPriorityType == EBHWarPriorityType::Resupply)
    {
        UWorld* OperationWorld = GetWorld();
        UGameInstance* OperationGameInstance =
            IsValid(OperationWorld)
                ? OperationWorld->GetGameInstance()
                : nullptr;
        UBHWarSubsystem* OperationWar =
            IsValid(OperationGameInstance)
                ? OperationGameInstance->GetSubsystem<
                    UBHWarSubsystem>()
                : nullptr;
        const ABHFieldTransport* DrivenTransport =
            Cast<ABHFieldTransport>(GetAttachParentActor());
        const bool bCarryingMissionCargo =
            IsValid(DrivenTransport) &&
            DrivenTransport->GetCargoSupply() >
                KINDA_SMALL_NUMBER;
        const FName WaypointSectorID =
            bCarryingMissionCargo
                ? AssignedWarSectorID
                : AssignedWarSupplySourceSectorID;

        if (IsValid(OperationWorld) &&
            IsValid(OperationWar) &&
            !WaypointSectorID.IsNone())
        {
            for (TActorIterator<ABHSectorResupplyStation> It(
                    OperationWorld);
                It;
                ++It)
            {
                ABHSectorResupplyStation* Station = *It;

                if (!IsValid(Station) ||
                    Station->GetSectorID() != WaypointSectorID)
                {
                    continue;
                }

                const FBHWarSectorState Sector =
                    OperationWar->GetSectorState(
                        WaypointSectorID
                    );
                const FVector ToStation =
                    Station->GetActorLocation() -
                    GetActorLocation();
                CombatStatusWidget->SetResupplyWaypoint(
                    true,
                    FText::Format(
                        bCarryingMissionCargo
                            ? NSLOCTEXT(
                                "BrokenHorizon",
                                "ResupplyOperationDeliveryWaypoint",
                                "DELIVER // {0}"
                            )
                            : NSLOCTEXT(
                                "BrokenHorizon",
                                "ResupplyOperationPickupWaypoint",
                                "LOAD CARGO // {0}"
                            ),
                        Sector.DisplayName.IsEmpty()
                            ? FText::FromName(WaypointSectorID)
                            : Sector.DisplayName
                    ),
                    ToStation,
                    ToStation.Size2D()
                );
                return;
            }
        }

        ClearWaypoint();
        return;
    }

    const bool bNeedsAmmo =
        IsValid(WeaponComponent) &&
        WeaponComponent->GetReserveAmmo() <=
            FMath::Max(
                30,
                WeaponComponent->GetMaxReserveAmmo() / 3
            );
    const bool bNeedsMedical =
        IsValid(InjuryComponent) &&
        (InjuryComponent->GetMedkitCount() <= 0 ||
         InjuryComponent->GetFieldDressingCount() <= 0);
    const bool bNeedsArmor =
        IsValid(InjuryComponent) &&
        (InjuryComponent->GetHelmetDurabilityPercentage() <= 0.35f ||
         InjuryComponent->GetBodyArmorDurabilityPercentage() <= 0.35f);
    const bool bNeedsGrenades =
        GetFragGrenadeCount() < GetMaxFragGrenades();
    const bool bNeedsEngineering =
        GetEngineeringChargeCount() < GetMaxEngineeringCharges();
    const int32 FieldSquadMembersNeedingService =
        GetFieldSquadMembersNeedingServiceCount();
    const bool bNeedsFieldSquadService =
        FieldSquadMembersNeedingService > 0;
    ABHFieldTransport* RelevantTransport =
        Cast<ABHFieldTransport>(GetAttachParentActor());

    if (!IsValid(RelevantTransport))
    {
        constexpr float NearbyTransportAwarenessRadius = 2000.0f;
        ABHFieldTransport* NearbyServiceTransport = nullptr;
        ABHFieldTransport* RemoteRecoveryTransport = nullptr;
        float NearestServiceDistanceSquared =
            FMath::Square(NearbyTransportAwarenessRadius);
        float NearestRecoveryDistanceSquared =
            TNumericLimits<float>::Max();
        UWorld* SearchWorld = GetWorld();

        if (IsValid(SearchWorld))
        {
            for (TActorIterator<ABHFieldTransport> It(SearchWorld);
                It;
                ++It)
            {
                ABHFieldTransport* Candidate = *It;

                if (!IsValid(Candidate) ||
                    IsValid(Candidate->GetOccupant()))
                {
                    continue;
                }

                const float DistanceSquared =
                    FVector::DistSquared2D(
                        GetActorLocation(),
                        Candidate->GetActorLocation()
                    );

                if (Candidate->IsImmobilized() &&
                    DistanceSquared <
                        NearestRecoveryDistanceSquared)
                {
                    RemoteRecoveryTransport = Candidate;
                    NearestRecoveryDistanceSquared =
                        DistanceSquared;
                }

                if (Candidate->NeedsService() &&
                    DistanceSquared <=
                        NearestServiceDistanceSquared)
                {
                    NearbyServiceTransport = Candidate;
                    NearestServiceDistanceSquared =
                        DistanceSquared;
                }
            }
        }

        RelevantTransport =
            IsValid(NearbyServiceTransport)
                ? NearbyServiceTransport
                : RemoteRecoveryTransport;
    }

    const bool bNeedsVehicleService =
        IsValid(RelevantTransport) &&
        (
            RelevantTransport->GetFuelPercentage() <= 0.25f ||
            RelevantTransport->GetHullPercentage() <= 0.35f
        );

    if (IsValid(RelevantTransport) &&
        RelevantTransport->GetCargoSupply() >
            KINDA_SMALL_NUMBER &&
        !bNeedsFieldSquadService)
    {
        ClearWaypoint();
        return;
    }

    if (!bNeedsAmmo &&
        !bNeedsMedical &&
        !bNeedsArmor &&
        !bNeedsGrenades &&
        !bNeedsEngineering &&
        !bNeedsFieldSquadService &&
        !bNeedsVehicleService)
    {
        ClearWaypoint();
        return;
    }

    UWorld* World = GetWorld();
    UGameInstance* GameInstance = IsValid(World)
        ? World->GetGameInstance()
        : nullptr;
    UBHWarSubsystem* WarSubsystem = IsValid(GameInstance)
        ? GameInstance->GetSubsystem<UBHWarSubsystem>()
        : nullptr;

    if (!IsValid(World) || !IsValid(WarSubsystem))
    {
        ClearWaypoint();
        return;
    }

    ABHSectorResupplyStation* NearestStation = nullptr;
    FBHWarSectorState NearestSector;
    float NearestDistanceSquared = TNumericLimits<float>::Max();

    for (TActorIterator<ABHSectorResupplyStation> It(World);
        It;
        ++It)
    {
        ABHSectorResupplyStation* Station = *It;

        if (!IsValid(Station))
        {
            continue;
        }

        const FBHWarSectorState Sector =
            WarSubsystem->GetSectorState(
                Station->GetSectorID()
            );

        if (Sector.SectorID.IsNone() ||
            Sector.Owner != EBHWarFaction::Friendly ||
            Sector.Supply + KINDA_SMALL_NUMBER <
                Station->GetResupplySupplyCost(
                    FieldSquadMembersNeedingService
                ))
        {
            continue;
        }

        const float DistanceSquared = FVector::DistSquared2D(
            GetActorLocation(),
            Station->GetActorLocation()
        );

        if (DistanceSquared < NearestDistanceSquared)
        {
            NearestDistanceSquared = DistanceSquared;
            NearestStation = Station;
            NearestSector = Sector;
        }
    }

    if (!IsValid(NearestStation))
    {
        ClearWaypoint();
        return;
    }

    const FVector ToStation =
        NearestStation->GetActorLocation() - GetActorLocation();
    const FText StationDisplayName =
        NearestSector.DisplayName.IsEmpty()
            ? FText::FromName(NearestSector.SectorID)
            : NearestSector.DisplayName;
    CombatStatusWidget->SetResupplyWaypoint(
        true,
        FText::FromString(
            UBHCombatStatusWidget::BuildResupplyWaypointLabel(
                StationDisplayName.ToString(),
                FieldSquadMembersNeedingService
            )
        ),
        ToStation,
        ToStation.Size2D()
    );
}

void ABHCharacter::UpdateConvoyWaypointHUD(float DeltaTime)
{
    if (!IsValid(CombatStatusWidget))
    {
        return;
    }

    ConvoyWaypointRefreshRemaining -=
        FMath::Max(0.0f, DeltaTime);

    if (ConvoyWaypointRefreshRemaining > 0.0f)
    {
        return;
    }

    ConvoyWaypointRefreshRemaining = 0.25f;

    UWorld* World = GetWorld();
    const UGameInstance* GameInstance = GetGameInstance();
    const UBHWarSubsystem* WarSubsystem =
        IsValid(GameInstance)
            ? GameInstance->GetSubsystem<UBHWarSubsystem>()
            : nullptr;
    const FName AssignedEscortConvoyID =
        bRuntimeWarOperation &&
            AssignedWarPriorityType ==
                EBHWarPriorityType::EscortRescue &&
            IsValid(WarSubsystem)
            ? WarSubsystem->GetCommittedOperationTargetID()
            : NAME_None;
    ABHSupplyConvoyTarget* NearestTarget = nullptr;
    float NearestDistanceSquared = TNumericLimits<float>::Max();

    if (IsValid(World))
    {
        for (TActorIterator<ABHSupplyConvoyTarget> It(World);
            It;
            ++It)
        {
            ABHSupplyConvoyTarget* Target = *It;

            if (!IsValid(Target) || Target->GetConvoyID().IsNone() ||
                (!AssignedEscortConvoyID.IsNone() &&
                 Target->GetConvoyID() != AssignedEscortConvoyID))
            {
                continue;
            }

            const float DistanceSquared = FVector::DistSquared2D(
                GetActorLocation(),
                Target->GetActorLocation()
            );

            if (DistanceSquared < NearestDistanceSquared)
            {
                NearestDistanceSquared = DistanceSquared;
                NearestTarget = Target;
            }
        }
    }

    if (!IsValid(NearestTarget))
    {
        CombatStatusWidget->SetConvoyWaypoint(
            false,
            EBHWarFaction::Neutral,
            FVector::ZeroVector,
            0.0f,
            0.0f,
            0.0f
        );
        return;
    }

    const FVector ToConvoy =
        NearestTarget->GetActorLocation() - GetActorLocation();
    CombatStatusWidget->SetConvoyWaypoint(
        true,
        NearestTarget->GetConvoyOwner(),
        ToConvoy,
        ToConvoy.Size2D(),
        NearestTarget->GetSupplyPayload(),
        NearestTarget->GetHealthPercentage()
    );
    const FBHRouteOperationProfile RouteProfile =
        NearestTarget->GetRouteOperationProfile();
    CombatStatusWidget->SetConvoyOperationProfile(
        RouteProfile.Variation,
        NearestTarget->GetOperationDeadlineRemaining()
    );
}

void ABHCharacter::IssueSquadPing()
{
    if (bIsHandlingDeath || bPauseMenuOpen || bWarMapOpen)
    {
        return;
    }

    if (HasAuthority())
    {
        ExecuteSquadPing();
    }
    else
    {
        ServerRequestSquadPing();
    }
}

void ABHCharacter::ServerRequestSquadPing_Implementation()
{
    ExecuteSquadPing();
}

void ABHCharacter::ExecuteSquadPing()
{
    UWorld* World = GetWorld();

    if (!HasAuthority() || !IsValid(World) || bIsHandlingDeath ||
        (IsValid(HealthComponent) && HealthComponent->IsDead()))
    {
        return;
    }

    const float CurrentTime = World->GetTimeSeconds();

    if (CurrentTime < NextSquadPingTime)
    {
        return;
    }

    FVector ViewLocation;
    FRotator ViewRotation;
    GetActorEyesViewPoint(ViewLocation, ViewRotation);
    const FVector TraceEnd = ViewLocation +
        (ViewRotation.Vector() *
         FMath::Max(1000.0f, SquadPingMaximumDistance));
    FCollisionQueryParams QueryParams(
        SCENE_QUERY_STAT(BHSquadPingTrace),
        true,
        this
    );
    QueryParams.AddIgnoredActor(this);

    FHitResult HitResult;
    const bool bHit = World->LineTraceSingleByChannel(
        HitResult,
        ViewLocation,
        TraceEnd,
        ECC_Visibility,
        QueryParams
    );
    const FVector PingLocation = bHit
        ? HitResult.ImpactPoint
        : TraceEnd;
    const FName ContextLabel = ResolveSquadPingContextLabel(
        bHit ? HitResult.GetActor() : nullptr
    );
    ABHWarGameState* WarGameState =
        World->GetGameState<ABHWarGameState>();

    if (!IsValid(WarGameState))
    {
        return;
    }

    FName IssuerLabel(TEXT("SQUAD"));

    if (const APlayerState* State = GetPlayerState())
    {
        FString PlayerName = State->GetPlayerName();
        PlayerName.LeftInline(18, EAllowShrinking::No);

        if (!PlayerName.IsEmpty())
        {
            IssuerLabel = FName(*PlayerName);
        }
    }

    WarGameState->PublishSquadPing(
        PingLocation,
        ContextLabel,
        IssuerLabel,
        SquadPingLifetime,
        bHit ? HitResult.GetActor() : nullptr
    );
    NextSquadPingTime = CurrentTime +
        FMath::Max(0.1f, SquadPingCooldown);
}

FName ABHCharacter::ResolveSquadPingContextLabel(
    AActor* HitActor
) const
{
    const ABHEnemySoldier* Soldier =
        Cast<ABHEnemySoldier>(HitActor);

    if (IsValid(Soldier))
    {
        if (Soldier->IsDead())
        {
            return FName(TEXT("CASUALTY"));
        }

        return Soldier->GetCombatFaction() ==
            EBHCombatFaction::Hostile
            ? FName(TEXT("HOSTILE"))
            : FName(TEXT("ALLY"));
    }

    if (IsValid(Cast<ABHFieldTransport>(HitActor)))
    {
        return FName(TEXT("TRANSPORT"));
    }

    if (IsValid(Cast<ABHSectorResupplyStation>(HitActor)))
    {
        return FName(TEXT("SUPPLY"));
    }

    if (IsValid(Cast<ABHSectorAnchor>(HitActor)))
    {
        return FName(TEXT("OBJECTIVE"));
    }

    return FName(TEXT("LOCATION"));
}

void ABHCharacter::UpdateTransportWaypointHUD(float DeltaTime)
{
    if (!IsValid(CombatStatusWidget))
    {
        return;
    }

    TransportWaypointRefreshRemaining -=
        FMath::Max(0.0f, DeltaTime);

    if (TransportWaypointRefreshRemaining > 0.0f)
    {
        return;
    }

    TransportWaypointRefreshRemaining = 0.25f;

    if (Cast<ABHFieldTransport>(GetAttachParentActor()))
    {
        CombatStatusWidget->SetTransportWaypoint(
            false,
            FVector::ZeroVector,
            0.0f,
            1.0f,
            1.0f,
            false
        );
        return;
    }

    UWorld* World = GetWorld();
    ABHFieldTransport* NearestTransport = nullptr;
    float NearestDistanceSquared =
        TNumericLimits<float>::Max();

    if (IsValid(World))
    {
        for (TActorIterator<ABHFieldTransport> It(World);
            It;
            ++It)
        {
            ABHFieldTransport* Candidate = *It;

            if (!IsValid(Candidate) ||
                Candidate->IsImmobilized() ||
                (IsValid(Candidate->GetOccupant()) &&
                 Candidate->GetOccupant() != this))
            {
                continue;
            }

            const float DistanceSquared =
                FVector::DistSquared2D(
                    GetActorLocation(),
                    Candidate->GetActorLocation()
                );

            if (DistanceSquared < NearestDistanceSquared)
            {
                NearestTransport = Candidate;
                NearestDistanceSquared = DistanceSquared;
            }
        }
    }

    constexpr float NearbyTransportMarkerSuppressionRadius =
        1500.0f;

    if (!IsValid(NearestTransport) ||
        NearestDistanceSquared <=
            FMath::Square(
                NearbyTransportMarkerSuppressionRadius
            ))
    {
        CombatStatusWidget->SetTransportWaypoint(
            false,
            FVector::ZeroVector,
            0.0f,
            1.0f,
            1.0f,
            false
        );
        return;
    }

    const FVector ToTransport =
        NearestTransport->GetActorLocation() -
        GetActorLocation();
    CombatStatusWidget->SetTransportWaypoint(
        true,
        ToTransport,
        ToTransport.Size2D(),
        NearestTransport->GetFuelPercentage(),
        NearestTransport->GetHullPercentage(),
        false
    );
}

void ABHCharacter::UpdateLogisticsWaypointHUD(float DeltaTime)
{
    if (!IsValid(CombatStatusWidget))
    {
        return;
    }

    LogisticsWaypointRefreshRemaining -=
        FMath::Max(0.0f, DeltaTime);

    if (LogisticsWaypointRefreshRemaining > 0.0f)
    {
        return;
    }

    LogisticsWaypointRefreshRemaining = 0.5f;

    const auto ClearWaypoints = [this]()
    {
        CombatStatusWidget->SetLogisticsWaypoint(
            false,
            FText::GetEmpty(),
            FVector::ZeroVector,
            0.0f,
            0.0f
        );
        CombatStatusWidget->SetSalvageWaypoint(
            false,
            FVector::ZeroVector,
            0.0f,
            0.0f
        );
    };

    ABHFieldTransport* TravelTransport =
        Cast<ABHFieldTransport>(GetAttachParentActor());
    UWorld* World = GetWorld();

    if (!IsValid(TravelTransport) ||
        TravelTransport->GetCargoSupply() <=
            KINDA_SMALL_NUMBER)
    {
        if (!IsValid(World))
        {
            ClearWaypoints();
            return;
        }

        ABHSupplyConvoyTarget* NearestSalvage = nullptr;
        float NearestDistanceSquared = BIG_NUMBER;

        for (TActorIterator<ABHSupplyConvoyTarget> It(World);
            It;
            ++It)
        {
            ABHSupplyConvoyTarget* Candidate = *It;

            if (!IsValid(Candidate) ||
                !Candidate->HasRecoverableSalvage())
            {
                continue;
            }

            const float DistanceSquared =
                FVector::DistSquared2D(
                    GetActorLocation(),
                    Candidate->GetActorLocation()
                );

            if (DistanceSquared < NearestDistanceSquared)
            {
                NearestSalvage = Candidate;
                NearestDistanceSquared = DistanceSquared;
            }
        }

        CombatStatusWidget->SetLogisticsWaypoint(
            false,
            FText::GetEmpty(),
            FVector::ZeroVector,
            0.0f,
            0.0f
        );

        if (!IsValid(NearestSalvage))
        {
            CombatStatusWidget->SetSalvageWaypoint(
                false,
                FVector::ZeroVector,
                0.0f,
                0.0f
            );
            return;
        }

        const FVector ToSalvage =
            NearestSalvage->GetActorLocation() -
            GetActorLocation();
        CombatStatusWidget->SetSalvageWaypoint(
            true,
            ToSalvage,
            ToSalvage.Size2D(),
            NearestSalvage->GetRecoverableSupply()
        );
        return;
    }

    CombatStatusWidget->SetSalvageWaypoint(
        false,
        FVector::ZeroVector,
        0.0f,
        0.0f
    );
    UGameInstance* GameInstance = IsValid(World)
        ? World->GetGameInstance()
        : nullptr;
    UBHWarSubsystem* WarSubsystem = IsValid(GameInstance)
        ? GameInstance->GetSubsystem<UBHWarSubsystem>()
        : nullptr;

    if (!IsValid(World) || !IsValid(WarSubsystem))
    {
        ClearWaypoints();
        return;
    }

    const bool bCivilianAid =
        TravelTransport->GetCargoType() ==
            EBHWarConvoyCargoType::CivilianAid;
    const FName DestinationSectorID =
        bCivilianAid
            ? TravelTransport->RefreshCivilianAidDestination()
            : TravelTransport->
                RefreshMilitaryCargoDestination();

    if (DestinationSectorID.IsNone())
    {
        ClearWaypoints();
        return;
    }

    ABHSectorResupplyStation* DestinationStation = nullptr;

    for (TActorIterator<ABHSectorResupplyStation> It(World);
        It;
        ++It)
    {
        ABHSectorResupplyStation* Candidate = *It;

        if (IsValid(Candidate) &&
            Candidate->GetSectorID() == DestinationSectorID)
        {
            DestinationStation = Candidate;
            break;
        }
    }

    if (!IsValid(DestinationStation))
    {
        ClearWaypoints();
        return;
    }

    const FBHWarSectorState DestinationSector =
        WarSubsystem->GetSectorState(DestinationSectorID);
    const FVector ToDestination =
        DestinationStation->GetActorLocation() -
        GetActorLocation();
    CombatStatusWidget->SetLogisticsWaypoint(
        true,
        bCivilianAid
            ? FText::Format(
                NSLOCTEXT(
                    "BrokenHorizon",
                    "CivilianAidWaypointSector",
                    "AID // {0}"
                ),
                DestinationSector.DisplayName.IsEmpty()
                    ? FText::FromName(DestinationSectorID)
                    : DestinationSector.DisplayName
            )
            : DestinationSector.DisplayName.IsEmpty()
                ? FText::FromName(DestinationSectorID)
                : DestinationSector.DisplayName,
        ToDestination,
        ToDestination.Size2D(),
        TravelTransport->GetCargoSupply()
    );
}

void ABHCharacter::UpdateVehicleReadinessHUD()
{
    if (!IsValid(CombatStatusWidget))
    {
        return;
    }

    const ABHFieldTransport* Transport =
        Cast<ABHFieldTransport>(GetAttachParentActor());
    const bool bDriving =
        IsValid(Transport) && Transport->GetOccupant() == this;

    float CargoSupply = 0.0f;
    EBHWarConvoyCargoType CargoType =
        EBHWarConvoyCargoType::MilitarySupply;
    FText DestinationName = FText::GetEmpty();

    if (bDriving)
    {
        CargoSupply = Transport->GetCargoSupply();
        CargoType = Transport->GetCargoType();

        if (CargoSupply > KINDA_SMALL_NUMBER)
        {
            const FName DestinationSectorID =
                Transport->GetCargoDestinationSectorID();
            UWorld* World = GetWorld();
            UGameInstance* GameInstance = IsValid(World)
                ? World->GetGameInstance()
                : nullptr;
            UBHWarSubsystem* WarSubsystem = IsValid(GameInstance)
                ? GameInstance->GetSubsystem<UBHWarSubsystem>()
                : nullptr;

            if (IsValid(WarSubsystem) &&
                !DestinationSectorID.IsNone())
            {
                DestinationName = WarSubsystem->GetSectorState(
                    DestinationSectorID
                ).DisplayName;
            }

            if (DestinationName.IsEmpty() &&
                !DestinationSectorID.IsNone())
            {
                DestinationName = FText::FromName(
                    DestinationSectorID
                );
            }
        }
    }

    CombatStatusWidget->SetVehicleReadiness(
        bDriving,
        bDriving ? Transport->GetFuelPercentage() : 1.0f,
        bDriving ? Transport->GetHullPercentage() : 1.0f,
        bDriving ? Transport->GetSpeedKPH() : 0.0f,
        bDriving && Transport->IsImmobilized()
    );
    CombatStatusWidget->SetVehicleLogisticsStatus(
        bDriving,
        CargoSupply,
        CargoType,
        DestinationName
    );
}

void ABHCharacter::UpdateFieldSquadStatusHUD()
{
    if (!IsValid(CombatStatusWidget))
    {
        return;
    }

    const int32 LivingOperatives = GetLivingFieldSquadCount();
    const int32 MembersNeedingService =
        GetFieldSquadMembersNeedingServiceCount();
    float TotalReadiness = 0.0f;
    float LowestReadiness = 1.0f;
    int32 ReadinessSampleCount = 0;
    for (const ABHEnemySoldier* Member : FieldSquadMembers)
    {
        if (!IsValid(Member) || Member->IsDead() ||
            Member->IsIncapacitated())
        {
            continue;
        }

        const float MemberReadiness = Member->GetCombatReadiness();
        TotalReadiness += MemberReadiness;
        LowestReadiness = FMath::Min(
            LowestReadiness,
            MemberReadiness
        );
        ++ReadinessSampleCount;
    }
    const float AverageReadiness = ReadinessSampleCount > 0
        ? TotalReadiness / static_cast<float>(ReadinessSampleCount)
        : 1.0f;
    CombatStatusWidget->SetFieldSquadStatus(
        LivingOperatives > 0,
        LivingOperatives,
        MaximumFieldSquadSize,
        bFieldSquadHolding,
        bFieldSquadEmbarked
    );
    CombatStatusWidget->SetFieldSquadServiceNeeds(
        MembersNeedingService,
        GetFieldSquadMembersRequiringEvacuationCount()
    );
    CombatStatusWidget->SetFieldSquadReadiness(
        AverageReadiness,
        LowestReadiness
    );
    const FString ContextActionLabel =
        FieldSquadContextAction ==
                EBHFieldSquadContextAction::CasualtyAid
            ? TEXT("AID")
            : FieldSquadContextAction ==
                    EBHFieldSquadContextAction::Sabotage
                ? TEXT("SABOTAGE")
                : FieldSquadContextAction ==
                        EBHFieldSquadContextAction::Secure
                    ? TEXT("SECURE")
                    : FieldSquadContextAction ==
                            EBHFieldSquadContextAction::Defend
                        ? TEXT("DEFEND")
                        : FString();
    CombatStatusWidget->SetFieldSquadContextStatus(
        ContextActionLabel,
        FieldSquadContextTargetLabel.ToString(),
        bFieldSquadContextActionReachedTarget
    );
}

void ABHCharacter::OnRep_FieldSquadContextStatus()
{
    if (FieldSquadContextAction ==
            EBHFieldSquadContextAction::None ||
        FieldSquadContextTargetLabel.IsNone())
    {
        return;
    }

    UE_LOG(
        LogTemp,
        Display,
        TEXT("BH_FIELD_SQUAD_CONTEXT_REPLICATED character=%s local=%d action=%d target=%s reached=%d"),
        *GetNameSafe(this),
        IsLocallyControlled() ? 1 : 0,
        static_cast<int32>(FieldSquadContextAction),
        *FieldSquadContextTargetLabel.ToString(),
        bFieldSquadContextActionReachedTarget ? 1 : 0
    );
}

#if !UE_BUILD_SHIPPING
bool ABHCharacter::ConfigureFieldSquadContextReplicationTest(
    EBHFieldSquadContextAction Action,
    FName TargetLabel,
    bool bReachedTarget
)
{
    if (!HasAuthority() ||
        Action == EBHFieldSquadContextAction::None ||
        TargetLabel.IsNone())
    {
        return false;
    }

    if (GetLivingFieldSquadCount() <= 0 &&
        !SpawnFieldSquadMember(0))
    {
        return false;
    }

    FieldSquadContextAction = Action;
    FieldSquadContextTargetLabel = TargetLabel;
    bFieldSquadContextActionReachedTarget = bReachedTarget;
    ForceNetUpdate();

    UE_LOG(
        LogTemp,
        Display,
        TEXT("BH_TEST_FIELD_SQUAD_CONTEXT_CONFIGURED result=success character=%s action=%d target=%s reached=%d members=%d"),
        *GetNameSafe(this),
        static_cast<int32>(Action),
        *TargetLabel.ToString(),
        bReachedTarget ? 1 : 0,
        GetLivingFieldSquadCount()
    );
    return true;
}

bool ABHCharacter::PrepareFieldSquadCasualtyForTransportTest()
{
    if (!HasAuthority())
    {
        return false;
    }

    for (ABHEnemySoldier* Member : FieldSquadMembers)
    {
        if (!IsValid(Member) ||
            Member->IsDead() ||
            Member->IsIncapacitated())
        {
            continue;
        }

        UBHHealthComponent* MemberHealth =
            Member->GetHealthComponent();
        if (!IsValid(MemberHealth))
        {
            continue;
        }

        const float DamageApplied = MemberHealth->ApplyDamage(
            MemberHealth->GetCurrentHealth() + 1.0f,
            this
        );
        if (DamageApplied <= 0.0f ||
            !Member->IsIncapacitated())
        {
            continue;
        }

        UE_LOG(
            LogTemp,
            Display,
            TEXT(
                "BH_TEST_FIELD_SQUAD_CASUALTY_PREPARED "
                "result=success id=%s"
            ),
            *Member->GetFieldOperativeID().ToString()
        );
        return true;
    }

    UE_LOG(
        LogTemp,
        Error,
        TEXT(
            "BH_TEST_FIELD_SQUAD_CASUALTY_PREPARED "
            "result=failure"
        )
    );
    return false;
}
#endif

void ABHCharacter::UpdateStrategicSituationHUD(float DeltaTime)
{
    if (!IsValid(CombatStatusWidget))
    {
        return;
    }

    StrategicSituationHUDRefreshRemaining -=
        FMath::Max(0.0f, DeltaTime);

    if (StrategicSituationHUDRefreshRemaining > 0.0f)
    {
        return;
    }

    StrategicSituationHUDRefreshRemaining = 0.25f;

    const UGameInstance* GameInstance = GetGameInstance();
    const UBHWarSubsystem* WarSubsystem = IsValid(GameInstance)
        ? GameInstance->GetSubsystem<UBHWarSubsystem>()
        : nullptr;
    const FName SectorID = IsValid(AmbientWarDirector)
        ? AmbientWarDirector->GetPlayerSectorID()
        : NAME_None;

    if (!IsValid(WarSubsystem) || SectorID.IsNone())
    {
        CombatStatusWidget->SetStrategicSituation(
            false,
            FText::GetEmpty(),
            EBHWarFaction::Neutral,
            0.0f,
            0.0f,
            0
        );
        CombatStatusWidget->SetEnemyResponsePressure(0.0f);
        CombatStatusWidget->SetCivilianSupport(50.0f);
        CombatStatusWidget->SetFieldReconStatus(
            false,
            0.0f,
            0.0f,
            1.0f,
            0.0f,
            1.0f,
            0.0f
        );
        LastPresentedStrategicSectorID = NAME_None;
        return;
    }

    const FBHWarSectorState SectorState =
        WarSubsystem->GetSectorState(SectorID);

    if (SectorState.SectorID.IsNone())
    {
        return;
    }

    const FText SectorDisplayName =
        SectorState.DisplayName.IsEmpty()
            ? FText::FromName(SectorID)
            : SectorState.DisplayName;

    CombatStatusWidget->SetStrategicSituation(
        true,
        SectorDisplayName,
        SectorState.Owner,
        SectorState.Supply,
        WarSubsystem->GetSectorSupplyChangePerTurn(SectorID),
        WarSubsystem->GetTurnNumber()
    );
    CombatStatusWidget->SetEnemyResponsePressure(
        SectorState.EnemyResponsePressure
    );
    CombatStatusWidget->SetCivilianSupport(
        SectorState.CivilianSupport
    );
    float IntelConfidence = SectorState.IntelConfidence;
    float ReconMovementProgress = 0.0f;
    float ReconMovementRequired = 1.0f;
    float ReconObservationProgress = 0.0f;
    float ReconObservationRequired = 1.0f;
    float ReconCooldownRemaining = 0.0f;
    const bool bFieldReconActive =
        IsValid(AmbientWarDirector) &&
        AmbientWarDirector->GetFieldReconStatus(
            IntelConfidence,
            ReconMovementProgress,
            ReconMovementRequired,
            ReconObservationProgress,
            ReconObservationRequired,
            ReconCooldownRemaining
        );
    CombatStatusWidget->SetFieldReconStatus(
        bFieldReconActive,
        IntelConfidence,
        ReconMovementProgress,
        ReconMovementRequired,
        ReconObservationProgress,
        ReconObservationRequired,
        ReconCooldownRemaining
    );

    if (LastPresentedStrategicSectorID != SectorID)
    {
        if (SectorState.EnemyResponsePressure >= 25.0f)
        {
            const FText ResponseSummary =
                WarSubsystem->GetSectorEnemyResponseSummary(
                    SectorID
                );
            const FText ResponseAdvice =
                SectorState.EnemyResponsePressure >= 75.0f
                    ? NSLOCTEXT(
                        "BrokenHorizon",
                        "FieldResponseCrackdownAdvice",
                        "Expect reinforced patrols and rapid reaction forces."
                    )
                    : SectorState.EnemyResponsePressure >= 50.0f
                        ? NSLOCTEXT(
                            "BrokenHorizon",
                            "FieldResponseHuntingAdvice",
                            "Enemy patrols are actively searching this area."
                        )
                        : NSLOCTEXT(
                            "BrokenHorizon",
                            "FieldResponseWatchfulAdvice",
                            "Enemy security is alert. Avoid unnecessary contact."
                        );

            ShowStatusNotification(FText::Format(
                NSLOCTEXT(
                    "BrokenHorizon",
                    "FieldResponseSectorEntry",
                    "{0}\n\n{1}\n{2}"
                ),
                SectorDisplayName,
                ResponseSummary,
                ResponseAdvice
            ));
        }

        UE_LOG(
            LogTemp,
            Display,
            TEXT(
                "BH_FIELD_SITUATION sector=%s owner=%d "
                "supply=%.1f response=%.1f turn=%d"
            ),
            *SectorID.ToString(),
            static_cast<int32>(SectorState.Owner),
            SectorState.Supply,
            SectorState.EnemyResponsePressure,
            WarSubsystem->GetTurnNumber()
        );
        LastPresentedStrategicSectorID = SectorID;
    }
}

void ABHCharacter::UpdateInitialWorldStreaming(float DeltaTime)
{
    if (!bWaitingForInitialWorldStreaming)
    {
        return;
    }

    InitialWorldStreamingElapsed += DeltaTime;

    const bool bMinimumHoldComplete =
        InitialWorldStreamingElapsed >=
        InitialWorldStreamingMinimumHoldTime;
    const bool bStreamingComplete =
        IsValid(OpenWorldStreamingSource) &&
        OpenWorldStreamingSource->IsStreamingCompleted();
    const bool bTimedOut =
        InitialWorldStreamingElapsed >=
        InitialWorldStreamingTimeout;

    if ((!bMinimumHoldComplete || !bStreamingComplete) &&
        !bTimedOut)
    {
        return;
    }

    bWaitingForInitialWorldStreaming = false;
    GetCharacterMovement()->SetMovementMode(MOVE_Walking);

    UE_LOG(
        LogTemp,
        Display,
        TEXT(
            "BH_WORLD_STREAMING_READY complete=%s elapsed=%.2f "
            "location=%s"
        ),
        bStreamingComplete ? TEXT("true") : TEXT("false"),
        InitialWorldStreamingElapsed,
        *GetActorLocation().ToCompactString()
    );
}

void ABHCharacter::PlayFirstPersonActionAnimation(
    UAnimSequenceBase* Animation
)
{
    if (!IsValid(FirstPersonArms) || !IsValid(Animation))
    {
        return;
    }

    bFirstPersonActionPlaying = true;
    ActiveFirstPersonLoop.Reset();
    FirstPersonArms->PlayAnimation(Animation, false);

    const float Duration = FMath::Max(
        Animation->GetPlayLength(),
        KINDA_SMALL_NUMBER
    );

    GetWorldTimerManager().SetTimer(
        FirstPersonActionTimerHandle,
        this,
        &ABHCharacter::HandleFirstPersonActionFinished,
        Duration,
        false
    );
}

void ABHCharacter::AddFirstPersonFireKick()
{
    if (!bEnableProceduralWeaponMotion)
    {
        return;
    }

    FirstPersonFireKickAlpha = 1.0f;
}

void ABHCharacter::PlayFirstPersonReloadMotion(float Duration)
{
    if (!bEnableProceduralWeaponMotion || Duration <= 0.0f)
    {
        return;
    }

    bFirstPersonReloadMotionPlaying = true;
    FirstPersonReloadElapsed = 0.0f;
    FirstPersonReloadDuration = Duration;
}

void ABHCharacter::RefreshFirstPersonArmsAnimation()
{
    if (bFirstPersonActionPlaying || !IsValid(FirstPersonArms))
    {
        return;
    }

    const bool bMoving =
        GetVelocity().SizeSquared2D() >
        FMath::Square(FirstPersonMovementAnimationThreshold);
    const bool bAiming =
        IsValid(WeaponComponent) && WeaponComponent->IsAiming();

    UAnimSequenceBase* DesiredAnimation = nullptr;

    if (bAiming)
    {
        DesiredAnimation = bMoving
            ? FirstPersonAimWalkAnimation
            : FirstPersonAimIdleAnimation;
    }
    else if (bMoving && bIsSprinting)
    {
        DesiredAnimation = FirstPersonRunAnimation;
    }
    else if (bMoving)
    {
        DesiredAnimation = FirstPersonWalkAnimation;
    }
    else
    {
        DesiredAnimation = FirstPersonIdleAnimation;
    }

    if (!IsValid(DesiredAnimation) ||
        ActiveFirstPersonLoop.Get() == DesiredAnimation)
    {
        return;
    }

    FirstPersonArms->PlayAnimation(DesiredAnimation, true);
    ActiveFirstPersonLoop = DesiredAnimation;
}

void ABHCharacter::HandleFirstPersonActionFinished()
{
    bFirstPersonActionPlaying = false;
    ActiveFirstPersonLoop.Reset();
    RefreshFirstPersonArmsAnimation();
}

void ABHCharacter::CancelFirstPersonActionAnimation()
{
    GetWorldTimerManager().ClearTimer(FirstPersonActionTimerHandle);
    bFirstPersonActionPlaying = false;
    ActiveFirstPersonLoop.Reset();
    RefreshFirstPersonArmsAnimation();
}

void ABHCharacter::CancelFirstPersonReloadMotion()
{
    bFirstPersonReloadMotionPlaying = false;
    FirstPersonReloadElapsed = 0.0f;
    FirstPersonReloadDuration = 0.0f;
}
void ABHCharacter::UpdateFirstPersonPresentationOffsets(float DeltaTime)
{
    if (!IsValid(FirstPersonArms) ||
        !bFirstPersonPresentationBaseCached)
    {
        return;
    }

    FirstPersonFireKickAlpha = FMath::FInterpTo(
        FirstPersonFireKickAlpha,
        0.0f,
        DeltaTime,
        FirstPersonFireKickRecoverySpeed
    );

    if (FMath::IsNearlyZero(FirstPersonFireKickAlpha, KINDA_SMALL_NUMBER))
    {
        FirstPersonFireKickAlpha = 0.0f;
    }

    float ReloadAlpha = 0.0f;

    if (bFirstPersonReloadMotionPlaying)
    {
        FirstPersonReloadElapsed += DeltaTime;

        const float ReloadProgress = FMath::Clamp(
            FirstPersonReloadElapsed /
                FMath::Max(FirstPersonReloadDuration, KINDA_SMALL_NUMBER),
            0.0f,
            1.0f
        );

        ReloadAlpha = FMath::Sin(ReloadProgress * PI);

        if (ReloadProgress >= 1.0f)
        {
            bFirstPersonReloadMotionPlaying = false;
            FirstPersonReloadElapsed = 0.0f;
            FirstPersonReloadDuration = 0.0f;
            ReloadAlpha = 0.0f;
        }
    }

    FVector PresentationLocation =
        FirstPersonPresentationBaseLocation +
        (FirstPersonFireKickLocation * FirstPersonFireKickAlpha) +
        (FirstPersonReloadLocation * ReloadAlpha);

    FRotator PresentationRotationOffset(
        (FirstPersonFireKickRotation.Pitch * FirstPersonFireKickAlpha) +
            (FirstPersonReloadRotation.Pitch * ReloadAlpha),
        (FirstPersonFireKickRotation.Yaw * FirstPersonFireKickAlpha) +
            (FirstPersonReloadRotation.Yaw * ReloadAlpha),
        (FirstPersonFireKickRotation.Roll * FirstPersonFireKickAlpha) +
            (FirstPersonReloadRotation.Roll * ReloadAlpha)
    );

    if (IsValid(InjuryComponent))
    {
        if (InjuryComponent->IsMedkitTreatmentActive())
        {
            const float TreatmentAlpha = FMath::Sin(
                InjuryComponent->GetMedkitTreatmentProgress() * PI
            );
            PresentationLocation +=
                FirstPersonMedicalLocation * TreatmentAlpha;
            PresentationRotationOffset +=
                FirstPersonMedicalRotation * TreatmentAlpha;
        }

        const float SwayDegrees =
            InjuryComponent->GetWeaponSwayDegrees();

        if (SwayDegrees > 0.0f && IsValid(GetWorld()))
        {
            const float SwayTime = GetWorld()->GetTimeSeconds();
            PresentationRotationOffset.Pitch +=
                FMath::Sin(SwayTime * 2.2f) * SwayDegrees;
            PresentationRotationOffset.Yaw +=
                FMath::Sin(SwayTime * 1.7f + 1.1f) *
                SwayDegrees;
        }
    }

    FirstPersonArms->SetRelativeLocationAndRotation(
        PresentationLocation,
        FirstPersonPresentationBaseRotation + PresentationRotationOffset
    );
}

void ABHCharacter::EnsureInteractionPromptWidget()
{
    if (IsValid(InteractionPromptWidget) || !IsPlayerControlled())
    {
        return;
    }

    APlayerController* PlayerController =
        ResolveOwningPlayerController();
    if (!IsValid(PlayerController) ||
        PlayerController->GetLocalPlayer() == nullptr)
    {
        return;
    }

    const TSubclassOf<UBHInteractionPromptWidget> WidgetClass =
        TSubclassOf<UBHInteractionPromptWidget>(
            UBHInteractionPromptWidget::StaticClass()
        );
    InteractionPromptWidget = CreateWidget<UBHInteractionPromptWidget>(
        PlayerController,
        WidgetClass
    );

    if (IsValid(InteractionPromptWidget))
    {
        InteractionPromptWidget->SetAlignmentInViewport(
            FVector2D(0.5f, 0.5f)
        );
        InteractionPromptWidget->SetPositionInViewport(
            FVector2D::ZeroVector,
            false
        );
        InteractionPromptWidget->SetDesiredSizeInViewport(
            FVector2D(760.0f, 96.0f)
        );
        InteractionPromptWidget->SetAnchorsInViewport(
            FAnchors(0.5f, 0.42f, 0.5f, 0.42f)
        );
        InteractionPromptWidget->AddToViewport(60);
        InteractionPromptWidget->SetVisibility(
            ESlateVisibility::Collapsed
        );

        UE_LOG(
            LogTemp,
            Display,
            TEXT(
                "BH_INTERACTION_PROMPT_NATIVE_RETRY class=%s"
            ),
            *WidgetClass->GetPathName()
        );
    }
    else
    {
        UE_LOG(
            LogTemp,
            Error,
            TEXT("BH_INTERACTION_PROMPT_CREATE_RETRY_FAILED class=%s"),
            *WidgetClass->GetPathName()
        );
    }
}

void ABHCharacter::UpdateInteractionPrompt()
{
    EnsureInteractionPromptWidget();

    if (!IsValid(InteractionPromptWidget) || !IsValid(FirstPersonCamera))
    {
        return;
    }

    if (!IsPlayerControlled())
    {
        InteractionPromptWidget->ClearInteractionText();
        return;
    }

    AActor* HitActor = nullptr;
    if (!ResolveInteractionTarget(HitActor))
    {
        InteractionPromptWidget->ClearInteractionText();
        return;
    }

    const UBHUserSettingsSubsystem* UserSettings = GetUserSettings();
    const auto ResolveInputPrompts = [UserSettings](const FText& SourceText)
    {
        return IsValid(UserSettings)
            ? UserSettings->ResolveLegacyInputPrompts(SourceText)
            : SourceText;
    };
    const auto ApplyInteractionPrompt = [this](const FText& PromptText)
    {
        InteractionPromptWidget->SetInteractionText(PromptText);
        InteractionPromptWidget->SetVisibility(ESlateVisibility::Visible);

#if !UE_BUILD_SHIPPING
        if (GEngine && IsLocallyControlled())
        {
            GEngine->AddOnScreenDebugMessage(
                0xBADC0DEULL,
                0.12f,
                FColor(255, 220, 96),
                FString::Printf(
                    TEXT("INTERACTION // %s"),
                    *PromptText.ToString()
                ),
                true,
                FVector2D(1.25f, 1.25f)
            );
        }
#endif
    };

    if (ABHCharacter* PlayerCasualty = Cast<ABHCharacter>(HitActor);
        IsValid(PlayerCasualty) && PlayerCasualty != this &&
        PlayerCasualty->IsPlayerIncapacitated())
    {
        const bool bStabilized =
            PlayerCasualty->IsPlayerCasualtyStabilized();
        ApplyInteractionPrompt(
            ResolveInputPrompts(
                bStabilized
                    ? NSLOCTEXT(
                        "BrokenHorizon",
                        "RevivePlayerCasualtyPrompt",
                        "Hold [F] to revive teammate (1 medkit)"
                    )
                    : FText::Format(
                        NSLOCTEXT(
                            "BrokenHorizon",
                            "StabilizePlayerCasualtyPrompt",
                            "Hold [F] to stabilize teammate "
                            "(1 field dressing // {0}s)"
                        ),
                        FText::AsNumber(FMath::CeilToInt(
                            PlayerCasualty
                                ->GetPlayerBleedOutSecondsRemaining()
                        ))
                    )
            )
        );
        return;
    }

    if (ABHCharacter* Teammate = Cast<ABHCharacter>(HitActor);
        IsValid(Teammate) && Teammate != this &&
        Teammate->IsPlayerControlled())
    {
        ApplyInteractionPrompt(
            ResolveInputPrompts(NSLOCTEXT(
                "BrokenHorizon",
                "ShareFieldSuppliesPrompt",
                "Hold [F] to share ammunition and critical supplies"
            ))
        );
        return;
    }

    if (ABHEnemySoldier* SurrenderedOperative =
            Cast<ABHEnemySoldier>(HitActor);
        IsValid(SurrenderedOperative) &&
        !SurrenderedOperative->IsDead() &&
        SurrenderedOperative->IsSurrendered())
    {
        const FText PromptText =
            SurrenderedOperative->IsSurrenderSecured()
                ? NSLOCTEXT(
                    "BrokenHorizon",
                    "SurrenderSecuredPrompt",
                    "SURRENDERED // CUSTODY SECURED"
                )
                : FText::Format(
                    NSLOCTEXT(
                        "BrokenHorizon",
                        "SecureSurrenderPrompt",
                        "Hold [F] to secure surrendered enemy "
                        "(escape window // {0}s)"
                    ),
                    FText::AsNumber(FMath::CeilToInt(
                        SurrenderedOperative
                            ->GetSurrenderEscapeSecondsRemaining()
                    ))
                );
        ApplyInteractionPrompt(
            ResolveInputPrompts(PromptText)
        );
        return;
    }

    if (ABHEnemySoldier* DownedOperative =
            Cast<ABHEnemySoldier>(HitActor);
        IsValid(DownedOperative) &&
        DownedOperative->IsIncapacitated() &&
        IsSharedFieldSquadMember(DownedOperative))
    {
        ApplyInteractionPrompt(
            ResolveInputPrompts(
                FText::Format(
                    NSLOCTEXT(
                        "BrokenHorizon",
                        "StabilizeFieldOperativePrompt",
                        "Press [F] to stabilize operative "
                        "(1 field dressing // {0}s)"
                    ),
                    FText::AsNumber(
                        FMath::CeilToInt(
                            DownedOperative
                                ->GetIncapacitationSecondsRemaining()
                        )
                    )
                )
            )
        );
        return;
    }

    if (IsValid(HitActor) &&
        HitActor->GetClass()->ImplementsInterface(
            UBHInteractable::StaticClass()
        ))
    {
        const FText PromptText =
            IBHInteractable::Execute_GetInteractionText(HitActor);

        ApplyInteractionPrompt(
            ResolveInputPrompts(PromptText)
        );
    }
    else
    {
        InteractionPromptWidget->ClearInteractionText();
    }
} 

void ABHCharacter::HandleInteractPressed()
{
    const UBHUserSettingsSubsystem* Settings = GetUserSettings();
    if (!IsValid(Settings) || !Settings->IsHoldInteractionEnabled())
    {
        Interact();
        return;
    }

    bInteractionInputHeld = true;
    InteractionPressTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
}

void ABHCharacter::HandleInteractReleased()
{
    if (!bInteractionInputHeld)
    {
        return;
    }
    const float ReleaseTime = GetWorld()
        ? GetWorld()->GetTimeSeconds()
        : InteractionPressTime;
    const float HeldDuration = FMath::Max(
        0.0f,
        ReleaseTime - InteractionPressTime
    );
    bInteractionInputHeld = false;
    if (ShouldCommitHeldInteraction(HeldDuration, HoldInteractionDuration))
    {
        Interact();
    }
}

void ABHCharacter::CancelInteractionInput()
{
    bInteractionInputHeld = false;
}

void ABHCharacter::Interact()
{
    if (bIsHandlingDeath ||
        bIsHandlingMissionComplete ||
        bIsTraversing ||
        !IsValid(FirstPersonCamera))
    {
        return;
    }

    AActor* TargetActor = nullptr;

    if (!ResolveInteractionTarget(TargetActor))
    {
        return;
    }

    if (HasAuthority())
    {
        ExecuteInteraction(TargetActor);
    }
    else
    {
        ServerInteract(TargetActor);
    }
}

void ABHCharacter::ServerInteract_Implementation(
    AActor* RequestedTarget
)
{
    AActor* AuthoritativeTarget = nullptr;

    if (!IsValid(RequestedTarget) ||
        !ResolveInteractionTarget(AuthoritativeTarget) ||
        AuthoritativeTarget != RequestedTarget)
    {
        return;
    }

    ExecuteInteraction(AuthoritativeTarget);
}

void ABHCharacter::IssueFieldSquadContextAction()
{
    if (bIsHandlingDeath ||
        bIsHandlingMissionComplete ||
        bIsTraversing)
    {
        return;
    }

    AActor* TargetActor = nullptr;

    if (!ResolveFieldSquadContextTarget(TargetActor))
    {
        ShowStatusNotification(
            NSLOCTEXT(
                "BrokenHorizon",
                "FieldSquadContextNoTarget",
                "SQUAD CONTEXT // NO SUPPORTED TARGET\n\n"
                "Aim at a downed friendly operative or active raid "
                "target, or the assigned attack/defense sector marker, "
                "and press X."
            )
        );
        return;
    }

    if (HasAuthority())
    {
        ExecuteFieldSquadContextAction(TargetActor);
    }
    else
    {
        ServerRequestFieldSquadContextAction(TargetActor);
    }
}

void ABHCharacter::ExecuteFieldSquadContextAction(
    AActor* TargetActor
)
{
    if (!HasAuthority())
    {
        return;
    }

    ABHEnemySoldier* Casualty = Cast<ABHEnemySoldier>(TargetActor);
    ABHCharacter* CasualtyOwner = FindFieldSquadOwner(Casualty);
    ABHRaidSabotageTarget* SabotageTarget =
        Cast<ABHRaidSabotageTarget>(TargetActor);
    ABHSectorAnchor* ObjectiveAnchor =
        Cast<ABHSectorAnchor>(TargetActor);
    const bool bCasualtyAidTarget =
        IsValid(Casualty) &&
        Casualty->IsIncapacitated() &&
        IsValid(CasualtyOwner);
    const bool bSabotageTarget =
        IsValid(SabotageTarget) &&
        SabotageTarget->CanAcceptFieldSquadSabotage();
    const bool bObjectivePresenceTarget =
        IsValid(ObjectiveAnchor) &&
        bRuntimeWarOperation &&
        IsValid(OpenWorldOperationDirector) &&
        OpenWorldOperationDirector->IsOperationActivated() &&
        ObjectiveAnchor->MatchesSector(AssignedWarSectorID) &&
        (AssignedWarPriorityType == EBHWarPriorityType::Attack ||
         AssignedWarPriorityType == EBHWarPriorityType::Defend);

    if (!bCasualtyAidTarget &&
        !bSabotageTarget &&
        !bObjectivePresenceTarget)
    {
        ShowStatusNotification(
            NSLOCTEXT(
                "BrokenHorizon",
                "FieldSquadContextUnsupportedTarget",
                "SQUAD CONTEXT // ACTION UNSUPPORTED\n\n"
                "Aim at a downed friendly operative, active raid "
                "target, or assigned attack/defense sector marker."
            )
        );
        return;
    }

    if (bFieldSquadEmbarked)
    {
        ShowStatusNotification(
            NSLOCTEXT(
                "BrokenHorizon",
                "FieldSquadContextEmbarked",
                "SQUAD CONTEXT // DISEMBARK REQUIRED\n\n"
                "Your operatives cannot perform Context actions while embarked."
            )
        );
        return;
    }

    if (bCasualtyAidTarget &&
        (!IsValid(InjuryComponent) ||
         InjuryComponent->GetFieldDressingCount() <= 0))
    {
        ShowStatusNotification(
            NSLOCTEXT(
                "BrokenHorizon",
                "FieldSquadContextNoDressing",
                "SQUAD CONTEXT // NO FIELD DRESSING\n\n"
                "Resupply before assigning casualty aid."
            )
        );
        return;
    }

    ABHEnemySoldier* BestResponder = nullptr;
    float BestDistanceSquared = TNumericLimits<float>::Max();

    for (ABHEnemySoldier* Member : FieldSquadMembers)
    {
        if (!IsValid(Member) ||
            (bCasualtyAidTarget && Member == Casualty) ||
            Member->IsDead() ||
            Member->IsIncapacitated())
        {
            continue;
        }

        const FVector ContextDestination =
            bObjectivePresenceTarget
                ? OpenWorldOperationDirector->GetOperationCenter()
                : TargetActor->GetActorLocation();
        const float DistanceSquared = FVector::DistSquared2D(
            Member->GetActorLocation(),
            ContextDestination
        );

        if (DistanceSquared < BestDistanceSquared)
        {
            BestDistanceSquared = DistanceSquared;
            BestResponder = Member;
        }
    }

    const bool bCanAssign = bCasualtyAidTarget
        ? BHWarOperationRules::CanAssignFieldSquadCasualtyAid(
            IsValid(BestResponder),
            true,
            bFieldSquadEmbarked,
            IsValid(InjuryComponent)
                ? InjuryComponent->GetFieldDressingCount()
                : 0)
        : bSabotageTarget
            ? BHWarOperationRules::CanAssignFieldSquadSabotage(
            IsValid(BestResponder),
            bSabotageTarget,
            bFieldSquadEmbarked
            )
            : BHWarOperationRules::
                CanAssignFieldSquadObjectivePresence(
                    IsValid(BestResponder),
                    IsValid(OpenWorldOperationDirector) &&
                        OpenWorldOperationDirector->
                            IsOperationActivated(),
                    AssignedWarPriorityType,
                    IsValid(ObjectiveAnchor) &&
                        ObjectiveAnchor->MatchesSector(
                            AssignedWarSectorID
                        ),
                    bFieldSquadEmbarked
                );

    if (!bCanAssign)
    {
        ShowStatusNotification(
            NSLOCTEXT(
                "BrokenHorizon",
                "FieldSquadContextNoResponder",
                "SQUAD CONTEXT // NO AVAILABLE OPERATIVE\n\n"
                "Only a living operative under your command can receive "
                "this order. Another player's order was not changed."
            )
        );
        return;
    }

    CancelFieldSquadContextAction(false);
    FieldSquadContextResponder = BestResponder;
    FieldSquadContextTarget = TargetActor;
    FieldSquadContextAction = bCasualtyAidTarget
        ? EBHFieldSquadContextAction::CasualtyAid
        : bSabotageTarget
            ? EBHFieldSquadContextAction::Sabotage
            : AssignedWarPriorityType == EBHWarPriorityType::Defend
                ? EBHFieldSquadContextAction::Defend
                : EBHFieldSquadContextAction::Secure;
    FieldSquadContextTargetLabel = bCasualtyAidTarget
        ? (
            Casualty->GetFieldOperativeID().IsNone()
                ? FName(*GetNameSafe(Casualty))
                : Casualty->GetFieldOperativeID()
        )
        : bSabotageTarget
            ? FName(TEXT("LOGISTICS CACHE"))
            : ObjectiveAnchor->GetSectorID();
    bFieldSquadContextActionReachedTarget = false;
    FieldSquadContextActionDeadline =
        GetWorld() ? GetWorld()->GetTimeSeconds() + 35.0f : 35.0f;
    ForceNetUpdate();

    if (ABHEnemyAIController* ResponderController =
            Cast<ABHEnemyAIController>(BestResponder->GetController()))
    {
        ResponderController->ClearHoldPosition();
        if (bObjectivePresenceTarget)
        {
            const float CommandYaw =
                IsValid(FirstPersonCamera)
                    ? FirstPersonCamera->GetComponentRotation().Yaw
                    : GetActorRotation().Yaw;
            ResponderController->SetMoveAndHoldPosition(
                OpenWorldOperationDirector->GetOperationCenter(),
                CommandYaw
            );
        }
        else
        {
            ResponderController->SetFollowTarget(
                TargetActor,
                FVector::ZeroVector
            );
        }
    }
    else
    {
        CancelFieldSquadContextAction(
            true,
            NSLOCTEXT(
                "BrokenHorizon",
                "FieldSquadContextNoController",
                "The selected operative cannot currently navigate to the target."
            )
        );
        return;
    }

    ShowPriorityStatusNotification(
        FText::Format(
            bCasualtyAidTarget
                ? NSLOCTEXT(
                    "BrokenHorizon",
                    "FieldSquadContextAidAssigned",
                    "SQUAD CONTEXT // CASUALTY AID ASSIGNED\n\n"
                    "Your operative {0} is moving to stabilize {1}. "
                    "Press C to cancel and restore the formation order."
                )
                : bSabotageTarget
                    ? NSLOCTEXT(
                    "BrokenHorizon",
                    "FieldSquadContextSabotageAssigned",
                    "SQUAD CONTEXT // SABOTAGE ASSIGNED\n\n"
                    "Your operative {0} is moving to arm charges at {1}. "
                    "Press C to cancel and restore the formation order."
                    )
                    : NSLOCTEXT(
                        "BrokenHorizon",
                        "FieldSquadContextObjectiveAssigned",
                        "SQUAD CONTEXT // OBJECTIVE ASSIGNED\n\n"
                        "Your operative {0} is moving to secure or "
                        "defend {1}. Press C to cancel."
                    ),
            FText::FromString(GetNameSafe(BestResponder)),
            FText::FromString(GetNameSafe(TargetActor))
        ),
        EBHNotificationPriority::High
    );

    UE_LOG(
        LogTemp,
        Display,
        TEXT("BH_FIELD_SQUAD_CONTEXT action=%s commander=%s responder=%s target=%s target_owner=%s"),
        bCasualtyAidTarget
            ? TEXT("casualty_aid")
            : bSabotageTarget
                ? TEXT("sabotage")
                : TEXT("objective_presence"),
        *GetNameSafe(this),
        *GetNameSafe(BestResponder),
        *GetNameSafe(TargetActor),
        *GetNameSafe(CasualtyOwner)
    );
}

void ABHCharacter::UpdateFieldSquadContextAction()
{
    if (!IsValid(FieldSquadContextResponder) ||
        !IsValid(FieldSquadContextTarget))
    {
        if (IsValid(FieldSquadContextResponder) ||
            IsValid(FieldSquadContextTarget))
        {
            CancelFieldSquadContextAction(false);
        }
        return;
    }

    ABHEnemySoldier* Casualty =
        Cast<ABHEnemySoldier>(FieldSquadContextTarget);
    ABHRaidSabotageTarget* SabotageTarget =
        Cast<ABHRaidSabotageTarget>(FieldSquadContextTarget);
    ABHSectorAnchor* ObjectiveAnchor =
        Cast<ABHSectorAnchor>(FieldSquadContextTarget);
    const bool bCasualtyStillEligible =
        IsValid(Casualty) && Casualty->IsIncapacitated();
    const bool bSabotageStillEligible =
        IsValid(SabotageTarget) &&
        SabotageTarget->CanAcceptFieldSquadSabotage();
    const bool bObjectiveStillEligible =
        IsValid(ObjectiveAnchor) &&
        bRuntimeWarOperation &&
        IsValid(OpenWorldOperationDirector) &&
        OpenWorldOperationDirector->IsOperationActivated() &&
        ObjectiveAnchor->MatchesSector(AssignedWarSectorID) &&
        (AssignedWarPriorityType == EBHWarPriorityType::Attack ||
         AssignedWarPriorityType == EBHWarPriorityType::Defend);

    if (IsValid(ObjectiveAnchor) && !bObjectiveStillEligible)
    {
        CancelFieldSquadContextAction(false);
        return;
    }

    if (FieldSquadContextResponder->IsDead() ||
        FieldSquadContextResponder->IsIncapacitated() ||
        (!bCasualtyStillEligible &&
         !bSabotageStillEligible &&
         !bObjectiveStillEligible))
    {
        CancelFieldSquadContextAction(
            true,
            NSLOCTEXT(
                "BrokenHorizon",
                "FieldSquadContextEnded",
                "The Context assignment ended because its responder or target is no longer eligible."
            )
        );
        return;
    }

    const float CurrentTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
    if (!bFieldSquadContextActionReachedTarget &&
        CurrentTime >= FieldSquadContextActionDeadline)
    {
        CancelFieldSquadContextAction(
            true,
            NSLOCTEXT(
                "BrokenHorizon",
                "FieldSquadContextTimedOut",
                "The assigned operative could not reach the target and has returned to the formation order."
            )
        );
        return;
    }

    // Follow movement settles within 300 cm of its target; keep the action
    // completion radius just beyond that bounded stop distance.
    constexpr float ContextActionRadius = 325.0f;
    const FVector ContextDestination =
        bObjectiveStillEligible
            ? OpenWorldOperationDirector->GetOperationCenter()
            : FieldSquadContextTarget->GetActorLocation();
    if (FVector::DistSquared2D(
            FieldSquadContextResponder->GetActorLocation(),
            ContextDestination) >
        FMath::Square(ContextActionRadius))
    {
        return;
    }

    if (bObjectiveStillEligible)
    {
        if (!bFieldSquadContextActionReachedTarget)
        {
            bFieldSquadContextActionReachedTarget = true;
            FieldSquadContextActionDeadline = 0.0f;
            ForceNetUpdate();
            ShowPriorityStatusNotification(
                FText::Format(
                    AssignedWarPriorityType ==
                            EBHWarPriorityType::Defend
                        ? NSLOCTEXT(
                            "BrokenHorizon",
                            "FieldSquadContextDefenseEstablished",
                            "SQUAD CONTEXT // DEFENSIVE LINE ESTABLISHED\n\n"
                            "Your operative is holding {0} and now "
                            "counts as a living defender."
                        )
                        : NSLOCTEXT(
                            "BrokenHorizon",
                            "FieldSquadContextSecureEstablished",
                            "SQUAD CONTEXT // SECURE PRESENCE ESTABLISHED\n\n"
                            "Your operative is holding {0} and now "
                            "contributes to objective security."
                        ),
                    ObjectiveAnchor->GetSectorDisplayName()
                ),
                EBHNotificationPriority::High
            );

            UE_LOG(
                LogTemp,
                Display,
                TEXT("BH_FIELD_SQUAD_CONTEXT_REACHED action=objective_presence commander=%s responder=%s sector=%s operation=%d"),
                *GetNameSafe(this),
                *GetNameSafe(FieldSquadContextResponder),
                *ObjectiveAnchor->GetSectorID().ToString(),
                static_cast<int32>(AssignedWarPriorityType)
            );
        }
        return;
    }

    if (bSabotageStillEligible)
    {
        const FString TargetName = GetNameSafe(SabotageTarget);
        const bool bSabotaged =
            SabotageTarget->SabotageByFieldOperative(
                FieldSquadContextResponder,
                this
            );

        if (!bSabotaged)
        {
            CancelFieldSquadContextAction(
                true,
                NSLOCTEXT(
                    "BrokenHorizon",
                    "FieldSquadContextSabotageFailed",
                    "The operative reached the target, but the sabotage contract was no longer valid."
                )
            );
            return;
        }

        CancelFieldSquadContextAction(false);
        UE_LOG(
            LogTemp,
            Display,
            TEXT("BH_FIELD_SQUAD_CONTEXT_COMPLETE action=sabotage commander=%s target=%s"),
            *GetNameSafe(this),
            *TargetName
        );
        return;
    }

    ABHEnemySoldier* StabilizedTarget = Casualty;
    ABHCharacter* StabilizedTargetOwner =
        FindFieldSquadOwner(StabilizedTarget);
    const bool bConsumedDressing =
        IsValid(InjuryComponent) &&
        InjuryComponent->ConsumeFieldDressingForSquadAid();
    const bool bStabilized =
        bConsumedDressing &&
        StabilizedTarget->StabilizeIncapacitatedSoldier();

    if (!bStabilized)
    {
        if (bConsumedDressing && IsValid(InjuryComponent))
        {
            InjuryComponent->AddMedicalSupplies(0, 1);
        }
        CancelFieldSquadContextAction(
            true,
            NSLOCTEXT(
                "BrokenHorizon",
                "FieldSquadContextAidFailed",
                "Casualty aid could not be completed. Check medical supplies and target condition."
            )
        );
        return;
    }

    CancelFieldSquadContextAction(false);

    if (IsValid(StabilizedTargetOwner) &&
        StabilizedTargetOwner != this)
    {
        StabilizedTargetOwner->ApplyFieldSquadOrder();
    }

    ShowPriorityStatusNotification(
        FText::Format(
            NSLOCTEXT(
                "BrokenHorizon",
                "FieldSquadContextAidComplete",
                "SQUAD CONTEXT // CASUALTY STABILIZED\n\n"
                "The operative completed aid. {0} field dressing(s) remain."
            ),
            FText::AsNumber(InjuryComponent->GetFieldDressingCount())
        ),
        EBHNotificationPriority::High
    );

    UE_LOG(
        LogTemp,
        Display,
        TEXT("BH_FIELD_SQUAD_CONTEXT_COMPLETE commander=%s target=%s dressings=%d"),
        *GetNameSafe(this),
        *GetNameSafe(StabilizedTarget),
        InjuryComponent->GetFieldDressingCount()
    );

    if (UBHSaveSubsystem* SaveSubsystem = GetGameInstance()
            ? GetGameInstance()->GetSubsystem<UBHSaveSubsystem>()
            : nullptr)
    {
        SaveSubsystem->SaveProgress();
    }
}

void ABHCharacter::CancelFieldSquadContextAction(
    bool bNotifyCommander,
    const FText& Reason
)
{
    const bool bHadAction =
        IsValid(FieldSquadContextResponder) ||
        IsValid(FieldSquadContextTarget);

    FieldSquadContextResponder = nullptr;
    FieldSquadContextTarget = nullptr;
    FieldSquadContextActionDeadline = 0.0f;
    FieldSquadContextAction = EBHFieldSquadContextAction::None;
    FieldSquadContextTargetLabel = NAME_None;
    bFieldSquadContextActionReachedTarget = false;

    if (bHadAction)
    {
        ApplyFieldSquadOrder();
        ForceNetUpdate();
    }

    if (bNotifyCommander && !Reason.IsEmpty())
    {
        ShowStatusNotification(
            FText::Format(
                NSLOCTEXT(
                    "BrokenHorizon",
                    "FieldSquadContextCancelled",
                    "SQUAD CONTEXT // ASSIGNMENT ENDED\n\n{0}"
                ),
                Reason
            )
        );
    }
}

void ABHCharacter::
    ServerRequestFieldSquadContextAction_Implementation(
        AActor* RequestedTarget
    )
{
    AActor* AuthoritativeTarget = nullptr;

    if (!IsValid(RequestedTarget) ||
        !ResolveFieldSquadContextTarget(AuthoritativeTarget) ||
        AuthoritativeTarget != RequestedTarget)
    {
        ShowStatusNotification(
            NSLOCTEXT(
                "BrokenHorizon",
                "FieldSquadContextTargetLost",
                "SQUAD CONTEXT // TARGET LOST\n\n"
                "Keep the supported ally or objective in view and press X again."
            )
        );
        return;
    }

    ExecuteFieldSquadContextAction(AuthoritativeTarget);
}

void ABHCharacter::ServerRequestDeployOperation_Implementation(
    FName SectorID,
    EBHWarPriorityType OperationType
)
{
    ExecuteDeployOperationRequest(SectorID, OperationType);
}

void ABHCharacter::ServerRequestWithdrawOperation_Implementation()
{
    ExecuteWithdrawOperationRequest();
}

void ABHCharacter::ServerRequestMobilizeMilitia_Implementation(
    FName SectorID
)
{
    ExecuteMobilizeMilitiaRequest(SectorID);
}

void ABHCharacter::ServerRequestRedeployGarrison_Implementation(
    FName DestinationSectorID
)
{
    ExecuteRedeployGarrisonRequest(DestinationSectorID);
}

void ABHCharacter::ServerRequestCivilianAid_Implementation(
    FName TargetSectorID,
    EBHWarPriorityType OperationType
)
{
    ExecuteCivilianAidRequest(TargetSectorID, OperationType);
}

void ABHCharacter::ServerRequestContinueDebrief_Implementation()
{
    HandleMissionContinueRequested();
}

void ABHCharacter::
    ServerRequestToggleFriendlySquadOrder_Implementation()
{
    ToggleFriendlySquadOrder();
}

bool ABHCharacter::ResolveInteractionTarget(
    AActor*& OutTarget
)
{
    OutTarget = nullptr;

    if (!IsValid(FirstPersonCamera) || !IsValid(GetWorld()))
    {
        return false;
    }

    const FVector TraceStart =
        FirstPersonCamera->GetComponentLocation();
    const FVector TraceEnd =
        TraceStart +
        (FirstPersonCamera->GetForwardVector() * InteractionDistance);

    const auto IsInteractable = [](const AActor* Candidate)
    {
        return IsValid(Candidate) &&
            Candidate->GetClass()->ImplementsInterface(
                UBHInteractable::StaticClass()
            );
    };

    FHitResult Hit;
    FCollisionQueryParams QueryParams(
        SCENE_QUERY_STAT(BHCharacterInteraction),
        false,
        this
    );
    QueryParams.AddIgnoredActor(this);

    const auto HasLineOfSight = [this, &TraceStart, &QueryParams](
        AActor* Candidate,
        const FVector& TargetPoint
    )
    {
        FHitResult VisibilityHit;
        const bool bVisibilityHit = GetWorld()->LineTraceSingleByChannel(
            VisibilityHit,
            TraceStart,
            TargetPoint,
            ECC_Visibility,
            QueryParams
        );

        return !bVisibilityHit || VisibilityHit.GetActor() == Candidate;
    };

    const bool bLineTraceHit = GetWorld()->LineTraceSingleByChannel(
        Hit,
        TraceStart,
        TraceEnd,
        ECC_Visibility,
        QueryParams
    );

    AActor* ResolvedTarget =
        bLineTraceHit && IsInteractable(Hit.GetActor())
            ? Hit.GetActor()
            : nullptr;
    float ResolvedDistance = ResolvedTarget ? Hit.Distance : 0.0f;

    bool bUsedSphereFallback = false;
    bool bUsedProximityFallback = false;
    if (!IsValid(ResolvedTarget))
    {
        TArray<FHitResult> SweepHits;
        const bool bSweepHit = GetWorld()->SweepMultiByChannel(
            SweepHits,
            TraceStart,
            TraceEnd,
            FQuat::Identity,
            ECC_Visibility,
            FCollisionShape::MakeSphere(72.0f),
            QueryParams
        );

        if (bSweepHit)
        {
            for (const FHitResult& SweepHit : SweepHits)
            {
                AActor* Candidate = SweepHit.GetActor();
                if (!IsInteractable(Candidate))
                {
                    continue;
                }

                if (HasLineOfSight(Candidate, Candidate->GetActorLocation()))
                {
                    ResolvedTarget = Candidate;
                    ResolvedDistance = SweepHit.Distance;
                    bUsedSphereFallback = true;
                    break;
                }
            }
        }
    }

    if (!IsValid(ResolvedTarget))
    {
        const FVector CameraForward = FirstPersonCamera->GetForwardVector();
        const float MaxInteractionDistance =
            FMath::Max(100.0f, InteractionDistance);
        const float MinimumViewDot = 0.45f;
        float BestCandidateScore = TNumericLimits<float>::Max();

        for (TActorIterator<AActor> It(GetWorld()); It; ++It)
        {
            AActor* Candidate = *It;
            if (!IsInteractable(Candidate) || Candidate == this)
            {
                continue;
            }

            const FBox CandidateBounds =
                Candidate->GetComponentsBoundingBox(true);
            if (!CandidateBounds.IsValid)
            {
                continue;
            }

            const FVector CandidatePoint = CandidateBounds.GetCenter();
            const FVector ToCandidate = CandidatePoint - TraceStart;
            const float CandidateCenterDistance = ToCandidate.Size();
            if (CandidateCenterDistance <= KINDA_SMALL_NUMBER)
            {
                continue;
            }

            const FVector ClosestPoint =
                CandidateBounds.GetClosestPointTo(TraceStart);
            const float CandidateDistance =
                FVector::Dist(TraceStart, ClosestPoint);
            if (CandidateDistance > MaxInteractionDistance)
            {
                continue;
            }

            const float ViewDot = FVector::DotProduct(
                CameraForward,
                ToCandidate / CandidateCenterDistance
            );
            if (ViewDot < MinimumViewDot ||
                !HasLineOfSight(Candidate, CandidatePoint))
            {
                continue;
            }

            const float CandidateScore =
                CandidateDistance + ((1.0f - ViewDot) * 400.0f);
            if (CandidateScore < BestCandidateScore)
            {
                BestCandidateScore = CandidateScore;
                ResolvedTarget = Candidate;
                ResolvedDistance = CandidateDistance;
                bUsedProximityFallback = true;
            }
        }
    }

    static double LastTraceDiagnosticTime = -1.0;
    const double CurrentTraceTime = GetWorld()->GetTimeSeconds();
    if (IsLocallyControlled() &&
        (LastTraceDiagnosticTime < 0.0 ||
         CurrentTraceTime - LastTraceDiagnosticTime >= 0.5))
    {
        LastTraceDiagnosticTime = CurrentTraceTime;
        UE_LOG(
            LogTemp,
            Display,
            TEXT(
                "BH_INTERACTION_TRACE hit=%d actor=%s class=%s "
                "implements=%d distance=%.1f sphere_fallback=%d "
                "proximity_fallback=%d"
            ),
            IsValid(ResolvedTarget) ? 1 : 0,
            IsValid(ResolvedTarget)
                ? *ResolvedTarget->GetName()
                : TEXT("None"),
            IsValid(ResolvedTarget)
                ? *ResolvedTarget->GetClass()->GetPathName()
                : TEXT("None"),
            IsInteractable(ResolvedTarget) ? 1 : 0,
            ResolvedDistance,
            bUsedSphereFallback ? 1 : 0,
            bUsedProximityFallback ? 1 : 0
        );
    }

    if (!IsValid(ResolvedTarget))
    {
        return false;
    }

    OutTarget = ResolvedTarget;
    return true;
}

bool ABHCharacter::ResolveFieldSquadContextTarget(
    AActor*& OutTarget
) const
{
    OutTarget = nullptr;

    if (!IsValid(FirstPersonCamera) || !IsValid(GetWorld()))
    {
        return false;
    }

    const FVector TraceStart =
        FirstPersonCamera->GetComponentLocation();
    const FVector TraceEnd =
        TraceStart +
        (FirstPersonCamera->GetForwardVector() *
         FMath::Max(1000.0f, FieldSquadCommandDistance));
    FHitResult Hit;
    FCollisionQueryParams QueryParams(
        SCENE_QUERY_STAT(BHFieldSquadContextCommand),
        true,
        this
    );
    QueryParams.AddIgnoredActor(this);

    if (!GetWorld()->LineTraceSingleByChannel(
            Hit,
            TraceStart,
            TraceEnd,
            ECC_Visibility,
            QueryParams))
    {
        return false;
    }

    OutTarget = Hit.GetActor();
    return IsValid(OutTarget);
}

void ABHCharacter::ExecuteInteraction(AActor* TargetActor)
{
    if (!HasAuthority() || !IsValid(TargetActor))
    {
        return;
    }

    if (ABHCharacter* PlayerCasualty = Cast<ABHCharacter>(TargetActor);
        IsValid(PlayerCasualty) && PlayerCasualty != this &&
        PlayerCasualty->IsPlayerIncapacitated())
    {
        TryTreatPlayerCasualty(PlayerCasualty);
    }
    else if (ABHCharacter* Teammate = Cast<ABHCharacter>(TargetActor);
        IsValid(Teammate) && Teammate != this &&
        Teammate->IsPlayerControlled())
    {
        TryShareFieldSuppliesWith(Teammate);
    }
    else if (ABHEnemySoldier* DownedOperative =
            Cast<ABHEnemySoldier>(TargetActor);
        IsValid(DownedOperative) &&
        DownedOperative->IsIncapacitated() &&
        IsSharedFieldSquadMember(DownedOperative))
    {
        TryStabilizeFieldSquadMember(DownedOperative);
    }
    else if (ABHEnemySoldier* SurrenderedOperative =
            Cast<ABHEnemySoldier>(TargetActor);
        IsValid(SurrenderedOperative) &&
        !SurrenderedOperative->IsDead() &&
        SurrenderedOperative->IsSurrendered())
    {
        if (SurrenderedOperative->SecureSurrender())
        {
            ShowPriorityStatusNotification(
                NSLOCTEXT(
                    "BrokenHorizon",
                    "SurrenderSecuredNotification",
                    "CUSTODY SECURED // ENEMY SURRENDERED"
                ),
                EBHNotificationPriority::High
            );
        }
    }
    else if (TargetActor->Implements<UBHInteractable>())
    {
        IBHInteractable::Execute_Interact(TargetActor, this);
    }
}

void ABHCharacter::AddKeycard(const FName KeycardID)
{
    if (!KeycardID.IsNone())
    {
        OwnedKeycards.AddUnique(KeycardID);
        ForceNetUpdate();
        RefreshOpenInventoryPanel();
    }
}

bool ABHCharacter::RemoveKeycard(const FName KeycardID)
{
    if (!HasAuthority() || KeycardID.IsNone())
    {
        return false;
    }

    const bool bRemoved = OwnedKeycards.Remove(KeycardID) > 0;
    if (bRemoved)
    {
        ForceNetUpdate();
        RefreshOpenInventoryPanel();
    }

    return bRemoved;
}

void ABHCharacter::EndPlay(
    const EEndPlayReason::Type EndPlayReason
)
{
    UnbindActiveOperationSnapshotPresentation();

    if (UGameInstance* GameInstance = GetGameInstance())
    {
        if (UBHUserSettingsSubsystem* SettingsSubsystem =
            GameInstance->GetSubsystem<UBHUserSettingsSubsystem>())
        {
            SettingsSubsystem->OnInputBindingsChanged.RemoveDynamic(
                this,
                &ABHCharacter::HandleInputBindingsChanged
            );
        }
        if (UBHWarSubsystem* WarSubsystem =
            GameInstance->GetSubsystem<UBHWarSubsystem>())
        {
            WarSubsystem->OnWarStateChanged.RemoveDynamic(
                this,
                &ABHCharacter::HandleWarStateChanged
            );
        }
    }

    if (IsValid(WarMapWidget))
    {
        WarMapWidget->OnCloseRequested.RemoveDynamic(
            this,
            &ABHCharacter::CloseWarMap
        );
        WarMapWidget->RemoveFromParent();
        WarMapWidget = nullptr;
    }

    bWarMapOpen = false;

    if (bPauseMenuOpen && ShouldPauseWorldForMenu(GetNetMode()))
    {
        UGameplayStatics::SetGamePaused(this, false);
    }

    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(
            RespawnTimerHandle
        );
        World->GetTimerManager().ClearTimer(
            FirstPersonActionTimerHandle
        );
    }

    DestroyFieldSquad();
    Super::EndPlay(EndPlayReason);
}

bool ABHCharacter::CollectKeycard(
    FName KeycardID,
    FName PickupPersistenceID
)
{
    if (KeycardID.IsNone() || PickupPersistenceID.IsNone())
    {
        UE_LOG(
            LogTemp,
            Error,
            TEXT(
                "Cannot collect keycard: both the inventory ID and "
                "pickup persistence ID are required."
            )
        );
        return false;
    }

    OwnedKeycards.AddUnique(KeycardID);
    CollectedKeycardPersistenceIDs.Add(PickupPersistenceID);
    ForceNetUpdate();
    RefreshOpenInventoryPanel();
    return true;
}

bool ABHCharacter::HasKeycard(const FName KeycardID) const
{
    return OwnedKeycards.Contains(KeycardID);
}

TArray<FName> ABHCharacter::GetOwnedKeycardIDs() const
{
    return OwnedKeycards;
}

TArray<FName>
ABHCharacter::GetCollectedKeycardPersistenceIDs() const
{
    return CollectedKeycardPersistenceIDs.Array();
}

bool ABHCharacter::CompleteObjective(FName ObjectiveID)
{
    if (!IsValid(ObjectiveComponent))
    {
        return false;
    }

    return ObjectiveComponent->CompleteObjectiveByID(ObjectiveID);
}

bool ABHCharacter::CompleteSharedObjective(FName ObjectiveID)
{
    if (!HasAuthority() || ObjectiveID.IsNone())
    {
        return false;
    }

    bool bInitiatingCharacterCompleted = false;
    int32 CompletedPlayerCount = 0;
    for (TActorIterator<ABHCharacter> It(GetWorld()); It; ++It)
    {
        ABHCharacter* PlayerCharacter = *It;
        if (!IsValid(PlayerCharacter) ||
            !PlayerCharacter->IsPlayerControlled() ||
            PlayerCharacter->GetCurrentObjectiveID() != ObjectiveID)
        {
            continue;
        }

        const bool bCompleted =
            PlayerCharacter->CompleteObjective(ObjectiveID);
        if (bCompleted)
        {
            ++CompletedPlayerCount;
        }
        if (PlayerCharacter == this)
        {
            bInitiatingCharacterCompleted = bCompleted;
        }
    }

    UE_LOG(
        LogTemp,
        Display,
        TEXT(
            "BH_SHARED_OBJECTIVE_COMPLETED objective=%s players=%d "
            "initiator=%s result=%s"
        ),
        *ObjectiveID.ToString(),
        CompletedPlayerCount,
        *GetName(),
        bInitiatingCharacterCompleted ? TEXT("success") : TEXT("failure")
    );
    return bInitiatingCharacterCompleted;
}

void ABHCharacter::FailSharedOperationObjectives()
{
    if (!HasAuthority() || GetWorld() == nullptr)
    {
        return;
    }

    int32 FailedParticipantCount = 0;
    for (TActorIterator<ABHCharacter> It(GetWorld()); It; ++It)
    {
        ABHCharacter* Participant = *It;
        if (!IsValid(Participant) ||
            Participant == this ||
            !Participant->IsPlayerControlled() ||
            !Participant->bRuntimeWarOperation ||
            Participant->AssignedWarSectorID != AssignedWarSectorID ||
            Participant->AssignedWarPriorityType !=
                AssignedWarPriorityType ||
            Participant->bIsHandlingMissionComplete ||
            !IsValid(Participant->ObjectiveComponent) ||
            Participant->ObjectiveComponent->IsMissionComplete() ||
            Participant->ObjectiveComponent->IsMissionFailed() ||
            Participant->ObjectiveComponent->GetCurrentObjectiveID().IsNone())
        {
            continue;
        }

        if (Participant->ObjectiveComponent->FailMission())
        {
            ++FailedParticipantCount;
        }
    }

    UE_LOG(
        LogTemp,
        Display,
        TEXT(
            "BH_SHARED_OPERATION_FAILURE_PROPAGATED "
            "sector=%s participants=%d"
        ),
        *AssignedWarSectorID.ToString(),
        FailedParticipantCount
    );

    if (FailedParticipantCount > 0)
    {
        UBHSaveSubsystem* SaveSubsystem = GetGameInstance()
            ? GetGameInstance()->GetSubsystem<UBHSaveSubsystem>()
            : nullptr;
        if (!IsValid(SaveSubsystem) || !SaveSubsystem->SaveProgress())
        {
            UE_LOG(
                LogTemp,
                Error,
                TEXT(
                    "BH_SHARED_OPERATION_FAILURE_CHECKPOINT_FAILED "
                    "participants=%d"
                ),
                FailedParticipantCount
            );
        }
    }
}

void ABHCharacter::PropagateSharedOperationFailure()
{
    if (!HasAuthority() ||
        GetWorld() == nullptr ||
        !bRuntimeWarOperation ||
        AssignedWarSectorID.IsNone() ||
        AssignedWarPriorityType == EBHWarPriorityType::None)
    {
        return;
    }

    FailSharedOperationObjectives();

    int32 DebriefParticipantCount = 0;
    for (TActorIterator<ABHCharacter> It(GetWorld()); It; ++It)
    {
        ABHCharacter* Participant = *It;
        if (!IsValid(Participant) ||
            !Participant->IsPlayerControlled() ||
            !Participant->IsRuntimeWarOperation() ||
            Participant->GetAssignedWarSectorID() !=
                AssignedWarSectorID ||
            Participant->GetAssignedWarPriorityType() !=
                AssignedWarPriorityType)
        {
            continue;
        }

        Participant->PresentSharedOperationDebrief(
            MissionCompleteMessage
        );
        ++DebriefParticipantCount;
    }

    UE_LOG(
        LogTemp,
        Display,
        TEXT(
            "BH_SHARED_OPERATION_FAILURE_DEBRIEF "
            "sector=%s participants=%d"
        ),
        *AssignedWarSectorID.ToString(),
        DebriefParticipantCount
    );
}

void ABHCharacter::RefreshReplicatedMissionPresentation()
{
    if (HasAuthority())
    {
        return;
    }

    RefreshObjectiveWidget();
    const bool bTerminalMissionState =
        IsMissionComplete() || IsMissionFailed();
    if (bTerminalMissionState && !bIsHandlingMissionComplete)
    {
#if !UE_BUILD_SHIPPING
        UE_LOG(
            LogTemp,
            Display,
            TEXT(
                "BH_TEST_OPERATION_DEBRIEF_RESTORED result=success "
                "failed=%d complete=%d"
            ),
            IsMissionFailed() ? 1 : 0,
            IsMissionComplete() ? 1 : 0
        );
#endif
        if (IsMissionFailed())
        {
            MissionCompleteMessage = NSLOCTEXT(
                "BrokenHorizon",
                "ReplicatedMissionFailureFallback",
                "MISSION FAILED\n\n"
                "The failed operation is recorded in the campaign."
            );
        }

        EnterMissionCompleteState(false);
    }
#if !UE_BUILD_SHIPPING
    if (IsLocallyControlled())
    {
        BHDefenseAMultiplayerTest::ObserveDebrief(MissionCompleteWidget, IsMissionComplete(), false);
    }
#endif
}

bool ABHCharacter::AdoptSharedMissionStateFrom(
    const ABHCharacter* SourceCharacter
)
{
    if (!HasAuthority() ||
        !IsValid(SourceCharacter) ||
        SourceCharacter == this ||
        !IsValid(ObjectiveComponent) ||
        !IsValid(SourceCharacter->GetMissionData()))
    {
        return false;
    }

    MissionData = SourceCharacter->GetMissionData();
    OwnedKeycards.Reset();
    for (const FName KeycardID :
         SourceCharacter->GetOwnedKeycardIDs())
    {
        OwnedKeycards.AddUnique(KeycardID);
    }
    CollectedKeycardPersistenceIDs.Reset();
    for (const FName PersistenceID :
         SourceCharacter->GetCollectedKeycardPersistenceIDs())
    {
        CollectedKeycardPersistenceIDs.Add(PersistenceID);
    }

    const bool bAdopted = ObjectiveComponent->RestoreMissionState(
        MissionData,
        SourceCharacter->GetCurrentObjectiveID(),
        SourceCharacter->GetCompletedObjectiveIDs(),
        SourceCharacter->IsMissionComplete(),
        SourceCharacter->IsMissionFailed()
    );
    RefreshObjectiveWidget();
    UE_LOG(
        LogTemp,
        Display,
        TEXT(
            "BH_SHARED_MISSION_ADOPTED target=%s source=%s "
            "completed=%d complete=%d result=%s"
        ),
        *GetName(),
        *SourceCharacter->GetName(),
        GetCompletedObjectiveIDs().Num(),
        IsMissionComplete() ? 1 : 0,
        bAdopted ? TEXT("success") : TEXT("failure")
    );
    return bAdopted;
}

UBHMissionData* ABHCharacter::GetMissionData() const
{
    if (IsValid(ObjectiveComponent))
    {
        if (UBHMissionData* ActiveMission =
            ObjectiveComponent->GetActiveMissionData())
        {
            return ActiveMission;
        }
    }

    return MissionData.Get();
}

FName ABHCharacter::GetCurrentObjectiveID() const
{
    return IsValid(ObjectiveComponent)
        ? ObjectiveComponent->GetCurrentObjectiveID()
        : NAME_None;
}

TArray<FName> ABHCharacter::GetCompletedObjectiveIDs() const
{
    return IsValid(ObjectiveComponent)
        ? ObjectiveComponent->GetCompletedObjectiveIDs()
        : TArray<FName>();
}

bool ABHCharacter::IsObjectiveCompleted(FName ObjectiveID) const
{
    return IsValid(ObjectiveComponent) &&
        ObjectiveComponent->IsObjectiveCompleted(ObjectiveID);
}

bool ABHCharacter::IsMissionComplete() const
{
    return IsValid(ObjectiveComponent) &&
        ObjectiveComponent->IsMissionComplete();
}

bool ABHCharacter::IsMissionFailed() const
{
    return IsValid(ObjectiveComponent) &&
        ObjectiveComponent->IsMissionFailed();
}

bool ABHCharacter::ReplayMission()
{
    if (bWarMapOpen)
    {
        CloseWarMap();
    }

    UBHSaveSubsystem* SaveSubsystem = GetGameInstance()
        ? GetGameInstance()->GetSubsystem<UBHSaveSubsystem>()
        : nullptr;

    if (IsValid(SaveSubsystem) &&
        !SaveSubsystem->DeleteSaveGame())
    {
        return false;
    }

    UWorld* World = GetWorld();

    if (!IsValid(World))
    {
        return false;
    }

    const FName CurrentLevelName(
        *UGameplayStatics::GetCurrentLevelName(World, true)
    );
    UGameplayStatics::OpenLevel(World, CurrentLevelName);
    return true;
}

void ABHCharacter::TogglePauseMenu()
{
    UE_LOG(
        LogTemp,
        Display,
        TEXT(
            "BH_PAUSE_INPUT_RECEIVED open=%d war_map=%d net_mode=%d"
        ),
        bPauseMenuOpen ? 1 : 0,
        bWarMapOpen ? 1 : 0,
        static_cast<int32>(GetNetMode())
    );

    if (bWarMapOpen)
    {
        CloseWarMap();
        return;
    }

    if (bPauseMenuOpen)
    {
        ResumeFromPause();
        return;
    }

    if (bIsHandlingDeath ||
        bIsHandlingMissionComplete ||
        !PauseMenuWidgetClass)
    {
        return;
    }

    APlayerController* PlayerController =
        ResolveOwningPlayerController();

    if (!IsValid(PlayerController) ||
        PlayerController->GetLocalPlayer() == nullptr)
    {
        return;
    }

    if (IsValid(WeaponComponent))
    {
        WeaponComponent->StopAllActions();
    }

    StopSprint();

    PauseMenuWidget = CreateWidget<UBHPauseMenuWidget>(
        PlayerController,
        PauseMenuWidgetClass
    );

    if (!IsValid(PauseMenuWidget))
    {
        return;
    }

    PauseMenuWidget->InitializePauseMenu(this);
    PauseMenuWidget->SetIsFocusable(true);
    PauseMenuWidget->AddToViewport(300);

    // Objective notifications can remain active while single-player world time
    // is paused. Keep their timers/content intact, but do not let them compete
    // with the modal pause menu for the center of the screen.
    if (IsValid(ObjectiveNotificationWidget))
    {
        ObjectiveNotificationWidget->SetPresentationSuppressed(true);
    }

    PlayerController->SetIgnoreMoveInput(true);
    PlayerController->SetIgnoreLookInput(true);

    FInputModeGameAndUI InputMode;
    InputMode.SetWidgetToFocus(PauseMenuWidget->TakeWidget());
    InputMode.SetLockMouseToViewportBehavior(
        EMouseLockMode::DoNotLock
    );
    InputMode.SetHideCursorDuringCapture(false);
    PlayerController->SetInputMode(InputMode);
    PlayerController->bShowMouseCursor = true;
    PauseMenuWidget->FocusInitialControl();

    bPauseMenuOpen = true;
    if (ShouldPauseWorldForMenu(GetNetMode()))
    {
        bPauseMenuOpen = UGameplayStatics::SetGamePaused(this, true);
    }

    if (!bPauseMenuOpen)
    {
        PauseMenuWidget->RemoveFromParent();
        PauseMenuWidget = nullptr;
        if (IsValid(ObjectiveNotificationWidget))
        {
            RefreshMissionPresentationVisibility();
        }
        PlayerController->SetIgnoreMoveInput(false);
        PlayerController->SetIgnoreLookInput(false);
        PlayerController->SetInputMode(FInputModeGameOnly());
        PlayerController->bShowMouseCursor = false;
    }
    else
    {
        UE_LOG(
            LogTemp,
            Display,
            TEXT("BH_PAUSE_MENU_OPENED cursor=%d world_paused=%d"),
            PlayerController->bShowMouseCursor ? 1 : 0,
            UGameplayStatics::IsGamePaused(this) ? 1 : 0
        );
    }
}

void ABHCharacter::ResumeFromPause()
{
    if (!bPauseMenuOpen)
    {
        return;
    }

    bPauseMenuOpen = false;
    if (ShouldPauseWorldForMenu(GetNetMode()))
    {
        UGameplayStatics::SetGamePaused(this, false);
    }

    if (IsValid(PauseMenuWidget))
    {
        PauseMenuWidget->RemoveFromParent();
        PauseMenuWidget = nullptr;
    }

    if (IsValid(ObjectiveNotificationWidget))
    {
        RefreshMissionPresentationVisibility();
    }

    APlayerController* PlayerController =
        ResolveOwningPlayerController();

    if (IsValid(PlayerController))
    {
        PlayerController->SetIgnoreMoveInput(false);
        PlayerController->SetIgnoreLookInput(false);
        PlayerController->SetInputMode(FInputModeGameOnly());
        PlayerController->bShowMouseCursor = false;
    }

    UE_LOG(
        LogTemp,
        Display,
        TEXT("BH_PAUSE_MENU_CLOSED input_mode=game_only")
    );
}

void ABHCharacter::ToggleFriendlySquadOrder()
{
    if (!HasAuthority())
    {
        ServerRequestToggleFriendlySquadOrder();
        return;
    }

    if (IsValid(FieldSquadContextResponder) ||
        IsValid(FieldSquadContextTarget))
    {
        CancelFieldSquadContextAction(
            true,
            NSLOCTEXT(
                "BrokenHorizon",
                "FieldSquadContextCancelledByOrder",
                "The assigned operative is returning to the current "
                "formation order. Press C again to change that order."
            )
        );
        return;
    }

    if (bRuntimeWarOperation &&
        IsValid(OpenWorldOperationDirector) &&
        OpenWorldOperationDirector->IsOperationActivated())
    {
        const int32 LivingFieldSquadCount =
            GetLivingFieldSquadCount();
        const int32 LivingOperationSupportCount =
            OpenWorldOperationDirector->
                GetLivingFriendlySupportCount();
        const int32 TotalFriendlyElements =
            LivingFieldSquadCount +
            LivingOperationSupportCount;

        if (TotalFriendlyElements <= 0)
        {
            ShowStatusNotification(
                NSLOCTEXT(
                    "BrokenHorizon",
                    "SquadOrderNoLivingSupport",
                    "SQUAD COMMAND UNAVAILABLE\n\n"
                    "No friendly support remains to receive the order."
                )
            );
            return;
        }

        const bool bIssueFollowOrder =
            (
                LivingFieldSquadCount > 0 &&
                bFieldSquadHolding
            ) ||
            (
                LivingOperationSupportCount > 0 &&
                OpenWorldOperationDirector->
                    IsFriendlySupportHolding()
            );

        if (bIssueFollowOrder)
        {
            bool bOrderReceived = false;

            if (LivingOperationSupportCount > 0)
            {
                bOrderReceived =
                    OpenWorldOperationDirector->
                        SetFriendlySupportFollowOrder() ||
                    bOrderReceived;
            }

            if (LivingFieldSquadCount > 0)
            {
                bFieldSquadHolding = false;
                bFieldSquadHasCommandLocation = false;
                FieldSquadCommandLocation = FVector::ZeroVector;
                FieldSquadCommandRotation = FRotator::ZeroRotator;
                ApplyFieldSquadOrder();
                bOrderReceived = true;
            }

            if (!bOrderReceived)
            {
                ShowStatusNotification(
                    NSLOCTEXT(
                        "BrokenHorizon",
                        "SquadOrderNoLivingSupport",
                        "SQUAD COMMAND UNAVAILABLE\n\n"
                        "No friendly support remains to receive "
                        "the order."
                    )
                );
                return;
            }

            ShowStatusNotification(
                FText::Format(
                    NSLOCTEXT(
                        "BrokenHorizon",
                        "UnifiedSquadOrderFollow",
                        "ALL ELEMENTS // FOLLOW\n\n"
                        "{0} friendly element(s) are reforming on "
                        "you. Aim at terrain and press C to order "
                        "MOVE AND HOLD."
                    ),
                    FText::AsNumber(TotalFriendlyElements)
                )
            );
        }
        else
        {
            FVector CommandLocation = FVector::ZeroVector;
            const bool bHasCommandLocation =
                TryResolveFieldSquadCommandLocation(
                    CommandLocation
                );
            const float CommandYaw =
                IsValid(FirstPersonCamera)
                    ? FirstPersonCamera
                        ->GetComponentRotation().Yaw
                    : GetActorRotation().Yaw;
            const FRotator CommandRotation(
                0.0f,
                CommandYaw,
                0.0f
            );
            const FVector CommandRight =
                FRotationMatrix(CommandRotation)
                    .GetUnitAxis(EAxis::Y);
            const bool bMultipleGroups =
                LivingFieldSquadCount > 0 &&
                LivingOperationSupportCount > 0;
            const FVector FieldCommandLocation =
                CommandLocation -
                (
                    bMultipleGroups
                        ? CommandRight * 350.0f
                        : FVector::ZeroVector
                );
            const FVector SupportCommandLocation =
                CommandLocation +
                (
                    bMultipleGroups
                        ? CommandRight * 350.0f
                        : FVector::ZeroVector
                );
            bool bOrderReceived = false;

            if (LivingOperationSupportCount > 0)
            {
                bOrderReceived =
                    (
                        bHasCommandLocation
                            ? OpenWorldOperationDirector->
                                SetFriendlySupportMoveAndHoldOrder(
                                    SupportCommandLocation,
                                    CommandYaw
                                )
                            : OpenWorldOperationDirector->
                                ToggleFriendlySupportHoldOrder()
                    ) ||
                    bOrderReceived;
            }

            if (LivingFieldSquadCount > 0)
            {
                bFieldSquadHolding = true;
                bFieldSquadHasCommandLocation =
                    bHasCommandLocation;
                FieldSquadCommandLocation =
                    bHasCommandLocation
                        ? FieldCommandLocation
                        : FVector::ZeroVector;
                FieldSquadCommandRotation =
                    bHasCommandLocation
                        ? CommandRotation
                        : FRotator::ZeroRotator;
                ApplyFieldSquadOrder();
                bOrderReceived = true;
            }

            if (!bOrderReceived)
            {
                ShowStatusNotification(
                    NSLOCTEXT(
                        "BrokenHorizon",
                        "SquadOrderNoLivingSupport",
                        "SQUAD COMMAND UNAVAILABLE\n\n"
                        "No friendly support remains to receive "
                        "the order."
                    )
                );
                return;
            }

            if (bHasCommandLocation)
            {
                const int32 CommandDistanceMeters =
                    FMath::Max(
                        1,
                        FMath::RoundToInt(
                            FVector::Dist2D(
                                GetActorLocation(),
                                CommandLocation
                            ) / 100.0f
                        )
                    );
                ShowStatusNotification(
                    FText::Format(
                        NSLOCTEXT(
                            "BrokenHorizon",
                            "UnifiedSquadOrderMoveHold",
                            "ALL ELEMENTS // MOVE AND HOLD\n\n"
                            "{0} friendly element(s) are moving to "
                            "designated ground {1} m away. Press C "
                            "to resume formation."
                        ),
                        FText::AsNumber(TotalFriendlyElements),
                        FText::AsNumber(CommandDistanceMeters)
                    )
                );
            }
            else
            {
                ShowStatusNotification(
                    FText::Format(
                        NSLOCTEXT(
                            "BrokenHorizon",
                            "UnifiedSquadOrderHoldFallback",
                            "ALL ELEMENTS // HOLD\n\n"
                            "No reachable terrain was designated. "
                            "{0} friendly element(s) will defend "
                            "their current ground. Press C to "
                            "resume formation."
                        ),
                        FText::AsNumber(TotalFriendlyElements)
                    )
                );
            }

            UE_LOG(
                LogTemp,
                Display,
                TEXT(
                    "BH_UNIFIED_SQUAD_ORDER order=hold mode=%s "
                    "location=%s field=%d support=%d"
                ),
                bHasCommandLocation
                    ? TEXT("designated")
                    : TEXT("current"),
                *CommandLocation.ToCompactString(),
                LivingFieldSquadCount,
                LivingOperationSupportCount
            );
        }

        if (UBHSaveSubsystem* SaveSubsystem =
                GetGameInstance()
                    ? GetGameInstance()->
                        GetSubsystem<UBHSaveSubsystem>()
                    : nullptr)
        {
            SaveSubsystem->SaveProgress();
        }

        return;
    }

    const int32 LivingFieldSquadCount =
        GetLivingFieldSquadCount();
    ABHSectorResupplyStation* NearbyFriendlyStation =
        FindNearbyFriendlyResupplyStation();

    if (LivingFieldSquadCount <= 0)
    {
        if (IsValid(NearbyFriendlyStation))
        {
            TryRecruitFieldSquadMember();
            return;
        }

        ShowStatusNotification(
            NSLOCTEXT(
                "BrokenHorizon",
                "FieldSquadUnavailable",
                "FIELD FIRETEAM UNAVAILABLE\n\n"
                "Visit a friendly resupply point and press C to "
                "recruit an operative."
            )
        );
        return;
    }

    if (bFieldSquadHolding)
    {
        bFieldSquadHolding = false;
        bFieldSquadHasCommandLocation = false;
        FieldSquadCommandLocation = FVector::ZeroVector;
        FieldSquadCommandRotation = FRotator::ZeroRotator;
        ApplyFieldSquadOrder();
        ShowStatusNotification(
            NSLOCTEXT(
                "BrokenHorizon",
                "FieldSquadOrderFollow",
                "FIELD FIRETEAM // FOLLOW\n\n"
                "Your operatives are reforming on you. "
                "Press C while aiming at terrain to issue a "
                "move-and-hold order."
            )
        );

        UE_LOG(
            LogTemp,
            Display,
            TEXT(
                "BH_FIELD_SQUAD_ORDER order=follow "
                "members=%d"
            ),
            LivingFieldSquadCount
        );
    }
    else if (IsValid(NearbyFriendlyStation) &&
             LivingFieldSquadCount < MaximumFieldSquadSize)
    {
        TryRecruitFieldSquadMember();
        return;
    }
    else
    {
        FVector CommandLocation = FVector::ZeroVector;
        bFieldSquadHasCommandLocation =
            TryResolveFieldSquadCommandLocation(CommandLocation);
        bFieldSquadHolding = true;

        if (bFieldSquadHasCommandLocation)
        {
            FieldSquadCommandLocation = CommandLocation;
            FieldSquadCommandRotation = FRotator(
                0.0f,
                IsValid(FirstPersonCamera)
                    ? FirstPersonCamera
                        ->GetComponentRotation().Yaw
                    : GetActorRotation().Yaw,
                0.0f
            );
        }
        else
        {
            FieldSquadCommandLocation = FVector::ZeroVector;
            FieldSquadCommandRotation = FRotator::ZeroRotator;
        }

        ApplyFieldSquadOrder();

        if (bFieldSquadHasCommandLocation)
        {
            const int32 CommandDistanceMeters =
                FMath::Max(
                    1,
                    FMath::RoundToInt(
                        FVector::Dist2D(
                            GetActorLocation(),
                            FieldSquadCommandLocation
                        ) / 100.0f
                    )
                );
            ShowStatusNotification(
                FText::Format(
                    bFieldSquadEmbarked
                        ? NSLOCTEXT(
                            "BrokenHorizon",
                            "FieldSquadOrderMoveHoldQueued",
                            "FIELD FIRETEAM // MOVE AND HOLD QUEUED\n\n"
                            "Designated ground is {0} m away. "
                            "Operatives will move there after "
                            "disembarking. Press C to resume "
                            "formation."
                        )
                        : NSLOCTEXT(
                            "BrokenHorizon",
                            "FieldSquadOrderMoveHold",
                            "FIELD FIRETEAM // MOVE AND HOLD\n\n"
                            "Operatives are moving to designated "
                            "ground {0} m away. Press C to resume "
                            "formation."
                        ),
                    FText::AsNumber(CommandDistanceMeters)
                )
            );
        }
        else
        {
            ShowStatusNotification(
                NSLOCTEXT(
                    "BrokenHorizon",
                    "FieldSquadOrderHoldFallback",
                    "FIELD FIRETEAM // HOLD\n\n"
                    "No reachable terrain was designated. "
                    "Operatives will defend their current ground. "
                    "Press C to resume formation."
                )
            );
        }

        UE_LOG(
            LogTemp,
            Display,
            TEXT(
                "BH_FIELD_SQUAD_ORDER order=hold mode=%s "
                "location=%s members=%d"
            ),
            bFieldSquadHasCommandLocation
                ? TEXT("designated")
                : TEXT("current"),
            *FieldSquadCommandLocation.ToCompactString(),
            LivingFieldSquadCount
        );
    }

    if (UBHSaveSubsystem* SaveSubsystem = GetGameInstance()
            ? GetGameInstance()->GetSubsystem<UBHSaveSubsystem>()
            : nullptr)
    {
        SaveSubsystem->SaveProgress();
    }
}

bool ABHCharacter::TryResolveFieldSquadCommandLocation(
    FVector& OutCommandLocation
) const
{
    const UWorld* World = GetWorld();

    if (!IsValid(World) ||
        !IsValid(FirstPersonCamera))
    {
        return false;
    }

    const FVector TraceStart =
        FirstPersonCamera->GetComponentLocation();
    const FVector TraceEnd =
        TraceStart +
        (FirstPersonCamera->GetForwardVector() *
         FMath::Max(1000.0f, FieldSquadCommandDistance));
    FCollisionQueryParams QueryParams(
        SCENE_QUERY_STAT(BHFieldSquadCommand),
        false,
        this
    );
    QueryParams.AddIgnoredActor(this);

    for (const ABHEnemySoldier* Member : FieldSquadMembers)
    {
        if (IsValid(Member))
        {
            QueryParams.AddIgnoredActor(Member);
        }
    }

    if (IsValid(FieldSquadTransport))
    {
        QueryParams.AddIgnoredActor(FieldSquadTransport);
    }

    FHitResult HitResult;
    const bool bHitTerrain =
        World->LineTraceSingleByChannel(
            HitResult,
            TraceStart,
            TraceEnd,
            ECC_Visibility,
            QueryParams
        ) &&
        HitResult.bBlockingHit &&
        HitResult.ImpactNormal.Z >= 0.25f;

    if (!bHitTerrain)
    {
        return false;
    }

    OutCommandLocation = HitResult.ImpactPoint;
    return true;
}

ABHSectorResupplyStation*
ABHCharacter::FindNearbyFriendlyResupplyStation() const
{
    const UWorld* World = GetWorld();
    const UGameInstance* GameInstance = GetGameInstance();
    const UBHWarSubsystem* WarSubsystem = IsValid(GameInstance)
        ? GameInstance->GetSubsystem<UBHWarSubsystem>()
        : nullptr;

    if (!IsValid(World) || !IsValid(WarSubsystem))
    {
        return nullptr;
    }

    ABHSectorResupplyStation* NearestStation = nullptr;
    float NearestDistanceSquared =
        FMath::Square(FMath::Max(100.0f, FieldSquadRecruitmentRadius));

    for (TActorIterator<ABHSectorResupplyStation> It(World);
         It;
         ++It)
    {
        ABHSectorResupplyStation* Station = *It;

        if (!IsValid(Station))
        {
            continue;
        }

        const FBHWarSectorState Sector =
            WarSubsystem->GetSectorState(Station->GetSectorID());
        const float DistanceSquared = FVector::DistSquared(
            GetActorLocation(),
            Station->GetActorLocation()
        );

        if (Sector.Owner == EBHWarFaction::Friendly &&
            DistanceSquared <= NearestDistanceSquared)
        {
            NearestStation = Station;
            NearestDistanceSquared = DistanceSquared;
        }
    }

    return NearestStation;
}

bool ABHCharacter::TryRecruitFieldSquadMember()
{
    ABHSectorResupplyStation* Station =
        FindNearbyFriendlyResupplyStation();
    UGameInstance* GameInstance = GetGameInstance();
    UBHWarSubsystem* WarSubsystem = IsValid(GameInstance)
        ? GameInstance->GetSubsystem<UBHWarSubsystem>()
        : nullptr;
    const int32 LivingCount = GetLivingFieldSquadCount();

    if (!IsValid(Station) || !IsValid(WarSubsystem))
    {
        return false;
    }

    if (LivingCount >= MaximumFieldSquadSize)
    {
        ShowStatusNotification(
            FText::Format(
                NSLOCTEXT(
                    "BrokenHorizon",
                    "FieldSquadAtCapacity",
                    "FIELD FIRETEAM FULL\n\n"
                    "{0}/{1} operatives ready. Move away from the "
                    "station and press C to issue orders."
                ),
                FText::AsNumber(LivingCount),
                FText::AsNumber(MaximumFieldSquadSize)
            )
        );
        return false;
    }

    const FName SectorID = Station->GetSectorID();

    if (!WarSubsystem->CanRecruitFieldOperative(SectorID))
    {
        ShowStatusNotification(
            FText::Format(
                NSLOCTEXT(
                    "BrokenHorizon",
                    "FieldSquadRecruitmentUnavailable",
                    "RECRUITMENT UNAVAILABLE\n\n"
                    "Requires 1 manpower and {0}% connected sector "
                    "supply."
                ),
                FText::AsNumber(FMath::RoundToInt(
                    WarSubsystem->GetFieldOperativeSupplyCost()
                ))
            )
        );
        return false;
    }

    if (!SpawnFieldSquadMember(LivingCount))
    {
        ShowStatusNotification(
            NSLOCTEXT(
                "BrokenHorizon",
                "FieldSquadSpawnBlocked",
                "RECRUITMENT DELAYED\n\n"
                "No safe deployment position is available nearby."
            )
        );
        return false;
    }

    if (!WarSubsystem->RecruitFieldOperative(SectorID))
    {
        ABHEnemySoldier* SpawnedMember =
            FieldSquadMembers.IsEmpty()
                ? nullptr
                : FieldSquadMembers.Pop();

        if (IsValid(SpawnedMember))
        {
            SpawnedMember->Destroy();
        }

        return false;
    }

    bFieldSquadHolding = false;
    bFieldSquadHasCommandLocation = false;
    FieldSquadCommandLocation = FVector::ZeroVector;
    FieldSquadCommandRotation = FRotator::ZeroRotator;
    ApplyFieldSquadOrder();
    const int32 UpdatedCount = GetLivingFieldSquadCount();
    ShowStatusNotification(
        FText::Format(
            NSLOCTEXT(
                "BrokenHorizon",
                "FieldSquadRecruited",
                "FIELD OPERATIVE RECRUITED\n\n"
                "Fireteam strength {0}/{1}. Away from this station, "
                "aim at terrain and press C to order MOVE AND HOLD."
            ),
            FText::AsNumber(UpdatedCount),
            FText::AsNumber(MaximumFieldSquadSize)
        )
    );

    if (UBHSaveSubsystem* SaveSubsystem =
            GameInstance->GetSubsystem<UBHSaveSubsystem>())
    {
        SaveSubsystem->SaveProgress();
    }

    return true;
}

bool ABHCharacter::SpawnFieldSquadMember(int32 FormationIndex)
{
    UWorld* World = GetWorld();

    if (!IsValid(World))
    {
        return false;
    }

    if (!FieldSquadSoldierClass)
    {
        for (TActorIterator<ABHEnemySoldier> It(World); It; ++It)
        {
            if (IsValid(*It))
            {
                FieldSquadSoldierClass = It->GetClass();
                break;
            }
        }
    }

    if (!FieldSquadSoldierClass)
    {
        FieldSquadSoldierClass = ABHEnemySoldier::StaticClass();
    }

    const FVector FormationOffset =
        BHWarOperationRules::CalculateFriendlyFormationOffset(
            FormationIndex,
            220.0f,
            300.0f
        );
    const FVector DesiredLocation =
        GetActorLocation() +
        (GetActorForwardVector() * FormationOffset.X) +
        (GetActorRightVector() * FormationOffset.Y) +
        FVector(0.0f, 0.0f, 100.0f);

    constexpr int32 MaximumSpawnAttempts = 6;

    for (int32 Attempt = 0;
         Attempt < MaximumSpawnAttempts;
         ++Attempt)
    {
        const float Angle = Attempt * (2.0f * PI / MaximumSpawnAttempts);
        const FVector AttemptOffset =
            Attempt == 0
                ? FVector::ZeroVector
                : FVector(
                    FMath::Cos(Angle),
                    FMath::Sin(Angle),
                    0.0f
                ) * (150.0f + Attempt * 60.0f);
        const FTransform SpawnTransform(
            GetActorRotation(),
            DesiredLocation + AttemptOffset,
            FVector::OneVector
        );
        ABHEnemySoldier* Soldier =
            World->SpawnActorDeferred<ABHEnemySoldier>(
                FieldSquadSoldierClass,
                SpawnTransform,
                this,
                nullptr,
                ESpawnActorCollisionHandlingMethod::
                    AdjustIfPossibleButDontSpawnIfColliding
            );

        if (!IsValid(Soldier))
        {
            continue;
        }

        Soldier->SetFlags(RF_Transient);
        Soldier->SetCombatFaction(EBHCombatFaction::Friendly);
        Soldier->SetFieldOperativeID(NAME_None);
        Soldier->SetObjectiveIdToCompleteOnDeath(NAME_None);
        UGameplayStatics::FinishSpawningActor(Soldier, SpawnTransform);

        if (!IsValid(Soldier))
        {
            continue;
        }

        if (!IsValid(Soldier->GetController()))
        {
            Soldier->SpawnDefaultController();
        }
        ABHEnemyAIController* SquadAIController =
            Cast<ABHEnemyAIController>(Soldier->GetController());

        if (!IsValid(SquadAIController))
        {
            Soldier->Destroy();
            continue;
        }

        SquadAIController->SetFollowTarget(
            this,
            FormationOffset
        );

        if (UBHHealthComponent* MemberHealth =
                Soldier->GetHealthComponent())
        {
            MemberHealth->OnDeath.AddDynamic(
                this,
                &ABHCharacter::HandleFieldSquadMemberDeath
            );
        }

        Soldier->OnFriendlyCasualtyExpired.AddDynamic(
            this,
            &ABHCharacter::HandleFieldSquadMemberCasualtyExpired
        );

        FieldSquadMembers.Add(Soldier);
        ForceNetUpdate();
        return true;
    }

    return false;
}

void ABHCharacter::ApplyFieldSquadOrder()
{
    if (bFieldSquadEmbarked)
    {
        return;
    }

    int32 FormationIndex = 0;

    for (ABHEnemySoldier* Member : FieldSquadMembers)
    {
        if (!IsValid(Member) || Member->IsDead())
        {
            continue;
        }

        ABHEnemyAIController* SquadAIController =
            Cast<ABHEnemyAIController>(Member->GetController());

        if (!IsValid(SquadAIController))
        {
            continue;
        }

        SquadAIController->OnHoldMoveFailed.RemoveAll(this);
        SquadAIController->OnHoldMoveFailed.AddUObject(
            this,
            &ABHCharacter::NotifySquadMoveAndHoldFailure
        );

        if (bFieldSquadHolding)
        {
            FVector HoldLocation = Member->GetActorLocation();

            if (bFieldSquadHasCommandLocation)
            {
                const FVector FormationOffset =
                    BHWarOperationRules::
                        CalculateFriendlyFormationOffset(
                            FormationIndex,
                            220.0f,
                            300.0f
                        );
                const FVector CommandForward =
                    FieldSquadCommandRotation.Vector();
                const FVector CommandRight =
                    FRotationMatrix(
                        FieldSquadCommandRotation
                    ).GetUnitAxis(EAxis::Y);
                HoldLocation =
                    FieldSquadCommandLocation +
                    (CommandForward * FormationOffset.X) +
                    (CommandRight * FormationOffset.Y);
            }

            if (bFieldSquadHasCommandLocation)
            {
                SquadAIController->SetMoveAndHoldPosition(
                    HoldLocation,
                    FieldSquadCommandRotation.Yaw
                );
            }
            else
            {
                SquadAIController->SetHoldPosition(HoldLocation);
            }
        }
        else
        {
            SquadAIController->SetFollowTarget(
                this,
                BHWarOperationRules::CalculateFriendlyFormationOffset(
                    FormationIndex,
                    220.0f,
                    300.0f
                )
            );
        }

        ++FormationIndex;
    }
}

bool ABHCharacter::ShouldPauseWorldForMenu(ENetMode NetMode)
{
    return NetMode == NM_Standalone;
}

void ABHCharacter::NotifySquadMoveAndHoldFailure(
    ABHEnemySoldier* SquadMember,
    const FVector& FailedDestination,
    const FVector& FallbackLocation
)
{
    UWorld* World = GetWorld();

    if (!HasAuthority() || !IsValid(World))
    {
        return;
    }

    const float CurrentTime = World->GetTimeSeconds();

    if (CurrentTime < NextSquadCommandFailureNotificationTime)
    {
        return;
    }

    NextSquadCommandFailureNotificationTime = CurrentTime + 2.5f;
    const int32 DistanceMeters = FMath::Max(
        1,
        FMath::RoundToInt(
            FVector::Dist2D(FailedDestination, FallbackLocation) /
            100.0f
        )
    );

    ShowPriorityStatusNotification(
        FText::Format(
            NSLOCTEXT(
                "BrokenHorizon",
                "SquadMoveHoldNavigationFailed",
                "SQUAD // ROUTE BLOCKED\n\n"
                "{0} could not reach the designated position "
                "({1} m away) and is holding safely in place. "
                "Aim at reachable ground and press C to redirect."
            ),
            FText::FromString(GetNameSafe(SquadMember)),
            FText::AsNumber(DistanceMeters)
        ),
        EBHNotificationPriority::High
    );

    UE_LOG(
        LogTemp,
        Display,
        TEXT(
            "BH_SQUAD_MOVE_HOLD_REPORTED commander=%s soldier=%s "
            "destination=%s fallback=%s"
        ),
        *GetNameSafe(this),
        *GetNameSafe(SquadMember),
        *FailedDestination.ToCompactString(),
        *FallbackLocation.ToCompactString()
    );
}

int32 ABHCharacter::GetLivingFieldSquadCount() const
{
    int32 LivingCount = 0;

    for (const ABHEnemySoldier* Member : FieldSquadMembers)
    {
        if (IsValid(Member) &&
            (!Member->IsDead() ||
             Member->IsIncapacitated()))
        {
            ++LivingCount;
        }
    }

    return LivingCount;
}

int32 ABHCharacter::GetIncapacitatedFieldSquadCount() const
{
    int32 IncapacitatedCount = 0;

    for (const ABHEnemySoldier* Member : FieldSquadMembers)
    {
        if (IsValid(Member) &&
            Member->IsIncapacitated())
        {
            ++IncapacitatedCount;
        }
    }

    return IncapacitatedCount;
}

int32 ABHCharacter::
    GetFieldSquadMembersRequiringEvacuationCount() const
{
    int32 EvacuationCount = 0;

    for (const ABHEnemySoldier* Member : FieldSquadMembers)
    {
        EvacuationCount +=
            IsValid(Member) &&
            Member->RequiresMedicalEvacuation()
                ? 1
                : 0;
    }

    return EvacuationCount;
}

FName ABHCharacter::GetFieldSquadRescueTargetID() const
{
    for (const ABHEnemySoldier* Member : FieldSquadMembers)
    {
        if (IsValid(Member) &&
            (Member->IsIncapacitated() ||
             Member->RequiresMedicalEvacuation()) &&
            !Member->GetFieldOperativeID().IsNone())
        {
            return Member->GetFieldOperativeID();
        }
    }

    return NAME_None;
}

bool ABHCharacter::HasFieldSquadRescueTarget() const
{
    return !GetFieldSquadRescueTargetID().IsNone();
}

TArray<FBHFieldSquadMemberState>
ABHCharacter::GetFieldSquadMemberStates() const
{
    TArray<FBHFieldSquadMemberState> MemberStates;
    MemberStates.Reserve(MaximumFieldSquadSize);
    int32 FormationIndex = 0;

    for (const ABHEnemySoldier* Member : FieldSquadMembers)
    {
        if (!IsValid(Member) ||
            (Member->IsDead() &&
             !Member->IsIncapacitated()))
        {
            continue;
        }

        FBHFieldSquadMemberState& MemberState =
            MemberStates.AddDefaulted_GetRef();
        MemberState.MemberID = Member->GetFieldOperativeID();
        MemberState.MagazineAmmo =
            Member->GetCurrentMagazineAmmo();
        MemberState.ReserveAmmo =
            Member->GetCurrentReserveAmmo();
        MemberState.FragGrenades =
            Member->GetCurrentFragGrenades();
        MemberState.CombatReadiness =
            Member->GetCombatReadiness();
        MemberState.bIncapacitated =
            Member->IsIncapacitated();
        MemberState.bRequiresMedicalEvacuation =
            Member->RequiresMedicalEvacuation();
        const bool bMemberEmbarked =
            IsValid(FieldSquadTransport) &&
            Member->GetAttachParentActor() ==
                FieldSquadTransport;
        MemberState.bEmbarked = bMemberEmbarked;
        MemberState.bHasWorldTransform =
            !bMemberEmbarked &&
            (MemberState.bIncapacitated ||
             bFieldSquadHolding);
        MemberState.IncapacitationSecondsRemaining =
            MemberState.bIncapacitated
                ? Member->GetIncapacitationSecondsRemaining()
                : 0.0f;

        if (MemberState.bHasWorldTransform)
        {
            FVector SavedLocation = Member->GetActorLocation();

            if (!MemberState.bIncapacitated &&
                bFieldSquadHasCommandLocation)
            {
                const FVector FormationOffset =
                    BHWarOperationRules::
                        CalculateFriendlyFormationOffset(
                            FormationIndex,
                            220.0f,
                            300.0f
                        );
                const FVector CommandForward =
                    FieldSquadCommandRotation.Vector();
                const FVector CommandRight =
                    FRotationMatrix(
                        FieldSquadCommandRotation
                    ).GetUnitAxis(EAxis::Y);
                SavedLocation =
                    FieldSquadCommandLocation +
                    (CommandForward * FormationOffset.X) +
                    (CommandRight * FormationOffset.Y);
            }

            MemberState.WorldTransform = FTransform(
                Member->GetActorRotation(),
                SavedLocation,
                Member->GetActorScale3D()
            );
        }

        if (const UBHHealthComponent* MemberHealth =
                Member->GetHealthComponent())
        {
            MemberState.Health =
                MemberHealth->GetCurrentHealth();
        }

        if (!MemberState.bIncapacitated)
        {
            ++FormationIndex;
        }
    }

    return MemberStates;
}

int32 ABHCharacter::CountFieldSquadMembersNeedingService(
    const FVector& ServiceLocation,
    float ServiceRadius
) const
{
    const float RadiusSquared = FMath::Square(
        FMath::Max(100.0f, ServiceRadius)
    );
    int32 ServiceableCount = 0;

    for (const ABHEnemySoldier* Member : FieldSquadMembers)
    {
        if (!IsValid(Member) ||
            FVector::DistSquared(
                ServiceLocation,
                Member->GetActorLocation()
            ) > RadiusSquared)
        {
            continue;
        }

        ServiceableCount +=
            Member->IsIncapacitated() ||
            (!Member->IsDead() &&
             Member->NeedsCombatService())
                ? 1
                : 0;
    }

    return ServiceableCount;
}

int32 ABHCharacter::GetFieldSquadMembersNeedingServiceCount() const
{
    int32 MembersNeedingService = 0;

    for (const ABHEnemySoldier* Member : FieldSquadMembers)
    {
        if (IsValid(Member) &&
            !Member->IsDead() &&
            Member->NeedsCombatService())
        {
            ++MembersNeedingService;
        }
    }

    return MembersNeedingService;
}

int32 ABHCharacter::ServiceFieldSquadMembers(
    const FVector& ServiceLocation,
    float ServiceRadius,
    bool bAtRescueTreatmentDestination
)
{
    const float RadiusSquared = FMath::Square(
        FMath::Max(100.0f, ServiceRadius)
    );
    int32 ServicedCount = 0;
    bool bStabilizedCasualty = false;
    bool bCompletedRescue = false;
    const UBHWarSubsystem* WarSubsystem = GetGameInstance()
        ? GetGameInstance()->GetSubsystem<UBHWarSubsystem>()
        : nullptr;
    const FName RescueTargetID =
        IsValid(WarSubsystem) &&
            WarSubsystem->GetCommittedOperationType() ==
                EBHWarPriorityType::Rescue
            ? WarSubsystem->GetCommittedOperationTargetID()
            : NAME_None;
    const bool bCanCompleteRescue =
        bAtRescueTreatmentDestination &&
        AssignedWarPriorityType == EBHWarPriorityType::Rescue &&
        IsValid(WarSubsystem) &&
        WarSubsystem->GetCommittedOperationSectorID() ==
            AssignedWarSectorID;

    for (ABHEnemySoldier* Member : FieldSquadMembers)
    {
        if (!IsValid(Member) ||
            FVector::DistSquared(
                ServiceLocation,
                Member->GetActorLocation()
            ) > RadiusSquared)
        {
            continue;
        }

        if (Member->IsIncapacitated())
        {
            if (Member->StabilizeIncapacitatedSoldier())
            {
                const bool bServiced =
                    Member->ServiceCombatLoadout();
                ++ServicedCount;
                bStabilizedCasualty = true;
                bCompletedRescue = bCompletedRescue ||
                    (bCanCompleteRescue &&
                     bServiced &&
                     Member->GetFieldOperativeID() ==
                        RescueTargetID);
            }
            continue;
        }

        if (Member->IsDead())
        {
            continue;
        }

        const bool bServiced = Member->ServiceCombatLoadout();
        ServicedCount += bServiced ? 1 : 0;
        bCompletedRescue = bCompletedRescue ||
            (bCanCompleteRescue &&
             bServiced &&
             Member->GetFieldOperativeID() == RescueTargetID);
    }

    if (bStabilizedCasualty)
    {
        ApplyFieldSquadOrder();
    }

    if (bCompletedRescue &&
        GetCurrentObjectiveID() == BHObjectiveIds::EvacuateCasualty)
    {
        UE_LOG(
            LogTemp,
            Display,
            TEXT(
                "BH_RESCUE_TREATMENT_COMPLETED casualty=%s "
                "destination=%s"
            ),
            *RescueTargetID.ToString(),
            IsValid(WarSubsystem)
                ? *WarSubsystem->GetCommittedOperationSectorID().ToString()
                : TEXT("None")
        );
        CompleteObjective(BHObjectiveIds::EvacuateCasualty);
    }

    return ServicedCount;
}

bool ABHCharacter::IsFieldSquadHolding() const
{
    return bFieldSquadHolding;
}

bool ABHCharacter::HasFieldSquadCommandLocation() const
{
    return
        bFieldSquadHolding &&
        bFieldSquadHasCommandLocation &&
        !FieldSquadCommandLocation.ContainsNaN();
}

FVector ABHCharacter::GetFieldSquadCommandLocation() const
{
    return HasFieldSquadCommandLocation()
        ? FieldSquadCommandLocation
        : FVector::ZeroVector;
}

float ABHCharacter::GetFieldSquadCommandYaw() const
{
    return HasFieldSquadCommandLocation()
        ? FieldSquadCommandRotation.Yaw
        : 0.0f;
}

bool ABHCharacter::RestoreFieldSquadState(
    int32 SavedLivingCount,
    bool bSavedHolding,
    bool bSavedHasCommandLocation,
    const FVector& SavedCommandLocation,
    float SavedCommandYaw
)
{
    DestroyFieldSquad();
    bFieldSquadHolding = bSavedHolding;
    bFieldSquadHasCommandLocation =
        bSavedHolding &&
        bSavedHasCommandLocation &&
        !SavedCommandLocation.ContainsNaN();
    FieldSquadCommandLocation =
        bFieldSquadHasCommandLocation
            ? SavedCommandLocation
            : FVector::ZeroVector;
    FieldSquadCommandRotation =
        bFieldSquadHasCommandLocation
            ? FRotator(
                0.0f,
                FRotator::NormalizeAxis(SavedCommandYaw),
                0.0f
            )
            : FRotator::ZeroRotator;
    const int32 CountToRestore = FMath::Clamp(
        SavedLivingCount,
        0,
        MaximumFieldSquadSize
    );

    for (int32 Index = 0; Index < CountToRestore; ++Index)
    {
        if (!SpawnFieldSquadMember(Index))
        {
            DestroyFieldSquad();
            return false;
        }
    }

    ApplyFieldSquadOrder();
    return true;
}

bool ABHCharacter::RestoreFieldSquadState(
    const TArray<FBHFieldSquadMemberState>& SavedMembers,
    bool bSavedHolding,
    bool bSavedHasCommandLocation,
    const FVector& SavedCommandLocation,
    float SavedCommandYaw
)
{
    DestroyFieldSquad();
    bFieldSquadHolding = bSavedHolding;
    bFieldSquadHasCommandLocation =
        bSavedHolding &&
        bSavedHasCommandLocation &&
        !SavedCommandLocation.ContainsNaN();
    FieldSquadCommandLocation =
        bFieldSquadHasCommandLocation
            ? SavedCommandLocation
            : FVector::ZeroVector;
    FieldSquadCommandRotation =
        bFieldSquadHasCommandLocation
            ? FRotator(
                0.0f,
                FRotator::NormalizeAxis(SavedCommandYaw),
                0.0f
            )
            : FRotator::ZeroRotator;
    const int32 CountToRestore = FMath::Clamp(
        SavedMembers.Num(),
        0,
        MaximumFieldSquadSize
    );

    for (int32 Index = 0; Index < CountToRestore; ++Index)
    {
        if (!SpawnFieldSquadMember(Index))
        {
            DestroyFieldSquad();
            return false;
        }

        ABHEnemySoldier* RestoredMember =
            FieldSquadMembers.IsValidIndex(Index)
                ? FieldSquadMembers[Index]
                : nullptr;

        if (!IsValid(RestoredMember))
        {
            DestroyFieldSquad();
            return false;
        }

        const FBHFieldSquadMemberState& SavedMember =
            SavedMembers[Index];

        RestoredMember->SetFieldOperativeID(
            SavedMember.MemberID
        );

        if (SavedMember.bEmbarked)
        {
            PendingFieldSquadTransportPassengers.Add(
                RestoredMember
            );
        }

        if (SavedMember.bHasWorldTransform)
        {
            RestoredMember->SetActorTransform(
                SavedMember.WorldTransform,
                false,
                nullptr,
                ETeleportType::TeleportPhysics
            );
        }

        RestoredMember->RestorePersistentCombatState(
            SavedMember.Health,
            SavedMember.MagazineAmmo,
            SavedMember.ReserveAmmo,
            SavedMember.FragGrenades,
            SavedMember.CombatReadiness
        );
        RestoredMember->RestoreMedicalEvacuationState(
            SavedMember.bRequiresMedicalEvacuation
        );

        if (SavedMember.bIncapacitated)
        {
            if (SavedMember.IncapacitationSecondsRemaining > 0.0f)
            {
                RestoredMember
                    ->RestoreIncapacitatedStateWithRemainingTime(
                        SavedMember
                            .IncapacitationSecondsRemaining
                    );
            }
            else
            {
                // Compatibility for schema 29 and older saves.
                RestoredMember->RestoreIncapacitatedState();
            }
        }
    }

    ApplyFieldSquadOrder();
    return true;
}

int32 ABHCharacter::BoardFieldSquadTransport(
    ABHFieldTransport* Transport,
    bool bUseSavedPassengerManifest
)
{
    if (!IsValid(Transport))
    {
        return 0;
    }

    if (bFieldSquadEmbarked &&
        FieldSquadTransport == Transport)
    {
        int32 ExistingPassengerCount = 0;
        for (const ABHEnemySoldier* Member : FieldSquadMembers)
        {
            ExistingPassengerCount +=
                IsValid(Member) &&
                Member->GetAttachParentActor() == Transport
                    ? 1
                    : 0;
        }
        return ExistingPassengerCount;
    }

    if (bFieldSquadEmbarked)
    {
        DisembarkFieldSquadTransport(FieldSquadTransport);
    }

    int32 PassengerIndex = 0;
    int32 CasualtyPassengerCount = 0;

    for (ABHEnemySoldier* Member : FieldSquadMembers)
    {
        if (!IsValid(Member))
        {
            continue;
        }

        const bool bSavedPassenger =
            PendingFieldSquadTransportPassengers.Contains(Member);
        const bool bEligibleToBoard =
            bUseSavedPassengerManifest
                ? bSavedPassenger
                : BHWarOperationRules::
                    IsFieldSquadMemberTransportEligible(
                        Member->IsDead(),
                        Member->IsIncapacitated(),
                        bFieldSquadHolding,
                        Member->RequiresMedicalEvacuation()
                    );

        if (!bEligibleToBoard)
        {
            continue;
        }

        if (!bUseSavedPassengerManifest &&
            FVector::DistSquared(
                Member->GetActorLocation(),
                Transport->GetActorLocation()
            ) >
            FMath::Square(
                FMath::Max(500.0f, FieldSquadBoardingRadius)
            ))
        {
            continue;
        }

        if (ABHEnemyAIController* SquadAIController =
                Cast<ABHEnemyAIController>(
                    Member->GetController()
                ))
        {
            SquadAIController->StopMovement();
            SquadAIController->ClearFollowTarget();
            SquadAIController->ClearHoldPosition();
            SquadAIController->SetActorTickEnabled(false);
        }

        Member->SetActorTickEnabled(false);
        Member->SetActorEnableCollision(false);
        Member->SetActorHiddenInGame(true);
        Member->AttachToActor(
            Transport,
            FAttachmentTransformRules::KeepWorldTransform
        );

        const float Side =
            PassengerIndex % 2 == 0 ? -1.0f : 1.0f;
        const float Row =
            static_cast<float>(PassengerIndex / 2);
        Member->SetActorRelativeLocation(
            FVector(
                -105.0f - (Row * 80.0f),
                Side * 62.0f,
                58.0f
            )
        );
        Member->SetActorRelativeRotation(FRotator::ZeroRotator);
        CasualtyPassengerCount +=
            Member->IsIncapacitated() ? 1 : 0;
        PendingFieldSquadTransportPassengers.Remove(Member);
        ++PassengerIndex;
    }

    if (bUseSavedPassengerManifest)
    {
        PendingFieldSquadTransportPassengers.Reset();
    }

    FieldSquadTransport =
        PassengerIndex > 0 ? Transport : nullptr;
    bFieldSquadEmbarked = PassengerIndex > 0;

    UE_LOG(
        LogTemp,
        Display,
        TEXT(
            "BH_FIELD_SQUAD_BOARDED transport=%s passengers=%d "
            "casualties=%d order=%s"
        ),
        *GetNameSafe(Transport),
        PassengerIndex,
        CasualtyPassengerCount,
        bFieldSquadHolding ? TEXT("hold") : TEXT("follow")
    );
    return PassengerIndex;
}

int32 ABHCharacter::DisembarkFieldSquadTransport(
    ABHFieldTransport* Transport
)
{
    if (!bFieldSquadEmbarked ||
        !IsValid(Transport) ||
        FieldSquadTransport != Transport)
    {
        return 0;
    }

    UWorld* World = GetWorld();
    int32 DeployedCount = 0;

    for (ABHEnemySoldier* Member : FieldSquadMembers)
    {
        if (!IsValid(Member) ||
            Member->GetAttachParentActor() != Transport)
        {
            continue;
        }

        if (Member->IsDead() &&
            !Member->IsIncapacitated())
        {
            Member->Destroy();
            continue;
        }

        Member->DetachFromActor(
            FDetachmentTransformRules::KeepWorldTransform
        );
        Member->SetActorHiddenInGame(false);
        Member->SetActorTickEnabled(true);
        Member->SetActorEnableCollision(true);

        if (ABHEnemyAIController* SquadAIController =
                Cast<ABHEnemyAIController>(
                    Member->GetController()
                ))
        {
            SquadAIController->SetActorTickEnabled(true);
        }

        const FVector FormationOffset =
            BHWarOperationRules::CalculateFriendlyFormationOffset(
                DeployedCount,
                240.0f,
                340.0f
            );
        FVector DeploymentLocation =
            GetActorLocation() +
            (GetActorForwardVector() * FormationOffset.X) +
            (GetActorRightVector() * FormationOffset.Y) +
            FVector(0.0f, 0.0f, 80.0f);
        const FRotator DeploymentRotation(
            0.0f,
            GetActorRotation().Yaw,
            0.0f
        );

        if (IsValid(World))
        {
            World->FindTeleportSpot(
                Member,
                DeploymentLocation,
                DeploymentRotation
            );
        }

        Member->SetActorLocationAndRotation(
            DeploymentLocation,
            DeploymentRotation,
            false,
            nullptr,
            ETeleportType::TeleportPhysics
        );
        ++DeployedCount;
    }

    FieldSquadTransport = nullptr;
    bFieldSquadEmbarked = false;
    PendingFieldSquadTransportPassengers.Reset();
    FieldSquadMembers.RemoveAllSwap(
        [](const TObjectPtr<ABHEnemySoldier>& Member)
        {
            return !IsValid(Member) ||
                (Member->IsDead() &&
                 !Member->IsIncapacitated());
        },
        EAllowShrinking::No
    );
    ApplyFieldSquadOrder();

    UE_LOG(
        LogTemp,
        Display,
        TEXT(
            "BH_FIELD_SQUAD_DISEMBARKED transport=%s deployed=%d "
            "order=%s"
        ),
        *GetNameSafe(Transport),
        DeployedCount,
        bFieldSquadHolding ? TEXT("hold") : TEXT("follow")
    );
    return DeployedCount;
}

void ABHCharacter::ApplyFieldSquadTransportDamage(
    float DamageAmount,
    const FDamageEvent& DamageEvent,
    AController* EventInstigator,
    AActor* DamageCauser
)
{
    if (!bFieldSquadEmbarked || DamageAmount <= 0.0f)
    {
        return;
    }

    for (ABHEnemySoldier* Member : FieldSquadMembers)
    {
        if (IsValid(Member) &&
            !Member->IsDead() &&
            Member->GetAttachParentActor() == FieldSquadTransport)
        {
            Member->TakeDamage(
                DamageAmount,
                DamageEvent,
                EventInstigator,
                DamageCauser
            );
        }
    }
}

bool ABHCharacter::IsFieldSquadEmbarked() const
{
    return bFieldSquadEmbarked;
}

FName ABHCharacter::GetFieldSquadTransportPersistenceID() const
{
    return bFieldSquadEmbarked &&
        IsValid(FieldSquadTransport)
            ? FieldSquadTransport->GetPersistenceID()
            : NAME_None;
}

void ABHCharacter::DestroyFieldSquad()
{
    FieldSquadContextResponder = nullptr;
    FieldSquadContextTarget = nullptr;
    FieldSquadContextActionDeadline = 0.0f;
    FieldSquadContextAction = EBHFieldSquadContextAction::None;
    FieldSquadContextTargetLabel = NAME_None;
    bFieldSquadContextActionReachedTarget = false;

    for (ABHEnemySoldier* Member : FieldSquadMembers)
    {
        if (IsValid(Member))
        {
            Member->Destroy();
        }
    }

    FieldSquadMembers.Reset();
    PendingFieldSquadTransportPassengers.Reset();
    FieldSquadTransport = nullptr;
    bFieldSquadHasCommandLocation = false;
    FieldSquadCommandLocation = FVector::ZeroVector;
    FieldSquadCommandRotation = FRotator::ZeroRotator;
    bFieldSquadEmbarked = false;
    ForceNetUpdate();
}

void ABHCharacter::HandleFieldSquadMemberDeath(
    AActor* DamageCauser
)
{
    const int32 RosterCount = GetLivingFieldSquadCount();
    const int32 IncapacitatedCount =
        GetIncapacitatedFieldSquadCount();
    const int32 CombatEffectiveCount =
        FMath::Max(0, RosterCount - IncapacitatedCount);

    ShowStatusNotification(
        IncapacitatedCount > 0
            ? FText::Format(
                NSLOCTEXT(
                    "BrokenHorizon",
                    "FieldSquadOperativeDown",
                    "FIELD OPERATIVE DOWN\n\n"
                    "{0} combat effective. {1} awaiting "
                    "stabilization.\n\nAim at the casualty and "
                    "press F. Requires one field dressing."
                ),
                FText::AsNumber(CombatEffectiveCount),
                FText::AsNumber(IncapacitatedCount)
            )
            : FText::Format(
                NSLOCTEXT(
                    "BrokenHorizon",
                    "FieldSquadCasualty",
                    "FIELD FIRETEAM CASUALTY\n\n"
                    "{0} operative(s) remain combat effective."
                ),
                FText::AsNumber(CombatEffectiveCount)
            )
    );

    UE_LOG(
        LogTemp,
        Display,
        TEXT(
            "BH_FIELD_SQUAD_CASUALTY combat_effective=%d "
            "incapacitated=%d causer=%s"
        ),
        CombatEffectiveCount,
        IncapacitatedCount,
        *GetNameSafe(DamageCauser)
    );

    if (UBHSaveSubsystem* SaveSubsystem = GetGameInstance()
            ? GetGameInstance()->GetSubsystem<UBHSaveSubsystem>()
            : nullptr)
    {
        SaveSubsystem->SaveProgress();
    }
}

void ABHCharacter::HandleFieldSquadMemberCasualtyExpired(
    AActor* ExpiredOperative
)
{
    ABHEnemySoldier* LostOperative =
        Cast<ABHEnemySoldier>(ExpiredOperative);
    if (!IsValid(LostOperative))
    {
        return;
    }

    const UBHWarSubsystem* WarSubsystem = GetGameInstance()
        ? GetGameInstance()->GetSubsystem<UBHWarSubsystem>()
        : nullptr;
    const bool bLostRescueTarget =
        IsValid(WarSubsystem) &&
        WarSubsystem->GetCommittedOperationType() ==
            EBHWarPriorityType::Rescue &&
        WarSubsystem->GetCommittedOperationTargetID() ==
            LostOperative->GetFieldOperativeID();

    if (FieldSquadMembers.RemoveSingle(LostOperative) <= 0)
    {
        return;
    }
    ForceNetUpdate();

    if (bLostRescueTarget)
    {
        FailCurrentWarOperation(
            NSLOCTEXT(
                "BrokenHorizon",
                "RescueCasualtyExpiredFailureReason",
                "The assigned casualty died before reaching "
                "a friendly treatment point."
            )
        );
    }

    const int32 RemainingOperatives =
        GetLivingFieldSquadCount();
    if (bFieldSquadEmbarked)
    {
        const bool bHasRemainingPassenger =
            FieldSquadMembers.ContainsByPredicate(
                [this](const TObjectPtr<ABHEnemySoldier>& Member)
                {
                    return IsValid(Member) &&
                        Member->GetAttachParentActor() ==
                            FieldSquadTransport;
                }
            );

        if (!bHasRemainingPassenger)
        {
            FieldSquadTransport = nullptr;
            bFieldSquadEmbarked = false;
        }
    }

    ShowStatusNotificationWithAudioCue(
        FText::Format(
            NSLOCTEXT(
                "BrokenHorizon",
                "FieldSquadOperativeLost",
                "OPERATIVE LOST\n\n"
                "The stabilization window expired. "
                "{0} operative(s) remain."
            ),
            FText::AsNumber(RemainingOperatives)
        ),
        EBHNotificationPriority::Critical,
        EBHNotificationAudioCue::StrategicWarning
    );

    UE_LOG(
        LogTemp,
        Display,
        TEXT(
            "BH_FIELD_OPERATIVE_LOST member=%s remaining=%d"
        ),
        *GetNameSafe(LostOperative),
        RemainingOperatives
    );

    if (UBHSaveSubsystem* SaveSubsystem = GetGameInstance()
            ? GetGameInstance()->GetSubsystem<UBHSaveSubsystem>()
            : nullptr)
    {
        SaveSubsystem->SaveProgress();
    }
}

bool ABHCharacter::TryStabilizeFieldSquadMember(
    ABHEnemySoldier* SquadMember
)
{
    if (!IsValid(SquadMember) ||
        !IsSharedFieldSquadMember(SquadMember) ||
        !SquadMember->IsIncapacitated() ||
        FVector::DistSquared(
            GetActorLocation(),
            SquadMember->GetActorLocation()
        ) > FMath::Square(InteractionDistance + 100.0f))
    {
        return false;
    }

    ABHCharacter* SquadOwner =
        FindFieldSquadOwner(SquadMember);

    if (!IsValid(InjuryComponent) ||
        !InjuryComponent->ConsumeFieldDressingForSquadAid())
    {
        ShowStatusNotification(
            NSLOCTEXT(
                "BrokenHorizon",
                "FieldSquadNoDressing",
                "NO FIELD DRESSING\n\n"
                "Resupply before stabilizing this operative."
            )
        );
        return false;
    }

    if (!SquadMember->StabilizeIncapacitatedSoldier())
    {
        InjuryComponent->AddMedicalSupplies(0, 1);
        return false;
    }

    if (IsValid(SquadOwner))
    {
        SquadOwner->ApplyFieldSquadOrder();
    }
    else
    {
        ApplyFieldSquadOrder();
    }
    ShowPriorityStatusNotification(
        FText::Format(
            NSLOCTEXT(
                "BrokenHorizon",
                "FieldSquadStabilized",
                "OPERATIVE STABILIZED\n\n"
                "{0} field dressing(s) remain.\n\n"
                "Combat readiness is degraded. Evacuate the "
                "operative to a friendly support point for full "
                "treatment."
            ),
            FText::AsNumber(
                InjuryComponent->GetFieldDressingCount()
            )
        ),
        EBHNotificationPriority::High
    );

    UE_LOG(
        LogTemp,
        Display,
        TEXT(
            "BH_FIELD_OPERATIVE_AID member=%s dressings=%d"
        ),
        *SquadMember->GetName(),
        InjuryComponent->GetFieldDressingCount()
    );

    if (UBHSaveSubsystem* SaveSubsystem = GetGameInstance()
            ? GetGameInstance()->GetSubsystem<UBHSaveSubsystem>()
            : nullptr)
    {
        SaveSubsystem->SaveProgress();
    }

    return true;
}

bool ABHCharacter::IsSharedFieldSquadMember(
    const ABHEnemySoldier* SquadMember
) const
{
    if (!IsValid(SquadMember))
    {
        return false;
    }

    if (FieldSquadMembers.Contains(SquadMember))
    {
        return true;
    }

    if (HasAuthority())
    {
        return IsValid(FindFieldSquadOwner(SquadMember));
    }

    return SquadMember->GetCombatFaction() ==
        EBHCombatFaction::Friendly;
}

ABHCharacter* ABHCharacter::FindFieldSquadOwner(
    const ABHEnemySoldier* SquadMember
) const
{
    if (!IsValid(SquadMember) || !IsValid(GetWorld()))
    {
        return nullptr;
    }

    for (TActorIterator<ABHCharacter> It(GetWorld()); It; ++It)
    {
        ABHCharacter* Candidate = *It;

        if (IsValid(Candidate) &&
            Candidate->FieldSquadMembers.Contains(SquadMember))
        {
            return Candidate;
        }
    }

    return nullptr;
}

void ABHCharacter::ToggleWarMap()
{
    if (bWarMapOpen)
    {
        CloseWarMap();
        return;
    }

    if (bPauseMenuOpen ||
        bIsHandlingDeath ||
        bIsHandlingMissionComplete)
    {
        return;
    }

    UBHWarSubsystem* WarSubsystem = GetGameInstance()
        ? GetGameInstance()->GetSubsystem<UBHWarSubsystem>()
        : nullptr;
    const bool bCanDeploy =
        !bRuntimeWarOperation &&
        IsValid(WarSubsystem) &&
        !WarSubsystem->IsCampaignResolved();

    OpenWarMap(bCanDeploy);
}

void ABHCharacter::OpenWarMap(bool bDeploymentMode)
{
    APlayerController* PlayerController =
        ResolveOwningPlayerController();
    UGameInstance* GameInstance = GetGameInstance();
    UBHWarSubsystem* WarSubsystem = GameInstance
        ? GameInstance->GetSubsystem<UBHWarSubsystem>()
        : nullptr;

    if (!IsValid(PlayerController) ||
        PlayerController->GetLocalPlayer() == nullptr ||
        !IsValid(WarSubsystem))
    {
        return;
    }

    if (IsValid(WeaponComponent))
    {
        WeaponComponent->StopAllActions();
    }

    StopSprint();

    TSubclassOf<UBHWarMapWidget> WidgetClass =
        WarMapWidgetClass;

    if (!WidgetClass)
    {
        WidgetClass = UBHWarMapWidget::StaticClass();
    }
    WarMapWidget = CreateWidget<UBHWarMapWidget>(
        PlayerController,
        WidgetClass
    );

    if (!IsValid(WarMapWidget))
    {
        return;
    }

    WarMapWidget->InitializeWarMap(WarSubsystem);
    WarMapWidget->SetDeploymentMode(bDeploymentMode);
    WarMapWidget->OnCloseRequested.AddDynamic(
        this,
        &ABHCharacter::CloseWarMap
    );
    WarMapWidget->OnDeployRequested.AddDynamic(
        this,
        &ABHCharacter::HandleDeployRequested
    );
    WarMapWidget->OnWithdrawRequested.AddDynamic(
        this,
        &ABHCharacter::HandleWithdrawRequested
    );
    WarMapWidget->OnMilitiaRequested.AddDynamic(
        this,
        &ABHCharacter::HandleMilitiaRequested
    );
    WarMapWidget->OnGarrisonRedeployRequested.AddDynamic(
        this,
        &ABHCharacter::HandleGarrisonRedeployRequested
    );
    WarMapWidget->OnCivilianAidRequested.AddDynamic(
        this,
        &ABHCharacter::HandleCivilianAidRequested
    );
    WarMapWidget->SetIsFocusable(true);
    WarMapWidget->SetAnchorsInViewport(
        FAnchors(0.0f, 0.0f, 1.0f, 1.0f)
    );
    WarMapWidget->AddToViewport(250);

    if (UGameInstance* TelemetryGameInstance = GetGameInstance())
    {
        TelemetryGameInstance->GetSubsystem<UBHPlaytestTelemetrySubsystem>()->RecordEvent(
            TEXT("war_map_opened"),
            {{TEXT("deploymentMode"), bDeploymentMode ? TEXT("true") : TEXT("false")}}
        );
    }

    PlayerController->SetIgnoreMoveInput(true);
    PlayerController->SetIgnoreLookInput(true);

    FInputModeGameAndUI InputMode;
    InputMode.SetWidgetToFocus(WarMapWidget->TakeWidget());
    InputMode.SetLockMouseToViewportBehavior(
        EMouseLockMode::DoNotLock
    );
    InputMode.SetHideCursorDuringCapture(false);
    PlayerController->SetInputMode(InputMode);
    PlayerController->bShowMouseCursor = true;
    WarMapWidget->SetKeyboardFocus();
    bWarMapOpen = true;
    bWarMapDeploymentMode = bDeploymentMode;

    UE_LOG(
        LogTemp,
        Log,
        TEXT("Strategic war map opened with %d sectors."),
        WarSubsystem->GetSectorStates().Num()
    );
}

void ABHCharacter::CloseWarMap()
{
    if (!bWarMapOpen && !IsValid(WarMapWidget))
    {
        return;
    }

    if (UGameInstance* TelemetryGameInstance = GetGameInstance())
    {
        TelemetryGameInstance->GetSubsystem<UBHPlaytestTelemetrySubsystem>()->RecordEvent(
            TEXT("war_map_closed")
        );
    }

    bWarMapOpen = false;
    bWarMapDeploymentMode = false;

    if (IsValid(WarMapWidget))
    {
        WarMapWidget->OnCloseRequested.RemoveDynamic(
            this,
            &ABHCharacter::CloseWarMap
        );
        WarMapWidget->OnDeployRequested.RemoveDynamic(
            this,
            &ABHCharacter::HandleDeployRequested
        );
        WarMapWidget->OnWithdrawRequested.RemoveDynamic(
            this,
            &ABHCharacter::HandleWithdrawRequested
        );
        WarMapWidget->OnMilitiaRequested.RemoveDynamic(
            this,
            &ABHCharacter::HandleMilitiaRequested
        );
        WarMapWidget->OnGarrisonRedeployRequested.RemoveDynamic(
            this,
            &ABHCharacter::HandleGarrisonRedeployRequested
        );
        WarMapWidget->OnCivilianAidRequested.RemoveDynamic(
            this,
            &ABHCharacter::HandleCivilianAidRequested
        );
        WarMapWidget->RemoveFromParent();
        WarMapWidget = nullptr;
    }

    APlayerController* PlayerController =
        ResolveOwningPlayerController();

    if (IsValid(PlayerController) &&
        !bPauseMenuOpen &&
        !bIsHandlingDeath &&
        !bIsHandlingMissionComplete)
    {
        PlayerController->SetIgnoreMoveInput(false);
        PlayerController->SetIgnoreLookInput(false);
        PlayerController->SetInputMode(FInputModeGameOnly());
        PlayerController->bShowMouseCursor = false;
    }

    UE_LOG(LogTemp, Log, TEXT("Strategic war map closed."));
}

void ABHCharacter::HandleMissionContinueRequested()
{
    if (!bIsHandlingMissionComplete)
    {
        return;
    }

    if (!HasAuthority())
    {
        ServerRequestContinueDebrief();
        return;
    }

    UBHWarSubsystem* WarSubsystem = GetGameInstance()
        ? GetGameInstance()->GetSubsystem<UBHWarSubsystem>()
        : nullptr;
    const bool bResolvedCampaign =
        IsValid(WarSubsystem) &&
        WarSubsystem->IsCampaignResolved();
    const bool bPreviousEpilogueAcknowledgement =
        bCampaignEpilogueAcknowledged;
    const bool bPreviousDebriefAcknowledgement =
        bOperationDebriefAcknowledged;
    bCampaignEpilogueAcknowledged = bResolvedCampaign;
    bOperationDebriefAcknowledged = !bResolvedCampaign;

    UGameInstance* GameInstance = GetGameInstance();
    UBHSaveSubsystem* SaveSubsystem = IsValid(GameInstance)
        ? GameInstance->GetSubsystem<UBHSaveSubsystem>()
        : nullptr;

    if (HasAuthority() &&
        (!IsValid(SaveSubsystem) ||
            !SaveSubsystem->SaveProgress()))
    {
        bCampaignEpilogueAcknowledged =
            bPreviousEpilogueAcknowledgement;
        bOperationDebriefAcknowledged =
            bPreviousDebriefAcknowledgement;
        ShowStatusNotification(
            NSLOCTEXT(
                "BrokenHorizon",
                "OperationResultCheckpointRetryFailed",
                "CHECKPOINT NOT SAVED\n\n"
                "Strategic command remains locked while the game "
                "retries the operation-result checkpoint. Press "
                "CONTINUE again."
            )
        );
        ClientPresentOperationDebrief(
            FText::Format(
                NSLOCTEXT(
                    "BrokenHorizon",
                    "OperationResultCheckpointRetryClient",
                    "{0}\n\n"
                    "CHECKPOINT NOT SAVED // Press CONTINUE to retry."
                ),
                MissionCompleteMessage
            )
        );
        UE_LOG(
            LogTemp,
            Error,
            TEXT(
                "BH_OPERATION_RESULT_CONTINUE_BLOCKED "
                "checkpoint=failed"
            )
        );
        return;
    }

    UE_LOG(
        LogTemp,
        Display,
        TEXT("BH_OPERATION_RESULT_CHECKPOINT_CONFIRMED")
    );

    if (bResolvedCampaign)
    {
        EnterCampaignEpilogueFreeRoam(true);
        ClientConfirmDebriefContinue(true);
        return;
    }

    EnterPostOperationFreeRoam(true);
    ClientConfirmDebriefContinue(false);

    if (!SaveSubsystem->SaveProgress())
    {
        UE_LOG(
            LogTemp,
            Warning,
            TEXT(
                "BH_POST_OPERATION_FREE_ROAM_CHECKPOINT_FAILED "
                "fallback=result_checkpoint"
            )
        );
    }
}

void ABHCharacter::HandleDeployRequested(
    FName SectorID,
    EBHWarPriorityType OperationType
)
{
    if (!bWarMapDeploymentMode ||
        bRuntimeWarOperation ||
        bIsHandlingDeath)
    {
        return;
    }

    if (HasAuthority())
    {
        ExecuteDeployOperationRequest(
            SectorID,
            OperationType
        );
        return;
    }

    ServerRequestDeployOperation(SectorID, OperationType);
}

void ABHCharacter::HandleWithdrawRequested()
{
    if (!bWarMapOpen ||
        bWarMapDeploymentMode ||
        !bRuntimeWarOperation ||
        bIsHandlingDeath ||
        bIsHandlingMissionComplete)
    {
        return;
    }

    if (HasAuthority())
    {
        ExecuteWithdrawOperationRequest();
        return;
    }

    ServerRequestWithdrawOperation();
}

void ABHCharacter::HandleMilitiaRequested(FName SectorID)
{
    if (SectorID.IsNone() || bIsHandlingDeath)
    {
        return;
    }

    if (HasAuthority())
    {
        ExecuteMobilizeMilitiaRequest(SectorID);
        return;
    }

    ServerRequestMobilizeMilitia(SectorID);
}

void ABHCharacter::HandleGarrisonRedeployRequested(
    FName DestinationSectorID
)
{
    if (DestinationSectorID.IsNone() || bIsHandlingDeath)
    {
        return;
    }

    if (HasAuthority())
    {
        ExecuteRedeployGarrisonRequest(DestinationSectorID);
        return;
    }

    ServerRequestRedeployGarrison(DestinationSectorID);
}

void ABHCharacter::HandleCivilianAidRequested(
    FName TargetSectorID,
    EBHWarPriorityType OperationType
)
{
    if (TargetSectorID.IsNone() ||
        OperationType == EBHWarPriorityType::None ||
        bIsHandlingDeath)
    {
        return;
    }

    if (HasAuthority())
    {
        ExecuteCivilianAidRequest(
            TargetSectorID,
            OperationType
        );
        return;
    }

    ServerRequestCivilianAid(TargetSectorID, OperationType);
}

bool ABHCharacter::ExecuteDeployOperationRequest(
    FName SectorID,
    EBHWarPriorityType OperationType
)
{
    if (!HasAuthority() ||
        SectorID.IsNone() ||
        OperationType == EBHWarPriorityType::None ||
        bRuntimeWarOperation ||
        bIsHandlingDeath)
    {
        ClientShowStatusNotification(
            NSLOCTEXT(
                "BrokenHorizon",
                "DeployRequestRejected",
                "DEPLOYMENT BLOCKED\n\n"
                "Command could not authorize this operation."
            )
        );
        return false;
    }

    UBHSaveSubsystem* SaveSubsystem = GetGameInstance()
        ? GetGameInstance()->GetSubsystem<UBHSaveSubsystem>()
        : nullptr;

    if (!IsValid(SaveSubsystem) ||
        !SaveSubsystem->DeployOperationForCharacter(
            this,
            SectorID,
            OperationType
        ))
    {
        ClientShowStatusNotification(
            NSLOCTEXT(
                "BrokenHorizon",
                "DeployNextOperationFailed",
                "DEPLOYMENT FAILED\n\n"
                "Unable to prepare the next operation."
            )
        );
        return false;
    }

    ClientConfirmOperationDeployment(
        AssignedWarSectorID,
        AssignedWarSupplySourceSectorID,
        AssignedWarPriorityType
    );
    ForceNetUpdate();
    return true;
}

bool ABHCharacter::ExecuteWithdrawOperationRequest()
{
    if (!HasAuthority() ||
        !bRuntimeWarOperation ||
        bIsHandlingDeath ||
        bIsHandlingMissionComplete)
    {
        ClientShowStatusNotification(
            NSLOCTEXT(
                "BrokenHorizon",
                "OperationWithdrawalRejected",
                "WITHDRAWAL BLOCKED\n\n"
                "There is no active operation to release."
            )
        );
        return false;
    }

    const EBHWarPriorityType WithdrawnOperationType =
        AssignedWarPriorityType;
    const FText WithdrawalReason =
        WithdrawnOperationType == EBHWarPriorityType::Defend
            ? NSLOCTEXT(
                "BrokenHorizon",
                "DefenseWithdrawalFailureReason",
                "Friendly forces withdrew before the defensive "
                "position was secured."
            )
            : WithdrawnOperationType == EBHWarPriorityType::Raid
            ? NSLOCTEXT(
                "BrokenHorizon",
                "RaidWithdrawalFailureReason",
                "The raid withdrew before enemy logistics were "
                "disrupted."
            )
            : WithdrawnOperationType ==
                    EBHWarPriorityType::Resupply
            ? NSLOCTEXT(
                "BrokenHorizon",
                "ResupplyWithdrawalFailureReason",
                "The logistics team withdrew before the assigned "
                "supply load reached its destination."
            )
            : WithdrawnOperationType ==
                    EBHWarPriorityType::EscortRescue
            ? NSLOCTEXT(
                "BrokenHorizon",
                "EscortWithdrawalFailureReason",
                "The escort withdrew before the protected convoy "
                "cleared the route."
            )
            : WithdrawnOperationType ==
                    EBHWarPriorityType::Rescue
            ? NSLOCTEXT(
                "BrokenHorizon",
                "RescueWithdrawalFailureReason",
                "The rescue team withdrew before the assigned "
                "casualty reached treatment."
            )
            : WithdrawnOperationType ==
                    EBHWarPriorityType::Recon
            ? NSLOCTEXT(
                "BrokenHorizon",
                "ReconWithdrawalFailureReason",
                "The reconnaissance team withdrew before filing a "
                "confirmed intelligence report."
            )
            : NSLOCTEXT(
                "BrokenHorizon",
                "AttackWithdrawalFailureReason",
                "Friendly forces withdrew before the assault "
                "objective was secured."
            );

    if (!FailCurrentWarOperation(WithdrawalReason))
    {
        ClientShowStatusNotification(
            NSLOCTEXT(
                "BrokenHorizon",
                "OperationWithdrawalFailed",
                "WITHDRAWAL FAILED\n\n"
                "The active operation could not be released."
            )
        );
        return false;
    }

    const FName WithdrawnSectorID = AssignedWarSectorID;

    if (IsValid(OpenWorldOperationDirector))
    {
        OpenWorldOperationDirector->Destroy();
        OpenWorldOperationDirector = nullptr;
    }

    UE_LOG(
        LogTemp,
        Warning,
        TEXT(
            "BH_OPERATION_WITHDRAWN sector=%s type=%d"
        ),
        *WithdrawnSectorID.ToString(),
        static_cast<int32>(WithdrawnOperationType)
    );

    ClientPresentOperationDebrief(MissionCompleteMessage);
    ForceNetUpdate();
    return true;
}

bool ABHCharacter::ExecuteMobilizeMilitiaRequest(FName SectorID)
{
    UBHWarSubsystem* WarSubsystem = GetGameInstance()
        ? GetGameInstance()->GetSubsystem<UBHWarSubsystem>()
        : nullptr;

    if (!HasAuthority() ||
        !IsValid(WarSubsystem) ||
        !WarSubsystem->MobilizeSectorMilitia(SectorID))
    {
        ClientShowStatusNotification(
            NSLOCTEXT(
                "BrokenHorizon",
                "MilitiaRequestRejected",
                "MOBILIZATION BLOCKED\n\n"
                "Support, supply, route, or garrison capacity "
                "requirements were not met."
            )
        );
        return false;
    }

    ClientShowStatusNotification(
        NSLOCTEXT(
            "BrokenHorizon",
            "MilitiaRequestAccepted",
            "MOBILIZATION AUTHORIZED\n\n"
            "Local militia are assembling at the selected sector."
        )
    );
    return true;
}

bool ABHCharacter::ExecuteRedeployGarrisonRequest(
    FName DestinationSectorID
)
{
    UBHWarSubsystem* WarSubsystem = GetGameInstance()
        ? GetGameInstance()->GetSubsystem<UBHWarSubsystem>()
        : nullptr;

    if (!HasAuthority() ||
        !IsValid(WarSubsystem) ||
        !WarSubsystem->RedeploySectorGarrison(
            DestinationSectorID
        ))
    {
        ClientShowStatusNotification(
            NSLOCTEXT(
                "BrokenHorizon",
                "GarrisonRedeployRejected",
                "REDEPLOYMENT BLOCKED\n\n"
                "No connected reserve force can complete the move."
            )
        );
        return false;
    }

    ClientShowStatusNotification(
        NSLOCTEXT(
            "BrokenHorizon",
            "GarrisonRedeployAccepted",
            "RESERVES DISPATCHED\n\n"
            "The transfer is now moving through the logistics network."
        )
    );
    return true;
}

bool ABHCharacter::ExecuteCivilianAidRequest(
    FName TargetSectorID,
    EBHWarPriorityType OperationType
)
{
    UBHWarSubsystem* WarSubsystem = GetGameInstance()
        ? GetGameInstance()->GetSubsystem<UBHWarSubsystem>()
        : nullptr;

    if (!HasAuthority() ||
        !IsValid(WarSubsystem) ||
        !WarSubsystem->DeliverCivilianAid(
            TargetSectorID,
            OperationType
        ))
    {
        ClientShowStatusNotification(
            NSLOCTEXT(
                "BrokenHorizon",
                "CivilianAidRejected",
                "AID DISPATCH BLOCKED\n\n"
                "The route, stockpile, or convoy capacity is unavailable."
            )
        );
        return false;
    }

    ClientShowStatusNotification(
        NSLOCTEXT(
            "BrokenHorizon",
            "CivilianAidAccepted",
            "AID CONVOY DISPATCHED\n\n"
            "Civilian support supplies are moving through the world."
        )
    );
    return true;
}

void ABHCharacter::CacheObservedWarState(
    UBHWarSubsystem* WarSubsystem
)
{
    LastObservedSectorOwners.Reset();
    LastObservedSectorLogisticsConnected.Reset();
    LastObservedSectorSupplyReadiness.Reset();

    if (!IsValid(WarSubsystem))
    {
        LastObservedPrioritySectorID = NAME_None;
        LastObservedPriorityType = EBHWarPriorityType::None;
        LastObservedCampaignOutcome =
            EBHWarCampaignOutcome::Ongoing;
        bLastObservedOperationFundingReady = false;
        LastObservedOperationSupplySource = NAME_None;
        LastObservedWarEventTurn = INDEX_NONE;
        LastObservedWarEventType = NAME_None;
        LastObservedWarEventSectorID = NAME_None;
        LastObservedWarEventSummary.Reset();
        return;
    }

    LastObservedPrioritySectorID =
        WarSubsystem->GetPrioritySectorID();
    LastObservedPriorityType =
        WarSubsystem->GetPriorityType();
    LastObservedCampaignOutcome =
        WarSubsystem->GetCampaignOutcome();
    bLastObservedOperationFundingReady =
        WarSubsystem->CanFundPriorityOperation();
    LastObservedOperationSupplySource =
        WarSubsystem->GetPriorityOperationSupplySource();

    const TArray<FBHWarEventRecord> RecentWarEvents =
        WarSubsystem->GetRecentWarEvents();

    if (RecentWarEvents.IsEmpty())
    {
        LastObservedWarEventTurn = INDEX_NONE;
        LastObservedWarEventType = NAME_None;
        LastObservedWarEventSectorID = NAME_None;
        LastObservedWarEventSummary.Reset();
    }
    else
    {
        const FBHWarEventRecord& LatestWarEvent =
            RecentWarEvents.Last();
        LastObservedWarEventTurn = LatestWarEvent.TurnNumber;
        LastObservedWarEventType = LatestWarEvent.EventType;
        LastObservedWarEventSectorID = LatestWarEvent.SectorID;
        LastObservedWarEventSummary = LatestWarEvent.Summary;
    }

    for (const FBHWarSectorState& Sector :
        WarSubsystem->GetSectorStates())
    {
        LastObservedSectorOwners.Add(
            Sector.SectorID,
            Sector.Owner
        );
        LastObservedSectorLogisticsConnected.Add(
            Sector.SectorID,
            WarSubsystem->IsSectorConnectedToFactionLogistics(
                Sector.SectorID
            )
        );
        LastObservedSectorSupplyReadiness.Add(
            Sector.SectorID,
            static_cast<uint8>(
                GetSectorSupplyReadiness(Sector.Supply)
            )
        );
    }
}

void ABHCharacter::HandleWarStateChanged(
    int32 NewTurnNumber,
    FName NewPrioritySectorID,
    EBHWarPriorityType NewPriorityType
)
{
    UBHWarSubsystem* WarSubsystem = GetGameInstance()
        ? GetGameInstance()->GetSubsystem<UBHWarSubsystem>()
        : nullptr;

    if (!IsValid(WarSubsystem))
    {
        return;
    }

    const bool bPreviouslyFunded =
        bLastObservedOperationFundingReady;
    const FName PreviousSupplySourceID =
        LastObservedOperationSupplySource;
    FBHWarEventRecord LatestWarEvent;
    bool bHasNewWarEvent = false;

    const TArray<FBHWarEventRecord> RecentWarEvents =
        WarSubsystem->GetRecentWarEvents();

    if (!RecentWarEvents.IsEmpty())
    {
        LatestWarEvent = RecentWarEvents.Last();
        bHasNewWarEvent =
            LatestWarEvent.TurnNumber !=
                LastObservedWarEventTurn ||
            LatestWarEvent.EventType !=
                LastObservedWarEventType ||
            LatestWarEvent.SectorID !=
                LastObservedWarEventSectorID ||
            LatestWarEvent.Summary !=
                LastObservedWarEventSummary;
    }

    FText StrategicUpdate;
    const EBHWarCampaignOutcome CurrentCampaignOutcome =
        WarSubsystem->GetCampaignOutcome();
    FText CampaignResolutionUpdate;

    if (CurrentCampaignOutcome != LastObservedCampaignOutcome)
    {
        if (CurrentCampaignOutcome ==
            EBHWarCampaignOutcome::FriendlyVictory)
        {
            CampaignResolutionUpdate = NSLOCTEXT(
                "BrokenHorizon",
                "FieldCampaignVictory",
                "CAMPAIGN VICTORY\n\n"
                "All Koronan sectors are under friendly control.\n"
                "The occupation has ended. The open world remains "
                "available for free operations."
            );
        }
        else if (
            CurrentCampaignOutcome ==
            EBHWarCampaignOutcome::EnemyVictory)
        {
            CampaignResolutionUpdate = NSLOCTEXT(
                "BrokenHorizon",
                "FieldCampaignDefeat",
                "CAMPAIGN DEFEAT\n\n"
                "Western command has fallen.\n"
                "The campaign is over. Start a new war from the "
                "main menu when ready."
            );
        }
    }

    for (const FBHWarSectorState& Sector :
        WarSubsystem->GetSectorStates())
    {
        const EBHWarFaction* PreviousOwner =
            LastObservedSectorOwners.Find(Sector.SectorID);

        if (!PreviousOwner || *PreviousOwner == Sector.Owner)
        {
            continue;
        }

        const FText NewOwnerText =
            Sector.Owner == EBHWarFaction::Friendly
                ? NSLOCTEXT(
                    "BrokenHorizon",
                    "WarMapFriendlySectorOwner",
                    "FRIENDLY"
                )
                : Sector.Owner == EBHWarFaction::Enemy
                    ? NSLOCTEXT(
                        "BrokenHorizon",
                        "WarMapEnemySectorOwner",
                        "ENEMY"
                    )
                    : NSLOCTEXT(
                        "BrokenHorizon",
                        "NeutralSectorControl",
                        "CONTESTED"
                    );

        if (WarSubsystem->IsLogisticsHubSector(
                Sector.SectorID
            ))
        {
            const float SupplyFlow =
                WarSubsystem->GetSectorSupplyChangePerTurn(
                    Sector.SectorID
                );

            if (Sector.Owner == EBHWarFaction::Friendly &&
                SupplyFlow > 0.0f)
            {
                StrategicUpdate = FText::Format(
                    NSLOCTEXT(
                        "BrokenHorizon",
                        "LogisticsHubOnline",
                        "LOGISTICS HUB ONLINE\n\n"
                        "{0} is now producing +{1} supply "
                        "per war turn.\nWar turn {2}."
                    ),
                    Sector.DisplayName,
                    FText::AsNumber(SupplyFlow),
                    FText::AsNumber(NewTurnNumber)
                );
            }
            else if (
                Sector.Owner == EBHWarFaction::Friendly)
            {
                StrategicUpdate = FText::Format(
                    NSLOCTEXT(
                        "BrokenHorizon",
                        "LogisticsHubSecured",
                        "LOGISTICS HUB SECURED\n\n"
                        "{0} is under friendly control. "
                        "Secure adjacent sectors to bring "
                        "high-rate supply production online.\n"
                        "War turn {1}."
                    ),
                    Sector.DisplayName,
                    FText::AsNumber(NewTurnNumber)
                );
            }
            else
            {
                StrategicUpdate = FText::Format(
                    NSLOCTEXT(
                        "BrokenHorizon",
                        "LogisticsHubLost",
                        "LOGISTICS HUB LOST\n\n"
                        "{0} is now under {1} control. "
                        "This strategic supply source is "
                        "offline.\nWar turn {2}."
                    ),
                    Sector.DisplayName,
                    NewOwnerText,
                    FText::AsNumber(NewTurnNumber)
                );
            }
        }
        else
        {
            StrategicUpdate = FText::Format(
                NSLOCTEXT(
                    "BrokenHorizon",
                    "FrontlineOwnershipUpdate",
                    "FRONTLINE UPDATE\n\n"
                    "{0} is now under {1} control.\n"
                    "War turn {2}."
                ),
                Sector.DisplayName,
                NewOwnerText,
                FText::AsNumber(NewTurnNumber)
            );
        }

        break;
    }

    if (!CampaignResolutionUpdate.IsEmpty())
    {
        StrategicUpdate = CampaignResolutionUpdate;
    }

    if (StrategicUpdate.IsEmpty())
    {
        for (const FBHWarSectorState& Sector :
            WarSubsystem->GetSectorStates())
        {
            const EBHWarFaction* PreviousOwner =
                LastObservedSectorOwners.Find(Sector.SectorID);
            const bool* bPreviouslyConnected =
                LastObservedSectorLogisticsConnected.Find(
                    Sector.SectorID
                );

            if (Sector.Owner != EBHWarFaction::Friendly ||
                !PreviousOwner ||
                *PreviousOwner != EBHWarFaction::Friendly ||
                !bPreviouslyConnected)
            {
                continue;
            }

            const bool bCurrentlyConnected =
                WarSubsystem
                    ->IsSectorConnectedToFactionLogistics(
                        Sector.SectorID
                    );

            if (*bPreviouslyConnected && !bCurrentlyConnected)
            {
                StrategicUpdate = FText::Format(
                    NSLOCTEXT(
                        "BrokenHorizon",
                        "SectorLogisticsSevered",
                        "LOGISTICS SEVERED\n\n"
                        "{0} is cut off from a friendly "
                        "logistics hub. Reinforcements are "
                        "suspended and local supply is "
                        "draining."
                    ),
                    Sector.DisplayName
                );
            }
            else if (!*bPreviouslyConnected &&
                bCurrentlyConnected)
            {
                StrategicUpdate = FText::Format(
                    NSLOCTEXT(
                        "BrokenHorizon",
                        "SectorLogisticsRestored",
                        "LOGISTICS RESTORED\n\n"
                        "{0} has reconnected to the friendly "
                        "logistics network. Reinforcements "
                        "can resume."
                    ),
                    Sector.DisplayName
                );
            }

            if (!StrategicUpdate.IsEmpty())
            {
                break;
            }
        }
    }

    if (StrategicUpdate.IsEmpty())
    {
        for (const FBHWarSectorState& Sector :
            WarSubsystem->GetSectorStates())
        {
            if (Sector.Owner != EBHWarFaction::Friendly)
            {
                continue;
            }

            const uint8* PreviousReadinessValue =
                LastObservedSectorSupplyReadiness.Find(
                    Sector.SectorID
                );

            if (!PreviousReadinessValue)
            {
                continue;
            }

            const EBHSectorSupplyReadiness
                PreviousReadiness =
                    static_cast<EBHSectorSupplyReadiness>(
                        *PreviousReadinessValue
                    );
            const EBHSectorSupplyReadiness CurrentReadiness =
                GetSectorSupplyReadiness(Sector.Supply);
            const bool bEnteredSupplyCrisis =
                static_cast<uint8>(CurrentReadiness) <
                    static_cast<uint8>(PreviousReadiness) &&
                CurrentReadiness <=
                    EBHSectorSupplyReadiness::Critical;
            const bool bRecoveredFromSupplyCrisis =
                static_cast<uint8>(CurrentReadiness) >
                    static_cast<uint8>(PreviousReadiness) &&
                PreviousReadiness <=
                    EBHSectorSupplyReadiness::Critical &&
                CurrentReadiness >=
                    EBHSectorSupplyReadiness::Stable;

            if (bEnteredSupplyCrisis)
            {
                const FText SupplyImpact =
                    CurrentReadiness ==
                        EBHSectorSupplyReadiness::Starved
                        ? NSLOCTEXT(
                            "BrokenHorizon",
                            "SectorSupplyStarvedImpact",
                            "Reinforcements have halted."
                        )
                        : NSLOCTEXT(
                            "BrokenHorizon",
                            "SectorSupplyCriticalImpact",
                            "Reinforcement capacity is reduced."
                        );
                StrategicUpdate = FText::Format(
                    NSLOCTEXT(
                        "BrokenHorizon",
                        "SectorSupplyCrisis",
                        "SUPPLY CRISIS\n\n"
                        "{0} logistics are {1} at {2}%.\n"
                        "{3}"
                    ),
                    Sector.DisplayName,
                    GetSectorSupplyReadinessText(
                        CurrentReadiness
                    ),
                    FText::AsNumber(
                        FMath::RoundToInt(Sector.Supply)
                    ),
                    SupplyImpact
                );
            }
            else if (bRecoveredFromSupplyCrisis)
            {
                StrategicUpdate = FText::Format(
                    NSLOCTEXT(
                        "BrokenHorizon",
                        "SectorSupplyRecovered",
                        "SUPPLY RECOVERED\n\n"
                        "{0} logistics are {1} at {2}%.\n"
                        "Reinforcement capacity is recovering."
                    ),
                    Sector.DisplayName,
                    GetSectorSupplyReadinessText(
                        CurrentReadiness
                    ),
                    FText::AsNumber(
                        FMath::RoundToInt(Sector.Supply)
                    )
                );
            }

            if (!StrategicUpdate.IsEmpty())
            {
                break;
            }
        }
    }

    const bool bPriorityChanged =
        LastObservedPrioritySectorID != NewPrioritySectorID ||
        LastObservedPriorityType != NewPriorityType;
    const bool bNowFunded =
        WarSubsystem->CanFundPriorityOperation();
    const FName NewSupplySourceID =
        WarSubsystem->GetPriorityOperationSupplySource();
    CacheObservedWarState(WarSubsystem);

    const bool bHasActivePriority =
        !NewPrioritySectorID.IsNone() &&
        NewPriorityType != EBHWarPriorityType::None;

    if (!bPriorityChanged &&
        bHasActivePriority &&
        !bRuntimeWarOperation)
    {
        const FBHWarSectorState PrioritySector =
            WarSubsystem->GetSectorState(
                NewPrioritySectorID
            );
        const FText PriorityName =
            PrioritySector.DisplayName.IsEmpty()
                ? FText::FromName(NewPrioritySectorID)
                : PrioritySector.DisplayName;

        if (bPreviouslyFunded && !bNowFunded)
        {
            StrategicUpdate = FText::Format(
                NSLOCTEXT(
                    "BrokenHorizon",
                    "SupplyRouteSevered",
                    "SUPPLY ROUTE SEVERED\n\n"
                    "{0} can no longer receive operation "
                    "supplies. Reconnect friendly sectors "
                    "or replenish a staging area."
                ),
                PriorityName
            );
        }
        else if (!bPreviouslyFunded && bNowFunded)
        {
            const FBHWarSectorState SourceSector =
                WarSubsystem->GetSectorState(
                    NewSupplySourceID
                );
            const FText SourceName =
                SourceSector.DisplayName.IsEmpty()
                    ? FText::FromName(NewSupplySourceID)
                    : SourceSector.DisplayName;
            StrategicUpdate = FText::Format(
                NSLOCTEXT(
                    "BrokenHorizon",
                    "SupplyRouteRestored",
                    "SUPPLY ROUTE RESTORED\n\n"
                    "{0} can now stage the operation at "
                    "{1}. Deployment requires {2} supply."
                ),
                SourceName,
                PriorityName,
                FText::AsNumber(
                    WarSubsystem
                        ->GetPriorityOperationSupplyCost()
                )
            );
        }
        else if (
            bPreviouslyFunded &&
            bNowFunded &&
            PreviousSupplySourceID != NewSupplySourceID)
        {
            const FBHWarSectorState PreviousSource =
                WarSubsystem->GetSectorState(
                    PreviousSupplySourceID
                );
            const FBHWarSectorState NewSource =
                WarSubsystem->GetSectorState(
                    NewSupplySourceID
                );
            const FText PreviousSourceName =
                PreviousSource.DisplayName.IsEmpty()
                    ? FText::FromName(
                        PreviousSupplySourceID
                    )
                    : PreviousSource.DisplayName;
            const FText NewSourceName =
                NewSource.DisplayName.IsEmpty()
                    ? FText::FromName(
                        NewSupplySourceID
                    )
                    : NewSource.DisplayName;
            StrategicUpdate = FText::Format(
                NSLOCTEXT(
                    "BrokenHorizon",
                    "SupplyRouteUpdated",
                    "SUPPLY ROUTE UPDATED\n\n"
                    "Staging for {0} has shifted from "
                    "{1} to {2}."
                ),
                PriorityName,
                PreviousSourceName,
                NewSourceName
            );
        }
    }

    if (StrategicUpdate.IsEmpty() &&
        bHasNewWarEvent &&
        !LatestWarEvent.Summary.IsEmpty())
    {
        UE_LOG(
            LogTemp,
            Display,
            TEXT(
                "BH_FIELD_CAMPAIGN_UPDATE "
                "turn=%d type=%s sector=%s summary=\"%s\""
            ),
            LatestWarEvent.TurnNumber,
            *LatestWarEvent.EventType.ToString(),
            *LatestWarEvent.SectorID.ToString(),
            *LatestWarEvent.Summary
        );
        StrategicUpdate = FText::Format(
            NSLOCTEXT(
                "BrokenHorizon",
                "CampaignEventFieldUpdate",
                "CAMPAIGN UPDATE\n\n{0}\nWar turn {1}."
            ),
            FText::FromString(LatestWarEvent.Summary),
            FText::AsNumber(LatestWarEvent.TurnNumber)
        );
    }

    if (StrategicUpdate.IsEmpty() &&
        bPriorityChanged &&
        NewPriorityType != EBHWarPriorityType::None)
    {
        StrategicUpdate = FText::Format(
            NSLOCTEXT(
                "BrokenHorizon",
                "StrategicPriorityUpdate",
                "COMMAND UPDATE\n\n{0}\nWar turn {1}."
            ),
            WarSubsystem->GetPriorityText(),
            FText::AsNumber(NewTurnNumber)
        );
    }

    const APlayerController* PlayerController =
        ResolveOwningPlayerController();

    if (!StrategicUpdate.IsEmpty() &&
        IsValid(PlayerController) &&
        PlayerController->GetLocalPlayer() != nullptr &&
        (
            !IsMissionComplete() ||
            CurrentCampaignOutcome !=
                EBHWarCampaignOutcome::Ongoing
        ))
    {
        ShowDeferredStrategicStatusNotification(StrategicUpdate);
    }
}

bool ABHCharacter::BeginNextOperationInWorld()
{
    UGameInstance* GameInstance = GetGameInstance();
    UBHWarSubsystem* WarSubsystem = GameInstance
        ? GameInstance->GetSubsystem<UBHWarSubsystem>()
        : nullptr;

    return IsValid(WarSubsystem) &&
        BeginOperationInWorld(
            WarSubsystem->GetPrioritySectorID(),
            WarSubsystem->GetPriorityType()
        );
}

bool ABHCharacter::BeginOperationInWorld(
    FName SectorID,
    EBHWarPriorityType OperationType
)
{
    UGameInstance* GameInstance = GetGameInstance();
    UBHWarSubsystem* WarSubsystem = GameInstance
        ? GameInstance->GetSubsystem<UBHWarSubsystem>()
        : nullptr;
    const FName RescueTargetID =
        OperationType == EBHWarPriorityType::Rescue
            ? GetFieldSquadRescueTargetID()
            : NAME_None;

    if (!IsValid(WarSubsystem) ||
        bRuntimeWarOperation ||
        WarSubsystem->HasCommittedOperation() ||
        !WarSubsystem->IsViableOperation(
            SectorID,
            OperationType
        ) ||
        !WarSubsystem->CanFundOperation(
            SectorID,
            OperationType
        ) ||
        (OperationType == EBHWarPriorityType::Rescue &&
         RescueTargetID.IsNone()) ||
        !IsValid(ObjectiveComponent))
    {
        return false;
    }

    const bool bPreviousRuntimeWarOperation =
        bRuntimeWarOperation;
    const FName PreviousAssignedWarSectorID =
        AssignedWarSectorID;
    const FName PreviousAssignedWarSupplySourceSectorID =
        AssignedWarSupplySourceSectorID;
    const EBHWarPriorityType PreviousAssignedWarPriorityType =
        AssignedWarPriorityType;
    const bool bRestoreDeploymentMap =
        bWarMapOpen && bWarMapDeploymentMode;
    bool bCommittedThisDeployment = false;

    const auto RollBackDeployment =
        [this,
         WarSubsystem,
         &bCommittedThisDeployment,
         bPreviousRuntimeWarOperation,
         PreviousAssignedWarSectorID,
         PreviousAssignedWarSupplySourceSectorID,
         PreviousAssignedWarPriorityType,
         bRestoreDeploymentMap]()
        {
            if (bCommittedThisDeployment &&
                IsValid(WarSubsystem))
            {
                WarSubsystem->ClearCommittedOperation();
                bCommittedThisDeployment = false;
            }

            if (IsValid(OpenWorldOperationDirector))
            {
                OpenWorldOperationDirector->Destroy();
                OpenWorldOperationDirector = nullptr;
            }

            bRuntimeWarOperation =
                bPreviousRuntimeWarOperation;
            AssignedWarSectorID =
                PreviousAssignedWarSectorID;
            AssignedWarSupplySourceSectorID =
                PreviousAssignedWarSupplySourceSectorID;
            AssignedWarPriorityType =
                PreviousAssignedWarPriorityType;

            if (bRestoreDeploymentMap)
            {
                OpenWarMap(true);
            }

            UE_LOG(
                LogTemp,
                Error,
                TEXT(
                    "Open-world operation deployment rolled back; "
                    "no mission state or strategic supply was committed."
                )
            );
        };

    if (bWarMapOpen)
    {
        CloseWarMap();
    }

    if (IsValid(MissionCompleteWidget))
    {
        MissionCompleteWidget->OnContinueRequested.RemoveDynamic(
            this,
            &ABHCharacter::HandleMissionContinueRequested
        );
        MissionCompleteWidget->RemoveFromParent();
        MissionCompleteWidget = nullptr;
    }

    if (IsValid(DefenseMissionDirector))
    {
        DefenseMissionDirector->Destroy();
        DefenseMissionDirector = nullptr;
    }

    if (IsValid(OpenWorldOperationDirector))
    {
        OpenWorldOperationDirector->Destroy();
        OpenWorldOperationDirector = nullptr;
    }

    bRuntimeWarOperation = true;
    bCampaignEpilogueAcknowledged = false;
    bOperationDebriefAcknowledged = false;
    AssignedWarSectorID = SectorID;
    AssignedWarPriorityType = OperationType;
    AssignedWarSupplySourceSectorID =
        WarSubsystem->GetOperationSupplySource(
            SectorID,
            OperationType
        );

    if (!StartOpenWorldOperationDirector())
    {
        RollBackDeployment();
        return false;
    }

    const bool bOperationCommitted =
        AssignedWarPriorityType == EBHWarPriorityType::Rescue
            ? WarSubsystem->SetCommittedRescueOperation(
                AssignedWarSectorID,
                RescueTargetID
            )
            : WarSubsystem->SetCommittedOperation(
                AssignedWarSectorID,
                AssignedWarPriorityType
            );
    if (!bOperationCommitted)
    {
        RollBackDeployment();
        return false;
    }
    bCommittedThisDeployment = true;

    if (!WarSubsystem->ConsumeOperationSupply(
            SectorID,
            OperationType
        ))
    {
        RollBackDeployment();
        return false;
    }
    bCommittedThisDeployment = false;

    const bool bSupportsTacticalPreparation =
        OperationType == EBHWarPriorityType::Attack ||
        OperationType == EBHWarPriorityType::Defend ||
        OperationType == EBHWarPriorityType::Raid;
    if (bSupportsTacticalPreparation &&
        IsValid(InjuryComponent))
    {
        int32 BonusMedkits = 0;
        int32 BonusFieldDressings = 0;
        BHWarOperationRules::GetTacticalMedicalSupplyGrant(
            WarSubsystem->GetActiveTacticalOption(),
            BonusMedkits,
            BonusFieldDressings
        );
        if (BonusMedkits > 0 || BonusFieldDressings > 0)
        {
            InjuryComponent->AddMedicalSupplies(
                BonusMedkits,
                BonusFieldDressings
            );
            UE_LOG(
                LogTemp,
                Display,
                TEXT(
                    "BH_MEDICAL_PREPARATION_APPLIED "
                    "medkits=%d dressings=%d"
                ),
                BonusMedkits,
                BonusFieldDressings
            );
        }
    }

    FBHObjectiveDefinition OperationObjective;
    OperationObjective.ObjectiveID =
        AssignedWarPriorityType == EBHWarPriorityType::Resupply
            ? BHObjectiveIds::DeliverResupply
            : AssignedWarPriorityType ==
                    EBHWarPriorityType::EscortRescue
                ? BHObjectiveIds::ProtectConvoy
            : AssignedWarPriorityType ==
                    EBHWarPriorityType::Rescue
                ? BHObjectiveIds::EvacuateCasualty
            : AssignedWarPriorityType ==
                    EBHWarPriorityType::Recon
                ? BHObjectiveIds::ObserveSector
            : BHObjectiveIds::EliminateGuard;
    OperationObjective.DisplayText =
        WarSubsystem->GetOperationObjectiveText(
            AssignedWarSectorID,
            AssignedWarPriorityType,
            OperationObjective.ObjectiveID
        );
    ObjectiveComponent->StartRuntimeMission(
        { OperationObjective }
    );
    ConfigureStrategicMissionPresentation();

    bIsHandlingDeath = false;
    bIsHandlingMissionComplete = false;
    GetWorldTimerManager().ClearTimer(RespawnTimerHandle);

    GetCharacterMovement()->SetMovementMode(MOVE_Walking);
    ApplyMovementSpeed();
    RefreshObjectiveWidget();

    if (IsValid(ObjectiveWidget))
    {
        RefreshMissionPresentationVisibility();
    }

    if (IsValid(AmmoHUDWidget))
    {
        AmmoHUDWidget->SetVisibility(ESlateVisibility::Visible);
    }

    if (IsValid(CombatStatusWidget))
    {
        CombatStatusWidget->SetVisibility(ESlateVisibility::Visible);
    }

    APlayerController* PlayerController =
        ResolveOwningPlayerController();

    if (IsValid(PlayerController))
    {
        EnableInput(PlayerController);
        PlayerController->SetIgnoreMoveInput(false);
        PlayerController->SetIgnoreLookInput(false);
        PlayerController->SetInputMode(FInputModeGameOnly());
        PlayerController->bShowMouseCursor = false;
    }

    UE_LOG(
        LogTemp,
        Display,
        TEXT(
            "BH_DEPLOYMENT_FIELD_READINESS "
            "health=%.1f ammo=%d/%d medkits=%d dressings=%d "
            "bleeding=%d arm_injured=%d leg_injured=%d"
        ),
        IsValid(HealthComponent)
            ? HealthComponent->GetCurrentHealth()
            : 0.0f,
        IsValid(WeaponComponent)
            ? WeaponComponent->GetMagazineAmmo()
            : 0,
        IsValid(WeaponComponent)
            ? WeaponComponent->GetReserveAmmo()
            : 0,
        IsValid(InjuryComponent)
            ? InjuryComponent->GetMedkitCount()
            : 0,
        IsValid(InjuryComponent)
            ? InjuryComponent->GetFieldDressingCount()
            : 0,
        IsValid(InjuryComponent) &&
            InjuryComponent->IsBleeding()
                ? 1
                : 0,
        IsValid(InjuryComponent) &&
            InjuryComponent->IsArmInjured()
                ? 1
                : 0,
        IsValid(InjuryComponent) &&
            InjuryComponent->IsLegInjured()
                ? 1
                : 0
    );

    UE_LOG(
        LogTemp,
        Log,
        TEXT(
            "Activated open-world operation %s in sector %s "
            "without reloading the world."
        ),
        AssignedWarPriorityType == EBHWarPriorityType::Defend
            ? TEXT("DEFEND")
            : AssignedWarPriorityType ==
                    EBHWarPriorityType::Resupply
                ? TEXT("RESUPPLY")
            : AssignedWarPriorityType ==
                    EBHWarPriorityType::EscortRescue
                ? TEXT("ESCORT")
            : AssignedWarPriorityType ==
                    EBHWarPriorityType::Rescue
                ? TEXT("RESCUE")
            : AssignedWarPriorityType ==
                    EBHWarPriorityType::Recon
                ? TEXT("RECON")
            : TEXT("ATTACK"),
        *AssignedWarSectorID.ToString()
    );
    return true;
}

bool ABHCharacter::IsWarMapOpen() const
{
    return bWarMapOpen;
}

bool ABHCharacter::IsRuntimeWarOperation() const
{
    return bRuntimeWarOperation;
}

bool ABHCharacter::IsCampaignEpilogueAcknowledged() const
{
    return bCampaignEpilogueAcknowledged;
}

bool ABHCharacter::IsOperationDebriefAcknowledged() const
{
    return bOperationDebriefAcknowledged;
}

bool ABHCharacter::CanCreateFieldAutosave() const
{
    const UWorld* World = GetWorld();
    const bool bRecentlyDamaged =
        IsValid(World) &&
        LastPlayerDamageTimeSeconds > -BIG_NUMBER &&
        World->GetTimeSeconds() -
            LastPlayerDamageTimeSeconds < 20.0f;

    return IsValid(World) &&
        World->IsGameWorld() &&
        !bIsHandlingDeath &&
        !bIsHandlingMissionComplete &&
        !bPauseMenuOpen &&
        !bWarMapOpen &&
        !bIsTraversing &&
        !bWaitingForInitialWorldStreaming &&
        !bRecentlyDamaged &&
        IsValid(HealthComponent) &&
        HealthComponent->GetHealthPercentage() >= 0.5f &&
        (!IsValid(InjuryComponent) ||
         (!InjuryComponent->IsBleeding() &&
          !InjuryComponent->IsMedkitTreatmentActive())) &&
        (!IsValid(WeaponComponent) ||
         (!WeaponComponent->IsFiring() &&
          !WeaponComponent->IsReloading()));
}

void ABHCharacter::RestoreCampaignEpilogueAcknowledgement(
    bool bAcknowledged
)
{
    bCampaignEpilogueAcknowledged = bAcknowledged;

    UBHWarSubsystem* WarSubsystem = GetGameInstance()
        ? GetGameInstance()->GetSubsystem<UBHWarSubsystem>()
        : nullptr;

    if (bCampaignEpilogueAcknowledged &&
        IsValid(WarSubsystem) &&
        WarSubsystem->IsCampaignResolved())
    {
        EnterCampaignEpilogueFreeRoam(false);
    }
}

void ABHCharacter::RestoreOperationDebriefAcknowledgement(
    bool bAcknowledged
)
{
    bOperationDebriefAcknowledged = bAcknowledged;

    if (bOperationDebriefAcknowledged &&
        !bCampaignEpilogueAcknowledged &&
        (
            !bRuntimeWarOperation ||
            (
                IsValid(ObjectiveComponent) &&
                (
                    ObjectiveComponent->IsMissionComplete() ||
                    ObjectiveComponent->IsMissionFailed()
                )
            )
        ))
    {
        EnterPostOperationFreeRoam(false);
    }
}

TArray<FBHObjectiveDefinition>
ABHCharacter::GetRuntimeObjectiveDefinitions() const
{
    return IsValid(ObjectiveComponent)
        ? ObjectiveComponent->GetRuntimeObjectiveDefinitions()
        : TArray<FBHObjectiveDefinition>();
}

FName ABHCharacter::GetAssignedWarSectorID() const
{
    return AssignedWarSectorID;
}

FName
ABHCharacter::GetAssignedWarSupplySourceSectorID() const
{
    return AssignedWarSupplySourceSectorID;
}

EBHWarPriorityType
ABHCharacter::GetAssignedWarPriorityType() const
{
    return AssignedWarPriorityType;
}

FBHOpenWorldOperationState
ABHCharacter::GetOpenWorldOperationState() const
{
    return bRuntimeWarOperation &&
        IsValid(OpenWorldOperationDirector)
        ? OpenWorldOperationDirector->CaptureSaveState()
        : FBHOpenWorldOperationState();
}

bool ABHCharacter::AdoptSharedWarOperationAuthority(
    ABHOpenWorldOperationDirector* OperationDirector,
    FName SectorID,
    FName SupplySourceSectorID,
    EBHWarPriorityType OperationType
)
{
    if (!HasAuthority() ||
        !IsValid(OperationDirector) ||
        SectorID.IsNone() ||
        OperationType == EBHWarPriorityType::None ||
        !IsValid(ObjectiveComponent))
    {
        return false;
    }

    const bool bAssignmentChanged =
        !bRuntimeWarOperation ||
        OpenWorldOperationDirector != OperationDirector ||
        AssignedWarSectorID != SectorID ||
        AssignedWarSupplySourceSectorID != SupplySourceSectorID ||
        AssignedWarPriorityType != OperationType;
    const bool bMissionNeedsInitialization =
        !ObjectiveComponent->IsRuntimeMission() ||
        ObjectiveComponent->IsMissionComplete() ||
        ObjectiveComponent->IsMissionFailed() ||
        ObjectiveComponent->GetCurrentObjectiveID() !=
            BHObjectiveIds::EliminateGuard;

    bRuntimeWarOperation = true;
    AssignedWarSectorID = SectorID;
    AssignedWarSupplySourceSectorID = SupplySourceSectorID;
    AssignedWarPriorityType = OperationType;
    OpenWorldOperationDirector = OperationDirector;

    if (bMissionNeedsInitialization)
    {
        FBHObjectiveDefinition OperationObjective;
        OperationObjective.ObjectiveID =
            BHObjectiveIds::EliminateGuard;
        ObjectiveComponent->StartRuntimeMission(
            {OperationObjective}
        );
    }

    if (bAssignmentChanged || bMissionNeedsInitialization)
    {
        ConfigureStrategicMissionPresentation();

        UE_LOG(
            LogTemp,
            Display,
            TEXT(
                "BH_SHARED_OPERATION_PARTICIPANT_REGISTERED "
                "player=%s sector=%s type=%d"
            ),
            *GetName(),
            *SectorID.ToString(),
            static_cast<int32>(OperationType)
        );
    }

    return true;
}

void ABHCharacter::PresentSharedOperationDebrief(
    const FText& Message
)
{
    if (!HasAuthority() || Message.IsEmpty())
    {
        return;
    }

    MissionCompleteMessage = Message;

    if (!bIsHandlingMissionComplete)
    {
        EnterMissionCompleteState(false);
    }

    ClientPresentOperationDebrief(Message);
    ForceNetUpdate();
}

FText ABHCharacter::GetMissionCompleteMessage() const
{
    return MissionCompleteMessage;
}

bool ABHCharacter::FailCurrentWarOperation(
    const FText& FailureReason
)
{
    if (!bRuntimeWarOperation ||
        bIsHandlingMissionComplete ||
        AssignedWarSectorID.IsNone() ||
        AssignedWarPriorityType == EBHWarPriorityType::None ||
        !IsValid(ObjectiveComponent) ||
        ObjectiveComponent->IsMissionComplete() ||
        ObjectiveComponent->IsMissionFailed() ||
        ObjectiveComponent->GetCurrentObjectiveID().IsNone())
    {
        return false;
    }

    UGameInstance* GameInstance = GetGameInstance();
    UBHWarSubsystem* WarSubsystem = IsValid(GameInstance)
        ? GameInstance->GetSubsystem<UBHWarSubsystem>()
        : nullptr;

    if (!IsValid(WarSubsystem))
    {
        return false;
    }

    const FBHWarSectorState PreviousSector =
        WarSubsystem->GetSectorState(AssignedWarSectorID);
    const FName ResolvedOperationID =
        WarSubsystem->GetCommittedOperationID();
    const FName ResolvedOperationTargetID =
        WarSubsystem->GetCommittedOperationTargetID();
    const int32 FriendlySupportLosses =
        IsValid(OpenWorldOperationDirector)
            ? OpenWorldOperationDirector
                ->GetFriendlySupportCasualties()
            : 0;
    const int32 EnemyLosses =
        IsValid(OpenWorldOperationDirector)
            ? OpenWorldOperationDirector->GetEnemyCasualties()
            : 0;
    const int32 EnemyRouted =
        IsValid(OpenWorldOperationDirector)
            ? OpenWorldOperationDirector->GetEnemyRoutedCount()
            : 0;
    EBHRaidOperationalSignature RaidSignature =
        EBHRaidOperationalSignature::Contested;
    const FName FriendlyLossSectorID =
        !AssignedWarSupplySourceSectorID.IsNone()
            ? AssignedWarSupplySourceSectorID
            : WarSubsystem->GetOperationSupplySource(
                AssignedWarSectorID,
                AssignedWarPriorityType
            );
    const FName EnemyLossSectorID =
        WarSubsystem->GetOperationEnemySource(
            AssignedWarSectorID,
            AssignedWarPriorityType
        );

    if (PreviousSector.SectorID.IsNone())
    {
        return false;
    }

    WarSubsystem->ApplyOperationCasualtyResult(
        AssignedWarSectorID,
        FriendlyLossSectorID,
        EnemyLossSectorID,
        FriendlySupportLosses,
        EnemyLosses
    );

    const bool bWarResultApplied =
        WarSubsystem->ApplyMissionResult(
            AssignedWarSectorID,
            AssignedWarPriorityType,
            false
        );

    if (!bWarResultApplied)
    {
        return false;
    }

    if (EnemyRouted > 0)
    {
        WarSubsystem->ApplyOperationRoutResult(
            EnemyLossSectorID,
            EnemyRouted
        );
    }

    if (AssignedWarPriorityType == EBHWarPriorityType::Raid)
    {
        RaidSignature =
            WarSubsystem->ApplyRaidOperationalSignature(
                AssignedWarSectorID,
                EnemyLosses,
                FriendlySupportLosses,
                false,
                IsValid(OpenWorldOperationDirector) &&
                    OpenWorldOperationDirector
                        ->WasRaidDetectedBeforeSabotage()
            );
    }

    if (!ObjectiveComponent->FailMission())
    {
        return false;
    }

    const FBHWarSectorState UpdatedSector =
        WarSubsystem->GetSectorState(AssignedWarSectorID);
    const FBHOperationAfterActionRecord AfterAction =
        UBHWarSubsystem::BuildAfterActionRecord(
            ResolvedOperationID,
            AssignedWarSectorID,
            AssignedWarPriorityType,
            false,
            FriendlySupportLosses,
            EnemyLosses,
            EnemyRouted,
            UpdatedSector.Supply - PreviousSector.Supply,
            0.0f,
            WarSubsystem->GetActiveTacticalOption(),
            WarSubsystem->GetActiveTacticalOptionSupplyCost()
        );
    WarSubsystem->RecordOperationAfterAction(AfterAction);
    const TCHAR* AfterActionGrade =
        AfterAction.Grade == EBHAfterActionGrade::Exceptional
            ? TEXT("EXCEPTIONAL")
            : AfterAction.Grade == EBHAfterActionGrade::Strong
                ? TEXT("STRONG")
                : AfterAction.Grade == EBHAfterActionGrade::Effective
                    ? TEXT("EFFECTIVE")
                    : TEXT("DIMINISHED");
    const TCHAR* TacticalPlanName =
        AfterAction.TacticalOption ==
                EBHOperationTacticalOption::ReconPlanning
            ? TEXT("RECON")
            : AfterAction.TacticalOption ==
                    EBHOperationTacticalOption::ReinforcementPriority
                ? TEXT("REINFORCEMENT")
                : AfterAction.TacticalOption ==
                        EBHOperationTacticalOption::MedicalPreparation
                    ? TEXT("MEDICAL")
                    : TEXT("STANDARD");
    const FText OperationLabel =
        AssignedWarPriorityType == EBHWarPriorityType::Defend
            ? NSLOCTEXT(
                "BrokenHorizon",
                "WarDefenseFailure",
                "DEFENSE"
            )
            : AssignedWarPriorityType == EBHWarPriorityType::Raid
            ? NSLOCTEXT(
                "BrokenHorizon",
                "WarRaidFailure",
                "RAID"
            )
            : AssignedWarPriorityType ==
                    EBHWarPriorityType::Resupply
            ? NSLOCTEXT(
                "BrokenHorizon",
                "WarResupplyFailure",
                "RESUPPLY"
            )
            : AssignedWarPriorityType ==
                    EBHWarPriorityType::EscortRescue
            ? NSLOCTEXT(
                "BrokenHorizon",
                "WarEscortFailure",
                "ESCORT"
            )
            : AssignedWarPriorityType ==
                    EBHWarPriorityType::Rescue
            ? NSLOCTEXT(
                "BrokenHorizon",
                "WarRescueFailure",
                "RESCUE"
            )
            : AssignedWarPriorityType ==
                    EBHWarPriorityType::Recon
            ? NSLOCTEXT(
                "BrokenHorizon",
                "WarReconFailure",
                "RECON"
            )
            : NSLOCTEXT(
                "BrokenHorizon",
                "WarAttackFailure",
                "ATTACK"
            );
    const FText RaidSignatureLabel =
        RaidSignature == EBHRaidOperationalSignature::Clean
            ? NSLOCTEXT(
                "BrokenHorizon",
                "FailedCleanRaidSignatureDebrief",
                "CLEAN"
            )
            : RaidSignature ==
                EBHRaidOperationalSignature::Loud
                ? NSLOCTEXT(
                    "BrokenHorizon",
                    "FailedLoudRaidSignatureDebrief",
                    "LOUD"
                )
                : NSLOCTEXT(
                    "BrokenHorizon",
                    "FailedContestedRaidSignatureDebrief",
                    "CONTESTED"
                );
    const FText OutcomeLabel =
        AssignedWarPriorityType == EBHWarPriorityType::Defend
            ? NSLOCTEXT(
                "BrokenHorizon",
                "WarSectorLostDebrief",
                "SECTOR LOST"
            )
            : AssignedWarPriorityType == EBHWarPriorityType::Raid
            ? FText::Format(
                NSLOCTEXT(
                    "BrokenHorizon",
                    "WarRaidRepelledDebrief",
                    "RAID REPULSED // {0} SIGNATURE"
                ),
                RaidSignatureLabel
            )
            : AssignedWarPriorityType ==
                    EBHWarPriorityType::Resupply
            ? NSLOCTEXT(
                "BrokenHorizon",
                "WarResupplyFailedDebrief",
                "SUPPLY LINE NOT RESTORED"
            )
            : AssignedWarPriorityType ==
                    EBHWarPriorityType::EscortRescue
            ? NSLOCTEXT(
                "BrokenHorizon",
                "WarEscortFailedDebrief",
                "PROTECTED CONVOY LOST"
            )
            : AssignedWarPriorityType ==
                    EBHWarPriorityType::Rescue
            ? ResolvedOperationTargetID.IsNone()
                ? NSLOCTEXT(
                    "BrokenHorizon",
                    "WarRescueFailedDebrief",
                    "CASUALTY NOT EVACUATED"
                )
                : FText::Format(
                    NSLOCTEXT(
                        "BrokenHorizon",
                        "WarRescueFailedAssignedDebrief",
                        "CASUALTY {0} NOT EVACUATED"
                    ),
                    FText::FromName(ResolvedOperationTargetID)
                )
            : AssignedWarPriorityType ==
                    EBHWarPriorityType::Recon
            ? NSLOCTEXT(
                "BrokenHorizon",
                "WarReconFailedDebrief",
                "INTELLIGENCE UNCONFIRMED"
            )
            : NSLOCTEXT(
                "BrokenHorizon",
                "WarAssaultRepelledDebrief",
                "ASSAULT REPELLED"
            );
    const FText ResolvedFailureReason =
        FailureReason.IsEmpty()
            ? NSLOCTEXT(
                "BrokenHorizon",
                "WarOperationFailureDefaultReason",
                "Friendly forces could not hold the objective."
            )
            : FailureReason;

    FFormatNamedArguments DebriefArguments;
    DebriefArguments.Add(TEXT("OperationType"), OperationLabel);
    DebriefArguments.Add(
        TEXT("SectorName"),
        PreviousSector.DisplayName
    );
    DebriefArguments.Add(TEXT("Outcome"), OutcomeLabel);
    DebriefArguments.Add(
        TEXT("Reason"),
        ResolvedFailureReason
    );
    DebriefArguments.Add(
        TEXT("FriendlyStrength"),
        FText::AsNumber(
            FMath::RoundToInt(UpdatedSector.FriendlyStrength)
        )
    );
    DebriefArguments.Add(
        TEXT("EnemyStrength"),
        FText::AsNumber(
            FMath::RoundToInt(UpdatedSector.EnemyStrength)
        )
    );
    DebriefArguments.Add(
        TEXT("SectorSupply"),
        FText::AsNumber(
            FMath::RoundToInt(UpdatedSector.Supply)
        )
    );
    DebriefArguments.Add(
        TEXT("EnemyResponse"),
        WarSubsystem->GetSectorEnemyResponseSummary(
            AssignedWarSectorID
        )
    );
    DebriefArguments.Add(
        TEXT("SupportLosses"),
        FText::AsNumber(FriendlySupportLosses)
    );
    DebriefArguments.Add(
        TEXT("HostileLosses"),
        FText::AsNumber(EnemyLosses)
    );
    DebriefArguments.Add(
        TEXT("HostileRouted"),
        FText::AsNumber(EnemyRouted)
    );
    DebriefArguments.Add(
        TEXT("WarTurn"),
        FText::AsNumber(WarSubsystem->GetTurnNumber())
    );
    DebriefArguments.Add(
        TEXT("FriendlyControl"),
        FText::AsNumber(
            FMath::RoundToInt(
                WarSubsystem->GetFriendlyControlPercentage()
            )
        )
    );
    DebriefArguments.Add(
        TEXT("AfterActionGrade"),
        FText::FromString(AfterActionGrade)
    );
    DebriefArguments.Add(
        TEXT("AfterActionScore"),
        FText::AsNumber(AfterAction.TotalScore)
    );
    DebriefArguments.Add(
        TEXT("CampaignMerit"),
        FText::AsNumber(
            WarSubsystem->GetCampaignProgression().CampaignMerit
        )
    );
    DebriefArguments.Add(
        TEXT("TacticalPlan"),
        FText::FromString(TacticalPlanName)
    );
    DebriefArguments.Add(
        TEXT("TacticalCost"),
        FText::AsNumber(
            FMath::RoundToInt(AfterAction.TacticalSupplyCost)
        )
    );
    DebriefArguments.Add(
        TEXT("TacticalScore"),
        FText::AsNumber(AfterAction.TacticalExecutionScore)
    );

    MissionCompleteMessage = FText::Format(
        NSLOCTEXT(
            "BrokenHorizon",
            "WarMissionFailedMessage",
            "MISSION FAILED\n\n"
            "{OperationType} FAILED // {SectorName} // {Outcome}\n"
            "{Reason}\n"
            "SECTOR FORCE // FRIENDLY {FriendlyStrength} // "
            "ENEMY {EnemyStrength} // SUPPLY {SectorSupply}%\n"
            "{EnemyResponse}\n"
            "SUPPORT LOSSES {SupportLosses} // "
            "HOSTILE LOSSES {HostileLosses} // "
            "ROUTED {HostileRouted}\n"
            "AFTER ACTION // {AfterActionGrade} // "
            "SCORE +{AfterActionScore} // MERIT {CampaignMerit}\n"
            "TACTICAL PLAN // {TacticalPlan} // COST {TacticalCost} "
            "// EFFECT +{TacticalScore}\n"
            "WAR TURN {WarTurn} // "
            "FRIENDLY CONTROL {FriendlyControl}%"
        ),
        DebriefArguments
    );

    UE_LOG(
        LogTemp,
        Warning,
        TEXT(
            "BH_OPERATION_FAILED sector=%s type=%d "
            "turn=%d support_losses=%d hostile_losses=%d "
            "hostile_routed=%d"
        ),
        *AssignedWarSectorID.ToString(),
        static_cast<int32>(AssignedWarPriorityType),
        WarSubsystem->GetTurnNumber(),
        FriendlySupportLosses,
        EnemyLosses,
        EnemyRouted
    );

    RefreshObjectiveWidget();
    EnterMissionCompleteState(true);
    return true;
}

bool ABHCharacter::RestartCheckpoint()
{
    if (bWarMapOpen)
    {
        CloseWarMap();
    }

    if (bPauseMenuOpen)
    {
        ResumeFromPause();
    }

    UBHSaveSubsystem* SaveSubsystem = GetGameInstance()
        ? GetGameInstance()->GetSubsystem<UBHSaveSubsystem>()
        : nullptr;

    if (IsValid(SaveSubsystem) &&
        SaveSubsystem->HasValidSaveGame())
    {
        return SaveSubsystem->LoadProgress();
    }

    UWorld* World = GetWorld();

    if (!IsValid(World))
    {
        return false;
    }

    const FName CurrentLevelName(
        *UGameplayStatics::GetCurrentLevelName(World, true)
    );
    UGameplayStatics::OpenLevel(World, CurrentLevelName);
    return true;
}

bool ABHCharacter::ReturnToMainMenu()
{
    const UBHGameShellSettings* ShellSettings =
        GetDefault<UBHGameShellSettings>();

    if (!IsValid(ShellSettings) ||
        ShellSettings->MainMenuMap.IsNull())
    {
        return false;
    }

    const FString PackageName =
        ShellSettings->MainMenuMap
            .ToSoftObjectPath()
            .GetLongPackageName();

    if (PackageName.IsEmpty())
    {
        return false;
    }

    UGameInstance* GameInstance = GetGameInstance();
    UBHSaveSubsystem* SaveSubsystem = IsValid(GameInstance)
        ? GameInstance->GetSubsystem<UBHSaveSubsystem>()
        : nullptr;

    if (HasAuthority() &&
        (!IsValid(SaveSubsystem) ||
            !SaveSubsystem->SaveProgress()))
    {
        UE_LOG(
            LogTemp,
            Error,
            TEXT(
                "Return to command aborted because campaign "
                "progress could not be saved."
            )
        );
        return false;
    }

    if (bWarMapOpen)
    {
        CloseWarMap();
    }

    if (bPauseMenuOpen)
    {
        ResumeFromPause();
    }

    UBHSessionSubsystem* SessionSubsystem = IsValid(GameInstance)
        ? GameInstance->GetSubsystem<UBHSessionSubsystem>()
        : nullptr;

    if (IsValid(SessionSubsystem))
    {
        return SessionSubsystem->LeaveSession();
    }

    UGameplayStatics::OpenLevel(this, FName(*PackageName));
    return true;
}

UBHHealthComponent* ABHCharacter::GetHealthComponent() const
{
    return HealthComponent;
}

bool ABHCharacter::IsPlayerIncapacitated() const
{
    return bPlayerIncapacitated;
}

bool ABHCharacter::IsPlayerCasualtyStabilized() const
{
    return bPlayerIncapacitated && bPlayerCasualtyStabilized;
}

float ABHCharacter::GetPlayerBleedOutSecondsRemaining() const
{
    if (!bPlayerIncapacitated || bPlayerCasualtyStabilized ||
        !IsValid(GetWorld()))
    {
        return 0.0f;
    }
    return FMath::Max(
        0.0f,
        PlayerBleedOutDeadline - GetWorld()->GetTimeSeconds()
    );
}

bool ABHCharacter::CanEnterCooperativeCasualty(
    bool bMultiplayerWorld,
    bool bAlreadyIncapacitated,
    bool bAlreadyDownedThisLife
)
{
    return bMultiplayerWorld &&
        !bAlreadyIncapacitated &&
        !bAlreadyDownedThisLife;
}

void ABHCharacter::ApplyRapidOperationRedeployment()
{
    if (!IsValid(OpenWorldOperationDirector))
    {
        return;
    }

    const FVector OperationCenter =
        OpenWorldOperationDirector->GetOperationCenter();
    const FVector TravelDirection =
        (OperationCenter - GetActorLocation()).GetSafeNormal2D();
    FVector InsertionLocation =
        OperationCenter -
        (TravelDirection.IsNearlyZero()
            ? GetActorForwardVector().GetSafeNormal2D()
            : TravelDirection) * 20000.0f;
    FRotator InsertionRotation =
        (OperationCenter - InsertionLocation).Rotation();

    if (GetWorld() &&
        GetWorld()->FindTeleportSpot(
            this,
            InsertionLocation,
            InsertionRotation))
    {
        SetActorLocationAndRotation(
            InsertionLocation,
            InsertionRotation,
            false,
            nullptr,
            ETeleportType::TeleportPhysics
        );
        ShowStatusNotification(NSLOCTEXT(
            "BrokenHorizon",
            "RapidOperationRedeployment",
            "RAPID REDEPLOYMENT // 200m from active operation"
        ));
        UE_LOG(
            LogTemp,
            Display,
            TEXT("BH_OPERATION_RAPID_REDEPLOYMENT center=%s"),
            *OperationCenter.ToCompactString()
        );
    }
}

void ABHCharacter::PrepareDeploymentModeForTest()
{
    if (!FParse::Param(
            FCommandLine::Get(),
            TEXT("BHTestBeginCommittedOperation")
        ) &&
        !FParse::Param(
            FCommandLine::Get(),
            TEXT("BHTestDefenseAGarrisonPersistence")
        ))
    {
        return;
    }

    bWarMapOpen = true;
    bWarMapDeploymentMode = true;
    UE_LOG(
        LogTemp,
        Display,
        TEXT("BH_TEST_DEPLOYMENT_MODE_PREPARED result=success")
    );
}

bool ABHCharacter::ShouldEscalateFriendlyFire(
    int32 RecentFriendlyHits,
    int32 EscalationThreshold
)
{
    return FMath::Max(0, RecentFriendlyHits) >=
        FMath::Max(1, EscalationThreshold);
}

bool ABHCharacter::CanBeginCasualtyDrag(
    bool bTargetIsPlayer,
    bool bTargetIsIncapacitated,
    bool bTargetIsFriendly,
    bool bAlreadyClaimed,
    float DistanceCentimeters,
    float MaximumDistanceCentimeters
)
{
    return bTargetIsPlayer &&
        bTargetIsIncapacitated &&
        bTargetIsFriendly &&
        !bAlreadyClaimed &&
        DistanceCentimeters >= 0.0f &&
        DistanceCentimeters <= FMath::Max(
            0.0f,
            MaximumDistanceCentimeters
        );
}

bool ABHCharacter::CanPerformWeaponBash(
    bool bSprinting,
    bool bTraversing,
    bool bHandlingDeath,
    bool bDraggingCasualty,
    float CurrentStamina,
    float RequiredStamina,
    float CurrentTime,
    float LastBashTime
)
{
    return !bSprinting &&
        !bTraversing &&
        !bHandlingDeath &&
        !bDraggingCasualty &&
        CurrentStamina >= FMath::Max(0.0f, RequiredStamina) &&
        CurrentTime >= LastBashTime;
}

bool ABHCharacter::CanPerformFieldObservation(
    bool bSprinting,
    bool bTraversing,
    bool bAiming,
    bool bHandlingDeath,
    bool bInVehicle,
    float HorizontalSpeed,
    float MaximumStableSpeed,
    float CurrentTime,
    float LastReportTime
)
{
    return bAiming &&
        !bSprinting &&
        !bTraversing &&
        !bHandlingDeath &&
        !bInVehicle &&
        HorizontalSpeed >= 0.0f &&
        HorizontalSpeed <= FMath::Max(0.0f, MaximumStableSpeed) &&
        CurrentTime >= LastReportTime;
}

float ABHCharacter::CalculateBlastConcussionIntensity(
    float DistanceCentimeters,
    float InnerRadiusCentimeters,
    float OuterRadiusCentimeters,
    bool bHardCover,
    float HardCoverAttenuation
)
{
    const float SafeDistance = FMath::Max(0.0f, DistanceCentimeters);
    const float SafeInnerRadius = FMath::Max(0.0f, InnerRadiusCentimeters);
    const float SafeOuterRadius = FMath::Max(
        SafeInnerRadius,
        OuterRadiusCentimeters
    );
    float Intensity = 0.0f;
    if (SafeDistance <= SafeInnerRadius)
    {
        Intensity = 1.0f;
    }
    else if (SafeDistance < SafeOuterRadius)
    {
        Intensity = 1.0f - FMath::Clamp(
            (SafeDistance - SafeInnerRadius) /
                FMath::Max(1.0f, SafeOuterRadius - SafeInnerRadius),
            0.0f,
            1.0f
        );
    }

    if (bHardCover)
    {
        Intensity *= FMath::Clamp(
            HardCoverAttenuation,
            0.0f,
            1.0f
        );
    }
    return FMath::Clamp(Intensity, 0.0f, 1.0f);
}

bool ABHCharacter::CanStartControlledBreathing(
    bool bAiming,
    bool bSprinting,
    bool bAlreadyHolding,
    float CurrentStamina,
    float MinimumStamina,
    float RecoveryRemaining
)
{
    return bAiming &&
        !bSprinting &&
        !bAlreadyHolding &&
        CurrentStamina >= FMath::Max(0.0f, MinimumStamina) &&
        RecoveryRemaining <= 0.0f;
}

float ABHCharacter::CalculateControlledBreathSpreadMultiplier(
    bool bHolding,
    float HeldDuration,
    float MaximumHeldDuration,
    float MinimumMultiplier,
    float MaximumMultiplier
)
{
    if (!bHolding)
    {
        return 1.0f;
    }

    const float SafeMaximumDuration = FMath::Max(
        0.01f,
        MaximumHeldDuration
    );
    const float Alpha = FMath::Clamp(
        FMath::Max(0.0f, HeldDuration) / SafeMaximumDuration,
        0.0f,
        1.0f
    );
    const float StableWindowAlpha = 0.25f;
    const float StrainAlpha = Alpha <= StableWindowAlpha
        ? 0.0f
        : (Alpha - StableWindowAlpha) /
            (1.0f - StableWindowAlpha);
    return FMath::Lerp(
        FMath::Clamp(MinimumMultiplier, 0.1f, 1.0f),
        FMath::Max(1.0f, MaximumMultiplier),
        FMath::Clamp(StrainAlpha, 0.0f, 1.0f)
    );
}

bool ABHCharacter::CanBraceWeapon(
    bool bAiming,
    bool bSprinting,
    bool bTraversing,
    float HorizontalSpeed,
    float MaximumStableSpeed,
    float DistanceToSupport,
    float MaximumSupportDistance,
    float SupportAlignment,
    float MinimumAlignment,
    bool bSupportUsable,
    float SupportIntegrity
)
{
    return bAiming &&
        !bSprinting &&
        !bTraversing &&
        HorizontalSpeed >= 0.0f &&
        HorizontalSpeed <= FMath::Max(0.0f, MaximumStableSpeed) &&
        DistanceToSupport >= 0.0f &&
        DistanceToSupport <= FMath::Max(0.0f, MaximumSupportDistance) &&
        SupportAlignment >= FMath::Clamp(MinimumAlignment, 0.0f, 1.0f) &&
        bSupportUsable &&
        SupportIntegrity > 0.0f;
}

float ABHCharacter::CalculateWeaponBraceMultiplier(
    bool bBraced,
    float SupportQuality,
    float MinimumMultiplier
)
{
    if (!bBraced)
    {
        return 1.0f;
    }

    return FMath::Lerp(
        1.0f,
        FMath::Clamp(MinimumMultiplier, 0.1f, 1.0f),
        FMath::Clamp(SupportQuality, 0.0f, 1.0f)
    );
}

void ABHCharacter::SetControlledBreathingRequested(bool bRequested)
{
    if (!HasAuthority())
    {
        ServerSetControlledBreathingRequested(bRequested);
        return;
    }

    const bool bAiming = IsValid(WeaponComponent) &&
        WeaponComponent->IsAiming();
    const bool bWasHolding = bHoldingControlledBreath;
    if (!bRequested)
    {
        bHoldingControlledBreath = false;
        ControlledBreathHeldDuration = 0.0f;
        if (bWasHolding)
        {
            ControlledBreathRecoveryRemaining = FMath::Max(
                0.0f,
                ControlledBreathRecoveryDuration
            );
        }
    }
    else if (CanStartControlledBreathing(
        bAiming,
        bIsSprinting,
        bHoldingControlledBreath,
        CurrentStamina,
        20.0f,
        ControlledBreathRecoveryRemaining
    ))
    {
        bHoldingControlledBreath = true;
        ControlledBreathHeldDuration = 0.0f;
    }

    if (IsValid(CombatStatusWidget))
    {
        CombatStatusWidget->SetControlledBreathing(
            bHoldingControlledBreath
        );
    }
    ForceNetUpdate();
}

void ABHCharacter::ServerSetControlledBreathingRequested_Implementation(
    bool bRequested
)
{
    SetControlledBreathingRequested(bRequested);
}

void ABHCharacter::OnRep_ControlledBreathing()
{
    if (!bHoldingControlledBreath)
    {
        ControlledBreathHeldDuration = 0.0f;
    }
    if (IsValid(CombatStatusWidget))
    {
        CombatStatusWidget->SetControlledBreathing(
            bHoldingControlledBreath
        );
    }
}

void ABHCharacter::OnRep_CurrentStamina()
{
    OnStaminaChanged.Broadcast(CurrentStamina, MaxStamina);

    if (IsValid(CombatStatusWidget))
    {
        CombatStatusWidget->SetStamina(CurrentStamina, MaxStamina);
    }
}

UBHInjuryComponent* ABHCharacter::GetInjuryComponent() const
{
    return InjuryComponent;
}

float ABHCharacter::CalculateIncomingBallisticDamage(
    const FHitResult& HitResult,
    float RawDamage,
    EBHPlayerHitZone& OutHitZone
) const
{
    if (IsValid(InjuryComponent))
    {
        return InjuryComponent->CalculateDamageForHit(
            HitResult,
            RawDamage,
            OutHitZone
        );
    }

    OutHitZone = EBHPlayerHitZone::Torso;
    return FMath::Max(0.0f, RawDamage);
}

void ABHCharacter::RegisterIncomingBallisticHit(
    EBHPlayerHitZone HitZone,
    float DamageApplied,
    AActor* DamageCauser
)
{
    if (IsValid(InjuryComponent))
    {
        InjuryComponent->RegisterBallisticHit(
            HitZone,
            DamageApplied,
            DamageCauser
        );
    }
}

float ABHCharacter::GetWeaponSpreadMultiplier() const
{
    const float InjuryMultiplier = IsValid(InjuryComponent)
        ? InjuryComponent->GetWeaponSpreadMultiplier()
        : 1.0f;
    const float LeanMultiplier = FMath::Lerp(
        1.0f,
        FMath::Max(1.0f, LeanWeaponSpreadMultiplier),
        FMath::Abs(CurrentLeanAmount)
    );
    const float HorizontalSpeed = GetVelocity().Size2D();
    const float StableVelocityThreshold =
        FMath::Max(0.0f, StableWeaponVelocityThreshold);
    const float MovementAlpha = FMath::GetMappedRangeValueClamped(
        FVector2D(
            StableVelocityThreshold,
            FMath::Max(
                StableVelocityThreshold + 1.0f,
                WalkSpeed
            )
        ),
        FVector2D(0.0f, 1.0f),
        HorizontalSpeed
    );
    const float MovementMultiplier = FMath::Lerp(
        1.0f,
        FMath::Max(1.0f, MovingWeaponSpreadMultiplier),
        MovementAlpha
    );
    const bool bStableStance =
        HorizontalSpeed <= StableVelocityThreshold;
    const bool bStableProne =
        bIsProne &&
        HorizontalSpeed <=
            FMath::Max(
                StableVelocityThreshold,
                ProneStableVelocityThreshold
            );
    const float StanceMultiplier = bStableProne
        ? FMath::Clamp(
            ProneStationarySpreadMultiplier,
            0.1f,
            1.0f
        )
        : (
            bIsCrouched && bStableStance
            ? FMath::Clamp(
                CrouchedStationarySpreadMultiplier,
                0.1f,
                1.0f
            )
            : 1.0f
        );
    const float StaminaFraction = MaxStamina > KINDA_SMALL_NUMBER
        ? FMath::Clamp(CurrentStamina / MaxStamina, 0.0f, 1.0f)
        : 1.0f;
    const float ExhaustionAlpha = 1.0f - FMath::Clamp(
        StaminaFraction / 0.5f,
        0.0f,
        1.0f
    );
    const float ExhaustionMultiplier = FMath::Lerp(
        1.0f,
        FMath::Max(1.0f, ExhaustedWeaponSpreadMultiplier),
        ExhaustionAlpha
    );
    const float SuppressionMultiplier = FMath::Lerp(
        1.0f,
        FMath::Max(1.0f, MaximumSuppressionSpreadMultiplier),
        FMath::Clamp(
            AuthoritativePlayerSuppression,
            0.0f,
            1.0f
        )
    );
    const float ControlledBreathMultiplier =
        CalculateControlledBreathSpreadMultiplier(
            bHoldingControlledBreath,
            ControlledBreathHeldDuration,
            ControlledBreathMaximumDuration,
            ControlledBreathMinimumSpreadMultiplier,
            ControlledBreathMaximumStrainMultiplier
        );
    const float WeaponBraceMultiplier = CalculateWeaponBraceMultiplier(
        bWeaponBraced,
        WeaponBraceSupportQuality,
        MinimumWeaponBraceSpreadMultiplier
    );

    return InjuryMultiplier *
        LeanMultiplier *
        MovementMultiplier *
        StanceMultiplier *
        ExhaustionMultiplier *
        SuppressionMultiplier *
        ControlledBreathMultiplier *
        WeaponBraceMultiplier *
        UBHBattlefieldConditions::GetCurrentProfile(this).
            WeaponSpreadMultiplier *
        GetCarryLoadProfile().WeaponSpreadMultiplier;
}

FBHCarryLoadProfile ABHCharacter::GetCarryLoadProfile() const
{
    FBHCarryLoadProfile Profile = UBHLoadoutWeight::BuildCarryLoadProfile(
        IsValid(WeaponComponent)
            ? WeaponComponent->GetWeaponRole() : EBHWeaponRole::Assault,
        IsValid(WeaponComponent) ? WeaponComponent->GetMagazineAmmo() : 0,
        IsValid(WeaponComponent) ? WeaponComponent->GetReserveAmmo() : 0,
        FragGrenadeCount,
        SmokeGrenadeCount,
        EngineeringChargeCount,
        IsValid(InjuryComponent) ? InjuryComponent->GetMedkitCount() : 0,
        IsValid(InjuryComponent)
            ? InjuryComponent->GetFieldDressingCount() : 0
    );
    Profile.TotalKilograms += GetAntiVehicleRoundCount() * 0.35f;
    return Profile;
}

int32 ABHCharacter::CalculateSupplyShareAmount(
    int32 DonorCount,
    int32 DonorEmergencyReserve,
    int32 ReceiverCount,
    int32 ReceiverCapacity,
    int32 TransferBatch
)
{
    return FMath::Max(
        0,
        FMath::Min3(
            FMath::Max(0, DonorCount - DonorEmergencyReserve),
            FMath::Max(0, ReceiverCapacity - ReceiverCount),
            FMath::Max(0, TransferBatch)
        )
    );
}

bool ABHCharacter::TryShareFieldSuppliesWith(ABHCharacter* Teammate)
{
    if (!HasAuthority() || !IsValid(Teammate) || Teammate == this ||
        !Teammate->IsPlayerControlled() || IsPlayerIncapacitated() ||
        Teammate->IsPlayerIncapacitated() ||
        FVector::DistSquared(GetActorLocation(), Teammate->GetActorLocation()) >
            FMath::Square(FMath::Max(100.0f, InteractionDistance + 50.0f)))
    {
        return false;
    }

    int32 AmmoShared = 0;
    int32 FragsShared = 0;
    int32 ChargesShared = 0;
    int32 MedkitsShared = 0;
    int32 DressingsShared = 0;
    UBHWeaponComponent* ReceiverWeapon = Teammate->GetWeaponComponent();
    if (IsValid(WeaponComponent) && IsValid(ReceiverWeapon) &&
        WeaponComponent->GetWeaponRole() == ReceiverWeapon->GetWeaponRole())
    {
        AmmoShared = CalculateSupplyShareAmount(
            WeaponComponent->GetReserveAmmo(),
            30,
            ReceiverWeapon->GetReserveAmmo(),
            ReceiverWeapon->GetMaxReserveAmmo(),
            30
        );
        if (AmmoShared > 0)
        {
            AmmoShared = WeaponComponent->RemoveReserveAmmo(AmmoShared);
            const int32 Accepted = ReceiverWeapon->AddReserveAmmo(AmmoShared);
            if (Accepted < AmmoShared)
            {
                WeaponComponent->AddReserveAmmo(AmmoShared - Accepted);
                AmmoShared = Accepted;
            }
        }
    }

    FragsShared = CalculateSupplyShareAmount(
        FragGrenadeCount, 1, Teammate->FragGrenadeCount,
        Teammate->GetMaxFragGrenades(), 1);
    if (FragsShared > 0)
    {
        FragGrenadeCount -= FragsShared;
        Teammate->FragGrenadeCount += FragsShared;
        OnRep_FragInventory();
        Teammate->OnRep_FragInventory();
    }

    ChargesShared = CalculateSupplyShareAmount(
        EngineeringChargeCount, 1, Teammate->EngineeringChargeCount,
        Teammate->GetMaxEngineeringCharges(), 1);
    if (ChargesShared > 0)
    {
        EngineeringChargeCount -= ChargesShared;
        Teammate->EngineeringChargeCount += ChargesShared;
        OnRep_EngineeringInventory();
        Teammate->OnRep_EngineeringInventory();
    }

    if (IsValid(InjuryComponent) && IsValid(Teammate->InjuryComponent))
    {
        MedkitsShared = CalculateSupplyShareAmount(
            InjuryComponent->GetMedkitCount(), 1,
            Teammate->InjuryComponent->GetMedkitCount(), 2, 1);
        DressingsShared = CalculateSupplyShareAmount(
            InjuryComponent->GetFieldDressingCount(), 1,
            Teammate->InjuryComponent->GetFieldDressingCount(), 3, 1);
        if (MedkitsShared > 0 || DressingsShared > 0)
        {
            InjuryComponent->AddMedicalSupplies(
                -MedkitsShared, -DressingsShared);
            Teammate->InjuryComponent->AddMedicalSupplies(
                MedkitsShared, DressingsShared);
        }
    }

    const bool bSharedAnything = AmmoShared + FragsShared + ChargesShared +
        MedkitsShared + DressingsShared > 0;
    if (!bSharedAnything)
    {
        ShowStatusNotification(NSLOCTEXT(
            "BrokenHorizon", "ShareFieldSuppliesUnavailable",
            "SUPPLY SHARE // NO COMPATIBLE SHORTFALL\n\n"
            "Keep an emergency reserve or match weapon roles to share ammunition."
        ));
        return false;
    }

    ForceNetUpdate();
    Teammate->ForceNetUpdate();
    UpdateCarryLoadHUD();
    Teammate->UpdateCarryLoadHUD();
    const FText Summary = FText::FromString(FString::Printf(
        TEXT("SUPPLIES SHARED // AMMO %d  FRAG %d  CHARGE %d  MEDKIT %d  DRESSING %d"),
        AmmoShared, FragsShared, ChargesShared, MedkitsShared, DressingsShared));
    ShowStatusNotification(Summary);
    Teammate->ShowStatusNotification(Summary);
    UE_LOG(LogTemp, Display, TEXT(
        "BH_COOP_SUPPLY_SHARED donor=%s receiver=%s ammo=%d frags=%d "
        "charges=%d medkits=%d dressings=%d donor_kg=%.2f receiver_kg=%.2f"),
        *GetName(), *Teammate->GetName(), AmmoShared, FragsShared,
        ChargesShared, MedkitsShared, DressingsShared,
        GetCarriedWeightKilograms(), Teammate->GetCarriedWeightKilograms());
    return true;
}

float ABHCharacter::GetCarriedWeightKilograms() const
{
    return GetCarryLoadProfile().TotalKilograms;
}

void ABHCharacter::UpdateCarryLoadHUD()
{
    ApplyMovementSpeed();
    if (!IsValid(CombatStatusWidget))
    {
        return;
    }
    const FBHCarryLoadProfile Profile = GetCarryLoadProfile();
    CombatStatusWidget->SetCarryLoad(
        Profile.TotalKilograms,
        Profile.State,
        Profile.MovementSpeedMultiplier,
        Profile.StaminaDrainMultiplier
    );
}

void ABHCharacter::RefreshOpenInventoryPanel()
{
    if (!IsLocallyControlled() ||
        !IsValid(InventoryWidget) ||
        !InventoryWidget->IsInventoryOpen())
    {
        return;
    }

    InventoryWidget->SetInventorySnapshot(GetInventorySnapshot());
}

void ABHCharacter::OnRep_FragInventory()
{
    FragGrenadeCount = FMath::Clamp(
        FragGrenadeCount,
        0,
        FMath::Max(0, MaxFragGrenades)
    );
    if (IsValid(CombatStatusWidget))
    {
        CombatStatusWidget->SetFragGrenadeCount(FragGrenadeCount);
    }
    UpdateCarryLoadHUD();
    RefreshOpenInventoryPanel();
}

void ABHCharacter::OnRep_SmokeGrenadeInventory()
{
    SmokeGrenadeCount = FMath::Clamp(
        SmokeGrenadeCount,
        0,
        FMath::Max(0, MaxSmokeGrenades)
    );
    RefreshSmokeGrenadeHUD();
    UpdateCarryLoadHUD();
    RefreshOpenInventoryPanel();
}

void ABHCharacter::OnRep_OwnedKeycards()
{
    if (!IsLocallyControlled())
    {
        return;
    }

#if !UE_BUILD_SHIPPING
    UE_LOG(
        LogTemp,
        Display,
        TEXT("BH_KEYCARD_INVENTORY_REPLICATED mission_items=%d"),
        OwnedKeycards.Num()
    );
#endif

    if (!IsValid(InventoryWidget))
    {
        return;
    }

    InventoryWidget->SetInventorySnapshot(GetInventorySnapshot());
}

float ABHCharacter::GetCurrentPlayerSuppression() const
{
    return HasAuthority()
        ? AuthoritativePlayerSuppression
        : LocalPlayerSuppressionPresentation;
}

float ABHCharacter::AccumulatePlayerSuppression(
    float CurrentSuppression,
    float IncomingIntensity,
    float AccumulationScale
)
{
    const float SafeCurrent = FMath::Clamp(
        CurrentSuppression,
        0.0f,
        1.0f
    );
    const float AppliedPressure =
        FMath::Clamp(IncomingIntensity, 0.0f, 1.0f) *
        FMath::Clamp(AccumulationScale, 0.0f, 1.0f);
    return FMath::Clamp(
        SafeCurrent + ((1.0f - SafeCurrent) * AppliedPressure),
        0.0f,
        1.0f
    );
}

float ABHCharacter::DecayPlayerSuppression(
    float CurrentSuppression,
    float DecayPerSecond,
    float DeltaTime
)
{
    return FMath::Max(
        0.0f,
        FMath::Clamp(CurrentSuppression, 0.0f, 1.0f) -
            (FMath::Max(0.0f, DecayPerSecond) *
             FMath::Max(0.0f, DeltaTime))
    );
}

float ABHCharacter::GetCurrentStamina() const
{
    return CurrentStamina;
}

float ABHCharacter::GetMaxStamina() const
{
    return MaxStamina;
}

float ABHCharacter::GetLeanAmount() const
{
    return CurrentLeanAmount;
}

bool ABHCharacter::IsProne() const
{
    return bIsProne;
}

float ABHCharacter::GetAISightRangeMultiplier() const
{
    const float PostureMultiplier = bIsProne
        ? FMath::Clamp(
            ProneAISightRangeMultiplier,
            0.1f,
            1.0f
        )
        : 1.0f;
    const UGameInstance* GameInstance = GetGameInstance();
    const UBHWarSubsystem* WarSubsystem = GameInstance
        ? GameInstance->GetSubsystem<UBHWarSubsystem>()
        : nullptr;
    const float DifficultyMultiplier = IsValid(WarSubsystem)
        ? FMath::Clamp(
            WarSubsystem->GetCampaignDifficulty()
                .EnemyPerceptionMultiplier,
            0.5f,
            2.0f
        )
        : 1.0f;
    return PostureMultiplier * DifficultyMultiplier;
}

UBHWeaponComponent* ABHCharacter::GetWeaponComponent() const
{
    return WeaponComponent;
}

FBHInventorySnapshot ABHCharacter::GetInventorySnapshot() const
{
    FBHInventorySnapshot Snapshot;

    if (IsValid(WeaponComponent))
    {
        Snapshot.ActiveWeaponRole = UEnum::GetValueAsName(
            WeaponComponent->GetWeaponRole()
        );
        Snapshot.MagazineRounds = FMath::Max(
            0,
            WeaponComponent->GetMagazineAmmo()
        );
        Snapshot.ReserveRounds = FMath::Max(
            0,
            WeaponComponent->GetReserveAmmo()
        );
        Snapshot.MaximumReserveRounds = FMath::Max(
            Snapshot.ReserveRounds,
            WeaponComponent->GetMaxReserveAmmo()
        );
    }

    Snapshot.FragGrenades = FMath::Max(0, GetFragGrenadeCount());
    Snapshot.SmokeGrenades = FMath::Max(0, GetSmokeGrenadeCount());
    Snapshot.EngineeringCharges = FMath::Max(
        0,
        GetEngineeringChargeCount()
    );
    Snapshot.AntiVehicleRounds = GetAntiVehicleRoundCount();
    Snapshot.MissionItemIDs = OwnedKeycards;
    Snapshot.MissionItemCount = Snapshot.MissionItemIDs.Num();

    if (IsValid(InjuryComponent))
    {
        Snapshot.Medkits = FMath::Max(
            0,
            InjuryComponent->GetMedkitCount()
        );
        Snapshot.FieldDressings = FMath::Max(
            0,
            InjuryComponent->GetFieldDressingCount()
        );
        Snapshot.HelmetDurabilityFraction = FMath::Clamp(
            InjuryComponent->GetHelmetDurabilityPercentage(),
            0.0f,
            1.0f
        );
        Snapshot.BodyArmorDurabilityFraction = FMath::Clamp(
            InjuryComponent->GetBodyArmorDurabilityPercentage(),
            0.0f,
            1.0f
        );
    }

    const FBHCarryLoadProfile CarryLoad = GetCarryLoadProfile();
    Snapshot.CarriedWeightKilograms = FMath::Max(
        0.0f,
        CarryLoad.TotalKilograms
    );
    Snapshot.ContainerCapacityKilograms = CarryLoad.ContainerCapacityKilograms;
    Snapshot.ContainerRemainingKilograms = CarryLoad.ContainerRemainingKilograms;
    Snapshot.CarryLoadState = CarryLoad.State;
    return Snapshot;
}

void ABHCharacter::ToggleInventoryPanel()
{
    if (!IsLocallyControlled())
    {
        return;
    }

    if (!IsValid(InventoryWidget))
    {
        APlayerController* PlayerController =
            ResolveOwningPlayerController();
        if (!InventoryWidgetClass ||
            !IsValid(GetWorld()) ||
            !IsValid(PlayerController))
        {
            return;
        }

        InventoryWidget = CreateWidget<UBHInventoryWidget>(
            PlayerController,
            InventoryWidgetClass
        );
        if (!IsValid(InventoryWidget))
        {
            return;
        }
        InventoryWidget->InitializeInventory(this);
        InventoryWidget->SetAnchorsInViewport(
            FAnchors(0.0f, 0.0f, 1.0f, 1.0f)
        );
        InventoryWidget->SetAlignmentInViewport(
            FVector2D::ZeroVector
        );
        InventoryWidget->SetPositionInViewport(
            FVector2D::ZeroVector,
            false
        );
        InventoryWidget->AddToViewport(280);
    }

    InventoryWidget->SetInventorySnapshot(GetInventorySnapshot());
    InventoryWidget->SetInventoryOpen(
        !InventoryWidget->IsInventoryOpen()
    );
    UE_LOG(
        LogTemp,
        Display,
        TEXT("BH_INVENTORY_PANEL state=%s weapon_role=%s magazine=%d reserve=%d mission_items=%d weight_kg=%.1f capacity_kg=%.1f remaining_kg=%.1f"),
        InventoryWidget->IsInventoryOpen() ? TEXT("OPEN") : TEXT("CLOSED"),
        *GetInventorySnapshot().ActiveWeaponRole.ToString(),
        GetInventorySnapshot().MagazineRounds,
        GetInventorySnapshot().ReserveRounds,
        GetInventorySnapshot().MissionItemCount,
        GetInventorySnapshot().CarriedWeightKilograms,
        GetInventorySnapshot().ContainerCapacityKilograms,
        GetInventorySnapshot().ContainerRemainingKilograms
    );
}

void ABHCharacter::CycleInventoryWeaponRole()
{
    if (!IsLocallyControlled() || !IsValid(WeaponComponent))
    {
        return;
    }

    WeaponComponent->CycleWeaponRole(true);
    if (IsValid(InventoryWidget))
    {
        InventoryWidget->SetInventorySnapshot(GetInventorySnapshot());
    }
    UE_LOG(LogTemp, Display, TEXT("BH_INVENTORY_ROLE_CHANGED role=%s"),
        *GetInventorySnapshot().ActiveWeaponRole.ToString());
}

bool ABHCharacter::DropInventoryItem(
    EBHSalvagePickupType ItemType,
    int32 QuantityToDrop
)
{
    QuantityToDrop = FMath::Clamp(QuantityToDrop, 1, 32);
    if (!HasAuthority())
    {
        ServerDropInventoryItem(ItemType, QuantityToDrop);
        return true;
    }

    int32 Removed = 0;
    switch (ItemType)
    {
        case EBHSalvagePickupType::FragGrenades:
            Removed = FMath::Min(QuantityToDrop, FragGrenadeCount);
            FragGrenadeCount -= Removed;
            RefreshFragGrenadeHUD();
            break;
        case EBHSalvagePickupType::SmokeGrenades:
            Removed = FMath::Min(QuantityToDrop, SmokeGrenadeCount);
            SmokeGrenadeCount -= Removed;
            RefreshSmokeGrenadeHUD();
            break;
        case EBHSalvagePickupType::EngineeringCharges:
            Removed = FMath::Min(QuantityToDrop, EngineeringChargeCount);
            EngineeringChargeCount -= Removed;
            break;
        case EBHSalvagePickupType::AntiVehicleRounds:
            Removed = FMath::Min(QuantityToDrop, AntiVehicleRoundCount);
            AntiVehicleRoundCount -= Removed;
            break;
        case EBHSalvagePickupType::Ammunition:
        default:
            if (IsValid(WeaponComponent))
            {
                Removed = WeaponComponent->RemoveReserveAmmo(QuantityToDrop);
            }
            break;
    }

    if (Removed <= 0 || !IsValid(GetWorld()))
    {
        ShowStatusNotification(NSLOCTEXT(
            "BrokenHorizon",
            "InventoryDropEmpty",
            "DISCARD FAILED // NOTHING AVAILABLE"
        ));
        return false;
    }

    const FVector DropLocation = GetActorLocation() +
        GetActorForwardVector() * 110.0f + FVector(0.0f, 0.0f, 20.0f);
    ABHSalvagePickup* Pickup = GetWorld()->SpawnActor<ABHSalvagePickup>(
        ABHSalvagePickup::StaticClass(),
        DropLocation,
        GetActorRotation()
    );
    if (!IsValid(Pickup))
    {
        return false;
    }
    Pickup->ConfigureSalvage(NAME_None, ItemType, Removed);
    Pickup->SetLifeSpan(300.0f);
    UpdateCarryLoadHUD();
    ShowStatusNotification(FText::Format(
        NSLOCTEXT("BrokenHorizon", "InventoryDropAccepted", "DISCARDED // {0} x{1}"),
        UEnum::GetDisplayValueAsText(ItemType),
        FText::AsNumber(Removed)
    ));
    UE_LOG(LogTemp, Display, TEXT("BH_INVENTORY_DROP type=%d quantity=%d"),
        static_cast<int32>(ItemType), Removed);
    return true;
}

void ABHCharacter::ServerDropInventoryItem_Implementation(
    EBHSalvagePickupType ItemType,
    int32 Quantity
)
{
    DropInventoryItem(ItemType, Quantity);
}

bool ABHCharacter::TransferInventoryItemTo(
    ABHCharacter* Recipient,
    EBHSalvagePickupType ItemType,
    int32 Quantity
)
{
    Quantity = FMath::Clamp(Quantity, 1, 32);
    if (!IsValid(Recipient) || Recipient == this ||
        !IsValid(GetWorld()) ||
        FVector::DistSquared(GetActorLocation(), Recipient->GetActorLocation()) >
            FMath::Square(250.0f))
    {
        return false;
    }
    if (!HasAuthority())
    {
        ServerTransferInventoryItem(Recipient, ItemType, Quantity);
        return true;
    }

    int32 Available = 0;
    switch (ItemType)
    {
        case EBHSalvagePickupType::FragGrenades: Available = FragGrenadeCount; break;
        case EBHSalvagePickupType::SmokeGrenades: Available = SmokeGrenadeCount; break;
        case EBHSalvagePickupType::EngineeringCharges: Available = EngineeringChargeCount; break;
        case EBHSalvagePickupType::AntiVehicleRounds: Available = AntiVehicleRoundCount; break;
        case EBHSalvagePickupType::Ammunition:
            Available = IsValid(WeaponComponent) ? WeaponComponent->GetReserveAmmo() : 0;
            break;
    }
    if (Available <= 0)
    {
        ShowStatusNotification(NSLOCTEXT("BrokenHorizon", "InventoryTransferEmpty", "TRANSFER FAILED // NOTHING AVAILABLE"));
        return false;
    }
    Quantity = FMath::Min(Quantity, Available);

    int32 Accepted = 0;
    switch (ItemType)
    {
        case EBHSalvagePickupType::FragGrenades:
            Accepted = Recipient->AddFragGrenades(Quantity);
            break;
        case EBHSalvagePickupType::SmokeGrenades:
            Accepted = Recipient->AddSmokeGrenades(Quantity);
            break;
        case EBHSalvagePickupType::EngineeringCharges:
            Accepted = Recipient->AddEngineeringCharges(Quantity);
            break;
        case EBHSalvagePickupType::AntiVehicleRounds:
            Accepted = Recipient->AddAntiVehicleRounds(Quantity);
            break;
        case EBHSalvagePickupType::Ammunition:
            if (IsValid(Recipient->GetWeaponComponent()))
            {
                Accepted = Recipient->GetWeaponComponent()->AddReserveAmmo(Quantity);
            }
            break;
    }

    if (Accepted <= 0)
    {
        Recipient->ShowStatusNotification(NSLOCTEXT(
            "BrokenHorizon",
            "InventoryTransferFull",
            "TRANSFER FAILED // RECIPIENT CAPACITY FULL"));
        return false;
    }

    int32 Removed = 0;
    switch (ItemType)
    {
        case EBHSalvagePickupType::FragGrenades:
            Removed = FMath::Min(Accepted, FragGrenadeCount);
            FragGrenadeCount -= Removed;
            break;
        case EBHSalvagePickupType::SmokeGrenades:
            Removed = FMath::Min(Accepted, SmokeGrenadeCount);
            SmokeGrenadeCount -= Removed;
            break;
        case EBHSalvagePickupType::EngineeringCharges:
            Removed = FMath::Min(Accepted, EngineeringChargeCount);
            EngineeringChargeCount -= Removed;
            break;
        case EBHSalvagePickupType::AntiVehicleRounds:
            Removed = FMath::Min(Accepted, AntiVehicleRoundCount);
            AntiVehicleRoundCount -= Removed;
            break;
        case EBHSalvagePickupType::Ammunition:
            if (IsValid(WeaponComponent))
            {
                Removed = WeaponComponent->RemoveReserveAmmo(Accepted);
            }
            break;
    }

    if (Removed != Accepted)
    {
        UE_LOG(LogTemp, Warning, TEXT("BH_INVENTORY_TRANSFER_ROLLBACK accepted=%d removed=%d"), Accepted, Removed);
        return false;
    }

    RefreshFragGrenadeHUD();
    RefreshSmokeGrenadeHUD();
    UpdateCarryLoadHUD();
    Recipient->UpdateCarryLoadHUD();
    ShowStatusNotification(FText::Format(
        NSLOCTEXT("BrokenHorizon", "InventoryTransferSent", "TRANSFERRED // {0} x{1}"),
        UEnum::GetDisplayValueAsText(ItemType), FText::AsNumber(Removed)));
    Recipient->ShowStatusNotification(FText::Format(
        NSLOCTEXT("BrokenHorizon", "InventoryTransferReceived", "RECEIVED // {0} x{1}"),
        UEnum::GetDisplayValueAsText(ItemType), FText::AsNumber(Removed)));
    UE_LOG(LogTemp, Display, TEXT("BH_INVENTORY_TRANSFER type=%d quantity=%d source=%s recipient=%s"),
        static_cast<int32>(ItemType), Removed, *GetName(), *Recipient->GetName());
    if (FParse::Param(FCommandLine::Get(), TEXT("BHTestInventoryTransferRuntime")))
    {
        UE_LOG(LogTemp, Display,
            TEXT("BH_TEST_INVENTORY_TRANSFER result=success type=%d quantity=%d source=%s recipient=%s"),
            static_cast<int32>(ItemType), Removed, *GetName(), *Recipient->GetName());
    }
    return true;
}

void ABHCharacter::ServerTransferInventoryItem_Implementation(
    ABHCharacter* Recipient,
    EBHSalvagePickupType ItemType,
    int32 Quantity
)
{
    TransferInventoryItemTo(Recipient, ItemType, Quantity);
}

bool ABHCharacter::TransferFragToNearestAlly(int32 Quantity)
{
    Quantity = FMath::Clamp(Quantity, 1, 32);
    if (!HasAuthority())
    {
        ServerTransferFragToNearestAlly(Quantity);
        return true;
    }

    if (!IsValid(GetWorld()))
    {
        return false;
    }

    return TransferInventoryItemToNearestAllyAuthority(
        EBHSalvagePickupType::FragGrenades,
        Quantity
    );
}

bool ABHCharacter::TransferInventoryItemToNearestAlly(
    EBHSalvagePickupType ItemType,
    int32 Quantity
)
{
    Quantity = FMath::Clamp(Quantity, 1, 32);
    if (!HasAuthority())
    {
        ServerTransferInventoryItemToNearestAlly(ItemType, Quantity);
        return true;
    }

    return TransferInventoryItemToNearestAllyAuthority(ItemType, Quantity);
}

bool ABHCharacter::TransferInventoryItemToNearestAllyAuthority(
    EBHSalvagePickupType ItemType,
    int32 Quantity
)
{
    if (!IsValid(GetWorld()))
    {
        return false;
    }

    ABHCharacter* NearestAlly = nullptr;
    float NearestDistanceSquared = TNumericLimits<float>::Max();
    for (TActorIterator<ABHCharacter> It(GetWorld()); It; ++It)
    {
        ABHCharacter* Candidate = *It;
        if (!IsValid(Candidate) || Candidate == this ||
            !IsValid(Candidate->GetHealthComponent()) ||
            Candidate->GetHealthComponent()->IsDead())
        {
            continue;
        }
        const float DistanceSquared = FVector::DistSquared(
            GetActorLocation(), Candidate->GetActorLocation());
        if (DistanceSquared < NearestDistanceSquared)
        {
            NearestDistanceSquared = DistanceSquared;
            NearestAlly = Candidate;
        }
    }
    return IsValid(NearestAlly) && TransferInventoryItemTo(
        NearestAlly, ItemType, Quantity);
}

void ABHCharacter::ServerTransferFragToNearestAlly_Implementation(
    int32 Quantity
)
{
    TransferFragToNearestAlly(Quantity);
}

void ABHCharacter::ServerTransferInventoryItemToNearestAlly_Implementation(
    EBHSalvagePickupType ItemType,
    int32 Quantity
)
{
    TransferInventoryItemToNearestAlly(ItemType, Quantity);
}

int32 ABHCharacter::GetFragGrenadeCount() const
{
    return FragGrenadeCount;
}

int32 ABHCharacter::GetSmokeGrenadeCount() const
{
    return SmokeGrenadeCount;
}

int32 ABHCharacter::GetMaxSmokeGrenades() const
{
    return FMath::Max(0, MaxSmokeGrenades);
}

int32 ABHCharacter::AddSmokeGrenades(int32 Amount)
{
    const int32 PreviousCount = SmokeGrenadeCount;
    SmokeGrenadeCount = FMath::Clamp(
        SmokeGrenadeCount + FMath::Max(0, Amount),
        0,
        GetMaxSmokeGrenades()
    );
    RefreshSmokeGrenadeHUD();
    UpdateCarryLoadHUD();
    return SmokeGrenadeCount - PreviousCount;
}

int32 ABHCharacter::GetMaxFragGrenades() const
{
    return FMath::Max(0, MaxFragGrenades);
}

int32 ABHCharacter::AddFragGrenades(int32 Amount)
{
    const int32 PreviousCount = FragGrenadeCount;
    FragGrenadeCount = FMath::Clamp(
        FragGrenadeCount + FMath::Max(0, Amount),
        0,
        GetMaxFragGrenades()
    );
    RefreshFragGrenadeHUD();
    return FragGrenadeCount - PreviousCount;
}

void ABHCharacter::RestoreFragGrenadeCount(int32 SavedCount)
{
    FragGrenadeCount = FMath::Clamp(
        SavedCount,
        0,
        GetMaxFragGrenades()
    );
    RefreshFragGrenadeHUD();
}

void ABHCharacter::RestoreSmokeGrenadeCount(int32 SavedCount)
{
    SmokeGrenadeCount = FMath::Clamp(
        SavedCount,
        0,
        GetMaxSmokeGrenades()
    );
    RefreshSmokeGrenadeHUD();
    UpdateCarryLoadHUD();
}

void ABHCharacter::RefreshFragGrenadeHUD()
{
    if (IsValid(CombatStatusWidget))
    {
        CombatStatusWidget->SetFragGrenadeCount(
            FragGrenadeCount
        );
    }
}

void ABHCharacter::RefreshSmokeGrenadeHUD()
{
    if (IsValid(CombatStatusWidget))
    {
        CombatStatusWidget->SetSmokeGrenadeCount(
            SmokeGrenadeCount
        );
    }
}

USkeletalMeshComponent* ABHCharacter::GetFirstPersonArmsMesh() const
{
    return FirstPersonArms;
}

bool ABHCharacter::RestorePersistentState(
    UBHMissionData* SavedMissionData,
    FName SavedCurrentObjectiveID,
    const TArray<FName>& SavedCompletedObjectiveIDs,
    bool bSavedMissionComplete,
    bool bSavedMissionFailed,
    const TArray<FName>& SavedOwnedKeycardIDs,
    const TArray<FName>& SavedCollectedKeycardPersistenceIDs
)
{
    bRuntimeWarOperation = false;
    AssignedWarSupplySourceSectorID = NAME_None;

    if (UGameInstance* GameInstance = GetGameInstance())
    {
        if (UBHWarSubsystem* WarSubsystem =
                GameInstance->GetSubsystem<UBHWarSubsystem>())
        {
            WarSubsystem->ClearCommittedOperation();
        }
    }

    if (!IsValid(ObjectiveComponent))
    {
        return false;
    }

    OwnedKeycards.Reset();

    for (const FName KeycardID : SavedOwnedKeycardIDs)
    {
        if (!KeycardID.IsNone())
        {
            OwnedKeycards.AddUnique(KeycardID);
        }
    }

    CollectedKeycardPersistenceIDs.Reset();

    for (const FName PersistenceID
        : SavedCollectedKeycardPersistenceIDs)
    {
        if (!PersistenceID.IsNone())
        {
            CollectedKeycardPersistenceIDs.Add(PersistenceID);
        }
    }

    MissionData = SavedMissionData;

    const bool bRestoredObjectives =
        ObjectiveComponent->RestoreMissionState(
            SavedMissionData,
            SavedCurrentObjectiveID,
            SavedCompletedObjectiveIDs,
            bSavedMissionComplete,
            bSavedMissionFailed
        );

    ConfigureStrategicMissionPresentation();
    RefreshObjectiveWidget();

    if (bRestoredObjectives &&
        (ObjectiveComponent->IsMissionComplete() ||
         ObjectiveComponent->IsMissionFailed()))
    {
        if (ObjectiveComponent->IsMissionFailed())
        {
            MissionCompleteMessage = NSLOCTEXT(
                "BrokenHorizon",
                "RestoredMissionFailureMessage",
                "MISSION FAILED\n\n"
                "The failed operation is recorded in the campaign."
            );
        }

        EnterMissionCompleteState(false);
    }

    return bRestoredObjectives;
}

bool ABHCharacter::RestoreRuntimeOperationState(
    UBHMissionData* SavedMissionData,
    const TArray<FBHObjectiveDefinition>& SavedRuntimeObjectives,
    FName SavedCurrentObjectiveID,
    const TArray<FName>& SavedCompletedObjectiveIDs,
    bool bSavedMissionComplete,
    bool bSavedMissionFailed,
    FName SavedAssignedSectorID,
    FName SavedAssignedSupplySourceSectorID,
    EBHWarPriorityType SavedAssignedPriorityType,
    const FBHOpenWorldOperationState&
        SavedOpenWorldOperationState,
    const TArray<FName>& SavedOwnedKeycardIDs,
    const TArray<FName>& SavedCollectedKeycardPersistenceIDs
)
{
    if (!IsValid(ObjectiveComponent) ||
        SavedRuntimeObjectives.IsEmpty())
    {
        return false;
    }

    OwnedKeycards.Reset();

    for (const FName KeycardID : SavedOwnedKeycardIDs)
    {
        if (!KeycardID.IsNone())
        {
            OwnedKeycards.AddUnique(KeycardID);
        }
    }

    CollectedKeycardPersistenceIDs.Reset();

    for (const FName PersistenceID :
        SavedCollectedKeycardPersistenceIDs)
    {
        if (!PersistenceID.IsNone())
        {
            CollectedKeycardPersistenceIDs.Add(PersistenceID);
        }
    }

    MissionData = SavedMissionData;
    bRuntimeWarOperation = true;
    AssignedWarSectorID = SavedAssignedSectorID;
    AssignedWarSupplySourceSectorID =
        SavedAssignedSupplySourceSectorID;
    AssignedWarPriorityType = SavedAssignedPriorityType;

    const bool bRestored =
        ObjectiveComponent->RestoreRuntimeMissionState(
            SavedRuntimeObjectives,
            SavedCurrentObjectiveID,
            SavedCompletedObjectiveIDs,
            bSavedMissionComplete,
            bSavedMissionFailed
        );

    UBHWarSubsystem* WarSubsystem = GetGameInstance()
        ? GetGameInstance()->GetSubsystem<UBHWarSubsystem>()
        : nullptr;
    const bool bCommitRestored =
        bSavedMissionComplete ||
        bSavedMissionFailed ||
        (bRestored &&
         IsValid(WarSubsystem) &&
         WarSubsystem->SetCommittedOperation(
             AssignedWarSectorID,
             AssignedWarPriorityType
         ));

    if (!bRestored || !bCommitRestored)
    {
        bRuntimeWarOperation = false;
        return false;
    }

    ConfigureStrategicMissionPresentation();
    RefreshObjectiveWidget();

    if (!bSavedMissionComplete && !bSavedMissionFailed)
    {
        const bool bDirectorStarted =
            StartOpenWorldOperationDirector(true);
        const bool bOperationUsesTacticalDirector =
            AssignedWarPriorityType == EBHWarPriorityType::Attack ||
            AssignedWarPriorityType == EBHWarPriorityType::Defend ||
            AssignedWarPriorityType == EBHWarPriorityType::Raid;
        const bool bOperationRestored =
            bDirectorStarted &&
            (
                !bOperationUsesTacticalDirector ||
                (
                    IsValid(OpenWorldOperationDirector) &&
                    OpenWorldOperationDirector->RestoreOperationState(
                        SavedOpenWorldOperationState
                    )
                )
            );

        if (!bOperationRestored)
        {
            if (IsValid(WarSubsystem))
            {
                WarSubsystem->ClearCommittedOperation();
            }

            bRuntimeWarOperation = false;
            return false;
        }
    }
    else
    {
        if (bSavedMissionFailed)
        {
            MissionCompleteMessage = NSLOCTEXT(
                "BrokenHorizon",
                "RestoredWarOperationFailureMessage",
                "MISSION FAILED\n\n"
                "The sector loss is recorded in the persistent war."
            );
        }

        EnterMissionCompleteState(false);
    }

    return bRestored;
}

void ABHCharacter::OnObjectiveCompleted(
    FName CompletedObjectiveID,
    FText CompletedObjectiveText
)
{
    QueueObjectiveCompletionRadio(CompletedObjectiveID);
    UE_LOG(
        LogTemp,
        Log,
        TEXT("Completed objective %s."),
        *CompletedObjectiveID.ToString()
    );

    ShowStatusNotificationWithAudioCue(
        FText::Format(
            NSLOCTEXT(
                "BrokenHorizon",
                "ObjectiveCompletedNotification",
                "OBJECTIVE COMPLETE\n\n\u2713 {0}"
            ),
            CompletedObjectiveText
        ),
        EBHNotificationPriority::Critical,
        EBHNotificationAudioCue::QuietConfirmation
    );

    RefreshObjectiveWidget();
    if (IsValid(ObjectiveComponent))
    {
        QueueObjectiveActivationRadio(
            ObjectiveComponent->GetCurrentObjectiveID()
        );
    }

    if (IsValid(ObjectiveComponent) &&
        !ObjectiveComponent->IsMissionComplete())
    {
        UBHSaveSubsystem* SaveSubsystem =
            GetGameInstance()
                ? GetGameInstance()->GetSubsystem<
                    UBHSaveSubsystem>()
                : nullptr;

        if (!IsValid(SaveSubsystem) ||
            !SaveSubsystem->SaveProgress())
        {
            UE_LOG(
                LogTemp,
                Warning,
                TEXT(
                    "BH_OBJECTIVE_CHECKPOINT_FAILED "
                    "objective=%s"
                ),
                *CompletedObjectiveID.ToString()
            );
        }
        else
        {
            UE_LOG(
                LogTemp,
                Display,
                TEXT(
                    "BH_OBJECTIVE_CHECKPOINT objective=%s"
                ),
                *CompletedObjectiveID.ToString()
            );
        }
    }
}

void ABHCharacter::ShowStatusNotification(
    const FText& Message
)
{
    ShowPriorityStatusNotification(
        Message,
        EBHNotificationPriority::Normal
    );
}

void ABHCharacter::ShowPriorityStatusNotification(
    const FText& Message,
    EBHNotificationPriority NotificationPriority
)
{
    ShowStatusNotificationWithAudioCue(
        Message,
        NotificationPriority,
        UBHObjectiveNotificationWidget::ResolveDefaultAudioCue(
            NotificationPriority
        )
    );
}

void ABHCharacter::ShowDeferredStrategicStatusNotification(
    const FText& Message
)
{
    if (HasAuthority() && !IsLocallyControlled())
    {
        if (GetNetOwningPlayer())
        {
            ClientShowDeferredStrategicStatusNotification(Message);
        }
        return;
    }

    DisplayDeferredStrategicStatusNotificationLocally(Message);
}

void ABHCharacter::ShowStatusNotificationWithAudioCue(
    const FText& Message,
    EBHNotificationPriority NotificationPriority,
    EBHNotificationAudioCue AudioCue
)
{
    if (HasAuthority() && !IsLocallyControlled())
    {
        if (GetNetOwningPlayer())
        {
            ClientShowStatusNotificationWithAudioCue(
                Message,
                NotificationPriority,
                AudioCue
            );
        }

        return;
    }

    DisplayStatusNotificationLocally(
        Message,
        NotificationPriority,
        AudioCue
    );
}

void ABHCharacter::DisplayStatusNotificationLocally(
    const FText& Message
)
{
    DisplayStatusNotificationLocally(
        Message,
        EBHNotificationPriority::Normal
    );
}

void ABHCharacter::DisplayStatusNotificationLocally(
    const FText& Message,
    EBHNotificationPriority NotificationPriority
)
{
    DisplayStatusNotificationLocally(
        Message,
        NotificationPriority,
        UBHObjectiveNotificationWidget::ResolveDefaultAudioCue(
            NotificationPriority
        )
    );
}

void ABHCharacter::DisplayStatusNotificationLocally(
    const FText& Message,
    EBHNotificationPriority NotificationPriority,
    EBHNotificationAudioCue AudioCue
)
{
    APlayerController* PlayerController =
        ResolveOwningPlayerController();

    if (!IsValid(PlayerController) ||
        PlayerController->GetLocalPlayer() == nullptr)
    {
        return;
    }

    if (!ObjectiveNotificationWidget &&
        ObjectiveNotificationWidgetClass)
    {
        ObjectiveNotificationWidget =
            CreateWidget<UBHObjectiveNotificationWidget>(
                PlayerController,
                ObjectiveNotificationWidgetClass
            );

        if (ObjectiveNotificationWidget)
        {
            ObjectiveNotificationWidget->AddToViewport();
        }
    }


    if (ObjectiveNotificationWidget)
    {
        RefreshMissionPresentationVisibility();
        ObjectiveNotificationWidget->SetCombatIntensityActive(
            IsLocalCombatIntensityActive()
        );
        ObjectiveNotificationWidget->ShowNotificationWithAudioCue(
            Message,
            NotificationPriority,
            AudioCue
        );
    }
}

void ABHCharacter::ClientShowStatusNotification_Implementation(
    const FText& Message
)
{
    DisplayStatusNotificationLocally(
        Message,
        EBHNotificationPriority::Normal
    );
}

void ABHCharacter::DisplayDeferredStrategicStatusNotificationLocally(
    const FText& Message
)
{
    APlayerController* PlayerController = ResolveOwningPlayerController();
    if (!IsValid(PlayerController) ||
        PlayerController->GetLocalPlayer() == nullptr)
    {
        return;
    }

    if (!ObjectiveNotificationWidget && ObjectiveNotificationWidgetClass)
    {
        ObjectiveNotificationWidget =
            CreateWidget<UBHObjectiveNotificationWidget>(
                PlayerController,
                ObjectiveNotificationWidgetClass
            );
        if (ObjectiveNotificationWidget)
        {
            ObjectiveNotificationWidget->AddToViewport();
        }
    }

    if (ObjectiveNotificationWidget)
    {
        RefreshMissionPresentationVisibility();
        ObjectiveNotificationWidget->SetCombatIntensityActive(
            IsLocalCombatIntensityActive()
        );
        ObjectiveNotificationWidget->ShowDeferredStrategicNotification(
            Message
        );
    }
}

bool ABHCharacter::IsLocalCombatIntensityActive() const
{
    const UWorld* World = GetWorld();
    const bool bRecentlyDamaged = IsValid(World) &&
        LastPlayerDamageTimeSeconds > -BIG_NUMBER &&
        World->GetTimeSeconds() - LastPlayerDamageTimeSeconds <
            FMath::Max(0.0f, StrategicNotificationCombatQuietSeconds);
    return bRecentlyDamaged ||
        LocalPlayerSuppressionPresentation > 0.05f;
}

void ABHCharacter::ClientShowPriorityStatusNotification_Implementation(
    const FText& Message,
    EBHNotificationPriority NotificationPriority
)
{
    DisplayStatusNotificationLocally(
        Message,
        NotificationPriority
    );
}

void ABHCharacter::ClientShowStatusNotificationWithAudioCue_Implementation(
    const FText& Message,
    EBHNotificationPriority NotificationPriority,
    EBHNotificationAudioCue AudioCue
)
{
    DisplayStatusNotificationLocally(
        Message,
        NotificationPriority,
        AudioCue
    );
}

void ABHCharacter::ClientShowDeferredStrategicStatusNotification_Implementation(
    const FText& Message
)
{
    DisplayDeferredStrategicStatusNotificationLocally(Message);
}

void ABHCharacter::ClientPresentDeath_Implementation(
    float DelaySeconds
)
{
    bIsHandlingDeath = true;
    LocalPlayerSuppressionPresentation = 0.0f;

    if (bWarMapOpen)
    {
        CloseWarMap();
    }

    FinishTraversal(false);

    if (IsValid(InjuryComponent))
    {
        InjuryComponent->CancelMedkitTreatment();
    }

    if (IsValid(WeaponComponent))
    {
        WeaponComponent->StopAllActions();
    }

    GetCharacterMovement()->DisableMovement();

    APlayerController* PlayerController =
        ResolveOwningPlayerController();

    if (!IsValid(PlayerController) ||
        PlayerController->GetLocalPlayer() == nullptr)
    {
        return;
    }

    DisableInput(PlayerController);
    PlayerController->SetIgnoreMoveInput(true);
    PlayerController->SetIgnoreLookInput(true);

    if (IsValid(InteractionPromptWidget))
    {
        InteractionPromptWidget->SetVisibility(
            ESlateVisibility::Collapsed
        );
    }

    if (IsValid(AmmoHUDWidget))
    {
        AmmoHUDWidget->SetVisibility(
            ESlateVisibility::Collapsed
        );
    }

    if (IsValid(CombatStatusWidget))
    {
        CombatStatusWidget->SetVisibility(
            ESlateVisibility::Collapsed
        );
    }

    if (!IsValid(DeathWidget) && DeathWidgetClass)
    {
        DeathWidget = CreateWidget<UBHDeathWidget>(
            PlayerController,
            DeathWidgetClass
        );

        if (IsValid(DeathWidget))
        {
            DeathWidget->AddToViewport(100);
        }
    }

    if (IsValid(DeathWidget))
    {
        DeathWidget->ShowDeathScreenWithRespawnDelay(
            FMath::Max(0.0f, DelaySeconds)
        );
    }
}

void ABHCharacter::ClientCompleteFieldRespawn_Implementation()
{
    bIsHandlingDeath = false;

    if (IsValid(DeathWidget))
    {
        DeathWidget->SetVisibility(ESlateVisibility::Collapsed);
    }

    GetCharacterMovement()->SetMovementMode(MOVE_Walking);
    ApplyMovementSpeed();

    if (IsValid(ObjectiveWidget))
    {
        RefreshMissionPresentationVisibility();
    }

    if (IsValid(AmmoHUDWidget))
    {
        AmmoHUDWidget->SetVisibility(ESlateVisibility::Visible);
    }

    if (IsValid(CombatStatusWidget))
    {
        CombatStatusWidget->SetVisibility(
            ESlateVisibility::Visible
        );
    }

    APlayerController* PlayerController =
        ResolveOwningPlayerController();

    if (IsValid(PlayerController) &&
        PlayerController->GetLocalPlayer() != nullptr)
    {
        EnableInput(PlayerController);
        PlayerController->SetIgnoreMoveInput(false);
        PlayerController->SetIgnoreLookInput(false);
        PlayerController->SetInputMode(FInputModeGameOnly());
        PlayerController->bShowMouseCursor = false;
    }
}

void ABHCharacter::ClientConfirmOperationDeployment_Implementation(
    FName SectorID,
    FName SupplySourceSectorID,
    EBHWarPriorityType OperationType
)
{
    bRuntimeWarOperation = true;
    AssignedWarSectorID = SectorID;
    AssignedWarSupplySourceSectorID = SupplySourceSectorID;
    AssignedWarPriorityType = OperationType;

    if (bWarMapOpen)
    {
        CloseWarMap();
    }

    if (IsValid(ObjectiveComponent) &&
        !ObjectiveComponent->IsRuntimeMission())
    {
        FBHObjectiveDefinition OperationObjective;
        OperationObjective.ObjectiveID =
            OperationType == EBHWarPriorityType::Resupply
                ? BHObjectiveIds::DeliverResupply
                : OperationType ==
                        EBHWarPriorityType::EscortRescue
                    ? BHObjectiveIds::ProtectConvoy
                : OperationType == EBHWarPriorityType::Rescue
                    ? BHObjectiveIds::EvacuateCasualty
                : OperationType == EBHWarPriorityType::Recon
                    ? BHObjectiveIds::ObserveSector
                : BHObjectiveIds::EliminateGuard;
        ObjectiveComponent->StartRuntimeMission(
            {OperationObjective}
        );
    }

    ConfigureStrategicMissionPresentation();
    RefreshObjectiveWidget();

    if (IsValid(ObjectiveComponent))
    {
        QueueObjectiveActivationRadio(
            ObjectiveComponent->GetCurrentObjectiveID()
        );
    }
    UpdateOperationWaypointHUD();

    if (IsValid(ObjectiveWidget))
    {
        RefreshMissionPresentationVisibility();
    }
}

void ABHCharacter::ClientPresentOperationDebrief_Implementation(
    const FText& Message
)
{
    MissionCompleteMessage = Message;

#if !UE_BUILD_SHIPPING
    UE_LOG(
        LogTemp,
        Display,
        TEXT("BH_TEST_OPERATION_DEBRIEF_CLIENT result=success")
    );
#endif

    if (!bIsHandlingMissionComplete)
    {
        EnterMissionCompleteState(false);
    }
    else if (IsValid(MissionCompleteWidget))
    {
        MissionCompleteWidget->SetMissionCompleteText(Message);
    }

    if (IsValid(MissionCompleteWidget))
    {
        MissionCompleteWidget->ResetContinueRequest();
    }
#if !UE_BUILD_SHIPPING
    if (IsLocallyControlled())
    {
        BHDefenseAMultiplayerTest::ObserveDebrief(MissionCompleteWidget, IsMissionComplete());
    }
#endif
}

void ABHCharacter::ClientConfirmDebriefContinue_Implementation(
    bool bCampaignResolved
)
{
    if (!bIsHandlingMissionComplete)
    {
        return;
    }

    if (bCampaignResolved)
    {
        EnterCampaignEpilogueFreeRoam(true);
    }
    else
    {
        EnterPostOperationFreeRoam(true);
    }
#if !UE_BUILD_SHIPPING
    BHDefenseAMultiplayerTest::ObserveContinueAcknowledgement(ResolveOwningPlayerController());
#endif
}

void ABHCharacter::RefreshObjectiveWidget()
{
    if (IsValid(ObjectiveWidget) && IsValid(ObjectiveComponent))
    {
        ObjectiveWidget->SetObjectiveList(
            ObjectiveComponent->GetCompletedObjectiveTexts(),
            ObjectiveComponent->GetCurrentObjectiveText()
        );
    }
    RefreshMissionPresentationVisibility();
}

bool ABHCharacter::HasTacticalOperationWaypoint() const
{
    const ABHWarGameState* State = GetWorld() ? GetWorld()->GetGameState<ABHWarGameState>() : nullptr;
    const EBHActiveOperationPhase Phase = IsValid(State) ? State->GetActiveOperationSnapshot().Phase : EBHActiveOperationPhase::None;
    return Phase == EBHActiveOperationPhase::Approach || Phase == EBHActiveOperationPhase::Combat ||
        Phase == EBHActiveOperationPhase::AwaitingWave || Phase == EBHActiveOperationPhase::Securing ||
        Phase == EBHActiveOperationPhase::RaidExfiltration ||
        (IsValid(OpenWorldOperationDirector) && OpenWorldOperationDirector->IsOperationInProgress());
}

void ABHCharacter::RefreshMissionPresentationVisibility()
{
    if (!IsLocallyControlled()) { return; }
    const ABHWarGameState* State = GetWorld() ? GetWorld()->GetGameState<ABHWarGameState>() : nullptr;
    const EBHActiveOperationPhase Phase = IsValid(State) ? State->GetActiveOperationSnapshot().Phase : EBHActiveOperationPhase::None;
    const bool bTerminalSnapshot = Phase == EBHActiveOperationPhase::DebriefSuccess || Phase == EBHActiveOperationPhase::DebriefFailure;
    const bool bTerminal = bIsHandlingDeath || bIsHandlingMissionComplete || bTerminalSnapshot;
    if (IsValid(ObjectiveWidget))
    {
        const bool bHasObjective = IsValid(ObjectiveComponent) && !ObjectiveComponent->GetCurrentObjectiveID().IsNone() &&
            !ObjectiveComponent->GetCurrentObjectiveText().IsEmpty();
        ObjectiveWidget->SetVisibility(!bTerminal && bHasObjective && !HasTacticalOperationWaypoint()
            ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    }
    if (IsValid(ObjectiveNotificationWidget))
    {
        ObjectiveNotificationWidget->SetPresentationSuppressed(bTerminal || bPauseMenuOpen);
        const bool bTacticalActivated = (Phase != EBHActiveOperationPhase::None && Phase != EBHActiveOperationPhase::Approach) ||
            (IsValid(OpenWorldOperationDirector) && OpenWorldOperationDirector->IsOperationActivated());
        if (bTerminal || bTacticalActivated || AssignedWarSectorID.IsNone())
        {
            ObjectiveNotificationWidget->CancelKeyedNotification(FName(TEXT("StrategicBriefing")));
        }
    }
#if !UE_BUILD_SHIPPING
    BHDefenseAMultiplayerTest::ObservePresentation(ResolveOwningPlayerController(),
        IsValid(ObjectiveWidget) && ObjectiveWidget->IsInViewport() && ObjectiveWidget->IsVisible(),
        IsValid(ObjectiveNotificationWidget) && ObjectiveNotificationWidget->HasNotificationForSource(FName(TEXT("StrategicBriefing"))),
        IsValid(ObjectiveNotificationWidget) && ObjectiveNotificationWidget->IsPresentationSuppressed());
#endif
}

void ABHCharacter::ConfigureStrategicMissionPresentation()
{
    if (!IsValid(ObjectiveComponent))
    {
        return;
    }

    ObjectiveComponent->ClearObjectiveDisplayOverrides();

    UGameInstance* GameInstance = GetGameInstance();
    UBHWarSubsystem* WarSubsystem = GameInstance
        ? GameInstance->GetSubsystem<UBHWarSubsystem>()
        : nullptr;

    if (!IsValid(WarSubsystem) ||
        WarSubsystem->GetPriorityType() ==
            EBHWarPriorityType::None)
    {
        AssignedWarSectorID = NAME_None;
        AssignedWarSupplySourceSectorID = NAME_None;
        AssignedWarPriorityType = EBHWarPriorityType::None;
        return;
    }

    if (!bRuntimeWarOperation ||
        AssignedWarSectorID.IsNone() ||
        AssignedWarPriorityType == EBHWarPriorityType::None)
    {
        AssignedWarSectorID =
            WarSubsystem->GetPrioritySectorID();
        AssignedWarPriorityType =
            WarSubsystem->GetPriorityType();
        AssignedWarSupplySourceSectorID =
            WarSubsystem->GetOperationSupplySource(
                AssignedWarSectorID,
                AssignedWarPriorityType
            );
    }

    if (!bRuntimeWarOperation &&
        AssignedWarPriorityType == EBHWarPriorityType::Defend)
    {
        if (!IsValid(DefenseMissionDirector))
        {
            DefenseMissionDirector =
                GetWorld()->SpawnActor<ABHDefenseMissionDirector>();
        }

        if (IsValid(DefenseMissionDirector))
        {
            DefenseMissionDirector->InitializeDefenseMission(this);
        }
    }
    else if (IsValid(DefenseMissionDirector))
    {
        DefenseMissionDirector->Destroy();
        DefenseMissionDirector = nullptr;
    }

    const TArray<FName> ObjectiveIDs =
    {
        BHObjectiveIds::FindRedKeycard,
        BHObjectiveIds::UnlockSecurityDoor,
        BHObjectiveIds::ExploreBeyondSecurityDoor,
        BHObjectiveIds::EliminateGuard,
        BHObjectiveIds::ReachExtraction,
        BHObjectiveIds::DeliverResupply,
        BHObjectiveIds::ProtectConvoy,
        BHObjectiveIds::EvacuateCasualty,
        BHObjectiveIds::ObserveSector
    };

    for (const FName ObjectiveID : ObjectiveIDs)
    {
        const FText StrategicText =
            WarSubsystem->GetOperationObjectiveText(
                AssignedWarSectorID,
                AssignedWarPriorityType,
                ObjectiveID
            );

        if (!StrategicText.IsEmpty())
        {
            ObjectiveComponent->SetObjectiveDisplayOverride(
                ObjectiveID,
                StrategicText
            );
        }
    }

    RefreshStrategicBriefing();
}

void ABHCharacter::RefreshStrategicBriefing()
{
    if (!IsLocallyControlled()) { return; }
    RefreshMissionPresentationVisibility();
    const ABHWarGameState* State = GetWorld() ? GetWorld()->GetGameState<ABHWarGameState>() : nullptr;
    const FBHActiveOperationSnapshot Snapshot = IsValid(State) ? State->GetActiveOperationSnapshot() : FBHActiveOperationSnapshot();
    const bool bTacticalActivated = (Snapshot.Phase != EBHActiveOperationPhase::None && Snapshot.Phase != EBHActiveOperationPhase::Approach) ||
        (IsValid(OpenWorldOperationDirector) && OpenWorldOperationDirector->IsOperationActivated());
    if (bIsHandlingDeath || bIsHandlingMissionComplete || bTacticalActivated ||
        AssignedWarSectorID.IsNone() || AssignedWarPriorityType == EBHWarPriorityType::None ||
        !IsValid(ObjectiveComponent) || ObjectiveComponent->GetCurrentObjectiveID().IsNone())
    {
        if (IsValid(ObjectiveNotificationWidget))
        {
            ObjectiveNotificationWidget->CancelKeyedNotification(FName(TEXT("StrategicBriefing")));
        }
        return;
    }
    UGameInstance* Instance = GetGameInstance();
    const UBHWarSubsystem* War = IsValid(Instance) ? Instance->GetSubsystem<UBHWarSubsystem>() : nullptr;
    if (!IsValid(War)) { return; }
    const FString Context = FString::Printf(TEXT("%s:%d:%s"), *AssignedWarSectorID.ToString(),
        static_cast<int32>(AssignedWarPriorityType), *Snapshot.OperationID.ToString());
    if (Context == LastStrategicBriefingContext) { return; }
    APlayerController* LocalController = ResolveOwningPlayerController();
    if (!IsValid(LocalController) || !LocalController->GetLocalPlayer()) { return; }
    if (!IsValid(ObjectiveNotificationWidget) && ObjectiveNotificationWidgetClass)
    {
        ObjectiveNotificationWidget = CreateWidget<UBHObjectiveNotificationWidget>(LocalController, ObjectiveNotificationWidgetClass);
        if (IsValid(ObjectiveNotificationWidget)) { ObjectiveNotificationWidget->AddToViewport(); }
    }
    if (!IsValid(ObjectiveNotificationWidget)) { return; }
    RefreshMissionPresentationVisibility();
    LastStrategicBriefingContext = Context;
    // Generate from the current local assignment; an older authority RPC cannot overwrite this context.
    ObjectiveNotificationWidget->ShowKeyedNotification(FName(TEXT("StrategicBriefing")), FText::Format(
        NSLOCTEXT("BrokenHorizon", "StrategicMissionBriefingNotification", "STRATEGIC BRIEFING\n\n{0}\n\n{1}"),
        War->GetOperationTitle(AssignedWarSectorID, AssignedWarPriorityType),
        War->GetOperationMissionBriefing(AssignedWarSectorID, AssignedWarPriorityType)),
        EBHNotificationPriority::High, EBHNotificationAudioCue::StrategicWarning);
}

bool ABHCharacter::StartOpenWorldOperationDirector(
    bool bRestoringSavedState
)
{
    if (!bRuntimeWarOperation ||
        AssignedWarSectorID.IsNone() ||
        AssignedWarPriorityType == EBHWarPriorityType::None)
    {
        return false;
    }

    if (AssignedWarPriorityType == EBHWarPriorityType::Resupply ||
        AssignedWarPriorityType ==
            EBHWarPriorityType::EscortRescue ||
        AssignedWarPriorityType == EBHWarPriorityType::Rescue ||
        AssignedWarPriorityType == EBHWarPriorityType::Recon)
    {
        if (IsValid(OpenWorldOperationDirector))
        {
            OpenWorldOperationDirector->Destroy();
            OpenWorldOperationDirector = nullptr;
        }

        return true;
    }

    if (IsValid(OpenWorldOperationDirector))
    {
        OpenWorldOperationDirector->Destroy();
    }

    OpenWorldOperationDirector =
        GetWorld()->SpawnActor<ABHOpenWorldOperationDirector>();

    const UGameInstance* GameInstance = GetGameInstance();
    const UBHWarSubsystem* WarSubsystem = GameInstance
        ? GameInstance->GetSubsystem<UBHWarSubsystem>()
        : nullptr;
    if (AssignedWarSupplySourceSectorID.IsNone() &&
        IsValid(WarSubsystem))
    {
        AssignedWarSupplySourceSectorID =
            WarSubsystem->GetOperationSupplySource(
                AssignedWarSectorID,
                AssignedWarPriorityType
            );
    }

    const bool bOperationStarted =
        IsValid(OpenWorldOperationDirector) &&
        OpenWorldOperationDirector->StartOperation(
            this,
            AssignedWarSectorID,
            AssignedWarPriorityType,
            AssignedWarSupplySourceSectorID,
            bRestoringSavedState
        );

    if (!bOperationStarted &&
        IsValid(OpenWorldOperationDirector))
    {
        OpenWorldOperationDirector->Destroy();
        OpenWorldOperationDirector = nullptr;
    }

    return bOperationStarted;
}

void ABHCharacter::OnMissionCompleted()
{
    const int32 FriendlySupportLosses =
        bRuntimeWarOperation &&
        IsValid(OpenWorldOperationDirector)
            ? OpenWorldOperationDirector
                ->GetFriendlySupportCasualties()
            : 0;
    const int32 EnemyLosses =
        bRuntimeWarOperation &&
        IsValid(OpenWorldOperationDirector)
            ? OpenWorldOperationDirector->GetEnemyCasualties()
            : 0;
    const int32 EnemyRouted =
        bRuntimeWarOperation &&
        IsValid(OpenWorldOperationDirector)
            ? OpenWorldOperationDirector->GetEnemyRoutedCount()
            : 0;
    UGameInstance* GameInstance = GetGameInstance();
    UBHWarSubsystem* WarSubsystem = GameInstance
        ? GameInstance->GetSubsystem<UBHWarSubsystem>()
        : nullptr;

    if (IsValid(WarSubsystem))
    {
        const bool bHasDeploymentAssignment =
            !AssignedWarSectorID.IsNone() &&
            AssignedWarPriorityType !=
                EBHWarPriorityType::None;
        const FName ResolvedSectorID =
            bHasDeploymentAssignment
                ? AssignedWarSectorID
                : WarSubsystem->GetPrioritySectorID();
        const EBHWarPriorityType ResolvedMissionType =
            bHasDeploymentAssignment
                ? AssignedWarPriorityType
                : WarSubsystem->GetPriorityType();
        const FBHWarSectorState ResolvedSector =
            WarSubsystem->GetSectorState(ResolvedSectorID);
        const FName ResolvedOperationID =
            WarSubsystem->GetCommittedOperationID();
        const FName ResolvedOperationTargetID =
            WarSubsystem->GetCommittedOperationTargetID();
        bool bWarResultApplied = false;
        EBHRaidOperationalSignature RaidSignature =
            EBHRaidOperationalSignature::Contested;
        const FName FriendlyLossSectorID =
            !AssignedWarSupplySourceSectorID.IsNone()
                ? AssignedWarSupplySourceSectorID
                : WarSubsystem->GetOperationSupplySource(
                    ResolvedSectorID,
                    ResolvedMissionType
                );
        const FName EnemyLossSectorID =
            WarSubsystem->GetOperationEnemySource(
                ResolvedSectorID,
                ResolvedMissionType
            );

        if (bHasDeploymentAssignment)
        {
            WarSubsystem->ApplyOperationCasualtyResult(
                ResolvedSectorID,
                FriendlyLossSectorID,
                EnemyLossSectorID,
                FriendlySupportLosses,
                EnemyLosses
            );
            bWarResultApplied =
                WarSubsystem->ApplyMissionResult(
                    ResolvedSectorID,
                    ResolvedMissionType,
                    true
                );

            if (bWarResultApplied &&
                ResolvedMissionType ==
                    EBHWarPriorityType::Raid)
            {
                RaidSignature =
                    WarSubsystem->ApplyRaidOperationalSignature(
                        ResolvedSectorID,
                        EnemyLosses,
                        FriendlySupportLosses,
                        true,
                        IsValid(OpenWorldOperationDirector) &&
                            OpenWorldOperationDirector
                                ->WasRaidDetectedBeforeSabotage()
                    );
            }
        }
        else
        {
            bWarResultApplied =
                WarSubsystem->ResolvePriorityMission(true);
        }
        if (bWarResultApplied && EnemyRouted > 0)
        {
            WarSubsystem->ApplyOperationRoutResult(
                EnemyLossSectorID,
                EnemyRouted
            );
        }
        const float RecoveredMateriel =
            bWarResultApplied &&
                ResolvedMissionType !=
                    EBHWarPriorityType::Resupply &&
                ResolvedMissionType !=
                    EBHWarPriorityType::EscortRescue &&
                ResolvedMissionType !=
                    EBHWarPriorityType::Rescue &&
                ResolvedMissionType !=
                    EBHWarPriorityType::Recon
                ? WarSubsystem->RecoverBattlefieldMateriel(
                    ResolvedSectorID,
                    EnemyLosses,
                    FriendlySupportLosses,
                    true
                )
                : 0.0f;
        const FBHWarSectorState UpdatedSector =
            WarSubsystem->GetSectorState(ResolvedSectorID);
        const FBHOperationAfterActionRecord AfterAction =
            UBHWarSubsystem::BuildAfterActionRecord(
                ResolvedOperationID,
                ResolvedSectorID,
                ResolvedMissionType,
                true,
                FriendlySupportLosses,
                EnemyLosses,
                EnemyRouted,
                UpdatedSector.Supply - ResolvedSector.Supply,
                RecoveredMateriel,
                WarSubsystem->GetActiveTacticalOption(),
                WarSubsystem->GetActiveTacticalOptionSupplyCost()
            );
        if (bHasDeploymentAssignment && bWarResultApplied)
        {
            WarSubsystem->RecordOperationAfterAction(AfterAction);
        }
        const FBHWarSectorState StagingSector =
            WarSubsystem->GetSectorState(
                AssignedWarSupplySourceSectorID
            );

        if (!ResolvedSector.SectorID.IsNone() &&
            ResolvedMissionType != EBHWarPriorityType::None &&
            bWarResultApplied)
        {
            const FText OperationType =
                ResolvedMissionType ==
                    EBHWarPriorityType::Defend
                    ? NSLOCTEXT(
                        "BrokenHorizon",
                        "WarDefenseSuccess",
                        "DEFENSE"
                    )
                    : ResolvedMissionType ==
                            EBHWarPriorityType::Raid
                    ? NSLOCTEXT(
                        "BrokenHorizon",
                        "WarRaidSuccess",
                        "RAID"
                    )
                    : ResolvedMissionType ==
                            EBHWarPriorityType::Resupply
                    ? NSLOCTEXT(
                        "BrokenHorizon",
                        "WarResupplySuccess",
                        "RESUPPLY"
                    )
                    : ResolvedMissionType ==
                            EBHWarPriorityType::EscortRescue
                    ? NSLOCTEXT(
                        "BrokenHorizon",
                        "WarEscortSuccess",
                        "ESCORT"
                    )
                    : ResolvedMissionType ==
                            EBHWarPriorityType::Rescue
                    ? NSLOCTEXT(
                        "BrokenHorizon",
                        "WarRescueSuccess",
                        "RESCUE"
                    )
                    : ResolvedMissionType ==
                            EBHWarPriorityType::Recon
                    ? NSLOCTEXT(
                        "BrokenHorizon",
                        "WarReconSuccess",
                        "RECON"
                    )
                    : NSLOCTEXT(
                        "BrokenHorizon",
                        "WarAttackSuccess",
                        "ATTACK"
                    );
            const FText StagingSectorName =
                StagingSector.SectorID.IsNone()
                    ? NSLOCTEXT(
                        "BrokenHorizon",
                        "UnassignedStagingSector",
                        "UNASSIGNED"
                    )
                    : StagingSector.DisplayName;
            const int32 StagingSupply =
                StagingSector.SectorID.IsNone()
                    ? 0
                    : FMath::RoundToInt(
                        StagingSector.Supply
                    );
            const auto GetFactionLabel =
                [](EBHWarFaction Faction) -> FText
            {
                switch (Faction)
                {
                case EBHWarFaction::Friendly:
                    return NSLOCTEXT(
                        "BrokenHorizon",
                        "WarFactionFriendlyDebrief",
                        "FRIENDLY"
                    );
                case EBHWarFaction::Enemy:
                    return NSLOCTEXT(
                        "BrokenHorizon",
                        "WarFactionEnemyDebrief",
                        "ENEMY"
                    );
                default:
                    return NSLOCTEXT(
                        "BrokenHorizon",
                        "WarFactionNeutralDebrief",
                        "NEUTRAL"
                    );
                }
            };
            const FText RaidSignatureLabel =
                RaidSignature ==
                    EBHRaidOperationalSignature::Clean
                    ? NSLOCTEXT(
                        "BrokenHorizon",
                        "CleanRaidSignatureDebrief",
                        "CLEAN"
                    )
                    : RaidSignature ==
                        EBHRaidOperationalSignature::Loud
                        ? NSLOCTEXT(
                            "BrokenHorizon",
                            "LoudRaidSignatureDebrief",
                            "LOUD"
                        )
                        : NSLOCTEXT(
                            "BrokenHorizon",
                            "ContestedRaidSignatureDebrief",
                            "CONTESTED"
                        );
            const FText SectorOutcome =
                ResolvedMissionType ==
                    EBHWarPriorityType::Defend
                    ? NSLOCTEXT(
                        "BrokenHorizon",
                        "WarSectorHeldDebrief",
                        "SECTOR HELD"
                    )
                    : ResolvedMissionType ==
                            EBHWarPriorityType::Raid
                    ? FText::Format(
                        NSLOCTEXT(
                            "BrokenHorizon",
                            "WarLogisticsDisruptedDebrief",
                            "LOGISTICS DISRUPTED // {0} SIGNATURE"
                        ),
                        RaidSignatureLabel
                    )
                    : ResolvedMissionType ==
                            EBHWarPriorityType::Resupply
                    ? NSLOCTEXT(
                        "BrokenHorizon",
                        "WarSupplyLineRestoredDebrief",
                        "SUPPLY LINE RESTORED"
                    )
                    : ResolvedMissionType ==
                            EBHWarPriorityType::EscortRescue
                    ? NSLOCTEXT(
                        "BrokenHorizon",
                        "WarConvoySecuredDebrief",
                        "PROTECTED CONVOY SECURED"
                    )
                    : ResolvedMissionType ==
                            EBHWarPriorityType::Rescue
                    ? ResolvedOperationTargetID.IsNone()
                        ? NSLOCTEXT(
                            "BrokenHorizon",
                            "WarCasualtyEvacuatedDebrief",
                            "CASUALTY EVACUATED"
                        )
                        : FText::Format(
                            NSLOCTEXT(
                                "BrokenHorizon",
                                "WarCasualtyEvacuatedAssignedDebrief",
                                "CASUALTY {0} EVACUATED"
                            ),
                            FText::FromName(
                                ResolvedOperationTargetID
                            )
                        )
                    : ResolvedMissionType ==
                            EBHWarPriorityType::Recon
                    ? NSLOCTEXT(
                        "BrokenHorizon",
                        "WarReconConfirmedDebrief",
                        "INTELLIGENCE CONFIRMED"
                    )
                    : ResolvedSector.Owner !=
                            EBHWarFaction::Friendly &&
                        UpdatedSector.Owner ==
                            EBHWarFaction::Friendly
                        ? NSLOCTEXT(
                            "BrokenHorizon",
                            "WarSectorCapturedDebrief",
                            "SECTOR CAPTURED"
                        )
                        : NSLOCTEXT(
                            "BrokenHorizon",
                            "WarSectorSecuredDebrief",
                            "SECTOR SECURED"
                        );
            const FText FriendlyStrengthDelta =
                FText::FromString(
                    FString::Printf(
                        TEXT("%+d"),
                        FMath::RoundToInt(
                            UpdatedSector.FriendlyStrength -
                            ResolvedSector.FriendlyStrength
                        )
                    )
                );
            const FText EnemyStrengthDelta =
                FText::FromString(
                    FString::Printf(
                        TEXT("%+d"),
                        FMath::RoundToInt(
                            UpdatedSector.EnemyStrength -
                            ResolvedSector.EnemyStrength
                        )
                    )
                );
            const FText SupplyDelta =
                FText::FromString(
                    FString::Printf(
                        TEXT("%+d%%"),
                        FMath::RoundToInt(
                            UpdatedSector.Supply -
                            ResolvedSector.Supply
                        )
                    )
                );
            FFormatNamedArguments DebriefArguments;
            DebriefArguments.Add(
                TEXT("OperationType"),
                OperationType
            );
            DebriefArguments.Add(
                TEXT("SectorName"),
                ResolvedSector.DisplayName
            );
            DebriefArguments.Add(
                TEXT("Outcome"),
                SectorOutcome
            );
            DebriefArguments.Add(
                TEXT("PreviousOwner"),
                GetFactionLabel(ResolvedSector.Owner)
            );
            DebriefArguments.Add(
                TEXT("CurrentOwner"),
                GetFactionLabel(UpdatedSector.Owner)
            );
            DebriefArguments.Add(
                TEXT("FriendlyDelta"),
                FriendlyStrengthDelta
            );
            DebriefArguments.Add(
                TEXT("EnemyDelta"),
                EnemyStrengthDelta
            );
            DebriefArguments.Add(TEXT("SupplyDelta"), SupplyDelta);
            DebriefArguments.Add(
                TEXT("FriendlyStrength"),
                FText::AsNumber(
                    FMath::RoundToInt(
                        UpdatedSector.FriendlyStrength
                    )
                )
            );
            DebriefArguments.Add(
                TEXT("EnemyStrength"),
                FText::AsNumber(
                    FMath::RoundToInt(
                        UpdatedSector.EnemyStrength
                    )
                )
            );
            DebriefArguments.Add(
                TEXT("SectorSupply"),
                FText::AsNumber(
                    FMath::RoundToInt(UpdatedSector.Supply)
                )
            );
            DebriefArguments.Add(
                TEXT("EnemyResponse"),
                WarSubsystem->GetSectorEnemyResponseSummary(
                    ResolvedSectorID
                )
            );
            DebriefArguments.Add(
                TEXT("StagingSector"),
                StagingSectorName
            );
            DebriefArguments.Add(
                TEXT("StagingSupply"),
                FText::AsNumber(StagingSupply)
            );
            DebriefArguments.Add(
                TEXT("SupportLosses"),
                FText::AsNumber(FriendlySupportLosses)
            );
            DebriefArguments.Add(
                TEXT("HostileLosses"),
                FText::AsNumber(EnemyLosses)
            );
            DebriefArguments.Add(
                TEXT("HostileRouted"),
                FText::AsNumber(EnemyRouted)
            );
            DebriefArguments.Add(
                TEXT("RecoveredMateriel"),
                FText::AsNumber(
                    FMath::RoundToInt(RecoveredMateriel)
                )
            );
            DebriefArguments.Add(
                TEXT("WarTurn"),
                FText::AsNumber(WarSubsystem->GetTurnNumber())
            );
            DebriefArguments.Add(
                TEXT("FriendlyControl"),
                FText::AsNumber(
                    FMath::RoundToInt(
                        WarSubsystem
                            ->GetFriendlyControlPercentage()
                    )
                )
            );
            const TCHAR* AfterActionGrade =
                AfterAction.Grade == EBHAfterActionGrade::Exceptional
                    ? TEXT("EXCEPTIONAL")
                    : AfterAction.Grade == EBHAfterActionGrade::Strong
                        ? TEXT("STRONG")
                        : AfterAction.Grade == EBHAfterActionGrade::Effective
                            ? TEXT("EFFECTIVE")
                            : TEXT("DIMINISHED");
            DebriefArguments.Add(
                TEXT("AfterActionGrade"),
                FText::FromString(AfterActionGrade)
            );
            DebriefArguments.Add(
                TEXT("AfterActionScore"),
                FText::AsNumber(AfterAction.TotalScore)
            );
            DebriefArguments.Add(
                TEXT("CampaignMerit"),
                FText::AsNumber(
                    WarSubsystem->GetCampaignProgression()
                        .CampaignMerit
                )
            );
            const TCHAR* TacticalPlanName =
                AfterAction.TacticalOption ==
                        EBHOperationTacticalOption::ReconPlanning
                    ? TEXT("RECON")
                    : AfterAction.TacticalOption ==
                            EBHOperationTacticalOption::ReinforcementPriority
                        ? TEXT("REINFORCEMENT")
                        : AfterAction.TacticalOption ==
                                EBHOperationTacticalOption::MedicalPreparation
                            ? TEXT("MEDICAL")
                            : TEXT("STANDARD");
            DebriefArguments.Add(
                TEXT("TacticalPlan"),
                FText::FromString(TacticalPlanName)
            );
            DebriefArguments.Add(
                TEXT("TacticalCost"),
                FText::AsNumber(
                    FMath::RoundToInt(
                        AfterAction.TacticalSupplyCost
                    )
                )
            );
            DebriefArguments.Add(
                TEXT("TacticalScore"),
                FText::AsNumber(
                    AfterAction.TacticalExecutionScore
                )
            );

            MissionCompleteMessage = FText::Format(
                NSLOCTEXT(
                    "BrokenHorizon",
                    "WarMissionCompleteMessage",
                    "MISSION COMPLETE\n\n"
                    "{OperationType} SUCCESS // {SectorName} // "
                    "{Outcome}\n"
                    "CONTROL {PreviousOwner} -> {CurrentOwner}\n"
                    "OPERATION IMPACT // FRIENDLY {FriendlyDelta} // "
                    "ENEMY {EnemyDelta} // SUPPLY {SupplyDelta}\n"
                    "SECTOR FORCE // FRIENDLY {FriendlyStrength} // "
                    "ENEMY {EnemyStrength} // SUPPLY {SectorSupply}%\n"
                    "{EnemyResponse}\n"
                    "STAGING {StagingSector} // "
                    "SUPPLY {StagingSupply}% // "
                    "SUPPORT LOSSES {SupportLosses} // "
                    "HOSTILE LOSSES {HostileLosses} // "
                    "ROUTED {HostileRouted}\n"
                    "BATTLEFIELD RECOVERY "
                    "+{RecoveredMateriel} SUPPLY\n"
                    "AFTER ACTION // {AfterActionGrade} // "
                    "SCORE +{AfterActionScore} // MERIT {CampaignMerit}\n"
                    "TACTICAL PLAN // {TacticalPlan} // "
                    "COST {TacticalCost} // EFFECT +{TacticalScore}\n"
                    "WAR TURN {WarTurn} // "
                    "FRIENDLY CONTROL {FriendlyControl}%"
                ),
                DebriefArguments
            );

            UE_LOG(
                LogTemp,
                Log,
                TEXT(
                    "Mission victory applied to war sector %s "
                    "on turn %d staging=%s support_losses=%d "
                    "hostile_losses=%d hostile_routed=%d "
                    "recovered_supply=%.1f."
                ),
                *ResolvedSectorID.ToString(),
                WarSubsystem->GetTurnNumber(),
                *AssignedWarSupplySourceSectorID.ToString(),
                FriendlySupportLosses,
                EnemyLosses,
                EnemyRouted,
                RecoveredMateriel
            );
        }
    }

    EnterMissionCompleteState(true);
}

void ABHCharacter::EnterMissionCompleteState(
    bool bSaveProgress
)
{
    if (bRuntimeWarOperation &&
        AssignedWarPriorityType != EBHWarPriorityType::None &&
        !MissionCompleteMessage.ToString().Contains(
            TEXT("NEXT DEPLOYMENT //")
        ))
    {
        MissionCompleteMessage = FText::Format(
            NSLOCTEXT(
                "BrokenHorizon",
                "PersistentOperationDebriefFieldForce",
                "{0}\n\n{1}"
            ),
            MissionCompleteMessage,
            FText::FromString(
                UBHCombatStatusWidget::
                    BuildFieldSquadDebriefStatusLabel(
                        GetLivingFieldSquadCount(),
                        GetIncapacitatedFieldSquadCount(),
                        GetFieldSquadMembersRequiringEvacuationCount(),
                        GetFieldSquadMembersNeedingServiceCount()
                    )
            )
        );
    }

    if (bIsHandlingMissionComplete)
    {
        return;
    }

    bIsHandlingMissionComplete = true;
    RefreshMissionPresentationVisibility();

    if (bWarMapOpen)
    {
        CloseWarMap();
    }

    FinishTraversal(false);

    if (IsValid(InjuryComponent))
    {
        InjuryComponent->CancelMedkitTreatment();
    }

    if (IsValid(WeaponComponent))
    {
        WeaponComponent->StopAllActions();
    }

    StopSprint();
    GetCharacterMovement()->DisableMovement();

    if (IsValid(InteractionPromptWidget))
    {
        InteractionPromptWidget->SetVisibility(
            ESlateVisibility::Collapsed
        );
    }

    if (IsValid(AmmoHUDWidget))
    {
        AmmoHUDWidget->SetVisibility(
            ESlateVisibility::Collapsed
        );
    }

    if (IsValid(CombatStatusWidget))
    {
        CombatStatusWidget->SetVisibility(
            ESlateVisibility::Collapsed
        );
    }

    if (IsValid(ObjectiveWidget))
    {
        ObjectiveWidget->SetVisibility(
            ESlateVisibility::Collapsed
        );
    }

    APlayerController* PlayerController =
        ResolveOwningPlayerController();

    if (IsValid(PlayerController))
    {
        DisableInput(PlayerController);
        PlayerController->SetIgnoreMoveInput(true);
        PlayerController->SetIgnoreLookInput(true);
    }

    if (IsValid(PlayerController) &&
        PlayerController->GetLocalPlayer() != nullptr)
    {
        TSubclassOf<UBHMissionCompleteWidget> DebriefWidgetClass =
            MissionCompleteWidgetClass;

        if (!DebriefWidgetClass)
        {
            DebriefWidgetClass =
                UBHMissionCompleteWidget::StaticClass();
        }

        MissionCompleteWidget =
            CreateWidget<UBHMissionCompleteWidget>(
                PlayerController,
                DebriefWidgetClass
            );

        if (!IsValid(MissionCompleteWidget) &&
            DebriefWidgetClass !=
                UBHMissionCompleteWidget::StaticClass())
        {
            MissionCompleteWidget =
                CreateWidget<UBHMissionCompleteWidget>(
                    PlayerController,
                    UBHMissionCompleteWidget::StaticClass()
                );
        }

        if (IsValid(MissionCompleteWidget))
        {
            MissionCompleteWidget->SetIsFocusable(true);
            MissionCompleteWidget->OnContinueRequested.AddDynamic(
                this,
                &ABHCharacter::HandleMissionContinueRequested
            );
            MissionCompleteWidget->SetMissionCompleteText(
                MissionCompleteMessage
            );
            MissionCompleteWidget->AddToViewport(200);

            FInputModeGameAndUI InputMode;
            InputMode.SetWidgetToFocus(
                MissionCompleteWidget->TakeWidget()
            );
            InputMode.SetLockMouseToViewportBehavior(
                EMouseLockMode::DoNotLock
            );
            InputMode.SetHideCursorDuringCapture(false);
            PlayerController->SetInputMode(InputMode);
            PlayerController->bShowMouseCursor = true;
            MissionCompleteWidget->SetKeyboardFocus();
        }
        else
        {
            UE_LOG(
                LogTemp,
                Error,
                TEXT(
                    "BH_OPERATION_DEBRIEF_WIDGET_FAILED "
                    "native_fallback=failed"
                )
            );
        }
    }

    if (bSaveProgress)
    {
        UBHSaveSubsystem* SaveSubsystem = GetGameInstance()
            ? GetGameInstance()->GetSubsystem<UBHSaveSubsystem>()
            : nullptr;

        const bool bOperationResultSaved =
            IsValid(SaveSubsystem) &&
            SaveSubsystem->SaveProgress();

        if (!bOperationResultSaved)
        {
            UE_LOG(
                LogTemp,
                Error,
                TEXT(
                    "BH_OPERATION_RESULT_CHECKPOINT_FAILED "
                    "continue=locked"
                )
            );

            if (IsValid(MissionCompleteWidget))
            {
                MissionCompleteWidget->SetMissionCompleteText(
                    FText::Format(
                        NSLOCTEXT(
                            "BrokenHorizon",
                            "MissionCompleteCheckpointPending",
                            "{0}\n\n"
                            "CHECKPOINT PENDING // "
                            "Press CONTINUE to retry before returning "
                            "to strategic command."
                        ),
                        MissionCompleteMessage
                    )
                );
            }
        }
        else
        {
            UE_LOG(
                LogTemp,
                Display,
                TEXT(
                    "BH_OPERATION_RESULT_CHECKPOINT_SAVED"
                )
            );
        }
    }
}

void ABHCharacter::EnterCampaignEpilogueFreeRoam(
    bool bOpenStrategicMap
)
{
    bCampaignEpilogueAcknowledged = true;
    bIsHandlingMissionComplete = false;
    bRuntimeWarOperation = false;
    AssignedWarSectorID = NAME_None;
    AssignedWarSupplySourceSectorID = NAME_None;
    AssignedWarPriorityType = EBHWarPriorityType::None;

    if (HasAuthority() && GetWorld() != nullptr)
    {
        if (ABHWarGameState* WarGameState =
                GetWorld()->GetGameState<ABHWarGameState>())
        {
            WarGameState->ClearActiveOperationSnapshot();
        }
    }

    if (IsValid(MissionCompleteWidget))
    {
        MissionCompleteWidget->OnContinueRequested.RemoveDynamic(
            this,
            &ABHCharacter::HandleMissionContinueRequested
        );
        MissionCompleteWidget->RemoveFromParent();
        MissionCompleteWidget = nullptr;
    }

    if (IsValid(OpenWorldOperationDirector))
    {
        OpenWorldOperationDirector->Destroy();
        OpenWorldOperationDirector = nullptr;
    }

    if (IsValid(ObjectiveComponent))
    {
        ObjectiveComponent->ClearMissionState();
    }

    GetCharacterMovement()->SetMovementMode(MOVE_Walking);

    RefreshMissionPresentationVisibility();

    if (IsValid(AmmoHUDWidget))
    {
        AmmoHUDWidget->SetVisibility(ESlateVisibility::Visible);
    }

    if (IsValid(CombatStatusWidget))
    {
        CombatStatusWidget->SetVisibility(
            ESlateVisibility::Visible
        );
    }

    APlayerController* PlayerController =
        ResolveOwningPlayerController();

    if (IsValid(PlayerController))
    {
        EnableInput(PlayerController);
        PlayerController->SetIgnoreMoveInput(false);
        PlayerController->SetIgnoreLookInput(false);
        PlayerController->SetInputMode(FInputModeGameOnly());
        PlayerController->bShowMouseCursor = false;
    }

    UE_LOG(
        LogTemp,
        Display,
        TEXT(
            "BH_CAMPAIGN_EPILOGUE_ENTERED strategic_map=%d"
        ),
        bOpenStrategicMap ? 1 : 0
    );

    if (bOpenStrategicMap)
    {
        OpenWarMap(false);
    }
}

void ABHCharacter::EnterPostOperationFreeRoam(
    bool bOpenStrategicMap
)
{
    bOperationDebriefAcknowledged = true;
    bIsHandlingMissionComplete = false;
    bRuntimeWarOperation = false;
    AssignedWarSectorID = NAME_None;
    AssignedWarSupplySourceSectorID = NAME_None;
    AssignedWarPriorityType = EBHWarPriorityType::None;

    if (HasAuthority() && GetWorld() != nullptr)
    {
        if (ABHWarGameState* WarGameState =
                GetWorld()->GetGameState<ABHWarGameState>())
        {
            WarGameState->ClearActiveOperationSnapshot();
        }
    }

    if (IsValid(MissionCompleteWidget))
    {
        MissionCompleteWidget->OnContinueRequested.RemoveDynamic(
            this,
            &ABHCharacter::HandleMissionContinueRequested
        );
        MissionCompleteWidget->RemoveFromParent();
        MissionCompleteWidget = nullptr;
    }

    if (IsValid(OpenWorldOperationDirector))
    {
        OpenWorldOperationDirector->Destroy();
        OpenWorldOperationDirector = nullptr;
    }

    if (IsValid(ObjectiveComponent))
    {
        ObjectiveComponent->ClearMissionState();
    }

    GetCharacterMovement()->SetMovementMode(MOVE_Walking);

    RefreshMissionPresentationVisibility();

    if (IsValid(AmmoHUDWidget))
    {
        AmmoHUDWidget->SetVisibility(
            ESlateVisibility::Visible
        );
    }

    if (IsValid(CombatStatusWidget))
    {
        CombatStatusWidget->SetVisibility(
            ESlateVisibility::Visible
        );
    }

    APlayerController* PlayerController =
        ResolveOwningPlayerController();

    if (IsValid(PlayerController))
    {
        EnableInput(PlayerController);
        PlayerController->SetIgnoreMoveInput(false);
        PlayerController->SetIgnoreLookInput(false);
        PlayerController->SetInputMode(FInputModeGameOnly());
        PlayerController->bShowMouseCursor = false;
    }

    UE_LOG(
        LogTemp,
        Display,
        TEXT(
            "BH_POST_OPERATION_FREE_ROAM_ENTERED "
            "strategic_map=%d"
        ),
        bOpenStrategicMap ? 1 : 0
    );

    if (bOpenStrategicMap)
    {
        OpenWarMap(true);
    }
}

void ABHCharacter::OnRep_PlayerCasualtyState()
{
    if (IsLocallyControlled())
    {
        ClientSetPlayerCasualtyPresentation_Implementation(
            bPlayerIncapacitated,
            bPlayerCasualtyStabilized,
            GetPlayerBleedOutSecondsRemaining()
        );
    }
}

void ABHCharacter::EnterPlayerIncapacitation(AActor* DamageCauser)
{
    if (!HasAuthority() || bPlayerIncapacitated)
    {
        return;
    }
    bPlayerIncapacitated = true;
    bPlayerCasualtyStabilized = false;
    bHasEnteredPlayerCasualtyThisLife = true;
    bIsHandlingDeath = true;
    PlayerBleedOutDeadline = GetWorld()->GetTimeSeconds() +
        FMath::Max(5.0f, PlayerBleedOutDuration);
    if (IsValid(HealthComponent))
    {
        HealthComponent->ReviveAtHealth(1.0f);
    }
    if (IsValid(InjuryComponent))
    {
        InjuryComponent->CancelMedkitTreatment();
    }
    if (IsValid(WeaponComponent))
    {
        WeaponComponent->StopAllActions();
    }
    GetCharacterMovement()->DisableMovement();
    GetWorldTimerManager().SetTimer(
        PlayerBleedOutTimerHandle,
        this,
        &ABHCharacter::HandlePlayerBleedOutExpired,
        FMath::Max(5.0f, PlayerBleedOutDuration),
        false
    );
    ClientSetPlayerCasualtyPresentation(
        true,
        false,
        FMath::Max(5.0f, PlayerBleedOutDuration)
    );
    for (TActorIterator<ABHCharacter> It(GetWorld()); It; ++It)
    {
        ABHCharacter* Teammate = *It;
        if (IsValid(Teammate) && Teammate != this &&
            Teammate->IsPlayerControlled())
        {
            Teammate->ShowPriorityStatusNotification(
                NSLOCTEXT(
                    "BrokenHorizon",
                    "PlayerCasualtyTeamAlert",
                    "TEAMMATE DOWN // 45 SECOND WINDOW\n\n"
                    "Stabilize with a field dressing, then revive with a medkit."
                ),
                EBHNotificationPriority::Critical
            );
        }
    }
    ForceNetUpdate();
    UE_LOG(
        LogTemp,
        Display,
        TEXT("BH_PLAYER_CASUALTY state=incapacitated causer=%s deadline=%.1f"),
        IsValid(DamageCauser) ? *DamageCauser->GetName() : TEXT("None"),
        PlayerBleedOutDeadline
    );
}

void ABHCharacter::HandlePlayerBleedOutExpired()
{
    if (!HasAuthority() || !bPlayerIncapacitated ||
        bPlayerCasualtyStabilized || !IsValid(HealthComponent))
    {
        return;
    }
    HealthComponent->ApplyDamage(
        HealthComponent->GetCurrentHealth() + 1.0f,
        this
    );
}

void ABHCharacter::FinalizePlayerCasualtyDeath(AActor* DamageCauser)
{
    GetWorldTimerManager().ClearTimer(PlayerBleedOutTimerHandle);
    bPlayerIncapacitated = false;
    bPlayerCasualtyStabilized = false;
    PlayerBleedOutDeadline = 0.0f;
    bIsHandlingDeath = false;
    ClientSetPlayerCasualtyPresentation(false, false, 0.0f);
    ForceNetUpdate();
    HandleDeath(DamageCauser);
}

void ABHCharacter::TryTreatPlayerCasualty(ABHCharacter* Casualty)
{
    if (!HasAuthority() || !IsValid(Casualty) || Casualty == this ||
        !Casualty->IsPlayerIncapacitated() || bPlayerIncapacitated ||
        !IsValid(InjuryComponent) ||
        FVector::DistSquared(GetActorLocation(), Casualty->GetActorLocation()) >
            FMath::Square(InteractionDistance + 100.0f))
    {
        return;
    }
    if (!Casualty->IsPlayerCasualtyStabilized())
    {
        if (!InjuryComponent->ConsumeFieldDressingForSquadAid())
        {
            ShowStatusNotification(NSLOCTEXT(
                "BrokenHorizon", "PlayerCasualtyNoDressing",
                "CASUALTY AID FAILED // FIELD DRESSING REQUIRED"
            ));
            return;
        }
        Casualty->bPlayerCasualtyStabilized = true;
        Casualty->PlayerBleedOutDeadline = 0.0f;
        Casualty->GetWorldTimerManager().ClearTimer(
            Casualty->PlayerBleedOutTimerHandle
        );
        if (IsValid(Casualty->InjuryComponent))
        {
            Casualty->InjuryComponent->ClearBleedingForSquadAid();
        }
        Casualty->ClientSetPlayerCasualtyPresentation(true, true, 0.0f);
        Casualty->ForceNetUpdate();
        ShowStatusNotification(NSLOCTEXT(
            "BrokenHorizon", "PlayerCasualtyStabilizedReviver",
            "CASUALTY STABILIZED // MEDKIT REQUIRED TO REVIVE"
        ));
        UE_LOG(LogTemp, Display, TEXT("BH_PLAYER_CASUALTY state=stabilized"));
        return;
    }
    if (!InjuryComponent->ConsumeMedkitForSquadAid())
    {
        ShowStatusNotification(NSLOCTEXT(
            "BrokenHorizon", "PlayerCasualtyNoMedkit",
            "REVIVE FAILED // MEDKIT REQUIRED"
        ));
        return;
    }
    Casualty->CompletePlayerRevive(this);
}

void ABHCharacter::CompletePlayerRevive(ABHCharacter* Reviver)
{
    if (!HasAuthority() || !bPlayerIncapacitated ||
        !bPlayerCasualtyStabilized)
    {
        return;
    }
    bPlayerIncapacitated = false;
    bPlayerCasualtyStabilized = false;
    PlayerBleedOutDeadline = 0.0f;
    bIsHandlingDeath = false;
    GetWorldTimerManager().ClearTimer(PlayerBleedOutTimerHandle);
    if (IsValid(HealthComponent))
    {
        HealthComponent->ReviveAtHealth(PlayerReviveHealth);
    }
    GetCharacterMovement()->SetMovementMode(MOVE_Walking);
    ApplyMovementSpeed();
    ClientSetPlayerCasualtyPresentation(false, false, 0.0f);
    ShowPriorityStatusNotification(NSLOCTEXT(
        "BrokenHorizon", "PlayerCasualtyRevived",
        "REVIVED // COMBAT EFFECTIVE\n\nHealth restored to 35. Reassess injuries."
    ), EBHNotificationPriority::High);
    ForceNetUpdate();
    UE_LOG(
        LogTemp,
        Display,
        TEXT("BH_PLAYER_CASUALTY state=revived reviver=%s health=%.1f"),
        IsValid(Reviver) ? *Reviver->GetName() : TEXT("None"),
        IsValid(HealthComponent) ? HealthComponent->GetCurrentHealth() : 0.0f
    );
}

void ABHCharacter::HandleDeath(AActor* DamageCauser)
{
    if (HasAuthority() && bPlayerIncapacitated)
    {
        FinalizePlayerCasualtyDeath(DamageCauser);
        return;
    }

    bool bHasAvailableTeammate = false;
    if (HasAuthority() && IsValid(GetWorld()))
    {
        for (TActorIterator<ABHCharacter> It(GetWorld()); It; ++It)
        {
            const ABHCharacter* Teammate = *It;
            if (IsValid(Teammate) && Teammate != this &&
                Teammate->IsPlayerControlled() &&
                !Teammate->IsPlayerIncapacitated() &&
                IsValid(Teammate->HealthComponent) &&
                !Teammate->HealthComponent->IsDead())
            {
                bHasAvailableTeammate = true;
                break;
            }
        }
    }
    const bool bRuntimeProbe = FParse::Param(
        FCommandLine::Get(), TEXT("BHTestPlayerCasualtyRuntime")
    );
    const bool bCooperativeWorld = IsValid(GetWorld()) &&
        ((GetWorld()->GetNetMode() != NM_Standalone &&
          bHasAvailableTeammate) || bRuntimeProbe);
    if (HasAuthority() && CanEnterCooperativeCasualty(
            bCooperativeWorld,
            bPlayerIncapacitated,
            bHasEnteredPlayerCasualtyThisLife))
    {
        EnterPlayerIncapacitation(DamageCauser);
        return;
    }

    if (bIsHandlingDeath || bIsHandlingMissionComplete)
    {
        return;
    }

    bIsHandlingDeath = true;
    AuthoritativePlayerSuppression = 0.0f;
    LocalPlayerSuppressionPresentation = 0.0f;

    if (bWarMapOpen)
    {
        CloseWarMap();
    }

    FinishTraversal(false);

    if (IsValid(InjuryComponent))
    {
        InjuryComponent->CancelMedkitTreatment();
    }

    if (IsValid(WeaponComponent))
    {
        WeaponComponent->StopAllActions();
    }

    GetCharacterMovement()->DisableMovement();

    APlayerController* PlayerController =
        ResolveOwningPlayerController();

    if (HasAuthority() &&
        IsValid(PlayerController) &&
        PlayerController->GetLocalPlayer() == nullptr)
    {
        ClientPresentDeath(RespawnDelay);
    }

    if (IsValid(PlayerController))
    {
        DisableInput(PlayerController);
        PlayerController->SetIgnoreMoveInput(true);
        PlayerController->SetIgnoreLookInput(true);
    }

    if (IsValid(InteractionPromptWidget))
    {
        InteractionPromptWidget->SetVisibility(
            ESlateVisibility::Collapsed
        );
    }

    if (IsValid(AmmoHUDWidget))
    {
        AmmoHUDWidget->SetVisibility(
            ESlateVisibility::Collapsed
        );
    }

    if (IsValid(CombatStatusWidget))
    {
        CombatStatusWidget->SetVisibility(
            ESlateVisibility::Collapsed
        );
    }

    if (!IsValid(DeathWidget) &&
        DeathWidgetClass &&
        IsValid(PlayerController) &&
        PlayerController->GetLocalPlayer() != nullptr)
    {
        DeathWidget = CreateWidget<UBHDeathWidget>(
            PlayerController,
            DeathWidgetClass
        );

        if (IsValid(DeathWidget))
        {
            DeathWidget->AddToViewport(100);
        }
    }

    if (IsValid(DeathWidget))
    {
        DeathWidget->ShowDeathScreenWithRespawnDelay(
            RespawnDelay
        );
    }

    UWorld* World = GetWorld();

    if (!IsValid(World) || RespawnDelay <= 0.0f)
    {
        RespawnAfterDeath();
        return;
    }

    World->GetTimerManager().SetTimer(
        RespawnTimerHandle,
        this,
        &ABHCharacter::RespawnAfterDeath,
        RespawnDelay,
        false
    );
}

void ABHCharacter::HandleHealthChanged(
    float NewCurrentHealth,
    float NewMaxHealth
)
{
    if (IsValid(CombatStatusWidget))
    {
        CombatStatusWidget->SetHealth(
            NewCurrentHealth,
            NewMaxHealth
        );
    }
}

void ABHCharacter::HandlePlayerDamaged(
    float DamageApplied,
    AActor* DamageCauser
)
{
    if (!IsValid(HealthComponent))
    {
        return;
    }

    const bool bShouldInterruptReload = !HealthComponent->IsDispatchingOngoingDamage();
    const bool bReloadInterrupted = bShouldInterruptReload && IsValid(WeaponComponent) &&
        WeaponComponent->InterruptReload(FName(TEXT("damage")));
    if (bReloadInterrupted && IsLocallyControlled())
    {
        ShowStatusNotification(
            NSLOCTEXT(
                "BrokenHorizon",
                "ReloadInterruptedByDamage",
                "RELOAD INTERRUPTED // INCOMING FIRE\n\n"
                "Break contact before clearing the weapon."
            )
        );
    }

    FVector DamageSourceDirection = FVector::ZeroVector;

    if (IsValid(DamageCauser))
    {
        DamageSourceDirection = (
            DamageCauser->GetActorLocation() - GetActorLocation()
        ).GetSafeNormal();
    }

    const float HealthPercentage =
        HealthComponent->GetHealthPercentage();

    if (HasAuthority() && !IsLocallyControlled())
    {
        ClientNotifyCombatDamage(
            DamageApplied,
            HealthPercentage,
            DamageSourceDirection,
            DamageCauser,
            bShouldInterruptReload
        );
        return;
    }

    DisplayCombatDamageLocally(
        DamageApplied,
        HealthPercentage,
        DamageSourceDirection,
        DamageCauser
    );
}

void ABHCharacter::HandleAmmoChanged(
    int32 MagazineAmmo,
    int32 ReserveAmmo
)
{
    if (IsValid(AmmoHUDWidget))
    {
        AmmoHUDWidget->SetAmmo(
            MagazineAmmo,
            ReserveAmmo
        );
    }
    RefreshOpenInventoryPanel();
}

void ABHCharacter::HandleInjuryStateChanged(
    bool bBleeding,
    float BleedRate,
    bool bArmInjured,
    bool bLegInjured,
    int32 FieldDressings
)
{
    ApplyMovementSpeed();

    if (IsValid(CombatStatusWidget))
    {
        CombatStatusWidget->SetInjuryState(
            bBleeding,
            BleedRate,
            bArmInjured,
            bLegInjured,
            FieldDressings
        );
    }
    RefreshOpenInventoryPanel();
}

void ABHCharacter::HandleMedicalStateChanged(
    int32 Medkits,
    float HelmetDurabilityPercentage,
    float BodyArmorDurabilityPercentage,
    bool bTreatmentActive,
    float TreatmentProgress
)
{
    if (IsValid(CombatStatusWidget))
    {
        CombatStatusWidget->SetMedicalState(
            Medkits,
            HelmetDurabilityPercentage,
            BodyArmorDurabilityPercentage,
            bTreatmentActive,
            TreatmentProgress
        );
    }
    RefreshOpenInventoryPanel();
}

void ABHCharacter::HandleMedkitTreatmentCompleted()
{
    SavePlayerConditionCheckpoint(TEXT("Medkit"));
}

void ABHCharacter::SavePlayerConditionCheckpoint(
    FName Reason
)
{
    UBHSaveSubsystem* SaveSubsystem =
        GetGameInstance()
            ? GetGameInstance()->GetSubsystem<
                UBHSaveSubsystem>()
            : nullptr;

    if (!IsValid(SaveSubsystem) ||
        !SaveSubsystem->SaveProgress())
    {
        UE_LOG(
            LogTemp,
            Warning,
            TEXT(
                "BH_PLAYER_CONDITION_CHECKPOINT_FAILED "
                "reason=%s"
            ),
            *Reason.ToString()
        );
        return;
    }

    UE_LOG(
        LogTemp,
        Display,
        TEXT(
            "BH_PLAYER_CONDITION_CHECKPOINT reason=%s"
        ),
        *Reason.ToString()
    );
}

void ABHCharacter::ShowHitConfirmation(
    bool bLethalHit,
    bool bHeadshot
)
{
    ShowDetailedHitConfirmation(
        bLethalHit,
        bHeadshot,
        false
    );
}

void ABHCharacter::ClientSetPlayerCasualtyPresentation_Implementation(
    bool bIncapacitated,
    bool bStabilized,
    float BleedOutSeconds
)
{
    bPlayerIncapacitated = bIncapacitated;
    bPlayerCasualtyStabilized = bStabilized;
    bIsHandlingDeath = bIncapacitated;
    APlayerController* PlayerController = ResolveOwningPlayerController();
    if (bIncapacitated)
    {
        if (IsValid(WeaponComponent))
        {
            WeaponComponent->StopAllActions();
        }
        GetCharacterMovement()->DisableMovement();
        if (IsValid(PlayerController))
        {
            DisableInput(PlayerController);
            PlayerController->SetIgnoreMoveInput(true);
            PlayerController->SetIgnoreLookInput(false);
        }
        DisplayStatusNotificationLocally(
            bStabilized
                ? NSLOCTEXT(
                    "BrokenHorizon",
                    "PlayerCasualtyStabilizedLocal",
                    "STABILIZED // AWAITING REVIVE\n\n"
                    "A teammate must use one medkit."
                )
                : FText::Format(
                    NSLOCTEXT(
                        "BrokenHorizon",
                        "PlayerCasualtyDownLocal",
                        "INCAPACITATED // {0}s TO BLEED OUT\n\n"
                        "A teammate must stabilize you."
                    ),
                    FText::AsNumber(FMath::CeilToInt(BleedOutSeconds))
                ),
            EBHNotificationPriority::Critical
        );
        return;
    }

    if (IsValid(DeathWidget))
    {
        DeathWidget->SetVisibility(ESlateVisibility::Collapsed);
    }
    GetCharacterMovement()->SetMovementMode(MOVE_Walking);
    ApplyMovementSpeed();
    if (IsValid(PlayerController))
    {
        EnableInput(PlayerController);
        PlayerController->SetIgnoreMoveInput(false);
        PlayerController->SetIgnoreLookInput(false);
    }
    if (IsValid(AmmoHUDWidget))
    {
        AmmoHUDWidget->SetVisibility(ESlateVisibility::Visible);
    }
    if (IsValid(CombatStatusWidget))
    {
        CombatStatusWidget->SetVisibility(ESlateVisibility::Visible);
    }
}

void ABHCharacter::HandleWeaponRoleChanged(
    EBHWeaponRole NewRole
)
{
    if (IsValid(AmmoHUDWidget))
    {
        AmmoHUDWidget->SetWeaponRole(
            UBHWeaponComponent::BuildWeaponRoleProfile(NewRole)
                .DisplayName
        );
        if (IsValid(WeaponComponent))
        {
            AmmoHUDWidget->SetFireMode(
                WeaponComponent->GetFireMode() ==
                    EBHFireMode::Automatic
                    ? NSLOCTEXT(
                        "BrokenHorizon",
                        "WeaponFireModeAutomaticShort",
                        "AUTO"
                    )
                    : NSLOCTEXT(
                        "BrokenHorizon",
                        "WeaponFireModeSemiAutomaticShort",
                        "SEMI"
                    )
            );
        }
    }
    RefreshOpenInventoryPanel();
}

void ABHCharacter::RefreshWeaponFireModeHUD(
    EBHFireMode NewFireMode
)
{
    if (!IsValid(AmmoHUDWidget))
    {
        return;
    }

    AmmoHUDWidget->SetFireMode(
        NewFireMode == EBHFireMode::Automatic
            ? NSLOCTEXT(
                "BrokenHorizon",
                "WeaponFireModeAutomaticShort",
                "AUTO"
            )
            : NSLOCTEXT(
                "BrokenHorizon",
                "WeaponFireModeSemiAutomaticShort",
                "SEMI"
            )
    );
}

void ABHCharacter::HandleWeaponHeatChanged(
    float HeatNormalized,
    bool bOverheated
)
{
    if (IsValid(AmmoHUDWidget))
    {
        AmmoHUDWidget->SetWeaponHeat(HeatNormalized, bOverheated);
    }
}

void ABHCharacter::ShowDetailedHitConfirmation(
    bool bLethalHit,
    bool bHeadshot,
    bool bArmorHit
)
{
    if (HasAuthority() && !IsLocallyControlled())
    {
        if (GetNetOwningPlayer())
        {
            ClientShowDetailedHitConfirmation(
                bLethalHit,
                bHeadshot,
                bArmorHit
            );
        }

        return;
    }

    DisplayDetailedHitConfirmationLocally(
        bLethalHit,
        bHeadshot,
        bArmorHit
    );
}

void ABHCharacter::DisplayHitConfirmationLocally(
    bool bLethalHit,
    bool bHeadshot
)
{
    DisplayDetailedHitConfirmationLocally(
        bLethalHit,
        bHeadshot,
        false
    );
}

void ABHCharacter::DisplayDetailedHitConfirmationLocally(
    bool bLethalHit,
    bool bHeadshot,
    bool bArmorHit
)
{
    if (!IsLocallyControlled())
    {
        return;
    }

    if (IsValid(HitMarkerWidget) &&
        !bIsHandlingDeath &&
        !bIsHandlingMissionComplete)
    {
        HitMarkerWidget->ShowDetailedHitMarker(
            bLethalHit,
            bHeadshot,
            bArmorHit
        );
    }
}

void ABHCharacter::ClientShowDetailedHitConfirmation_Implementation(
    bool bLethalHit,
    bool bHeadshot,
    bool bArmorHit
)
{
    DisplayDetailedHitConfirmationLocally(
        bLethalHit,
        bHeadshot,
        bArmorHit
    );
}

void ABHCharacter::ClientShowHitConfirmation_Implementation(
    bool bLethalHit,
    bool bHeadshot
)
{
    DisplayHitConfirmationLocally(
        bLethalHit,
        bHeadshot
    );
}

void ABHCharacter::NotifyIncomingRound(
    const FVector& SourceDirection,
    float Intensity
)
{
    if (HasAuthority())
    {
        AuthoritativePlayerSuppression =
            AccumulatePlayerSuppression(
                AuthoritativePlayerSuppression,
                Intensity,
                PlayerSuppressionAccumulationScale
            );
    }

    if (HasAuthority() && !IsLocallyControlled())
    {
        ClientNotifyIncomingRound(
            SourceDirection,
            Intensity
        );
        return;
    }

    DisplayIncomingRoundLocally(
        SourceDirection,
        Intensity
    );
}

void ABHCharacter::DisplayIncomingRoundLocally(
    const FVector& SourceDirection,
    float Intensity
)
{
    if (!IsLocallyControlled())
    {
        return;
    }

    LocalPlayerSuppressionPresentation =
        AccumulatePlayerSuppression(
            LocalPlayerSuppressionPresentation,
            Intensity,
            PlayerSuppressionAccumulationScale
        );

    if (IsValid(NearMissSound) && IsValid(GetWorld()) &&
        GetWorld()->GetTimeSeconds() - LastNearMissSoundTimeSeconds >=
            FMath::Max(0.0f, MinimumNearMissSoundInterval))
    {
        const FVector CueLocation = GetActorLocation() +
            SourceDirection.GetSafeNormal() * 120.0f +
            FVector(0.0f, 0.0f, 60.0f);
        UGameplayStatics::PlaySoundAtLocation(
            this,
            NearMissSound,
            CueLocation,
            FMath::Lerp(0.45f, 1.0f, FMath::Clamp(Intensity, 0.0f, 1.0f)),
            CalculateNearMissPitch(Intensity)
        );
        LastNearMissSoundTimeSeconds = GetWorld()->GetTimeSeconds();
    }

    if (IsValid(CombatStatusWidget) &&
        !bIsHandlingDeath &&
        !bIsHandlingMissionComplete)
    {
        CombatStatusWidget->SetSuppression(
            LocalPlayerSuppressionPresentation
        );
        CombatStatusWidget->NotifyNearMiss(
            SourceDirection,
            Intensity
        );
    }
}

float ABHCharacter::CalculateNearMissPitch(float Intensity)
{
    return FMath::Lerp(
        0.92f,
        1.08f,
        FMath::Clamp(Intensity, 0.0f, 1.0f)
    );
}

void ABHCharacter::ClientNotifyIncomingRound_Implementation(
    FVector SourceDirection,
    float Intensity
)
{
    DisplayIncomingRoundLocally(
        SourceDirection,
        Intensity
    );
}

void ABHCharacter::DisplayCombatDamageLocally(
    float DamageApplied,
    float HealthPercentage,
    const FVector& DamageSourceDirection,
    AActor* DamageCauser
)
{
    if (!IsLocallyControlled())
    {
        return;
    }

    if (const UWorld* World = GetWorld())
    {
        LastPlayerDamageTimeSeconds = World->GetTimeSeconds();
    }

    if (bIsHandlingMissionComplete ||
        !IsValid(CombatStatusWidget))
    {
        return;
    }

    CombatStatusWidget->NotifyPlayerDamaged(
        DamageApplied,
        HealthPercentage,
        DamageSourceDirection,
        DamageCauser
    );
}

void ABHCharacter::ClientNotifyCombatDamage_Implementation(
    float DamageApplied,
    float HealthPercentage,
    FVector DamageSourceDirection,
    AActor* DamageCauser,
    bool bShouldInterruptReload
)
{
    if (bShouldInterruptReload && IsValid(WeaponComponent) &&
        WeaponComponent->InterruptReload(FName(TEXT("damage"))))
    {
        DisplayStatusNotificationLocally(
            NSLOCTEXT(
                "BrokenHorizon",
                "ReloadInterruptedByDamage",
                "RELOAD INTERRUPTED // INCOMING FIRE\n\n"
                "Break contact before clearing the weapon."
            ),
            EBHNotificationPriority::High
        );
    }

    DisplayCombatDamageLocally(
        DamageApplied,
        HealthPercentage,
        DamageSourceDirection,
        DamageCauser
    );
}

void ABHCharacter::NotifyGrenadeThreat(
    AActor* GrenadeActor,
    const FVector& GrenadeLocation,
    float TimeUntilDetonation
)
{
    if (!IsValid(GrenadeActor) ||
        !IsValid(CombatStatusWidget) ||
        bIsHandlingDeath ||
        bIsHandlingMissionComplete)
    {
        return;
    }

    const FVector ToGrenade =
        GrenadeLocation - GetActorLocation();
    CombatStatusWidget->NotifyGrenadeThreat(
        GrenadeActor,
        ToGrenade,
        ToGrenade.Size(),
        TimeUntilDetonation
    );
}

void ABHCharacter::NotifyArmoredThreatContact(
    AActor* ThreatActor,
    const FVector& ThreatLocation,
    bool bActive
)
{
    if (!IsLocallyControlled() || !IsValid(ThreatActor))
    {
        return;
    }

    if (bActive &&
        (bIsHandlingDeath || bIsHandlingMissionComplete))
    {
        return;
    }

    if (bActive)
    {
        EnsureCombatStatusWidget();
    }

    if (!IsValid(CombatStatusWidget))
    {
        return;
    }

    const FVector ToThreat = ThreatLocation - GetActorLocation();
    CombatStatusWidget->NotifyArmoredThreat(
        ThreatActor,
        ToThreat,
        ToThreat.Size(),
        bActive
    );
    UE_LOG(
        LogTemp,
        Display,
        TEXT(
            "BH_ARMORED_THREAT_PRESENTATION local=1 threat=%s "
            "active=%d distance_m=%.0f"
        ),
        *ThreatActor->GetName(),
        bActive ? 1 : 0,
        ToThreat.Size() / 100.0f
    );
}

void ABHCharacter::RespawnAfterDeath()
{
    GetWorldTimerManager().ClearTimer(PlayerBleedOutTimerHandle);
    bPlayerIncapacitated = false;
    bPlayerCasualtyStabilized = false;
    bHasEnteredPlayerCasualtyThisLife = false;
    PlayerBleedOutDeadline = 0.0f;

    if (IsValid(InjuryComponent))
    {
        InjuryComponent->ResetInjuries();
    }

    if (IsValid(HealthComponent))
    {
        HealthComponent->ResetHealth();
    }

    UGameInstance* GameInstance = GetGameInstance();
    UBHSaveSubsystem* SaveSubsystem = GameInstance
        ? GameInstance->GetSubsystem<UBHSaveSubsystem>()
        : nullptr;
    const FName CasualtySectorID =
        bRuntimeWarOperation
            ? (
                AssignedWarSupplySourceSectorID.IsNone()
                    ? AssignedWarSectorID
                    : AssignedWarSupplySourceSectorID
            )
            : NAME_None;

    UWorld* World = GetWorld();
    APlayerController* PlayerController =
        ResolveOwningPlayerController();

    if (bRuntimeWarOperation &&
        IsValid(OpenWorldOperationDirector) &&
        OpenWorldOperationDirector->IsOperationActivated())
    {
        bIsHandlingDeath = false;
        GetCharacterMovement()->SetMovementMode(MOVE_Walking);
        if (IsValid(HealthComponent))
        {
            HealthComponent->ResetHealth();
        }
        if (IsValid(PlayerController))
        {
            EnableInput(PlayerController);
            PlayerController->SetIgnoreMoveInput(false);
            PlayerController->SetIgnoreLookInput(false);
        }
        ApplyRapidOperationRedeployment();
        ShowStatusNotification(NSLOCTEXT(
            "BrokenHorizon",
            "RapidOperationRedeploymentImmediate",
            "RAPID REDEPLOYMENT // ACTIVE OPERATION PRESERVED"
        ));
        return;
    }

    if (HasAuthority() &&
        IsValid(World) &&
        World->GetNetMode() != NM_Standalone)
    {
        for (TActorIterator<ABHFieldTransport> It(World); It; ++It)
        {
            if (It->GetOccupant() == this)
            {
                It->ForceExitOccupantForRespawn(this);
                break;
            }
        }

        UBHWarSubsystem* WarSubsystem = GameInstance
            ? GameInstance->GetSubsystem<UBHWarSubsystem>()
            : nullptr;
        ABHSectorAnchor* RespawnAnchor = nullptr;
        float NearestFriendlyDistanceSquared =
            TNumericLimits<float>::Max();

        for (TActorIterator<ABHSectorAnchor> It(World); It; ++It)
        {
            ABHSectorAnchor* Candidate = *It;

            if (!IsValid(Candidate))
            {
                continue;
            }

            if (!CasualtySectorID.IsNone() &&
                Candidate->MatchesSector(CasualtySectorID))
            {
                RespawnAnchor = Candidate;
                break;
            }

            if (!IsValid(WarSubsystem) ||
                WarSubsystem->GetSectorState(
                    Candidate->GetSectorID()
                ).Owner != EBHWarFaction::Friendly)
            {
                continue;
            }

            const float DistanceSquared =
                FVector::DistSquared2D(
                    GetActorLocation(),
                    Candidate->GetOperationCenter()
                );

            if (DistanceSquared <
                NearestFriendlyDistanceSquared)
            {
                NearestFriendlyDistanceSquared =
                    DistanceSquared;
                RespawnAnchor = Candidate;
            }
        }

        if (IsValid(RespawnAnchor))
        {
            FVector RespawnLocation =
                RespawnAnchor->GetOperationCenter() +
                FVector(0.0f, 0.0f, 150.0f);
            FRotator RespawnRotation =
                RespawnAnchor->GetActorRotation();
            World->FindTeleportSpot(
                this,
                RespawnLocation,
                RespawnRotation
            );
            SetActorLocationAndRotation(
                RespawnLocation,
                RespawnRotation,
                false,
                nullptr,
                ETeleportType::TeleportPhysics
            );
        }

        bIsHandlingDeath = false;
        GetWorldTimerManager().ClearTimer(RespawnTimerHandle);
        GetCharacterMovement()->SetMovementMode(MOVE_Walking);
        ApplyMovementSpeed();

        if (IsValid(WarSubsystem) &&
            !CasualtySectorID.IsNone())
        {
            WarSubsystem->ApplyAmbientBattleResult(
                CasualtySectorID,
                1,
                0
            );
        }

        const bool bCampaignSaved =
            IsValid(SaveSubsystem) &&
            SaveSubsystem->SaveProgress();

        ClientCompleteFieldRespawn();
        ShowStatusNotification(
            FText::Format(
                NSLOCTEXT(
                    "BrokenHorizon",
                    "MultiplayerFieldRespawnComplete",
                    "FIELD REDEPLOYMENT COMPLETE\n\n"
                    "A replacement operator has arrived at "
                    "{0}.\nCampaign casualty recorded. {1}"
                ),
                IsValid(RespawnAnchor)
                    ? RespawnAnchor->GetSectorDisplayName()
                    : NSLOCTEXT(
                        "BrokenHorizon",
                        "MultiplayerRespawnFallback",
                        "the nearest safe position"
                    ),
                bCampaignSaved
                    ? NSLOCTEXT(
                        "BrokenHorizon",
                        "MultiplayerRespawnSaved",
                        "CAMPAIGN UPDATED"
                    )
                    : NSLOCTEXT(
                        "BrokenHorizon",
                        "MultiplayerRespawnSavePending",
                        "CAMPAIGN SAVE PENDING"
                    )
            )
        );

        UE_LOG(
            LogTemp,
            Display,
            TEXT(
                "BH_MULTIPLAYER_FIELD_RESPAWN sector=%s "
                "anchor=%s saved=%d"
            ),
            *CasualtySectorID.ToString(),
            IsValid(RespawnAnchor)
                ? *RespawnAnchor->GetSectorID().ToString()
                : TEXT("None"),
            bCampaignSaved ? 1 : 0
        );
        return;
    }

    if (IsValid(SaveSubsystem) &&
        SaveSubsystem->HasSaveGame() &&
        SaveSubsystem->ReloadCheckpointAfterPlayerDeath(
            CasualtySectorID
        ))
    {
        return;
    }

    if (!IsValid(World))
    {
        return;
    }

    const FName CurrentLevelName(
        *UGameplayStatics::GetCurrentLevelName(World, true)
    );
    UGameplayStatics::OpenLevel(World, CurrentLevelName);
}
