#if WITH_DEV_AUTOMATION_TESTS

#include "BHBleedingReloadTestObserver.h"
#include "BHCharacter.h"
#include "BHHealthComponent.h"
#include "BHInjuryComponent.h"
#include "BHWeaponComponent.h"
#include "Engine/World.h"
#include "EngineGlobals.h"
#include "Misc/AutomationTest.h"
#include "Templates/UnrealTemplate.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/StructOnScope.h"
#include "UObject/UnrealType.h"

namespace
{
struct FBHBleedingReloadWorld
{
    TStrongObjectPtr<UWorld> World;
    ABHCharacter* Character = nullptr;
    UBHHealthComponent* Health = nullptr;
    UBHInjuryComponent* Injury = nullptr;
    UBHWeaponComponent* Weapon = nullptr;
    TArray<UActorComponent*> StartedComponents;
    uint64 TimerFrame = GFrameCounter;

    FBHBleedingReloadWorld()
        : World(UWorld::CreateWorld(EWorldType::Game, false))
    {}

    ~FBHBleedingReloadWorld()
    {
        for (UActorComponent* Component : StartedComponents)
        {
            if (Component->HasBegunPlay()) { Component->EndPlay(EEndPlayReason::Quit); }
            Component->RegisterAllComponentTickFunctions(false);
        }
        World->DestroyWorld(false);
    }

    bool Initialize(FAutomationTestBase& Test)
    {
        const FURL URL(nullptr, *World->GetOutermost()->GetName(), TRAVEL_Absolute);
        World->InitializeActorsForPlay(URL);
        FActorSpawnParameters Parameters;
        Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        Character = World->SpawnActor<ABHCharacter>(Parameters);
        if (!Test.TestNotNull(TEXT("Real character spawned"), Character)) { return false; }
        Health = Character->GetHealthComponent();
        Injury = Character->FindComponentByClass<UBHInjuryComponent>();
        Weapon = Character->GetWeaponComponent();
        FClassProperty* RifleClass = FindFProperty<FClassProperty>(UBHWeaponComponent::StaticClass(), TEXT("DefaultRifleClass"));
        if (!Test.TestTrue(TEXT("Real combat components and native rifle setting exist"),
            Health && Injury && Weapon && RifleClass)) { return false; }
        RifleClass->SetObjectPropertyValue_InContainer(Weapon, ABHRifle::StaticClass());
        // Start the real components without character startup's HUD/map side effects.
        for (UActorComponent* Component : {static_cast<UActorComponent*>(Health),
            static_cast<UActorComponent*>(Injury), static_cast<UActorComponent*>(Weapon)})
        {
            Component->RegisterAllComponentTickFunctions(true);
            Component->BeginPlay();
            StartedComponents.Add(Component);
        }
        // Bind the same real character handlers as Character::BeginPlay.
        FScriptDelegate DamageHandler;
        DamageHandler.BindUFunction(Character, TEXT("HandlePlayerDamaged"));
        Health->OnDamaged.Add(DamageHandler);
        FScriptDelegate DeathHandler;
        DeathHandler.BindUFunction(Character, TEXT("HandleDeath"));
        Health->OnDeath.Add(DeathHandler);
        return Test.TestNotNull(TEXT("Normal weapon startup equipped rifle"), Weapon->GetEquippedRifle());
    }

    bool StartReload(FAutomationTestBase& Test)
    {
        return Test.TestTrue(TEXT("Partial magazine restored"), Weapon->RestoreAmmoState(5, 60)) &&
            Test.TestTrue(TEXT("Reload started"), Weapon->StartReload()) &&
            Test.TestTrue(TEXT("Reload is active"), Weapon->IsReloading());
    }

    void AdvanceReloadTimer()
    {
        // TimerManager executes once per frame; restore the global counter after
        // each manual tick and never tick the world or unrelated game systems.
        for (int32 Step = 0; Step < 2; ++Step)
        {
            TGuardValue<uint64> FrameGuard(GFrameCounter, ++TimerFrame);
            // The first tick promotes newly pending timers; the second expires them.
            World->GetTimerManager().Tick(10.0f);
        }
    }

