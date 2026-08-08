#include "BHInteractionPromptWidget.h"

#include "BHUIStyle.h"
#include "Components/TextBlock.h"

void UBHInteractionPromptWidget::SetInteractionText(const FText& NewText)
{
    BHUIStyle::Apply(*this, EBHUIStyleContext::Gameplay);

    if (InteractionText)
    {
        InteractionText->SetText(NewText);
    }
}
