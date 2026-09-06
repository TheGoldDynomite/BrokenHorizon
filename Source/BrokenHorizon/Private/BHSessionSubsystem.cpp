#include "BHSessionSubsystem.h"

#include "BHGameShellSettings.h"
#include "BHSaveSubsystem.h"
#if !UE_BUILD_SHIPPING
#include "BHWarGameState.h"
#include "BHMainMenuWidget.h"
#include "BHCharacter.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "GameFramework/PlayerState.h"
#include "UObject/UnrealType.h"
#include "BHWarSubsystem.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "String/LexFromString.h"
#include "UObject/UObjectArray.h"
#endif
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/NetConnection.h"
#include "Engine/NetDriver.h"
#include "Engine/PendingNetGame.h"
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
    BindTravelDelegates();
#if !UE_BUILD_SHIPPING
    StartSessionRecoveryTest();
    StartSameProcessReconnectTest();
#endif
}

void UBHSessionSubsystem::Deinitialize()
{
    CancelPendingContinue();
#if !UE_BUILD_SHIPPING
    FTSTicker::GetCoreTicker().RemoveTicker(SessionRecoveryTickerHandle);
    SessionRecoveryTickerHandle.Reset();
    FTSTicker::GetCoreTicker().RemoveTicker(ReconnectTestTickerHandle);
    ReconnectTestTickerHandle.Reset();
#endif
    ClearSessionDelegates();

    ClearTravelDelegates();
    ClearPendingTravel();

    SessionSearch.Reset();
    SessionInterface.Reset();
    Super::Deinitialize();
}

bool UBHSessionSubsystem::IsCurrentSessionWorld(const UWorld* World) const
{
    const UGameInstance* GameInstance = GetGameInstance();
    return IsValid(GameInstance) && IsValid(World) && World->GetGameInstance() == GameInstance &&
        GameInstance->GetWorld() == World;
}

void UBHSessionSubsystem::TrackPendingTravel(UWorld* World, ENetMode ExpectedMode, const FString& ExpectedPackage)
{
    PendingTravelOrigin = World;
    PendingTravelMode = ExpectedMode;
    PendingTravelPackage = ExpectedPackage;
}

void UBHSessionSubsystem::ClearPendingTravel()
{
    PendingTravelOrigin.Reset();
    PendingTravelPackage.Reset();
    PendingTravelMode = NM_Standalone;
}

void UBHSessionSubsystem::BindTravelDelegates()
{
    ClearTravelDelegates();
    PostLoadMapDelegateHandle = FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &UBHSessionSubsystem::HandlePostLoadMap);
    if (IsValid(GEngine))
    {
        BoundTravelEngine = GEngine;
        TravelFailureDelegateHandle = GEngine->OnTravelFailure().AddUObject(this, &UBHSessionSubsystem::HandleTravelFailure);
        NetworkFailureDelegateHandle = GEngine->OnNetworkFailure().AddUObject(this, &UBHSessionSubsystem::HandleNetworkFailure);
    }
}

void UBHSessionSubsystem::ClearTravelDelegates()
{
    FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(PostLoadMapDelegateHandle);
    PostLoadMapDelegateHandle.Reset();
    if (UEngine* Engine = BoundTravelEngine.Get())
    {
        Engine->OnTravelFailure().Remove(TravelFailureDelegateHandle);
        Engine->OnNetworkFailure().Remove(NetworkFailureDelegateHandle);
    }
    TravelFailureDelegateHandle.Reset();
    NetworkFailureDelegateHandle.Reset();
    BoundTravelEngine.Reset();
}

void UBHSessionSubsystem::HandleTravelFailure(UWorld* World, ETravelFailure::Type FailureType, const FString& ErrorString)
{
    if (!IsCurrentSessionWorld(World) || !IsSessionActionPending() || SessionState == EBHSessionState::Leaving)
    {
        return;
    }
    UE_LOG(LogTemp, Warning, TEXT("BH_SESSION_TRAVEL_FAILURE type=%d detail=%s"), static_cast<int32>(FailureType), *ErrorString);
    FailSessionAction(NSLOCTEXT("BrokenHorizon", "SessionTravelFailure", "Campaign travel failed. Please try again."));
}

