#include "BHKeycard.h"

#include "BHCharacter.h"
#include "BHMissionData.h"
#include "Components/StaticMeshComponent.h"

ABHKeycard::ABHKeycard()
{
    PrimaryActorTick.bCanEverTick = false;

    KeycardMesh = CreateDefaultSubobject<UStaticMeshComponent>(
        TEXT("KeycardMesh")
    );

    SetRootComponent(KeycardMesh);

    KeycardMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    KeycardMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
    KeycardMesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
    KeycardMesh->SetSimulatePhysics(false);
}

void ABHKeycard::BeginPlay()
{
    Super::BeginPlay();

    if (PersistenceID.IsNone())
    {
        UE_LOG(
            LogTemp,
            Error,
            TEXT("Keycard %s has no persistence ID."),
            *GetPathName()
        );
    }
}

void ABHKeycard::Interact_Implementation(AActor* InteractingActor)
{
    ABHCharacter* Character = Cast<ABHCharacter>(InteractingActor);

    if (!IsValid(Character))
    {
        return;
    }

    if (!Character->CollectKeycard(KeycardID, PersistenceID))
    {
        return;
    }

    Character->CompleteObjective(
        BHObjectiveIds::FindRedKeycard
    );

    Destroy();
}

FText ABHKeycard::GetInteractionText_Implementation() const
{
    return FText::FromString(TEXT("Press [F] to Pick Up Red Keycard"));
}

FName ABHKeycard::GetPersistenceID() const
{
    return PersistenceID;
}
