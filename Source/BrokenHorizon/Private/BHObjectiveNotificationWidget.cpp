#include "BHObjectiveNotificationWidget.h"

#include "Components/TextBlock.h"
#include "Engine/World.h"
#include "TimerManager.h"

void UBHObjectiveNotificationWidget::ShowNotification(
    const FText& Message
)
{
    if (IsValid(NotificationText))
    {
        NotificationText->SetText(Message);
    }

    SetVisibility(ESlateVisibility::Visible);

    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(HideTimerHandle);
        World->GetTimerManager().SetTimer(
            HideTimerHandle,
            this,
            &UBHObjectiveNotificationWidget::HideNotification,
            DisplayDuration,
            false
        );
    }
}

void UBHObjectiveNotificationWidget::HideNotification()
{
    SetVisibility(ESlateVisibility::Collapsed);
}

void UBHObjectiveNotificationWidget::NativeDestruct()
{
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(HideTimerHandle);
    }

    Super::NativeDestruct();
}
