#include "BHCheckpoint.h"

#include "BHCharacter.h"
#include "BHSaveSubsystem.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"

ABHCheckpoint::ABHCheckpoint()
{
    PrimaryActorTick.bCanEverTick = false;

    CheckpointMesh = CreateDefaultSubobject<UStaticMeshComponent>(
        TEXT("CheckpointMesh")
    );
    SetRootComponent(CheckpointMesh);

    CheckpointMesh->SetCollisionEnabled(
        ECollisionEnabled::QueryOnly
    );
    CheckpointMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
    CheckpointMesh->SetCollisionResponseToChannel(
        ECC_Visibility,
        ECR_Block
    );

    InteractionText = NSLOCTEXT(
        "BrokenHorizon",
        "CheckpointInteraction",
        "Save at Checkpoint"
    );
}

void ABHCheckpoint::Interact_Implementation(
    AActor* InteractingActor
)
{
    if (!IsValid(Cast<ABHCharacter>(InteractingActor)))
    {
        return;
    }

    UGameInstance* GameInstance = GetGameInstance();
    UBHSaveSubsystem* SaveSubsystem = GameInstance
        ? GameInstance->GetSubsystem<UBHSaveSubsystem>()
        : nullptr;

    if (!IsValid(SaveSubsystem) ||
        !SaveSubsystem->SaveProgress())
    {
        UE_LOG(LogTemp, Error, TEXT("Checkpoint save failed."));
    }
}

FText ABHCheckpoint::GetInteractionText_Implementation() const
{
    return InteractionText;
}
