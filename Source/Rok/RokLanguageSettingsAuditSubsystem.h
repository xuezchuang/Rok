#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "RokLanguageSettingsAuditSubsystem.generated.h"

class URokMainHudWidget;
class URokUnityModalWidget;
class URokUnityFeatureScreenWidget;
class UGameInstance;
class APlayerController;
class UUserWidget;
class FJsonObject;
class FJsonValue;

/** Explicit standalone audit of the source language draft; never applies a locale. */
UCLASS()
class ROK_API URokLanguageSettingsAuditSubsystem : public UTickableWorldSubsystem
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
	enum class EView : uint8 { City, Profile, Settings, Opening, Language, Closing };
	struct FPreferenceFile { FString Path; TArray<uint8> Bytes; FDateTime Modified; bool bExists = false; };
	TWeakObjectPtr<URokMainHudWidget> Hud;
	TWeakObjectPtr<URokUnityModalWidget> Modal;
	TWeakObjectPtr<URokUnityFeatureScreenWidget> Feature;
	TWeakObjectPtr<UGameInstance> InitialGameInstance;
	TWeakObjectPtr<APlayerController> InitialController;
	TArray<FPreferenceFile> PreferenceFiles;
	TMap<FString, FString> PreferencePathsByKey;
	TArray<uint8> InitialClientSettings;
	TArray<TSharedPtr<FJsonValue>> Actions, Captures;
	TArray<FString> Failures;
	TSharedPtr<FJsonObject> PendingCapture;
	FDelegateHandle ScreenshotHandle, CultureChangedHandle;
	FString Directory, PendingPath, InitialLanguage, InitialLocale;
	double Elapsed = 0, StepStarted = 0, ScreenshotStarted = 0;
	uint64 Frame = 0;
	int32 Step = 0, ExpectedDraftId = 40, CultureChangeCount = 0, DiskComparisonCount = 0;
	EView View = EView::City;
	bool bFinished = false, bSnapshot = false, bObservedLanguageRemoval = false;
	bool Begin();
	bool CheckIdentity();
	bool SnapshotPreferences();
	bool CheckUnchanged(bool bCheckDisk);
	bool Validate();
	bool ValidateLanguage();
	bool Click(UUserWidget* Owner, FName Name);
	bool Select(int32 SourceId);
	void CultureChanged();
	void Enter(int32 NextStep);
	TSharedRef<FJsonObject> State() const;
	void Capture(const TCHAR* Name);
	void ScreenshotProcessed();
	bool Save(bool bComplete);
	void Finish(const FString& Failure = FString());
};
