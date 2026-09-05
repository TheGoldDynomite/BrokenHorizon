#include "BHAmmoHUDWidget.h"

#include "BHUIStyle.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/ContentWidget.h"
#include "Components/TextBlock.h"
#include "HAL/FileManager.h"
#include "HighResScreenshot.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"

namespace
{
constexpr float AmmoHUDSafeInset = 32.0f;
}

void UBHAmmoHUDWidget::NativeConstruct()
{
    Super::NativeConstruct();
    ApplySafeAreaLayout();
    BHUIStyle::Apply(*this, EBHUIStyleContext::Gameplay);
}

void UBHAmmoHUDWidget::ApplySafeAreaLayout()
{
    if (!IsValid(AmmoText) || !IsValid(WidgetTree)) { return; }
    if (!IsValid(AmmoBackdrop))
    {
        UCanvasPanel* CanvasParent = Cast<UCanvasPanel>(AmmoText->GetParent());
        UContentWidget* ContentParent = Cast<UContentWidget>(AmmoText->GetParent());
        const bool bRootText = WidgetTree->RootWidget == AmmoText;
        // The configured Blueprint uses a canvas. Preserve that parent and the bound text identity.
        if (!IsValid(CanvasParent) && !IsValid(ContentParent) && !bRootText) { return; }
        const UCanvasPanelSlot* OriginalSlot = Cast<UCanvasPanelSlot>(AmmoText->Slot);
        const int32 OriginalZOrder = OriginalSlot ? OriginalSlot->GetZOrder() : 0;
        AmmoBackdrop = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("NativeAmmoBackdrop"));
        AmmoText->RemoveFromParent();
        AmmoBackdrop->SetContent(AmmoText);
        AmmoBackdrop->SetPadding(FMargin(14.0f, 10.0f));
        if (IsValid(CanvasParent))
        {
            CanvasParent->AddChildToCanvas(AmmoBackdrop)->SetZOrder(OriginalZOrder);
        }
        else if (IsValid(ContentParent))
        {
            ContentParent->SetContent(AmmoBackdrop);
        }
        else
        {
            WidgetTree->RootWidget = AmmoBackdrop;
        }
    }
    // The outer container now owns the anchor and HUD scale; don't retain old text transforms.
    AmmoText->SetRenderTranslation(FVector2D::ZeroVector);
    AmmoText->SetRenderScale(FVector2D(1.0f, 1.0f));
    if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(AmmoBackdrop->Slot))
    {
        CanvasSlot->SetAnchors(FAnchors(1.0f, 1.0f));
        CanvasSlot->SetAlignment(FVector2D(1.0f, 1.0f));
        CanvasSlot->SetPosition(FVector2D(-AmmoHUDSafeInset, -AmmoHUDSafeInset));
        CanvasSlot->SetAutoSize(true);
        AmmoBackdrop->SetRenderTranslation(FVector2D::ZeroVector);
        return;
    }
    AmmoBackdrop->SetRenderTranslation(FVector2D(-AmmoHUDSafeInset, -AmmoHUDSafeInset));
}

void UBHAmmoHUDWidget::SetAmmo(
    int32 MagazineAmmo,
    int32 ReserveAmmo
)
{
    ApplySafeAreaLayout();
    BHUIStyle::Apply(*this, EBHUIStyleContext::Gameplay);

    CachedMagazineAmmo = MagazineAmmo;
    CachedReserveAmmo = ReserveAmmo;
    RefreshAmmoText();

#if !UE_BUILD_SHIPPING
    if (MagazineAmmo == 30 &&
        ReserveAmmo == 180 &&
        FParse::Param(
            FCommandLine::Get(),
            TEXT("BHTestBattlefieldLootHUD")))
    {
        UE_LOG(
            LogTemp,
            Display,
            TEXT(
                "BH_BATTLEFIELD_LOOT_HUD_UPDATED "
                "result=success magazine=%d reserve=%d text=\"%s\""
            ),
            MagazineAmmo,
            ReserveAmmo,
            IsValid(AmmoText) ? *AmmoText->GetText().ToString() : TEXT("")
        );

        FString ScreenshotPath;
        if (FParse::Value(
                FCommandLine::Get(),
                TEXT("BHTestBattlefieldLootHUDScreenshotPath="),
                ScreenshotPath) &&
            !ScreenshotPath.IsEmpty())
        {
            IFileManager::Get().MakeDirectory(
                *FPaths::GetPath(ScreenshotPath),
                true
            );
            FScreenshotRequest::RequestScreenshot(
                ScreenshotPath,
                true,
                false
            );
            UE_LOG(
                LogTemp,
                Display,
                TEXT(
                    "BH_BATTLEFIELD_LOOT_HUD_SCREENSHOT "
                    "result=requested path=%s"
                ),
                *ScreenshotPath
            );
        }
    }
#endif
}

