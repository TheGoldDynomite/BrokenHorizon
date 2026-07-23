#include "BHObjectiveNotificationWidget.h"

#include "Components/TextBlock.h"
#include "Engine/World.h"

void UBHObjectiveNotificationWidget::ShowNotification(
    const FText& Message)
{
    ClearNotificationTimers();

    if (!IsValid(NotificationText))
    {
        return;
    }

    NotificationText->SetText(Message);
    SetRenderOpacity(1.0f);
    SetVisibility(ESlateVisibility::Visible);

    UWorld* World = GetWorld();

    if (!IsValid(World))
    {
        return;
    }

    if (DisplayDuration <= 0.0f)
    {
        StartFadeOut();
        return;
    }

    World->GetTimerManager().SetTimer(
        DisplayTimerHandle,
        this,
        &UBHObjectiveNotificationWidget::StartFadeOut,
        DisplayDuration,
        false);
}

void UBHObjectiveNotificationWidget::StartFadeOut()
{
    UWorld* World = GetWorld();

    if (!IsValid(World) || FadeDuration <= KINDA_SMALL_NUMBER)
    {
        HideNotification();
        return;
    }

    World->GetTimerManager().ClearTimer(DisplayTimerHandle);
    FadeStartTime = World->GetTimeSeconds();

    World->GetTimerManager().SetTimer(
        FadeTimerHandle,
        this,
        &UBHObjectiveNotificationWidget::UpdateFadeOut,
        FadeUpdateInterval,
        true);
}

void UBHObjectiveNotificationWidget::UpdateFadeOut()
{
    UWorld* World = GetWorld();

    if (!IsValid(World))
    {
        HideNotification();
        return;
    }

    const float ElapsedTime = World->GetTimeSeconds() - FadeStartTime;
    const float FadeAlpha = FMath::Clamp(
        ElapsedTime / FadeDuration,
        0.0f,
        1.0f);

    SetRenderOpacity(1.0f - FadeAlpha);

    if (FadeAlpha >= 1.0f)
    {
        HideNotification();
    }
}

void UBHObjectiveNotificationWidget::HideNotification()
{
    ClearNotificationTimers();
    SetVisibility(ESlateVisibility::Collapsed);
    SetRenderOpacity(1.0f);
}

void UBHObjectiveNotificationWidget::ClearNotificationTimers()
{
    UWorld* World = GetWorld();

    if (!IsValid(World))
    {
        return;
    }

    World->GetTimerManager().ClearTimer(DisplayTimerHandle);
    World->GetTimerManager().ClearTimer(FadeTimerHandle);
}

void UBHObjectiveNotificationWidget::NativeDestruct()
{
    ClearNotificationTimers();
    Super::NativeDestruct();
}
