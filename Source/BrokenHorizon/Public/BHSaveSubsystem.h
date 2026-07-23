#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "BHSaveSubsystem.generated.h"

class ABHCharacter;
class UBHSaveGame;
class UWorld;
class FSubsystemCollectionBase;

UCLASS()
class BROKENHORIZON_API UBHSaveSubsystem
    : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(
        FSubsystemCollectionBase& Collection
    ) override;

    virtual void Deinitialize() override;

    UFUNCTION(BlueprintCallable, Category = "Save")
    bool SaveProgress();

    UFUNCTION(BlueprintCallable, Category = "Save")
    bool LoadProgress();

    UFUNCTION(BlueprintPure, Category = "Save")
    bool HasSaveGame() const;

private:
    static const FString SaveSlotName;
    static constexpr int32 SaveUserIndex = 0;

    ABHCharacter* FindPlayerCharacter(UWorld* World) const;

    bool ValidatePersistenceIDs(UWorld* World) const;

    bool ApplySaveData(
        UBHSaveGame* SaveData,
        UWorld* World
    );

    void HandlePostLoadMap(UWorld* LoadedWorld);

    void ApplyPendingSave(UWorld* LoadedWorld);

    UPROPERTY(Transient)
    TObjectPtr<UBHSaveGame> PendingSaveData;

    FDelegateHandle PostLoadMapHandle;
};
