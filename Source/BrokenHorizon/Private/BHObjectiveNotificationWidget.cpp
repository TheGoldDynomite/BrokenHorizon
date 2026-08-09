#include "BHObjectiveNotificationWidget.h"

#include "BHUIStyle.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Sound/SoundBase.h"
#include "UObject/ConstructorHelpers.h"

UBHObjectiveNotificationWidget::UBHObjectiveNotificationWidget(
    const FObjectInitializer& ObjectInitializer
)
    : Super(ObjectInitializer)
{
    const ConstructorHelpers::FObjectFinder<USoundBase> ConfirmationAsset(
        TEXT("/Game/BrokenHorizon/Audio/SW_FirstLight_UIConfirm.SW_FirstLight_UIConfirm")
    );
    if (ConfirmationAsset.Succeeded())
    {
        QuietConfirmationSound = ConfirmationAsset.Object;
    }
    const ConstructorHelpers::FObjectFinder<USoundBase> WarningAsset(
        TEXT("/Game/BrokenHorizon/Audio/SW_FirstLight_UIWarning.SW_FirstLight_UIWarning")
    );
    if (WarningAsset.Succeeded())
    {
        StrategicWarningSound = WarningAsset.Object;
    }
    const ConstructorHelpers::FObjectFinder<USoundBase> AlarmAsset(
        TEXT("/Game/BrokenHorizon/Audio/SW_FirstLight_UIAlarm.SW_FirstLight_UIAlarm")
    );
    if (AlarmAsset.Succeeded())
    {
        CombatAlarmSound = AlarmAsset.Object;
    }
}

void UBHObjectiveNotificationWidget::ShowNotification(
    const FText& Message)
{
    ShowPriorityNotification(
        Message,
        EBHNotificationPriority::Normal
    );
}

void UBHObjectiveNotificationWidget::ShowPriorityNotification(
    const FText& Message,
    EBHNotificationPriority NotificationPriority
)
{
    ShowNotificationWithAudioCue(
        Message,
        NotificationPriority,
        ResolveDefaultAudioCue(NotificationPriority)
    );
}

void UBHObjectiveNotificationWidget::ShowNotificationWithAudioCue(
    const FText& Message,
    EBHNotificationPriority NotificationPriority,
    EBHNotificationAudioCue AudioCue
)
{
    ShowNotificationInternal(
        Message,
        NotificationPriority,
        AudioCue,
        false
    );
}

void UBHObjectiveNotificationWidget::ShowDeferredStrategicNotification(
    const FText& Message
)
{
    ShowNotificationInternal(
        Message,
        EBHNotificationPriority::Normal,
        EBHNotificationAudioCue::StrategicWarning,
        true
    );
}

void UBHObjectiveNotificationWidget::ShowNotificationInternal(
    const FText& Message,
    EBHNotificationPriority NotificationPriority,
    EBHNotificationAudioCue AudioCue,
    bool bDeferDuringCombat
)
{
    if (Message.IsEmpty())
    {
        return;
    }

    if (ShouldDeferNotification(
            bCombatIntensityActive,
            bDeferDuringCombat))
    {
        const bool bAlreadyQueued =
            PendingNotifications.ContainsByPredicate(
                [&Message](const FPendingNotification& Queued)
                {
                    return Queued.Message.EqualTo(Message);
                }
            );
        if (!bAlreadyQueued && MaxPendingNotifications > 0)
        {
            QueueNotification(
                Message,
                NotificationPriority,
                AudioCue,
                false,
                true
            );
            UE_LOG(
                LogTemp,
                Verbose,
                TEXT("BH_NOTIFICATION_COMBAT_DEFERRED pending=%d"),
                PendingNotifications.Num()
            );
        }
        return;
    }

    if (!IsValid(NotificationText))
    {
        return;
    }

    if (IsNotificationActive())
    {
        const bool bMatchesCurrent =
            NotificationText->GetText().EqualTo(Message);
        const bool bAlreadyQueued =
            PendingNotifications.ContainsByPredicate(
                [&Message](
                    const FPendingNotification& QueuedNotification
                )
                {
                    return QueuedNotification.Message.EqualTo(Message);
                }
            );

        if (bMatchesCurrent || bAlreadyQueued)
        {
            return;
        }

        if (ShouldPreemptNotification(
                NotificationPriority,
                ActivePriority))
        {
            if (MaxPendingNotifications > 0)
            {
                QueueNotification(
                    NotificationText->GetText(),
                    ActivePriority,
                    ActiveAudioCue,
                    true,
                    false
                );
            }
            PresentNotification(Message, NotificationPriority, AudioCue);
            UE_LOG(
                LogTemp,
                Verbose,
                TEXT("BH_NOTIFICATION_PREEMPTED priority=%d"),
                static_cast<int32>(NotificationPriority)
            );
            return;
        }

        if (MaxPendingNotifications <= 0)
        {
            return;
        }

        QueueNotification(
            Message,
            NotificationPriority,
            AudioCue,
            false,
            bDeferDuringCombat
        );
        UE_LOG(
            LogTemp,
            Verbose,
            TEXT("BH_NOTIFICATION_QUEUED pending=%d"),
            PendingNotifications.Num()
        );
        return;
    }

    PresentNotification(Message, NotificationPriority, AudioCue);
}

