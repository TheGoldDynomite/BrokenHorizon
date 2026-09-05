#include "BHWarSubsystem.h"
#include "BHWarGameState.h"
#include "BHReplicationTestGameInstance.h"
#include "Engine/Engine.h"
#include "Engine/NetConnection.h"
#include "Engine/World.h"
#include "IpNetDriver.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"
#include "Engine/GameInstance.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBHReplicatedSnapshotRevisionAllocationTest,
    "BrokenHorizon.Multiplayer.PersistentWar.SnapshotRevisionAllocation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FBHReplicatedSnapshotRevisionAllocationTest::RunTest(
    const FString& Parameters
)
{
    (void)Parameters;

    UGameInstance* GameInstance = NewObject<UGameInstance>();
    UBHWarSubsystem* WarSubsystem =
        NewObject<UBHWarSubsystem>(GameInstance);

    const int32 FirstRevision =
        WarSubsystem->AllocateReplicatedSnapshotRevision();
    const int32 SecondRevision =
        WarSubsystem->AllocateReplicatedSnapshotRevision();

    TestEqual(
        TEXT("The first authoritative snapshot revision starts at one"),
        FirstRevision,
        1
    );
    TestEqual(
        TEXT("Persistent snapshot revisions increase across GameState lifetimes"),
        SecondRevision,
        2
    );

    return true;
}

namespace
{
struct FBHScopedSnapshotClient
{
    TStrongObjectPtr<UBHReplicationTestGameInstance> GameInstance;
    TStrongObjectPtr<UBHWarSubsystem> War;
    TStrongObjectPtr<UIpNetDriver> Driver;
    TArray<TStrongObjectPtr<UWorld>> Worlds;

    FBHScopedSnapshotClient()
        : GameInstance(NewObject<UBHReplicationTestGameInstance>(GEngine))
        , War(NewObject<UBHWarSubsystem>(GameInstance.Get()))
        , Driver(NewObject<UIpNetDriver>(GameInstance.Get()))
    {
        GameInstance->InitializeForMinimalNetRPC(NewWorldName());
        Worlds.Emplace(GameInstance->GetWorld());
        GameInstance->GetWorld()->SetNetDriver(Driver.Get());
    }

    ~FBHScopedSnapshotClient()
    {
        Driver->ServerConnection = nullptr;
        GEngine->DestroyWorldContext(GameInstance->GetWorld());
        GameInstance->Shutdown();
        for (const TStrongObjectPtr<UWorld>& World : Worlds)
        {
            World->SetNetDriver(nullptr);
            World->SetGameInstance(nullptr);
            World->DestroyWorld(false);
        }
    }

    static FName NewWorldName()
    {
        return MakeUniqueObjectName(
            nullptr, UPackage::StaticClass(),
            TEXT("/Temp/BHReplicationSnapshotTest")
        );
    }

    void ReplaceWorld()
    {
        GameInstance->GetWorld()->SetNetDriver(nullptr);
        UPackage* Package = nullptr;
        UWorld* World = nullptr;
        UGameInstance::CreateMinimalNetRPCWorld(NewWorldName(), Package, World);
        Worlds.Emplace(World);
        World->SetGameInstance(GameInstance.Get());
        World->SetNetDriver(Driver.Get());
        GameInstance->GetWorldContext()->SetCurrentWorld(World);
    }

