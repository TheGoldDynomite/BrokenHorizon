#include "BHKeycard.h"

#include "BHCharacter.h"
#include "BHMissionData.h"
#include "Components/StaticMeshComponent.h"
#include "EngineUtils.h"

ABHKeycard::ABHKeycard()
{
    PrimaryActorTick.bCanEverTick = false;
    SetReplicates(true);

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
    if (!HasAuthority())
    {
        return;
    }

    ABHCharacter* Character = Cast<ABHCharacter>(InteractingActor);

    if (!IsValid(Character))
    {
        return;
    }

    bool bCollectorReceivedKeycard = false;
    int32 RecipientCount = 0;
    for (TActorIterator<ABHCharacter> It(GetWorld()); It; ++It)
    {
        ABHCharacter* PlayerCharacter = *It;
        if (!IsValid(PlayerCharacter) ||
            !PlayerCharacter->IsPlayerControlled())
        {
            continue;
        }

        const bool bCollected = PlayerCharacter->CollectKeycard(
            KeycardID,
            PersistenceID
        );
        if (bCollected)
        {
            ++RecipientCount;
        }
        if (PlayerCharacter == Character)
        {
            bCollectorReceivedKeycard = bCollected;
        }
    }

    if (!bCollectorReceivedKeycard)
    {
        return;
    }

    UE_LOG(
        LogTemp,
        Display,
        TEXT("BH_SHARED_KEYCARD_COLLECTED id=%s players=%d"),
        *KeycardID.ToString(),
        RecipientCount
    );

    Character->CompleteSharedObjective(
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
