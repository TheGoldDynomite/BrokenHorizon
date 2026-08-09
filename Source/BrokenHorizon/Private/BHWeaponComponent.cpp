#include "BHWeaponComponent.h"

#include "BHCharacter.h"
#include "BHRifle.h"
#include "BHHealthComponent.h"
#include "BHPlaytestTelemetrySubsystem.h"
#include "BHUserSettingsSubsystem.h"
#include "Camera/CameraComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Net/UnrealNetwork.h"

UBHWeaponComponent::UBHWeaponComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    SetIsReplicatedByDefault(true);
}

FBHWeaponRoleProfile UBHWeaponComponent::BuildWeaponRoleProfile(
    EBHWeaponRole Role
)
{
    FBHWeaponRoleProfile Profile;
    Profile.Role = Role;

    switch (Role)
    {
    case EBHWeaponRole::Marksman:
        Profile.DisplayName = NSLOCTEXT(
            "BrokenHorizon",
            "WeaponRoleMarksman",
            "MARKSMAN"
        );
        Profile.TacticalDescription = NSLOCTEXT(
            "BrokenHorizon",
            "WeaponRoleMarksmanDescription",
            "Precision fire // high damage // low capacity"
        );
        Profile.RifleConfig.MagazineSize = 15;
        Profile.RifleConfig.StartingReserveAmmo = 60;
        Profile.RifleConfig.ReloadDuration = 2.0f;
        Profile.RifleConfig.RoundsPerMinute = 360.0f;
        Profile.RifleConfig.bAutomatic = false;
        Profile.RifleConfig.Damage = 42.0f;
        Profile.RifleConfig.Range = 80000.0f;
        Profile.RifleConfig.MuzzleVelocity = 90000.0f;
        Profile.RifleConfig.DamageFalloffStartDistance = 18000.0f;
        Profile.RifleConfig.DamageFalloffEndDistance = 65000.0f;
        Profile.RifleConfig.MinimumDamageRetention = 0.62f;
        Profile.RifleConfig.HipSpreadDegrees = 1.15f;
        Profile.RifleConfig.ADSSpreadDegrees = 0.025f;
        Profile.RifleConfig.SpreadPerShotDegrees = 0.16f;
        Profile.RifleConfig.MaxSpreadBloomDegrees = 0.9f;
        Profile.RifleConfig.SpreadRecoveryDelay = 0.24f;
        Profile.RifleConfig.SpreadRecoverySpeed = 2.0f;
        Profile.RifleConfig.RecoilPitch = 0.9f;
        Profile.RifleConfig.RecoilYaw = 0.18f;
        Profile.RifleConfig.ADSRecoilMultiplier = 0.62f;
        Profile.RifleConfig.ADSFieldOfView = 48.0f;
        Profile.RifleConfig.FOVInterpSpeed = 10.0f;
        Profile.RifleConfig.NoiseLoudness = 1.15f;
        Profile.RifleConfig.NoiseRange = 4500.0f;
        Profile.RifleConfig.SuppressionRadius = 300.0f;
        Profile.RifleConfig.SuppressionAmount = 0.42f;
        Profile.MaximumReserveAmmo = 90;
        Profile.HeatPerShot = 0.080f;
        Profile.HeatCoolingPerSecond = 0.22f;
        break;

    case EBHWeaponRole::Support:
        Profile.DisplayName = NSLOCTEXT(
            "BrokenHorizon",
            "WeaponRoleSupport",
            "SUPPORT"
        );
        Profile.TacticalDescription = NSLOCTEXT(
            "BrokenHorizon",
            "WeaponRoleSupportDescription",
            "Sustained fire // strong suppression // slow reload"
        );
        Profile.RifleConfig.MagazineSize = 60;
        Profile.RifleConfig.StartingReserveAmmo = 180;
        Profile.RifleConfig.ReloadDuration = 3.1f;
        Profile.RifleConfig.RoundsPerMinute = 720.0f;
        Profile.RifleConfig.bAutomatic = true;
        Profile.RifleConfig.Damage = 21.0f;
        Profile.RifleConfig.Range = 45000.0f;
        Profile.RifleConfig.MuzzleVelocity = 76000.0f;
        Profile.RifleConfig.DamageFalloffStartDistance = 8000.0f;
        Profile.RifleConfig.DamageFalloffEndDistance = 35000.0f;
        Profile.RifleConfig.MinimumDamageRetention = 0.50f;
        Profile.RifleConfig.HipSpreadDegrees = 1.25f;
        Profile.RifleConfig.ADSSpreadDegrees = 0.14f;
        Profile.RifleConfig.SpreadPerShotDegrees = 0.14f;
        Profile.RifleConfig.MaxSpreadBloomDegrees = 1.7f;
        Profile.RifleConfig.SpreadRecoveryDelay = 0.28f;
        Profile.RifleConfig.SpreadRecoverySpeed = 1.8f;
        Profile.RifleConfig.RecoilPitch = 0.48f;
        Profile.RifleConfig.RecoilYaw = 0.15f;
        Profile.RifleConfig.ADSRecoilMultiplier = 0.78f;
        Profile.RifleConfig.ADSFieldOfView = 68.0f;
        Profile.RifleConfig.FOVInterpSpeed = 8.0f;
        Profile.RifleConfig.NoiseLoudness = 1.2f;
        Profile.RifleConfig.NoiseRange = 5000.0f;
        Profile.RifleConfig.SuppressionRadius = 450.0f;
        Profile.RifleConfig.SuppressionAmount = 0.55f;
        Profile.RifleConfig.SuppressionMinimumIntensity = 0.15f;
        Profile.MaximumReserveAmmo = 240;
        Profile.HeatPerShot = 0.018f;
        Profile.HeatCoolingPerSecond = 0.14f;
        break;

    case EBHWeaponRole::Assault:
    default:
        Profile.Role = EBHWeaponRole::Assault;
        Profile.DisplayName = NSLOCTEXT(
            "BrokenHorizon",
            "WeaponRoleAssault",
            "ASSAULT"
        );
        Profile.TacticalDescription = NSLOCTEXT(
            "BrokenHorizon",
            "WeaponRoleAssaultDescription",
            "Balanced handling // selectable fire // mobile"
        );
        Profile.MaximumReserveAmmo = 180;
        break;
    }

    return Profile;
}

EBHWeaponRole UBHWeaponComponent::GetNextWeaponRole(
    EBHWeaponRole Role
)
{
    switch (Role)
    {
    case EBHWeaponRole::Assault:
        return EBHWeaponRole::Marksman;
    case EBHWeaponRole::Marksman:
        return EBHWeaponRole::Support;
    case EBHWeaponRole::Support:
    default:
        return EBHWeaponRole::Assault;
    }
}

