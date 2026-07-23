#include "BHDoor.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"

ABHDoor::ABHDoor()
{
    PrimaryActorTick.bCanEverTick = false;

    DoorRoot = CreateDefaultSubobject<USceneComponent>(TEXT("DoorRoot"));
    SetRootComponent(DoorRoot);

    DoorMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DoorMesh"));
    DoorMesh->SetupAttachment(DoorRoot);
}

void ABHDoor::Interact_Implementation(
    AActor* InteractingActor
)
{
    bIsOpen = !bIsOpen;

    const float TargetYaw = bIsOpen ? OpenAngle : 0.0f;

    DoorRoot->SetRelativeRotation(
        FRotator(0.0f, TargetYaw, 0.0f)
    );
}

FText ABHDoor::GetInteractionText_Implementation() const
{
    return bIsOpen
        ? FText::FromString(TEXT("Press [F] to Close Door"))
        : FText::FromString(TEXT("Press [F] to Open Door"));
}