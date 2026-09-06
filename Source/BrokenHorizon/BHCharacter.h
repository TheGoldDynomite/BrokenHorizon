#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineBaseTypes.h"
#include "BHMissionData.h"
#include "BHObjectiveNotificationWidget.h"
#include "BHLoadoutWeight.h"
#include "BHWarTypes.h"
#include "GameFramework/Character.h"
#include "InputActionValue.h"
#include "TimerManager.h"
#include "BHCharacter.generated.h"

class UCameraComponent;
class USpotLightComponent;
class UNavigationInvokerComponent;
class UWorldPartitionStreamingSourceComponent;
class UInputAction;
class UInputMappingContext;
class UInputComponent; 
class UBHInteractionPromptWidget;
class UBHObjectiveWidget;
class UBHObjectiveComponent;
class UBHMissionData;
class UBHHealthComponent;
class UBHDeathWidget;
class UBHWeaponComponent;
enum class EBHWeaponRole : uint8;
enum class EBHFireMode : uint8;
class UBHAmmoHUDWidget;
struct FBHCarryLoadProfile;
class UBHHitMarkerWidget;
class UBHMissionCompleteWidget;
class UBHPauseMenuWidget;
class UBHWarMapWidget;
class UBHWarSubsystem;
class UBHCombatStatusWidget;
class UBHInventoryWidget;
enum class EBHSalvagePickupType : uint8;
class UBHInjuryComponent;
class UBHUserSettingsSubsystem;
class UBHSubtitleWidget;
class ABHDefenseMissionDirector;
class ABHAmbientWarDirector;
class ABHWarGameState;
class ABHOpenWorldOperationDirector;
class ABHSectorResupplyStation;
class ABHFieldTransport;
class ABHEnemySoldier;
class ABHFragGrenade;
class ABHSmokeGrenade;
class ABHEngineeringCharge;
class ABHAntiVehicleProjectile;
enum class EBHEngineeringChargeMode : uint8;
class UAnimSequenceBase;
class USkeletalMeshComponent;
class USoundBase;
struct FHitResult;
struct FBHActiveOperationSnapshot;
enum class EBHPlayerHitZone : uint8;

UENUM(BlueprintType)
enum class EBHFieldSquadContextAction : uint8
{
    None,
    CasualtyAid,
    Sabotage,
    Secure,
    Defend
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
    FBHOnStaminaChanged,
    float,
    CurrentStamina,
    float,
    MaxStamina
);

USTRUCT(BlueprintType)
struct BROKENHORIZON_API FBHInventorySnapshot
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory")
    FName ActiveWeaponRole = NAME_None;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory")
    int32 MagazineRounds = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory")
    int32 ReserveRounds = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory")
    int32 MaximumReserveRounds = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory")
    int32 FragGrenades = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory")
    int32 SmokeGrenades = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory")
    int32 EngineeringCharges = 0;
    int32 AntiVehicleRounds = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory")
    int32 Medkits = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory")
    int32 FieldDressings = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory")
    TArray<FName> MissionItemIDs;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory")
    int32 MissionItemCount = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory")
    float HelmetDurabilityFraction = 1.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory")
    float BodyArmorDurabilityFraction = 1.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory")
    float CarriedWeightKilograms = 0.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory")
    float ContainerCapacityKilograms = 40.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory")
    float ContainerRemainingKilograms = 40.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory")
    EBHCarryLoadState CarryLoadState = EBHCarryLoadState::FightingLoad;
};

UCLASS()
class BROKENHORIZON_API ABHCharacter : public ACharacter
{
    GENERATED_BODY()

    friend class UBHWeaponComponent;

public:
    ABHCharacter();

    virtual void GetLifetimeReplicatedProps(
        TArray<FLifetimeProperty>& OutLifetimeProps
    ) const override;

#if !UE_BUILD_SHIPPING
    bool ConfigureFieldSquadContextReplicationTest(
        EBHFieldSquadContextAction Action,
        FName TargetLabel,
        bool bReachedTarget
    );

    bool PrepareFieldSquadCasualtyForTransportTest();
#endif


    UFUNCTION(BlueprintCallable, Category = "Inventory")
    void AddKeycard(FName KeycardID);

    bool RemoveKeycard(FName KeycardID);

    bool CollectKeycard(
        FName KeycardID,
        FName PickupPersistenceID
    );

    UFUNCTION(BlueprintPure, Category = "Inventory")
    bool HasKeycard(FName KeycardID) const;

    TArray<FName> GetOwnedKeycardIDs() const;

    TArray<FName> GetCollectedKeycardPersistenceIDs() const;

    UFUNCTION(BlueprintCallable, Category = "Objectives")
    bool CompleteObjective(FName ObjectiveID);

    UFUNCTION(BlueprintCallable, Category = "Broken Horizon|Accessibility|Subtitles")
    void ShowSubtitle(
        const FText& Speaker,
        const FText& Line,
        float DurationSeconds = 3.0f,
        float DirectionAngleDegrees = 0.0f,
        bool bHasDirection = false
    );

    bool CompleteSharedObjective(FName ObjectiveID);

    void FailSharedOperationObjectives();

    void PropagateSharedOperationFailure();

    void RefreshReplicatedMissionPresentation();

    bool AdoptSharedMissionStateFrom(
        const ABHCharacter* SourceCharacter
    );

    UBHMissionData* GetMissionData() const;

    FName GetCurrentObjectiveID() const;

    TArray<FName> GetCompletedObjectiveIDs() const;

    UFUNCTION(BlueprintPure, Category = "Objectives")
    bool IsObjectiveCompleted(FName ObjectiveID) const;

    UFUNCTION(BlueprintPure, Category = "Objectives")
    bool IsMissionComplete() const;

    UFUNCTION(BlueprintPure, Category = "Objectives")
    bool IsMissionFailed() const;

    UFUNCTION(BlueprintCallable, Category = "Mission")
    bool ReplayMission();

    UFUNCTION(BlueprintCallable, Category = "Game Shell")
    void TogglePauseMenu();

    UFUNCTION(BlueprintCallable, Category = "Game Shell")
    void ResumeFromPause();

    static bool ShouldPauseWorldForMenu(ENetMode NetMode);

    static bool ResolveToggleHoldState(
        bool bCurrentState,
        bool bToggleMode,
        bool bPressed
    );

    static bool ShouldCommitHeldInteraction(
        float HeldDuration,
        float RequiredDuration
    );

    static float CalculateMovementNoiseLoudness(
        float HorizontalSpeed,
        bool bSprinting,
        bool bCrouched,
        bool bProne,
        float SurfaceLoudnessMultiplier,
        float EquipmentLoudnessMultiplier
    );

    static float CalculateFootstepInterval(
        float HorizontalSpeed,
        bool bSprinting,
        bool bCrouched,
        bool bProne
    );

    UFUNCTION(BlueprintCallable, Category = "Persistent War")
    void ToggleWarMap();

    UFUNCTION(BlueprintCallable, Category = "Persistent War|Squad")
    void ToggleFriendlySquadOrder();

    UFUNCTION(BlueprintCallable, Category = "Persistent War")
    void CloseWarMap();

    UFUNCTION(BlueprintCallable, Category = "Persistent War")
    bool BeginNextOperationInWorld();

    UFUNCTION(BlueprintCallable, Category = "Persistent War")
    bool BeginOperationInWorld(
        FName SectorID,
        EBHWarPriorityType OperationType
    );

    void PrepareDeploymentModeForTest();

    UFUNCTION(BlueprintPure, Category = "Persistent War")
    bool IsWarMapOpen() const;

    bool IsRuntimeWarOperation() const;

    bool IsCampaignEpilogueAcknowledged() const;

    bool IsOperationDebriefAcknowledged() const;

    bool CanCreateFieldAutosave() const;

    void RestoreCampaignEpilogueAcknowledgement(
        bool bAcknowledged
    );

    void RestoreOperationDebriefAcknowledgement(
        bool bAcknowledged
    );

    TArray<FBHObjectiveDefinition>
        GetRuntimeObjectiveDefinitions() const;

    FName GetAssignedWarSectorID() const;

    FName GetAssignedWarSupplySourceSectorID() const;

    EBHWarPriorityType GetAssignedWarPriorityType() const;

    FBHOpenWorldOperationState
        GetOpenWorldOperationState() const;

    int32 GetLivingFieldSquadCount() const;

    int32 GetIncapacitatedFieldSquadCount() const;

    int32 GetFieldSquadMembersRequiringEvacuationCount() const;

    FName GetFieldSquadRescueTargetID() const;

    bool HasFieldSquadRescueTarget() const;

    TArray<FBHFieldSquadMemberState>
        GetFieldSquadMemberStates() const;

    int32 CountFieldSquadMembersNeedingService(
        const FVector& ServiceLocation,
        float ServiceRadius
    ) const;

    int32 GetFieldSquadMembersNeedingServiceCount() const;

    int32 ServiceFieldSquadMembers(
        const FVector& ServiceLocation,
        float ServiceRadius,
        bool bAtRescueTreatmentDestination = false
    );

    bool IsFieldSquadHolding() const;

    bool HasFieldSquadCommandLocation() const;

    FVector GetFieldSquadCommandLocation() const;

    float GetFieldSquadCommandYaw() const;

