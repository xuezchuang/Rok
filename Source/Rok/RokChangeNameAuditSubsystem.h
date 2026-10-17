#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "RokChangeNameAuditSubsystem.generated.h"

class URokMainHudWidget;
class URokUnityModalWidget;
class UUserWidget;
class UEditableTextBox;
class SWidget;
class FJsonObject;
class FJsonValue;

/** Explicit standalone local-input audit. Never submits a name or accesses the clipboard. */
UCLASS()
class ROK_API URokChangeNameAuditSubsystem : public UTickableWorldSubsystem
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
	enum class EView : uint8 { City, Profile, Opening, Name, Closing };
	TWeakObjectPtr<URokMainHudWidget> Hud;
	TWeakObjectPtr<URokUnityModalWidget> Modal;
	TWeakObjectPtr<UEditableTextBox> Input, PreviousInput;
	TWeakPtr<SWidget> FocusedWidget, PreviousFocus;
	TArray<TSharedPtr<FJsonValue>> Actions, Captures;
	TArray<FString> Failures;
	TSharedPtr<FJsonObject> PendingCapture;
	FDelegateHandle ScreenshotHandle;
	FString Directory, PendingPath, ExpectedText, ExpectedFeedback;
	double Elapsed = 0, StepStarted = 0, ScreenshotStarted = 0;
	uint64 Frame = 0;
	int32 Step = 0;
	EView View = EView::City;
	bool bFinished = false, bCheckFocus = false, bObservedNameRemoval = false;
	bool Begin();
	bool CheckIdentity();
	bool CheckUnknownProfile();
	bool CheckText(FName Name, const FString& Expected);
	bool Validate();
	bool BindInput(bool bReopened);
	bool BindFocus(bool bReopened);
	bool Click(UUserWidget* Owner, FName Name);
	bool EditFixture(const FString& Fixture, const FString& Result, const FString& Feedback, bool bKeyboard);
	void PrepareClose();
	void Enter(int32 NextStep);
	TSharedRef<FJsonObject> State() const;
	void Capture(const TCHAR* Name);
	void ScreenshotProcessed();
	bool Save(bool bComplete);
	void Finish(const FString& Failure = FString());
};
