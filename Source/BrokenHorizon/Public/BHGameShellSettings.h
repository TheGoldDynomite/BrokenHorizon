#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "BHGameShellSettings.generated.h"

class USoundClass;
class USoundMix;
class UWorld;

UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Broken Horizon Game Shell"))
class BROKENHORIZON_API UBHGameShellSettings
    : public UDeveloperSettings
{
    GENERATED_BODY()

public:
    UPROPERTY(
        Config,
        EditAnywhere,
        BlueprintReadOnly,
        Category = "Maps",
        meta = (AllowedClasses = "/Script/Engine.World")
    )
    TSoftObjectPtr<UWorld> MainMenuMap;

    UPROPERTY(
        Config,
        EditAnywhere,
        BlueprintReadOnly,
        Category = "Maps",
        meta = (AllowedClasses = "/Script/Engine.World")
    )
    TSoftObjectPtr<UWorld> GameplayMap;

    UPROPERTY(
        Config,
        EditAnywhere,
        BlueprintReadOnly,
        Category = "Maps",
        meta = (AllowedClasses = "/Script/Engine.World")
    )
    TArray<TSoftObjectPtr<UWorld>> AdditionalGameplayMaps;

    UPROPERTY(
        Config,
        EditAnywhere,
        BlueprintReadOnly,
        Category = "Audio"
    )
    TSoftObjectPtr<USoundMix> MasterSoundMix;

    UPROPERTY(
        Config,
        EditAnywhere,
        BlueprintReadOnly,
        Category = "Audio"
    )
    TSoftObjectPtr<USoundClass> MasterSoundClass;
};
