#pragma once

#include "CoreMinimal.h"
#include "Camera/PlayerCameraManager.h"
#include "BrokenHorizonCameraManager.generated.h"

UCLASS()
class BROKENHORIZON_API ABrokenHorizonCameraManager
    : public APlayerCameraManager
{
    GENERATED_BODY()

public:
    ABrokenHorizonCameraManager();
};