    void StartBleeding(AActor* Causer)
    { Injury->RegisterBallisticHit(EBHPlayerHitZone::Torso, 10.0f, Causer); }
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBHBleedingPreservesReloadTest,
    "BrokenHorizon.Combat.BleedingReload.BleedingPreservesReload",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBHBleedingPreservesReloadTest::RunTest(const FString& Parameters)
{
    FBHBleedingReloadWorld Fixture;
    if (!Fixture.Initialize(*this)) { return false; }
    Fixture.StartBleeding(nullptr);
    TestTrue(TEXT("Ballistic injury causes bleeding"), Fixture.Injury->IsBleeding());
    if (!Fixture.StartReload(*this)) { return false; }
    const float InitialHealth = Fixture.Health->GetCurrentHealth();
    for (int32 Tick = 0; Tick < 4; ++Tick)
    {
        Fixture.Injury->TickComponent(0.5f, LEVELTICK_All, nullptr);
        TestTrue(TEXT("Actual bleed tick preserves reload"), Fixture.Weapon->IsReloading());
    }
    TestTrue(TEXT("Bleeding still drains health"), Fixture.Health->GetCurrentHealth() < InitialHealth);
    TestEqual(TEXT("No ammunition transferred before completion"), Fixture.Weapon->GetMagazineAmmo(), 5);
    Fixture.AdvanceReloadTimer();
    TestFalse(TEXT("Reload completes while bleeding"), Fixture.Weapon->IsReloading());
    const int32 MagazineSize = Fixture.Weapon->GetEquippedRifle()->GetConfig().MagazineSize;
    TestEqual(TEXT("Completion fills magazine"), Fixture.Weapon->GetMagazineAmmo(), MagazineSize);
    TestEqual(TEXT("Completion conserves total ammunition"), Fixture.Weapon->GetReserveAmmo(), 65 - MagazineSize);
    Fixture.AdvanceReloadTimer();
    TestEqual(TEXT("Later timer tick does not transfer ammunition again"), Fixture.Weapon->GetReserveAmmo(), 65 - MagazineSize);
    if (!Fixture.StartReload(*this)) { return false; }
    Fixture.Health->ApplyDamage(1.0f, nullptr);
    TestFalse(TEXT("Direct damage while bleeding interrupts reload"), Fixture.Weapon->IsReloading());
    Fixture.AdvanceReloadTimer();
    TestEqual(TEXT("Cancelled reload never loads magazine"), Fixture.Weapon->GetMagazineAmmo(), 5);
    TestEqual(TEXT("Cancelled reload preserves reserve"), Fixture.Weapon->GetReserveAmmo(), 60);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBHBleedingReloadClientPolicyTest,
    "BrokenHorizon.Combat.BleedingReload.OwnerNotificationPolicy",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBHBleedingReloadClientPolicyTest::RunTest(const FString& Parameters)
{
    FBHBleedingReloadWorld Fixture;
    if (!Fixture.Initialize(*this) || !Fixture.StartReload(*this)) { return false; }
    UFunction* Notification = Fixture.Character->FindFunction(TEXT("ClientNotifyCombatDamage"));
    FBoolProperty* Interrupt = Notification
        ? FindFProperty<FBoolProperty>(Notification, TEXT("bShouldInterruptReload")) : nullptr;
    if (!TestNotNull(TEXT("Owner damage notification carries interruption policy"), Interrupt)) { return false; }
    FStructOnScope Arguments(Notification);
    Interrupt->SetPropertyValue_InContainer(Arguments.GetStructMemory(), false);
    // Standalone invokes the actual owning-client RPC implementation locally.
    // This verifies its behavior, not network delivery or replication ordering.
    Fixture.Character->ProcessEvent(Notification, Arguments.GetStructMemory());
    TestTrue(TEXT("Ongoing owner notification preserves reload"), Fixture.Weapon->IsReloading());
    Interrupt->SetPropertyValue_InContainer(Arguments.GetStructMemory(), true);
    Fixture.Character->ProcessEvent(Notification, Arguments.GetStructMemory());
    TestFalse(TEXT("Direct owner notification interrupts reload"), Fixture.Weapon->IsReloading());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBHOngoingDamageDispatchContextTest,
    "BrokenHorizon.Combat.BleedingReload.NestedDamageContext",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBHOngoingDamageDispatchContextTest::RunTest(const FString& Parameters)
{
    TStrongObjectPtr<UBHHealthComponent> Health(NewObject<UBHHealthComponent>());
    TStrongObjectPtr<UBHBleedingReloadTestObserver> Observer(NewObject<UBHBleedingReloadTestObserver>());
    int32 Calls = 0;
    Observer->OnDamage = [&](float Damage, AActor* Causer)
    {
        ++Calls;
        if (Calls == 1)
        {
            TestTrue(TEXT("Outer damage is ongoing"), Health->IsDispatchingOngoingDamage());
            Health->ApplyDamage(2.0f, nullptr);
            TestTrue(TEXT("Nested direct dispatch restores outer ongoing context"), Health->IsDispatchingOngoingDamage());
        }
        else { TestFalse(TEXT("Nested ApplyDamage remains direct"), Health->IsDispatchingOngoingDamage()); }
    };
    Health->OnDamaged.AddDynamic(Observer.Get(), &UBHBleedingReloadTestObserver::HandleDamage);
    Health->ApplyOngoingDamage(1.0f, nullptr);
    TestEqual(TEXT("Both real damage events dispatched"), Calls, 2);
    TestFalse(TEXT("Ongoing context cleared after dispatch"), Health->IsDispatchingOngoingDamage());
    TestEqual(TEXT("Both events reduce health"), Health->GetCurrentHealth(), 97.0f);
    Health->OnDamaged.RemoveDynamic(Observer.Get(), &UBHBleedingReloadTestObserver::HandleDamage);
    Observer->OnDamage = nullptr;
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBHLethalBleedingReloadTest,
    "BrokenHorizon.Combat.BleedingReload.LethalBleedingStopsActions",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBHLethalBleedingReloadTest::RunTest(const FString& Parameters)
{
    FBHBleedingReloadWorld Fixture;
    if (!Fixture.Initialize(*this)) { return false; }
    AActor* Causer = Fixture.World->SpawnActor<AActor>();
    if (!TestNotNull(TEXT("Damage causer spawned"), Causer)) { return false; }
    Fixture.StartBleeding(Causer);
    // Persistent living health is clamped to at least one hit point.
    Fixture.Health->RestorePersistentHealthState(1.0f);
    TestEqual(TEXT("Lethal bleeding fixture starts at one health"), Fixture.Health->GetCurrentHealth(), 1.0f);
    if (!Fixture.StartReload(*this)) { return false; }
    TStrongObjectPtr<UBHBleedingReloadTestObserver> Observer(NewObject<UBHBleedingReloadTestObserver>());
    int32 Deaths = 0;
    Observer->OnDeath = [&](AActor* ActualCauser)
    {
        ++Deaths;
        TestTrue(TEXT("Bleed death retains original damage causer"), ActualCauser == Causer);
    };
    Fixture.Health->OnDeath.AddDynamic(Observer.Get(), &UBHBleedingReloadTestObserver::HandleDeath);
    // Three torso bleed intervals apply 1.2 damage, crossing the one-health floor.
    Fixture.Injury->TickComponent(1.5f, LEVELTICK_All, nullptr);
    TestTrue(TEXT("Actual bleeding can still kill"), Fixture.Health->IsDead());
    TestEqual(TEXT("Death dispatched once"), Deaths, 1);
    TestFalse(TEXT("Lethal bleeding terminates reload through death handler"), Fixture.Weapon->IsReloading());
    TestFalse(TEXT("Dead character cannot restart reload"), Fixture.Weapon->StartReload());
    TestEqual(TEXT("Death does not transfer reload ammunition"), Fixture.Weapon->GetMagazineAmmo(), 5);
    Fixture.Health->OnDeath.RemoveDynamic(Observer.Get(), &UBHBleedingReloadTestObserver::HandleDeath);
    Observer->OnDeath = nullptr;
    return true;
}

#endif
