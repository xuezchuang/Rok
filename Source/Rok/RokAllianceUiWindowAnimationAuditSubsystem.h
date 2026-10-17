#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "RokAllianceUiWindowAnimationAuditSubsystem.generated.h"

class ARokPlayerController;
class UButton;
class UWidget;
class URokMainHudWidget;
class URokUnityFeatureScreenWidget;
class URokServerSubsystem;
class URokUnityRuntimeStateSubsystem;
class FJsonObject;
class FJsonValue;

/** Opt-in local-only real HUD/Slate audit of five source-backed Alliance roots. */
UCLASS()
class ROK_API URokAllianceUiWindowAnimationAuditSubsystem : public UTickableWorldSubsystem
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
	struct FPlan
	{
		const TCHAR* Label;
		const TCHAR* Button;
		int32 Action;
		const TCHAR* Root;
		bool bClose;
		const TCHAR* Destination;
		const TCHAR* Png;
	};
	TWeakObjectPtr<ARokPlayerController> Controller;
	TWeakObjectPtr<URokMainHudWidget> Hud;
	TWeakObjectPtr<URokUnityFeatureScreenWidget> Screen;
	TWeakObjectPtr<URokServerSubsystem> Server;
	TWeakObjectPtr<URokUnityRuntimeStateSubsystem> Runtime;
	TArray<TSharedPtr<FJsonValue>> Actions;
	TArray<TSharedPtr<FJsonValue>> Transitions;
	TArray<TSharedPtr<FJsonValue>> Captures;
	TArray<FString> Failures;
	TSharedPtr<FJsonObject> ActiveTransition;
	TSharedPtr<FJsonObject> PendingCapture;
	TArray<TSharedPtr<FJsonValue>> ActiveSamples;
	TSharedPtr<FJsonObject> Inputs;
	FDelegateHandle ScreenshotHandle;
	FString Directory;
	FString Profile;
	FString PendingPng;
	FVector2D OriginalPointer = FVector2D::ZeroVector;
	FVector2D LastPointer = FVector2D::ZeroVector;
	FVector2D StableTopLeft = FVector2D::ZeroVector;
	FVector2D StableBottomRight = FVector2D::ZeroVector;
	TWeakObjectPtr<UWidget> StableTarget;
	uint64 StableFrame = 0;
	uint64 ScreenshotFrame = 0;
	uint64 BaselinePrimary = 0;
	uint64 DispatchBefore = 0;
	uint64 ExpectedDispatches = 0;
	int32 PlanIndex = 0;
	int32 StableFrames = 0;
	double Elapsed = 0;
	double PlanSince = 0;
	bool bBegun = false;
	bool bFinished = false;
	bool bTracking = false;
	bool bRepeatAttempted = false;
	bool bScreenshotDone = false;
	bool bPointerSaved = false;

	bool Identity() const;
	bool Begin();
	bool Boundary() const;
	bool FocusOwnWindow();
	bool Expect(bool Condition, const TCHAR* Reason);
	bool Click(UButton* Target, int32 ExpectedAction, const TCHAR* Label);
	bool RepeatBackThroughSlate(const FPlan& Plan);
	bool Start(const FPlan& Plan);
	bool Observe(const FPlan& Plan);
	bool Capture(const FPlan& Plan);
	void ScreenshotProcessed();
	bool Save(bool Complete) const;
	void Finish(const FString& Error = FString());
	const FPlan* CurrentPlan() const;
};
