#include "BHNotificationTestWidget.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "TimerManager.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"

namespace
{
const FName BriefingSource(TEXT("MissionBriefing"));
const FName TacticalSource(TEXT("TacticalGuidance"));

struct FBHScopedNotificationWidget
{
    TStrongObjectPtr<UWorld> World;
    TStrongObjectPtr<UBHNotificationTestWidget> Widget;

    FBHScopedNotificationWidget()
    {
        UPackage* Package = nullptr;
        UWorld* NewWorld = nullptr;
        UGameInstance::CreateMinimalNetRPCWorld(
            MakeUniqueObjectName(nullptr, UPackage::StaticClass(),
                TEXT("/Temp/BHNotificationLifecycleTest")),
            Package, NewWorld);
        World.Reset(NewWorld);
        Widget.Reset(NewObject<UBHNotificationTestWidget>(NewWorld));
        Widget->InitializeFixture();
    }

    ~FBHScopedNotificationWidget()
    {
        World->GetTimerManager().ClearAllTimersForObject(Widget.Get());
        World->DestroyWorld(false);
    }
};

class FBHNotificationLifecycleCommand : public IAutomationLatentCommand
{
public:
    FBHNotificationLifecycleCommand(FAutomationTestBase& InTest, bool bInSuppression)
        : Test(InTest), bSuppression(bInSuppression)
    {
    }

    virtual bool Update() override
    {
        FTimerManager& Timers = Fixture.World->GetTimerManager();
        if (Timers.HasBeenTickedThisFrame())
        {
            return false;
        }
        // Advance only this unregistered world's timers once per automation
        // frame. No real-time sleeps, world simulation, or global frame edits.
        Timers.Tick(Step == 0 ? 0.0f : 2.0f);
        const bool bDone = bSuppression ? UpdateSuppression() : UpdateKeyed();
        ++Step;
        return bDone;
    }

private:
    void ShowKeyed(FName Source, const TCHAR* Message, EBHNotificationPriority Priority)
    {
        Fixture.Widget->ShowKeyedNotification(Source, FText::FromString(Message), Priority);
    }

    void ExpectMessage(const TCHAR* Label, const TCHAR* Expected)
    {
        Test.TestEqual(Label, Fixture.Widget->DisplayedMessage(), FString(Expected));
        Test.TestEqual(TEXT("A presented notification is visible"),
            Fixture.Widget->GetVisibility(), ESlateVisibility::Visible);
    }

    void ExpectNoBriefing()
    {
        Test.TestFalse(TEXT("Canceled briefing cannot return through preemption or expiry"),
            Fixture.Widget->HasNotificationForSource(BriefingSource));
    }

