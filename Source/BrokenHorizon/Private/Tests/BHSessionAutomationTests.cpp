#if WITH_DEV_AUTOMATION_TESTS

#include "BHMainMenuWidget.h"
#include "BHSessionSubsystem.h"
#include "BHSaveSubsystem.h"
#include "BHReplicationTestGameInstance.h"
#include "Engine/Engine.h"
#include "Engine/NetConnection.h"
#include "Engine/PendingNetGame.h"
#include "Engine/World.h"
#include "GameFramework/GameModeBase.h"
#include "IpNetDriver.h"
#include "Online/OnlineSessionNames.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UObjectGlobals.h"
#include "Misc/AutomationTest.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBHMultiplayerSessionContractTest,
    "BrokenHorizon.Multiplayer.Session.Contract",
    EAutomationTestFlags::EditorContext |
        EAutomationTestFlags::EngineFilter
)

bool FBHMultiplayerSessionContractTest::RunTest(
    const FString& Parameters
)
{
    const UBHSessionSubsystem* SessionDefaults =
        GetDefault<UBHSessionSubsystem>();

    TestNotNull(
        TEXT("Session subsystem has class defaults"),
        SessionDefaults
    );

    if (!IsValid(SessionDefaults))
    {
        return false;
    }

    TestEqual(
        TEXT("Session subsystem starts idle"),
        SessionDefaults->GetSessionState(),
        EBHSessionState::Idle
    );

    const UFunction* HostFunction =
        UBHSessionSubsystem::StaticClass()->FindFunctionByName(
            GET_FUNCTION_NAME_CHECKED(
                UBHSessionSubsystem,
                HostCampaign
            )
        );
    const UFunction* JoinFunction =
        UBHSessionSubsystem::StaticClass()->FindFunctionByName(
            GET_FUNCTION_NAME_CHECKED(
                UBHSessionSubsystem,
                FindAndJoinCampaign
            )
        );
    const UFunction* LeaveFunction =
        UBHSessionSubsystem::StaticClass()->FindFunctionByName(
            GET_FUNCTION_NAME_CHECKED(
                UBHSessionSubsystem,
                LeaveSession
            )
        );

    TestTrue(
        TEXT("Host campaign is Blueprint callable"),
        IsValid(HostFunction) &&
            HostFunction->HasAnyFunctionFlags(FUNC_BlueprintCallable)
    );
    TestTrue(
        TEXT("Join campaign is Blueprint callable"),
        IsValid(JoinFunction) &&
            JoinFunction->HasAnyFunctionFlags(FUNC_BlueprintCallable)
    );
    TestTrue(
        TEXT("Leave session is Blueprint callable"),
        IsValid(LeaveFunction) &&
            LeaveFunction->HasAnyFunctionFlags(FUNC_BlueprintCallable)
    );

    const FProperty* JoinButtonProperty =
        UBHMainMenuWidget::StaticClass()->FindPropertyByName(
            TEXT("JoinCampaignButton")
        );
    const FProperty* SessionStatusProperty =
        UBHMainMenuWidget::StaticClass()->FindPropertyByName(
            TEXT("SessionStatusText")
        );

    TestNotNull(
        TEXT("Main menu exposes an optional join campaign button"),
        JoinButtonProperty
    );
    TestNotNull(
        TEXT("Main menu exposes multiplayer session status"),
        SessionStatusProperty
    );

    struct FExpectedSessionHeading
    {
        EBHSessionState State;
        const TCHAR* Heading;
    };
    const FExpectedSessionHeading ExpectedHeadings[] =
    {
        {EBHSessionState::Idle, TEXT("MULTIPLAYER // READY")},
        {EBHSessionState::Hosting, TEXT("MULTIPLAYER // HOSTING")},
        {EBHSessionState::Searching, TEXT("MULTIPLAYER // SEARCHING")},
        {EBHSessionState::Joining, TEXT("MULTIPLAYER // JOINING")},
        {EBHSessionState::Traveling, TEXT("MULTIPLAYER // CONNECTING")},
        {EBHSessionState::InSession, TEXT("MULTIPLAYER // CONNECTED")},
        {EBHSessionState::Leaving, TEXT("MULTIPLAYER // LEAVING")},
        {EBHSessionState::Error, TEXT("MULTIPLAYER // ACTION FAILED")}
    };

    for (const FExpectedSessionHeading& Expected : ExpectedHeadings)
    {
        const FString Status =
            UBHMainMenuWidget::BuildSessionStatusText(
                Expected.State,
                FText::FromString(TEXT("Details"))
            ).ToString();
        TestTrue(
            FString::Printf(
                TEXT("Session state %d has a clear heading"),
                static_cast<int32>(Expected.State)
            ),
            Status.StartsWith(Expected.Heading) &&
                Status.EndsWith(TEXT("Details"))
        );
    }

    TestTrue(
        TEXT("Session errors use a distinct high-visibility color"),
        UBHMainMenuWidget::GetSessionStatusColor(
            EBHSessionState::Error
        ) != UBHMainMenuWidget::GetSessionStatusColor(
            EBHSessionState::Idle
        )
    );

    return true;
}


