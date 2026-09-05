#include "BHExtractionZone.h"

#include "BHCharacter.h"
#include "BHMissionData.h"
#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"

ABHExtractionZone::ABHExtractionZone()
{
    PrimaryActorTick.bCanEverTick = false;

    ExtractionRoot = CreateDefaultSubobject<USceneComponent>(
        TEXT("ExtractionRoot")
    );
    SetRootComponent(ExtractionRoot);

    ExtractionBox = CreateDefaultSubobject<UBoxComponent>(
        TEXT("ExtractionBox")
    );
    ExtractionBox->SetupAttachment(ExtractionRoot);
    ExtractionBox->SetBoxExtent(FVector(200.0f, 200.0f, 100.0f));
    ExtractionBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    ExtractionBox->SetCollisionResponseToAllChannels(ECR_Ignore);
    ExtractionBox->SetCollisionResponseToChannel(
        ECC_Pawn,
        ECR_Overlap
    );
    ExtractionBox->SetGenerateOverlapEvents(true);
    ExtractionBox->SetHiddenInGame(true);

    RequiredObjectiveId = BHObjectiveIds::EliminateGuard;
    ExtractionObjectiveId = BHObjectiveIds::ReachExtraction;
}

void ABHExtractionZone::BeginPlay()
{
    Super::BeginPlay();

    // Extraction changes shared mission state and must only be resolved by the
    // authority. Clients receive the resulting objective/debrief state through
    // the existing replicated character path.
    if (!HasAuthority())
    {
        return;
    }

    ExtractionBox->OnComponentBeginOverlap.AddDynamic(
        this,
        &ABHExtractionZone::HandleExtractionOverlap
    );
}

bool ABHExtractionZone::CanCompleteExtraction(
    FName RequiredObjectiveID,
    FName ExtractionObjectiveID,
    FName CurrentObjectiveID,
    const TArray<FName>& CompletedObjectiveIDs
)
{
    return !ExtractionObjectiveID.IsNone() &&
        CurrentObjectiveID == ExtractionObjectiveID &&
        (
            RequiredObjectiveID.IsNone() ||
            CompletedObjectiveIDs.Contains(RequiredObjectiveID)
        );
}

void ABHExtractionZone::HandleExtractionOverlap(
    UPrimitiveComponent* OverlappedComponent,
    AActor* OtherActor,
    UPrimitiveComponent* OtherComponent,
    int32 OtherBodyIndex,
    bool bFromSweep,
    const FHitResult& SweepResult
)
{
    if (!HasAuthority() || bCompletionTriggered)
    {
        return;
    }

    ABHCharacter* Character = Cast<ABHCharacter>(OtherActor);

    if (!IsValid(Character) || !Character->IsPlayerControlled())
    {
        return;
    }

    if (!CanCompleteExtraction(
            RequiredObjectiveId,
            ExtractionObjectiveId,
            Character->GetCurrentObjectiveID(),
            Character->GetCompletedObjectiveIDs()
        ))
    {
        OnExtractionUnavailable(Character);
        return;
    }

    OnExtractionAvailable(Character);

    if (!Character->CompleteSharedObjective(ExtractionObjectiveId) ||
        !Character->IsMissionComplete())
    {
        UE_LOG(
            LogTemp,
            Error,
            TEXT(
                "Extraction zone %s could not complete mission. "
                "Ensure %s is the final mission objective."
            ),
            *GetPathName(),
            *ExtractionObjectiveId.ToString()
        );
        OnExtractionUnavailable(Character);
        return;
    }

    bCompletionTriggered = true;
    ExtractionBox->SetGenerateOverlapEvents(false);
    OnExtractionCompleted(Character);
}