    bool UpdateKeyed()
    {
        UBHNotificationTestWidget* Widget = Fixture.Widget.Get();
        switch (Step)
        {
        case 0:
            ShowKeyed(BriefingSource, TEXT("Obsolete Korona briefing"), EBHNotificationPriority::Normal);
            ShowKeyed(BriefingSource, TEXT("Current Dovren briefing"), EBHNotificationPriority::Normal);
            ExpectMessage(TEXT("Replacing an active briefing presents the current location"), TEXT("Current Dovren briefing"));
            ShowKeyed(BriefingSource, TEXT("Current Dovren briefing"), EBHNotificationPriority::Normal);
            Test.TestEqual(TEXT("Identical keyed briefing is not queued again"), Widget->GetPendingNotificationCount(), 0);
            Widget->ShowNotification(FText::FromString(TEXT("Routine status")));
            Widget->ShowPriorityNotification(FText::FromString(TEXT("Critical danger")), EBHNotificationPriority::Critical);
            ShowKeyed(TacticalSource, TEXT("Hold the defensive line"), EBHNotificationPriority::High);
            Test.TestEqual(TEXT("Preemption retains briefing, tactical, and ordinary messages"), Widget->GetPendingNotificationCount(), 3);
            ShowKeyed(BriefingSource, TEXT("Replacement pending briefing"), EBHNotificationPriority::Normal);
            ExpectMessage(TEXT("Replacing a pending briefing never interrupts critical danger"), TEXT("Critical danger"));
            Test.TestEqual(TEXT("Pending replacement changes only one source"), Widget->GetPendingNotificationCount(), 3);
            Widget->CancelKeyedNotification(BriefingSource);
            ExpectNoBriefing();
            Test.TestTrue(TEXT("Cancel briefing preserves unrelated tactical source"), Widget->HasNotificationForSource(TacticalSource));
            Test.TestEqual(TEXT("Only briefing was removed from the queue"), Widget->GetPendingNotificationCount(), 2);
            Widget->CancelKeyedNotification(NAME_None);
            ExpectMessage(TEXT("Canceling None cannot discard an unkeyed critical notification"), TEXT("Critical danger"));
            return false;
        case 1:
            ExpectMessage(TEXT("Actual critical expiry advances to unrelated tactical guidance"), TEXT("Hold the defensive line"));
            ExpectNoBriefing();
            return false;
        case 2:
            ExpectMessage(TEXT("Actual tactical expiry retains the ordinary notification"), TEXT("Routine status"));
            ExpectNoBriefing();
            return false;
        case 3:
            Test.TestEqual(TEXT("Queue empties without replaying an obsolete briefing"), Widget->GetVisibility(), ESlateVisibility::Collapsed);
            Test.TestEqual(TEXT("No expired message remains pending"), Widget->GetPendingNotificationCount(), 0);
            ExpectNoBriefing();
            ShowKeyed(BriefingSource, TEXT("Cancel this active briefing"), EBHNotificationPriority::Normal);
            Widget->ShowNotification(FText::FromString(TEXT("Unrelated queued status")));
            Widget->CancelKeyedNotification(BriefingSource);
            ExpectMessage(TEXT("Canceling an active briefing advances the unrelated queued message"), TEXT("Unrelated queued status"));
            ExpectNoBriefing();
            return false;
        case 4:
            Test.TestEqual(TEXT("Unrelated message still expires normally"), Widget->GetVisibility(), ESlateVisibility::Collapsed);
            ShowKeyed(NAME_None, TEXT("Legacy unkeyed status"), EBHNotificationPriority::Normal);
            Widget->ShowNotification(FText::FromString(TEXT("Legacy unkeyed status")));
            Widget->CancelKeyedNotification(NAME_None);
            ExpectMessage(TEXT("None-source calls retain ordinary unkeyed presentation"), TEXT("Legacy unkeyed status"));
            Test.TestEqual(TEXT("Ordinary unkeyed duplicate remains deduplicated"), Widget->GetPendingNotificationCount(), 0);
            return false;
        case 5:
            Test.TestEqual(TEXT("Unkeyed fallback retains normal timer expiry"), Widget->GetVisibility(), ESlateVisibility::Collapsed);
            ShowKeyed(BriefingSource, TEXT("High-priority briefing"), EBHNotificationPriority::High);
            Widget->ShowPriorityNotification(FText::FromString(TEXT("Unrelated high-priority warning")), EBHNotificationPriority::High);
            ShowKeyed(BriefingSource, TEXT("Lower-priority replacement"), EBHNotificationPriority::Normal);
            ExpectMessage(TEXT("Lowering a keyed priority releases the queued higher-priority message first"), TEXT("Unrelated high-priority warning"));
            Test.TestEqual(TEXT("Lower-priority replacement remains queued"), Widget->GetPendingNotificationCount(), 1);
            return false;
        case 6:
            ExpectMessage(TEXT("Replacement follows the preserved higher-priority message"), TEXT("Lower-priority replacement"));
            return false;
        case 7:
            Test.TestEqual(TEXT("Replacement expires after its normal duration"), Widget->GetVisibility(), ESlateVisibility::Collapsed);
            Widget->ShowDeferredStrategicNotification(FText::FromString(TEXT("Obsolete strategic update")));
            Widget->SetCombatIntensityActive(true);
            Widget->ShowDeferredStrategicNotification(FText::FromString(TEXT("Newest strategic update")));
            Widget->ShowPriorityNotification(FText::FromString(TEXT("Critical during strategic update")), EBHNotificationPriority::Critical);
            ExpectMessage(TEXT("Critical event still preempts active background strategy"), TEXT("Critical during strategic update"));
            Test.TestEqual(TEXT("Preemption keeps one latest deferred strategic update"), Widget->GetPendingDeferredStrategicNotificationCount(), 1);
            return false;
        case 8:
            Test.TestEqual(TEXT("Background strategy remains deferred during combat after critical expiry"), Widget->GetVisibility(), ESlateVisibility::Collapsed);
            Widget->SetCombatIntensityActive(false);
            ExpectMessage(TEXT("Preempted old strategy cannot overwrite the newer queued strategy"), TEXT("Newest strategic update"));
            return false;
        default:
            Test.TestEqual(TEXT("Latest strategic update expires without replaying old content"), Widget->GetVisibility(), ESlateVisibility::Collapsed);
            Test.TestEqual(TEXT("All keyed and unkeyed queues finish empty"), Widget->GetPendingNotificationCount(), 0);
            return true;
        }
    }

