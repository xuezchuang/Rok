#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "RokPlayerHeadAuditSubsystem.generated.h"

class URokMainHudWidget;
class URokUnityModalWidget;
class UUserWidget;
class FJsonObject;
class FJsonValue;

/** Opt-in standalone audit, real delegates/ticks/screenshots; no account writes. */
UCLASS()
class ROK_API URokPlayerHeadAuditSubsystem : public UTickableWorldSubsystem
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
	TWeakObjectPtr<URokMainHudWidget> Hud;
	TWeakObjectPtr<URokUnityModalWidget> Modal;
	TArray<TSharedPtr<FJsonValue>> Captures;
	TArray<TSharedPtr<FJsonValue>> Actions;
	TArray<FString> Failures;
	FDelegateHandle ScreenshotHandle;
	FString Directory, PendingPath;
	TSharedPtr<FJsonObject> PendingCapture;
	double Elapsed = 0, StepStarted = 0, ScreenshotStarted = 0;
	uint64 Frame = 0;
	int32 Step = 0;
	float ScrollBeforeSelection = 0;
	bool bFinished = false;
	bool bIdentityVerified = false;
	bool FindWidgets();
	bool Click(UUserWidget* Owner, const TCHAR* Name);
	void Next();
	void Capture(const TCHAR* Name);
	void ScreenshotProcessed();
	TSharedRef<FJsonObject> State() const;
	bool WriteManifest(bool Complete);
	void Finish(const FString& Failure = FString());
};
