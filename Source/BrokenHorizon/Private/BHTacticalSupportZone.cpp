#include "BHTacticalSupportZone.h"
#include "BHBattlefieldConditions.h"

#include "BHCharacter.h"
#include "BHEnemySoldier.h"
#include "BHImpactEffect.h"
#include "Components/PointLightComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "GameFramework/DamageType.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "Net/UnrealNetwork.h"
#include "Perception/AISense_Hearing.h"
#include "UObject/ConstructorHelpers.h"

ABHTacticalSupportZone::ABHTacticalSupportZone()
{
    PrimaryActorTick.bCanEverTick = true;
    bReplicates = true;
    SetReplicateMovement(true);
    SetActorEnableCollision(false);

    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
    SetRootComponent(SceneRoot);

    MarkerMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MarkerMesh"));
    MarkerMesh->SetupAttachment(SceneRoot);
    MarkerMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    MarkerMesh->SetCastShadow(false);

    static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(
        TEXT("/Engine/BasicShapes/Sphere.Sphere")
    );
    static ConstructorHelpers::FObjectFinder<UStaticMesh> SmokeCardMesh(
        TEXT("/Engine/BasicShapes/Plane.Plane")
    );
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> SmokeCardMaterial(
        TEXT(
            "/Game/BrokenHorizon/Effects/Materials/"
            "M_BH_TacticalSmokeCardFinal.M_BH_TacticalSmokeCardFinal"
        )
    );
    if (SphereMesh.Succeeded())
    {
        MarkerMesh->SetStaticMesh(SphereMesh.Object);

        static const FVector PuffLocations[] = {
            FVector(-520.0f, -190.0f, 90.0f),
            FVector(-390.0f, 310.0f, 150.0f),
            FVector(-220.0f, -420.0f, 190.0f),
            FVector(-90.0f, 120.0f, 100.0f),
            FVector(80.0f, -180.0f, 220.0f),
            FVector(190.0f, 440.0f, 120.0f),
            FVector(320.0f, -370.0f, 160.0f),
            FVector(480.0f, 170.0f, 210.0f),
            FVector(-610.0f, 410.0f, 240.0f),
            FVector(610.0f, -70.0f, 110.0f),
            FVector(-40.0f, 570.0f, 260.0f),
            FVector(110.0f, -590.0f, 130.0f)
        };
        static const float PuffScales[] = {
            1.8f, 2.2f, 1.6f, 2.4f, 2.0f, 1.7f,
            2.1f, 1.9f, 1.5f, 1.8f, 1.6f, 2.0f
        };
        for (int32 Index = 0; Index < UE_ARRAY_COUNT(PuffLocations); ++Index)
        {
            UStaticMeshComponent* Puff = CreateDefaultSubobject<UStaticMeshComponent>(
                *FString::Printf(TEXT("SmokePuff_%02d"), Index)
            );
            Puff->SetupAttachment(SceneRoot);
            Puff->SetStaticMesh(
                SmokeCardMesh.Succeeded()
                    ? SmokeCardMesh.Object
                    : SphereMesh.Object
            );
            Puff->SetRelativeLocation(PuffLocations[Index]);
            Puff->SetRelativeRotation(
                FRotator(
                    90.0f,
                    Index * 31.0f,
                    (Index % 4) * 11.0f
                )
            );
            Puff->SetRelativeScale3D(
                FVector(
                    PuffScales[Index] * 2.2f,
                    PuffScales[Index] * 1.6f,
                    PuffScales[Index]
                )
            );
            Puff->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            Puff->SetCastShadow(false);
            Puff->SetTranslucentSortPriority(Index);
            if (SmokeCardMaterial.Succeeded())
            {
                Puff->SetMaterial(0, SmokeCardMaterial.Object);
            }
            SmokePuffs.Add(Puff);
        }
    }

    SupportLabel = CreateDefaultSubobject<UTextRenderComponent>(TEXT("SupportLabel"));
    SupportLabel->SetupAttachment(SceneRoot);
    SupportLabel->SetRelativeLocation(FVector(0.0f, 0.0f, 180.0f));
    SupportLabel->SetHorizontalAlignment(EHTA_Center);
    SupportLabel->SetWorldSize(35.0f);

    SupportLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("SupportLight"));
    SupportLight->SetupAttachment(SceneRoot);
    SupportLight->SetRelativeLocation(FVector(0.0f, 0.0f, 80.0f));
    SupportLight->SetAttenuationRadius(900.0f);
    SupportLight->SetIntensity(2400.0f);
    SupportLight->SetCastShadows(false);
}

