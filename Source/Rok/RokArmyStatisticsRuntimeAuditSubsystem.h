#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "RokArmyStatisticsRuntimeAuditSubsystem.generated.h"

class ARokPlayerController;
class FJsonObject;
class FJsonValue;
class UWidget;
class URokBuildingUpgradeSubsystem;
class URokMainHudWidget;
class URokServerSubsystem;
class URokUnityFeatureScreenWidget;
class URokUnityModalWidget;
class URokUnityRuntimeStateSubsystem;

/** Opt-in, local-only, real Slate Army Statistics audit. This file is staged pending integration. */
UCLASS()
class ROK_API URokArmyStatisticsRuntimeAuditSubsystem : public UTickableWorldSubsystem
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
	TWeakObjectPtr<URokUnityModalWidget> Modal;
	TWeakObjectPtr<URokUnityFeatureScreenWidget> Screen;
	TWeakObjectPtr<URokBuildingUpgradeSubsystem> Buildings;
	TWeakObjectPtr<URokUnityRuntimeStateSubsystem> Runtime;
	TWeakObjectPtr<URokServerSubsystem> Server;
	FDelegateHandle ScreenshotHandle;
	TArray<TSharedPtr<FJsonValue>> Actions;
	TArray<TSharedPtr<FJsonValue>> Captures;
	TArray<FString> Failures;
	TSharedPtr<FJsonObject> Inputs;
	TSharedPtr<FJsonObject> Pending;
	FString BaselineBusiness;
	FString Directory;
	FString PendingPath;
	FTransform BaselineCamera;
	FVector2D OriginalPointer = FVector2D::ZeroVector;
	FVector2D LastPointer = FVector2D::ZeroVector;
	TArray<TSharedPtr<FJsonValue>> Attempts;
	TWeakObjectPtr<UWidget> StableClickTarget;
	FVector2D StableClickTopLeft = FVector2D::ZeroVector;
	FVector2D StableClickBottomRight = FVector2D::ZeroVector;
	FVector2D StableClickLocalSize = FVector2D::ZeroVector;
	TArray<FVector2D> StableClickCorners;
	uint64 StableClickFrame = 0;
	int32 StableClickFrames = 0;
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
	bool Ready(int32 Page) const;
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
