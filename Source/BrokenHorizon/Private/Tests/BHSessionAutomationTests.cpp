#if WITH_DEV_AUTOMATION_TESTS

#include "BHMainMenuWidget.h"
#include "BHSessionSubsystem.h"
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

#endif
