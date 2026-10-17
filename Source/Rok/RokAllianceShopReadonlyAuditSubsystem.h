#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "RokAllianceShopReadonlyAuditSubsystem.generated.h"

class AActor;
class ARokPlayerController;
class FJsonObject;
class FJsonValue;
class UButton;
class UWidget;
class SWidget;
class FSlateVirtualUserHandle;
class URokBuildingUpgradeSubsystem;
class URokMainHudWidget;
class URokServerSubsystem;
class URokUnityFeatureScreenWidget;
class URokUnityModalWidget;
class URokUnityRuntimeStateSubsystem;
class URokUnityGameplayCatalogSubsystem;

/** Opt-in local-only real Slate audit of Shop static definitions. */
UCLASS()
class ROK_API URokAllianceShopReadonlyAuditSubsystem : public UTickableWorldSubsystem
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
	FVector2D LastPointer = FVector2D::ZeroVector;
	TSharedPtr<FSlateVirtualUserHandle> VirtualUser;
	FString ForegroundAtBegin;
	const SWidget* UserZeroFocusAtBegin = nullptr;
	int32 HardwareCursorXAtBegin = 0;
	int32 HardwareCursorYAtBegin = 0;
	bool bHardwareCursorAtBegin = false;
	FVector2D StableTopLeft = FVector2D::ZeroVector;
	FVector2D StableBottomRight = FVector2D::ZeroVector;
	TWeakObjectPtr<UWidget> StableTarget;
	uint64 StableFrame = 0;
	uint64 BaselinePrimary = 0;
	uint64 ScreenshotFrame = 0;
	int32 GroupCursor = 1;
	int32 RowCursor = 0;
	uint64 EffectFrame = 0;
	TArray<TSharedPtr<FJsonValue>> Effects;
	double Elapsed = 0.0;
	double StepSince = 0.0;
	int32 Step = 0;
	int32 NextStep = 0;
	int32 StableFrames = 0;
	int32 ClickStableFrames = 0;
	int32 CompletionEvents = 0;
	bool bBegun = false;
	bool bBaselineSet = false;
	bool bScreenshotDone = false;
	bool bFinished = false;

	bool Identity() const;
	bool Begin();
	bool Expect(bool Condition, const TCHAR* Reason);
	bool Ready(int32 Phase) const;
	bool Click(UButton* Target, const TCHAR* Label);
	bool RecordEffect(int32 ExpectedGroup, int32 ExpectedItem, bool bOpen);
	bool RowsMatchCurrent() const;
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
