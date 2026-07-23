#include "BHInteractionPromptWidget.h"

#include "Components/TextBlock.h"

void UBHInteractionPromptWidget::SetInteractionText(const FText& NewText)
{
    if (InteractionText)
    {
        InteractionText->SetText(NewText);
    }
}