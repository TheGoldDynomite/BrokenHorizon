#include "BHSessionSubsystem.h"

#include "BHGameShellSettings.h"
#include "BHSaveSubsystem.h"
#if !UE_BUILD_SHIPPING
#include "BHWarGameState.h"
#include "BHWarSubsystem.h"
#include "Engine/NetConnection.h"
#include "Engine/NetDriver.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "String/LexFromString.h"
#include "UObject/UObjectArray.h"
#endif
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Online/OnlineSessionNames.h"
#include "OnlineSubsystem.h"
#include "UObject/UObjectGlobals.h"

namespace
{
const FName BHCampaignSessionKeyword(TEXT("BHCampaign"));
}

void UBHSessionSubsystem::Initialize(
    FSubsystemCollectionBase& Collection
)
{
    Super::Initialize(Collection);
    ResolveSessionInterface();
    PostLoadMapDelegateHandle =
        FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(
            this,
            &UBHSessionSubsystem::HandlePostLoadMap
        );
#if !UE_BUILD_SHIPPING
    StartSameProcessReconnectTest();
#endif
}

void UBHSessionSubsystem::Deinitialize()
{
#if !UE_BUILD_SHIPPING
    FTSTicker::GetCoreTicker().RemoveTicker(ReconnectTestTickerHandle);
    ReconnectTestTickerHandle.Reset();
#endif
    ClearSessionDelegates();

    if (PostLoadMapDelegateHandle.IsValid())
    {
        FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(
            PostLoadMapDelegateHandle
        );
        PostLoadMapDelegateHandle.Reset();
    }

    SessionSearch.Reset();
    SessionInterface.Reset();
    Super::Deinitialize();
}

bool UBHSessionSubsystem::HostCampaign(
    bool bContinueCampaign,
    int32 MaximumPlayers,
    bool bLANMatch
)
{
    if (IsSessionActionPending() || !ResolveSessionInterface())
    {
        return false;
    }

    PendingAction = EPendingAction::Host;
    bPendingContinueCampaign = bContinueCampaign;
    bPendingLANMatch = bLANMatch;
    PendingMaximumPlayers = FMath::Clamp(MaximumPlayers, 2, 16);
    SetSessionState(
        EBHSessionState::Hosting,
        NSLOCTEXT(
            "BrokenHorizon",
            "SessionHosting",
            "Creating campaign session..."
        )
    );

    if (SessionInterface->GetNamedSession(NAME_GameSession) != nullptr)
    {
        DestroySessionDelegateHandle =
            SessionInterface->AddOnDestroySessionCompleteDelegate_Handle(
                FOnDestroySessionCompleteDelegate::CreateUObject(
                    this,
                    &UBHSessionSubsystem::HandleDestroySessionComplete
                )
            );

        if (!SessionInterface->DestroySession(NAME_GameSession))
        {
            SessionInterface->
                ClearOnDestroySessionCompleteDelegate_Handle(
                    DestroySessionDelegateHandle
                );
            DestroySessionDelegateHandle.Reset();
            FailSessionAction(
                NSLOCTEXT(
                    "BrokenHorizon",
                    "SessionDestroyBeforeHostFailed",
                    "The previous campaign session could not be closed."
                )
            );
            return false;
        }

        return true;
    }

    return CreatePendingSession();
}

bool UBHSessionSubsystem::FindAndJoinCampaign(bool bLANMatch)
{
    if (IsSessionActionPending() || !ResolveSessionInterface())
    {
        return false;
    }

    PendingAction = EPendingAction::None;
    bPendingLANMatch = bLANMatch;
    SessionSearch = MakeShared<FOnlineSessionSearch>();
    SessionSearch->bIsLanQuery = bLANMatch;
    SessionSearch->MaxSearchResults = 50;
    SessionSearch->PingBucketSize = 50;

    FindSessionsDelegateHandle =
        SessionInterface->AddOnFindSessionsCompleteDelegate_Handle(
            FOnFindSessionsCompleteDelegate::CreateUObject(
                this,
                &UBHSessionSubsystem::HandleFindSessionsComplete
            )
        );

    SetSessionState(
        EBHSessionState::Searching,
        NSLOCTEXT(
            "BrokenHorizon",
            "SessionSearching",
            "Searching for campaign sessions..."
        )
    );

    if (!SessionInterface->FindSessions(0, SessionSearch.ToSharedRef()))
    {
        SessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(
            FindSessionsDelegateHandle
        );
        FindSessionsDelegateHandle.Reset();
        SessionSearch.Reset();
        FailSessionAction(
            NSLOCTEXT(
                "BrokenHorizon",
                "SessionSearchStartFailed",
                "Campaign search could not be started."
            )
        );
        return false;
    }

    return true;
}