    bool RestoreFieldSquadState(
        int32 SavedLivingCount,
        bool bSavedHolding,
        bool bSavedHasCommandLocation = false,
        const FVector& SavedCommandLocation = FVector::ZeroVector,
        float SavedCommandYaw = 0.0f
    );

    bool RestoreFieldSquadState(
        const TArray<FBHFieldSquadMemberState>& SavedMembers,
        bool bSavedHolding,
        bool bSavedHasCommandLocation = false,
        const FVector& SavedCommandLocation = FVector::ZeroVector,
        float SavedCommandYaw = 0.0f
    );

    int32 BoardFieldSquadTransport(
        ABHFieldTransport* Transport,
        bool bUseSavedPassengerManifest = false
    );

    int32 DisembarkFieldSquadTransport(
        ABHFieldTransport* Transport
    );

    void ApplyFieldSquadTransportDamage(
        float DamageAmount,
        const FDamageEvent& DamageEvent,
        AController* EventInstigator,
        AActor* DamageCauser
    );

    bool IsFieldSquadEmbarked() const;

    FName GetFieldSquadTransportPersistenceID() const;

    bool FailCurrentWarOperation(const FText& FailureReason);

    bool AdoptSharedWarOperationAuthority(
        ABHOpenWorldOperationDirector* OperationDirector,
        FName SectorID,
        FName SupplySourceSectorID,
        EBHWarPriorityType OperationType
    );

    void PresentSharedOperationDebrief(const FText& Message);
    FText GetMissionCompleteMessage() const;
    bool ApplyRapidOperationRedeployment();

    UFUNCTION(BlueprintCallable, Category = "Game Shell")
    bool RestartCheckpoint();

    UFUNCTION(BlueprintCallable, Category = "Game Shell")
    bool ReturnToMainMenu();

    UFUNCTION(BlueprintPure, Category = "Health")
    UBHHealthComponent* GetHealthComponent() const;

    UFUNCTION(BlueprintPure, Category = "Health|Cooperative Casualty")
    bool IsPlayerIncapacitated() const;

    UFUNCTION(BlueprintPure, Category = "Health|Cooperative Casualty")
    bool IsPlayerCasualtyStabilized() const;

    UFUNCTION(BlueprintPure, Category = "Health|Cooperative Casualty")
    float GetPlayerBleedOutSecondsRemaining() const;

    static bool CanEnterCooperativeCasualty(
        bool bMultiplayerWorld,
        bool bAlreadyIncapacitated,
        bool bAlreadyDownedThisLife
    );

    static bool ShouldEscalateFriendlyFire(
        int32 RecentFriendlyHits,
        int32 EscalationThreshold
    );

    static bool CanBeginCasualtyDrag(
        bool bTargetIsPlayer,
        bool bTargetIsIncapacitated,
        bool bTargetIsFriendly,
        bool bAlreadyClaimed,
        float DistanceCentimeters,
        float MaximumDistanceCentimeters
    );

    static bool CanPerformWeaponBash(
        bool bSprinting,
        bool bTraversing,
        bool bHandlingDeath,
        bool bDraggingCasualty,
        float CurrentStamina,
        float RequiredStamina,
        float CurrentTime,
        float LastBashTime
    );

    static bool CanPerformFieldObservation(
        bool bSprinting,
        bool bTraversing,
        bool bAiming,
        bool bHandlingDeath,
        bool bInVehicle,
        float HorizontalSpeed,
        float MaximumStableSpeed,
        float CurrentTime,
        float LastReportTime
    );

    static float CalculateBlastConcussionIntensity(
        float DistanceCentimeters,
        float InnerRadiusCentimeters,
        float OuterRadiusCentimeters,
        bool bHardCover,
        float HardCoverAttenuation
    );

    static bool CanStartControlledBreathing(
        bool bAiming,
        bool bSprinting,
        bool bAlreadyHolding,
        float CurrentStamina,
        float MinimumStamina,
        float RecoveryRemaining
    );

    static float CalculateControlledBreathSpreadMultiplier(
        bool bHolding,
        float HeldDuration,
        float MaximumHeldDuration,
        float MinimumMultiplier,
        float MaximumMultiplier
    );

    static bool CanBraceWeapon(
        bool bAiming,
        bool bSprinting,
        bool bTraversing,
        float HorizontalSpeed,
        float MaximumStableSpeed,
        float DistanceToSupport,
        float MaximumSupportDistance,
        float SupportAlignment,
        float MinimumAlignment,
        bool bSupportUsable,
        float SupportIntegrity
    );

    static float CalculateWeaponBraceMultiplier(
        bool bBraced,
        float SupportQuality,
        float MinimumMultiplier
    );

    UFUNCTION(BlueprintCallable, Category = "Combat|Breathing")
    void SetControlledBreathingRequested(bool bRequested);

    UFUNCTION(Server, Reliable)
    void ServerSetControlledBreathingRequested(bool bRequested);

    UFUNCTION(BlueprintPure, Category = "Injuries")
    UBHInjuryComponent* GetInjuryComponent() const;

    float CalculateIncomingBallisticDamage(
        const FHitResult& HitResult,
        float RawDamage,
        EBHPlayerHitZone& OutHitZone
    ) const;

    void RegisterIncomingBallisticHit(
        EBHPlayerHitZone HitZone,
        float DamageApplied,
        AActor* DamageCauser
    );

    float GetWeaponSpreadMultiplier() const;

    UFUNCTION(BlueprintPure, Category = "Combat|Suppression")
    float GetCurrentPlayerSuppression() const;

    static float AccumulatePlayerSuppression(
        float CurrentSuppression,
        float IncomingIntensity,
        float AccumulationScale
    );

    static float DecayPlayerSuppression(
        float CurrentSuppression,
        float DecayPerSecond,
        float DeltaTime
    );

    static float CalculateNearMissPitch(float Intensity);

    UFUNCTION(BlueprintPure, Category = "Stamina")
    float GetCurrentStamina() const;

    UFUNCTION(BlueprintPure, Category = "Stamina")
    float GetMaxStamina() const;

    UFUNCTION(BlueprintPure, Category = "Loadout|Weight")
    float GetCarriedWeightKilograms() const;

    FBHCarryLoadProfile GetCarryLoadProfile() const;

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Cooperative Logistics")
    bool TryShareFieldSuppliesWith(ABHCharacter* Teammate);

    static int32 CalculateSupplyShareAmount(
        int32 DonorCount,
        int32 DonorEmergencyReserve,
        int32 ReceiverCount,
        int32 ReceiverCapacity,
        int32 TransferBatch
    );

    UFUNCTION(BlueprintPure, Category = "Movement|Lean")
    float GetLeanAmount() const;

    UFUNCTION(BlueprintPure, Category = "Movement|Prone")
    bool IsProne() const;

    UFUNCTION(BlueprintPure, Category = "Movement|Prone")
    float GetAISightRangeMultiplier() const;

    UFUNCTION(BlueprintPure, Category = "Movement|Traversal")
    bool IsTraversing() const;

    UFUNCTION(BlueprintPure, Category = "Movement|Traversal")
    bool IsMantling() const;

    UPROPERTY(BlueprintAssignable, Category = "Stamina")
    FBHOnStaminaChanged OnStaminaChanged;

    UFUNCTION(BlueprintPure, Category = "Weapon")
    UBHWeaponComponent* GetWeaponComponent() const;

    UFUNCTION(BlueprintPure, Category = "Inventory")
    FBHInventorySnapshot GetInventorySnapshot() const;

    UFUNCTION(BlueprintCallable, Category = "Inventory")
    void ToggleInventoryPanel();

    UFUNCTION(BlueprintCallable, Category = "Inventory")
    void CycleInventoryWeaponRole();

    UFUNCTION(BlueprintCallable, Category = "Inventory")
    bool DropInventoryItem(EBHSalvagePickupType ItemType, int32 Quantity = 1);

    UFUNCTION(BlueprintCallable, Category = "Inventory|Transfer")
    bool TransferInventoryItemTo(
        ABHCharacter* Recipient,
        EBHSalvagePickupType ItemType,
        int32 Quantity = 1
    );

    UFUNCTION(BlueprintCallable, Category = "Inventory|Transfer")
    bool TransferFragToNearestAlly(int32 Quantity = 1);

    UFUNCTION(BlueprintCallable, Category = "Inventory|Transfer")
    bool TransferInventoryItemToNearestAlly(
        EBHSalvagePickupType ItemType,
        int32 Quantity = 1
    );

    UFUNCTION(BlueprintPure, Category = "Combat|Grenades")
    int32 GetFragGrenadeCount() const;

    UFUNCTION(BlueprintPure, Category = "Combat|Grenades")
    int32 GetMaxFragGrenades() const;

    UFUNCTION(BlueprintPure, Category = "Combat|Grenades")
    int32 GetSmokeGrenadeCount() const;

    UFUNCTION(BlueprintCallable, Category = "Combat|Grenades")
    int32 AddFragGrenades(int32 Amount);

    void RestoreFragGrenadeCount(int32 SavedCount);

    UFUNCTION(BlueprintPure, Category = "Combat|Grenades")
    int32 GetMaxSmokeGrenades() const;

    UFUNCTION(BlueprintCallable, Category = "Combat|Grenades")
    int32 AddSmokeGrenades(int32 Amount);

    void RestoreSmokeGrenadeCount(int32 SavedCount);

    UFUNCTION(BlueprintPure, Category = "Equipment|Tactical Flashlight")
    bool IsTacticalFlashlightOn() const;

