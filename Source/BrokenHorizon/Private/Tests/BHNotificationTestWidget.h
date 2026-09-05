#pragma once

#include "BHObjectiveNotificationWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/TextBlock.h"
#include "BHNotificationTestWidget.generated.h"

// Native widget fixture: bind the real text control and isolate queue expiry
// from audio assets and cosmetic fading, without changing production APIs.
UCLASS(Transient)
class UBHNotificationTestWidget : public UBHObjectiveNotificationWidget
{
    GENERATED_BODY()

public:
    void InitializeFixture()
    {
        WidgetTree = NewObject<UWidgetTree>(this);
        NotificationText = WidgetTree->ConstructWidget<UTextBlock>(
            UTextBlock::StaticClass(), TEXT("NotificationText"));
        WidgetTree->RootWidget = NotificationText;
        DisplayDuration = 1.0f;
        FadeDuration = 0.0f;
        QuietConfirmationSound = nullptr;
        StrategicWarningSound = nullptr;
        CombatAlarmSound = nullptr;
        SetVisibility(ESlateVisibility::Collapsed);
    }

    FString DisplayedMessage() const
    {
        return NotificationText->GetText().ToString();
    }
};
