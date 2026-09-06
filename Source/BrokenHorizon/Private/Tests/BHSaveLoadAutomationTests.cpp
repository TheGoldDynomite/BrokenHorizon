#if WITH_DEV_AUTOMATION_TESTS

#include "BHSaveSubsystem.h"
#include "BHSaveGame.h"
#include "BHSaveLoadTestObserver.h"
#include "BHDoor.h"
#include "UObject/UnrealType.h"
#include "BHCharacter.h"
#include "BHHealthComponent.h"
#include "BHWeaponComponent.h"
#include "BHMissionData.h"
#include "BHReplicationTestGameInstance.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "IpNetDriver.h"
#include "Misc/AutomationTest.h"
#include "Misc/PackageName.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"

struct FBHSaveSubsystemTestAccess
{
    static bool Prepare(UBHSaveSubsystem& Save, FGuid ID, UBHSaveGame* Data, FBHLoadProgressCompletion Completion)
    { return Save.BeginPreparedLoad(ID, Data, false, MoveTemp(Completion)); }
    static void Arrived(UBHSaveSubsystem& Save, UWorld* World) { Save.HandlePostLoadMap(World); }
    static void Apply(UBHSaveSubsystem& Save, FGuid ID, UWorld* World) { Save.ApplyPendingSave(ID, TWeakObjectPtr<UWorld>(World)); }
    static ABHCharacter* FindPlayer(const UBHSaveSubsystem& Save, UWorld* World) { return Save.FindPlayerCharacter(World); }
    static bool Protected(const UBHSaveSubsystem& Save) { return Save.bCheckpointWritesProtected; }
    static bool Active(const UBHSaveSubsystem& Save, FGuid ID) { return Save.ActiveLoadRequestID == ID; }
    static bool Applying(const UBHSaveSubsystem& Save) { return Save.LoadPhase == UBHSaveSubsystem::ELoadProgressPhase::Applying; }
    static int32 Attempts(const UBHSaveSubsystem& Save) { return Save.PendingSaveApplyAttempts; }
    static void SetCasualty(UBHSaveSubsystem& Save, FName Sector) { Save.PendingPlayerDeathAttritionSectorID = Sector; }
    static FName Casualty(const UBHSaveSubsystem& Save) { return Save.PendingPlayerDeathAttritionSectorID; }
    static void Expire(UBHSaveSubsystem& Save) { Save.LoadDeadlineSeconds = 0.0; Save.TickLoadDeadline(0.0f); }
    static bool Cleared(const UBHSaveSubsystem& Save)
    {
        return !Save.ActiveLoadRequestID.IsValid() && !IsValid(Save.PendingSaveData) &&
            Save.LoadPhase == UBHSaveSubsystem::ELoadProgressPhase::None && !Save.LoadApplyTimer.IsValid() &&
            !Save.LoadDeadlineTicker.IsValid() && Save.PendingPlayerDeathAttritionSectorID.IsNone();
    }
};

namespace
{
struct FBHLoadResultRecord
{
    FGuid ID;
    EBHLoadProgressResult Result;
    FName Reason;
    TWeakObjectPtr<UWorld> World;
};

// Initialized Game worlds support real character components and world subsystems.
// The minimal test GI still skips GI subsystem startup, disk slots and map loads.
struct FBHScopedSaveLoadWorld
{
    TStrongObjectPtr<UBHReplicationTestGameInstance> GameInstance;
    TStrongObjectPtr<UBHSaveSubsystem> Save;
    TStrongObjectPtr<UIpNetDriver> Driver;
    TStrongObjectPtr<UBHMissionData> Mission;
    TStrongObjectPtr<UBHSaveGame> Data;
    TArray<TStrongObjectPtr<UWorld>> Worlds;
    TArray<TWeakObjectPtr<UBHWeaponComponent>> StartedWeapons;
    TArray<FBHLoadResultRecord> Results;
    FName DestinationPackage;
    TFunction<void()> OnCompletion;