void UBHWeaponComponent::GetLifetimeReplicatedProps(
    TArray<FLifetimeProperty>& OutLifetimeProps
) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME_CONDITION_NOTIFY(
        UBHWeaponComponent,
        MagazineAmmo,
        COND_OwnerOnly,
        REPNOTIFY_Always
    );
    DOREPLIFETIME_CONDITION_NOTIFY(
        UBHWeaponComponent,
        ReserveAmmo,
        COND_OwnerOnly,
        REPNOTIFY_Always
    );
    DOREPLIFETIME_CONDITION_NOTIFY(
        UBHWeaponComponent,
        WeaponHeatNormalized,
        COND_OwnerOnly,
        REPNOTIFY_Always
    );
    DOREPLIFETIME_CONDITION_NOTIFY(
        UBHWeaponComponent,
        bWeaponOverheated,
        COND_OwnerOnly,
        REPNOTIFY_Always
    );
    DOREPLIFETIME(UBHWeaponComponent, ReloadType);
    DOREPLIFETIME_CONDITION_NOTIFY(
        UBHWeaponComponent,
        WeaponState,
        COND_None,
        REPNOTIFY_Always
    );
    DOREPLIFETIME_CONDITION(
        UBHWeaponComponent,
        FireMode,
        COND_OwnerOnly
    );
    DOREPLIFETIME_CONDITION_NOTIFY(
        UBHWeaponComponent,
        bIsAiming,
        COND_None,
        REPNOTIFY_Always
    );
    DOREPLIFETIME_CONDITION_NOTIFY(
        UBHWeaponComponent,
        WeaponRole,
        COND_None,
        REPNOTIFY_Always
    );
}

void UBHWeaponComponent::BeginPlay()
{
    Super::BeginPlay();

    AimCamera = GetOwner()
        ? GetOwner()->FindComponentByClass<UCameraComponent>()
        : nullptr;

    if (IsValid(AimCamera))
    {
        BaseFieldOfView = AimCamera->FieldOfView;
    }

    EquipDefaultRifle();

#if !UE_BUILD_SHIPPING
    if (HasWeaponAuthority() &&
        FParse::Param(
            FCommandLine::Get(),
            TEXT("BHTestWeaponRoleRuntime")))
    {
        const bool bEquipped = EquipWeaponRole(
            EBHWeaponRole::Marksman,
            true
        );
        const FBHRifleConfig* ActiveConfig = IsValid(EquippedRifle)
            ? &EquippedRifle->GetConfig()
            : nullptr;
        const bool bPassed = bEquipped &&
            WeaponRole == EBHWeaponRole::Marksman &&
            ActiveConfig &&
            ActiveConfig->MagazineSize == 15 &&
            !ActiveConfig->bAutomatic &&
            MagazineAmmo == 15 &&
            ReserveAmmo == 90;
        if (bPassed)
        {
            UE_LOG(
                LogTemp,
                Display,
                TEXT(
                    "BH_WEAPON_ROLE_RUNTIME result=success role=%d "
                    "magazine=%d reserve=%d automatic=%d"
                ),
                static_cast<int32>(WeaponRole),
                MagazineAmmo,
                ReserveAmmo,
                ActiveConfig && ActiveConfig->bAutomatic ? 1 : 0
            );
        }
        else
        {
            UE_LOG(
                LogTemp,
                Error,
                TEXT(
                    "BH_WEAPON_ROLE_RUNTIME result=failure role=%d "
                    "magazine=%d reserve=%d automatic=%d"
                ),
                static_cast<int32>(WeaponRole),
                MagazineAmmo,
                ReserveAmmo,
                ActiveConfig && ActiveConfig->bAutomatic ? 1 : 0
            );
        }
    }
#endif
}

void UBHWeaponComponent::EndPlay(
    const EEndPlayReason::Type EndPlayReason
)
{
    StopAllActions();

    if (IsValid(EquippedRifle))
    {
        EquippedRifle->Destroy();
        EquippedRifle = nullptr;
    }

    Super::EndPlay(EndPlayReason);
}

void UBHWeaponComponent::TickComponent(
    float DeltaTime,
    ELevelTick TickType,
    FActorComponentTickFunction* ThisTickFunction
)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    UpdateWeaponRecovery(DeltaTime);

    if (!IsValid(AimCamera))
    {
        return;
    }

    float TargetFieldOfView = BaseFieldOfView;
    float InterpSpeed = 12.0f;

    if (IsValid(EquippedRifle))
    {
        const FBHRifleConfig& Config = EquippedRifle->GetConfig();
        InterpSpeed = Config.FOVInterpSpeed;

        if (bIsAiming)
        {
            TargetFieldOfView = Config.ADSFieldOfView;
        }
    }

    AimCamera->SetFieldOfView(
        FMath::FInterpTo(
            AimCamera->FieldOfView,
            TargetFieldOfView,
            DeltaTime,
            InterpSpeed
        )
    );
}

void UBHWeaponComponent::StartFiring()
{
    if (!HasWeaponAuthority())
    {
        if (!CanOperateWeapon() || bWeaponOverheated)
        {
            return;
        }

        if (WeaponState == EBHWeaponState::Reloading)
        {
            InterruptReload(FName(TEXT("fire_input")));
        }

        if (MagazineAmmo <= 0)
        {
            EquippedRifle->PlayDryFirePresentation();
            return;
        }

        bWantsToFire = true;
        SetWeaponState(EBHWeaponState::Firing);
        TryPredictedFire();
        ServerStartFiring();
        return;
    }

    if (!CanOperateWeapon() || bWeaponOverheated)
    {
        return;
    }

    if (WeaponState == EBHWeaponState::Reloading)
    {
        InterruptReload(FName(TEXT("fire_input")));
    }

    bWantsToFire = true;
    SetWeaponState(EBHWeaponState::Firing);
    TryFire();
}

void UBHWeaponComponent::StopFiring()
{
    if (!HasWeaponAuthority())
    {
        bWantsToFire = false;

        if (UWorld* World = GetWorld())
        {
            World->GetTimerManager().ClearTimer(
                RefireTimerHandle
            );
        }

        if (WeaponState == EBHWeaponState::Firing)
        {
            SetWeaponState(EBHWeaponState::Idle);
        }

        ServerStopFiring();
        return;
    }

    bWantsToFire = false;

    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(
            RefireTimerHandle
        );
    }

    if (WeaponState == EBHWeaponState::Firing)
    {
        SetWeaponState(EBHWeaponState::Idle);
    }
}

bool UBHWeaponComponent::StartReload()
{
    const EBHReloadType RequestedType = MagazineAmmo <= 0
        ? EBHReloadType::Emergency
        : EBHReloadType::Tactical;
    return StartReloadInternal(RequestedType);
}

bool UBHWeaponComponent::StartEmergencyReload()
{
    return StartReloadInternal(EBHReloadType::Emergency);
}

EBHReloadType UBHWeaponComponent::GetReloadType() const
{
    return ReloadType;
}

