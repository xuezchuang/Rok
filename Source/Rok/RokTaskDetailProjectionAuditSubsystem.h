#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "RokTaskDetailProjectionAuditSubsystem.generated.h"

class AActor;
class ARokPlayerController;
class FJsonObject;
class FJsonValue;
class UWidget;
class URokBuildingPanelWidget;
class URokBuildingUpgradeSubsystem;
class URokMainHudWidget;
class URokServerSubsystem;
class URokUnityFeatureScreenWidget;
class URokUnityModalWidget;
class URokUnityRuntimeStateSubsystem;

/** Opt-in GameApp audit. The only business mutation is the ordinary Hall upgrade UI. */
UCLASS()
class ROK_API URokTaskDetailProjectionAuditSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

protected:
	virtual bool DoesSupportWorldType(EWorldType::Type Type) const override;

private:
	TWeakObjectPtr<ARokPlayerController> Controller;
	TWeakObjectPtr<URokMainHudWidget> Hud;
	TWeakObjectPtr<URokBuildingPanelWidget> Panel;
	TWeakObjectPtr<URokUnityModalWidget> Modal;
	TWeakObjectPtr<URokUnityFeatureScreenWidget> Screen;
	TWeakObjectPtr<URokBuildingUpgradeSubsystem> Buildings;
	TWeakObjectPtr<URokUnityRuntimeStateSubsystem> Runtime;
	TWeakObjectPtr<URokServerSubsystem> Server;
	TWeakObjectPtr<AActor> Hall;
	FDelegateHandle ScreenshotHandle;
	FDelegateHandle CompletionHandle;
	TArray<TSharedPtr<FJsonValue>> Actions;
	TArray<TSharedPtr<FJsonValue>> Captures;
	TArray<FString> Failures;
	TSharedPtr<FJsonObject> Pending;
	TSharedPtr<FJsonObject> Inputs;
	FString Directory;
	FString PendingPath;
	FVector2D OriginalPointer = FVector2D::ZeroVector;
	FVector2D LastPointer = FVector2D::ZeroVector;
	double Elapsed = 0.0;
	double StepSince = 0.0;
	double UpgradePressedWall = 0.0;
	double UpgradeCompletedWall = 0.0;
	int32 Step = 0;
	int32 NextStep = 0;
	int32 UpgradeCompletionCount = 0;
	int32 PostCompletionStableFrames = 0;
	uint64 ScreenshotFrame = 0;
	bool bScreenshotRequested = false;
	bool bScreenshotDone = false;
	bool bFinished = false;

	bool Begin();
	bool Identity() const;
	bool Expect(bool Condition, const TCHAR* Error);
	bool Click(UWidget* Target, const TCHAR* Label);
	bool ValidateState(int32 HallLevel, int32 DisplayCurrent, bool bScreenOpen) const;
	TSharedRef<FJsonObject> State() const;
	void Capture(const TCHAR* Phase, int32 After);
	void AdvanceCapture();
	void ScreenshotProcessed();
	void OnUpgradeCompleted(const struct FRokLocalBuildingUpgradeCompleted& Completed);
	void Enter(int32 NewStep);
	bool Save(bool Complete) const;
	void Finish(const FString& Error = FString());
};
