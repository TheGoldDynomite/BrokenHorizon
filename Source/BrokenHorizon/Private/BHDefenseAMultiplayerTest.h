#pragma once

#include "CoreMinimal.h"

#if !UE_BUILD_SHIPPING
class UUserWidget;
class APlayerController;
struct FBHActiveOperationSnapshot;

// Nonshipping, explicitly enabled local multiplayer validation only.
namespace BHDefenseAMultiplayerTest
{
bool IsEnabled();
bool IsHost();
bool HasControl(const FString& FileName);
void Fail(const TCHAR* Reason);
void Finish();
void ObserveHUD(UUserWidget* Widget, bool bVisible, bool bActive, const FText& Text);
void ObservePresentation(APlayerController* Owner, bool bObjectiveVisible, bool bBriefingPresent, bool bNotificationsSuppressed);
void ObserveContinueAcknowledgement(APlayerController* Owner);
void ObserveDebrief(UUserWidget* Widget, bool bMissionComplete, bool bFromDebriefRPC = true);
void ObserveHost(const FBHActiveOperationSnapshot& Snapshot, int32 Authored,
    int32 Active, int32 Runtime, int32 Participants, bool bDormant);
void CompleteHost(const FBHActiveOperationSnapshot& Snapshot, int32 Participants, int32 Completed);
}
#endif