void UBHSessionSubsystem::HandleNetworkFailure(UWorld* World, UNetDriver* NetDriver, ENetworkFailure::Type FailureType, const FString& ErrorString)
{
    if (!IsValid(NetDriver) || SessionState == EBHSessionState::Idle || SessionState == EBHSessionState::Error || SessionState == EBHSessionState::Leaving)
    {
        return;
    }
    const UGameInstance* GameInstance = GetGameInstance();
    const FWorldContext* Context = IsValid(GameInstance) ? GameInstance->GetWorldContext() : nullptr;
    const bool bOwnedCurrentDriver = IsCurrentSessionWorld(World) && World->GetNetDriver() == NetDriver;
    // Pending connection failures legitimately carry no UWorld. Match the exact
    // pending driver in this GameInstance's context, never a process-global guess.
    const bool bOwnedPendingDriver = Context && Context->OwningGameInstance == GameInstance &&
        IsValid(Context->PendingNetGame) && Context->PendingNetGame->NetDriver == NetDriver &&
        (!World || IsCurrentSessionWorld(World));
    if (!bOwnedCurrentDriver && !bOwnedPendingDriver)
    {
        return;
    }
    if (SessionState == EBHSessionState::InSession &&
        (!bOwnedCurrentDriver || World->GetNetMode() != NM_Client))
    {
        return;
    }
    // A listen host losing one remote participant has not lost its own session.
    if (bOwnedCurrentDriver && NetDriver->GetNetMode() != NM_Client &&
        (FailureType == ENetworkFailure::ConnectionLost || FailureType == ENetworkFailure::ConnectionTimeout ||
            FailureType == ENetworkFailure::NetGuidMismatch || FailureType == ENetworkFailure::NetChecksumMismatch))
    {
        return;
    }
#if !UE_BUILD_SHIPPING
    if (bSessionRecoveryTestValid && SessionState == EBHSessionState::InSession && bOwnedCurrentDriver)
    {
        bSessionRecoveryDisconnectObserved = true;
    }
#endif
    UE_LOG(LogTemp, Warning, TEXT("BH_SESSION_NETWORK_FAILURE type=%d detail=%s"), static_cast<int32>(FailureType), *ErrorString);
    FailSessionAction(NSLOCTEXT("BrokenHorizon", "SessionNetworkFailure", "Campaign connection failed. Please try again."));
}

