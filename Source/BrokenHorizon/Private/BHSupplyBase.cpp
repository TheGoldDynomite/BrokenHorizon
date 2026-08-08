#include "BHSupplyBase.h"

#include "BHCharacter.h"
#include "BHSaveSubsystem.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/GameInstance.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Net/UnrealNetwork.h"

ABHSupplyBase::ABHSupplyBase()
{
    PrimaryActorTick.bCanEverTick = false;
    SetReplicates(true);

    SupplyRoot = CreateDefaultSubobject<USceneComponent>(
        TEXT("SupplyRoot")
    );
    SetRootComponent(SupplyRoot);

    SupplyMesh = CreateDefaultSubobject<UStaticMeshComponent>(
        TEXT("SupplyMesh")
    );
    SupplyMesh->SetupAttachment(SupplyRoot);
    SupplyMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    SupplyMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
    SupplyMesh->SetCollisionResponseToChannel(
        ECC_Visibility,
        ECR_Block
    );
}

void ABHSupplyBase::GetLifetimeReplicatedProps(
    TArray<FLifetimeProperty>& OutLifetimeProps
) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(ABHSupplyBase, bConsumed);
    DOREPLIFETIME(ABHSupplyBase, bRuntimeSupply);
}

void ABHSupplyBase::BeginPlay()
{
    Super::BeginPlay();

    if (PersistenceID.IsNone())
    {
        if (!bRuntimeSupply)
        {
            UE_LOG(
                LogTemp,
                Warning,
                TEXT(
                    "Supply %s has no persistence ID and will only "
                    "remain consumed for the current level instance."
                ),
                *GetPathName()
            );
        }
        return;
    }

    UGameInstance* GameInstance = GetGameInstance();
    const UBHSaveSubsystem* SaveSubsystem = GameInstance
        ? GameInstance->GetSubsystem<UBHSaveSubsystem>()
        : nullptr;

    if (IsValid(SaveSubsystem) &&
        SaveSubsystem->IsWorldItemConsumed(PersistenceID))
    {
        RestoreConsumedState(true);
    }
}

void ABHSupplyBase::Interact_Implementation(
    AActor* InteractingActor
)
{
    if (!HasAuthority())
    {
        return;
    }

    ABHCharacter* Character = Cast<ABHCharacter>(
        InteractingActor
    );

    if (bConsumed ||
        !IsValid(Character) ||
        !TryApplyToCharacter(Character))
    {
        if (IsValid(Character))
        {
            OnSupplyUnavailable(Character);
        }
        return;
    }

    if (bConsumeOnUse && !PersistenceID.IsNone())
    {
        UGameInstance* GameInstance = GetGameInstance();
        UBHSaveSubsystem* SaveSubsystem = GameInstance
            ? GameInstance->GetSubsystem<UBHSaveSubsystem>()
            : nullptr;

        if (!IsValid(SaveSubsystem) ||
            !SaveSubsystem->RecordConsumedWorldItem(PersistenceID))
        {
            UE_LOG(
                LogTemp,
                Error,
                TEXT(
                    "Supply %s was used, but persistence ID %s "
                    "could not be recorded."
                ),
                *GetPathName(),
                *PersistenceID.ToString()
            );
        }
    }

    OnSupplyUsed(Character);

    if (bConsumeOnUse)
    {
        bConsumed = true;
        DisableConsumedSupply();
    }
}

FText ABHSupplyBase::GetInteractionText_Implementation() const
{
    return bConsumed ? FText::GetEmpty() : InteractionText;
}

FName ABHSupplyBase::GetPersistenceID() const
{
    return PersistenceID;
}

bool ABHSupplyBase::IsConsumed() const
{
    return bConsumed;
}

bool ABHSupplyBase::IsRuntimeSupply() const
{
    return bRuntimeSupply;
}

void ABHSupplyBase::MarkAsRuntimeSupply()
{
    bRuntimeSupply = true;
    PersistenceID = NAME_None;
}

void ABHSupplyBase::RestoreConsumedState(
    bool bShouldBeConsumed
)
{
    bConsumed = bShouldBeConsumed;

    if (bConsumed)
    {
        DisableConsumedSupply();
    }
    else
    {
        SetActorHiddenInGame(false);
        SupplyMesh->SetCollisionEnabled(
            ECollisionEnabled::QueryOnly
        );
    }
}

void ABHSupplyBase::DisableConsumedSupply()
{
    SupplyMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    SetActorHiddenInGame(true);
}

void ABHSupplyBase::OnRep_Consumed()
{
    if (bConsumed)
    {
        DisableConsumedSupply();
    }
    else
    {
        SetActorHiddenInGame(false);
        SupplyMesh->SetCollisionEnabled(
            ECollisionEnabled::QueryOnly
        );
    }

    LogRuntimeSupplyReplicationIfReady();
}

void ABHSupplyBase::OnRep_RuntimeSupply()
{
#if !UE_BUILD_SHIPPING
    if (!HasAuthority() &&
        bRuntimeSupply &&
        !bConsumed &&
        !bRuntimeSupplyAvailableReplicationLogged &&
        FParse::Param(
            FCommandLine::Get(),
            TEXT("BHTestRuntimeSupplyReplication")))
    {
        bRuntimeSupplyAvailableReplicationLogged = true;
        UE_LOG(
            LogTemp,
            Display,
            TEXT(
                "BH_RUNTIME_SUPPLY_AVAILABLE_REPLICATED "
                "result=success supply=%s"
            ),
            *GetName()
        );
    }
#endif

    LogRuntimeSupplyReplicationIfReady();
}

void ABHSupplyBase::LogRuntimeSupplyReplicationIfReady()
{
#if !UE_BUILD_SHIPPING
    if (!HasAuthority() &&
        bRuntimeSupply &&
        bConsumed &&
        !bRuntimeSupplyReplicationLogged &&
        FParse::Param(
            FCommandLine::Get(),
            TEXT("BHTestRuntimeSupplyReplication")))
    {
        bRuntimeSupplyReplicationLogged = true;
        UE_LOG(
            LogTemp,
            Display,
            TEXT(
                "BH_RUNTIME_SUPPLY_CONSUMED_REPLICATED "
                "result=success supply=%s"
            ),
            *GetName()
        );
    }
#endif
}
