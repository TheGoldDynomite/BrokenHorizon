#include "BHSessionSubsystem.h"

#include "BHGameShellSettings.h"
#include "BHSaveSubsystem.h"
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
}

void UBHSessionSubsystem::Deinitialize()
{
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