float UBHWeaponComponent::CalculateReloadDuration(
    float BaseDuration,
    EBHReloadType RequestedType
)
{
    return FMath::Max(0.0f, BaseDuration) *
        (RequestedType == EBHReloadType::Emergency ? 0.65f : 1.0f);
}

bool UBHWeaponComponent::StartReloadInternal(
    EBHReloadType RequestedType
)
{
    if (!HasWeaponAuthority())
    {
        if (!CanOperateWeapon() ||
            !IsValid(EquippedRifle) ||
            MagazineAmmo >=
                EquippedRifle->GetConfig().MagazineSize ||
            ReserveAmmo <= 0 ||
            (WeaponState == EBHWeaponState::Reloading &&
                RequestedType != EBHReloadType::Emergency))
        {
            return false;
        }

        StopFiring();
        ApplyAimingState(false);
        if (RequestedType == EBHReloadType::Emergency)
        {
            ServerStartEmergencyReload();
        }
        else
        {
            ServerStartReload();
        }
        return true;
    }

    if (!CanOperateWeapon() ||
        !IsValid(EquippedRifle))
    {
        return false;
    }

    const FBHRifleConfig& Config = EquippedRifle->GetConfig();

    if (MagazineAmmo >= Config.MagazineSize ||
        ReserveAmmo <= 0)
    {
        return false;
    }

    StopFiring();
    ApplyAimingState(false);
    if (WeaponState == EBHWeaponState::Reloading)
    {
        if (RequestedType != EBHReloadType::Emergency ||
            ReloadType == EBHReloadType::Emergency)
        {
            return false;
        }
        GetWorld()->GetTimerManager().ClearTimer(ReloadTimerHandle);
    }

    ReloadType = RequestedType;
    const int32 DiscardedRounds = ReloadType == EBHReloadType::Emergency
        ? MagazineAmmo : 0;
    if (DiscardedRounds > 0)
    {
        MagazineAmmo = 0;
        OnAmmoChanged.Broadcast(MagazineAmmo, ReserveAmmo);
    }
    SetWeaponState(EBHWeaponState::Reloading);
    const float ReloadDuration = CalculateReloadDuration(
        Config.ReloadDuration,
        ReloadType
    );
    if (GetNetMode() != NM_DedicatedServer)
    {
        EquippedRifle->PlayReloadPresentation(ReloadDuration);
    }

    UE_LOG(LogTemp, Display, TEXT(
        "BH_RELOAD_STARTED owner=%s type=%s discarded=%d duration=%.2f "
        "magazine=%d reserve=%d"),
        *GetNameSafe(GetOwner()),
        ReloadType == EBHReloadType::Emergency
            ? TEXT("emergency") : TEXT("tactical"),
        DiscardedRounds,
        ReloadDuration,
        MagazineAmmo,
        ReserveAmmo);

    if (ReloadDuration <= 0.0f)
    {
        FinishReload();
        return true;
    }

    GetWorld()->GetTimerManager().SetTimer(
        ReloadTimerHandle,
        this,
        &UBHWeaponComponent::FinishReload,
        ReloadDuration,
        false
    );
    return true;
}

EBHFireMode UBHWeaponComponent::ToggleFireMode()
{
    if (!HasWeaponAuthority())
    {
        if (!CanOperateWeapon() ||
            WeaponState == EBHWeaponState::Reloading ||
            !IsValid(EquippedRifle))
        {
            return FireMode;
        }

        StopFiring();

        if (EquippedRifle->GetConfig().bAutomatic)
        {
            FireMode = FireMode == EBHFireMode::Automatic
                ? EBHFireMode::SemiAutomatic
                : EBHFireMode::Automatic;
        }
        else
        {
            FireMode = EBHFireMode::SemiAutomatic;
        }

        OnFireModeChanged.Broadcast(FireMode);
        ServerToggleFireMode();
        return FireMode;
    }

    if (!CanOperateWeapon() ||
        WeaponState == EBHWeaponState::Reloading ||
        !IsValid(EquippedRifle))
    {
        return FireMode;
    }

    StopFiring();

    if (!EquippedRifle->GetConfig().bAutomatic)
    {
        FireMode = EBHFireMode::SemiAutomatic;
        OnFireModeChanged.Broadcast(FireMode);
        return FireMode;
    }

    FireMode = FireMode == EBHFireMode::Automatic
        ? EBHFireMode::SemiAutomatic
        : EBHFireMode::Automatic;
    OnFireModeChanged.Broadcast(FireMode);
    return FireMode;
}

void UBHWeaponComponent::StartAiming()
{
    if (!CanOperateWeapon())
    {
        return;
    }

    bWantsToAim = true;

    if (!HasWeaponAuthority())
    {
        if (WeaponState != EBHWeaponState::Reloading)
        {
            ApplyAimingState(true);
        }

        ServerSetAiming(true);
        return;
    }

    if (WeaponState != EBHWeaponState::Reloading)
    {
        ApplyAimingState(true);
    }
}

void UBHWeaponComponent::StopAiming()
{
    bWantsToAim = false;

    if (!HasWeaponAuthority())
    {
        ApplyAimingState(false);
        ServerSetAiming(false);
        return;
    }

    ApplyAimingState(false);
}

void UBHWeaponComponent::ApplyAimingState(bool bNewIsAiming)
{
    if (bIsAiming == bNewIsAiming)
    {
        return;
    }

    bIsAiming = bNewIsAiming;

    if (IsValid(EquippedRifle))
    {
        EquippedRifle->SetAimPresentation(bIsAiming);
    }

    OnAimChanged.Broadcast(bIsAiming);
}

void UBHWeaponComponent::StopAllActions()
{
    if (!HasWeaponAuthority())
    {
        ServerStopAllActions();
    }

    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(
            RefireTimerHandle
        );
        World->GetTimerManager().ClearTimer(
            ReloadTimerHandle
        );
    }

    bWantsToFire = false;
    bWantsToAim = false;
    ApplyAimingState(false);
    SetWeaponState(EBHWeaponState::Idle);
}

bool UBHWeaponComponent::IsFiring() const
{
    return WeaponState == EBHWeaponState::Firing;
}

bool UBHWeaponComponent::IsReloading() const
{
    return WeaponState == EBHWeaponState::Reloading;
}

float UBHWeaponComponent::GetWeaponHeatNormalized() const
{
    return FMath::Clamp(WeaponHeatNormalized, 0.0f, 1.0f);
}

bool UBHWeaponComponent::IsWeaponOverheated() const
{
    return bWeaponOverheated;
}

float UBHWeaponComponent::CalculateHeatAfterShot(
    float CurrentHeat,
    float HeatPerShot
)
{
    return FMath::Clamp(
        FMath::Max(0.0f, CurrentHeat) + FMath::Max(0.0f, HeatPerShot),
        0.0f,
        1.0f
    );
}

