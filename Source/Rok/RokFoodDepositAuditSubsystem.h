#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "RokFoodDepositAuditSubsystem.generated.h"

class ARokWorldMapCameraPawn;
class ARokWorldMapEntityActor;
class URokFoodDepositComponent;
class FJsonObject;
class FJsonValue;

/** Opt-in real world-map replay of the captured Unity zoom sequence. */
UCLASS()
class ROK_API URokFoodDepositAuditSubsystem : public UTickableWorldSubsystem
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
	TWeakObjectPtr<ARokWorldMapCameraPawn> Camera;
	TWeakObjectPtr<ARokWorldMapEntityActor> Entity;
	TWeakObjectPtr<URokFoodDepositComponent> Food;
	TArray<TSharedPtr<FJsonValue>> Samples, Captures;
	TArray<FString> Failures;
	TMap<FString, double> SourcePlaneAreas;
	TSharedPtr<FJsonObject> Pending;
	FDelegateHandle ScreenshotHandle;
	FString Directory, PendingPath;
	double Elapsed = 0, StageStarted = 0;
	int32 SampleIndex = -1;
	bool bFinished = false;
	bool Begin();
	bool Validate();
	TSharedRef<FJsonObject> State() const;
	void NextSample();
	void ScreenshotProcessed();
	bool Save(bool Complete);
	void Finish(const FString& Failure = FString());
};
