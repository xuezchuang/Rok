#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "RokPlayerController.generated.h"

class URokMainHudWidget;

UCLASS()
class ROK_API ARokPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ARokPlayerController();

protected:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;

private:
	UPROPERTY()
	TObjectPtr<URokMainHudWidget> MainHudWidget;

	UPROPERTY()
	TWeakObjectPtr<AActor> SelectedBuildingActor;

	FVector SelectedBuildingOriginalScale = FVector::OneVector;
	bool bHasSelectedBuildingOriginalScale = false;
	FTimerHandle AutoScreenshotTimerHandle;
	FTimerHandle AutoScreenshotQuitTimerHandle;

	void ScheduleAutoScreenshot();
	void TakeAutoScreenshot();
	void QuitAfterAutoScreenshot();
	void HandlePrimaryClick();
	void SelectBuilding(AActor* Actor);
	void ClearSelection();
	void ApplySelectionHighlight(AActor* Actor, bool bHighlighted);
	bool IsSelectableBuilding(AActor* Actor) const;
	FString GetActorDisplayString(AActor* Actor) const;
};
