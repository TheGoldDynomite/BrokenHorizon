#include "BHSaveSubsystem.h"

#if !UE_BUILD_SHIPPING
#include "BHCharacter.h"
#include "BHDoor.h"
#include "BHHealthComponent.h"
#include "BHMainMenuWidget.h"
#include "BHObjectiveComponent.h"
#include "BHSaveGame.h"
#include "BHSessionSubsystem.h"
#include "BHWarSubsystem.h"
#include "BHWeaponComponent.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"
#include "UObject/UnrealType.h"

namespace
{
FString ContinueSlotHash(const FString& Slot)
{
    TArray<uint8> Bytes;
    if (!UGameplayStatics::LoadDataFromSlot(Bytes, Slot, 0) || Bytes.IsEmpty()) { return FString(); }
    uint8 Hash[FSHA1::DigestSize];
    FSHA1::HashBuffer(Bytes.GetData(), Bytes.Num(), Hash);
    return BytesToHex(Hash, FSHA1::DigestSize);
}
UBHMainMenuWidget* ContinueMenu(UGameInstance* GI)
{
    if (!IsValid(GI)) { return nullptr; }
    TArray<UUserWidget*> Widgets;
    UWidgetBlueprintLibrary::GetAllWidgetsOfClass(GI, Widgets, UBHMainMenuWidget::StaticClass(), false);
    for (UUserWidget* Widget : Widgets)
    {
        if (IsValid(Widget) && Widget->GetWorld() == GI->GetWorld() && Widget->IsInViewport() && Widget->IsVisible() &&
            Widget->GetOwningPlayer() == GI->GetFirstLocalPlayerController()) { return Cast<UBHMainMenuWidget>(Widget); }
    }
    return nullptr;
}
template<typename T> T* ContinueField(UBHMainMenuWidget* Menu, const TCHAR* Name)
{
    const FObjectPropertyBase* Property = IsValid(Menu) ? FindFProperty<FObjectPropertyBase>(Menu->GetClass(), Name) : nullptr;
    return Property ? Cast<T>(Property->GetObjectPropertyValue_InContainer(Menu)) : nullptr;
}
bool ContinueActionable(UBHMainMenuWidget* Menu)
{
    if (!IsValid(Menu) || !Menu->GetIsEnabled()) { return false; }
    for (const TCHAR* Name : {TEXT("NewGameButton"), TEXT("JoinCampaignButton"), TEXT("ContinueButton")})
    {
        const UButton* Button = ContinueField<UButton>(Menu, Name);
        if (!IsValid(Button) || !Button->GetIsEnabled() || !Button->IsVisible() || !Button->OnClicked.IsBound()) { return false; }
    }
    return true;
}
}

void UBHSaveSubsystem::StartContinueRecoveryTest()
{
    if (!FParse::Param(FCommandLine::Get(), TEXT("BHTestContinueRecovery"))) { return; }
    FString UserDir, Suffix;
    FParse::Value(FCommandLine::Get(), TEXT("BHTestContinueRunId="), ContinueRecoveryRunID);
    FParse::Value(FCommandLine::Get(), TEXT("UserDir="), UserDir);
    FParse::Value(FCommandLine::Get(), TEXT("BHTestSaveSlotSuffix="), Suffix);
    bool bSafe = !ContinueRecoveryRunID.IsEmpty() && ContinueRecoveryRunID.Len() <= 48;
    for (TCHAR C : ContinueRecoveryRunID) { bSafe &= (FChar::IsAlnum(C) && C < 128) || C == TEXT('_') || C == TEXT('-'); }
    FPaths::NormalizeDirectoryName(UserDir);
    const FString Saved = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir());
    bSafe &= !UserDir.IsEmpty() && UserDir.EndsWith(TEXT("/") + ContinueRecoveryRunID + TEXT("/User")) &&
        Suffix == TEXT("ContinueRecovery_") + ContinueRecoveryRunID.Replace(TEXT("-"), TEXT("_")) &&
        FPaths::IsUnderDirectory(Saved, FPaths::ConvertRelativePathToFull(UserDir));
    if (!bSafe) { LogContinueRecoveryTest(TEXT("failure"), TEXT("reason=invalid_isolation")); return; }
    bContinueRecoveryValid = true;
    ContinueRecoveryDirectory = FPaths::Combine(Saved, TEXT("Automation/ContinueRecovery"), ContinueRecoveryRunID);
    IFileManager::Get().MakeDirectory(*ContinueRecoveryDirectory, true);
    ContinueRecoveryDeadline = FPlatformTime::Seconds() + 600.0;
    ContinueRecoveryTicker = FTSTicker::GetCoreTicker().AddTicker(
        FTickerDelegate::CreateUObject(this, &UBHSaveSubsystem::TickContinueRecoveryTest), 0.1f);
}

