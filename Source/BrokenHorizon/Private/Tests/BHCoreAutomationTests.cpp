#if WITH_DEV_AUTOMATION_TESTS

#include "BHHealthComponent.h"
#include "BHImpactEffect.h"
#include "BHSmokeGrenade.h"
#include "BHBattlefieldConditions.h"
#include "BHLoadoutWeight.h"
#include "BHAmbientWarDirector.h"
#include "BHAmmoHUDWidget.h"
#include "BHHitMarkerWidget.h"
#include "BHCharacter.h"
#include "BHCombatStatusWidget.h"
#include "BHEnemySoldier.h"
#include "BHEnemyAIController.h"
#include "BHEngineeringCharge.h"
#include "BHDoor.h"
#include "BHFieldTransport.h"
#include "BHFieldFortification.h"
#include "BHFieldSupportRelay.h"
#include "BHGameMode.h"
#include "BHInjuryComponent.h"
#include "BHMissionData.h"
#include "BHObjectiveComponent.h"
#include "BHObjectiveNotificationWidget.h"
#include "BHPlaytestTelemetrySubsystem.h"
#include "BHRaidSabotageTarget.h"
#include "BHRifle.h"
#include "BHSaveGame.h"
#include "BHSubtitleWidget.h"
#include "BHTacticalSupportZone.h"
#include "BHSectorAnchor.h"
#include "BHUIStyle.h"
#include "BHWarSubsystem.h"
#include "BHUserSettingsSaveGame.h"
#include "BHUserSettingsSubsystem.h"
#include "BHSectorResupplyStation.h"
#include "BHFragGrenade.h"
#include "BHSupplyConvoyTarget.h"
#include "BHWarOperationRules.h"
#include "BHWarGameState.h"
#include "BHWarMapWidget.h"
#include "BHWarSubsystem.h"
#include "BHWeaponComponent.h"

#include "Engine/GameInstance.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/AutomationTest.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBHPlaytestTelemetryContractTest,
    "BrokenHorizon.Technical.PlaytestTelemetry.Contract",
    EAutomationTestFlags::EditorContext |
        EAutomationTestFlags::EngineFilter
)

bool FBHPlaytestTelemetryContractTest::RunTest(
    const FString& Parameters
)
{
    TestEqual(
        TEXT("Telemetry tokens remove free-form punctuation"),
        UBHPlaytestTelemetrySubsystem::SanitizeToken(
            TEXT("Player Name <email@example.com>")
        ),
        FString(TEXT("Player_Name_emailexample.com"))
    );
    TestEqual(
        TEXT("Telemetry tokens are length bounded"),
        UBHPlaytestTelemetrySubsystem::SanitizeToken(
            TEXT("ABCDEFGHIJKLMNOPQRSTUVWXYZ"),
            8
        ).Len(),
        8
    );

    TMap<FString, FString> Fields;
    Fields.Add(TEXT("result"), TEXT("damage"));
    Fields.Add(TEXT("unsafe key"), TEXT("value with spaces"));
    for (int32 Index = 0; Index < 20; ++Index)
    {
        Fields.Add(
            FString::Printf(TEXT("field%d"), Index),
            TEXT("bounded")
        );
    }

    const FString Json =
        UBHPlaytestTelemetrySubsystem::BuildEventJsonForTesting(
            TEXT("player shot"),
            Fields
        );
    TestTrue(
        TEXT("Telemetry emits the stable schema"),
        Json.Contains(TEXT("\"schema\":1"))
    );
    TestTrue(
        TEXT("Telemetry event names are sanitized"),
        Json.Contains(TEXT("\"event\":\"player_shot\""))
    );
    TestTrue(
        TEXT("Telemetry field keys and values are sanitized"),
        Json.Contains(TEXT("\"unsafe_key\":\"value_with_spaces\""))
    );
    TestFalse(
        TEXT("Telemetry excludes raw free-form whitespace"),
        Json.Contains(TEXT("value with spaces"))
    );
    TestTrue(
        TEXT("Telemetry includes an anonymous session identifier"),
        Json.Contains(TEXT("\"session\":\"TESTSESSION\""))
    );
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBHFieldSquadCasualtyAidTest,
    "BrokenHorizon.PersistentWar.FieldSquad.CasualtyAid",
    EAutomationTestFlags::EditorContext |
        EAutomationTestFlags::EngineFilter
)

bool FBHFieldSquadCasualtyAidTest::RunTest(
    const FString& Parameters
)
{
    UBHInjuryComponent* InjuryComponent =
        NewObject<UBHInjuryComponent>();

    TestNotNull(
        TEXT("Casualty aid has an injury component"),
        InjuryComponent
    );

    if (!IsValid(InjuryComponent))
    {
        return false;
    }

    TestTrue(
        TEXT("Injury and medical state replicates by default"),
        InjuryComponent->GetIsReplicated()
    );

    InjuryComponent->RestorePersistentSupplyState(
        1,
        2,
        50.0f,
        50.0f
    );
    TestTrue(
        TEXT("One field dressing can stabilize an operative"),
        InjuryComponent->ConsumeFieldDressingForSquadAid()
    );
    TestEqual(
        TEXT("Casualty aid consumes exactly one dressing"),
        InjuryComponent->GetFieldDressingCount(),
        1
    );
    TestTrue(
        TEXT("Second dressing can also be consumed"),
        InjuryComponent->ConsumeFieldDressingForSquadAid()
    );
    TestFalse(
        TEXT("Casualty aid fails when no dressing remains"),
        InjuryComponent->ConsumeFieldDressingForSquadAid()
    );

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBHFieldSquadContextCasualtyAidTest,
    "BrokenHorizon.PersistentWar.FieldSquad.ContextCasualtyAid",
    EAutomationTestFlags::EditorContext |
        EAutomationTestFlags::EngineFilter
)

bool FBHFieldSquadContextCasualtyAidTest::RunTest(
    const FString& Parameters
)
{
    TestTrue(
        TEXT("Owned responder can aid a friendly casualty with supplies"),
        BHWarOperationRules::CanAssignFieldSquadCasualtyAid(
            true,
            true,
            false,
            1
        )
    );
    TestFalse(
        TEXT("Another player's responder cannot be silently redirected"),
        BHWarOperationRules::CanAssignFieldSquadCasualtyAid(
            false,
            true,
            false,
            1
        )
    );
    TestFalse(
        TEXT("Non-casualty targets do not receive a fabricated action"),
        BHWarOperationRules::CanAssignFieldSquadCasualtyAid(
            true,
            false,
            false,
            1
        )
    );
    TestFalse(
        TEXT("Embarked operatives cannot accept field aid orders"),
        BHWarOperationRules::CanAssignFieldSquadCasualtyAid(
            true,
            true,
            true,
            1
        )
    );
    TestFalse(
        TEXT("Aid is rejected before movement when supplies are empty"),
        BHWarOperationRules::CanAssignFieldSquadCasualtyAid(
            true,
            true,
            false,
            0
        )
    );

    const UFunction* ContextRPC = ABHCharacter::StaticClass()->FindFunctionByName(
        TEXT("ServerRequestFieldSquadContextAction")
    );
    TestNotNull(TEXT("Context action exposes an authoritative RPC"), ContextRPC);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBHFieldSquadContextSabotageTest,
    "BrokenHorizon.PersistentWar.FieldSquad.ContextSabotage",
    EAutomationTestFlags::EditorContext |
        EAutomationTestFlags::EngineFilter
)

bool FBHFieldSquadContextSabotageTest::RunTest(
    const FString& Parameters
)
{
    TestTrue(
        TEXT("Owned operative can receive an active raid objective"),
        BHWarOperationRules::CanAssignFieldSquadSabotage(
            true,
            true,
            false
        )
    );
    TestFalse(
        TEXT("Foreign operatives cannot be redirected"),
        BHWarOperationRules::CanAssignFieldSquadSabotage(
            false,
            true,
            false
        )
    );
    TestFalse(
        TEXT("Inactive and already-resolved targets reject assignment"),
        BHWarOperationRules::CanAssignFieldSquadSabotage(
            true,
            false,
            false
        )
    );
    TestFalse(
        TEXT("Embarked operatives cannot sabotage remotely"),
        BHWarOperationRules::CanAssignFieldSquadSabotage(
            true,
            true,
            true
        )
    );

    const UFunction* LegacyInteraction =
        ABHRaidSabotageTarget::StaticClass()->FindFunctionByName(
            TEXT("Interact")
        );
    TestNotNull(
        TEXT("Direct player sabotage remains interface-callable"),
        LegacyInteraction
    );

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBHFieldSquadContextObjectivePresenceTest,
    "BrokenHorizon.PersistentWar.FieldSquad.ContextObjectivePresence",
    EAutomationTestFlags::EditorContext |
        EAutomationTestFlags::EngineFilter
)

bool FBHFieldSquadContextObjectivePresenceTest::RunTest(
    const FString& Parameters
)
{
    TestTrue(
        TEXT("Owned operative can secure the assigned attack sector"),
        BHWarOperationRules::CanAssignFieldSquadObjectivePresence(
            true,
            true,
            EBHWarPriorityType::Attack,
            true,
            false
        )
    );
    TestTrue(
        TEXT("Owned operative can defend the assigned sector"),
        BHWarOperationRules::CanAssignFieldSquadObjectivePresence(
            true,
            true,
            EBHWarPriorityType::Defend,
            true,
            false
        )
    );
    TestFalse(
        TEXT("Raid objectives use sabotage rather than area presence"),
        BHWarOperationRules::CanAssignFieldSquadObjectivePresence(
            true,
            true,
            EBHWarPriorityType::Raid,
            true,
            false
        )
    );
    TestFalse(
        TEXT("A different sector marker cannot redirect the squad"),
        BHWarOperationRules::CanAssignFieldSquadObjectivePresence(
            true,
            true,
            EBHWarPriorityType::Attack,
            false,
            false
        )
    );
    TestFalse(
        TEXT("Inactive operations reject objective presence"),
        BHWarOperationRules::CanAssignFieldSquadObjectivePresence(
            true,
            false,
            EBHWarPriorityType::Defend,
            true,
            false
        )
    );
    TestFalse(
        TEXT("Foreign operatives cannot receive objective orders"),
        BHWarOperationRules::CanAssignFieldSquadObjectivePresence(
            false,
            true,
            EBHWarPriorityType::Defend,
            true,
            false
        )
    );
    TestFalse(
        TEXT("Embarked squads must disembark before objective orders"),
        BHWarOperationRules::CanAssignFieldSquadObjectivePresence(
            true,
            true,
            EBHWarPriorityType::Attack,
            true,
            true
        )
    );

    const FProperty* ContextTargetProperty =
        FindFProperty<FProperty>(
            ABHSectorAnchor::StaticClass(),
            TEXT("SquadContextTarget")
        );
    TestNotNull(
        TEXT("Sector anchors expose a source-backed Context target"),
        ContextTargetProperty
    );

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBHFieldSquadCasualtyEvacuationTest,
    "BrokenHorizon.PersistentWar.FieldSquad.CasualtyEvacuation",
    EAutomationTestFlags::EditorContext |
        EAutomationTestFlags::EngineFilter
)

bool FBHFieldSquadCasualtyEvacuationTest::RunTest(
    const FString& Parameters
)
{
    const FProperty* EvacuationProperty =
        FindFProperty<FProperty>(
            ABHEnemySoldier::StaticClass(),
            TEXT("bRequiresMedicalEvacuation")
        );
    TestTrue(
        TEXT("Medical evacuation state is replicated"),
        EvacuationProperty &&
            EvacuationProperty->HasAnyPropertyFlags(CPF_Net)
    );
    const FProperty* FieldSquadRosterProperty =
        FindFProperty<FProperty>(
            ABHCharacter::StaticClass(),
            TEXT("FieldSquadMembers")
        );
    TestTrue(
        TEXT("Owning client receives its field-squad roster"),
        FieldSquadRosterProperty &&
            FieldSquadRosterProperty->HasAnyPropertyFlags(CPF_Net)
    );

    TestTrue(
        TEXT("Following combat-effective operatives board"),
        BHWarOperationRules::
            IsFieldSquadMemberTransportEligible(
                false,
                false,
                false
            )
    );
    TestFalse(
        TEXT("Healthy operatives obey a hold order"),
        BHWarOperationRules::
            IsFieldSquadMemberTransportEligible(
                false,
                false,
                true
            )
    );
    TestTrue(
        TEXT("Downed operatives can be evacuated while squad holds"),
        BHWarOperationRules::
            IsFieldSquadMemberTransportEligible(
                true,
                true,
                true
            )
    );
    TestTrue(
        TEXT("Stabilized casualties can evacuate while squad holds"),
        BHWarOperationRules::
            IsFieldSquadMemberTransportEligible(
                false,
                false,
                true,
                true
            )
    );
    TestFalse(
        TEXT("Expired casualties cannot board"),
        BHWarOperationRules::
            IsFieldSquadMemberTransportEligible(
                true,
                false,
                false
            )
    );
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBHFieldSquadReadinessTest,
    "BrokenHorizon.PersistentWar.FieldSquad.CombatReadiness",
    EAutomationTestFlags::EditorContext |
        EAutomationTestFlags::EngineFilter
)

bool FBHFieldSquadReadinessTest::RunTest(
    const FString& Parameters
)
{
    TestTrue(
        TEXT("Full readiness adds no weapon spread"),
        FMath::IsNearlyZero(
            BHWarOperationRules::
                CalculateFieldOperativeReadinessSpread(
                    1.0f,
                    4.0f
                )
        )
    );
    TestTrue(
        TEXT("Post-casualty readiness adds controlled spread"),
        FMath::IsNearlyEqual(
            BHWarOperationRules::
                CalculateFieldOperativeReadinessSpread(
                    0.55f,
                    4.0f
                ),
            1.8f
        )
    );
    TestTrue(
        TEXT("Full readiness preserves normal firing cadence"),
        FMath::IsNearlyEqual(
            BHWarOperationRules::
                CalculateFieldOperativeFireInterval(
                    0.75f,
                    1.0f,
                    1.35f
                ),
            0.75f
        )
    );
    TestTrue(
        TEXT("Post-casualty readiness slows firing cadence"),
        FMath::IsNearlyEqual(
            BHWarOperationRules::
                CalculateFieldOperativeFireInterval(
                    0.75f,
                    0.55f,
                    1.35f
                ),
            0.868125f
        )
    );
    TestTrue(
        TEXT("Readiness inputs clamp below zero"),
        FMath::IsNearlyEqual(
            BHWarOperationRules::
                CalculateFieldOperativeReadinessSpread(
                    -1.0f,
                    4.0f
                ),
            4.0f
        )
    );
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBHFieldSquadServiceCostTest,
    "BrokenHorizon.PersistentWar.Logistics.FieldSquadServiceCost",
    EAutomationTestFlags::EditorContext |
        EAutomationTestFlags::EngineFilter
)

bool FBHFieldSquadServiceCostTest::RunTest(
    const FString& Parameters
)
{
    TestEqual(
        TEXT("Three depleted operatives cost six supply"),
        ABHSectorResupplyStation::
            CalculateFireteamServiceSupplyCost(3, 2.0f),
        6.0f
    );
    TestEqual(
        TEXT("No depleted operatives cost no supply"),
        ABHSectorResupplyStation::
            CalculateFireteamServiceSupplyCost(0, 2.0f),
        0.0f
    );
    TestEqual(
        TEXT("Invalid negative inputs cannot generate supply"),
        ABHSectorResupplyStation::
            CalculateFireteamServiceSupplyCost(-2, -4.0f),
        0.0f
    );
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBHEmergencyFallbackKitTest,
    "BrokenHorizon.PersistentWar.Logistics.EmergencyFallbackKit",
    EAutomationTestFlags::EditorContext |
        EAutomationTestFlags::EngineFilter
)

bool FBHEmergencyFallbackKitTest::RunTest(
    const FString& Parameters
)
{
    TestTrue(
        TEXT("No reserve ammunition qualifies for recovery"),
        ABHSectorResupplyStation::ShouldIssueEmergencyFallbackKit(
            0,
            2,
            3
        )
    );
    TestTrue(
        TEXT("No medical resources qualifies for recovery"),
        ABHSectorResupplyStation::ShouldIssueEmergencyFallbackKit(
            30,
            0,
            0
        )
    );
    TestFalse(
        TEXT("A combat-effective loadout cannot farm free kits"),
        ABHSectorResupplyStation::ShouldIssueEmergencyFallbackKit(
            1,
            0,
            1
        )
    );
    TestEqual(
        TEXT("An empty reserve receives one fallback magazine"),
        ABHSectorResupplyStation::
            CalculateEmergencyFallbackAmmoRequest(0, 90, 30),
        30
    );
    TestEqual(
        TEXT("Fallback ammunition respects reserve capacity"),
        ABHSectorResupplyStation::
            CalculateEmergencyFallbackAmmoRequest(80, 90, 30),
        10
    );
    TestEqual(
        TEXT("Invalid fallback values cannot add ammunition"),
        ABHSectorResupplyStation::
            CalculateEmergencyFallbackAmmoRequest(20, 90, -30),
        0
    );
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBHHealthLifecycleTest,
    "BrokenHorizon.Gameplay.Health.Lifecycle",
    EAutomationTestFlags::EditorContext |
        EAutomationTestFlags::EngineFilter
)

bool FBHHealthLifecycleTest::RunTest(const FString& Parameters)
{
    UBHHealthComponent* Health =
        NewObject<UBHHealthComponent>(GetTransientPackage());
    TestNotNull(TEXT("Health component is created"), Health);
    if (!Health)
    {
        return false;
    }

    TestTrue(
        TEXT("Health component replicates by default"),
        Health->GetIsReplicated()
    );
    Health->ResetHealth();
    TestEqual(TEXT("Health starts full"), Health->GetCurrentHealth(), 100.0f);
    TestEqual(TEXT("Damage is applied"), Health->ApplyDamage(35.0f, nullptr), 35.0f);
    TestEqual(TEXT("Health decreases"), Health->GetCurrentHealth(), 65.0f);
    TestEqual(TEXT("Healing is applied"), Health->Heal(20.0f), 20.0f);
    TestEqual(TEXT("Health increases"), Health->GetCurrentHealth(), 85.0f);
    TestEqual(TEXT("Lethal damage clamps"), Health->ApplyDamage(500.0f, nullptr), 85.0f);
    TestTrue(TEXT("Lethal damage marks death"), Health->IsDead());
    TestEqual(TEXT("Dead actors cannot heal"), Health->Heal(10.0f), 0.0f);

    TestEqual(
        TEXT("Friendly fire retains risk at thirty-five percent damage"),
        UBHHealthComponent::CalculateFriendlyFireDamage(
            40.0f, true, 0.35f),
        14.0f
    );
    TestEqual(
        TEXT("Hostile fire keeps full damage"),
        UBHHealthComponent::CalculateFriendlyFireDamage(
            40.0f, false, 0.35f),
        40.0f
    );
    TestTrue(
        TEXT("Third rapid friendly hit escalates combat discipline"),
        ABHCharacter::ShouldEscalateFriendlyFire(3, 3)
    );
    TestFalse(
        TEXT("A single accidental hit remains a warning"),
        ABHCharacter::ShouldEscalateFriendlyFire(1, 3)
    );

    Health->RestorePersistentHealthState(-50.0f);
    TestEqual(TEXT("Restored health clamps above zero"), Health->GetCurrentHealth(), 1.0f);
    TestFalse(TEXT("Restored health revives"), Health->IsDead());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBHEnemyLethalDamageReactionTest,
    "BrokenHorizon.Gameplay.AI.LethalDamageReaction",
    EAutomationTestFlags::EditorContext |
        EAutomationTestFlags::EngineFilter
)

bool FBHEnemyLethalDamageReactionTest::RunTest(
    const FString& Parameters
)
{
    TestTrue(
        TEXT("Living wounded enemies notify their controller"),
        ABHEnemySoldier::ShouldNotifyControllerOfDamage(25.0f, false)
    );
    TestFalse(
        TEXT("Zero-health enemies do not start a damage reaction"),
        ABHEnemySoldier::ShouldNotifyControllerOfDamage(0.0f, false)
    );
    TestFalse(
        TEXT("Already dead enemies do not start a damage reaction"),
        ABHEnemySoldier::ShouldNotifyControllerOfDamage(25.0f, true)
    );
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBHEnemyVisualContactHandoffTest,
    "BrokenHorizon.Gameplay.AI.VisualContactHandoff",
    EAutomationTestFlags::EditorContext |
        EAutomationTestFlags::EngineFilter
)

bool FBHEnemyVisualContactHandoffTest::RunTest(
    const FString& Parameters
)
{
    TestFalse(
        TEXT("A new unseen target clears stale visual contact"),
        ABHEnemyAIController::ResolveVisualContactState(
            true, false, true, false)
    );
    TestFalse(
        TEXT("A squad alert does not claim visual contact"),
        ABHEnemyAIController::ResolveVisualContactState(
            false, true, true, false)
    );
    TestTrue(
        TEXT("An unchanged target retains valid visual contact"),
        ABHEnemyAIController::ResolveVisualContactState(
            false, false, true, false)
    );
    TestTrue(
        TEXT("Direct line of sight confirms a new target"),
        ABHEnemyAIController::ResolveVisualContactState(
            true, false, false, true)
    );
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBHWeaponReplicationContractTest,
    "BrokenHorizon.Gameplay.Weapon.ReplicationContract",
    EAutomationTestFlags::EditorContext |
        EAutomationTestFlags::EngineFilter
)

bool FBHWeaponReplicationContractTest::RunTest(
    const FString& Parameters
)
{
    UBHWeaponComponent* Weapon =
        NewObject<UBHWeaponComponent>(GetTransientPackage());

    TestNotNull(TEXT("Weapon component is created"), Weapon);

    if (!IsValid(Weapon))
    {
        return false;
    }

    TestTrue(
        TEXT("Weapon component replicates by default"),
        Weapon->GetIsReplicated()
    );
    TestEqual(
        TEXT("Unowned weapon component starts without magazine ammo"),
        Weapon->GetMagazineAmmo(),
        0
    );
    TestEqual(
        TEXT("Unowned weapon component starts without reserve ammo"),
        Weapon->GetReserveAmmo(),
        0
    );
    TestFalse(
        TEXT("Unowned weapon component cannot mutate reserve ammo"),
        Weapon->AddReserveAmmo(30) > 0
    );

    const UClass* WeaponClass =
        UBHWeaponComponent::StaticClass();
    const UFunction* ServerStartFiring =
        WeaponClass->FindFunctionByName(
            TEXT("ServerStartFiring")
        );
    const UFunction* ServerStopFiring =
        WeaponClass->FindFunctionByName(
            TEXT("ServerStopFiring")
        );
    const UFunction* ServerStartReload =
        WeaponClass->FindFunctionByName(
            TEXT("ServerStartReload")
        );
    const UFunction* MulticastPresentation =
        WeaponClass->FindFunctionByName(
            TEXT("MulticastFirePresentation")
        );

    TestNotNull(
        TEXT("Start-fire server RPC exists"),
        ServerStartFiring
    );
    TestNotNull(
        TEXT("Stop-fire server RPC exists"),
        ServerStopFiring
    );
    TestNotNull(
        TEXT("Reload server RPC exists"),
        ServerStartReload
    );
    TestNotNull(
        TEXT("Shot-presentation multicast exists"),
        MulticastPresentation
    );

    if (ServerStartFiring)
    {
        TestTrue(
            TEXT("Start-fire RPC executes on the server"),
            ServerStartFiring->HasAnyFunctionFlags(FUNC_NetServer)
        );
        TestTrue(
            TEXT("Start-fire RPC is reliable"),
            ServerStartFiring->HasAnyFunctionFlags(FUNC_NetReliable)
        );
    }

    if (ServerStopFiring)
    {
        TestTrue(
            TEXT("Stop-fire RPC executes on the server"),
            ServerStopFiring->HasAnyFunctionFlags(FUNC_NetServer)
        );
        TestTrue(
            TEXT("Stop-fire RPC is reliable"),
            ServerStopFiring->HasAnyFunctionFlags(FUNC_NetReliable)
        );
    }

    if (ServerStartReload)
    {
        TestTrue(
            TEXT("Reload RPC executes on the server"),
            ServerStartReload->HasAnyFunctionFlags(FUNC_NetServer)
        );
        TestTrue(
            TEXT("Reload RPC is reliable"),
            ServerStartReload->HasAnyFunctionFlags(FUNC_NetReliable)
        );
    }

    if (MulticastPresentation)
    {
        TestTrue(
            TEXT("Accepted shot presentation multicasts"),
            MulticastPresentation->HasAnyFunctionFlags(
                FUNC_NetMulticast
            )
        );
        TestFalse(
            TEXT("Per-shot presentation remains unreliable"),
            MulticastPresentation->HasAnyFunctionFlags(
                FUNC_NetReliable
            )
        );
    }

    const FProperty* MagazineProperty =
        FindFProperty<FProperty>(
            WeaponClass,
            TEXT("MagazineAmmo")
        );
    const FProperty* ReserveProperty =
        FindFProperty<FProperty>(
            WeaponClass,
            TEXT("ReserveAmmo")
        );
    const FProperty* StateProperty =
        FindFProperty<FProperty>(
            WeaponClass,
            TEXT("WeaponState")
        );

    TestTrue(
        TEXT("Magazine ammunition is replicated"),
        MagazineProperty &&
            MagazineProperty->HasAnyPropertyFlags(CPF_Net)
    );
    TestTrue(
        TEXT("Reserve ammunition is replicated"),
        ReserveProperty &&
            ReserveProperty->HasAnyPropertyFlags(CPF_Net)
    );
    TestTrue(
        TEXT("Weapon state is replicated"),
        StateProperty &&
            StateProperty->HasAnyPropertyFlags(CPF_Net)
    );
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBHWarReplicationContractTest,
    "BrokenHorizon.PersistentWar.Network.AuthorityContract",
    EAutomationTestFlags::EditorContext |
        EAutomationTestFlags::EngineFilter
)

bool FBHWarReplicationContractTest::RunTest(
    const FString& Parameters
)
{
    const ABHGameMode* GameModeDefaults =
        GetDefault<ABHGameMode>();
    const ABHWarGameState* GameStateDefaults =
        GetDefault<ABHWarGameState>();

    TestNotNull(
        TEXT("Broken Horizon game mode defaults exist"),
        GameModeDefaults
    );
    TestNotNull(
        TEXT("Authoritative war game state defaults exist"),
        GameStateDefaults
    );

    if (!IsValid(GameModeDefaults) ||
        !IsValid(GameStateDefaults))
    {
        return false;
    }

    TestEqual(
        TEXT("Broken Horizon uses the replicated war game state"),
        GameModeDefaults->GameStateClass.Get(),
        ABHWarGameState::StaticClass()
    );
    TestTrue(
        TEXT("Campaign server travel preserves client connections"),
        GameModeDefaults->bUseSeamlessTravel
    );
    TestTrue(
        TEXT("War game state replicates"),
        GameStateDefaults->GetIsReplicated()
    );

    const FProperty* SnapshotProperty =
        FindFProperty<FProperty>(
            ABHWarGameState::StaticClass(),
            TEXT("WarStateSnapshot")
        );
    TestTrue(
        TEXT("Authoritative war snapshot is replicated"),
        SnapshotProperty &&
            SnapshotProperty->HasAnyPropertyFlags(CPF_Net)
    );

    UGameInstance* SourceGameInstance =
        NewObject<UGameInstance>(GetTransientPackage());
    UGameInstance* ReplicaGameInstance =
        NewObject<UGameInstance>(GetTransientPackage());
    UBHWarSubsystem* SourceWar =
        NewObject<UBHWarSubsystem>(SourceGameInstance);
    UBHWarSubsystem* ReplicaWar =
        NewObject<UBHWarSubsystem>(ReplicaGameInstance);

    TestNotNull(TEXT("Source war subsystem is created"), SourceWar);
    TestNotNull(TEXT("Replica war subsystem is created"), ReplicaWar);

    if (!IsValid(SourceWar) || !IsValid(ReplicaWar))
    {
        return false;
    }

    SourceWar->ResetCampaign();
    SourceWar->AdvanceWarTurn();
    TestTrue(
        TEXT("Server commits an operation before replication"),
        SourceWar->SetCommittedOperation(
            SourceWar->GetPrioritySectorID(),
            SourceWar->GetPriorityType()
        )
    );

    const FBHWarStateSnapshot Snapshot =
        SourceWar->CaptureReplicatedSnapshot(17);

    TestTrue(
        TEXT("Captured war snapshot is initialized"),
        Snapshot.bInitialized
    );
    TestEqual(
        TEXT("Captured war snapshot retains its revision"),
        Snapshot.Revision,
        17
    );
    TestEqual(
        TEXT("Captured war snapshot contains every sector"),
        Snapshot.SectorStates.Num(),
        SourceWar->GetSectorStates().Num()
    );
    TestEqual(
        TEXT("Captured war snapshot contains the server turn"),
        Snapshot.TurnNumber,
        SourceWar->GetTurnNumber()
    );
    TestEqual(
        TEXT("Captured snapshot contains stable operation identity"),
        Snapshot.CommittedOperationID,
        SourceWar->GetCommittedOperationID()
    );
    TestEqual(
        TEXT("Captured snapshot contains stable operation target identity"),
        Snapshot.CommittedOperationTargetID,
        SourceWar->GetCommittedOperationTargetID()
    );

    TestTrue(
        TEXT("Replica accepts the authoritative snapshot"),
        ReplicaWar->ApplyReplicatedSnapshot(Snapshot)
    );
    TestEqual(
        TEXT("Replica turn matches the server"),
        ReplicaWar->GetTurnNumber(),
        SourceWar->GetTurnNumber()
    );
    TestEqual(
        TEXT("Replica sector count matches the server"),
        ReplicaWar->GetSectorStates().Num(),
        SourceWar->GetSectorStates().Num()
    );
    TestEqual(
        TEXT("Replica priority sector matches the server"),
        ReplicaWar->GetPrioritySectorID(),
        SourceWar->GetPrioritySectorID()
    );
    TestEqual(
        TEXT("Replica operation identity matches the server"),
        ReplicaWar->GetCommittedOperationID(),
        SourceWar->GetCommittedOperationID()
    );
    TestEqual(
        TEXT("Replica operation target identity matches the server"),
        ReplicaWar->GetCommittedOperationTargetID(),
        SourceWar->GetCommittedOperationTargetID()
    );
    TestEqual(
        TEXT("Replica friendly manpower matches the server"),
        ReplicaWar->GetFactionManpowerReserve(
            EBHWarFaction::Friendly
        ),
        SourceWar->GetFactionManpowerReserve(
            EBHWarFaction::Friendly
        )
    );
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBHObjectiveSequenceTest,
    "BrokenHorizon.Gameplay.Objectives.OrderedSequence",
    EAutomationTestFlags::EditorContext |
        EAutomationTestFlags::EngineFilter
)

bool FBHObjectiveSequenceTest::RunTest(const FString& Parameters)
{
    const TArray<FName> ExpectedOrder = {
        BHObjectiveIds::FindRedKeycard,
        BHObjectiveIds::UnlockSecurityDoor,
        BHObjectiveIds::EliminateGuard,
        BHObjectiveIds::ReachExtraction
    };

    TArray<FBHObjectiveDefinition> Definitions;
    for (const FName ObjectiveID : ExpectedOrder)
    {
        FBHObjectiveDefinition& Definition = Definitions.AddDefaulted_GetRef();
        Definition.ObjectiveID = ObjectiveID;
        Definition.DisplayText = FText::FromName(ObjectiveID);
    }

    UBHObjectiveComponent* Objectives =
        NewObject<UBHObjectiveComponent>(GetTransientPackage());
    TestNotNull(TEXT("Objective component is created"), Objectives);
    if (!Objectives)
    {
        return false;
    }

    TestTrue(
        TEXT("Objective component replicates shared mission state"),
        Objectives->GetIsReplicated()
    );
    for (const FName PropertyName : {
             FName(TEXT("CurrentObjectiveID")),
             FName(TEXT("CompletedObjectiveIDs")),
             FName(TEXT("bMissionComplete")),
             FName(TEXT("bMissionFailed"))
         })
    {
        const FProperty* Property = FindFProperty<FProperty>(
            UBHObjectiveComponent::StaticClass(),
            PropertyName
        );
        TestTrue(
            *FString::Printf(
                TEXT("%s is an authoritative replicated field"),
                *PropertyName.ToString()
            ),
            Property && Property->HasAnyPropertyFlags(CPF_Net)
        );
    }

    Objectives->StartRuntimeMission(Definitions);
    TestEqual(TEXT("First objective activates"), Objectives->GetCurrentObjectiveID(), ExpectedOrder[0]);
    TestFalse(
        TEXT("Out-of-order completion is rejected"),
        Objectives->CompleteObjectiveByID(ExpectedOrder[1])
    );

    for (int32 Index = 0; Index < ExpectedOrder.Num(); ++Index)
    {
        TestTrue(
            *FString::Printf(TEXT("Objective %s completes"), *ExpectedOrder[Index].ToString()),
            Objectives->CompleteObjectiveByID(ExpectedOrder[Index])
        );
    }

    TestTrue(TEXT("Mission completes after final objective"), Objectives->IsMissionComplete());
    TestTrue(TEXT("Current objective clears"), Objectives->GetCurrentObjectiveID().IsNone());
    TestEqual(
        TEXT("Every objective is recorded once"),
        Objectives->GetCompletedObjectiveIDs().Num(),
        ExpectedOrder.Num()
    );
    Objectives->ClearMissionState();
    TestFalse(
        TEXT("Post-operation free roam clears completion state"),
        Objectives->IsMissionComplete()
    );
    TestFalse(
        TEXT("Post-operation free roam clears failure state"),
        Objectives->IsMissionFailed()
    );
    TestTrue(
        TEXT("Post-operation free roam has no active objective"),
        Objectives->GetCurrentObjectiveID().IsNone()
    );
    TestTrue(
        TEXT("Post-operation free roam clears runtime objectives"),
        Objectives->GetRuntimeObjectiveDefinitions().IsEmpty()
    );
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBHFriendlyFormationTest,
    "BrokenHorizon.Gameplay.AI.FriendlyFormation",
    EAutomationTestFlags::EditorContext |
        EAutomationTestFlags::EngineFilter
)

bool FBHFriendlyFormationTest::RunTest(
    const FString& Parameters
)
{
    const FVector LeftWing =
        BHWarOperationRules::CalculateFriendlyFormationOffset(0);
    const FVector RightWing =
        BHWarOperationRules::CalculateFriendlyFormationOffset(1);
    const FVector RearLeft =
        BHWarOperationRules::CalculateFriendlyFormationOffset(2);

    TestTrue(
        TEXT("First ally trails on the left"),
        LeftWing.Equals(FVector(-350.0f, -250.0f, 0.0f))
    );
    TestTrue(
        TEXT("Second ally trails on the right"),
        RightWing.Equals(FVector(-350.0f, 250.0f, 0.0f))
    );
    TestTrue(
        TEXT("Additional allies form a deeper second rank"),
        RearLeft.Equals(FVector(-600.0f, -250.0f, 0.0f))
    );
    TestTrue(
        TEXT("Negative indices safely use the lead-left slot"),
        BHWarOperationRules::CalculateFriendlyFormationOffset(-1)
            .Equals(LeftWing)
    );
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBHEnemyNavigationFailureFallbackTest,
    "BrokenHorizon.Gameplay.AI.NavigationFailureFallback",
    EAutomationTestFlags::EditorContext |
        EAutomationTestFlags::EngineFilter
)

bool FBHEnemyNavigationFailureFallbackTest::RunTest(
    const FString& Parameters
)
{
    TestEqual(
        TEXT("Failed investigation degrades to a bounded search"),
        ABHEnemyAIController::ResolveNavigationFailureFallback(
            EBHEnemyAIState::AlertInvestigate
        ),
        EBHEnemyAIState::Search
    );
    TestEqual(
        TEXT("Failed explosive evasion searches from the safe hold"),
        ABHEnemyAIController::ResolveNavigationFailureFallback(
            EBHEnemyAIState::EvadeExplosive
        ),
        EBHEnemyAIState::Search
    );
    TestEqual(
        TEXT("Failed return does not falsely retain return state"),
        ABHEnemyAIController::ResolveNavigationFailureFallback(
            EBHEnemyAIState::ReturnToPatrol
        ),
        EBHEnemyAIState::Patrol
    );
    TestEqual(
        TEXT("Combat failure holds combat state for a bounded retry"),
        ABHEnemyAIController::ResolveNavigationFailureFallback(
            EBHEnemyAIState::Combat
        ),
        EBHEnemyAIState::Combat
    );
    TestEqual(
        TEXT("Retreat failure preserves withdrawal resolution"),
        ABHEnemyAIController::ResolveNavigationFailureFallback(
            EBHEnemyAIState::Retreat
        ),
        EBHEnemyAIState::Retreat
    );
    TestEqual(
        TEXT("Repeated search failure safely holds search state"),
        ABHEnemyAIController::ResolveNavigationFailureFallback(
            EBHEnemyAIState::Search
        ),
        EBHEnemyAIState::Search
    );
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBHSharedSquadPingContractTest,
    "BrokenHorizon.Multiplayer.Coordination.SharedSquadPing",
    EAutomationTestFlags::EditorContext |
        EAutomationTestFlags::EngineFilter
)

bool FBHSharedSquadPingContractTest::RunTest(
    const FString& Parameters
)
{
    FBHSquadPingSnapshot Ping;
    Ping.Revision = 3;
    Ping.Location = FVector(1200.0f, -300.0f, 50.0f);
    Ping.ContextLabel = FName(TEXT("HOSTILE"));
    Ping.IssuerLabel = FName(TEXT("ALPHA"));
    Ping.ExpiryServerWorldTimeSeconds = 25.0f;

    TestTrue(
        TEXT("Replicated squad ping remains active before server expiry"),
        Ping.IsActiveAt(24.99f)
    );
    TestFalse(
        TEXT("Replicated squad ping expires at its server deadline"),
        Ping.IsActiveAt(25.0f)
    );
    TestEqual(
        TEXT("Nearby squad ping label includes context distance and issuer"),
        UBHCombatStatusWidget::BuildSquadPingWaypointLabel(
            12500.0f,
            TEXT("hostile"),
            TEXT("alpha")
        ),
        FString(TEXT("PING // HOSTILE // 125 M // ALPHA"))
    );
    TestEqual(
        TEXT("Long-range squad ping label uses kilometers"),
        UBHCombatStatusWidget::BuildSquadPingWaypointLabel(
            150000.0f,
            TEXT("objective"),
            TEXT("bravo")
        ),
        FString(TEXT("PING // OBJECTIVE // 1.5 KM // BRAVO"))
    );
    TestEqual(
        TEXT("Visible moving target is explicitly tracked"),
        UBHCombatStatusWidget::BuildSquadPingWaypointLabel(
            8400.0f,
            TEXT("hostile"),
            TEXT("alpha"),
            true,
            true
        ),
        FString(TEXT("PING // HOSTILE // 84 M // ALPHA // TRACKED"))
    );
    TestEqual(
        TEXT("Occluded moving target becomes last-known intel"),
        UBHCombatStatusWidget::BuildSquadPingWaypointLabel(
            9100.0f,
            TEXT("hostile"),
            TEXT("alpha"),
            true,
            false
        ),
        FString(TEXT("PING // HOSTILE // 91 M // ALPHA // LAST KNOWN"))
    );

    const AActor* TrackedTarget =
        ABHEnemySoldier::StaticClass()->GetDefaultObject<AActor>();
    const AActor* Occluder =
        ABHCharacter::StaticClass()->GetDefaultObject<AActor>();
    TestTrue(
        TEXT("Unblocked trace keeps moving-target tracking live"),
        UBHCombatStatusWidget::IsSquadPingTargetVisible(
            false,
            nullptr,
            TrackedTarget
        )
    );
    TestTrue(
        TEXT("Trace hitting the marked actor keeps tracking live"),
        UBHCombatStatusWidget::IsSquadPingTargetVisible(
            true,
            TrackedTarget,
            TrackedTarget
        )
    );
    TestFalse(
        TEXT("Solid occluder freezes the marker at last-known position"),
        UBHCombatStatusWidget::IsSquadPingTargetVisible(
            true,
            Occluder,
            TrackedTarget
        )
    );

    const FProperty* PingProperty = FindFProperty<FProperty>(
        ABHWarGameState::StaticClass(),
        FName(TEXT("SquadPingSnapshot"))
    );
    TestTrue(
        TEXT("Shared squad ping snapshot is replicated to every client"),
        PingProperty && PingProperty->HasAnyPropertyFlags(CPF_Net)
    );
    const FProperty* TrackedActorProperty = FindFProperty<FProperty>(
        FBHSquadPingSnapshot::StaticStruct(),
        FName(TEXT("TrackedActor"))
    );
    TestNotNull(
        TEXT("Squad ping snapshot can carry an authoritative moving target"),
        TrackedActorProperty
    );
    const UFunction* PingRPC = ABHCharacter::StaticClass()->FindFunctionByName(
        FName(TEXT("ServerRequestSquadPing"))
    );
    TestTrue(
        TEXT("Squad ping request is an authoritative server RPC"),
        PingRPC && PingRPC->HasAnyFunctionFlags(FUNC_NetServer)
    );
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBHFieldTransportPassengerProtectionTest,
    "BrokenHorizon.Gameplay.Transport.PassengerProtection",
    EAutomationTestFlags::EditorContext |
        EAutomationTestFlags::EngineFilter
)

bool FBHFieldTransportPassengerProtectionTest::RunTest(
    const FString& Parameters
)
{
    TestEqual(
        TEXT("Passengers receive reduced protected crew damage"),
        ABHFieldTransport::CalculatePassengerDamage(
            100.0f,
            0.35f,
            0.65f
        ),
        22.75f
    );
    TestEqual(
        TEXT("Negative vehicle damage cannot harm passengers"),
        ABHFieldTransport::CalculatePassengerDamage(
            -100.0f,
            0.35f,
            0.65f
        ),
        0.0f
    );
    TestEqual(
        TEXT("Protection inputs are clamped to safe bounds"),
        ABHFieldTransport::CalculatePassengerDamage(
            100.0f,
            2.0f,
            2.0f
        ),
        100.0f
    );
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBHFieldTransportPassengerPersistenceTest,
    "BrokenHorizon.Gameplay.Transport.PassengerPersistence",
    EAutomationTestFlags::EditorContext |
        EAutomationTestFlags::EngineFilter
)

bool FBHFieldTransportPassengerPersistenceTest::RunTest(
    const FString& Parameters
)
{
    const FName WesternTransport(
        TEXT("WesternFOBFieldTransport01")
    );
    const FName DovrenTransport(
        TEXT("DovrenVillageFieldTransport01")
    );

    TestTrue(
        TEXT("Legacy driver saves retain automatic passenger boarding"),
        ABHFieldTransport::ShouldRestoreFieldSquadPassengers(
            true,
            false,
            false,
            NAME_None,
            WesternTransport
        )
    );
    TestTrue(
        TEXT("Explicit embarked squad restores into its saved vehicle"),
        ABHFieldTransport::ShouldRestoreFieldSquadPassengers(
            true,
            true,
            true,
            WesternTransport,
            WesternTransport
        )
    );
    TestFalse(
        TEXT("Explicit disembarked squad stays out of the vehicle"),
        ABHFieldTransport::ShouldRestoreFieldSquadPassengers(
            true,
            true,
            false,
            WesternTransport,
            WesternTransport
        )
    );
    TestFalse(
        TEXT("Passenger state cannot migrate to another vehicle"),
        ABHFieldTransport::ShouldRestoreFieldSquadPassengers(
            true,
            true,
            true,
            WesternTransport,
            DovrenTransport
        )
    );
    TestFalse(
        TEXT("An unoccupied vehicle cannot restore passengers"),
        ABHFieldTransport::ShouldRestoreFieldSquadPassengers(
            false,
            true,
            true,
            WesternTransport,
            WesternTransport
        )
    );

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBHFieldSquadStatusLabelTest,
    "BrokenHorizon.Gameplay.UI.FieldSquadStatus",
    EAutomationTestFlags::EditorContext |
        EAutomationTestFlags::EngineFilter
)

bool FBHFieldSquadStatusLabelTest::RunTest(
    const FString& Parameters
)
{
    TestEqual(
        TEXT("Following fireteam exposes its command shortcut"),
        UBHCombatStatusWidget::BuildFieldSquadStatusLabel(
            3,
            3,
            false,
            false,
            0
        ),
        FString(
            TEXT(
                "FIRETEAM // 3/3 // READY\n"
                "ORDER FOLLOW // AIM + [C] MOVE/HOLD"
            )
        )
    );
    TestEqual(
        TEXT("Holding fireteam exposes the follow shortcut"),
        UBHCombatStatusWidget::BuildFieldSquadStatusLabel(
            2,
            3,
            true,
            false,
            1
        ),
        FString(
            TEXT("FIRETEAM // 2/3 // SERVICE 1\n"
                 "ORDER HOLD // [C] FOLLOW")
        )
    );
    TestEqual(
        TEXT("Vehicle passengers show protected mounted state"),
        UBHCombatStatusWidget::BuildFieldSquadStatusLabel(
            2,
            3,
            false,
            true,
            2
        ),
        FString(
            TEXT("FIRETEAM // 2/3 // SERVICE 2\n"
                 "MOUNTED // VEHICLE PROTECTED")
        )
    );
    TestEqual(
        TEXT("Stabilized casualties receive explicit medevac status"),
        UBHCombatStatusWidget::BuildFieldSquadStatusLabel(
            2,
            3,
            true,
            false,
            1,
            1
        ),
        FString(
            TEXT("FIRETEAM // 2/3 // MEDEVAC 1\n"
                 "ORDER HOLD // [C] FOLLOW")
        )
    );
    TestEqual(
        TEXT("Nearby squad command uses meters"),
        UBHCombatStatusWidget::BuildSquadCommandWaypointLabel(
            8500.0f
        ),
        FString(TEXT("SQUAD HOLD POINT // 85 M // [C] FOLLOW"))
    );
    TestEqual(
        TEXT("Distant squad command uses kilometers"),
        UBHCombatStatusWidget::BuildSquadCommandWaypointLabel(
            140000.0f
        ),
        FString(TEXT("SQUAD HOLD POINT // 1.4 KM // [C] FOLLOW"))
    );
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBHFieldSquadReadinessHUDContractTest,
    "BrokenHorizon.Gameplay.UI.FieldSquadReadiness",
    EAutomationTestFlags::EditorContext |
        EAutomationTestFlags::EngineFilter
)

bool FBHFieldSquadReadinessHUDContractTest::RunTest(
    const FString& Parameters
)
{
    TestEqual(
        TEXT("Readiness pressure is visible beside the active order"),
        UBHCombatStatusWidget::BuildFieldSquadStatusLabel(
            3,
            3,
            false,
            false,
            0,
            0,
            0.78f,
            0.55f
        ),
        FString(
            TEXT(
                "FIRETEAM // 3/3 // READY\n"
                "COHESION 78% // LOWEST 55%\n"
                "ORDER FOLLOW // AIM + [C] MOVE/HOLD"
            )
        )
    );
    TestEqual(
        TEXT("Readiness values clamp and lowest cannot exceed average"),
        UBHCombatStatusWidget::BuildFieldSquadStatusLabel(
            2,
            3,
            true,
            false,
            1,
            0,
            1.5f,
            1.2f
        ),
        FString(
            TEXT(
                "FIRETEAM // 2/3 // SERVICE 1\n"
                "COHESION 100% // LOWEST 100%\n"
                "ORDER HOLD // [C] FOLLOW"
            )
        )
    );

    const UFunction* ReadinessFunction =
        UBHCombatStatusWidget::StaticClass()->FindFunctionByName(
            GET_FUNCTION_NAME_CHECKED(
                UBHCombatStatusWidget,
                SetFieldSquadReadiness
            )
        );
    TestNotNull(
        TEXT("Squad readiness remains available to Blueprint HUDs"),
        ReadinessFunction
    );
    if (ReadinessFunction)
    {
        TestTrue(
            TEXT("Squad readiness exposes a Blueprint-callable API"),
            ReadinessFunction->HasAnyFunctionFlags(
                FUNC_BlueprintCallable
            )
        );
    }

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBHFieldSquadContextOwnershipHUDTest,
    "BrokenHorizon.Gameplay.UI.FieldSquadContextOwnership",
    EAutomationTestFlags::EditorContext |
        EAutomationTestFlags::EngineFilter
)

bool FBHFieldSquadContextOwnershipHUDTest::RunTest(
    const FString& Parameters
)
{
    const FString MovingContext =
        UBHCombatStatusWidget::BuildFieldSquadContextStatusLine(
            TEXT("defend"),
            TEXT("WesternFOB"),
            false
        );
    TestEqual(
        TEXT("Pending Context ownership remains visible"),
        MovingContext,
        FString(TEXT("CONTEXT DEFEND // MOVING // WESTERNFOB"))
    );
    TestEqual(
        TEXT("Reached Context order has an explicit active state"),
        UBHCombatStatusWidget::BuildFieldSquadContextStatusLine(
            TEXT("sabotage"),
            TEXT("logistics cache"),
            true
        ),
        FString(
            TEXT("CONTEXT SABOTAGE // ACTIVE // LOGISTICS CACHE")
        )
    );
    TestTrue(
        TEXT("Empty Context input clears the persistent HUD line"),
        UBHCombatStatusWidget::BuildFieldSquadContextStatusLine(
            FString(),
            TEXT("WesternFOB"),
            false
        ).IsEmpty()
    );
    TestTrue(
        TEXT("Context status is composed into the fireteam panel"),
        UBHCombatStatusWidget::BuildFieldSquadStatusLabel(
            2,
            3,
            true,
            false,
            0,
            0,
            0.8f,
            0.7f,
            MovingContext
        ).Contains(MovingContext)
    );

    const FProperty* ActionProperty = FindFProperty<FProperty>(
        ABHCharacter::StaticClass(),
        TEXT("FieldSquadContextAction")
    );
    const FProperty* TargetProperty = FindFProperty<FProperty>(
        ABHCharacter::StaticClass(),
        TEXT("FieldSquadContextTargetLabel")
    );
    const FProperty* ReachedProperty = FindFProperty<FProperty>(
        ABHCharacter::StaticClass(),
        TEXT("bFieldSquadContextActionReachedTarget")
    );
    TestTrue(
        TEXT("Context action state replicates to its commander"),
        ActionProperty && ActionProperty->HasAnyPropertyFlags(CPF_Net)
    );
    TestTrue(
        TEXT("Context target label replicates to its commander"),
        TargetProperty && TargetProperty->HasAnyPropertyFlags(CPF_Net)
    );
    TestTrue(
        TEXT("Context arrival state replicates to its commander"),
        ReachedProperty && ReachedProperty->HasAnyPropertyFlags(CPF_Net)
    );

    const UFunction* ContextHUDContract =
        UBHCombatStatusWidget::StaticClass()->FindFunctionByName(
            GET_FUNCTION_NAME_CHECKED(
                UBHCombatStatusWidget,
                SetFieldSquadContextStatus
            )
        );
    TestNotNull(
        TEXT("Context status remains Blueprint-callable"),
        ContextHUDContract
    );

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBHFieldLogisticsTransferTest,
    "BrokenHorizon.PersistentWar.Logistics.FieldTransportTransfer",
    EAutomationTestFlags::EditorContext |
        EAutomationTestFlags::EngineFilter
)

bool FBHFieldLogisticsTransferTest::RunTest(
    const FString& Parameters
)
{
    UGameInstance* GameInstance =
        NewObject<UGameInstance>(GetTransientPackage());
    UBHWarSubsystem* War =
        NewObject<UBHWarSubsystem>(GameInstance);
    TestNotNull(TEXT("War subsystem is created"), War);

    if (!War)
    {
        return false;
    }

    War->ResetCampaign();
    const FName SourceSectorID = TEXT("WesternFOB");
    const FName DestinationSectorID = TEXT("DovrenVillage");
    const float SourceSupplyBefore =
        War->GetSectorState(SourceSectorID).Supply;
    const float DestinationSupplyBefore =
        War->GetSectorState(DestinationSectorID).Supply;
    TestEqual(
        TEXT(
            "Logistics guidance prioritizes the undersupplied "
            "friendly frontline"
        ),
        War->GetRecommendedFieldLogisticsDestination(
            SourceSectorID
        ),
        DestinationSectorID
    );
    TestTrue(
        TEXT("Friendly destination exposes delivery capacity"),
        War->GetFieldLogisticsDeliveryCapacity(
            DestinationSectorID
        ) > 0.0f
    );
    TestEqual(
        TEXT("Enemy territory cannot receive friendly logistics"),
        War->GetFieldLogisticsDeliveryCapacity(
            TEXT("EasternDepot")
        ),
        0.0f
    );
    const float LoadedSupply =
        War->WithdrawFieldLogisticsSupply(
            SourceSectorID,
            15.0f
        );

    TestEqual(
        TEXT("Transport loads its requested strategic cargo"),
        LoadedSupply,
        15.0f
    );
    TestEqual(
        TEXT("Loading removes supply from the source sector"),
        War->GetSectorState(SourceSectorID).Supply,
        SourceSupplyBefore - LoadedSupply
    );

    const float DeliveredSupply =
        War->DeliverFieldLogisticsSupply(
            SourceSectorID,
            DestinationSectorID,
            LoadedSupply
        );
    TestEqual(
        TEXT("Transport delivers all available cargo"),
        DeliveredSupply,
        LoadedSupply
    );
    TestEqual(
        TEXT("Delivery reinforces the destination sector"),
        War->GetSectorState(DestinationSectorID).Supply,
        DestinationSupplyBefore + DeliveredSupply
    );
    TestEqual(
        TEXT("Enemy sectors cannot issue friendly field cargo"),
        War->WithdrawFieldLogisticsSupply(
            TEXT("EasternDepot"),
            15.0f
        ),
        0.0f
    );
    War->DeliverFieldLogisticsSupply(
        SourceSectorID,
        DestinationSectorID,
        100.0f
    );
    TestEqual(
        TEXT("A full destination reports no remaining capacity"),
        War->GetFieldLogisticsDeliveryCapacity(
            DestinationSectorID
        ),
        0.0f
    );
    TestTrue(
        TEXT(
            "Logistics guidance clears when no other friendly "
            "sector can accept cargo"
        ),
        War->GetRecommendedFieldLogisticsDestination(
            SourceSectorID
        ).IsNone()
    );
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBHResupplyOperationTest,
    "BrokenHorizon.PersistentWar.Operations.ResupplyDelivery",
    EAutomationTestFlags::EditorContext |
        EAutomationTestFlags::EngineFilter
)

bool FBHResupplyOperationTest::RunTest(
    const FString& Parameters
)
{
    UGameInstance* GameInstance =
        NewObject<UGameInstance>(GetTransientPackage());
    UBHWarSubsystem* War =
        NewObject<UBHWarSubsystem>(GameInstance);
    TestNotNull(TEXT("War subsystem is created"), War);

    if (!War)
    {
        return false;
    }

    War->ResetCampaign();
    const FName SourceSectorID = TEXT("WesternFOB");
    const FName DestinationSectorID = TEXT("DovrenVillage");
    const float MissionLoad =
        War->GetPriorityOperationSupplyCost();
    const float SourceSupplyBefore =
        War->GetSectorState(SourceSectorID).Supply;

    TestTrue(
        TEXT("Undersupplied friendly sector offers resupply"),
        War->IsViableOperation(
            DestinationSectorID,
            EBHWarPriorityType::Resupply
        )
    );
    TestEqual(
        TEXT("Resupply chooses a different stocked staging sector"),
        War->GetOperationSupplySource(
            DestinationSectorID,
            EBHWarPriorityType::Resupply
        ),
        SourceSectorID
    );
    TestTrue(
        TEXT("Staging sector can fund a full cargo load"),
        War->CanFundOperation(
            DestinationSectorID,
            EBHWarPriorityType::Resupply
        )
    );
    TestTrue(
        TEXT("Resupply operation commits"),
        War->SetCommittedOperation(
            DestinationSectorID,
            EBHWarPriorityType::Resupply
        )
    );
    TestEqual(
        TEXT("Loaded mission cargo is routed to its assignment"),
        War->GetRecommendedFieldLogisticsDestination(
            SourceSectorID
        ),
        DestinationSectorID
    );
    TestTrue(
        TEXT("Deployment validates cargo funding"),
        War->ConsumeOperationSupply(
            DestinationSectorID,
            EBHWarPriorityType::Resupply
        )
    );
    TestEqual(
        TEXT("Deployment does not remove cargo before loading"),
        War->GetSectorState(SourceSectorID).Supply,
        SourceSupplyBefore
    );
    TestFalse(
        TEXT("Partial delivery does not complete the operation"),
        War->DoesFieldLogisticsDeliveryCompleteOperation(
            SourceSectorID,
            DestinationSectorID,
            MissionLoad - 1.0f
        )
    );
    TestFalse(
        TEXT("Cargo from the wrong source does not complete it"),
        War->DoesFieldLogisticsDeliveryCompleteOperation(
            TEXT("LydonCheckpoint"),
            DestinationSectorID,
            MissionLoad
        )
    );
    TestTrue(
        TEXT("Assigned full-load delivery completes it"),
        War->DoesFieldLogisticsDeliveryCompleteOperation(
            SourceSectorID,
            DestinationSectorID,
            MissionLoad
        )
    );
    TestTrue(
        TEXT("Successful resupply resolves strategically"),
        War->ApplyMissionResult(
            DestinationSectorID,
            EBHWarPriorityType::Resupply,
            true
        )
    );
    TestFalse(
        TEXT("Resolved resupply releases its operation lock"),
        War->HasCommittedOperation()
    );
    TestEqual(
        TEXT("Resupply does not change friendly ownership"),
        War->GetSectorState(DestinationSectorID).Owner,
        EBHWarFaction::Friendly
    );
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBHReconOperationTest,
    "BrokenHorizon.PersistentWar.Operations.FieldRecon",
    EAutomationTestFlags::EditorContext |
        EAutomationTestFlags::EngineFilter
)

bool FBHReconOperationTest::RunTest(
    const FString& Parameters
)
{
    UGameInstance* GameInstance =
        NewObject<UGameInstance>(GetTransientPackage());
    UBHWarSubsystem* War =
        NewObject<UBHWarSubsystem>(GameInstance);
    TestNotNull(TEXT("War subsystem is created"), War);
    if (!War)
    {
        return false;
    }

    War->ResetCampaign();
    FName ReconSectorID = NAME_None;
    for (const FBHWarSectorState& Sector : War->GetSectorStates())
    {
        if (War->IsViableOperation(
                Sector.SectorID,
                EBHWarPriorityType::Recon))
        {
            ReconSectorID = Sector.SectorID;
            break;
        }
    }
    TestFalse(
        TEXT("Campaign offers an adjacent unconfirmed recon sector"),
        ReconSectorID.IsNone()
    );
    if (ReconSectorID.IsNone())
    {
        return false;
    }

    const FBHWarSectorState Before =
        War->GetSectorState(ReconSectorID);
    const FName SupplySourceID = War->GetOperationSupplySource(
        ReconSectorID,
        EBHWarPriorityType::Recon
    );
    const float SupplyBefore =
        War->GetSectorState(SupplySourceID).Supply;
    TestTrue(
        TEXT("Recon targets territory outside friendly control"),
        Before.Owner != EBHWarFaction::Friendly
    );
    TestEqual(
        TEXT("Recon uses its bounded field-report supply cost"),
        War->GetOperationSupplyCost(
            ReconSectorID,
            EBHWarPriorityType::Recon
        ),
        4.0f
    );
    TestTrue(
        TEXT("Recon briefing has a dedicated operation identity"),
        War->GetOperationTitle(
            ReconSectorID,
            EBHWarPriorityType::Recon
        ).ToString().Contains(TEXT("WATCHTOWER"))
    );
    TestFalse(
        TEXT("Recon publishes a stable observation objective"),
        War->GetOperationObjectiveText(
            ReconSectorID,
            EBHWarPriorityType::Recon,
            BHObjectiveIds::ObserveSector
        ).IsEmpty()
    );
    TestTrue(
        TEXT("Recon operation commits authoritatively"),
        War->SetCommittedOperation(
            ReconSectorID,
            EBHWarPriorityType::Recon
        )
    );
    TestTrue(
        TEXT("Recon consumes its field-report kit"),
        War->ConsumeOperationSupply(
            ReconSectorID,
            EBHWarPriorityType::Recon
        )
    );
    TestEqual(
        TEXT("Recon deducts four supply from its staging route"),
        War->GetSectorState(SupplySourceID).Supply,
        SupplyBefore - 4.0f
    );

    const FBHWarStateSnapshot Snapshot =
        War->CaptureReplicatedSnapshot(45);
    TestEqual(
        TEXT("Replicated snapshot retains committed recon type"),
        Snapshot.CommittedOperationType,
        EBHWarPriorityType::Recon
    );
    UGameInstance* ReplicaGameInstance =
        NewObject<UGameInstance>(GetTransientPackage());
    UBHWarSubsystem* Replica =
        NewObject<UBHWarSubsystem>(ReplicaGameInstance);
    TestTrue(
        TEXT("Remote campaign accepts recon snapshot"),
        IsValid(Replica) && Replica->ApplyReplicatedSnapshot(Snapshot)
    );
    TestEqual(
        TEXT("Remote campaign exposes the same recon assignment"),
        Replica->GetCommittedOperationType(),
        EBHWarPriorityType::Recon
    );

    UBHSaveGame* Save = NewObject<UBHSaveGame>(GetTransientPackage());
    Save->bRuntimeWarOperation = true;
    Save->AssignedWarSectorID = ReconSectorID;
    Save->AssignedWarSupplySourceSectorID = SupplySourceID;
    Save->AssignedWarPriorityType = EBHWarPriorityType::Recon;
    Save->CurrentObjectiveID = BHObjectiveIds::ObserveSector;
    FBHObjectiveDefinition ReconObjective;
    ReconObjective.ObjectiveID = BHObjectiveIds::ObserveSector;
    Save->RuntimeObjectives = {ReconObjective};
    TArray<uint8> SaveBytes;
    TestTrue(
        TEXT("Active recon assignment serializes"),
        UGameplayStatics::SaveGameToMemory(Save, SaveBytes)
    );
    const UBHSaveGame* RestoredSave = Cast<UBHSaveGame>(
        UGameplayStatics::LoadGameFromMemory(SaveBytes)
    );
    TestEqual(
        TEXT("Recon type survives save round trip"),
        IsValid(RestoredSave)
            ? RestoredSave->AssignedWarPriorityType
            : EBHWarPriorityType::None,
        EBHWarPriorityType::Recon
    );
    TestEqual(
        TEXT("Observation objective survives save round trip"),
        IsValid(RestoredSave)
            ? RestoredSave->CurrentObjectiveID
            : NAME_None,
        BHObjectiveIds::ObserveSector
    );

    TestTrue(
        TEXT("Field reporting can confirm the recon target"),
        War->ReportSectorRecon(ReconSectorID, 100.0f)
    );
    TestFalse(
        TEXT("Partial intelligence cannot complete recon"),
        BHWarOperationRules::IsReconReportComplete(
            true,
            EBHWarPriorityType::Recon,
            ReconSectorID,
            ReconSectorID,
            99.0f
        )
    );
    TestFalse(
        TEXT("A report from the wrong sector cannot complete recon"),
        BHWarOperationRules::IsReconReportComplete(
            true,
            EBHWarPriorityType::Recon,
            ReconSectorID,
            TEXT("WrongSector"),
            100.0f
        )
    );
    TestTrue(
        TEXT("Confirmed intelligence in the assigned sector completes recon"),
        BHWarOperationRules::IsReconReportComplete(
            true,
            EBHWarPriorityType::Recon,
            ReconSectorID,
            ReconSectorID,
            War->GetSectorState(ReconSectorID).IntelConfidence
        )
    );
    TestTrue(
        TEXT("Confirmed recon resolves strategically"),
        War->ApplyMissionResult(
            ReconSectorID,
            EBHWarPriorityType::Recon,
            true
        )
    );
    const FBHWarSectorState After = War->GetSectorState(ReconSectorID);
    TestEqual(
        TEXT("Recon confirms intelligence"),
        After.IntelConfidence,
        100.0f
    );
    TestEqual(
        TEXT("Recon never captures the observed sector"),
        After.Owner,
        Before.Owner
    );
    TestFalse(
        TEXT("Resolved recon releases the operation lock"),
        War->HasCommittedOperation()
    );
    const FBHOperationAfterActionRecord ReconAAR =
        UBHWarSubsystem::BuildAfterActionRecord(
            TEXT("AAR_FieldRecon"),
            ReconSectorID,
            EBHWarPriorityType::Recon,
            true,
            0,
            0,
            0,
            -4.0f,
            0.0f,
            EBHOperationTacticalOption::ReconPlanning,
            6.0f
        );
    TestTrue(
        TEXT("Recon AAR rewards preservation without fake combat credit"),
        ReconAAR.ForcePreservationScore == 25 &&
        ReconAAR.EnemyOutcomeScore == 0 &&
        ReconAAR.TacticalOption == EBHOperationTacticalOption::None
    );

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBHEscortOperationTest,
    "BrokenHorizon.PersistentWar.Operations.EscortConvoy",
    EAutomationTestFlags::EditorContext |
        EAutomationTestFlags::EngineFilter
)

bool FBHEscortOperationTest::RunTest(
    const FString& Parameters
)
{
    UGameInstance* GameInstance =
        NewObject<UGameInstance>(GetTransientPackage());
    UBHWarSubsystem* War =
        NewObject<UBHWarSubsystem>(GameInstance);
    TestNotNull(TEXT("War subsystem is created"), War);

    if (!War)
    {
        return false;
    }

    const FName ConvoyID = TEXT("Escort_Test_Convoy");
    const FName SourceSectorID = TEXT("WesternFOB");
    const FName DestinationSectorID = TEXT("DovrenVillage");

    const auto RestoreEscortFixture = [War, ConvoyID, SourceSectorID,
                                        DestinationSectorID]()
    {
        War->ResetCampaign();

        FBHWarSupplyConvoyState Convoy;
        Convoy.ConvoyID = ConvoyID;
        Convoy.SourceSectorID = SourceSectorID;
        Convoy.DestinationSectorID = DestinationSectorID;
        Convoy.Owner = EBHWarFaction::Friendly;
        Convoy.CargoType = EBHWarConvoyCargoType::MilitarySupply;
        Convoy.SupplyPayload = 15.0f;
        Convoy.TurnsRemaining = 2;
        Convoy.DispatchTurn = 1;
        BHRouteOperations::Initialize(Convoy);
        Convoy.RouteOperationProfile.Variation =
            EBHRouteOperationVariation::TimeCritical;
        Convoy.RouteOperationProfile.CompletionDeadlineSeconds = 150.0f;
        Convoy.OperationDeadlineSecondsRemaining = 91.0f;

        return War->RestoreWarState(
            War->GetSectorStates(),
            {Convoy},
            War->GetRecentWarEvents(),
            War->GetTurnNumber(),
            War->GetSimulationAccumulator(),
            BHSave::CurrentSchemaVersion
        );
    };

    TestTrue(
        TEXT("Escort fixture restores a valid friendly convoy"),
        RestoreEscortFixture()
    );
    TestEqual(
        TEXT("Escort selection targets the exact eligible convoy"),
        War->GetEscortOperationTargetID(DestinationSectorID),
        ConvoyID
    );
    TestTrue(
        TEXT("Friendly convoy destination offers escort"),
        War->IsViableOperation(
            DestinationSectorID,
            EBHWarPriorityType::EscortRescue
        )
    );
    TestEqual(
        TEXT("Escort deploys from the convoy source"),
        War->GetOperationSupplySource(
            DestinationSectorID,
            EBHWarPriorityType::EscortRescue
        ),
        SourceSectorID
    );
    TestTrue(
        TEXT("Escort operation commits"),
        War->SetCommittedOperation(
            DestinationSectorID,
            EBHWarPriorityType::EscortRescue
        )
    );
    TestEqual(
        TEXT("Committed escort pins its convoy identity"),
        War->GetCommittedOperationTargetID(),
        ConvoyID
    );
    TestTrue(
        TEXT("Pinned convoy matches the committed escort"),
        War->DoesConvoyMatchCommittedEscort(ConvoyID)
    );
    TestFalse(
        TEXT("A different convoy cannot resolve the escort"),
        War->DoesConvoyMatchCommittedEscort(TEXT("Wrong_Convoy"))
    );
    TestTrue(
        TEXT("Escort route choice commits authoritatively"),
        War->SetConvoySelectedWorldRoute(
            ConvoyID,
            TEXT("AlternateValleyRoute")
        )
    );

    const FBHWarStateSnapshot Snapshot =
        War->CaptureReplicatedSnapshot(23);
    UGameInstance* ReplicaGameInstance =
        NewObject<UGameInstance>(GetTransientPackage());
    UBHWarSubsystem* ReplicaWar =
        NewObject<UBHWarSubsystem>(ReplicaGameInstance);
    TestTrue(
        TEXT("Escort snapshot applies to a remote campaign"),
        IsValid(ReplicaWar) &&
            ReplicaWar->ApplyReplicatedSnapshot(Snapshot)
    );
    TestEqual(
        TEXT("Remote campaign receives the exact escorted convoy"),
        IsValid(ReplicaWar)
            ? ReplicaWar->GetCommittedOperationTargetID()
            : NAME_None,
        ConvoyID
    );
    TestEqual(
        TEXT("Remote campaign receives the selected world route"),
        IsValid(ReplicaWar)
            ? ReplicaWar->GetSupplyConvoyState(ConvoyID)
                .SelectedWorldRouteID
            : NAME_None,
        FName(TEXT("AlternateValleyRoute"))
    );
    TestEqual(
        TEXT("Remote campaign receives the urgent deadline"),
        IsValid(ReplicaWar)
            ? ReplicaWar->GetSupplyConvoyState(ConvoyID)
                .OperationDeadlineSecondsRemaining
            : 0.0f,
        91.0f
    );

    UBHSaveGame* EscortSave =
        NewObject<UBHSaveGame>(GetTransientPackage());
    EscortSave->AssignedWarSectorID = DestinationSectorID;
    EscortSave->AssignedWarPriorityType =
        EBHWarPriorityType::EscortRescue;
    EscortSave->WarSectorStates = War->GetSectorStates();
    EscortSave->WarSupplyConvoys = War->GetSupplyConvoys();
    EscortSave->WarEventHistory = War->GetRecentWarEvents();
    EscortSave->WarTurnNumber = War->GetTurnNumber();
    EscortSave->WarSimulationAccumulator =
        War->GetSimulationAccumulator();
    EscortSave->WarCommittedOperationID =
        War->GetCommittedOperationID();
    EscortSave->WarCommittedOperationTargetID =
        War->GetCommittedOperationTargetID();

    TArray<uint8> EscortSaveBytes;
    TestTrue(
        TEXT("Committed escort serializes"),
        UGameplayStatics::SaveGameToMemory(
            EscortSave,
            EscortSaveBytes
        )
    );
    const UBHSaveGame* RestoredEscortSave = Cast<UBHSaveGame>(
        UGameplayStatics::LoadGameFromMemory(EscortSaveBytes)
    );
    TestNotNull(
        TEXT("Committed escort deserializes"),
        RestoredEscortSave
    );
    TestEqual(
        TEXT("Serialized escort retains its exact convoy"),
        IsValid(RestoredEscortSave)
            ? RestoredEscortSave->WarCommittedOperationTargetID
            : NAME_None,
        ConvoyID
    );
    TestEqual(
        TEXT("Serialized escort retains its selected route"),
        IsValid(RestoredEscortSave)
            ? RestoredEscortSave->WarSupplyConvoys[0]
                .SelectedWorldRouteID
            : NAME_None,
        FName(TEXT("AlternateValleyRoute"))
    );

    UGameInstance* RestoredGameInstance =
        NewObject<UGameInstance>(GetTransientPackage());
    UBHWarSubsystem* RestoredWar =
        NewObject<UBHWarSubsystem>(RestoredGameInstance);
    const bool bRestoredEscortState =
        IsValid(RestoredEscortSave) && IsValid(RestoredWar) &&
        RestoredWar->RestoreWarState(
            RestoredEscortSave->WarSectorStates,
            RestoredEscortSave->WarSupplyConvoys,
            RestoredEscortSave->WarEventHistory,
            RestoredEscortSave->WarTurnNumber,
            RestoredEscortSave->WarSimulationAccumulator,
            RestoredEscortSave->SchemaVersion
        ) &&
        RestoredWar->RestoreCommittedOperation(
            RestoredEscortSave->AssignedWarSectorID,
            RestoredEscortSave->AssignedWarPriorityType,
            RestoredEscortSave->WarCommittedOperationID,
            RestoredEscortSave->WarCommittedOperationTargetID
        );
    TestTrue(
        TEXT("Committed escort restores into the campaign"),
        bRestoredEscortState
    );
    TestEqual(
        TEXT("Campaign restore retains the exact escorted convoy"),
        IsValid(RestoredWar)
            ? RestoredWar->GetCommittedOperationTargetID()
            : NAME_None,
        ConvoyID
    );
    TestEqual(
        TEXT("Campaign restore retains the urgent deadline"),
        IsValid(RestoredWar)
            ? RestoredWar->GetSupplyConvoyState(ConvoyID)
                .OperationDeadlineSecondsRemaining
            : 0.0f,
        91.0f
    );

    TestTrue(
        TEXT("Successful escort resolves strategically"),
        War->ApplyMissionResult(
            DestinationSectorID,
            EBHWarPriorityType::EscortRescue,
            true
        )
    );
    TestFalse(
        TEXT("Successful escort releases its operation lock"),
        War->HasCommittedOperation()
    );
    TestEqual(
        TEXT("Successful escort preserves friendly ownership"),
        War->GetSectorState(DestinationSectorID).Owner,
        EBHWarFaction::Friendly
    );

    TestTrue(
        TEXT("Escort fixture can be restored for failure coverage"),
        RestoreEscortFixture()
    );
    TestTrue(
        TEXT("Failure-path escort commits"),
        War->SetCommittedOperation(
            DestinationSectorID,
            EBHWarPriorityType::EscortRescue
        )
    );
    TestTrue(
        TEXT("Destroyed convoy is removed from strategic transit"),
        War->InterdictSupplyConvoy(ConvoyID)
    );
    TestTrue(
        TEXT("Removed convoy still identifies the active escort failure"),
        War->DoesConvoyMatchCommittedEscort(ConvoyID)
    );
    TestTrue(
        TEXT("Failed escort resolves strategically"),
        War->ApplyMissionResult(
            DestinationSectorID,
            EBHWarPriorityType::EscortRescue,
            false
        )
    );
    TestFalse(
        TEXT("Failed escort releases its operation lock"),
        War->HasCommittedOperation()
    );
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBHRescueOperationTest,
    "BrokenHorizon.PersistentWar.Operations.CasualtyRescue",
    EAutomationTestFlags::EditorContext |
        EAutomationTestFlags::EngineFilter
)

bool FBHRescueOperationTest::RunTest(
    const FString& Parameters
)
{
    UGameInstance* GameInstance =
        NewObject<UGameInstance>(GetTransientPackage());
    UBHWarSubsystem* War =
        NewObject<UBHWarSubsystem>(GameInstance);
    TestNotNull(TEXT("War subsystem is created"), War);

    if (!War)
    {
        return false;
    }

    War->ResetCampaign();
    const FName DestinationSectorID = TEXT("DovrenVillage");
    const FName CasualtyID = TEXT("FieldOperative_TestRescue");

    TestTrue(
        TEXT("Friendly treatment sector supports rescue"),
        War->IsViableOperation(
            DestinationSectorID,
            EBHWarPriorityType::Rescue
        )
    );
    TestFalse(
        TEXT("Rescue rejects an unidentified casualty"),
        War->SetCommittedRescueOperation(
            DestinationSectorID,
            NAME_None
        )
    );
    TestTrue(
        TEXT("Rescue commits an exact casualty target"),
        War->SetCommittedRescueOperation(
            DestinationSectorID,
            CasualtyID
        )
    );
    TestEqual(
        TEXT("Committed rescue retains casualty identity"),
        War->GetCommittedOperationTargetID(),
        CasualtyID
    );

    const FBHWarStateSnapshot Snapshot =
        War->CaptureReplicatedSnapshot(31);
    UGameInstance* ReplicaGameInstance =
        NewObject<UGameInstance>(GetTransientPackage());
    UBHWarSubsystem* ReplicaWar =
        NewObject<UBHWarSubsystem>(ReplicaGameInstance);
    TestTrue(
        TEXT("Remote campaign accepts rescue snapshot"),
        IsValid(ReplicaWar) &&
            ReplicaWar->ApplyReplicatedSnapshot(Snapshot)
    );
    TestEqual(
        TEXT("Remote campaign receives casualty identity"),
        IsValid(ReplicaWar)
            ? ReplicaWar->GetCommittedOperationTargetID()
            : NAME_None,
        CasualtyID
    );

    UGameInstance* RestoredGameInstance =
        NewObject<UGameInstance>(GetTransientPackage());
    UBHWarSubsystem* RestoredWar =
        NewObject<UBHWarSubsystem>(RestoredGameInstance);
    TestTrue(
        TEXT("Rescue strategic state restores"),
        IsValid(RestoredWar) &&
            RestoredWar->RestoreWarState(
                War->GetSectorStates(),
                War->GetSupplyConvoys(),
                War->GetRecentWarEvents(),
                War->GetTurnNumber(),
                War->GetSimulationAccumulator(),
                BHSave::CurrentSchemaVersion
            ) &&
            RestoredWar->RestoreCommittedOperation(
                DestinationSectorID,
                EBHWarPriorityType::Rescue,
                War->GetCommittedOperationID(),
                CasualtyID
            )
    );
    TestEqual(
        TEXT("Restored rescue retains casualty identity"),
        IsValid(RestoredWar)
            ? RestoredWar->GetCommittedOperationTargetID()
            : NAME_None,
        CasualtyID
    );

    TestTrue(
        TEXT("Successful rescue resolves strategically"),
        War->ApplyMissionResult(
            DestinationSectorID,
            EBHWarPriorityType::Rescue,
            true
        )
    );
    TestFalse(
        TEXT("Resolved rescue releases operation lock"),
        War->HasCommittedOperation()
    );
    TestEqual(
        TEXT("Rescue preserves friendly sector ownership"),
        War->GetSectorState(DestinationSectorID).Owner,
        EBHWarFaction::Friendly
    );
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBHConvoySalvageTest,
    "BrokenHorizon.PersistentWar.Logistics.ConvoySalvage",
    EAutomationTestFlags::EditorContext |
        EAutomationTestFlags::EngineFilter
)

bool FBHConvoySalvageTest::RunTest(
    const FString& Parameters
)
{
    TestEqual(
        TEXT("Enemy convoy wreck preserves most recoverable cargo"),
        ABHSupplyConvoyTarget::CalculateRecoverableSupply(
            15.0f,
            0.60f
        ),
        9.0f
    );
    TestEqual(
        TEXT("Negative convoy cargo cannot create salvage"),
        ABHSupplyConvoyTarget::CalculateRecoverableSupply(
            -10.0f,
            0.60f
        ),
        0.0f
    );
    TestEqual(
        TEXT("Recovery fraction is clamped to available cargo"),
        ABHSupplyConvoyTarget::CalculateRecoverableSupply(
            15.0f,
            2.0f
        ),
        15.0f
    );
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBHRouteOperationVariationsTest,
    "BrokenHorizon.PersistentWar.Operations.RouteVariations",
    EAutomationTestFlags::EditorContext |
        EAutomationTestFlags::EngineFilter
)

bool FBHRouteOperationVariationsTest::RunTest(
    const FString& Parameters
)
{
    TSet<EBHRouteOperationVariation> Variations;

    for (int32 Index = 0; Index < 256; ++Index)
    {
        FBHWarSupplyConvoyState Convoy;
        Convoy.ConvoyID = FName(*FString::Printf(
            TEXT("RouteVariation_%d"),
            Index
        ));
        Convoy.SourceSectorID = TEXT("WesternFOB");
        Convoy.DestinationSectorID = TEXT("Crossroads");
        const FBHRouteOperationProfile Profile =
            ABHSupplyConvoyTarget::BuildRouteOperationProfile(Convoy);
        const FBHRouteOperationProfile RepeatedProfile =
            ABHSupplyConvoyTarget::BuildRouteOperationProfile(Convoy);

        TestEqual(
            TEXT("Route profile is stable for a convoy contract"),
            Profile.Variation,
            RepeatedProfile.Variation
        );
        Variations.Add(Profile.Variation);

        if (Profile.Variation == EBHRouteOperationVariation::Ambush)
        {
            TestTrue(
                TEXT("Ambush variation increases hostile pressure"),
                Profile.AdditionalAmbushers > 0
            );
        }
        else if (Profile.Variation ==
            EBHRouteOperationVariation::DamagedVehicle)
        {
            TestTrue(
                TEXT("Damaged variation begins below full integrity"),
                Profile.InitialIntegrity > 0.0f &&
                    Profile.InitialIntegrity < 1.0f
            );
        }
        else if (Profile.Variation ==
            EBHRouteOperationVariation::TimeCritical)
        {
            TestTrue(
                TEXT("Urgent variation has a completion deadline"),
                Profile.CompletionDeadlineSeconds > 0.0f
            );
        }
    }

    TestTrue(
        TEXT("Deterministic contracts cover every route variation"),
        Variations.Contains(EBHRouteOperationVariation::Standard) &&
            Variations.Contains(EBHRouteOperationVariation::Ambush) &&
            Variations.Contains(
                EBHRouteOperationVariation::DamagedVehicle) &&
            Variations.Contains(
                EBHRouteOperationVariation::TimeCritical)
    );
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBHCampaignDifficultyTest,
    "BrokenHorizon.PersistentWar.Progression.CampaignDifficulty",
    EAutomationTestFlags::EditorContext |
        EAutomationTestFlags::EngineFilter
)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBHCustomDifficultyAxisControlTest,
    "BrokenHorizon.PersistentWar.Progression.CustomDifficultyAxes",
    EAutomationTestFlags::EditorContext |
        EAutomationTestFlags::EngineFilter
)

bool FBHCustomDifficultyAxisControlTest::RunTest(
    const FString& Parameters
)
{
    const FBHCampaignDifficultyProfile Baseline =
        BHDifficulty::BuildPreset(
            EBHCampaignDifficultyPreset::Operator
        );
    for (int32 AxisIndex = 0; AxisIndex < 6; ++AxisIndex)
    {
        const FBHCampaignDifficultyProfile Adjusted =
            UBHWarMapWidget::AdjustCustomDifficultyAxis(
                Baseline,
                AxisIndex,
                0.25f
            );
        TestEqual(
            *FString::Printf(
                TEXT("Axis %d enters the custom preset"),
                AxisIndex
            ),
            Adjusted.Preset,
            EBHCampaignDifficultyPreset::Custom
        );
        TestTrue(
            *FString::Printf(
                TEXT("Axis %d exposes a readable label"),
                AxisIndex
            ),
            !UBHWarMapWidget::GetCustomDifficultyAxisLabel(
                AxisIndex
            ).IsEmpty()
        );
        TestTrue(
            *FString::Printf(
                TEXT("Axis %d adjusts independently"),
                AxisIndex
            ),
            FMath::IsNearlyEqual(
                UBHWarMapWidget::GetCustomDifficultyAxisValue(
                    Adjusted,
                    AxisIndex
                ),
                1.25f
            )
        );
    }

    const FBHCampaignDifficultyProfile MinimumDamage =
        UBHWarMapWidget::AdjustCustomDifficultyAxis(
            Baseline,
            0,
            -10.0f
        );
    const FBHCampaignDifficultyProfile MinimumCheckpoint =
        UBHWarMapWidget::AdjustCustomDifficultyAxis(
            Baseline,
            5,
            -10.0f
        );
    TestEqual(
        TEXT("Damage respects its safe minimum"),
        MinimumDamage.IncomingDamageMultiplier,
        0.5f
    );
    TestEqual(
        TEXT("Checkpoint cadence respects its distinct minimum"),
        MinimumCheckpoint.CheckpointIntervalMultiplier,
        0.25f
    );
    return true;
}

bool FBHCampaignDifficultyTest::RunTest(
    const FString& Parameters
)
{
    const FBHCampaignDifficultyProfile Recruit =
        BHDifficulty::BuildPreset(
            EBHCampaignDifficultyPreset::Recruit
        );
    const FBHCampaignDifficultyProfile Operator =
        BHDifficulty::BuildPreset(
            EBHCampaignDifficultyPreset::Operator
        );
    const FBHCampaignDifficultyProfile Veteran =
        BHDifficulty::BuildPreset(
            EBHCampaignDifficultyPreset::Veteran
        );
    TestEqual(TEXT("Recruit incoming damage follows GDD"),
        Recruit.IncomingDamageMultiplier, 0.75f);
    TestEqual(TEXT("Operator remains compatibility baseline"),
        Operator.IncomingDamageMultiplier, 1.0f);
    TestEqual(TEXT("Veteran incoming damage follows GDD"),
        Veteran.IncomingDamageMultiplier, 1.15f);
    TestTrue(TEXT("Recruit eases every pressure axis"),
        Recruit.EnemyPerceptionMultiplier < 1.0f &&
        Recruit.EnemyCoordinationMultiplier < 1.0f &&
        Recruit.MedicalPressureMultiplier < 1.0f &&
        Recruit.StrategicPressureMultiplier < 1.0f &&
        Recruit.CheckpointIntervalMultiplier < 1.0f);
    TestTrue(TEXT("Veteran raises tactical and campaign pressure"),
        Veteran.EnemyPerceptionMultiplier > 1.0f &&
        Veteran.EnemyCoordinationMultiplier > 1.0f &&
        Veteran.MedicalPressureMultiplier > 1.0f &&
        Veteran.StrategicPressureMultiplier > 1.0f);

    UGameInstance* GameInstance =
        NewObject<UGameInstance>(GetTransientPackage());
    UBHWarSubsystem* War =
        NewObject<UBHWarSubsystem>(GameInstance);
    TestTrue(TEXT("Authority accepts Veteran preset"),
        IsValid(War) && War->SetCampaignDifficultyPreset(
            EBHCampaignDifficultyPreset::Veteran));
    const FBHWarStateSnapshot Snapshot =
        War->CaptureReplicatedSnapshot(42);
    TestEqual(TEXT("Snapshot carries campaign difficulty"),
        Snapshot.CampaignDifficulty.Preset,
        EBHCampaignDifficultyPreset::Veteran);

    UGameInstance* ReplicaGameInstance =
        NewObject<UGameInstance>(GetTransientPackage());
    UBHWarSubsystem* Replica =
        NewObject<UBHWarSubsystem>(ReplicaGameInstance);
    TestTrue(TEXT("Remote campaign applies difficulty snapshot"),
        IsValid(Replica) && Replica->ApplyReplicatedSnapshot(Snapshot));
    TestEqual(TEXT("Remote campaign retains Veteran pressure"),
        Replica->GetCampaignDifficulty().EnemyCoordinationMultiplier,
        Veteran.EnemyCoordinationMultiplier);

    FBHCampaignDifficultyProfile UnsafeCustom;
    UnsafeCustom.Preset = EBHCampaignDifficultyPreset::Custom;
    UnsafeCustom.IncomingDamageMultiplier = 9.0f;
    UnsafeCustom.CheckpointIntervalMultiplier = 0.01f;
    TestTrue(TEXT("Authority accepts sanitized custom axes"),
        War->SetCustomCampaignDifficulty(UnsafeCustom));
    TestEqual(TEXT("Custom damage axis is bounded"),
        War->GetCampaignDifficulty().IncomingDamageMultiplier, 2.0f);
    TestEqual(TEXT("Custom checkpoint axis is bounded"),
        War->GetCampaignDifficulty().CheckpointIntervalMultiplier, 0.25f);

    UBHSaveGame* Save =
        NewObject<UBHSaveGame>(GetTransientPackage());
    Save->CampaignDifficulty = War->GetCampaignDifficulty();
    TArray<uint8> Bytes;
    TestTrue(TEXT("Difficulty serializes"),
        UGameplayStatics::SaveGameToMemory(Save, Bytes));
    const UBHSaveGame* Restored = Cast<UBHSaveGame>(
        UGameplayStatics::LoadGameFromMemory(Bytes));
    TestEqual(TEXT("Custom difficulty survives save round trip"),
        IsValid(Restored)
            ? Restored->CampaignDifficulty.Preset
            : EBHCampaignDifficultyPreset::Operator,
        EBHCampaignDifficultyPreset::Custom);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBHAfterActionProgressionTest,
    "BrokenHorizon.PersistentWar.Progression.AfterAction",
    EAutomationTestFlags::EditorContext |
        EAutomationTestFlags::EngineFilter
)

bool FBHAfterActionProgressionTest::RunTest(
    const FString& Parameters
)
{
    const FBHOperationAfterActionRecord StrongRecord =
        UBHWarSubsystem::BuildAfterActionRecord(
            TEXT("AAR_1"), TEXT("DovrenVillage"),
            EBHWarPriorityType::Defend, true,
            0, 5, 5, 12.0f, 5.0f);
    const FBHOperationAfterActionRecord FailedRecord =
        UBHWarSubsystem::BuildAfterActionRecord(
            TEXT("AAR_Failed"), TEXT("DovrenVillage"),
            EBHWarPriorityType::Defend, false,
            3, 0, 0, -20.0f, 0.0f);
    TestTrue(TEXT("Successful force preservation earns strong evaluation"),
        StrongRecord.TotalScore >= 85 &&
        StrongRecord.Grade == EBHAfterActionGrade::Exceptional);
    TestTrue(TEXT("Failure and casualties reduce evaluation"),
        FailedRecord.TotalScore < StrongRecord.TotalScore &&
        FailedRecord.ForcePreservationScore <
            StrongRecord.ForcePreservationScore);
    const FBHOperationAfterActionRecord ReconRecord =
        UBHWarSubsystem::BuildAfterActionRecord(
            TEXT("AAR_Recon"), TEXT("KoronaCrossroads"),
            EBHWarPriorityType::Attack, true,
            0, 4, 2, 0.0f, 0.0f,
            EBHOperationTacticalOption::ReconPlanning, 6.0f);
    const FBHOperationAfterActionRecord ReinforcementRecord =
        UBHWarSubsystem::BuildAfterActionRecord(
            TEXT("AAR_Reinforcement"), TEXT("KoronaCrossroads"),
            EBHWarPriorityType::Attack, true,
            2, 4, 2, 0.0f, 0.0f,
            EBHOperationTacticalOption::ReinforcementPriority, 12.0f);
    const FBHOperationAfterActionRecord MedicalRecord =
        UBHWarSubsystem::BuildAfterActionRecord(
            TEXT("AAR_Medical"), TEXT("KoronaCrossroads"),
            EBHWarPriorityType::Attack, true,
            0, 4, 2, 0.0f, 0.0f,
            EBHOperationTacticalOption::MedicalPreparation, 8.0f);
    TestTrue(TEXT("Recon plan records committed logistics and execution value"),
        ReconRecord.TacticalOption ==
            EBHOperationTacticalOption::ReconPlanning &&
        ReconRecord.TacticalSupplyCost == 6.0f &&
        ReconRecord.TacticalExecutionScore == 5);
    TestEqual(TEXT("Reinforcement losses reduce tactical execution value"),
        ReinforcementRecord.TacticalExecutionScore, 1);
    TestEqual(TEXT("Medical preparation rewards casualty prevention"),
        MedicalRecord.TacticalExecutionScore, 5);
    TestTrue(TEXT("Tactical logistics reduce resource-efficiency score"),
        ReconRecord.ResourceEfficiencyScore <
            StrongRecord.ResourceEfficiencyScore);
    const FBHOperationAfterActionRecord LogisticsRecord =
        UBHWarSubsystem::BuildAfterActionRecord(
            TEXT("AAR_Logistics"), TEXT("DovrenVillage"),
            EBHWarPriorityType::Resupply, true,
            0, 0, 0, 10.0f, 0.0f,
            EBHOperationTacticalOption::ReconPlanning, 6.0f);
    TestTrue(TEXT("Noncombat operation rejects inapplicable tactical telemetry"),
        LogisticsRecord.TacticalOption ==
            EBHOperationTacticalOption::None &&
        LogisticsRecord.TacticalSupplyCost == 0.0f &&
        LogisticsRecord.TacticalExecutionScore == 0);

    UGameInstance* GameInstance =
        NewObject<UGameInstance>(GetTransientPackage());
    UBHWarSubsystem* War =
        NewObject<UBHWarSubsystem>(GameInstance);
    TestNotNull(TEXT("War subsystem is created"), War);
    if (!War)
    {
        return false;
    }
    War->ResetCampaign();

    for (int32 Index = 0; Index < 5; ++Index)
    {
        FBHOperationAfterActionRecord Record = StrongRecord;
        Record.OperationID = FName(*FString::Printf(
            TEXT("AAR_%d"), Index));
        TestTrue(TEXT("After-action record commits"),
            War->RecordOperationAfterAction(Record));
    }
    FBHCampaignProgressionState Progression =
        War->GetCampaignProgression();
    TestEqual(TEXT("Operations accumulate"),
        Progression.CompletedOperations, 5);
    TestEqual(TEXT("Successful operations accumulate"),
        Progression.SuccessfulOperations, 5);
    TestTrue(TEXT("Merit unlocks intelligence without damage inflation"),
        War->HasCampaignCapability(
            EBHCampaignCapability::IntelligenceNetwork));
    TestTrue(TEXT("Merit unlocks casualty recovery"),
        War->HasCampaignCapability(
            EBHCampaignCapability::CasualtyRecoveryNetwork));
    TestTrue(TEXT("Merit unlocks transport support"),
        War->HasCampaignCapability(
            EBHCampaignCapability::TransportSupportNetwork));
    TestTrue(TEXT("Intelligence network preserves uncertainty"),
        War->GetSectorStates().ContainsByPredicate(
            [](const FBHWarSectorState& Sector)
            {
                return Sector.IntelConfidence >= 35.0f &&
                    Sector.IntelConfidence < 100.0f;
            }));

    const FName TacticalTarget(TEXT("KoronaCrossroads"));
    const EBHWarPriorityType TacticalOperation =
        EBHWarPriorityType::Attack;
    const FName TacticalSource = War->GetOperationSupplySource(
        TacticalTarget,
        TacticalOperation
    );
    TestTrue(TEXT("Recon planning unlocks from intelligence progression"),
        War->IsTacticalOptionUnlocked(
            EBHOperationTacticalOption::ReconPlanning));
    TestTrue(TEXT("Reinforcement priority unlocks from transport progression"),
        War->IsTacticalOptionUnlocked(
            EBHOperationTacticalOption::ReinforcementPriority));
    TestTrue(TEXT("Medical preparation unlocks from casualty recovery"),
        War->IsTacticalOptionUnlocked(
            EBHOperationTacticalOption::MedicalPreparation));
    TestTrue(TEXT("Commander selects recon planning"),
        War->SetActiveTacticalOption(
            EBHOperationTacticalOption::ReconPlanning));
    TestEqual(TEXT("Recon planning adds a six-supply preparation cost"),
        War->GetOperationSupplyCost(
            TacticalTarget,
            TacticalOperation),
        16.0f);
    const FBHWarOperationForcePackage ReconPackage =
        BHWarOperationRules::BuildForcePackage(
            War,
            TacticalTarget,
            TacticalOperation,
            TacticalSource
        );
    TestTrue(TEXT("Recon planning confirms operation intelligence"),
        ReconPackage.bReconPlanningApplied &&
        ReconPackage.IntelConfidence >= 80.0f);
    TestTrue(TEXT("Commander selects reinforcement priority"),
        War->SetActiveTacticalOption(
            EBHOperationTacticalOption::ReinforcementPriority));
    TestEqual(TEXT("Reinforcement priority adds a twelve-supply commitment"),
        War->GetOperationSupplyCost(
            TacticalTarget,
            TacticalOperation),
        22.0f);
    const FBHWarOperationForcePackage ReinforcementPackage =
        BHWarOperationRules::BuildForcePackage(
            War,
            TacticalTarget,
            TacticalOperation,
            TacticalSource
        );
    TestTrue(TEXT("Reinforcement priority enters the force package"),
        ReinforcementPackage.bReinforcementPriorityApplied);
    TestTrue(TEXT("Commander selects medical preparation"),
        War->SetActiveTacticalOption(
            EBHOperationTacticalOption::MedicalPreparation));
    TestEqual(TEXT("Medical preparation adds an eight-supply commitment"),
        War->GetOperationSupplyCost(
            TacticalTarget,
            TacticalOperation),
        18.0f);
    const FBHWarOperationForcePackage MedicalPackage =
        BHWarOperationRules::BuildForcePackage(
            War,
            TacticalTarget,
            TacticalOperation,
            TacticalSource
        );
    TestTrue(TEXT("Medical preparation enters the force package"),
        MedicalPackage.bMedicalPreparationApplied);
    int32 GrantedMedkits = 0;
    int32 GrantedDressings = 0;
    BHWarOperationRules::GetTacticalMedicalSupplyGrant(
        EBHOperationTacticalOption::MedicalPreparation,
        GrantedMedkits,
        GrantedDressings
    );
    TestEqual(TEXT("Medical preparation deploys one medkit"),
        GrantedMedkits, 1);
    TestEqual(TEXT("Medical preparation deploys two dressings"),
        GrantedDressings, 2);
    TestEqual(TEXT("Noncombat operations ignore tactical plan cost"),
        War->GetOperationSupplyCost(
            TacticalTarget,
            EBHWarPriorityType::Resupply),
        10.0f);
    const FBHWarOperationForcePackage NoncombatMedicalPackage =
        BHWarOperationRules::BuildForcePackage(
            War,
            TacticalTarget,
            EBHWarPriorityType::Resupply,
            TacticalSource
        );
    TestFalse(TEXT("Noncombat packages reject medical preparation"),
        NoncombatMedicalPackage.bMedicalPreparationApplied);

    bool bReducedAnyStagingSupply = false;
    for (const FBHWarSectorState& Sector : War->GetSectorStates())
    {
        if (Sector.Owner == EBHWarFaction::Friendly &&
            Sector.Supply > 15.0f)
        {
            bReducedAnyStagingSupply |= War->ConsumeSectorSupply(
                Sector.SectorID,
                Sector.Supply - 15.0f
            );
        }
    }
    TestTrue(TEXT("Test theater staging supply can be constrained"),
        bReducedAnyStagingSupply);
    TestTrue(TEXT("Costly reinforcement plan can be reselected"),
        War->SetActiveTacticalOption(
            EBHOperationTacticalOption::ReinforcementPriority));
    TestFalse(TEXT("Costly reinforcement plan is blocked by low supply"),
        War->CanFundOperation(
            TacticalTarget,
            TacticalOperation));
    TestTrue(TEXT("Commander can fall back to standard planning"),
        War->SetActiveTacticalOption(
            EBHOperationTacticalOption::None));
    TestEqual(TEXT("Standard planning retains the base operation cost"),
        War->GetOperationSupplyCost(
            TacticalTarget,
            TacticalOperation),
        10.0f);
    TestTrue(TEXT("Standard plan remains fundable at fifteen supply"),
        War->CanFundOperation(
            TacticalTarget,
            TacticalOperation));
    TestTrue(TEXT("Medical selection is restored for persistence"),
        War->SetActiveTacticalOption(
            EBHOperationTacticalOption::MedicalPreparation));
    FBHOperationAfterActionRecord PersistedTacticalRecord =
        MedicalRecord;
    PersistedTacticalRecord.OperationID = TEXT("AAR_PersistedTactical");
    TestTrue(TEXT("Tactical after-action record commits"),
        War->RecordOperationAfterAction(PersistedTacticalRecord));
    Progression = War->GetCampaignProgression();

    const FBHWarStateSnapshot Snapshot =
        War->CaptureReplicatedSnapshot(BHSave::CurrentSchemaVersion);
    UGameInstance* ReplicaGameInstance =
        NewObject<UGameInstance>(GetTransientPackage());
    UBHWarSubsystem* Replica =
        NewObject<UBHWarSubsystem>(ReplicaGameInstance);
    TestTrue(TEXT("Progression snapshot applies remotely"),
        IsValid(Replica) && Replica->ApplyReplicatedSnapshot(Snapshot));
    TestEqual(TEXT("Remote campaign receives merit"),
        Replica->GetCampaignProgression().CampaignMerit,
        Progression.CampaignMerit);
    TestEqual(TEXT("Remote campaign receives tactical selection"),
        Replica->GetActiveTacticalOption(),
        EBHOperationTacticalOption::MedicalPreparation);
    TestEqual(TEXT("Remote campaign receives tactical debrief cost"),
        Replica->GetCampaignProgression()
            .LastAfterAction.TacticalSupplyCost,
        8.0f);

    UBHSaveGame* Save =
        NewObject<UBHSaveGame>(GetTransientPackage());
    Save->CampaignProgression = Progression;
    TArray<uint8> Bytes;
    TestTrue(TEXT("Progression serializes"),
        UGameplayStatics::SaveGameToMemory(Save, Bytes));
    const UBHSaveGame* Restored = Cast<UBHSaveGame>(
        UGameplayStatics::LoadGameFromMemory(Bytes));
    TestEqual(TEXT("Progression merit survives save round trip"),
        IsValid(Restored)
            ? Restored->CampaignProgression.CampaignMerit
            : 0,
        Progression.CampaignMerit);
    TestEqual(TEXT("Tactical selection survives save round trip"),
        IsValid(Restored)
            ? Restored->CampaignProgression.ActiveTacticalOption
            : EBHOperationTacticalOption::None,
        EBHOperationTacticalOption::MedicalPreparation);
    TestEqual(TEXT("Tactical debrief survives save round trip"),
        IsValid(Restored)
            ? Restored->CampaignProgression.LastAfterAction.TacticalOption
            : EBHOperationTacticalOption::None,
        EBHOperationTacticalOption::MedicalPreparation);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBHFieldCivilianAidDeliveryTest,
    "BrokenHorizon.PersistentWar.Population.FieldCivilianAidDelivery",
    EAutomationTestFlags::EditorContext |
        EAutomationTestFlags::EngineFilter
)

bool FBHFieldCivilianAidDeliveryTest::RunTest(
    const FString& Parameters
)
{
    UGameInstance* GameInstance =
        NewObject<UGameInstance>(GetTransientPackage());
    UBHWarSubsystem* War =
        NewObject<UBHWarSubsystem>(GameInstance);
    TestNotNull(TEXT("War subsystem is created"), War);

    if (!War)
    {
        return false;
    }

    War->ResetCampaign();
    const FName SourceSectorID = TEXT("WesternFOB");
    const FName DestinationSectorID =
        War->GetRecommendedFieldCivilianAidDestination(
            SourceSectorID
        );
    TestFalse(
        TEXT("A high-need community receives an aid route"),
        DestinationSectorID.IsNone()
    );

    if (DestinationSectorID.IsNone())
    {
        return false;
    }

    const FBHWarSectorState SourceBefore =
        War->GetSectorState(SourceSectorID);
    const FBHWarSectorState DestinationBefore =
        War->GetSectorState(DestinationSectorID);
    const float LoadedAid =
        War->WithdrawFieldCivilianAidSupply(
            SourceSectorID,
            DestinationSectorID
        );
    TestEqual(
        TEXT("Field transport loads one aid package"),
        LoadedAid,
        War->GetCivilianAidSupplyCost()
    );
    TestEqual(
        TEXT("Loading aid consumes source-sector supply"),
        War->GetSectorState(SourceSectorID).Supply,
        SourceBefore.Supply - LoadedAid
    );
    TestEqual(
        TEXT(
            "In-transit aid retains its useful assigned community"
        ),
        War->GetRecommendedInTransitCivilianAidDestination(
            SourceSectorID,
            DestinationSectorID
        ),
        DestinationSectorID
    );
    TestFalse(
        TEXT("An incomplete aid package cannot be delivered"),
        War->DeliverFieldCivilianAidSupply(
            SourceSectorID,
            DestinationSectorID,
            LoadedAid * 0.5f
        )
    );
    TestTrue(
        TEXT("Driving the full package completes field aid"),
        War->DeliverFieldCivilianAidSupply(
            SourceSectorID,
            DestinationSectorID,
            LoadedAid
        )
    );

    const FBHWarSectorState DestinationAfter =
        War->GetSectorState(DestinationSectorID);
    TestTrue(
        TEXT("Physical aid improves civilian support"),
        DestinationAfter.CivilianSupport >
            DestinationBefore.CivilianSupport
    );
    TestTrue(
        TEXT("Physical aid improves local intelligence"),
        DestinationAfter.IntelConfidence >
            DestinationBefore.IntelConfidence
    );
    for (int32 DeliveryIndex = 0;
        DeliveryIndex < 20;
        ++DeliveryIndex)
    {
        War->DeliverFieldCivilianAidSupply(
            SourceSectorID,
            DestinationSectorID,
            LoadedAid
        );
    }

    const FName ReroutedDestinationSectorID =
        War->GetRecommendedInTransitCivilianAidDestination(
            SourceSectorID,
            DestinationSectorID
        );
    TestFalse(
        TEXT(
            "Aid cargo keeps a valid route when local need changes"
        ),
        ReroutedDestinationSectorID.IsNone()
    );
    TestNotEqual(
        TEXT(
            "Aid cargo reroutes after its assigned community "
            "is fully supported"
        ),
        ReroutedDestinationSectorID,
        DestinationSectorID
    );
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBHStrategicTempoTest,
    "BrokenHorizon.PersistentWar.Campaign.StrategicTempo",
    EAutomationTestFlags::EditorContext |
        EAutomationTestFlags::EngineFilter
)

bool FBHStrategicTempoTest::RunTest(const FString& Parameters)
{
    UGameInstance* GameInstance =
        NewObject<UGameInstance>(GetTransientPackage());
    UBHWarSubsystem* War =
        NewObject<UBHWarSubsystem>(GameInstance);
    TestNotNull(TEXT("War subsystem is created"), War);

    if (!War)
    {
        return false;
    }

    War->ResetCampaign();
    TestEqual(
        TEXT("Free-roam strategic pulse uses field tempo"),
        War->GetCurrentSimulationIntervalSeconds(),
        120.0f
    );
    TestEqual(
        TEXT("Fresh campaign exposes the full field countdown"),
        War->GetSecondsUntilNextWarTurn(),
        120.0f
    );

    const FName PrioritySectorID = War->GetPrioritySectorID();
    const EBHWarPriorityType PriorityType = War->GetPriorityType();
    TestTrue(
        TEXT("Current priority can be committed"),
        War->SetCommittedOperation(
            PrioritySectorID,
            PriorityType
        )
    );
    TestEqual(
        TEXT("Committed operation slows background strategic tempo"),
        War->GetCurrentSimulationIntervalSeconds(),
        180.0f
    );
    TestEqual(
        TEXT("Engaged countdown reflects the committed tempo"),
        War->GetSecondsUntilNextWarTurn(),
        180.0f
    );
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBHFortyMinuteCampaignEnduranceTest,
    "BrokenHorizon.PersistentWar.Campaign.FortyMinuteEndurance",
    EAutomationTestFlags::EditorContext |
        EAutomationTestFlags::EngineFilter
)

bool FBHFortyMinuteCampaignEnduranceTest::RunTest(
    const FString& Parameters
)
{
    UGameInstance* GameInstance =
        NewObject<UGameInstance>(GetTransientPackage());
    UBHWarSubsystem* War =
        NewObject<UBHWarSubsystem>(GameInstance);
    TestNotNull(TEXT("War subsystem is created"), War);

    if (!War)
    {
        return false;
    }

    War->ResetCampaign();
    constexpr int32 SessionTurnCount = 20;
    TSet<FName> InitialSectorIDs;

    for (const FBHWarSectorState& Sector :
        War->GetSectorStates())
    {
        InitialSectorIDs.Add(Sector.SectorID);
    }

    TestTrue(
        TEXT("Campaign starts with multiple persistent sectors"),
        InitialSectorIDs.Num() >= 3
    );

    for (int32 TurnIndex = 0;
        TurnIndex < SessionTurnCount;
        ++TurnIndex)
    {
        War->AdvanceWarTurn();
        const TArray<FBHWarSectorState> CurrentSectors =
            War->GetSectorStates();
        TSet<FName> CurrentSectorIDs;

        TestEqual(
            *FString::Printf(
                TEXT("Turn %d preserves every sector"),
                TurnIndex + 1
            ),
            CurrentSectors.Num(),
            InitialSectorIDs.Num()
        );

        for (const FBHWarSectorState& Sector : CurrentSectors)
        {
            CurrentSectorIDs.Add(Sector.SectorID);
            TestTrue(
                *FString::Printf(
                    TEXT("Turn %d keeps %s supply valid"),
                    TurnIndex + 1,
                    *Sector.SectorID.ToString()
                ),
                FMath::IsFinite(Sector.Supply) &&
                    Sector.Supply >= 0.0f &&
                    Sector.Supply <= 100.0f
            );
            TestTrue(
                *FString::Printf(
                    TEXT("Turn %d keeps %s forces valid"),
                    TurnIndex + 1,
                    *Sector.SectorID.ToString()
                ),
                FMath::IsFinite(Sector.FriendlyStrength) &&
                    FMath::IsFinite(Sector.EnemyStrength) &&
                    Sector.FriendlyStrength >= 0.0f &&
                    Sector.EnemyStrength >= 0.0f
            );
            TestTrue(
                *FString::Printf(
                    TEXT("Turn %d keeps %s population valid"),
                    TurnIndex + 1,
                    *Sector.SectorID.ToString()
                ),
                FMath::IsFinite(Sector.CivilianSupport) &&
                    Sector.CivilianSupport >= 0.0f &&
                    Sector.CivilianSupport <= 100.0f
            );
        }

        TestEqual(
            *FString::Printf(
                TEXT("Turn %d preserves unique sector identities"),
                TurnIndex + 1
            ),
            CurrentSectorIDs.Num(),
            CurrentSectors.Num()
        );

        if (War->IsCampaignResolved())
        {
            AddError(
                FString::Printf(
                    TEXT(
                        "Background war resolved prematurely on turn %d "
                        "during a 40-minute-equivalent session."
                    ),
                    TurnIndex + 1
                )
            );
            break;
        }
    }

    TestEqual(
        TEXT("A full field session advances twenty strategic turns"),
        War->GetTurnNumber(),
        SessionTurnCount
    );
    TestEqual(
        TEXT("Campaign remains open after a full field session"),
        War->GetCampaignOutcome(),
        EBHWarCampaignOutcome::Ongoing
    );
    TestFalse(
        TEXT("Ongoing campaign retains a priority sector"),
        War->GetPrioritySectorID().IsNone()
    );
    TestTrue(
        TEXT("Ongoing campaign retains an actionable priority"),
        War->GetPriorityType() != EBHWarPriorityType::None &&
            War->IsViableOperation(
                War->GetPrioritySectorID(),
                War->GetPriorityType()
            )
    );
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBHFortyMinuteGameplayLoopTest,
    "BrokenHorizon.PersistentWar.Campaign.FortyMinuteGameplayLoop",
    EAutomationTestFlags::EditorContext |
        EAutomationTestFlags::EngineFilter
)

bool FBHFortyMinuteGameplayLoopTest::RunTest(
    const FString& Parameters
)
{
    UGameInstance* GameInstance =
        NewObject<UGameInstance>(GetTransientPackage());
    UBHWarSubsystem* War =
        NewObject<UBHWarSubsystem>(GameInstance);
    TestNotNull(TEXT("War subsystem is created"), War);

    if (!War)
    {
        return false;
    }

    War->ResetCampaign();
    constexpr int32 SessionTurnCount = 20;
    constexpr int32 OperationIntervalTurns = 5;
    int32 OperationsAttempted = 0;
    int32 OperationsResolved = 0;
    int32 SuccessfulOperations = 0;
    float TotalRecoveredMateriel = 0.0f;

    const auto FindFundedOperation =
        [War](
            FName& OutSectorID,
            EBHWarPriorityType& OutOperationType
        ) -> bool
        {
            OutSectorID = NAME_None;
            OutOperationType = EBHWarPriorityType::None;

            const FName PrioritySectorID =
                War->GetPrioritySectorID();
            const EBHWarPriorityType PriorityType =
                War->GetPriorityType();

            if (War->IsViableOperation(
                    PrioritySectorID,
                    PriorityType
                ) &&
                War->CanFundOperation(
                    PrioritySectorID,
                    PriorityType
                ))
            {
                OutSectorID = PrioritySectorID;
                OutOperationType = PriorityType;
                return true;
            }

            const EBHWarPriorityType CandidateTypes[] = {
                EBHWarPriorityType::Defend,
                EBHWarPriorityType::Raid,
                EBHWarPriorityType::Attack
            };

            for (const FBHWarSectorState& Sector :
                War->GetSectorStates())
            {
                for (const EBHWarPriorityType CandidateType :
                    CandidateTypes)
                {
                    if (War->IsViableOperation(
                            Sector.SectorID,
                            CandidateType
                        ) &&
                        War->CanFundOperation(
                            Sector.SectorID,
                            CandidateType
                        ))
                    {
                        OutSectorID = Sector.SectorID;
                        OutOperationType = CandidateType;
                        return true;
                    }
                }
            }

            return false;
        };

    for (int32 TurnIndex = 0;
        TurnIndex < SessionTurnCount;
        ++TurnIndex)
    {
        if ((TurnIndex % OperationIntervalTurns) == 0)
        {
            ++OperationsAttempted;
            FName SectorID = NAME_None;
            EBHWarPriorityType OperationType =
                EBHWarPriorityType::None;
            const bool bFoundOperation =
                FindFundedOperation(
                    SectorID,
                    OperationType
                );
            TestTrue(
                *FString::Printf(
                    TEXT(
                        "Operation cycle %d finds a viable funded task"
                    ),
                    OperationsAttempted
                ),
                bFoundOperation
            );

            if (!bFoundOperation)
            {
                break;
            }

            const FName FriendlySourceSectorID =
                War->GetOperationSupplySource(
                    SectorID,
                    OperationType
                );
            const FName EnemySourceSectorID =
                War->GetOperationEnemySource(
                    SectorID,
                    OperationType
                );
            const bool bFriendlySucceeded =
                (OperationsAttempted % 2) == 1;
            const int32 FriendlyCasualties =
                bFriendlySucceeded ? 0 : 1;
            const int32 EnemyCasualties =
                bFriendlySucceeded ? 3 : 1;

            TestTrue(
                *FString::Printf(
                    TEXT("Operation cycle %d commits"),
                    OperationsAttempted
                ),
                War->SetCommittedOperation(
                    SectorID,
                    OperationType
                )
            );
            TestTrue(
                *FString::Printf(
                    TEXT("Operation cycle %d consumes routed supply"),
                    OperationsAttempted
                ),
                War->ConsumeOperationSupply(
                    SectorID,
                    OperationType
                )
            );
            TestTrue(
                *FString::Printf(
                    TEXT("Operation cycle %d applies field casualties"),
                    OperationsAttempted
                ),
                War->ApplyOperationCasualtyResult(
                    SectorID,
                    FriendlySourceSectorID,
                    EnemySourceSectorID,
                    FriendlyCasualties,
                    EnemyCasualties
                )
            );
            const bool bResultApplied =
                War->ApplyMissionResult(
                    SectorID,
                    OperationType,
                    bFriendlySucceeded
                );
            TestTrue(
                *FString::Printf(
                    TEXT("Operation cycle %d resolves"),
                    OperationsAttempted
                ),
                bResultApplied
            );

            if (!bResultApplied)
            {
                break;
            }

            ++OperationsResolved;

            if (OperationType ==
                EBHWarPriorityType::Raid)
            {
                War->ApplyRaidOperationalSignature(
                    SectorID,
                    EnemyCasualties,
                    FriendlyCasualties,
                    bFriendlySucceeded,
                    !bFriendlySucceeded
                );
            }

            if (bFriendlySucceeded)
            {
                ++SuccessfulOperations;
                TotalRecoveredMateriel +=
                    War->RecoverBattlefieldMateriel(
                        SectorID,
                        EnemyCasualties,
                        FriendlyCasualties,
                        true
                    );
            }

            TestFalse(
                *FString::Printf(
                    TEXT(
                        "Operation cycle %d releases its commitment"
                    ),
                    OperationsAttempted
                ),
                War->HasCommittedOperation()
            );
        }

        War->AdvanceWarTurn();
        TestFalse(
            *FString::Printf(
                TEXT(
                    "Turn %d remains in an ongoing campaign"
                ),
                TurnIndex + 1
            ),
            War->IsCampaignResolved()
        );
    }

    TestEqual(
        TEXT(
            "Forty-minute loop advances background and operation turns"
        ),
        War->GetTurnNumber(),
        SessionTurnCount + OperationsResolved
    );
    TestEqual(
        TEXT("Four operation opportunities occur during the session"),
        OperationsAttempted,
        4
    );
    TestEqual(
        TEXT("Every selected operation resolves cleanly"),
        OperationsResolved,
        OperationsAttempted
    );
    TestEqual(
        TEXT("Session includes both successful and failed operations"),
        SuccessfulOperations,
        2
    );
    TestTrue(
        TEXT("Successful field actions recover strategic materiel"),
        TotalRecoveredMateriel > 0.0f
    );
    TestFalse(
        TEXT("Session ends without a stuck operation commitment"),
        War->HasCommittedOperation()
    );
    TestEqual(
        TEXT("Repeatable field loop leaves campaign ongoing"),
        War->GetCampaignOutcome(),
        EBHWarCampaignOutcome::Ongoing
    );

    FName NextSectorID = NAME_None;
    EBHWarPriorityType NextOperationType =
        EBHWarPriorityType::None;
    TestTrue(
        TEXT("A funded follow-up operation remains available"),
        FindFundedOperation(
            NextSectorID,
            NextOperationType
        )
    );
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBHLongSessionCampaignRoundTripTest,
    "BrokenHorizon.Persistence.Campaign.LongSessionRoundTrip",
    EAutomationTestFlags::EditorContext |
        EAutomationTestFlags::EngineFilter
)

bool FBHLongSessionCampaignRoundTripTest::RunTest(
    const FString& Parameters
)
{
    UGameInstance* SourceGameInstance =
        NewObject<UGameInstance>(GetTransientPackage());
    UBHWarSubsystem* SourceWar =
        NewObject<UBHWarSubsystem>(SourceGameInstance);
    TestNotNull(TEXT("Source war subsystem is created"), SourceWar);

    if (!SourceWar)
    {
        return false;
    }

    SourceWar->ResetCampaign();

    for (int32 TurnIndex = 0; TurnIndex < 10; ++TurnIndex)
    {
        SourceWar->AdvanceWarTurn();
    }

    const FName OperationSectorID =
        SourceWar->GetPrioritySectorID();
    const EBHWarPriorityType OperationType =
        SourceWar->GetPriorityType();
    TestFalse(
        TEXT("Long session retains an operation target"),
        OperationSectorID.IsNone()
    );
    TestTrue(
        TEXT("Long session priority can be committed"),
        SourceWar->SetCommittedOperation(
            OperationSectorID,
            OperationType
        )
    );

    UBHSaveGame* SaveData =
        NewObject<UBHSaveGame>(GetTransientPackage());
    TestNotNull(TEXT("Campaign save object is created"), SaveData);

    if (!SaveData)
    {
        return false;
    }

    SaveData->bRuntimeWarOperation = true;
    SaveData->AssignedWarSectorID = OperationSectorID;
    SaveData->AssignedWarSupplySourceSectorID =
        SourceWar->GetCommittedOperationSupplySourceSectorID();
    SaveData->AssignedWarPriorityType = OperationType;
    SaveData->OpenWorldOperationState.bHasSnapshot = true;
    SaveData->OpenWorldOperationState.bOperationActivated = false;
    SaveData->OpenWorldOperationState
        .SecondsUntilApproachDeadline = 315.0f;
    SaveData->WarSectorStates = SourceWar->GetSectorStates();
    SaveData->WarSupplyConvoys = SourceWar->GetSupplyConvoys();
    SaveData->WarGarrisonTransfers =
        SourceWar->GetGarrisonTransfers();
    SaveData->WarFriendlyManpowerReserve =
        SourceWar->GetFactionManpowerReserve(
            EBHWarFaction::Friendly
        );
    SaveData->WarEnemyManpowerReserve =
        SourceWar->GetFactionManpowerReserve(
            EBHWarFaction::Enemy
        );
    SaveData->WarFriendlyRecruitmentProgress =
        SourceWar->GetFactionRecruitmentProgress(
            EBHWarFaction::Friendly
        );
    SaveData->WarEnemyRecruitmentProgress =
        SourceWar->GetFactionRecruitmentProgress(
            EBHWarFaction::Enemy
        );
    SaveData->WarEventHistory =
        SourceWar->GetRecentWarEvents();
    SaveData->WarTurnNumber = SourceWar->GetTurnNumber();
    SaveData->WarSimulationAccumulator = 73.25f;
    SaveData->WarCommittedOperationID =
        SourceWar->GetCommittedOperationID();
    SaveData->WarCommittedOperationTargetID =
        SourceWar->GetCommittedOperationTargetID();

    TArray<uint8> Bytes;
    TestTrue(
        TEXT("Long-session campaign serializes"),
        UGameplayStatics::SaveGameToMemory(SaveData, Bytes)
    );
    UBHSaveGame* RestoredSave = Cast<UBHSaveGame>(
        UGameplayStatics::LoadGameFromMemory(Bytes)
    );
    TestNotNull(
        TEXT("Long-session campaign deserializes"),
        RestoredSave
    );

    if (!RestoredSave)
    {
        return false;
    }

    TestTrue(
        TEXT("Runtime operation flag survives"),
        RestoredSave->bRuntimeWarOperation
    );
    TestEqual(
        TEXT("Assigned sector survives"),
        RestoredSave->AssignedWarSectorID,
        OperationSectorID
    );
    TestEqual(
        TEXT("Assigned staging source survives"),
        RestoredSave->AssignedWarSupplySourceSectorID,
        SaveData->AssignedWarSupplySourceSectorID
    );
    TestEqual(
        TEXT("Assigned operation type survives"),
        RestoredSave->AssignedWarPriorityType,
        OperationType
    );
    TestEqual(
        TEXT("Stable operation identity survives serialization"),
        RestoredSave->WarCommittedOperationID,
        SourceWar->GetCommittedOperationID()
    );
    TestEqual(
        TEXT("Stable operation target survives serialization"),
        RestoredSave->WarCommittedOperationTargetID,
        SourceWar->GetCommittedOperationTargetID()
    );
    TestEqual(
        TEXT("Tactical approach window survives"),
        RestoredSave->OpenWorldOperationState
            .SecondsUntilApproachDeadline,
        315.0f
    );

    UGameInstance* RestoredGameInstance =
        NewObject<UGameInstance>(GetTransientPackage());
    UBHWarSubsystem* RestoredWar =
        NewObject<UBHWarSubsystem>(RestoredGameInstance);
    TestNotNull(
        TEXT("Restored war subsystem is created"),
        RestoredWar
    );

    if (!RestoredWar)
    {
        return false;
    }

    TestTrue(
        TEXT("Strategic state restores from serialized campaign"),
        RestoredWar->RestoreWarState(
            RestoredSave->WarSectorStates,
            RestoredSave->WarSupplyConvoys,
            RestoredSave->WarEventHistory,
            RestoredSave->WarTurnNumber,
            RestoredSave->WarSimulationAccumulator,
            RestoredSave->SchemaVersion
        )
    );
    TestTrue(
        TEXT("Garrison transfers restore"),
        RestoredWar->RestoreGarrisonTransfers(
            RestoredSave->WarGarrisonTransfers
        )
    );
    TestTrue(
        TEXT("Faction manpower restores"),
        RestoredWar->RestoreManpowerState(
            RestoredSave->WarFriendlyManpowerReserve,
            RestoredSave->WarEnemyManpowerReserve,
            RestoredSave->WarFriendlyRecruitmentProgress,
            RestoredSave->WarEnemyRecruitmentProgress
        )
    );
    TestTrue(
        TEXT("Active operation commitment restores"),
        RestoredWar->RestoreCommittedOperation(
            RestoredSave->AssignedWarSectorID,
            RestoredSave->AssignedWarPriorityType,
            RestoredSave->WarCommittedOperationID,
            RestoredSave->WarCommittedOperationTargetID
        )
    );
    TestEqual(
        TEXT("Stable operation identity survives campaign restore"),
        RestoredWar->GetCommittedOperationID(),
        SourceWar->GetCommittedOperationID()
    );
    TestEqual(
        TEXT("Stable operation target survives campaign restore"),
        RestoredWar->GetCommittedOperationTargetID(),
        SourceWar->GetCommittedOperationTargetID()
    );
    TestEqual(
        TEXT("Strategic turn survives"),
        RestoredWar->GetTurnNumber(),
        SourceWar->GetTurnNumber()
    );
    TestEqual(
        TEXT("Partial strategic countdown survives"),
        RestoredWar->GetSimulationAccumulator(),
        73.25f
    );
    TestEqual(
        TEXT("Operation staging source remains stable"),
        RestoredWar->GetCommittedOperationSupplySourceSectorID(),
        SourceWar->GetCommittedOperationSupplySourceSectorID()
    );
    TestEqual(
        TEXT("Operation hostile source remains stable"),
        RestoredWar->GetCommittedOperationEnemySourceSectorID(),
        SourceWar->GetCommittedOperationEnemySourceSectorID()
    );
    TestEqual(
        TEXT("Friendly manpower remains stable"),
        RestoredWar->GetFactionManpowerReserve(
            EBHWarFaction::Friendly
        ),
        SourceWar->GetFactionManpowerReserve(
            EBHWarFaction::Friendly
        )
    );
    TestEqual(
        TEXT("Enemy manpower remains stable"),
        RestoredWar->GetFactionManpowerReserve(
            EBHWarFaction::Enemy
        ),
        SourceWar->GetFactionManpowerReserve(
            EBHWarFaction::Enemy
        )
    );

    const auto TestEquivalentSectorStates =
        [this](
            const UBHWarSubsystem* Expected,
            const UBHWarSubsystem* Actual,
            const FString& Phase
        )
        {
            const TArray<FBHWarSectorState> ExpectedSectors =
                Expected->GetSectorStates();
            TestEqual(
                *(Phase + TEXT(" sector count matches")),
                Actual->GetSectorStates().Num(),
                ExpectedSectors.Num()
            );

            for (const FBHWarSectorState& ExpectedSector :
                ExpectedSectors)
            {
                const FBHWarSectorState ActualSector =
                    Actual->GetSectorState(
                        ExpectedSector.SectorID
                    );
                const FString Prefix = FString::Printf(
                    TEXT("%s %s"),
                    *Phase,
                    *ExpectedSector.SectorID.ToString()
                );
                TestEqual(
                    *(Prefix + TEXT(" owner matches")),
                    ActualSector.Owner,
                    ExpectedSector.Owner
                );
                TestEqual(
                    *(Prefix + TEXT(" friendly garrison matches")),
                    ActualSector.FriendlyGarrison,
                    ExpectedSector.FriendlyGarrison
                );
                TestEqual(
                    *(Prefix + TEXT(" enemy garrison matches")),
                    ActualSector.EnemyGarrison,
                    ExpectedSector.EnemyGarrison
                );
                TestTrue(
                    *(Prefix + TEXT(" friendly strength matches")),
                    FMath::IsNearlyEqual(
                        ActualSector.FriendlyStrength,
                        ExpectedSector.FriendlyStrength,
                        0.01f
                    )
                );
                TestTrue(
                    *(Prefix + TEXT(" enemy strength matches")),
                    FMath::IsNearlyEqual(
                        ActualSector.EnemyStrength,
                        ExpectedSector.EnemyStrength,
                        0.01f
                    )
                );
                TestTrue(
                    *(Prefix + TEXT(" supply matches")),
                    FMath::IsNearlyEqual(
                        ActualSector.Supply,
                        ExpectedSector.Supply,
                        0.01f
                    )
                );
                TestTrue(
                    *(Prefix + TEXT(" civilian support matches")),
                    FMath::IsNearlyEqual(
                        ActualSector.CivilianSupport,
                        ExpectedSector.CivilianSupport,
                        0.01f
                    )
                );
                TestTrue(
                    *(Prefix + TEXT(" intelligence matches")),
                    FMath::IsNearlyEqual(
                        ActualSector.IntelConfidence,
                        ExpectedSector.IntelConfidence,
                        0.01f
                    )
                );
            }
        };

    TestEquivalentSectorStates(
        SourceWar,
        RestoredWar,
        TEXT("Restored")
    );
    SourceWar->AdvanceWarTurn();
    RestoredWar->AdvanceWarTurn();
    TestEquivalentSectorStates(
        SourceWar,
        RestoredWar,
        TEXT("Continued")
    );
    TestEqual(
        TEXT("Restored campaign advances the same turn"),
        RestoredWar->GetTurnNumber(),
        SourceWar->GetTurnNumber()
    );
    TestTrue(
        TEXT("Restored operation remains committed after continuation"),
        RestoredWar->HasCommittedOperation()
    );
    TestEqual(
        TEXT("Restored operation remains the strategic priority"),
        RestoredWar->GetPrioritySectorID(),
        OperationSectorID
    );
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBHPersistentGarrisonTest,
    "BrokenHorizon.PersistentWar.Garrison.Lifecycle",
    EAutomationTestFlags::EditorContext |
        EAutomationTestFlags::EngineFilter
)

bool FBHPersistentGarrisonTest::RunTest(
    const FString& Parameters
)
{
    UGameInstance* GameInstance =
        NewObject<UGameInstance>(GetTransientPackage());
    UBHWarSubsystem* War =
        NewObject<UBHWarSubsystem>(GameInstance);
    TestNotNull(TEXT("War subsystem is created"), War);

    if (!War)
    {
        return false;
    }

    War->ResetCampaign();
    const FBHWarSectorState InitialCrossroads =
        War->GetSectorState(TEXT("KoronaCrossroads"));
    TestEqual(
        TEXT("Crossroads starts with finite defenders"),
        InitialCrossroads.EnemyGarrison,
        9
    );
    TestEqual(
        TEXT("Crossroads garrison has a fixed capacity"),
        InitialCrossroads.GarrisonCapacity,
        12
    );

    TestTrue(
        TEXT("Battle casualties are accepted"),
        War->ApplyAmbientBattleResult(
            TEXT("KoronaCrossroads"),
            0,
            3
        )
    );
    TestEqual(
        TEXT("Enemy losses persist in the garrison"),
        War->GetSectorGarrisonCount(
            TEXT("KoronaCrossroads"),
            EBHWarFaction::Enemy
        ),
        6
    );

    TestTrue(
        TEXT("Successful assault resolves"),
        War->ApplyMissionResult(
            TEXT("KoronaCrossroads"),
            EBHWarPriorityType::Attack,
            true
        )
    );
    const FBHWarSectorState CapturedCrossroads =
        War->GetSectorState(TEXT("KoronaCrossroads"));
    TestEqual(
        TEXT("Captured site removes enemy defenders"),
        CapturedCrossroads.EnemyGarrison,
        0
    );
    TestTrue(
        TEXT("Captured site receives occupation troops"),
        CapturedCrossroads.FriendlyGarrison >= 2
    );

    TArray<FBHWarSectorState> LegacySectors =
        War->GetSectorStates();
    for (FBHWarSectorState& Sector : LegacySectors)
    {
        Sector.GarrisonCapacity = 0;
        Sector.FriendlyGarrison = 0;
        Sector.EnemyGarrison = 0;
    }

    War->RestoreWarState(
        LegacySectors,
        TArray<FBHWarSupplyConvoyState>(),
        TArray<FBHWarEventRecord>(),
        7,
        0.0f,
        15
    );

    const FBHWarSectorState MigratedCrossroads =
        War->GetSectorState(TEXT("KoronaCrossroads"));
    TestEqual(
        TEXT("Legacy saves receive the default garrison capacity"),
        MigratedCrossroads.GarrisonCapacity,
        12
    );
    TestEqual(
        TEXT("Legacy saves do not restore defeated defenders"),
        MigratedCrossroads.EnemyGarrison,
        0
    );
    TestEqual(
        TEXT("Legacy saves preserve the occupying faction"),
        MigratedCrossroads.FriendlyGarrison,
        2
    );
    TestEqual(
        TEXT("Legacy friendly sectors receive reliable intel"),
        MigratedCrossroads.IntelConfidence,
        85.0f
    );

    TArray<FBHWarSectorState> CurrentSchemaSectors =
        War->GetSectorStates();
    for (FBHWarSectorState& Sector : CurrentSchemaSectors)
    {
        if (Sector.SectorID == TEXT("KoronaCrossroads"))
        {
            Sector.IntelConfidence = 0.0f;
        }
    }

    War->RestoreWarState(
        CurrentSchemaSectors,
        TArray<FBHWarSupplyConvoyState>(),
        TArray<FBHWarEventRecord>(),
        8,
        0.0f,
        16
    );
    TestEqual(
        TEXT("Friendly territory never restores as unknown"),
        War->GetSectorIntelConfidence(
            TEXT("KoronaCrossroads")
        ),
        85.0f
    );
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBHOperationRoutStrategicImpactTest,
    "BrokenHorizon.PersistentWar.Operation.RoutStrategicImpact",
    EAutomationTestFlags::EditorContext |
        EAutomationTestFlags::EngineFilter
)

bool FBHOperationRoutStrategicImpactTest::RunTest(
    const FString& Parameters
)
{
    UGameInstance* GameInstance =
        NewObject<UGameInstance>(GetTransientPackage());
    UBHWarSubsystem* War =
        NewObject<UBHWarSubsystem>(GameInstance);
    TestNotNull(TEXT("War subsystem is created"), War);

    if (!War)
    {
        return false;
    }

    War->ResetCampaign();
    const FName SectorID(TEXT("KoronaCrossroads"));
    const FBHWarSectorState Before =
        War->GetSectorState(SectorID);

    TestTrue(
        TEXT("Operation rout is applied"),
        War->ApplyOperationRoutResult(SectorID, 3)
    );

    const FBHWarSectorState After =
        War->GetSectorState(SectorID);
    TestTrue(
        TEXT("Routed troops reduce immediate enemy strength"),
        After.EnemyStrength < Before.EnemyStrength
    );
    TestTrue(
        TEXT("Rout consumes less battlefield supply"),
        After.Supply < Before.Supply
    );
    TestEqual(
        TEXT("Routed troops are not counted as casualties"),
        After.EnemyGarrison,
        Before.EnemyGarrison
    );

    const TArray<FBHWarEventRecord> Events =
        War->GetRecentWarEvents();
    bool bFoundRoutEvent = false;
    for (const FBHWarEventRecord& Event : Events)
    {
        if (Event.EventType ==
            FName(TEXT("EnemyOperationForcesRouted")))
        {
            bFoundRoutEvent = true;
            break;
        }
    }
    TestTrue(
        TEXT("Operation rout is recorded in campaign history"),
        bFoundRoutEvent
    );
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBHOperationCasualtyStrategicImpactTest,
    "BrokenHorizon.PersistentWar.Operation.CasualtyStrategicImpact",
    EAutomationTestFlags::EditorContext |
        EAutomationTestFlags::EngineFilter
)

bool FBHOperationCasualtyStrategicImpactTest::RunTest(
    const FString& Parameters
)
{
    UGameInstance* GameInstance =
        NewObject<UGameInstance>(GetTransientPackage());
    UBHWarSubsystem* War =
        NewObject<UBHWarSubsystem>(GameInstance);
    TestNotNull(TEXT("War subsystem is created"), War);

    if (!War)
    {
        return false;
    }

    War->ResetCampaign();
    const FName TargetSectorID(TEXT("KoronaCrossroads"));
    const FName FriendlySourceSectorID(TEXT("WesternFOB"));
    const FName EnemySourceSectorID(TEXT("KoronaCrossroads"));
    const int32 FriendlyGarrisonBefore =
        War->GetSectorGarrisonCount(
            FriendlySourceSectorID,
            EBHWarFaction::Friendly
        );
    const int32 EnemyGarrisonBefore =
        War->GetSectorGarrisonCount(
            EnemySourceSectorID,
            EBHWarFaction::Enemy
        );

    TestTrue(
        TEXT("Operation casualties are applied"),
        War->ApplyOperationCasualtyResult(
            TargetSectorID,
            FriendlySourceSectorID,
            EnemySourceSectorID,
            1,
            2
        )
    );
    TestEqual(
        TEXT("Friendly support casualties leave staging"),
        War->GetSectorGarrisonCount(
            FriendlySourceSectorID,
            EBHWarFaction::Friendly
        ),
        FriendlyGarrisonBefore - 1
    );
    TestEqual(
        TEXT("Enemy casualties leave their source garrison"),
        War->GetSectorGarrisonCount(
            EnemySourceSectorID,
            EBHWarFaction::Enemy
        ),
        EnemyGarrisonBefore - 2
    );

    const TArray<FBHWarEventRecord> Events =
        War->GetRecentWarEvents();
    bool bFoundCasualtyEvent = false;
    for (const FBHWarEventRecord& Event : Events)
    {
        if (Event.EventType ==
            FName(TEXT("OperationCasualtiesApplied")))
        {
            bFoundCasualtyEvent = true;
            break;
        }
    }
    TestTrue(
        TEXT("Operation casualties enter campaign history"),
        bFoundCasualtyEvent
    );
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBHPersistentEnemyResponseTest,
    "BrokenHorizon.PersistentWar.Response.Escalation",
    EAutomationTestFlags::EditorContext |
        EAutomationTestFlags::EngineFilter
)

bool FBHPersistentEnemyResponseTest::RunTest(
    const FString& Parameters
)
{
    UGameInstance* GameInstance =
        NewObject<UGameInstance>(GetTransientPackage());
    UBHWarSubsystem* War =
        NewObject<UBHWarSubsystem>(GameInstance);
    TestNotNull(TEXT("War subsystem is created"), War);

    if (!War)
    {
        return false;
    }

    const FName SectorID(TEXT("KoronaCrossroads"));
    War->ResetCampaign();
    TestEqual(
        TEXT("A quiet sector starts dormant"),
        War->GetSectorEnemyResponsePressure(SectorID),
        0.0f
    );

    const FBHWarOperationForcePackage QuietPackage =
        BHWarOperationRules::BuildForcePackage(
            War,
            SectorID,
            EBHWarPriorityType::Attack,
            War->GetOperationSupplySource(
                SectorID,
                EBHWarPriorityType::Attack
            )
        );

    for (int32 ReportIndex = 0; ReportIndex < 4; ++ReportIndex)
    {
        TestTrue(
            TEXT("Repeated contact reports are accepted"),
            War->ApplyAmbientBattleResult(
                SectorID,
                16,
                0
            )
        );
    }

    const float EscalatedPressure =
        War->GetSectorEnemyResponsePressure(SectorID);
    TestTrue(
        TEXT("Repeated combat raises local enemy response"),
        EscalatedPressure >= 50.0f
    );
    TestTrue(
        TEXT("Strategic summary exposes the response state"),
        War->GetSectorEnemyResponseSummary(SectorID)
            .ToString()
            .Contains(TEXT("HUNTING"))
    );
    const TArray<FBHWarEventRecord> ResponseEvents =
        War->GetRecentWarEvents().FilterByPredicate(
            [](const FBHWarEventRecord& Event)
            {
                return Event.EventType ==
                    FName(TEXT("EnemyResponse"));
            }
        );
    TestEqual(
        TEXT("Only crossed response thresholds create events"),
        ResponseEvents.Num(),
        2
    );
    TestTrue(
        TEXT("Latest response event records active hunting"),
        !ResponseEvents.IsEmpty() &&
            ResponseEvents.Last().Summary.Contains(
                TEXT("HUNTING")
            )
    );

    const FBHWarOperationForcePackage EscalatedPackage =
        BHWarOperationRules::BuildForcePackage(
            War,
            SectorID,
            EBHWarPriorityType::Attack,
            War->GetOperationSupplySource(
                SectorID,
                EBHWarPriorityType::Attack
            )
        );
    TestEqual(
        TEXT("Force packages preserve response pressure"),
        EscalatedPackage.EnemyResponsePressure,
        EscalatedPressure
    );
    TestTrue(
        TEXT("Escalated response can add a reinforcement wave"),
        EscalatedPackage.AttackReinforcementWaveCount >=
            QuietPackage.AttackReinforcementWaveCount
    );

    TestTrue(
        TEXT("An operation can lock the escalated sector"),
        War->SetCommittedOperation(
            SectorID,
            EBHWarPriorityType::Attack
        )
    );
    War->AdvanceWarTurn();
    TestEqual(
        TEXT("Response pressure holds during an active operation"),
        War->GetSectorEnemyResponsePressure(SectorID),
        EscalatedPressure
    );
    War->ClearCommittedOperation();
    War->AdvanceWarTurn();
    TestTrue(
        TEXT("Enemy response decays after operations leave the sector"),
        War->GetSectorEnemyResponsePressure(SectorID) <
            EscalatedPressure
    );
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBHPersistentWarIntelTest,
    "BrokenHorizon.PersistentWar.Intelligence.Lifecycle",
    EAutomationTestFlags::EditorContext |
        EAutomationTestFlags::EngineFilter
)

bool FBHPersistentWarIntelTest::RunTest(
    const FString& Parameters
)
{
    UGameInstance* GameInstance =
        NewObject<UGameInstance>(GetTransientPackage());
    UBHWarSubsystem* War =
        NewObject<UBHWarSubsystem>(GameInstance);
    TestNotNull(TEXT("War subsystem is created"), War);

    if (!War)
    {
        return false;
    }

    War->ResetCampaign();
    TestEqual(
        TEXT("Hostile town begins with incomplete intelligence"),
        War->GetSectorIntelConfidence(
            TEXT("KoronaCrossroads")
        ),
        20.0f
    );
    TestTrue(
        TEXT("Reconnaissance report is accepted"),
        War->ReportSectorRecon(
            TEXT("KoronaCrossroads"),
            45.0f
        )
    );
    TestTrue(
        TEXT("Recon reveals only an estimate"),
        War->GetSectorEnemyIntelSummary(
            TEXT("KoronaCrossroads")
        ).ToString().Contains(TEXT("EST ENEMY"))
    );

    War->AdvanceWarTurn();
    TestEqual(
        TEXT("Enemy intelligence becomes stale each war turn"),
        War->GetSectorIntelConfidence(
            TEXT("KoronaCrossroads")
        ),
        59.0f
    );

    TestTrue(
        TEXT("Follow-up recon improves confidence"),
        War->ReportSectorRecon(
            TEXT("KoronaCrossroads"),
            50.0f
        )
    );
    TestTrue(
        TEXT("High confidence reveals confirmed strength"),
        War->GetSectorEnemyIntelSummary(
            TEXT("KoronaCrossroads")
        ).ToString().Contains(TEXT("CONFIRMED ENEMY"))
    );
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBHCivilianSupportLifecycleTest,
    "BrokenHorizon.PersistentWar.Population.CivilianSupport",
    EAutomationTestFlags::EditorContext |
        EAutomationTestFlags::EngineFilter
)

bool FBHCivilianSupportLifecycleTest::RunTest(
    const FString& Parameters
)
{
    UGameInstance* GameInstance =
        NewObject<UGameInstance>(GetTransientPackage());
    UBHWarSubsystem* War =
        NewObject<UBHWarSubsystem>(GameInstance);
    TestNotNull(TEXT("War subsystem is created"), War);

    if (!War)
    {
        return false;
    }

    War->ResetCampaign();
    const FName SectorID(TEXT("KoronaCrossroads"));
    const float InitialSupport =
        War->GetSectorCivilianSupport(SectorID);
    TestEqual(
        TEXT("Occupied town starts with low local support"),
        InitialSupport,
        30.0f
    );

    TestTrue(
        TEXT("Successful liberation applies"),
        War->ApplyMissionResult(
            SectorID,
            EBHWarPriorityType::Attack,
            true
        )
    );
    TestTrue(
        TEXT("Liberation raises local support"),
        War->GetSectorCivilianSupport(SectorID) >
            InitialSupport
    );
    TestTrue(
        TEXT("Local support creates an intelligence network"),
        War->GetSectorIntelConfidence(SectorID) >=
            War->GetSectorCivilianSupport(SectorID) * 0.5f
    );
    const float LiberatedSupport =
        War->GetSectorCivilianSupport(SectorID);
    TestTrue(
        TEXT("Local security victory raises civilian support"),
        War->ReportCivilianSecurityOutcome(
            SectorID,
            true
        )
    );
    TestTrue(
        TEXT("Security victory changes the local population"),
        War->GetSectorCivilianSupport(SectorID) >
            LiberatedSupport
    );
    TestTrue(
        TEXT("Local security defeat lowers civilian support"),
        War->ReportCivilianSecurityOutcome(
            SectorID,
            false
        )
    );
    TestEqual(
        TEXT("Equal security outcomes cancel each other"),
        War->GetSectorCivilianSupport(SectorID),
        LiberatedSupport
    );
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBHCivilianAidNetworkTest,
    "BrokenHorizon.PersistentWar.Population.CivilianAidNetwork",
    EAutomationTestFlags::EditorContext |
        EAutomationTestFlags::EngineFilter
)

bool FBHCivilianAidNetworkTest::RunTest(
    const FString& Parameters
)
{
    UGameInstance* GameInstance =
        NewObject<UGameInstance>(GetTransientPackage());
    UBHWarSubsystem* War =
        NewObject<UBHWarSubsystem>(GameInstance);
    TestNotNull(TEXT("War subsystem is created"), War);

    if (!War)
    {
        return false;
    }

    War->ResetCampaign();
    const FName TargetID(TEXT("KoronaCrossroads"));
    const FName SourceID =
        War->GetOperationSupplySource(
            TargetID,
            EBHWarPriorityType::Attack
        );
    const FBHWarSectorState TargetBefore =
        War->GetSectorState(TargetID);
    const FBHWarSectorState SourceBefore =
        War->GetSectorState(SourceID);
    const float RecruitmentBefore =
        War->GetFactionRecruitmentPerTurn(
            EBHWarFaction::Friendly
        );

    TestFalse(
        TEXT("Aid has a valid friendly supply source"),
        SourceID.IsNone()
    );
    TestTrue(
        TEXT("Connected hostile communities can receive aid"),
        War->CanDeliverCivilianAid(
            TargetID,
            EBHWarPriorityType::Attack
        )
    );
    TestTrue(
        TEXT("Civilian aid delivery succeeds"),
        War->DeliverCivilianAid(
            TargetID,
            EBHWarPriorityType::Attack
        )
    );

    const FBHWarSectorState TargetDispatched =
        War->GetSectorState(TargetID);
    const FBHWarSectorState SourceDispatched =
        War->GetSectorState(SourceID);
    TestEqual(
        TEXT("Aid consumes strategic supply"),
        SourceDispatched.Supply,
        SourceBefore.Supply -
            War->GetCivilianAidSupplyCost()
    );
    TestEqual(
        TEXT("Aid has no effect before the convoy arrives"),
        TargetDispatched.CivilianSupport,
        TargetBefore.CivilianSupport
    );
    const TArray<FBHWarSupplyConvoyState> DispatchedConvoys =
        War->GetSupplyConvoys();
    const FBHWarSupplyConvoyState* AidConvoy =
        DispatchedConvoys.FindByPredicate(
            [TargetID](
                const FBHWarSupplyConvoyState& Convoy)
            {
                return Convoy.CargoType ==
                        EBHWarConvoyCargoType::CivilianAid &&
                    Convoy.DestinationSectorID == TargetID;
            }
        );
    TestNotNull(
        TEXT("Aid dispatch creates a vulnerable convoy"),
        AidConvoy
    );
    TestFalse(
        TEXT("A second aid shipment cannot stack in transit"),
        War->CanDeliverCivilianAid(
            TargetID,
            EBHWarPriorityType::Attack
        )
    );

    War->AdvanceWarTurn();
    const FBHWarSectorState TargetAfter =
        War->GetSectorState(TargetID);

    TestTrue(
        TEXT("Aid builds local support after surviving transit"),
        TargetAfter.CivilianSupport >
            TargetBefore.CivilianSupport
    );
    TestTrue(
        TEXT("Aid improves local intelligence after arrival"),
        TargetAfter.IntelConfidence >
            TargetBefore.IntelConfidence
    );
    TestTrue(
        TEXT("Occupied aid networks contribute recruits"),
        War->GetFactionRecruitmentPerTurn(
            EBHWarFaction::Friendly
        ) > RecruitmentBefore
    );
    TestTrue(
        TEXT("Aid delivery enters campaign history"),
        War->GetRecentWarEvents().ContainsByPredicate(
            [TargetID](const FBHWarEventRecord& Event)
            {
                return Event.EventType ==
                        FName(TEXT("CivilianAidDelivered")) &&
                    Event.SectorID == TargetID;
            }
        )
    );
    TestFalse(
        TEXT("Delivered aid convoy leaves the active route"),
        War->GetSupplyConvoys().ContainsByPredicate(
            [TargetID](
                const FBHWarSupplyConvoyState& Convoy)
            {
                return Convoy.CargoType ==
                        EBHWarConvoyCargoType::CivilianAid &&
                    Convoy.DestinationSectorID == TargetID;
            }
        )
    );
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBHBattlefieldMaterielRecoveryTest,
    "BrokenHorizon.PersistentWar.Logistics.BattlefieldRecovery",
    EAutomationTestFlags::EditorContext |
        EAutomationTestFlags::EngineFilter
)

bool FBHBattlefieldMaterielRecoveryTest::RunTest(
    const FString& Parameters
)
{
    UGameInstance* GameInstance =
        NewObject<UGameInstance>(GetTransientPackage());
    UBHWarSubsystem* War =
        NewObject<UBHWarSubsystem>(GameInstance);
    TestNotNull(TEXT("War subsystem is created"), War);

    if (!War)
    {
        return false;
    }

    War->ResetCampaign();
    const FName FriendlySectorID(TEXT("DovrenVillage"));
    const FBHWarSectorState Before =
        War->GetSectorState(FriendlySectorID);
    const float AmbientRecovery =
        War->RecoverBattlefieldMateriel(
            FriendlySectorID,
            4,
            1,
            false
        );
    const FBHWarSectorState AfterAmbient =
        War->GetSectorState(FriendlySectorID);

    TestEqual(
        TEXT("Ambient recovery is bounded by its field cap"),
        AmbientRecovery,
        4.0f
    );
    TestEqual(
        TEXT("Recovered materiel enters local strategic supply"),
        AfterAmbient.Supply,
        Before.Supply + AmbientRecovery
    );

    const float OperationRecovery =
        War->RecoverBattlefieldMateriel(
            FriendlySectorID,
            10,
            0,
            true
        );
    TestEqual(
        TEXT("Major operations can recover a larger stockpile"),
        OperationRecovery,
        10.0f
    );
    TestEqual(
        TEXT("Recovery does not occur in unsecured territory"),
        War->RecoverBattlefieldMateriel(
            TEXT("KoronaCrossroads"),
            6,
            0,
            false
        ),
        0.0f
    );
    TestTrue(
        TEXT("Battlefield recovery enters campaign history"),
        War->GetRecentWarEvents().ContainsByPredicate(
            [FriendlySectorID](
                const FBHWarEventRecord& Event)
            {
                return Event.EventType ==
                        FName(
                            TEXT(
                                "BattlefieldMaterielRecovered"
                            )
                        ) &&
                    Event.SectorID == FriendlySectorID;
            }
        )
    );
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBHManpowerEconomyTest,
    "BrokenHorizon.PersistentWar.Population.ManpowerEconomy",
    EAutomationTestFlags::EditorContext |
        EAutomationTestFlags::EngineFilter
)

bool FBHManpowerEconomyTest::RunTest(
    const FString& Parameters
)
{
    UGameInstance* GameInstance =
        NewObject<UGameInstance>(GetTransientPackage());
    UBHWarSubsystem* War =
        NewObject<UBHWarSubsystem>(GameInstance);
    TestNotNull(TEXT("War subsystem is created"), War);

    if (!War)
    {
        return false;
    }

    War->ResetCampaign();
    const int32 InitialFriendlyReserve =
        War->GetFactionManpowerReserve(
            EBHWarFaction::Friendly
        );
    const int32 InitialEnemyReserve =
        War->GetFactionManpowerReserve(
            EBHWarFaction::Enemy
        );
    TestTrue(
        TEXT("Resistance begins with a finite reserve"),
        InitialFriendlyReserve > 0
    );
    TestTrue(
        TEXT("Occupation begins with a finite reserve"),
        InitialEnemyReserve > 0
    );
    TestTrue(
        TEXT("Friendly population generates recruits"),
        War->GetFactionRecruitmentPerTurn(
            EBHWarFaction::Friendly
        ) > 0.0f
    );
    TestTrue(
        TEXT("Enemy-controlled population generates replacements"),
        War->GetFactionRecruitmentPerTurn(
            EBHWarFaction::Enemy
        ) > 0.0f
    );

    const int32 HeadquartersGarrisonBefore =
        War->GetSectorGarrisonCount(
            TEXT("WesternFOB"),
            EBHWarFaction::Friendly
        );
    War->AdvanceWarTurn();
    TestTrue(
        TEXT("Automatic replacements consume friendly reserve"),
        War->GetFactionManpowerReserve(
            EBHWarFaction::Friendly
        ) < InitialFriendlyReserve
    );
    TestTrue(
        TEXT("Automatic replacements consume enemy reserve"),
        War->GetFactionManpowerReserve(
            EBHWarFaction::Enemy
        ) < InitialEnemyReserve
    );
    TestTrue(
        TEXT("Reserve manpower fills an understrength garrison"),
        War->GetSectorGarrisonCount(
            TEXT("WesternFOB"),
            EBHWarFaction::Friendly
        ) > HeadquartersGarrisonBefore
    );

    TestTrue(
        TEXT("Manpower state accepts valid persisted values"),
        War->RestoreManpowerState(
            7,
            11,
            0.25f,
            0.50f
        )
    );
    TestEqual(
        TEXT("Friendly reserve restores"),
        War->GetFactionManpowerReserve(
            EBHWarFaction::Friendly
        ),
        7
    );
    TestEqual(
        TEXT("Enemy reserve restores"),
        War->GetFactionManpowerReserve(
            EBHWarFaction::Enemy
        ),
        11
    );
    TestEqual(
        TEXT("Friendly recruitment progress restores"),
        War->GetFactionRecruitmentProgress(
            EBHWarFaction::Friendly
        ),
        0.25f
    );
    TestEqual(
        TEXT("Enemy recruitment progress restores"),
        War->GetFactionRecruitmentProgress(
            EBHWarFaction::Enemy
        ),
        0.50f
    );
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBHFieldOperativeRecruitmentTest,
    "BrokenHorizon.PersistentWar.Population.FieldOperativeRecruitment",
    EAutomationTestFlags::EditorContext |
        EAutomationTestFlags::EngineFilter
)

bool FBHFieldOperativeRecruitmentTest::RunTest(
    const FString& Parameters
)
{
    UGameInstance* GameInstance =
        NewObject<UGameInstance>(GetTransientPackage());
    UBHWarSubsystem* War =
        NewObject<UBHWarSubsystem>(GameInstance);
    TestNotNull(TEXT("War subsystem is created"), War);

    if (!War)
    {
        return false;
    }

    War->ResetCampaign();
    const FName HeadquartersID(TEXT("WesternFOB"));
    const int32 ManpowerBefore =
        War->GetFactionManpowerReserve(
            EBHWarFaction::Friendly
        );
    const float SupplyBefore =
        War->GetSectorState(HeadquartersID).Supply;
    const float RecruitSupplyCost =
        War->GetFieldOperativeSupplyCost();

    TestTrue(
        TEXT("Connected friendly headquarters can recruit"),
        War->CanRecruitFieldOperative(HeadquartersID)
    );
    TestTrue(
        TEXT("Field operative recruitment succeeds"),
        War->RecruitFieldOperative(HeadquartersID)
    );
    TestEqual(
        TEXT("Recruitment consumes one manpower"),
        War->GetFactionManpowerReserve(
            EBHWarFaction::Friendly
        ),
        ManpowerBefore - 1
    );
    TestEqual(
        TEXT("Recruitment consumes sector supply"),
        War->GetSectorState(HeadquartersID).Supply,
        SupplyBefore - RecruitSupplyCost
    );

    TestTrue(
        TEXT("Manpower can be exhausted for gate coverage"),
        War->RestoreManpowerState(
            0,
            War->GetFactionManpowerReserve(
                EBHWarFaction::Enemy
            ),
            War->GetFactionRecruitmentProgress(
                EBHWarFaction::Friendly
            ),
            War->GetFactionRecruitmentProgress(
                EBHWarFaction::Enemy
            )
        )
    );
    TestFalse(
        TEXT("Recruitment fails without friendly manpower"),
        War->CanRecruitFieldOperative(HeadquartersID)
    );
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBHMilitiaMobilizationTest,
    "BrokenHorizon.PersistentWar.Population.MilitiaMobilization",
    EAutomationTestFlags::EditorContext |
        EAutomationTestFlags::EngineFilter
)

bool FBHMilitiaMobilizationTest::RunTest(
    const FString& Parameters
)
{
    UGameInstance* GameInstance =
        NewObject<UGameInstance>(GetTransientPackage());
    UBHWarSubsystem* War =
        NewObject<UBHWarSubsystem>(GameInstance);
    TestNotNull(TEXT("War subsystem is created"), War);

    if (!War)
    {
        return false;
    }

    War->ResetCampaign();
    const FName HeadquartersID(TEXT("WesternFOB"));
    const FBHWarOperationForcePackage QuietHeadquartersDefense =
        BHWarOperationRules::BuildForcePackage(
            War,
            HeadquartersID,
            EBHWarPriorityType::Defend,
            HeadquartersID
        );
    const FBHWarSectorState Before =
        War->GetSectorState(HeadquartersID);
    const int32 FirstMobilization =
        War->GetSectorMilitiaMobilizationCount(
            HeadquartersID
        );
    const float FirstSupplyCost =
        War->GetSectorMilitiaMobilizationSupplyCost(
            HeadquartersID
        );

    TestEqual(
        TEXT("High local support rallies two militia"),
        FirstMobilization,
        2
    );
    TestEqual(
        TEXT("Militia mobilization has a strategic supply cost"),
        FirstSupplyCost,
        8.0f
    );
    TestTrue(
        TEXT("Friendly supplied headquarters can mobilize"),
        War->CanMobilizeSectorMilitia(HeadquartersID)
    );
    TestTrue(
        TEXT("Militia mobilization succeeds"),
        War->MobilizeSectorMilitia(HeadquartersID)
    );

    const FBHWarSectorState After =
        War->GetSectorState(HeadquartersID);
    TestEqual(
        TEXT("Mobilized locals join the persistent garrison"),
        After.FriendlyGarrison,
        Before.FriendlyGarrison + FirstMobilization
    );
    TestEqual(
        TEXT("Mobilized locals add territorial combat strength"),
        After.FriendlyStrength,
        Before.FriendlyStrength +
            (FirstMobilization * 2.0f)
    );
    TestEqual(
        TEXT("Mobilization consumes local supply"),
        After.Supply,
        Before.Supply - FirstSupplyCost
    );
    TestEqual(
        TEXT("Mobilization strains civilian support"),
        After.CivilianSupport,
        Before.CivilianSupport -
            (FirstMobilization * 4.0f)
    );
    TestEqual(
        TEXT("Arming local militia attracts enemy attention"),
        After.EnemyResponsePressure,
        Before.EnemyResponsePressure +
            (FirstMobilization * 12.5f)
    );

    const TArray<FBHWarEventRecord> Events =
        War->GetRecentWarEvents();
    TestTrue(
        TEXT("Mobilization enters campaign history"),
        Events.ContainsByPredicate(
            [HeadquartersID](const FBHWarEventRecord& Event)
            {
                return Event.EventType ==
                        FName(TEXT("MilitiaMobilized")) &&
                    Event.SectorID == HeadquartersID;
            }
        )
    );
    TestFalse(
        TEXT("Enemy sectors cannot mobilize friendly militia"),
        War->MobilizeSectorMilitia(
            FName(TEXT("KoronaCrossroads"))
        )
    );

    TestTrue(
        TEXT("Remaining headquarters capacity can be filled"),
        War->MobilizeSectorMilitia(HeadquartersID)
    );
    TestEqual(
        TEXT("Hunting pressure creates a defense operation"),
        War->GetPriorityType(),
        EBHWarPriorityType::Defend
    );
    TestEqual(
        TEXT("Enemy sweep targets the mobilized sector"),
        War->GetPrioritySectorID(),
        HeadquartersID
    );
    TestTrue(
        TEXT("Command identifies the counterinsurgency threat"),
        War->GetPriorityReasonText().ToString().Contains(
            TEXT("ENEMY SWEEP")
        )
    );
    TestTrue(
        TEXT("Enemy sweeps receive a distinct operation identity"),
        War->GetPriorityOperationTitle().ToString().Contains(
            TEXT("SAFEHOUSE")
        )
    );
    TestTrue(
        TEXT("Sweep briefing explains the threatened local network"),
        War->GetPriorityMissionBriefing().ToString().Contains(
            TEXT("militia cells")
        )
    );

    War->AdvanceWarTurn();
    TestEqual(
        TEXT("Sweep pressure persists through the next war turn"),
        War->GetSectorEnemyResponsePressure(HeadquartersID),
        42.0f
    );
    TestEqual(
        TEXT("Command retains the sweep defense long enough to deploy"),
        War->GetPrioritySectorID(),
        HeadquartersID
    );
    TestEqual(
        TEXT("Persisting sweep remains a defense operation"),
        War->GetPriorityType(),
        EBHWarPriorityType::Defend
    );
    const FBHWarOperationForcePackage SweepDefense =
        BHWarOperationRules::BuildForcePackage(
            War,
            HeadquartersID,
            EBHWarPriorityType::Defend,
            HeadquartersID
        );
    TestTrue(
        TEXT("Safehouse sweep adds a real field reinforcement wave"),
        SweepDefense.DefenseWaveCount >
            QuietHeadquartersDefense.DefenseWaveCount
    );
    TestEqual(
        TEXT("Tested Safehouse defense deploys three hostile waves"),
        SweepDefense.DefenseWaveCount,
        3
    );
    TestFalse(
        TEXT("Safehouse sweep identifies a hostile launch sector"),
        SweepDefense.EnemySourceSectorID.IsNone()
    );
    TestNotEqual(
        TEXT("Rear-area sweep is not attributed to its friendly target"),
        SweepDefense.EnemySourceSectorID,
        HeadquartersID
    );
    const FBHWarSectorState SweepSourceSector =
        War->GetSectorState(
            SweepDefense.EnemySourceSectorID
        );
    TestEqual(
        TEXT("Safehouse sweep source is enemy controlled"),
        SweepSourceSector.Owner,
        EBHWarFaction::Enemy
    );
    TestEqual(
        TEXT("Safehouse force package uses source-sector strength"),
        SweepDefense.EnemyStrength,
        SweepSourceSector.EnemyStrength
    );
    TestEqual(
        TEXT("Safehouse force package uses source-sector garrison"),
        SweepDefense.EnemyGarrisonCount,
        SweepSourceSector.EnemyGarrison
    );
    TestTrue(
        TEXT("Safehouse deployment does not exceed its source garrison"),
        SweepDefense.DefenseWaveCount *
            SweepDefense.DefenseEnemiesPerWave <=
            SweepDefense.EnemyGarrisonCount
    );

    TestTrue(
        TEXT("Successful Safehouse defense resolves"),
        War->ApplyMissionResult(
            HeadquartersID,
            EBHWarPriorityType::Defend,
            true
        )
    );
    TestEqual(
        TEXT("Breaking the sweep sharply lowers enemy response"),
        War->GetSectorEnemyResponsePressure(HeadquartersID),
        7.0f
    );
    TestFalse(
        TEXT("Successful Safehouse defense does not immediately repeat"),
        War->GetPrioritySectorID() == HeadquartersID &&
            War->GetPriorityType() ==
                EBHWarPriorityType::Defend
    );
    TestTrue(
        TEXT("Breaking the sweep enters campaign history"),
        War->GetRecentWarEvents().ContainsByPredicate(
            [HeadquartersID](const FBHWarEventRecord& Event)
            {
                return Event.EventType ==
                        FName(TEXT("CounterinsurgencyBroken")) &&
                    Event.SectorID == HeadquartersID;
            }
        )
    );
    TestFalse(
        TEXT("A full garrison cannot mobilize again"),
        War->CanMobilizeSectorMilitia(HeadquartersID)
    );
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBHCampaignResolutionTest,
    "BrokenHorizon.PersistentWar.Campaign.Resolution",
    EAutomationTestFlags::EditorContext |
        EAutomationTestFlags::EngineFilter
)

bool FBHCampaignResolutionTest::RunTest(
    const FString& Parameters
)
{
    UGameInstance* VictoryGameInstance =
        NewObject<UGameInstance>(GetTransientPackage());
    UBHWarSubsystem* VictoryWar =
        NewObject<UBHWarSubsystem>(VictoryGameInstance);
    TestNotNull(TEXT("Victory war subsystem is created"), VictoryWar);

    if (!VictoryWar)
    {
        return false;
    }

    VictoryWar->ResetCampaign();
    const TArray<FBHWarSectorState> InitialSectors =
        VictoryWar->GetSectorStates();

    for (const FBHWarSectorState& Sector : InitialSectors)
    {
        if (Sector.Owner == EBHWarFaction::Friendly)
        {
            continue;
        }

        TestTrue(
            *FString::Printf(
                TEXT("Liberation succeeds at %s"),
                *Sector.SectorID.ToString()
            ),
            VictoryWar->ApplyMissionResult(
                Sector.SectorID,
                EBHWarPriorityType::Attack,
                true
            )
        );
    }

    TestEqual(
        TEXT("Full territorial control wins the campaign"),
        VictoryWar->GetCampaignOutcome(),
        EBHWarCampaignOutcome::FriendlyVictory
    );
    TestTrue(
        TEXT("Victory resolves the campaign"),
        VictoryWar->IsCampaignResolved()
    );
    TestEqual(
        TEXT("Victory clears the strategic priority"),
        VictoryWar->GetPriorityType(),
        EBHWarPriorityType::None
    );
    TestFalse(
        TEXT("Resolved campaigns reject further operations"),
        VictoryWar->SetCommittedOperation(
            TEXT("WesternFOB"),
            EBHWarPriorityType::Defend
        )
    );
    const int32 VictoryTurn = VictoryWar->GetTurnNumber();
    VictoryWar->AdvanceWarTurn();
    TestEqual(
        TEXT("Victory freezes autonomous war turns"),
        VictoryWar->GetTurnNumber(),
        VictoryTurn
    );

    UGameInstance* DefeatGameInstance =
        NewObject<UGameInstance>(GetTransientPackage());
    UBHWarSubsystem* DefeatWar =
        NewObject<UBHWarSubsystem>(DefeatGameInstance);
    TestNotNull(TEXT("Defeat war subsystem is created"), DefeatWar);

    if (!DefeatWar)
    {
        return false;
    }

    DefeatWar->ResetCampaign();
    TestTrue(
        TEXT("Failed headquarters defense applies"),
        DefeatWar->ApplyMissionResult(
            TEXT("WesternFOB"),
            EBHWarPriorityType::Defend,
            false
        )
    );
    TestEqual(
        TEXT("Losing headquarters loses the campaign"),
        DefeatWar->GetCampaignOutcome(),
        EBHWarCampaignOutcome::EnemyVictory
    );
    TestTrue(
        TEXT("Defeat text identifies Western command loss"),
        DefeatWar->GetCampaignOutcomeText().ToString().Contains(
            TEXT("WESTERN COMMAND LOST")
        )
    );
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBHCommittedOperationDefersCampaignResolutionTest,
    "BrokenHorizon.PersistentWar.Campaign."
        "CommittedOperationDefersResolution",
    EAutomationTestFlags::EditorContext |
        EAutomationTestFlags::EngineFilter
)

bool FBHCommittedOperationDefersCampaignResolutionTest::RunTest(
    const FString& Parameters
)
{
    UGameInstance* GameInstance =
        NewObject<UGameInstance>(GetTransientPackage());
    UBHWarSubsystem* War =
        NewObject<UBHWarSubsystem>(GameInstance);
    TestNotNull(TEXT("War subsystem is created"), War);

    if (!War)
    {
        return false;
    }

    War->ResetCampaign();
    const FName CommittedSectorID(TEXT("WesternFOB"));
    TestTrue(
        TEXT("A final active defense can be committed"),
        War->SetCommittedOperation(
            CommittedSectorID,
            EBHWarPriorityType::Defend
        )
    );

    const TArray<FBHWarSectorState> InitialSectors =
        War->GetSectorStates();

    for (const FBHWarSectorState& Sector : InitialSectors)
    {
        if (Sector.Owner == EBHWarFaction::Friendly)
        {
            continue;
        }

        TestTrue(
            *FString::Printf(
                TEXT("Background liberation succeeds at %s"),
                *Sector.SectorID.ToString()
            ),
            War->ApplyMissionResult(
                Sector.SectorID,
                EBHWarPriorityType::Attack,
                true
            )
        );
    }

    TestEqual(
        TEXT("The campaign remains active until the player operation ends"),
        War->GetCampaignOutcome(),
        EBHWarCampaignOutcome::Ongoing
    );
    TestTrue(
        TEXT("The active operation remains committed"),
        War->HasCommittedOperation()
    );
    TestEqual(
        TEXT("The committed operation remains the strategic priority"),
        War->GetPrioritySectorID(),
        CommittedSectorID
    );

    TestTrue(
        TEXT("The player's final defense result applies"),
        War->ApplyMissionResult(
            CommittedSectorID,
            EBHWarPriorityType::Defend,
            true
        )
    );
    TestEqual(
        TEXT("Campaign victory resolves after the commitment is released"),
        War->GetCampaignOutcome(),
        EBHWarCampaignOutcome::FriendlyVictory
    );
    TestFalse(
        TEXT("The resolved operation no longer remains committed"),
        War->HasCommittedOperation()
    );
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBHOperationIntelligenceReadinessTest,
    "BrokenHorizon.PersistentWar.Intelligence.OperationReadiness",
    EAutomationTestFlags::EditorContext |
        EAutomationTestFlags::EngineFilter
)

bool FBHOperationIntelligenceReadinessTest::RunTest(
    const FString& Parameters
)
{
    UGameInstance* GameInstance =
        NewObject<UGameInstance>(GetTransientPackage());
    UBHWarSubsystem* War =
        NewObject<UBHWarSubsystem>(GameInstance);
    TestNotNull(TEXT("War subsystem is created"), War);

    if (!War)
    {
        return false;
    }

    War->ResetCampaign();
    FBHWarOperationForceTuning Tuning;
    Tuning.MaximumFriendlySupport = 3;
    const FBHWarOperationForcePackage LowIntelPackage =
        BHWarOperationRules::BuildForcePackage(
            War,
            TEXT("KoronaCrossroads"),
            EBHWarPriorityType::Attack,
            TEXT("DovrenVillage"),
            Tuning
        );

    TestEqual(
        TEXT("Operation preview captures low confidence"),
        LowIntelPackage.IntelConfidence,
        20.0f
    );
    TestTrue(
        TEXT("Field recon reaches confirmed confidence"),
        War->ReportSectorRecon(
            TEXT("KoronaCrossroads"),
            80.0f
        )
    );

    const FBHWarOperationForcePackage ConfirmedPackage =
        BHWarOperationRules::BuildForcePackage(
            War,
            TEXT("KoronaCrossroads"),
            EBHWarPriorityType::Attack,
            TEXT("DovrenVillage"),
            Tuning
        );

    TestEqual(
        TEXT("Operation preview captures confirmed confidence"),
        ConfirmedPackage.IntelConfidence,
        100.0f
    );
    TestTrue(
        TEXT("Confirmed intel reduces the opening hostile force"),
        ConfirmedPackage.AttackEnemyCount <
            LowIntelPackage.AttackEnemyCount
    );
    TestTrue(
        TEXT("Confirmed intel reduces surprise reinforcements"),
        ConfirmedPackage.AttackReinforcementWaveCount <
            LowIntelPackage.AttackReinforcementWaveCount
    );
    TestTrue(
        TEXT("Confirmed intel improves friendly support"),
        ConfirmedPackage.FriendlySupportCount >
            LowIntelPackage.FriendlySupportCount
    );
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBHLogisticsRaidTest,
    "BrokenHorizon.PersistentWar.Operations.LogisticsRaid",
    EAutomationTestFlags::EditorContext |
        EAutomationTestFlags::EngineFilter
)

bool FBHLogisticsRaidTest::RunTest(
    const FString& Parameters
)
{
    TestEqual(
        TEXT("Zero-casualty raid starts clean"),
        BHWarOperationRules::ClassifyRaidOperationalSignature(
            0,
            0
        ),
        EBHRaidOperationalSignature::Clean
    );
    TestEqual(
        TEXT("Two hostile casualties make the raid contested"),
        BHWarOperationRules::ClassifyRaidOperationalSignature(
            2,
            0
        ),
        EBHRaidOperationalSignature::Contested
    );
    TestEqual(
        TEXT("Four hostile casualties make the raid loud"),
        BHWarOperationRules::ClassifyRaidOperationalSignature(
            4,
            0
        ),
        EBHRaidOperationalSignature::Loud
    );
    TestEqual(
        TEXT("Any support casualty makes the raid loud"),
        BHWarOperationRules::ClassifyRaidOperationalSignature(
            0,
            1
        ),
        EBHRaidOperationalSignature::Loud
    );
    TestEqual(
        TEXT("Negative casualty input is safely clamped"),
        BHWarOperationRules::ClassifyRaidOperationalSignature(
            -3,
            -2
        ),
        EBHRaidOperationalSignature::Clean
    );
    TestEqual(
        TEXT("Detection makes a zero-casualty raid contested"),
        BHWarOperationRules::ClassifyRaidOperationalSignature(
            0,
            0,
            true
        ),
        EBHRaidOperationalSignature::Contested
    );
    TestEqual(
        TEXT("A detected high-casualty raid remains loud"),
        BHWarOperationRules::ClassifyRaidOperationalSignature(
            4,
            0,
            true
        ),
        EBHRaidOperationalSignature::Loud
    );
    TestEqual(
        TEXT("Undetected sabotage reduces the immediate reaction force"),
        BHWarOperationRules::CalculateRaidReactionForceCount(
            1,
            3,
            false
        ),
        2
    );
    TestEqual(
        TEXT("Compromised sabotage receives the full reaction force"),
        BHWarOperationRules::CalculateRaidReactionForceCount(
            1,
            3,
            true
        ),
        3
    );
    TestEqual(
        TEXT("A minimal covert raid can avoid a fresh response team"),
        BHWarOperationRules::CalculateRaidReactionForceCount(
            1,
            1,
            false
        ),
        0
    );
    TestFalse(
        TEXT("An advancing enemy cannot be routed by distance alone"),
        BHWarOperationRules::IsEnemyRoutedFromOperation(
            false,
            2000.0f,
            1400.0f
        )
    );
    TestFalse(
        TEXT("A retreating enemy still inside the perimeter remains engaged"),
        BHWarOperationRules::IsEnemyRoutedFromOperation(
            true,
            1399.0f,
            1400.0f
        )
    );
    TestTrue(
        TEXT("A retreating enemy clears the operation at the boundary"),
        BHWarOperationRules::IsEnemyRoutedFromOperation(
            true,
            1400.0f,
            1400.0f
        )
    );
    TestTrue(
        TEXT("A living possessed combatant can hold an operation open"),
        BHWarOperationRules::IsOperationCombatantReady(
            true,
            true
        )
    );
    TestFalse(
        TEXT("An uncontrolled combatant cannot deadlock an operation"),
        BHWarOperationRules::IsOperationCombatantReady(
            true,
            false
        )
    );
    TestFalse(
        TEXT("A dead combatant is never operation-ready"),
        BHWarOperationRules::IsOperationCombatantReady(
            false,
            true
        )
    );
    TestFalse(
        TEXT("Raid remains active inside the exfiltration radius"),
        BHWarOperationRules::IsRaidExfiltrationComplete(
            2999.0f,
            3000.0f
        )
    );
    TestTrue(
        TEXT("Raid completes at the exfiltration boundary"),
        BHWarOperationRules::IsRaidExfiltrationComplete(
            3000.0f,
            3000.0f
        )
    );
    TestTrue(
        TEXT("Raid completes beyond the exfiltration boundary"),
        BHWarOperationRules::IsRaidExfiltrationComplete(
            3500.0f,
            3000.0f
        )
    );

    UGameInstance* GameInstance =
        NewObject<UGameInstance>(GetTransientPackage());
    UBHWarSubsystem* War =
        NewObject<UBHWarSubsystem>(GameInstance);
    TestNotNull(TEXT("War subsystem is created"), War);

    if (!War)
    {
        return false;
    }

    War->ResetCampaign();
    const FName RaidSectorID(TEXT("KoronaCrossroads"));
    const FBHWarSectorState Before =
        War->GetSectorState(RaidSectorID);

    TestTrue(
        TEXT("Enemy logistics sector offers a raid"),
        War->IsViableOperation(
            RaidSectorID,
            EBHWarPriorityType::Raid
        )
    );
    TestFalse(
        TEXT("Friendly territory does not offer a hostile raid"),
        War->IsViableOperation(
            TEXT("WesternFOB"),
            EBHWarPriorityType::Raid
        )
    );

    const FBHWarOperationForcePackage RaidPackage =
        BHWarOperationRules::BuildForcePackage(
            War,
            RaidSectorID,
            EBHWarPriorityType::Raid,
            War->GetOperationSupplySource(
                RaidSectorID,
                EBHWarPriorityType::Raid
            )
        );
    TestTrue(
        TEXT("Raid force remains a small asymmetric package"),
        RaidPackage.AttackEnemyCount <= 4 &&
            RaidPackage.AttackReinforcementWaveCount <= 1 &&
            RaidPackage.FriendlySupportCount <= 1
    );
    TestEqual(
        TEXT("Raid does not reserve an occupation garrison"),
        RaidPackage.OccupationTransferCount,
        0
    );
    TestTrue(
        TEXT("Successful raid applies its strategic result"),
        War->ApplyMissionResult(
            RaidSectorID,
            EBHWarPriorityType::Raid,
            true
        )
    );

    const FBHWarSectorState After =
        War->GetSectorState(RaidSectorID);
    TestEqual(
        TEXT("Raid does not capture the target"),
        After.Owner,
        EBHWarFaction::Enemy
    );
    TestTrue(
        TEXT("Raid disrupts enemy supply"),
        After.Supply < Before.Supply
    );
    TestTrue(
        TEXT("Raid weakens enemy strength"),
        After.EnemyStrength < Before.EnemyStrength
    );
    TestTrue(
        TEXT("Raid raises enemy response"),
        After.EnemyResponsePressure >
            Before.EnemyResponsePressure
    );
    TestTrue(
        TEXT("Raid enters campaign history"),
        War->GetRecentWarEvents().ContainsByPredicate(
            [RaidSectorID](const FBHWarEventRecord& Event)
            {
                return Event.SectorID == RaidSectorID &&
                    Event.EventType ==
                        FName(TEXT("LogisticsRaidSucceeded"));
            }
        )
    );

    TestEqual(
        TEXT("Minimal casualties produce a clean raid signature"),
        War->ApplyRaidOperationalSignature(
            RaidSectorID,
            1,
            0
        ),
        EBHRaidOperationalSignature::Clean
    );
    const FBHWarSectorState CleanRaid =
        War->GetSectorState(RaidSectorID);
    TestTrue(
        TEXT("Clean raid lowers post-operation enemy attention"),
        CleanRaid.EnemyResponsePressure <
            After.EnemyResponsePressure
    );
    TestTrue(
        TEXT("Clean raid earns additional civilian support"),
        CleanRaid.CivilianSupport >
            After.CivilianSupport
    );
    TestTrue(
        TEXT("Clean signature enters campaign history"),
        War->GetRecentWarEvents().ContainsByPredicate(
            [RaidSectorID](const FBHWarEventRecord& Event)
            {
                return Event.SectorID == RaidSectorID &&
                    Event.EventType ==
                        FName(TEXT("CleanRaidSignature"));
            }
        )
    );

    War->ResetCampaign();
    TestTrue(
        TEXT("Detected raid baseline result applies"),
        War->ApplyMissionResult(
            RaidSectorID,
            EBHWarPriorityType::Raid,
            true
        )
    );
    const FBHWarSectorState BeforeDetectedSignature =
        War->GetSectorState(RaidSectorID);
    TestEqual(
        TEXT("Detection prevents a clean raid signature"),
        War->ApplyRaidOperationalSignature(
            RaidSectorID,
            0,
            0,
            true,
            true
        ),
        EBHRaidOperationalSignature::Contested
    );
    const FBHWarSectorState DetectedRaid =
        War->GetSectorState(RaidSectorID);
    TestEqual(
        TEXT("Detected low-casualty raid receives no clean attention bonus"),
        DetectedRaid.EnemyResponsePressure,
        BeforeDetectedSignature.EnemyResponsePressure
    );
    TestEqual(
        TEXT("Detected low-casualty raid receives no clean support bonus"),
        DetectedRaid.CivilianSupport,
        BeforeDetectedSignature.CivilianSupport
    );

    War->ResetCampaign();
    TestTrue(
        TEXT("Loud raid baseline result applies"),
        War->ApplyMissionResult(
            RaidSectorID,
            EBHWarPriorityType::Raid,
            true
        )
    );
    const FBHWarSectorState BeforeLoudSignature =
        War->GetSectorState(RaidSectorID);
    TestEqual(
        TEXT("Heavy casualties produce a loud raid signature"),
        War->ApplyRaidOperationalSignature(
            RaidSectorID,
            4,
            0
        ),
        EBHRaidOperationalSignature::Loud
    );
    const FBHWarSectorState LoudRaid =
        War->GetSectorState(RaidSectorID);
    TestTrue(
        TEXT("Loud raid increases enemy attention"),
        LoudRaid.EnemyResponsePressure >
            BeforeLoudSignature.EnemyResponsePressure
    );
    TestTrue(
        TEXT("Loud raid costs civilian support"),
        LoudRaid.CivilianSupport <
            BeforeLoudSignature.CivilianSupport
    );

    War->ResetCampaign();
    TestTrue(
        TEXT("Failed raid baseline result applies"),
        War->ApplyMissionResult(
            RaidSectorID,
            EBHWarPriorityType::Raid,
            false
        )
    );
    const FBHWarSectorState BeforeCleanFailureSignature =
        War->GetSectorState(RaidSectorID);
    TestEqual(
        TEXT("Clean withdrawal retains a clean signature"),
        War->ApplyRaidOperationalSignature(
            RaidSectorID,
            0,
            0,
            false
        ),
        EBHRaidOperationalSignature::Clean
    );
    const FBHWarSectorState CleanFailure =
        War->GetSectorState(RaidSectorID);
    TestEqual(
        TEXT("Clean withdrawal does not reduce enemy attention"),
        CleanFailure.EnemyResponsePressure,
        BeforeCleanFailureSignature.EnemyResponsePressure
    );
    TestEqual(
        TEXT("Clean withdrawal does not earn civilian support"),
        CleanFailure.CivilianSupport,
        BeforeCleanFailureSignature.CivilianSupport
    );
    TestEqual(
        TEXT("Loud failed raid retains a loud signature"),
        War->ApplyRaidOperationalSignature(
            RaidSectorID,
            4,
            0,
            false
        ),
        EBHRaidOperationalSignature::Loud
    );
    const FBHWarSectorState LoudFailure =
        War->GetSectorState(RaidSectorID);
    TestTrue(
        TEXT("Loud failed raid further increases enemy attention"),
        LoudFailure.EnemyResponsePressure >
            CleanFailure.EnemyResponsePressure
    );
    TestTrue(
        TEXT("Loud failed raid still costs civilian support"),
        LoudFailure.CivilianSupport <
            CleanFailure.CivilianSupport
    );
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBHEnemyPatternAdaptationTest,
    "BrokenHorizon.PersistentWar.Response.PatternAdaptation",
    EAutomationTestFlags::EditorContext |
        EAutomationTestFlags::EngineFilter
)

bool FBHEnemyPatternAdaptationTest::RunTest(
    const FString& Parameters
)
{
    UGameInstance* GameInstance =
        NewObject<UGameInstance>(GetTransientPackage());
    UBHWarSubsystem* War =
        NewObject<UBHWarSubsystem>(GameInstance);
    TestNotNull(TEXT("War subsystem is created"), War);

    if (!War)
    {
        return false;
    }

    War->ResetCampaign();
    const FName SectorID(TEXT("KoronaCrossroads"));
    const FName SupplySource =
        War->GetOperationSupplySource(
            SectorID,
            EBHWarPriorityType::Raid
        );
    const FBHWarOperationForcePackage Baseline =
        BHWarOperationRules::BuildForcePackage(
            War,
            SectorID,
            EBHWarPriorityType::Raid,
            SupplySource
        );
    TestEqual(
        TEXT("Fresh sector has no pattern preparation"),
        Baseline.EnemyPatternPreparationLevel,
        0
    );

    TestTrue(
        TEXT("First raid result is accepted"),
        War->ApplyMissionResult(
            SectorID,
            EBHWarPriorityType::Raid,
            true
        )
    );
    const FBHWarSectorState First =
        War->GetSectorState(SectorID);
    TestEqual(
        TEXT("Enemy anticipates the observed raid"),
        First.AnticipatedOperationType,
        EBHWarPriorityType::Raid
    );
    TestEqual(
        TEXT("First raid establishes one pattern read"),
        First.RepeatedOperationCount,
        1
    );
    TestEqual(
        TEXT("Repeating the raid activates counter level one"),
        BHWarOperationRules::BuildForcePackage(
            War,
            SectorID,
            EBHWarPriorityType::Raid,
            SupplySource
        ).EnemyPatternPreparationLevel,
        1
    );
    TestEqual(
        TEXT("Changing tactics breaks the current prediction"),
        BHWarOperationRules::BuildForcePackage(
            War,
            SectorID,
            EBHWarPriorityType::Attack,
            SupplySource
        ).EnemyPatternPreparationLevel,
        0
    );

    TestTrue(
        TEXT("Second raid result is accepted"),
        War->ApplyMissionResult(
            SectorID,
            EBHWarPriorityType::Raid,
            true
        )
    );
    const FBHWarSectorState Second =
        War->GetSectorState(SectorID);
    TestEqual(
        TEXT("Repeated raid history accumulates"),
        Second.RepeatedOperationCount,
        2
    );
    TestEqual(
        TEXT("Repeated raids reach maximum preparation"),
        BHWarOperationRules::BuildForcePackage(
            War,
            SectorID,
            EBHWarPriorityType::Raid,
            SupplySource
        ).EnemyPatternPreparationLevel,
        2
    );
    TestTrue(
        TEXT("Strategic summary exposes the pattern read"),
        War->GetSectorEnemyAdaptationSummary(
            SectorID
        ).ToString().Contains(TEXT("COUNTER READY RAID"))
    );
    TestTrue(
        TEXT("Adaptation enters campaign history"),
        War->GetRecentWarEvents().ContainsByPredicate(
            [SectorID](const FBHWarEventRecord& Event)
            {
                return Event.SectorID == SectorID &&
                    Event.EventType ==
                        FName(TEXT("EnemyPatternAdapted"));
            }
        )
    );

    TestTrue(
        TEXT("Different operation resets the enemy read"),
        War->ApplyMissionResult(
            SectorID,
            EBHWarPriorityType::Attack,
            false
        )
    );
    const FBHWarSectorState Changed =
        War->GetSectorState(SectorID);
    TestEqual(
        TEXT("Enemy anticipates the changed operation"),
        Changed.AnticipatedOperationType,
        EBHWarPriorityType::Attack
    );
    TestEqual(
        TEXT("Tactic change resets repetition"),
        Changed.RepeatedOperationCount,
        1
    );
    TestEqual(
        TEXT("Old raid counter no longer applies"),
        BHWarOperationRules::BuildForcePackage(
            War,
            SectorID,
            EBHWarPriorityType::Raid,
            SupplySource
        ).EnemyPatternPreparationLevel,
        0
    );
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBHOperationSelectionTest,
    "BrokenHorizon.PersistentWar.Operations.PlayerSelection",
    EAutomationTestFlags::EditorContext |
        EAutomationTestFlags::EngineFilter
)

bool FBHOperationSelectionTest::RunTest(
    const FString& Parameters
)
{
    UGameInstance* GameInstance =
        NewObject<UGameInstance>(GetTransientPackage());
    UBHWarSubsystem* War =
        NewObject<UBHWarSubsystem>(GameInstance);
    TestNotNull(TEXT("War subsystem is created"), War);

    if (!War)
    {
        return false;
    }

    War->ResetCampaign();
    int32 ViableOperationCount = 0;
    FName AlternativeSectorID = NAME_None;
    EBHWarPriorityType AlternativeType =
        EBHWarPriorityType::None;

    for (const FBHWarSectorState& Sector :
        War->GetSectorStates())
    {
        const EBHWarPriorityType OperationType =
            Sector.Owner == EBHWarFaction::Friendly
                ? EBHWarPriorityType::Defend
                : EBHWarPriorityType::Attack;

        if (!War->IsViableOperation(
                Sector.SectorID,
                OperationType
            ))
        {
            continue;
        }

        ++ViableOperationCount;

        if (Sector.SectorID != War->GetPrioritySectorID())
        {
            AlternativeSectorID = Sector.SectorID;
            AlternativeType = OperationType;
        }
    }

    TestTrue(
        TEXT("Campaign offers more than the recommended operation"),
        ViableOperationCount > 1
    );
    TestFalse(
        TEXT("An alternative operation is available"),
        AlternativeSectorID.IsNone()
    );

    if (AlternativeSectorID.IsNone())
    {
        return false;
    }

    const FName SupplySource =
        War->GetOperationSupplySource(
            AlternativeSectorID,
            AlternativeType
        );
    TestFalse(
        TEXT("Alternative has a friendly staging route"),
        SupplySource.IsNone()
    );
    TestTrue(
        TEXT("Alternative can be funded"),
        War->CanFundOperation(
            AlternativeSectorID,
            AlternativeType
        )
    );

    const float SupplyBefore =
        War->GetSectorState(SupplySource).Supply;
    TestTrue(
        TEXT("Alternative operation consumes its own route supply"),
        War->ConsumeOperationSupply(
            AlternativeSectorID,
            AlternativeType
        )
    );
    TestEqual(
        TEXT("Selected operation deducts the campaign supply cost"),
        War->GetSectorState(SupplySource).Supply,
        SupplyBefore - War->GetPriorityOperationSupplyCost()
    );

    const FName DefenseSectorID(TEXT("WesternFOB"));
    const FName DefenseSupplySource =
        War->GetOperationSupplySource(
            DefenseSectorID,
            EBHWarPriorityType::Defend
        );
    const FName DefenseEnemySource =
        War->GetOperationEnemySource(
            DefenseSectorID,
            EBHWarPriorityType::Defend
        );
    TestTrue(
        TEXT("Defense operation can be committed"),
        War->SetCommittedOperation(
            DefenseSectorID,
            EBHWarPriorityType::Defend
        )
    );
    TestEqual(
        TEXT("Committed defense preserves its staging sector"),
        War->GetCommittedOperationSupplySourceSectorID(),
        DefenseSupplySource
    );
    TestEqual(
        TEXT("Committed defense preserves its hostile source"),
        War->GetCommittedOperationEnemySourceSectorID(),
        DefenseEnemySource
    );
    TestTrue(
        TEXT("Committed defense locks its target"),
        War->IsOperationSectorLocked(DefenseSectorID)
    );
    TestTrue(
        TEXT("Committed defense locks its staging source"),
        War->IsOperationSectorLocked(DefenseSupplySource)
    );
    TestTrue(
        TEXT("Committed defense locks its hostile source"),
        War->IsOperationSectorLocked(DefenseEnemySource)
    );
    TestTrue(
        TEXT("Restoring the same committed defense is idempotent"),
        War->SetCommittedOperation(
            DefenseSectorID,
            EBHWarPriorityType::Defend
        )
    );
    TestFalse(
        TEXT("A conflicting operation is not viable during deployment"),
        War->IsViableOperation(
            DefenseEnemySource,
            EBHWarPriorityType::Attack
        )
    );
    TestFalse(
        TEXT("A conflicting operation cannot replace active deployment"),
        War->SetCommittedOperation(
            DefenseEnemySource,
            EBHWarPriorityType::Attack
        )
    );
    TestEqual(
        TEXT("Rejected conflict preserves the original operation"),
        War->GetCommittedOperationSectorID(),
        DefenseSectorID
    );
    War->ClearCommittedOperation();
    TestFalse(
        TEXT("Clearing an operation releases its hostile source"),
        War->IsOperationSectorLocked(DefenseEnemySource)
    );

    const FName CaptureSectorID(TEXT("KoronaCrossroads"));
    const FName CaptureStagingSectorID =
        War->GetOperationSupplySource(
            CaptureSectorID,
            EBHWarPriorityType::Attack
        );
    TestEqual(
        TEXT("Attack mobilization reinforces its staging sector"),
        BHWarOperationRules::GetMobilizationSectorID(
            War,
            CaptureSectorID,
            EBHWarPriorityType::Attack
        ),
        CaptureStagingSectorID
    );
    TestEqual(
        TEXT("Defense mobilization reinforces its target sector"),
        BHWarOperationRules::GetMobilizationSectorID(
            War,
            DefenseSectorID,
            EBHWarPriorityType::Defend
        ),
        DefenseSectorID
    );
    TestTrue(
        TEXT("Invalid operation has no mobilization sector"),
        BHWarOperationRules::GetMobilizationSectorID(
            War,
            CaptureSectorID,
            EBHWarPriorityType::None
        ).IsNone()
    );
    const int32 StagingGarrisonBeforeCapture =
        War->GetSectorGarrisonCount(
            CaptureStagingSectorID,
            EBHWarFaction::Friendly
        );
    const int32 LocalFriendlyGarrisonBeforeCapture =
        War->GetSectorGarrisonCount(
            CaptureSectorID,
            EBHWarFaction::Friendly
        );
    const float StagingEffectiveStrengthBeforeCapture =
        War->GetSectorEffectiveStrength(
            CaptureStagingSectorID,
            EBHWarFaction::Friendly
        );
    const FBHWarOperationForcePackage CapturePreview =
        BHWarOperationRules::BuildForcePackage(
            War,
            CaptureSectorID,
            EBHWarPriorityType::Attack,
            CaptureStagingSectorID
        );
    TestEqual(
        TEXT("Attack preview identifies desired occupation force"),
        CapturePreview.DesiredOccupationGarrisonCount,
        2
    );
    TestEqual(
        TEXT("Attack preview counts local resistance"),
        CapturePreview.OccupationGarrisonCount,
        2
    );
    TestEqual(
        TEXT("Attack preview forecasts the staging transfer"),
        CapturePreview.OccupationTransferCount,
        1
    );
    TestEqual(
        TEXT("Attack preview forecasts remaining rear garrison"),
        CapturePreview.RemainingStagingGarrisonCount,
        StagingGarrisonBeforeCapture - 1
    );
    TestFalse(
        TEXT("Funded attack has no occupation shortfall"),
        CapturePreview.bOccupationGarrisonShortfall
    );
    TestTrue(
        TEXT("Attack operation can be committed for occupation"),
        War->SetCommittedOperation(
            CaptureSectorID,
            EBHWarPriorityType::Attack
        )
    );
    TestTrue(
        TEXT("Successful attack resolves the captured sector"),
        War->ApplyMissionResult(
            CaptureSectorID,
            EBHWarPriorityType::Attack,
            true
        )
    );
    const int32 CapturedSectorGarrison =
        War->GetSectorGarrisonCount(
            CaptureSectorID,
            EBHWarFaction::Friendly
        );
    TestEqual(
        TEXT("Captured sector receives an occupation garrison"),
        CapturedSectorGarrison,
        2
    );
    TestEqual(
        TEXT("Occupation troops leave the staging garrison"),
        War->GetSectorGarrisonCount(
            CaptureStagingSectorID,
            EBHWarFaction::Friendly
        ),
        StagingGarrisonBeforeCapture -
            (
                CapturedSectorGarrison -
                LocalFriendlyGarrisonBeforeCapture
            )
    );
    TestEqual(
        TEXT("Occupation transfer conserves friendly manpower"),
        War->GetSectorGarrisonCount(
            CaptureStagingSectorID,
            EBHWarFaction::Friendly
        ) + CapturedSectorGarrison,
        StagingGarrisonBeforeCapture +
            LocalFriendlyGarrisonBeforeCapture
    );
    const float ExpectedStagingStrengthReduction =
        (
            CapturedSectorGarrison -
            LocalFriendlyGarrisonBeforeCapture
        ) *
        4.0f *
        War->GetSectorCombatSupplyFactor(
            CaptureStagingSectorID
        );
    TestTrue(
        TEXT("Transferred garrison reduces rear combat power"),
        FMath::IsNearlyEqual(
            StagingEffectiveStrengthBeforeCapture -
                War->GetSectorEffectiveStrength(
                    CaptureStagingSectorID,
                    EBHWarFaction::Friendly
                ),
            ExpectedStagingStrengthReduction,
            0.01f
        )
    );

    UGameInstance* DefenseGameInstance =
        NewObject<UGameInstance>(GetTransientPackage());
    UBHWarSubsystem* DefenseWar =
        NewObject<UBHWarSubsystem>(DefenseGameInstance);
    DefenseWar->ResetCampaign();
    const FName LostSectorID(TEXT("DovrenVillage"));
    const FName EnemyOccupationSourceID =
        DefenseWar->GetOperationEnemySource(
            LostSectorID,
            EBHWarPriorityType::Defend
        );
    const int32 EnemySourceGarrisonBefore =
        DefenseWar->GetSectorGarrisonCount(
            EnemyOccupationSourceID,
            EBHWarFaction::Enemy
        );
    const int32 LocalEnemyGarrisonBefore =
        DefenseWar->GetSectorGarrisonCount(
            LostSectorID,
            EBHWarFaction::Enemy
        );
    TestTrue(
        TEXT("Defense can be committed for enemy occupation test"),
        DefenseWar->SetCommittedOperation(
            LostSectorID,
            EBHWarPriorityType::Defend
        )
    );
    TestTrue(
        TEXT("Failed defense resolves enemy occupation"),
        DefenseWar->ApplyMissionResult(
            LostSectorID,
            EBHWarPriorityType::Defend,
            false
        )
    );
    const int32 LostSectorEnemyGarrison =
        DefenseWar->GetSectorGarrisonCount(
            LostSectorID,
            EBHWarFaction::Enemy
        );
    TestEqual(
        TEXT("Lost sector receives an enemy occupation garrison"),
        LostSectorEnemyGarrison,
        2
    );
    TestEqual(
        TEXT("Enemy occupation conserves hostile manpower"),
        DefenseWar->GetSectorGarrisonCount(
            EnemyOccupationSourceID,
            EBHWarFaction::Enemy
        ) + LostSectorEnemyGarrison,
        EnemySourceGarrisonBefore +
            LocalEnemyGarrisonBefore
    );

    UGameInstance* AmbientGameInstance =
        NewObject<UGameInstance>(GetTransientPackage());
    UBHWarSubsystem* AmbientWar =
        NewObject<UBHWarSubsystem>(AmbientGameInstance);
    AmbientWar->ResetCampaign();
    TArray<FBHWarSectorState> AmbientStates =
        AmbientWar->GetSectorStates();

    for (FBHWarSectorState& Sector : AmbientStates)
    {
        if (Sector.SectorID == TEXT("NorthPass"))
        {
            Sector.Owner = EBHWarFaction::Neutral;
            Sector.FriendlyStrength = 100.0f;
            Sector.EnemyStrength = 0.0f;
            Sector.FriendlyGarrison = 0;
            Sector.EnemyGarrison = 0;
            break;
        }
    }

    TestTrue(
        TEXT("Ambient capture fixture restores"),
        AmbientWar->RestoreWarState(
            AmbientStates,
            {},
            {},
            0,
            0.0f,
            20
        )
    );
    AmbientWar->AdvanceWarTurn();
    TestEqual(
        TEXT("Ambient capture changes sector owner"),
        AmbientWar->GetSectorState(TEXT("NorthPass")).Owner,
        EBHWarFaction::Friendly
    );
    TestEqual(
        TEXT("Ambient capture does not create a free garrison"),
        AmbientWar->GetSectorGarrisonCount(
            TEXT("NorthPass"),
            EBHWarFaction::Friendly
        ),
        0
    );

    UGameInstance* RiskGameInstance =
        NewObject<UGameInstance>(GetTransientPackage());
    UBHWarSubsystem* RiskWar =
        NewObject<UBHWarSubsystem>(RiskGameInstance);
    RiskWar->ResetCampaign();
    TArray<FBHWarSectorState> RiskStates =
        RiskWar->GetSectorStates();

    for (FBHWarSectorState& Sector : RiskStates)
    {
        if (Sector.Owner == EBHWarFaction::Friendly ||
            Sector.SectorID == CaptureSectorID)
        {
            Sector.FriendlyGarrison = 0;
        }
    }

    TestTrue(
        TEXT("Occupation-risk fixture restores"),
        RiskWar->RestoreWarState(
            RiskStates,
            {},
            {},
            0,
            0.0f,
            20
        )
    );
    const FName RiskSupplySourceID =
        RiskWar->GetOperationSupplySource(
            CaptureSectorID,
            EBHWarPriorityType::Attack
        );
    const FBHWarOperationForcePackage RiskPreview =
        BHWarOperationRules::BuildForcePackage(
            RiskWar,
            CaptureSectorID,
            EBHWarPriorityType::Attack,
            RiskSupplySourceID
        );
    TestTrue(
        TEXT("Attack preview warns about an occupation shortfall"),
        RiskPreview.bOccupationGarrisonShortfall
    );
    TestEqual(
        TEXT("Shortfall preview does not invent occupation troops"),
        RiskPreview.OccupationGarrisonCount,
        0
    );

    UGameInstance* RedeploymentGameInstance =
        NewObject<UGameInstance>(GetTransientPackage());
    UBHWarSubsystem* RedeploymentWar =
        NewObject<UBHWarSubsystem>(RedeploymentGameInstance);
    RedeploymentWar->ResetCampaign();
    const FName RedeploymentDestinationID(TEXT("WesternFOB"));
    const FName RedeploymentSourceID =
        RedeploymentWar->GetSectorGarrisonRedeploymentSource(
            RedeploymentDestinationID
        );
    TestEqual(
        TEXT("Rear garrison is selected as a redeployment source"),
        RedeploymentSourceID,
        FName(TEXT("DovrenVillage"))
    );
    const int32 RedeploymentCount =
        RedeploymentWar->GetSectorGarrisonRedeploymentCount(
            RedeploymentDestinationID
        );
    const float RedeploymentSupplyCost =
        RedeploymentWar
            ->GetSectorGarrisonRedeploymentSupplyCost(
                RedeploymentDestinationID
            );
    const int32 RedeploymentSourceGarrisonBefore =
        RedeploymentWar->GetSectorGarrisonCount(
            RedeploymentSourceID,
            EBHWarFaction::Friendly
        );
    const int32 RedeploymentDestinationGarrisonBefore =
        RedeploymentWar->GetSectorGarrisonCount(
            RedeploymentDestinationID,
            EBHWarFaction::Friendly
        );
    const float RedeploymentSourceSupplyBefore =
        RedeploymentWar->GetSectorState(
            RedeploymentSourceID
        ).Supply;
    TestEqual(
        TEXT("Redeployment moves a bounded two-person element"),
        RedeploymentCount,
        2
    );
    TestEqual(
        TEXT("Redeployment charges supply per transferred troop"),
        RedeploymentSupplyCost,
        6.0f
    );
    TestTrue(
        TEXT("Connected rear troops can reinforce the destination"),
        RedeploymentWar->CanRedeploySectorGarrison(
            RedeploymentDestinationID
        )
    );
    TestTrue(
        TEXT("Garrison redeployment succeeds"),
        RedeploymentWar->RedeploySectorGarrison(
            RedeploymentDestinationID
        )
    );
    TestEqual(
        TEXT("Redeployment removes troops from its source"),
        RedeploymentWar->GetSectorGarrisonCount(
            RedeploymentSourceID,
            EBHWarFaction::Friendly
        ),
        RedeploymentSourceGarrisonBefore - RedeploymentCount
    );
    TestEqual(
        TEXT("Redeployment does not teleport troops to its destination"),
        RedeploymentWar->GetSectorGarrisonCount(
            RedeploymentDestinationID,
            EBHWarFaction::Friendly
        ),
        RedeploymentDestinationGarrisonBefore
    );
    TestEqual(
        TEXT("Redeployment creates an inbound troop movement"),
        RedeploymentWar->GetIncomingGarrisonTransferCount(
            RedeploymentDestinationID
        ),
        RedeploymentCount
    );
    TestEqual(
        TEXT("Adjacent troop movement arrives next turn"),
        RedeploymentWar->GetIncomingGarrisonTransferTurns(
            RedeploymentDestinationID
        ),
        1
    );
    TestEqual(
        TEXT("Redeployment conserves manpower including troops in transit"),
        RedeploymentWar->GetSectorGarrisonCount(
            RedeploymentSourceID,
            EBHWarFaction::Friendly
        ) +
            RedeploymentWar->GetSectorGarrisonCount(
                RedeploymentDestinationID,
                EBHWarFaction::Friendly
            ) +
            RedeploymentWar->GetIncomingGarrisonTransferCount(
                RedeploymentDestinationID
            ),
        RedeploymentSourceGarrisonBefore +
            RedeploymentDestinationGarrisonBefore
    );
    TestEqual(
        TEXT("Redeployment consumes source-sector supply"),
        RedeploymentWar->GetSectorState(
            RedeploymentSourceID
        ).Supply,
        RedeploymentSourceSupplyBefore -
            RedeploymentSupplyCost
    );
    const TArray<FBHWarGarrisonTransferState> RedeploymentTransfers =
        RedeploymentWar->GetGarrisonTransfers();
    TestTrue(
        TEXT("Redeployment order retains its source and destination"),
        RedeploymentTransfers.Num() == 1 &&
            RedeploymentTransfers[0].SourceSectorID ==
                RedeploymentSourceID &&
            RedeploymentTransfers[0].DestinationSectorID ==
                RedeploymentDestinationID
    );
    const TArray<FBHWarEventRecord> RedeploymentEvents =
        RedeploymentWar->GetRecentWarEvents();
    TestTrue(
        TEXT("Redeployment dispatch is recorded in campaign history"),
        !RedeploymentEvents.IsEmpty() &&
            RedeploymentEvents.Last().EventType ==
                TEXT("GarrisonTransferDispatched")
    );
    RedeploymentWar->AdvanceWarTurn();
    TestEqual(
        TEXT("Arrived troop movement is removed from transit"),
        RedeploymentWar->GetIncomingGarrisonTransferCount(
            RedeploymentDestinationID
        ),
        0
    );
    TestTrue(
        TEXT("Arriving reserves reinforce the destination"),
        RedeploymentWar->GetSectorGarrisonCount(
            RedeploymentDestinationID,
            EBHWarFaction::Friendly
        ) >=
            RedeploymentDestinationGarrisonBefore +
                RedeploymentCount
    );
    const TArray<FBHWarEventRecord> ArrivalEvents =
        RedeploymentWar->GetRecentWarEvents();
    TestTrue(
        TEXT("Reserve arrival is recorded in campaign history"),
        ArrivalEvents.ContainsByPredicate(
            [](const FBHWarEventRecord& Event)
            {
                return Event.EventType ==
                    TEXT("GarrisonTransferArrived");
            }
        )
    );

    UGameInstance* LockedRedeploymentGameInstance =
        NewObject<UGameInstance>(GetTransientPackage());
    UBHWarSubsystem* LockedRedeploymentWar =
        NewObject<UBHWarSubsystem>(
            LockedRedeploymentGameInstance
        );
    LockedRedeploymentWar->ResetCampaign();
    TestTrue(
        TEXT("Operation fixture commits its deployment"),
        LockedRedeploymentWar->SetCommittedOperation(
            TEXT("DovrenVillage"),
            EBHWarPriorityType::Defend
        )
    );
    TestFalse(
        TEXT("Committed operation sectors reject redeployment"),
        LockedRedeploymentWar->CanRedeploySectorGarrison(
            TEXT("WesternFOB")
        )
    );
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBHSaveSchemaMigrationBoundariesTest,
    "BrokenHorizon.Persistence.Migration.SchemaBoundaries",
    EAutomationTestFlags::EditorContext |
        EAutomationTestFlags::EngineFilter
)

bool FBHSaveSchemaMigrationBoundariesTest::RunTest(
    const FString& Parameters
)
{
    UGameInstance* GameInstance =
        NewObject<UGameInstance>(GetTransientPackage());
    UBHWarSubsystem* War =
        NewObject<UBHWarSubsystem>(GameInstance);
    TestNotNull(TEXT("Migration war subsystem is created"), War);
    if (!War)
    {
        return false;
    }

    War->ResetCampaign();
    const FBHCampaignDifficultyProfile Veteran =
        BHDifficulty::BuildPreset(
            EBHCampaignDifficultyPreset::Veteran
        );
    TestTrue(
        TEXT("Schema 41 difficulty restores through legacy default"),
        War->RestoreCampaignDifficulty(Veteran, 41)
    );
    TestEqual(
        TEXT("Pre-42 saves migrate to the Operator baseline"),
        War->GetCampaignDifficulty().Preset,
        EBHCampaignDifficultyPreset::Operator
    );
    TestTrue(
        TEXT("Schema 42 difficulty payload is accepted"),
        War->RestoreCampaignDifficulty(Veteran, 42)
    );
    TestEqual(
        TEXT("Schema 42 preserves the saved difficulty"),
        War->GetCampaignDifficulty().Preset,
        EBHCampaignDifficultyPreset::Veteran
    );

    FBHCampaignProgressionState SavedProgression;
    SavedProgression.CampaignMerit = 500;
    SavedProgression.CompletedOperations = 5;
    SavedProgression.SuccessfulOperations = 4;
    SavedProgression.UnlockedCapabilities = {
        EBHCampaignCapability::IntelligenceNetwork,
        EBHCampaignCapability::CasualtyRecoveryNetwork,
        EBHCampaignCapability::TransportSupportNetwork
    };
    SavedProgression.ActiveTacticalOption =
        EBHOperationTacticalOption::ReconPlanning;

    TestTrue(
        TEXT("Schema 42 progression restores through legacy default"),
        War->RestoreCampaignProgression(SavedProgression, 42)
    );
    TestEqual(
        TEXT("Pre-43 saves start with compatible empty progression"),
        War->GetCampaignProgression().CampaignMerit,
        0
    );
    TestTrue(
        TEXT("Schema 43 progression payload is accepted"),
        War->RestoreCampaignProgression(SavedProgression, 43)
    );
    TestEqual(
        TEXT("Schema 43 preserves progression merit"),
        War->GetCampaignProgression().CampaignMerit,
        500
    );
    TestEqual(
        TEXT("Pre-44 saves clear the active tactical option"),
        War->GetCampaignProgression().ActiveTacticalOption,
        EBHOperationTacticalOption::None
    );
    TestTrue(
        TEXT("Schema 44 tactical progression payload is accepted"),
        War->RestoreCampaignProgression(SavedProgression, 44)
    );
    TestEqual(
        TEXT("Schema 44 preserves an unlocked tactical option"),
        War->GetCampaignProgression().ActiveTacticalOption,
        EBHOperationTacticalOption::ReconPlanning
    );
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBHSaveRoundTripTest,
    "BrokenHorizon.Persistence.SaveGame.RoundTrip",
    EAutomationTestFlags::EditorContext |
        EAutomationTestFlags::EngineFilter
)

bool FBHSaveRoundTripTest::RunTest(const FString& Parameters)
{
    UBHSaveGame* Source = NewObject<UBHSaveGame>(GetTransientPackage());
    TestNotNull(TEXT("Save game is created"), Source);
    if (!Source)
    {
        return false;
    }

    Source->SavedLevelName = FName(TEXT("L_FirstLight_Graybox"));
    Source->CurrentObjectiveID = BHObjectiveIds::EliminateGuard;
    Source->CompletedObjectiveIDs = {
        BHObjectiveIds::FindRedKeycard,
        BHObjectiveIds::UnlockSecurityDoor
    };
    Source->ConsumedWorldItemIDs = {
        FName(TEXT("FirstLightAmmoSupply"))
    };
    Source->SavedHealth = 72.0f;
    Source->SavedMagazineAmmo = 17;
    Source->SavedReserveAmmo = 61;
    Source->bCampaignEpilogueAcknowledged = true;
    Source->bOperationDebriefAcknowledged = true;
    Source->OpenWorldOperationState.bHasSnapshot = true;
    Source->OpenWorldOperationState.bOperationActivated = false;
    Source->OpenWorldOperationState.bFriendlySupportHolding = true;
    Source->OpenWorldOperationState
        .bFriendlySupportHasCommandLocation = true;
    Source->OpenWorldOperationState
        .FriendlySupportCommandLocation =
            FVector(-516700.0f, -289100.0f, 4520.0f);
    Source->OpenWorldOperationState
        .FriendlySupportCommandYaw = 62.5f;
    Source->OpenWorldOperationState.SecondsUntilApproachDeadline =
        347.5f;
    Source->LivingFieldSquadCount = 2;
    Source->bFieldSquadHolding = true;
    Source->bFieldSquadHasCommandLocation = true;
    Source->FieldSquadCommandLocation =
        FVector(-516950.0f, -289350.0f, 4510.0f);
    Source->FieldSquadCommandYaw = 18.0f;
    Source->bFieldSquadEmbarked = true;
    Source->FieldSquadTransportPersistenceID =
        TEXT("WesternFOBFieldTransport01");
    FBHFieldSquadMemberState& SavedPointOperative =
        Source->FieldSquadMemberStates.AddDefaulted_GetRef();
    SavedPointOperative.MemberID =
        TEXT("FieldOperative_Point");
    SavedPointOperative.Health = 63.0f;
    SavedPointOperative.MagazineAmmo = 11;
    SavedPointOperative.ReserveAmmo = 47;
    SavedPointOperative.FragGrenades = 1;
    SavedPointOperative.bEmbarked = true;
    SavedPointOperative.bRequiresMedicalEvacuation = true;
    FBHFieldSquadMemberState& SavedRearOperative =
        Source->FieldSquadMemberStates.AddDefaulted_GetRef();
    SavedRearOperative.Health = 28.0f;
    SavedRearOperative.MagazineAmmo = 3;
    SavedRearOperative.ReserveAmmo = 19;
    SavedRearOperative.FragGrenades = 0;
    SavedRearOperative.bIncapacitated = true;
    SavedRearOperative.bHasWorldTransform = true;
    SavedRearOperative.IncapacitationSecondsRemaining = 187.0f;
    SavedRearOperative.WorldTransform = FTransform(
        FRotator(0.0f, 75.0f, 0.0f),
        FVector(-517000.0f, -289500.0f, 4500.0f)
    );
    const FTransform WesternTransportTransform(
        FRotator(0.0f, 42.0f, 0.0f),
        FVector(-947249.0f, -456239.0f, 9275.0f)
    );
    const FTransform DovrenTransportTransform(
        FRotator(0.0f, 24.0f, 0.0f),
        FVector(-517306.0f, -289821.0f, 4500.0f)
    );
    FBHFieldTransportSaveState& SavedTransport =
        Source->FieldTransportStates.AddDefaulted_GetRef();
    SavedTransport.PersistenceID =
        TEXT("WesternFOBFieldTransport01");
    SavedTransport.Transform = WesternTransportTransform;
    SavedTransport.bPlayerWasDriving = true;
    SavedTransport.FuelFraction = 0.42f;
    SavedTransport.HullFraction = 0.68f;
    SavedTransport.CargoSupply = 12.0f;
    SavedTransport.CargoSourceSectorID = TEXT("WesternFOB");
    SavedTransport.CargoDestinationSectorID =
        TEXT("KoronaCrossroads");
    SavedTransport.CargoType =
        EBHWarConvoyCargoType::CivilianAid;
    FBHFieldTransportSaveState& SavedDovrenTransport =
        Source->FieldTransportStates.AddDefaulted_GetRef();
    SavedDovrenTransport.PersistenceID =
        TEXT("DovrenVillageFieldTransport01");
    SavedDovrenTransport.Transform = DovrenTransportTransform;
    SavedDovrenTransport.bPlayerWasDriving = false;
    SavedDovrenTransport.FuelFraction = 0.91f;
    SavedDovrenTransport.HullFraction = 0.83f;
    SavedDovrenTransport.CargoSupply = 0.0f;
    SavedDovrenTransport.CargoSourceSectorID = NAME_None;
    FBHConvoySalvageSaveState& SavedSalvage =
        Source->ConvoySalvageStates.AddDefaulted_GetRef();
    SavedSalvage.ConvoyID =
        TEXT("EnemyConvoy_12_EasternDepot_KoronaCrossroads");
    SavedSalvage.SourceSectorID = TEXT("EasternDepot");
    SavedSalvage.DestinationSectorID =
        TEXT("KoronaCrossroads");
    SavedSalvage.Transform = FTransform(
        FRotator(0.0f, 15.0f, 0.0f),
        FVector(-215000.0f, 85000.0f, 3200.0f)
    );
    SavedSalvage.OriginalSupplyPayload = 15.0f;
    SavedSalvage.RecoverableSupply = 6.0f;
    SavedSalvage.LifetimeRemaining = 73.5f;
    SavedSalvage.SurvivingSecurityCount = 2;
    FBHWarSectorState& SavedSector =
        Source->WarSectorStates.AddDefaulted_GetRef();
    SavedSector.SectorID = TEXT("KoronaCrossroads");
    SavedSector.SiteType = EBHWarSiteType::Town;
    SavedSector.GarrisonCapacity = 12;
    SavedSector.FriendlyGarrison = 3;
    SavedSector.EnemyGarrison = 4;
    SavedSector.IntelConfidence = 63.0f;
    SavedSector.CivilianSupport = 71.0f;
    SavedSector.EnemyResponsePressure = 57.0f;
    SavedSector.AnticipatedOperationType =
        EBHWarPriorityType::Raid;
    SavedSector.RepeatedOperationCount = 2;
    FBHWarGarrisonTransferState& SavedTransfer =
        Source->WarGarrisonTransfers.AddDefaulted_GetRef();
    SavedTransfer.TransferID =
        TEXT("Transfer_4_0_WesternFOB_DovrenVillage");
    SavedTransfer.SourceSectorID = TEXT("WesternFOB");
    SavedTransfer.DestinationSectorID = TEXT("DovrenVillage");
    SavedTransfer.TroopCount = 2;
    SavedTransfer.TurnsRemaining = 1;
    SavedTransfer.DispatchTurn = 4;
    Source->WarFriendlyManpowerReserve = 9;
    Source->WarEnemyManpowerReserve = 17;
    Source->WarFriendlyRecruitmentProgress = 0.35f;
    Source->WarEnemyRecruitmentProgress = 0.65f;

    TArray<uint8> Bytes;
    TestTrue(
        TEXT("Save game serializes to memory"),
        UGameplayStatics::SaveGameToMemory(Source, Bytes)
    );

    UBHSaveGame* Restored = Cast<UBHSaveGame>(
        UGameplayStatics::LoadGameFromMemory(Bytes)
    );
    TestNotNull(TEXT("Save game deserializes"), Restored);
    if (!Restored)
    {
        return false;
    }

    TestEqual(TEXT("Schema survives"), Restored->SchemaVersion, BHSave::CurrentSchemaVersion);
    TestEqual(TEXT("Level survives"), Restored->SavedLevelName, Source->SavedLevelName);
    TestEqual(TEXT("Objective survives"), Restored->CurrentObjectiveID, Source->CurrentObjectiveID);
    TestEqual(TEXT("Health survives"), Restored->SavedHealth, Source->SavedHealth);
    TestEqual(TEXT("Magazine ammo survives"), Restored->SavedMagazineAmmo, Source->SavedMagazineAmmo);
    TestEqual(TEXT("Reserve ammo survives"), Restored->SavedReserveAmmo, Source->SavedReserveAmmo);
    TestTrue(
        TEXT("Campaign epilogue acknowledgement survives"),
        Restored->bCampaignEpilogueAcknowledged
    );
    TestTrue(
        TEXT("Operation debrief acknowledgement survives"),
        Restored->bOperationDebriefAcknowledged
    );
    TestTrue(
        TEXT("Operation approach snapshot survives"),
        Restored->OpenWorldOperationState.bHasSnapshot &&
            !Restored->OpenWorldOperationState.bOperationActivated
    );
    TestTrue(
        TEXT("Friendly hold order survives"),
        Restored->OpenWorldOperationState.bFriendlySupportHolding
    );
    TestTrue(
        TEXT("Friendly designated hold order survives"),
        Restored->OpenWorldOperationState
            .bFriendlySupportHasCommandLocation
    );
    TestTrue(
        TEXT("Friendly support command location survives"),
        Restored->OpenWorldOperationState
            .FriendlySupportCommandLocation.Equals(
                FVector(-516700.0f, -289100.0f, 4520.0f)
            )
    );
    TestEqual(
        TEXT("Friendly support command facing survives"),
        Restored->OpenWorldOperationState
            .FriendlySupportCommandYaw,
        62.5f
    );
    TestEqual(
        TEXT("Operation mobilization deadline survives"),
        Restored->OpenWorldOperationState
            .SecondsUntilApproachDeadline,
        347.5f
    );
    TestEqual(
        TEXT("Living field fireteam count survives"),
        Restored->LivingFieldSquadCount,
        2
    );
    TestTrue(
        TEXT("Field fireteam hold order survives"),
        Restored->bFieldSquadHolding
    );
    TestTrue(
        TEXT("Field fireteam designated rally survives"),
        Restored->bFieldSquadHasCommandLocation
    );
    TestTrue(
        TEXT("Field fireteam rally location survives"),
        Restored->FieldSquadCommandLocation.Equals(
            FVector(-516950.0f, -289350.0f, 4510.0f)
        )
    );
    TestEqual(
        TEXT("Field fireteam rally facing survives"),
        Restored->FieldSquadCommandYaw,
        18.0f
    );
    TestTrue(
        TEXT("Field fireteam passenger state survives"),
        Restored->bFieldSquadEmbarked
    );
    TestEqual(
        TEXT("Field fireteam transport identity survives"),
        Restored->FieldSquadTransportPersistenceID,
        FName(TEXT("WesternFOBFieldTransport01"))
    );
    TestEqual(
        TEXT("Field fireteam combat-state count survives"),
        Restored->FieldSquadMemberStates.Num(),
        2
    );
    if (Restored->FieldSquadMemberStates.Num() == 2)
    {
        const FBHFieldSquadMemberState& RestoredPointOperative =
            Restored->FieldSquadMemberStates[0];
        const FBHFieldSquadMemberState& RestoredRearOperative =
            Restored->FieldSquadMemberStates[1];
        TestEqual(
            TEXT("Point operative identity survives"),
            RestoredPointOperative.MemberID,
            FName(TEXT("FieldOperative_Point"))
        );
        TestTrue(
            TEXT("Point operative medevac state survives"),
            RestoredPointOperative.bRequiresMedicalEvacuation
        );
        TestEqual(
            TEXT("Point operative health survives"),
            RestoredPointOperative.Health,
            63.0f
        );
        TestEqual(
            TEXT("Point operative magazine survives"),
            RestoredPointOperative.MagazineAmmo,
            11
        );
        TestEqual(
            TEXT("Point operative reserve survives"),
            RestoredPointOperative.ReserveAmmo,
            47
        );
        TestEqual(
            TEXT("Point operative grenades survive"),
            RestoredPointOperative.FragGrenades,
            1
        );
        TestTrue(
            TEXT("Point operative passenger state survives"),
            RestoredPointOperative.bEmbarked
        );
        TestEqual(
            TEXT("Rear operative health survives"),
            RestoredRearOperative.Health,
            28.0f
        );
        TestEqual(
            TEXT("Rear operative magazine survives"),
            RestoredRearOperative.MagazineAmmo,
            3
        );
        TestEqual(
            TEXT("Rear operative reserve survives"),
            RestoredRearOperative.ReserveAmmo,
            19
        );
        TestEqual(
            TEXT("Rear operative grenades survive"),
            RestoredRearOperative.FragGrenades,
            0
        );
        TestTrue(
            TEXT("Rear operative incapacitation survives"),
            RestoredRearOperative.bIncapacitated
        );
        TestFalse(
            TEXT("Rear operative remains outside transport"),
            RestoredRearOperative.bEmbarked
        );
        TestTrue(
            TEXT("Rear operative casualty location survives"),
            RestoredRearOperative.bHasWorldTransform
        );
        TestTrue(
            TEXT("Rear operative world transform survives"),
            RestoredRearOperative.WorldTransform.Equals(
                SavedRearOperative.WorldTransform
            )
        );
        TestEqual(
            TEXT("Rear operative recovery window survives"),
            RestoredRearOperative
                .IncapacitationSecondsRemaining,
            187.0f
        );
    }
    TestEqual(
        TEXT("Transport state count survives"),
        Restored->FieldTransportStates.Num(),
        2
    );
    TestEqual(
        TEXT("Recoverable convoy wreck count survives"),
        Restored->ConvoySalvageStates.Num(),
        1
    );
    if (Restored->ConvoySalvageStates.Num() == 1)
    {
        const FBHConvoySalvageSaveState&
            RestoredSalvage =
                Restored->ConvoySalvageStates[0];
        TestEqual(
            TEXT("Salvage convoy identity survives"),
            RestoredSalvage.ConvoyID,
            SavedSalvage.ConvoyID
        );
        TestEqual(
            TEXT("Salvage source survives"),
            RestoredSalvage.SourceSectorID,
            SavedSalvage.SourceSectorID
        );
        TestEqual(
            TEXT("Salvage destination survives"),
            RestoredSalvage.DestinationSectorID,
            SavedSalvage.DestinationSectorID
        );
        TestTrue(
            TEXT("Salvage wreck transform survives"),
            RestoredSalvage.Transform.Equals(
                SavedSalvage.Transform
            )
        );
        TestEqual(
            TEXT("Remaining salvage supply survives"),
            RestoredSalvage.RecoverableSupply,
            6.0f
        );
        TestEqual(
            TEXT("Salvage recovery window survives"),
            RestoredSalvage.LifetimeRemaining,
            73.5f
        );
        TestEqual(
            TEXT("Salvage security detail survives"),
            RestoredSalvage.SurvivingSecurityCount,
            2
        );
    }
    const FBHFieldTransportSaveState* RestoredWesternTransport =
        Restored->FieldTransportStates.FindByPredicate(
            [](const FBHFieldTransportSaveState& State)
            {
                return State.PersistenceID ==
                    TEXT("WesternFOBFieldTransport01");
            }
        );
    const FBHFieldTransportSaveState* RestoredDovrenTransport =
        Restored->FieldTransportStates.FindByPredicate(
            [](const FBHFieldTransportSaveState& State)
            {
                return State.PersistenceID ==
                    TEXT("DovrenVillageFieldTransport01");
            }
        );
    TestNotNull(
        TEXT("Western transport identity survives"),
        RestoredWesternTransport
    );
    TestNotNull(
        TEXT("Dovren transport identity survives"),
        RestoredDovrenTransport
    );

    if (!RestoredWesternTransport ||
        !RestoredDovrenTransport)
    {
        return false;
    }

    TestTrue(
        TEXT("Western transport transform survives"),
        RestoredWesternTransport->Transform.Equals(
            WesternTransportTransform
        )
    );
    TestTrue(
        TEXT("Western transport driver state survives"),
        RestoredWesternTransport->bPlayerWasDriving
    );
    TestEqual(
        TEXT("Western transport fuel survives"),
        RestoredWesternTransport->FuelFraction,
        0.42f
    );
    TestEqual(
        TEXT("Western transport hull survives"),
        RestoredWesternTransport->HullFraction,
        0.68f
    );
    TestEqual(
        TEXT("Western transport cargo survives"),
        RestoredWesternTransport->CargoSupply,
        12.0f
    );
    TestEqual(
        TEXT("Western transport cargo origin survives"),
        RestoredWesternTransport->CargoSourceSectorID,
        FName(TEXT("WesternFOB"))
    );
    TestEqual(
        TEXT("Western transport aid destination survives"),
        RestoredWesternTransport->CargoDestinationSectorID,
        FName(TEXT("KoronaCrossroads"))
    );
    TestEqual(
        TEXT("Western transport aid cargo type survives"),
        RestoredWesternTransport->CargoType,
        EBHWarConvoyCargoType::CivilianAid
    );
    TestTrue(
        TEXT("Dovren transport transform survives"),
        RestoredDovrenTransport->Transform.Equals(
            DovrenTransportTransform
        )
    );
    TestFalse(
        TEXT("Dovren transport remains unoccupied"),
        RestoredDovrenTransport->bPlayerWasDriving
    );
    TestEqual(
        TEXT("Dovren transport fuel survives independently"),
        RestoredDovrenTransport->FuelFraction,
        0.91f
    );
    TestEqual(
        TEXT("Dovren transport hull survives independently"),
        RestoredDovrenTransport->HullFraction,
        0.83f
    );
    TestEqual(
        TEXT("Dovren transport remains cargo-empty"),
        RestoredDovrenTransport->CargoSupply,
        0.0f
    );
    TestTrue(
        TEXT("Dovren transport has no cargo origin"),
        RestoredDovrenTransport->CargoSourceSectorID.IsNone()
    );
    TestEqual(
        TEXT("Garrison state survives"),
        Restored->WarSectorStates[0].EnemyGarrison,
        4
    );
    TestEqual(
        TEXT("Intelligence confidence survives"),
        Restored->WarSectorStates[0].IntelConfidence,
        63.0f
    );
    TestEqual(
        TEXT("Civilian support survives"),
        Restored->WarSectorStates[0].CivilianSupport,
        71.0f
    );
    TestEqual(
        TEXT("Enemy response pressure survives"),
        Restored->WarSectorStates[0].EnemyResponsePressure,
        57.0f
    );
    TestEqual(
        TEXT("Enemy anticipated operation survives"),
        Restored->WarSectorStates[0].AnticipatedOperationType,
        EBHWarPriorityType::Raid
    );
    TestEqual(
        TEXT("Enemy operation pattern count survives"),
        Restored->WarSectorStates[0].RepeatedOperationCount,
        2
    );
    TestEqual(
        TEXT("Garrison transfer count survives"),
        Restored->WarGarrisonTransfers.Num(),
        1
    );
    TestEqual(
        TEXT("Garrison transfer payload survives"),
        Restored->WarGarrisonTransfers[0].TroopCount,
        2
    );
    TestEqual(
        TEXT("Garrison transfer ETA survives"),
        Restored->WarGarrisonTransfers[0].TurnsRemaining,
        1
    );
    TestEqual(
        TEXT("Friendly manpower reserve survives"),
        Restored->WarFriendlyManpowerReserve,
        9
    );
    TestEqual(
        TEXT("Enemy manpower reserve survives"),
        Restored->WarEnemyManpowerReserve,
        17
    );
    TestEqual(
        TEXT("Friendly recruitment progress survives"),
        Restored->WarFriendlyRecruitmentProgress,
        0.35f
    );
    TestEqual(
        TEXT("Enemy recruitment progress survives"),
        Restored->WarEnemyRecruitmentProgress,
        0.65f
    );
    TestTrue(
        TEXT("Consumed item survives"),
        Restored->ConsumedWorldItemIDs.Contains(FName(TEXT("FirstLightAmmoSupply")))
    );
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBHStrategicControlClarityContractTest,
    "BrokenHorizon.Multiplayer.UI.StrategicControlClarity",
    EAutomationTestFlags::EditorContext |
        EAutomationTestFlags::EngineFilter
)

bool FBHStrategicControlClarityContractTest::RunTest(
    const FString& Parameters
)
{
    TestEqual(
        TEXT("Connected clients receive explicit host-authority feedback"),
        UBHWarMapWidget::BuildStrategicControlRejection(
            false,
            false
        ),
        FString(TEXT("COMMAND LOCKED // HOST AUTHORITY REQUIRED"))
    );
    TestEqual(
        TEXT("Host controls explain the active-operation lock"),
        UBHWarMapWidget::BuildStrategicControlRejection(
            true,
            true
        ),
        FString(
            TEXT(
                "COMMAND LOCKED // ACTIVE OPERATION IN PROGRESS"
            )
        )
    );
    TestEqual(
        TEXT("Authority feedback takes precedence for remote clients"),
        UBHWarMapWidget::BuildStrategicControlRejection(
            false,
            true
        ),
        FString(TEXT("COMMAND LOCKED // HOST AUTHORITY REQUIRED"))
    );
    TestTrue(
        TEXT("Unlocked host controls return no rejection"),
        UBHWarMapWidget::BuildStrategicControlRejection(
            true,
            false
        ).IsEmpty()
    );

    const UFunction* AuthorityContract =
        UBHWarSubsystem::StaticClass()->FindFunctionByName(
            GET_FUNCTION_NAME_CHECKED(
                UBHWarSubsystem,
                CanIssueStrategicCommands
            )
        );
    TestNotNull(
        TEXT("Strategic authority is exposed to Blueprint UI"),
        AuthorityContract
    );
    if (AuthorityContract)
    {
        TestTrue(
            TEXT("Strategic authority contract is Blueprint pure"),
            AuthorityContract->HasAnyFunctionFlags(
                FUNC_BlueprintPure
            )
        );
    }

    const UFunction* FortificationSummaryContract =
        UBHWarSubsystem::StaticClass()->FindFunctionByName(
            GET_FUNCTION_NAME_CHECKED(
                UBHWarSubsystem,
                GetSectorFortificationSummary
            )
        );
    TestNotNull(
        TEXT("Fortification summary contract is exposed"),
        FortificationSummaryContract
    );
    if (FortificationSummaryContract)
    {
        TestTrue(
            TEXT("Fortification summary contract remains Blueprint pure"),
            FortificationSummaryContract->HasAnyFunctionFlags(
                FUNC_BlueprintPure
            )
        );
    }

    UBHWarSubsystem* WarSubsystemCDO =
        UBHWarSubsystem::StaticClass()->GetDefaultObject<UBHWarSubsystem>();
    if (IsValid(WarSubsystemCDO))
    {
        int32 ConstructedFortifications = -1;
        int32 UnfinishedFortifications = -1;
        float FortificationDefense = -1.0f;

        WarSubsystemCDO->GetSectorFortificationSummary(
            NAME_None,
            ConstructedFortifications,
            UnfinishedFortifications,
            FortificationDefense
        );

        TestEqual(
            TEXT("Fortification summary defaults constructed count to zero"),
            ConstructedFortifications,
            0
        );
        TestEqual(
            TEXT("Fortification summary defaults unfinished count to zero"),
            UnfinishedFortifications,
            0
        );
        TestEqual(
            TEXT("Fortification summary defaults defense value to zero"),
            FortificationDefense,
            0.0f
        );
    }

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBHSquadSuppressiveFireContractTest,
    "BrokenHorizon.Combat.Suppression.SquadOrder",
    EAutomationTestFlags::EditorContext |
        EAutomationTestFlags::EngineFilter
)

bool FBHSquadSuppressiveFireContractTest::RunTest(
    const FString& Parameters
)
{
    TestTrue(
        TEXT("A living armed operative can suppress a valid hostile"),
        ABHEnemyAIController::CanAcceptSuppressiveFireOrder(
            true, false, 30, true)
    );
    TestFalse(
        TEXT("An empty weapon prevents a suppressive-fire assignment"),
        ABHEnemyAIController::CanAcceptSuppressiveFireOrder(
            true, false, 0, true)
    );
    TestFalse(
        TEXT("An incapacitated operative cannot accept the order"),
        ABHEnemyAIController::CanAcceptSuppressiveFireOrder(
            true, true, 30, true)
    );
    TestFalse(
        TEXT("A non-hostile target cannot receive suppressive fire"),
        ABHEnemyAIController::CanAcceptSuppressiveFireOrder(
            true, false, 30, false)
    );
    TestEqual(
        TEXT("Suppressive doctrine adds a deliberate accuracy penalty"),
        ABHEnemyAIController::CalculateSuppressiveFireSpread(1.0f, true),
        3.5f
    );
    TestEqual(
        TEXT("Normal fire retains its incoming suppression spread"),
        ABHEnemyAIController::CalculateSuppressiveFireSpread(1.0f, false),
        1.0f
    );
    TestEqual(
        TEXT("Invalid negative spread is clamped"),
        ABHEnemyAIController::CalculateSuppressiveFireSpread(-1.0f, false),
        0.0f
    );

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBHPlayerSuppressionContractTest,
    "BrokenHorizon.Combat.Suppression.PlayerPressure",
    EAutomationTestFlags::EditorContext |
        EAutomationTestFlags::EngineFilter
)

bool FBHPlayerSuppressionContractTest::RunTest(
    const FString& Parameters
)
{
    const float FirstNearMiss =
        ABHCharacter::AccumulatePlayerSuppression(
            0.0f,
            1.0f,
            0.35f
        );
    const float RepeatedNearMiss =
        ABHCharacter::AccumulatePlayerSuppression(
            FirstNearMiss,
            1.0f,
            0.35f
        );

    TestTrue(
        TEXT("A full-intensity near miss applies configured pressure"),
        FMath::IsNearlyEqual(FirstNearMiss, 0.35f)
    );
    TestTrue(
        TEXT("Repeated near misses stack suppression"),
        RepeatedNearMiss > FirstNearMiss
    );
    TestTrue(
        TEXT("Diminishing stacking remains below the hard cap"),
        RepeatedNearMiss < 0.70f
    );
    TestTrue(
        TEXT("Suppression decays predictably while out of fire"),
        FMath::IsNearlyEqual(
            ABHCharacter::DecayPlayerSuppression(
                0.60f,
                0.20f,
                2.0f
            ),
            0.20f,
            0.001f
        )
    );
    TestEqual(
        TEXT("Suppression decay cannot underflow"),
        ABHCharacter::DecayPlayerSuppression(
            0.10f,
            0.20f,
            2.0f
        ),
        0.0f
    );
    TestEqual(
        TEXT("Invalid negative pressure cannot add suppression"),
        ABHCharacter::AccumulatePlayerSuppression(
            0.25f,
            -1.0f,
            0.35f
        ),
        0.25f
    );

    const UFunction* SuppressionHUDContract =
        UBHCombatStatusWidget::StaticClass()->FindFunctionByName(
            GET_FUNCTION_NAME_CHECKED(
                UBHCombatStatusWidget,
                SetSuppression
            )
        );
    TestNotNull(
        TEXT("Suppression HUD remains Blueprint-callable"),
        SuppressionHUDContract
    );
    TestTrue(
        TEXT("Isolated wounded enemies can surrender under overwhelming suppression"),
        ABHEnemySoldier::ShouldSurrender(
            1.0f, 0.40f, false, 1)
    );
    TestTrue(
        TEXT("Isolated enemies without ammunition can surrender"),
        ABHEnemySoldier::ShouldSurrender(
            0.95f, 1.0f, true, 0)
    );
    TestFalse(
        TEXT("Healthy equipped enemies do not surrender from suppression alone"),
        ABHEnemySoldier::ShouldSurrender(
            1.0f, 1.0f, false, 0)
    );
    TestFalse(
        TEXT("Cohesive enemy groups continue fighting"),
        ABHEnemySoldier::ShouldSurrender(
            1.0f, 0.30f, false, 2)
    );
    const FProperty* SurrenderedProperty = FindFProperty<FProperty>(
        ABHEnemySoldier::StaticClass(),
        FName(TEXT("bSurrendered"))
    );
    TestTrue(
        TEXT("Surrender state replicates to every player"),
        SurrenderedProperty &&
            SurrenderedProperty->HasAnyPropertyFlags(CPF_Net)
    );
    const FProperty* SecuredProperty = FindFProperty<FProperty>(
        ABHEnemySoldier::StaticClass(),
        FName(TEXT("bSurrenderSecured"))
    );
    TestTrue(
        TEXT("Secured custody state replicates to every player"),
        SecuredProperty && SecuredProperty->HasAnyPropertyFlags(CPF_Net)
    );
    const UFunction* SecureFunction =
        ABHEnemySoldier::StaticClass()->FindFunctionByName(
            FName(TEXT("SecureSurrender"))
        );
    TestTrue(
        TEXT("Surrender custody remains Blueprint-callable"),
        SecureFunction && SecureFunction->HasAnyFunctionFlags(
            FUNC_BlueprintCallable
        )
    );
    TestEqual(
        TEXT("An unguarded surrendered enemy counts down toward escape"),
        ABHEnemySoldier::CalculateSurrenderEscapeRemaining(
            30.0f, 7.0f, false, 30.0f),
        23.0f
    );
    TestEqual(
        TEXT("A nearby living player resets the custody grace window"),
        ABHEnemySoldier::CalculateSurrenderEscapeRemaining(
            8.0f, 1.0f, true, 30.0f),
        30.0f
    );
    TestEqual(
        TEXT("The surrender escape countdown cannot become negative"),
        ABHEnemySoldier::CalculateSurrenderEscapeRemaining(
            2.0f, 5.0f, false, 30.0f),
        0.0f
    );
    const FProperty* EscapeProperty = FindFProperty<FProperty>(
        ABHEnemySoldier::StaticClass(),
        FName(TEXT("SurrenderEscapeSecondsRemaining"))
    );
    TestTrue(
        TEXT("Custody countdown replicates to every player"),
        EscapeProperty && EscapeProperty->HasAnyPropertyFlags(CPF_Net)
    );

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBHArmoredHitFeedbackContractTest,
    "BrokenHorizon.Combat.Feedback.ArmoredHit",
    EAutomationTestFlags::EditorContext |
        EAutomationTestFlags::EngineFilter
)

bool FBHArmoredHitFeedbackContractTest::RunTest(
    const FString& Parameters
)
{
    TestTrue(
        TEXT("Helmet mitigation classifies a protected head hit"),
        ABHEnemySoldier::DoesArmorMitigateHitZone(
            EBHHitZone::Head,
            true,
            0.55f,
            false,
            1.0f
        )
    );
    TestTrue(
        TEXT("Body armor mitigation classifies a protected torso hit"),
        ABHEnemySoldier::DoesArmorMitigateHitZone(
            EBHHitZone::Torso,
            false,
            1.0f,
            true,
            0.75f
        )
    );
    TestFalse(
        TEXT("Armor does not misclassify exposed limbs"),
        ABHEnemySoldier::DoesArmorMitigateHitZone(
            EBHHitZone::Arm,
            true,
            0.1f,
            true,
            0.1f
        )
    );
    TestFalse(
        TEXT("Cosmetic armor with no mitigation is not reported"),
        ABHEnemySoldier::DoesArmorMitigateHitZone(
            EBHHitZone::Torso,
            false,
            1.0f,
            true,
            1.0f
        )
    );
    TestFalse(
        TEXT("Unequipped armor cannot report mitigation"),
        ABHEnemySoldier::DoesArmorMitigateHitZone(
            EBHHitZone::Head,
            false,
            0.0f,
            false,
            0.0f
        )
    );

    const UFunction* LegacyFunction =
        UBHHitMarkerWidget::StaticClass()->FindFunctionByName(
            GET_FUNCTION_NAME_CHECKED(
                UBHHitMarkerWidget,
                ShowHitMarker
            )
        );
    const UFunction* DetailedFunction =
        UBHHitMarkerWidget::StaticClass()->FindFunctionByName(
            GET_FUNCTION_NAME_CHECKED(
                UBHHitMarkerWidget,
                ShowDetailedHitMarker
            )
        );
    TestNotNull(
        TEXT("Legacy Blueprint hit-marker API remains available"),
        LegacyFunction
    );
    TestNotNull(
        TEXT("Detailed Blueprint hit-marker API is available"),
        DetailedFunction
    );

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBHNotificationPriorityContractTest,
    "BrokenHorizon.UI.Notification.PriorityContract",
    EAutomationTestFlags::EditorContext |
        EAutomationTestFlags::EngineFilter
)

bool FBHNotificationPriorityContractTest::RunTest(
    const FString& Parameters
)
{
    TestTrue(
        TEXT("Critical mission events preempt routine status"),
        UBHObjectiveNotificationWidget::ShouldPreemptNotification(
            EBHNotificationPriority::Critical,
            EBHNotificationPriority::Normal
        )
    );
    TestTrue(
        TEXT("High-priority guidance preempts routine status"),
        UBHObjectiveNotificationWidget::ShouldPreemptNotification(
            EBHNotificationPriority::High,
            EBHNotificationPriority::Normal
        )
    );
    TestFalse(
        TEXT("Routine status cannot hide critical mission events"),
        UBHObjectiveNotificationWidget::ShouldPreemptNotification(
            EBHNotificationPriority::Normal,
            EBHNotificationPriority::Critical
        )
    );
    TestFalse(
        TEXT("Equal-priority messages preserve FIFO presentation"),
        UBHObjectiveNotificationWidget::ShouldPreemptNotification(
            EBHNotificationPriority::High,
            EBHNotificationPriority::High
        )
    );
    TestEqual(
        TEXT("Routine UI events use quiet confirmation audio"),
        UBHObjectiveNotificationWidget::ResolveDefaultAudioCue(
            EBHNotificationPriority::Normal
        ),
        EBHNotificationAudioCue::QuietConfirmation
    );
    TestEqual(
        TEXT("Strategic warnings remain distinct from combat alarms"),
        UBHObjectiveNotificationWidget::ResolveDefaultAudioCue(
            EBHNotificationPriority::High
        ),
        EBHNotificationAudioCue::StrategicWarning
    );
    TestEqual(
        TEXT("Immediate critical warnings use the combat alarm"),
        UBHObjectiveNotificationWidget::ResolveDefaultAudioCue(
            EBHNotificationPriority::Critical
        ),
        EBHNotificationAudioCue::CombatAlarm
    );
    TestTrue(
        TEXT("Background strategy waits during immediate combat"),
        UBHObjectiveNotificationWidget::ShouldDeferNotification(
            true,
            true
        )
    );
    TestFalse(
        TEXT("Active-operation and danger messages remain immediate"),
        UBHObjectiveNotificationWidget::ShouldDeferNotification(
            true,
            false
        )
    );
    TestFalse(
        TEXT("Deferred strategy releases after combat quiets"),
        UBHObjectiveNotificationWidget::ShouldDeferNotification(
            false,
            true
        )
    );
    TestTrue(
        TEXT("Latest deferred strategy supersedes stale queued strategy"),
        UBHObjectiveNotificationWidget::
            ShouldCoalesceDeferredStrategicNotification(true, true)
    );
    TestFalse(
        TEXT("Deferred strategy never coalesces tactical notifications"),
        UBHObjectiveNotificationWidget::
            ShouldCoalesceDeferredStrategicNotification(true, false)
    );
    TestFalse(
        TEXT("Immediate notifications never coalesce deferred strategy"),
        UBHObjectiveNotificationWidget::
            ShouldCoalesceDeferredStrategicNotification(false, true)
    );

    UBHObjectiveNotificationWidget* CadenceWidget =
        NewObject<UBHObjectiveNotificationWidget>();
    TestNotNull(
        TEXT("Post-combat cadence fixture can create the native widget"),
        CadenceWidget
    );
    if (CadenceWidget)
    {
        CadenceWidget->SetCombatIntensityActive(true);
        CadenceWidget->ShowDeferredStrategicNotification(
            FText::FromString(TEXT("Western front contested"))
        );
        CadenceWidget->ShowDeferredStrategicNotification(
            FText::FromString(TEXT("Western front secured"))
        );
        TestEqual(
            TEXT("Combat retains only the latest strategic snapshot"),
            CadenceWidget->GetPendingNotificationCount(),
            1
        );
        TestEqual(
            TEXT("The retained snapshot remains combat-deferred"),
            CadenceWidget->
                GetPendingDeferredStrategicNotificationCount(),
            1
        );
    }

    const FName NotificationAudioFields[] = {
        FName(TEXT("QuietConfirmationSound")),
        FName(TEXT("StrategicWarningSound")),
        FName(TEXT("CombatAlarmSound"))
    };
    for (const FName FieldName : NotificationAudioFields)
    {
        TestNotNull(
            *FString::Printf(TEXT("UI audio field %s remains authorable"), *FieldName.ToString()),
            UBHObjectiveNotificationWidget::StaticClass()->FindPropertyByName(FieldName)
        );
    }

    const UFunction* PriorityFunction =
        UBHObjectiveNotificationWidget::StaticClass()->FindFunctionByName(
            GET_FUNCTION_NAME_CHECKED(
                UBHObjectiveNotificationWidget,
                ShowPriorityNotification
            )
        );
    TestNotNull(
        TEXT("Priority notification remains Blueprint-callable"),
        PriorityFunction
    );
    if (PriorityFunction)
    {
        TestTrue(
            TEXT("Priority notification exposes Blueprint callable API"),
            PriorityFunction->HasAnyFunctionFlags(
                FUNC_BlueprintCallable
            )
        );
    }
    const UFunction* ExplicitAudioFunction =
        UBHObjectiveNotificationWidget::StaticClass()->FindFunctionByName(
            GET_FUNCTION_NAME_CHECKED(
                UBHObjectiveNotificationWidget,
                ShowNotificationWithAudioCue
            )
        );
    TestTrue(
        TEXT("Gameplay can override notification audio semantics"),
        ExplicitAudioFunction &&
            ExplicitAudioFunction->HasAnyFunctionFlags(FUNC_BlueprintCallable)
    );
    const UFunction* DeferredStrategyFunction =
        UBHObjectiveNotificationWidget::StaticClass()->FindFunctionByName(
            GET_FUNCTION_NAME_CHECKED(
                UBHObjectiveNotificationWidget,
                ShowDeferredStrategicNotification
            )
        );
    TestTrue(
        TEXT("UI exposes combat-aware strategic notification delivery"),
        DeferredStrategyFunction &&
            DeferredStrategyFunction->HasAnyFunctionFlags(
                FUNC_BlueprintCallable
            )
    );
    const UFunction* DeferredCountFunction =
        UBHObjectiveNotificationWidget::StaticClass()->FindFunctionByName(
            GET_FUNCTION_NAME_CHECKED(
                UBHObjectiveNotificationWidget,
                GetPendingDeferredStrategicNotificationCount
            )
        );
    TestTrue(
        TEXT("UI exposes deferred strategic queue diagnostics"),
        DeferredCountFunction &&
            DeferredCountFunction->HasAnyFunctionFlags(FUNC_BlueprintPure)
    );
    const UFunction* CharacterAudioFunction =
        ABHCharacter::StaticClass()->FindFunctionByName(
            FName(TEXT("ShowStatusNotificationWithAudioCue"))
        );
    TestTrue(
        TEXT("Networked character notifications expose explicit audio cues"),
        CharacterAudioFunction &&
            CharacterAudioFunction->HasAnyFunctionFlags(FUNC_BlueprintCallable)
    );
    const UFunction* DeferredCharacterFunction =
        ABHCharacter::StaticClass()->FindFunctionByName(
            GET_FUNCTION_NAME_CHECKED(
                ABHCharacter,
                ShowDeferredStrategicStatusNotification
            )
        );
    TestTrue(
        TEXT("Networked characters expose deferred strategic updates"),
        DeferredCharacterFunction &&
            DeferredCharacterFunction->HasAnyFunctionFlags(
                FUNC_BlueprintCallable
            )
    );

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBHAccessibilitySettingsContractTest,
    "BrokenHorizon.UI.Accessibility.SettingsContract",
    EAutomationTestFlags::EditorContext |
        EAutomationTestFlags::EngineFilter
)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBHMissionRadioSubtitleContractTest,
    "BrokenHorizon.UI.Accessibility.MissionRadioSubtitleContract",
    EAutomationTestFlags::EditorContext |
        EAutomationTestFlags::EngineFilter
)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBHEnemyVoiceBarkContractTest,
    "BrokenHorizon.Gameplay.AI.VoiceBarkContract",
    EAutomationTestFlags::EditorContext |
        EAutomationTestFlags::EngineFilter
)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBHMovementAcousticsContractTest,
    "BrokenHorizon.Gameplay.Movement.AcousticsContract",
    EAutomationTestFlags::EditorContext |
        EAutomationTestFlags::EngineFilter
)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBHAmbientWarAudioContractTest,
    "BrokenHorizon.Gameplay.World.AmbientWarAudioContract",
    EAutomationTestFlags::EditorContext |
        EAutomationTestFlags::EngineFilter
)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBHWeaponAudioSpaceContractTest,
    "BrokenHorizon.Gameplay.Combat.WeaponAudioSpaceContract",
    EAutomationTestFlags::EditorContext |
        EAutomationTestFlags::EngineFilter
)

bool FBHWeaponAudioSpaceContractTest::RunTest(const FString& Parameters)
{
    TestFalse(
        TEXT("Open probes select the outdoor weapon tail"),
        ABHRifle::ShouldUseIndoorFireTail(0, 5)
    );
    TestFalse(
        TEXT("A single nearby wall does not misclassify outdoors"),
        ABHRifle::ShouldUseIndoorFireTail(1, 5)
    );
    TestTrue(
        TEXT("A majority of blocked probes selects the indoor tail"),
        ABHRifle::ShouldUseIndoorFireTail(3, 5)
    );
    TestFalse(
        TEXT("An empty probe set is safely outdoor"),
        ABHRifle::ShouldUseIndoorFireTail(0, 0)
    );
    TestTrue(
        TEXT("More intense near misses raise pitch"),
        ABHCharacter::CalculateNearMissPitch(1.0f) >
            ABHCharacter::CalculateNearMissPitch(0.0f)
    );
    TestTrue(
        TEXT("Near-miss pitch clamps invalid intensity"),
        FMath::IsNearlyEqual(
            ABHCharacter::CalculateNearMissPitch(2.0f),
            ABHCharacter::CalculateNearMissPitch(1.0f)
        )
    );

    const FName RifleTailFields[] = {
        FName(TEXT("IndoorFireTailSound")),
        FName(TEXT("OutdoorFireTailSound"))
    };
    for (const FName FieldName : RifleTailFields)
    {
        TestNotNull(
            *FString::Printf(TEXT("Player rifle tail %s remains authorable"), *FieldName.ToString()),
            ABHRifle::StaticClass()->FindPropertyByName(FieldName)
        );
        TestNotNull(
            *FString::Printf(TEXT("Enemy rifle tail %s remains authorable"), *FieldName.ToString()),
            ABHEnemySoldier::StaticClass()->FindPropertyByName(FieldName)
        );
    }
    TestNotNull(
        TEXT("Player near-miss sound remains authorable"),
        ABHCharacter::StaticClass()->FindPropertyByName(FName(TEXT("NearMissSound")))
    );
    return true;
}

bool FBHAmbientWarAudioContractTest::RunTest(const FString& Parameters)
{
    TestEqual(
        TEXT("Quiet rear sectors retain a restrained distant bed"),
        ABHAmbientWarDirector::ResolveAmbientAudioState(false, false, 0, 0.0f),
        EBHAmbientAudioState::Quiet
    );
    TestEqual(
        TEXT("Strategic pressure raises tension before direct contact"),
        ABHAmbientWarDirector::ResolveAmbientAudioState(false, false, 0, 25.0f),
        EBHAmbientAudioState::Tense
    );
    TestEqual(
        TEXT("Frontline geography drives the wider-war layer"),
        ABHAmbientWarDirector::ResolveAmbientAudioState(true, false, 0, 0.0f),
        EBHAmbientAudioState::Frontline
    );
    TestEqual(
        TEXT("Active operations take precedence over ambient tension"),
        ABHAmbientWarDirector::ResolveAmbientAudioState(false, true, 0, 0.0f),
        EBHAmbientAudioState::Combat
    );
    TestEqual(
        TEXT("Physical hostiles drive combat ambience"),
        ABHAmbientWarDirector::ResolveAmbientAudioState(false, false, 1, 0.0f),
        EBHAmbientAudioState::Combat
    );
    TestTrue(
        TEXT("War-bed intensity rises monotonically with danger"),
        ABHAmbientWarDirector::CalculateWarBedVolume(EBHAmbientAudioState::Quiet) <
            ABHAmbientWarDirector::CalculateWarBedVolume(EBHAmbientAudioState::Tense) &&
        ABHAmbientWarDirector::CalculateWarBedVolume(EBHAmbientAudioState::Tense) <
            ABHAmbientWarDirector::CalculateWarBedVolume(EBHAmbientAudioState::Frontline) &&
        ABHAmbientWarDirector::CalculateWarBedVolume(EBHAmbientAudioState::Frontline) <
            ABHAmbientWarDirector::CalculateWarBedVolume(EBHAmbientAudioState::Combat)
    );

    const FName AudioFields[] = {
        FName(TEXT("WindLoopSound")),
        FName(TEXT("RainLoopSound")),
        FName(TEXT("DistantWarLoopSound")),
        FName(TEXT("DistantArtillerySound")),
        FName(TEXT("DistantAircraftSound")),
        FName(TEXT("DistantSmallArmsSound"))
    };
    for (const FName FieldName : AudioFields)
    {
        TestNotNull(
            *FString::Printf(TEXT("Ambient audio field %s remains authorable"), *FieldName.ToString()),
            ABHAmbientWarDirector::StaticClass()->FindPropertyByName(FieldName)
        );
    }
    const UFunction* WeatherFunction = ABHAmbientWarDirector::StaticClass()->FindFunctionByName(
        FName(TEXT("SetWeatherMix"))
    );
    TestTrue(
        TEXT("Weather systems can drive the replicated mix through Blueprint"),
        WeatherFunction && WeatherFunction->HasAnyFunctionFlags(FUNC_BlueprintCallable)
    );
    const UFunction* EventMulticast = ABHAmbientWarDirector::StaticClass()->FindFunctionByName(
        FName(TEXT("MulticastPlayDistantWarEvent"))
    );
    TestTrue(
        TEXT("Distant war events replicate as shared presentation"),
        EventMulticast && EventMulticast->HasAnyFunctionFlags(FUNC_NetMulticast)
    );
    return true;
}

bool FBHMovementAcousticsContractTest::RunTest(const FString& Parameters)
{
    const float WalkNoise = ABHCharacter::CalculateMovementNoiseLoudness(
        400.0f, false, false, false, 1.0f, 1.0f
    );
    const float SprintNoise = ABHCharacter::CalculateMovementNoiseLoudness(
        700.0f, true, false, false, 1.0f, 1.0f
    );
    const float CrouchNoise = ABHCharacter::CalculateMovementNoiseLoudness(
        260.0f, false, true, false, 1.0f, 1.0f
    );
    const float ProneNoise = ABHCharacter::CalculateMovementNoiseLoudness(
        140.0f, false, false, true, 1.0f, 1.0f
    );
    TestTrue(TEXT("Sprinting is louder than walking"), SprintNoise > WalkNoise);
    TestTrue(TEXT("Walking is louder than crouching"), WalkNoise > CrouchNoise);
    TestTrue(TEXT("Crouching is louder than prone movement"), CrouchNoise > ProneNoise);
    TestEqual(
        TEXT("Stationary players emit no movement noise"),
        ABHCharacter::CalculateMovementNoiseLoudness(
            0.0f, false, false, false, 1.0f, 1.0f
        ),
        0.0f
    );
    TestTrue(
        TEXT("Metal-like surfaces amplify movement noise"),
        ABHCharacter::CalculateMovementNoiseLoudness(
            400.0f, false, false, false, 1.35f, 1.0f
        ) > WalkNoise
    );
    TestTrue(
        TEXT("Equipment load amplifies movement noise"),
        ABHCharacter::CalculateMovementNoiseLoudness(
            400.0f, false, false, false, 1.0f, 1.25f
        ) > WalkNoise
    );
    TestTrue(
        TEXT("Sprint cadence is faster than walk cadence"),
        ABHCharacter::CalculateFootstepInterval(700.0f, true, false, false) <
            ABHCharacter::CalculateFootstepInterval(400.0f, false, false, false)
    );
    TestTrue(
        TEXT("Prone cadence is slower than crouched cadence"),
        ABHCharacter::CalculateFootstepInterval(140.0f, false, false, true) >
            ABHCharacter::CalculateFootstepInterval(260.0f, false, true, false)
    );

    const FName FootstepFields[] = {
        FName(TEXT("DefaultFootstepSound")),
        FName(TEXT("ConcreteFootstepSound")),
        FName(TEXT("DirtFootstepSound")),
        FName(TEXT("GrassFootstepSound")),
        FName(TEXT("MetalFootstepSound")),
        FName(TEXT("WaterFootstepSound")),
        FName(TEXT("EquipmentNoiseMultiplier"))
    };
    for (const FName FieldName : FootstepFields)
    {
        TestNotNull(
            *FString::Printf(TEXT("Movement audio field %s remains authorable"), *FieldName.ToString()),
            ABHCharacter::StaticClass()->FindPropertyByName(FieldName)
        );
    }
    const UFunction* MulticastFunction = ABHCharacter::StaticClass()->FindFunctionByName(
        FName(TEXT("MulticastPlayFootstep"))
    );
    TestTrue(
        TEXT("Footstep presentation replicates to all nearby players"),
        MulticastFunction && MulticastFunction->HasAnyFunctionFlags(FUNC_NetMulticast)
    );
    return true;
}

bool FBHEnemyVoiceBarkContractTest::RunTest(const FString& Parameters)
{
    TestFalse(
        TEXT("AI bark cooldown rejects early repeated speech"),
        ABHEnemySoldier::CanPlayBark(11.0f, 10.0f, 2.5f)
    );
    TestTrue(
        TEXT("AI bark cooldown permits speech after its interval"),
        ABHEnemySoldier::CanPlayBark(12.5f, 10.0f, 2.5f)
    );
    const FName BarkFields[] = {
        FName(TEXT("AlertBark")),
        FName(TEXT("ContactBark")),
        FName(TEXT("ReloadBark")),
        FName(TEXT("GrenadeBark")),
        FName(TEXT("CasualtyBark")),
        FName(TEXT("RetreatBark")),
        FName(TEXT("SearchBark"))
    };
    for (const FName FieldName : BarkFields)
    {
        TestNotNull(
            *FString::Printf(TEXT("AI voice asset %s remains authorable"), *FieldName.ToString()),
            ABHEnemySoldier::StaticClass()->FindPropertyByName(FieldName)
        );
    }
    const UFunction* BarkFunction = ABHEnemySoldier::StaticClass()->FindFunctionByName(
        FName(TEXT("PlayBark"))
    );
    TestTrue(
        TEXT("AI bark requests remain Blueprint-callable"),
        BarkFunction && BarkFunction->HasAnyFunctionFlags(FUNC_BlueprintCallable)
    );
    const UFunction* MulticastFunction = ABHEnemySoldier::StaticClass()->FindFunctionByName(
        FName(TEXT("MulticastPlayBark"))
    );
    TestTrue(
        TEXT("AI barks replicate as multicast presentation"),
        MulticastFunction && MulticastFunction->HasAnyFunctionFlags(FUNC_NetMulticast)
    );
    return true;
}

bool FBHMissionRadioSubtitleContractTest::RunTest(
    const FString& Parameters
)
{
    const UScriptStruct* ObjectiveStruct = FBHObjectiveDefinition::StaticStruct();
    TestNotNull(TEXT("Mission objectives expose reflected radio metadata"), ObjectiveStruct);
    const FName RadioFields[] = {
        GET_MEMBER_NAME_CHECKED(FBHObjectiveDefinition, RadioSpeaker),
        GET_MEMBER_NAME_CHECKED(FBHObjectiveDefinition, ActivationRadioLine),
        GET_MEMBER_NAME_CHECKED(FBHObjectiveDefinition, CompletionRadioLine),
        GET_MEMBER_NAME_CHECKED(FBHObjectiveDefinition, RadioSubtitleDuration),
        GET_MEMBER_NAME_CHECKED(FBHObjectiveDefinition, bRadioHasDirection),
        GET_MEMBER_NAME_CHECKED(FBHObjectiveDefinition, RadioDirectionDegrees)
    };
    for (const FName FieldName : RadioFields)
    {
        TestNotNull(
            *FString::Printf(TEXT("Mission radio field %s remains authorable"), *FieldName.ToString()),
            ObjectiveStruct ? ObjectiveStruct->FindPropertyByName(FieldName) : nullptr
        );
    }
    const UFunction* ShowSubtitleFunction =
        ABHCharacter::StaticClass()->FindFunctionByName(FName(TEXT("ShowSubtitle")));
    TestTrue(
        TEXT("Mission and Blueprint gameplay can present accessible subtitles"),
        ShowSubtitleFunction && ShowSubtitleFunction->HasAnyFunctionFlags(FUNC_BlueprintCallable)
    );
    return true;
}

bool FBHAccessibilitySettingsContractTest::RunTest(
    const FString& Parameters
)
{
    const UBHUserSettingsSaveGame* Defaults =
        GetDefault<UBHUserSettingsSaveGame>();
    TestNotNull(TEXT("Accessibility settings save defaults exist"), Defaults);
    if (!Defaults)
    {
        return false;
    }

    TestEqual(
        TEXT("Settings schema includes prompt preference migration"),
        Defaults->SchemaVersion,
        8
    );
    TestEqual(TEXT("Default horizontal sensitivity is neutral"), Defaults->HorizontalLookSensitivity, 1.0f);
    TestEqual(TEXT("Default vertical sensitivity is neutral"), Defaults->VerticalLookSensitivity, 1.0f);
    TestEqual(TEXT("Default ADS sensitivity is deliberate"), Defaults->ADSSensitivityMultiplier, 0.75f);
    TestFalse(TEXT("Vertical inversion remains opt-in"), Defaults->bInvertVerticalLook);
    TestFalse(TEXT("Aim defaults to hold"), Defaults->bToggleAim);
    TestFalse(TEXT("Sprint defaults to hold"), Defaults->bToggleSprint);
    TestFalse(TEXT("Crouch defaults to hold"), Defaults->bToggleCrouch);
    TestTrue(TEXT("Prone preserves its toggle default"), Defaults->bToggleProne);
    TestFalse(TEXT("Lean defaults to hold"), Defaults->bToggleLean);
    TestFalse(TEXT("Interaction preserves its tap default"), Defaults->bHoldInteraction);
    TestEqual(TEXT("Camera shake defaults to full presentation"), Defaults->CameraShakeScale, 1.0f);
    TestEqual(TEXT("Recoil motion defaults to full presentation"), Defaults->RecoilAnimationScale, 1.0f);
    TestEqual(TEXT("Head bob defaults to full presentation"), Defaults->HeadBobScale, 1.0f);
    TestEqual(TEXT("Damage flash defaults to full presentation"), Defaults->HitFlashScale, 1.0f);
    TestTrue(TEXT("Motion blur remains enabled by default"), Defaults->bMotionBlurEnabled);
    TestTrue(TEXT("Depth of field remains enabled by default"), Defaults->bDepthOfFieldEnabled);
    TestTrue(TEXT("Chromatic aberration remains enabled by default"), Defaults->bChromaticAberrationEnabled);
    TestTrue(TEXT("Subtitles remain enabled by default"), Defaults->bSubtitlesEnabled);
    TestTrue(TEXT("Speaker labels remain enabled by default"), Defaults->bSubtitleSpeakerLabels);
    TestTrue(TEXT("Direction indicators remain enabled by default"), Defaults->bSubtitleDirectionalIndicators);
    TestEqual(TEXT("Subtitle size defaults to standard"), Defaults->SubtitleTextScale, 1.0f);
    TestEqual(TEXT("Subtitle background defaults readable"), Defaults->SubtitleBackgroundOpacity, 0.75f);
    TestEqual(TEXT("UI defaults inside the safe frame"), Defaults->UISafeAreaScale, 0.95f);
    const FName LookSaveFields[] = {
        GET_MEMBER_NAME_CHECKED(
            UBHUserSettingsSaveGame,
            HorizontalLookSensitivity
        ),
        GET_MEMBER_NAME_CHECKED(
            UBHUserSettingsSaveGame,
            VerticalLookSensitivity
        ),
        GET_MEMBER_NAME_CHECKED(
            UBHUserSettingsSaveGame,
            ADSSensitivityMultiplier
        ),
        GET_MEMBER_NAME_CHECKED(
            UBHUserSettingsSaveGame,
            bInvertVerticalLook
        )
    };
    for (const FName FieldName : LookSaveFields)
    {
        const FProperty* Property =
            UBHUserSettingsSaveGame::StaticClass()->FindPropertyByName(
                FieldName
            );
        TestTrue(
            *FString::Printf(
                TEXT("%s is part of the persisted settings contract"),
                *FieldName.ToString()
            ),
            Property && Property->HasAnyPropertyFlags(CPF_SaveGame)
        );
    }
    const FName BindingSaveFields[] = {
        GET_MEMBER_NAME_CHECKED(
            UBHUserSettingsSaveGame,
            KeyboardBindings
        ),
        GET_MEMBER_NAME_CHECKED(
            UBHUserSettingsSaveGame,
            GamepadBindings
        )
    };
    for (const FName FieldName : BindingSaveFields)
    {
        const FProperty* Property =
            UBHUserSettingsSaveGame::StaticClass()->FindPropertyByName(
                FieldName
            );
        TestTrue(
            *FString::Printf(
                TEXT("%s is part of the persisted settings contract"),
                *FieldName.ToString()
            ),
            Property && Property->HasAnyPropertyFlags(CPF_SaveGame)
        );
    }
    const FName InputModeSaveFields[] = {
        GET_MEMBER_NAME_CHECKED(UBHUserSettingsSaveGame, bToggleAim),
        GET_MEMBER_NAME_CHECKED(UBHUserSettingsSaveGame, bToggleSprint),
        GET_MEMBER_NAME_CHECKED(UBHUserSettingsSaveGame, bToggleCrouch),
        GET_MEMBER_NAME_CHECKED(UBHUserSettingsSaveGame, bToggleProne),
        GET_MEMBER_NAME_CHECKED(UBHUserSettingsSaveGame, bToggleLean),
        GET_MEMBER_NAME_CHECKED(UBHUserSettingsSaveGame, bHoldInteraction)
    };
    for (const FName FieldName : InputModeSaveFields)
    {
        const FProperty* Property =
            UBHUserSettingsSaveGame::StaticClass()->FindPropertyByName(FieldName);
        TestTrue(
            *FString::Printf(TEXT("%s persists across sessions"), *FieldName.ToString()),
            Property && Property->HasAnyPropertyFlags(CPF_SaveGame)
        );
    }
    const FName VisualComfortSaveFields[] = {
        GET_MEMBER_NAME_CHECKED(UBHUserSettingsSaveGame, CameraShakeScale),
        GET_MEMBER_NAME_CHECKED(UBHUserSettingsSaveGame, RecoilAnimationScale),
        GET_MEMBER_NAME_CHECKED(UBHUserSettingsSaveGame, HeadBobScale),
        GET_MEMBER_NAME_CHECKED(UBHUserSettingsSaveGame, HitFlashScale),
        GET_MEMBER_NAME_CHECKED(UBHUserSettingsSaveGame, bMotionBlurEnabled),
        GET_MEMBER_NAME_CHECKED(UBHUserSettingsSaveGame, bDepthOfFieldEnabled),
        GET_MEMBER_NAME_CHECKED(UBHUserSettingsSaveGame, bChromaticAberrationEnabled)
    };
    for (const FName FieldName : VisualComfortSaveFields)
    {
        const FProperty* Property =
            UBHUserSettingsSaveGame::StaticClass()->FindPropertyByName(FieldName);
        TestTrue(
            *FString::Printf(TEXT("%s persists across sessions"), *FieldName.ToString()),
            Property && Property->HasAnyPropertyFlags(CPF_SaveGame)
        );
    }
    const FName SubtitleSaveFields[] = {
        GET_MEMBER_NAME_CHECKED(UBHUserSettingsSaveGame, bSubtitlesEnabled),
        GET_MEMBER_NAME_CHECKED(UBHUserSettingsSaveGame, bSubtitleSpeakerLabels),
        GET_MEMBER_NAME_CHECKED(UBHUserSettingsSaveGame, bSubtitleDirectionalIndicators),
        GET_MEMBER_NAME_CHECKED(UBHUserSettingsSaveGame, SubtitleTextScale),
        GET_MEMBER_NAME_CHECKED(UBHUserSettingsSaveGame, SubtitleBackgroundOpacity),
        GET_MEMBER_NAME_CHECKED(UBHUserSettingsSaveGame, UISafeAreaScale)
    };
    for (const FName FieldName : SubtitleSaveFields)
    {
        const FProperty* Property = UBHUserSettingsSaveGame::StaticClass()->FindPropertyByName(FieldName);
        TestTrue(
            *FString::Printf(TEXT("%s persists across sessions"), *FieldName.ToString()),
            Property && Property->HasAnyPropertyFlags(CPF_SaveGame)
        );
    }
    TestEqual(TEXT("Default HUD scale is neutral"), Defaults->HUDScale, 1.0f);
    TestFalse(TEXT("High contrast remains opt-in"), Defaults->bHighContrastHUD);
    TestFalse(TEXT("Reduced motion remains opt-in"), Defaults->bReducedMotion);

    const EBHColorVisionMode Modes[] = {
        EBHColorVisionMode::Standard,
        EBHColorVisionMode::Deuteranopia,
        EBHColorVisionMode::Protanopia,
        EBHColorVisionMode::Tritanopia
    };
    for (const EBHColorVisionMode Mode : Modes)
    {
        const FLinearColor FriendlyColor = BHUIStyle::ResolveFriendlyColor(Mode);
        const FLinearColor DangerColor = BHUIStyle::ResolveDangerColor(Mode);
        const FVector FriendlyRGB(FriendlyColor.R, FriendlyColor.G, FriendlyColor.B);
        const FVector DangerRGB(DangerColor.R, DangerColor.G, DangerColor.B);
        TestTrue(
            TEXT("Friendly and danger cues remain visibly separated in every palette"),
            FVector::Dist(FriendlyRGB, DangerRGB) > 0.45f
        );
    }

    const FVector2D HipLook =
        UBHUserSettingsSubsystem::CalculateLookInput(
            FVector2D(1.0f, 1.0f),
            2.0f,
            0.5f,
            0.75f,
            false,
            false
        );
    TestTrue(
        TEXT("Horizontal and vertical look sensitivity are independent"),
        HipLook.Equals(FVector2D(2.0f, 0.5f))
    );
    const FVector2D ADSInvertedLook =
        UBHUserSettingsSubsystem::CalculateLookInput(
            FVector2D(1.0f, 1.0f),
            2.0f,
            0.5f,
            0.75f,
            true,
            true
        );
    TestTrue(
        TEXT("ADS multiplier and vertical inversion compose deterministically"),
        ADSInvertedLook.Equals(FVector2D(1.5f, -0.375f))
    );

    const UFunction* LookSettingsFunction =
        UBHUserSettingsSubsystem::StaticClass()->FindFunctionByName(
            FName(TEXT("ApplyLookSettings"))
        );
    TestTrue(
        TEXT("Independent look settings remain Blueprint-callable"),
        LookSettingsFunction &&
            LookSettingsFunction->HasAnyFunctionFlags(
                FUNC_BlueprintCallable
            )
    );

    const TArray<FBHInputBindingDefinition>& Bindings =
        UBHUserSettingsSubsystem::GetDefaultInputBindingDefinitions();
    TestEqual(
        TEXT("Every gameplay control has a stable remapping definition"),
        Bindings.Num(),
        33
    );
    TestTrue(
        TEXT("Tactical flashlight has a stable remappable binding"),
        Bindings.ContainsByPredicate(
            [](const FBHInputBindingDefinition& Binding)
            {
                return Binding.BindingID == FName(TEXT("Flashlight")) &&
                    Binding.DefaultKeyboardKey == EKeys::L;
            }));
    TestTrue(
        TEXT("Weapon bash has a stable remappable binding"),
        Bindings.ContainsByPredicate(
            [](const FBHInputBindingDefinition& Binding)
            {
                return Binding.BindingID == FName(TEXT("WeaponBash")) &&
                    Binding.DefaultKeyboardKey == EKeys::T;
            }));
    TestTrue(
        TEXT("Field observation has a stable remappable binding"),
        Bindings.ContainsByPredicate(
            [](const FBHInputBindingDefinition& Binding)
            {
                return Binding.BindingID == FName(TEXT("FieldObservation")) &&
                    Binding.DefaultKeyboardKey == EKeys::O;
            }));
    TestTrue(
        TEXT("Controlled breathing has a stable remappable binding"),
        Bindings.ContainsByPredicate(
            [](const FBHInputBindingDefinition& Binding)
            {
                return Binding.BindingID == FName(TEXT("ControlledBreathing")) &&
                    Binding.DefaultKeyboardKey == EKeys::LeftAlt;
            }));
    TSet<FName> BindingIDs;
    TSet<FKey> KeyboardKeys;
    TSet<FKey> GamepadKeys;
    for (const FBHInputBindingDefinition& Binding : Bindings)
    {
        TestFalse(TEXT("Binding ID is unique"), BindingIDs.Contains(Binding.BindingID));
        BindingIDs.Add(Binding.BindingID);
        if (Binding.DefaultKeyboardKey.IsValid())
        {
            TestFalse(
                TEXT("Keyboard and mouse defaults do not conflict"),
                KeyboardKeys.Contains(Binding.DefaultKeyboardKey)
            );
            TestFalse(
                TEXT("Keyboard binding is not a gamepad key"),
                Binding.DefaultKeyboardKey.IsGamepadKey()
            );
            KeyboardKeys.Add(Binding.DefaultKeyboardKey);
        }
        if (Binding.DefaultGamepadKey.IsValid())
        {
            TestFalse(
                TEXT("Controller defaults do not conflict"),
                GamepadKeys.Contains(Binding.DefaultGamepadKey)
            );
            TestTrue(
                TEXT("Controller binding uses a gamepad key"),
                Binding.DefaultGamepadKey.IsGamepadKey()
            );
            GamepadKeys.Add(Binding.DefaultGamepadKey);
        }
    }
    TestTrue(
        TEXT("Handheld smoke has a stable remappable input definition"),
        BindingIDs.Contains(FName(TEXT("SmokeGrenade")))
    );
    TestTrue(TEXT("Movement stick is remappable"), BindingIDs.Contains(TEXT("MoveStick")));
    TestTrue(TEXT("Look stick is remappable"), BindingIDs.Contains(TEXT("LookStick")));
    TestTrue(TEXT("Squad ping is remappable"), BindingIDs.Contains(TEXT("SquadPing")));
    TestTrue(TEXT("Medical controls are remappable"),
        BindingIDs.Contains(TEXT("FieldDressing")) && BindingIDs.Contains(TEXT("Medkit")));
    TestTrue(TEXT("Engineering control is remappable"),
        BindingIDs.Contains(TEXT("Engineering")));

    const FString ExpectedInteractPrompt = FString::Printf(
        TEXT("%s / FACE TOP"),
        *EKeys::F.GetDisplayName(false).ToString().ToUpper()
    );
    TestEqual(
        TEXT("Binding prompts expose keyboard and controller controls"),
        UBHUserSettingsSubsystem::BuildInputBindingPrompt(
            EKeys::F,
            EKeys::Gamepad_FaceButton_Top
        ),
        ExpectedInteractPrompt
    );
    TestEqual(
        TEXT("Single-device controls do not render an empty separator"),
        UBHUserSettingsSubsystem::BuildInputBindingPrompt(
            EKeys::Q,
            FKey()
        ),
        EKeys::Q.GetDisplayName(false).ToString().ToUpper()
    );
    TestEqual(
        TEXT("Controller prompts use compact platform-neutral labels"),
        UBHUserSettingsSubsystem::BuildInputBindingPrompt(
            EKeys::H,
            EKeys::Gamepad_LeftShoulder
        ),
        FString(TEXT("H / L SHOULDER"))
    );
    TestEqual(
        TEXT("Auto prompts begin with keyboard and mouse labels"),
        UBHUserSettingsSubsystem::BuildInputBindingPromptForMode(
            EKeys::F,
            EKeys::Gamepad_FaceButton_Top,
            EBHInputPromptMode::Auto,
            false
        ),
        FString(TEXT("F"))
    );
    TestEqual(
        TEXT("Auto prompts follow the last gamepad input"),
        UBHUserSettingsSubsystem::BuildInputBindingPromptForMode(
            EKeys::F,
            EKeys::Gamepad_FaceButton_Top,
            EBHInputPromptMode::Auto,
            true
        ),
        FString(TEXT("FACE TOP"))
    );
    TestEqual(
        TEXT("Explicit dual prompts retain both remapped controls"),
        UBHUserSettingsSubsystem::BuildInputBindingPromptForMode(
            EKeys::F,
            EKeys::Gamepad_FaceButton_Top,
            EBHInputPromptMode::Both,
            true
        ),
        ExpectedInteractPrompt
    );
    TestEqual(
        TEXT("Missing preferred-device bindings fall back safely"),
        UBHUserSettingsSubsystem::BuildInputBindingPromptForMode(
            EKeys::Q,
            FKey(),
            EBHInputPromptMode::Gamepad,
            true
        ),
        FString(TEXT("Q"))
    );
    TestEqual(
        TEXT("User settings schema includes prompt preference migration"),
        GetDefault<UBHUserSettingsSaveGame>()->SchemaVersion,
        8
    );
    UGameInstance* PromptTestGameInstance = NewObject<UGameInstance>();
    const UBHUserSettingsSubsystem* PromptSettings =
        NewObject<UBHUserSettingsSubsystem>(PromptTestGameInstance);
    TestEqual(
        TEXT("Legacy interact text resolves through the binding contract"),
        PromptSettings->ResolveLegacyInputPrompts(
            FText::FromString(TEXT("Press [F] to interact"))
        ).ToString(),
        FString(TEXT("Press [F] to interact"))
    );

    const UFunction* ApplyBindingsFunction =
        UBHUserSettingsSubsystem::StaticClass()->FindFunctionByName(
            FName(TEXT("ApplyInputBindings"))
        );
    TestTrue(
        TEXT("Batch input remapping remains Blueprint-callable"),
        ApplyBindingsFunction &&
            ApplyBindingsFunction->HasAnyFunctionFlags(FUNC_BlueprintCallable)
    );
    const UFunction* BindingPromptFunction =
        UBHUserSettingsSubsystem::StaticClass()->FindFunctionByName(
            FName(TEXT("GetInputBindingPrompt"))
        );
    TestTrue(
        TEXT("Dynamic binding prompts remain available to Blueprint UI"),
        BindingPromptFunction &&
            BindingPromptFunction->HasAnyFunctionFlags(FUNC_BlueprintCallable)
    );
    const UFunction* PromptModeFunction =
        UBHUserSettingsSubsystem::StaticClass()->FindFunctionByName(
            FName(TEXT("ApplyInputPromptMode"))
        );
    TestTrue(
        TEXT("Input prompt preference remains Blueprint-callable"),
        PromptModeFunction &&
            PromptModeFunction->HasAnyFunctionFlags(FUNC_BlueprintCallable)
    );

    TestTrue(
        TEXT("Solo pause menu may pause its standalone world"),
        ABHCharacter::ShouldPauseWorldForMenu(NM_Standalone)
    );
    TestFalse(
        TEXT("Listen-server menu never pauses the shared session"),
        ABHCharacter::ShouldPauseWorldForMenu(NM_ListenServer)
    );
    TestFalse(
        TEXT("Dedicated-server world is never paused by a menu"),
        ABHCharacter::ShouldPauseWorldForMenu(NM_DedicatedServer)
    );
    TestFalse(
        TEXT("Client menu never requests a shared world pause"),
        ABHCharacter::ShouldPauseWorldForMenu(NM_Client)
    );
    TestTrue(
        TEXT("Hold press activates an inactive action"),
        ABHCharacter::ResolveToggleHoldState(false, false, true)
    );
    TestFalse(
        TEXT("Hold release deactivates an active action"),
        ABHCharacter::ResolveToggleHoldState(true, false, false)
    );
    TestTrue(
        TEXT("Toggle press activates an inactive action"),
        ABHCharacter::ResolveToggleHoldState(false, true, true)
    );
    TestFalse(
        TEXT("Second toggle press deactivates an active action"),
        ABHCharacter::ResolveToggleHoldState(true, true, true)
    );
    TestTrue(
        TEXT("Toggle release preserves the active state"),
        ABHCharacter::ResolveToggleHoldState(true, true, false)
    );
    TestFalse(
        TEXT("Short interaction hold does not commit"),
        ABHCharacter::ShouldCommitHeldInteraction(0.2f, 0.35f)
    );
    TestTrue(
        TEXT("Completed interaction hold commits"),
        ABHCharacter::ShouldCommitHeldInteraction(0.35f, 0.35f)
    );
    const UFunction* InputModeFunction =
        UBHUserSettingsSubsystem::StaticClass()->FindFunctionByName(
            FName(TEXT("ApplyInputModeSettings"))
        );
    TestTrue(
        TEXT("Toggle and hold settings remain Blueprint-callable"),
        InputModeFunction &&
            InputModeFunction->HasAnyFunctionFlags(FUNC_BlueprintCallable)
    );
    const UFunction* VisualComfortFunction =
        UBHUserSettingsSubsystem::StaticClass()->FindFunctionByName(
            FName(TEXT("ApplyVisualComfortSettings"))
        );
    TestTrue(
        TEXT("Visual comfort settings remain Blueprint-callable"),
        VisualComfortFunction &&
            VisualComfortFunction->HasAnyFunctionFlags(FUNC_BlueprintCallable)
    );
    const UFunction* SubtitleSettingsFunction =
        UBHUserSettingsSubsystem::StaticClass()->FindFunctionByName(
            FName(TEXT("ApplySubtitleSettings"))
        );
    TestTrue(
        TEXT("Subtitle settings remain Blueprint-callable"),
        SubtitleSettingsFunction && SubtitleSettingsFunction->HasAnyFunctionFlags(FUNC_BlueprintCallable)
    );
    TestEqual(TEXT("Front subtitle uses upward direction glyph"),
        UBHSubtitleWidget::BuildDirectionalIndicator(0.0f).ToString(), FString(TEXT("^")));
    TestEqual(TEXT("Right subtitle uses right direction glyph"),
        UBHSubtitleWidget::BuildDirectionalIndicator(90.0f).ToString(), FString(TEXT(">")));
    TestEqual(TEXT("Behind subtitle uses downward direction glyph"),
        UBHSubtitleWidget::BuildDirectionalIndicator(180.0f).ToString(), FString(TEXT("v")));
    const FMargin DefaultSafeInsets = BHUIStyle::CalculateSafeAreaInsets(0.95f);
    TestTrue(TEXT("Default safe area provides equal normalized edge insets"),
        FMath::IsNearlyEqual(DefaultSafeInsets.Left, 0.025f) &&
        FMath::IsNearlyEqual(DefaultSafeInsets.Top, 0.025f) &&
        FMath::IsNearlyEqual(DefaultSafeInsets.Right, 0.025f) &&
        FMath::IsNearlyEqual(DefaultSafeInsets.Bottom, 0.025f));
    TestTrue(TEXT("Safe area clamps profiles to at least eighty percent"),
        FMath::IsNearlyEqual(BHUIStyle::CalculateSafeAreaInsets(0.5f).Left, 0.1f));
    TestTrue(TEXT("HUD scale clamps at the supported minimum"),
        FMath::IsNearlyEqual(BHUIStyle::ResolveContextScale(
            0.25f, EBHUIStyleContext::Gameplay), 0.75f));
    TestTrue(TEXT("HUD scale clamps at the supported maximum"),
        FMath::IsNearlyEqual(BHUIStyle::ResolveContextScale(
            2.0f, EBHUIStyleContext::Overlay), 1.5f));
    TestTrue(TEXT("HUD scale does not resize menus"),
        FMath::IsNearlyEqual(BHUIStyle::ResolveContextScale(
            1.5f, EBHUIStyleContext::Menu), 1.0f));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBHFieldFortificationRulesTest,
    "BrokenHorizon.PersistentWar.Engineering.FieldFortification",
    EAutomationTestFlags::EditorContext |
        EAutomationTestFlags::EngineFilter
)

bool FBHFieldFortificationRulesTest::RunTest(
    const FString& Parameters
)
{
    FBHWarSectorState FriendlySector;
    FriendlySector.SectorID = TEXT("WesternFOB");
    FriendlySector.Owner = EBHWarFaction::Friendly;
    FriendlySector.Supply = 12.0f;

    TestTrue(
        TEXT("Friendly sectors can fund an exact-cost barricade"),
        ABHFieldFortification::CanConstructForSector(
            FriendlySector,
            12.0f
        )
    );
    FriendlySector.Supply = 11.9f;
    TestFalse(
        TEXT("Construction rejects insufficient sector supply"),
        ABHFieldFortification::CanConstructForSector(
            FriendlySector,
            12.0f
        )
    );
    FriendlySector.Supply = 100.0f;
    FriendlySector.Owner = EBHWarFaction::Enemy;
    TestFalse(
        TEXT("Enemy-controlled sectors reject construction"),
        ABHFieldFortification::CanConstructForSector(
            FriendlySector,
            12.0f
        )
    );
    TestEqual(
        TEXT("Half-damaged barricades cost half the full repair amount"),
        ABHFieldFortification::CalculateRepairSupplyCost(0.5f, 6.0f),
        3.0f
    );
    TestEqual(
        TEXT("Destroyed barricades require the full repair amount"),
        ABHFieldFortification::CalculateRepairSupplyCost(0.0f, 6.0f),
        6.0f
    );
    TestEqual(
        TEXT("Full-health barricades have no repair cost"),
        ABHFieldFortification::CalculateRepairSupplyCost(1.0f, 6.0f),
        0.0f
    );
    TestTrue(
        TEXT("Construction progress reaches halfway after half the work"),
        FMath::IsNearlyEqual(
            ABHFieldFortification::CalculateWorkProgress(2.5f, 5.0f),
            0.5f
        )
    );
    TestTrue(
        TEXT("Construction progress clamps at completion"),
        FMath::IsNearlyEqual(
            ABHFieldFortification::CalculateWorkProgress(8.0f, 5.0f),
            1.0f
        )
    );
    TestTrue(
        TEXT("A second player materially accelerates construction"),
        FMath::IsNearlyEqual(
            ABHFieldFortification::CalculateAssistedWorkRate(
                2, 0.65f, 2.3f),
            1.65f
        )
    );
    TestTrue(
        TEXT("Cooperative acceleration remains bounded"),
        FMath::IsNearlyEqual(
            ABHFieldFortification::CalculateAssistedWorkRate(
                8, 0.65f, 2.3f),
            2.3f
        )
    );
    TestEqual(
        TEXT("Intact defenses recover half their construction materiel"),
        ABHFieldFortification::CalculateDismantleRecovery(
            20.0f, 1.0f, 0.5f),
        10.0f
    );
    TestEqual(
        TEXT("Damaged defenses recover proportionally less materiel"),
        ABHFieldFortification::CalculateDismantleRecovery(
            20.0f, 0.4f, 0.5f),
        4.0f
    );
    TestFalse(
        TEXT("Leaving the work radius interrupts assignment"),
        ABHFieldFortification::ShouldWorkerRemainAssigned(
            true, false, false, 351.0f, 350.0f
        )
    );
    TestFalse(
        TEXT("Incapacitated players cannot continue engineering work"),
        ABHFieldFortification::ShouldWorkerRemainAssigned(
            true, true, false, 100.0f, 350.0f
        )
    );
    const FBHFortificationPlanProfile Hasty =
        ABHFieldFortification::BuildPlanProfile(
            EBHFortificationPlan::HastyBarricade);
    const FBHFortificationPlanProfile Bulwark =
        ABHFieldFortification::BuildPlanProfile(
            EBHFortificationPlan::ReinforcedBulwark);
    const FBHFortificationPlanProfile FiringPosition =
        ABHFieldFortification::BuildPlanProfile(
            EBHFortificationPlan::FiringPosition);
    const FBHFortificationPlanProfile SupplyCache =
        ABHFieldFortification::BuildPlanProfile(
            EBHFortificationPlan::FieldSupplyCache);
    const FBHFortificationPlanProfile ObservationPost =
        ABHFieldFortification::BuildPlanProfile(
            EBHFortificationPlan::ObservationPost);
    const FBHFortificationPlanProfile RallyPoint =
        ABHFieldFortification::BuildPlanProfile(
            EBHFortificationPlan::FieldRallyPoint);
    TestTrue(
        TEXT("Reinforced bulwarks trade more supply and time for durability"),
        Bulwark.ConstructionSupplyCost > Hasty.ConstructionSupplyCost &&
            Bulwark.ConstructionWorkDuration > Hasty.ConstructionWorkDuration &&
            Bulwark.MaximumHealth > Hasty.MaximumHealth
    );
    TestTrue(
        TEXT("Firing positions are faster and lower but less durable"),
        FiringPosition.ConstructionWorkDuration <
                Hasty.ConstructionWorkDuration &&
            FiringPosition.MeshScale.Z < Hasty.MeshScale.Z &&
            FiringPosition.MaximumHealth < Hasty.MaximumHealth
    );
    TestTrue(
        TEXT("Firing positions provide the steadiest weapon support"),
        FiringPosition.WeaponBraceQuality >
                Bulwark.WeaponBraceQuality &&
            Bulwark.WeaponBraceQuality > Hasty.WeaponBraceQuality
    );
    TestTrue(
        TEXT("Reinforced bulwarks are the most trusted AI cover"),
        Bulwark.AICoverQuality > Hasty.AICoverQuality &&
            Hasty.AICoverQuality > FiringPosition.AICoverQuality
    );
    TestTrue(
        TEXT("Reinforced bulwarks contribute the strongest strategic defense"),
        Bulwark.StrategicDefenseValue > Hasty.StrategicDefenseValue &&
            Hasty.StrategicDefenseValue >
                FiringPosition.StrategicDefenseValue
    );
    TestTrue(
        TEXT("Field supply caches trade protection for frontline logistics"),
        SupplyCache.ConstructionSupplyCost >
                Hasty.ConstructionSupplyCost &&
            SupplyCache.AICoverQuality < FiringPosition.AICoverQuality &&
            SupplyCache.StrategicDefenseValue <
                FiringPosition.StrategicDefenseValue &&
            FMath::IsNearlyZero(SupplyCache.WeaponBraceQuality)
    );
    TestTrue(
        TEXT("Observation posts trade protection for persistent-war intelligence"),
        ObservationPost.MaximumHealth < Hasty.MaximumHealth &&
            ObservationPost.AICoverQuality < Hasty.AICoverQuality &&
            ObservationPost.StrategicDefenseValue <
                SupplyCache.StrategicDefenseValue &&
            ObservationPost.MeshScale.Z > Hasty.MeshScale.Z);
    TestTrue(
        TEXT("Field rally points demand substantial logistics but offer weak protection"),
        RallyPoint.ConstructionSupplyCost >
                SupplyCache.ConstructionSupplyCost &&
            RallyPoint.ConstructionWorkDuration >
                SupplyCache.ConstructionWorkDuration &&
            RallyPoint.AICoverQuality < Hasty.AICoverQuality);
    TestTrue(
        TEXT("A healthy stocked rally beyond hostile pressure is safe"),
        ABHFieldFortification::IsRallyDeploymentSafe(
            true, EBHFortificationPlan::FieldRallyPoint,
            1.0f, 2, 1200.0f, 1200.0f));
    TestFalse(
        TEXT("Hostile pressure blocks frontline rally deployment"),
        ABHFieldFortification::IsRallyDeploymentSafe(
            true, EBHFortificationPlan::FieldRallyPoint,
            1.0f, 2, 1199.0f, 1200.0f));
    TestFalse(
        TEXT("A badly damaged rally cannot receive replacements"),
        ABHFieldFortification::IsRallyDeploymentSafe(
            true, EBHFortificationPlan::FieldRallyPoint,
            0.49f, 2, BIG_NUMBER, 1200.0f));
    const FProperty* SupplyCacheChargesProperty = FindFProperty<FProperty>(
        ABHFieldFortification::StaticClass(),
        FName(TEXT("SupplyCacheChargesRemaining")));
    TestTrue(
        TEXT("Field supply cache charges replicate to every player"),
        SupplyCacheChargesProperty &&
            SupplyCacheChargesProperty->HasAnyPropertyFlags(CPF_Net));
    TestTrue(
        TEXT("Save schema includes persistent field supply cache charges"),
        BHSave::CurrentSchemaVersion >= 49);
    const FProperty* ObservationProgressProperty = FindFProperty<FProperty>(
        ABHFieldFortification::StaticClass(),
        FName(TEXT("ObservationProgress")));
    const FProperty* ObservationTurnProperty = FindFProperty<FProperty>(
        ABHFieldFortification::StaticClass(),
        FName(TEXT("LastObservationTurn")));
    TestTrue(
        TEXT("Observation progress replicates to all squad members"),
        ObservationProgressProperty &&
            ObservationProgressProperty->HasAnyPropertyFlags(CPF_Net));
    TestTrue(
        TEXT("Observation availability replicates to all squad members"),
        ObservationTurnProperty &&
            ObservationTurnProperty->HasAnyPropertyFlags(CPF_Net));
    TestNotNull(
        TEXT("Observation report turns persist in schema 51 saves"),
        FindFProperty<FProperty>(
            FBHFieldFortificationSaveState::StaticStruct(),
            GET_MEMBER_NAME_CHECKED(
                FBHFieldFortificationSaveState, LastObservationTurn)));
    TestTrue(
        TEXT("Save schema includes persistent observation post reports"),
        BHSave::CurrentSchemaVersion >= 51);
    const FProperty* RallyDeploymentsProperty = FindFProperty<FProperty>(
        ABHFieldFortification::StaticClass(),
        FName(TEXT("RallyDeploymentsRemaining")));
    TestTrue(
        TEXT("Rally deployment capacity replicates to every player"),
        RallyDeploymentsProperty &&
            RallyDeploymentsProperty->HasAnyPropertyFlags(CPF_Net));
    TestNotNull(
        TEXT("Rally deployment capacity persists in fortification saves"),
        FindFProperty<FProperty>(
            FBHFieldFortificationSaveState::StaticStruct(),
            GET_MEMBER_NAME_CHECKED(
                FBHFieldFortificationSaveState,
                RallyDeploymentsRemaining)));
    TestTrue(
        TEXT("Save schema includes persistent rally deployments"),
        BHSave::CurrentSchemaVersion >= 53);
    TestTrue(
        TEXT("Two reinforced positions reduce controlling-force casualties by about one third"),
        FMath::IsNearlyEqual(
            UBHWarSubsystem::CalculateFortificationCasualtyMultiplier(16.0f),
            0.68f,
            0.001f)
    );
    TestEqual(
        TEXT("Fortification casualty reduction remains capped"),
        UBHWarSubsystem::CalculateFortificationCasualtyMultiplier(100.0f),
        0.65f
    );
    TestTrue(
        TEXT("AI will accept a short detour for substantially safer cover"),
        ABHEnemyAIController::CalculateCoverSelectionScore(
            700.0f, 1000.0f, 1.0f) <
        ABHEnemyAIController::CalculateCoverSelectionScore(
            500.0f, 1000.0f, 0.35f)
    );
    TestTrue(
        TEXT("Damaged fortification quality weakens cover selection benefit"),
        ABHEnemyAIController::CalculateCoverSelectionScore(
            500.0f, 1000.0f, 0.25f) >
        ABHEnemyAIController::CalculateCoverSelectionScore(
            500.0f, 1000.0f, 1.0f)
    );
    const FProperty* WorkProgressProperty = FindFProperty<FProperty>(
        ABHFieldFortification::StaticClass(),
        FName(TEXT("WorkProgress"))
    );
    const FProperty* WorkerCountProperty = FindFProperty<FProperty>(
        ABHFieldFortification::StaticClass(),
        FName(TEXT("ActiveWorkerCount"))
    );
    const FProperty* DismantleWorkProperty = FindFProperty<FProperty>(
        ABHFieldFortification::StaticClass(),
        FName(TEXT("bDismantleWork"))
    );
    TestTrue(
        TEXT("Construction progress replicates"),
        WorkProgressProperty &&
            WorkProgressProperty->HasAnyPropertyFlags(CPF_Net)
    );
    TestTrue(
        TEXT("Cooperative worker count replicates"),
        WorkerCountProperty &&
            WorkerCountProperty->HasAnyPropertyFlags(CPF_Net)
    );
    TestTrue(
        TEXT("Dismantling work state replicates"),
        DismantleWorkProperty &&
            DismantleWorkProperty->HasAnyPropertyFlags(CPF_Net)
    );
    const FProperty* PlanProperty = FindFProperty<FProperty>(
        ABHFieldFortification::StaticClass(),
        FName(TEXT("SelectedPlan"))
    );
    TestTrue(
        TEXT("Selected fortification plans replicate"),
        PlanProperty && PlanProperty->HasAnyPropertyFlags(CPF_Net)
    );
    TestNotNull(
        TEXT("Selected fortification plans persist in schema 48 saves"),
        FindFProperty<FProperty>(
            FBHFieldFortificationSaveState::StaticStruct(),
            GET_MEMBER_NAME_CHECKED(FBHFieldFortificationSaveState, Plan)
        )
    );
    TestNotNull(
        TEXT("Fortification save states remain part of the campaign contract"),
        FindFProperty<FArrayProperty>(
            UBHSaveGame::StaticClass(),
            GET_MEMBER_NAME_CHECKED(
                UBHSaveGame,
                FieldFortificationStates
            )
        )
    );
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBHWeaponRoleProfilesTest,
    "BrokenHorizon.Gameplay.Weapon.RoleProfiles",
    EAutomationTestFlags::EditorContext |
        EAutomationTestFlags::EngineFilter
)

bool FBHWeaponRoleProfilesTest::RunTest(
    const FString& Parameters
)
{
    const FBHWeaponRoleProfile Assault =
        UBHWeaponComponent::BuildWeaponRoleProfile(
            EBHWeaponRole::Assault
        );
    const FBHWeaponRoleProfile Marksman =
        UBHWeaponComponent::BuildWeaponRoleProfile(
            EBHWeaponRole::Marksman
        );
    const FBHWeaponRoleProfile Support =
        UBHWeaponComponent::BuildWeaponRoleProfile(
            EBHWeaponRole::Support
        );

    TestTrue(
        TEXT("Marksman role trades capacity for damage"),
        Marksman.RifleConfig.MagazineSize <
            Assault.RifleConfig.MagazineSize &&
        Marksman.RifleConfig.Damage > Assault.RifleConfig.Damage
    );
    TestTrue(
        TEXT("Marksman role is more accurate while aiming"),
        Marksman.RifleConfig.ADSSpreadDegrees <
            Assault.RifleConfig.ADSSpreadDegrees
    );
    TestFalse(
        TEXT("Marksman role remains semi-automatic"),
        Marksman.RifleConfig.bAutomatic
    );
    TestTrue(
        TEXT("Support role trades accuracy and reload speed for capacity"),
        Support.RifleConfig.MagazineSize >
            Assault.RifleConfig.MagazineSize &&
        Support.RifleConfig.ADSSpreadDegrees >
            Assault.RifleConfig.ADSSpreadDegrees &&
        Support.RifleConfig.ReloadDuration >
            Assault.RifleConfig.ReloadDuration
    );
    TestTrue(
        TEXT("Support role creates stronger suppression pressure"),
        Support.RifleConfig.SuppressionRadius >
            Assault.RifleConfig.SuppressionRadius &&
        Support.RifleConfig.SuppressionAmount >
            Assault.RifleConfig.SuppressionAmount
    );
    TestEqual(
        TEXT("Role cycle advances assault to marksman"),
        UBHWeaponComponent::GetNextWeaponRole(
            EBHWeaponRole::Assault
        ),
        EBHWeaponRole::Marksman
    );
    TestEqual(
        TEXT("Role cycle advances marksman to support"),
        UBHWeaponComponent::GetNextWeaponRole(
            EBHWeaponRole::Marksman
        ),
        EBHWeaponRole::Support
    );
    TestEqual(
        TEXT("Role cycle wraps support to assault"),
        UBHWeaponComponent::GetNextWeaponRole(
            EBHWeaponRole::Support
        ),
        EBHWeaponRole::Assault
    );
    const FProperty* WeaponRoleProperty =
        FindFProperty<FProperty>(
            UBHWeaponComponent::StaticClass(),
            FName(TEXT("WeaponRole"))
        );
    TestTrue(
        TEXT("Weapon role remains a replicated multiplayer contract"),
        WeaponRoleProperty &&
            WeaponRoleProperty->HasAnyPropertyFlags(CPF_Net)
    );
    TestNotNull(
        TEXT("Weapon role remains part of the campaign save contract"),
        FindFProperty<FProperty>(
            UBHSaveGame::StaticClass(),
            GET_MEMBER_NAME_CHECKED(UBHSaveGame, SavedWeaponRole)
        )
    );
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBHCombatantArchetypeProfilesTest,
    "BrokenHorizon.Gameplay.AI.CombatantArchetypes",
    EAutomationTestFlags::EditorContext |
        EAutomationTestFlags::EngineFilter
)

bool FBHCombatantArchetypeProfilesTest::RunTest(
    const FString& Parameters
)
{
    const FBHCombatantArchetypeProfile Rifleman =
        ABHEnemySoldier::BuildCombatantArchetypeProfile(
            EBHCombatantArchetype::Rifleman
        );
    const FBHCombatantArchetypeProfile Scout =
        ABHEnemySoldier::BuildCombatantArchetypeProfile(
            EBHCombatantArchetype::Scout
        );
    const FBHCombatantArchetypeProfile Gunner =
        ABHEnemySoldier::BuildCombatantArchetypeProfile(
            EBHCombatantArchetype::Gunner
        );

    TestTrue(
        TEXT("Scout trades durability and capacity for mobility and reach"),
        Scout.MaximumHealth < Rifleman.MaximumHealth &&
        Scout.MovementSpeed > Rifleman.MovementSpeed &&
        Scout.MagazineCapacity < Rifleman.MagazineCapacity &&
        Scout.MaximumEngagementDistance >
            Rifleman.MaximumEngagementDistance
    );
    TestTrue(
        TEXT("Scout repositions and seeks cover more aggressively"),
        Scout.CombatRepositionInterval <
            Rifleman.CombatRepositionInterval &&
        Scout.CombatRepositionRadius >
            Rifleman.CombatRepositionRadius &&
        Scout.SuppressionCoverThreshold <
            Rifleman.SuppressionCoverThreshold
    );
    TestTrue(
        TEXT("Gunner provides sustained fire and greater suppression tolerance"),
        Gunner.MagazineCapacity > Rifleman.MagazineCapacity &&
        Gunner.MaximumBurstShots > Rifleman.MaximumBurstShots &&
        Gunner.FireInterval < Rifleman.FireInterval &&
        Gunner.RetreatSuppressionThreshold >
            Rifleman.RetreatSuppressionThreshold
    );
    TestTrue(
        TEXT("Gunner trades mobility for armor and durability"),
        Gunner.MovementSpeed < Rifleman.MovementSpeed &&
        Gunner.MaximumHealth > Rifleman.MaximumHealth &&
        Gunner.bHasBodyArmor
    );
    TestEqual(
        TEXT("Three-unit formations include a scout"),
        ABHEnemySoldier::ChooseFormationArchetype(1, 3),
        EBHCombatantArchetype::Scout
    );
    TestEqual(
        TEXT("Three-unit formations include a gunner"),
        ABHEnemySoldier::ChooseFormationArchetype(2, 3),
        EBHCombatantArchetype::Gunner
    );
    TestEqual(
        TEXT("Solo combatants remain riflemen"),
        ABHEnemySoldier::ChooseFormationArchetype(0, 1),
        EBHCombatantArchetype::Rifleman
    );
    const FProperty* ArchetypeProperty = FindFProperty<FProperty>(
        ABHEnemySoldier::StaticClass(),
        FName(TEXT("CombatantArchetype"))
    );
    TestTrue(
        TEXT("Combatant archetype remains replicated"),
        ArchetypeProperty &&
            ArchetypeProperty->HasAnyPropertyFlags(CPF_Net)
    );
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBHTacticalSupportContractTest,
    "BrokenHorizon.PersistentWar.Support.TacticalCallIns",
    EAutomationTestFlags::EditorContext |
        EAutomationTestFlags::EngineFilter
)

bool FBHTacticalSupportContractTest::RunTest(
    const FString& Parameters
)
{
    TestTrue(
        TEXT("Mortar support costs more logistics than smoke"),
        ABHFieldSupportRelay::GetSupplyCost(
            EBHTacticalSupportType::MortarBarrage
        ) > ABHFieldSupportRelay::GetSupplyCost(
            EBHTacticalSupportType::SmokeScreen
        )
    );
    TestTrue(
        TEXT("A current in-range cooperative ping is eligible"),
        ABHFieldSupportRelay::IsPingEligible(10.0f, 20.0f, 1000.0f, 2000.0f)
    );
    TestFalse(
        TEXT("An expired ping cannot authorize indirect fire"),
        ABHFieldSupportRelay::IsPingEligible(20.0f, 20.0f, 1000.0f, 2000.0f)
    );
    TestFalse(
        TEXT("A ping outside relay range cannot authorize indirect fire"),
        ABHFieldSupportRelay::IsPingEligible(10.0f, 20.0f, 2001.0f, 2000.0f)
    );
    TestEqual(
        TEXT("Mortar barrage uses a short adjustment pattern"),
        ABHTacticalSupportZone::GetMortarShellCount(),
        3
    );
    TestTrue(
        TEXT("Smoke covers a broader area than one mortar burst"),
        ABHTacticalSupportZone::GetSupportRadius(
            EBHTacticalSupportType::SmokeScreen
        ) > ABHTacticalSupportZone::GetSupportRadius(
            EBHTacticalSupportType::MortarBarrage
        )
    );
    TestTrue(
        TEXT("Hostiles inside the beaten zone are affected"),
        ABHTacticalSupportZone::ShouldAffectSoldier(
            EBHTacticalSupportType::MortarBarrage,
            true,
            300.0f
        )
    );
    TestFalse(
        TEXT("Smoke is not modeled as an arbitrary friendly debuff"),
        ABHTacticalSupportZone::ShouldAffectSoldier(
            EBHTacticalSupportType::SmokeScreen,
            false,
            300.0f
        )
    );
    const ABHFragGrenade* FragGrenadeDefaults =
        GetDefault<ABHFragGrenade>();
    TestNotNull(
        TEXT("Handheld frag grenade has a native projectile class"),
        FragGrenadeDefaults
    );
    if (FragGrenadeDefaults)
    {
        TestEqual(
            TEXT("Handheld frag grenade starts with the expected base fuse"),
            FragGrenadeDefaults->GetFuseDuration(),
            3.5f
        );
    }

    const ABHSmokeGrenade* SmokeGrenadeDefaults =
        GetDefault<ABHSmokeGrenade>();
    TestNotNull(
        TEXT("Handheld smoke grenade has a native deployable class"),
        SmokeGrenadeDefaults
    );
    if (SmokeGrenadeDefaults)
    {
        TestTrue(
            TEXT("Handheld smoke uses a deliberate non-instant fuse"),
            SmokeGrenadeDefaults->GetFuseDuration() >= 1.0f
        );
    }

    const FProperty* SupportTypeProperty = FindFProperty<FProperty>(
        ABHTacticalSupportZone::StaticClass(),
        FName(TEXT("SupportType"))
    );
    const FProperty* CooldownProperty = FindFProperty<FProperty>(
        ABHFieldSupportRelay::StaticClass(),
        FName(TEXT("CooldownEndsServerTime"))
    );
    const FProperty* FragCookProperty = FindFProperty<FProperty>(
        ABHCharacter::StaticClass(),
        FName(TEXT("MaxFragGrenadeCookDuration"))
    );
    const FProperty* FragThrowProperty = FindFProperty<FProperty>(
        ABHFragGrenade::StaticClass(),
        FName(TEXT("MinimumRemainingFuse"))
    );
    const FProperty* FragInventoryProperty = FindFProperty<FProperty>(
        ABHCharacter::StaticClass(),
        FName(TEXT("FragGrenadeCount"))
    );
    const FProperty* SavedFragInventoryProperty = FindFProperty<FProperty>(
        UBHSaveGame::StaticClass(),
        FName(TEXT("SavedFragGrenadeCount"))
    );
    const FProperty* SmokeInventoryProperty = FindFProperty<FProperty>(
        ABHCharacter::StaticClass(),
        FName(TEXT("SmokeGrenadeCount"))
    );
    const FProperty* SavedSmokeInventoryProperty = FindFProperty<FProperty>(
        UBHSaveGame::StaticClass(),
        FName(TEXT("SavedSmokeGrenadeCount"))
    );
    TestTrue(
        TEXT("Active support type replicates to every client"),
        SupportTypeProperty && SupportTypeProperty->HasAnyPropertyFlags(CPF_Net)
    );
    TestTrue(
        TEXT("Relay cooldown remains server-authoritative and replicated"),
        CooldownProperty && CooldownProperty->HasAnyPropertyFlags(CPF_Net)
    );
    TestTrue(
        TEXT("Frag grenade inventory replicates to its owning player"),
        FragInventoryProperty &&
            FragInventoryProperty->HasAnyPropertyFlags(CPF_Net)
    );
    TestTrue(
        TEXT("Handheld smoke inventory replicates to its owning player"),
        SmokeInventoryProperty &&
            SmokeInventoryProperty->HasAnyPropertyFlags(CPF_Net)
    );
    TestTrue(
        TEXT("Frag grenade cooking time is a tunable gameplay contract"),
        FragCookProperty &&
            FragCookProperty->HasAnyPropertyFlags(CPF_Edit)
    );
    TestTrue(
        TEXT("Frag grenade minimum fuse remains a stable editable knob"),
        FragThrowProperty &&
            FragThrowProperty->HasAnyPropertyFlags(CPF_Edit)
    );
    TestTrue(
        TEXT("Frag grenade inventory is part of the save contract"),
        SavedFragInventoryProperty &&
            SavedFragInventoryProperty->HasAnyPropertyFlags(CPF_SaveGame)
    );
    TestTrue(
        TEXT("Handheld smoke inventory is part of the save contract"),
        SavedSmokeInventoryProperty &&
            SavedSmokeInventoryProperty->HasAnyPropertyFlags(CPF_SaveGame)
    );
    TestTrue(
        TEXT("Save schema includes handheld frag grenade inventory"),
        BHSave::CurrentSchemaVersion >= 50
    );
    TestTrue(
        TEXT("Save schema includes handheld smoke grenade inventory"),
        BHSave::CurrentSchemaVersion >= 50
    );
    const FProperty* FlashlightStateProperty = FindFProperty<FProperty>(
        ABHCharacter::StaticClass(), FName(TEXT("bTacticalFlashlightOn")));
    const FProperty* FlashlightBatteryProperty = FindFProperty<FProperty>(
        ABHCharacter::StaticClass(), FName(TEXT("TacticalFlashlightBattery")));
    const FProperty* SavedFlashlightBatteryProperty = FindFProperty<FProperty>(
        UBHSaveGame::StaticClass(),
        FName(TEXT("SavedTacticalFlashlightBattery")));
    TestTrue(
        TEXT("Flashlight illumination state replicates to every player"),
        FlashlightStateProperty &&
            FlashlightStateProperty->HasAnyPropertyFlags(CPF_Net));
    TestTrue(
        TEXT("Flashlight battery remains server-authoritative and replicated"),
        FlashlightBatteryProperty &&
            FlashlightBatteryProperty->HasAnyPropertyFlags(CPF_Net));
    TestTrue(
        TEXT("Flashlight battery is part of the save contract"),
        SavedFlashlightBatteryProperty &&
            SavedFlashlightBatteryProperty->HasAnyPropertyFlags(CPF_SaveGame));
    TestTrue(
        TEXT("Save schema includes persistent flashlight battery"),
        BHSave::CurrentSchemaVersion >= 52);
    TestTrue(
        TEXT("A rested mobile combatant can perform a weapon bash"),
        ABHCharacter::CanPerformWeaponBash(
            false, false, false, false,
            50.0f, 18.0f, 10.0f, 9.0f));
    TestFalse(
        TEXT("Weapon bash cannot bypass its stamina requirement"),
        ABHCharacter::CanPerformWeaponBash(
            false, false, false, false,
            17.9f, 18.0f, 10.0f, 9.0f));
    TestFalse(
        TEXT("Casualty dragging prevents an unsafe weapon bash"),
        ABHCharacter::CanPerformWeaponBash(
            false, false, false, true,
            50.0f, 18.0f, 10.0f, 9.0f));
    TestFalse(
        TEXT("Weapon bash cooldown rejects immediate repeated strikes"),
        ABHCharacter::CanPerformWeaponBash(
            false, false, false, false,
            50.0f, 18.0f, 8.9f, 9.0f));
    TestTrue(
        TEXT("A steady aiming combatant can perform field observation"),
        ABHCharacter::CanPerformFieldObservation(
            false, false, true, false, false,
            20.0f, 35.0f, 70.0f, 60.0f));
    TestFalse(
        TEXT("Field observation requires deliberate aiming"),
        ABHCharacter::CanPerformFieldObservation(
            false, false, false, false, false,
            0.0f, 35.0f, 70.0f, 60.0f));
    TestFalse(
        TEXT("Movement prevents a stable range solution"),
        ABHCharacter::CanPerformFieldObservation(
            false, false, true, false, false,
            35.1f, 35.0f, 70.0f, 60.0f));
    TestFalse(
        TEXT("Field observation respects its report cooldown"),
        ABHCharacter::CanPerformFieldObservation(
            false, false, true, false, false,
            0.0f, 35.0f, 59.9f, 60.0f));
    TestEqual(
        TEXT("Blast concussion is strongest inside the pressure core"),
        ABHCharacter::CalculateBlastConcussionIntensity(
            200.0f, 250.0f, 1250.0f, false, 0.3f),
        1.0f);
    TestEqual(
        TEXT("Blast concussion falls off with distance"),
        ABHCharacter::CalculateBlastConcussionIntensity(
            750.0f, 250.0f, 1250.0f, false, 0.3f),
        0.5f);
    TestEqual(
        TEXT("Hard cover substantially attenuates blast concussion"),
        ABHCharacter::CalculateBlastConcussionIntensity(
            750.0f, 250.0f, 1250.0f, true, 0.3f),
        0.15f);
    TestEqual(
        TEXT("Actors outside the pressure radius are unaffected"),
        ABHCharacter::CalculateBlastConcussionIntensity(
            1250.0f, 250.0f, 1250.0f, false, 0.3f),
        0.0f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBHBallisticRealismContractTest,
    "BrokenHorizon.Gameplay.Weapon.BallisticRealism",
    EAutomationTestFlags::EditorContext |
        EAutomationTestFlags::EngineFilter
)

bool FBHBallisticRealismContractTest::RunTest(
    const FString& Parameters
)
{
    TestTrue(
        TEXT("A rifle bullet accumulates measurable drop at 500 meters"),
        ABHRifle::CalculateBulletDropCentimeters(
            50000.0f,
            80000.0f,
            1.0f
        ) > 180.0f
    );
    TestTrue(
        TEXT("Close-range energy remains higher than long-range energy"),
        ABHRifle::CalculateDamageRetention(
            5000.0f, 10000.0f, 40000.0f, 0.55f
        ) > ABHRifle::CalculateDamageRetention(
            40000.0f, 10000.0f, 40000.0f, 0.55f
        )
    );
    TestTrue(
        TEXT("Timber permits more penetration than structural metal"),
        ABHRifle::GetSurfaceResponse(SurfaceType6)
            .MaximumPenetrationDepth >
        ABHRifle::GetSurfaceResponse(SurfaceType4)
            .MaximumPenetrationDepth
    );
    TestTrue(
        TEXT("Thin timber can be defeated"),
        ABHRifle::CanPenetrateSurface(
            ABHRifle::GetSurfaceResponse(SurfaceType6),
            12.0f
        )
    );
    TestFalse(
        TEXT("Thick timber stops the projectile"),
        ABHRifle::CanPenetrateSurface(
            ABHRifle::GetSurfaceResponse(SurfaceType6),
            35.0f
        )
    );
    TestTrue(
        TEXT("Shallow metal impacts can ricochet"),
        ABHRifle::ShouldRicochet(
            ABHRifle::GetSurfaceResponse(SurfaceType4),
            0.15f
        )
    );
    TestFalse(
        TEXT("Head-on metal impacts do not ricochet"),
        ABHRifle::ShouldRicochet(
            ABHRifle::GetSurfaceResponse(SurfaceType4),
            0.90f
        )
    );

    const FBHImpactPresentationProfile BaseImpactProfile;
    const FBHImpactPresentationProfile MetalImpactProfile =
        ABHImpactEffect::GetSurfacePresentationProfile(
            SurfaceType4,
            BaseImpactProfile
        );
    const FBHImpactPresentationProfile ConcreteImpactProfile =
        ABHImpactEffect::GetSurfacePresentationProfile(
            SurfaceType1,
            BaseImpactProfile
        );
    const FBHImpactPresentationProfile WoodImpactProfile =
        ABHImpactEffect::GetSurfacePresentationProfile(
            SurfaceType6,
            BaseImpactProfile
        );
    const FBHImpactPresentationProfile FleshImpactProfile =
        ABHImpactEffect::GetSurfacePresentationProfile(
            SurfaceType8,
            BaseImpactProfile
        );
    TestTrue(
        TEXT("Metal impacts create a stronger fragment response than concrete"),
        MetalImpactProfile.SparkCount > ConcreteImpactProfile.SparkCount &&
            MetalImpactProfile.SparkSpeed >
                ConcreteImpactProfile.SparkSpeed &&
            MetalImpactProfile.FlashIntensity >
                ConcreteImpactProfile.FlashIntensity
    );
    TestEqual(
        TEXT("Wood impacts do not emit generic sparks"),
        WoodImpactProfile.SparkCount,
        0
    );
    TestEqual(
        TEXT("Flesh impacts do not emit generic sparks"),
        FleshImpactProfile.SparkCount,
        0
    );
    TestFalse(
        TEXT("Flesh impacts do not place a hard-surface bullet mark"),
        FleshImpactProfile.bShowBulletMark
    );
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBHCooperativePlayerCasualtyContractTest,
    "BrokenHorizon.Gameplay.Coop.PlayerCasualty",
    EAutomationTestFlags::EditorContext |
        EAutomationTestFlags::EngineFilter
)

bool FBHCooperativePlayerCasualtyContractTest::RunTest(
    const FString& Parameters
)
{
    TestTrue(
        TEXT("First lethal event in multiplayer enters casualty state"),
        ABHCharacter::CanEnterCooperativeCasualty(true, false, false)
    );
    TestFalse(
        TEXT("Standalone play retains immediate death behavior"),
        ABHCharacter::CanEnterCooperativeCasualty(false, false, false)
    );
    TestFalse(
        TEXT("A second lethal event cannot create an endless down loop"),
        ABHCharacter::CanEnterCooperativeCasualty(true, false, true)
    );
    TestFalse(
        TEXT("An already incapacitated player cannot re-enter casualty state"),
        ABHCharacter::CanEnterCooperativeCasualty(true, true, false)
    );
    TestTrue(
        TEXT("A nearby incapacitated ally can be dragged by a combat-effective player"),
        ABHCharacter::CanBeginCasualtyDrag(
            true, true, true, false, 250.0f, 400.0f)
    );
    TestFalse(
        TEXT("A conscious target cannot enter casualty drag"),
        ABHCharacter::CanBeginCasualtyDrag(
            true, false, true, false, 250.0f, 400.0f)
    );
    TestFalse(
        TEXT("An enemy casualty cannot be dragged"),
        ABHCharacter::CanBeginCasualtyDrag(
            true, true, false, false, 250.0f, 400.0f)
    );
    TestFalse(
        TEXT("A casualty cannot be claimed by two draggers"),
        ABHCharacter::CanBeginCasualtyDrag(
            true, true, true, true, 250.0f, 400.0f)
    );
    TestFalse(
        TEXT("Server range validation rejects a distant casualty"),
        ABHCharacter::CanBeginCasualtyDrag(
            true, true, true, false, 401.0f, 400.0f)
    );

    UBHHealthComponent* Health = NewObject<UBHHealthComponent>();
    TestNotNull(TEXT("Health test component exists"), Health);
    if (Health)
    {
        Health->ConfigureMaximumHealth(100.0f, true);
        Health->ApplyDamage(200.0f, nullptr);
        TestTrue(TEXT("Lethal health state is reached"), Health->IsDead());
        Health->ReviveAtHealth(35.0f);
        TestFalse(TEXT("Revive clears lethal health state"), Health->IsDead());
        TestEqual(TEXT("Revive restores configured health"),
            Health->GetCurrentHealth(), 35.0f);
    }

    const FProperty* IncapacitatedProperty = FindFProperty<FProperty>(
        ABHCharacter::StaticClass(), FName(TEXT("bPlayerIncapacitated"))
    );
    const FProperty* StabilizedProperty = FindFProperty<FProperty>(
        ABHCharacter::StaticClass(), FName(TEXT("bPlayerCasualtyStabilized"))
    );
    const FProperty* DeadlineProperty = FindFProperty<FProperty>(
        ABHCharacter::StaticClass(), FName(TEXT("PlayerBleedOutDeadline"))
    );
    const FProperty* DraggedCasualtyProperty = FindFProperty<FProperty>(
        ABHCharacter::StaticClass(), FName(TEXT("DraggedCasualty"))
    );
    TestTrue(TEXT("Incapacitation replicates"),
        IncapacitatedProperty &&
        IncapacitatedProperty->HasAnyPropertyFlags(CPF_Net));
    TestTrue(TEXT("Stabilization replicates"),
        StabilizedProperty &&
        StabilizedProperty->HasAnyPropertyFlags(CPF_Net));
    TestTrue(TEXT("Bleed-out deadline replicates"),
        DeadlineProperty && DeadlineProperty->HasAnyPropertyFlags(CPF_Net));
    TestTrue(TEXT("The active casualty drag target replicates"),
        DraggedCasualtyProperty &&
        DraggedCasualtyProperty->HasAnyPropertyFlags(CPF_Net));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBHWeaponHeatContractTest,
    "BrokenHorizon.Gameplay.Combat.WeaponHeat",
    EAutomationTestFlags::EditorContext |
        EAutomationTestFlags::EngineFilter
)

bool FBHWeaponHeatContractTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    const FBHWeaponRoleProfile Assault =
        UBHWeaponComponent::BuildWeaponRoleProfile(EBHWeaponRole::Assault);
    const FBHWeaponRoleProfile Marksman =
        UBHWeaponComponent::BuildWeaponRoleProfile(EBHWeaponRole::Marksman);
    const FBHWeaponRoleProfile Support =
        UBHWeaponComponent::BuildWeaponRoleProfile(EBHWeaponRole::Support);
    TestTrue(TEXT("Marksman barrel gains more heat per shot than assault"),
        Marksman.HeatPerShot > Assault.HeatPerShot);
    TestTrue(TEXT("Support barrel sustains longer strings than assault"),
        Support.HeatPerShot < Assault.HeatPerShot);
    TestTrue(TEXT("Heat accumulation clamps at the overheat threshold"),
        FMath::IsNearlyEqual(
            UBHWeaponComponent::CalculateHeatAfterShot(0.98f, 0.10f),
            1.0f));
    TestTrue(TEXT("Hot barrel materially increases dispersion"),
        UBHWeaponComponent::CalculateHeatSpreadMultiplier(0.9f) > 1.45f);
    TestTrue(TEXT("Cool barrel has no dispersion penalty"),
        FMath::IsNearlyEqual(
            UBHWeaponComponent::CalculateHeatSpreadMultiplier(0.2f),
            1.0f));
    const FProperty* HeatProperty = FindFProperty<FProperty>(
        UBHWeaponComponent::StaticClass(),
        FName(TEXT("WeaponHeatNormalized")));
    const FProperty* OverheatProperty = FindFProperty<FProperty>(
        UBHWeaponComponent::StaticClass(),
        FName(TEXT("bWeaponOverheated")));
    TestTrue(TEXT("Weapon heat replicates to the owning client"),
        HeatProperty && HeatProperty->HasAnyPropertyFlags(CPF_Net));
    TestTrue(TEXT("Overheat lockout replicates to the owning client"),
        OverheatProperty && OverheatProperty->HasAnyPropertyFlags(CPF_Net));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBHControlledBreathingContractTest,
    "BrokenHorizon.Gameplay.Combat.ControlledBreathing",
    EAutomationTestFlags::EditorContext |
        EAutomationTestFlags::EngineFilter
)

bool FBHControlledBreathingContractTest::RunTest(
    const FString& Parameters
)
{
    (void)Parameters;
    TestTrue(TEXT("A rested aiming player can steady breathing"),
        ABHCharacter::CanStartControlledBreathing(
            true, false, false, 100.0f, 20.0f, 0.0f));
    TestFalse(TEXT("Controlled breathing requires ADS"),
        ABHCharacter::CanStartControlledBreathing(
            false, false, false, 100.0f, 20.0f, 0.0f));
    TestFalse(TEXT("Sprint and controlled breathing are exclusive"),
        ABHCharacter::CanStartControlledBreathing(
            true, true, false, 100.0f, 20.0f, 0.0f));
    TestFalse(TEXT("Exhaustion blocks controlled breathing"),
        ABHCharacter::CanStartControlledBreathing(
            true, false, false, 10.0f, 20.0f, 0.0f));
    TestFalse(TEXT("Recovery prevents repeated breath tapping"),
        ABHCharacter::CanStartControlledBreathing(
            true, false, false, 100.0f, 20.0f, 0.5f));

    const float SteadyMultiplier =
        ABHCharacter::CalculateControlledBreathSpreadMultiplier(
            true, 1.0f, 6.0f, 0.65f, 1.15f);
    const float StrainedMultiplier =
        ABHCharacter::CalculateControlledBreathSpreadMultiplier(
            true, 6.0f, 6.0f, 0.65f, 1.15f);
    TestTrue(TEXT("Early controlled breathing materially steadies aim"),
        SteadyMultiplier <= 0.66f);
    TestTrue(TEXT("Holding too long becomes less stable than normal breathing"),
        StrainedMultiplier > 1.0f);
    TestTrue(TEXT("Released breath has no spread modifier"),
        FMath::IsNearlyEqual(
            ABHCharacter::CalculateControlledBreathSpreadMultiplier(
                false, 0.0f, 6.0f, 0.65f, 1.15f),
            1.0f));

    const FProperty* HoldingProperty = FindFProperty<FProperty>(
        ABHCharacter::StaticClass(),
        FName(TEXT("bHoldingControlledBreath")));
    const FProperty* StaminaProperty = FindFProperty<FProperty>(
        ABHCharacter::StaticClass(),
        FName(TEXT("CurrentStamina")));
    TestTrue(TEXT("Authoritative breathing state replicates"),
        HoldingProperty && HoldingProperty->HasAnyPropertyFlags(CPF_Net));
    TestTrue(TEXT("Authoritative stamina replicates to its owner"),
        StaminaProperty && StaminaProperty->HasAnyPropertyFlags(CPF_Net));
    TestNotNull(TEXT("Blueprints can request controlled breathing"),
        ABHCharacter::StaticClass()->FindFunctionByName(
            FName(TEXT("SetControlledBreathingRequested"))));
    TestNotNull(TEXT("HUD exposes controlled-breath feedback"),
        UBHCombatStatusWidget::StaticClass()->FindFunctionByName(
            FName(TEXT("SetControlledBreathing"))));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBHWeaponBracingContractTest,
    "BrokenHorizon.Gameplay.Combat.FortificationWeaponBracing",
    EAutomationTestFlags::EditorContext |
        EAutomationTestFlags::EngineFilter
)

bool FBHWeaponBracingContractTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    TestTrue(TEXT("Stationary ADS can brace on a healthy fortification"),
        ABHCharacter::CanBraceWeapon(
            true, false, false, 0.0f, 35.0f,
            150.0f, 220.0f, 0.8f, 0.1f, true, 1.0f));
    TestFalse(TEXT("Hip fire cannot receive bracing benefits"),
        ABHCharacter::CanBraceWeapon(
            false, false, false, 0.0f, 35.0f,
            150.0f, 220.0f, 0.8f, 0.1f, true, 1.0f));
    TestFalse(TEXT("Movement breaks a weapon brace"),
        ABHCharacter::CanBraceWeapon(
            true, false, false, 80.0f, 35.0f,
            150.0f, 220.0f, 0.8f, 0.1f, true, 1.0f));
    TestFalse(TEXT("An unbuilt emplacement cannot brace a weapon"),
        ABHCharacter::CanBraceWeapon(
            true, false, false, 0.0f, 35.0f,
            150.0f, 220.0f, 0.8f, 0.1f, false, 1.0f));
    TestFalse(TEXT("Destroyed cover cannot brace a weapon"),
        ABHCharacter::CanBraceWeapon(
            true, false, false, 0.0f, 35.0f,
            150.0f, 220.0f, 0.8f, 0.1f, true, 0.0f));

    TestTrue(TEXT("Full-integrity support materially reduces spread"),
        ABHCharacter::CalculateWeaponBraceMultiplier(
            true, 1.0f, 0.55f) <= 0.56f);
    TestTrue(TEXT("Damaged support provides a smaller handling benefit"),
        ABHCharacter::CalculateWeaponBraceMultiplier(
            true, 0.5f, 0.55f) > 0.55f &&
        ABHCharacter::CalculateWeaponBraceMultiplier(
            true, 0.5f, 0.55f) < 1.0f);
    TestTrue(TEXT("Unbraced weapons retain baseline handling"),
        FMath::IsNearlyEqual(
            ABHCharacter::CalculateWeaponBraceMultiplier(
                false, 1.0f, 0.55f),
            1.0f));

    const FProperty* BracedProperty = FindFProperty<FProperty>(
        ABHCharacter::StaticClass(), FName(TEXT("bWeaponBraced")));
    const FProperty* QualityProperty = FindFProperty<FProperty>(
        ABHCharacter::StaticClass(),
        FName(TEXT("WeaponBraceSupportQuality")));
    TestTrue(TEXT("Weapon-brace state replicates to its owner"),
        BracedProperty && BracedProperty->HasAnyPropertyFlags(CPF_Net));
    TestTrue(TEXT("Support quality replicates to its owner"),
        QualityProperty && QualityProperty->HasAnyPropertyFlags(CPF_Net));
    TestNotNull(TEXT("Ammo HUD exposes weapon-brace feedback"),
        UBHAmmoHUDWidget::StaticClass()->FindFunctionByName(
            FName(TEXT("SetWeaponBraced"))));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBHMagazineReloadContractTest,
    "BrokenHorizon.Gameplay.Combat.MagazineReloads",
    EAutomationTestFlags::EditorContext |
        EAutomationTestFlags::EngineFilter
)

bool FBHMagazineReloadContractTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    TestEqual(TEXT("Tactical reload keeps authored weapon timing"),
        UBHWeaponComponent::CalculateReloadDuration(
            3.0f, EBHReloadType::Tactical), 3.0f);
    TestTrue(TEXT("Emergency reload is materially faster"),
        UBHWeaponComponent::CalculateReloadDuration(
            3.0f, EBHReloadType::Emergency) < 2.0f);
    TestTrue(TEXT("Emergency reload still has a physical handling delay"),
        UBHWeaponComponent::CalculateReloadDuration(
            3.0f, EBHReloadType::Emergency) > 1.5f);
    const FProperty* ReloadTypeProperty = FindFProperty<FProperty>(
        UBHWeaponComponent::StaticClass(), FName(TEXT("ReloadType")));
    TestTrue(TEXT("Reload choice replicates for multiplayer presentation"),
        ReloadTypeProperty &&
        ReloadTypeProperty->HasAnyPropertyFlags(CPF_Net));
    TestNotNull(TEXT("Emergency reload is Blueprint-callable"),
        UBHWeaponComponent::StaticClass()->FindFunctionByName(
            FName(TEXT("StartEmergencyReload"))));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBHCooperativeSupplySharingContractTest,
    "BrokenHorizon.Gameplay.Coop.SupplySharing",
    EAutomationTestFlags::EditorContext |
        EAutomationTestFlags::EngineFilter
)

bool FBHCooperativeSupplySharingContractTest::RunTest(
    const FString& Parameters)
{
    (void)Parameters;
    TestEqual(TEXT("A donor keeps the emergency ammunition reserve"),
        ABHCharacter::CalculateSupplyShareAmount(80, 30, 20, 180, 30), 30);
    TestEqual(TEXT("Receiver capacity limits a transfer"),
        ABHCharacter::CalculateSupplyShareAmount(100, 30, 175, 180, 30), 5);
    TestEqual(TEXT("Emergency reserve cannot be donated"),
        ABHCharacter::CalculateSupplyShareAmount(30, 30, 0, 180, 30), 0);
    TestEqual(TEXT("No transfer occurs when the receiver is full"),
        ABHCharacter::CalculateSupplyShareAmount(100, 30, 180, 180, 30), 0);
    TestEqual(TEXT("Single critical item transfers above reserve"),
        ABHCharacter::CalculateSupplyShareAmount(2, 1, 0, 2, 1), 1);
    TestNotNull(TEXT("Supply sharing is exposed to server gameplay"),
        ABHCharacter::StaticClass()->FindFunctionByName(
            FName(TEXT("TryShareFieldSuppliesWith"))));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBHCarryLoadContractTest,
    "BrokenHorizon.Gameplay.Logistics.CarriedLoad",
    EAutomationTestFlags::EditorContext |
        EAutomationTestFlags::EngineFilter
)

bool FBHCarryLoadContractTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    const FBHCarryLoadProfile Light =
        UBHLoadoutWeight::BuildCarryLoadProfile(
            EBHWeaponRole::Assault, 10, 30, 0, 0, 0, 0, 0);
    const FBHCarryLoadProfile Support =
        UBHLoadoutWeight::BuildCarryLoadProfile(
            EBHWeaponRole::Support, 60, 240, 2, 0, 2, 2, 3);
    const FBHCarryLoadProfile ReducedSupport =
        UBHLoadoutWeight::BuildCarryLoadProfile(
            EBHWeaponRole::Support, 20, 40, 0, 0, 0, 1, 1);

    TestTrue(TEXT("Support gun and full supplies create greater burden"),
        Support.TotalKilograms > Light.TotalKilograms);
    TestTrue(TEXT("Dropping expendables materially reduces burden"),
        ReducedSupport.TotalKilograms < Support.TotalKilograms);
    TestTrue(TEXT("Heavy load slows movement"),
        Support.MovementSpeedMultiplier < Light.MovementSpeedMultiplier);
    TestTrue(TEXT("Heavy load drains sprint endurance faster"),
        Support.StaminaDrainMultiplier > Light.StaminaDrainMultiplier);
    TestTrue(TEXT("Heavy load delays stamina recovery"),
        Support.StaminaRecoveryMultiplier < Light.StaminaRecoveryMultiplier);
    TestTrue(TEXT("Heavy equipment is acoustically less discreet"),
        Support.MovementNoiseMultiplier > Light.MovementNoiseMultiplier);
    TestTrue(TEXT("Load effects remain graduated rather than immobilizing"),
        Support.MovementSpeedMultiplier >= 0.72f);
    const FProperty* FragProperty = FindFProperty<FProperty>(
        ABHCharacter::StaticClass(), FName(TEXT("FragGrenadeCount")));
    TestTrue(TEXT("Grenade mass source replicates to the owning client"),
        FragProperty && FragProperty->HasAnyPropertyFlags(CPF_Net));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBHBattlefieldConditionsContractTest,
    "BrokenHorizon.Gameplay.World.BattlefieldConditions",
    EAutomationTestFlags::EditorContext |
        EAutomationTestFlags::EngineFilter
)

bool FBHBattlefieldConditionsContractTest::RunTest(
    const FString& Parameters
)
{
    (void)Parameters;
    FBHBattlefieldConditionProfile Fog;
    FBHBattlefieldConditionProfile Storm;
    bool bFoundFog = false;
    bool bFoundStorm = false;
    for (int32 Turn = 0; Turn < 48; ++Turn)
    {
        const FBHBattlefieldConditionProfile Profile =
            UBHBattlefieldConditions::BuildProfileForTurn(Turn);
        if (Profile.Weather == EBHBattlefieldWeather::Fog)
        {
            Fog = Profile;
            bFoundFog = true;
        }
        if (Profile.Weather == EBHBattlefieldWeather::Storm)
        {
            Storm = Profile;
            bFoundStorm = true;
        }
    }

    TestTrue(TEXT("Deterministic campaign cycle contains fog"), bFoundFog);
    TestTrue(TEXT("Deterministic campaign cycle contains storms"), bFoundStorm);
    TestTrue(TEXT("Fog materially limits visual detection"),
        Fog.SightRangeMultiplier < 0.55f);
    TestTrue(TEXT("Storm noise masks infantry footsteps"),
        Storm.MovementNoiseMultiplier < 0.75f);
    TestTrue(TEXT("Storm noise masks explosive hearing"),
        Storm.ExplosionNoiseMultiplier < 0.75f);
    TestTrue(TEXT("Storm disperses smoke faster"),
        Storm.SmokePersistenceMultiplier < 0.50f);
    TestTrue(TEXT("Dense fog preserves smoke persistence"),
        Fog.SmokePersistenceMultiplier > 1.0f);
    TestTrue(TEXT("Wet storm conditions reduce vehicle traction"),
        Storm.VehicleTractionMultiplier < 0.75f);
    TestTrue(TEXT("Storm corrections increase indirect-fire dispersion"),
        Storm.MortarDispersionMultiplier > 1.35f);
    TestTrue(TEXT("Severe weather increases fuel demand"),
        Storm.VehicleFuelBurnMultiplier > 1.0f);
    TestEqual(TEXT("Same turn always resolves the same condition"),
        UBHBattlefieldConditions::BuildProfileForTurn(17).ConditionLabel,
        UBHBattlefieldConditions::BuildProfileForTurn(17).ConditionLabel);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBHCombatEngineeringContractTest,
    "BrokenHorizon.Gameplay.Engineering.CommandCharges",
    EAutomationTestFlags::EditorContext |
        EAutomationTestFlags::EngineFilter
)

bool FBHCombatEngineeringContractTest::RunTest(
    const FString& Parameters
)
{
    TestTrue(
        TEXT("Area-denial charge controls a wider zone than a door charge"),
        ABHEngineeringCharge::GetOuterDamageRadius(
            EBHEngineeringChargeMode::AreaDenial
        ) > ABHEngineeringCharge::GetOuterDamageRadius(
            EBHEngineeringChargeMode::Breach
        )
    );
    TestTrue(
        TEXT("Focused breaching charge has higher peak damage"),
        ABHEngineeringCharge::GetMaximumDamage(
            EBHEngineeringChargeMode::Breach
        ) > ABHEngineeringCharge::GetMaximumDamage(
            EBHEngineeringChargeMode::AreaDenial
        )
    );
    TestTrue(
        TEXT("Armed owner can command detonate"),
        ABHEngineeringCharge::CanCommandDetonate(true, false, true)
    );
    TestFalse(
        TEXT("Unarmed charge cannot detonate"),
        ABHEngineeringCharge::CanCommandDetonate(false, false, true)
    );
    TestFalse(
        TEXT("Non-owner cannot command detonate"),
        ABHEngineeringCharge::CanCommandDetonate(true, false, false)
    );

    const FProperty* ChargeModeProperty = FindFProperty<FProperty>(
        ABHEngineeringCharge::StaticClass(), FName(TEXT("ChargeMode"))
    );
    const FProperty* InventoryProperty = FindFProperty<FProperty>(
        ABHCharacter::StaticClass(), FName(TEXT("EngineeringChargeCount"))
    );
    const FProperty* DoorBreachedProperty = FindFProperty<FProperty>(
        ABHDoor::StaticClass(), FName(TEXT("bBreached"))
    );
    TestTrue(TEXT("Charge mode replicates"), ChargeModeProperty &&
        ChargeModeProperty->HasAnyPropertyFlags(CPF_Net));
    TestTrue(TEXT("Carried engineering inventory replicates owner state"),
        InventoryProperty && InventoryProperty->HasAnyPropertyFlags(CPF_Net));
    TestTrue(TEXT("Breached door state replicates"), DoorBreachedProperty &&
        DoorBreachedProperty->HasAnyPropertyFlags(CPF_Net));

    const FProperty* SavedChargeProperty = FindFProperty<FProperty>(
        UBHSaveGame::StaticClass(),
        GET_MEMBER_NAME_CHECKED(UBHSaveGame, SavedEngineeringChargeCount)
    );
    TestTrue(TEXT("Engineering inventory is a save-game contract"),
        SavedChargeProperty &&
        SavedChargeProperty->HasAnyPropertyFlags(CPF_SaveGame));
    return true;
}

#endif