bool UBHSessionSubsystem::LeaveSession()
{
    if (IsSessionActionPending())
    {
        return false;
    }

    if (!ResolveSessionInterface() ||
        SessionInterface->GetNamedSession(NAME_GameSession) == nullptr)
    {
        return OpenMainMenu();
    }

    PendingAction = EPendingAction::Leave;
    SetSessionState(
        EBHSessionState::Leaving,
        NSLOCTEXT(
            "BrokenHorizon",
            "SessionLeaving",
            "Leaving campaign session..."
        )
    );

    DestroySessionDelegateHandle =
        SessionInterface->AddOnDestroySessionCompleteDelegate_Handle(
            FOnDestroySessionCompleteDelegate::CreateUObject(
                this,
                &UBHSessionSubsystem::HandleDestroySessionComplete
            )
        );

    if (!SessionInterface->DestroySession(NAME_GameSession))
    {
        SessionInterface->ClearOnDestroySessionCompleteDelegate_Handle(
            DestroySessionDelegateHandle
        );
        DestroySessionDelegateHandle.Reset();
        PendingAction = EPendingAction::None;
        FailSessionAction(
            NSLOCTEXT(
                "BrokenHorizon",
                "SessionLeaveFailed",
                "The campaign session could not be closed."
            )
        );
        return false;
    }

    return true;
}

EBHSessionState UBHSessionSubsystem::GetSessionState() const
{
    return SessionState;
}

bool UBHSessionSubsystem::IsSessionActionPending() const
{
    return SessionState == EBHSessionState::Hosting ||
        SessionState == EBHSessionState::Searching ||
        SessionState == EBHSessionState::Joining ||
        SessionState == EBHSessionState::Traveling ||
        SessionState == EBHSessionState::Leaving;
}

bool UBHSessionSubsystem::ResolveSessionInterface()
{
    if (SessionInterface.IsValid())
    {
        return true;
    }

    IOnlineSubsystem* OnlineSubsystem =
        IOnlineSubsystem::Get();

    if (!OnlineSubsystem)
    {
        FailSessionAction(
            NSLOCTEXT(
                "BrokenHorizon",
                "OnlineSubsystemMissing",
                "No online service is available."
            )
        );
        return false;
    }

    SessionInterface = OnlineSubsystem->GetSessionInterface();

    if (!SessionInterface.IsValid())
    {
        FailSessionAction(
            NSLOCTEXT(
                "BrokenHorizon",
                "SessionInterfaceMissing",
                "The online service does not support sessions."
            )
        );
        return false;
    }

    return true;
}

bool UBHSessionSubsystem::CreatePendingSession()
{
    if (!SessionInterface.IsValid())
    {
        FailSessionAction(
            NSLOCTEXT(
                "BrokenHorizon",
                "SessionInterfaceLost",
                "The online session service became unavailable."
            )
        );
        return false;
    }

    FOnlineSessionSettings Settings;
    Settings.bIsLANMatch = bPendingLANMatch;
    Settings.bShouldAdvertise = true;
    Settings.bAllowJoinInProgress = true;
    Settings.bAllowJoinViaPresence = true;
    Settings.bUsesPresence = true;
    Settings.bUseLobbiesIfAvailable = true;
    Settings.NumPublicConnections = PendingMaximumPlayers;
    Settings.NumPrivateConnections = 0;
    Settings.Set(
        SETTING_MAPNAME,
        FString(TEXT("BrokenHorizonCampaign")),
        EOnlineDataAdvertisementType::ViaOnlineServiceAndPing
    );
    Settings.Set(
        BHCampaignSessionKeyword,
        true,
        EOnlineDataAdvertisementType::ViaOnlineServiceAndPing
    );

    CreateSessionDelegateHandle =
        SessionInterface->AddOnCreateSessionCompleteDelegate_Handle(
            FOnCreateSessionCompleteDelegate::CreateUObject(
                this,
                &UBHSessionSubsystem::HandleCreateSessionComplete
            )
        );

    if (!SessionInterface->CreateSession(
            0,
            NAME_GameSession,
            Settings
        ))
    {
        SessionInterface->ClearOnCreateSessionCompleteDelegate_Handle(
            CreateSessionDelegateHandle
        );
        CreateSessionDelegateHandle.Reset();
        FailSessionAction(
            NSLOCTEXT(
                "BrokenHorizon",
                "SessionCreateStartFailed",
                "The campaign session could not be created."
            )
        );
        return false;
    }

    return true;
}

