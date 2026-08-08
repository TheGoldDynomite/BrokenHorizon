// Fill out your copyright notice in the Description page of Project Settings.


#include "BHGameMode.h"

#include "BHCharacter.h"
#include "BHWarGameState.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"

ABHGameMode::ABHGameMode()
{
	GameStateClass = ABHWarGameState::StaticClass();
	bUseSeamlessTravel = true;
}

void ABHGameMode::HandleStartingNewPlayer_Implementation(
	APlayerController* NewPlayer
)
{
	Super::HandleStartingNewPlayer_Implementation(NewPlayer);

	ABHCharacter* NewCharacter = IsValid(NewPlayer)
		? Cast<ABHCharacter>(NewPlayer->GetPawn())
		: nullptr;
	if (!IsValid(NewCharacter))
	{
		return;
	}

	const ABHCharacter* SharedStateSource = nullptr;
	int32 BestCompletedObjectiveCount = -1;
	for (TActorIterator<ABHCharacter> It(GetWorld()); It; ++It)
	{
		const ABHCharacter* Candidate = *It;
		if (!IsValid(Candidate) ||
			Candidate == NewCharacter ||
			!Candidate->IsPlayerControlled())
		{
			continue;
		}

		const int32 CompletedObjectiveCount =
			Candidate->GetCompletedObjectiveIDs().Num();
		if (CompletedObjectiveCount > BestCompletedObjectiveCount)
		{
			SharedStateSource = Candidate;
			BestCompletedObjectiveCount = CompletedObjectiveCount;
		}
	}

	if (IsValid(SharedStateSource))
	{
		NewCharacter->AdoptSharedMissionStateFrom(SharedStateSource);
	}
}

void ABHGameMode::InitGameState()
{
	Super::InitGameState();

	if (!IsValid(Cast<ABHWarGameState>(GameState)))
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT(
				"BH_WAR_GAME_STATE_MISSING actual=%s expected=%s"
			),
			IsValid(GameState)
				? *GameState->GetClass()->GetPathName()
				: TEXT("None"),
			*ABHWarGameState::StaticClass()->GetPathName()
		);
	}
}

void ABHGameMode::PostSeamlessTravel()
{
	Super::PostSeamlessTravel();

	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return;
	}

	for (FConstPlayerControllerIterator It =
			World->GetPlayerControllerIterator();
		 It;
		 ++It)
	{
		APlayerController* PlayerController = It->Get();
		if (!IsValid(PlayerController) ||
			IsValid(PlayerController->GetPawn()))
		{
			continue;
		}

		RestartPlayer(PlayerController);
		UE_LOG(
			LogTemp,
			Display,
			TEXT(
				"BH_SEAMLESS_TRAVEL_PLAYER_RECOVERED controller=%s "
				"pawn=%s"
			),
			*GetNameSafe(PlayerController),
			*GetNameSafe(PlayerController->GetPawn())
		);
	}
}