    UNetConnection* NewConnection()
    {
        Driver->ServerConnection =
            NewObject<USimulatedClientNetConnection>(Driver.Get());
        return Driver->ServerConnection.Get();
    }
};

FBHWarStateSnapshot MakeRevisionSnapshot(int32 Revision, int32 Turn)
{
    FBHWarStateSnapshot Snapshot;
    Snapshot.bInitialized = true;
    Snapshot.Revision = Revision;
    Snapshot.TurnNumber = Turn;
    Snapshot.SectorStates.AddDefaulted_GetRef().SectorID =
        TEXT("RevisionTestSector");
    return Snapshot;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBHReplicatedSnapshotConnectionRevisionTest,
    "BrokenHorizon.Multiplayer.PersistentWar.SnapshotConnectionRevision",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FBHReplicatedSnapshotConnectionRevisionTest::RunTest(
    const FString& Parameters
)
{
    (void)Parameters;
    if (!TestNotNull(TEXT("The engine is available for a scoped world"), GEngine))
    {
        return false;
    }

    const int32 InitialWorldContextCount = GEngine->GetWorldContexts().Num();
    {
        FBHScopedSnapshotClient Client;
        UNetConnection* FirstConnection = Client.NewConnection();
        TestEqual(TEXT("Fixture uses the real client net mode"),
            Client.War->GetWorld()->GetNetMode(), NM_Client);
        TestTrue(TEXT("First connection accepts revision 100"),
            Client.War->ApplyReplicatedSnapshot(MakeRevisionSnapshot(100, 10)));
        TestFalse(TEXT("Same connection rejects revision 99"),
            Client.War->ApplyReplicatedSnapshot(MakeRevisionSnapshot(99, 999)));
        TestEqual(TEXT("Rejected revision cannot mutate campaign data"),
            Client.War->GetTurnNumber(), 10);

        Client.ReplaceWorld();
        TestTrue(TEXT("Subsystem resolves the replacement world"),
            Client.War->GetWorld() == Client.Worlds.Last().Get());
        TestFalse(TEXT("World replacement retains the same-connection watermark"),
            Client.War->ApplyReplicatedSnapshot(MakeRevisionSnapshot(99, 999)));
        TestEqual(TEXT("World replacement cannot admit stale campaign data"),
            Client.War->GetTurnNumber(), 10);

        UNetConnection* SecondConnection = Client.NewConnection();
        FBHWarStateSnapshot Malformed = MakeRevisionSnapshot(1000, 999);
        Malformed.SectorStates.Reset();
        TestFalse(TEXT("New connection rejects a snapshot without sectors"),
            Client.War->ApplyReplicatedSnapshot(Malformed));
        Malformed = MakeRevisionSnapshot(-1, 999);
        TestFalse(TEXT("New connection rejects a negative revision"),
            Client.War->ApplyReplicatedSnapshot(Malformed));
        Malformed = MakeRevisionSnapshot(1000, 999);
        Malformed.bInitialized = false;
        TestFalse(TEXT("New connection rejects an uninitialized snapshot"),
            Client.War->ApplyReplicatedSnapshot(Malformed));
        TestEqual(TEXT("Malformed snapshots preserve accepted campaign data"),
            Client.War->GetTurnNumber(), 10);

        Client.Driver->ServerConnection = FirstConnection;
        TestFalse(TEXT("Malformed new-source data cannot reset the old watermark"),
            Client.War->ApplyReplicatedSnapshot(MakeRevisionSnapshot(99, 999)));
        Client.Driver->ServerConnection = SecondConnection;
        TestTrue(TEXT("A new connection accepts revision 1 after revision 100"),
            Client.War->ApplyReplicatedSnapshot(MakeRevisionSnapshot(1, 20)));
        TestEqual(TEXT("Reconnect installs the new campaign data"),
            Client.War->GetTurnNumber(), 20);
        TestFalse(TEXT("The new connection rejects its own stale revision 0"),
            Client.War->ApplyReplicatedSnapshot(MakeRevisionSnapshot(0, 999)));

        Client.Driver->ServerConnection = nullptr;
        TestFalse(TEXT("Missing connection cannot admit an older snapshot"),
            Client.War->ApplyReplicatedSnapshot(MakeRevisionSnapshot(0, 999)));
        Client.Driver->ServerConnection = SecondConnection;
        TestFalse(TEXT("Restoring the same connection retains its watermark"),
            Client.War->ApplyReplicatedSnapshot(MakeRevisionSnapshot(0, 999)));
        TestTrue(TEXT("Equal revisions preserve the existing acceptance contract"),
            Client.War->ApplyReplicatedSnapshot(MakeRevisionSnapshot(1, 21)));
        TestTrue(TEXT("The connection advances its watermark again"),
            Client.War->ApplyReplicatedSnapshot(MakeRevisionSnapshot(100, 22)));

        TWeakObjectPtr<UNetConnection> ExpiredConnection = SecondConnection;
        SecondConnection->MarkAsGarbage();
        TestFalse(TEXT("Prior connection is now invalid to weak references"),
            ExpiredConnection.IsValid());
        TestFalse(TEXT("An invalid connection cannot reset the watermark"),
            Client.War->ApplyReplicatedSnapshot(MakeRevisionSnapshot(1, 999)));
        Client.NewConnection();
        TestTrue(TEXT("Replacement of an expired connection accepts revision 1"),
            Client.War->ApplyReplicatedSnapshot(MakeRevisionSnapshot(1, 30)));
        TestEqual(TEXT("Expired connection replacement installs campaign data"),
            Client.War->GetTurnNumber(), 30);
        TestFalse(TEXT("Replacement also establishes a new stale-revision gate"),
            Client.War->ApplyReplicatedSnapshot(MakeRevisionSnapshot(0, 999)));
        TestEqual(TEXT("Rejected data never changes the final campaign state"),
            Client.War->GetTurnNumber(), 30);
    }
    TestEqual(TEXT("The fixture removes its world context"),
        GEngine->GetWorldContexts().Num(), InitialWorldContextCount);
    return true;
}