bool UBHSessionSubsystem::TravelToCampaign()
{
    UGameInstance* GameInstance = GetGameInstance();
    const UBHGameShellSettings* ShellSettings =
        GetDefault<UBHGameShellSettings>();
    UWorld* World = IsValid(GameInstance)
        ? GameInstance->GetWorld()
        : nullptr;

    if (!IsValid(ShellSettings) ||
        ShellSettings->GameplayMap.IsNull() ||
        !IsValid(World))
    {
        return false;
    }

    const FString PackageName =
        ShellSettings->GameplayMap.ToSoftObjectPath().GetLongPackageName();

    if (PackageName.IsEmpty())
    {
        return false;
    }

    SetSessionState(
        EBHSessionState::Traveling,
        bPendingContinueCampaign
            ? NSLOCTEXT(
                "BrokenHorizon",
                "SessionLoadingCampaign",
                "Loading shared campaign..."
            )
            : NSLOCTEXT(
                "BrokenHorizon",
                "SessionStartingCampaign",
                "Starting shared campaign..."
            )
    );
    bLoadContinueAfterListenTravel = bPendingContinueCampaign;
    World->ServerTravel(PackageName + TEXT("?listen"));
    return true;
}

bool UBHSessionSubsystem::OpenMainMenu()
{
    const UBHGameShellSettings* ShellSettings =
        GetDefault<UBHGameShellSettings>();

    if (!IsValid(ShellSettings) ||
        ShellSettings->MainMenuMap.IsNull())
    {
        return false;
    }

    const FString PackageName =
        ShellSettings->MainMenuMap.ToSoftObjectPath().GetLongPackageName();

    if (PackageName.IsEmpty())
    {
        return false;
    }

    UGameplayStatics::OpenLevel(this, FName(*PackageName));
    SetSessionState(
        EBHSessionState::Idle,
        NSLOCTEXT(
            "BrokenHorizon",
            "SessionIdle",
            "Ready."
        )
    );
    return true;
}

void UBHSessionSubsystem::SetSessionState(
    EBHSessionState NewState,
    const FText& Message
)
{
    SessionState = NewState;
    OnSessionStateChanged.Broadcast(NewState, Message);
}

void UBHSessionSubsystem::FailSessionAction(const FText& Message)
{
    PendingAction = EPendingAction::None;
    bLoadContinueAfterListenTravel = false;
    SetSessionState(EBHSessionState::Error, Message);
    UE_LOG(
        LogTemp,
        Error,
        TEXT("BH_SESSION_ERROR %s"),
        *Message.ToString()
    );
}
void UBHSessionSubsystem::ClearSessionDelegates()
{
    if (!SessionInterface.IsValid())
    {
        return;
    }

    if (CreateSessionDelegateHandle.IsValid())
    {
        SessionInterface->ClearOnCreateSessionCompleteDelegate_Handle(
            CreateSessionDelegateHandle
        );
        CreateSessionDelegateHandle.Reset();
    }

    if (FindSessionsDelegateHandle.IsValid())
    {
        SessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(
            FindSessionsDelegateHandle
        );
        FindSessionsDelegateHandle.Reset();
    }

    if (JoinSessionDelegateHandle.IsValid())
    {
        SessionInterface->ClearOnJoinSessionCompleteDelegate_Handle(
            JoinSessionDelegateHandle
        );
        JoinSessionDelegateHandle.Reset();
    }

    if (DestroySessionDelegateHandle.IsValid())
    {
        SessionInterface->ClearOnDestroySessionCompleteDelegate_Handle(
            DestroySessionDelegateHandle
        );
        DestroySessionDelegateHandle.Reset();
    }
}

