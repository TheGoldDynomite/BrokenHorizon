#include "BHEngineeringCharge.h"

#include "BHCharacter.h"
#include "BHDoor.h"
#include "BHRaidSabotageTarget.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/DamageType.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "Perception/AISense_Hearing.h"
#include "UObject/ConstructorHelpers.h"

ABHEngineeringCharge::ABHEngineeringCharge()
{
    PrimaryActorTick.bCanEverTick = false;
    bReplicates = true;
    SetReplicateMovement(true);

    CollisionRoot = CreateDefaultSubobject<UBoxComponent>(TEXT("CollisionRoot"));
    SetRootComponent(CollisionRoot);
    CollisionRoot->SetBoxExtent(FVector(16.0f, 10.0f, 5.0f));
    CollisionRoot->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    CollisionRoot->SetCollisionResponseToAllChannels(ECR_Block);
    CollisionRoot->SetCanEverAffectNavigation(false);

    ChargeMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ChargeMesh"));
    ChargeMesh->SetupAttachment(CollisionRoot);
    ChargeMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    ChargeMesh->SetRelativeScale3D(FVector(0.32f, 0.20f, 0.08f));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(
        TEXT("/Engine/BasicShapes/Cube.Cube")
    );
    if (Cube.Succeeded())
    {
        ChargeMesh->SetStaticMesh(Cube.Object);
    }

    ChargeLabel = CreateDefaultSubobject<UTextRenderComponent>(TEXT("ChargeLabel"));
    ChargeLabel->SetupAttachment(CollisionRoot);
    ChargeLabel->SetRelativeLocation(FVector(0.0f, 0.0f, 22.0f));
    ChargeLabel->SetHorizontalAlignment(EHTA_Center);
    ChargeLabel->SetWorldSize(15.0f);
    RefreshPresentation();
}

void ABHEngineeringCharge::GetLifetimeReplicatedProps(
    TArray<FLifetimeProperty>& OutLifetimeProps
) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ABHEngineeringCharge, ChargeMode);
    DOREPLIFETIME(ABHEngineeringCharge, bArmed);
    DOREPLIFETIME(ABHEngineeringCharge, bDetonated);
    DOREPLIFETIME(ABHEngineeringCharge, AttachedTarget);
}

void ABHEngineeringCharge::BeginPlay()
{
    Super::BeginPlay();
    CurrentHealth = FMath::Max(1.0f, MaximumHealth);
    RefreshPresentation();
}

void ABHEngineeringCharge::InitializeCharge(
    ABHCharacter* PlacingCharacter,
    AActor* PlacementTarget,
    EBHEngineeringChargeMode NewMode
)
{
    if (!HasAuthority() || !IsValid(PlacingCharacter))
    {
        return;
    }
    PlacedByCharacter = PlacingCharacter;
    SetOwner(PlacingCharacter);
    SetInstigator(PlacingCharacter);
    AttachedTarget = PlacementTarget;
    ChargeMode = NewMode;
    bArmed = false;
    bDetonated = false;
    if (IsValid(AttachedTarget))
    {
        AttachToActor(
            AttachedTarget,
            FAttachmentTransformRules::KeepWorldTransform
        );
    }
    GetWorldTimerManager().SetTimer(
        ArmingTimerHandle,
        this,
        &ABHEngineeringCharge::FinishArming,
        FMath::Max(0.1f, ArmingDelay),
        false
    );
    RefreshPresentation();
    ForceNetUpdate();
}

void ABHEngineeringCharge::FinishArming()
{
    if (!HasAuthority() || bDetonated)
    {
        return;
    }
    bArmed = true;
    RefreshPresentation();
    ForceNetUpdate();
    UAISense_Hearing::ReportNoiseEvent(
        this, GetActorLocation(), 0.2f, PlacedByCharacter, 700.0f,
        FName(TEXT("Engineering"))
    );
    UE_LOG(LogTemp, Display, TEXT("BH_ENGINEERING_CHARGE state=armed mode=%d"),
        static_cast<int32>(ChargeMode));
}

