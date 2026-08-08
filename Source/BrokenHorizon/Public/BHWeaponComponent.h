#pragma once

#include "CoreMinimal.h"
#include "BHRifle.h"
#include "Components/ActorComponent.h"
#include "TimerManager.h"
#include "BHWeaponComponent.generated.h"

class UCameraComponent;

UENUM(BlueprintType)
enum class EBHWeaponRole : uint8
{
    Assault,
    Marksman,
    Support
};

USTRUCT(BlueprintType)
struct BROKENHORIZON_API FBHWeaponRoleProfile
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    EBHWeaponRole Role = EBHWeaponRole::Assault;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    FText DisplayName;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    FText TacticalDescription;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    FBHRifleConfig RifleConfig;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    int32 MaximumReserveAmmo = 180;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    float HeatPerShot = 0.032f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    float HeatCoolingPerSecond = 0.18f;
};

UENUM(BlueprintType)
enum class EBHWeaponState : uint8
{
    Idle,
    Firing,
    Reloading
};

UENUM(BlueprintType)
enum class EBHReloadType : uint8
{
    Tactical,
    Emergency
};

UENUM(BlueprintType)
enum class EBHFireMode : uint8
{
    SemiAutomatic,
    Automatic
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
    FBHOnAmmoChanged,
    int32,
    MagazineAmmo,
    int32,
    ReserveAmmo
);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
    FBHOnWeaponStateChanged,
    EBHWeaponState,
    NewState
);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
    FBHOnWeaponHeatChanged,
    float,
    HeatNormalized,
    bool,
    bOverheated
);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
    FBHOnWeaponRoleChanged,
    EBHWeaponRole,
    NewRole
);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
    FBHOnFireModeChanged,
    EBHFireMode,
    NewFireMode
);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
    FBHOnAimChanged,
    bool,
    bIsAiming
);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
    FBHOnWeaponEquipped,
    ABHRifle*,
    EquippedRifle
);

