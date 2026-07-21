// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "BHCharacter.generated.h"

class UCameraComponent;
class UInputMappingContext;

UCLASS()
class BROKENHORIZON_API ABHCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	// Sets default values for this character's properties
	ABHCharacter();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	UPROPERTY(
	VisibleAnywhere,
	BlueprintReadOnly,
	Category = "Broken Horizon|Camera"
	)
	TObjectPtr<UCameraComponent> FirstPersonCamera;
	
	UPROPERTY(
		EditDefaultsOnly,
		BlueprintReadOnly,
		Category = "Broken Horizon|Input"
	)
	TObjectPtr<UInputMappingContext> PlayerMappingContext;


public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	// Called to bind functionality to input
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

};
