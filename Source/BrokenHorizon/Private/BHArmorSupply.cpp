#include "BHArmorSupply.h"

#include "BHCharacter.h"
#include "BHInjuryComponent.h"

ABHArmorSupply::ABHArmorSupply()
{
    InteractionText = NSLOCTEXT(
        "BrokenHorizon",
        "ArmorSupplyInteraction",
        "Install Armor Plate"
    );
}

bool ABHArmorSupply::TryApplyToCharacter(
    ABHCharacter* Character
)
{
    UBHInjuryComponent* InjuryComponent = IsValid(Character)
        ? Character->GetInjuryComponent()
        : nullptr;
    const float SafeHelmetAmount =
        FMath::Max(0.0f, HelmetDurabilityAmount);
    const float SafeBodyArmorAmount =
        FMath::Max(0.0f, BodyArmorDurabilityAmount);

    if (!IsValid(InjuryComponent) ||
        !InjuryComponent->RepairArmor(
            SafeHelmetAmount,
            SafeBodyArmorAmount
        ))
    {
        return false;
    }

    FString Feedback(TEXT("ARMOR RESUPPLY"));

    if (SafeHelmetAmount > 0.0f)
    {
        Feedback += TEXT("\nHELMET RESTORED");
    }

    if (SafeBodyArmorAmount > 0.0f)
    {
        Feedback += SafeHelmetAmount > 0.0f
            ? TEXT("  VEST RESTORED")
            : TEXT("\nVEST RESTORED");
    }

    Character->ShowStatusNotification(
        FText::FromString(Feedback)
    );
    return true;
}