void UBHSaveSubsystem::LogContinueRecoveryTest(const TCHAR* Phase, const FString& Detail) const
{
    UGameInstance* GI = GetGameInstance();
    UWorld* World = IsValid(GI) ? GI->GetWorld() : nullptr;
    UBHSessionSubsystem* Session = IsValid(GI) ? GI->GetSubsystem<UBHSessionSubsystem>() : nullptr;
    UBHMainMenuWidget* Menu = ContinueMenu(GI);
    const UTextBlock* Status = ContinueField<UTextBlock>(Menu, TEXT("SessionStatusText"));
    FString Text = IsValid(Status) ? Status->GetText().ToString() : FString();
    Text.ReplaceInline(TEXT("\n"), TEXT(" | ")); Text.ReplaceInline(TEXT("\r"), TEXT("")); Text.ReplaceInline(TEXT("\""), TEXT("'"));
    UE_LOG(LogTemp, Display, TEXT("BH_TEST_CONTINUE_RECOVERY run_id=%s phase=%s pid=%u state=%d pending=%d protected=%d actionable=%d net_mode=%d control_dir=\"%s\" primary=\"%s\" backup=\"%s\" status=\"%s\" detail=\"%s\""),
        *ContinueRecoveryRunID, Phase, FPlatformProcess::GetCurrentProcessId(), Session ? static_cast<int32>(Session->GetSessionState()) : -1,
        Session && Session->IsSessionActionPending() ? 1 : 0, bCheckpointWritesProtected ? 1 : 0, ContinueActionable(Menu) ? 1 : 0,
        IsValid(World) ? static_cast<int32>(World->GetNetMode()) : -1, *ContinueRecoveryDirectory,
        *FPaths::Combine(FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()), TEXT("SaveGames"), GetActiveSaveSlotName() + TEXT(".sav")),
        *FPaths::Combine(FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()), TEXT("SaveGames"), GetActiveBackupSaveSlotName() + TEXT(".sav")), *Text, *Detail);
}

bool UBHSaveSubsystem::CheckContinueRecoveryWrites(const TCHAR* Stage)
{
    UWorld* World = GetGameInstance()->GetWorld();
    ABHCharacter* Character = FindPlayerCharacter(World);
    const FName AttemptID(*FString::Printf(TEXT("ContinueAttempt_%s"), *ContinueRecoveryRunID));
    const bool bTrackedBefore = RuntimeConsumedWorldItemIDs.Contains(AttemptID);
    const bool bFull = SaveProgress();
    const bool bCharacter = SaveProgressForCharacter(Character);
    const bool bResource = SavePlayerResources();
    const bool bItem = RecordConsumedWorldItem(AttemptID);
    const bool bCentral = SavePrimaryWithBackup(ContinueRecoveryExpected.Get());
    const bool bUnchanged = ContinueSlotHash(GetActiveSaveSlotName()) == ContinueRecoveryPrimaryHash &&
        ContinueSlotHash(GetActiveBackupSaveSlotName()) == ContinueRecoveryBackupHash &&
        RuntimeConsumedWorldItemIDs.Contains(AttemptID) == bTrackedBefore;
    LogContinueRecoveryTest(Stage, FString::Printf(TEXT("full=%d character=%d resource=%d item=%d central=%d tracking_unchanged=%d hashes_unchanged=%d valid_data=%d live_character=%d"),
        bFull, bCharacter, bResource, bItem, bCentral, RuntimeConsumedWorldItemIDs.Contains(AttemptID) == bTrackedBefore,
        bUnchanged, IsValid(ContinueRecoveryExpected.Get()), IsValid(Character)));
    return !bFull && !bCharacter && !bResource && !bItem && !bCentral && bUnchanged && IsValid(ContinueRecoveryExpected.Get()) && IsValid(Character);
}

