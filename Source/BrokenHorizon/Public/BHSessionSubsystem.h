#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Engine/EngineBaseTypes.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "OnlineSessionSettings.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "BHSessionSubsystem.generated.h"

class UEngine;
class UNetDriver;
class UNetConnection;
class UBHWarSubsystem;
enum class EBHLoadProgressResult : uint8;

UENUM(BlueprintType)
enum class EBHSessionState : uint8
{
    Idle,
    Hosting,
    Searching,
    Joining,
    Traveling,
    InSession,
    Leaving,
    Error
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
    FBHSessionStateChangedSignature,
    EBHSessionState,
    State,
    FText,
    Message
);

UCLASS()
class BROKENHORIZON_API UBHSessionSubsystem
    : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(
        FSubsystemCollectionBase& Collection
    ) override;

    virtual void Deinitialize() override;

    UFUNCTION(
        BlueprintCallable,
        Category = "Broken Horizon|Multiplayer"
    )
    bool HostCampaign(
        bool bContinueCampaign,
        int32 MaximumPlayers = 4,
        bool bLANMatch = true
    );

    UFUNCTION(
        BlueprintCallable,
        Category = "Broken Horizon|Multiplayer"
    )
    bool FindAndJoinCampaign(bool bLANMatch = true);

    UFUNCTION(
        BlueprintCallable,
        Category = "Broken Horizon|Multiplayer"
    )
    bool LeaveSession();

    UFUNCTION(
        BlueprintPure,
        Category = "Broken Horizon|Multiplayer"
    )
    EBHSessionState GetSessionState() const;
    FText GetSessionStatusMessage() const;

    UFUNCTION(
        BlueprintPure,
        Category = "Broken Horizon|Multiplayer"
    )
    bool IsSessionActionPending() const;

    UPROPERTY(
        BlueprintAssignable,
        Category = "Broken Horizon|Multiplayer"
    )
    FBHSessionStateChangedSignature OnSessionStateChanged;

private:
    enum class EPendingAction : uint8
    {
        None,
        Host,
        Join,
        Leave
    };

    bool ResolveSessionInterface();
    bool CreatePendingSession();
    bool BeginPendingSearch();
    bool TravelToCampaign();
    bool BeginCampaignTravel(UWorld* World, const FString& PackageName);
    void TrackPendingTravel(UWorld* World, ENetMode ExpectedMode, const FString& ExpectedPackage = FString());
    void ClearPendingTravel();
    bool IsCurrentSessionWorld(const UWorld* World) const;
    void BindTravelDelegates();
    void ClearTravelDelegates();
    void HandleTravelFailure(UWorld* World, ETravelFailure::Type FailureType, const FString& ErrorString);
    void HandleNetworkFailure(UWorld* World, UNetDriver* NetDriver, ENetworkFailure::Type FailureType, const FString& ErrorString);
    bool OpenMainMenu();
    void SetSessionState(
        EBHSessionState NewState,
        const FText& Message
    );
    void FailSessionAction(const FText& Message);
    void ClearSessionDelegates();
    void HandleCreateSessionComplete(
        FName SessionName,
        bool bWasSuccessful
    );
    void HandleFindSessionsComplete(bool bWasSuccessful);
    void HandleJoinSessionComplete(
        FName SessionName,
        EOnJoinSessionCompleteResult::Type Result
    );
    void HandleDestroySessionComplete(
        FName SessionName,
        bool bWasSuccessful
    );
    void HandlePostLoadMap(UWorld* LoadedWorld);
    void HandleContinueLoadComplete(FGuid RequestID, EBHLoadProgressResult Result, FName Reason, UWorld* AppliedWorld);
    void CancelPendingContinue();
    void ReturnToMenuAfterContinueFailure();
    FGuid PendingContinueRequestID;
    FText SessionStatusMessage;
    bool bContinueFailureMenuTravelPending = false;

#if !UE_BUILD_SHIPPING
    void StartSessionRecoveryTest();
    bool TickSessionRecoveryTest(float DeltaTime);
    void LogSessionRecoveryTest(const TCHAR* Phase, const TCHAR* Result, const FString& Detail = FString()) const;
    bool bSessionRecoveryTestRequested = false;
    bool bSessionRecoveryTestValid = false;
    bool bSessionRecoveryRejectNextTravel = false;
    bool bSessionRecoveryRejectedTravelObserved = false;
    bool bSessionRecoveryNoMatchObserved = false;
    bool bSessionRecoveryDisconnectObserved = false;
    FTSTicker::FDelegateHandle SessionRecoveryTickerHandle;
    TWeakObjectPtr<UGameInstance> SessionRecoveryGameInstance;
    TWeakObjectPtr<UNetConnection> SessionRecoveryInitialConnection;
    FString SessionRecoveryRunID;
    FString SessionRecoveryRole;
    FString SessionRecoveryControlDirectory;
    double SessionRecoveryDeadline = 0.0;
    int32 SessionRecoveryPhase = 0;

    void StartSameProcessReconnectTest();
    bool TickSameProcessReconnectTest(float DeltaTime);
    void LogSameProcessReconnectPhase(const TCHAR* Phase) const;

    FTSTicker::FDelegateHandle ReconnectTestTickerHandle;
    TWeakObjectPtr<UGameInstance> ReconnectTestGameInstance;
    TWeakObjectPtr<UBHWarSubsystem> ReconnectTestWarSubsystem;
    TWeakObjectPtr<UNetConnection> ReconnectTestInitialConnection;
    FString ReconnectTestRunID;
    FString ReconnectTestControlDirectory;
    FString ReconnectTestAddress;
    double ReconnectTestDeadline = 0.0;
    enum class EReconnectTestPhase : uint8
    {
        AwaitLeave,
        AwaitMenu,
        AwaitReconnectSignal,
        AwaitReconnected
    };
    EReconnectTestPhase ReconnectTestPhase = EReconnectTestPhase::AwaitLeave;
#endif

#if WITH_DEV_AUTOMATION_TESTS
    friend struct FBHSessionSubsystemTestAccess;
#endif

    TWeakObjectPtr<UEngine> BoundTravelEngine;
    TWeakObjectPtr<UWorld> PendingTravelOrigin;
    FString PendingTravelPackage;
    ENetMode PendingTravelMode = NM_Standalone;
    FDelegateHandle NetworkFailureDelegateHandle;
    FDelegateHandle TravelFailureDelegateHandle;
    IOnlineSessionPtr SessionInterface;
    TSharedPtr<FOnlineSessionSearch> SessionSearch;
    FDelegateHandle CreateSessionDelegateHandle;
    FDelegateHandle FindSessionsDelegateHandle;
    FDelegateHandle JoinSessionDelegateHandle;
    FDelegateHandle DestroySessionDelegateHandle;
    FDelegateHandle PostLoadMapDelegateHandle;
    EBHSessionState SessionState = EBHSessionState::Idle;
    EPendingAction PendingAction = EPendingAction::None;
    bool bPendingContinueCampaign = false;
    bool bLoadContinueAfterListenTravel = false;
    bool bPendingLANMatch = true;
    int32 PendingMaximumPlayers = 4;
};
