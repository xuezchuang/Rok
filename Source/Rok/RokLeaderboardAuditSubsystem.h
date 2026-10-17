#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "RokLeaderboardCatalog.h"
#include "RokLeaderboardAuditSubsystem.generated.h"

class URokMainHudWidget;
class URokUnityModalWidget;
class UUserWidget;
class FJsonObject;
class FJsonValue;

/** Opt-in runtime browse audit. Uses existing buttons and never requests rank data. */
UCLASS()
class ROK_API URokLeaderboardAuditSubsystem : public UTickableWorldSubsystem
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
	TArray<FRokLeaderboardRow> Rows;
	TArray<TSharedPtr<FJsonValue>> Actions, Captures;
	TArray<FString> Failures;
	TSharedPtr<FJsonObject> PendingCapture;
	FDelegateHandle ScreenshotHandle;
	FString Directory, PendingPath;
	double Elapsed = 0, StepStarted = 0, ScreenshotStarted = 0;
	uint64 Frame = 0;
	int32 Step = 0, CategoryOrdinal = 0;
	bool bFinished = false;
	bool Begin();
	bool CheckIdentity();
	bool CheckText(FName Name, const FString& Expected);
	bool CheckIcon(FName Name, const FRokLeaderboardRow& Row);
	bool ValidateCity();
	bool ValidateProfile();
	bool ValidateRanking(bool bDetail);
	bool ValidateCapture();
	bool Click(UUserWidget* Owner, FName Name);
	void Enter(int32 NextStep);
	void Capture(const FString& Name);
	void ScreenshotProcessed();
	TSharedRef<FJsonObject> State() const;
	bool Save(bool bComplete);
	void Finish(const FString& Failure = FString());
};
