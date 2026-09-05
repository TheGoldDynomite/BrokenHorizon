#pragma once

#include "Engine/GameInstance.h"
#include "BHReplicationTestGameInstance.generated.h"

// A real GameInstance world context without session, input, or subsystem startup.
// This keeps the synchronous network fixture from changing global delegates.
UCLASS(Transient)
class UBHReplicationTestGameInstance : public UGameInstance
{
    GENERATED_BODY()

public:
    virtual void Init() override {}
    virtual void Shutdown() override { WorldContext = nullptr; }
};
