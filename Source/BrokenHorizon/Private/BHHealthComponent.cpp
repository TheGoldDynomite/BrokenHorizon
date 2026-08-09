#include "BHHealthComponent.h"

#include "BHCharacter.h"
#include "BHWarSubsystem.h"
#include "Engine/GameInstance.h"
#include "Net/UnrealNetwork.h"

UBHHealthComponent::UBHHealthComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    SetIsReplicatedByDefault(true);
}

float UBHHealthComponent::CalculateFriendlyFireDamage(
    float DamageAmount,
    bool bFriendlyFire,
    float FriendlyFireMultiplier
)
{
    const float SafeDamage = FMath::Max(0.0f, DamageAmount);
    if (!bFriendlyFire)
    {
        return SafeDamage;
    }

    return SafeDamage * FMath::Clamp(
        FriendlyFireMultiplier,
        0.0f,
        1.0f
    );
}

void UBHHealthComponent::GetLifetimeReplicatedProps(
    TArray<FLifetimeProperty>& OutLifetimeProps
) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(
        UBHHealthComponent,
        CurrentHealth
    );
}

void UBHHealthComponent::BeginPlay()
{
    Super::BeginPlay();

    MaxHealth = FMath::Max(1.0f, MaxHealth);
    CurrentHealth = FMath::Clamp(
        CurrentHealth,
        0.0f,
        MaxHealth
    );
    bIsDead = CurrentHealth <= 0.0f;

    if (AActor* Owner = GetOwner())
    {
        Owner->OnTakeAnyDamage.AddDynamic(
            this,
            &UBHHealthComponent::HandleOwnerDamage
        );
    }
}

void UBHHealthComponent::EndPlay(
    const EEndPlayReason::Type EndPlayReason
)
{
    if (AActor* Owner = GetOwner())
    {
        Owner->OnTakeAnyDamage.RemoveDynamic(
            this,
            &UBHHealthComponent::HandleOwnerDamage
        );
    }

    Super::EndPlay(EndPlayReason);
}

void UBHHealthComponent::HandleOwnerDamage(
    AActor* DamagedActor,
    float Damage,
    const UDamageType* DamageType,
    AController* InstigatedBy,
    AActor* DamageCauser
)
{
    if (!HasMutationAuthority())
    {
        return;
    }

    float DifficultyAdjustedDamage = Damage;
    if (Cast<ABHCharacter>(DamagedActor))
    {
        const UGameInstance* GameInstance =
            DamagedActor ? DamagedActor->GetGameInstance() : nullptr;
        const UBHWarSubsystem* WarSubsystem = GameInstance
            ? GameInstance->GetSubsystem<UBHWarSubsystem>()
            : nullptr;
        if (IsValid(WarSubsystem))
        {
            DifficultyAdjustedDamage *= FMath::Clamp(
                WarSubsystem->GetCampaignDifficulty()
                    .IncomingDamageMultiplier,
                0.5f,
                2.0f
            );
        }
    }
    ApplyDamage(DifficultyAdjustedDamage, DamageCauser);
}

float UBHHealthComponent::ApplyDamage(
    float DamageAmount,
    AActor* DamageCauser
)
{
    if (!HasMutationAuthority() ||
        bIsDead ||
        DamageAmount <= 0.0f)
    {
        return 0.0f;
    }

    const float PreviousHealth = CurrentHealth;
    CurrentHealth = FMath::Clamp(
        CurrentHealth - DamageAmount,
        0.0f,
        MaxHealth
    );
    const float DamageApplied = PreviousHealth - CurrentHealth;

    if (DamageApplied <= 0.0f)
    {
        return 0.0f;
    }

    OnDamaged.Broadcast(DamageApplied, DamageCauser);
    OnHealthChanged.Broadcast(CurrentHealth, MaxHealth);

    if (CurrentHealth <= 0.0f && !bIsDead)
    {
        bIsDead = true;
        OnDeath.Broadcast(DamageCauser);
    }

    return DamageApplied;
}

