#include "BHSalvagePickup.h"

#include "BHCharacter.h"
#include "BHWeaponComponent.h"
#include "BHSaveSubsystem.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/GameInstance.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "HAL/IConsoleManager.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
FText SalvageTypeText(EBHSalvagePickupType Type)
{
    switch (Type)
    {
        case EBHSalvagePickupType::FragGrenades:
            return NSLOCTEXT("BrokenHorizon", "SalvageFrag", "FRAG");
        case EBHSalvagePickupType::SmokeGrenades:
            return NSLOCTEXT("BrokenHorizon", "SalvageSmoke", "SMOKE");
        case EBHSalvagePickupType::EngineeringCharges:
            return NSLOCTEXT("BrokenHorizon", "SalvageEngineering", "ENGINEERING");
        case EBHSalvagePickupType::AntiVehicleRounds:
            return NSLOCTEXT("BrokenHorizon", "SalvageAntiVehicle", "ANTI-VEHICLE");
        default:
            return NSLOCTEXT("BrokenHorizon", "SalvageAmmo", "AMMO");
    }
}

#if !UE_BUILD_SHIPPING
static FAutoConsoleCommandWithWorldAndArgs GSpawnSalvagePickupCommand(
    TEXT("BHTestSpawnSalvagePickup"),
    TEXT("Spawns a test ammunition salvage pickup in front of the local player."),
    FConsoleCommandWithWorldAndArgsDelegate::CreateLambda(
        [](const TArray<FString>& Args, UWorld* World)
        {
            if (!IsValid(World))
            {
                return;
            }

            APlayerController* PlayerController =
                World->GetFirstPlayerController();
            APawn* Pawn = IsValid(PlayerController)
                ? PlayerController->GetPawn()
                : nullptr;
            if (!IsValid(Pawn))
            {
                return;
            }

            const FVector SpawnLocation =
                Pawn->GetActorLocation() +
                Pawn->GetActorForwardVector() * 180.0f +
                FVector(0.0f, 0.0f, 20.0f);
            ABHSalvagePickup* Pickup = World->SpawnActor<ABHSalvagePickup>(
                ABHSalvagePickup::StaticClass(),
                SpawnLocation,
                FRotator::ZeroRotator
            );
            if (IsValid(Pickup))
            {
                Pickup->ConfigureSalvage(
                    TEXT("BHTestSalvagePickup01"),
                    EBHSalvagePickupType::Ammunition,
                    30
                );
                UE_LOG(LogTemp, Display,
                    TEXT("BH_TEST_SALVAGE_SPAWNED id=BHTestSalvagePickup01"));
            }
        }
    )
);

static FAutoConsoleCommandWithWorldAndArgs GCollectSalvagePickupCommand(
    TEXT("BHTestCollectSalvagePickup"),
    TEXT("Collects the authored or test salvage pickup through the interaction contract."),
    FConsoleCommandWithWorldAndArgsDelegate::CreateLambda(
        [](const TArray<FString>& Args, UWorld* World)
        {
            if (!IsValid(World))
            {
                return;
            }

            const bool bSaveRequested = Args.Contains(TEXT("save"));
            TWeakObjectPtr<UWorld> WeakWorld(World);
            World->GetTimerManager().SetTimerForNextTick(
                FTimerDelegate::CreateLambda(
                    [WeakWorld, bSaveRequested]()
                    {
                        UWorld* DeferredWorld = WeakWorld.Get();
                        if (!IsValid(DeferredWorld))
                        {
                            return;
                        }

                        APlayerController* PlayerController =
                            DeferredWorld->GetFirstPlayerController();
                        APawn* Pawn = IsValid(PlayerController)
                            ? PlayerController->GetPawn()
                            : nullptr;
                        if (!IsValid(Pawn))
                        {
                            UE_LOG(LogTemp, Warning,
                                TEXT("BH_TEST_SALVAGE_COLLECT_REQUEST no_pawn"));
                            return;
                        }

                        const TArray<FName> CandidateIDs = {
                            FName(TEXT("FirstLightSalvageCache01")),
                            FName(TEXT("BHTestSalvagePickup01"))
                        };
                        for (TActorIterator<ABHSalvagePickup> It(DeferredWorld); It; ++It)
                        {
                            if (IsValid(*It) && CandidateIDs.Contains((*It)->GetPersistenceID()))
                            {
                                IBHInteractable::Execute_Interact(*It, Pawn);
                                UE_LOG(LogTemp, Display,
                                    TEXT("BH_TEST_SALVAGE_COLLECT_REQUEST id=%s"),
                                    *(*It)->GetPersistenceID().ToString());
                                if (bSaveRequested)
                                {
                                    if (UGameInstance* GameInstance =
                                            DeferredWorld->GetGameInstance())
                                    {
                                        if (UBHSaveSubsystem* SaveSubsystem =
                                                GameInstance->GetSubsystem<UBHSaveSubsystem>())
                                        {
                                            const bool bSaved =
                                                SaveSubsystem->SaveProgress();
                                            UE_LOG(LogTemp, Display,
                                                TEXT("BH_TEST_SALVAGE_SAVE_RESULT saved=%s id=%s"),
                                                bSaved ? TEXT("true") : TEXT("false"),
                                                *(*It)->GetPersistenceID().ToString());
                                        }
                                    }
                                }
                                return;
                            }
                        }
                        UE_LOG(LogTemp, Warning,
                            TEXT("BH_TEST_SALVAGE_COLLECT_REQUEST no_target"));
                    }));
            UE_LOG(LogTemp, Display,
                TEXT("BH_TEST_SALVAGE_COLLECT_REQUEST deferred"));
        }
    )
);

