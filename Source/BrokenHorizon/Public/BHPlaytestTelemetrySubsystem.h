#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "BHPlaytestTelemetrySubsystem.generated.h"

/**
 * Opt-in, local-only playtest telemetry for non-shipping builds.
 *
 * Events are written as bounded JSON lines under Saved/Telemetry. The
 * subsystem never performs network IO and callers must use stable gameplay
 * labels rather than player names or free-form user text.
 */
UCLASS()
class BROKENHORIZON_API UBHPlaytestTelemetrySubsystem
    : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    UFUNCTION(BlueprintPure, Category = "Playtest Telemetry")
    bool IsTelemetryEnabled() const;

    void RecordEvent(
        FName EventName,
        const TMap<FString, FString>& Fields = {}
    );

    static FString SanitizeToken(const FString& Value, int32 MaxLength = 64);
    static FString BuildEventJsonForTesting(
        FName EventName,
        const TMap<FString, FString>& Fields
    );

private:
    FString BuildEventJson(
        FName EventName,
        const TMap<FString, FString>& Fields
    ) const;

    bool bEnabled = false;
    FString SessionId;
    FString OutputPath;
    double SessionStartSeconds = 0.0;
};
