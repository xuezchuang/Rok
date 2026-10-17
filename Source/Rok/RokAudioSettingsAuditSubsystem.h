#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "RokAudioSettingsAuditSubsystem.generated.h"

class ARokPlayerController;
class FJsonObject;
class FJsonValue;
class UUserWidget;
class UWidget;
class URokAudioRouterSubsystem;
class URokBuildingUpgradeSubsystem;
class URokClientSettingsSubsystem;
class URokMainHudWidget;
class URokServerSubsystem;
class URokUnityFeatureScreenWidget;
class URokUnityModalWidget;
class URokUnityRuntimeStateSubsystem;

/** Opt-in local-only, real Slate input audit for Unity GeneralSetting audio rows. */
UCLASS()
class ROK_API URokAudioSettingsAuditSubsystem : public UTickableWorldSubsystem
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
	TWeakObjectPtr<URokClientSettingsSubsystem> Settings;
	TWeakObjectPtr<URokAudioRouterSubsystem> Audio;
	TWeakObjectPtr<URokBuildingUpgradeSubsystem> Buildings;
	TWeakObjectPtr<URokUnityRuntimeStateSubsystem> Runtime;
	TWeakObjectPtr<URokServerSubsystem> Server;
	FDelegateHandle ScreenshotHandle;
	TArray<TSharedPtr<FJsonValue>> Actions;
	TArray<TSharedPtr<FJsonValue>> InputAttempts;
	TArray<TSharedPtr<FJsonValue>> Captures;
	TArray<FString> Failures;
	TSharedPtr<FJsonObject> Inputs;
	TSharedPtr<FJsonObject> Pending;
	TArray<TSharedPtr<FJsonValue>> ImportedPcm;
	FString BaselineBusiness;
	FString Directory;
	FString PendingPath;
	FString InitialScalabilityHash;
	FString ScalabilityPath;
	FString ZeroTrackPath;
	FVector2D OriginalPointer = FVector2D::ZeroVector;
	FVector2D LastPointer = FVector2D::ZeroVector;
	double Elapsed = 0.0;
	double StepSince = 0.0;
	uint64 BaselinePrimary = 0;
	uint64 ScreenshotFrame = 0;
	int32 ZeroFinishedCount = -1;
	double ZeroTrackStartedWorldSeconds = -1.0;
	int32 Step = 0;
	int32 NextStep = 0;
	int32 StableFrames = 0;
	int32 InputRetryCount = 0;
	FString InputRetryKey;
	int32 ExpectedMode = 0;
	float InitialGraphics = 0.0f;
	float InitialFrameRate = 0.0f;
	float ExpectedSfx = 1.0f;
	float ExpectedMusic = 1.0f;
	bool bResume = false;
	bool bBegun = false;
	bool bFinished = false;
	bool bScreenshotDone = false;
	bool Begin();
	bool ExportImportedPcm();
	bool Identity() const;
	bool Boundary() const;
	bool Expect(bool Condition, const TCHAR* Error);
	bool OwnWindowFocus();
	bool RetryInput(const TCHAR* Kind, const TCHAR* Name, const FVector2D& Position,
		const TArray<TSharedPtr<FJsonValue>>& Types, const FString& Terminal,
		bool bFocused, bool bOwn, bool bForeground, bool bHit, bool bHovered);
	bool Click(UUserWidget* Owner, const TCHAR* Name);
	bool WheelToAudio();
	bool Slide(const TCHAR* Name, float TargetValue);
	bool Ready(int32 Mode) const;
	TSharedRef<FJsonObject> BusinessState() const;
	TSharedRef<FJsonObject> State() const;
	void Capture(const TCHAR* Phase, int32 After);
	void ScreenshotProcessed();
	void AdvanceCapture();
	void Enter(int32 NewStep);
	bool Save(bool Complete) const;
	void Finish(const FString& Error = FString());
};
