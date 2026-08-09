#include "BHCharacter.h"

#include "BHCombatStatusWidget.h"
#include "BHHealthComponent.h"
#include "Blueprint/UserWidget.h"
#include "GameFramework/PlayerController.h"

void ABHCharacter::EnsureCombatStatusWidget()
{
    if (IsValid(CombatStatusWidget))
    {
        return;
    }

    APlayerController* PlayerController =
        ResolveOwningPlayerController();

    if (!IsValid(PlayerController) ||
        PlayerController->GetLocalPlayer() == nullptr)
    {
        return;
    }

    if (CombatStatusWidgetClass)
    {
        CombatStatusWidget = CreateWidget<UBHCombatStatusWidget>(
            PlayerController,
            CombatStatusWidgetClass
        );
    }
    else
    {
        CombatStatusWidget = CreateWidget<UBHCombatStatusWidget>(
            PlayerController,
            UBHCombatStatusWidget::StaticClass()
        );
    }

    if (!IsValid(CombatStatusWidget))
    {
        return;
    }

    CombatStatusWidget->AddToViewport();
    CombatStatusWidget->SetHealth(
        HealthComponent
            ? HealthComponent->GetCurrentHealth()
            : 0.0f,
        HealthComponent
            ? HealthComponent->GetMaxHealth()
            : 1.0f
    );
    CombatStatusWidget->SetStamina(
        CurrentStamina,
        MaxStamina
    );
    RefreshFragGrenadeHUD();
    RefreshEngineeringHUD();

    UE_LOG(
        LogTemp,
        Display,
        TEXT("BH_COMBAT_STATUS_WIDGET_READY local=1")
    );
}