    UFUNCTION(BlueprintPure, Category = "Equipment|Tactical Flashlight")
    float GetTacticalFlashlightBattery() const;

    UFUNCTION(BlueprintCallable, Category = "Equipment|Tactical Flashlight")
    void SetTacticalFlashlightOn(bool bEnabled);

    void RestoreTacticalFlashlightState(
        float SavedBattery,
        bool bSavedOn
    );

    UFUNCTION(BlueprintPure, Category = "Combat|Engineering")
    int32 GetEngineeringChargeCount() const;

    UFUNCTION(BlueprintPure, Category = "Combat|Engineering")
    int32 GetMaxEngineeringCharges() const;

    UFUNCTION(BlueprintCallable, Category = "Combat|Engineering")
    int32 AddEngineeringCharges(int32 Amount);

    void RestoreEngineeringChargeCount(int32 SavedCount);
    bool TryPlaceBreachingCharge(AActor* TargetActor);
    void NotifyEngineeringChargeRemoved(ABHEngineeringCharge* Charge);

    void ShowHitConfirmation(
        bool bLethalHit,
        bool bHeadshot = false
    );

    void ShowDetailedHitConfirmation(
        bool bLethalHit,
        bool bHeadshot,
        bool bArmorHit
    );

    void NotifyIncomingRound(
        const FVector& SourceDirection,
        float Intensity
    );

    void NotifyGrenadeThreat(
        AActor* GrenadeActor,
        const FVector& GrenadeLocation,
        float TimeUntilDetonation
    );

    void NotifyArmoredThreatContact(
        AActor* ThreatActor,
        const FVector& ThreatLocation,
        bool bActive
    );

    UFUNCTION(BlueprintPure, Category = "Presentation")
    USkeletalMeshComponent* GetFirstPersonArmsMesh() const;

    void PlayFirstPersonActionAnimation(
        UAnimSequenceBase* Animation
    );

    void AddFirstPersonFireKick();
    void PlayFirstPersonReloadMotion(float Duration);
    void CancelFirstPersonActionAnimation();
    void CancelFirstPersonReloadMotion();

    UFUNCTION(BlueprintCallable, Category = "UI")
    void ShowStatusNotification(const FText& Message);

    UFUNCTION(BlueprintCallable, Category = "UI")
    void ShowPriorityStatusNotification(
        const FText& Message,
        EBHNotificationPriority NotificationPriority
    );

    UFUNCTION(BlueprintCallable, Category = "UI")
    void ShowDeferredStrategicStatusNotification(const FText& Message);

    UFUNCTION(BlueprintCallable, Category = "UI")
    void ShowStatusNotificationWithAudioCue(
        const FText& Message,
        EBHNotificationPriority NotificationPriority,
        EBHNotificationAudioCue AudioCue
    );

    void NotifySquadMoveAndHoldFailure(
        ABHEnemySoldier* SquadMember,
        const FVector& FailedDestination,
        const FVector& FallbackLocation
    );

    bool RestorePersistentState(
        UBHMissionData* SavedMissionData,
        FName SavedCurrentObjectiveID,
        const TArray<FName>& SavedCompletedObjectiveIDs,
        bool bSavedMissionComplete,
        bool bSavedMissionFailed,
        const TArray<FName>& SavedOwnedKeycardIDs,
        const TArray<FName>& SavedCollectedKeycardPersistenceIDs
    );

    bool RestoreRuntimeOperationState(
        UBHMissionData* SavedMissionData,
        const TArray<FBHObjectiveDefinition>& SavedRuntimeObjectives,
        FName SavedCurrentObjectiveID,
        const TArray<FName>& SavedCompletedObjectiveIDs,
        bool bSavedMissionComplete,
        bool bSavedMissionFailed,
        FName SavedAssignedSectorID,
        FName SavedAssignedSupplySourceSectorID,
        EBHWarPriorityType SavedAssignedPriorityType,
        const FBHOpenWorldOperationState&
            SavedOpenWorldOperationState,
        const TArray<FName>& SavedOwnedKeycardIDs,
        const TArray<FName>& SavedCollectedKeycardPersistenceIDs
    );

    UFUNCTION()
    void OnObjectiveCompleted(
        FName CompletedObjectiveID,
        FText CompletedObjectiveText
    );

    UFUNCTION()
    void OnMissionCompleted();

    UFUNCTION(BlueprintCallable, Category = "Combat|Anti Vehicle")
    void LaunchAntiVehicleProjectile();

    UFUNCTION(BlueprintPure, Category = "Combat|Anti Vehicle")
    int32 GetAntiVehicleRoundCount() const;

    UFUNCTION(BlueprintCallable, Category = "Combat|Anti Vehicle")
    int32 AddAntiVehicleRounds(int32 Amount);

    void RestoreAntiVehicleRoundCount(int32 SavedCount);

protected:
    virtual void BeginPlay() override;

    virtual void EndPlay(
        const EEndPlayReason::Type EndPlayReason
    ) override;

    virtual void SetupPlayerInputComponent(
        UInputComponent* PlayerInputComponent) override;

    void EnsureRuntimeInputActions();
    void RefreshPlayerInputMappings();

    UFUNCTION()
    void HandleInputBindingsChanged();

    APlayerController* ResolveOwningPlayerController() const;

    void Move(const FInputActionValue& Value);
    void Look(const FInputActionValue& Value);

    void StartJump();
    void StopJump();

    void StartSprint();
    void StopSprint();
    void HandleSprintPressed();
    void HandleSprintReleased();

    void StartCrouch();
    void StopCrouch();
    void HandleCrouchPressed();
    void HandleCrouchReleased();

    void StartLeanLeft();
    void StopLeanLeft();
    void StartLeanRight();
    void StopLeanRight();
    void HandleLeanLeftPressed();
    void HandleLeanLeftReleased();
    void HandleLeanRightPressed();
    void HandleLeanRightReleased();

    void ToggleProne();
    void HandlePronePressed();
    void HandleProneReleased();
    bool EnterProne();
    bool TryExitProne();

    void StartFire();
    void StopFire();
    void StartAim();
    void StopAim();
    void HandleAimPressed();
    void HandleAimReleased();
    void Reload();

    void RefreshWeaponFireModeHUD(EBHFireMode NewFireMode);
    void ToggleFireMode();
    void ThrowFragGrenade();
    void BeginFragGrenadeCook();
    void ReleaseFragGrenade();
    void ThrowFragGrenade(float CookDuration);
    void ThrowSmokeGrenade();
    void BeginControlledBreathing();
    void EndControlledBreathing();
    void ToggleTacticalFlashlight();
    void PerformWeaponBash();
    void PerformFieldObservation();
    void UseEngineeringTool();

    bool PlaceEngineeringCharge(
        AActor* TargetActor,
        EBHEngineeringChargeMode Mode
    );
    void DetonateEngineeringCharges();
    void RefreshEngineeringHUD();
    void IssueSquadPing();
    void UseFieldDressing();
    void UseMedkit();

    virtual void Tick(float DeltaTime) override;

    void UpdateMovementAudio(float DeltaTime);
    void UpdateInputPromptDevice();
    void EmitAuthoritativeFootstep();
    uint8 ResolveFootstepSurfaceType() const;
    USoundBase* ResolveFootstepSound(uint8 SurfaceType) const;

    UFUNCTION(NetMulticast, Unreliable)
    void MulticastPlayFootstep(
        uint8 SurfaceType,
        float VolumeMultiplier,
        float PitchMultiplier
    );
    
    void EnsureInteractionPromptWidget();
    void UpdateInteractionPrompt();
    
    void Interact();
    void HandleInteractPressed();
    void HandleInteractReleased();
    void CancelInteractionInput();

    const UBHUserSettingsSubsystem* GetUserSettings() const;

    void IssueFieldSquadContextAction();

    UFUNCTION(Server, Reliable)
    void ServerInteract(AActor* RequestedTarget);

    UFUNCTION(Server, Reliable)
    void ServerDropInventoryItem(EBHSalvagePickupType ItemType, int32 Quantity);

    UFUNCTION(Server, Reliable)
    void ServerTransferInventoryItem(
        ABHCharacter* Recipient,
        EBHSalvagePickupType ItemType,
        int32 Quantity
    );

    UFUNCTION(Server, Reliable)
    void ServerTransferFragToNearestAlly(int32 Quantity);

    UFUNCTION(Server, Reliable)
    void ServerTransferInventoryItemToNearestAlly(
        EBHSalvagePickupType ItemType,
        int32 Quantity
    );

    bool TransferInventoryItemToNearestAllyAuthority(
        EBHSalvagePickupType ItemType,
        int32 Quantity
    );

    UFUNCTION(Server, Reliable)
    void ServerUseEngineeringTool();

    UFUNCTION(Server, Reliable)
    void ServerLaunchAntiVehicleProjectile();

    UFUNCTION(Server, Reliable)
    void ServerUseFieldDressing();

    UFUNCTION(Server, Reliable)
    void ServerUseMedkit();

    UFUNCTION(Server, Reliable)
    void ServerSetTacticalFlashlightOn(bool bEnabled);

    UFUNCTION(Server, Reliable)
    void ServerPerformWeaponBash();

    UFUNCTION(Server, Reliable)
    void ServerRequestFieldSquadContextAction(
        AActor* RequestedTarget
    );

    UFUNCTION(Server, Reliable)
    void ServerRequestSquadPing();