void UBHObjectiveNotificationWidget::SetCombatIntensityActive(
    bool bActive
)
{
    if (bCombatIntensityActive == bActive)
    {
        return;
    }

    bCombatIntensityActive = bActive;
    if (!bCombatIntensityActive && !IsNotificationActive())
    {
        TryPresentNextQueuedNotification();
    }
}

int32
UBHObjectiveNotificationWidget::GetPendingNotificationCount() const
{
    return PendingNotifications.Num();
}

int32 UBHObjectiveNotificationWidget::
GetPendingDeferredStrategicNotificationCount() const
{
    int32 DeferredCount = 0;
    for (const FPendingNotification& Pending : PendingNotifications)
    {
        if (Pending.bDeferDuringCombat)
        {
            ++DeferredCount;
        }
    }
    return DeferredCount;
}

void UBHObjectiveNotificationWidget::PresentNotification(
    const FText& Message,
    EBHNotificationPriority NotificationPriority,
    EBHNotificationAudioCue AudioCue
)
{
    ClearNotificationTimers();

    if (Message.IsEmpty() || !IsValid(NotificationText))
    {
        return;
    }

    const FVector2D ViewportSize =
        UWidgetLayoutLibrary::GetViewportSize(this);
    const float ResponsiveWrapWidth = FMath::Clamp(
        ViewportSize.X * 0.62f,
        440.0f,
        900.0f
    );
    if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(
            NotificationText->Slot))
    {
        CanvasSlot->SetAnchors(FAnchors(0.5f, 0.5f));
        CanvasSlot->SetAlignment(FVector2D(0.5f, 0.5f));
        CanvasSlot->SetPosition(FVector2D::ZeroVector);
        CanvasSlot->SetSize(FVector2D(
            ResponsiveWrapWidth,
            FMath::Clamp(
                ViewportSize.Y * 0.68f,
                320.0f,
                640.0f
            )
        ));
        CanvasSlot->SetAutoSize(false);
    }
    NotificationText->SetAutoWrapText(true);
    NotificationText->SetMinDesiredWidth(ResponsiveWrapWidth);
    NotificationText->SetWrapTextAt(
        FMath::Max(320.0f, ResponsiveWrapWidth - 48.0f)
    );
    NotificationText->SetJustification(ETextJustify::Center);
    NotificationText->SetText(Message);
    ActivePriority = NotificationPriority;
    ActiveAudioCue = AudioCue;
    BHUIStyle::Apply(*this, EBHUIStyleContext::Overlay);
    FSlateFontInfo NotificationFont = NotificationText->GetFont();
    const int32 MessageLength = Message.ToString().Len();
    if (MessageLength >= 120)
    {
        const int32 LongFormFontSize = ViewportSize.Y < 900.0f
            ? 20
            : 24;
        NotificationFont.Size = FMath::Min(
            NotificationFont.Size,
            LongFormFontSize
        );
        NotificationText->SetLineHeightPercentage(0.9f);
    }
    else
    {
        NotificationText->SetLineHeightPercentage(1.0f);
    }
    NotificationFont.OutlineSettings.OutlineSize = 2;
    NotificationFont.OutlineSettings.OutlineColor = FLinearColor::Black;
    NotificationText->SetFont(NotificationFont);
    bNotificationInProgress = true;
    SetRenderOpacity(1.0f);
    SetVisibility(ESlateVisibility::Visible);
    PlayNotificationAudio(AudioCue);

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
    bNotificationInProgress = false;
    ActivePriority = EBHNotificationPriority::Normal;
    ActiveAudioCue = EBHNotificationAudioCue::QuietConfirmation;
    SetVisibility(ESlateVisibility::Collapsed);
    SetRenderOpacity(1.0f);

    TryPresentNextQueuedNotification();
}