static FAutoConsoleCommandWithWorldAndArgs GVerifySalvagePersistenceCommand(
    TEXT("BHTestVerifySalvagePersistence"),
    TEXT("Logs whether the salvage persistence ID is consumed in the current campaign save."),
    FConsoleCommandWithWorldAndArgsDelegate::CreateLambda(
        [](const TArray<FString>&, UWorld* World)
        {
            if (!IsValid(World) || !IsValid(World->GetGameInstance()))
            {
                return;
            }
            if (UBHSaveSubsystem* SaveSubsystem =
                    World->GetGameInstance()->GetSubsystem<UBHSaveSubsystem>())
            {
                const FName ID(TEXT("FirstLightSalvageCache01"));
                UE_LOG(LogTemp, Display,
                    TEXT("BH_TEST_SALVAGE_PERSISTENCE_RESULT id=%s consumed=%s"),
                    *ID.ToString(),
                    SaveSubsystem->IsWorldItemConsumed(ID)
                        ? TEXT("true")
                        : TEXT("false"));
            }
        }
    )
);

static FAutoConsoleCommandWithWorldAndArgs GResetSalvagePersistenceCommand(
    TEXT("BHTestResetSalvagePersistence"),
    TEXT("Deletes the development campaign save so the authored salvage cache can be retested."),
    FConsoleCommandWithWorldAndArgsDelegate::CreateLambda(
        [](const TArray<FString>&, UWorld* World)
        {
            if (!IsValid(World) || !IsValid(World->GetGameInstance()))
            {
                return;
            }
            if (UBHSaveSubsystem* SaveSubsystem =
                    World->GetGameInstance()->GetSubsystem<UBHSaveSubsystem>())
            {
                UE_LOG(LogTemp, Display,
                    TEXT("BH_TEST_SALVAGE_RESET_RESULT deleted=%s"),
                    SaveSubsystem->DeleteSaveGame() ? TEXT("true") : TEXT("false"));
            }
        }
    )
);
#endif
}

ABHSalvagePickup::ABHSalvagePickup()
{
    PrimaryActorTick.bCanEverTick = false;
    SetReplicates(true);

    PickupMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PickupMesh"));
    SetRootComponent(PickupMesh);
    PickupMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    PickupMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
    PickupMesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);

    static ConstructorHelpers::FObjectFinder<UStaticMesh> MeshAsset(
        TEXT("/Engine/BasicShapes/Cube.Cube")
    );
    if (MeshAsset.Succeeded())
    {
        PickupMesh->SetStaticMesh(MeshAsset.Object);
        PickupMesh->SetRelativeScale3D(FVector(0.35f, 0.35f, 0.18f));
    }

    PickupLabel = CreateDefaultSubobject<UTextRenderComponent>(TEXT("PickupLabel"));
    PickupLabel->SetupAttachment(PickupMesh);
    PickupLabel->SetRelativeLocation(FVector(0.0f, 0.0f, 45.0f));
    PickupLabel->SetHorizontalAlignment(EHTA_Center);
    PickupLabel->SetWorldSize(20.0f);
    PickupLabel->SetTextRenderColor(FColor(210, 220, 205));
    PickupLabel->SetText(NSLOCTEXT("BrokenHorizon", "SalvageLabelDefault", "SALVAGE"));
}

void ABHSalvagePickup::BeginPlay()
{
    Super::BeginPlay();

    if (!HasAuthority())
    {
        UE_LOG(
            LogTemp,
            Display,
            TEXT(
                "BH_SALVAGE_STATE_REPLICATED id=%s type=%s quantity=%d "
                "source=begin_play"
            ),
            *PersistenceID.ToString(),
            *SalvageTypeText(SalvageType).ToString(),
            Quantity
        );
    }

    if (PersistenceID.IsNone())
    {
        return;
    }

    UGameInstance* GameInstance = GetGameInstance();
    UBHSaveSubsystem* SaveSubsystem = IsValid(GameInstance)
        ? GameInstance->GetSubsystem<UBHSaveSubsystem>()
        : nullptr;
    if (IsValid(SaveSubsystem) &&
        SaveSubsystem->IsWorldItemConsumed(PersistenceID))
    {
        Destroy();
    }
}

