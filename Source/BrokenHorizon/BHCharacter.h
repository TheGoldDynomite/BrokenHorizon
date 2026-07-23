#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "InputActionValue.h"

#include "BHCharacter.generated.h"

class UCameraComponent;
class UInputAction;
class UInputMappingContext;
class UInputComponent; 
class UBHInteractionPromptWidget;
class UBHObjectiveWidget;
class UBHObjectiveComponent;
class UBHObjectiveNotificationWidget;

UCLASS()
class BROKENHORIZON_API ABHCharacter : public ACharacter
{
    GENERATED_BODY()

public:
    ABHCharacter();


    UFUNCTION(BlueprintCallable, Category = "Inventory")
    void AddKeycard(FName KeycardID);

    UFUNCTION(BlueprintPure, Category = "Inventory")
    bool HasKeycard(FName KeycardID) const;

    UFUNCTION(BlueprintCallable, Category = "Objective")
    void SetObjective(const FText& NewObjective);

    UFUNCTION()
    void OnObjectiveCompleted(FText CompletedObjective);

protected:
    virtual void BeginPlay() override;

    virtual void SetupPlayerInputComponent(
        UInputComponent* PlayerInputComponent) override;

    void Move(const FInputActionValue& Value);
    void Look(const FInputActionValue& Value);

    void StartJump();
    void StopJump();

    void StartSprint();
    void StopSprint();

    void StartCrouch();
    void StopCrouch();

    virtual void Tick(float DeltaTime) override;
    
    void UpdateInteractionPrompt();
    
    void Interact();
  

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

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction")
    float InteractionDistance = 300.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Broken Horizon|Input")
    TObjectPtr<UInputAction> InteractAction;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Broken Horizon|Interaction")
    TSubclassOf<UBHInteractionPromptWidget> InteractionPromptClass;

    UPROPERTY()
    TObjectPtr<UBHInteractionPromptWidget> InteractionPromptWidget;


    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory", meta = (AllowPrivateAccess = "true"))
    TSet<FName> OwnedKeycards;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objective")
    TSubclassOf<UBHObjectiveWidget> ObjectiveWidgetClass;

    UPROPERTY()
    TObjectPtr<UBHObjectiveWidget> ObjectiveWidget;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Objectives", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UBHObjectiveComponent> ObjectiveComponent;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objectives")
    TSubclassOf<UBHObjectiveNotificationWidget> ObjectiveNotificationWidgetClass;

    UPROPERTY()
    TObjectPtr<UBHObjectiveNotificationWidget> ObjectiveNotificationWidget;
  
};
