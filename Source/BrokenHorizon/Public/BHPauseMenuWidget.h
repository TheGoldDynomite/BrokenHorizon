#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BHPauseMenuWidget.generated.h"

class ABHCharacter;
class UBHSettingsWidget;
class UButton;
class UWidget;

UCLASS()
class BROKENHORIZON_API UBHPauseMenuWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    void InitializePauseMenu(ABHCharacter* InCharacter);

    void FocusInitialControl();

#if !UE_BUILD_SHIPPING
    bool OpenSettingsForRenderedReview(bool bOpenRemapping);
#endif

protected:
    virtual void NativeConstruct() override;

    UFUNCTION()
    void HandleResumeClicked();

    UFUNCTION()
    void HandleRestartCheckpointClicked();

    UFUNCTION()
    void HandleSettingsClicked();

    UFUNCTION()
    void HandleMainMenuClicked();

    UFUNCTION()
    void HandleSettingsClosed();

    UFUNCTION(BlueprintImplementableEvent, Category = "Pause")
    void OnPauseActionFailed(const FText& ErrorMessage);

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> ResumeButton;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> RestartCheckpointButton;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> SettingsButton;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> MainMenuButton;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Pause"
    )
    TSubclassOf<UBHSettingsWidget> SettingsWidgetClass;

private:
    void FocusControl(UWidget* Control, const TCHAR* ControlName);

    TWeakObjectPtr<ABHCharacter> OwningCharacter;

    UPROPERTY()
    TObjectPtr<UBHSettingsWidget> SettingsWidget;
};