    FBHScopedSaveLoadWorld()
        : GameInstance(NewObject<UBHReplicationTestGameInstance>(GEngine))
        , Save(NewObject<UBHSaveSubsystem>(GameInstance.Get()))
        , Driver(NewObject<UIpNetDriver>(GameInstance.Get()))
        , Mission(NewObject<UBHMissionData>())
        , Data(NewObject<UBHSaveGame>())
    {
        const FName InitialPackageName = NewPackageName();
        UPackage* InitialPackage = NewObject<UPackage>(nullptr, InitialPackageName, RF_Transient);
        GameInstance->InitializeStandalone(FPackageName::GetShortFName(InitialPackageName), InitialPackage);
        Worlds.Emplace(World());
        World()->SetNetDriver(Driver.Get());
        FBHObjectiveDefinition& First = Mission->Objectives.AddDefaulted_GetRef();
        First.ObjectiveID = BHObjectiveIds::FindRedKeycard;
        First.DisplayText = FText::FromString(TEXT("Find the keycard"));
        FBHObjectiveDefinition& Second = Mission->Objectives.AddDefaulted_GetRef();
        Second.ObjectiveID = BHObjectiveIds::UnlockSecurityDoor;
        Second.DisplayText = FText::FromString(TEXT("Unlock the door"));
        Data->MissionData = Mission.Get();
        Data->CurrentObjectiveID = BHObjectiveIds::UnlockSecurityDoor;
        Data->CompletedObjectiveIDs = {BHObjectiveIds::FindRedKeycard};
        Data->ConsumedWorldItemIDs = {FName(TEXT("ScopedConsumedSupply"))};
        Data->SavedHealth = 72.0f;
        Data->SavedMagazineAmmo = 17;
        Data->SavedReserveAmmo = 61;
        SetNextDestination();
    }
    ~FBHScopedSaveLoadWorld()
    {
        Save->Deinitialize();
        for (const TWeakObjectPtr<UBHWeaponComponent>& StartedWeapon : StartedWeapons)
        {
            if (UBHWeaponComponent* Weapon = StartedWeapon.Get())
            {
                UActorComponent* LifecycleComponent = Weapon;
                if (LifecycleComponent->HasBegunPlay()) { LifecycleComponent->EndPlay(EEndPlayReason::Quit); }
                LifecycleComponent->RegisterAllComponentTickFunctions(false);
            }
        }
        UWorld* FinalWorld = World();
        for (const TStrongObjectPtr<UWorld>& OwnedWorld : Worlds)
        {
            OwnedWorld->SetNetDriver(nullptr);
            // DestroyWorld unregisters components before deinitializing their
            // world subsystems and removes the root added by CreateWorld.
            OwnedWorld->DestroyWorld(false);
            OwnedWorld->SetGameInstance(nullptr);
        }
        GEngine->DestroyWorldContext(FinalWorld);
        GameInstance->Shutdown();
    }
    static FName NewPackageName()
    { return MakeUniqueObjectName(nullptr, UPackage::StaticClass(), TEXT("/Temp/BHSaveLoadTest")); }
    UWorld* World() const { return GameInstance->GetWorld(); }
    void SetNextDestination()
    {
        DestinationPackage = NewPackageName();
        Data->SavedLevelName = FPackageName::GetShortFName(DestinationPackage);
    }
    UWorld* ReplaceWorld(FName PackageName)
    {
        World()->SetNetDriver(nullptr);
        UPackage* Package = NewObject<UPackage>(nullptr, PackageName, RF_Transient);
        UWorld* Replacement = UWorld::CreateWorld(EWorldType::Game, false,
            FPackageName::GetShortFName(PackageName), Package);
        Worlds.Emplace(Replacement);
        Replacement->SetGameInstance(GameInstance.Get());
        Replacement->SetNetDriver(Driver.Get());
        GameInstance->GetWorldContext()->SetCurrentWorld(Replacement);
        return Replacement;
    }
    bool Prepare(FGuid ID)
    {
        return FBHSaveSubsystemTestAccess::Prepare(*Save, ID, Data.Get(),
            FBHLoadProgressCompletion::CreateWeakLambda(GameInstance.Get(),
                [this](FGuid CompletedID, EBHLoadProgressResult Result, FName Reason, UWorld* AppliedWorld)
                { Results.Add({CompletedID, Result, Reason, AppliedWorld}); if (OnCompletion) { OnCompletion(); } }));
    }
    ABHCharacter* SpawnPlayer(FAutomationTestBase& Test)
    {
        // CreateWorld initializes world subsystems; actor initialization is a
        // separate engine step required for PostInitializeComponents to register
        // controllers. This does not dispatch BeginPlay or tick the world.
        if (!World()->AreActorsInitialized())
        {
            const FURL WorldURL(nullptr, *World()->GetOutermost()->GetName(), TRAVEL_Absolute);
            World()->InitializeActorsForPlay(WorldURL);
        }
        if (!Test.TestTrue(TEXT("Fixture world initialized actors before player spawn"), World()->AreActorsInitialized())) { return nullptr; }
        FActorSpawnParameters Parameters;
        Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        APlayerController* Controller = World()->SpawnActor<APlayerController>(Parameters);
        ABHCharacter* Character = World()->SpawnActor<ABHCharacter>(Parameters);
        if (!Test.TestNotNull(TEXT("Fixture spawned controller"), Controller) ||
            !Test.TestNotNull(TEXT("Fixture spawned character"), Character)) { return nullptr; }
        Controller->Possess(Character);
        bool bRegisteredController = false;
        for (FConstPlayerControllerIterator It = World()->GetPlayerControllerIterator(); It; ++It)
        {
            bRegisteredController |= It->Get() == Controller;
        }
        if (!Test.TestTrue(TEXT("Fixture controller is registered in the actual world iterator"), bRegisteredController) ||
            !Test.TestTrue(TEXT("Fixture controller possesses the actual character"), Controller->GetPawn() == Character) ||
            !Test.TestTrue(TEXT("Actual Save player resolver finds the fixture character"),
                FBHSaveSubsystemTestAccess::FindPlayer(*Save, World()) == Character)) { return nullptr; }
        UBHWeaponComponent* Weapon = Character->GetWeaponComponent();
        FClassProperty* RifleClass = FindFProperty<FClassProperty>(UBHWeaponComponent::StaticClass(), TEXT("DefaultRifleClass"));
        if (!Test.TestNotNull(TEXT("Fixture has its real weapon component"), Weapon) ||
            !Test.TestNotNull(TEXT("Authored default rifle class property exists"), RifleClass)) { return nullptr; }
        // Supply the native rifle class normally configured by the character BP,
        // then run only the component's normal startup, not Character BeginPlay.
        RifleClass->SetObjectPropertyValue_InContainer(Weapon, ABHRifle::StaticClass());
        UActorComponent* LifecycleComponent = Weapon;
        LifecycleComponent->RegisterAllComponentTickFunctions(true);
        LifecycleComponent->BeginPlay();
        StartedWeapons.Add(Weapon);
        ABHRifle* Rifle = Weapon->GetEquippedRifle();
        if (!Test.TestTrue(TEXT("Weapon component completed normal startup"), LifecycleComponent->HasBegunPlay()) ||
            !Test.TestNotNull(TEXT("Production startup spawned a real rifle"), Rifle) ||
            !Test.TestTrue(TEXT("Spawned rifle belongs to the scoped player and world"),
                Rifle->GetOwner() == Character && Rifle->GetWorld() == World()) ||
            !Test.TestTrue(TEXT("Rifle has nonzero ammunition distinct from the checkpoint before restoration"),
                Weapon->GetMagazineAmmo() > 0 && Weapon->GetReserveAmmo() > 0 &&
                Weapon->GetMagazineAmmo() != Data->SavedMagazineAmmo && Weapon->GetReserveAmmo() != Data->SavedReserveAmmo)) { return nullptr; }
        return Character;
    }
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBHSaveLoadRequestLifecycleTest,
    "BrokenHorizon.Persistence.Load.RequestLifecycle",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)
bool FBHSaveLoadRequestLifecycleTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    FBHScopedSaveLoadWorld Fixture;
    const FGuid First = FGuid::NewGuid();
    const FGuid Second = FGuid::NewGuid();
    TestFalse(TEXT("Invalid request cannot be accepted"), Fixture.Prepare(FGuid()));
    if (!TestTrue(TEXT("An in-memory payload is accepted before travel"), Fixture.Prepare(First))) { return false; }
    TestTrue(TEXT("Accepted preparation immediately protects checkpoint writes"), FBHSaveSubsystemTestAccess::Protected(*Fixture.Save));
    FBHSaveSubsystemTestAccess::SetCasualty(*Fixture.Save, TEXT("RetainedCasualtySector"));
    TestFalse(TEXT("Public prepare rejects a busy request before reading slots"), Fixture.Save->PrepareLoadProgress(Second, FBHLoadProgressCompletion()));
    TestFalse(TEXT("Busy casualty reload is rejected before reading slots"), Fixture.Save->ReloadCheckpointAfterPlayerDeath(TEXT("OtherSector")));
    TestEqual(TEXT("Rejected casualty request preserves active casualty intent"), FBHSaveSubsystemTestAccess::Casualty(*Fixture.Save), FName(TEXT("RetainedCasualtySector")));
    TestFalse(TEXT("Wrong ID cannot start active travel"), Fixture.Save->StartPreparedLoad(Second));
    TestFalse(TEXT("Wrong ID cannot cancel active work"), Fixture.Save->CancelLoadProgress(Second));
    TestTrue(TEXT("Original request remains active"), FBHSaveSubsystemTestAccess::Active(*Fixture.Save, First));
    TestTrue(TEXT("Matching cancellation succeeds"), Fixture.Save->CancelLoadProgress(First));
    TestEqual(TEXT("Cancellation completes exactly once"), Fixture.Results.Num(), 1);
    if (Fixture.Results.Num() == 1)
    {
        TestEqual(TEXT("Cancellation result"), Fixture.Results[0].Result, EBHLoadProgressResult::Cancelled);
        TestEqual(TEXT("Cancellation retains request identity"), Fixture.Results[0].ID, First);
    }
    TestTrue(TEXT("Cancellation retires data, handles and casualty intent"), FBHSaveSubsystemTestAccess::Cleared(*Fixture.Save));
    TestTrue(TEXT("Cancellation keeps the checkpoint protected"), FBHSaveSubsystemTestAccess::Protected(*Fixture.Save));
    TestFalse(TEXT("Repeated cancellation cannot complete twice"), Fixture.Save->CancelLoadProgress(First));
    if (!TestTrue(TEXT("A fresh request can retry after cancellation"), Fixture.Prepare(Second))) { return false; }
    FBHSaveSubsystemTestAccess::Apply(*Fixture.Save, First, Fixture.World());
    TestTrue(TEXT("Stale apply ID cannot steal the retry"), FBHSaveSubsystemTestAccess::Active(*Fixture.Save, Second));
    FBHSaveSubsystemTestAccess::Expire(*Fixture.Save);
    TestEqual(TEXT("Deadline produces one additional terminal callback"), Fixture.Results.Num(), 2);
    if (Fixture.Results.Num() == 2) { TestEqual(TEXT("Deadline result"), Fixture.Results[1].Result, EBHLoadProgressResult::TimedOut); }
    TestTrue(TEXT("Timeout retains checkpoint protection"), FBHSaveSubsystemTestAccess::Protected(*Fixture.Save));
    TestTrue(TEXT("Timeout clears scheduled work"), FBHSaveSubsystemTestAccess::Cleared(*Fixture.Save));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBHSaveLoadWorldOwnershipTest,
    "BrokenHorizon.Persistence.Load.WorldOwnershipAndPlayerTimeout",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)
bool FBHSaveLoadWorldOwnershipTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    FBHScopedSaveLoadWorld Fixture;
    FBHScopedSaveLoadWorld Foreign;
    const FGuid ID = FGuid::NewGuid();
    if (!TestTrue(TEXT("Owned request prepares"), Fixture.Prepare(ID)) ||
        !TestTrue(TEXT("Real server travel accepts the saved destination"), Fixture.Save->StartPreparedLoad(ID))) { return false; }
    UWorld* Origin = Fixture.World();
    FBHSaveSubsystemTestAccess::Arrived(*Fixture.Save, Origin);
    FBHSaveSubsystemTestAccess::Arrived(*Fixture.Save, Foreign.World());
    TestFalse(TEXT("Origin and foreign GI do not begin application"), FBHSaveSubsystemTestAccess::Applying(*Fixture.Save));
    UWorld* WrongDestination = Fixture.ReplaceWorld(FBHScopedSaveLoadWorld::NewPackageName());
    FBHSaveSubsystemTestAccess::Arrived(*Fixture.Save, WrongDestination);
    TestFalse(TEXT("Wrong owned destination does not apply another map's save"), FBHSaveSubsystemTestAccess::Applying(*Fixture.Save));
    UWorld* Destination = Fixture.ReplaceWorld(Fixture.DestinationPackage);
    FBHSaveSubsystemTestAccess::Arrived(*Fixture.Save, Destination);
    TestTrue(TEXT("Matching owned destination begins application"), FBHSaveSubsystemTestAccess::Applying(*Fixture.Save));
    FBHSaveSubsystemTestAccess::Apply(*Fixture.Save, FGuid::NewGuid(), Destination);
    FBHSaveSubsystemTestAccess::Apply(*Fixture.Save, ID, Foreign.World());
    FBHSaveSubsystemTestAccess::Apply(*Fixture.Save, ID, Origin);
    TestEqual(TEXT("Foreign/stale apply callbacks cannot advance retry attempts"), FBHSaveSubsystemTestAccess::Attempts(*Fixture.Save), 0);
    for (int32 Attempt = 0; Attempt < 40; ++Attempt) { FBHSaveSubsystemTestAccess::Apply(*Fixture.Save, ID, Destination); }
    TestEqual(TEXT("No possessed player remains pending through forty attempts"), Fixture.Results.Num(), 0);
    FBHSaveSubsystemTestAccess::Apply(*Fixture.Save, ID, Destination);
    TestEqual(TEXT("Bounded player timeout completes once"), Fixture.Results.Num(), 1);
    if (Fixture.Results.Num() == 1)
    {
        TestEqual(TEXT("Player timeout result"), Fixture.Results[0].Result, EBHLoadProgressResult::TimedOut);
        TestEqual(TEXT("Player timeout reason"), Fixture.Results[0].Reason, FName(TEXT("player_timeout")));
    }
    FBHSaveSubsystemTestAccess::Apply(*Fixture.Save, ID, Destination);
    TestEqual(TEXT("Late timer cannot repeat completion"), Fixture.Results.Num(), 1);
    TestTrue(TEXT("Timed-out application remains write protected"), FBHSaveSubsystemTestAccess::Protected(*Fixture.Save));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBHSaveLoadApplyRecoveryTest,
    "BrokenHorizon.Persistence.Load.ApplyFailureAndRecovery",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)
bool FBHSaveLoadApplyRecoveryTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    FBHScopedSaveLoadWorld Fixture;

    const FGuid FailedID = FGuid::NewGuid();
    if (!TestTrue(TEXT("Valid payload is accepted before destination validation"), Fixture.Prepare(FailedID)) ||
        !TestTrue(TEXT("Failure fixture travel accepted"), Fixture.Save->StartPreparedLoad(FailedID))) { return false; }
    UWorld* FailedWorld = Fixture.ReplaceWorld(Fixture.DestinationPackage);
    if (!TestNotNull(TEXT("Failure fixture has a real possessed character"), Fixture.SpawnPlayer(*this))) { return false; }
    FBHSaveSubsystemTestAccess::Arrived(*Fixture.Save, FailedWorld);
    // A valid checkpoint can fail against a destination containing duplicate IDs.
    // Change only scoped actor instances through their actual reflected property.
    FNameProperty* Identity = FindFProperty<FNameProperty>(ABHDoor::StaticClass(), TEXT("PersistenceID"));
    FActorSpawnParameters SpawnParameters;
    SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    ABHDoor* FirstDoor = FailedWorld->SpawnActor<ABHDoor>(SpawnParameters);
    ABHDoor* SecondDoor = FailedWorld->SpawnActor<ABHDoor>(SpawnParameters);
    if (!TestNotNull(TEXT("Door persistence property exists"), Identity) ||
        !TestNotNull(TEXT("First scoped door exists"), FirstDoor) ||
        !TestNotNull(TEXT("Second scoped door exists"), SecondDoor)) { return false; }
    Identity->SetPropertyValue_InContainer(FirstDoor, TEXT("DuplicateScopedDoor"));
    Identity->SetPropertyValue_InContainer(SecondDoor, TEXT("DuplicateScopedDoor"));
    AddExpectedError(TEXT("Duplicate door persistence ID"), EAutomationExpectedErrorFlags::Contains, 1);
    AddExpectedError(TEXT("Checkpoint state could not be applied"), EAutomationExpectedErrorFlags::Contains, 1);
    FBHSaveSubsystemTestAccess::Apply(*Fixture.Save, FailedID, FailedWorld);
    TestEqual(TEXT("Actual apply failure completes once"), Fixture.Results.Num(), 1);
    if (Fixture.Results.Num() == 1) { TestEqual(TEXT("Actual validation failure result"), Fixture.Results[0].Result, EBHLoadProgressResult::Failed); }
    TestTrue(TEXT("Failed restoration retains write protection"), FBHSaveSubsystemTestAccess::Protected(*Fixture.Save));

    Fixture.SetNextDestination();
    const FGuid RetryID = FGuid::NewGuid();
    if (!TestTrue(TEXT("Valid retry prepares"), Fixture.Prepare(RetryID)) ||
        !TestTrue(TEXT("Retry server travel accepted"), Fixture.Save->StartPreparedLoad(RetryID))) { return false; }
    UWorld* AppliedWorld = Fixture.ReplaceWorld(Fixture.DestinationPackage);
    ABHCharacter* Character = Fixture.SpawnPlayer(*this);
    if (!TestNotNull(TEXT("Retry owns a real possessed character"), Character)) { return false; }
    FBHSaveSubsystemTestAccess::Arrived(*Fixture.Save, AppliedWorld);
    TestEqual(TEXT("Arrival alone does not report Applied"), Fixture.Results.Num(), 1);
    FBHSaveSubsystemTestAccess::Apply(*Fixture.Save, RetryID, AppliedWorld);
    TestEqual(TEXT("Actual application produces one success completion"), Fixture.Results.Num(), 2);
    if (Fixture.Results.Num() == 2)
    {
        TestEqual(TEXT("Retry completion is Applied"), Fixture.Results[1].Result, EBHLoadProgressResult::Applied);
        TestEqual(TEXT("Retry completion identifies the owned applied world"), Fixture.Results[1].World.Get(), AppliedWorld);
    }
    TestEqual(TEXT("Objective restored by real character application"), Character->GetCurrentObjectiveID(), BHObjectiveIds::UnlockSecurityDoor);
    TestTrue(TEXT("Completed objective restored"), Character->GetCompletedObjectiveIDs().Contains(BHObjectiveIds::FindRedKeycard));
    TestEqual(TEXT("Health restored"), Character->GetHealthComponent()->GetCurrentHealth(), 72.0f);
    TestEqual(TEXT("Magazine restored"), Character->GetWeaponComponent()->GetMagazineAmmo(), 17);
    TestEqual(TEXT("Reserve restored"), Character->GetWeaponComponent()->GetReserveAmmo(), 61);
    TestTrue(TEXT("Consumed-item state restored"), Fixture.Save->IsWorldItemConsumed(TEXT("ScopedConsumedSupply")));
    TestFalse(TEXT("Only actual successful application releases write protection"), FBHSaveSubsystemTestAccess::Protected(*Fixture.Save));
    FBHSaveSubsystemTestAccess::Apply(*Fixture.Save, RetryID, AppliedWorld);
    TestEqual(TEXT("Duplicate completion callback is retired"), Fixture.Results.Num(), 2);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBHSaveLoadReentrantCancellationTest,
    "BrokenHorizon.Persistence.Load.ReentrantCancellation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)
bool FBHSaveLoadReentrantCancellationTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    FBHScopedSaveLoadWorld Fixture;
    const FGuid ID = FGuid::NewGuid();
    const FGuid ReentrantID = FGuid::NewGuid();
    if (!TestTrue(TEXT("Request prepares"), Fixture.Prepare(ID)) ||
        !TestTrue(TEXT("Real travel accepted"), Fixture.Save->StartPreparedLoad(ID))) { return false; }
    UWorld* Destination = Fixture.ReplaceWorld(Fixture.DestinationPackage);
    ABHCharacter* Character = Fixture.SpawnPlayer(*this);
    if (!TestNotNull(TEXT("Real player exists"), Character)) { return false; }
    TStrongObjectPtr<UBHSaveLoadTestObserver> Observer(NewObject<UBHSaveLoadTestObserver>());
    int32 Observations = 0;
    Observer->OnObservedHealth = [&](float CurrentHealth, float MaxHealth)
    {
        (void)MaxHealth;
        ++Observations;
        TestEqual(TEXT("Real restore broadcasts saved health"), CurrentHealth, 72.0f);
        TestTrue(TEXT("Restore delegate can request cancellation"), Fixture.Save->CancelLoadProgress(ID));
        TestEqual(TEXT("Cancellation completion waits for application to unwind"), Fixture.Results.Num(), 0);
        TestFalse(TEXT("Reentrant public preparation cannot read slots or steal mutation"),
            Fixture.Save->PrepareLoadProgress(ReentrantID, FBHLoadProgressCompletion()));
        TestFalse(TEXT("Reentrant in-memory acceptance cannot replace the request"), Fixture.Prepare(ReentrantID));
        TestTrue(TEXT("Cancellation during mutation keeps writes protected"), FBHSaveSubsystemTestAccess::Protected(*Fixture.Save));
    };
    Character->GetHealthComponent()->OnHealthChanged.AddDynamic(Observer.Get(), &UBHSaveLoadTestObserver::HandleHealthChanged);
    FBHSaveSubsystemTestAccess::Arrived(*Fixture.Save, Destination);
    FBHSaveSubsystemTestAccess::Apply(*Fixture.Save, ID, Destination);
    Character->GetHealthComponent()->OnHealthChanged.RemoveDynamic(Observer.Get(), &UBHSaveLoadTestObserver::HandleHealthChanged);
    Observer->OnObservedHealth = nullptr;
    TestEqual(TEXT("Application used the actual health event once"), Observations, 1);
    TestEqual(TEXT("Deferred cancellation completes once"), Fixture.Results.Num(), 1);
    if (Fixture.Results.Num() == 1)
    {
        TestEqual(TEXT("Cancelled restoration never reports Applied"), Fixture.Results[0].Result, EBHLoadProgressResult::Cancelled);
        TestEqual(TEXT("Cancelled restoration retains its identity"), Fixture.Results[0].ID, ID);
    }
    TestTrue(TEXT("Cancelled real application leaves checkpoint protected"), FBHSaveSubsystemTestAccess::Protected(*Fixture.Save));
    TestTrue(TEXT("Cancelled application clears its intent and timers"), FBHSaveSubsystemTestAccess::Cleared(*Fixture.Save));
    FBHSaveSubsystemTestAccess::Apply(*Fixture.Save, ID, Destination);
    TestEqual(TEXT("A late callback cannot produce Applied"), Fixture.Results.Num(), 1);
    TestTrue(TEXT("Preparation becomes available after the mutation unwinds"), Fixture.Prepare(ReentrantID));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBHSaveLoadTeardownReentrancyTest,
    "BrokenHorizon.Persistence.Load.TeardownReentrancy",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)
bool FBHSaveLoadTeardownReentrancyTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    FBHScopedSaveLoadWorld Fixture;
    const FGuid ID = FGuid::NewGuid();
    const FGuid ReplacementID = FGuid::NewGuid();
    if (!TestTrue(TEXT("Request is active before teardown"), Fixture.Prepare(ID))) { return false; }
    int32 Attempts = 0;
    Fixture.OnCompletion = [&]()
    {
        ++Attempts;
        TestFalse(TEXT("Completion during teardown cannot start public preparation"),
            Fixture.Save->PrepareLoadProgress(ReplacementID, FBHLoadProgressCompletion()));
        TestFalse(TEXT("Completion during teardown cannot accept in-memory replacement"), Fixture.Prepare(ReplacementID));
    };
    Fixture.Save->Deinitialize();
    Fixture.OnCompletion = nullptr;
    TestEqual(TEXT("Teardown sends exactly one terminal completion"), Fixture.Results.Num(), 1);
    TestEqual(TEXT("Completion attempted reentrant preparation once"), Attempts, 1);
    if (Fixture.Results.Num() == 1) { TestEqual(TEXT("Teardown cancels the active request"), Fixture.Results[0].Result, EBHLoadProgressResult::Cancelled); }
    TestTrue(TEXT("Teardown retires data, intent and timers"), FBHSaveSubsystemTestAccess::Cleared(*Fixture.Save));
    TestTrue(TEXT("Teardown does not unlock checkpoint writes"), FBHSaveSubsystemTestAccess::Protected(*Fixture.Save));
    Fixture.Save->Deinitialize();
    TestEqual(TEXT("Repeated teardown cannot repeat completion"), Fixture.Results.Num(), 1);
    return true;
}
#endif
