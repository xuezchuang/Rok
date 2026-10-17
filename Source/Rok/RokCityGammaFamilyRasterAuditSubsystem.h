#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "RokCityGammaFamilyRasterAuditSubsystem.generated.h"

class FJsonObject;
struct FRokGammaFamilyRasterJob;

/** Local-only isolated GPU fixture. Never registers a game viewport or SceneCapture. */
UCLASS()
class ROK_API URokCityGammaFamilyRasterAuditSubsystem : public UTickableWorldSubsystem
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
	void BeginUpload();
	void BeginInputReadback();
	void BeginFrame();
	void Poll();
	void RecordReady();
	void Finish(const FString& Failure = FString());
	void ReleaseJob();
	FString Directory, FixtureHash;
	TSharedPtr<FJsonObject> Fixture;
	TSharedPtr<FRokGammaFamilyRasterJob, ESPMode::ThreadSafe> Job;
	TArray<TSharedPtr<FJsonObject>> InputResults, CaseResults;
	TArray<FString> Failures;
	int32 InputIndex = 0, CaseIndex = 0;
	uint64 InitialHttpDispatches = 0;
	float MeasuredNightTimeY = 0.0f;
	double StartSeconds = 0, PendingSeconds = 0;
	bool bPrepared = false, bFinished = false, bUploadStarted = false;
};
