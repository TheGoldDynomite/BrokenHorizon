#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TimerManager.h"
#include "BHObjectiveNotificationWidget.generated.h"

class UTextBlock;
class USoundBase;

UENUM(BlueprintType)
enum class EBHNotificationPriority : uint8
{
    Normal,
    High,
    Critical
};

UENUM(BlueprintType)
enum class EBHNotificationAudioCue : uint8
{
    None,
    QuietConfirmation,
    StrategicWarning,
    CombatAlarm
};

UCLASS()
class BROKENHORIZON_API UBHObjectiveNotificationWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    explicit UBHObjectiveNotificationWidget(
        const FObjectInitializer& ObjectInitializer
    );

    UFUNCTION(BlueprintCallable, Category = "Objectives")
    void ShowNotification(const FText& Message);

    UFUNCTION(BlueprintCallable, Category = "Objectives")
    void ShowPriorityNotification(
        const FText& Message,
        EBHNotificationPriority NotificationPriority
    );

    UFUNCTION(BlueprintCallable, Category = "Objectives")
    void ShowNotificationWithAudioCue(
        const FText& Message,
        EBHNotificationPriority NotificationPriority,
        EBHNotificationAudioCue AudioCue
    );

    UFUNCTION(BlueprintCallable, Category = "Objectives")
    void ShowDeferredStrategicNotification(const FText& Message);

    UFUNCTION(BlueprintCallable, Category = "Objectives")
    void SetCombatIntensityActive(bool bActive);

    UFUNCTION(BlueprintPure, Category = "Objectives")
    int32 GetPendingNotificationCount() const;

    UFUNCTION(BlueprintPure, Category = "Objectives")
    int32 GetPendingDeferredStrategicNotificationCount() const;

    static bool ShouldPreemptNotification(
        EBHNotificationPriority Incoming,
        EBHNotificationPriority Active
    );

    static EBHNotificationAudioCue ResolveDefaultAudioCue(
        EBHNotificationPriority NotificationPriority
    );

    static bool ShouldDeferNotification(
        bool bCombatActive,
        bool bDeferDuringCombat
    );

    static bool ShouldCoalesceDeferredStrategicNotification(
        bool bIncomingDeferredStrategic,
        bool bQueuedDeferredStrategic
    );

protected:
    virtual void NativeDestruct() override;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> NotificationText;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Objectives|Notification",
        meta = (ClampMin = "0.0"))
    float DisplayDuration = 2.5f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Objectives|Notification",
        meta = (ClampMin = "0.01"))
    float FadeDuration = 0.5f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Objectives|Notification",
        meta = (ClampMin = "0"))
    int32 MaxPendingNotifications = 6;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Objectives|Notification|Audio")
    TObjectPtr<USoundBase> QuietConfirmationSound;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Objectives|Notification|Audio")
    TObjectPtr<USoundBase> StrategicWarningSound;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Objectives|Notification|Audio")
    TObjectPtr<USoundBase> CombatAlarmSound;

private:
    struct FPendingNotification
    {
        FText Message;
        EBHNotificationPriority Priority =
            EBHNotificationPriority::Normal;
        EBHNotificationAudioCue AudioCue =
            EBHNotificationAudioCue::QuietConfirmation;
        bool bDeferDuringCombat = false;
    };

    void ShowNotificationInternal(
        const FText& Message,
        EBHNotificationPriority NotificationPriority,
        EBHNotificationAudioCue AudioCue,
        bool bDeferDuringCombat
    );

    void PresentNotification(
        const FText& Message,
        EBHNotificationPriority NotificationPriority,
        EBHNotificationAudioCue AudioCue
    );
    void QueueNotification(
        const FText& Message,
        EBHNotificationPriority NotificationPriority,
        EBHNotificationAudioCue AudioCue,
        bool bInsertAtFrontOfPriority = false,
        bool bDeferDuringCombat = false
    );
    void TryPresentNextQueuedNotification();
    void StartFadeOut();
    void UpdateFadeOut();
    void HideNotification();
    void ClearNotificationTimers();
    bool IsNotificationActive() const;
    void PlayNotificationAudio(EBHNotificationAudioCue AudioCue);

    FTimerHandle DisplayTimerHandle;
    FTimerHandle FadeTimerHandle;

    TArray<FPendingNotification> PendingNotifications;
    EBHNotificationPriority ActivePriority =
        EBHNotificationPriority::Normal;
    EBHNotificationAudioCue ActiveAudioCue =
        EBHNotificationAudioCue::QuietConfirmation;
    bool bNotificationInProgress = false;
    bool bCombatIntensityActive = false;
    float FadeStartTime = 0.0f;
    float FadeUpdateInterval = 0.02f;
};