void UBHSessionSubsystem::HandleCreateSessionComplete(
    FName SessionName,
    bool bWasSuccessful
)
{
    if (SessionInterface.IsValid() &&
        CreateSessionDelegateHandle.IsValid())
    {
        SessionInterface->ClearOnCreateSessionCompleteDelegate_Handle(
            CreateSessionDelegateHandle
        );
        CreateSessionDelegateHandle.Reset();
    }

    if (!bWasSuccessful || SessionName != NAME_GameSession)
    {
        FailSessionAction(
            NSLOCTEXT(
                "BrokenHorizon",
                "SessionCreateFailed",
                "The campaign session could not be created."
            )
        );
        return;
    }

    PendingAction = EPendingAction::None;

    if (!TravelToCampaign())
    {
        FailSessionAction(
            NSLOCTEXT(
                "BrokenHorizon",
                "SessionTravelFailed",
                "The shared campaign could not be opened."
            )
        );
    }
}

void UBHSessionSubsystem::HandleFindSessionsComplete(
    bool bWasSuccessful
)
{
    if (SessionInterface.IsValid() &&
        FindSessionsDelegateHandle.IsValid())
    {
        SessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(
            FindSessionsDelegateHandle
        );
        FindSessionsDelegateHandle.Reset();
    }

    if (!bWasSuccessful || !SessionSearch.IsValid())
    {
        SessionSearch.Reset();
        FailSessionAction(
            NSLOCTEXT(
                "BrokenHorizon",
                "SessionSearchFailed",
                "Campaign search failed."
            )
        );
        return;
    }

    const FOnlineSessionSearchResult* SelectedResult = nullptr;

    for (const FOnlineSessionSearchResult& Result
        : SessionSearch->SearchResults)
    {
        bool bIsCampaign = false;
        Result.Session.SessionSettings.Get(
            BHCampaignSessionKeyword,
            bIsCampaign
        );

        if (bIsCampaign)
        {
            SelectedResult = &Result;
            break;
        }
    }

    if (!SelectedResult)
    {
        SessionSearch.Reset();
        FailSessionAction(
            NSLOCTEXT(
                "BrokenHorizon",
                "SessionNoneFound",
                "No joinable Broken Horizon campaign was found."
            )
        );
        return;
    }

    JoinSessionDelegateHandle =
        SessionInterface->AddOnJoinSessionCompleteDelegate_Handle(
            FOnJoinSessionCompleteDelegate::CreateUObject(
                this,
                &UBHSessionSubsystem::HandleJoinSessionComplete
            )
        );

    SetSessionState(
        EBHSessionState::Joining,
        NSLOCTEXT(
            "BrokenHorizon",
            "SessionJoining",
            "Joining shared campaign..."
        )
    );

    if (!SessionInterface->JoinSession(
            0,
            NAME_GameSession,
            *SelectedResult
        ))
    {
        SessionInterface->ClearOnJoinSessionCompleteDelegate_Handle(
            JoinSessionDelegateHandle
        );
        JoinSessionDelegateHandle.Reset();
        SessionSearch.Reset();
        FailSessionAction(
            NSLOCTEXT(
                "BrokenHorizon",
                "SessionJoinStartFailed",
                "The selected campaign could not be joined."
            )
        );
    }
}

