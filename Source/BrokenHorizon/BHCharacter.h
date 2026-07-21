#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "InputActionValue.h"
#include "BHCharacter.generated.h"

class UCameraComponent;
class UInputAction;
class UInputMappingContext;
class UInputComponent;

UCLASS()
class BROKENHORIZON_API ABHCharacter : public ACharacter
{
    GENERATED_BODY()

public:
    ABHCharacter();

protected:
    virtual void BeginPlay() override;

    virtual void SetupPlayerInputComponent(
        UInputComponent* PlayerInputComponent) override;

    void Move(const FInputActionValue& Value);
    void Look(const FInputActionValue& Value);\

    void StartJump();
    void StopJump();

    void StartSprint();
    void StopSprint();

    void StartCrouch();
    void StopCrouch();

    virtual void Tick(float DeltaTime) override;

    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "Broken Horizon|Camera")
    TObjectPtr<UCameraComponent> FirstPersonCamera;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Broken Horizon|Input")
    TObjectPtr<UInputMappingContext> PlayerMappingContext;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Broken Horizon|Input")
    TObjectPtr<UInputAction> MoveAction;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Broken Horizon|Input")
    TObjectPtr<UInputAction> JumpAction;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Broken Horizon|Input")
    TObjectPtr<UInputAction> LookAction;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Broken Horizon|Input")
    TObjectPtr<UInputAction> SprintAction;

    UPROPERTY(
        EditDefaultsOnly, 
        BlueprintReadOnly,
        Category = "Broken Horizon|Input")
    TObjectPtr<UInputAction> CrouchAction;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Broken Horizon|Movement")
    float WalkSpeed = 400.0f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Broken Horizon|Movement")
    float SprintSpeed = 700.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Broken Horizon|Stamina")
    float MaxStamina = 100.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Broken Horizon|Stamina")
    float CurrentStamina = 100.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Broken Horizon|Stamina")
    float StaminaDrainRate = 25.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Broken Horizon|Stamina")
    float StaminaRecoveryRate = 20.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Broken Horizon|Stamina")
    float StaminaRecoveryDelay = 1.0f;

    bool bIsSprinting = false;
    float TimeSinceSprintStopped = 0.0f;

  
};