bool UBHSaveSubsystem::CheckContinueRecoveryReset()
{
    const bool bMutating = bExecutingLoadMutation && LoadPhase == ELoadProgressPhase::Applying;
    const bool bDeleted = DeleteSaveGame();
    const bool bUnchanged = ContinueSlotHash(GetActiveSaveSlotName()) == ContinueRecoveryPrimaryHash &&
        ContinueSlotHash(GetActiveBackupSaveSlotName()) == ContinueRecoveryBackupHash;
    LogContinueRecoveryTest(TEXT("apply_delete"), FString::Printf(TEXT("delete=%d hashes_unchanged=%d mutation=%d"), bDeleted, bUnchanged, bMutating));
    return bMutating && !bDeleted && bUnchanged;
}

bool UBHSaveSubsystem::PrepareContinueRecoveryNegativeControl()
{
    UBHWarSubsystem* War = GetGameInstance()->GetSubsystem<UBHWarSubsystem>();
    if (!bCheckpointWritesProtected || !IsValid(War) || !IsValid(ContinueRecoveryExpected.Get()) || ContinueRecoveryExpected->WarSectorStates.IsEmpty()) { return false; }
    const FName SectorID = ContinueRecoveryExpected->WarSectorStates[0].SectorID;
    TArray<FBHWarSectorState> Sectors = War->GetSectorStates();
    for (FBHWarSectorState& Sector : Sectors) { if (Sector.SectorID == SectorID) { Sector.Supply = 11.0f; } }
    if (!War->RestoreWarState(Sectors, War->GetSupplyConvoys(), War->GetRecentWarEvents(), 9, 0.0f, BHSave::CurrentSchemaVersion)) { return false; }
    const FName ConsumedID(*FString::Printf(TEXT("ContinueConsumed_%s"), *ContinueRecoveryRunID));
    RuntimeConsumedWorldItemIDs.Remove(ConsumedID);
    const bool bDiffers = War->GetTurnNumber() != ContinueRecoveryExpected->WarTurnNumber &&
        !FMath::IsNearlyEqual(War->GetSectorState(SectorID).Supply, ContinueRecoveryExpected->WarSectorStates[0].Supply) &&
        !RuntimeConsumedWorldItemIDs.Contains(ConsumedID) && ContinueRecoveryExpected->ConsumedWorldItemIDs.Contains(ConsumedID);
    LogContinueRecoveryTest(ContinueRecoveryPhase == 2 ? TEXT("negative_before_failure") : TEXT("negative_before_retry"),
        FString::Printf(TEXT("turn=%d supply=%.1f consumed=0 checkpoint_turn=%d checkpoint_supply=%.1f tracking_differs=%d"),
            War->GetTurnNumber(), War->GetSectorState(SectorID).Supply, ContinueRecoveryExpected->WarTurnNumber,
            ContinueRecoveryExpected->WarSectorStates[0].Supply, bDiffers));
    return bDiffers;
}