void UBHSessionSubsystem::HandleJoinSessionComplete(
    FName SessionName,
    EOnJoinSessionCompleteResult::Type Result
)
{
    if (SessionInterface.IsValid() &&
        JoinSessionDelegateHandle.IsValid())
    {
        SessionInterface->ClearOnJoinSessionCompleteDelegate_Handle(
            JoinSessionDelegateHandle
        );
        JoinSessionDelegateHandle.Reset();
    }

    SessionSearch.Reset();

    if (Result != EOnJoinSessionCompleteResult::Success ||
        SessionName != NAME_GameSession ||
        !SessionInterface.IsValid())
    {
        FailSessionAction(
            NSLOCTEXT(
                "BrokenHorizon",
                "SessionJoinFailed",
                "The selected campaign could not be joined."
            )
        );
        return;
    }

    FString ConnectString;

    if (!SessionInterface->GetResolvedConnectString(
            NAME_GameSession,
            ConnectString
        ) ||
        ConnectString.IsEmpty())
    {
        FailSessionAction(
            NSLOCTEXT(
                "BrokenHorizon",
                "SessionAddressMissing",
                "The campaign server address could not be resolved."
            )
        );
        return;
    }

    UGameInstance* GameInstance = GetGameInstance();
    APlayerController* PlayerController = IsValid(GameInstance)
        ? GameInstance->GetFirstLocalPlayerController()
        : nullptr;

    if (!IsValid(PlayerController))
    {
        FailSessionAction(
            NSLOCTEXT(
                "BrokenHorizon",
                "SessionPlayerMissing",
                "No local player is available to join the campaign."
            )
        );
        return;
    }

    SetSessionState(
        EBHSessionState::Traveling,
        NSLOCTEXT(
            "BrokenHorizon",
            "SessionConnecting",
            "Connecting to shared campaign..."
        )
    );
    PlayerController->ClientTravel(
        ConnectString,
        ETravelType::TRAVEL_Absolute
    );
}
void UBHSessionSubsystem::HandleDestroySessionComplete(
    FName SessionName,
    bool bWasSuccessful
)
{
    if (SessionInterface.IsValid() &&
        DestroySessionDelegateHandle.IsValid())
    {
        SessionInterface->ClearOnDestroySessionCompleteDelegate_Handle(
            DestroySessionDelegateHandle
        );
        DestroySessionDelegateHandle.Reset();
    }

    if (!bWasSuccessful || SessionName != NAME_GameSession)
    {
        FailSessionAction(
            NSLOCTEXT(
                "BrokenHorizon",
                "SessionDestroyFailed",
                "The previous campaign session could not be closed."
            )
        );
        return;
    }

    const EPendingAction CompletedAction = PendingAction;

    if (CompletedAction == EPendingAction::Host)
    {
        CreatePendingSession();
        return;
    }

    PendingAction = EPendingAction::None;

    if (CompletedAction == EPendingAction::Leave &&
        !OpenMainMenu())
    {
        FailSessionAction(
            NSLOCTEXT(
                "BrokenHorizon",
                "SessionReturnMenuFailed",
                "The main menu could not be opened."
            )
        );
    }
}

void UBHSessionSubsystem::HandlePostLoadMap(UWorld* LoadedWorld)
{
    if (!IsValid(LoadedWorld) ||
        SessionState != EBHSessionState::Traveling)
    {
        return;
    }

    if (bLoadContinueAfterListenTravel)
    {
        bLoadContinueAfterListenTravel = false;
        UGameInstance* GameInstance = GetGameInstance();
        UBHSaveSubsystem* SaveSubsystem = IsValid(GameInstance)
            ? GameInstance->GetSubsystem<UBHSaveSubsystem>()
            : nullptr;

        const bool bCanLoadSavedCampaign =
            LoadedWorld->GetNetMode() == NM_ListenServer &&
            IsValid(SaveSubsystem) &&
            SaveSubsystem->HasValidSaveGame();

        if (bCanLoadSavedCampaign &&
            SaveSubsystem->LoadProgress())
        {
            return;
        }

        if (bCanLoadSavedCampaign)
        {
            UE_LOG(
                LogTemp,
                Warning,
                TEXT("BH_SESSION_CONTINUE_LOAD_FAILED")
            );
        }

        SetSessionState(
            EBHSessionState::InSession,
            NSLOCTEXT(
                "BrokenHorizon",
                "SessionConnected",
                "Connected to shared campaign."
            )
        );
        return;
    }

    SetSessionState(
        EBHSessionState::InSession,
        NSLOCTEXT(
            "BrokenHorizon",
            "SessionConnected",
            "Connected to shared campaign."
        )
    );
}


