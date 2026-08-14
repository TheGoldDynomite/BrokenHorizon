#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BHOperationSiteMarker.generated.h"

class UArrowComponent;
class UTextRenderComponent;

UCLASS(BlueprintType, Blueprintable)
class BROKENHORIZON_API ABHOperationSiteMarker : public AActor
{
    GENERATED_BODY()

public:
    ABHOperationSiteMarker();

    UFUNCTION(BlueprintCallable, Category = "Operations|Authoring")
    void ConfigureOperationSite(
        FName NewPersistenceID,
        FName NewFamily,
        FName NewVariation,
        FName NewSitePurpose,
        const FText& NewApproachLabel
    );

    UFUNCTION(BlueprintPure, Category = "Operations|Authoring")
    FName GetOperationSitePersistenceID() const;

    UFUNCTION(BlueprintPure, Category = "Operations|Authoring")
    FName GetOperationFamily() const;

    UFUNCTION(BlueprintPure, Category = "Operations|Authoring")
    FName GetOperationVariation() const;

    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Operations|Authoring")
    FName PersistenceID = NAME_None;

    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Operations|Authoring")
    FName OperationFamily = NAME_None;

    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Operations|Authoring")
    FName OperationVariation = NAME_None;

    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Operations|Authoring")
    FName SitePurpose = NAME_None;

    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Operations|Authoring")
    FText ApproachLabel;

protected:
    virtual void BeginPlay() override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Operations|Presentation")
    TObjectPtr<USceneComponent> SceneRoot;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Operations|Presentation")
    TObjectPtr<UArrowComponent> ApproachDirection;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Operations|Presentation")
    TObjectPtr<UTextRenderComponent> SiteLabel;
};
