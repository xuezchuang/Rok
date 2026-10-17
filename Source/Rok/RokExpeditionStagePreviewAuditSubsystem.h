#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "UObject/StrongObjectPtr.h"
#include "RokExpeditionStagePreviewAuditSubsystem.generated.h"

class ARokPlayerController;
class FJsonObject;
class FJsonValue;
class UTexture2D;
class UWidget;
class URokBuildingUpgradeSubsystem;
class URokMainHudWidget;
class URokServerSubsystem;
class URokUnityFeatureScreenWidget;
class URokUnityRuntimeStateSubsystem;

/** Opt-in, local-only real Slate/GameViewport read-only expedition preview audit. */
UCLASS()
class ROK_API URokExpeditionStagePreviewAuditSubsystem : public UTickableWorldSubsystem
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
	TArray<TSharedPtr<FJsonValue>> BlockedSlateActions;
	TArray<TSharedPtr<FJsonValue>> Captures;
	TArray<FString> Failures;
	TSharedPtr<FJsonObject> Pending;
	TSharedPtr<FJsonObject> PendingBlockedSlateAction;
	TSharedPtr<FJsonObject> Inputs;
	TSharedPtr<FJsonObject> BaselineBusinessJson;
	TArray<TStrongObjectPtr<UTexture2D>> WarmArtTextures;
	FString Directory;
	FString PendingPath;
	FString BaselineBusinessState;
	FVector2D OriginalPointer = FVector2D::ZeroVector;
	FVector2D LastPointer = FVector2D::ZeroVector;
	double Elapsed = 0.0;
	double StepSince = 0.0;
	int32 Step = 0;
	int32 NextStep = 0;
	int32 StableFrames = 0;
	uint64 ScreenshotFrame = 0;
	uint64 BaselinePrimaryPressCount = 0;
	uint64 WarmStartFrame = 0;
	uint64 WarmReadyFrame = 0;
	bool bScreenshotRequested = false;
	bool bScreenshotDone = false;
	bool bFinished = false;
	bool bArtAudit = false;
	bool bWarmArtReady = false;

	bool Begin();
	bool PrepareOwnGameWindowFocus();
	bool StartArtTextureWarmup();
	bool ArtTexturesReady();
	TSharedRef<FJsonObject> ArtTextureWarmState() const;
	bool Identity() const;
	bool Expect(bool Condition, const TCHAR* Error);
	bool Click(UWidget* Target, const TCHAR* Label);
	bool ProbeBlockedChallenge();
	bool FinishBlockedChallengeProbe();
	bool ValidateBusinessBoundary() const;
	TSharedRef<FJsonObject> BusinessState() const;
	TSharedRef<FJsonObject> State() const;
	void Capture(const TCHAR* Phase, int32 After);
	void AdvanceCapture();
	void ScreenshotProcessed();
	void Enter(int32 NewStep);
	bool Save(bool Complete) const;
	void Finish(const FString& Error = FString());
	void TickArt();
};