// Access only existing production transitions. No online interface is initialized,
// no map is loaded, and no global network failure delegate is broadcast by these tests.
struct FBHSessionSubsystemTestAccess
{
    static void Arm(UBHSessionSubsystem& Session, UWorld* Origin, const FString& Package = FString())
    {
        Session.PendingAction = UBHSessionSubsystem::EPendingAction::Host;
        Session.bPendingContinueCampaign = false;
        Session.bLoadContinueAfterListenTravel = false;
        Session.TrackPendingTravel(Origin, NM_ListenServer, Package);
        Session.SetSessionState(EBHSessionState::Traveling, FText::GetEmpty());
    }
    static void ArmContinue(UBHSessionSubsystem& Session, UWorld* Origin, FGuid ID)
    {
        Arm(Session, Origin);
        Session.PendingContinueRequestID = ID;
        Session.bPendingContinueCampaign = true;
        // The saved-map request has already started: map arrival must still wait for Applied.
        Session.bLoadContinueAfterListenTravel = false;
        // Keep consumer-only unit tests from requesting an actual menu map change.
        // Real menu dispatch and retained widget text are covered by the isolated runtime fixture.
        Session.bContinueFailureMenuTravelPending = true;
    }
    static void ContinueResult(UBHSessionSubsystem& Session, FGuid ID, EBHLoadProgressResult Result, UWorld* World)
    { Session.HandleContinueLoadComplete(ID, Result, TEXT("scoped_test_result"), World); }
    static bool HasContinueID(const UBHSessionSubsystem& Session, FGuid ID)
    { return Session.PendingContinueRequestID == ID; }
    static bool BeginContinue(UBHSessionSubsystem& Session, UWorld* World, const FString& Package)
    {
        Session.PendingAction = UBHSessionSubsystem::EPendingAction::Host;
        Session.bPendingContinueCampaign = true;
        return Session.BeginCampaignTravel(World, Package);
    }
    static void TravelFailure(UBHSessionSubsystem& Session, UWorld* World)
    {
        Session.HandleTravelFailure(World, ETravelFailure::InvalidURL, TEXT("Scoped session test failure"));
    }
    static void NetworkFailure(UBHSessionSubsystem& Session, UWorld* World, UNetDriver* Driver)
    {
        Session.HandleNetworkFailure(World, Driver, ENetworkFailure::ConnectionLost, TEXT("Scoped connection loss"));
    }
    static void Complete(UBHSessionSubsystem& Session, UWorld* World) { Session.HandlePostLoadMap(World); }
    static void Bind(UBHSessionSubsystem& Session) { Session.BindTravelDelegates(); }
    static bool IntentCleared(const UBHSessionSubsystem& Session)
    {
        return Session.PendingAction == UBHSessionSubsystem::EPendingAction::None &&
            !Session.bPendingContinueCampaign && !Session.bLoadContinueAfterListenTravel &&
            !Session.PendingTravelOrigin.IsValid() && Session.PendingTravelPackage.IsEmpty() &&
            Session.PendingTravelMode == NM_Standalone;
    }
    static void CheckRejectedOnlineCallbacks(UBHSessionSubsystem& Session, UWorld* World, FAutomationTestBase& Test)
    {
        enum class ECallback { Create, Find, Join, Destroy };
        struct FCase
        {
            const TCHAR* Label;
            ECallback Callback;
            EBHSessionState State;
            UBHSessionSubsystem::EPendingAction Action;
            bool bForeignName = false;
            bool bMissingHandle = false;
        };
        using EAction = UBHSessionSubsystem::EPendingAction;
        const FCase Cases[] = {
            {TEXT("Inactive create"), ECallback::Create, EBHSessionState::Error, EAction::None},
            {TEXT("Inactive find"), ECallback::Find, EBHSessionState::Error, EAction::None},
            {TEXT("Inactive join"), ECallback::Join, EBHSessionState::Error, EAction::None},
            {TEXT("Inactive destroy"), ECallback::Destroy, EBHSessionState::Error, EAction::None},
            {TEXT("Foreign create name"), ECallback::Create, EBHSessionState::Hosting, EAction::Host, true},
            {TEXT("Foreign join name"), ECallback::Join, EBHSessionState::Joining, EAction::None, true},
            {TEXT("Foreign destroy name"), ECallback::Destroy, EBHSessionState::Leaving, EAction::Leave, true},
            {TEXT("Create during another action"), ECallback::Create, EBHSessionState::Hosting, EAction::Leave},
            {TEXT("Find during hosting"), ECallback::Find, EBHSessionState::Hosting, EAction::Host},
            {TEXT("Join during hosting"), ECallback::Join, EBHSessionState::Hosting, EAction::Host},
            {TEXT("Destroy mismatched state and action"), ECallback::Destroy, EBHSessionState::Searching, EAction::Host},
            {TEXT("Create without current delegate"), ECallback::Create, EBHSessionState::Hosting, EAction::Host, false, true},
            {TEXT("Find without current delegate"), ECallback::Find, EBHSessionState::Searching, EAction::None, false, true},
            {TEXT("Join without current delegate"), ECallback::Join, EBHSessionState::Joining, EAction::None, false, true},
            {TEXT("Destroy without current delegate"), ECallback::Destroy, EBHSessionState::Leaving, EAction::Leave, false, true}
        };
        for (const FCase& Case : Cases)
        {
            // These handles are identities only; nothing is registered with an online service.
            Session.CreateSessionDelegateHandle = FDelegateHandle(FDelegateHandle::GenerateNewHandle);
            Session.FindSessionsDelegateHandle = FDelegateHandle(FDelegateHandle::GenerateNewHandle);
            Session.JoinSessionDelegateHandle = FDelegateHandle(FDelegateHandle::GenerateNewHandle);
            Session.DestroySessionDelegateHandle = FDelegateHandle(FDelegateHandle::GenerateNewHandle);
            if (Case.bMissingHandle)
            {
                switch (Case.Callback)
                {
                case ECallback::Create: Session.CreateSessionDelegateHandle.Reset(); break;
                case ECallback::Find: Session.FindSessionsDelegateHandle.Reset(); break;
                case ECallback::Join: Session.JoinSessionDelegateHandle.Reset(); break;
                case ECallback::Destroy: Session.DestroySessionDelegateHandle.Reset(); break;
                }
            }
            const FDelegateHandle CreateBefore = Session.CreateSessionDelegateHandle;
            const FDelegateHandle FindBefore = Session.FindSessionsDelegateHandle;
            const FDelegateHandle JoinBefore = Session.JoinSessionDelegateHandle;
            const FDelegateHandle DestroyBefore = Session.DestroySessionDelegateHandle;
            Session.SessionSearch = MakeShared<FOnlineSessionSearch>();
            const FOnlineSessionSearch* SearchBefore = Session.SessionSearch.Get();
            Session.PendingAction = Case.Action;
            Session.bPendingContinueCampaign = true;
            Session.bLoadContinueAfterListenTravel = true;
            Session.TrackPendingTravel(World, NM_ListenServer, TEXT("/Temp/RetainedCampaign"));
            Session.SetSessionState(Case.State, FText::GetEmpty());
            const FName SessionName = Case.bForeignName ? FName(TEXT("ForeignSession")) : NAME_GameSession;
            switch (Case.Callback)
            {
            case ECallback::Create: Session.HandleCreateSessionComplete(SessionName, false); break;
            case ECallback::Find: Session.HandleFindSessionsComplete(false); break;
            case ECallback::Join: Session.HandleJoinSessionComplete(SessionName, EOnJoinSessionCompleteResult::UnknownError); break;
            case ECallback::Destroy: Session.HandleDestroySessionComplete(SessionName, false); break;
            }
            Test.TestEqual(FString::Printf(TEXT("%s preserves state"), Case.Label), Session.SessionState, Case.State);
            Test.TestTrue(FString::Printf(TEXT("%s preserves pending intent"), Case.Label),
                Session.PendingAction == Case.Action && Session.bPendingContinueCampaign && Session.bLoadContinueAfterListenTravel &&
                Session.PendingTravelOrigin.Get() == World && Session.PendingTravelMode == NM_ListenServer &&
                Session.PendingTravelPackage == TEXT("/Temp/RetainedCampaign"));
            Test.TestTrue(FString::Printf(TEXT("%s preserves current delegate identities and search"), Case.Label),
                Session.CreateSessionDelegateHandle == CreateBefore && Session.FindSessionsDelegateHandle == FindBefore &&
                Session.JoinSessionDelegateHandle == JoinBefore && Session.DestroySessionDelegateHandle == DestroyBefore &&
                Session.SessionSearch.Get() == SearchBefore);
        }
        Session.CreateSessionDelegateHandle.Reset();
        Session.FindSessionsDelegateHandle.Reset();
        Session.JoinSessionDelegateHandle.Reset();
        Session.DestroySessionDelegateHandle.Reset();
        Session.SessionSearch.Reset();
    }
    static bool HandlesCleared(const UBHSessionSubsystem& Session)
    {
        return !Session.PostLoadMapDelegateHandle.IsValid() && !Session.TravelFailureDelegateHandle.IsValid() &&
            !Session.NetworkFailureDelegateHandle.IsValid() && !Session.BoundTravelEngine.IsValid();
    }
};