float UBHHealthComponent::Heal(float HealingAmount)
{
    if (!HasMutationAuthority() ||
        bIsDead ||
        HealingAmount <= 0.0f)
    {
        return 0.0f;
    }

    const float PreviousHealth = CurrentHealth;
    CurrentHealth = FMath::Clamp(
        CurrentHealth + HealingAmount,
        0.0f,
        MaxHealth
    );
    const float HealingApplied = CurrentHealth - PreviousHealth;

    if (HealingApplied > 0.0f)
    {
        OnHealed.Broadcast(HealingApplied);
        OnHealthChanged.Broadcast(CurrentHealth, MaxHealth);
    }

    return HealingApplied;
}

void UBHHealthComponent::ResetHealth()
{
    if (!HasMutationAuthority())
    {
        return;
    }

    MaxHealth = FMath::Max(1.0f, MaxHealth);
    CurrentHealth = MaxHealth;
    bIsDead = false;
    OnHealthChanged.Broadcast(CurrentHealth, MaxHealth);
}

void UBHHealthComponent::ReviveAtHealth(float RevivedHealth)
{
    if (!HasMutationAuthority())
    {
        return;
    }
    MaxHealth = FMath::Max(1.0f, MaxHealth);
    CurrentHealth = FMath::Clamp(RevivedHealth, 1.0f, MaxHealth);
    bIsDead = false;
    OnHealthChanged.Broadcast(CurrentHealth, MaxHealth);
}

void UBHHealthComponent::ConfigureMaximumHealth(
    float NewMaximumHealth,
    bool bResetCurrentHealth
)
{
    MaxHealth = FMath::Max(1.0f, NewMaximumHealth);
    CurrentHealth = bResetCurrentHealth
        ? MaxHealth
        : FMath::Clamp(CurrentHealth, 0.0f, MaxHealth);
    bIsDead = CurrentHealth <= 0.0f;
    OnHealthChanged.Broadcast(CurrentHealth, MaxHealth);
}

void UBHHealthComponent::RestorePersistentHealthState(
    float SavedHealth
)
{
    if (!HasMutationAuthority())
    {
        return;
    }

    MaxHealth = FMath::Max(1.0f, MaxHealth);
    CurrentHealth = FMath::Clamp(
        SavedHealth,
        1.0f,
        MaxHealth
    );
    bIsDead = false;
    OnHealthChanged.Broadcast(CurrentHealth, MaxHealth);
}

void UBHHealthComponent::RestorePersistentDeathState()
{
    if (!HasMutationAuthority())
    {
        return;
    }

    MaxHealth = FMath::Max(1.0f, MaxHealth);
    CurrentHealth = 0.0f;
    bIsDead = true;
    OnHealthChanged.Broadcast(CurrentHealth, MaxHealth);
}

float UBHHealthComponent::GetCurrentHealth() const
{
    return CurrentHealth;
}

float UBHHealthComponent::GetMaxHealth() const
{
    return MaxHealth;
}

bool UBHHealthComponent::IsDead() const
{
    return bIsDead;
}

bool UBHHealthComponent::IsFullHealth() const
{
    return CurrentHealth >= MaxHealth;
}

float UBHHealthComponent::GetHealthPercentage() const
{
    return MaxHealth > 0.0f
        ? FMath::Clamp(CurrentHealth / MaxHealth, 0.0f, 1.0f)
        : 0.0f;
}

void UBHHealthComponent::OnRep_CurrentHealth(
    float PreviousHealth
)
{
    static_cast<void>(PreviousHealth);
    MaxHealth = FMath::Max(1.0f, MaxHealth);
    CurrentHealth = FMath::Clamp(
        CurrentHealth,
        0.0f,
        MaxHealth
    );
    bIsDead = CurrentHealth <= 0.0f;
    OnHealthChanged.Broadcast(CurrentHealth, MaxHealth);
}

bool UBHHealthComponent::HasMutationAuthority() const
{
    const AActor* Owner = GetOwner();
    return !IsValid(Owner) || Owner->HasAuthority();
}
