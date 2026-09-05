#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "OnlineSessionSettings.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "BHSessionSubsystem.generated.h"

class UNetConnection;
class UBHWarSubsystem;

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
        Leave
    };

    bool ResolveSessionInterface();
    bool CreatePendingSession();
    bool TravelToCampaign();
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

#if !UE_BUILD_SHIPPING
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