    UFUNCTION(Server, Reliable)
    void ServerRequestDeployOperation(
        FName SectorID,
        EBHWarPriorityType OperationType
    );

    UFUNCTION(Server, Reliable)
    void ServerRequestWithdrawOperation();

    UFUNCTION(Server, Reliable)
    void ServerRequestMobilizeMilitia(FName SectorID);

    UFUNCTION(Server, Reliable)
    void ServerRequestRedeployGarrison(
        FName DestinationSectorID
    );

    UFUNCTION(Server, Reliable)
    void ServerRequestCivilianAid(
        FName TargetSectorID,
        EBHWarPriorityType OperationType
    );

    UFUNCTION(Server, Reliable)
    void ServerRequestContinueDebrief();

    UFUNCTION(Server, Reliable)
    void ServerRequestToggleFriendlySquadOrder();

    UFUNCTION(Client, Reliable)
    void ClientShowStatusNotification(const FText& Message);

    UFUNCTION(Client, Reliable)
    void ClientShowPriorityStatusNotification(
        const FText& Message,
        EBHNotificationPriority NotificationPriority
    );

    UFUNCTION(Client, Reliable)
    void ClientShowStatusNotificationWithAudioCue(
        const FText& Message,
        EBHNotificationPriority NotificationPriority,
        EBHNotificationAudioCue AudioCue
    );

    UFUNCTION(Client, Reliable)
    void ClientShowDeferredStrategicStatusNotification(
        const FText& Message
    );

    UFUNCTION(Client, Reliable)
    void ClientPresentDeath(float DelaySeconds);

    UFUNCTION(Client, Reliable)
    void ClientSetPlayerCasualtyPresentation(
        bool bIncapacitated,
        bool bStabilized,
        float BleedOutSeconds
    );

    UFUNCTION(Client, Reliable)
    void ClientCompleteFieldRespawn();

    UFUNCTION(Client, Reliable)
    void ClientConfirmOperationDeployment(
        FName SectorID,
        FName SupplySourceSectorID,
        EBHWarPriorityType OperationType
    );

    UFUNCTION(Client, Reliable)
    void ClientPresentOperationDebrief(const FText& Message);

    UFUNCTION(Client, Reliable)
    void ClientConfirmDebriefContinue(bool bCampaignResolved);

    void DisplayStatusNotificationLocally(const FText& Message);
    void DisplayStatusNotificationLocally(
        const FText& Message,
        EBHNotificationPriority NotificationPriority
    );
    void DisplayDeferredStrategicStatusNotificationLocally(
        const FText& Message
    );
    bool IsLocalCombatIntensityActive() const;
    void DisplayStatusNotificationLocally(
        const FText& Message,
        EBHNotificationPriority NotificationPriority,
        EBHNotificationAudioCue AudioCue
    );

    UFUNCTION(Client, Unreliable)
    void ClientShowHitConfirmation(
        bool bLethalHit,
        bool bHeadshot
    );

    UFUNCTION(Client, Unreliable)
    void ClientShowDetailedHitConfirmation(
        bool bLethalHit,
        bool bHeadshot,
        bool bArmorHit
    );

    UFUNCTION(Client, Unreliable)
    void ClientNotifyIncomingRound(
        FVector SourceDirection,
        float Intensity
    );

    UFUNCTION(Client, Unreliable)
    void ClientNotifyCombatDamage(
        float DamageApplied,
        float HealthPercentage,
        FVector DamageSourceDirection,
        AActor* DamageCauser,
        bool bShouldInterruptReload
    );

    void DisplayHitConfirmationLocally(
        bool bLethalHit,
        bool bHeadshot
    );

    void DisplayDetailedHitConfirmationLocally(
        bool bLethalHit,
        bool bHeadshot,
        bool bArmorHit
    );

    void DisplayIncomingRoundLocally(
        const FVector& SourceDirection,
        float Intensity
    );

    void DisplayCombatDamageLocally(
        float DamageApplied,
        float HealthPercentage,
        const FVector& DamageSourceDirection,
        AActor* DamageCauser
    );

    bool ResolveInteractionTarget(AActor*& OutTarget);

    bool ResolveFieldSquadContextTarget(AActor*& OutTarget) const;

    void ExecuteInteraction(AActor* TargetActor);

    void RefreshObjectiveWidget();
    void RefreshMissionPresentationVisibility();
    bool HasTacticalOperationWaypoint() const;
    void RefreshStrategicBriefing();

    void RefreshFragGrenadeHUD();
    void RefreshSmokeGrenadeHUD();

    void UpdateOperationWaypointHUD();
    void SynchronizeReplicatedOperationPresentation();
    bool ShouldBindActiveOperationSnapshotPresentation() const;
    void TryBindActiveOperationSnapshotPresentation();
    void UnbindActiveOperationSnapshotPresentation();
    void HandleActiveOperationSnapshotChanged(
        const FBHActiveOperationSnapshot& Snapshot
    );

    void UpdateSquadCommandWaypointHUD();

    void EnsureCombatStatusWidget();

    void UpdateSquadPingWaypointHUD();

    void ExecuteSquadPing();

    FName ResolveSquadPingContextLabel(AActor* HitActor) const;

    void UpdateResupplyWaypointHUD(float DeltaTime);

    void UpdateConvoyWaypointHUD(float DeltaTime);

    void UpdateTransportWaypointHUD(float DeltaTime);

    void UpdateLogisticsWaypointHUD(float DeltaTime);

    void UpdateVehicleReadinessHUD();

    void UpdateFieldSquadStatusHUD();

    UFUNCTION()
    void OnRep_FieldSquadContextStatus();

    void UpdateStrategicSituationHUD(float DeltaTime);

    void CacheObservedWarState(UBHWarSubsystem* WarSubsystem);

    void ConfigureStrategicMissionPresentation();

    bool StartOpenWorldOperationDirector(
        bool bRestoringSavedState = false
    );

    bool TryRecruitFieldSquadMember();

    bool SpawnFieldSquadMember(int32 FormationIndex);

    bool TryStabilizeFieldSquadMember(
        ABHEnemySoldier* SquadMember
    );

    void ExecuteFieldSquadContextAction(AActor* TargetActor);

    void UpdateFieldSquadContextAction();

    void CancelFieldSquadContextAction(
        bool bNotifyCommander,
        const FText& Reason = FText::GetEmpty()
    );

    bool IsSharedFieldSquadMember(
        const ABHEnemySoldier* SquadMember
    ) const;

    ABHCharacter* FindFieldSquadOwner(
        const ABHEnemySoldier* SquadMember
    ) const;

    bool TryResolveFieldSquadCommandLocation(
        FVector& OutCommandLocation
    ) const;

    void ApplyFieldSquadOrder();

    ABHSectorResupplyStation*
        FindNearbyFriendlyResupplyStation() const;

    void DestroyFieldSquad();

    UFUNCTION()
    void HandleFieldSquadMemberDeath(AActor* DamageCauser);

    UFUNCTION()
    void HandleFieldSquadMemberCasualtyExpired(
        AActor* ExpiredOperative
    );

    void EnterMissionCompleteState(bool bSaveProgress);

    void EnterCampaignEpilogueFreeRoam(
        bool bOpenStrategicMap
    );

    void EnterPostOperationFreeRoam(
        bool bOpenStrategicMap
    );

    void OpenWarMap(bool bDeploymentMode);

    UFUNCTION()
    void HandleMissionContinueRequested();

    UFUNCTION()
    void HandleDeployRequested(
        FName SectorID,
        EBHWarPriorityType OperationType
    );

    UFUNCTION()
    void HandleWithdrawRequested();

    UFUNCTION()
    void HandleMilitiaRequested(FName SectorID);

    UFUNCTION()
    void HandleGarrisonRedeployRequested(
        FName DestinationSectorID
    );

    UFUNCTION()
    void HandleCivilianAidRequested(
        FName TargetSectorID,
        EBHWarPriorityType OperationType
    );

    bool ExecuteDeployOperationRequest(
        FName SectorID,
        EBHWarPriorityType OperationType
    );

    bool ExecuteWithdrawOperationRequest();
    bool ExecuteMobilizeMilitiaRequest(FName SectorID);
    bool ExecuteRedeployGarrisonRequest(
        FName DestinationSectorID
    );
    bool ExecuteCivilianAidRequest(
        FName TargetSectorID,
        EBHWarPriorityType OperationType
    );

    UFUNCTION()
    void HandleWarStateChanged(
        int32 NewTurnNumber,
        FName NewPrioritySectorID,
        EBHWarPriorityType NewPriorityType
    );

    UFUNCTION()
    void HandleDeath(AActor* DamageCauser);

    void EnterPlayerIncapacitation(AActor* DamageCauser);
    void FinalizePlayerCasualtyDeath(AActor* DamageCauser);
    void HandlePlayerBleedOutExpired();
    void TryTreatPlayerCasualty(ABHCharacter* Casualty);
    void CompletePlayerRevive(ABHCharacter* Reviver);

    UFUNCTION()
    void OnRep_PlayerCasualtyState();

    UFUNCTION()
    void HandleHealthChanged(
        float NewCurrentHealth,
        float NewMaxHealth
    );

    UFUNCTION()
    void HandlePlayerDamaged(
        float DamageApplied,
        AActor* DamageCauser
    );

    UFUNCTION()
    void HandleAmmoChanged(
        int32 MagazineAmmo,
        int32 ReserveAmmo
    );

    UFUNCTION()
    void HandleWeaponRoleChanged(EBHWeaponRole NewRole);

