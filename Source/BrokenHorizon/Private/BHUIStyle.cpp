#include "BHUIStyle.h"

#include "BHUserSettingsSaveGame.h"
#include "BHUserSettingsSubsystem.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/PanelWidget.h"
#include "Components/ProgressBar.h"
#include "Components/Slider.h"
#include "Components/TextBlock.h"
#include "Blueprint/UserWidget.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Styling/CoreStyle.h"

namespace
{
TMap<TWeakObjectPtr<UUserWidget>, EBHUIStyleContext> StyledWidgets;

void ApplyAnchoredScale(UWidget& RootWidget, float Scale)
{
    UPanelWidget* Panel = Cast<UPanelWidget>(&RootWidget);
    if (!IsValid(Panel))
    {
        RootWidget.SetRenderScale(FVector2D(Scale, Scale));
        return;
    }

    for (int32 ChildIndex = 0;
         ChildIndex < Panel->GetChildrenCount();
         ++ChildIndex)
    {
        UWidget* Child = Panel->GetChildAt(ChildIndex);
        if (!IsValid(Child))
        {
            continue;
        }

        if (UCanvasPanelSlot* CanvasSlot =
                Cast<UCanvasPanelSlot>(Child->Slot))
        {
            const FAnchors Anchors = CanvasSlot->GetAnchors();
            const bool bStretched =
                !Anchors.Minimum.Equals(Anchors.Maximum);
            if (bStretched && IsValid(Cast<UPanelWidget>(Child)))
            {
                ApplyAnchoredScale(*Child, Scale);
                continue;
            }

            const FVector2D AnchorPivot =
                (Anchors.Minimum + Anchors.Maximum) * 0.5f;
            Child->SetRenderTransformPivot(AnchorPivot);
            Child->SetRenderScale(FVector2D(Scale, Scale));
            continue;
        }

        if (IsValid(Cast<UPanelWidget>(Child)))
        {
            ApplyAnchoredScale(*Child, Scale);
        }
    }
}

void ApplySafeArea(UUserWidget& Widget, float SafeAreaScale)
{
    UWidgetTree* Tree = Widget.WidgetTree;
    if (!IsValid(Tree) || !IsValid(Tree->RootWidget))
    {
        return;
    }

    UCanvasPanel* SafeCanvas = Cast<UCanvasPanel>(Tree->FindWidget(
        TEXT("BHGlobalSafeAreaRoot")
    ));
    if (!IsValid(SafeCanvas))
    {
        UWidget* PreviousRoot = Tree->RootWidget;
        SafeCanvas = Tree->ConstructWidget<UCanvasPanel>(
            UCanvasPanel::StaticClass(),
            TEXT("BHGlobalSafeAreaRoot")
        );
        Tree->RootWidget = SafeCanvas;
        UCanvasPanelSlot* ContentSlot = SafeCanvas->AddChildToCanvas(
            PreviousRoot
        );
        ContentSlot->SetOffsets(FMargin(0.0f));
        ContentSlot->SetAlignment(FVector2D::ZeroVector);
    }

    if (SafeCanvas->GetChildrenCount() > 0)
    {
        if (UCanvasPanelSlot* ContentSlot = Cast<UCanvasPanelSlot>(
            SafeCanvas->GetChildAt(0)->Slot
        ))
        {
            const FMargin Insets = BHUIStyle::CalculateSafeAreaInsets(
                SafeAreaScale
            );
            ContentSlot->SetAnchors(FAnchors(
                Insets.Left,
                Insets.Top,
                1.0f - Insets.Right,
                1.0f - Insets.Bottom
            ));
            ContentSlot->SetOffsets(FMargin(0.0f));
        }
    }
}

bool ContainsAny(
    const FString& Value,
    std::initializer_list<const TCHAR*> Terms
)
{
    for (const TCHAR* Term : Terms)
    {
        if (Value.Contains(Term, ESearchCase::IgnoreCase))
        {
            return true;
        }
    }

    return false;
}

FLinearColor ResolveTextColor(
    const FString& Descriptor,
    EBHUIStyleContext Context,
    const FLinearColor& FriendlyColor,
    const FLinearColor& DangerColor
)
{
    if (ContainsAny(
        Descriptor,
        {
            TEXT("death"),
            TEXT("died"),
            TEXT("bleed"),
            TEXT("denied"),
            TEXT("failed")
        }
    ))
    {
        return DangerColor;
    }

    if (ContainsAny(
        Descriptor,
        {
            TEXT("complete"),
            TEXT("success"),
            TEXT("friendly")
        }
    ))
    {
        return FriendlyColor;
    }

    if (ContainsAny(
        Descriptor,
        {
            TEXT("title"),
            TEXT("header"),
            TEXT("objective"),
            TEXT("ammo"),
            TEXT("interaction"),
            TEXT("prompt")
        }
    ))
    {
        return BHUIStyle::Gold;
    }

    return Context == EBHUIStyleContext::Alert
        ? BHUIStyle::Warning
        : BHUIStyle::White;
}

void StyleText(
    UTextBlock& TextBlock,
    EBHUIStyleContext Context,
    const FLinearColor& FriendlyColor,
    const FLinearColor& DangerColor,
    bool bHighContrast
)
{
    const FString Descriptor =
        TextBlock.GetName() +
        TEXT(" ") +
        TextBlock.GetText().ToString();
    const bool bHeading = ContainsAny(
        Descriptor,
        {
            TEXT("title"),
            TEXT("header"),
            TEXT("objective"),
            TEXT("mission complete"),
            TEXT("you died")
        }
    );
    const bool bReadout = ContainsAny(
        Descriptor,
        {
            TEXT("ammo"),
            TEXT("health"),
            TEXT("stamina"),
            TEXT("interaction"),
            TEXT("prompt")
        }
    );
    FSlateFontInfo Font = TextBlock.GetFont();

    if (bHeading)
    {
        Font.TypefaceFontName = TEXT("Bold");
        Font.Size = FMath::Max(Font.Size, 24);
    }
    else if (bReadout)
    {
        Font.TypefaceFontName = TEXT("Bold");
        Font.Size = FMath::Max(Font.Size, 18);
    }
    else
    {
        Font.Size = FMath::Max(Font.Size, 16);
    }

    TextBlock.SetFont(Font);
    TextBlock.SetColorAndOpacity(
        FSlateColor(ResolveTextColor(
            Descriptor,
            Context,
            FriendlyColor,
            DangerColor
        ))
    );
    TextBlock.SetShadowOffset(
        bHighContrast ? FVector2D(2.5f, 2.5f) : FVector2D(1.5f, 1.5f)
    );
    TextBlock.SetShadowColorAndOpacity(
        FLinearColor(0.0f, 0.0f, 0.0f, bHighContrast ? 1.0f : 0.82f)
    );
}

void StyleButton(UButton& Button)
{
    FButtonStyle Style = Button.GetStyle();
    const FSlateBrush SolidBrush =
        *FCoreStyle::Get().GetBrush(TEXT("WhiteBrush"));
    Style.Normal = SolidBrush;
    Style.Hovered = SolidBrush;
    Style.Pressed = SolidBrush;
    Style.Disabled = SolidBrush;
    Style.Normal.TintColor = FSlateColor(FLinearColor(
        0.045f,
        0.060f,
        0.062f,
        0.98f
    ));
    Style.Hovered.TintColor = FSlateColor(FLinearColor(
        0.19f,
        0.15f,
        0.07f,
        1.0f
    ));
    Style.Pressed.TintColor = FSlateColor(FLinearColor(
        0.72f,
        0.46f,
        0.08f,
        1.0f
    ));
    Style.Disabled.TintColor = FSlateColor(FLinearColor(
        0.028f,
        0.034f,
        0.035f,
        0.58f
    ));
    Style.NormalPadding = FMargin(18.0f, 10.0f);
    Style.PressedPadding = FMargin(
        18.0f,
        11.0f,
        18.0f,
        9.0f
    );
    Button.SetStyle(Style);
    Button.SetColorAndOpacity(BHUIStyle::White);
}

void StyleProgressBar(
    UProgressBar& ProgressBar,
    const FLinearColor& FriendlyColor,
    const FLinearColor& DangerColor
)
{
    const FString Name = ProgressBar.GetName();

    if (Name.Contains(TEXT("Health"), ESearchCase::IgnoreCase))
    {
        ProgressBar.SetFillColorAndOpacity(
            DangerColor
        );
    }
    else if (Name.Contains(
        TEXT("Stamina"),
        ESearchCase::IgnoreCase
    ))
    {
        ProgressBar.SetFillColorAndOpacity(
            FLinearColor(0.68f, 0.54f, 0.22f, 1.0f)
        );
    }
    else
    {
        ProgressBar.SetFillColorAndOpacity(FriendlyColor);
    }
}
}

