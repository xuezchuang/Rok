#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "RokCityGammaLayerCensusSubsystem.generated.h"

/** Flag-only, read-only inventory of the actual GameApp renderer inputs. */
UCLASS()
class ROK_API URokCityGammaLayerCensusSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

	/** Optional second read-only snapshot after a construction audit has settled. */
	bool CaptureNow(const FString& Reason, FString& OutManifestPath);

protected:
	virtual bool DoesSupportWorldType(EWorldType::Type Type) const override;

private:
	FString BaselineSignature;
	float ElapsedSeconds = 0.0f;
	float StableSeconds = 0.0f;
	int32 CaptureSerial = 0;
	bool bAutomaticCaptureDone = false;
};
