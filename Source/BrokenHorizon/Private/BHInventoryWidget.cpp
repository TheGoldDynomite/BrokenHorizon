#include "BHInventoryWidget.h"

#include "BHCharacter.h"
#include "BHSalvagePickup.h"
#include "BHUIStyle.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"

void UBHInventoryWidget::InitializeInventory(ABHCharacter* InCharacter)
{
    OwningCharacter = InCharacter;
}

void UBHInventoryWidget::NativeConstruct()
{
    Super::NativeConstruct();

    if (IsValid(WidgetTree) && !IsValid(InventoryText))
    {
        UCanvasPanel* RootCanvas = Cast<UCanvasPanel>(
            WidgetTree->RootWidget
        );
        if (!IsValid(RootCanvas))
        {
            RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(
                UCanvasPanel::StaticClass(),
                TEXT("InventoryRoot")
            );
            WidgetTree->RootWidget = RootCanvas;
        }

        UBorder* Backdrop = WidgetTree->ConstructWidget<UBorder>(
            UBorder::StaticClass(),
            TEXT("InventoryBackdrop")
        );
        Backdrop->SetBrushColor(FLinearColor(0.008f, 0.012f, 0.014f, 0.92f));
        if (UCanvasPanelSlot* BackdropSlot =
                RootCanvas->AddChildToCanvas(Backdrop))
        {
            BackdropSlot->SetAnchors(FAnchors(0.18f, 0.16f, 0.82f, 0.84f));
            BackdropSlot->SetOffsets(FMargin(0.0f));
        }

        InventoryText = WidgetTree->ConstructWidget<UTextBlock>(
            UTextBlock::StaticClass(),
            TEXT("InventoryText")
        );
        InventoryText->SetColorAndOpacity(FSlateColor(
            FLinearColor(0.82f, 0.88f, 0.84f, 1.0f)
        ));
        InventoryText->SetJustification(ETextJustify::Left);
        if (UCanvasPanelSlot* TextSlot =
                RootCanvas->AddChildToCanvas(InventoryText))
        {
            TextSlot->SetAnchors(FAnchors(0.22f, 0.20f, 0.78f, 0.80f));
            TextSlot->SetOffsets(FMargin(0.0f));
        }

        CycleRoleButton = WidgetTree->ConstructWidget<UButton>(
            UButton::StaticClass(),
            TEXT("CycleRoleButton")
        );
        UTextBlock* CycleRoleLabel = WidgetTree->ConstructWidget<UTextBlock>(
            UTextBlock::StaticClass(),
            TEXT("CycleRoleLabel")
        );
        CycleRoleLabel->SetText(FText::FromString(TEXT("CYCLE PRIMARY ROLE")));
        CycleRoleLabel->SetJustification(ETextJustify::Center);
        CycleRoleButton->SetContent(CycleRoleLabel);
        if (UCanvasPanelSlot* ButtonSlot =
                RootCanvas->AddChildToCanvas(CycleRoleButton))
        {
            ButtonSlot->SetAnchors(FAnchors(0.22f, 0.72f, 0.48f, 0.78f));
            ButtonSlot->SetOffsets(FMargin(0.0f));
        }

        DropFragButton = WidgetTree->ConstructWidget<UButton>(
            UButton::StaticClass(),
            TEXT("DropFragButton")
        );
        UTextBlock* DropFragLabel = WidgetTree->ConstructWidget<UTextBlock>(
            UTextBlock::StaticClass(),
            TEXT("DropFragLabel")
        );
        DropFragLabel->SetText(FText::FromString(TEXT("DISCARD ONE FRAG")));
        DropFragLabel->SetJustification(ETextJustify::Center);
        DropFragButton->SetContent(DropFragLabel);
        if (UCanvasPanelSlot* DropSlot =
                RootCanvas->AddChildToCanvas(DropFragButton))
        {
            DropSlot->SetAnchors(FAnchors(0.52f, 0.72f, 0.78f, 0.78f));
            DropSlot->SetOffsets(FMargin(0.0f));
        }

        auto AddDropButton = [this, RootCanvas](
            const TCHAR* Name,
            const TCHAR* Label,
            float Left,
            float Top
        ) -> UButton*
        {
            UButton* Button = WidgetTree->ConstructWidget<UButton>(
                UButton::StaticClass(), Name);
            UTextBlock* ButtonLabel = WidgetTree->ConstructWidget<UTextBlock>(
                UTextBlock::StaticClass());
            ButtonLabel->SetText(FText::FromString(Label));
            ButtonLabel->SetJustification(ETextJustify::Center);
            Button->SetContent(ButtonLabel);
            if (UCanvasPanelSlot* Slot = RootCanvas->AddChildToCanvas(Button))
            {
                Slot->SetAnchors(FAnchors(Left, Top, Left + 0.26f, Top + 0.06f));
                Slot->SetOffsets(FMargin(0.0f));
            }
            return Button;
        };
        DropSmokeButton = AddDropButton(
            TEXT("DropSmokeButton"), TEXT("DISCARD ONE SMOKE"), 0.22f, 0.80f);
        DropEngineeringButton = AddDropButton(
            TEXT("DropEngineeringButton"), TEXT("DISCARD ONE TOOL"), 0.50f, 0.80f);
        DropAntiVehicleButton = AddDropButton(
            TEXT("DropAntiVehicleButton"), TEXT("DISCARD ONE AT"), 0.22f, 0.87f);
        DropAmmoButton = AddDropButton(
            TEXT("DropAmmoButton"), TEXT("DISCARD 30 AMMO"), 0.50f, 0.87f);
        TransferFragButton = AddDropButton(
            TEXT("TransferFragButton"), TEXT("TRANSFER FRAG TO NEAREST ALLY"), 0.22f, 0.94f);
    }

    SetVisibility(ESlateVisibility::Collapsed);
    if (IsValid(CycleRoleButton))
    {
        CycleRoleButton->OnClicked.AddUniqueDynamic(
            this,
            &UBHInventoryWidget::HandleCycleRoleClicked
        );
    }
    if (IsValid(DropFragButton))
    {
        DropFragButton->OnClicked.AddUniqueDynamic(
            this,
            &UBHInventoryWidget::HandleDropFragClicked
        );
    }
    if (IsValid(DropSmokeButton))
    {
        DropSmokeButton->OnClicked.AddUniqueDynamic(
            this, &UBHInventoryWidget::HandleDropSmokeClicked);
    }
    if (IsValid(DropEngineeringButton))
    {
        DropEngineeringButton->OnClicked.AddUniqueDynamic(
            this, &UBHInventoryWidget::HandleDropEngineeringClicked);
    }
    if (IsValid(DropAntiVehicleButton))
    {
        DropAntiVehicleButton->OnClicked.AddUniqueDynamic(
            this, &UBHInventoryWidget::HandleDropAntiVehicleClicked);
    }
    if (IsValid(DropAmmoButton))
    {
        DropAmmoButton->OnClicked.AddUniqueDynamic(
            this, &UBHInventoryWidget::HandleDropAmmoClicked);
    }
    if (IsValid(TransferFragButton))
    {
        TransferFragButton->OnClicked.AddUniqueDynamic(
            this, &UBHInventoryWidget::HandleTransferFragClicked);
    }
    RefreshInventoryText();
}