float UBHWeaponComponent::CalculateHeatSpreadMultiplier(float HeatNormalized)
{
    return FMath::Lerp(
        1.0f,
        1.65f,
        FMath::GetMappedRangeValueClamped(
            FVector2D(0.40f, 1.0f),
            FVector2D(0.0f, 1.0f),
            HeatNormalized
        )
    );
}

bool UBHWeaponComponent::IsAiming() const
{
    return bIsAiming;
}

bool UBHWeaponComponent::InterruptReload(FName InterruptReason)
{
    if (WeaponState != EBHWeaponState::Reloading)
    {
        return false;
    }

    if (IsValid(EquippedRifle))
    {
        EquippedRifle->CancelReloadPresentation();
    }
    StopAllActions();
    const FString Reason = InterruptReason.IsNone()
        ? TEXT("external")
        : InterruptReason.ToString();
    UE_LOG(
        LogTemp,
        Display,
        TEXT("BH_RELOAD_INTERRUPTED owner=%s reason=%s"),
        *GetNameSafe(GetOwner()),
        *Reason
    );
    return true;
}

EBHFireMode UBHWeaponComponent::GetFireMode() const
{
    return FireMode;
}

int32 UBHWeaponComponent::GetMagazineAmmo() const
{
    return MagazineAmmo;
}

int32 UBHWeaponComponent::GetReserveAmmo() const
{
    return ReserveAmmo;
}

int32 UBHWeaponComponent::GetMaxReserveAmmo() const
{
    return MaxReserveAmmo;
}

float UBHWeaponComponent::GetCurrentSpreadBloomDegrees() const
{
    return CurrentSpreadBloomDegrees;
}

EBHWeaponRole UBHWeaponComponent::GetWeaponRole() const
{
    return WeaponRole;
}

FText UBHWeaponComponent::GetWeaponRoleDisplayName() const
{
    return BuildWeaponRoleProfile(WeaponRole).DisplayName;
}

bool UBHWeaponComponent::EquipWeaponRole(
    EBHWeaponRole NewRole,
    bool bRefillAmmo
)
{
    if (!IsValid(EquippedRifle) ||
        static_cast<uint8>(NewRole) >
            static_cast<uint8>(EBHWeaponRole::Support))
    {
        return false;
    }

    if (!HasWeaponAuthority())
    {
        ServerEquipWeaponRole(NewRole, bRefillAmmo);
        return true;
    }

    StopAllActions();
    WeaponRole = NewRole;
    ApplyWeaponRoleProfile(bRefillAmmo);
    OnFireModeChanged.Broadcast(FireMode);
    OnWeaponRoleChanged.Broadcast(WeaponRole);
    OnAmmoChanged.Broadcast(MagazineAmmo, ReserveAmmo);
    if (AActor* OwnerActor = GetOwner())
    {
        OwnerActor->ForceNetUpdate();
    }
    return true;
}

EBHWeaponRole UBHWeaponComponent::CycleWeaponRole(
    bool bRefillAmmo
)
{
    const EBHWeaponRole NewRole = GetNextWeaponRole(WeaponRole);
    EquipWeaponRole(NewRole, bRefillAmmo);
    return NewRole;
}

int32 UBHWeaponComponent::AddReserveAmmo(int32 AmmoAmount)
{
    if (!HasWeaponAuthority() ||
        AmmoAmount <= 0 ||
        !IsValid(EquippedRifle) ||
        !CanOperateWeapon())
    {
        return 0;
    }

    MaxReserveAmmo = FMath::Max(0, MaxReserveAmmo);
    const int32 PreviousReserveAmmo = ReserveAmmo;
    ReserveAmmo = FMath::Clamp(
        ReserveAmmo + AmmoAmount,
        0,
        MaxReserveAmmo
    );
    const int32 AmmoAdded = ReserveAmmo - PreviousReserveAmmo;

    if (AmmoAdded > 0)
    {
        OnAmmoChanged.Broadcast(MagazineAmmo, ReserveAmmo);
    }

    return AmmoAdded;
}

int32 UBHWeaponComponent::RemoveReserveAmmo(int32 AmmoAmount)
{
    if (!HasWeaponAuthority() || AmmoAmount <= 0)
    {
        return 0;
    }
    const int32 PreviousReserveAmmo = ReserveAmmo;
    ReserveAmmo = FMath::Max(0, ReserveAmmo - AmmoAmount);
    const int32 Removed = PreviousReserveAmmo - ReserveAmmo;
    if (Removed > 0)
    {
        OnAmmoChanged.Broadcast(MagazineAmmo, ReserveAmmo);
    }
    return Removed;
}

bool UBHWeaponComponent::RestoreAmmoState(
    int32 SavedMagazineAmmo,
    int32 SavedReserveAmmo
)
{
    if (!HasWeaponAuthority() || !IsValid(EquippedRifle))
    {
        return false;
    }

    StopAllActions();

    const FBHRifleConfig& Config = EquippedRifle->GetConfig();
    MagazineAmmo = FMath::Clamp(
        SavedMagazineAmmo,
        0,
        FMath::Max(1, Config.MagazineSize)
    );
    ReserveAmmo = FMath::Clamp(
        SavedReserveAmmo,
        0,
        FMath::Max(0, MaxReserveAmmo)
    );
    OnAmmoChanged.Broadcast(MagazineAmmo, ReserveAmmo);
    return true;
}

bool UBHWeaponComponent::RestoreWeaponHeatState(
    float SavedHeatNormalized,
    bool bSavedOverheated
)
{
    if (!HasWeaponAuthority())
    {
        return false;
    }

    WeaponHeatNormalized = FMath::Clamp(
        SavedHeatNormalized,
        0.0f,
        1.0f
    );
    bWeaponOverheated = bSavedOverheated &&
        WeaponHeatNormalized >= 0.55f;
    OnWeaponHeatChanged.Broadcast(
        WeaponHeatNormalized,
        bWeaponOverheated
    );
    return true;
}

bool UBHWeaponComponent::RestoreFireModeState(
    EBHFireMode SavedFireMode
)
{
    if (!HasWeaponAuthority() || !IsValid(EquippedRifle))
    {
        return false;
    }

    FireMode = EquippedRifle->GetConfig().bAutomatic &&
        SavedFireMode == EBHFireMode::Automatic
        ? EBHFireMode::Automatic
        : EBHFireMode::SemiAutomatic;
    OnFireModeChanged.Broadcast(FireMode);
    if (ABHCharacter* CharacterOwner = Cast<ABHCharacter>(GetOwner()))
    {
        CharacterOwner->RefreshWeaponFireModeHUD(FireMode);
    }
    return true;
}

ABHRifle* UBHWeaponComponent::GetEquippedRifle() const
{
    return EquippedRifle;
}

void UBHWeaponComponent::ServerStartFiring_Implementation()
{
    StartFiring();
}

void UBHWeaponComponent::ServerStopFiring_Implementation()
{
    StopFiring();
}

void UBHWeaponComponent::ServerStartReload_Implementation()
{
    StartReload();
}