void ABHTacticalSupportZone::BeginPlay()
{
    Super::BeginPlay();
    RefreshPresentation();


    if (!HasAuthority())
    {
        return;
    }

    if (SupportType == EBHTacticalSupportType::SmokeScreen)
    {
        SetLifeSpan(FMath::Max(1.0f, SmokeDuration));
    }
    else
    {
        for (TActorIterator<ABHCharacter> It(GetWorld()); It; ++It)
        {
            ABHCharacter* Character = *It;
            if (IsValid(Character) &&
                FVector::DistSquared(
                    Character->GetActorLocation(),
                    GetActorLocation()
                ) <= FMath::Square(
                    GetSupportRadius(SupportType) + 500.0f
                ))
            {
                Character->ShowPriorityStatusNotification(
                    NSLOCTEXT(
                        "BrokenHorizon",
                        "MortarDangerCloseWarning",
                        "DANGER CLOSE // MORTAR INBOUND\n\nLeave the marked beaten zone immediately."
                    ),
                    EBHNotificationPriority::Critical
                );
            }
        }
        GetWorldTimerManager().SetTimer(
            SupportTimer,
            this,
            &ABHTacticalSupportZone::FireNextMortarShell,
            FMath::Max(0.1f, MortarWarningDelay),
            false
        );
        const float MortarLifeSpan =
            FMath::Max(1.0f, MortarWarningDelay) +
            (GetMortarShellCount() * FMath::Max(0.1f, MortarShellInterval)) +
            2.0f;
        SetLifeSpan(
            FParse::Param(
                FCommandLine::Get(),
                TEXT("BHVisualTacticalSupportRuntime")
            ) ? 22.0f : MortarLifeSpan
        );
    }
}

void ABHTacticalSupportZone::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (IsValid(MarkerMesh))
    {
        MarkerMesh->SetVisibility(
            SupportType == EBHTacticalSupportType::MortarBarrage,
            true
        );
        MarkerMesh->AddLocalRotation(FRotator(0.0f, DeltaSeconds * 35.0f, 0.0f));
        const float Pulse = 1.0f + FMath::Sin(GetGameTimeSinceCreation() * 3.0f) * 0.12f;
        const float BaseScale = SupportType == EBHTacticalSupportType::SmokeScreen ? 1.2f : 0.35f;
        MarkerMesh->SetRelativeScale3D(FVector(BaseScale * Pulse));
    }
    if (SupportType == EBHTacticalSupportType::SmokeScreen)
    {
        for (int32 Index = 0; Index < SmokePuffs.Num(); ++Index)
        {
            UStaticMeshComponent* Puff = SmokePuffs[Index];
            if (!IsValid(Puff))
            {
                continue;
            }
            const float DriftDirection = (Index % 2 == 0) ? 1.0f : -1.0f;
            Puff->AddLocalRotation(
                FRotator(
                    DeltaSeconds * 2.0f * DriftDirection,
                    DeltaSeconds * (4.0f + Index * 0.25f),
                    0.0f
                )
            );
        }
    }
}

void ABHTacticalSupportZone::GetLifetimeReplicatedProps(
    TArray<FLifetimeProperty>& OutLifetimeProps
) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ABHTacticalSupportZone, SupportType);
}

void ABHTacticalSupportZone::InitializeSupport(
    EBHTacticalSupportType NewSupportType,
    ABHCharacter* NewRequestingCharacter
)
{
    SupportType = NewSupportType;
    RequestingCharacter = NewRequestingCharacter;
    SetOwner(NewRequestingCharacter);
    SetInstigator(NewRequestingCharacter);
    RefreshPresentation();
}

EBHTacticalSupportType ABHTacticalSupportZone::GetSupportType() const
{
    return SupportType;
}

int32 ABHTacticalSupportZone::GetFiredMortarShells() const
{
    return FiredMortarShells;
}

FText ABHTacticalSupportZone::GetSupportDisplayName(EBHTacticalSupportType Type)
{
    return Type == EBHTacticalSupportType::MortarBarrage
        ? NSLOCTEXT("BrokenHorizon", "MortarBarrageName", "MORTAR BARRAGE")
        : NSLOCTEXT("BrokenHorizon", "SmokeScreenName", "SMOKE SCREEN");
}

float ABHTacticalSupportZone::GetSupportRadius(EBHTacticalSupportType Type)
{
    return Type == EBHTacticalSupportType::MortarBarrage ? 650.0f : 900.0f;
}

int32 ABHTacticalSupportZone::GetMortarShellCount()
{
    return 3;
}

bool ABHTacticalSupportZone::ShouldAffectSoldier(
    EBHTacticalSupportType Type,
    bool bSoldierHostileToRequester,
    float DistanceCentimeters
)
{
    return bSoldierHostileToRequester &&
        DistanceCentimeters <= GetSupportRadius(Type);
}

