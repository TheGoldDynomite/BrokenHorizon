#include "BHCharacter.h"


#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/PlayerController.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "BHInteractable.h"
#include "Blueprint/UserWidget.h"
#include "BHInteractionPromptWidget.h"
#include "BHObjectiveWidget.h"
#include "BHObjectiveComponent.h"
#include "BHObjectiveNotificationWidget.h"


ABHCharacter::ABHCharacter()
{

    PrimaryActorTick.bCanEverTick = false;

    ObjectiveComponent = CreateDefaultSubobject<UBHObjectiveComponent>(
        TEXT("ObjectiveComponent")
    );

    FirstPersonCamera =
        CreateDefaultSubobject<UCameraComponent>(
            TEXT("FirstPersonCamera"));

    FirstPersonCamera->SetupAttachment(GetCapsuleComponent());

    FirstPersonCamera->SetRelativeLocation(
        FVector(0.0f, 0.0f, 64.0f));

    FirstPersonCamera->bUsePawnControlRotation = true;
    FirstPersonCamera->SetFieldOfView(90.0f);

    bUseControllerRotationPitch = false;
    bUseControllerRotationYaw = true;
    bUseControllerRotationRoll = false;

    GetCharacterMovement()->MaxWalkSpeed = WalkSpeed;

    GetCharacterMovement()->NavAgentProps.bCanCrouch = true;

    PrimaryActorTick.bCanEverTick = true;

    CurrentStamina = MaxStamina;
}

void ABHCharacter::BeginPlay()
{
    Super::BeginPlay();

    if (ObjectiveWidgetClass)
    {
        ObjectiveWidget = CreateWidget<UBHObjectiveWidget>(
            GetWorld(),
            ObjectiveWidgetClass
        );

        if (ObjectiveWidget)
        {
            ObjectiveWidget->AddToViewport();
            SetObjective(
                FText::FromString(TEXT("Find the Red Keycard"))
            );
        }
    }

    if (InteractionPromptClass)
    {
        InteractionPromptWidget = CreateWidget<UBHInteractionPromptWidget>(
            GetWorld(),
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


    // Objective Notification Event
    if (ObjectiveComponent)
    {
        ObjectiveComponent->OnObjectiveCompleted.AddDynamic(
            this,
            &ABHCharacter::OnObjectiveCompleted
        );
    }

    APlayerController* PlayerController =
        Cast<APlayerController>(GetController());

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
    
     if (InteractAction)
    {
        EnhancedInputComponent->BindAction(
            InteractAction,
            ETriggerEvent::Started,
            this,
            &ABHCharacter::Interact);
    }
}


void ABHCharacter::Move(const FInputActionValue& Value)
{
    const FVector2D MovementInput =
        Value.Get<FVector2D>();

    if (!Controller)
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

    AddControllerYawInput(LookInput.X);
    AddControllerPitchInput(LookInput.Y);
}

void StartJump();
void StopJump();

void ABHCharacter::StartJump()
{
    UE_LOG(LogTemp, Warning, TEXT("StartJump was called"));
    Jump();
}

void ABHCharacter::StopJump()
{
    StopJumping();
}

void ABHCharacter::StartSprint()
{
    if (CurrentStamina <= 0.0f)
    {
        return;
    }

    bIsSprinting = true;
    TimeSinceSprintStopped = 0.0f;

    GetCharacterMovement()->MaxWalkSpeed = SprintSpeed;
}

void ABHCharacter::StopSprint()
{
    bIsSprinting = false;
    TimeSinceSprintStopped = 0.0f;

    GetCharacterMovement()->MaxWalkSpeed = WalkSpeed;
}


void ABHCharacter::StartCrouch()
{
    Crouch();
}

void ABHCharacter::StopCrouch()
{
    UnCrouch();
}


void ABHCharacter::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

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

    UpdateInteractionPrompt();
}

void ABHCharacter::UpdateInteractionPrompt()
{
    if (!IsValid(InteractionPromptWidget) || !IsValid(FirstPersonCamera))
    {
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
    const FVector Start = FirstPersonCamera->GetComponentLocation();
    const FVector End =
        Start + (FirstPersonCamera->GetForwardVector() * 300.0f);

    FHitResult Hit;

    FCollisionQueryParams Params;
    Params.AddIgnoredActor(this);

    if (GetWorld()->LineTraceSingleByChannel(
        Hit,
        Start,
        End,
        ECC_Visibility,
        Params))
    {
        AActor* HitActor = Hit.GetActor();

        if (IsValid(HitActor) &&
            HitActor->Implements<UBHInteractable>())
        {
            IBHInteractable::Execute_Interact(HitActor, this);
        }
        else if (IsValid(HitActor))
        {
            UE_LOG(
                LogTemp,
                Warning,
                TEXT("%s is not interactable"),
                *HitActor->GetName()
            );
        }
    }

    DrawDebugLine(
        GetWorld(),
        Start,
        End,
        FColor::Green,
        false,
        2.0f,
        0,
        2.0f
    );
} 

void ABHCharacter::AddKeycard(const FName KeycardID)
{
    if (!KeycardID.IsNone())
    {
        OwnedKeycards.Add(KeycardID);
    }
}

bool ABHCharacter::HasKeycard(const FName KeycardID) const
{
    return OwnedKeycards.Contains(KeycardID);
}

void ABHCharacter::SetObjective(const FText& NewObjective)
{
    if (IsValid(ObjectiveComponent))
    {
        ObjectiveComponent->SetObjective(NewObjective);
    }

    if (IsValid(ObjectiveWidget) &&
        IsValid(ObjectiveComponent))
    {
        ObjectiveWidget->SetObjectiveList(
            ObjectiveComponent->GetCompletedObjectives(),
            ObjectiveComponent->GetCurrentObjective()
        );
    }
}

void ABHCharacter::OnObjectiveCompleted(
    FText CompletedObjective
)
{
    if (!ObjectiveNotificationWidget &&
        ObjectiveNotificationWidgetClass)
    {
        ObjectiveNotificationWidget =
            CreateWidget<UBHObjectiveNotificationWidget>(
                GetWorld(),
                ObjectiveNotificationWidgetClass
            );

        if (ObjectiveNotificationWidget)
        {
            ObjectiveNotificationWidget->AddToViewport();
        }
    }


    if (ObjectiveNotificationWidget)
    {
        ObjectiveNotificationWidget->ShowNotification(
            FText::Format(
                NSLOCTEXT(
                    "BrokenHorizon",
                    "ObjectiveCompletedNotification",
                    "OBJECTIVE COMPLETE\n\n\u2713 {0}"
                ),
                CompletedObjective
            )
        );
    }

    if (IsValid(ObjectiveWidget) && IsValid(ObjectiveComponent))
    {
        ObjectiveWidget->SetObjectiveList(
            ObjectiveComponent->GetCompletedObjectives(),
            ObjectiveComponent->GetCurrentObjective()
        );
    }
}
