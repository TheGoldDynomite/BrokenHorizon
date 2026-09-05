#include "BHMissionItemContainer.h"

#include "BHCharacter.h"
#include "BHSaveSubsystem.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/StaticMesh.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

ABHMissionItemContainer::ABHMissionItemContainer()
{
    PrimaryActorTick.bCanEverTick = false;
    SetReplicates(true);

    ContainerMesh = CreateDefaultSubobject<UStaticMeshComponent>(
        TEXT("ContainerMesh")
    );
    SetRootComponent(ContainerMesh);
    ContainerMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    ContainerMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
    ContainerMesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
    ContainerMesh->SetRelativeScale3D(FVector(0.9f, 0.9f, 0.55f));

    static ConstructorHelpers::FObjectFinder<UStaticMesh> MeshAsset(
        TEXT("/Engine/BasicShapes/Cube.Cube")
    );
    if (MeshAsset.Succeeded())
    {
        ContainerMesh->SetStaticMesh(MeshAsset.Object);
    }

    ContainerLabel = CreateDefaultSubobject<UTextRenderComponent>(
        TEXT("ContainerLabel")
    );
    ContainerLabel->SetupAttachment(ContainerMesh);
    ContainerLabel->SetRelativeLocation(FVector(0.0f, 0.0f, 75.0f));
    ContainerLabel->SetHorizontalAlignment(EHTA_Center);
    ContainerLabel->SetWorldSize(18.0f);
    ContainerLabel->SetTextRenderColor(FColor(210, 220, 205));
}

void ABHMissionItemContainer::BeginPlay()
{
    Super::BeginPlay();

    if (PersistenceID.IsNone())
    {
        UE_LOG(
            LogTemp,
            Error,
            TEXT("Mission item container %s has no persistence ID."),
            *GetPathName()
        );
    }

    if (MissionItemID.IsNone())
    {
        UE_LOG(
            LogTemp,
            Error,
            TEXT("Mission item container %s has no mission item ID."),
            *GetPathName()
        );
    }

    RefreshContainerLabel();
}

void ABHMissionItemContainer::GetLifetimeReplicatedProps(
    TArray<FLifetimeProperty>& OutLifetimeProps
) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ABHMissionItemContainer, PersistenceID);
    DOREPLIFETIME(ABHMissionItemContainer, MissionItemID);
    DOREPLIFETIME(ABHMissionItemContainer, StoredMissionItemID);
}

void ABHMissionItemContainer::OnRep_StoredMissionItemID()
{
    RefreshContainerLabel();
#if !UE_BUILD_SHIPPING
    UE_LOG(
        LogTemp,
        Display,
        TEXT(
            "BH_MISSION_CACHE_REPLICATED container=%s stored=%s"
        ),
        *PersistenceID.ToString(),
        *StoredMissionItemID.ToString()
    );
#endif
}

void ABHMissionItemContainer::RefreshContainerLabel()
{
    if (!IsValid(ContainerLabel))
    {
        return;
    }

    const FName DisplayID = StoredMissionItemID.IsNone()
        ? MissionItemID
        : StoredMissionItemID;
    const FString StateText = StoredMissionItemID.IsNone()
        ? TEXT("EMPTY")
        : TEXT("STORED");
    ContainerLabel->SetText(FText::FromString(FString::Printf(
        TEXT("MISSION CACHE // %s // %s"),
        *StateText,
        *DisplayID.ToString()
    )));
}

bool ABHMissionItemContainer::PersistContainerState(ABHCharacter* Character) const
{
    if (UGameInstance* GameInstance = GetGameInstance())
    {
        if (UBHSaveSubsystem* SaveSubsystem =
                GameInstance->GetSubsystem<UBHSaveSubsystem>())
        {
            return SaveSubsystem->SaveProgressForCharacter(Character);
        }
    }

    return false;
}

void ABHMissionItemContainer::ShowTransferFailure(
    ABHCharacter* Character,
    const FText& Reason
) const
{
    if (IsValid(Character))
    {
        Character->ShowStatusNotification(Reason);
    }
}