void UBHWeaponComponent::ServerStartEmergencyReload_Implementation()
{
    StartEmergencyReload();
}

void UBHWeaponComponent::ServerToggleFireMode_Implementation()
{
    ToggleFireMode();
}

void UBHWeaponComponent::ServerSetAiming_Implementation(
    bool bNewIsAiming
)
{
    if (bNewIsAiming)
    {
        StartAiming();
    }
    else
    {
        StopAiming();
    }
}

void UBHWeaponComponent::ServerStopAllActions_Implementation()
{
    StopAllActions();
}

void UBHWeaponComponent::ServerEquipWeaponRole_Implementation(
    EBHWeaponRole NewRole,
    bool bRefillAmmo
)
{
    EquipWeaponRole(NewRole, bRefillAmmo);
}

void UBHWeaponComponent::MulticastFirePresentation_Implementation(
    const FHitResult& ShotHit,
    bool bHadBlockingHit
)
{
    if (HasWeaponAuthority() || !IsValid(EquippedRifle))
    {
        return;
    }

    const APawn* PawnOwner = Cast<APawn>(GetOwner());

    if (IsValid(PawnOwner) && PawnOwner->IsLocallyControlled())
    {
        EquippedRifle->PlayReplicatedImpactPresentation(
            ShotHit,
            bHadBlockingHit
        );
        return;
    }

    EquippedRifle->PlayReplicatedFirePresentation(
        ShotHit,
        bHadBlockingHit
    );
}

void UBHWeaponComponent::ClientDryFirePresentation_Implementation()
{
    if (IsValid(EquippedRifle))
    {
        EquippedRifle->PlayDryFirePresentation();
    }
}

void UBHWeaponComponent::OnRep_Ammo()
{
    OnAmmoChanged.Broadcast(MagazineAmmo, ReserveAmmo);

#if !UE_BUILD_SHIPPING
    const APawn* OwnerPawn = Cast<APawn>(GetOwner());
    if (!bBattlefieldLootAmmoReplicationLogged &&
        IsValid(OwnerPawn) &&
        OwnerPawn->IsLocallyControlled() &&
        ReserveAmmo == MaxReserveAmmo &&
        FParse::Param(
            FCommandLine::Get(),
            TEXT("BHTestBattlefieldLootAmmoReplication")))
    {
        bBattlefieldLootAmmoReplicationLogged = true;
        UE_LOG(
            LogTemp,
            Display,
            TEXT(
                "BH_BATTLEFIELD_LOOT_AMMO_REPLICATED "
                "result=success owner=%s magazine=%d reserve=%d"
            ),
            *GetNameSafe(OwnerPawn),
            MagazineAmmo,
            ReserveAmmo
        );
    }
#endif
}

void UBHWeaponComponent::OnRep_WeaponHeat()
{
    WeaponHeatNormalized = FMath::Clamp(
        WeaponHeatNormalized,
        0.0f,
        1.0f
    );
    OnWeaponHeatChanged.Broadcast(
        WeaponHeatNormalized,
        bWeaponOverheated
    );
}

void UBHWeaponComponent::OnRep_WeaponState()
{
    if (WeaponState == EBHWeaponState::Reloading &&
        IsValid(EquippedRifle))
    {
        EquippedRifle->PlayReloadPresentation(
            CalculateReloadDuration(
                EquippedRifle->GetConfig().ReloadDuration,
                ReloadType
            )
        );
    }

    OnWeaponStateChanged.Broadcast(WeaponState);
}

void UBHWeaponComponent::OnRep_IsAiming()
{
    if (IsValid(EquippedRifle))
    {
        EquippedRifle->SetAimPresentation(bIsAiming);
    }

    OnAimChanged.Broadcast(bIsAiming);
}

void UBHWeaponComponent::OnRep_WeaponRole()
{
    ApplyWeaponRoleProfile(false);
    OnFireModeChanged.Broadcast(FireMode);
    OnWeaponRoleChanged.Broadcast(WeaponRole);
}

void UBHWeaponComponent::OnRep_FireMode()
{
    OnFireModeChanged.Broadcast(FireMode);
    if (ABHCharacter* CharacterOwner = Cast<ABHCharacter>(GetOwner()))
    {
        CharacterOwner->RefreshWeaponFireModeHUD(FireMode);
    }
}

bool UBHWeaponComponent::HasWeaponAuthority() const
{
    const AActor* Owner = GetOwner();
    return IsValid(Owner) && Owner->HasAuthority();
}

bool UBHWeaponComponent::CanOperateWeapon() const
{
    if (!IsValid(EquippedRifle) || !IsValid(GetOwner()))
    {
        return false;
    }

    const UBHHealthComponent* HealthComponent =
        GetOwner()->FindComponentByClass<UBHHealthComponent>();

    return !IsValid(HealthComponent) ||
        !HealthComponent->IsDead();
}

void UBHWeaponComponent::EquipDefaultRifle()
{
    if (!DefaultRifleClass || !IsValid(GetWorld()))
    {
        UE_LOG(
            LogTemp,
            Warning,
            TEXT("Weapon component has no default rifle class.")
        );
        return;
    }

    FActorSpawnParameters SpawnParameters;
    SpawnParameters.Owner = GetOwner();
    SpawnParameters.Instigator = Cast<APawn>(GetOwner());
    SpawnParameters.SpawnCollisionHandlingOverride =
        ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    EquippedRifle = GetWorld()->SpawnActor<ABHRifle>(
        DefaultRifleClass,
        GetOwner()->GetActorTransform(),
        SpawnParameters
    );

    if (!IsValid(EquippedRifle))
    {
        return;
    }

    AttachEquippedRifle();

    AssaultBaselineConfig = EquippedRifle->GetConfig();
    AssaultBaselineMaximumReserveAmmo = MaxReserveAmmo;
    bHasAssaultBaselineConfig = true;
    ApplyWeaponRoleProfile(false);

    const FBHRifleConfig& Config = EquippedRifle->GetConfig();
    CurrentSpreadBloomDegrees = 0.0f;
    WeaponHeatNormalized = 0.0f;
    bWeaponOverheated = false;
    OnWeaponHeatChanged.Broadcast(WeaponHeatNormalized, bWeaponOverheated);
    PendingRecoilOffset = FVector2D::ZeroVector;
    LastShotTime = -BIG_NUMBER;
    LastPredictedShotTime = -BIG_NUMBER;
    MaxReserveAmmo = FMath::Max(0, MaxReserveAmmo);

    if (HasWeaponAuthority())
    {
        MagazineAmmo = FMath::Max(1, Config.MagazineSize);
        FireMode = Config.bAutomatic
            ? EBHFireMode::Automatic
            : EBHFireMode::SemiAutomatic;
        ReserveAmmo = FMath::Clamp(
            Config.StartingReserveAmmo,
            0,
            MaxReserveAmmo
        );
        SetWeaponState(EBHWeaponState::Idle);
    }

    OnWeaponEquipped.Broadcast(EquippedRifle);
    OnFireModeChanged.Broadcast(FireMode);
    if (ABHCharacter* CharacterOwner = Cast<ABHCharacter>(GetOwner()))
    {
        CharacterOwner->RefreshWeaponFireModeHUD(FireMode);
    }
    OnAmmoChanged.Broadcast(MagazineAmmo, ReserveAmmo);
}

