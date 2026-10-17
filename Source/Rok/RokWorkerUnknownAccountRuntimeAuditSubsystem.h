#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "RokWorkerUnknownAccountRuntimeAuditSubsystem.generated.h"

class AActor;
class ARokPlayerController;
class FJsonObject;
class FJsonValue;
class UButton;
class UWidget;
class URokBuildingUpgradeSubsystem;
class URokMainHudWidget;
class URokServerSubsystem;
class URokUnityFeatureScreenWidget;
class URokUnityRuntimeStateSubsystem;
class SWidget;
class FSlateVirtualUserHandle;

/** Opt-in read-only local Worker UI route audit. Never starts an upgrade or account request. */
UCLASS()
class ROK_API URokWorkerUnknownAccountRuntimeAuditSubsystem : public UTickableWorldSubsystem
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
	TWeakObjectPtr<URokUnityFeatureScreenWidget> Screen;
	TWeakObjectPtr<URokBuildingUpgradeSubsystem> Buildings;
	TWeakObjectPtr<URokUnityRuntimeStateSubsystem> Runtime;
	TWeakObjectPtr<URokServerSubsystem> Server;
	TWeakObjectPtr<AActor> Worker;
	TWeakObjectPtr<AActor> BaselineUpgradeActor;
	TWeakObjectPtr<UWidget> StableTarget;
	TWeakPtr<SWidget> UserZeroFocus;
	TSharedPtr<FSlateVirtualUserHandle> VirtualUser;
	FDelegateHandle ScreenshotHandle;
	TArray<TSharedPtr<FJsonValue>> Actions;
	TArray<TSharedPtr<FJsonValue>> Captures;
	TArray<FString> Failures;
	TSharedPtr<FJsonObject> Pending;
	FString Directory;
	FString PendingPath;
	FString BaselineResources;
	FString ForegroundAtBegin;
	int32 HardwareCursorX = 0;
	int32 HardwareCursorY = 0;
	FVector2D StableTopLeft = FVector2D::ZeroVector;
	FVector2D StableBottomRight = FVector2D::ZeroVector;
	FTransform BaselineCamera;
	uint64 StableFrame = 0;
	uint64 ReadyFrame = 0;
	uint64 ScreenshotFrame = 0;
	uint64 BaselinePrimaryPresses = 0;
	double Elapsed = 0.0;
	double StepSince = 0.0;
	int32 Step = 0;
	int32 AfterCaptureStep = 0;
	int32 StableReadyFrames = 0;
	int32 StableClickFrames = 0;
	int32 BaselineActiveUpgradeCount = INDEX_NONE;
	int32 BaselineActiveConstructionCount = INDEX_NONE;
	int32 BaselineWorkerLevel = INDEX_NONE;
	bool bBaselineWorkerUpgrading = false;
	bool bBaselineQueueBusy = false;
	bool bStarted = false;
	bool bPendingProcessed = false;
	bool bFinished = false;
	bool bHardwareCursorRead = false;
	bool bDeinitializing = false;
	bool Begin();
	bool OwnOffscreenWindow() const;
	bool Boundary() const;
	bool Ready(int32 Phase) const;
	bool Click(UButton* Target, const TCHAR* Label, int32 ExpectedAction);
	bool Expect(bool Value, const TCHAR* Reason);
	void Capture(const TCHAR* Phase, int32 NextStep);
	void ScreenshotProcessed();
	void AdvanceCapture();
	void Enter(int32 NewStep);
	bool Save(bool Complete) const;
	void Finish(const FString& Error = FString());
};
