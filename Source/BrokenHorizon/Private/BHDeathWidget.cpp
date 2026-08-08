#include "BHDeathWidget.h"

#include "BHUIStyle.h"
#include "Components/TextBlock.h"

void UBHDeathWidget::NativeConstruct()
{
    Super::NativeConstruct();

    if (DeathMessage.IsEmpty())
    {
        DeathMessage = NSLOCTEXT(
            "BrokenHorizon",
            "PlayerDeathMessage",
            "YOU DIED"
        );
    }

    BHUIStyle::Apply(*this, EBHUIStyleContext::Alert);
    SetVisibility(ESlateVisibility::Collapsed);
}

void UBHDeathWidget::ShowDeathScreen()
{
    ShowDeathScreenWithRespawnDelay(0.0f);
}

void UBHDeathWidget::ShowDeathScreenWithRespawnDelay(
    float RespawnDelay
)
{
    if (IsValid(DeathText))
    {
        DeathText->SetText(DeathMessage);
    }

    SetRenderOpacity(1.0f);
    SetVisibility(ESlateVisibility::Visible);
    OnDeathFeedback(FMath::Max(0.0f, RespawnDelay));
}