void UBHWeaponComponent::ApplyWeaponRoleProfile(
    bool bRefillAmmo
)
{
    if (!IsValid(EquippedRifle))
    {
        return;
    }

    FBHWeaponRoleProfile Profile =
        BuildWeaponRoleProfile(WeaponRole);
    if (WeaponRole == EBHWeaponRole::Assault &&
        bHasAssaultBaselineConfig)
    {
        Profile.RifleConfig = AssaultBaselineConfig;
        Profile.MaximumReserveAmmo =
            AssaultBaselineMaximumReserveAmmo;
    }
    EquippedRifle->ApplyWeaponConfig(Profile.RifleConfig);
    MaxReserveAmmo = FMath::Max(0, Profile.MaximumReserveAmmo);
    CurrentSpreadBloomDegrees = 0.0f;
    PendingRecoilOffset = FVector2D::ZeroVector;
    LastShotTime = -BIG_NUMBER;
    LastPredictedShotTime = -BIG_NUMBER;

    if (HasWeaponAuthority())
    {
        const FBHRifleConfig& Config = EquippedRifle->GetConfig();
        MagazineAmmo = bRefillAmmo
            ? FMath::Max(1, Config.MagazineSize)
            : FMath::Clamp(
                MagazineAmmo,
                0,
                FMath::Max(1, Config.MagazineSize)
            );
        ReserveAmmo = bRefillAmmo
            ? MaxReserveAmmo
            : FMath::Clamp(ReserveAmmo, 0, MaxReserveAmmo);
        FireMode = Config.bAutomatic
            ? EBHFireMode::Automatic
            : EBHFireMode::SemiAutomatic;
        SetWeaponState(EBHWeaponState::Idle);
    }
}

void UBHWeaponComponent::AttachEquippedRifle()
{
    if (!IsValid(EquippedRifle) || !IsValid(GetOwner()))
    {
        return;
    }

    static const FName FirstPersonWeaponSocketName(
        TEXT("ik_hand_gun")
    );

    USceneComponent* AttachParent = nullptr;
    FName AttachSocketName = NAME_None;
    const APawn* PawnOwner = Cast<APawn>(GetOwner());
    const bool bLocalFirstPerson =
        IsValid(PawnOwner) && PawnOwner->IsLocallyControlled();

    if (const ABHCharacter* CharacterOwner =
        Cast<ABHCharacter>(GetOwner()))
    {
        if (bLocalFirstPerson)
        {
            USkeletalMeshComponent* FirstPersonArms =
                CharacterOwner->GetFirstPersonArmsMesh();

            if (IsValid(FirstPersonArms) &&
                FirstPersonArms->DoesSocketExist(
                    FirstPersonWeaponSocketName
                ))
            {
                AttachParent = FirstPersonArms;
                AttachSocketName = FirstPersonWeaponSocketName;
            }
        }
        else
        {
            USkeletalMeshComponent* ThirdPersonMesh =
                CharacterOwner->GetMesh();

            if (IsValid(ThirdPersonMesh))
            {
                AttachParent = ThirdPersonMesh;

                if (!ThirdPersonWeaponSocketName.IsNone() &&
                    ThirdPersonMesh->DoesSocketExist(
                        ThirdPersonWeaponSocketName
                    ))
                {
                    AttachSocketName =
                        ThirdPersonWeaponSocketName;
                }
            }
        }
    }

    if (!IsValid(AttachParent))
    {
        AttachParent = IsValid(AimCamera)
            ? Cast<USceneComponent>(AimCamera)
            : GetOwner()->GetRootComponent();
    }

    if (IsValid(AttachParent))
    {
        EquippedRifle->SetFirstPersonPresentation(
            bLocalFirstPerson
        );
        EquippedRifle->AttachToComponent(
            AttachParent,
            FAttachmentTransformRules::SnapToTargetNotIncludingScale,
            AttachSocketName
        );
    }
}

void UBHWeaponComponent::TryFire()
{
    if (!HasWeaponAuthority())
    {
        return;
    }

    if (!bWantsToFire ||
        !CanOperateWeapon() ||
        WeaponState == EBHWeaponState::Reloading)
    {
        StopFiring();
        return;
    }

    if (MagazineAmmo <= 0)
    {
        const APawn* PawnOwner = Cast<APawn>(GetOwner());

        if (IsValid(EquippedRifle) &&
            GetNetMode() != NM_DedicatedServer)
        {
            EquippedRifle->PlayDryFirePresentation();
        }

        if (IsValid(PawnOwner) &&
            !PawnOwner->IsLocallyControlled())
        {
            ClientDryFirePresentation();
        }

        StopFiring();
        return;
    }

    UWorld* World = GetWorld();

    if (!IsValid(World))
    {
        StopFiring();
        return;
    }

    const float FireInterval = GetFireInterval();
    const float TimeSinceLastShot =
        World->GetTimeSeconds() - LastShotTime;

    if (TimeSinceLastShot < FireInterval)
    {
        World->GetTimerManager().SetTimer(
            RefireTimerHandle,
            this,
            &UBHWeaponComponent::TryFire,
            FireInterval - TimeSinceLastShot,
            false
        );
        return;
    }

    APawn* PawnOwner = Cast<APawn>(GetOwner());
    AController* Controller = PawnOwner
        ? PawnOwner->GetController()
        : nullptr;
    FVector CameraOrigin;
    FRotator CameraRotation;

    if (IsValid(Controller))
    {
        Controller->GetPlayerViewPoint(
            CameraOrigin,
            CameraRotation
        );
    }
    else if (IsValid(AimCamera))
    {
        CameraOrigin = AimCamera->GetComponentLocation();
        CameraRotation = AimCamera->GetComponentRotation();
    }
    else
    {
        StopFiring();
        return;
    }

    const FBHRifleConfig& Config = EquippedRifle->GetConfig();
    float SpreadDegrees = bIsAiming
        ? Config.ADSSpreadDegrees
        : Config.HipSpreadDegrees;
    SpreadDegrees += CurrentSpreadBloomDegrees;
    SpreadDegrees *= CalculateHeatSpreadMultiplier(WeaponHeatNormalized);

    if (const ABHCharacter* CharacterOwner =
        Cast<ABHCharacter>(GetOwner());
        IsValid(CharacterOwner))
    {
        SpreadDegrees *=
            CharacterOwner->GetWeaponSpreadMultiplier();
    }

    FHitResult ShotHit;
    bool bHadBlockingHit = false;

    const bool bDamagedTarget = EquippedRifle->PerformHitscan(
        CameraOrigin,
        CameraRotation.Vector(),
        SpreadDegrees,
        Controller,
        &ShotHit,
        &bHadBlockingHit
    );
    if (UGameInstance* GameInstance = GetWorld()
        ? GetWorld()->GetGameInstance()
        : nullptr)
    {
        const float EngagementRange = bHadBlockingHit
            ? FVector::Distance(CameraOrigin, ShotHit.ImpactPoint)
            : 0.0f;
        GameInstance->GetSubsystem<UBHPlaytestTelemetrySubsystem>()->RecordEvent(
            TEXT("player_shot"),
            {{TEXT("result"), bDamagedTarget ? TEXT("damage") :
                (bHadBlockingHit ? TEXT("impact") : TEXT("miss"))},
             {TEXT("rangeMeters"), FString::FromInt(FMath::RoundToInt(EngagementRange / 100.0f))},
             {TEXT("aiming"), bIsAiming ? TEXT("true") : TEXT("false")}}
        );
    }
    MulticastFirePresentation(ShotHit, bHadBlockingHit);

    --MagazineAmmo;
    AddShotHeat(BuildWeaponRoleProfile(WeaponRole));
    LastShotTime = World->GetTimeSeconds();
    CurrentSpreadBloomDegrees = FMath::Min(
        CurrentSpreadBloomDegrees +
            FMath::Max(0.0f, Config.SpreadPerShotDegrees),
        FMath::Max(0.0f, Config.MaxSpreadBloomDegrees)
    );

    if (IsValid(PawnOwner) && PawnOwner->IsLocallyControlled())
    {
        ApplyRecoil();
    }

    OnAmmoChanged.Broadcast(MagazineAmmo, ReserveAmmo);

    if (FireMode == EBHFireMode::Automatic &&
        Config.bAutomatic &&
        bWantsToFire &&
        MagazineAmmo > 0 &&
        !bWeaponOverheated)
    {
        World->GetTimerManager().SetTimer(
            RefireTimerHandle,
            this,
            &UBHWeaponComponent::TryFire,
            FireInterval,
            false
        );
    }
    else
    {
        bWantsToFire = false;
        SetWeaponState(EBHWeaponState::Idle);
    }
}

