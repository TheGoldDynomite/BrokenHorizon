#include "BHCharacter.h"


#include "Animation/AnimSequenceBase.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/WorldPartitionStreamingSourceComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/PlayerController.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputCoreTypes.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/World.h"
#include "BHInteractable.h"
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
#include "BHEnemyAIController.h"
#include "BHEnemySoldier.h"
#include "BHWarOperationRules.h"
#include "BHPlayerResolver.h"
#include "BHFieldTransport.h"
#include "BHSectorResupplyStation.h"
#include "BHSectorAnchor.h"
#include "BHSupplyConvoyTarget.h"
#include "BHSaveSubsystem.h"
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
#include "BHCombatStatusWidget.h"
#include "BHFragGrenade.h"
#include "Engine/GameInstance.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "NavigationInvokerComponent.h"
#include "Net/UnrealNetwork.h"

namespace
{
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
    MissionCompleteMessage = NSLOCTEXT(
        "BrokenHorizon",
        "MissionCompleteMessage",
        "MISSION COMPLETE"
    );
    WarMapWidgetClass = UBHWarMapWidget::StaticClass();
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
}

void ABHCharacter::BeginPlay()
{
    Super::BeginPlay();

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
        PlayerController->IsLocalController())
    {
        UGameplayStatics::SetGamePaused(this, false);
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

    if (ObjectiveWidgetClass &&
        IsValid(PlayerController) &&
        PlayerController->IsLocalController())
    {
        ObjectiveWidget = CreateWidget<UBHObjectiveWidget>(
            PlayerController,
            ObjectiveWidgetClass
        );

        if (ObjectiveWidget)
        {
            ObjectiveWidget->AddToViewport();
        }
    }

    if (InteractionPromptClass &&
        IsValid(PlayerController) &&
        PlayerController->IsLocalController())
    {
        InteractionPromptWidget = CreateWidget<UBHInteractionPromptWidget>(
            PlayerController,
            InteractionPromptClass
        );

        if (InteractionPromptWidget)
        {
            InteractionPromptWidget->AddToViewport();

            InteractionPromptWidget->SetVisibility(
                ESlateVisibility::Collapsed
            );
        }
    }


    if (IsValid(WeaponComponent))
    {
        WeaponComponent->OnAmmoChanged.AddDynamic(
            this,
            &ABHCharacter::HandleAmmoChanged
        );
    }

    if (AmmoHUDWidgetClass &&
        IsValid(PlayerController) &&
        PlayerController->IsLocalController())
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
        }
    }

    if (IsValid(PlayerController) &&
        PlayerController->IsLocalController())
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
        PlayerController->IsLocalController())
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

    if (!PlayerController || !PlayerMappingContext)
    {
        return;
    }

    ULocalPlayer* LocalPlayer =
        PlayerController->GetLocalPlayer();

    if (!LocalPlayer)
    {
        return;
    }

    UEnhancedInputLocalPlayerSubsystem* InputSubsystem =
        LocalPlayer->GetSubsystem<
        UEnhancedInputLocalPlayerSubsystem>();

    if (InputSubsystem)
    {
        InputSubsystem->AddMappingContext(
            PlayerMappingContext,
            0);
    }
}

