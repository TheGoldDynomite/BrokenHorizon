#include "BHWorldKitModule.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"

#if !UE_BUILD_SHIPPING
#include "BHCharacter.h"
#include "HAL/IConsoleManager.h"

static FAutoConsoleCommandWithWorldAndArgs GSpawnWorldKitModuleCommand(
    TEXT("BHTestSpawnWorldKitModule"),
    TEXT("Spawns a modular world-kit shelter near the local player."),
    FConsoleCommandWithWorldAndArgsDelegate::CreateLambda(
        [](const TArray<FString>& Args, UWorld* World)
        {
            if (!IsValid(World))
            {
                return;
            }

            APlayerController* PC = World->GetFirstPlayerController();
            ABHCharacter* Character = IsValid(PC)
                ? Cast<ABHCharacter>(PC->GetPawn())
                : nullptr;
            if (!IsValid(Character))
            {
                UE_LOG(LogTemp, Warning,
                    TEXT("BH_TEST_WORLD_KIT_MODULE result=failure reason=no_player"));
                return;
            }

            const FVector SpawnLocation =
                Character->GetActorLocation() +
                Character->GetActorForwardVector() * 900.0f;
            ABHWorldKitModule* Module = World->SpawnActor<ABHWorldKitModule>(
                ABHWorldKitModule::StaticClass(),
                SpawnLocation,
                Character->GetActorRotation()
            );
            if (IsValid(Module))
            {
                Module->SetPersistenceIDForTesting(
                    FName(TEXT("BHTestWorldKitShelter01"))
                );
                if (Args.Num() > 0)
                {
                    const FString Variant = Args[0].ToLower();
                    if (Variant == TEXT("checkpoint"))
                    {
                        Module->SetModuleTypeForTesting(
                            EBHWorldKitModuleType::Checkpoint
                        );
                    }
                    else if (Variant == TEXT("depot"))
                    {
                        Module->SetModuleTypeForTesting(
                            EBHWorldKitModuleType::Depot
                        );
                    }
                    else if (Variant == TEXT("base"))
                    {
                        Module->SetModuleTypeForTesting(
                            EBHWorldKitModuleType::ResistanceBase
                        );
                    }
                }
            }
            UE_LOG(
                LogTemp,
                Display,
                TEXT("BH_TEST_WORLD_KIT_MODULE result=%s id=%s type=%d"),
                IsValid(Module) ? TEXT("success") : TEXT("failure"),
                IsValid(Module)
                    ? *Module->GetPersistenceID().ToString()
                    : TEXT("None"),
                IsValid(Module)
                    ? static_cast<int32>(Module->GetModuleType())
                    : -1
            );
        }
    )
);
#endif

