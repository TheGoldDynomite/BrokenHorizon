#include "BHMedicalSupply.h"

#include "BHCharacter.h"
#include "BHHealthComponent.h"
#include "BHInjuryComponent.h"

ABHMedicalSupply::ABHMedicalSupply()
{
    InteractionText = NSLOCTEXT(
        "BrokenHorizon",
        "MedicalSupplyInteraction",
        "Restock Medical Supplies"
    );
}

bool ABHMedicalSupply::TryApplyToCharacter(
    ABHCharacter* Character
)
{
    if (!IsValid(Character))
    {
        return false;
    }

    UBHInjuryComponent* InjuryComponent =
        Character->GetInjuryComponent();
    UBHHealthComponent* HealthComponent = IsValid(Character)
        ? Character->GetHealthComponent()
        : nullptr;

    const int32 SafeMedkitAmount = FMath::Max(0, MedkitAmount);
    const int32 SafeDressingAmount =
        FMath::Max(0, FieldDressingAmount);
    const bool bGrantedSupplies =
        IsValid(InjuryComponent) &&
        (SafeMedkitAmount > 0 || SafeDressingAmount > 0);

    if (bGrantedSupplies)
    {
        InjuryComponent->AddMedicalSupplies(
            SafeMedkitAmount,
            SafeDressingAmount
        );
    }

    const bool bHealed =
        HealAmount > 0.0f &&
        IsValid(HealthComponent) &&
        !HealthComponent->IsDead() &&
        !HealthComponent->IsFullHealth() &&
        HealthComponent->Heal(FMath::Max(0.0f, HealAmount)) > 0.0f;

    if (!bGrantedSupplies && !bHealed)
    {
        return false;
    }

    FString Feedback(TEXT("MEDICAL RESUPPLY"));

    if (SafeMedkitAmount > 0)
    {
        Feedback += FString::Printf(
            TEXT("\n+%d MEDKIT%s"),
            SafeMedkitAmount,
            SafeMedkitAmount == 1 ? TEXT("") : TEXT("S")
        );
    }

    if (SafeDressingAmount > 0)
    {
        Feedback += FString::Printf(
            TEXT("  +%d DRESSING%s"),
            SafeDressingAmount,
            SafeDressingAmount == 1 ? TEXT("") : TEXT("S")
        );
    }

    if (bHealed)
    {
        Feedback += TEXT("\nIMMEDIATE TREATMENT APPLIED");
    }

    Character->ShowStatusNotification(
        FText::FromString(Feedback)
    );
    return true;
}
