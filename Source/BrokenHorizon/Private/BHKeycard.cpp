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

void ABHKeycard::Interact_Implementation(AActor* InteractingActor)
{
    ABHCharacter* Character = Cast<ABHCharacter>(InteractingActor);

    if (!IsValid(Character))
    {
        return;
    }

    Character->AddKeycard(KeycardID);
    Character->CompleteObjective(
        BHObjectiveIds::FindRedKeycard
    );

    Destroy();
}

FText ABHKeycard::GetInteractionText_Implementation() const
{
    return FText::FromString(TEXT("Press [F] to Pick Up Red Keycard"));
}
