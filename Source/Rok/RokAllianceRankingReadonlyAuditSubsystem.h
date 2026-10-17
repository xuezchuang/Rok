#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "RokAllianceRankingReadonlyAuditSubsystem.generated.h"

class ARokPlayerController;
class FJsonObject;
class FJsonValue;
class FSlateVirtualUserHandle;
class SWidget;
class UButton;
class URokMainHudWidget;
class URokServerSubsystem;
class URokUnityFeatureScreenWidget;
class URokUnityModalWidget;
class URokUnityRuntimeStateSubsystem;
class UWidget;

/** Opt-in, local-fixture-only Slate audit of the Alliance ranking configuration preview. */
UCLASS()
class ROK_API URokAllianceRankingReadonlyAuditSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()
public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual bool DoesSupportWorldType(EWorldType::Type Type) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
private:
	TWeakObjectPtr<ARokPlayerController> Controller;
	TWeakObjectPtr<URokMainHudWidget> Hud;
	TWeakObjectPtr<URokUnityFeatureScreenWidget> Screen;
	TWeakObjectPtr<URokUnityModalWidget> Modal;
	TWeakObjectPtr<URokServerSubsystem> Server;
	TWeakObjectPtr<URokUnityRuntimeStateSubsystem> Runtime;
	TArray<TSharedPtr<FJsonValue>> Actions;
	TArray<TSharedPtr<FJsonValue>> Injections;
	TArray<TSharedPtr<FJsonValue>> Captures;
	TArray<FString> Failures;
	TSharedPtr<FJsonObject> Inputs;
	mutable TSharedPtr<FJsonObject> CatalogAtlasDiagnostic;
	TSharedPtr<FJsonObject> AtlasAsyncWaitDiagnostic;
	double AtlasAsyncWaitStart = -1.0;
	int32 AtlasAsyncWaitPolls = 0;
	TSharedPtr<FJsonObject> Pending;
	FDelegateHandle ScreenshotHandle;
	FString Directory;
	FString PendingPath;
	FString PendingComparableState;
	int32 PendingExpectedPage = 0;
	int32 PendingExpectedCategory = INDEX_NONE;
	bool bPendingExpectedRanking = false;
	bool bPendingExpectedClosed = false;
	TWeakPtr<SWidget> UserZeroFocus;
	TSharedPtr<FSlateVirtualUserHandle> VirtualUser;
	FString ForegroundAtBegin;
	FVector2D BeginPhysicalViewport = FVector2D::ZeroVector;
	FVector2D BeginWidgetViewport = FVector2D::ZeroVector;
	float BeginViewportDpiScale = 0.0f;
	float BeginExpectedDpiScale = 0.0f;
	bool bBeginBoundaryObserved = false;
	bool bBeginOwnOffscreenWindow = false;
	int32 HardwareCursorX = 0;
	int32 HardwareCursorY = 0;
	FVector2D StableTopLeft = FVector2D::ZeroVector;
	FVector2D StableBottomRight = FVector2D::ZeroVector;
	TWeakObjectPtr<UWidget> StableTarget;
	uint64 StableTargetFrame = 0;
	uint64 EffectFrame = 0;
	uint64 ScreenshotFrame = 0;
	uint64 BaselinePrimary = 0;
	uint64 BaselineHttpDispatches = 0;
	uint64 BaselineSuppressedRequests = 0;
	double Elapsed = 0.0;
	double StepSince = 0.0;
	int32 Step = 0;
	int32 NextStep = 0;
	int32 CategoryCursor = 0;
	int32 StableFrames = 0;
	int32 ClickStableFrames = 0;
	int32 SlateClicks = 0;
	int32 LocalInjections = 0;
	bool bBegun = false;
	bool bHardwareCursorRead = false;
	bool bScreenshotDone = false;
	bool bFinished = false;
	bool bBaselineSet = false;
	bool bInWrongContext = false;
	bool bInvalidActionTested = false;
	bool bReopened = false;

	bool Identity() const;
	bool Expect(bool Condition, const TCHAR* Reason);
	bool Begin();
	bool AwaitAtlasReady();
	bool OwnOffscreenWindow() const;
	bool InputBoundary() const;
	bool Ready(int32 ExpectedPage, bool bRanking, int32 ExpectedCategory, bool bClosed = false) const;
	bool Click(UButton* Target, int32 ActionId, const TCHAR* Label);
	bool InjectLocalNegative(int32 ActionId, const TCHAR* Label);
	bool EvidenceState(int32 ExpectedPage, bool bRanking, int32 ExpectedCategory) const;
	bool CatalogAndAtlas() const;
	TSharedRef<FJsonObject> State() const;
	FString ComparableState() const;
	void Capture(const FString& Phase, int32 After);
	void ScreenshotProcessed();
	void AdvanceCapture();
	void Enter(int32 NewStep);
	bool Save(bool Complete) const;
	void Finish(const FString& Error = FString());
};