namespace
{
struct FBHScopedSessionWorld
{
    TStrongObjectPtr<UBHReplicationTestGameInstance> GameInstance;
    TStrongObjectPtr<UBHSessionSubsystem> Session;
    TStrongObjectPtr<UIpNetDriver> Driver;
    TArray<TStrongObjectPtr<UWorld>> Worlds;

    FBHScopedSessionWorld()
        : GameInstance(NewObject<UBHReplicationTestGameInstance>(GEngine))
        , Session(NewObject<UBHSessionSubsystem>(GameInstance.Get()))
        , Driver(NewObject<UIpNetDriver>(GameInstance.Get()))
    {
        GameInstance->InitializeForMinimalNetRPC(NewWorldName());
        Worlds.Emplace(GameInstance->GetWorld());
        World()->SetNetDriver(Driver.Get());
    }
    ~FBHScopedSessionWorld()
    {
        Session->Deinitialize();
        if (GameInstance->GetWorldContext()->PendingNetGame)
        {
            GameInstance->GetWorldContext()->PendingNetGame->NetDriver = nullptr;
            GameInstance->GetWorldContext()->PendingNetGame = nullptr;
        }
        Driver->ServerConnection = nullptr;
        GEngine->DestroyWorldContext(World());
        GameInstance->Shutdown();
        for (const TStrongObjectPtr<UWorld>& OwnedWorld : Worlds)
        {
            OwnedWorld->SetNetDriver(nullptr);
            OwnedWorld->SetGameInstance(nullptr);
            OwnedWorld->DestroyWorld(false);
        }
    }
    static FName NewWorldName()
    {
        return MakeUniqueObjectName(nullptr, UPackage::StaticClass(), TEXT("/Temp/BHSessionRecoveryTest"));
    }
    UWorld* World() const { return GameInstance->GetWorld(); }
    UWorld* ReplaceWorld()
    {
        World()->SetNetDriver(nullptr);
        UPackage* Package = nullptr;
        UWorld* Replacement = nullptr;
        UGameInstance::CreateMinimalNetRPCWorld(NewWorldName(), Package, Replacement);
        Worlds.Emplace(Replacement);
        Replacement->SetGameInstance(GameInstance.Get());
        Replacement->SetNetDriver(Driver.Get());
        GameInstance->GetWorldContext()->SetCurrentWorld(Replacement);
        return Replacement;
    }
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBHSessionImmediateTravelRejectionTest,
    "BrokenHorizon.Multiplayer.Session.ImmediateTravelRejection",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)
bool FBHSessionImmediateTravelRejectionTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    FBHScopedSessionWorld Fixture;
    const FURL GameModeURL(nullptr, TEXT("/Temp/BHSessionRecovery?game=/Script/Engine.GameModeBase"), TRAVEL_Absolute);
    if (!TestTrue(TEXT("Scoped real world creates the engine game mode"), Fixture.World()->SetGameMode(GameModeURL))) { return false; }
    AddExpectedError(TEXT("Contains illegal character"), EAutomationExpectedErrorFlags::Contains, 1);
    AddExpectedError(TEXT("BH_SESSION_ERROR"), EAutomationExpectedErrorFlags::Contains, 1);
    TestFalse(TEXT("Actual ServerTravel rejection is propagated"),
        FBHSessionSubsystemTestAccess::BeginContinue(*Fixture.Session, Fixture.World(), TEXT("/Game/Invalid%Campaign")));
    TestEqual(TEXT("Rejected travel enters Error"), Fixture.Session->GetSessionState(), EBHSessionState::Error);
    TestFalse(TEXT("Retry and Leave are not blocked by a pending action"), Fixture.Session->IsSessionActionPending());
    TestTrue(TEXT("Failure clears pending host and Continue intent"), FBHSessionSubsystemTestAccess::IntentCleared(*Fixture.Session));
    TestTrue(TEXT("Rejected travel did not queue a world change"), Fixture.World()->NextURL.IsEmpty());
    FBHSessionSubsystemTestAccess::Complete(*Fixture.Session, Fixture.ReplaceWorld());
    TestEqual(TEXT("A stale completion cannot resurrect the failed request"), Fixture.Session->GetSessionState(), EBHSessionState::Error);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBHSessionFailureOwnershipTest,
    "BrokenHorizon.Multiplayer.Session.FailureOwnership",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)
bool FBHSessionFailureOwnershipTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    FBHScopedSessionWorld Owned;
    FBHScopedSessionWorld Foreign;
    FBHSessionSubsystemTestAccess::Arm(*Owned.Session, Owned.World());
    FBHSessionSubsystemTestAccess::TravelFailure(*Owned.Session, Foreign.World());
    FBHSessionSubsystemTestAccess::TravelFailure(*Owned.Session, nullptr);
    FBHSessionSubsystemTestAccess::NetworkFailure(*Owned.Session, Owned.World(), Foreign.Driver.Get());
    FBHSessionSubsystemTestAccess::NetworkFailure(*Owned.Session, Foreign.World(), Owned.Driver.Get());
    FBHSessionSubsystemTestAccess::NetworkFailure(*Owned.Session, nullptr, Foreign.Driver.Get());
    TestEqual(TEXT("Foreign game-instance/world/driver failures leave owned travel pending"), Owned.Session->GetSessionState(), EBHSessionState::Traveling);
    FBHSessionSubsystemTestAccess::NetworkFailure(*Owned.Session, Owned.World(), Owned.Driver.Get());
    TestEqual(TEXT("Listen host losing a remote is not its own session loss"), Owned.Session->GetSessionState(), EBHSessionState::Traveling);
    AddExpectedError(TEXT("BH_SESSION_ERROR"), EAutomationExpectedErrorFlags::Contains, 3);
    FBHSessionSubsystemTestAccess::TravelFailure(*Owned.Session, Owned.World());
    TestEqual(TEXT("Owned asynchronous travel failure enters Error"), Owned.Session->GetSessionState(), EBHSessionState::Error);
    TestTrue(TEXT("Owned failure clears all pending intent"), FBHSessionSubsystemTestAccess::IntentCleared(*Owned.Session));

    FBHSessionSubsystemTestAccess::Arm(*Owned.Session, Owned.World());
    Owned.Driver->ServerConnection = NewObject<USimulatedClientNetConnection>(Owned.Driver.Get());
    FBHSessionSubsystemTestAccess::NetworkFailure(*Owned.Session, Owned.World(), Owned.Driver.Get());
    TestEqual(TEXT("Owned client driver loss enters Error"), Owned.Session->GetSessionState(), EBHSessionState::Error);
    TestFalse(TEXT("Client failure releases the pending action"), Owned.Session->IsSessionActionPending());
    Owned.Driver->ServerConnection = nullptr;

    FBHSessionSubsystemTestAccess::Arm(*Owned.Session, Owned.World());
    UPendingNetGame* Pending = NewObject<UPendingNetGame>(Owned.GameInstance.Get());
    Pending->NetDriver = Owned.Driver.Get();
    Owned.GameInstance->GetWorldContext()->PendingNetGame = Pending;
    FBHSessionSubsystemTestAccess::NetworkFailure(*Owned.Session, nullptr, Foreign.Driver.Get());
    TestEqual(TEXT("Null-world callback cannot use another pending driver"), Owned.Session->GetSessionState(), EBHSessionState::Traveling);
    FBHSessionSubsystemTestAccess::NetworkFailure(*Owned.Session, nullptr, Owned.Driver.Get());
    TestEqual(TEXT("Exact pending driver proves ownership without a world"), Owned.Session->GetSessionState(), EBHSessionState::Error);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBHSessionTravelCompletionGuardTest,
    "BrokenHorizon.Multiplayer.Session.TravelCompletionGuard",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)
bool FBHSessionTravelCompletionGuardTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    FBHScopedSessionWorld Owned;
    FBHScopedSessionWorld Foreign;
    if (!TestEqual(TEXT("Scoped server driver has listen mode"), Owned.World()->GetNetMode(), NM_ListenServer)) { return false; }
    UWorld* Origin = Owned.World();
    FBHSessionSubsystemTestAccess::Arm(*Owned.Session, Origin);
    FBHSessionSubsystemTestAccess::Complete(*Owned.Session, Origin);
    FBHSessionSubsystemTestAccess::Complete(*Owned.Session, Foreign.World());
    TestEqual(TEXT("Origin and foreign-world callbacks cannot complete travel"), Owned.Session->GetSessionState(), EBHSessionState::Traveling);
    AddExpectedError(TEXT("BH_SESSION_ERROR"), EAutomationExpectedErrorFlags::Contains, 1);
    FBHSessionSubsystemTestAccess::TravelFailure(*Owned.Session, Origin);
    UWorld* Destination = Owned.ReplaceWorld();
    FBHSessionSubsystemTestAccess::Complete(*Owned.Session, Destination);
    TestEqual(TEXT("New owned world cannot complete an already failed action"), Owned.Session->GetSessionState(), EBHSessionState::Error);
    FBHSessionSubsystemTestAccess::Arm(*Owned.Session, Destination);
    FBHSessionSubsystemTestAccess::Complete(*Owned.Session, Origin);
    FBHSessionSubsystemTestAccess::Complete(*Owned.Session, Destination);
    TestEqual(TEXT("Retry ignores both previous and current origin callbacks"), Owned.Session->GetSessionState(), EBHSessionState::Traveling);
    UWorld* RetryDestination = Owned.ReplaceWorld();
    RetryDestination->SetNetDriver(nullptr);
    FBHSessionSubsystemTestAccess::Complete(*Owned.Session, RetryDestination);
    TestEqual(TEXT("Wrong net mode cannot complete a listen request"), Owned.Session->GetSessionState(), EBHSessionState::Traveling);
    RetryDestination->SetNetDriver(Owned.Driver.Get());
    FBHSessionSubsystemTestAccess::Arm(*Owned.Session, Destination, TEXT("/Temp/OtherCampaign"));
    FBHSessionSubsystemTestAccess::Complete(*Owned.Session, RetryDestination);
    TestEqual(TEXT("Wrong campaign package cannot complete host travel"), Owned.Session->GetSessionState(), EBHSessionState::Traveling);
    FBHSessionSubsystemTestAccess::Arm(*Owned.Session, Destination, RetryDestination->GetOutermost()->GetName());
    FBHSessionSubsystemTestAccess::Complete(*Owned.Session, RetryDestination);
    TestEqual(TEXT("Correct current game-instance destination completes retry"), Owned.Session->GetSessionState(), EBHSessionState::InSession);
    TestFalse(TEXT("Successful transition is no longer pending"), Owned.Session->IsSessionActionPending());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBHSessionTravelDelegateLifecycleTest,
    "BrokenHorizon.Multiplayer.Session.TravelDelegateLifecycle",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)
bool FBHSessionTravelDelegateLifecycleTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    FBHScopedSessionWorld Fixture;
    FBHSessionSubsystemTestAccess::Bind(*Fixture.Session);
    FBHSessionSubsystemTestAccess::Bind(*Fixture.Session);
    TestTrue(TEXT("Travel failure delegate binds the actual subsystem"), GEngine->OnTravelFailure().IsBoundToObject(Fixture.Session.Get()));
    TestTrue(TEXT("Network failure delegate binds the actual subsystem"), GEngine->OnNetworkFailure().IsBoundToObject(Fixture.Session.Get()));
    TestTrue(TEXT("Map completion delegate binds the actual subsystem"), FCoreUObjectDelegates::PostLoadMapWithWorld.IsBoundToObject(Fixture.Session.Get()));
    Fixture.Session->Deinitialize();
    TestFalse(TEXT("Travel failure delegate releases the subsystem"), GEngine->OnTravelFailure().IsBoundToObject(Fixture.Session.Get()));
    TestFalse(TEXT("Network failure delegate releases the subsystem"), GEngine->OnNetworkFailure().IsBoundToObject(Fixture.Session.Get()));
    TestFalse(TEXT("Map completion delegate releases the subsystem"), FCoreUObjectDelegates::PostLoadMapWithWorld.IsBoundToObject(Fixture.Session.Get()));
    TestTrue(TEXT("Deinitialize clears handles and engine reference"), FBHSessionSubsystemTestAccess::HandlesCleared(*Fixture.Session));
    Fixture.Session->Deinitialize();
    TestTrue(TEXT("Repeated cleanup is harmless"), FBHSessionSubsystemTestAccess::HandlesCleared(*Fixture.Session));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBHSessionStaleOnlineCallbacksTest,
    "BrokenHorizon.Multiplayer.Session.StaleOnlineCallbacks",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)
bool FBHSessionStaleOnlineCallbacksTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    FBHScopedSessionWorld Fixture;
    FBHSessionSubsystemTestAccess::CheckRejectedOnlineCallbacks(*Fixture.Session, Fixture.World(), *this);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBHSessionContinueCompletionTest,
    "BrokenHorizon.Multiplayer.Session.ContinueCompletion",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)
bool FBHSessionContinueCompletionTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    FBHScopedSessionWorld Owned;
    FBHScopedSessionWorld Foreign;
    const FGuid FailedID = FGuid::NewGuid();
    FBHSessionSubsystemTestAccess::ArmContinue(*Owned.Session, Owned.World(), FailedID);
    UWorld* Destination = Owned.ReplaceWorld();
    FBHSessionSubsystemTestAccess::Complete(*Owned.Session, Destination);
    TestEqual(TEXT("Continue map arrival alone remains Traveling"), Owned.Session->GetSessionState(), EBHSessionState::Traveling);
    FBHSessionSubsystemTestAccess::ContinueResult(*Owned.Session, FGuid::NewGuid(), EBHLoadProgressResult::Applied, Destination);
    TestTrue(TEXT("Foreign completion ID cannot consume active Continue"), FBHSessionSubsystemTestAccess::HasContinueID(*Owned.Session, FailedID));
    TestEqual(TEXT("Foreign result cannot report connected"), Owned.Session->GetSessionState(), EBHSessionState::Traveling);
    AddExpectedError(TEXT("BH_SESSION_ERROR"), EAutomationExpectedErrorFlags::Contains, 1);
    FBHSessionSubsystemTestAccess::ContinueResult(*Owned.Session, FailedID, EBHLoadProgressResult::Applied, Foreign.World());
    TestEqual(TEXT("Applied from a foreign world fails closed"), Owned.Session->GetSessionState(), EBHSessionState::Error);
    TestFalse(TEXT("Failed Continue is no longer pending"), Owned.Session->IsSessionActionPending());
    const FText FailureMessage = Owned.Session->GetSessionStatusMessage();
    TestFalse(TEXT("Failure retains a message for the newly constructed menu"), FailureMessage.IsEmpty());
    FBHSessionSubsystemTestAccess::Complete(*Owned.Session, Destination);
    TestEqual(TEXT("Stale map arrival cannot erase retained failure text"), Owned.Session->GetSessionStatusMessage().ToString(), FailureMessage.ToString());
    const FGuid RetryID = FGuid::NewGuid();
    FBHSessionSubsystemTestAccess::ArmContinue(*Owned.Session, Destination, RetryID);
    UWorld* RetryDestination = Owned.ReplaceWorld();
    FBHSessionSubsystemTestAccess::Complete(*Owned.Session, RetryDestination);
    TestEqual(TEXT("Retry still waits for application after travel"), Owned.Session->GetSessionState(), EBHSessionState::Traveling);
    // Exercise the actual Session consumer contract. Real Save application/result
    // generation is independently covered by Persistence.Load.ApplyFailureAndRecovery.
    FBHSessionSubsystemTestAccess::ContinueResult(*Owned.Session, RetryID, EBHLoadProgressResult::Applied, RetryDestination);
    TestEqual(TEXT("Matching Applied in the current listen world connects"), Owned.Session->GetSessionState(), EBHSessionState::InSession);
    TestFalse(TEXT("Applied Continue retires its request identity"), FBHSessionSubsystemTestAccess::HasContinueID(*Owned.Session, RetryID));
    FBHSessionSubsystemTestAccess::ContinueResult(*Owned.Session, FailedID, EBHLoadProgressResult::Failed, RetryDestination);
    TestEqual(TEXT("Stale failure cannot overwrite successful retry"), Owned.Session->GetSessionState(), EBHSessionState::InSession);
    return true;
}
#endif
