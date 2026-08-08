// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "BHGameMode.generated.h"

/**
 *
 */
UCLASS()
class BROKENHORIZON_API ABHGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ABHGameMode();

	virtual void InitGameState() override;
	virtual void HandleStartingNewPlayer_Implementation(
		APlayerController* NewPlayer
	) override;
	virtual void PostSeamlessTravel() override;
};
