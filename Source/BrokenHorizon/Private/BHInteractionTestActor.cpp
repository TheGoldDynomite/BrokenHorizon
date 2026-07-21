#include "BHInteractionTestActor.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"

ABHInteractionTestActor::ABHInteractionTestActor()
{
    PrimaryActorTick.bCanEverTick = false;

    Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
    SetRootComponent(Mesh);
}

void ABHInteractionTestActor::Interact_Implementation(
    AActor* InteractingActor
)
{
    UE_LOG(
        LogTemp,
        Warning,
        TEXT("%s was interacted with by %s"),
        *GetName(),
        IsValid(InteractingActor)
        ? *InteractingActor->GetName()
        : TEXT("Unknown Actor")
    );

    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(
            -1,
            2.0f,
            FColor::Yellow,
            TEXT("Interaction successful!")
        );
    }
}