#include "BHDoor.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "BHCharacter.h"

ABHDoor::ABHDoor()
{
    PrimaryActorTick.bCanEverTick = true;

    DoorRoot = CreateDefaultSubobject<USceneComponent>(TEXT("DoorRoot"));
    SetRootComponent(DoorRoot);

    DoorMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DoorMesh"));
    DoorMesh->SetupAttachment(DoorRoot);
}

void ABHDoor::Interact_Implementation(AActor* InteractingActor)
{
    ABHCharacter* Character = Cast<ABHCharacter>(InteractingActor);

    if (!IsValid(Character))
    {
        return;
    }

    if (bLocked)
    {
        if (!Character->HasKeycard(RequiredKeycard))
        {
            UE_LOG(LogTemp, Warning, TEXT("Access Denied"));
            return;
        }

        bLocked = false;

        Character->SetObjective(
            FText::FromString(TEXT("Explore Beyond the Security Door"))
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