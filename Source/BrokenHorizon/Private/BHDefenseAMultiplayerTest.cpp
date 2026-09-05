#include "BHDefenseAMultiplayerTest.h"

#if !UE_BUILD_SHIPPING
#include "BHWarGameState.h"
#include "BHCharacter.h"
#include "BHMissionCompleteWidget.h"
#include "Blueprint/UserWidget.h"
#include "Components/TextBlock.h"
#include "Containers/Ticker.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformTime.h"
#include "HighResScreenshot.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "String/LexFromString.h"

namespace BHDefenseAMultiplayerTest
{
namespace
{
struct FContinueContext
{
    TWeakObjectPtr<UBHMissionCompleteWidget> Widget;
    TWeakObjectPtr<ABHCharacter> Character;
    TWeakObjectPtr<APlayerController> Owner;
    TWeakObjectPtr<UWorld> World;
    FName OperationID;
    bool bTerminalAgreement = false;
    bool bRequested = false;
    bool bAcknowledged = false;
    bool bHasPresentationObservation = false;
    bool bObjectiveVisible = true;
    bool bNotificationsSuppressed = true;
};
FContinueContext ContinueContext;
bool TickContinue();

struct FOptions
{
    bool bEnabled = false;
    bool bCapture = false;
    FString RunID;
    FString Role;
    FString Root;
    FTSTicker::FDelegateHandle Watchdog;
};
FOptions& Options()
{
    static FOptions Value = []
    {
        FOptions Result;
        const TCHAR* CommandLine = FCommandLine::Get();
        if (!FParse::Param(CommandLine, TEXT("BHTestDefenseAMultiplayer")))
        {
            return Result;
        }
        FParse::Value(CommandLine, TEXT("BHTestDefenseARunId="), Result.RunID);
        FParse::Value(CommandLine, TEXT("BHTestDefenseARole="), Result.Role);
        FString TimeoutText(TEXT("900"));
        FParse::Value(CommandLine, TEXT("BHTestDefenseATimeout="), TimeoutText);
        int32 Timeout = 0;
        bool bValid = !Result.RunID.IsEmpty() && Result.RunID.Len() <= 64;
        for (TCHAR Character : Result.RunID)
        {
            bValid &= (Character >= TEXT('a') && Character <= TEXT('z')) ||
                (Character >= TEXT('A') && Character <= TEXT('Z')) ||
                (Character >= TEXT('0') && Character <= TEXT('9')) ||
                Character == TEXT('_') || Character == TEXT('-');
        }
        bValid &= Result.Role == TEXT("Host") || Result.Role == TEXT("ClientA") || Result.Role == TEXT("ClientB");
        bValid &= !TimeoutText.IsEmpty() && TimeoutText.Len() <= 4;
        for (TCHAR Character : TimeoutText)
        {
            bValid &= Character >= TEXT('0') && Character <= TEXT('9');
        }
        if (!bValid || !LexTryParseString(Timeout, *TimeoutText) || Timeout < 120 || Timeout > 1800)
        {
            UE_LOG(LogTemp, Error, TEXT("BH_TEST_DEFENSE_A_MULTIPLAYER run_id=invalid role=invalid result=failure reason=invalid_arguments"));
            return Result;
        }
        Result.bEnabled = true;
        Result.bCapture = FParse::Param(CommandLine, TEXT("BHTestDefenseACapture"));
        // ProjectSavedDir can follow -UserDir; these harness controls must remain shared.
        Result.Root = FPaths::ConvertRelativePathToFull(FPaths::Combine(
            FPaths::ProjectDir(), TEXT("Saved/Automation/DefenseAMultiplayer"), Result.RunID));
        const double Deadline = FPlatformTime::Seconds() + Timeout;
        const FString RunID = Result.RunID;
        const FString Role = Result.Role;
        Result.Watchdog = FTSTicker::GetCoreTicker().AddTicker(
            FTickerDelegate::CreateLambda([Deadline, RunID, Role](float)
            {
                if (!TickContinue()) { return false; }
                if (FPlatformTime::Seconds() < Deadline)
                {
                    return true;
                }
                UE_LOG(LogTemp, Error, TEXT("BH_TEST_DEFENSE_A_MULTIPLAYER run_id=%s role=%s result=failure reason=total_timeout"),
                    *RunID, *Role);
                return false;
            }), 0.5f);
        return Result;
    }();
    return Value;
}
FString NormalizeText(const FText& Text)
{
    TArray<FString> Words;
    Text.ToString().Replace(TEXT("\""), TEXT("'")).ParseIntoArrayWS(Words);
    return Words.IsEmpty() ? FString(TEXT("none")) : FString::Join(Words, TEXT("_"));
}
FString Stage(const FBHActiveOperationSnapshot& Snapshot)
{
    switch (Snapshot.Phase)
    {
    case EBHActiveOperationPhase::Combat:
        return FString::Printf(TEXT("combat_wave%d"), Snapshot.OperationState.CurrentWave);
    case EBHActiveOperationPhase::AwaitingWave: return TEXT("awaiting_wave");
    case EBHActiveOperationPhase::Securing: return TEXT("securing");
    case EBHActiveOperationPhase::DebriefSuccess: return TEXT("debrief");
    default: return TEXT("other");
    }
}
struct FCapture
{
    FString Stage;
    FString Path;
    double Deadline = 0.0;
    bool bRequested = false;
};
TArray<FCapture> CaptureQueue;
TSet<FString> QueuedStages;
TSet<FName> TerminalHUDOperations;
TSet<FName> DebriefOperations;
FTSTicker::FDelegateHandle CaptureTicker;
void QueueCapture(const FString& CaptureStage, FName OperationID)
{
    const FOptions& Config = Options();
    const bool bAllowedStage = CaptureStage == TEXT("combat_wave1") || CaptureStage == TEXT("combat_wave2") ||
        CaptureStage == TEXT("awaiting_wave") || CaptureStage == TEXT("securing") || CaptureStage == TEXT("debrief");
    if (!Config.bCapture || !bAllowedStage || OperationID.IsNone())
    {
        return;
    }
    const FString Key = OperationID.ToString() + TEXT(":") + CaptureStage;
    if (QueuedStages.Contains(Key))
    {
        return;
    }
    QueuedStages.Add(Key);
    const FString Path = FPaths::Combine(Config.Root, Config.Role, CaptureStage + TEXT(".png"));
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), true);
    CaptureQueue.Add({CaptureStage, Path});
    if (CaptureTicker.IsValid())
    {
        return;
    }
    // Only value data is retained. Each request is serialized and bounded.
    const FString RunID = Config.RunID;
    const FString Role = Config.Role;
    CaptureTicker = FTSTicker::GetCoreTicker().AddTicker(
        FTickerDelegate::CreateLambda([RunID, Role](float)
        {
            if (CaptureQueue.IsEmpty())
            {
                CaptureTicker.Reset();
                return false;
            }
            FCapture& Request = CaptureQueue[0];
            if (Request.Deadline == 0.0)
            {
                Request.Deadline = FPlatformTime::Seconds() + 20.0;
            }
            if (FPlatformTime::Seconds() >= Request.Deadline)
            {
                Fail(TEXT("capture_timeout"));
                CaptureQueue.Reset();
                CaptureTicker.Reset();
                return false;
            }
            if (!Request.bRequested && !FScreenshotRequest::IsScreenshotRequested())
            {
                FScreenshotRequest::RequestScreenshot(Request.Path, true, false);
                Request.bRequested = true;
                UE_LOG(LogTemp, Display, TEXT("BH_TEST_DEFENSE_A_CAPTURE run_id=%s role=%s stage=%s result=requested path=%s"),
                    *RunID, *Role, *Request.Stage, *Request.Path);
            }
            else if (Request.bRequested && IFileManager::Get().FileSize(*Request.Path) > 0)
            {
                UE_LOG(LogTemp, Display, TEXT("BH_TEST_DEFENSE_A_CAPTURE run_id=%s role=%s stage=%s result=written path=%s"),
                    *RunID, *Role, *Request.Stage, *Request.Path);
                CaptureQueue.RemoveAt(0);
            }
            return true;
        }), 0.1f);
}
void TryCaptureDebrief(FName OperationID)
{
    if (TerminalHUDOperations.Contains(OperationID) && DebriefOperations.Contains(OperationID))
    {
        QueueCapture(TEXT("debrief"), OperationID);
        ContinueContext.bTerminalAgreement = true;
    }
}
bool TickContinue()
{
    if (!ContinueContext.bTerminalAgreement) { return true; }
    ABHCharacter* Character = ContinueContext.Character.Get();
    APlayerController* Owner = ContinueContext.Owner.Get();
    if (!IsValid(Character) || !IsValid(Owner) || !ContinueContext.World.IsValid() ||
        Character->GetWorld() != ContinueContext.World.Get() || Owner->GetPawn() != Character || !Owner->IsLocalController())
    {
        Fail(TEXT("continue_context_changed")); return false;
    }
    if (!ContinueContext.bRequested)
    {
        if (!HasControl(TEXT("continue.ready"))) { return true; }
        UBHMissionCompleteWidget* Widget = ContinueContext.Widget.Get();
        if (!IsValid(Widget) || !Widget->IsInViewport()) { Fail(TEXT("continue_widget_missing")); return false; }
        ContinueContext.bRequested = true;
        UE_LOG(LogTemp, Display, TEXT("BH_TEST_DEFENSE_A_CONTINUE run_id=%s role=%s result=requested operation_id=%s"),
            *Options().RunID, *Options().Role, *ContinueContext.OperationID.ToString());
        Widget->OnContinueRequested.Broadcast();
        return true;
    }
    const ABHWarGameState* State = Character->GetWorld()->GetGameState<ABHWarGameState>();
    const bool bDebriefAbsent = !ContinueContext.Widget.IsValid() || !ContinueContext.Widget->IsInViewport();
    if (ContinueContext.bAcknowledged && Character->IsWarMapOpen() && Character->GetCurrentObjectiveID().IsNone() &&
        ContinueContext.bHasPresentationObservation && !ContinueContext.bObjectiveVisible && !ContinueContext.bNotificationsSuppressed &&
        bDebriefAbsent && IsValid(State) && State->GetActiveOperationSnapshot().Phase == EBHActiveOperationPhase::None)
    {
        UE_LOG(LogTemp, Display, TEXT("BH_TEST_DEFENSE_A_CONTINUE run_id=%s role=%s result=success operation_id=%s acknowledged=1 war_map_open=1 current_objective=None objective_visible=0 notifications_suppressed=0 debrief_viewport=0 snapshot_phase=0"),
            *Options().RunID, *Options().Role, *ContinueContext.OperationID.ToString());
        Options().Watchdog.Reset();
        return false;
    }
    return true;
}