void UBHObjectiveNotificationWidget::QueueNotification(
    const FText& Message,
    EBHNotificationPriority NotificationPriority,
    EBHNotificationAudioCue AudioCue,
    bool bInsertAtFrontOfPriority,
    bool bDeferDuringCombat
)
{
    if (Message.IsEmpty() || MaxPendingNotifications <= 0)
    {
        return;
    }

    int32 CoalescedStrategicUpdates = 0;
    if (bDeferDuringCombat)
    {
        CoalescedStrategicUpdates = PendingNotifications.RemoveAll(
            [bDeferDuringCombat](
                const FPendingNotification& QueuedNotification
            )
            {
                return ShouldCoalesceDeferredStrategicNotification(
                    bDeferDuringCombat,
                    QueuedNotification.bDeferDuringCombat
                );
            }
        );
    }

    int32 InsertIndex = PendingNotifications.Num();
    for (int32 Index = 0;
        Index < PendingNotifications.Num();
        ++Index)
    {
        const EBHNotificationPriority ExistingPriority =
            PendingNotifications[Index].Priority;
        if (static_cast<uint8>(NotificationPriority) >
                static_cast<uint8>(ExistingPriority) ||
            (bInsertAtFrontOfPriority &&
                NotificationPriority == ExistingPriority))
        {
            InsertIndex = Index;
            break;
        }
    }

    FPendingNotification Notification;
    Notification.Message = Message;
    Notification.Priority = NotificationPriority;
    Notification.AudioCue = AudioCue;
    Notification.bDeferDuringCombat = bDeferDuringCombat;
    PendingNotifications.Insert(Notification, InsertIndex);

    if (CoalescedStrategicUpdates > 0)
    {
        UE_LOG(
            LogTemp,
            Verbose,
            TEXT(
                "BH_NOTIFICATION_STRATEGIC_COALESCED replaced=%d "
                "pending=%d"
            ),
            CoalescedStrategicUpdates,
            PendingNotifications.Num()
        );
    }

    while (PendingNotifications.Num() > MaxPendingNotifications)
    {
        PendingNotifications.RemoveAt(
            PendingNotifications.Num() - 1
        );
    }
}

void UBHObjectiveNotificationWidget::TryPresentNextQueuedNotification()
{
    const int32 NextIndex = PendingNotifications.IndexOfByPredicate(
        [this](const FPendingNotification& Pending)
        {
            return !ShouldDeferNotification(
                bCombatIntensityActive,
                Pending.bDeferDuringCombat
            );
        }
    );
    if (NextIndex == INDEX_NONE)
    {
        return;
    }

    const FPendingNotification NextNotification =
        PendingNotifications[NextIndex];
    PendingNotifications.RemoveAt(NextIndex);
    PresentNotification(
        NextNotification.Message,
        NextNotification.Priority,
        NextNotification.AudioCue
    );
}

EBHNotificationAudioCue
UBHObjectiveNotificationWidget::ResolveDefaultAudioCue(
    EBHNotificationPriority NotificationPriority
)
{
    switch (NotificationPriority)
    {
        case EBHNotificationPriority::Critical:
            return EBHNotificationAudioCue::CombatAlarm;
        case EBHNotificationPriority::High:
            return EBHNotificationAudioCue::StrategicWarning;
        default:
            return EBHNotificationAudioCue::QuietConfirmation;
    }
}

void UBHObjectiveNotificationWidget::PlayNotificationAudio(
    EBHNotificationAudioCue AudioCue
)
{
    USoundBase* Sound = nullptr;
    switch (AudioCue)
    {
        case EBHNotificationAudioCue::QuietConfirmation:
            Sound = QuietConfirmationSound;
            break;
        case EBHNotificationAudioCue::StrategicWarning:
            Sound = StrategicWarningSound;
            break;
        case EBHNotificationAudioCue::CombatAlarm:
            Sound = CombatAlarmSound;
            break;
        default:
            return;
    }
    if (IsValid(Sound))
    {
        UGameplayStatics::PlaySound2D(this, Sound);
    }
}

bool UBHObjectiveNotificationWidget::ShouldPreemptNotification(
    EBHNotificationPriority Incoming,
    EBHNotificationPriority Active
)
{
    return static_cast<uint8>(Incoming) >
        static_cast<uint8>(Active);
}

bool UBHObjectiveNotificationWidget::ShouldDeferNotification(
    bool bCombatActive,
    bool bDeferDuringCombat
)
{
    return bCombatActive && bDeferDuringCombat;
}

bool UBHObjectiveNotificationWidget::
ShouldCoalesceDeferredStrategicNotification(
    bool bIncomingDeferredStrategic,
    bool bQueuedDeferredStrategic
)
{
    // Background war-state updates supersede older background snapshots.
    // Tactical and operation notifications are never coalesced here.
    return bIncomingDeferredStrategic && bQueuedDeferredStrategic;
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

bool UBHObjectiveNotificationWidget::IsNotificationActive() const
{
    return bNotificationInProgress &&
        IsValid(NotificationText) &&
        !NotificationText->GetText().IsEmpty();
}

void UBHObjectiveNotificationWidget::NativeDestruct()
{
    ClearNotificationTimers();
    PendingNotifications.Reset();
    bNotificationInProgress = false;
    bCombatIntensityActive = false;
    ActivePriority = EBHNotificationPriority::Normal;
    ActiveAudioCue = EBHNotificationAudioCue::QuietConfirmation;
    Super::NativeDestruct();
}
