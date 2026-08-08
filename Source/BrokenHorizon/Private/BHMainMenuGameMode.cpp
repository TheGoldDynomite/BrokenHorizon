#include "BHMainMenuGameMode.h"

#include "BHMainMenuWidget.h"
#include "BHUserSettingsSubsystem.h"
#include "Engine/GameInstance.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

#if !UE_BUILD_SHIPPING
#include "Containers/Ticker.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"
#endif

ABHMainMenuGameMode::ABHMainMenuGameMode()
{
    DefaultPawnClass = nullptr;
}

void ABHMainMenuGameMode::BeginPlay()
{
    Super::BeginPlay();

    if (UGameInstance* GameInstance = GetGameInstance())
    {
        if (UBHUserSettingsSubsystem* SettingsSubsystem =
            GameInstance->GetSubsystem<UBHUserSettingsSubsystem>())
        {
            SettingsSubsystem->ApplyPersistedSettings();
        }
    }

    APlayerController* PlayerController =
        UGameplayStatics::GetPlayerController(this, 0);

    if (!IsValid(PlayerController) ||
        !PlayerController->IsLocalController() ||
        !MainMenuWidgetClass)
    {
        UE_LOG(
            LogTemp,
            Error,
            TEXT(
                "Main menu requires a local player controller and "
                "MainMenuWidgetClass."
            )
        );
        return;
    }

    MainMenuWidget = CreateWidget<UBHMainMenuWidget>(
        PlayerController,
        MainMenuWidgetClass
    );

    if (!IsValid(MainMenuWidget))
    {
        return;
    }

    MainMenuWidget->SetIsFocusable(true);
    MainMenuWidget->AddToViewport();

    FInputModeUIOnly InputMode;
    InputMode.SetWidgetToFocus(MainMenuWidget->TakeWidget());
    InputMode.SetLockMouseToViewportBehavior(
        EMouseLockMode::DoNotLock
    );
    PlayerController->SetInputMode(InputMode);
    PlayerController->bShowMouseCursor = true;
    MainMenuWidget->FocusInitialControl();

#if !UE_BUILD_SHIPPING
    FString SessionReviewMode;
    if (FParse::Value(
            FCommandLine::Get(),
            TEXT("BHTestRenderedSessionReview="),
            SessionReviewMode))
    {
        GetWorldTimerManager().SetTimer(
            RenderedSessionReviewTimer,
            this,
            &ABHMainMenuGameMode::RunRenderedSessionReview,
            2.0f,
            false
        );
    }
#endif
}

#if !UE_BUILD_SHIPPING
void ABHMainMenuGameMode::RunRenderedSessionReview()
{
    if (!IsValid(MainMenuWidget))
    {
        UE_LOG(LogTemp, Error, TEXT(
            "BH_RENDERED_SESSION_REVIEW result=failure reason=missing_widget"));
        FPlatformMisc::RequestExit(false);
        return;
    }

    FString ReviewMode;
    if (!FParse::Value(
            FCommandLine::Get(),
            TEXT("BHTestRenderedSessionReview="),
            ReviewMode))
    {
        return;
    }
    ReviewMode = ReviewMode.ToUpper();

    EBHSessionState State = EBHSessionState::Idle;
    FText Message;
    if (ReviewMode == TEXT("READY"))
    {
        Message = NSLOCTEXT(
            "BrokenHorizon", "RenderedSessionReady",
            "Host a new campaign, continue the persistent war, or join a LAN campaign."
        );
    }
    else if (ReviewMode == TEXT("SEARCHING"))
    {
        State = EBHSessionState::Searching;
        Message = NSLOCTEXT(
            "BrokenHorizon", "RenderedSessionSearching",
            "Scanning the local network for a compatible Broken Horizon campaign..."
        );
    }
    else if (ReviewMode == TEXT("CONNECTED"))
    {
        State = EBHSessionState::InSession;
        Message = NSLOCTEXT(
            "BrokenHorizon", "RenderedSessionConnected",
            "Campaign connection established. Preparing synchronized travel."
        );
    }
    else if (ReviewMode == TEXT("ERROR"))
    {
        State = EBHSessionState::Error;
        Message = NSLOCTEXT(
            "BrokenHorizon", "RenderedSessionError",
            "No compatible campaign was found. Check the game version and network, then retry."
        );
    }
    else
    {
        UE_LOG(
            LogTemp,
            Error,
            TEXT(
                "BH_RENDERED_SESSION_REVIEW result=failure "
                "reason=unsupported_mode mode=%s"
            ),
            *ReviewMode
        );
        FPlatformMisc::RequestExit(false);
        return;
    }

    MainMenuWidget->SetSessionStateForRenderedReview(State, Message);

    const FString ReportDirectory = FPaths::Combine(
        FPaths::ProjectSavedDir(),
        TEXT("Reports")
    );
    IFileManager::Get().MakeDirectory(*ReportDirectory, true);
    const FString OutputPath = FPaths::Combine(
        ReportDirectory,
        FString::Printf(
            TEXT("BHRenderedUI-SESSION_%s.png"),
            *ReviewMode
        )
    );
    FScreenshotRequest::RequestScreenshot(OutputPath, true, false);
    UE_LOG(
        LogTemp,
        Display,
        TEXT(
            "BH_RENDERED_SESSION_REVIEW result=requested "
            "mode=SESSION_%s path=%s"
        ),
        *ReviewMode,
        *OutputPath
    );

    FTSTicker::GetCoreTicker().AddTicker(
        FTickerDelegate::CreateLambda(
            [](float)
            {
                FPlatformMisc::RequestExit(false);
                return false;
            }
        ),
        1.0f
    );
}
#endif