ABHWorldKitModule::ABHWorldKitModule()
{
    SetReplicates(true);

    USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(
        TEXT("WorldKitRoot")
    );
    SetRootComponent(Root);
    Root->SetMobility(EComponentMobility::Static);

    FoundationMesh = CreateDefaultSubobject<UStaticMeshComponent>(
        TEXT("FoundationMesh")
    );
    WallLeftMesh = CreateDefaultSubobject<UStaticMeshComponent>(
        TEXT("WallLeftMesh")
    );
    WallRightMesh = CreateDefaultSubobject<UStaticMeshComponent>(
        TEXT("WallRightMesh")
    );
    RoofMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RoofMesh"));
    DoorwayMesh = CreateDefaultSubobject<UStaticMeshComponent>(
        TEXT("DoorwayMesh")
    );
    SignageMesh = CreateDefaultSubobject<UStaticMeshComponent>(
        TEXT("SignageMesh")
    );

    static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(
        TEXT("/Engine/BasicShapes/Cube.Cube")
    );
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> MilitaryMaterial(
        TEXT("/Game/BrokenHorizon/Environment/WorldKit/Materials/M_BH_Military.M_BH_Military")
    );

    const TArray<UStaticMeshComponent*> Parts = {
        FoundationMesh,
        WallLeftMesh,
        WallRightMesh,
        RoofMesh,
        DoorwayMesh,
        SignageMesh
    };
    for (UStaticMeshComponent* Part : Parts)
    {
        Part->SetupAttachment(Root);
        Part->SetMobility(EComponentMobility::Static);
        if (CubeMesh.Succeeded())
        {
            Part->SetStaticMesh(CubeMesh.Object);
        }
        if (MilitaryMaterial.Succeeded())
        {
            Part->SetMaterial(0, MilitaryMaterial.Object);
        }
    }

    FoundationMesh->SetRelativeScale3D(FVector(6.0f, 4.0f, 0.25f));
    FoundationMesh->SetRelativeLocation(FVector(0.0f, 0.0f, 25.0f));
    WallLeftMesh->SetRelativeScale3D(FVector(0.25f, 4.0f, 2.0f));
    WallLeftMesh->SetRelativeLocation(FVector(-575.0f, 0.0f, 225.0f));
    WallRightMesh->SetRelativeScale3D(FVector(0.25f, 4.0f, 2.0f));
    WallRightMesh->SetRelativeLocation(FVector(575.0f, 0.0f, 225.0f));
    RoofMesh->SetRelativeScale3D(FVector(6.0f, 4.0f, 0.25f));
    RoofMesh->SetRelativeLocation(FVector(0.0f, 0.0f, 425.0f));
    DoorwayMesh->SetRelativeScale3D(FVector(0.25f, 1.1f, 1.8f));
    DoorwayMesh->SetRelativeLocation(FVector(0.0f, 390.0f, 205.0f));
    SignageMesh->SetRelativeScale3D(FVector(1.5f, 0.12f, 0.35f));
    SignageMesh->SetRelativeLocation(FVector(0.0f, 405.0f, 360.0f));
}

static const TCHAR* WorldKitModuleTypeLabel(
    EBHWorldKitModuleType ModuleType
)
{
    switch (ModuleType)
    {
    case EBHWorldKitModuleType::Checkpoint:
        return TEXT("CHECKPOINT");
    case EBHWorldKitModuleType::Depot:
        return TEXT("DEPOT");
    case EBHWorldKitModuleType::ResistanceBase:
        return TEXT("RESISTANCE_BASE");
    case EBHWorldKitModuleType::Shelter:
    default:
        return TEXT("SHELTER");
    }
}