void UBHWeaponComponent::TryPredictedFire()
{
    if (HasWeaponAuthority())
    {
        return;
    }

    if (!bWantsToFire ||
        !CanOperateWeapon() ||
        bWeaponOverheated ||
        WeaponState == EBHWeaponState::Reloading ||
        MagazineAmmo <= 0)
    {
        bWantsToFire = false;

        if (UWorld* World = GetWorld())
        {
            World->GetTimerManager().ClearTimer(
                RefireTimerHandle
            );
        }

        if (WeaponState == EBHWeaponState::Firing)
        {
            SetWeaponState(EBHWeaponState::Idle);
        }
        return;
    }

    UWorld* World = GetWorld();

    if (!IsValid(World) || !IsValid(EquippedRifle))
    {
        return;
    }

    const float FireInterval = GetFireInterval();
    const float TimeSinceLastPredictedShot =
        World->GetTimeSeconds() - LastPredictedShotTime;

    if (TimeSinceLastPredictedShot < FireInterval)
    {
        World->GetTimerManager().SetTimer(
            RefireTimerHandle,
            this,
            &UBHWeaponComponent::TryPredictedFire,
            FireInterval - TimeSinceLastPredictedShot,
            false
        );
        return;
    }

    EquippedRifle->PlayPredictedFirePresentation();
    --MagazineAmmo;
    LastPredictedShotTime = World->GetTimeSeconds();
    LastShotTime = LastPredictedShotTime;

    const FBHRifleConfig& Config = EquippedRifle->GetConfig();
    CurrentSpreadBloomDegrees = FMath::Min(
        CurrentSpreadBloomDegrees +
            FMath::Max(0.0f, Config.SpreadPerShotDegrees),
        FMath::Max(0.0f, Config.MaxSpreadBloomDegrees)
    );
    ApplyRecoil();
    OnAmmoChanged.Broadcast(MagazineAmmo, ReserveAmmo);

    if (FireMode == EBHFireMode::Automatic &&
        Config.bAutomatic &&
        bWantsToFire &&
        MagazineAmmo > 0)
    {
        World->GetTimerManager().SetTimer(
            RefireTimerHandle,
            this,
            &UBHWeaponComponent::TryPredictedFire,
            FireInterval,
            false
        );
    }
    else
    {
        bWantsToFire = false;
        SetWeaponState(EBHWeaponState::Idle);
    }
}

void UBHWeaponComponent::FinishReload()
{
    if (!HasWeaponAuthority())
    {
        return;
    }

    if (!CanOperateWeapon() || !IsValid(EquippedRifle))
    {
        SetWeaponState(EBHWeaponState::Idle);
        return;
    }

    const int32 MagazineSize =
        FMath::Max(1, EquippedRifle->GetConfig().MagazineSize);
    const int32 AmmoNeeded = MagazineSize - MagazineAmmo;
    const int32 AmmoToLoad = FMath::Min(AmmoNeeded, ReserveAmmo);

    MagazineAmmo += AmmoToLoad;
    ReserveAmmo -= AmmoToLoad;

    UE_LOG(LogTemp, Display, TEXT(
        "BH_RELOAD_COMPLETED owner=%s type=%s magazine=%d reserve=%d"),
        *GetNameSafe(GetOwner()),
        ReloadType == EBHReloadType::Emergency
            ? TEXT("emergency") : TEXT("tactical"),
        MagazineAmmo,
        ReserveAmmo);

    OnAmmoChanged.Broadcast(MagazineAmmo, ReserveAmmo);
    SetWeaponState(EBHWeaponState::Idle);

    if (bWantsToAim)
    {
        StartAiming();
    }
}

