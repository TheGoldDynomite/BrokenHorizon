#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BHTacticalSupportZone.generated.h"

class ABHCharacter;
class UPointLightComponent;
class USceneComponent;
class UStaticMeshComponent;
class UTextRenderComponent;

UENUM(BlueprintType)
enum class EBHTacticalSupportType : uint8
{
    SmokeScreen UMETA(DisplayName = "Smoke Screen"),
    MortarBarrage UMETA(DisplayName = "Mortar Barrage")
};

UCLASS(BlueprintType, Blueprintable)
class BROKENHORIZON_API ABHTacticalSupportZone : public AActor
{
    GENERATED_BODY()

public:
    ABHTacticalSupportZone();

    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void GetLifetimeReplicatedProps(
        TArray<FLifetimeProperty>& OutLifetimeProps
    ) const override;

    void InitializeSupport(
        EBHTacticalSupportType NewSupportType,
        ABHCharacter* NewRequestingCharacter
    );

    UFUNCTION(BlueprintPure, Category = "Tactical Support")
    EBHTacticalSupportType GetSupportType() const;

    UFUNCTION(BlueprintPure, Category = "Tactical Support")
    int32 GetFiredMortarShells() const;

    static FText GetSupportDisplayName(EBHTacticalSupportType Type);
    static float GetSupportRadius(EBHTacticalSupportType Type);
    static int32 GetMortarShellCount();
    static bool ShouldAffectSoldier(
        EBHTacticalSupportType Type,
        bool bSoldierHostileToRequester,
        float DistanceCentimeters
    );
    static bool IsLineObscuredBySmoke(
        const UWorld* World,
        const FVector& SightOrigin,
        const FVector& TargetLocation
    );

protected:
    UFUNCTION()
    void OnRep_SupportType();

    UFUNCTION(NetMulticast, Unreliable)
    void MulticastMortarImpact(FVector_NetQuantize ImpactLocation);

private:
    void RefreshPresentation();
    void FireNextMortarShell();
    FVector ResolveShellImpactLocation(int32 ShellIndex) const;

    UPROPERTY(VisibleAnywhere, Category = "Tactical Support|Components")
    TObjectPtr<USceneComponent> SceneRoot;

    UPROPERTY(VisibleAnywhere, Category = "Tactical Support|Components")
    TObjectPtr<UStaticMeshComponent> MarkerMesh;

    UPROPERTY(VisibleAnywhere, Category = "Tactical Support|Components")
    TObjectPtr<UTextRenderComponent> SupportLabel;

    UPROPERTY(VisibleAnywhere, Category = "Tactical Support|Components")
    TObjectPtr<UPointLightComponent> SupportLight;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UStaticMeshComponent>> SmokePuffs;


    UPROPERTY(ReplicatedUsing = OnRep_SupportType)
    EBHTacticalSupportType SupportType = EBHTacticalSupportType::SmokeScreen;

    UPROPERTY()
    TObjectPtr<ABHCharacter> RequestingCharacter;

    UPROPERTY(EditDefaultsOnly, Category = "Tactical Support|Smoke")
    float SmokeDuration = 25.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Tactical Support|Mortar")
    float MortarWarningDelay = 2.5f;

    UPROPERTY(EditDefaultsOnly, Category = "Tactical Support|Mortar")
    float MortarShellInterval = 0.8f;

    UPROPERTY(EditDefaultsOnly, Category = "Tactical Support|Mortar")
    float MortarMaximumDamage = 70.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Tactical Support|Mortar")
    float MortarMinimumDamage = 15.0f;

    FTimerHandle SupportTimer;
    int32 FiredMortarShells = 0;
};
