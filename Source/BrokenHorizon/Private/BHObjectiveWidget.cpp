#include "BHObjectiveWidget.h"

#include "Components/TextBlock.h"


void UBHObjectiveWidget::SetObjectiveText(const FText& NewText)
{
    if (ObjectiveText)
    {
        ObjectiveText->SetText(NewText);
    }
}


void UBHObjectiveWidget::SetObjectiveList(
    const TArray<FText>& Completed,
    const FText& Current
)
{
    if (!ObjectiveText)
    {
        return;
    }

    FString DisplayText = TEXT("OBJECTIVES\n\n");

    for (const FText& Objective : Completed)
    {
        DisplayText += TEXT("✓ ");
        DisplayText += Objective.ToString();
        DisplayText += TEXT("\n");
    }

    if (!Current.IsEmpty())
    {
        DisplayText += TEXT("\n→ ");
        DisplayText += Current.ToString();
    }

    ObjectiveText->SetText(
        FText::FromString(DisplayText)
    );
}