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
};