void ABHWorldKitModule::ApplyVariantPresentation()
{
    if (!IsValid(FoundationMesh) ||
        !IsValid(WallLeftMesh) ||
        !IsValid(WallRightMesh) ||
        !IsValid(RoofMesh) ||
        !IsValid(DoorwayMesh) ||
        !IsValid(SignageMesh))
    {
        return;
    }

    // Variant dimensions are selected from the authored module type during
    // BeginPlay. Temporarily allow transform updates, then restore static
    // mobility so the finished world-kit remains a static renderable.
    const TArray<UStaticMeshComponent*> Parts = {
        FoundationMesh,
        WallLeftMesh,
        WallRightMesh,
        RoofMesh,
        DoorwayMesh,
        SignageMesh
    };
    for (UStaticMeshComponent* Part : Parts)
    {
        if (IsValid(Part))
        {
            Part->SetMobility(EComponentMobility::Movable);
        }
    }

    switch (ModuleType)
    {
    case EBHWorldKitModuleType::Checkpoint:
        FoundationMesh->SetRelativeScale3D(FVector(3.5f, 2.0f, 0.25f));
        WallLeftMesh->SetRelativeLocation(FVector(-325.0f, 0.0f, 225.0f));
        WallRightMesh->SetRelativeLocation(FVector(325.0f, 0.0f, 225.0f));
        RoofMesh->SetRelativeScale3D(FVector(3.5f, 2.0f, 0.25f));
        DoorwayMesh->SetRelativeLocation(FVector(0.0f, 190.0f, 205.0f));
        SignageMesh->SetRelativeLocation(FVector(0.0f, 205.0f, 360.0f));
        break;
    case EBHWorldKitModuleType::Depot:
        FoundationMesh->SetRelativeScale3D(FVector(8.0f, 5.0f, 0.25f));
        WallLeftMesh->SetRelativeLocation(FVector(-775.0f, 0.0f, 225.0f));
        WallRightMesh->SetRelativeLocation(FVector(775.0f, 0.0f, 225.0f));
        RoofMesh->SetRelativeScale3D(FVector(8.0f, 5.0f, 0.25f));
        DoorwayMesh->SetRelativeLocation(FVector(0.0f, 490.0f, 205.0f));
        SignageMesh->SetRelativeLocation(FVector(0.0f, 505.0f, 360.0f));
        break;
    case EBHWorldKitModuleType::ResistanceBase:
        FoundationMesh->SetRelativeScale3D(FVector(10.0f, 7.0f, 0.25f));
        WallLeftMesh->SetRelativeLocation(FVector(-975.0f, 0.0f, 225.0f));
        WallRightMesh->SetRelativeLocation(FVector(975.0f, 0.0f, 225.0f));
        RoofMesh->SetRelativeScale3D(FVector(10.0f, 7.0f, 0.25f));
        DoorwayMesh->SetRelativeLocation(FVector(0.0f, 690.0f, 205.0f));
        SignageMesh->SetRelativeLocation(FVector(0.0f, 705.0f, 360.0f));
        break;
    case EBHWorldKitModuleType::Shelter:
    default:
        break;
    }

    for (UStaticMeshComponent* Part : Parts)
    {
        if (IsValid(Part))
        {
            Part->SetMobility(EComponentMobility::Static);
        }
    }
}

void ABHWorldKitModule::BeginPlay()
{
    Super::BeginPlay();
    ApplyVariantPresentation();

    UE_LOG(
        LogTemp,
        Display,
        TEXT(
            "BH_WORLD_KIT_MODULE id=%s type=%s foundation=%d walls=%d "
            "roof=%d doorway=%d signage=%d"
        ),
        *PersistenceID.ToString(),
        WorldKitModuleTypeLabel(ModuleType),
        IsValid(FoundationMesh) ? 1 : 0,
        IsValid(WallLeftMesh) && IsValid(WallRightMesh) ? 1 : 0,
        IsValid(RoofMesh) ? 1 : 0,
        IsValid(DoorwayMesh) ? 1 : 0,
        IsValid(SignageMesh) ? 1 : 0
    );
}

void ABHWorldKitModule::ConfigureModuleTypeForAuthoring(
    const FString& TypeName
)
{
    if (TypeName.Equals(TEXT("CHECKPOINT"), ESearchCase::IgnoreCase))
    {
        ModuleType = EBHWorldKitModuleType::Checkpoint;
    }
    else if (TypeName.Equals(TEXT("DEPOT"), ESearchCase::IgnoreCase))
    {
        ModuleType = EBHWorldKitModuleType::Depot;
    }
    else if (TypeName.Equals(TEXT("RESISTANCE_BASE"), ESearchCase::IgnoreCase) ||
             TypeName.Equals(TEXT("BASE"), ESearchCase::IgnoreCase))
    {
        ModuleType = EBHWorldKitModuleType::ResistanceBase;
    }
    else
    {
        ModuleType = EBHWorldKitModuleType::Shelter;
    }
}

#if !UE_BUILD_SHIPPING
void ABHWorldKitModule::SetPersistenceIDForTesting(FName InPersistenceID)
{
    PersistenceID = InPersistenceID;
}

void ABHWorldKitModule::SetModuleTypeForTesting(
    EBHWorldKitModuleType InModuleType
)
{
    ModuleType = InModuleType;
}
#endif
