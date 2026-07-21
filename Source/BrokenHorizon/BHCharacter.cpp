#include "BHCharacter.h"

#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/PlayerController.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "GameFramework/CharacterMovementComponent.h"

ABHCharacter::ABHCharacter()
{
    PrimaryActorTick.bCanEverTick = false;

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
}

void ABHCharacter::BeginPlay()
{
    Super::BeginPlay();

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
    GetCharacterMovement()->MaxWalkSpeed = SprintSpeed;
}

void ABHCharacter::StopSprint()
{
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