void ABHSalvagePickup::ConfigureSalvage(
    FName NewPersistenceID,
    EBHSalvagePickupType NewType,
    int32 NewQuantity
)
{
    PersistenceID = NewPersistenceID;
    SalvageType = NewType;
    Quantity = FMath::Max(1, NewQuantity);
    RefreshSalvageLabel();
}

void ABHSalvagePickup::RefreshSalvageLabel()
{
    if (IsValid(PickupLabel))
    {
        PickupLabel->SetText(FText::Format(
            NSLOCTEXT("BrokenHorizon", "SalvageLabel", "SALVAGE // {0} // {1}"),
            SalvageTypeText(SalvageType),
            FText::AsNumber(Quantity)
        ));
    }
}

void ABHSalvagePickup::OnRep_SalvageState()
{
    RefreshSalvageLabel();
    UE_LOG(
        LogTemp,
        Display,
        TEXT(
            "BH_SALVAGE_STATE_REPLICATED id=%s type=%s quantity=%d"
        ),
        *PersistenceID.ToString(),
        *SalvageTypeText(SalvageType).ToString(),
        Quantity
    );
}

void ABHSalvagePickup::GetLifetimeReplicatedProps(
    TArray<FLifetimeProperty>& OutLifetimeProps
) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ABHSalvagePickup, PersistenceID);
    DOREPLIFETIME(ABHSalvagePickup, SalvageType);
    DOREPLIFETIME(ABHSalvagePickup, Quantity);
}

void ABHSalvagePickup::Interact_Implementation(AActor* InteractingActor)
{
    ABHCharacter* Character = Cast<ABHCharacter>(InteractingActor);
    if (!IsValid(Character) || !HasAuthority())
    {
        return;
    }

    int32 Accepted = 0;
    switch (SalvageType)
    {
        case EBHSalvagePickupType::Ammunition:
            if (UBHWeaponComponent* Weapon = Character->GetWeaponComponent())
            {
                Accepted = Weapon->AddReserveAmmo(Quantity);
            }
            break;
        case EBHSalvagePickupType::FragGrenades:
            Accepted = Character->AddFragGrenades(Quantity);
            break;
        case EBHSalvagePickupType::SmokeGrenades:
            Accepted = Character->AddSmokeGrenades(Quantity);
            break;
        case EBHSalvagePickupType::EngineeringCharges:
            Accepted = Character->AddEngineeringCharges(Quantity);
            break;
        case EBHSalvagePickupType::AntiVehicleRounds:
            Accepted = Character->AddAntiVehicleRounds(Quantity);
            break;
    }

    if (Accepted <= 0)
    {
        Character->ShowStatusNotification(
            FText::Format(
                NSLOCTEXT("BrokenHorizon", "SalvagePickupFull", "RECOVERY FAILED // {0} CAPACITY FULL"),
                SalvageTypeText(SalvageType)));
        return;
    }

    Quantity -= Accepted;
    if (HasAuthority())
    {
        ForceNetUpdate();
    }
    UE_LOG(LogTemp, Display, TEXT("BH_SALVAGE_PICKUP id=%s type=%s accepted=%d remaining=%d"),
        *PersistenceID.ToString(),
        *SalvageTypeText(SalvageType).ToString(),
        Accepted,
        Quantity);
    Character->ShowStatusNotification(
        FText::Format(
            NSLOCTEXT("BrokenHorizon", "SalvagePickupAccepted", "RECOVERED // {0} x{1}"),
            SalvageTypeText(SalvageType),
            FText::AsNumber(Accepted)));
    if (Quantity <= 0)
    {
        if (!PersistenceID.IsNone())
        {
            if (UGameInstance* GameInstance = GetGameInstance())
            {
                if (UBHSaveSubsystem* SaveSubsystem =
                        GameInstance->GetSubsystem<UBHSaveSubsystem>())
                {
                    SaveSubsystem->RecordConsumedWorldItem(PersistenceID);
                }
            }
        }
        Destroy();
    }
}

FText ABHSalvagePickup::BuildInteractionText(
    EBHSalvagePickupType Type,
    int32 InQuantity
)
{
    return FText::Format(
        NSLOCTEXT(
            "BrokenHorizon",
            "SalvageInteraction",
            "Press [F] to RECOVER {0} // {1}"
        ),
        SalvageTypeText(Type),
        FText::AsNumber(FMath::Max(0, InQuantity))
    );
}

FText ABHSalvagePickup::GetInteractionText_Implementation() const
{
    return BuildInteractionText(SalvageType, Quantity);
}

FName ABHSalvagePickup::GetPersistenceID() const { return PersistenceID; }
EBHSalvagePickupType ABHSalvagePickup::GetSalvageType() const { return SalvageType; }
int32 ABHSalvagePickup::GetQuantity() const { return Quantity; }
