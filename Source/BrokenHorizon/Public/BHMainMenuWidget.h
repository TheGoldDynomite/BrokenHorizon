#pragma once

#include "CoreMinimal.h"
#include "BHSessionSubsystem.h"
#include "Blueprint/UserWidget.h"
#include "BHMainMenuWidget.generated.h"

class UBHSettingsWidget;
class UButton;
class UTextBlock;
class UWidget;

UCLASS()
class BROKENHORIZON_API UBHMainMenuWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    void FocusInitialControl();

    UFUNCTION(BlueprintCallable, Category = "Main Menu")
    void RefreshContinueState();

    static FText BuildSessionStatusText(
        EBHSessionState State,
        const FText& Message
    );

    static FLinearColor GetSessionStatusColor(
        EBHSessionState State
    );

#if !UE_BUILD_SHIPPING
    void SetSessionStateForRenderedReview(
        EBHSessionState State,
        const FText& Message
    );
#endif

protected:
    virtual void NativeConstruct() override;

    virtual void NativeDestruct() override;

    UFUNCTION()
    void HandleNewGameClicked();

    UFUNCTION()
    void HandleContinueClicked();

    UFUNCTION()
    void HandleJoinCampaignClicked();

    UFUNCTION()
    void HandleSettingsClicked();

    UFUNCTION()
    void HandleQuitClicked();

    UFUNCTION()
    void HandleSettingsClosed();

    UFUNCTION()
    void HandleSessionStateChanged(
        EBHSessionState State,
        FText Message
    );

    UFUNCTION(BlueprintImplementableEvent, Category = "Main Menu")
    void OnMenuActionFailed(const FText& ErrorMessage);

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> NewGameButton;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> ContinueButton;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> JoinCampaignButton;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> SettingsButton;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> QuitButton;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> SessionStatusText;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Main Menu"
    )
    TSubclassOf<UBHSettingsWidget> SettingsWidgetClass;

private:
    void FocusControl(UWidget* Control, const TCHAR* ControlName);

    void EnsureMultiplayerMenuControls();

    void ReportSessionActionStartFailure(
        UBHSessionSubsystem* SessionSubsystem,
        const FText& FallbackMessage
    );

    UBHSessionSubsystem* GetSessionSubsystem() const;

    UPROPERTY()
    TObjectPtr<UBHSettingsWidget> SettingsWidget;
};
