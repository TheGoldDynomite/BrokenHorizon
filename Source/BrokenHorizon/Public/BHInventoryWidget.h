#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BHInventoryWidget.generated.h"

struct FBHInventorySnapshot;
class UTextBlock;
class UButton;
class ABHCharacter;

UCLASS(BlueprintType, Blueprintable)
class BROKENHORIZON_API UBHInventoryWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    void InitializeInventory(ABHCharacter* InCharacter);

    UFUNCTION(BlueprintCallable, Category = "Inventory")
    void SetInventorySnapshot(const FBHInventorySnapshot& Snapshot);

    UFUNCTION(BlueprintCallable, Category = "Inventory")
    void SetInventoryOpen(bool bOpen);

    UFUNCTION(BlueprintPure, Category = "Inventory")
    bool IsInventoryOpen() const;

protected:
    virtual void NativeConstruct() override;

private:
    void RefreshInventoryText();

    UFUNCTION()
    void HandleCycleRoleClicked();

    UFUNCTION()
    void HandleDropFragClicked();

    UFUNCTION()
    void HandleDropSmokeClicked();

    UFUNCTION()
    void HandleDropEngineeringClicked();

    UFUNCTION()
    void HandleDropAntiVehicleClicked();

    UFUNCTION()
    void HandleDropAmmoClicked();

    UFUNCTION()
    void HandleTransferFragClicked();

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> InventoryText;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> CycleRoleButton;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> DropFragButton;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> DropSmokeButton;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> DropEngineeringButton;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> DropAntiVehicleButton;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> DropAmmoButton;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> TransferFragButton;

    FString SnapshotText;
    bool bInventoryOpen = false;
    TWeakObjectPtr<ABHCharacter> OwningCharacter;
};