void UBHWeaponComponent::ApplyRecoil()
{
    APawn* PawnOwner = Cast<APawn>(GetOwner());

    if (!IsValid(PawnOwner) || !IsValid(EquippedRifle))
    {
        return;
    }

    const FBHRifleConfig& Config = EquippedRifle->GetConfig();
    AController* Controller = PawnOwner->GetController();

    if (!IsValid(Controller))
    {
        return;
    }

    const UGameInstance* GameInstance = PawnOwner->GetGameInstance();
    const UBHUserSettingsSubsystem* SettingsSubsystem = GameInstance
        ? GameInstance->GetSubsystem<UBHUserSettingsSubsystem>()
        : nullptr;
    const float ComfortScale = SettingsSubsystem
        ? SettingsSubsystem->GetRecoilAnimationScale()
        : 1.0f;
    const float AimMultiplier = (bIsAiming
        ? FMath::Clamp(Config.ADSRecoilMultiplier, 0.0f, 1.0f)
        : 1.0f) * FMath::Clamp(ComfortScale, 0.0f, 1.0f) *
        FMath::Lerp(1.0f, 1.35f, WeaponHeatNormalized);
    const float PitchVariation = FMath::Clamp(
        Config.RecoilPitchVariation,
        0.0f,
        1.0f
    );
    const float PitchImpulse =
        FMath::Max(0.0f, Config.RecoilPitch) *
        FMath::FRandRange(
            1.0f - PitchVariation,
            1.0f + PitchVariation
        ) *
        AimMultiplier;
    const float YawImpulse = FMath::FRandRange(
        -FMath::Max(0.0f, Config.RecoilYaw),
        FMath::Max(0.0f, Config.RecoilYaw)
    ) * AimMultiplier;

    FRotator RecoilRotation = Controller->GetControlRotation();
    RecoilRotation.Pitch = FMath::Clamp(
        FRotator::NormalizeAxis(RecoilRotation.Pitch) -
            PitchImpulse,
        -89.0f,
        89.0f
    );
    RecoilRotation.Yaw = FRotator::NormalizeAxis(
        RecoilRotation.Yaw + YawImpulse
    );
    Controller->SetControlRotation(RecoilRotation);

    PendingRecoilOffset.X += PitchImpulse;
    PendingRecoilOffset.Y += YawImpulse;
}

void UBHWeaponComponent::UpdateWeaponRecovery(float DeltaTime)
{
    if (!IsValid(EquippedRifle) ||
        !IsValid(GetWorld()) ||
        DeltaTime <= 0.0f)
    {
        return;
    }

    const FBHRifleConfig& Config = EquippedRifle->GetConfig();
    const float TimeSinceLastShot =
        GetWorld()->GetTimeSeconds() - LastShotTime;

    if (HasWeaponAuthority() && TimeSinceLastShot >= 0.55f &&
        WeaponHeatNormalized > 0.0f)
    {
        const FBHWeaponRoleProfile HeatProfile =
            BuildWeaponRoleProfile(WeaponRole);
        const float PreviousHeat = WeaponHeatNormalized;
        WeaponHeatNormalized = FMath::Max(
            0.0f,
            WeaponHeatNormalized -
                FMath::Max(0.01f, HeatProfile.HeatCoolingPerSecond) *
                DeltaTime
        );
        if (bWeaponOverheated && WeaponHeatNormalized <= 0.55f)
        {
            bWeaponOverheated = false;
            if (ABHCharacter* CharacterOwner =
                Cast<ABHCharacter>(GetOwner()))
            {
                CharacterOwner->ShowStatusNotification(NSLOCTEXT(
                    "BrokenHorizon", "WeaponHeatRecovered",
                    "WEAPON COOLED // FIRE CONTROL RESTORED"
                ));
            }
        }
        if (!FMath::IsNearlyEqual(PreviousHeat, WeaponHeatNormalized, 0.002f))
        {
            OnWeaponHeatChanged.Broadcast(
                WeaponHeatNormalized,
                bWeaponOverheated
            );
        }
    }

    if (TimeSinceLastShot >=
        FMath::Max(0.0f, Config.SpreadRecoveryDelay))
    {
        CurrentSpreadBloomDegrees = FMath::FInterpConstantTo(
            CurrentSpreadBloomDegrees,
            0.0f,
            DeltaTime,
            FMath::Max(0.0f, Config.SpreadRecoverySpeed)
        );
    }

    if (TimeSinceLastShot <
            FMath::Max(0.0f, Config.RecoilRecoveryDelay) ||
        PendingRecoilOffset.IsNearlyZero())
    {
        return;
    }

    APawn* PawnOwner = Cast<APawn>(GetOwner());
    AController* Controller = IsValid(PawnOwner)
        ? PawnOwner->GetController()
        : nullptr;

    if (!IsValid(Controller))
    {
        PendingRecoilOffset = FVector2D::ZeroVector;
        return;
    }

    const float RecoverySpeed =
        FMath::Max(0.0f, Config.RecoilRecoverySpeed);
    const float RecoveredPitch = FMath::Min(
        PendingRecoilOffset.X,
        RecoverySpeed * DeltaTime
    );
    const float RemainingYaw = FMath::FInterpConstantTo(
        PendingRecoilOffset.Y,
        0.0f,
        DeltaTime,
        RecoverySpeed
    );
    const float RecoveredYaw =
        PendingRecoilOffset.Y - RemainingYaw;

    FRotator RecoveredRotation =
        Controller->GetControlRotation();
    RecoveredRotation.Pitch = FMath::Clamp(
        FRotator::NormalizeAxis(RecoveredRotation.Pitch) +
            RecoveredPitch,
        -89.0f,
        89.0f
    );
    RecoveredRotation.Yaw = FRotator::NormalizeAxis(
        RecoveredRotation.Yaw - RecoveredYaw
    );
    Controller->SetControlRotation(RecoveredRotation);

    PendingRecoilOffset.X = FMath::Max(
        0.0f,
        PendingRecoilOffset.X - RecoveredPitch
    );
    PendingRecoilOffset.Y = RemainingYaw;
}

void UBHWeaponComponent::AddShotHeat(const FBHWeaponRoleProfile& Profile)
{
    if (!HasWeaponAuthority())
    {
        return;
    }
    WeaponHeatNormalized = CalculateHeatAfterShot(
        WeaponHeatNormalized,
        Profile.HeatPerShot
    );
    if (WeaponHeatNormalized >= 1.0f && !bWeaponOverheated)
    {
        bWeaponOverheated = true;
        bWantsToFire = false;
        if (ABHCharacter* CharacterOwner = Cast<ABHCharacter>(GetOwner()))
        {
            CharacterOwner->ShowPriorityStatusNotification(
                NSLOCTEXT(
                    "BrokenHorizon", "WeaponOverheated",
                    "WEAPON OVERHEATED // CEASE FIRE\n\n"
                    "Cooling below 55% is required."
                ),
                EBHNotificationPriority::High
            );
        }
        UE_LOG(LogTemp, Display, TEXT(
            "BH_WEAPON_OVERHEATED owner=%s role=%d magazine=%d"),
            *GetNameSafe(GetOwner()),
            static_cast<int32>(WeaponRole),
            MagazineAmmo);
    }
    OnWeaponHeatChanged.Broadcast(WeaponHeatNormalized, bWeaponOverheated);
}

void UBHWeaponComponent::SetWeaponState(
    EBHWeaponState NewState
)
{
    if (WeaponState == NewState)
    {
        return;
    }

    WeaponState = NewState;
    OnWeaponStateChanged.Broadcast(WeaponState);
}

float UBHWeaponComponent::GetFireInterval() const
{
    if (!IsValid(EquippedRifle))
    {
        return 0.1f;
    }

    return 60.0f / FMath::Max(
        1.0f,
        EquippedRifle->GetConfig().RoundsPerMinute
    );
}
