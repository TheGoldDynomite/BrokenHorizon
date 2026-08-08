#include "BHObjectiveWidget.h"

#include "BHUIStyle.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"

namespace
{
void ApplyCompactObjectiveLayout(UBHObjectiveWidget& Widget, UTextBlock* Text)
{
    if (!IsValid(Text))
    {
        return;
    }

    Text->SetAutoWrapText(true);
    Text->SetWrapTextAt(420.0f);
    Text->SetJustification(ETextJustify::Left);
    FSlateFontInfo Font = Text->GetFont();
    Font.Size = 16;
    Text->SetFont(Font);
    if (UCanvasPanelSlot* TextSlot =
            Cast<UCanvasPanelSlot>(Text->Slot))
    {
        TextSlot->SetPosition(FVector2D(16.0f, 12.0f));
        TextSlot->SetSize(FVector2D(388.0f, 112.0f));
    }

    if (IsValid(Widget.WidgetTree))
    {
        Widget.WidgetTree->ForEachWidget([](UWidget* Child)
        {
            if (UBorder* Border = Cast<UBorder>(Child))
            {
                FLinearColor Color = Border->GetBrushColor();
                Color.A = FMath::Min(Color.A, 0.58f);
                Border->SetBrushColor(Color);
                Border->SetPadding(FMargin(16.0f, 12.0f));
                if (UCanvasPanelSlot* BorderSlot =
                        Cast<UCanvasPanelSlot>(Border->Slot))
                {
                    BorderSlot->SetSize(FVector2D(420.0f, 140.0f));
                }
            }
            else if (USizeBox* SizeBox = Cast<USizeBox>(Child))
            {
                SizeBox->SetWidthOverride(420.0f);
                SizeBox->SetHeightOverride(140.0f);
            }
        });
    }
}
}

void UBHObjectiveWidget::NativeConstruct()
{
    Super::NativeConstruct();

    BHUIStyle::Apply(*this, EBHUIStyleContext::Gameplay);
    ApplyCompactObjectiveLayout(*this, ObjectiveText);
}


void UBHObjectiveWidget::SetObjectiveText(const FText& NewText)
{
    BHUIStyle::Apply(*this, EBHUIStyleContext::Gameplay);

    if (ObjectiveText)
    {
        ObjectiveText->SetText(NewText);
        ApplyCompactObjectiveLayout(*this, ObjectiveText);
    }
}


void UBHObjectiveWidget::SetObjectiveList(
    const TArray<FText>& Completed,
    const FText& Current
)
{
    BHUIStyle::Apply(*this, EBHUIStyleContext::Gameplay);

    if (!ObjectiveText)
    {
        return;
    }

    FString DisplayText = TEXT("OBJECTIVE\n");

    for (const FText& Objective : Completed)
    {
        DisplayText += TEXT("✓ ");
        DisplayText += Objective.ToString();
        DisplayText += TEXT("\n");
    }

    if (!Current.IsEmpty())
    {
        DisplayText += TEXT("→ ");
        DisplayText += Current.ToString();
    }

    ObjectiveText->SetText(
        FText::FromString(DisplayText)
    );
    ApplyCompactObjectiveLayout(*this, ObjectiveText);
}