void BHUIStyle::Apply(
    UUserWidget& Widget,
    EBHUIStyleContext Context
)
{
    // Gameplay widgets can call Apply from high-frequency state setters.
    // Avoid rebuilding their complete widget-tree style on every health,
    // stamina, or ammo update; RefreshAll invalidates this entry when user
    // settings change and a full restyle is actually required.
    if (const EBHUIStyleContext* AppliedContext = StyledWidgets.Find(&Widget))
    {
        if (*AppliedContext == Context)
        {
            return;
        }
    }
    StyledWidgets.Add(&Widget, Context);

    float HUDScale = 1.0f;
    EBHColorVisionMode ColorVisionMode = EBHColorVisionMode::Standard;
    bool bHighContrast = false;
    bool bReducedMotion = false;
    float SafeAreaScale = 0.95f;
    if (UGameInstance* GameInstance = Widget.GetGameInstance())
    {
        if (const UBHUserSettingsSubsystem* Settings =
            GameInstance->GetSubsystem<UBHUserSettingsSubsystem>())
        {
            HUDScale = Settings->GetHUDScale();
            ColorVisionMode = Settings->GetColorVisionMode();
            bHighContrast = Settings->IsHighContrastHUDEnabled();
            bReducedMotion = Settings->IsReducedMotionEnabled();
            SafeAreaScale = Settings->GetUISafeAreaScale();
        }
    }

#if !UE_BUILD_SHIPPING
    float TestHUDScale = HUDScale;
    if (FParse::Value(
            FCommandLine::Get(),
            TEXT("BHTestHUDScale="),
            TestHUDScale))
    {
        HUDScale = FMath::Clamp(TestHUDScale, 0.75f, 1.5f);
    }
    float TestSafeAreaScale = SafeAreaScale;
    if (FParse::Value(
            FCommandLine::Get(),
            TEXT("BHTestUISafeAreaScale="),
            TestSafeAreaScale))
    {
        SafeAreaScale = FMath::Clamp(TestSafeAreaScale, 0.8f, 1.0f);
    }
#endif

    const float ContextScale = ResolveContextScale(HUDScale, Context);
    Widget.SetRenderScale(FVector2D(1.0f, 1.0f));
    Widget.SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
    if (bReducedMotion)
    {
        Widget.StopAllAnimations();
    }

    const FLinearColor FriendlyColor = ResolveFriendlyColor(ColorVisionMode);
    const FLinearColor DangerColor = ResolveDangerColor(ColorVisionMode);
    UWidgetTree* WidgetTree = Widget.WidgetTree;

    if (!IsValid(WidgetTree))
    {
        return;
    }

    ApplySafeArea(Widget, SafeAreaScale);
    if (IsValid(WidgetTree->RootWidget))
    {
        ApplyAnchoredScale(*WidgetTree->RootWidget, ContextScale);
    }

    TArray<UWidget*> Widgets;
    WidgetTree->GetAllWidgets(Widgets);
    int32 TextCount = 0;
    int32 ButtonCount = 0;
    int32 BorderCount = 0;
    int32 ImageCount = 0;

    for (UWidget* ChildWidget : Widgets)
    {
        if (UTextBlock* TextBlock =
            Cast<UTextBlock>(ChildWidget))
        {
            StyleText(
                *TextBlock,
                Context,
                FriendlyColor,
                DangerColor,
                bHighContrast
            );
            ++TextCount;
        }
        else if (UButton* Button =
            Cast<UButton>(ChildWidget))
        {
            StyleButton(*Button);
            ++ButtonCount;
        }
        else if (UBorder* Border =
            Cast<UBorder>(ChildWidget))
        {
            const FString Name = Border->GetName();
            const bool bAccent = ContainsAny(
                Name,
                {
                    TEXT("accent"),
                    TEXT("header"),
                    TEXT("selected")
                }
            );
            FSlateBrush BorderBrush =
                *FCoreStyle::Get().GetBrush(TEXT("WhiteBrush"));
            BorderBrush.TintColor = FSlateColor(
                bAccent
                    ? Gold
                    : (bHighContrast ? Charcoal : Gunmetal)
            );
            Border->SetBrush(BorderBrush);
            ++BorderCount;
        }
        else if (UImage* Image =
            Cast<UImage>(ChildWidget))
        {
            const FString Name = Image->GetName();

            if (ContainsAny(
                Name,
                {
                    TEXT("background"),
                    TEXT("backdrop"),
                    TEXT("overlay"),
                    TEXT("panel")
                }
            ))
            {
                Image->SetColorAndOpacity(
                    Context == EBHUIStyleContext::Menu
                        ? Charcoal
                        : Gunmetal
                );
            }

            ++ImageCount;
        }
        else if (UProgressBar* ProgressBar =
            Cast<UProgressBar>(ChildWidget))
        {
            StyleProgressBar(*ProgressBar, FriendlyColor, DangerColor);
        }
        else if (USlider* Slider =
            Cast<USlider>(ChildWidget))
        {
            Slider->SetSliderBarColor(Muted);
            Slider->SetSliderHandleColor(Gold);
        }
    }

    UE_LOG(
        LogTemp,
        Display,
        TEXT(
            "BHUIStyle applied to %s: widgets=%d text=%d "
            "buttons=%d borders=%d images=%d"
        ),
        *Widget.GetClass()->GetPathName(),
        Widgets.Num(),
        TextCount,
        ButtonCount,
        BorderCount,
        ImageCount
    );
}