UCLASS(ClassGroup = (Gameplay), meta = (BlueprintSpawnableComponent))
class BROKENHORIZON_API UBHWeaponComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UBHWeaponComponent();

    virtual void GetLifetimeReplicatedProps(
        TArray<FLifetimeProperty>& OutLifetimeProps
    ) const override;

    virtual void TickComponent(
        float DeltaTime,
        ELevelTick TickType,
        FActorComponentTickFunction* ThisTickFunction
    ) override;

    UFUNCTION(BlueprintCallable, Category = "Weapon")
    void StartFiring();

    UFUNCTION(BlueprintCallable, Category = "Weapon")
    void StopFiring();

    UFUNCTION(BlueprintCallable, Category = "Weapon")
    bool StartReload();

    UFUNCTION(BlueprintCallable, Category = "Weapon")
    bool StartEmergencyReload();

    UFUNCTION(BlueprintPure, Category = "Weapon")
    EBHReloadType GetReloadType() const;

    static float CalculateReloadDuration(
        float BaseDuration,
        EBHReloadType ReloadType
    );

    UFUNCTION(BlueprintPure, Category = "Weapon|Heat")
    float GetWeaponHeatNormalized() const;

    UFUNCTION(BlueprintPure, Category = "Weapon|Heat")
    bool IsWeaponOverheated() const;

    static float CalculateHeatAfterShot(float CurrentHeat, float HeatPerShot);
    static float CalculateHeatSpreadMultiplier(float HeatNormalized);

    UFUNCTION(BlueprintCallable, Category = "Weapon")
    EBHFireMode ToggleFireMode();

    UFUNCTION(BlueprintCallable, Category = "Weapon")
    void StartAiming();

    UFUNCTION(BlueprintCallable, Category = "Weapon")
    void StopAiming();

    void StopAllActions();

    UFUNCTION(BlueprintPure, Category = "Weapon")
    bool IsFiring() const;

    UFUNCTION(BlueprintPure, Category = "Weapon")
    bool IsReloading() const;

    UFUNCTION(BlueprintPure, Category = "Weapon")
    bool IsAiming() const;

    bool InterruptReload(FName InterruptReason = NAME_None);

    UFUNCTION(BlueprintPure, Category = "Weapon")
    EBHFireMode GetFireMode() const;

    UFUNCTION(BlueprintPure, Category = "Weapon")
    int32 GetMagazineAmmo() const;

    UFUNCTION(BlueprintPure, Category = "Weapon")
    int32 GetReserveAmmo() const;

    UFUNCTION(BlueprintPure, Category = "Weapon")
    int32 GetMaxReserveAmmo() const;

    UFUNCTION(BlueprintPure, Category = "Weapon")
    float GetCurrentSpreadBloomDegrees() const;

    UFUNCTION(BlueprintPure, Category = "Weapon|Role")
    EBHWeaponRole GetWeaponRole() const;

    UFUNCTION(BlueprintPure, Category = "Weapon|Role")
    FText GetWeaponRoleDisplayName() const;

    UFUNCTION(BlueprintCallable, Category = "Weapon|Role")
    bool EquipWeaponRole(
        EBHWeaponRole NewRole,
        bool bRefillAmmo = true
    );

    UFUNCTION(BlueprintCallable, Category = "Weapon|Role")
    EBHWeaponRole CycleWeaponRole(bool bRefillAmmo = true);

    static FBHWeaponRoleProfile BuildWeaponRoleProfile(
        EBHWeaponRole Role
    );

    static EBHWeaponRole GetNextWeaponRole(EBHWeaponRole Role);

    UFUNCTION(BlueprintCallable, Category = "Weapon")
    int32 AddReserveAmmo(int32 AmmoAmount);

    int32 RemoveReserveAmmo(int32 AmmoAmount);

    bool RestoreAmmoState(
        int32 SavedMagazineAmmo,
        int32 SavedReserveAmmo
    );

    bool RestoreWeaponHeatState(
        float SavedHeatNormalized,
        bool bSavedOverheated
    );

    bool RestoreFireModeState(EBHFireMode SavedFireMode);

    UFUNCTION(BlueprintPure, Category = "Weapon")
    ABHRifle* GetEquippedRifle() const;

    UPROPERTY(BlueprintAssignable, Category = "Weapon")
    FBHOnAmmoChanged OnAmmoChanged;

    UPROPERTY(BlueprintAssignable, Category = "Weapon")
    FBHOnWeaponStateChanged OnWeaponStateChanged;

    UPROPERTY(BlueprintAssignable, Category = "Weapon")
    FBHOnAimChanged OnAimChanged;

    UPROPERTY(BlueprintAssignable, Category = "Weapon")
    FBHOnWeaponEquipped OnWeaponEquipped;

    UPROPERTY(BlueprintAssignable, Category = "Weapon|Role")
    FBHOnWeaponRoleChanged OnWeaponRoleChanged;

    UPROPERTY(BlueprintAssignable, Category = "Weapon")
    FBHOnFireModeChanged OnFireModeChanged;

    UPROPERTY(BlueprintAssignable, Category = "Weapon|Heat")
    FBHOnWeaponHeatChanged OnWeaponHeatChanged;

