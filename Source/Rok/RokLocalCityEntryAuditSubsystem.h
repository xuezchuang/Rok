#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "RokLocalCityEntryAuditSubsystem.generated.h"

class AActor;
class FJsonObject;
class FJsonValue;
class URokBuildingUpgradeSubsystem;
class URokMainHudWidget;
class URokUnityFeatureScreenWidget;
class URokUnityRuntimeStateSubsystem;
class UWidget;

/** Opt-in GameApp check of the BuildCity card-to-placement handoff only. */
UCLASS()
class ROK_API URokLocalCityEntryAuditSubsystem : public UTickableWorldSubsystem
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
	TWeakObjectPtr<URokUnityFeatureScreenWidget> Screen;
	TWeakObjectPtr<URokBuildingUpgradeSubsystem> Buildings;
	TWeakObjectPtr<URokUnityRuntimeStateSubsystem> Runtime;
	TMap<TWeakObjectPtr<AActor>, TArray<uint8>> InitialBuildingSnapshots;
	TMap<int32, TWeakObjectPtr<AActor>> BoundInitialActors;
	TMap<int32, FString> BoundInitialIdentities;
	TArray<uint8> ProfileSnapshot;
	TArray<uint8> ResourceSnapshot;
	TArray<TSharedPtr<FJsonValue>> Captures;
	TArray<TSharedPtr<FJsonValue>> Actions;
	TArray<FString> Failures;
	TSharedPtr<FJsonObject> Inputs;
	TSharedPtr<FJsonObject> Baseline;
	TSharedPtr<FJsonObject> Previews;
	TSharedPtr<FJsonObject> Pending;
	TSharedPtr<FJsonObject> FailureState;
	FString Directory;
	FString PendingPath;
	FDelegateHandle ScreenshotHandle;
	FDelegateHandle PlacementHandle;
	double Elapsed = 0.0;
	double StepStarted = 0.0;
	double ShotStarted = 0.0;
	int32 Step = 0;
	int32 UpgradeQueueCount = 0;
	int32 OwnedQueueCount = 0;
	int32 LocalQueueCount = 0;
	int32 CallbackCount = 0;
	int32 CallbackTypeId = 0;
	bool bOwnsWorld = false;
	bool bFinished = false;
	bool bBaselineReady = false;
	bool bCallbackScreenClosed = false;

	bool Identity();
	bool Begin();
	bool Expect(bool Condition, const TCHAR* Error);
	bool Unchanged();
	bool CheckCardsAndSource();
	bool SlateClick(UWidget* Target, const TCHAR* Name);
	void OnPlacementRequested(int32 TypeId);
	void Enter(int32 Next);
	void Capture(const TCHAR* Name);
	void ScreenshotProcessed();
	TSharedRef<FJsonObject> State() const;
	bool Save(bool Complete);
	void Finish(const FString& Error = FString());
};