bool ABHEngineeringCharge::Detonate(ABHCharacter* RequestingCharacter)
{
    if (!HasAuthority() || !CanCommandDetonate(
            bArmed,
            bDetonated,
            IsValid(RequestingCharacter) &&
                RequestingCharacter == PlacedByCharacter))
    {
        return false;
    }
    bDetonated = true;
    SetActorEnableCollision(false);
    if (IsValid(ChargeMesh))
    {
        ChargeMesh->SetVisibility(false, true);
    }
    if (ChargeMode == EBHEngineeringChargeMode::Breach)
    {
        if (ABHDoor* Door = Cast<ABHDoor>(AttachedTarget))
        {
            Door->BreachDoor(RequestingCharacter);
        }
    }
    if (ABHRaidSabotageTarget* Target =
        Cast<ABHRaidSabotageTarget>(AttachedTarget))
    {
        Target->SabotageByEngineeringCharge(RequestingCharacter, this);
    }

    TArray<AActor*> IgnoredActors;
    IgnoredActors.Add(this);
    UGameplayStatics::ApplyRadialDamageWithFalloff(
        this,
        GetMaximumDamage(ChargeMode),
        25.0f,
        GetActorLocation(),
        ChargeMode == EBHEngineeringChargeMode::Breach ? 120.0f : 180.0f,
        GetOuterDamageRadius(ChargeMode),
        1.0f,
        UDamageType::StaticClass(),
        IgnoredActors,
        this,
        RequestingCharacter ? RequestingCharacter->GetController() : nullptr,
        ECC_Visibility
    );
    UAISense_Hearing::ReportNoiseEvent(
        this, GetActorLocation(), 3.0f, RequestingCharacter, 8500.0f,
        FName(TEXT("Explosion"))
    );
    UE_LOG(LogTemp, Display,
        TEXT("BH_ENGINEERING_CHARGE state=detonated mode=%d radius=%.0f damage=%.0f"),
        static_cast<int32>(ChargeMode), GetOuterDamageRadius(ChargeMode),
        GetMaximumDamage(ChargeMode));
    NotifyOwningCharacterRemoved();
    Destroy();
    return true;
}

float ABHEngineeringCharge::TakeDamage(
    float DamageAmount,
    const FDamageEvent& DamageEvent,
    AController* EventInstigator,
    AActor* DamageCauser
)
{
    static_cast<void>(DamageEvent);
    static_cast<void>(EventInstigator);
    static_cast<void>(DamageCauser);
    if (!HasAuthority() || bDetonated || DamageAmount <= 0.0f)
    {
        return 0.0f;
    }
    const float Applied = FMath::Min(CurrentHealth, DamageAmount);
    CurrentHealth -= Applied;
    if (CurrentHealth <= 0.0f)
    {
        if (bArmed && IsValid(PlacedByCharacter))
        {
            Detonate(PlacedByCharacter);
        }
        else
        {
            NotifyOwningCharacterRemoved();
            Destroy();
        }
    }
    return Applied;
}

void ABHEngineeringCharge::Interact_Implementation(AActor* InteractingActor)
{
    ABHCharacter* Character = Cast<ABHCharacter>(InteractingActor);
    if (!HasAuthority() || !IsValid(Character) || bDetonated)
    {
        return;
    }
    if (Character == PlacedByCharacter)
    {
        Character->AddEngineeringCharges(1);
    }
    NotifyOwningCharacterRemoved();
    Destroy();
}

FText ABHEngineeringCharge::GetInteractionText_Implementation() const
{
    return bArmed
        ? NSLOCTEXT("BrokenHorizon", "DisarmEngineeringCharge", "Hold [F] to disarm charge")
        : NSLOCTEXT("BrokenHorizon", "RecoverEngineeringCharge", "Hold [F] to recover charge");
}

bool ABHEngineeringCharge::IsArmed() const
{
    return bArmed && !bDetonated;
}

EBHEngineeringChargeMode ABHEngineeringCharge::GetChargeMode() const
{
    return ChargeMode;
}

float ABHEngineeringCharge::GetOuterDamageRadius(EBHEngineeringChargeMode Mode)
{
    return Mode == EBHEngineeringChargeMode::Breach ? 450.0f : 650.0f;
}

float ABHEngineeringCharge::GetMaximumDamage(EBHEngineeringChargeMode Mode)
{
    return Mode == EBHEngineeringChargeMode::Breach ? 180.0f : 140.0f;
}

bool ABHEngineeringCharge::CanCommandDetonate(
    bool bIsArmed,
    bool bAlreadyDetonated,
    bool bRequesterOwnsCharge
)
{
    return bIsArmed && !bAlreadyDetonated && bRequesterOwnsCharge;
}

void ABHEngineeringCharge::OnRep_ChargeState()
{
    RefreshPresentation();
}

void ABHEngineeringCharge::RefreshPresentation()
{
    if (!IsValid(ChargeLabel))
    {
        return;
    }
    ChargeLabel->SetText(bArmed
        ? NSLOCTEXT("BrokenHorizon", "EngineeringChargeArmedLabel", "ARMED")
        : NSLOCTEXT("BrokenHorizon", "EngineeringChargeArmingLabel", "ARMING"));
    ChargeLabel->SetTextRenderColor(bArmed
        ? FColor(255, 70, 35) : FColor(255, 190, 45));
}

void ABHEngineeringCharge::NotifyOwningCharacterRemoved()
{
    if (!bDetonated)
    {
        if (ABHDoor* Door = Cast<ABHDoor>(AttachedTarget))
        {
            Door->SetBreachChargePlanted(false);
        }
    }
    if (IsValid(PlacedByCharacter))
    {
        PlacedByCharacter->NotifyEngineeringChargeRemoved(this);
    }
}