protected:
    virtual void BeginPlay() override;

    virtual void EndPlay(
        const EEndPlayReason::Type EndPlayReason
    ) override;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
    TSubclassOf<ABHRifle> DefaultRifleClass;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Weapon|Presentation"
    )
    FName ThirdPersonWeaponSocketName = TEXT("hand_r");

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Weapon")
    TObjectPtr<ABHRifle> EquippedRifle;

    UPROPERTY(
        ReplicatedUsing = OnRep_Ammo,
        VisibleInstanceOnly,
        BlueprintReadOnly,
        Category = "Weapon"
    )
    int32 MagazineAmmo = 0;

    UPROPERTY(
        ReplicatedUsing = OnRep_Ammo,
        VisibleInstanceOnly,
        BlueprintReadOnly,
        Category = "Weapon"
    )
    int32 ReserveAmmo = 0;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Weapon|Ammo",
        meta = (ClampMin = "0")
    )
    int32 MaxReserveAmmo = 180;

    UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category = "Weapon|Reload")
    EBHReloadType ReloadType = EBHReloadType::Tactical;

    UPROPERTY(ReplicatedUsing = OnRep_WeaponHeat, VisibleInstanceOnly, BlueprintReadOnly, Category = "Weapon|Heat")
    float WeaponHeatNormalized = 0.0f;

    UPROPERTY(ReplicatedUsing = OnRep_WeaponHeat, VisibleInstanceOnly, BlueprintReadOnly, Category = "Weapon|Heat")
    bool bWeaponOverheated = false;

    UPROPERTY(
        ReplicatedUsing = OnRep_WeaponState,
        VisibleInstanceOnly,
        BlueprintReadOnly,
        Category = "Weapon"
    )
    EBHWeaponState WeaponState = EBHWeaponState::Idle;

    UPROPERTY(
        ReplicatedUsing = OnRep_FireMode,
        VisibleInstanceOnly,
        BlueprintReadOnly,
        Category = "Weapon"
    )
    EBHFireMode FireMode = EBHFireMode::SemiAutomatic;

    UPROPERTY(
        ReplicatedUsing = OnRep_IsAiming,
        VisibleInstanceOnly,
        BlueprintReadOnly,
        Category = "Weapon"
    )
    bool bIsAiming = false;

    UPROPERTY(
        ReplicatedUsing = OnRep_WeaponRole,
        VisibleInstanceOnly,
        BlueprintReadOnly,
        Category = "Weapon|Role"
    )
    EBHWeaponRole WeaponRole = EBHWeaponRole::Assault;

private:
    UFUNCTION(Server, Reliable)
    void ServerStartFiring();

    UFUNCTION(Server, Reliable)
    void ServerStopFiring();

    UFUNCTION(Server, Reliable)
    void ServerStartReload();

    UFUNCTION(Server, Reliable)
    void ServerStartEmergencyReload();

    UFUNCTION(Server, Reliable)
    void ServerToggleFireMode();

    UFUNCTION(Server, Reliable)
    void ServerSetAiming(bool bNewIsAiming);

    UFUNCTION(Server, Reliable)
    void ServerStopAllActions();

    UFUNCTION(Server, Reliable)
    void ServerEquipWeaponRole(
        EBHWeaponRole NewRole,
        bool bRefillAmmo
    );

    UFUNCTION(NetMulticast, Unreliable)
    void MulticastFirePresentation(
        const FHitResult& ShotHit,
        bool bHadBlockingHit
    );

    UFUNCTION(Client, Unreliable)
    void ClientDryFirePresentation();

    UFUNCTION()
    void OnRep_Ammo();

    UFUNCTION()
    void OnRep_WeaponState();

    UFUNCTION()
    void OnRep_IsAiming();

    UFUNCTION()
    void OnRep_WeaponRole();

    UFUNCTION()
    void OnRep_FireMode();

    UFUNCTION()
    void OnRep_WeaponHeat();

    bool bBattlefieldLootAmmoReplicationLogged = false;

    bool HasWeaponAuthority() const;

    bool CanOperateWeapon() const;

    void EquipDefaultRifle();

    void ApplyWeaponRoleProfile(bool bRefillAmmo);

    void AttachEquippedRifle();

    void TryFire();

    void TryPredictedFire();

    void FinishReload();
    bool StartReloadInternal(EBHReloadType RequestedType);

    void ApplyAimingState(bool bNewIsAiming);

    void ApplyRecoil();

    void UpdateWeaponRecovery(float DeltaTime);
    void AddShotHeat(const FBHWeaponRoleProfile& Profile);

    void SetWeaponState(EBHWeaponState NewState);

    float GetFireInterval() const;

    TObjectPtr<UCameraComponent> AimCamera;
    float BaseFieldOfView = 90.0f;
    float LastShotTime = -BIG_NUMBER;
    float LastPredictedShotTime = -BIG_NUMBER;
    float CurrentSpreadBloomDegrees = 0.0f;
    FVector2D PendingRecoilOffset = FVector2D::ZeroVector;
    bool bWantsToFire = false;
    bool bWantsToAim = false;
    FBHRifleConfig AssaultBaselineConfig;
    int32 AssaultBaselineMaximumReserveAmmo = 180;
    bool bHasAssaultBaselineConfig = false;

    FTimerHandle RefireTimerHandle;
    FTimerHandle ReloadTimerHandle;
};
