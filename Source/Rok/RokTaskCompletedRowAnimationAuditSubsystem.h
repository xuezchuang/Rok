#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "RokTaskCompletedRowAnimationAuditSubsystem.generated.h"

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
class URokUnityModalWidget;
class URokUnityRuntimeStateSubsystem;
class URokUnityGameplayCatalogSubsystem;

/** Opt-in, non-shipping, local-only real HUD/Slate audit. Staged pending integration. */
UCLASS()
class ROK_API URokTaskCompletedRowAnimationAuditSubsystem : public UTickableWorldSubsystem
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
	TWeakObjectPtr<URokUnityModalWidget> Modal;
	TWeakObjectPtr<URokBuildingUpgradeSubsystem> Buildings;
	TWeakObjectPtr<URokUnityRuntimeStateSubsystem> Runtime;
	TWeakObjectPtr<URokUnityGameplayCatalogSubsystem> Catalog;
	TWeakObjectPtr<URokServerSubsystem> Server;
	TWeakObjectPtr<AActor> Hall;
	FDelegateHandle ScreenshotHandle;
	FDelegateHandle CompletionHandle;
	TArray<TSharedPtr<FJsonValue>> Actions;
	TArray<TSharedPtr<FJsonValue>> Attempts;
	TArray<TSharedPtr<FJsonValue>> Captures;
	TArray<FString> Failures;
	TSharedPtr<FJsonObject> Inputs;
	TSharedPtr<FJsonObject> Pending;
	FString Directory;
	FString PendingPath;
	FString BaselineBusiness;
	TSharedPtr<FJsonObject> BaselineBusinessJson;
	FTransform BaselineCamera;
	FVector2D OriginalPointer = FVector2D::ZeroVector;
	FVector2D LastPointer = FVector2D::ZeroVector;
	FVector2D StableTopLeft = FVector2D::ZeroVector;
	FVector2D StableBottomRight = FVector2D::ZeroVector;
	TWeakObjectPtr<UWidget> StableTarget;
	uint64 StableFrame = 0;
	uint64 BaselinePrimary = 0;
	uint64 ScreenshotFrame = 0;
	TArray<TSharedPtr<FJsonValue>> AnimationSamples;
	TMap<FName, float> FirstOriginalGraphics;
	TMap<FName, float> SecondOriginalGraphics;
	// Keep the actual second-run widgets alive after CloseScreen detaches its content tree.
	UPROPERTY()
	TArray<TObjectPtr<UWidget>> SecondOriginalGraphicWidgets;
	int32 MidSamples = 0;
	int32 SecondMidSamples = 0;
	double FirstClickSeconds = -1.0;
	double FirstCallbackSeconds = -1.0;
	double SecondClickSeconds = -1.0;
	double CancelSeconds = -1.0;
	double Elapsed = 0.0;
	double StepSince = 0.0;
	int32 Step = 0;
	int32 NextStep = 0;
	int32 StableFrames = 0;
	int32 ClickStableFrames = 0;
	int32 CompletionEvents = 0;
	bool bBegun = false;
	bool bPointerSaved = false;
	bool bBaselineSet = false;
	bool bScreenshotDone = false;
	bool bFinished = false;

	bool Identity() const;
	bool Begin();
	bool PrepareOwnWindowFocus();
	bool Expect(bool Condition, const TCHAR* Reason);
	bool Ready(int32 Phase) const;
	bool PrimeStableTarget(UButton* Target);
	bool Click(UButton* Target, const TCHAR* Label);
	void SampleAnimation(const TCHAR* Phase);
	bool Boundary() const;
	TSharedRef<FJsonObject> BusinessState() const;
	TSharedRef<FJsonObject> State() const;
	void Capture(const TCHAR* Phase, int32 After);
	void ScreenshotProcessed();
	void AdvanceCapture();
	void OnUpgradeCompleted(const struct FRokLocalBuildingUpgradeCompleted& Completed);
	void Enter(int32 NewStep);
	bool Save(bool Complete) const;
	void Finish(const FString& Error = FString());
};
