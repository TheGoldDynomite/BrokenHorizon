#if WITH_DEV_AUTOMATION_TESTS

#include "BHWaterSurface.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/AutomationTest.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"
#include <limits>

namespace
{
struct FBHScopedWaterWorld
{
    TStrongObjectPtr<UWorld> World;
    ABHWaterSurface* Surface = nullptr;
    UBoxComponent* Volume = nullptr;
    UStaticMeshComponent* Mesh = nullptr;
    UTextRenderComponent* Label = nullptr;
    FStructProperty* ExtentsProperty = nullptr;
    FFloatProperty* HeightProperty = nullptr;

    FBHScopedWaterWorld()
    {
        const FName PackageName = MakeUniqueObjectName(nullptr, UPackage::StaticClass(), TEXT("/Temp/BHWaterGeometryTest"));
        UPackage* Package = NewObject<UPackage>(nullptr, PackageName, RF_Transient);
        const UWorld::InitializationValues Values = UWorld::InitializationValues()
            .InitializeScenes(false).AllowAudioPlayback(false).RequiresHitProxies(false)
            .CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false)
            .ShouldSimulatePhysics(false).EnableTraceCollision(false).SetTransactional(false).CreateFXSystem(false);
        World.Reset(UWorld::CreateWorld(EWorldType::EditorPreview, false, TEXT("WaterGeometry"), Package,
            false, ERHIFeatureLevel::Num, &Values));
        FActorSpawnParameters Parameters;
        Parameters.ObjectFlags |= RF_Transient;
        Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        Surface = World->SpawnActor<ABHWaterSurface>(Parameters);
        if (Surface)
        {
            Volume = Surface->FindComponentByClass<UBoxComponent>();
            Mesh = Surface->FindComponentByClass<UStaticMeshComponent>();
            Label = Surface->FindComponentByClass<UTextRenderComponent>();
            ExtentsProperty = FindFProperty<FStructProperty>(Surface->GetClass(), TEXT("SurfaceExtents"));
            HeightProperty = FindFProperty<FFloatProperty>(Surface->GetClass(), TEXT("SurfaceHeight"));
        }
    }
    ~FBHScopedWaterWorld()
    {
        // No game instance, engine world context, actor startup, ticks or saves.
        // DestroyWorld unregisters components and deinitializes the scoped world.
        if (World.IsValid()) { World->DestroyWorld(false); }
    }
    bool Ready(FAutomationTestBase& Test) const
    {
        return Test.TestNotNull(TEXT("Scoped water actor"), Surface) &&
            Test.TestNotNull(TEXT("Actual water volume"), Volume) &&
            Test.TestNotNull(TEXT("Actual water mesh component"), Mesh) &&
            Test.TestNotNull(TEXT("Actual water label"), Label) &&
            Test.TestNotNull(TEXT("Authored extent property"), ExtentsProperty) &&
            Test.TestNotNull(TEXT("Authored height property"), HeightProperty) &&
            Test.TestTrue(TEXT("Components registered through actual spawn lifecycle"), Volume->IsRegistered() && Mesh->IsRegistered());
    }
    void Author(FVector Extents, float Height)
    {
        *ExtentsProperty->ContainerPtrToValuePtr<FVector>(Surface) = Extents;
        HeightProperty->SetPropertyValue_InContainer(Surface, Height);
    }
    FVector AuthoredExtents() const { return *ExtentsProperty->ContainerPtrToValuePtr<FVector>(Surface); }
    float AuthoredHeight() const { return HeightProperty->GetPropertyValue_InContainer(Surface); }
    void Construct() { Surface->OnConstruction(Surface->GetActorTransform()); }
};

