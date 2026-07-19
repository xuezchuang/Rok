#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "RokPlayerController.generated.h"

class URokMainHudWidget;
class ARokBuildingSelectionIndicator;
class ARokCitySpriteActor;

UCLASS()
class ROK_API ARokPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ARokPlayerController();

protected:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;
	virtual void PlayerTick(float DeltaTime) override;

private:
	UPROPERTY()
	TObjectPtr<URokMainHudWidget> MainHudWidget;

	UPROPERTY()
	TWeakObjectPtr<AActor> SelectedBuildingActor;

	UPROPERTY()
	TObjectPtr<ARokBuildingSelectionIndicator> BuildingSelectionIndicator;

	FVector SelectedBuildingOriginalScale = FVector::OneVector;
	bool bHasSelectedBuildingOriginalScale = false;
	FTimerHandle AutoScreenshotTimerHandle;
	FTimerHandle AutoScreenshotQuitTimerHandle;

	void ScheduleAutoScreenshot();
	void AutoSelectBuildingFromCommandLine();
	void TakeAutoScreenshot();
	void QuitAfterAutoScreenshot();
	void HandlePrimaryClick();
	AActor* FindSelectableBuildingUnderCursor() const;
	ARokCitySpriteActor* FindCityBuildingByUnityFootprint(const FVector2D& ScreenPosition) const;
	void DrawBuildingGridDebug() const;
	void DrawBuildingGridDebugForActor(
		const ARokCitySpriteActor& BuildingActor,
		const FColor& Color,
		float Thickness) const;
	void SelectBuilding(AActor* Actor);
	void ClearSelection();
	void ApplySelectionHighlight(AActor* Actor, bool bHighlighted);
	bool IsSelectableBuilding(AActor* Actor) const;
	FString GetActorDisplayString(AActor* Actor) const;
};