void ABHCharacter::SetupPlayerInputComponent(
    UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);

    PlayerInputComponent->BindKey(
        EKeys::M,
        IE_Pressed,
        this,
        &ABHCharacter::ToggleWarMap
    );

    PlayerInputComponent->BindKey(
        EKeys::G,
        IE_Pressed,
        this,
        &ABHCharacter::ThrowFragGrenade
    );

    PlayerInputComponent->BindKey(
        EKeys::C,
        IE_Pressed,
        this,
        &ABHCharacter::ToggleFriendlySquadOrder
    );

    UEnhancedInputComponent* EnhancedInputComponent =
        Cast<UEnhancedInputComponent>(PlayerInputComponent);

    if (!EnhancedInputComponent)
    {
        return;
    }

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
            &ABHCharacter::StartSprint);

        EnhancedInputComponent->BindAction(
            SprintAction,
            ETriggerEvent::Completed,
            this,
            &ABHCharacter::StopSprint);
    }

    if (CrouchAction)
    {
        EnhancedInputComponent->BindAction(
            CrouchAction,
            ETriggerEvent::Started,
            this,
            &ABHCharacter::StartCrouch);

        EnhancedInputComponent->BindAction(
            CrouchAction,
            ETriggerEvent::Completed,
            this,
            &ABHCharacter::StopCrouch);
    }

    if (LeanLeftAction)
    {
        EnhancedInputComponent->BindAction(
            LeanLeftAction,
            ETriggerEvent::Started,
            this,
            &ABHCharacter::StartLeanLeft
        );
        EnhancedInputComponent->BindAction(
            LeanLeftAction,
            ETriggerEvent::Completed,
            this,
            &ABHCharacter::StopLeanLeft
        );
        EnhancedInputComponent->BindAction(
            LeanLeftAction,
            ETriggerEvent::Canceled,
            this,
            &ABHCharacter::StopLeanLeft
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
            &ABHCharacter::StartLeanRight
        );
        EnhancedInputComponent->BindAction(
            LeanRightAction,
            ETriggerEvent::Completed,
            this,
            &ABHCharacter::StopLeanRight
        );
        EnhancedInputComponent->BindAction(
            LeanRightAction,
            ETriggerEvent::Canceled,
            this,
            &ABHCharacter::StopLeanRight
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
            &ABHCharacter::ToggleProne
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
            &ABHCharacter::Interact);
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
            &ABHCharacter::StartAim
        );
        EnhancedInputComponent->BindAction(
            AimAction,
            ETriggerEvent::Completed,
            this,
            &ABHCharacter::StopAim
        );
        EnhancedInputComponent->BindAction(
            AimAction,
            ETriggerEvent::Canceled,
            this,
            &ABHCharacter::StopAim
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

    PlayerInputComponent->BindKey(
        EKeys::B,
        IE_Pressed,
        this,
        &ABHCharacter::ToggleFireMode
    );

    if (PauseAction)
    {
        EnhancedInputComponent->BindAction(
            PauseAction,
            ETriggerEvent::Started,
            this,
            &ABHCharacter::TogglePauseMenu
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

    float MouseSensitivity = 1.0f;

    if (UGameInstance* GameInstance = GetGameInstance())
    {
        if (const UBHUserSettingsSubsystem* SettingsSubsystem =
            GameInstance->GetSubsystem<UBHUserSettingsSubsystem>())
        {
            MouseSensitivity =
                SettingsSubsystem->GetMouseSensitivity();
        }
    }

    AddControllerYawInput(LookInput.X * MouseSensitivity);
    AddControllerPitchInput(LookInput.Y * MouseSensitivity);
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
    const float ClampedAmount = FMath::Max(0.0f, Amount);

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

    MovementComponent->MaxWalkSpeed =
        BaseSpeed * InjurySpeedMultiplier;
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

    WeaponComponent->StartReload();
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

void ABHCharacter::ThrowFragGrenade()
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

void ABHCharacter::UseFieldDressing()
{
    if (bIsHandlingDeath ||
        bIsHandlingMissionComplete ||
        bIsTraversing ||
        !IsValid(InjuryComponent))
    {
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
        !IsValid(InjuryComponent) ||
        !InjuryComponent->StartMedkitTreatment())
    {
        return;
    }

    if (IsValid(WeaponComponent))
    {
        WeaponComponent->StopAllActions();
    }

    StopSprint();
}


void ABHCharacter::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    UpdateInitialWorldStreaming(DeltaTime);

    UpdateTraversal(DeltaTime);

    const float PreviousStamina = CurrentStamina;

    if (bIsSprinting && GetVelocity().SizeSquared2D() > 0.0f)
    {
        CurrentStamina -= StaminaDrainRate * DeltaTime;
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
            CurrentStamina += StaminaRecoveryRate * DeltaTime;
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
    UpdateLean(DeltaTime);
    SynchronizeReplicatedOperationPresentation();
    UpdateOperationWaypointHUD();
    UpdateSquadCommandWaypointHUD();
    UpdateResupplyWaypointHUD(DeltaTime);
    UpdateConvoyWaypointHUD(DeltaTime);
    UpdateTransportWaypointHUD(DeltaTime);
    UpdateLogisticsWaypointHUD(DeltaTime);
    UpdateVehicleReadinessHUD();
    UpdateFieldSquadStatusHUD();
    UpdateStrategicSituationHUD(DeltaTime);

    if (!bIsHandlingDeath &&
        !bIsHandlingMissionComplete &&
        !bIsTraversing)
    {
        RefreshFirstPersonArmsAnimation();
        UpdateInteractionPrompt();
    }

    UpdateFirstPersonPresentationOffsets(DeltaTime);
}

void ABHCharacter::SynchronizeReplicatedOperationPresentation()
{
    if (!IsLocallyControlled() || GetWorld() == nullptr)
    {
        return;
    }

    const ABHWarGameState* WarGameState =
        GetWorld()->GetGameState<ABHWarGameState>();

    if (!IsValid(WarGameState))
    {
        return;
    }

    const FBHActiveOperationSnapshot Snapshot =
        WarGameState->GetActiveOperationSnapshot();
    const uint8 SnapshotPhase =
        static_cast<uint8>(Snapshot.Phase);

    if (Snapshot.SectorID == LastPresentedOperationSectorID &&
        SnapshotPhase == LastPresentedOperationPhase)
    {
        return;
    }

    LastPresentedOperationSectorID = Snapshot.SectorID;
    LastPresentedOperationPhase = SnapshotPhase;

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
        else if (
            Snapshot.Phase ==
                EBHActiveOperationPhase::DebriefSuccess)
        {
            DisplayStatusNotificationLocally(
                NSLOCTEXT(
                    "BrokenHorizon",
                    "ReplicatedOperationDebriefSuccess",
                    "SHARED OPERATION COMPLETE\n\n"
                    "Campaign command is processing the outcome."
                )
            );
        }
        else if (
            Snapshot.Phase ==
                EBHActiveOperationPhase::DebriefFailure)
        {
            DisplayStatusNotificationLocally(
                NSLOCTEXT(
                    "BrokenHorizon",
                    "ReplicatedOperationDebriefFailure",
                    "SHARED OPERATION FAILED\n\n"
                    "Campaign command is assessing the losses."
                )
            );
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
        ObjectiveWidget->SetVisibility(ESlateVisibility::Visible);
    }

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
        )
    );
}

void ABHCharacter::UpdateOperationWaypointHUD()
{
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
    const bool bLocalDirectorHasWaypoint =
        IsValid(OpenWorldOperationDirector) &&
        OpenWorldOperationDirector->IsOperationInProgress();
    const bool bShowWaypoint =
        bSnapshotHasWaypoint || bLocalDirectorHasWaypoint;

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
            OperationStatusText = NSLOCTEXT(
                "BrokenHorizon",
                "ReplicatedOperationAwaitingWave",
                "Prepare for the next enemy wave"
            );
            break;
        case EBHActiveOperationPhase::Securing:
            OperationStatusText = NSLOCTEXT(
                "BrokenHorizon",
                "ReplicatedOperationSecuring",
                "Secure and hold the objective"
            );
            break;
        case EBHActiveOperationPhase::RaidExfiltration:
            OperationStatusText = NSLOCTEXT(
                "BrokenHorizon",
                "ReplicatedOperationRaidExfiltration",
                "Break contact and leave the raid area"
            );
            break;
        default:
            OperationStatusText = NSLOCTEXT(
                "BrokenHorizon",
                "ReplicatedOperationCombat",
                "Operation active"
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
    CombatStatusWidget->SetResupplyWaypoint(
        true,
        NearestSector.DisplayName.IsEmpty()
            ? FText::FromName(NearestSector.SectorID)
            : NearestSector.DisplayName,
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
    ABHSupplyConvoyTarget* NearestTarget = nullptr;
    float NearestDistanceSquared = TNumericLimits<float>::Max();

    if (IsValid(World))
    {
        for (TActorIterator<ABHSupplyConvoyTarget> It(World);
            It;
            ++It)
        {
            ABHSupplyConvoyTarget* Target = *It;

            if (!IsValid(Target) || Target->GetConvoyID().IsNone())
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

    CombatStatusWidget->SetVehicleReadiness(
        bDriving,
        bDriving ? Transport->GetFuelPercentage() : 1.0f,
        bDriving ? Transport->GetHullPercentage() : 1.0f,
        bDriving ? Transport->GetSpeedKPH() : 0.0f,
        bDriving && Transport->IsImmobilized()
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
    CombatStatusWidget->SetFieldSquadStatus(
        LivingOperatives > 0,
        LivingOperatives,
        MaximumFieldSquadSize,
        bFieldSquadHolding,
        bFieldSquadEmbarked
    );
    CombatStatusWidget->SetFieldSquadServiceNeeds(
        MembersNeedingService
    );
}

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
            0,
            0,
            0,
            0.0f,
            1.0f,
            true
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
        WarSubsystem->GetTurnNumber(),
        WarSubsystem->GetSectorConstructedFortificationCount(SectorID),
        WarSubsystem->GetSectorFortificationCapacity(SectorID),
        WarSubsystem->GetSectorFortificationCoverage(SectorID),
        WarSubsystem->GetSectorFortificationDefenseMultiplier(SectorID),
        WarSubsystem->IsSectorConnectedToFactionLogistics(SectorID)
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

void ABHCharacter::UpdateInteractionPrompt()
{
    if (!IsValid(InteractionPromptWidget) || !IsValid(FirstPersonCamera))
    {
        return;
    }

    if (!IsPlayerControlled())
    {
        InteractionPromptWidget->SetVisibility(
            ESlateVisibility::Hidden
        );
        return;
    }

    const FVector TraceStart = FirstPersonCamera->GetComponentLocation();
    const FVector TraceEnd =
        TraceStart +
        (FirstPersonCamera->GetForwardVector() * InteractionDistance);

    FHitResult HitResult;

    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActor(this);

    const bool bHit = GetWorld()->LineTraceSingleByChannel(
        HitResult,
        TraceStart,
        TraceEnd,
        ECC_Visibility,
        QueryParams
    );

    if (!bHit)
    {
        InteractionPromptWidget->SetVisibility(
            ESlateVisibility::Hidden
        );
        return;
    }

    AActor* HitActor = HitResult.GetActor();

    if (ABHEnemySoldier* DownedOperative =
            Cast<ABHEnemySoldier>(HitActor);
        IsValid(DownedOperative) &&
        DownedOperative->IsIncapacitated() &&
        IsSharedFieldSquadMember(DownedOperative))
    {
        InteractionPromptWidget->SetInteractionText(
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
        );
        InteractionPromptWidget->SetVisibility(
            ESlateVisibility::Visible
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

        InteractionPromptWidget->SetInteractionText(PromptText);
        InteractionPromptWidget->SetVisibility(
            ESlateVisibility::Visible
        );
    }
    else
    {
        InteractionPromptWidget->SetVisibility(
            ESlateVisibility::Hidden
        );
    }
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

    FHitResult Hit;
    FCollisionQueryParams QueryParams(
        SCENE_QUERY_STAT(BHCharacterInteraction),
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

    if (ABHEnemySoldier* DownedOperative =
            Cast<ABHEnemySoldier>(TargetActor);
        IsValid(DownedOperative) &&
        DownedOperative->IsIncapacitated() &&
        IsSharedFieldSquadMember(DownedOperative))
    {
        TryStabilizeFieldSquadMember(DownedOperative);
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
        OwnedKeycards.Add(KeycardID);
    }
}

void ABHCharacter::EndPlay(
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

    if (bPauseMenuOpen)
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

    OwnedKeycards.Add(KeycardID);
    CollectedKeycardPersistenceIDs.Add(PickupPersistenceID);
    return true;
}

bool ABHCharacter::HasKeycard(const FName KeycardID) const
{
    return OwnedKeycards.Contains(KeycardID);
}

TArray<FName> ABHCharacter::GetOwnedKeycardIDs() const
{
    return OwnedKeycards.Array();
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
        !PlayerController->IsLocalController())
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

    bPauseMenuOpen = UGameplayStatics::SetGamePaused(
        this,
        true
    );

    if (!bPauseMenuOpen)
    {
        PauseMenuWidget->RemoveFromParent();
        PauseMenuWidget = nullptr;
        PlayerController->SetIgnoreMoveInput(false);
        PlayerController->SetIgnoreLookInput(false);
        PlayerController->SetInputMode(FInputModeGameOnly());
        PlayerController->bShowMouseCursor = false;
    }
}

void ABHCharacter::ResumeFromPause()
{
    if (!bPauseMenuOpen)
    {
        return;
    }

    bPauseMenuOpen = false;
    UGameplayStatics::SetGamePaused(this, false);

    if (IsValid(PauseMenuWidget))
    {
        PauseMenuWidget->RemoveFromParent();
        PauseMenuWidget = nullptr;
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
}

void ABHCharacter::ToggleFriendlySquadOrder()
{
    if (!HasAuthority())
    {
        ServerRequestToggleFriendlySquadOrder();
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
            &ABHCharacter::
                HandleFieldSquadMemberCasualtyExpired
        );

        FieldSquadMembers.Add(Soldier);
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

            SquadAIController->SetHoldPosition(
                HoldLocation
            );
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
    float ServiceRadius
)
{
    const float RadiusSquared = FMath::Square(
        FMath::Max(100.0f, ServiceRadius)
    );
    int32 ServicedCount = 0;
    bool bStabilizedCasualty = false;

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
                Member->ServiceCombatLoadout();
                ++ServicedCount;
                bStabilizedCasualty = true;
            }
            continue;
        }

        if (Member->IsDead())
        {
            continue;
        }

        ServicedCount += Member->ServiceCombatLoadout() ? 1 : 0;
    }

    if (bStabilizedCasualty)
    {
        ApplyFieldSquadOrder();
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
                        bFieldSquadHolding
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
    if (!IsValid(LostOperative) ||
        FieldSquadMembers.RemoveSingle(LostOperative) <= 0)
    {
        return;
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

    ShowStatusNotification(
        FText::Format(
            NSLOCTEXT(
                "BrokenHorizon",
                "FieldSquadOperativeLost",
                "OPERATIVE LOST\n\n"
                "The stabilization window expired. "
                "{0} operative(s) remain."
            ),
            FText::AsNumber(RemainingOperatives)
        )
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
    ShowStatusNotification(
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
        )
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
        !PlayerController->IsLocalController() ||
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
                    "FriendlySectorControl",
                    "FRIENDLY"
                )
                : Sector.Owner == EBHWarFaction::Enemy
                    ? NSLOCTEXT(
                        "BrokenHorizon",
                        "EnemySectorControl",
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
        PlayerController->IsLocalController() &&
        (
            !IsMissionComplete() ||
            CurrentCampaignOutcome !=
                EBHWarCampaignOutcome::Ongoing
        ))
    {
        ShowStatusNotification(StrategicUpdate);
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

    if (!WarSubsystem->SetCommittedOperation(
            AssignedWarSectorID,
            AssignedWarPriorityType
        ))
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

    FBHObjectiveDefinition OperationObjective;
    OperationObjective.ObjectiveID =
        BHObjectiveIds::EliminateGuard;
    OperationObjective.DisplayText =
        WarSubsystem->GetOperationObjectiveText(
            AssignedWarSectorID,
            AssignedWarPriorityType,
            BHObjectiveIds::EliminateGuard
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
        ObjectiveWidget->SetVisibility(ESlateVisibility::Visible);
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

    return InjuryMultiplier *
        LeanMultiplier *
        MovementMultiplier *
        StanceMultiplier *
        ExhaustionMultiplier;
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
    return bIsProne
        ? FMath::Clamp(
            ProneAISightRangeMultiplier,
            0.1f,
            1.0f
        )
        : 1.0f;
}

UBHWeaponComponent* ABHCharacter::GetWeaponComponent() const
{
    return WeaponComponent;
}

int32 ABHCharacter::GetFragGrenadeCount() const
{
    return FragGrenadeCount;
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

void ABHCharacter::RefreshFragGrenadeHUD()
{
    if (IsValid(CombatStatusWidget))
    {
        CombatStatusWidget->SetFragGrenadeCount(
            FragGrenadeCount
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
            OwnedKeycards.Add(KeycardID);
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
            OwnedKeycards.Add(KeycardID);
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
        const bool bOperationRestored =
            bDirectorStarted &&
            IsValid(OpenWorldOperationDirector) &&
            OpenWorldOperationDirector->RestoreOperationState(
                SavedOpenWorldOperationState
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
    UE_LOG(
        LogTemp,
        Log,
        TEXT("Completed objective %s."),
        *CompletedObjectiveID.ToString()
    );

    ShowStatusNotification(
        FText::Format(
            NSLOCTEXT(
                "BrokenHorizon",
                "ObjectiveCompletedNotification",
                "OBJECTIVE COMPLETE\n\n\u2713 {0}"
            ),
            CompletedObjectiveText
        )
    );

    RefreshObjectiveWidget();

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
    if (HasAuthority() && !IsLocallyControlled())
    {
        if (GetNetOwningPlayer())
        {
            ClientShowStatusNotification(Message);
        }

        return;
    }

    DisplayStatusNotificationLocally(Message);
}

void ABHCharacter::DisplayStatusNotificationLocally(
    const FText& Message
)
{
    APlayerController* PlayerController =
        ResolveOwningPlayerController();

    if (!IsValid(PlayerController) ||
        !PlayerController->IsLocalController())
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
        ObjectiveNotificationWidget->ShowNotification(Message);
    }
}

void ABHCharacter::ClientShowStatusNotification_Implementation(
    const FText& Message
)
{
    DisplayStatusNotificationLocally(Message);
}

void ABHCharacter::ClientPresentDeath_Implementation(
    float DelaySeconds
)
{
    bIsHandlingDeath = true;

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
        !PlayerController->IsLocalController())
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
        ObjectiveWidget->SetVisibility(ESlateVisibility::Visible);
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
        PlayerController->IsLocalController())
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
            BHObjectiveIds::EliminateGuard;
        ObjectiveComponent->StartRuntimeMission(
            {OperationObjective}
        );
    }

    ConfigureStrategicMissionPresentation();
    RefreshObjectiveWidget();
    UpdateOperationWaypointHUD();

    if (IsValid(ObjectiveWidget))
    {
        ObjectiveWidget->SetVisibility(ESlateVisibility::Visible);
    }
}

void ABHCharacter::ClientPresentOperationDebrief_Implementation(
    const FText& Message
)
{
    MissionCompleteMessage = Message;

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
        BHObjectiveIds::ReachExtraction
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

    ShowStatusNotification(
        FText::Format(
            NSLOCTEXT(
                "BrokenHorizon",
                "StrategicMissionBriefingNotification",
                "STRATEGIC BRIEFING\n\n{0}\n\n{1}"
            ),
            WarSubsystem->GetOperationTitle(
                AssignedWarSectorID,
                AssignedWarPriorityType
            ),
            WarSubsystem->GetOperationMissionBriefing(
                AssignedWarSectorID,
                AssignedWarPriorityType
            )
        )
    );
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
            bWarResultApplied
                ? WarSubsystem->RecoverBattlefieldMateriel(
                    ResolvedSectorID,
                    EnemyLosses,
                    FriendlySupportLosses,
                    true
                )
                : 0.0f;
        const FBHWarSectorState UpdatedSector =
            WarSubsystem->GetSectorState(ResolvedSectorID);
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
    if (bIsHandlingMissionComplete)
    {
        return;
    }

    bIsHandlingMissionComplete = true;

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

    APlayerController* PlayerController =
        ResolveOwningPlayerController();

    if (IsValid(PlayerController))
    {
        DisableInput(PlayerController);
        PlayerController->SetIgnoreMoveInput(true);
        PlayerController->SetIgnoreLookInput(true);
    }

    if (IsValid(PlayerController) &&
        PlayerController->IsLocalController())
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

    if (IsValid(ObjectiveWidget))
    {
        ObjectiveWidget->SetVisibility(
            ESlateVisibility::Collapsed
        );
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

    if (IsValid(ObjectiveWidget))
    {
        ObjectiveWidget->SetVisibility(
            ESlateVisibility::Collapsed
        );
    }

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

void ABHCharacter::HandleDeath(AActor* DamageCauser)
{
    if (bIsHandlingDeath || bIsHandlingMissionComplete)
    {
        return;
    }

    bIsHandlingDeath = true;

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
        !PlayerController->IsLocalController())
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
        PlayerController->IsLocalController())
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
            DamageCauser
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
    if (HasAuthority() && !IsLocallyControlled())
    {
        if (GetNetOwningPlayer())
        {
            ClientShowHitConfirmation(
                bLethalHit,
                bHeadshot
            );
        }

        return;
    }

    DisplayHitConfirmationLocally(
        bLethalHit,
        bHeadshot
    );
}

void ABHCharacter::DisplayHitConfirmationLocally(
    bool bLethalHit,
    bool bHeadshot
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
        HitMarkerWidget->ShowHitMarker(
            bLethalHit,
            bHeadshot
        );
    }
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

    if (IsValid(CombatStatusWidget) &&
        !bIsHandlingDeath &&
        !bIsHandlingMissionComplete)
    {
        CombatStatusWidget->NotifyNearMiss(
            SourceDirection,
            Intensity
        );
    }
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
    AActor* DamageCauser
)
{
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

void ABHCharacter::RespawnAfterDeath()
{
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