void UBHInventoryWidget::SetInventorySnapshot(
    const FBHInventorySnapshot& Snapshot
)
{
    SnapshotText = FString::Printf(
        TEXT("FIELD LOADOUT\n\n"
             "PRIMARY  %s\n"
             "MAGAZINE  %d     RESERVE  %d / %d\n\n"
             "FRAG  %d     SMOKE  %d     ENGINEERING  %d     AT  %d\n"
             "MEDKITS  %d     DRESSINGS  %d     MISSION ITEMS  %d\n\n"
             "HELMET  %d%%     BODY ARMOR  %d%%\n"
             "CARRIED LOAD  %.1f / %.1f kg     AVAILABLE  %.1f kg\n"
             "STATUS  %s\n\n"
             "INVENTORY / LOADOUT"),
        *Snapshot.ActiveWeaponRole.ToString(),
        Snapshot.MagazineRounds,
        Snapshot.ReserveRounds,
        Snapshot.MaximumReserveRounds,
        Snapshot.FragGrenades,
        Snapshot.SmokeGrenades,
        Snapshot.EngineeringCharges,
        Snapshot.AntiVehicleRounds,
        Snapshot.Medkits,
        Snapshot.FieldDressings,
        Snapshot.MissionItemCount,
        FMath::RoundToInt(Snapshot.HelmetDurabilityFraction * 100.0f),
        FMath::RoundToInt(Snapshot.BodyArmorDurabilityFraction * 100.0f),
        Snapshot.CarriedWeightKilograms,
        Snapshot.ContainerCapacityKilograms,
        Snapshot.ContainerRemainingKilograms,
        *UEnum::GetValueAsString(Snapshot.CarryLoadState)
    );
    RefreshInventoryText();
}