void UBHSaveSubsystem::InstallContinueRecoveryFault(UWorld* World)
{
    if (!bContinueRecoveryValid || ContinueRecoveryPhase != 2 || bContinueRecoveryFaultInstalled) { return; }
    for (TActorIterator<ABHDoor> It(World); It; ++It)
    {
        ABHDoor* Original = *It;
        if (!IsValid(Original) || Original->GetPersistenceID().IsNone()) { continue; }
        FActorSpawnParameters Params;
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        ABHDoor* Duplicate = World->SpawnActor<ABHDoor>(ABHDoor::StaticClass(), FVector(0, 0, -100000), FRotator::ZeroRotator, Params);
        FNameProperty* Property = FindFProperty<FNameProperty>(ABHDoor::StaticClass(), TEXT("PersistenceID"));
        if (IsValid(Duplicate) && Property)
        {
            Property->SetPropertyValue_InContainer(Duplicate, Original->GetPersistenceID());
            bContinueRecoveryFaultInstalled = Duplicate->GetPersistenceID() == Original->GetPersistenceID();
            LogContinueRecoveryTest(TEXT("fault_installed"), TEXT("reason=duplicate_door_persistence_id"));
        }
        return;
    }
}

bool UBHSaveSubsystem::TickContinueRecoveryTest(float DeltaTime)
{
    const auto Fail = [this](const TCHAR* Reason) { LogContinueRecoveryTest(TEXT("failure"), Reason); ContinueRecoveryTicker.Reset(); return false; };
    if (FPlatformTime::Seconds() > ContinueRecoveryDeadline) { return Fail(TEXT("reason=timeout")); }
    UGameInstance* GI = GetGameInstance();
    UWorld* World = IsValid(GI) ? GI->GetWorld() : nullptr;
    UBHSessionSubsystem* Session = IsValid(GI) ? GI->GetSubsystem<UBHSessionSubsystem>() : nullptr;
    UBHWarSubsystem* War = IsValid(GI) ? GI->GetSubsystem<UBHWarSubsystem>() : nullptr;
    if (!IsValid(World) || !World->HasBegunPlay() || !IsValid(Session) || !IsValid(War)) { return true; }
    ABHCharacter* Character = FindPlayerCharacter(World);
    UBHMainMenuWidget* Menu = ContinueMenu(GI);
    const auto Signal = [this](const TCHAR* Name) { return IFileManager::Get().FileExists(*FPaths::Combine(ContinueRecoveryDirectory, Name)); };
    if (ContinueRecoveryPhase == 0)
    {
        if (!IsValid(Character) || !IsValid(Character->GetMissionData()) || World->GetNetMode() != NM_ListenServer) { return true; }
        UBHObjectiveComponent* Objective = Character->FindComponentByClass<UBHObjectiveComponent>();
        UBHHealthComponent* Health = Character->GetHealthComponent();
        UBHWeaponComponent* Weapon = Character->GetWeaponComponent();
        if (!IsValid(Objective) || !IsValid(Health) || !IsValid(Weapon)) { return true; }
        Health->RestorePersistentHealthState(73.0f);
        Weapon->RestoreAmmoState(17, 91);
        Character->AddKeycard(TEXT("Red"));
        if (!Objective->CompleteObjectiveByID(TEXT("FindRedKeycard"))) { return Fail(TEXT("reason=seed_objective")); }
        TArray<FBHWarSectorState> Sectors = War->GetSectorStates();
        if (Sectors.IsEmpty()) { return Fail(TEXT("reason=seed_war")); }
        Sectors[0].Supply = 63.0f;
        if (!War->RestoreWarState(Sectors, War->GetSupplyConvoys(), War->GetRecentWarEvents(), 37, 0.0f, BHSave::CurrentSchemaVersion)) { return Fail(TEXT("reason=seed_war_restore")); }
        if (!RecordConsumedWorldItem(FName(*FString::Printf(TEXT("ContinueConsumed_%s"), *ContinueRecoveryRunID))) || !SaveProgress() || !SaveProgress()) { return Fail(TEXT("reason=seed_save")); }
        ContinueRecoveryExpected.Reset(LoadBestSaveGame());
        ContinueRecoveryPrimaryHash = ContinueSlotHash(GetActiveSaveSlotName());
        ContinueRecoveryBackupHash = ContinueSlotHash(GetActiveBackupSaveSlotName());
        if (!IsValid(ContinueRecoveryExpected.Get()) || ContinueRecoveryPrimaryHash.IsEmpty() || ContinueRecoveryBackupHash.IsEmpty()) { return Fail(TEXT("reason=seed_slots")); }
        LogContinueRecoveryTest(TEXT("seeded"), TEXT("health=73 magazine=17 reserve=91 turn=37"));
        ContinueRecoveryPhase = 1;
        if (!Session->LeaveSession()) { return Fail(TEXT("reason=seed_leave")); }
    }
    else if (ContinueRecoveryPhase == 1 && ContinueActionable(Menu) && !Session->IsSessionActionPending() && Signal(TEXT("fail.ready")))
    {
        ContinueRecoveryPhase = 2;
        LogContinueRecoveryTest(TEXT("continue_requested"));
        ContinueField<UButton>(Menu, TEXT("ContinueButton"))->OnClicked.Broadcast();
    }
    else if (ContinueRecoveryPhase == 2 && bContinueRecoveryApplyFailed && Session->GetSessionState() == EBHSessionState::Error && ContinueActionable(Menu))
    {
        if (!bContinueRecoveryFaultInstalled || !bCheckpointWritesProtected) { return Fail(TEXT("reason=failed_protection")); }
        LogContinueRecoveryTest(TEXT("failed_menu"));
        ContinueRecoveryPhase = 3;
    }
    else if (ContinueRecoveryPhase == 3 && ContinueActionable(Menu) && Signal(TEXT("retry.ready")))
    {
        ContinueRecoveryPhase = 4;
        LogContinueRecoveryTest(TEXT("retry_requested"));
        ContinueField<UButton>(Menu, TEXT("ContinueButton"))->OnClicked.Broadcast();
    }
    else if (ContinueRecoveryPhase == 4 && bContinueRecoveryApplied && Session->GetSessionState() == EBHSessionState::InSession)
    {
        if (!IsValid(Character) || !IsValid(ContinueRecoveryExpected.Get())) { return Fail(TEXT("reason=missing_restored_player")); }
        const UBHObjectiveComponent* Objective = Character->FindComponentByClass<UBHObjectiveComponent>();
        const UBHWeaponComponent* Weapon = Character->GetWeaponComponent();
        const UBHHealthComponent* Health = Character->GetHealthComponent();
        const UBHSaveGame* Expected = ContinueRecoveryExpected.Get();
        const bool bState = World->GetNetMode() == NM_ListenServer && !bCheckpointWritesProtected && IsValid(Objective) && IsValid(Health) && IsValid(Weapon) &&
            Objective->GetCurrentObjectiveID() == Expected->CurrentObjectiveID && Objective->GetCompletedObjectiveIDs() == Expected->CompletedObjectiveIDs &&
            FMath::IsNearlyEqual(Health->GetCurrentHealth(), Expected->SavedHealth, 0.1f) && Weapon->GetMagazineAmmo() == Expected->SavedMagazineAmmo &&
            Weapon->GetReserveAmmo() == Expected->SavedReserveAmmo && Character->HasKeycard(TEXT("Red")) && War->GetTurnNumber() == Expected->WarTurnNumber &&
            Expected->WarSectorStates.Num() > 0 && FMath::IsNearlyEqual(War->GetSectorState(Expected->WarSectorStates[0].SectorID).Supply, Expected->WarSectorStates[0].Supply, 0.1f) &&
            IsWorldItemConsumed(FName(*FString::Printf(TEXT("ContinueConsumed_%s"), *ContinueRecoveryRunID)));
        if (!bState) { return Fail(TEXT("reason=restored_state_mismatch")); }
        LogContinueRecoveryTest(TEXT("success"), FString::Printf(TEXT("health=%.1f magazine=%d reserve=%d turn=%d objective=%s consumed=1"), Health->GetCurrentHealth(), Weapon->GetMagazineAmmo(), Weapon->GetReserveAmmo(), War->GetTurnNumber(), *Objective->GetCurrentObjectiveID().ToString()));
        ContinueRecoveryTicker.Reset(); return false;
    }
    return true;
}
#endif