bool IsLocalClientWidget(UUserWidget* Widget)
{
    const APlayerController* Owner = IsValid(Widget) ? Widget->GetOwningPlayer() : nullptr;
    return IsEnabled() && !IsHost() && IsValid(Owner) && Owner->IsLocalController() &&
        Owner->GetNetMode() == NM_Client && Widget->IsInViewport();
}
}
bool IsEnabled() { return Options().bEnabled; }
bool IsHost() { return IsEnabled() && Options().Role == TEXT("Host"); }
bool HasControl(const FString& FileName)
{
    return IsEnabled() && IFileManager::Get().FileExists(*FPaths::Combine(Options().Root, FileName));
}
void Finish()
{
    FTSTicker::GetCoreTicker().RemoveTicker(Options().Watchdog);
    Options().Watchdog.Reset();
}
void Fail(const TCHAR* Reason)
{
    UE_LOG(LogTemp, Error, TEXT("BH_TEST_DEFENSE_A_MULTIPLAYER run_id=%s role=%s result=failure reason=%s"),
        *Options().RunID, *Options().Role, Reason);
    Finish();
}
void ObservePresentation(APlayerController* Owner, bool bObjectiveVisible, bool bBriefingPresent, bool bNotificationsSuppressed)
{
    if (!IsEnabled() || IsHost() || !IsValid(Owner) || !Owner->IsLocalController() || Owner->GetNetMode() != NM_Client) { return; }
    const ABHWarGameState* State = Owner->GetWorld()->GetGameState<ABHWarGameState>();
    if (!IsValid(State)) { return; }
    const FBHActiveOperationSnapshot Snapshot = State->GetActiveOperationSnapshot();
    if (ContinueContext.Owner == Owner)
    {
        ContinueContext.bHasPresentationObservation = true;
        ContinueContext.bObjectiveVisible = bObjectiveVisible;
        ContinueContext.bNotificationsSuppressed = bNotificationsSuppressed;
    }
    const FString Key = FString::Printf(TEXT("%s:%d:%d:%d:%d"), *Snapshot.OperationID.ToString(),
        static_cast<int32>(Snapshot.Phase), bObjectiveVisible, bBriefingPresent, bNotificationsSuppressed);
    static FString LastKey;
    if (LastKey == Key) { return; }
    LastKey = Key;
    UE_LOG(LogTemp, Display, TEXT("BH_TEST_DEFENSE_A_PRESENTATION run_id=%s role=%s result=observed operation_id=%s revision=%d phase=%d objective_visible=%d briefing_present=%d notifications_suppressed=%d"),
        *Options().RunID, *Options().Role, *Snapshot.OperationID.ToString(), Snapshot.Revision, static_cast<int32>(Snapshot.Phase),
        bObjectiveVisible, bBriefingPresent, bNotificationsSuppressed);
}

