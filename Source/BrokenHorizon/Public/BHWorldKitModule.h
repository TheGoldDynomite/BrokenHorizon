#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BHWorldKitModule.generated.h"

class UStaticMeshComponent;

UENUM(BlueprintType)
enum class EBHWorldKitModuleType : uint8
{
    Shelter,
    Checkpoint,
    Depot,
    ResistanceBase
};

UCLASS(BlueprintType, Blueprintable)
class BROKENHORIZON_API ABHWorldKitModule : public AActor
{
    GENERATED_BODY()

public:
    ABHWorldKitModule();

    virtual void BeginPlay() override;

    UFUNCTION(BlueprintPure, Category = "World Kit")
    FName GetPersistenceID() const { return PersistenceID; }

    UFUNCTION(BlueprintPure, Category = "World Kit")
    EBHWorldKitModuleType GetModuleType() const { return ModuleType; }

    UFUNCTION(BlueprintCallable, Category = "World Kit")
    void ConfigureModuleTypeForAuthoring(const FString& TypeName);

#if !UE_BUILD_SHIPPING
    void SetPersistenceIDForTesting(FName InPersistenceID);
    void SetModuleTypeForTesting(EBHWorldKitModuleType InModuleType);
#endif

protected:
    void ApplyVariantPresentation();

    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "World Kit")
    FName PersistenceID = NAME_None;

    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "World Kit")
    EBHWorldKitModuleType ModuleType = EBHWorldKitModuleType::Shelter;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "World Kit|Components")
    TObjectPtr<UStaticMeshComponent> FoundationMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "World Kit|Components")
    TObjectPtr<UStaticMeshComponent> WallLeftMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "World Kit|Components")
    TObjectPtr<UStaticMeshComponent> WallRightMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "World Kit|Components")
    TObjectPtr<UStaticMeshComponent> RoofMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "World Kit|Components")
    TObjectPtr<UStaticMeshComponent> DoorwayMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "World Kit|Components")
    TObjectPtr<UStaticMeshComponent> SignageMesh;
};
