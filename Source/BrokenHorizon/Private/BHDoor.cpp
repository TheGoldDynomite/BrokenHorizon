#include "BHDoor.h"

#include "BHCharacter.h"
#include "BHMissionData.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Net/UnrealNetwork.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

ABHDoor::ABHDoor()
{
    PrimaryActorTick.bCanEverTick = true;
    SetReplicates(true);

    DoorRoot = CreateDefaultSubobject<USceneComponent>(TEXT("DoorRoot"));
    SetRootComponent(DoorRoot);

    DoorMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DoorMesh"));
    DoorMesh->SetupAttachment(DoorRoot);
}

void ABHDoor::GetLifetimeReplicatedProps(
    TArray<FLifetimeProperty>& OutLifetimeProps
) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(ABHDoor, bIsOpen);
    DOREPLIFETIME(ABHDoor, bLocked);
    DOREPLIFETIME(ABHDoor, TargetOpenRotation);
    DOREPLIFETIME(ABHDoor, bBreached);
    DOREPLIFETIME(ABHDoor, bBreachChargePlanted);
}

bool ABHDoor::HasRequiredKeycard(
    FName RequiredKeycardID,
    const TArray<FName>& OwnedKeycardIDs
)
{
    return !RequiredKeycardID.IsNone() &&
        OwnedKeycardIDs.Contains(RequiredKeycardID);
}

void ABHDoor::Interact_Implementation(AActor* InteractingActor)
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

    if (bLocked)
    {
        if (!HasRequiredKeycard(
                RequiredKeycard,
                Character->GetOwnedKeycardIDs()))
        {
            if (!bBreachChargePlanted &&
                Character->TryPlaceBreachingCharge(this))
            {
                bBreachChargePlanted = true;
                ForceNetUpdate();
                UE_LOG(LogTemp, Display, TEXT("BH_DOOR_BREACH state=charge_planted door=%s"), *GetName());
            }
            else
            {
                Character->ShowPriorityStatusNotification(
                    FText::Format(
                        NSLOCTEXT(
                            "BrokenHorizon",
                            "DoorAccessDeniedNotification",
                            "ACCESS DENIED // REQUIRED KEYCARD: {0}"
                        ),
                        RequiredKeycard.IsNone()
                            ? NSLOCTEXT(
                                "BrokenHorizon",
                                "DoorAccessDeniedUnconfiguredCredential",
                                "UNCONFIGURED"
                            )
                            : FText::FromName(RequiredKeycard)
                    ),
                    EBHNotificationPriority::High
                );
                UE_LOG(LogTemp, Warning, TEXT("Access Denied"));
            }
            return;
        }

        bLocked = false;

        Character->CompleteSharedObjective(
            BHObjectiveIds::UnlockSecurityDoor
        );

        Character->ShowPriorityStatusNotification(
            NSLOCTEXT(
                "BrokenHorizon",
                "DoorAccessGrantedNotification",
                "ACCESS GRANTED // SECURITY DOOR UNLOCKED"
            ),
            EBHNotificationPriority::High
        );

        UE_LOG(LogTemp, Warning, TEXT("Door Unlocked"));
    }

    if (!bIsOpen)
    {
        const FVector DoorToPlayer =
            Character->GetActorLocation() - GetActorLocation();

        const float SideDot = FVector::DotProduct(
            GetActorRightVector(),
            DoorToPlayer
        );

        const float Direction =
            SideDot >= 0.0f ? -1.0f : 1.0f;

        TargetOpenRotation =
            ClosedRotation +
            FRotator(0.0f, OpenAngle * Direction, 0.0f);
    }

    bIsOpen = !bIsOpen;
}

FText ABHDoor::GetInteractionText_Implementation() const
{
    if (bLocked)
    {
        return bBreachChargePlanted
            ? NSLOCTEXT("BrokenHorizon", "DoorBreachChargePlanted", "Breach Charge Planted")
            : NSLOCTEXT("BrokenHorizon", "DoorLockedBreachPrompt", "Press [F] to use keycard or plant breaching charge");
    }
    return bIsOpen
        ? FText::FromString(TEXT("Press [F] to Close Door"))
        : FText::FromString(TEXT("Press [F] to Open Door"));
}

void ABHDoor::BeginPlay()
{
    Super::BeginPlay();

    ClosedRotation = DoorRoot->GetRelativeRotation();
    OpenRotation = ClosedRotation + FRotator(0.0f, OpenAngle, 0.0f);
    TargetOpenRotation = OpenRotation;

    if (PersistenceID.IsNone() &&
        !FParse::Param(
            FCommandLine::Get(),
            TEXT("BHTestEngineeringRuntime")
        ))
    {
        UE_LOG(
            LogTemp,
            Error,
            TEXT("Door %s has no persistence ID."),
            *GetPathName()
        );
    }
}

void ABHDoor::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    const FRotator TargetRotation =
        bIsOpen ? TargetOpenRotation : ClosedRotation;

    const FRotator NewRotation = FMath::RInterpTo(
        DoorRoot->GetRelativeRotation(),
        TargetRotation,
        DeltaTime,
        DoorOpenSpeed
    );

    DoorRoot->SetRelativeRotation(NewRotation);
}

FName ABHDoor::GetPersistenceID() const
{
    return PersistenceID;
}

bool ABHDoor::IsUnlocked() const
{
    return !bLocked;
}

void ABHDoor::RestoreUnlockedState(bool bShouldBeUnlocked)
{
    bLocked = !bShouldBeUnlocked;

    if (bLocked)
    {
        bIsOpen = false;
        TargetOpenRotation = ClosedRotation;
        DoorRoot->SetRelativeRotation(ClosedRotation);
    }
}

bool ABHDoor::BreachDoor(ABHCharacter* BreachingCharacter)
{
    if (!HasAuthority() || !bLocked)
    {
        return false;
    }
    bLocked = false;
    bBreached = true;
    bBreachChargePlanted = false;
    bIsOpen = true;
    TargetOpenRotation = ClosedRotation + FRotator(0.0f, OpenAngle, 0.0f);
    if (IsValid(BreachingCharacter))
    {
        BreachingCharacter->CompleteSharedObjective(
            BHObjectiveIds::UnlockSecurityDoor
        );
        BreachingCharacter->ShowPriorityStatusNotification(
            NSLOCTEXT("BrokenHorizon", "DoorBreachedNotification",
                "BREACH COMPLETE // ENTRY OPEN\n\nDanger-close blast affected both sides of the doorway."),
            EBHNotificationPriority::High
        );
    }
    ForceNetUpdate();
    UE_LOG(LogTemp, Display, TEXT("BH_DOOR_BREACH state=breached door=%s"), *GetName());
    return true;
}

void ABHDoor::SetBreachChargePlanted(bool bPlanted)
{
    if (HasAuthority())
    {
        bBreachChargePlanted = bPlanted && bLocked;
        ForceNetUpdate();
    }
}

bool ABHDoor::CanBeBreached() const
{
    return bLocked && !bBreachChargePlanted;
}
