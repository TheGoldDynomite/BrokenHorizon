// Fill out your copyright notice in the Description page of Project Settings.


#include "BHCharacter.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "GameFramework/PlayerController.h"

// Sets default values
ABHCharacter::ABHCharacter()
{
 	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
    PrimaryActorTick.bCanEverTick = false;

    FirstPersonCamera =
        CreateDefaultSubobject<UCameraComponent>(
            TEXT("FirstPersonCamera"));

    FirstPersonCamera->SetupAttachment(GetCapsuleComponent());

    FirstPersonCamera->SetRelativeLocation(
        FVector(0.0f, 0.0f, 64.0f));

    FirstPersonCamera->bUsePawnControlRotation = true;
    FirstPersonCamera->SetFieldOfView(90.0f);

    bUseControllerRotationYaw = true;

}

// Called when the game starts or when spawned
void ABHCharacter::BeginPlay()
{
    Super::BeginPlay();

    APlayerController* PlayerController =
        Cast<APlayerController>(GetController());

    if (!PlayerController)
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

    if (InputSubsystem && PlayerMappingContext)
    {
        InputSubsystem->AddMappingContext(
            PlayerMappingContext,
            0);

    }
}

// Called every frame
void ABHCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

// Called to bind functionality to input
void ABHCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

}