#if !UE_BUILD_SHIPPING
void UBHSessionSubsystem::StartSameProcessReconnectTest()
{
    const TCHAR* CommandLine = FCommandLine::Get();
    if (!FParse::Param(CommandLine, TEXT("BHTestSameProcessReconnect")))
    {
        return;
    }

    FString PortText;
    FString TimeoutText;
    int32 Port = 0;
    int32 TimeoutSeconds = 240;
    FParse::Value(CommandLine, TEXT("BHTestReconnectRunId="), ReconnectTestRunID);
    FParse::Value(CommandLine, TEXT("BHTestReconnectPort="), PortText);
    FParse::Value(CommandLine, TEXT("BHTestReconnectTimeout="), TimeoutText);
    const auto ParseDigits = [](const FString& Text, int32& Value)
    {
        if (Text.IsEmpty() || Text.Len() > 6)
        {
            return false;
        }
        for (TCHAR Character : Text)
        {
            if (Character < TEXT('0') || Character > TEXT('9'))
            {
                return false;
            }
        }
        return LexTryParseString(Value, *Text);
    };
    bool bSafeRunID = !ReconnectTestRunID.IsEmpty() && ReconnectTestRunID.Len() <= 64;
    for (TCHAR Character : ReconnectTestRunID)
    {
        bSafeRunID &= (Character >= TEXT('a') && Character <= TEXT('z')) ||
            (Character >= TEXT('A') && Character <= TEXT('Z')) ||
            (Character >= TEXT('0') && Character <= TEXT('9')) ||
            Character == TEXT('_') || Character == TEXT('-');
    }
    if (!bSafeRunID || !ParseDigits(PortText, Port) || Port < 1024 || Port > 65535 ||
        (!TimeoutText.IsEmpty() && !ParseDigits(TimeoutText, TimeoutSeconds)) ||
        TimeoutSeconds < 30 || TimeoutSeconds > 600)
    {
        UE_LOG(LogTemp, Error, TEXT("BH_TEST_SAME_PROCESS_RECONNECT phase=failed result=failure reason=invalid_arguments"));
        return;
    }

    ReconnectTestGameInstance = GetGameInstance();
    ReconnectTestAddress = FString::Printf(TEXT("127.0.0.1:%d"), Port);
    ReconnectTestControlDirectory = FPaths::Combine(
        FPaths::ProjectSavedDir(), TEXT("Automation/SameProcessReconnect"), ReconnectTestRunID);
    ReconnectTestDeadline = FPlatformTime::Seconds() + TimeoutSeconds;
    ReconnectTestTickerHandle = FTSTicker::GetCoreTicker().AddTicker(
        FTickerDelegate::CreateWeakLambda(this, [this](float DeltaTime)
        {
            return TickSameProcessReconnectTest(DeltaTime);
        }),
        0.1f
    );
}

void UBHSessionSubsystem::LogSameProcessReconnectPhase(const TCHAR* Phase) const
{
    const auto ObjectIdentity = [](const UObject* Object)
    {
        if (!IsValid(Object))
        {
            return FString(TEXT("none"));
        }
        const FWeakObjectPtr Identity(Object);
        const int32 ObjectIndex = static_cast<int32>(Identity.Get()->GetUniqueID());
        return FString::Printf(TEXT("%d:%d"), ObjectIndex, GUObjectArray.GetSerialNumber(ObjectIndex));
    };
    UGameInstance* GameInstance = GetGameInstance();
    UWorld* World = IsValid(GameInstance) ? GameInstance->GetWorld() : nullptr;
    const UBHWarSubsystem* War = IsValid(GameInstance) ? GameInstance->GetSubsystem<UBHWarSubsystem>() : nullptr;
    const UNetDriver* Driver = IsValid(World) ? World->GetNetDriver() : nullptr;
    const UNetConnection* Connection = IsValid(Driver) ? Driver->ServerConnection.Get() : nullptr;
    const ABHWarGameState* State = IsValid(World) ? World->GetGameState<ABHWarGameState>() : nullptr;
    UE_LOG(LogTemp, Display, TEXT(
        "BH_TEST_SAME_PROCESS_RECONNECT phase=%s result=observed run_id=%s pid=%u "
        "game_instance=%s war_subsystem=%s connection=%s revision=%d turn=%d sectors=%d operation_id=%s"),
        Phase, *ReconnectTestRunID, FPlatformProcess::GetCurrentProcessId(),
        *ObjectIdentity(GameInstance), *ObjectIdentity(War), *ObjectIdentity(Connection),
        IsValid(State) ? State->GetWarStateRevision() : 0,
        IsValid(War) ? War->GetTurnNumber() : -1,
        IsValid(War) ? War->GetSectorStates().Num() : 0,
        IsValid(War) ? *War->GetCommittedOperationID().ToString() : TEXT("None"));
}