void UBHInventoryWidget::SetInventoryOpen(bool bOpen)
{
    bInventoryOpen = bOpen;
    SetVisibility(
        bInventoryOpen
            ? ESlateVisibility::Visible
            : ESlateVisibility::Collapsed
    );
    RefreshInventoryText();
}

bool UBHInventoryWidget::IsInventoryOpen() const
{
    return bInventoryOpen;
}

void UBHInventoryWidget::RefreshInventoryText()
{
    if (IsValid(InventoryText))
    {
        InventoryText->SetText(FText::FromString(SnapshotText));
    }
}

void UBHInventoryWidget::HandleCycleRoleClicked()
{
    if (OwningCharacter.IsValid())
    {
        OwningCharacter->CycleInventoryWeaponRole();
    }
}

void UBHInventoryWidget::HandleDropFragClicked()
{
    if (OwningCharacter.IsValid())
    {
        OwningCharacter->DropInventoryItem(
            EBHSalvagePickupType::FragGrenades,
            1
        );
        SetInventorySnapshot(OwningCharacter->GetInventorySnapshot());
    }
}

void UBHInventoryWidget::HandleDropSmokeClicked()
{
    if (OwningCharacter.IsValid())
    {
        OwningCharacter->DropInventoryItem(EBHSalvagePickupType::SmokeGrenades, 1);
        SetInventorySnapshot(OwningCharacter->GetInventorySnapshot());
    }
}

void UBHInventoryWidget::HandleDropEngineeringClicked()
{
    if (OwningCharacter.IsValid())
    {
        OwningCharacter->DropInventoryItem(EBHSalvagePickupType::EngineeringCharges, 1);
        SetInventorySnapshot(OwningCharacter->GetInventorySnapshot());
    }
}

void UBHInventoryWidget::HandleDropAntiVehicleClicked()
{
    if (OwningCharacter.IsValid())
    {
        OwningCharacter->DropInventoryItem(EBHSalvagePickupType::AntiVehicleRounds, 1);
        SetInventorySnapshot(OwningCharacter->GetInventorySnapshot());
    }
}

void UBHInventoryWidget::HandleDropAmmoClicked()
{
    if (OwningCharacter.IsValid())
    {
        OwningCharacter->DropInventoryItem(
            EBHSalvagePickupType::Ammunition,
            30
        );
        SetInventorySnapshot(OwningCharacter->GetInventorySnapshot());
    }
}

void UBHInventoryWidget::HandleTransferFragClicked()
{
    if (OwningCharacter.IsValid())
    {
        OwningCharacter->TransferFragToNearestAlly(1);
        SetInventorySnapshot(OwningCharacter->GetInventorySnapshot());
    }
}