void UBHAmmoHUDWidget::SetWeaponRole(
    const FText& WeaponRoleName
)
{
    CachedWeaponRoleName = WeaponRoleName;
    RefreshAmmoText();
}

void UBHAmmoHUDWidget::SetWeaponHeat(
    float HeatNormalized,
    bool bOverheated
)
{
    CachedWeaponHeat = FMath::Clamp(HeatNormalized, 0.0f, 1.0f);
    bCachedWeaponOverheated = bOverheated;
    RefreshAmmoText();
}

void UBHAmmoHUDWidget::SetFireMode(const FText& FireModeName)
{
    CachedFireModeName = FireModeName;
    RefreshAmmoText();
}

void UBHAmmoHUDWidget::SetWeaponBraced(bool bBraced)
{
    bCachedWeaponBraced = bBraced;
    RefreshAmmoText();
}

void UBHAmmoHUDWidget::RefreshAmmoText()
{

    if (!IsValid(AmmoText))
    {
        return;
    }

    const FText DisplayFireMode = CachedFireModeName.IsEmpty()
        ? NSLOCTEXT(
            "BrokenHorizon",
            "WeaponFireModeUnknown",
            "--"
        )
        : CachedFireModeName;

    AmmoText->SetText(CachedWeaponRoleName.IsEmpty()
        ? FText::Format(
            NSLOCTEXT(
                "BrokenHorizon",
                "AmmoDisplay",
                "{0} / {1}\nMODE {2}\n{3}"
            ),
            FText::AsNumber(CachedMagazineAmmo),
            FText::AsNumber(CachedReserveAmmo),
            DisplayFireMode,
            bCachedWeaponBraced
                ? NSLOCTEXT(
                    "BrokenHorizon",
                    "WeaponBraced",
                    "BRACED"
                )
                : NSLOCTEXT(
                    "BrokenHorizon",
                    "WeaponUnbraced",
                    "FREE"
                )
        )
        : FText::Format(
            NSLOCTEXT(
                "BrokenHorizon",
                "AmmoRoleDisplay",
                "{0}\n{1} / {2}\nMODE {3}  HEAT {4}%  {5}\n{6}"
            ),
            CachedWeaponRoleName,
            FText::AsNumber(CachedMagazineAmmo),
            FText::AsNumber(CachedReserveAmmo),
            DisplayFireMode,
            FText::AsNumber(FMath::RoundToInt(CachedWeaponHeat * 100.0f)),
            bCachedWeaponOverheated
                ? NSLOCTEXT("BrokenHorizon", "WeaponHeatOverheated", "OVERHEATED")
                : CachedWeaponHeat >= 0.70f
                    ? NSLOCTEXT("BrokenHorizon", "WeaponHeatCritical", "CRITICAL")
                    : CachedWeaponHeat >= 0.40f
                        ? NSLOCTEXT("BrokenHorizon", "WeaponHeatHot", "HOT")
                        : NSLOCTEXT("BrokenHorizon", "WeaponHeatStable", "STABLE")
            ,
            bCachedWeaponBraced
                ? NSLOCTEXT(
                    "BrokenHorizon",
                    "WeaponBraced",
                    "BRACED"
                )
                : NSLOCTEXT(
                    "BrokenHorizon",
                    "WeaponUnbraced",
                    "FREE"
                )
        ));
}