void ABHMissionItemContainer::Interact_Implementation(
    AActor* InteractingActor
)
{
    if (!HasAuthority())
    {
        return;
    }

    ABHCharacter* Character = Cast<ABHCharacter>(InteractingActor);
    if (!IsValid(Character) || MissionItemID.IsNone())
    {
        return;
    }

    const FText ItemText = FText::FromName(MissionItemID);
    if (!StoredMissionItemID.IsNone())
    {
        if (StoredMissionItemID != MissionItemID)
        {
            ShowTransferFailure(
                Character,
                NSLOCTEXT(
                    "BrokenHorizon",
                    "MissionCacheConfigurationMismatch",
                    "CACHE UNAVAILABLE // CONFIGURATION MISMATCH"
                )
            );
            return;
        }

        if (Character->HasKeycard(MissionItemID))
        {
            ShowTransferFailure(
                Character,
                FText::Format(
                    NSLOCTEXT(
                        "BrokenHorizon",
                        "MissionCacheAlreadyCarried",
                        "RETRIEVE FAILED // ALREADY CARRYING {0}"
                    ),
                    ItemText
                )
            );
            return;
        }

        const FName PreviousStoredMissionItemID = StoredMissionItemID;
        StoredMissionItemID = NAME_None;
        Character->AddKeycard(MissionItemID);
        ForceNetUpdate();
        RefreshContainerLabel();

        if (!PersistContainerState(Character))
        {
            Character->RemoveKeycard(MissionItemID);
            StoredMissionItemID = PreviousStoredMissionItemID;
            ForceNetUpdate();
            RefreshContainerLabel();
            ShowTransferFailure(
                Character,
                NSLOCTEXT(
                    "BrokenHorizon",
                    "MissionCacheSaveFailedRetrieve",
                    "RETRIEVE FAILED // CHECKPOINT NOT SAVED"
                )
            );
            return;
        }

        Character->ShowStatusNotification(FText::Format(
            NSLOCTEXT(
                "BrokenHorizon",
                "MissionCacheRetrieved",
                "RETRIEVED // {0}"
            ),
            ItemText
        ));
        UE_LOG(
            LogTemp,
            Display,
            TEXT(
                "BH_MISSION_CACHE_TRANSFER operation=retrieve "
                "container=%s item=%s"
            ),
            *PersistenceID.ToString(),
            *MissionItemID.ToString()
        );
        return;
    }

    if (!Character->HasKeycard(MissionItemID))
    {
        ShowTransferFailure(
            Character,
            FText::Format(
                NSLOCTEXT(
                    "BrokenHorizon",
                    "MissionCacheRequiredItem",
                    "CACHE EMPTY // REQUIRED ITEM: {0}"
                ),
                ItemText
            )
        );
        return;
    }

    if (!Character->RemoveKeycard(MissionItemID))
    {
        return;
    }

    StoredMissionItemID = MissionItemID;
    ForceNetUpdate();
    RefreshContainerLabel();

    if (!PersistContainerState(Character))
    {
        StoredMissionItemID = NAME_None;
        Character->AddKeycard(MissionItemID);
        ForceNetUpdate();
        RefreshContainerLabel();
        ShowTransferFailure(
            Character,
            NSLOCTEXT(
                "BrokenHorizon",
                "MissionCacheSaveFailedStore",
                "STORE FAILED // CHECKPOINT NOT SAVED"
            )
        );
        return;
    }

    Character->ShowStatusNotification(FText::Format(
        NSLOCTEXT(
            "BrokenHorizon",
            "MissionCacheStored",
            "STORED // {0}"
        ),
        ItemText
    ));
    UE_LOG(
        LogTemp,
        Display,
        TEXT(
            "BH_MISSION_CACHE_TRANSFER operation=store "
            "container=%s item=%s"
        ),
        *PersistenceID.ToString(),
        *MissionItemID.ToString()
    );
}

FText ABHMissionItemContainer::BuildInteractionText(
    FName ConfiguredMissionItemID,
    FName InStoredMissionItemID
)
{
    if (ConfiguredMissionItemID.IsNone())
    {
        return NSLOCTEXT(
            "BrokenHorizon",
            "MissionCacheUnconfigured",
            "MISSION CACHE UNCONFIGURED"
        );
    }

    const bool bHasStoredItem = !InStoredMissionItemID.IsNone();
    const FName DisplayItemID = bHasStoredItem
        ? InStoredMissionItemID
        : ConfiguredMissionItemID;

    return FText::Format(
        bHasStoredItem
            ? NSLOCTEXT(
                "BrokenHorizon",
                "MissionCacheRetrievePrompt",
                "Press [F] to RETRIEVE {0} FROM MISSION CACHE"
            )
            : NSLOCTEXT(
                "BrokenHorizon",
                "MissionCacheStorePrompt",
                "Press [F] to STORE {0} IN MISSION CACHE"
            ),
        FText::FromName(DisplayItemID)
    );
}

FText ABHMissionItemContainer::GetInteractionText_Implementation() const
{
    return BuildInteractionText(
        MissionItemID,
        StoredMissionItemID
    );
}

void ABHMissionItemContainer::RestoreStoredMissionItem(
    FName NewStoredMissionItemID
)
{
    if (!HasAuthority())
    {
        return;
    }

    StoredMissionItemID = NewStoredMissionItemID == MissionItemID
        ? NewStoredMissionItemID
        : NAME_None;
    ForceNetUpdate();
    RefreshContainerLabel();
}

FName ABHMissionItemContainer::GetPersistenceID() const
{
    return PersistenceID;
}

FName ABHMissionItemContainer::GetMissionItemID() const
{
    return MissionItemID;
}

FName ABHMissionItemContainer::GetStoredMissionItemID() const
{
    return StoredMissionItemID;
}
