#include "BHObjectiveWidget.h"

#include "Components/TextBlock.h"

void UBHObjectiveWidget::SetObjectiveText(const FText& NewText)
{
    if (ObjectiveText)
    {
        ObjectiveText->SetText(NewText);
    }
}