#include "BHFieldArmory.h"

#include "BHCharacter.h"
#include "BHSaveSubsystem.h"
#include "BHWarSubsystem.h"
#include "BHWeaponComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "UObject/ConstructorHelpers.h"

ABHFieldArmory::ABHFieldArmory()
{
    PrimaryActorTick.bCanEverTick = false;

#if WITH_EDITOR
    SetIsSpatiallyLoaded(false);
#endif

    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
    SetRootComponent(SceneRoot);

    ArmoryMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ArmoryMesh"));
    ArmoryMesh->SetupAttachment(SceneRoot);
    ArmoryMesh->SetRelativeLocation(FVector(0.0f, 0.0f, 75.0f));
    ArmoryMesh->SetRelativeScale3D(FVector(1.6f, 0.8f, 0.75f));
    ArmoryMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    ArmoryMesh->SetCollisionResponseToAllChannels(ECR_Block);

    static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(
        TEXT("/Engine/BasicShapes/Cube.Cube")
    );
    if (CubeMesh.Succeeded())
    {
        ArmoryMesh->SetStaticMesh(CubeMesh.Object);
    }

    ArmoryLabel = CreateDefaultSubobject<UTextRenderComponent>(TEXT("ArmoryLabel"));
    ArmoryLabel->SetupAttachment(SceneRoot);
    ArmoryLabel->SetRelativeLocation(FVector(0.0f, 0.0f, 190.0f));
    ArmoryLabel->SetHorizontalAlignment(EHTA_Center);
    ArmoryLabel->SetWorldSize(25.0f);
    ArmoryLabel->SetTextRenderColor(FColor(75, 175, 235));
    ArmoryLabel->SetText(
        NSLOCTEXT("BrokenHorizon", "FieldArmoryLabel", "FIELD ARMORY")
    );
}

void ABHFieldArmory::Interact_Implementation(AActor* InteractingActor)
{
    ABHCharacter* Character = Cast<ABHCharacter>(InteractingActor);
    UGameInstance* GameInstance = GetGameInstance();
    UBHWarSubsystem* WarSubsystem = IsValid(GameInstance)
        ? GameInstance->GetSubsystem<UBHWarSubsystem>()
        : nullptr;
    UBHSaveSubsystem* SaveSubsystem = IsValid(GameInstance)
        ? GameInstance->GetSubsystem<UBHSaveSubsystem>()
        : nullptr;
    UBHWeaponComponent* WeaponComponent = IsValid(Character)
        ? Character->GetWeaponComponent()
        : nullptr;

    if (!HasAuthority() || !IsValid(Character) ||
        !IsValid(WeaponComponent) || !IsValid(WarSubsystem) ||
        SectorID.IsNone())
    {
        return;
    }

    const FBHWarSectorState Sector = WarSubsystem->GetSectorState(SectorID);
    if (Sector.SectorID.IsNone() || Sector.Owner != EBHWarFaction::Friendly)
    {
        Character->ShowStatusNotification(
            NSLOCTEXT(
                "BrokenHorizon",
                "FieldArmoryLocked",
                "FIELD ARMORY LOCKED\n\nSecure friendly control of this sector first."
            )
        );
        return;
    }

    if (!WarSubsystem->ConsumeSectorSupply(SectorID, RoleChangeSupplyCost))
    {
        Character->ShowStatusNotification(
            FText::Format(
                NSLOCTEXT(
                    "BrokenHorizon",
                    "FieldArmorySupplyRequired",
                    "ARMORY SUPPLY REQUIRED\n\n{0}% sector supply required to reconfigure and refill."
                ),
                FText::AsNumber(FMath::CeilToInt(RoleChangeSupplyCost))
            )
        );
        return;
    }

    const EBHWeaponRole NewRole = WeaponComponent->CycleWeaponRole(true);
    const FBHWeaponRoleProfile Profile =
        UBHWeaponComponent::BuildWeaponRoleProfile(NewRole);
    const bool bSaved = IsValid(SaveSubsystem) && SaveSubsystem->SaveProgress();
    const FBHWarSectorState UpdatedSector = WarSubsystem->GetSectorState(SectorID);
    Character->ShowStatusNotification(
        FText::Format(
            NSLOCTEXT(
                "BrokenHorizon",
                "FieldArmoryRoleChanged",
                "LOADOUT // {0}\n\n{1}\nFULL AMMUNITION // SECTOR SUPPLY {2}% // {3}"
            ),
            Profile.DisplayName,
            Profile.TacticalDescription,
            FText::AsNumber(FMath::RoundToInt(UpdatedSector.Supply)),
            bSaved
                ? NSLOCTEXT("BrokenHorizon", "FieldArmorySaved", "CHECKPOINT SAVED")
                : NSLOCTEXT("BrokenHorizon", "FieldArmorySaveFailed", "CHECKPOINT SAVE FAILED")
        )
    );
    UE_LOG(
        LogTemp,
        Display,
        TEXT("BH_WEAPON_ROLE_CHANGED sector=%s role=%d cost=%.1f saved=%d"),
        *SectorID.ToString(),
        static_cast<int32>(NewRole),
        RoleChangeSupplyCost,
        bSaved ? 1 : 0
    );
}

FText ABHFieldArmory::GetInteractionText_Implementation() const
{
    return FText::Format(
        NSLOCTEXT(
            "BrokenHorizon",
            "FieldArmoryPrompt",
            "[F] Cycle Weapon Role / Refill // {0}% Supply"
        ),
        FText::AsNumber(FMath::CeilToInt(RoleChangeSupplyCost))
    );
}

void ABHFieldArmory::ConfigureArmory(FName NewSectorID)
{
    SectorID = NewSectorID;
}

FName ABHFieldArmory::GetSectorID() const
{
    return SectorID;
}
