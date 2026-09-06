#if WITH_DEV_AUTOMATION_TESTS

#include "BHOperationPlacement.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "Misc/AutomationTest.h"
#include "UObject/StrongObjectPtr.h"

namespace
{
struct FBHPlacementWorld
{
    TStrongObjectPtr<UWorld> World;
    ACharacter* Character = nullptr;

    FBHPlacementWorld()
        : World(UWorld::CreateWorld(EWorldType::Game, false))
    {
        FActorSpawnParameters Parameters;
        Parameters.ObjectFlags |= RF_Transient;
        Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        Character = World->SpawnActor<ACharacter>(FVector(-6000, 0, 200), FRotator::ZeroRotator, Parameters);
    }
    ~FBHPlacementWorld() { World->DestroyWorld(false); }

    UBoxComponent* AddBox(const FVector& Location, const FVector& Extent,
        const FRotator& Rotation = FRotator::ZeroRotator)
    {
        AActor* Actor = World->SpawnActor<AActor>();
        UBoxComponent* Box = NewObject<UBoxComponent>(Actor);
        Actor->SetRootComponent(Box);
        Actor->AddInstanceComponent(Box);
        Box->SetBoxExtent(Extent);
        Box->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
        Box->SetCollisionObjectType(ECC_WorldStatic);
        Box->SetCollisionResponseToAllChannels(ECR_Block);
        Box->RegisterComponent();
        Actor->SetActorLocationAndRotation(Location, Rotation);
        return Box;
    }

    bool Find(float Distance, FVector& Location, FRotator& Rotation)
    { return BHOperationPlacement::TryFindGroundedInsertion(*Character, FVector::ZeroVector, Distance, Location, Rotation); }
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBHOperationGroundedPlacementTest,
    "BrokenHorizon.Operations.Placement.GroundedQuery",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBHOperationGroundedPlacementTest::RunTest(const FString& Parameters)
{
    FBHPlacementWorld Fixture;
    if (!TestNotNull(TEXT("Real collision-query character spawned"), Fixture.Character)) { return false; }
    Fixture.AddBox(FVector(0, 0, -50), FVector(10000, 10000, 50));
    const FTransform Original = Fixture.Character->GetActorTransform();
    FVector Location(123, 456, 789);
    FRotator Rotation(12, 34, 56);
    if (!TestTrue(TEXT("Walkable blocking floor yields insertion"), Fixture.Find(2000, Location, Rotation))) { return false; }
    const float HalfHeight = Fixture.Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
    TestTrue(TEXT("Capsule bottom rests just above floor"),
        Location.Z >= HalfHeight && Location.Z < HalfHeight + 10.0f);
    TestTrue(TEXT("Result respects preferred distance on unobstructed floor"), FMath::IsNearlyEqual(Location.Size2D(), 2000.0, 1.0));
    TestTrue(TEXT("Placement query leaves character transform unchanged"), Fixture.Character->GetActorTransform().Equals(Original));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBHOperationMissingFloorTest,
    "BrokenHorizon.Operations.Placement.MissingFloorPreservesOutputs",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBHOperationMissingFloorTest::RunTest(const FString& Parameters)
{
    FBHPlacementWorld Fixture;
    if (!TestNotNull(TEXT("Character spawned"), Fixture.Character)) { return false; }
    const FVector OriginalLocation(123, 456, 789);
    const FRotator OriginalRotation(12, 34, 56);
    FVector Location = OriginalLocation;
    FRotator Rotation = OriginalRotation;
    TestFalse(TEXT("Empty world cannot provide grounded insertion"), Fixture.Find(2000, Location, Rotation));
    TestEqual(TEXT("Failed query preserves output location"), Location, OriginalLocation);
    TestEqual(TEXT("Failed query preserves output rotation"), Rotation, OriginalRotation);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBHOperationBlockedPlacementTest,
    "BrokenHorizon.Operations.Placement.BlockedCapsule",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBHOperationBlockedPlacementTest::RunTest(const FString& Parameters)
{
    FBHPlacementWorld Fixture;
    if (!TestNotNull(TEXT("Character spawned"), Fixture.Character)) { return false; }
    // Solid geometry encloses every candidate and the sweep origin; accepting
    // its initial penetrating hit would strand the character inside collision.
    Fixture.AddBox(FVector(0, 0, 10000), FVector(100000, 100000, 30000));
    FVector Location = FVector::ZeroVector;
    FRotator Rotation = FRotator::ZeroRotator;
    TestFalse(TEXT("Penetrating capsule cannot be used as insertion"), Fixture.Find(2000, Location, Rotation));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBHOperationSteepPlacementTest,
    "BrokenHorizon.Operations.Placement.RejectsSteepGround",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBHOperationSteepPlacementTest::RunTest(const FString& Parameters)
{
    FBHPlacementWorld Fixture;
    if (!TestNotNull(TEXT("Character spawned"), Fixture.Character)) { return false; }
    Fixture.AddBox(FVector::ZeroVector, FVector(100000, 100000, 20), FRotator(60, 0, 0));
    FVector Location = FVector::ZeroVector;
    FRotator Rotation = FRotator::ZeroRotator;
    TestFalse(TEXT("Sixty-degree slope is not a walkable insertion"), Fixture.Find(2000, Location, Rotation));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBHOperationCloserFallbackTest,
    "BrokenHorizon.Operations.Placement.FallsBackToNearbyGround",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBHOperationCloserFallbackTest::RunTest(const FString& Parameters)
{
    FBHPlacementWorld Fixture;
    if (!TestNotNull(TEXT("Character spawned"), Fixture.Character)) { return false; }
    // The preferred 400m ring and its half/quarter rings have no floor.
    // Authored ground exists only within roughly 20m of the operation.
    Fixture.AddBox(FVector(0, 0, -50), FVector(2200, 2200, 50));
    FVector Location = FVector::ZeroVector;
    FRotator Rotation = FRotator::ZeroRotator;
    if (!TestTrue(TEXT("Distant preference falls back onto nearby real ground"), Fixture.Find(40000, Location, Rotation))) { return false; }
    TestTrue(TEXT("Fallback stays on the nearby floor"), Location.Size2D() <= 2201.0 && Location.Size2D() >= 999.0);
    TestTrue(TEXT("Fallback is above floor"), Location.Z >= Fixture.Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
    return true;
}

#endif
