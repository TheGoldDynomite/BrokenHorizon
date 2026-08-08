#pragma once

#include "CoreMinimal.h"
#include "BHInteractable.h"
#include "BHTacticalSupportZone.h"
#include "GameFramework/Actor.h"
#include "BHFieldSupportRelay.generated.h"

class ABHCharacter;
class UStaticMeshComponent;
class UTextRenderComponent;

UCLASS(BlueprintType, Blueprintable)
class BROKENHORIZON_API ABHFieldSupportRelay : public AActor, public IBHInteractable
{
    GENERATED_BODY()

public:
    ABHFieldSupportRelay();

    virtual void BeginPlay() override;

    virtual void Interact_Implementation(AActor* InteractingActor) override;
    virtual FText GetInteractionText_Implementation() const override;
    virtual void GetLifetimeReplicatedProps(
        TArray<FLifetimeProperty>& OutLifetimeProps
    ) const override;

    UFUNCTION(BlueprintCallable, Category = "Field Support")
    void ConfigureRelay(
        FName NewSectorID,
        EBHTacticalSupportType NewSupportType
    );

    UFUNCTION(BlueprintPure, Category = "Field Support")
    FName GetSectorID() const;

    UFUNCTION(BlueprintPure, Category = "Field Support")
    EBHTacticalSupportType GetSupportType() const;

    static float GetSupplyCost(EBHTacticalSupportType Type);
    static bool IsPingEligible(
        float ServerTimeSeconds,
        float PingExpiryServerTimeSeconds,
        float DistanceFromRelayCentimeters,
        float MaximumRangeCentimeters
    );

protected:
    UFUNCTION()
    void OnRep_SupportType();

private:
    void RefreshPresentation();
#if !UE_BUILD_SHIPPING
    void RunTacticalSupportRuntimeProbe();
#endif

    UPROPERTY(VisibleAnywhere, Category = "Field Support|Components")
    TObjectPtr<USceneComponent> SceneRoot;

    UPROPERTY(VisibleAnywhere, Category = "Field Support|Components")
    TObjectPtr<UStaticMeshComponent> RelayMesh;

    UPROPERTY(VisibleAnywhere, Category = "Field Support|Components")
    TObjectPtr<UTextRenderComponent> RelayLabel;

    UPROPERTY(EditInstanceOnly, Category = "Field Support|War")
    FName SectorID = NAME_None;

    UPROPERTY(EditInstanceOnly, ReplicatedUsing = OnRep_SupportType, Category = "Field Support")
    EBHTacticalSupportType SupportType = EBHTacticalSupportType::SmokeScreen;

    UPROPERTY(EditDefaultsOnly, Category = "Field Support|Targeting")
    float MaximumCallInRange = 500000.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Field Support|Cooldown")
    float RelayCooldownSeconds = 45.0f;

    UPROPERTY(Replicated)
    float CooldownEndsServerTime = 0.0f;
};
