#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BHAmmoHUDWidget.generated.h"

class UTextBlock;

UCLASS()
class BROKENHORIZON_API UBHAmmoHUDWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "Weapon")
    void SetAmmo(int32 MagazineAmmo, int32 ReserveAmmo);

    UFUNCTION(BlueprintCallable, Category = "Weapon")
    void SetWeaponRole(const FText& WeaponRoleName);

    UFUNCTION(BlueprintCallable, Category = "Weapon")
    void SetWeaponHeat(float HeatNormalized, bool bOverheated);

    UFUNCTION(BlueprintCallable, Category = "Weapon")
    void SetFireMode(const FText& FireModeName);

    UFUNCTION(BlueprintCallable, Category = "Weapon")
    void SetWeaponBraced(bool bBraced);

protected:
    virtual void NativeConstruct() override;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> AmmoText;

private:
    void ApplySafeAreaLayout();
    void RefreshAmmoText();

    int32 CachedMagazineAmmo = 0;
    int32 CachedReserveAmmo = 0;
    FText CachedWeaponRoleName;
    FText CachedFireModeName;
    float CachedWeaponHeat = 0.0f;
    bool bCachedWeaponOverheated = false;
    bool bCachedWeaponBraced = false;
};