void ObserveContinueAcknowledgement(APlayerController* Owner)
{
    if (!IsEnabled() || IsHost() || !ContinueContext.bRequested || ContinueContext.Owner != Owner || ContinueContext.bAcknowledged) { return; }
    ContinueContext.bAcknowledged = true;
    UE_LOG(LogTemp, Display, TEXT("BH_TEST_DEFENSE_A_CONTINUE run_id=%s role=%s result=acknowledged operation_id=%s"),
        *Options().RunID, *Options().Role, *ContinueContext.OperationID.ToString());
}

void ObserveHUD(UUserWidget* Widget, bool bVisible, bool bActive, const FText& Text)
{
    if (!IsLocalClientWidget(Widget)) { return; }
    const FOptions& Config = Options();
    static bool bReadyLogged = false;
    if (!bReadyLogged)
    {
        bReadyLogged = true;
        UE_LOG(LogTemp, Display, TEXT("BH_TEST_DEFENSE_A_HUD_READY run_id=%s role=%s result=success"), *Config.RunID, *Config.Role);
    }
    const ABHWarGameState* State = Widget->GetWorld()->GetGameState<ABHWarGameState>();
    if (!IsValid(State)) { return; }
    const FBHActiveOperationSnapshot Snapshot = State->GetActiveOperationSnapshot();
    if (Snapshot.OperationID.IsNone()) { return; }
    const FBHOpenWorldOperationState& Operation = Snapshot.OperationState;
    const FString Normalized = NormalizeText(Text);
    const FString Key = FString::Printf(TEXT("%s:%d:%d:%d:%d:%d:%d:%s"), *Snapshot.OperationID.ToString(),
        static_cast<int32>(Snapshot.Phase), Operation.CurrentWave, Operation.LivingEnemyCount,
        Operation.EnemyCasualties, bVisible, bActive, *Normalized);
    static FString LastKey;
    if (LastKey == Key) { return; }
    LastKey = Key;
    UE_LOG(LogTemp, Display, TEXT("BH_TEST_DEFENSE_A_HUD run_id=%s role=%s result=observed operation_id=%s revision=%d phase=%d wave=%d total=%d hostiles=%d defeated=%d visible=%d active=%d text=%s"),
        *Config.RunID, *Config.Role, *Snapshot.OperationID.ToString(), Snapshot.Revision, static_cast<int32>(Snapshot.Phase),
        Operation.CurrentWave, Operation.DefenseWaveCount, Operation.LivingEnemyCount, Operation.EnemyCasualties, bVisible, bActive, *Normalized);
    if (Snapshot.Phase == EBHActiveOperationPhase::DebriefSuccess && !bVisible && !bActive)
    {
        TerminalHUDOperations.Add(Snapshot.OperationID);
        TryCaptureDebrief(Snapshot.OperationID);
    }
    else if (bVisible && bActive)
    {
        QueueCapture(Stage(Snapshot), Snapshot.OperationID);
    }
}
void ObserveDebrief(UUserWidget* Widget, bool bMissionComplete, bool bFromDebriefRPC)
{
    if (!IsLocalClientWidget(Widget)) { return; }
    struct FPendingDebrief
    {
        TWeakObjectPtr<UWorld> World;
        TWeakObjectPtr<APlayerController> Owner;
        TWeakObjectPtr<APawn> Pawn;
        FName OperationID;
    };
    static TMap<TWeakObjectPtr<UUserWidget>, FPendingDebrief> PendingDebriefs;
    const ABHWarGameState* State = Widget->GetWorld()->GetGameState<ABHWarGameState>();
    if (!IsValid(State)) { return; }
    const FName OperationID = State->GetActiveOperationSnapshot().OperationID;
    if (OperationID.IsNone()) { return; }
    if (DebriefOperations.Contains(OperationID)) { return; }
    APlayerController* Owner = Widget->GetOwningPlayer();
    if (bFromDebriefRPC)
    {
        PendingDebriefs.Add(Widget, {Widget->GetWorld(), Owner, Owner->GetPawn(), OperationID});
    }
    const FPendingDebrief* Pending = PendingDebriefs.Find(Widget);
    if (!Pending) { return; }
    if (Pending->World != Widget->GetWorld() || Pending->Owner != Owner ||
        Pending->Pawn != Owner->GetPawn() || Pending->OperationID != OperationID)
    {
        Fail(TEXT("debrief_context_changed")); return;
    }
    // The reliable RPC and objective replication may arrive in either order.
    // A replication callback can retry only the actual RPC-populated widget.
    if (!bMissionComplete) { return; }
    const UTextBlock* Text = Cast<UTextBlock>(Widget->GetWidgetFromName(TEXT("MissionCompleteText")));
    if (!IsValid(Text) || Text->GetText().IsEmpty())
    {
        Fail(TEXT("debrief_content_missing")); return;
    }
    const FOptions& Config = Options();
    UE_LOG(LogTemp, Display, TEXT("BH_TEST_DEFENSE_A_DEBRIEF run_id=%s role=%s result=observed operation_id=%s viewport=1 mission_complete=%d text=%s"),
        *Config.RunID, *Config.Role, *OperationID.ToString(), bMissionComplete, *NormalizeText(Text->GetText()));
    if (bMissionComplete)
    {
        ContinueContext.Widget = Cast<UBHMissionCompleteWidget>(Widget);
        ContinueContext.Character = Cast<ABHCharacter>(Owner->GetPawn());
        ContinueContext.Owner = Owner;
        ContinueContext.World = Widget->GetWorld();
        ContinueContext.OperationID = OperationID;
        DebriefOperations.Add(OperationID);
        TryCaptureDebrief(OperationID);
    }
}
void ObserveHost(const FBHActiveOperationSnapshot& Snapshot, int32 Authored,
    int32 Active, int32 Runtime, int32 Participants, bool bDormant)
{
    const FOptions& Config = Options();
    const auto& Operation = Snapshot.OperationState;
    const FString Step = bDormant ? TEXT("dormant") : Stage(Snapshot);
    const FString Key = FString::Printf(TEXT("%s:%s:%d:%d:%d:%d:%d"), *Snapshot.OperationID.ToString(), *Step,
        Operation.CurrentWave, Operation.LivingEnemyCount, Operation.EnemyCasualties, Active, Runtime);
    static FString LastKey;
    if (LastKey == Key) { return; }
    LastKey = Key;
    UE_LOG(LogTemp, Display, TEXT("BH_TEST_DEFENSE_A_HOST run_id=%s role=Host step=%s result=observed operation_id=%s revision=%d phase=%d wave=%d total=%d hostiles=%d defeated=%d authored=%d active=%d runtime=%d participants=%d"),
        *Config.RunID, *Step, *Snapshot.OperationID.ToString(), Snapshot.Revision, static_cast<int32>(Snapshot.Phase),
        Operation.CurrentWave, Operation.DefenseWaveCount, Operation.LivingEnemyCount, Operation.EnemyCasualties, Authored, Active, Runtime, Participants);
}
void CompleteHost(const FBHActiveOperationSnapshot& Snapshot, int32 Participants, int32 Completed)
{
    const auto& Operation = Snapshot.OperationState;
    UE_LOG(LogTemp, Display, TEXT("BH_TEST_DEFENSE_A_MULTIPLAYER run_id=%s role=Host result=success operation_id=%s revision=%d phase=%d participants=%d completed=%d wave=%d total=%d defeated=%d"),
        *Options().RunID, *Snapshot.OperationID.ToString(), Snapshot.Revision, static_cast<int32>(Snapshot.Phase),
        Participants, Completed, Operation.CurrentWave, Operation.DefenseWaveCount, Operation.EnemyCasualties);
    Finish();
}
}
#endif
