#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "RokCityHallCameraMaterialAuditSubsystem.generated.h"

class FJsonObject;
struct FRokCityHallCameraMaterialAuditJob;

/** Opt-in source-camera Hall day/night material audit on a private GPU render target. */
UCLASS()
class ROK_API URokCityHallCameraMaterialAuditSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()
public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual TStatId GetStatId() const override;
	virtual void Tick(float DeltaTime) override;
protected:
	virtual bool DoesSupportWorldType(EWorldType::Type Type) const override;
private:
	bool Prepare();
	void StartJob();
	void StartInputReadback();
	void RecordCompressedInput();
	void StartFrame();
	void StartSample();
	void Poll();
	void RecordReady();
	void ReleaseJob();
	void Finish(const FString& Failure = FString());
	FString RunDirectory, FixtureSha, ExpectedFixtureSha;
	FString FixtureRelative;
	TSharedPtr<FJsonObject> Fixture;
	TSharedPtr<FRokCityHallCameraMaterialAuditJob, ESPMode::ThreadSafe> Job;
	TArray<TSharedPtr<FJsonObject>> InputResults, FrameResults;
	TArray<TSharedPtr<FJsonObject>> SampleResults;
	TArray<FString> Failures;
	int32 InputIndex = 0, CaseIndex = 0;
	int32 SampleIndex = 0;
	uint64 InitialHttp = 0;
	double StartSeconds = 0.0, PendingSeconds = 0.0;
	bool bPrepared = false, bFinished = false;
	bool bBcVariant = false;
	bool bOrderVariant = false;
	bool bSamplingVariant = false;
};