void BHUIStyle::RefreshAll(UWorld& World)
{
    TArray<TPair<TWeakObjectPtr<UUserWidget>, EBHUIStyleContext>> PendingRefresh;
    for (auto It = StyledWidgets.CreateIterator(); It; ++It)
    {
        UUserWidget* Widget = It.Key().Get();
        if (!IsValid(Widget))
        {
            It.RemoveCurrent();
            continue;
        }

        if (Widget->GetWorld() == &World)
        {
            PendingRefresh.Emplace(It.Key(), It.Value());
        }
    }

    for (const TPair<TWeakObjectPtr<UUserWidget>, EBHUIStyleContext>& Entry : PendingRefresh)
    {
        if (UUserWidget* Widget = Entry.Key.Get())
        {
            StyledWidgets.Remove(Widget);
            Apply(*Widget, Entry.Value);
        }
    }
}

FLinearColor BHUIStyle::ResolveFriendlyColor(EBHColorVisionMode Mode)
{
    switch (Mode)
    {
        case EBHColorVisionMode::Deuteranopia:
        case EBHColorVisionMode::Protanopia:
            return FLinearColor(0.12f, 0.62f, 1.0f, 1.0f);
        case EBHColorVisionMode::Tritanopia:
            return FLinearColor(0.12f, 0.82f, 0.56f, 1.0f);
        default:
            return Friendly;
    }
}

FMargin BHUIStyle::CalculateSafeAreaInsets(float SafeAreaScale)
{
    const float Scale = FMath::Clamp(SafeAreaScale, 0.8f, 1.0f);
    const float Inset = (1.0f - Scale) * 0.5f;
    return FMargin(Inset);
}

float BHUIStyle::ResolveContextScale(
    float HUDScale,
    EBHUIStyleContext Context
)
{
    return Context == EBHUIStyleContext::Menu
        ? 1.0f
        : FMath::Clamp(HUDScale, 0.75f, 1.5f);
}

FLinearColor BHUIStyle::ResolveDangerColor(EBHColorVisionMode Mode)
{
    switch (Mode)
    {
        case EBHColorVisionMode::Deuteranopia:
        case EBHColorVisionMode::Protanopia:
            return FLinearColor(1.0f, 0.64f, 0.08f, 1.0f);
        case EBHColorVisionMode::Tritanopia:
            return FLinearColor(0.96f, 0.18f, 0.52f, 1.0f);
        default:
            return Danger;
    }
}
