#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BHExtractionZone.generated.h"

class ABHCharacter;
class UBoxComponent;
class USceneComponent;

UCLASS(Blueprintable)
class BROKENHORIZON_API ABHExtractionZone : public AActor
{
    GENERATED_BODY()

public:
    ABHExtractionZone();

    UFUNCTION(BlueprintImplementableEvent, Category = "Extraction")
    void OnExtractionUnavailable(ABHCharacter* PlayerCharacter);

    UFUNCTION(BlueprintImplementableEvent, Category = "Extraction")
    void OnExtractionAvailable(ABHCharacter* PlayerCharacter);

    UFUNCTION(BlueprintImplementableEvent, Category = "Extraction")
    void OnExtractionCompleted(ABHCharacter* PlayerCharacter);

protected:
    virtual void BeginPlay() override;

    UFUNCTION()
    void HandleExtractionOverlap(
        UPrimitiveComponent* OverlappedComponent,
        AActor* OtherActor,
        UPrimitiveComponent* OtherComponent,
        int32 OtherBodyIndex,
        bool bFromSweep,
        const FHitResult& SweepResult
    );

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Extraction")
    TObjectPtr<USceneComponent> ExtractionRoot;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Extraction")
    TObjectPtr<UBoxComponent> ExtractionBox;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Extraction")
    FName RequiredObjectiveId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Extraction")
    FName ExtractionObjectiveId = NAME_None;

private:
    bool bCompletionTriggered = false;
};
