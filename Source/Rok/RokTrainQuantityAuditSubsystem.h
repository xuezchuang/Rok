#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "RokTrainQuantityAuditSubsystem.generated.h"

class URokMainHudWidget;
class URokUnityFeatureScreenWidget;
class UUserWidget;
class UEditableTextBox;
class USlider;
class SWidget;
class FJsonObject;
class FJsonValue;

/** Opt-in real-city quantity presentation audit; never starts training. */
UCLASS()
class ROK_API URokTrainQuantityAuditSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()
public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
protected:
	virtual bool DoesSupportWorldType(EWorldType::Type WorldType) const override;
private:
	TWeakObjectPtr<URokMainHudWidget> Hud;
	TWeakObjectPtr<URokUnityFeatureScreenWidget> Feature;
	TWeakObjectPtr<UEditableTextBox> Input;
	TWeakObjectPtr<USlider> Slider;
	TWeakPtr<SWidget> FocusedWidget;
	TSharedPtr<FJsonObject> PendingCapture;
	TArray<TSharedPtr<FJsonValue>> Actions, Captures;
	TArray<FString> Failures;
	FString OutputDirectory, ScreenshotPath;
	FDelegateHandle ScreenshotHandle;
	int32 Step = 0, ExpectedCount = 1;
	float ExpectedSlider = 0.05f;
	double Elapsed = 0, StepStarted = 0, RequestedAt = 0;
	uint64 Frame = 0;
	bool bFinished = false, bCheckFocus = false, bUnknownTier = false;
	bool FindRuntime();
	bool BindControls();
	bool Click(UUserWidget* Owner, FName Name);
	bool Validate();
	void EditText(const FString& Value, int32 Count);
	bool KeyboardReplace(bool bEmpty);
	void EditSlider(float Value, int32 Count);
	void Advance();
	void Capture(const FString& Name);
	void ScreenshotProcessed();
	bool Save(bool bComplete);
	void Fail(const FString& Reason);
	void Finish();
};