    UFUNCTION()
    void HandleWeaponHeatChanged(float HeatNormalized, bool bOverheated);

    UFUNCTION()
    void HandleInjuryStateChanged(
        bool bBleeding,
        float BleedRate,
        bool bArmInjured,
        bool bLegInjured,
        int32 FieldDressings
    );

    UFUNCTION()
    void HandleMedicalStateChanged(
        int32 Medkits,
        float HelmetDurabilityPercentage,
        float BodyArmorDurabilityPercentage,
        bool bTreatmentActive,
        float TreatmentProgress
    );

    UFUNCTION()
    void HandleMedkitTreatmentCompleted();

    void UpdateCarryLoadHUD();
    void RefreshOpenInventoryPanel();

    UFUNCTION()
    void OnRep_FragInventory();
    UFUNCTION()
    void OnRep_SmokeGrenadeInventory();

    UFUNCTION()
    void OnRep_TacticalFlashlight();

    UFUNCTION()
    void OnRep_ControlledBreathing();

    UFUNCTION()
    void OnRep_CurrentStamina();

    UFUNCTION()
    void OnRep_WeaponBrace();

    UFUNCTION()
    void OnRep_OwnedKeycards();

    void UpdateTacticalFlashlightVisual();
    void UpdateWeaponBraceState();

    void SavePlayerConditionCheckpoint(FName Reason);
    const FBHObjectiveDefinition* FindAuthoredObjectiveDefinition(
        FName ObjectiveID
    ) const;
    void QueueObjectiveActivationRadio(FName ObjectiveID);
    void QueueObjectiveCompletionRadio(FName ObjectiveID);

    void RespawnAfterDeath();

    bool CanLean() const;
    float GetRequestedLeanDirection() const;
    float ResolveCollisionLimitedLean(float LeanDirection) const;
    void UpdateLean(float DeltaTime);
    void UpdateHeadBob(float DeltaTime);

    bool CanEnterProne() const;
    bool HasProneExitClearance(
        float TargetCapsuleRadius,
        float TargetCapsuleHalfHeight
    ) const;
    void UpdateProne(float DeltaTime);
    void ApplyMovementSpeed();

    bool TryStartTraversal();
    bool CanStartTraversal() const;
    bool FindTraversalTarget(
        bool& bOutMantle,
        FVector& OutTargetLocation,
        FVector& OutApexLocation
    ) const;
    bool IsTraversalPathClear(
        const FVector& StartLocation,
        const FVector& ApexLocation,
        const FVector& TargetLocation
    ) const;
    bool SpendStamina(float Amount);
    void UpdateTraversal(float DeltaTime);
    void FinishTraversal(bool bCompleted);