bool UBHSessionSubsystem::HostCampaign(
    bool bContinueCampaign,
    int32 MaximumPlayers,
    bool bLANMatch
)
{
#if !UE_BUILD_SHIPPING
    if (bSessionRecoveryTestRequested && !bSessionRecoveryTestValid)
    {
        FailSessionAction(NSLOCTEXT("BrokenHorizon", "SessionFixtureInvalid", "Session acceptance configuration is invalid."));
        return false;
    }
#endif
    if (IsSessionActionPending() || !ResolveSessionInterface())
    {
        return false;
    }

    if (bContinueCampaign)
    {
        UBHSaveSubsystem* Save = GetGameInstance() ? GetGameInstance()->GetSubsystem<UBHSaveSubsystem>() : nullptr;
        const FGuid RequestID = FGuid::NewGuid();
        if (!IsValid(Save) || !Save->PrepareLoadProgress(RequestID,
            FBHLoadProgressCompletion::CreateUObject(this, &UBHSessionSubsystem::HandleContinueLoadComplete)))
        {
            FailSessionAction(NSLOCTEXT("BrokenHorizon", "ContinuePrepareFailed", "Checkpoint could not be prepared. Please try again."));
            return false;
        }
        PendingContinueRequestID = RequestID;
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
#if !UE_BUILD_SHIPPING
    if (bSessionRecoveryTestRequested && !bSessionRecoveryTestValid)
    {
        FailSessionAction(NSLOCTEXT("BrokenHorizon", "SessionFixtureInvalid", "Session acceptance configuration is invalid."));
        return false;
    }
#endif
    if (IsSessionActionPending() || !ResolveSessionInterface())
    {
        return false;
    }

    PendingAction = EPendingAction::Join;
    bPendingLANMatch = bLANMatch;
    SetSessionState(EBHSessionState::Searching, NSLOCTEXT("BrokenHorizon", "SessionPreparingSearch", "Preparing campaign search..."));
    if (SessionInterface->GetNamedSession(NAME_GameSession) != nullptr)
    {
        DestroySessionDelegateHandle = SessionInterface->AddOnDestroySessionCompleteDelegate_Handle(
            FOnDestroySessionCompleteDelegate::CreateUObject(this, &UBHSessionSubsystem::HandleDestroySessionComplete));
        if (!SessionInterface->DestroySession(NAME_GameSession))
        {
            FailSessionAction(NSLOCTEXT("BrokenHorizon", "SessionDestroyBeforeJoinFailed", "The previous campaign session could not be closed before joining."));
            return false;
        }
        return SessionState != EBHSessionState::Error;
    }
    return BeginPendingSearch();
}

bool UBHSessionSubsystem::BeginPendingSearch()
{
    if (!SessionInterface.IsValid())
    {
        FailSessionAction(NSLOCTEXT("BrokenHorizon", "SessionSearchInterfaceLost", "The online session service became unavailable before search."));
        return false;
    }
    PendingAction = EPendingAction::None;
    SessionSearch = MakeShared<FOnlineSessionSearch>();
    SessionSearch->bIsLanQuery = bPendingLANMatch;
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

FText UBHSessionSubsystem::GetSessionStatusMessage() const
{
    return SessionStatusMessage.IsEmpty()
        ? NSLOCTEXT("BrokenHorizon", "SessionReadyHelp", "Host a campaign or join a LAN campaign.") : SessionStatusMessage;
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

#if !UE_BUILD_SHIPPING
    if (bSessionRecoveryTestValid)
    {
        Settings.Set(FName(TEXT("BHCampaignTestRun")), SessionRecoveryRunID, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
    }
#endif

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

#if !UE_BUILD_SHIPPING
    if (bSessionRecoveryTestValid && bSessionRecoveryRejectNextTravel)
    {
        bSessionRecoveryRejectNextTravel = false;
        return BeginCampaignTravel(World, TEXT("/Game/BH%SessionRejected"));
    }
#endif
    return BeginCampaignTravel(World, PackageName);
}

bool UBHSessionSubsystem::BeginCampaignTravel(UWorld* World, const FString& PackageName)
{
    if (!IsCurrentSessionWorld(World) || PackageName.IsEmpty())
    {
        FailSessionAction(NSLOCTEXT("BrokenHorizon", "SessionTravelInvalid", "The campaign travel target is unavailable."));
        return false;
    }
    TrackPendingTravel(World, NM_ListenServer, PackageName);
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
    if (!World->ServerTravel(PackageName + TEXT("?listen")))
    {
#if !UE_BUILD_SHIPPING
        if (bSessionRecoveryTestValid && PackageName == TEXT("/Game/BH%SessionRejected"))
        {
            bSessionRecoveryRejectedTravelObserved = true;
            LogSessionRecoveryTest(TEXT("server_travel_rejected"), TEXT("observed"), TEXT("accepted=0"));
        }
#endif
        FailSessionAction(NSLOCTEXT("BrokenHorizon", "SessionTravelRejected", "The shared campaign travel request was rejected."));
        return false;
    }
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

    CancelPendingContinue();
    ClearPendingTravel();
    bPendingContinueCampaign = false;
    bLoadContinueAfterListenTravel = false;
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
    SessionStatusMessage = Message;
    OnSessionStateChanged.Broadcast(NewState, Message);
}

void UBHSessionSubsystem::FailSessionAction(const FText& Message)
{
    const bool bFailedContinue = PendingContinueRequestID.IsValid();
    CancelPendingContinue();
    ClearSessionDelegates();
    SessionSearch.Reset();
    PendingAction = EPendingAction::None;
    bPendingContinueCampaign = false;
    bLoadContinueAfterListenTravel = false;
    ClearPendingTravel();
    SetSessionState(EBHSessionState::Error, Message);
    UE_LOG(
        LogTemp,
        Error,
        TEXT("BH_SESSION_ERROR %s"),
        *Message.ToString()
    );
    if (bFailedContinue) { ReturnToMenuAfterContinueFailure(); }
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
    if (SessionName != NAME_GameSession || SessionState != EBHSessionState::Hosting || PendingAction != EPendingAction::Host || !CreateSessionDelegateHandle.IsValid()) { return; }

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

    if (!TravelToCampaign() && SessionState != EBHSessionState::Error)
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
    if (SessionState != EBHSessionState::Searching || !FindSessionsDelegateHandle.IsValid()) { return; }

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
#if !UE_BUILD_SHIPPING
            if (bSessionRecoveryTestValid)
            {
                FString AdvertisedRunID;
                if (!Result.Session.SessionSettings.Get(FName(TEXT("BHCampaignTestRun")), AdvertisedRunID) || AdvertisedRunID != SessionRecoveryRunID)
                {
                    continue; // Fixture searches never fall back to unrelated LAN campaigns.
                }
            }
#endif
            SelectedResult = &Result;
            break;
        }
    }

    if (!SelectedResult)
    {
#if !UE_BUILD_SHIPPING
        bSessionRecoveryNoMatchObserved = bSessionRecoveryTestValid;
#endif
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
    if (SessionName != NAME_GameSession || SessionState != EBHSessionState::Joining || !JoinSessionDelegateHandle.IsValid()) { return; }

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

#if !UE_BUILD_SHIPPING
    if (bSessionRecoveryTestValid)
    {
        LogSessionRecoveryTest(TEXT("resolved_join"), TEXT("observed"), FString::Printf(TEXT("address=%s"), *ConnectString));
    }
#endif
    TrackPendingTravel(PlayerController->GetWorld(), NM_Client);
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
    if (SessionName != NAME_GameSession || !DestroySessionDelegateHandle.IsValid() ||
        !((PendingAction == EPendingAction::Host && SessionState == EBHSessionState::Hosting) ||
          (PendingAction == EPendingAction::Join && SessionState == EBHSessionState::Searching) ||
          (PendingAction == EPendingAction::Leave && SessionState == EBHSessionState::Leaving))) { return; }

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

    if (CompletedAction == EPendingAction::Join)
    {
        BeginPendingSearch();
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

void UBHSessionSubsystem::CancelPendingContinue()
{
    const FGuid RequestID = PendingContinueRequestID;
    PendingContinueRequestID.Invalidate();
    if (RequestID.IsValid())
    {
        if (UBHSaveSubsystem* Save = GetGameInstance() ? GetGameInstance()->GetSubsystem<UBHSaveSubsystem>() : nullptr)
        { Save->CancelLoadProgress(RequestID); }
    }
}

void UBHSessionSubsystem::ReturnToMenuAfterContinueFailure()
{
    // Error listeners may synchronously begin a newer action. Its travel wins.
    if (SessionState != EBHSessionState::Error || IsSessionActionPending() || PendingContinueRequestID.IsValid()) { return; }
    UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
    const UBHGameShellSettings* Settings = GetDefault<UBHGameShellSettings>();
    if (!IsCurrentSessionWorld(World) || World->bIsTearingDown || !IsValid(Settings) || Settings->MainMenuMap.IsNull()) { return; }
    const FString Package = Settings->MainMenuMap.ToSoftObjectPath().GetLongPackageName();
    if (Package.IsEmpty() || bContinueFailureMenuTravelPending ||
        UWorld::RemovePIEPrefix(World->GetOutermost()->GetName()) == Package) { return; }
    bContinueFailureMenuTravelPending = true;
    // Keep Error and its message through the new menu's construction.
    UGameplayStatics::OpenLevel(this, FName(*Package));
}

void UBHSessionSubsystem::HandleContinueLoadComplete(FGuid RequestID, EBHLoadProgressResult Result, FName Reason, UWorld* AppliedWorld)
{
    if (!RequestID.IsValid() || PendingContinueRequestID != RequestID) { return; }
    PendingContinueRequestID.Invalidate();
    if (Result == EBHLoadProgressResult::Applied && SessionState == EBHSessionState::Traveling &&
        IsCurrentSessionWorld(AppliedWorld) && AppliedWorld->GetNetMode() == NM_ListenServer)
    {
        ClearPendingTravel();
        bPendingContinueCampaign = false;
        bLoadContinueAfterListenTravel = false;
        SetSessionState(EBHSessionState::InSession, NSLOCTEXT("BrokenHorizon", "ContinueRestored", "Campaign checkpoint restored."));
        UE_LOG(LogTemp, Display, TEXT("BH_SESSION_CONTINUE_APPLIED request=%s world=%s"), *RequestID.ToString(), *GetNameSafe(AppliedWorld));
        return;
    }
    UE_LOG(LogTemp, Warning, TEXT("BH_SESSION_CONTINUE_FAILED request=%s result=%d reason=%s"), *RequestID.ToString(), static_cast<int32>(Result), *Reason.ToString());
    FailSessionAction(NSLOCTEXT("BrokenHorizon", "ContinueRestoreFailed", "Checkpoint restoration failed. Your checkpoint is protected. Please try again."));
    ReturnToMenuAfterContinueFailure();
}

void UBHSessionSubsystem::HandlePostLoadMap(UWorld* LoadedWorld)
{
    if (IsCurrentSessionWorld(LoadedWorld)) { bContinueFailureMenuTravelPending = false; }
    if (SessionState != EBHSessionState::Traveling || !IsCurrentSessionWorld(LoadedWorld) ||
        PendingTravelMode == NM_Standalone || LoadedWorld->GetNetMode() != PendingTravelMode ||
        PendingTravelOrigin.Get() == LoadedWorld ||
        (!PendingTravelPackage.IsEmpty() && UWorld::RemovePIEPrefix(LoadedWorld->GetOutermost()->GetName()) != PendingTravelPackage)) { return; }
    if (PendingContinueRequestID.IsValid())
    {
        if (bLoadContinueAfterListenTravel)
        {
            bLoadContinueAfterListenTravel = false;
            PendingTravelOrigin.Reset();
            PendingTravelPackage.Reset();
            UBHSaveSubsystem* Save = GetGameInstance()->GetSubsystem<UBHSaveSubsystem>();
            if (!IsValid(Save) || !Save->StartPreparedLoad(PendingContinueRequestID))
            {
                // Rejection can synchronously complete and report the failure already.
                if (PendingContinueRequestID.IsValid())
                { FailSessionAction(NSLOCTEXT("BrokenHorizon", "ContinueStartFailed", "Checkpoint restoration could not start. Please try again.")); }
            }
        }
        return; // Map readiness is not proof that checkpoint data was applied.
    }
    ClearPendingTravel();
    bPendingContinueCampaign = false;
    SetSessionState(EBHSessionState::InSession, NSLOCTEXT("BrokenHorizon", "SessionConnected", "Connected to campaign."));
}


#if !UE_BUILD_SHIPPING
namespace
{
UBHMainMenuWidget* FindSessionRecoveryMenu(UGameInstance* GameInstance)
{
    if (!IsValid(GameInstance)) { return nullptr; }
    TArray<UUserWidget*> Widgets;
    UWidgetBlueprintLibrary::GetAllWidgetsOfClass(GameInstance, Widgets, UBHMainMenuWidget::StaticClass(), false);
    for (UUserWidget* Widget : Widgets)
    {
        if (IsValid(Widget) && Widget->GetWorld() == GameInstance->GetWorld() && Widget->IsInViewport() && Widget->IsVisible() &&
            Widget->GetOwningPlayer() == GameInstance->GetFirstLocalPlayerController())
        {
            return Cast<UBHMainMenuWidget>(Widget);
        }
    }
    return nullptr;
}
template<typename T> T* GetSessionMenuField(UBHMainMenuWidget* Menu, const TCHAR* Name)
{
    const FObjectPropertyBase* Property = IsValid(Menu) ? FindFProperty<FObjectPropertyBase>(Menu->GetClass(), Name) : nullptr;
    return Property ? Cast<T>(Property->GetObjectPropertyValue_InContainer(Menu)) : nullptr;
}
bool IsSessionMenuActionable(UBHMainMenuWidget* Menu)
{
    const UButton* Host = GetSessionMenuField<UButton>(Menu, TEXT("NewGameButton"));
    const UButton* Join = GetSessionMenuField<UButton>(Menu, TEXT("JoinCampaignButton"));
    return IsValid(Menu) && Menu->GetIsEnabled() && IsValid(Host) && IsValid(Join) &&
        Host->GetIsEnabled() && Join->GetIsEnabled() && Host->IsVisible() && Join->IsVisible();
}
FString SessionRecoveryIdentity(const UObject* Object)
{
    if (!IsValid(Object)) { return TEXT("none"); }
    const int32 Index = static_cast<int32>(Object->GetUniqueID());
    return FString::Printf(TEXT("%d:%d"), Index, GUObjectArray.GetSerialNumber(Index));
}
}

void UBHSessionSubsystem::StartSessionRecoveryTest()
{
    bSessionRecoveryTestRequested = FParse::Param(FCommandLine::Get(), TEXT("BHTestSessionRecovery"));
    if (!bSessionRecoveryTestRequested) { return; }
    FString UserDirectory, SaveSuffix, TimeoutText;
    FParse::Value(FCommandLine::Get(), TEXT("BHTestSessionRunId="), SessionRecoveryRunID);
    FParse::Value(FCommandLine::Get(), TEXT("BHTestSessionRole="), SessionRecoveryRole);
    FParse::Value(FCommandLine::Get(), TEXT("BHTestSessionTimeout="), TimeoutText);
    FParse::Value(FCommandLine::Get(), TEXT("UserDir="), UserDirectory);
    FParse::Value(FCommandLine::Get(), TEXT("BHTestSaveSlotSuffix="), SaveSuffix);
    bool bSafe = !SessionRecoveryRunID.IsEmpty() && SessionRecoveryRunID.Len() <= 48;
    for (TCHAR Character : SessionRecoveryRunID)
    {
        bSafe &= (FChar::IsAlnum(Character) && Character < 128) || Character == TEXT('_') || Character == TEXT('-');
    }
    bSafe &= SessionRecoveryRole == TEXT("Host") || SessionRecoveryRole == TEXT("Client") || SessionRecoveryRole == TEXT("RestartHost");
    int32 TimeoutSeconds = 600;
    if (!TimeoutText.IsEmpty())
    {
        for (TCHAR Character : TimeoutText) { bSafe &= Character >= TEXT('0') && Character <= TEXT('9'); }
        bSafe &= TimeoutText.Len() <= 4 && LexTryParseString(TimeoutSeconds, *TimeoutText);
    }
    bSafe &= TimeoutSeconds >= 120 && TimeoutSeconds <= 900;
    const FString ExpectedSuffix = TEXT("SessionRecovery_") + SessionRecoveryRunID.Replace(TEXT("-"), TEXT("_")) + TEXT("_") + SessionRecoveryRole;
    FPaths::NormalizeDirectoryName(UserDirectory);
    bSafe &= !UserDirectory.IsEmpty() && UserDirectory.EndsWith(TEXT("/") + SessionRecoveryRunID + TEXT("/") + SessionRecoveryRole + TEXT("/User")) && SaveSuffix == ExpectedSuffix;
    const FString SavedDirectory = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir());
    bSafe &= !UserDirectory.IsEmpty() && FPaths::IsUnderDirectory(SavedDirectory, FPaths::ConvertRelativePathToFull(UserDirectory));
    if (!bSafe)
    {
        LogSessionRecoveryTest(TEXT("failed"), TEXT("failure"), TEXT("invalid_isolation_arguments"));
        return;
    }
    bSessionRecoveryTestValid = true;
    SessionRecoveryGameInstance = GetGameInstance();
    SessionRecoveryControlDirectory = FPaths::Combine(SavedDirectory, TEXT("Automation/SessionRecovery"), SessionRecoveryRunID);
    IFileManager::Get().MakeDirectory(*SessionRecoveryControlDirectory, true);
    SessionRecoveryDeadline = FPlatformTime::Seconds() + TimeoutSeconds;
    SessionRecoveryTickerHandle = FTSTicker::GetCoreTicker().AddTicker(
        FTickerDelegate::CreateWeakLambda(this, [this](float DeltaTime) { return TickSessionRecoveryTest(DeltaTime); }), 0.1f);
}

void UBHSessionSubsystem::LogSessionRecoveryTest(const TCHAR* Phase, const TCHAR* Result, const FString& Detail) const
{
    UGameInstance* GameInstance = GetGameInstance();
    UWorld* World = IsValid(GameInstance) ? GameInstance->GetWorld() : nullptr;
    APlayerController* Local = IsValid(GameInstance) ? GameInstance->GetFirstLocalPlayerController() : nullptr;
    const UNetDriver* Driver = IsValid(World) ? World->GetNetDriver() : nullptr;
    const UNetConnection* Connection = IsValid(Driver) ? Driver->ServerConnection.Get() : nullptr;
    UBHMainMenuWidget* Menu = FindSessionRecoveryMenu(GameInstance);
    const UTextBlock* Status = GetSessionMenuField<UTextBlock>(Menu, TEXT("SessionStatusText"));
    const UButton* HostButton = GetSessionMenuField<UButton>(Menu, TEXT("NewGameButton"));
    const UButton* JoinButton = GetSessionMenuField<UButton>(Menu, TEXT("JoinCampaignButton"));
    FString StatusText = IsValid(Status) ? Status->GetText().ToString() : TEXT("");
    StatusText.ReplaceInline(TEXT("\n"), TEXT(" | ")); StatusText.ReplaceInline(TEXT("\r"), TEXT("")); StatusText.ReplaceInline(TEXT("\""), TEXT("'"));
    int32 Players = 0, RemoteOpen = 0;
    if (IsValid(World))
    {
        for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
        {
            APlayerController* Player = It->Get();
            if (IsValid(Player) && IsValid(Cast<ABHCharacter>(Player->GetPawn())) && IsValid(Player->PlayerState))
            {
                ++Players;
                const UNetConnection* Remote = Player->GetNetConnection();
                if (IsValid(Remote) && Remote->GetConnectionState() == USOCK_Open) { ++RemoteOpen; }
            }
        }
    }
    UE_LOG(LogTemp, Display, TEXT("BH_TEST_SESSION_RECOVERY run_id=%s role=%s phase=%s result=%s pid=%u state=%d pending=%d named_session=%d net_mode=%d players=%d remote_open=%d local_possessed=%d connection_open=%d game_instance=%s connection=%s widget=%d actionable=%d host_enabled=%d join_enabled=%d status=\"%s\" control_dir=\"%s\" detail=\"%s\""),
        *SessionRecoveryRunID, *SessionRecoveryRole, Phase, Result, FPlatformProcess::GetCurrentProcessId(), static_cast<int32>(SessionState), IsSessionActionPending() ? 1 : 0,
        SessionInterface.IsValid() && SessionInterface->GetNamedSession(NAME_GameSession) ? 1 : 0,
        IsValid(World) ? static_cast<int32>(World->GetNetMode()) : -1, Players, RemoteOpen,
        IsValid(Local) && IsValid(Cast<ABHCharacter>(Local->GetPawn())) && IsValid(Local->PlayerState) ? 1 : 0,
        IsValid(Connection) && Connection->GetConnectionState() == USOCK_Open ? 1 : 0,
        *SessionRecoveryIdentity(GameInstance), *SessionRecoveryIdentity(Connection), IsValid(Menu) ? 1 : 0, IsSessionMenuActionable(Menu) ? 1 : 0,
        IsValid(HostButton) && HostButton->GetIsEnabled() ? 1 : 0, IsValid(JoinButton) && JoinButton->GetIsEnabled() ? 1 : 0,
        *StatusText, *SessionRecoveryControlDirectory, *Detail);
}

bool UBHSessionSubsystem::TickSessionRecoveryTest(float DeltaTime)
{
    const auto Fail = [this](const TCHAR* Reason)
    {
        LogSessionRecoveryTest(TEXT("failed"), TEXT("failure"), Reason);
        SessionRecoveryTickerHandle.Reset();
        return false;
    };
    if (FPlatformTime::Seconds() >= SessionRecoveryDeadline) { return Fail(TEXT("timeout")); }
    UGameInstance* GameInstance = GetGameInstance();
    if (!IsValid(GameInstance) || SessionRecoveryGameInstance != GameInstance) { return Fail(TEXT("game_instance_changed")); }
    UWorld* World = GameInstance->GetWorld();
    APlayerController* Local = GameInstance->GetFirstLocalPlayerController();
    if (!IsCurrentSessionWorld(World) || !World->HasBegunPlay() || !IsValid(Local) || Local->GetWorld() != World || !Local->IsLocalController()) { return true; }
    UBHMainMenuWidget* Menu = FindSessionRecoveryMenu(GameInstance);
    const bool bActionable = IsSessionMenuActionable(Menu) && !IsSessionActionPending();
    const auto Signal = [this](const TCHAR* Name) { return IFileManager::Get().FileExists(*FPaths::Combine(SessionRecoveryControlDirectory, Name)); };
    const auto InvokeMenu = [this, Menu, bActionable](const TCHAR* ButtonName, const TCHAR* Phase)
    {
        UButton* Button = GetSessionMenuField<UButton>(Menu, ButtonName);
        if (!bActionable || !IsValid(Button) || !Button->OnClicked.IsBound()) { return false; }
        LogSessionRecoveryTest(Phase, TEXT("requested"));
        Button->OnClicked.Broadcast();
        return true;
    };
    UNetDriver* Driver = World->GetNetDriver();
    UNetConnection* Connection = IsValid(Driver) ? Driver->ServerConnection.Get() : nullptr;
    const bool bLocalPossessed = IsValid(Cast<ABHCharacter>(Local->GetPawn())) && IsValid(Local->PlayerState);
    const bool bJoined = SessionState == EBHSessionState::InSession && World->GetNetMode() == NM_Client &&
        bLocalPossessed && IsValid(Connection) && Connection->GetConnectionState() == USOCK_Open;
    if (SessionRecoveryRole == TEXT("Client"))
    {
        switch (SessionRecoveryPhase)
        {
        case 0:
            if (bActionable) { LogSessionRecoveryTest(TEXT("ready"), TEXT("observed")); SessionRecoveryPhase = 1; }
            break;
        case 1:
            if (Signal(TEXT("search.ready")) && InvokeMenu(TEXT("JoinCampaignButton"), TEXT("search"))) { SessionRecoveryPhase = 2; }
            break;
        case 2:
            if (SessionState == EBHSessionState::Error && bActionable && bSessionRecoveryNoMatchObserved) { LogSessionRecoveryTest(TEXT("no_match"), TEXT("observed")); SessionRecoveryPhase = 3; }
            break;
        case 3:
            if (Signal(TEXT("join.ready")) && InvokeMenu(TEXT("JoinCampaignButton"), TEXT("join"))) { SessionRecoveryPhase = 4; }
            break;
        case 4:
            if (bJoined) { SessionRecoveryInitialConnection = Connection; LogSessionRecoveryTest(TEXT("joined"), TEXT("observed")); SessionRecoveryPhase = 5; }
            break;
        case 5:
            if (SessionState == EBHSessionState::Error && bActionable && bSessionRecoveryDisconnectObserved)
            { LogSessionRecoveryTest(TEXT("disconnected"), TEXT("observed")); SessionRecoveryPhase = 6; }
            break;
        case 6:
            if (Signal(TEXT("retry.ready")) && InvokeMenu(TEXT("JoinCampaignButton"), TEXT("retry"))) { SessionRecoveryPhase = 7; }
            break;
        case 7:
            if (bJoined)
            {
                if (SessionRecoveryInitialConnection == Connection) { return Fail(TEXT("connection_not_replaced")); }
                LogSessionRecoveryTest(TEXT("rejoined"), TEXT("observed")); SessionRecoveryTickerHandle.Reset(); return false;
            }
            break;
        }
    }
    else
    {
        switch (SessionRecoveryPhase)
        {
        case 0:
            if (bActionable) { LogSessionRecoveryTest(TEXT("ready"), TEXT("observed")); SessionRecoveryPhase = SessionRecoveryRole == TEXT("Host") ? 1 : 3; }
            break;
        case 1:
            if (Signal(TEXT("reject.ready")) && bActionable)
            {
                bSessionRecoveryRejectNextTravel = true;
                if (InvokeMenu(TEXT("NewGameButton"), TEXT("reject"))) { SessionRecoveryPhase = 2; }
            }
            break;
        case 2:
            if (SessionState == EBHSessionState::Error && bActionable && bSessionRecoveryRejectedTravelObserved)
            { LogSessionRecoveryTest(TEXT("rejected"), TEXT("observed")); SessionRecoveryPhase = 3; }
            break;
        case 3:
            if (Signal(TEXT("host.ready")) && InvokeMenu(TEXT("NewGameButton"), TEXT("host"))) { SessionRecoveryPhase = 4; }
            break;
        case 4:
            if (SessionState == EBHSessionState::InSession && World->GetNetMode() == NM_ListenServer && bLocalPossessed)
            { LogSessionRecoveryTest(TEXT("hosted"), TEXT("observed")); SessionRecoveryPhase = 5; }
            break;
        case 5:
            if (IsValid(Driver) && Driver->ClientConnections.Num() == 1 &&
                IsValid(Driver->ClientConnections[0]) && Driver->ClientConnections[0]->GetConnectionState() == USOCK_Open)
            {
                const APlayerController* Remote = Driver->ClientConnections[0]->PlayerController;
                if (IsValid(Remote) && IsValid(Cast<ABHCharacter>(Remote->GetPawn())) && IsValid(Remote->PlayerState))
                { LogSessionRecoveryTest(TEXT("participants"), TEXT("observed")); SessionRecoveryTickerHandle.Reset(); return false; }
            }
            break;
        }
    }
    return true;
}

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
