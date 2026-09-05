#include "BHDefenseMissionDirector.h"

#include "BHCharacter.h"
#include "BHEnemySoldier.h"
#include "BHMissionData.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"

ABHDefenseMissionDirector::ABHDefenseMissionDirector()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickInterval = 0.25f;
}

void ABHDefenseMissionDirector::InitializeDefenseMission(
    ABHCharacter* InPlayerCharacter
)
{
    if (!HasAuthority() || !IsValid(InPlayerCharacter))
    {
        return;
    }

    PlayerCharacter = InPlayerCharacter;
    CaptureExistingDefenders();
}

void ABHDefenseMissionDirector::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    if (!HasAuthority())
    {
        SetActorTickEnabled(false);
        return;
    }

    if (!IsValid(PlayerCharacter) || bDefenseComplete)
    {
        return;
    }

    if (!bDefenseActive)
    {
        if (PlayerCharacter->GetCurrentObjectiveID() ==
            BHObjectiveIds::EliminateGuard)
        {
            BeginDefense();
        }

        return;
    }

    if (HasLivingTrackedEnemies())
    {
        return;
    }

    if (CurrentWave >= TotalWaves)
    {
        CompleteDefense();
        return;
    }

    if (!bWaitingForWave)
    {
        ScheduleNextWave();
        return;
    }

    const UWorld* World = GetWorld();

    if (IsValid(World) &&
        World->GetTimeSeconds() >= NextWaveTime)
    {
        SpawnNextWave();
    }
}

int32 ABHDefenseMissionDirector::GetCurrentWave() const
{
    return CurrentWave;
}

int32 ABHDefenseMissionDirector::GetTotalWaves() const
{
    return TotalWaves;
}

void ABHDefenseMissionDirector::EndPlay(
    const EEndPlayReason::Type EndPlayReason
)
{
    if (HasAuthority() && !bDefenseComplete)
    {
        for (ABHEnemySoldier* Enemy : TrackedEnemies)
        {
            if (IsValid(Enemy) && !Enemy->IsDead())
            {
                Enemy->SetObjectiveIdToCompleteOnDeath(
                    BHObjectiveIds::EliminateGuard
                );
            }
        }
    }

    Super::EndPlay(EndPlayReason);
}

void ABHDefenseMissionDirector::CaptureExistingDefenders()
{
    TrackedEnemies.Reset();
    SpawnTransforms.Reset();
    EnemyClass = nullptr;

    UWorld* World = GetWorld();

    if (!IsValid(World))
    {
        return;
    }

    for (TActorIterator<ABHEnemySoldier> It(World); It; ++It)
    {
        ABHEnemySoldier* Enemy = *It;

        if (!IsValid(Enemy) || Enemy->IsDead())
        {
            continue;
        }

        // A defense mission may only claim unassigned legacy defenders. Authored
        // objective actors and the dormant Defense A garrison have separate
        // operation owners and must retain their own lifecycle contracts.
        if (Enemy->ActorHasTag(
                FName(TEXT("BH_Auto_DefenseA_Garrison"))
            ) ||
            !Enemy->GetObjectiveIdToCompleteOnDeath().IsNone())
        {
            continue;
        }

        if (!EnemyClass)
        {
            EnemyClass = Enemy->GetClass();
        }

        SpawnTransforms.Add(Enemy->GetActorTransform());
        Enemy->SetObjectiveIdToCompleteOnDeath(NAME_None);
        TrackedEnemies.Add(Enemy);
    }

    if (!EnemyClass)
    {
        EnemyClass = ABHEnemySoldier::StaticClass();
    }
}

void ABHDefenseMissionDirector::BeginDefense()
{
    bDefenseActive = true;

    if (HasLivingTrackedEnemies())
    {
        CurrentWave = 1;
        NotifyWave(FText::Format(
            NSLOCTEXT(
                "BrokenHorizon",
                "DefenseWaveActive",
                "DEFENSE ACTIVE // WAVE {0}/{1}\n\n"
                "Hold the sector and eliminate the assault force."
            ),
            FText::AsNumber(CurrentWave),
            FText::AsNumber(TotalWaves)
        ));
    }
    else
    {
        SpawnNextWave();
    }
}

void ABHDefenseMissionDirector::ScheduleNextWave()
{
    const UWorld* World = GetWorld();

    if (!IsValid(World))
    {
        return;
    }

    bWaitingForWave = true;
    NextWaveTime =
        World->GetTimeSeconds() + FMath::Max(0.0f, InterWaveDelay);

    NotifyWave(FText::Format(
        NSLOCTEXT(
            "BrokenHorizon",
            "DefenseWaveIncoming",
            "WAVE {0} CLEARED\n\n"
            "Enemy reinforcements inbound."
        ),
        FText::AsNumber(CurrentWave)
    ));
}

