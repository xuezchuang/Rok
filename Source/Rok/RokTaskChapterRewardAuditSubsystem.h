#pragma once

#include "CoreMinimal.h"
#include "RokTaskChapterRewardPreviewData.h"
#include "Subsystems/WorldSubsystem.h"
#include "RokTaskChapterRewardAuditSubsystem.generated.h"

class ARokPlayerController;
class FJsonObject;
class FJsonValue;
class UWidget;
class URokBuildingUpgradeSubsystem;
class URokMainHudWidget;
class URokServerSubsystem;
class URokUnityFeatureScreenWidget;
class URokUnityRuntimeStateSubsystem;

/** Opt-in read-only Task Chapter Reward GameApp/Slate audit. */
UCLASS()
class ROK_API URokTaskChapterRewardAuditSubsystem : public UTickableWorldSubsystem
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
	FDelegateHandle ScreenshotHandle;
	TArray<TSharedPtr<FJsonValue>> Actions;
	TArray<TSharedPtr<FJsonValue>> Captures;
	TArray<FString> Failures;
	TArray<int32> ExpectedTaskIdsByChapter[7];
	TArray<FRokTaskChapterRewardGroup> RewardGroups;
	TSharedPtr<FJsonObject> Inputs;
	TSharedPtr<FJsonObject> Pending;
	FString BaselineBusiness;
	FString Directory;
	FString PendingPath;
	FVector2D OriginalPointer = FVector2D::ZeroVector;
	FVector2D LastPointer = FVector2D::ZeroVector;
	double Elapsed = 0.0;
	double StepSince = 0.0;
	uint64 BaselinePrimary = 0;
	uint64 ScreenshotFrame = 0;
	int32 Step = 0;
	int32 NextStep = 0;
	int32 StableFrames = 0;
	bool bScreenshotDone = false;
	bool bBegun = false;
	bool bFinished = false;
	bool Begin();
	bool PrepareOwnWindowFocus();
	bool Identity() const;
	bool Expect(bool Condition, const TCHAR* Error);
	bool Click(UWidget* Target, const TCHAR* Label);
	bool WheelChapterScroll(int32 Chapter);
	bool Ready(int32 Page, int32 Chapter) const;
	bool Boundary() const;
	TSharedRef<FJsonObject> BusinessState() const;
	TSharedRef<FJsonObject> State() const;
	void Capture(const TCHAR* Phase, int32 After);
	void ScreenshotProcessed();
	void AdvanceCapture();
	void Enter(int32 NewStep);
	bool Save(bool Complete) const;
	void Finish(const FString& Error = FString());
};