void CheckFootprint(FAutomationTestBase& Test, const FBHScopedWaterWorld& Fixture, FVector Extents, float Height)
{
    const FBoxSphereBounds LocalVisualBounds = Fixture.Mesh->CalcBounds(Fixture.Mesh->GetRelativeTransform());
    Test.TestTrue(TEXT("Actual volume matches effective half-extents"), Fixture.Volume->GetUnscaledBoxExtent().Equals(Extents, 0.01));
    Test.TestTrue(TEXT("Assigned visual bounds match volume X footprint"), FMath::IsNearlyEqual(LocalVisualBounds.BoxExtent.X, Extents.X, 0.01));
    Test.TestTrue(TEXT("Assigned visual bounds match volume Y footprint"), FMath::IsNearlyEqual(LocalVisualBounds.BoxExtent.Y, Extents.Y, 0.01));
    Test.TestTrue(TEXT("Actual visual bounds are centered at the authored water height"), LocalVisualBounds.Origin.Equals(FVector(0.0, 0.0, Height), 0.01));
    Test.TestTrue(TEXT("Water remains a thin surface"), FMath::IsNearlyEqual(Fixture.Mesh->GetRelativeScale3D().Z, 0.02, 0.0001));
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBHWaterAuthoredGeometryTest,
    "BrokenHorizon.Environment.Water.AuthoredGeometryAndRegistration",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)
bool FBHWaterAuthoredGeometryTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    FBHScopedWaterWorld Fixture;
    if (!Fixture.Ready(*this) || !TestNotNull(TEXT("Constructor-assigned native cube"), Fixture.Mesh->GetStaticMesh().Get())) { return false; }
    TestTrue(TEXT("Cube asset has actual 50cm XY half-bounds"),
        Fixture.Mesh->GetStaticMesh()->GetBoundingBox().GetExtent().Equals(FVector(50.0), 0.01));
    const FTransform ActorTransform(FRotator(0.0, 25.0, 0.0), FVector(1300.0, -250.0, 400.0), FVector(1.25, 0.75, 1.0));
    Fixture.Surface->SetActorTransform(ActorTransform);
    Fixture.Author(FVector(4200.0, 900.0, 250.0), -35.0f);
    UMaterialInstanceDynamic* OverrideMaterial = UMaterialInstanceDynamic::Create(UMaterial::GetDefaultMaterial(MD_Surface), Fixture.Surface);
    if (!TestNotNull(TEXT("Scoped material override"), OverrideMaterial)) { return false; }
    Fixture.Mesh->SetMaterial(0, OverrideMaterial);
    Fixture.Volume->SetCollisionProfileName(TEXT("OverlapAll"));
    const FName VolumeProfile = Fixture.Volume->GetCollisionProfileName();
    const FName MeshProfile = Fixture.Mesh->GetCollisionProfileName();
    const ECollisionEnabled::Type MeshCollision = Fixture.Mesh->GetCollisionEnabled();
    const FTransform LabelTransform = Fixture.Label->GetRelativeTransform();
    Fixture.Construct();
    CheckFootprint(*this, Fixture, FVector(4200.0, 900.0, 250.0), -35.0f);
    TestTrue(TEXT("Cube scales to full 8400 by 1800cm surface"), Fixture.Mesh->GetRelativeScale3D().Equals(FVector(84.0, 18.0, 0.02), 0.001));
    TestTrue(TEXT("Extent getter exposes the applied authored dimensions"), Fixture.Surface->GetSurfaceExtents().Equals(FVector(4200.0, 900.0, 250.0)));
    TestEqual(TEXT("Negative authored height remains valid"), Fixture.Surface->GetSurfaceHeight(), -35.0f);

    // Corrupt registered component data and exercise actual unregister/register.
    Fixture.Volume->SetBoxExtent(FVector(150.0));
    Fixture.Mesh->SetRelativeScale3D(FVector(1.0));
    Fixture.Mesh->SetRelativeLocation(FVector(17.0, 23.0, 31.0));
    Fixture.Surface->ReregisterAllComponents();
    CheckFootprint(*this, Fixture, FVector(4200.0, 900.0, 250.0), -35.0f);
    for (int32 Repeat = 0; Repeat < 3; ++Repeat) { Fixture.Construct(); Fixture.Surface->ReregisterAllComponents(); }
    TestTrue(TEXT("Repeated lifecycle preserves actor transform"), Fixture.Surface->GetActorTransform().Equals(ActorTransform));
    TestTrue(TEXT("Volume object identity is stable"), Fixture.Surface->FindComponentByClass<UBoxComponent>() == Fixture.Volume);
    TestTrue(TEXT("Mesh object identity is stable"), Fixture.Surface->FindComponentByClass<UStaticMeshComponent>() == Fixture.Mesh);
    TestTrue(TEXT("Label object identity is stable"), Fixture.Surface->FindComponentByClass<UTextRenderComponent>() == Fixture.Label);
    TestTrue(TEXT("Material override survives synchronization"), Fixture.Mesh->GetMaterial(0) == OverrideMaterial);
    TestEqual(TEXT("Volume profile survives synchronization"), Fixture.Volume->GetCollisionProfileName(), VolumeProfile);
    TestEqual(TEXT("Mesh profile survives synchronization"), Fixture.Mesh->GetCollisionProfileName(), MeshProfile);
    TestEqual(TEXT("Mesh collision mode survives synchronization"), Fixture.Mesh->GetCollisionEnabled(), MeshCollision);
    TestTrue(TEXT("Label placement is unaffected"), Fixture.Label->GetRelativeTransform().Equals(LabelTransform));
    TestTrue(TEXT("Existing containment getter accepts actual volume center"), Fixture.Surface->ContainsWorldLocation(Fixture.Volume->Bounds.Origin));
    TestFalse(TEXT("Existing containment getter rejects a point outside actual volume bounds"),
        Fixture.Surface->ContainsWorldLocation(Fixture.Volume->Bounds.Origin + FVector(Fixture.Volume->Bounds.BoxExtent.X + 100.0, 0.0, 0.0)));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBHWaterAssignedMeshBoundsTest,
    "BrokenHorizon.Environment.Water.AssignedMeshBounds",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)
bool FBHWaterAssignedMeshBoundsTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    FBHScopedWaterWorld Fixture;
    if (!Fixture.Ready(*this)) { return false; }
    // A transient bounds fixture exercises the assigned-mesh API; no asset build/save.
    TStrongObjectPtr<UStaticMesh> AssignedMesh(NewObject<UStaticMesh>());
    AssignedMesh->SetExtendedBounds(FBoxSphereBounds(FVector(25.0, -10.0, 8.0), FVector(20.0, 40.0, 0.0), 50.0));
    Fixture.Mesh->SetStaticMesh(AssignedMesh.Get());
    Fixture.Author(FVector(4200.0, 900.0, 250.0), 47.0f);
    Fixture.Construct();
    CheckFootprint(*this, Fixture, FVector(4200.0, 900.0, 250.0), 47.0f);
    TestTrue(TEXT("Valid plane with zero local Z extent is supported"),
        Fixture.Mesh->GetRelativeScale3D().Equals(FVector(210.0, 22.5, 0.02), 0.001));
    TestTrue(TEXT("Assigned mesh object is retained"), Fixture.Mesh->GetStaticMesh() == AssignedMesh.Get());
    const FRotator AuthoredRotation(11.0, 33.0, 7.0);
    Fixture.Mesh->SetRelativeRotation(AuthoredRotation);
    Fixture.Surface->ReregisterAllComponents();
    TestTrue(TEXT("Mesh relative rotation is preserved"), Fixture.Mesh->GetRelativeRotation().Equals(AuthoredRotation, 0.001));
    const FBoxSphereBounds RotatedBounds = Fixture.Mesh->CalcBounds(Fixture.Mesh->GetRelativeTransform());
    TestTrue(TEXT("Off-center rotated assigned mesh remains centered at water height"), RotatedBounds.Origin.Equals(FVector(0.0, 0.0, 47.0), 0.01));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBHWaterInvalidGeometryTest,
    "BrokenHorizon.Environment.Water.InvalidGeometryInputs",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)
bool FBHWaterInvalidGeometryTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    FBHScopedWaterWorld Fixture;
    if (!Fixture.Ready(*this)) { return false; }
    const double NaN = std::numeric_limits<double>::quiet_NaN();
    const double Infinity = std::numeric_limits<double>::infinity();
    const float FloatNaN = std::numeric_limits<float>::quiet_NaN();
    Fixture.Author(FVector(0.0, 0.0, -25.0), FloatNaN);
    // Simulate serialized authored corruption directly, without constructing an
    // invalid FVector that UE's own diagnostic constructor would sanitize first.
    FVector* CorruptAuthoredExtents = Fixture.ExtentsProperty->ContainerPtrToValuePtr<FVector>(Fixture.Surface);
    CorruptAuthoredExtents->X = NaN;
    CorruptAuthoredExtents->Y = Infinity;
    Fixture.Construct();
    CheckFootprint(*this, Fixture, FVector(100.0), 0.0f);
    TestTrue(TEXT("Effective extent getter sanitizes each invalid/undersized axis"), Fixture.Surface->GetSurfaceExtents().Equals(FVector(100.0)));
    TestEqual(TEXT("Nonfinite height has effective zero"), Fixture.Surface->GetSurfaceHeight(), 0.0f);
    TestTrue(TEXT("Authored invalid values are not rewritten"), FMath::IsNaN(Fixture.AuthoredExtents().X) &&
        !FMath::IsFinite(Fixture.AuthoredExtents().Y) && Fixture.AuthoredExtents().Z == -25.0 && FMath::IsNaN(Fixture.AuthoredHeight()));
    TestFalse(TEXT("Sanitized transform contains no NaN or infinity"), Fixture.Mesh->GetRelativeTransform().ContainsNaN());
    Fixture.Author(FVector(0.0, 99.0, 101.0), -22.0f);
    Fixture.Construct();
    CheckFootprint(*this, Fixture, FVector(100.0, 100.0, 101.0), -22.0f);
    TestTrue(TEXT("Finite undersized authored values are preserved"), Fixture.AuthoredExtents().Equals(FVector(0.0, 99.0, 101.0)));

    TStrongObjectPtr<UStaticMesh> DegenerateMesh(NewObject<UStaticMesh>());
    const FVector DegenerateExtents[] = {FVector(0.0, 40.0, 10.0), FVector(40.0, 0.0, 10.0)};
    for (const FVector& LocalExtents : DegenerateExtents)
    {
        DegenerateMesh->SetExtendedBounds(FBoxSphereBounds(FVector::ZeroVector, LocalExtents, 50.0));
        Fixture.Mesh->SetStaticMesh(DegenerateMesh.Get());
        const FTransform BeforeMesh = Fixture.Mesh->GetRelativeTransform();
        Fixture.Author(FVector(777.0, 333.0, 110.0), 82.0f);
        Fixture.Volume->SetBoxExtent(FVector(100.0));
        Fixture.Construct();
        TestTrue(TEXT("Degenerate assigned XY mesh bounds preserve finite existing geometry"), Fixture.Mesh->GetRelativeTransform().Equals(BeforeMesh));
        TestFalse(TEXT("Degenerate assigned mesh cannot introduce invalid transform"), Fixture.Mesh->GetRelativeTransform().ContainsNaN());
        TestTrue(TEXT("Volume still updates when mesh cannot be fitted"), Fixture.Volume->GetUnscaledBoxExtent().Equals(FVector(777.0, 333.0, 110.0)));
    }
    Fixture.Mesh->SetStaticMesh(nullptr);
    const FTransform BeforeMissingMesh = Fixture.Mesh->GetRelativeTransform();
    Fixture.Author(FVector(550.0, 660.0, 120.0), 14.0f);
    Fixture.Surface->ReregisterAllComponents();
    TestTrue(TEXT("Missing mesh does not prevent volume synchronization"), Fixture.Volume->GetUnscaledBoxExtent().Equals(FVector(550.0, 660.0, 120.0)));
    TestTrue(TEXT("Missing mesh preserves last finite mesh transform"), Fixture.Mesh->GetRelativeTransform().Equals(BeforeMissingMesh));
    return true;
}

#endif