    void RefreshFirstPersonArmsAnimation();
    void HandleFirstPersonActionFinished();
    void UpdateFirstPersonPresentationOffsets(float DeltaTime);
    void UpdateInitialWorldStreaming(float DeltaTime);
  

    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "Broken Horizon|Camera")
    TObjectPtr<UCameraComponent> FirstPersonCamera;

    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "Broken Horizon|Equipment|Tactical Flashlight"
    )
    TObjectPtr<USpotLightComponent> TacticalFlashlight;

    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "Broken Horizon|World Streaming"
    )
    TObjectPtr<UWorldPartitionStreamingSourceComponent>
        OpenWorldStreamingSource;

    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "Broken Horizon|Navigation"
    )
    TObjectPtr<UNavigationInvokerComponent> NavigationInvoker;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Broken Horizon|World Streaming",
        meta = (ClampMin = "0.0")
    )
    float InitialWorldStreamingMinimumHoldTime = 2.0f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Broken Horizon|World Streaming",
        meta = (ClampMin = "1.0")
    )
    float InitialWorldStreamingTimeout = 20.0f;

    bool bWaitingForInitialWorldStreaming = false;
    float InitialWorldStreamingElapsed = 0.0f;

    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "Broken Horizon|Presentation"
    )
    TObjectPtr<USkeletalMeshComponent> FirstPersonArms;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Broken Horizon|Presentation|Animation"
    )
    TObjectPtr<UAnimSequenceBase> FirstPersonIdleAnimation;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Broken Horizon|Presentation|Animation"
    )
    TObjectPtr<UAnimSequenceBase> FirstPersonWalkAnimation;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Broken Horizon|Presentation|Animation"
    )
    TObjectPtr<UAnimSequenceBase> FirstPersonRunAnimation;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Broken Horizon|Presentation|Animation"
    )
    TObjectPtr<UAnimSequenceBase> FirstPersonAimIdleAnimation;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Broken Horizon|Presentation|Animation"
    )
    TObjectPtr<UAnimSequenceBase> FirstPersonAimWalkAnimation;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Broken Horizon|Presentation|Animation",
        meta = (ClampMin = "0.0")
    )
    float FirstPersonMovementAnimationThreshold = 5.0f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Broken Horizon|Presentation|Procedural"
    )
    bool bEnableProceduralWeaponMotion = true;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Broken Horizon|Presentation|Procedural"
    )
    FVector FirstPersonFireKickLocation = FVector(-2.0f, 0.0f, -0.5f);

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Broken Horizon|Presentation|Procedural"
    )
    FRotator FirstPersonFireKickRotation = FRotator(2.0f, 0.0f, 0.75f);

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Broken Horizon|Presentation|Procedural",
        meta = (ClampMin = "0.0")
    )
    float FirstPersonFireKickRecoverySpeed = 18.0f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Broken Horizon|Presentation|Procedural"
    )
    FVector FirstPersonReloadLocation = FVector(-6.0f, 3.0f, -18.0f);

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Broken Horizon|Presentation|Procedural"
    )
    FRotator FirstPersonReloadRotation = FRotator(-12.0f, 0.0f, 8.0f);

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Broken Horizon|Presentation|Procedural"
    )
    FVector FirstPersonMedicalLocation =
        FVector(-8.0f, 5.0f, -22.0f);

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Broken Horizon|Presentation|Procedural"
    )
    FRotator FirstPersonMedicalRotation =
        FRotator(-18.0f, 8.0f, 12.0f);

    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "Broken Horizon|Health"
    )
    TObjectPtr<UBHHealthComponent> HealthComponent;

    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "Broken Horizon|Injuries"
    )
    TObjectPtr<UBHInjuryComponent> InjuryComponent;

    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "Broken Horizon|Weapon"
    )
    TObjectPtr<UBHWeaponComponent> WeaponComponent;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Broken Horizon|Combat|Grenades"
    )
    TSubclassOf<ABHFragGrenade> FragGrenadeClass;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Broken Horizon|Combat|Grenades"
    )
    TSubclassOf<ABHSmokeGrenade> SmokeGrenadeClass;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Broken Horizon|Combat|Engineering"
    )
    TSubclassOf<ABHEngineeringCharge> EngineeringChargeClass;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Broken Horizon|Combat|Anti Vehicle")
    TSubclassOf<ABHAntiVehicleProjectile> AntiVehicleProjectileClass;

    UPROPERTY(Replicated, EditDefaultsOnly, BlueprintReadOnly, Category = "Broken Horizon|Combat|Anti Vehicle", meta = (ClampMin = "0"))
    int32 AntiVehicleRoundCount = 2;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Broken Horizon|Combat|Anti Vehicle", meta = (ClampMin = "0"))
    int32 MaxAntiVehicleRounds = 2;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Broken Horizon|Combat|Grenades",
        meta = (ClampMin = "0")
    )
    int32 MaxFragGrenades = 2;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Broken Horizon|Combat|Grenades",
        meta = (ClampMin = "0")
    )
    int32 MaxSmokeGrenades = 1;

    UPROPERTY(
        ReplicatedUsing = OnRep_FragInventory,
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "Broken Horizon|Combat|Grenades"
    )
    int32 FragGrenadeCount = 2;

    UPROPERTY(
        ReplicatedUsing = OnRep_SmokeGrenadeInventory,
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "Broken Horizon|Combat|Grenades"
    )
    int32 SmokeGrenadeCount = 1;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Broken Horizon|Combat|Grenades",
        meta = (ClampMin = "0.0", Units = "cm/s")
    )
    float FragGrenadeThrowSpeed = 1400.0f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Broken Horizon|Combat|Grenades",
        meta = (ClampMin = "0.0", Units = "s")
    )
    float MaxFragGrenadeCookDuration = 2.5f;

    UPROPERTY(
        ReplicatedUsing = OnRep_TacticalFlashlight,
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "Broken Horizon|Equipment|Tactical Flashlight"
    )
    bool bTacticalFlashlightOn = false;

    UPROPERTY(
        Replicated,
        EditAnywhere,
        BlueprintReadOnly,
        Category = "Broken Horizon|Equipment|Tactical Flashlight",
        meta = (ClampMin = "0.0", ClampMax = "100.0")
    )
    float TacticalFlashlightBattery = 100.0f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Broken Horizon|Equipment|Tactical Flashlight",
        meta = (ClampMin = "1.0")
    )
    float TacticalFlashlightMaxBattery = 100.0f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Broken Horizon|Equipment|Tactical Flashlight",
        meta = (ClampMin = "0.0", Units = "s")
    )
    float TacticalFlashlightBatteryDrainPerSecond = 0.18f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Broken Horizon|Combat|Weapon Bash",
        meta = (ClampMin = "0.0")
    )
    float WeaponBashStaminaCost = 18.0f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Broken Horizon|Combat|Weapon Bash",
        meta = (ClampMin = "0.05", Units = "s")
    )
    float WeaponBashCooldown = 1.0f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Broken Horizon|Combat|Weapon Bash",
        meta = (ClampMin = "0.0", Units = "cm")
    )
    float WeaponBashRange = 180.0f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Broken Horizon|Combat|Weapon Bash",
        meta = (ClampMin = "0.0")
    )
    float WeaponBashDamage = 25.0f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Broken Horizon|Recon|Field Observation",
        meta = (ClampMin = "0.1", Units = "s")
    )
    float FieldObservationCooldown = 8.0f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Broken Horizon|Recon|Field Observation",
        meta = (ClampMin = "1.0", Units = "cm/s")
    )
    float FieldObservationMaximumStableSpeed = 80.0f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Broken Horizon|Recon|Field Observation",
        meta = (ClampMin = "0.0", Units = "cm")
    )
    float FieldObservationRange = 15000.0f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Broken Horizon|Combat|Breathing",
        meta = (ClampMin = "0.5", Units = "s")
    )
    float ControlledBreathMaximumDuration = 4.0f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Broken Horizon|Combat|Breathing",
        meta = (ClampMin = "0.0")
    )
    float ControlledBreathStaminaDrainPerSecond = 6.0f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Broken Horizon|Combat|Breathing",
        meta = (ClampMin = "0.0", Units = "s")
    )
    float ControlledBreathRecoveryDuration = 2.0f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Broken Horizon|Combat|Breathing",
        meta = (ClampMin = "0.1", ClampMax = "1.0")
    )
    float ControlledBreathMinimumSpreadMultiplier = 0.35f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Broken Horizon|Combat|Breathing",
        meta = (ClampMin = "1.0")
    )
    float ControlledBreathMaximumStrainMultiplier = 1.2f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Broken Horizon|Combat|Fortification",
        meta = (ClampMin = "0.1", ClampMax = "1.0")
    )
    float MinimumWeaponBraceSpreadMultiplier = 0.45f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Broken Horizon|Combat|Fortification",
        meta = (ClampMin = "1.0", Units = "cm")
    )
    float WeaponBraceMaximumSupportDistance = 55.0f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Broken Horizon|Combat|Fortification",
        meta = (ClampMin = "1.0", Units = "cm/s")
    )
    float WeaponBraceMaximumStableSpeed = 80.0f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Broken Horizon|Combat|Fortification",
        meta = (ClampMin = "0.0", ClampMax = "1.0")
    )
    float WeaponBraceMinimumAlignment = 0.72f;

    UPROPERTY(
        ReplicatedUsing = OnRep_EngineeringInventory,
        EditAnywhere,
        BlueprintReadOnly,
        Category = "Broken Horizon|Combat|Engineering",
        meta = (ClampMin = "0")
    )
    int32 EngineeringChargeCount = 2;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Broken Horizon|Combat|Engineering",
        meta = (ClampMin = "0")
    )
    int32 MaxEngineeringCharges = 2;

    UPROPERTY(ReplicatedUsing = OnRep_EngineeringInventory)
    int32 ActiveEngineeringChargeCount = 0;

    UPROPERTY()
    TArray<TObjectPtr<ABHEngineeringCharge>> ActiveEngineeringCharges;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Broken Horizon|Squad|Ping",
        meta = (ClampMin = "1000.0", Units = "cm")
    )
    float SquadPingMaximumDistance = 20000.0f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Broken Horizon|Squad|Ping",
        meta = (ClampMin = "1.0", Units = "s")
    )
    float SquadPingLifetime = 12.0f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Broken Horizon|Squad|Ping",
        meta = (ClampMin = "0.1", Units = "s")
    )
    float SquadPingCooldown = 1.0f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Broken Horizon|Input")
    TObjectPtr<UInputMappingContext> PlayerMappingContext;

    UPROPERTY(Transient)
    TObjectPtr<UInputMappingContext> RuntimePlayerMappingContext;

    UPROPERTY(Transient)
    TObjectPtr<UInputAction> WarMapInputAction;

    UPROPERTY(Transient)
    TObjectPtr<UInputAction> GrenadeInputAction;

    UPROPERTY(Transient)
    TObjectPtr<UInputAction> SmokeGrenadeInputAction;

    UPROPERTY(Transient)
    TObjectPtr<UInputAction> TacticalFlashlightInputAction;

    UPROPERTY(Transient)
    TObjectPtr<UInputAction> WeaponBashInputAction;

    UPROPERTY(Transient)
    TObjectPtr<UInputAction> FieldObservationInputAction;

    UPROPERTY(Transient)
    TObjectPtr<UInputAction> ControlledBreathingInputAction;

    UPROPERTY(Transient)
    TObjectPtr<UInputAction> EngineeringInputAction;

    UPROPERTY(Transient)
    TObjectPtr<UInputAction> AntiVehicleInputAction;

    UPROPERTY(Transient)
    TObjectPtr<UInputAction> SquadOrderInputAction;

    UPROPERTY(Transient)
    TObjectPtr<UInputAction> ContextInputAction;

    UPROPERTY(Transient)
    TObjectPtr<UInputAction> SquadPingInputAction;

    UPROPERTY(Transient)
    TObjectPtr<UInputAction> FireModeInputAction;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Broken Horizon|Input")
    TObjectPtr<UInputAction> MoveAction;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Broken Horizon|Input")
    TObjectPtr<UInputAction> JumpAction;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Broken Horizon|Input")
    TObjectPtr<UInputAction> LookAction;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Broken Horizon|Input")
    TObjectPtr<UInputAction> SprintAction;

    UPROPERTY(
        EditDefaultsOnly, 
        BlueprintReadOnly,
        Category = "Broken Horizon|Input")
    TObjectPtr<UInputAction> CrouchAction;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Broken Horizon|Input")
    TObjectPtr<UInputAction> LeanLeftAction;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Broken Horizon|Input")
    TObjectPtr<UInputAction> LeanRightAction;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Broken Horizon|Input")
    TObjectPtr<UInputAction> ProneAction;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Broken Horizon|Input")
    TObjectPtr<UInputAction> FireAction;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Broken Horizon|Input")
    TObjectPtr<UInputAction> AimAction;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Broken Horizon|Input")
    TObjectPtr<UInputAction> ReloadAction;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Broken Horizon|Combat|Reload",
        meta = (ClampMin = "0.15", ClampMax = "0.60", Units = "s")
    )
    float EmergencyReloadDoubleTapWindow = 0.35f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Broken Horizon|Input"
    )
    TObjectPtr<UInputAction> PauseAction;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Broken Horizon|Input"
    )
    TObjectPtr<UInputAction> InventoryAction;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Broken Horizon|Input"
    )
    TObjectPtr<UInputAction> InventoryCycleAction;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Broken Horizon|Input"
    )
    TObjectPtr<UInputAction> FieldDressingAction;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Broken Horizon|Input"
    )
    TObjectPtr<UInputAction> MedkitAction;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Broken Horizon|Movement")
    float WalkSpeed = 400.0f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Broken Horizon|Movement")
    float SprintSpeed = 700.0f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Broken Horizon|Movement|Audio",
        meta = (ClampMin = "0.0")
    )
    float EquipmentNoiseMultiplier = 1.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Broken Horizon|Movement|Audio")
    TObjectPtr<USoundBase> DefaultFootstepSound;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Broken Horizon|Movement|Audio")
    TObjectPtr<USoundBase> ConcreteFootstepSound;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Broken Horizon|Movement|Audio")
    TObjectPtr<USoundBase> DirtFootstepSound;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Broken Horizon|Movement|Audio")
    TObjectPtr<USoundBase> GrassFootstepSound;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Broken Horizon|Movement|Audio")
    TObjectPtr<USoundBase> MetalFootstepSound;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Broken Horizon|Movement|Audio")
    TObjectPtr<USoundBase> WaterFootstepSound;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Broken Horizon|Combat|Accuracy",
        meta = (ClampMin = "1.0")
    )
    float MovingWeaponSpreadMultiplier = 1.8f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Broken Horizon|Combat|Accuracy",
        meta = (ClampMin = "0.1", ClampMax = "1.0")
    )
    float CrouchedStationarySpreadMultiplier = 0.85f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Broken Horizon|Combat|Accuracy",
        meta = (ClampMin = "1.0")
    )
    float ExhaustedWeaponSpreadMultiplier = 1.35f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Broken Horizon|Combat|Suppression",
        meta = (ClampMin = "0.0", ClampMax = "1.0")
    )
    float PlayerSuppressionAccumulationScale = 0.35f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Broken Horizon|Combat|Suppression",
        meta = (ClampMin = "0.0")
    )
    float PlayerSuppressionDecayPerSecond = 0.20f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Broken Horizon|Combat|Suppression",
        meta = (ClampMin = "1.0")
    )
    float MaximumSuppressionSpreadMultiplier = 1.45f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Broken Horizon|Combat|Readability",
        meta = (ClampMin = "0.0", Units = "s"))
    float StrategicNotificationCombatQuietSeconds = 8.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Broken Horizon|Combat|Suppression|Audio")
    TObjectPtr<USoundBase> NearMissSound;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Broken Horizon|Combat|Suppression|Audio", meta = (ClampMin = "0.0", Units = "s"))
    float MinimumNearMissSoundInterval = 0.08f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Broken Horizon|Combat|Accuracy",
        meta = (ClampMin = "0.0", Units = "cm/s")
    )
    float StableWeaponVelocityThreshold = 10.0f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Broken Horizon|Movement|Lean",
        meta = (ClampMin = "0.0", Units = "cm")
    )
    float MaximumLeanDistance = 32.0f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Broken Horizon|Movement|Lean",
        meta = (ClampMin = "0.0", ClampMax = "30.0", Units = "Degrees")
    )
    float MaximumLeanRoll = 9.0f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Broken Horizon|Movement|Lean",
        meta = (ClampMin = "0.0")
    )
    float LeanInterpolationSpeed = 12.0f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Broken Horizon|Movement|Lean",
        meta = (ClampMin = "0.0", Units = "cm")
    )
    float LeanCollisionRadius = 8.0f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Broken Horizon|Movement|Lean",
        meta = (ClampMin = "0.0", Units = "cm")
    )
    float LeanCollisionPadding = 2.0f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Broken Horizon|Movement|Lean",
        meta = (ClampMin = "1.0")
    )
    float LeanWeaponSpreadMultiplier = 1.1f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Broken Horizon|Movement|Prone",
        meta = (ClampMin = "0.0", Units = "cm/s")
    )
    float ProneSpeed = 140.0f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Broken Horizon|Movement|Prone",
        meta = (ClampMin = "1.0", Units = "cm")
    )
    float ProneCapsuleRadius = 30.0f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Broken Horizon|Movement|Prone",
        meta = (ClampMin = "1.0", Units = "cm")
    )
    float ProneCapsuleHalfHeight = 34.0f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Broken Horizon|Movement|Prone",
        meta = (ClampMin = "0.0", Units = "cm")
    )
    float ProneCameraDrop = 42.0f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Broken Horizon|Movement|Prone",
        meta = (ClampMin = "0.0")
    )
    float ProneTransitionSpeed = 8.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Broken Horizon|Movement|Camera Comfort", meta = (ClampMin = "0.0", Units = "cm"))
    float HeadBobAmplitude = 1.5f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Broken Horizon|Movement|Camera Comfort", meta = (ClampMin = "0.0"))
    float HeadBobFrequency = 9.0f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Broken Horizon|Movement|Prone",
        meta = (ClampMin = "0.1", ClampMax = "1.0")
    )
    float ProneStationarySpreadMultiplier = 0.7f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Broken Horizon|Movement|Prone",
        meta = (ClampMin = "0.1", ClampMax = "1.0")
    )
    float ProneAISightRangeMultiplier = 0.65f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Broken Horizon|Movement|Prone",
        meta = (ClampMin = "0.0", Units = "cm/s")
    )
    float ProneStableVelocityThreshold = 10.0f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Broken Horizon|Movement|Traversal",
        meta = (ClampMin = "1.0", Units = "cm")
    )
    float TraversalReach = 120.0f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Broken Horizon|Movement|Traversal",
        meta = (ClampMin = "0.0", Units = "cm")
    )
    float TraversalMinimumHeight = 30.0f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Broken Horizon|Movement|Traversal",
        meta = (ClampMin = "1.0", Units = "cm")
    )
    float VaultMaximumHeight = 100.0f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Broken Horizon|Movement|Traversal",
        meta = (ClampMin = "1.0", Units = "cm")
    )
    float MantleMaximumHeight = 180.0f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Broken Horizon|Movement|Traversal",
        meta = (ClampMin = "1.0", Units = "cm")
    )
    float TraversalSurfaceProbeDepth = 20.0f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Broken Horizon|Movement|Traversal",
        meta = (ClampMin = "1.0", Units = "cm")
    )
    float VaultForwardDistance = 140.0f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Broken Horizon|Movement|Traversal",
        meta = (ClampMin = "0.0", Units = "cm")
    )
    float MantleForwardOffset = 10.0f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Broken Horizon|Movement|Traversal",
        meta = (ClampMin = "0.0", Units = "cm")
    )
    float TraversalApexClearance = 18.0f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Broken Horizon|Movement|Traversal",
        meta = (ClampMin = "0.0", Units = "cm")
    )
    float TraversalLandingClearance = 2.0f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Broken Horizon|Movement|Traversal",
        meta = (ClampMin = "0.0", Units = "cm")
    )
    float TraversalMaximumDrop = 120.0f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Broken Horizon|Movement|Traversal",
        meta = (ClampMin = "0.1", Units = "s")
    )
    float VaultDuration = 0.55f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Broken Horizon|Movement|Traversal",
        meta = (ClampMin = "0.1", Units = "s")
    )
    float MantleDuration = 0.85f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Broken Horizon|Movement|Traversal",
        meta = (ClampMin = "0.0")
    )
    float VaultStaminaCost = 12.0f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Broken Horizon|Movement|Traversal",
        meta = (ClampMin = "0.0")
    )
    float MantleStaminaCost = 20.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Broken Horizon|Stamina")
    float MaxStamina = 100.0f;

    UPROPERTY(
        ReplicatedUsing = OnRep_CurrentStamina,
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "Broken Horizon|Stamina"
    )
    float CurrentStamina = 100.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Broken Horizon|Stamina")
    float StaminaDrainRate = 10.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Broken Horizon|Stamina")
    float StaminaRecoveryRate = 30.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Broken Horizon|Stamina")
    float StaminaRecoveryDelay = 0.75f;

    bool bIsSprinting = false;
    float TimeSinceSprintStopped = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction")
    float InteractionDistance = 300.0f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Broken Horizon|Interaction",
        meta = (ClampMin = "0.1", Units = "s")
    )
    float HoldInteractionDuration = 0.35f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Broken Horizon|Input")
    TObjectPtr<UInputAction> InteractAction;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Broken Horizon|Interaction")
    TSubclassOf<UBHInteractionPromptWidget> InteractionPromptClass;

    UPROPERTY()
    TObjectPtr<UBHInteractionPromptWidget> InteractionPromptWidget;


    UPROPERTY(
        ReplicatedUsing = OnRep_OwnedKeycards,
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "Inventory",
        meta = (AllowPrivateAccess = "true")
    )
    TArray<FName> OwnedKeycards;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory", meta = (AllowPrivateAccess = "true"))
    TSet<FName> CollectedKeycardPersistenceIDs;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objective")
    TSubclassOf<UBHObjectiveWidget> ObjectiveWidgetClass;

    UPROPERTY()
    TObjectPtr<UBHObjectiveWidget> ObjectiveWidget;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Objectives")
    TObjectPtr<UBHMissionData> MissionData;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Objectives", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UBHObjectiveComponent> ObjectiveComponent;

    UPROPERTY()
    TObjectPtr<ABHDefenseMissionDirector> DefenseMissionDirector;

    UPROPERTY()
    TObjectPtr<ABHAmbientWarDirector> AmbientWarDirector;

    UPROPERTY()
    TObjectPtr<ABHOpenWorldOperationDirector>
        OpenWorldOperationDirector;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Persistent War|Field Squad"
    )
    TSubclassOf<ABHEnemySoldier> FieldSquadSoldierClass;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Persistent War|Field Squad",
        meta = (ClampMin = "1", ClampMax = "8")
    )
    int32 MaximumFieldSquadSize = 3;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Persistent War|Field Squad",
        meta = (ClampMin = "100.0", Units = "cm")
    )
    float FieldSquadRecruitmentRadius = 900.0f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Persistent War|Field Squad",
        meta = (ClampMin = "500.0", Units = "cm")
    )
    float FieldSquadBoardingRadius = 2500.0f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Persistent War|Field Squad",
        meta = (ClampMin = "1000.0", Units = "cm")
    )
    float FieldSquadCommandDistance = 12000.0f;

    UPROPERTY(Replicated)
    TArray<TObjectPtr<ABHEnemySoldier>> FieldSquadMembers;

    UPROPERTY()
    TSet<TObjectPtr<ABHEnemySoldier>>
        PendingFieldSquadTransportPassengers;

    UPROPERTY()
    TObjectPtr<ABHFieldTransport> FieldSquadTransport;

    bool bFieldSquadHolding = false;

    bool bFieldSquadHasCommandLocation = false;

    FVector FieldSquadCommandLocation = FVector::ZeroVector;

    FRotator FieldSquadCommandRotation = FRotator::ZeroRotator;

    float NextSquadCommandFailureNotificationTime = 0.0f;

    float NextSquadPingTime = 0.0f;

    int32 ObservedSquadPingRevision = 0;

    FVector LastVisibleSquadPingLocation = FVector::ZeroVector;

    int32 LoggedSquadPingPresentationRevision = 0;

    bool bLoggedSquadPingTargetVisible = false;

    UPROPERTY()
    TObjectPtr<ABHEnemySoldier> FieldSquadContextResponder;

    UPROPERTY()
    TObjectPtr<AActor> FieldSquadContextTarget;

    float FieldSquadContextActionDeadline = 0.0f;

    UPROPERTY(ReplicatedUsing = OnRep_FieldSquadContextStatus)
    EBHFieldSquadContextAction FieldSquadContextAction =
        EBHFieldSquadContextAction::None;

    UPROPERTY(ReplicatedUsing = OnRep_FieldSquadContextStatus)
    FName FieldSquadContextTargetLabel = NAME_None;

    UPROPERTY(ReplicatedUsing = OnRep_FieldSquadContextStatus)
    bool bFieldSquadContextActionReachedTarget = false;

    bool bFieldSquadEmbarked = false;

    float ResupplyWaypointRefreshRemaining = 0.0f;

    float ConvoyWaypointRefreshRemaining = 0.0f;

    float TransportWaypointRefreshRemaining = 0.0f;

    float LogisticsWaypointRefreshRemaining = 0.0f;

    float StrategicSituationHUDRefreshRemaining = 0.0f;

    UPROPERTY(Replicated)
    FName AssignedWarSectorID = NAME_None;

    UPROPERTY(Replicated)
    FName AssignedWarSupplySourceSectorID = NAME_None;

    UPROPERTY(Replicated)
    EBHWarPriorityType AssignedWarPriorityType =
        EBHWarPriorityType::None;

    UPROPERTY(Replicated)
    bool bRuntimeWarOperation = false;
    FName LastObservedPrioritySectorID = NAME_None;
    EBHWarPriorityType LastObservedPriorityType =
        EBHWarPriorityType::None;
    EBHWarCampaignOutcome LastObservedCampaignOutcome =
        EBHWarCampaignOutcome::Ongoing;
    TMap<FName, EBHWarFaction> LastObservedSectorOwners;
    TMap<FName, bool> LastObservedSectorLogisticsConnected;
    TMap<FName, uint8> LastObservedSectorSupplyReadiness;
    bool bLastObservedOperationFundingReady = false;
    FName LastObservedOperationSupplySource = NAME_None;
    FString LastStrategicBriefingContext;
    int32 LastPresentedOperationRevision = INDEX_NONE;
    FName LastPresentedOperationID = NAME_None;
    FName LastPresentedOperationSectorID = NAME_None;
    uint8 LastPresentedOperationPhase = MAX_uint8;
    FName LastNotifiedOperationID = NAME_None;
    FName LastNotifiedOperationSectorID = NAME_None;
    uint8 LastNotifiedOperationPhase = MAX_uint8;
    int32 LastObservedWarEventTurn = INDEX_NONE;
    FName LastObservedWarEventType = NAME_None;
    FName LastObservedWarEventSectorID = NAME_None;
    FString LastObservedWarEventSummary;
    FName LastPresentedStrategicSectorID = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objectives")
    TSubclassOf<UBHObjectiveNotificationWidget> ObjectiveNotificationWidgetClass;

    UPROPERTY()
    TObjectPtr<UBHObjectiveNotificationWidget> ObjectiveNotificationWidget;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Objectives|Mission Complete"
    )
    TSubclassOf<UBHMissionCompleteWidget> MissionCompleteWidgetClass;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Objectives|Mission Complete"
    )
    FText MissionCompleteMessage;

    UPROPERTY()
    TObjectPtr<UBHMissionCompleteWidget> MissionCompleteWidget;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Game Shell|Pause"
    )
    TSubclassOf<UBHPauseMenuWidget> PauseMenuWidgetClass;

    UPROPERTY()
    TObjectPtr<UBHPauseMenuWidget> PauseMenuWidget;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Persistent War|UI"
    )
    TSubclassOf<UBHWarMapWidget> WarMapWidgetClass;

    UPROPERTY()
    TObjectPtr<UBHWarMapWidget> WarMapWidget;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Combat Status"
    )
    TSubclassOf<UBHCombatStatusWidget> CombatStatusWidgetClass;

    UPROPERTY()
    TObjectPtr<UBHCombatStatusWidget> CombatStatusWidget;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory", meta = (AllowPrivateAccess = "true"))
    TSubclassOf<UBHInventoryWidget> InventoryWidgetClass;

    UPROPERTY()
    TObjectPtr<UBHInventoryWidget> InventoryWidget;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Game Shell|Accessibility")
    TSubclassOf<UBHSubtitleWidget> SubtitleWidgetClass;

    UPROPERTY()
    TObjectPtr<UBHSubtitleWidget> SubtitleWidget;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Broken Horizon|Weapon"
    )
    TSubclassOf<UBHAmmoHUDWidget> AmmoHUDWidgetClass;

    UPROPERTY()
    TObjectPtr<UBHAmmoHUDWidget> AmmoHUDWidget;

    UPROPERTY()
    TObjectPtr<UBHHitMarkerWidget> HitMarkerWidget;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Broken Horizon|Death"
    )
    TSubclassOf<UBHDeathWidget> DeathWidgetClass;

    UPROPERTY()
    TObjectPtr<UBHDeathWidget> DeathWidget;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Broken Horizon|Death",
        meta = (ClampMin = "0.0")
    )
    float RespawnDelay = 2.0f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Broken Horizon|Cooperative Casualty",
        meta = (ClampMin = "5.0", Units = "s")
    )
    float PlayerBleedOutDuration = 45.0f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Broken Horizon|Cooperative Casualty",
        meta = (ClampMin = "1.0")
    )
    float PlayerReviveHealth = 35.0f;

    UPROPERTY(ReplicatedUsing = OnRep_PlayerCasualtyState)
    bool bPlayerIncapacitated = false;

    UPROPERTY(ReplicatedUsing = OnRep_PlayerCasualtyState)
    bool bPlayerCasualtyStabilized = false;

    UPROPERTY(ReplicatedUsing = OnRep_PlayerCasualtyState)
    float PlayerBleedOutDeadline = 0.0f;

    UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly,
        Category = "Broken Horizon|Cooperative Casualty")
    TObjectPtr<ABHCharacter> DraggedCasualty;

    UPROPERTY(ReplicatedUsing = OnRep_ControlledBreathing,
        VisibleInstanceOnly, BlueprintReadOnly,
        Category = "Broken Horizon|Combat|Breathing")
    bool bHoldingControlledBreath = false;

    UPROPERTY(ReplicatedUsing = OnRep_WeaponBrace,
        VisibleInstanceOnly, BlueprintReadOnly,
        Category = "Broken Horizon|Combat|Fortification")
    bool bWeaponBraced = false;

    UPROPERTY(ReplicatedUsing = OnRep_WeaponBrace,
        VisibleInstanceOnly, BlueprintReadOnly,
        Category = "Broken Horizon|Combat|Fortification",
        meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float WeaponBraceSupportQuality = 0.0f;

    bool bIsHandlingDeath = false;
    bool bHasEnteredPlayerCasualtyThisLife = false;
    bool bIsHandlingMissionComplete = false;
    bool bCampaignEpilogueAcknowledged = false;
    bool bOperationDebriefAcknowledged = false;
    bool bPauseMenuOpen = false;
    bool bWarMapOpen = false;
    bool bWarMapDeploymentMode = false;
    bool bFirstPersonActionPlaying = false;
    bool bFirstPersonReloadMotionPlaying = false;
    bool bFirstPersonPresentationBaseCached = false;
    bool bLeanCameraBaseCached = false;
    bool bLeanLeftHeld = false;

    TWeakObjectPtr<ABHWarGameState>
        BoundActiveOperationSnapshotGameState;
    FDelegateHandle ActiveOperationSnapshotChangedHandle;

    bool bInteractionInputHeld = false;
    float InteractionPressTime = 0.0f;
    float LastReloadInputTime = -BIG_NUMBER;
    float LastWeaponBashAllowedTime = -BIG_NUMBER;
    float LastFieldObservationAllowedTime = -BIG_NUMBER;
    bool bLeanRightHeld = false;
    bool bIsProne = false;
    bool bIsTraversing = false;
    bool bTraversalIsMantle = false;
    float FirstPersonFireKickAlpha = 0.0f;
    float FirstPersonReloadElapsed = 0.0f;
    float FirstPersonReloadDuration = 0.0f;
    float CurrentLeanAmount = 0.0f;
    float CurrentProneAlpha = 0.0f;
    bool bFragGrenadeCooking = false;
    float FragGrenadeCookStartedTime = 0.0f;
    float HeadBobPhase = 0.0f;
    float FootstepElapsed = 0.0f;
    float CurrentHeadBobOffsetZ = 0.0f;
    float ProneCameraTransitionCompensationZ = 0.0f;
    float PreProneCapsuleRadius = 0.0f;
    float PreProneCapsuleHalfHeight = 0.0f;
    float TraversalElapsed = 0.0f;
    float TraversalDuration = 0.0f;
    float LastPlayerDamageTimeSeconds = -BIG_NUMBER;
    float LastNearMissSoundTimeSeconds = -BIG_NUMBER;
    float ControlledBreathHeldDuration = 0.0f;
    float ControlledBreathRecoveryRemaining = 0.0f;
    float AuthoritativePlayerSuppression = 0.0f;
    float LocalPlayerSuppressionPresentation = 0.0f;
    FVector TraversalStartLocation = FVector::ZeroVector;
    FVector TraversalApexLocation = FVector::ZeroVector;
    FVector TraversalTargetLocation = FVector::ZeroVector;
    FVector FirstPersonPresentationBaseLocation = FVector::ZeroVector;
    FRotator FirstPersonPresentationBaseRotation = FRotator::ZeroRotator;
    FVector LeanCameraBaseRelativeLocation = FVector::ZeroVector;
    FRotator LeanCameraBaseRelativeRotation = FRotator::ZeroRotator;
    TWeakObjectPtr<UAnimSequenceBase> ActiveFirstPersonLoop;
    FTimerHandle RespawnTimerHandle;
    FTimerHandle PlayerBleedOutTimerHandle;

    UFUNCTION()
    void OnRep_EngineeringInventory();
    FTimerHandle FirstPersonActionTimerHandle;
  
};