bool UBHSessionSubsystem::TickSameProcessReconnectTest(float DeltaTime)
{
    const auto Fail = [this](const TCHAR* Reason)
    {
        UE_LOG(LogTemp, Error, TEXT("BH_TEST_SAME_PROCESS_RECONNECT phase=failed result=failure run_id=%s reason=%s"),
            *ReconnectTestRunID, Reason);
        ReconnectTestTickerHandle.Reset();
        return false; // Returning false removes this core ticker, including on timeout.
    };
    if (FPlatformTime::Seconds() >= ReconnectTestDeadline)
    {
        return Fail(TEXT("timeout"));
    }

    UGameInstance* GameInstance = GetGameInstance();
    if (!IsValid(GameInstance) || ReconnectTestGameInstance != GameInstance)
    {
        return Fail(TEXT("game_instance_changed"));
    }
    UWorld* World = GameInstance->GetWorld();
    APlayerController* Controller = GameInstance->GetFirstLocalPlayerController();
    UBHWarSubsystem* War = GameInstance->GetSubsystem<UBHWarSubsystem>();
    if (ReconnectTestPhase != EReconnectTestPhase::AwaitLeave &&
        (!IsValid(War) || ReconnectTestWarSubsystem != War))
    {
        return Fail(TEXT("war_subsystem_changed"));
    }
    if (!IsValid(World) || !World->HasBegunPlay() || !IsValid(Controller) ||
        !Controller->IsLocalController() || Controller->GetWorld() != World || !IsValid(War))
    {
        return true;
    }
    const UNetDriver* Driver = World->GetNetDriver();
    UNetConnection* Connection = IsValid(Driver) ? Driver->ServerConnection.Get() : nullptr;
    const ABHWarGameState* State = World->GetGameState<ABHWarGameState>();
    const bool bConnected = World->GetNetMode() == NM_Client && IsValid(Connection) &&
        Connection->GetConnectionState() == USOCK_Open && IsValid(State) &&
        State->GetWarStateRevision() > 0 && !War->GetCommittedOperationID().IsNone();
    const UBHGameShellSettings* Settings = GetDefault<UBHGameShellSettings>();
    const bool bInMenu = IsValid(Settings) && !Settings->MainMenuMap.IsNull() &&
        World->GetOutermost()->GetName() == Settings->MainMenuMap.ToSoftObjectPath().GetLongPackageName() &&
        World->GetNetMode() == NM_Standalone && !IsValid(Driver);

    switch (ReconnectTestPhase)
    {
    case EReconnectTestPhase::AwaitLeave:
        if (bConnected && IFileManager::Get().FileExists(
                *FPaths::Combine(ReconnectTestControlDirectory, TEXT("leave.ready"))))
        {
            ReconnectTestWarSubsystem = War;
            ReconnectTestInitialConnection = Connection;
            // The harness correlates this received revision with the real successful apply log.
            LogSameProcessReconnectPhase(TEXT("before"));
            ReconnectTestPhase = EReconnectTestPhase::AwaitMenu;
            if (!LeaveSession())
            {
                return Fail(TEXT("leave_session_failed"));
            }
        }
        break;
    case EReconnectTestPhase::AwaitMenu:
        if (bInMenu)
        {
            LogSameProcessReconnectPhase(TEXT("menu"));
            ReconnectTestPhase = EReconnectTestPhase::AwaitReconnectSignal;
        }
        break;
    case EReconnectTestPhase::AwaitReconnectSignal:
        if (bInMenu && IFileManager::Get().FileExists(
                *FPaths::Combine(ReconnectTestControlDirectory, TEXT("reconnect.ready"))))
        {
            LogSameProcessReconnectPhase(TEXT("travel_requested"));
            ReconnectTestPhase = EReconnectTestPhase::AwaitReconnected;
            Controller->ClientTravel(ReconnectTestAddress, ETravelType::TRAVEL_Absolute);
        }
        break;
    case EReconnectTestPhase::AwaitReconnected:
        if (bConnected)
        {
            if (ReconnectTestInitialConnection == Connection)
            {
                return Fail(TEXT("connection_not_replaced"));
            }
            LogSameProcessReconnectPhase(TEXT("reconnected"));
            ReconnectTestTickerHandle.Reset();
            return false;
        }
        break;
    }
    return true;
}
#endif
