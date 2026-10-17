#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "RokWorldMapPlayerController.generated.h"

class UWorld;
class ARokWorldMapCityActor;
class URokWorldMapHudWidget;

UCLASS()
class ROK_API ARokWorldMapPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ARokWorldMapPlayerController();

	static bool IsWorldMapSelectableActor(const AActor* Actor);
	static bool CanSelectWorldObjectAtDxf(float Dxf);
	virtual void PlayerTick(float DeltaTime) override;

	UFUNCTION(BlueprintPure, Category = "Rok|World Map")
	AActor* GetSelectedWorldMapActor() const { return SelectedWorldMapActor.Get(); }

	bool SelectWorldMapActorAtScreenPosition(FVector2D ScreenPosition);
	void ClearWorldMapSelection();
	bool RequestEnterPlayerCity(
		TSoftObjectPtr<UWorld> PlayerCityLevel,
		float EntryDxf);

	UFUNCTION(BlueprintCallable, Category = "Rok|World Map")
	bool FocusPlayerLocation();

protected:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;

private:
	UPROPERTY()
	TWeakObjectPtr<AActor> SelectedWorldMapActor;

	UPROPERTY()
	TObjectPtr<URokWorldMapHudWidget> WorldMapHudWidget;

	bool bPlayerCityEntryRequested = false;
	FTimerHandle AutoScreenshotTimerHandle;
	FTimerHandle AutoScreenshotQuitTimerHandle;
#if !UE_BUILD_SHIPPING
	int32 SelectionLodAuditStage = 0;
	TWeakObjectPtr<AActor> SelectionLodAuditActor;
	bool RunSelectionLodAudit();
#endif

	void ScheduleAutoScreenshot();
	void ApplyCommandLineFocus();
	void ApplyDeferredCommandLineFocus();

	FTimerHandle FocusTimerHandle;
	FVector2D TargetFocusDisplay = FVector2D::ZeroVector;
	void TakeAutoScreenshot();
	void QuitAfterAutoScreenshot();
	void HandlePrimaryClick();
	void HandleFocusPlayerLocation();
	ARokWorldMapCityActor* FindPlayerCity() const;
	AActor* FindSelectableActorAtScreenPosition(
		FVector2D ScreenPosition) const;
	void SelectWorldMapActor(AActor* Actor);
	void ApplySelectionHighlight(AActor* Actor, bool bSelected) const;
};