void ABHDefenseMissionDirector::SpawnNextWave()
{
    UWorld* World = GetWorld();

    if (!IsValid(World) || !EnemyClass)
    {
        return;
    }

    bWaitingForWave = false;
    ++CurrentWave;
    TrackedEnemies.RemoveAll(
        [](const TObjectPtr<ABHEnemySoldier>& Enemy)
        {
            return !IsValid(Enemy) || Enemy->IsDead();
        }
    );

    for (int32 EnemyIndex = 0;
        EnemyIndex < ReinforcementsPerWave;
        ++EnemyIndex)
    {
        ABHEnemySoldier* SpawnedEnemy =
            World->SpawnActorDeferred<ABHEnemySoldier>(
                EnemyClass,
                BuildSpawnTransform(EnemyIndex),
                this,
                nullptr,
                ESpawnActorCollisionHandlingMethod::
                    AdjustIfPossibleButAlwaysSpawn
            );

        if (IsValid(SpawnedEnemy))
        {
            SpawnedEnemy->SetCombatantArchetype(
                ABHEnemySoldier::ChooseFormationArchetype(
                    EnemyIndex,
                    ReinforcementsPerWave
                )
            );
            SpawnedEnemy->SetObjectiveIdToCompleteOnDeath(NAME_None);
            UGameplayStatics::FinishSpawningActor(
                SpawnedEnemy,
                BuildSpawnTransform(EnemyIndex)
            );
            TrackedEnemies.Add(SpawnedEnemy);
        }
    }

    NotifyWave(FText::Format(
        NSLOCTEXT(
            "BrokenHorizon",
            "DefenseReinforcementWave",
            "DEFENSE ACTIVE // WAVE {0}/{1}\n\n"
            "Enemy reinforcements have entered the sector."
        ),
        FText::AsNumber(CurrentWave),
        FText::AsNumber(TotalWaves)
    ));
}

void ABHDefenseMissionDirector::CompleteDefense()
{
    if (!HasAuthority())
    {
        return;
    }

    bDefenseComplete = true;
    bDefenseActive = false;
    SetActorTickEnabled(false);

    NotifyWave(NSLOCTEXT(
        "BrokenHorizon",
        "DefenseSectorHeld",
        "SECTOR HELD\n\nAll enemy waves have been repelled."
    ));

    if (IsValid(PlayerCharacter))
    {
        PlayerCharacter->CompleteObjective(
            BHObjectiveIds::EliminateGuard
        );
    }
}

void ABHDefenseMissionDirector::NotifyWave(
    const FText& Message
) const
{
    if (IsValid(PlayerCharacter))
    {
        PlayerCharacter->ShowStatusNotification(Message);
    }
}

bool ABHDefenseMissionDirector::HasLivingTrackedEnemies() const
{
    for (const ABHEnemySoldier* Enemy : TrackedEnemies)
    {
        if (IsValid(Enemy) && !Enemy->IsDead())
        {
            return true;
        }
    }

    return false;
}

FTransform ABHDefenseMissionDirector::BuildSpawnTransform(
    int32 EnemyIndex
) const
{
    if (!SpawnTransforms.IsEmpty())
    {
        FTransform SpawnTransform =
            SpawnTransforms[
                (CurrentWave + EnemyIndex) %
                SpawnTransforms.Num()
            ];
        const float Angle =
            (CurrentWave * 137.5f) + (EnemyIndex * 180.0f);
        const FVector Offset(
            FMath::Cos(FMath::DegreesToRadians(Angle)),
            FMath::Sin(FMath::DegreesToRadians(Angle)),
            0.0f
        );
        SpawnTransform.AddToTranslation(
            Offset * SpawnOffsetRadius
        );
        return SpawnTransform;
    }

    const FVector PlayerLocation = IsValid(PlayerCharacter)
        ? PlayerCharacter->GetActorLocation()
        : GetActorLocation();
    const float Angle =
        (CurrentWave * 120.0f) + (EnemyIndex * 180.0f);
    const FVector Offset(
        FMath::Cos(FMath::DegreesToRadians(Angle)),
        FMath::Sin(FMath::DegreesToRadians(Angle)),
        0.0f
    );

    return FTransform(
        Offset.Rotation(),
        PlayerLocation + (Offset * 1200.0f)
    );
}