    bool UpdateSuppression()
    {
        UBHNotificationTestWidget* Widget = Fixture.Widget.Get();
        switch (Step)
        {
        case 0:
            ShowKeyed(TacticalSource, TEXT("Retained tactical guidance"), EBHNotificationPriority::High);
            ShowKeyed(BriefingSource, TEXT("Old briefing"), EBHNotificationPriority::Normal);
            Widget->SetPresentationSuppressed(true);
            Test.TestTrue(TEXT("Terminal presentation enables suppression"), Widget->IsPresentationSuppressed());
            Widget->ShowPriorityNotification(FText::FromString(TEXT("Critical while suppressed")), EBHNotificationPriority::Critical);
            Widget->ShowNotification(FText::FromString(TEXT("Routine while suppressed")));
            ShowKeyed(BriefingSource, TEXT("Updated while suppressed"), EBHNotificationPriority::Normal);
            Widget->CancelKeyedNotification(BriefingSource);
            Widget->SetCombatIntensityActive(true);
            Widget->ShowDeferredStrategicNotification(FText::FromString(TEXT("Deferred strategic status")));
            Widget->SetCombatIntensityActive(false);
            Test.TestEqual(TEXT("Suppressed arrivals and combat release cannot reopen the overlay"), Widget->GetVisibility(), ESlateVisibility::Collapsed);
            Test.TestEqual(TEXT("Suppression retains all three unrelated pending messages"), Widget->GetPendingNotificationCount(), 3);
            ExpectNoBriefing();
            return false;
        case 1:
            Test.TestEqual(TEXT("Elapsed display duration cannot reopen a suppressed result overlay"), Widget->GetVisibility(), ESlateVisibility::Collapsed);
            Test.TestEqual(TEXT("Suppressed timers cannot advance or consume the pending queue"), Widget->GetPendingNotificationCount(), 3);
            Test.TestTrue(TEXT("Suppression retains the original active tactical source"), Widget->HasNotificationForSource(TacticalSource));
            Widget->SetPresentationSuppressed(false);
            Test.TestFalse(TEXT("Presentation can leave terminal suppression"), Widget->IsPresentationSuppressed());
            ExpectMessage(TEXT("Release honors a higher-priority pending critical notification"), TEXT("Critical while suppressed"));
            Test.TestEqual(TEXT("Preemption on release preserves the saved active message"), Widget->GetPendingNotificationCount(), 3);
            return false;
        case 2:
            ExpectMessage(TEXT("Critical expiry restores the retained active message"), TEXT("Retained tactical guidance"));
            ExpectNoBriefing();
            return false;
        case 3:
            ExpectMessage(TEXT("Release preserves ordinary pending FIFO order"), TEXT("Routine while suppressed"));
            return false;
        case 4:
            ExpectMessage(TEXT("Eligible deferred strategy follows earlier equal-priority status"), TEXT("Deferred strategic status"));
            return false;
        case 5:
            Test.TestEqual(TEXT("Preserved queue drains completely after release"), Widget->GetVisibility(), ESlateVisibility::Collapsed);
            ShowKeyed(TacticalSource, TEXT("Resume original active message"), EBHNotificationPriority::High);
            Widget->ShowNotification(FText::FromString(TEXT("Pending after active cancellation")));
            Widget->SetPresentationSuppressed(true);
            Widget->SetPresentationSuppressed(false);
            ExpectMessage(TEXT("Without a higher priority, release resumes the original active message"), TEXT("Resume original active message"));
            Widget->SetPresentationSuppressed(true);
            Widget->CancelKeyedNotification(TacticalSource);
            Widget->SetCombatIntensityActive(true);
            Widget->ShowDeferredStrategicNotification(FText::FromString(TEXT("Strategy after active cancellation")));
            Widget->SetCombatIntensityActive(false);
            Test.TestEqual(TEXT("Canceling active content and combat release stay suppressed"), Widget->GetVisibility(), ESlateVisibility::Collapsed);
            Test.TestEqual(TEXT("Suppressed active cancellation preserves unrelated pending messages"), Widget->GetPendingNotificationCount(), 2);
            return false;
        case 6:
            Test.TestEqual(TEXT("An empty active slot cannot auto-present while suppressed"), Widget->GetVisibility(), ESlateVisibility::Collapsed);
            Widget->SetPresentationSuppressed(false);
            ExpectMessage(TEXT("Release presents the next eligible message after active cancellation"), TEXT("Pending after active cancellation"));
            return false;
        case 7:
            ExpectMessage(TEXT("Remaining deferred content is preserved after active cancellation"), TEXT("Strategy after active cancellation"));
            return false;
        case 8:
            Test.TestEqual(TEXT("Released queue ends collapsed"), Widget->GetVisibility(), ESlateVisibility::Collapsed);
            Test.TestEqual(TEXT("Released queue is empty"), Widget->GetPendingNotificationCount(), 0);
            ExpectNoBriefing();
            Widget->ShowNotification(FText::FromString(TEXT("Legacy suppressed duplicate")));
            Widget->SetPresentationSuppressed(true);
            Widget->ShowNotification(FText::FromString(TEXT("Legacy suppressed duplicate")));
            ShowKeyed(NAME_None, TEXT("Legacy suppressed duplicate"), EBHNotificationPriority::Normal);
            Test.TestEqual(TEXT("Suppression preserves active unkeyed duplicate rejection"), Widget->GetPendingNotificationCount(), 0);
            Widget->SetPresentationSuppressed(false);
            ExpectMessage(TEXT("Unkeyed active message resumes once"), TEXT("Legacy suppressed duplicate"));
            return false;
        case 9:
            Test.TestEqual(TEXT("Suppressed duplicate is not replayed after expiry"), Widget->GetVisibility(), ESlateVisibility::Collapsed);
            Widget->ShowDeferredStrategicNotification(FText::FromString(TEXT("Old strategy before suppression")));
            Widget->SetCombatIntensityActive(true);
            Widget->ShowDeferredStrategicNotification(FText::FromString(TEXT("Newest strategy after suppression")));
            Widget->SetPresentationSuppressed(true);
            Widget->ShowPriorityNotification(FText::FromString(TEXT("Critical queued during suppression")), EBHNotificationPriority::Critical);
            Widget->SetCombatIntensityActive(false);
            Widget->SetPresentationSuppressed(false);
            ExpectMessage(TEXT("Release preempts saved background strategy for pending critical content"), TEXT("Critical queued during suppression"));
            Test.TestEqual(TEXT("Release preserves the latest deferred strategic update"), Widget->GetPendingDeferredStrategicNotificationCount(), 1);
            return false;
        case 10:
            ExpectMessage(TEXT("Resuming old strategy cannot discard newer deferred data"), TEXT("Newest strategy after suppression"));
            return false;
        default:
            Test.TestEqual(TEXT("Suppression lifecycle finishes collapsed without obsolete replay"), Widget->GetVisibility(), ESlateVisibility::Collapsed);
            Test.TestEqual(TEXT("Suppression lifecycle leaves no pending content"), Widget->GetPendingNotificationCount(), 0);
            return true;
        }
    }

    FAutomationTestBase& Test;
    FBHScopedNotificationWidget Fixture;
    bool bSuppression = false;
    int32 Step = 0;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBHNotificationKeyedLifecycleTest,
    "BrokenHorizon.UI.Notification.KeyedLifecycle",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FBHNotificationKeyedLifecycleTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    ADD_LATENT_AUTOMATION_COMMAND(FBHNotificationLifecycleCommand(*this, false));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBHNotificationSuppressionLifecycleTest,
    "BrokenHorizon.UI.Notification.SuppressionLifecycle",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FBHNotificationSuppressionLifecycleTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    ADD_LATENT_AUTOMATION_COMMAND(FBHNotificationLifecycleCommand(*this, true));
    return true;
}