bool ABHTacticalSupportZone::IsLineObscuredBySmoke(
    const UWorld* World,
    const FVector& SightOrigin,
    const FVector& TargetLocation
)
{
    if (!IsValid(World) || SightOrigin.Equals(TargetLocation))
    {
        return false;
    }

    for (TActorIterator<ABHTacticalSupportZone> It(World); It; ++It)
    {
        const ABHTacticalSupportZone* Zone = *It;
        if (!IsValid(Zone) ||
            Zone->SupportType != EBHTacticalSupportType::SmokeScreen)
        {
            continue;
        }

        const FVector Closest = FMath::ClosestPointOnSegment(
            Zone->GetActorLocation(),
            SightOrigin,
            TargetLocation
        );
        if (FVector::DistSquared(Closest, Zone->GetActorLocation()) <=
            FMath::Square(GetSupportRadius(EBHTacticalSupportType::SmokeScreen)))
        {
            return true;
        }
    }
    return false;
}

void ABHTacticalSupportZone::OnRep_SupportType()
{
    RefreshPresentation();
}

void ABHTacticalSupportZone::RefreshPresentation()
{
    const bool bMortar = SupportType == EBHTacticalSupportType::MortarBarrage;
    const FColor Color = bMortar ? FColor(255, 80, 35) : FColor(150, 175, 185);
    if (IsValid(SupportLabel))
    {
        SupportLabel->SetText(GetSupportDisplayName(SupportType));
        SupportLabel->SetTextRenderColor(Color);
        SupportLabel->SetVisibility(bMortar, true);
    }
    if (IsValid(SupportLight))
    {
        SupportLight->SetLightColor(FLinearColor(Color));
        SupportLight->SetIntensity(bMortar ? 3000.0f : 0.0f);
    }
    for (UStaticMeshComponent* Puff : SmokePuffs)
    {
        if (IsValid(Puff))
        {
            Puff->SetVisibility(!bMortar, true);
        }
    }
}

FVector ABHTacticalSupportZone::ResolveShellImpactLocation(int32 ShellIndex) const
{
    static const FVector2D Pattern[] = {
        FVector2D(0.0f, 0.0f),
        FVector2D(390.0f, -260.0f),
        FVector2D(-340.0f, 320.0f)
    };
    const FVector2D Offset = Pattern[
        FMath::Clamp(ShellIndex, 0, GetMortarShellCount() - 1)
    ] * UBHBattlefieldConditions::GetCurrentProfile(this).
        MortarDispersionMultiplier;
    return GetActorLocation() + FVector(Offset.X, Offset.Y, 20.0f);
}

void ABHTacticalSupportZone::FireNextMortarShell()
{
    if (!HasAuthority() || FiredMortarShells >= GetMortarShellCount())
    {
        return;
    }

    const FVector Impact = ResolveShellImpactLocation(FiredMortarShells);
    TArray<AActor*> Ignored;
    Ignored.Add(this);
    UGameplayStatics::ApplyRadialDamageWithFalloff(
        this,
        MortarMaximumDamage,
        MortarMinimumDamage,
        Impact,
        180.0f,
        GetSupportRadius(SupportType),
        1.0f,
        UDamageType::StaticClass(),
        Ignored,
        this,
        IsValid(RequestingCharacter) ? RequestingCharacter->GetController() : nullptr,
        ECC_Visibility
    );
    UAISense_Hearing::ReportNoiseEvent(
        this,
        Impact,
        3.0f,
        RequestingCharacter,
        5000.0f,
        FName(TEXT("Artillery"))
    );
    MulticastMortarImpact(Impact);
    ++FiredMortarShells;

    UE_LOG(
        LogTemp,
        Display,
        TEXT("BH_MORTAR_SHELL_IMPACT shell=%d/%d location=%s"),
        FiredMortarShells,
        GetMortarShellCount(),
        *Impact.ToCompactString()
    );

    if (FiredMortarShells < GetMortarShellCount())
    {
        GetWorldTimerManager().SetTimer(
            SupportTimer,
            this,
            &ABHTacticalSupportZone::FireNextMortarShell,
            FMath::Max(0.1f, MortarShellInterval),
            false
        );
    }
}

void ABHTacticalSupportZone::MulticastMortarImpact_Implementation(
    FVector_NetQuantize ImpactLocation
)
{
    UWorld* World = GetWorld();
    if (!IsValid(World))
    {
        return;
    }
    FHitResult Hit;
    Hit.ImpactPoint = ImpactLocation;
    Hit.Location = ImpactLocation;
    Hit.ImpactNormal = FVector::UpVector;
    ABHImpactEffect* Effect = World->SpawnActor<ABHImpactEffect>(
        ABHImpactEffect::StaticClass(),
        ImpactLocation,
        FRotator::ZeroRotator
    );
    if (IsValid(Effect))
    {
        Effect->InitializeImpact(Hit);
        Effect->SetActorScale3D(FVector(4.0f));
    }
}
